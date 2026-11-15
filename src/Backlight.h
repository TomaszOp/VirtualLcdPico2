#pragma once

#include <stdint.h>

// Emulacja pinu LED (podswietlenie) wyswietlacza: pomiar wypelnienia sygnalu na pinie.
// Stan staly (0/1) i PWM sa mierzone tak samo - licznik PWM RP2350 w trybie "B high"
// liczy takty tylko przy stanie wysokim na pinie (pin musi byc kanalem B plastra PWM, czyli GPIO nieparzyste).
class Backlight {
public:
    bool begin(int pin);
    // Wywolywane w petli rdzenia 0; odczyt co kWindowMs, okna dluzsze niz kMaxWindowUs sa pomijane.
    void poll();
    // Zmierzona jasnosc 0..100 % (ostatnie okno pomiaru).
    uint8_t measured() const { return percent_; }
    int pin() const { return pin_; }

private:
    static constexpr uint32_t kWindowMs = 50;
    static constexpr uint32_t kClkDiv = 256;  // takt licznika = zegar systemowy / 256 (16-bit wystarcza na okno)
    static constexpr uint32_t kMaxWindowUs = 60000;  // ponizej przepelnienia licznika przy 250 MHz

    int pin_ = -1;
    unsigned slice_ = 0;
    uint32_t windowStartUs_ = 0;
    uint8_t percent_ = 0;
};
