#pragma once

#include "MipiDcsDriver.h"

// ILI9341 240x320. Typowy modul ma panel BGR zamontowany w lustrze X
// (dlatego biblioteki w rotacji 0 ustawiaja MADCTL = MX | BGR).
class Ili9341Driver : public MipiDcsDriver {
public:
    // Nazwa sterownika - takze na stronie, w ustawieniach i w profilach testow (test/SpiLcdMaster.cpp).
    static constexpr const char* kName = "ILI9341";

    // Rozdzielczosci paneli z tym sterownikiem (pierwsza = domyslna).
    static constexpr PanelSize kPanels[] = {
        {"240x320", 240, 320, 0, 0},
    };

    Ili9341Driver() : MipiDcsDriver({240, 320, true, true, false}) {}

    const char* name() const override { return kName; }

protected:
    // 0xB6 Display Function Control, drugi parametr: SS (bit 5) odwraca kierunek zrodel (poziomo),
    // GS (bit 6) kierunek bramek (pionowo). Adafruit wysyla 0x82 (oba 0), LovyanGFX 0xC2 (GS = 1).
    void onParam(uint8_t cmd, uint8_t index, uint8_t value) override {
        MipiDcsDriver::onParam(cmd, index, value);
        if (cmd == 0xB6 && index == 1) setScanMirror(value & 0x20, value & 0x40);
    }

public:

    void testInit(const PanelSize& panel) override {
        static const uint8_t madctl = MADCTL_MX | MADCTL_BGR;
        static const uint8_t colmod = 0x55;
        send(CMD_SWRESET);
        send(CMD_SLPOUT);
        send(CMD_MADCTL, &madctl, 1);
        send(CMD_COLMOD, &colmod, 1);
        send(CMD_DISPON);
        testPattern(panel);
    }
};
