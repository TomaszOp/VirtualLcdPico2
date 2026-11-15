#pragma once

#include "St7735Base.h"

// ST7735R - wersja ukladu ST7735. Najczestszy w modulach 1,8" i 1,44" (Adafruit initR).
// Komendy obrazu takie same jak w St7735Base; rozni sie sekwencja startowa (test/SpiLcdMaster.cpp).
class St7735RDriver : public St7735Base {
public:
    // Nazwa sterownika - takze na stronie, w ustawieniach i w profilach testow (test/SpiLcdMaster.cpp).
    static constexpr const char* kName = "ST7735R";

    const char* name() const override { return kName; }
};
