#pragma once

#include <stddef.h>
#include <stdint.h>

// Skad pochodza dane sieci uzyte przy starcie.
enum class WifiSource : uint8_t { Nvs, Secrets, Default };

struct WifiStatus {
    WifiSource source;
    bool apActive;      // dziala punkt dostepu (tryb AP albo awaryjny)
    bool staEnabled;    // urzadzenie probuje polaczyc sie z siecia
    bool staConnected;
    char staSsid[33];
    char apSsid[33];
    char staIp[16];
    char apIp[16];
    int8_t rssi;
};

// WiFi (CYW43 w Pico 2 W): najpierw ustawienia zapisane na stronie (flash), gdy ich brak albo siec
// nie wystartuje - z secrets.h, a na koniec punkt dostepu "VirtualLCD" (siec otwarta, 192.168.4.1).
// Proba STA trwa najwyzej timeoutMs. Przy awaryjnym AP pierwsza siec STA jest dalej ponawiana co kilka minut,
// gdy do AP nikt nie jest podlaczony (CYW43 nie laczy sie z siecia w trakcie pracy jako AP).
class Wifi {
public:
    static constexpr const char* kFallbackSsid = "VirtualLCD";

    static void start(const char* secretSsid, const char* secretPassword, bool secretAp, uint32_t timeoutMs);
    static WifiStatus status();
    // Wywolywane w petli rdzenia 0: ponawianie polaczenia przy awaryjnym AP.
    static void poll();
    // Wyszukiwanie sieci (blokuje kilka sekund), wynik jako tablica JSON. -1 = blad.
    static int scanJson(char* out, size_t len);
};
