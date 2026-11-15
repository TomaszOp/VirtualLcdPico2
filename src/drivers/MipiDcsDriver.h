#pragma once

#include "LcdDriver.h"

// Wspolna czesc sterownikow zgodnych z MIPI DCS (ILI9341, ST7789, ST7735, ILI9488...):
// CASET/RASET/RAMWR, MADCTL (obrot, lustro, BGR), COLMOD (16/18 bit), INVON, DISPON.
class MipiDcsDriver : public LcdDriver {
public:
    // Cechy fizycznego panelu, ktore biblioteki kompensuja ustawieniami MADCTL/INVON.
    struct PanelTraits {
        uint16_t width;      // pamiec obrazu, orientacja natywna
        uint16_t height;
        bool bgr;            // panel ma kolejnosc BGR
        bool mirrorX;        // panel zamontowany w lustrze w osi X
        bool inverted;       // panel IPS: kolory odwrocone bez INVON
        bool mirrorY = false;  // panel w lustrze w osi Y (ST7735: biblioteki w rotacji 0 wysylaja MX | MY)
    };

    explicit MipiDcsDriver(const PanelTraits& traits) : panel_(traits) {}

    uint16_t nativeWidth() const override { return panel_.width; }
    uint16_t nativeHeight() const override { return panel_.height; }

    void setPanel(const PanelSize& p) override {
        if (p.bgr >= 0) panel_.bgr = p.bgr;
        if (p.inverted >= 0) panel_.inverted = p.inverted;
    }
    void reset() override;
    void command(uint8_t cmd) override;
    void data(const uint8_t* bytes, size_t len) override;

    bool displayOn() const override { return displayOn_ && !sleeping_; }
    bool inverted() const override { return invertOn_ != panel_.inverted; }

protected:
    enum : uint8_t {
        CMD_SWRESET = 0x01,
        CMD_SLPIN = 0x10,
        CMD_SLPOUT = 0x11,
        CMD_INVOFF = 0x20,
        CMD_INVON = 0x21,
        CMD_DISPOFF = 0x28,
        CMD_DISPON = 0x29,
        CMD_CASET = 0x2A,
        CMD_RASET = 0x2B,
        CMD_RAMWR = 0x2C,
        CMD_MADCTL = 0x36,
        CMD_COLMOD = 0x3A,
        CMD_RAMWRC = 0x3C,
    };
    enum : uint8_t { MADCTL_MY = 0x80, MADCTL_MX = 0x40, MADCTL_MV = 0x20, MADCTL_BGR = 0x08 };

    // Parametr komendy innej niz zapis pikseli. Klasy pochodne moga obsluzyc wlasne komendy.
    virtual void onParam(uint8_t cmd, uint8_t index, uint8_t value);

    // Kierunek skanowania panelu ustawiany komenda producenta (np. ILI9341 0xB6: SS i GS).
    // Odwraca obraz niezaleznie od MADCTL - LovyanGFX tak obraca ILI9341 zamiast przez MX.
    void setScanMirror(bool x, bool y);

    // Pomocnicze dla testInit(): wysylka komendy z parametrami i prostokata w jednym kolorze.
    void send(uint8_t cmd, const uint8_t* params = nullptr, size_t len = 0);
    void testFillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
    void testPattern(const PanelSize& panel);

    PanelTraits panel_;

private:
    void setMadctl(uint8_t value);
    void startRamWrite(bool restart);
    void writePixels(const uint8_t* bytes, size_t len);
    inline void putPixel(uint16_t color);

    uint8_t cmd_ = 0;
    uint8_t paramIndex_ = 0;
    uint8_t params_[4] = {};

    uint16_t colStart_ = 0, colEnd_ = 0, rowStart_ = 0, rowEnd_ = 0;
    uint16_t col_ = 0, row_ = 0;
    uint8_t madctl_ = 0;
    bool mv_ = false, mirrorX_ = false, mirrorY_ = false, swapRB_ = false;
    bool scanMirrorX_ = false, scanMirrorY_ = false;
    uint8_t bytesPerPixel_ = 3;
    uint8_t pixelBuf_[3] = {};
    uint8_t pixelBytes_ = 0;

    bool displayOn_ = false;
    bool sleeping_ = true;
    bool invertOn_ = false;
};
