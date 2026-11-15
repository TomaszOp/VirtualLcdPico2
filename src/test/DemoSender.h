#pragma once

#include <stddef.h>
#include <stdint.h>

#include "SpiLcdMaster.h"
#include "TestLog.h"
#include "capture/SpiCapture.h"

class VirtualLcd;

// Demo przez SPI: nadajnik SPI (te same przewody co test petli) wysyla komendy
// wybranego sterownika jak prawdziwy program z biblioteka LCD:
//   1. rozrzucone kolorowe prostokaty,
//   2. ekran z napisami w roznych kolorach,
//   3. animacja zegara przez 5 sekund (sprite odswiezany w petli, jak w TFT_eSPI).
// Obraz ogladamy na stronie - to test emulatora na "zywym" programie.
class DemoSender {
public:
    DemoSender(VirtualLcd& lcd, TestLog& log) : lcd_(lcd), log_(log) {}

    void setPins(const LoopbackPins& pins, const SpiPins& capturePins) {
        pins_ = pins;
        capturePins_ = capturePins;
    }
    bool enabled() const { return pins_.sck >= 0; }

    // driver: "ILI9341" albo "ST7789", resolution z listy sterownika (pusta = pierwsza), clockMhz: 10, 20 albo 40.
    bool start(const char* driver, const char* resolution, uint32_t clockMhz);
    void runPending();  // w petli rdzenia 0

private:

    void run();

    void rectangles(const LcdProfile& p);
    void texts(const LcdProfile& p, uint32_t clockMhz);
    void clock(const LcdProfile& p);

    // Grafika
    void drawText(uint16_t x, uint16_t y, const char* text, uint8_t size, uint16_t fg, uint16_t bg);
    uint16_t textWidth(const char* text, uint8_t size) const { return strlenSafe(text) * 6 * size; }
    static size_t strlenSafe(const char* s);
    void spriteLine(float x0, float y0, float x1, float y1, int thickness, uint16_t color);
    void spriteDot(int cx, int cy, int r, uint16_t color);
    void renderClockFace(float hours, float minutes, float seconds);
    uint32_t random();

    VirtualLcd& lcd_;
    TestLog& log_;
    LoopbackPins pins_ = {-1, -1, -1, -1, -1};
    SpiPins capturePins_ = {-1, -1, -1, -1, -1};
    char driver_[16] = {};
    char res_[16] = {};
    uint16_t W_ = 240, H_ = 320;      // widoczne okno panelu (rysowanie wzgledem niego)
    uint16_t clockSize_ = 180;         // sprite zegara (kwadrat), dopasowany do panelu
    uint32_t clockMhz_ = 20;
    volatile bool pending_ = false;
    SpiLcdMaster master_;
    uint16_t* sprite_ = nullptr;
    uint16_t glyph_[6 * 4 * 8 * 4];  // znak w rozmiarze do 4
    uint32_t seed_ = 1;
};
