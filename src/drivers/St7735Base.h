#pragma once

#include "MipiDcsDriver.h"

// Wspolna czesc ukladow ST7735, ST7735R, ST7735S: pamiec obrazu 132x162, panel widzi tylko jej okno
// (przesuniecie "colstart/rowstart" w bibliotekach). Biblioteki (Adafruit, TFT_eSPI) w rotacji 0 wysylaja
// MADCTL = MX | MY (+ BGR zaleznie od panelu), wiec panel jest zamontowany w lustrze w obu osiach.
// Rozdzielczosc, okno, RGB/BGR i IPS zaleza od szyby, nie od wersji ukladu - dlatego sa w kPanels.
class St7735Base : public MipiDcsDriver {
public:
    // Rozdzielczosci paneli (pierwsza = domyslna); okno w pamieci jak colstart/rowstart w rotacji 0.
    // Wersja ukladu moze zaslonic te liste wlasna kPanels.
    static constexpr PanelSize kPanels[] = {
        // 1,8" "green tab" (Adafruit INITR_GREENTAB, TFT_eSPI ST7735_GREENTAB): BGR
        {"128x160", 128, 160, 2, 1, false, 1, 0},
        // 1,44" (Adafruit INITR_144GREENTAB): BGR; wiersz okna 3 jak Adafruit (inne biblioteki 1 - do sprawdzenia)
        {"128x128", 128, 128, 2, 3, false, 1, 0},
        // 0,96" IPS (TFT_eSPI ST7735_GREENTAB160x80, LovyanGFX 26/1; Adafruit INITR_MINI160x80 uzywa 24/0):
        // RGB, kolory odwrocone - biblioteki wysylaja INVON
        {"80x160", 80, 160, 26, 1, false, 0, 1},
    };

    void testInit(const PanelSize& panel) override {
        const uint8_t madctl = MADCTL_MX | MADCTL_MY | (panel_.bgr ? MADCTL_BGR : 0);
        static const uint8_t colmod = 0x05;
        send(CMD_SWRESET);
        send(CMD_SLPOUT);
        send(CMD_COLMOD, &colmod, 1);
        send(CMD_MADCTL, &madctl, 1);
        send(panel_.inverted ? CMD_INVON : CMD_INVOFF);
        send(CMD_DISPON);
        testPattern(panel);
    }

protected:
    // Domyslnie panel BGR, bez IPS - kazdy panel z kPanels i tak podaje swoje cechy.
    St7735Base() : MipiDcsDriver({132, 162, true, true, false, true}) {}
};
