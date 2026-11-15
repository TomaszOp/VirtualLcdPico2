#pragma once

#include "MipiDcsDriver.h"

// GC9A01 (GC9A01A) - okragly panel IPS 240x240. Jak ILI9341: panel BGR w lustrze X
// (Adafruit_GC9A01A w rotacji 0 wysyla MADCTL = MX | BGR), a jak ST7789: kolory odwrocone, biblioteki wysylaja INVON.
// Komendy producenta (0xFE/0xEF odblokowanie rejestrow, gamma, napiecia) sa pomijane.
class Gc9a01Driver : public MipiDcsDriver {
public:
    // Nazwa sterownika - takze na stronie, w ustawieniach i w profilach testow (test/SpiLcdMaster.cpp).
    static constexpr const char* kName = "GC9A01";

    // Rozdzielczosci paneli z tym sterownikiem (pierwsza = domyslna).
    static constexpr PanelSize kPanels[] = {
        {"240x240", 240, 240, 0, 0, true},
    };

    Gc9a01Driver() : MipiDcsDriver({240, 240, true, true, true}) {}

    const char* name() const override { return kName; }

protected:
    // 0xB6 Display Function Control, drugi parametr: SS (bit 5) i GS (bit 6) - jak w ILI9341.
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
        send(CMD_INVON);
        send(CMD_DISPON);
        testPattern(panel);
    }
};
