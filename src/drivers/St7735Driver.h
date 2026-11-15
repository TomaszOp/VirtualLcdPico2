#pragma once

#include "St7735Base.h"

// ST7735 - wersja ukladu ST7735. Pierwsza wersja ukladu (Adafruit initB, rzadko spotykana).
// Komendy obrazu takie same jak w St7735Base; rozni sie sekwencja startowa (test/SpiLcdMaster.cpp).
class St7735Driver : public St7735Base {
public:
    // Nazwa sterownika - takze na stronie, w ustawieniach i w profilach testow (test/SpiLcdMaster.cpp).
    static constexpr const char* kName = "ST7735";

    const char* name() const override { return kName; }
};
