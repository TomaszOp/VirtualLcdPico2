#include "Wifi.h"

#include <Arduino.h>
#include <WiFi.h>
#include <stdio.h>
#include <string.h>

#include "Settings.h"
#include "web/Json.h"

namespace {

constexpr uint32_t kFallbackRetryMs = 3 * 60 * 1000;  // przy awaryjnym AP: proba STA, gdy nikt nie jest podlaczony
constexpr uint32_t kRetryTimeoutMs = 10000;

WifiSource s_source = WifiSource::Default;
bool s_staEnabled = false;
bool s_fallback = false;  // awaryjny AP, siec STA ponawiana w tle
WifiSettings s_retry;
uint32_t s_nextRetry = 0;
char s_staSsid[33];
char s_apSsid[33];

bool startAp(const char* ssid, const char* password) {
    WiFi.disconnect();
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
    size_t pl = password ? strlen(password) : 0;
    if (pl && pl < 8) Serial.println("[wifi] Haslo AP krotsze niz 8 znakow - siec bedzie otwarta");
    bool ok = WiFi.softAP(ssid, pl >= 8 ? password : nullptr);
    strncpy(s_apSsid, ssid, sizeof(s_apSsid) - 1);
    s_staEnabled = false;
    Serial.printf("[wifi] Punkt dostepu \"%s\" (%s), adres: http://%s/ %s\n", ssid, pl >= 8 ? "WPA2" : "otwarty",
                  WiFi.softAPIP().toString().c_str(), ok ? "" : "- BLAD");
    return ok;
}

bool tryStation(const WifiSettings& ws, uint32_t timeoutMs) {
    s_apSsid[0] = 0;
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    WiFi.setTimeout(timeoutMs);
    strncpy(s_staSsid, ws.ssid, sizeof(s_staSsid) - 1);
    s_staEnabled = true;
    Serial.printf("[wifi] Laczenie z %s...\n", ws.ssid);
    WiFi.begin(ws.ssid, ws.password[0] ? ws.password : nullptr);
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) delay(100);
    if (WiFi.status() != WL_CONNECTED) {
        Serial.printf("[wifi] Brak polaczenia z %s po %lu ms\n", ws.ssid, (unsigned long)timeoutMs);
        return false;
    }
    WiFi.noLowPowerMode();  // mniejsze opoznienia WebSocket
    Serial.printf("[wifi] Polaczono z %s, adres: http://%s/\n", ws.ssid, WiFi.localIP().toString().c_str());
    return true;
}

bool tryStart(const WifiSettings& ws, uint32_t timeoutMs) {
    return ws.ap ? startAp(ws.ssid, ws.password) : tryStation(ws, timeoutMs);
}

}  // namespace

void Wifi::start(const char* secretSsid, const char* secretPassword, bool secretAp, uint32_t timeoutMs) {
    WiFi.setHostname("virtual-lcd");
    WifiSettings saved, secrets;
    bool hasSaved = Settings::loadWifi(saved);
    bool hasSecrets = secretSsid && secretSsid[0];
    if (hasSecrets) {
        secrets.ap = secretAp;
        strncpy(secrets.ssid, secretSsid, sizeof(secrets.ssid) - 1);
        strncpy(secrets.password, secretPassword ? secretPassword : "", sizeof(secrets.password) - 1);
    }
    const WifiSettings* retrySta = nullptr;
    if (hasSaved) {
        Serial.printf("[wifi] Ustawienia zapisane na stronie, tryb %s\n", saved.ap ? "AP" : "STA");
        if (tryStart(saved, timeoutMs)) {
            s_source = WifiSource::Nvs;
            return;
        }
        if (!saved.ap) retrySta = &saved;
    }
    if (hasSecrets) {
        Serial.printf("[wifi] Ustawienia z secrets.h, tryb %s\n", secrets.ap ? "AP" : "STA");
        if (tryStart(secrets, timeoutMs)) {
            s_source = WifiSource::Secrets;
            return;
        }
        if (!secrets.ap && !retrySta) retrySta = &secrets;
    }

    // Punkt dostepu "VirtualLCD" (siec otwarta), a co kilka minut proba polaczenia z siecia.
    s_source = WifiSource::Default;
    startAp(kFallbackSsid, nullptr);
    if (retrySta) {
        s_retry = *retrySta;
        s_fallback = true;
        s_nextRetry = millis() + kFallbackRetryMs;
        strncpy(s_staSsid, s_retry.ssid, sizeof(s_staSsid) - 1);
        Serial.printf("[wifi] Proby polaczenia z %s co %lu s, gdy nikt nie jest podlaczony do AP\n", s_retry.ssid,
                      (unsigned long)(kFallbackRetryMs / 1000));
    }
}

