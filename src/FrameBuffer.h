#pragma once

#include <stddef.h>
#include <stdint.h>

#include "pico/critical_section.h"

// Prostokat (wspolrzedne wlacznie).
struct Rect {
    uint16_t x0, y0, x1, y1;
    uint16_t width() const { return x1 - x0 + 1; }
    uint16_t height() const { return y1 - y0 + 1; }
};

// Obraz wyswietlacza w RGB565 (wewnetrzna SRAM) + sledzenie zmienionego obszaru.
// Zapis pikseli bez blokady (rdzen 1), obszar zmian pod sekcja krytyczna (wspolna dla obu rdzeni).
class FrameBuffer {
public:
    FrameBuffer();
    ~FrameBuffer();

    // Zmiana rozmiaru korzysta z istniejacej pamieci, gdy wystarcza - bez zwalniania (patrz reserve()).
    bool begin(uint16_t width, uint16_t height);
    // Przydziela pamiec na najwiekszy obraz z gory: przy zmianie sterownika malloc mniejszego bufora
    // i potem wiekszego zawodzi po fragmentacji sterty (WiFi, LittleFS zajmuja zwolniona reszte).
    bool reserve(size_t pixels);
    void end();

    uint16_t width() const { return width_; }
    uint16_t height() const { return height_; }
    const uint16_t* pixels() const { return pixels_; }

    inline void setPixel(uint16_t x, uint16_t y, uint16_t color) { pixels_[y * width_ + x] = color; }
    inline uint16_t* row(uint16_t y) { return pixels_ + y * width_; }
    void fill(uint16_t color);

    void markDirty(const Rect& r);
    void markAllDirty();
    // Zwraca zmieniony obszar i go czysci. false = nic sie nie zmienilo.
    bool takeDirty(Rect& out);

private:
    uint16_t* pixels_ = nullptr;
    size_t capacity_ = 0;  // pikseli w przydzielonej pamieci
    uint16_t width_ = 0;
    uint16_t height_ = 0;
    bool dirty_ = false;
    Rect dirtyRect_ = {};
    critical_section_t lock_;
};
