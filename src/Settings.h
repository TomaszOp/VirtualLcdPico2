#pragma once

#include <stdint.h>

// Ustawienia zapisywane w pliku na LittleFS (partycja flasha Pico), odczytywane przy starcie.
// Zapis do flasha na chwile wstrzymuje rdzen 1 (przechwytywanie SPI) - robiony tylko na zadanie ze strony.

struct LcdSettings {
    char driver[16] = "ILI9341";
    char resolution[16] = "";  // z listy sterownika, np. "135x240"; pusta = pierwsza z listy
    uint16_t rotation = 0;  // 0, 90, 180, 270 (obrot podgladu na stronie)
    uint8_t scale = 0;      // 0 = dopasuj do okna, 1..4 = staly mnoznik
    bool ignoreBacklight = true;  // pomin pin LED - podswietlenie zawsze 100 %
    uint8_t reserved[4] = {};     // na przyszlosc (zmiana rozmiaru = stary plik ignorowany)
};

struct WifiSettings {
    bool ap = false;  // true: wlasny punkt dostepu, false: polaczenie z siecia (STA)
    char ssid[33] = "";
    char password[65] = "";  // puste = siec otwarta
};

// Ustawienia strony WWW (osobny plik - zapis jezyka nie dotyka ustawien LCD ani WiFi).
struct UiSettings {
    bool english = false;     // jezyk stron: false = polski, true = angielski
    uint8_t reserved[3] = {}; // na przyszlosc
};

namespace Settings {

// Brak pliku = wartosci domyslne ze struktury, wynik false.
bool loadLcd(LcdSettings& out);
bool saveLcd(const LcdSettings& s);

// Wynik true tylko gdy zapisany jest SSID.
bool loadWifi(WifiSettings& out);
bool saveWifi(const WifiSettings& s);
bool clearWifi();

bool loadUi(UiSettings& out);
bool saveUi(const UiSettings& s);

}  // namespace Settings