void Wifi::poll() {
    if (!s_fallback || (int32_t)(millis() - s_nextRetry) < 0) return;
    s_nextRetry = millis() + kFallbackRetryMs;
    if (WiFi.softAPgetStationNum() > 0) return;  // ktos korzysta z AP - nie przerywamy
    if (tryStation(s_retry, kRetryTimeoutMs)) {
        s_fallback = false;
        Serial.println("[wifi] Awaryjny punkt dostepu wylaczony");
        return;
    }
    startAp(kFallbackSsid, nullptr);
}

WifiStatus Wifi::status() {
    WifiStatus s = {};
    s.source = s_source;
    s.apActive = s_apSsid[0] != 0;
    s.staEnabled = s_staEnabled || s_fallback;
    s.staConnected = s_staEnabled && WiFi.status() == WL_CONNECTED;
    strncpy(s.staSsid, s_staSsid, sizeof(s.staSsid) - 1);
    if (s.apActive) {
        strncpy(s.apSsid, s_apSsid, sizeof(s.apSsid) - 1);
        strncpy(s.apIp, WiFi.softAPIP().toString().c_str(), sizeof(s.apIp) - 1);
    }
    if (s.staConnected) {
        strncpy(s.staIp, WiFi.localIP().toString().c_str(), sizeof(s.staIp) - 1);
        s.rssi = WiFi.RSSI();
    }
    return s;
}

int Wifi::scanJson(char* out, size_t len) {
    int count = WiFi.scanNetworks();
    if (count < 0) {
        Serial.printf("[wifi] Skanowanie nie powiodlo sie (%d)\n", count);
        return -1;
    }
    // Od najsilniejszego sygnalu, bez powtorzen (ta sama siec z kilku punktow dostepu).
    uint8_t order[64];
    int n = count < 64 ? count : 64;
    for (int i = 0; i < n; i++) order[i] = i;
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && WiFi.RSSI(order[j]) > WiFi.RSSI(order[j - 1]); j--) {
            uint8_t t = order[j];
            order[j] = order[j - 1];
            order[j - 1] = t;
        }
    size_t o = snprintf(out, len, "[");
    bool first = true;
    for (int k = 0; k < n && o + 80 < len; k++) {
        int i = order[k];
        const char* ssid = WiFi.SSID(i);
        if (!ssid || !ssid[0]) continue;
        bool dup = false;
        for (int m = 0; m < k && !dup; m++) dup = strcmp(ssid, WiFi.SSID(order[m])) == 0;
        if (dup) continue;
        o += snprintf(out + o, len - o, "%s{\"ssid\":", first ? "" : ",");
        o = jsonString(out, len, o, ssid);
        o += snprintf(out + o, len - o, ",\"rssi\":%ld,\"open\":%s}", (long)WiFi.RSSI(i),
                      WiFi.encryptionType(i) == ENC_TYPE_NONE ? "true" : "false");
        first = false;
    }
    o += snprintf(out + o, len - o, "]");
    WiFi.scanDelete();
    return (int)o;
}
