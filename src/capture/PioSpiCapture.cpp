#include "PioSpiCapture.h"

#include <Arduino.h>

#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "spi_capture.pio.h"

// Pierscien DMA: musi byc wyrownany do swojego rozmiaru (zawijanie adresu zapisu w DMA).
static uint16_t s_ring[(1u << 15) / 2] __attribute__((aligned(1u << 15)));

// Slowo z PIO (8 par D/C, MOSI) -> bajt SPI: bity MOSI leza na pozycjach parzystych, najstarszy na 14.
// Pelna tablica 64 KB - jeden odczyt na bajt (dekoder musi nadazac za 40 MHz przy wielu krotkich komendach).
static uint8_t s_lut[65536];

bool PioSpiCapture::begin(const SpiPins& pins, SpiSink* sink) {
    pins_ = pins;
    sink_ = sink;
    if (pins.dc != pins.mosi + 1 || pins.sck != pins.mosi + 2 || pins.cs != pins.mosi + 3) {
        Serial.println("[capture] MOSI, D/C, SCK, CS musza byc kolejnymi GPIO");
        return false;
    }
    for (uint32_t w = 0; w < 65536; w++) {
        uint8_t v = 0;
        for (int b = 7; b >= 0; b--) v = (v << 1) | ((w >> (2 * b)) & 1);
        s_lut[w] = v;
    }

    // Wejscia: CS w gore (nieaktywny bez polaczenia), reszta w dol - niepodlaczone piny nie plywaja.
    for (int p = pins.mosi; p <= pins.cs; p++) {
        gpio_init(p);
        gpio_set_dir(p, GPIO_IN);
        if (p == pins.cs) gpio_pull_up(p);
        else gpio_pull_down(p);
    }
    if (pins.rst >= 0) {
        gpio_init(pins.rst);
        gpio_set_dir(pins.rst, GPIO_IN);
        gpio_pull_up(pins.rst);
        lastRst_ = gpio_get(pins.rst);
    }

    if (!pio_claim_free_sm_and_add_program_for_gpio_range(&spi_capture_program, &pio_, &sm_, &offset_, pins.mosi, 4,
                                                          true)) {
        Serial.println("[capture] Brak wolnej maszyny PIO");
        return false;
    }
    pio_sm_config c = spi_capture_program_get_default_config(offset_);
    sm_config_set_in_pins(&c, pins.mosi);
    sm_config_set_in_shift(&c, false, true, 16);  // w lewo, autopush co 16 bitow = 1 bajt SPI
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);
    sm_config_set_clkdiv(&c, 1.0f);
    pio_sm_init(pio_, sm_, offset_, &c);

    dmaChannel_ = dma_claim_unused_channel(true);
    armDma();
    pio_sm_set_enabled(pio_, sm_, true);

    Serial.printf("[capture] PIO%u SM%u: MOSI=GP%d DC=GP%d SCK=GP%d CS=GP%d RST=GP%d, DMA kanal %d\n",
                  pio_get_index(pio_), sm_, pins.mosi, pins.dc, pins.sck, pins.cs, pins.rst, dmaChannel_);
    return true;
}

void PioSpiCapture::armDma() {
    dma_channel_config cfg = dma_channel_get_default_config(dmaChannel_);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, true);
    channel_config_set_ring(&cfg, true, kRingBits);  // zawijanie adresu zapisu
    channel_config_set_dreq(&cfg, pio_get_dreq(pio_, sm_, false));
    dma_channel_configure(dmaChannel_, &cfg, s_ring, &pio_->rxf[sm_], dma_encode_transfer_count(kMaxTransfers), true);
}

uint64_t PioSpiCapture::dmaTotal() const {
    uint32_t remaining = dma_hw->ch[dmaChannel_].transfer_count & 0x0FFFFFFF;
    return dmaBase_ + (kMaxTransfers - remaining);
}

