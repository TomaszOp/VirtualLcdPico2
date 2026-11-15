#pragma once

#include <stddef.h>
#include <stdint.h>

// Odbiorca zdekodowanego strumienia SPI wyswietlacza.
class SpiSink {
public:
    virtual ~SpiSink() = default;
    virtual void onReset() = 0;                                // zbocze opadajace na RST
    virtual void onCommand(uint8_t cmd) = 0;                   // bajt przy D/C = 0
    virtual void onData(const uint8_t* bytes, size_t len) = 0; // bajty przy D/C = 1
};

// Piny emulatora. MOSI, D/C, SCK, CS musza byc kolejnymi GPIO (od mosi) - tak czyta je PIO.
struct SpiPins {
    int mosi;  // = base
    int dc;    // base + 1
    int sck;   // base + 2
    int cs;    // base + 3
    int rst;   // -1 = nieuzywany
};

struct SpiCaptureStats {
    uint32_t bytes;        // zdekodowane bajty
    uint32_t commands;     // z tego komendy (D/C = 0)
    uint32_t smRestarts;   // restarty PIO po urwanym bajcie (CS w gore w srodku bajtu)
    uint32_t resets;       // zbocza na RST
    uint32_t overruns;     // DMA okrazylo dekoder (utrata danych)
    uint32_t maxPending;   // najwiecej bajtow czekajacych na dekodowanie
    uint32_t dmaRearms;    // ponowne uzbrojenia kanalu DMA (licznik transferow sie skonczyl)
    uint32_t rxStalls;     // PIO czekalo na miejsce w FIFO (DMA nie nadazalo) - gubi wtedy zbocza SCK
    uint32_t loadPct;      // obciazenie rdzenia 1 dekodowaniem w ostatniej sekundzie
    uint32_t peakLoadPct;  // najwieksze obciazenie w jednej sekundzie
    uint32_t peakKBps;     // najwiecej zdekodowanych KB/s w jednej sekundzie
    uint32_t restartAt[25];  // restarty PIO wg miejsca w programie (0 = czeka na CS, 1+3k.. = bit 7-k)
};

// Interfejs przechwytywania SPI. Kazdy uklad ma wlasna implementacje (RP2350: PIO + DMA).
class SpiCapture {
public:
    virtual ~SpiCapture() = default;
    virtual bool begin(const SpiPins& pins, SpiSink* sink) = 0;
    // Wywolywane w petli rdzenia 1: dekoduje wszystko, co DMA zdazylo zapisac.
    virtual void poll() = 0;
    virtual SpiCaptureStats stats() const = 0;
};
