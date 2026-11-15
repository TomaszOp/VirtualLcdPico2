#include "LoopbackTester.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <Arduino.h>

#include "VirtualLcd.h"
#include "hardware/gpio.h"

static constexpr uint32_t kClocks[] = {10000000, 20000000, 40000000};

bool LoopbackTester::start(const char* driver) {
    if (!enabled() || !log_.begin()) return false;
    // "ILI9341" albo "ILI9341@10" - tylko jeden zegar (MHz)
    strncpy(driver_, driver, sizeof(driver_) - 1);
    clockMhz_ = 0;
    if (char* at = strchr(driver_, '@')) {
        *at = 0;
        clockMhz_ = atoi(at + 1);
    }
    // "ST7789/135x240" - rozdzielczosc z listy sterownika (pusta = pierwsza)
    res_[0] = 0;
    if (char* slash = strchr(driver_, '/')) {
        *slash = 0;
        strncpy(res_, slash + 1, sizeof(res_) - 1);
    }
    pending_ = true;
    return true;
}

void LoopbackTester::runPending() {
    if (!pending_) return;
    pending_ = false;
    run();
    log_.end();
}

void LoopbackTester::run() {
    if (!checkWires(pins_, capturePins_, log_)) {
        log_.log("KONIEC: popraw polaczenia");
        return;
    }
    bool all = strcasecmp(driver_, "all") == 0;
    int passed = 0, total = 0;
    for (size_t i = 0; i < LcdProfile::count(); i++) {
        const LcdProfile& p = LcdProfile::at(i);
        if (!all && strcasecmp(p.driver, driver_) != 0) continue;
        const PanelSize& panel = *VirtualLcd::findPanel(p.driver, all ? nullptr : res_);
        if (!lcd_.selectDriver(p.driver, panel.name)) {
            log_.log("%s: BLAD - brak sterownika", p.driver);
            continue;
        }
        for (uint32_t clock : kClocks) {
            if (clockMhz_ && clock != clockMhz_ * 1000000) continue;
            total++;
            if (runProfile(p, panel, clock)) passed++;
        }
    }
    if (total == 0) log_.log("Nieznany sterownik: %s", driver_);
    else log_.log("KONIEC: %d/%d OK", passed, total);
    lcd_.markAllDirty();
}

bool LoopbackTester::runProfile(const LcdProfile& p, const PanelSize& panel, uint32_t clockHz) {
    uint32_t actualHz = master_.open(pins_, clockHz);
    if (!actualHz) {
        log_.log("%s @ %lu MHz: BLAD - SPI nie wystartowal", p.driver, (unsigned long)(clockHz / 1000000));
        return false;
    }
    VirtualLcdStatus before = lcd_.status();
    uint32_t t0 = millis();

    // 1. Inicjalizacja jak biblioteka.
    master_.hardwareReset();
    master_.init(p, panel);

    // 2. Caly panel wzorem, w ktorym kazdy piksel jest inny (okno przesuniete jak colstart/rowstart w bibliotece).
    const uint16_t pw = panel.width, ph = panel.height;
    master_.setOrigin(panel.x, panel.y);
    master_.window(0, 0, pw - 1, ph - 1);
    master_.command(0x2C);
    uint8_t* buf = master_.txBuf();
    const size_t pixelsPerChunk = SpiLcdMaster::kTxBufSize / 2;
    size_t total = (size_t)pw * ph, sent = 0;
    while (sent < total) {
        size_t n = total - sent < pixelsPerChunk ? total - sent : pixelsPerChunk;
        for (size_t i = 0; i < n; i++) {
            uint16_t c = patternColor((sent + i) % pw, (sent + i) / pw);
            buf[i * 2] = c >> 8;
            buf[i * 2 + 1] = c;
        }
        master_.data(buf, n * 2);
        sent += n;
    }
    master_.select(false);
    const uint32_t expected = master_.bytesSent();  // sekwencja, CASET, RASET, RAMWR + piksele
    uint32_t sendMs = millis() - t0;
    SpiLcdMaster::waitMs(100);  // przechwytywanie konczy dekodowanie

    char firstError[96] = {};
    uint32_t bad = verify(panel, firstError, sizeof(firstError));
    VirtualLcdStatus s = lcd_.status();

    // 3. Rotacja 1: prostokat 30x10 w punkcie (10, 20) musi trafic na obrocony obszar pamieci.
    // Przesuniecie okna w rotacji 1 (pamiec x = W-1-wiersz, y = kolumna): kolumny od panel.y,
    // wiersze od W - panel.x - szerokosc panelu - tak liczy je biblioteka przy obrocie.
    const uint16_t xOff = panel.y, yOff = p.width - panel.x - pw;
    master_.select(true);
    const uint8_t rot1 = LcdProfile::madctlFor(p.madctlRot1, panel), rot0 = LcdProfile::madctlFor(p.madctlRot0, panel);
    master_.command(0x36, &rot1, 1);
    master_.setOrigin(xOff, yOff);
    master_.fillRect(10, 20, 30, 10, 0xF800);
    master_.command(0x36, &rot0, 1);
    master_.select(false);
    master_.close();
    SpiLcdMaster::waitMs(50);

    uint16_t row[10];
    uint32_t badRot = 0;
    for (uint16_t col = 10; col <= 39; col++) {  // pamiec: x = W-1 - wiersz, y = kolumna
        uint16_t y = col + xOff;
        lcd_.copyRegion({(uint16_t)(p.width - 1 - (29 + yOff)), y, (uint16_t)(p.width - 1 - (20 + yOff)), y}, row);
        for (uint16_t c : row) badRot += (c != 0xF800);
    }

    uint32_t bytes = s.capture.bytes - before.capture.bytes;
    bool ok = bad == 0 && badRot == 0 && s.displayOn && !s.inverted && bytes == expected;
    log_.log("%s %s @ %.1f MHz: %s - bledne piksele %lu/%lu, rotacja %lu/300, bajty %lu/%lu, ekran %s%s, wysylka %lu ms",
             p.driver, panel.name, actualHz / 1e6, ok ? "OK" : "BLAD", (unsigned long)bad,
             (unsigned long)total, (unsigned long)badRot, (unsigned long)bytes, (unsigned long)expected,
             s.displayOn ? "wl." : "WYL.", s.inverted ? " ODWROCONY" : "", (unsigned long)sendMs);
    if (bad) log_.log("  pierwszy blad: %s", firstError);
    return ok;
}

