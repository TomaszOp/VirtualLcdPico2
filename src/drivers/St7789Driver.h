#pragma once

#include "MipiDcsDriver.h"

// ST7789 z pamiecia 240x320. Panele IPS maja odwrocone kolory,
// dlatego biblioteki wysylaja INVON, zeby obraz wygladal normalnie.
// Mniejsze panele pokazuja tylko czesc pamieci - przesuniecie okna jak w TFT_eSPI (rotacja 0).
class St7789Driver : public MipiDcsDriver {
public:
    // Nazwa sterownika - takze na stronie, w ustawieniach i w profilach testow (test/SpiLcdMaster.cpp).
    static constexpr const char* kName = "ST7789";

    // Rozdzielczosci paneli z tym sterownikiem (pierwsza = domyslna).
    static constexpr PanelSize kPanels[] = {
        {"240x320", 240, 320, 0, 0},
        {"240x280", 240, 280, 0, 20},
        {"240x240", 240, 240, 0, 0},
        {"172x320", 172, 320, 34, 0},
        {"170x320", 170, 320, 35, 0},
        {"135x240", 135, 240, 52, 40},  // np. TTGO T-Display; Adafruit uzywa x = 53
        {"76x284", 76, 284, 82, 18},
    };

    St7789Driver() : MipiDcsDriver({240, 320, false, false, true}) {}

    const char* name() const override { return kName; }

    void testInit(const PanelSize& panel) override {
        static const uint8_t madctl = 0x00;
        static const uint8_t colmod = 0x55;
        send(CMD_SWRESET);
        send(CMD_SLPOUT);
        send(CMD_COLMOD, &colmod, 1);
        send(CMD_MADCTL, &madctl, 1);
        send(CMD_INVON);
        send(CMD_DISPON);
        testPattern(panel);
    }
};
