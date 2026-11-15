#pragma once

#include <stddef.h>
#include <stdint.h>

class FrameBuffer;

// Rozdzielczosc panelu: widoczne okno w pamieci obrazu sterownika (orientacja natywna, wspolrzedne
// po uwzglednieniu lustra panelu - takie jak w FrameBuffer). Biblioteki dodaja to przesuniecie
// w CASET/RASET (np. "colstart/rowstart" w Adafruit, TFT_eSPI).
struct PanelSize {
    const char* name;  // np. "135x240"
    uint16_t width;
    uint16_t height;
    uint16_t x;  // przesuniecie okna w pamieci
    uint16_t y;
    bool round = false;  // okragly panel (np. GC9A01) - podglad na stronie z okragla maska
    // Cechy zalezne od panelu, nie od ukladu (ST7735: ta sama kosc z roznymi szybami); -1 = cecha sterownika.
    int8_t bgr = -1;       // 1: panel BGR (biblioteka ustawia bit BGR w MADCTL), 0: RGB
    int8_t inverted = -1;  // 1: panel IPS z odwroconymi kolorami (biblioteka wysyla INVON), 0: zwykly
};

// Klasa bazowa emulowanego sterownika LCD.
// Dostaje strumien bajtow z przechwytywania SPI rozdzielony linia D/C
// (komenda / dane) i rysuje w FrameBuffer.
class LcdDriver {
public:
    virtual ~LcdDriver() = default;

    virtual const char* name() const = 0;
    // Rozmiar pamieci obrazu sterownika (orientacja natywna panelu).
    virtual uint16_t nativeWidth() const = 0;
    virtual uint16_t nativeHeight() const = 0;

    void attach(FrameBuffer* fb) { fb_ = fb; }
    // Wybrany panel - przed reset(); sterownik moze przyjac z niego cechy (PanelSize::bgr, inverted).
    virtual void setPanel(const PanelSize&) {}

    // Reset sprzetowy (pin RST) albo programowy.
    virtual void reset() = 0;
    virtual void command(uint8_t cmd) = 0;
    virtual void data(const uint8_t* bytes, size_t len) = 0;

    // Stan pokazywany na stronie (efektywny, z uwzglednieniem cech panelu).
    virtual bool displayOn() const = 0;
    virtual bool inverted() const = 0;

    // Sekwencja startowa typowej biblioteki i obraz testowy w widocznym oknie panelu.
    virtual void testInit(const PanelSize& panel) = 0;

protected:
    FrameBuffer* fb_ = nullptr;
};
