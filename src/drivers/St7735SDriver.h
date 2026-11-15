#pragma once

#include "St7735Base.h"

// ST7735S - wersja ukladu ST7735. Nowsza wersja, m.in. moduly 0,96" 80x160 IPS i czesc 1,44".
// Komendy obrazu takie same jak w St7735Base; rozni sie sekwencja startowa (test/SpiLcdMaster.cpp).
class St7735SDriver : public St7735Base {
public:
    // Nazwa sterownika - takze na stronie, w ustawieniach i w profilach testow (test/SpiLcdMaster.cpp).
    static constexpr const char* kName = "ST7735S";

    const char* name() const override { return kName; }
};
