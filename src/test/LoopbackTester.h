#pragma once

#include <stddef.h>
#include <stdint.h>

#include "SpiLcdMaster.h"
#include "drivers/LcdDriver.h"
#include "TestLog.h"
#include "capture/SpiCapture.h"

class VirtualLcd;

// Test pętli zwrotnej: ten sam ESP32-S3 wysyla przez SPI2 (master) sekwencje
// jak typowa biblioteka LCD, a przechwytywanie odbiera ja na pinach emulatora.
// Po wyslaniu obraz w buforze jest porownywany piksel po pikselu z oczekiwanym.
class LoopbackTester {
public:
    LoopbackTester(VirtualLcd& lcd, TestLog& log) : lcd_(lcd), log_(log) {}

    void setPins(const LoopbackPins& pins, const SpiPins& capturePins) {
        pins_ = pins;
        capturePins_ = capturePins;
    }
    bool enabled() const { return pins_.sck >= 0; }

    // Zleca test. driver: "ILI9341", "ST7789/135x240" albo "all" (kazdy sterownik z pierwsza rozdzielczoscia),
    // opcjonalnie "@10" = tylko 10 MHz.
    // Sam test wykonuje runPending() w petli rdzenia 0 (nie w obsludze strony).
    bool start(const char* driver);
    void runPending();

    // Sprawdza kazdy przewod osobno (wspolne z DemoSender). Wynik trafia do logu.
    static bool checkWires(const LoopbackPins& pins, const SpiPins& capturePins, TestLog& log);

private:
    void run();
    bool runProfile(const LcdProfile& p, const PanelSize& panel, uint32_t clockHz);
    uint32_t verify(const PanelSize& panel, char* firstError, size_t errLen);

    static uint16_t patternColor(uint16_t x, uint16_t y);

    VirtualLcd& lcd_;
    TestLog& log_;
    LoopbackPins pins_ = {-1, -1, -1, -1, -1};
    SpiPins capturePins_ = {-1, -1, -1, -1, -1};
    char driver_[24] = {};
    char res_[16] = {};
    uint32_t clockMhz_ = 0;
    volatile bool pending_ = false;
    SpiLcdMaster master_;
};
