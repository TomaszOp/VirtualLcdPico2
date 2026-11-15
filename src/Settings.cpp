#include "Settings.h"

#include <Arduino.h>
#include <LittleFS.h>

namespace {

constexpr uint32_t kMagic = 0x564C4344;  // "VLCD"
constexpr const char* kLcdFile = "/lcd.bin";
constexpr const char* kWifiFile = "/wifi.bin";

bool s_mounted = false;

bool mount() {
    if (!s_mounted) s_mounted = LittleFS.begin();  // przy pierwszym uzyciu partycja jest formatowana
    if (!s_mounted) Serial.println("[settings] LittleFS niedostepny");
    return s_mounted;
}

// Plik: [magic][rozmiar struktury][struktura] - inna wersja struktury = wartosci domyslne.
template <typename T>
bool load(const char* path, T& out) {
    if (!mount()) return false;
    File f = LittleFS.open(path, "r");
    if (!f) return false;
    uint32_t head[2] = {};
    T tmp;
    bool ok = f.read((uint8_t*)head, sizeof(head)) == sizeof(head) && head[0] == kMagic && head[1] == sizeof(T) &&
              f.read((uint8_t*)&tmp, sizeof(T)) == sizeof(T);
    f.close();
    if (ok) out = tmp;
    return ok;
}

template <typename T>
bool save(const char* path, const T& s) {
    if (!mount()) return false;
    File f = LittleFS.open(path, "w");
    if (!f) return false;
    uint32_t head[2] = {kMagic, sizeof(T)};
    bool ok = f.write((const uint8_t*)head, sizeof(head)) == sizeof(head) &&
              f.write((const uint8_t*)&s, sizeof(T)) == sizeof(T);
    f.close();
    return ok;
}

}  // namespace

namespace Settings {

bool loadLcd(LcdSettings& out) {
    bool ok = load(kLcdFile, out);
    out.driver[sizeof(out.driver) - 1] = 0;
    out.resolution[sizeof(out.resolution) - 1] = 0;
    return ok;
}

bool saveLcd(const LcdSettings& s) { return save(kLcdFile, s); }

bool loadWifi(WifiSettings& out) {
    WifiSettings tmp;
    if (!load(kWifiFile, tmp)) return false;
    tmp.ssid[sizeof(tmp.ssid) - 1] = 0;
    tmp.password[sizeof(tmp.password) - 1] = 0;
    out = tmp;
    return out.ssid[0] != 0;
}

bool saveWifi(const WifiSettings& s) { return save(kWifiFile, s); }

bool clearWifi() {
    if (!mount()) return false;
    return !LittleFS.exists(kWifiFile) || LittleFS.remove(kWifiFile);
}

}  // namespace Settings