void __not_in_flash_func(PioSpiCapture::poll)() {
    // RST: zbocze opadajace (impuls trwa co najmniej kilka us - wystarczy sprawdzanie w petli).
    if (pins_.rst >= 0) {
        bool level = gpio_get(pins_.rst);
        if (lastRst_ && !level) {
            flushData();
            sink_->onReset();
            stats_.resets++;
        }
        lastRst_ = level;
    }

    // Licznik transferow DMA sie wyczerpal - uzbrajamy dalej (adres zapisu zostaje w pierscieniu).
    if (!dma_channel_is_busy(dmaChannel_)) {
        dmaBase_ += kMaxTransfers;
        dma_channel_set_trans_count(dmaChannel_, dma_encode_transfer_count(kMaxTransfers), true);
        stats_.dmaRearms++;
    }

    // PIO stalo na pelnym FIFO - flaga RXSTALL (kasowana zapisem 1).
    const uint32_t stallBit = 1u << (PIO_FDEBUG_RXSTALL_LSB + sm_);
    if (pio_->fdebug & stallBit) {
        pio_->fdebug = stallBit;
        stats_.rxStalls++;
    }

    // Obciazenie rdzenia 1 liczone w oknach 1 s.
    const uint32_t now = time_us_32();
    if (now - windowStart_ >= 1000000) {
        uint32_t span = now - windowStart_;
        stats_.loadPct = (uint64_t)busyUs_ * 100 / span;
        if (stats_.loadPct > stats_.peakLoadPct) stats_.peakLoadPct = stats_.loadPct;
        uint32_t kbps = (uint64_t)windowBytes_ * 1000 / span;
        if (kbps > stats_.peakKBps) stats_.peakKBps = kbps;
        busyUs_ = windowBytes_ = 0;
        windowStart_ = now;
    }

    uint64_t total = dmaTotal();
    uint64_t pending = total - readTotal_;
    if (pending > stats_.maxPending) stats_.maxPending = pending;
    if (pending > kRingWords - 256) {
        // DMA okrazylo dekoder - dane stracone, zaczynamy od biezacej pozycji.
        stats_.overruns++;
        readTotal_ = total;
        dataLen_ = 0;
        return;
    }

    const uint32_t start = time_us_32();
    windowBytes_ += (uint32_t)pending;
    // Dekodowanie odcinkami pierscienia (bez zawijania indeksu w petli). Bajty danych (D/C = 1) leca ciagiem
    // do bufora, komenda (D/C = 0) najpierw oddaje zebrane dane sterownikowi.
    uint32_t idx = (uint32_t)readTotal_ & (kRingWords - 1);
    uint32_t left = (uint32_t)pending;
    while (left) {
        uint32_t seg = left < kRingWords - idx ? left : kRingWords - idx;
        const uint16_t* p = s_ring + idx;
        const uint16_t* const end = p + seg;
        while (p < end) {
            uint8_t* d = data_ + dataLen_;
            uint8_t* const dEnd = data_ + kDataChunk;
            while (p < end && d < dEnd && (*p & 0x0002)) *d++ = s_lut[*p++];
            dataLen_ = d - data_;
            if (dataLen_ == kDataChunk) flushData();
            if (p < end && !(*p & 0x0002)) {
                flushData();
                sink_->onCommand(s_lut[*p++]);
                stats_.commands++;
            }
        }
        left -= seg;
        idx = 0;
    }
    readTotal_ = total;
    stats_.bytes += (uint32_t)pending;
    flushData();
    if (pending) busyUs_ += time_us_32() - start;

    // CS w gore, a PIO nie stoi na poczatku bajtu - bajt urwany (zaklocenie albo niepelna transmisja).
    // Restart, zeby nastepna transakcja zaczela sie od pelnego bajtu. Dwa kolejne odczyty dla pewnosci.
    // PC = offset+1 (czeka na pierwsze zbocze nowego bajtu) to nie blad: po ostatnim bajcie CS byl jeszcze
    // chwile nisko, wiec PIO zaczelo czekac na kolejny bajt. Zaden bit nie przepadl - bez restartu.
    const uint32_t pc = pio_sm_get_pc(pio_, sm_);
    if (gpio_get(pins_.cs) && pc != offset_ && pc != offset_ + 1) {
        if (++csStuck_ >= 2) {
            uint32_t at = pio_sm_get_pc(pio_, sm_) - offset_;
            if (at < 25) stats_.restartAt[at]++;
            pio_sm_restart(pio_, sm_);
            pio_sm_exec(pio_, sm_, pio_encode_jmp(offset_));
            stats_.smRestarts++;
            csStuck_ = 0;
        }
    } else {
        csStuck_ = 0;
    }
}

void __not_in_flash_func(PioSpiCapture::flushData)() {
    if (dataLen_ == 0) return;
    sink_->onData(data_, dataLen_);
    dataLen_ = 0;
}