uint32_t LoopbackTester::verify(const PanelSize& panel, char* firstError, size_t errLen) {
    const uint16_t w = panel.width, h = panel.height;
    uint16_t* line = (uint16_t*)malloc(w * sizeof(uint16_t));
    if (!line) return UINT32_MAX;
    uint32_t bad = 0;
    for (uint16_t y = 0; y < h; y++) {
        uint16_t my = panel.y + y;
        lcd_.copyRegion({panel.x, my, (uint16_t)(panel.x + w - 1), my}, line);
        for (uint16_t x = 0; x < w; x++) {
            uint16_t want = patternColor(x, y);
            if (line[x] == want) continue;
            if (bad == 0)
                snprintf(firstError, errLen, "(%u,%u) jest %04X, powinno %04X", x, y, line[x], want);
            bad++;
        }
    }
    free(line);
    return bad;
}

uint16_t LoopbackTester::patternColor(uint16_t x, uint16_t y) {
    return (uint16_t)((x * 0x0123u) ^ (y * 0x2B1Du) ^ (x << 9) ^ (y >> 3));
}

// Kazdy przewod osobno: wyjscie nadajnika 1/0 musi byc widoczne na wejsciu emulatora.
bool LoopbackTester::checkWires(const LoopbackPins& pins, const SpiPins& capturePins, TestLog& log) {
    struct Wire {
        const char* name;
        int out;
        int in;
    };
    const Wire wires[] = {{"SCK", pins.sck, capturePins.sck},
                          {"MOSI", pins.mosi, capturePins.mosi},
                          {"DC", pins.dc, capturePins.dc},
                          {"CS", pins.cs, capturePins.cs},
                          {"RST", pins.rst, capturePins.rst}};
    char line[160];
    int n = snprintf(line, sizeof(line), "Przewody:");
    bool ok = true;
    for (const auto& w : wires) {
        if (w.out < 0 || w.in < 0) continue;
        gpio_init(w.out);
        gpio_set_dir(w.out, GPIO_OUT);
        gpio_put(w.out, 1);
        sleep_us(100);
        int high = gpio_get(w.in);
        gpio_put(w.out, 0);
        sleep_us(100);
        int low = gpio_get(w.in);
        gpio_put(w.out, 1);
        bool wireOk = high == 1 && low == 0;
        ok &= wireOk;
        n += snprintf(line + n, sizeof(line) - n, " %s(GP%d->GP%d) %s", w.name, w.out, w.in,
                      wireOk ? "OK" : (high == low ? "BRAK" : "ODWROCONY"));
    }
    log.log("%s", line);
    for (const auto& w : wires)
        if (w.out >= 0) gpio_set_dir(w.out, GPIO_IN);
    return ok;
}
