#include "WebServer.h"

#include <Arduino.h>
#include <Hash.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "Json.h"
#include "Pages.h"
#include "Settings.h"
#include "TestHtml.h"
#include "VirtualLcd.h"
#include "net/Wifi.h"

namespace {

void base64(const uint8_t* in, size_t len, char* out) {
    static const char tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t o = 0;
    for (size_t i = 0; i < len; i += 3) {
        uint32_t v = in[i] << 16 | (i + 1 < len ? in[i + 1] << 8 : 0) | (i + 2 < len ? in[i + 2] : 0);
        out[o++] = tbl[(v >> 18) & 63];
        out[o++] = tbl[(v >> 12) & 63];
        out[o++] = i + 1 < len ? tbl[(v >> 6) & 63] : '=';
        out[o++] = i + 2 < len ? tbl[v & 63] : '=';
    }
    out[o] = 0;
}

int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Pole formularza application/x-www-form-urlencoded z dekodowaniem %XX i '+'.
// false = brak pola albo za dlugie.
bool formField(const char* body, const char* key, char* out, size_t len) {
    size_t kl = strlen(key);
    for (const char* p = body; p && *p;) {
        const char* end = strchr(p, '&');
        if (strncmp(p, key, kl) == 0 && p[kl] == '=') {
            size_t n = 0;
            for (const char* v = p + kl + 1; *v && v != end; v++) {
                char c = *v;
                if (c == '+') {
                    c = ' ';
                } else if (c == '%' && hexValue(v[1]) >= 0 && hexValue(v[2]) >= 0) {
                    c = (char)(hexValue(v[1]) * 16 + hexValue(v[2]));
                    v += 2;
                }
                if (n + 1 >= len) return false;
                out[n++] = c;
            }
            out[n] = 0;
            return true;
        }
        p = end ? end + 1 : nullptr;
    }
    return false;
}

uint32_t ipOf(WiFiClient& c) { return (uint32_t)c.remoteIP(); }

// RLE na pikselach 16-bit. Slowa wyjscia:
//   bit 15 = 1: powtorzenie - (slowo & 0x7FFF) + 1 razy kolor z nastepnego slowa,
//   bit 15 = 0: (slowo + 1) kolejnych pikseli skopiowanych dalej.
// Powtorzenia od 3 pikseli (krotsze sa tansze jako kopia), wiec wynik jest najwyzej o kilka slow dluzszy.
size_t __not_in_flash_func(rleEncode)(const uint16_t* in, size_t n, uint16_t* out) {
    size_t o = 0, i = 0, litStart = 0;
    auto flushLiteral = [&](size_t end) {
        while (litStart < end) {
            size_t k = end - litStart > 0x8000 ? 0x8000 : end - litStart;
            out[o++] = (uint16_t)(k - 1);
            memcpy(out + o, in + litStart, k * 2);
            o += k;
            litStart += k;
        }
    };
    while (i < n) {
        size_t j = i + 1;
        while (j < n && in[j] == in[i] && j - i < 0x8000) j++;
        if (j - i >= 3) {
            flushLiteral(i);
            out[o++] = 0x8000 | (uint16_t)(j - i - 1);
            out[o++] = in[i];
            litStart = j;
        }
        i = j;
    }
    flushLiteral(n);
    return o;
}

// Dopisuje tekst od pozycji n; przy braku miejsca nic nie dopisuje (n nigdy nie wyjdzie poza bufor).
size_t append(char* out, size_t len, size_t n, const char* s) {
    size_t l = strlen(s);
    if (n + l >= len) return n;
    memcpy(out + n, s, l + 1);
    return n + l;
}

// [{"name":"ILI9341","res":["240x320"]},...] - sterowniki z lista rozdzielczosci.
// Bufor wymiarowany w kDriversJsonSize (2 KB starcza na kilkanascie sterownikow).
size_t driversJson(char* out, size_t len, size_t n) {
    n = append(out, len, n, "[");
    for (size_t i = 0; i < VirtualLcd::driverCount(); i++) {
        n = append(out, len, n, i ? ",{\"name\":" : "{\"name\":");
        n = jsonString(out, len, n, VirtualLcd::driverName(i));
        n = append(out, len, n, ",\"res\":[");
        for (size_t j = 0; j < VirtualLcd::panelCount(i); j++) {
            if (j) n = append(out, len, n, ",");
            n = jsonString(out, len, n, VirtualLcd::panel(i, j).name);
        }
        n = append(out, len, n, "]}");
    }
    n = append(out, len, n, "]");
    return n;
}

// Lista sterownikow w JSON ma stala zawartosc - wystarczy jeden bufor na ustawienia i WebSocket.
constexpr size_t kDriversJsonSize = 2048;

// "ST7789/135x240" -> sterownik i rozdzielczosc (pusta, gdy brak).
void splitDriver(const char* in, char* driver, size_t dl, char* res, size_t rl) {
    strncpy(driver, in, dl - 1);
    driver[dl - 1] = 0;
    res[0] = 0;
    if (char* slash = strchr(driver, '/')) {
        *slash = 0;
        strncpy(res, slash + 1, rl - 1);
        res[rl - 1] = 0;
    }
}

}  // namespace

bool WebServer::begin(uint16_t port) {
    sendBuf_ = (uint8_t*)malloc(8 + kMaxPixelsPerFrame * 2);
    rleBuf_ = (uint8_t*)malloc(8 + kMaxPixelsPerFrame * 2 + 64);
    if (!sendBuf_ || !rleBuf_) return false;
    server_ = new WiFiServer(port);
    server_->begin();
    Serial.printf("[web] Serwer WWW na porcie %u\n", port);
    return true;
}

void WebServer::poll() {
    if (inPoll_ || !server_) return;  // wywolanie z testu w trakcie obslugi strony
    inPoll_ = true;

    WiFiClient c = server_->accept();
    if (c) handleHttp(c);

    uint32_t now = millis();
    for (size_t i = 0; i < kMaxClients; i++) {
        if (!clients_[i].open) continue;
        if (!clients_[i].conn.connected()) {
            closeClient(i);
        } else if (now - clients_[i].lastSeen > kClientTimeoutMs) {
            Serial.println("[web] Klient nie odpowiada, rozlaczam");
            silentDrops_++;
            closeClient(i);
        } else {
            readWs(i);
        }
    }
    if (clientCount()) sendUpdates();

    if (restartAt_ && (int32_t)(now - restartAt_) >= 0) {
        Serial.println("[web] Restart");
        Serial.flush();
        rp2040.reboot();
    }
    inPoll_ = false;
}

// --- HTTP ---

bool WebServer::readRequest(WiFiClient& c, Request& r) {
    memset(&r, 0, sizeof(r));
    c.setTimeout(1000);
    String line = c.readStringUntil('\n');
    if (sscanf(line.c_str(), "%7s %63s", r.method, r.path) != 2) return false;
    if (char* q = strchr(r.path, '?')) *q = 0;
    size_t contentLength = 0;
    for (;;) {
        String h = c.readStringUntil('\n');
        h.trim();
        if (h.length() == 0) break;
        if (strncasecmp(h.c_str(), "Sec-WebSocket-Key:", 18) == 0) {
            String k = h.substring(18);
            k.trim();
            strncpy(r.wsKey, k.c_str(), sizeof(r.wsKey) - 1);
        } else if (strncasecmp(h.c_str(), "Upgrade:", 8) == 0 && h.indexOf("websocket") >= 0) {
            r.upgrade = true;
        } else if (strncasecmp(h.c_str(), "Content-Length:", 15) == 0) {
            contentLength = atoi(h.c_str() + 15);
        }
    }
    if (contentLength >= sizeof(r.body)) return false;
    if (contentLength && c.readBytes(r.body, contentLength) != contentLength) return false;
    r.body[contentLength] = 0;
    return true;
}

void WebServer::handleHttp(WiFiClient& c) {
    Request* r = new Request;
    if (!readRequest(c, *r)) {
        delete r;
        c.stop();
        return;
    }
    const bool get = strcmp(r->method, "GET") == 0;
    const bool post = strcmp(r->method, "POST") == 0;
    const char* p = r->path;

    if (get && strcmp(p, "/ws") == 0 && r->upgrade && r->wsKey[0]) {
        upgrade(c, r->wsKey);
        delete r;
        return;  // polaczenie zostaje otwarte
    }
    if (get && strcmp(p, "/") == 0) sendPage(c, kMainHtml);
    else if (get && strcmp(p, "/lcd") == 0) sendPage(c, kLcdHtml);
    else if (get && strcmp(p, "/wifi") == 0) sendPage(c, kWifiHtml);
    else if (get && strcmp(p, "/test") == 0) sendPage(c, kTestHtml);
    else if (get && strcmp(p, "/api/settings") == 0) settingsGet(c);
    else if (post && strcmp(p, "/api/settings") == 0) settingsPost(c, r->body);
    else if (post && strcmp(p, "/api/test") == 0) {
        lcd_.runTestPattern();
        sendJson(c, "{\"ok\":true}");
    }
    else if (get && strcmp(p, "/api/wifi") == 0) wifiGet(c);
    else if (post && strcmp(p, "/api/wifi") == 0) wifiPost(c, r->body);
    else if (post && strcmp(p, "/api/wifi/clear") == 0) {
        if (!Settings::clearWifi()) {
            sendError(c, "blad zapisu");
        } else {
            Serial.println("[web] Usunieto ustawienia WiFi, restart");
            sendJson(c, "{\"ok\":true}");
            restartAt_ = millis() + 800;
        }
    } else if (get && strcmp(p, "/api/scan") == 0) scan(c);
    else if (post && strcmp(p, "/api/reboot") == 0) {
        sendJson(c, "{\"ok\":true}");
        restartAt_ = millis() + 800;
    } else if (get && strcmp(p, "/status") == 0) status(c);
    else c.print("HTTP/1.1 404 Not Found\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
    delete r;
    c.stop();
}

void WebServer::sendPage(WiFiClient& c, const char* html) {
    // Wczytanie strony = poprzednia strona w tej przegladarce jest juz nieaktualna.
    kickIp(ipOf(c), -1);
    size_t len = strlen(html);
    c.printf("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: %u\r\n"
             "Connection: close\r\n\r\n",
             (unsigned)len);
    c.write((const uint8_t*)html, len);
}

void WebServer::sendJson(WiFiClient& c, const char* json, const char* status) {
    c.printf("HTTP/1.1 %s\r\nContent-Type: application/json\r\nCache-Control: no-store\r\nContent-Length: %u\r\n"
             "Connection: close\r\n\r\n",
             status, (unsigned)strlen(json));
    c.print(json);
}

void WebServer::sendError(WiFiClient& c, const char* message) {
    char json[160];
    size_t n = snprintf(json, sizeof(json), "{\"ok\":false,\"error\":");
    n = jsonString(json, sizeof(json), n, message);
    snprintf(json + n, sizeof(json) - n, "}");
    sendJson(c, json, "400 Bad Request");
}

void WebServer::settingsGet(WiFiClient& c) {
    LcdSettings ls;
    Settings::loadLcd(ls);
    VirtualLcdStatus s = lcd_.status();
    const PanelSize* saved = VirtualLcd::findPanel(ls.driver, ls.resolution);
    static char json[512 + kDriversJsonSize];  // tylko rdzen 0
    size_t n = snprintf(json, sizeof(json), "{\"driver\":");
    n = jsonString(json, sizeof(json), n, ls.driver);
    n += snprintf(json + n, sizeof(json) - n, ",\"res\":");
    n = jsonString(json, sizeof(json), n, saved ? saved->name : "");
    n += snprintf(json + n, sizeof(json) - n,
                  ",\"rot\":%u,\"scale\":%u,\"ignoreLed\":%s,\"led\":%u,\"ledPin\":7,\"current\":", ls.rotation,
                  ls.scale, ls.ignoreBacklight ? "true" : "false", s.backlightMeasured);
    n = jsonString(json, sizeof(json), n, s.driver);
    n += snprintf(json + n, sizeof(json) - n, ",\"currentRes\":");
    n = jsonString(json, sizeof(json), n, s.resolution);
    n += snprintf(json + n, sizeof(json) - n, ",\"drivers\":");
    n = driversJson(json, sizeof(json), n);
    append(json, sizeof(json), n, "}");
    sendJson(c, json);
}

void WebServer::settingsPost(WiFiClient& c, const char* body) {
    char value[16];
    LcdSettings ls;
    if (!formField(body, "driver", ls.driver, sizeof(ls.driver))) return sendError(c, "brak modelu LCD");
    bool known = false;
    for (size_t i = 0; i < VirtualLcd::driverCount() && !known; i++)
        known = strcasecmp(ls.driver, VirtualLcd::driverName(i)) == 0;
    if (!known) return sendError(c, "nieznany model LCD");
    if (!formField(body, "res", ls.resolution, sizeof(ls.resolution))) ls.resolution[0] = 0;
    const PanelSize* panel = VirtualLcd::findPanel(ls.driver, ls.resolution);
    if (ls.resolution[0] && strcmp(panel->name, ls.resolution) != 0) return sendError(c, "nieznana rozdzielczosc");
    strncpy(ls.resolution, panel->name, sizeof(ls.resolution) - 1);
    int rot = formField(body, "rot", value, sizeof(value)) ? atoi(value) : -1;
    int scale = formField(body, "scale", value, sizeof(value)) ? atoi(value) : -1;
    if (rot < 0 || rot >= 360 || rot % 90 != 0) return sendError(c, "bledny obrot");
    if (scale < 0 || scale > 4) return sendError(c, "bledna skala");
    ls.rotation = rot;
    ls.scale = scale;
    ls.ignoreBacklight = formField(body, "ignoreLed", value, sizeof(value)) && strcmp(value, "true") == 0;
    lcd_.setBacklightIgnored(ls.ignoreBacklight);
    VirtualLcdStatus now = lcd_.status();
    if (strcasecmp(now.driver, ls.driver) != 0 || strcmp(now.resolution, ls.resolution) != 0) {
        if (!lcd_.selectDriver(ls.driver, ls.resolution)) return sendError(c, "nie udalo sie przelaczyc sterownika");
        fullFrame_ = true;
    }
    if (!Settings::saveLcd(ls)) return sendError(c, "blad zapisu");
    Serial.printf("[web] Zapisano ustawienia: %s %s, obrot %d, skala %d\n", ls.driver, ls.resolution, rot, scale);
    sendJson(c, "{\"ok\":true}");
}

void WebServer::wifiGet(WiFiClient& c) {
    WifiSettings ws;
    bool set = Settings::loadWifi(ws);
    WifiStatus st = Wifi::status();
    static const char* const kSources[] = {"nvs", "secrets", "default"};
    char json[512];
    size_t n = snprintf(json, sizeof(json), "{\"saved\":{\"set\":%s,\"ap\":%s,\"hasPass\":%s,\"ssid\":",
                        set ? "true" : "false", ws.ap ? "true" : "false", ws.password[0] ? "true" : "false");
    n = jsonString(json, sizeof(json), n, ws.ssid);
    n += snprintf(json + n, sizeof(json) - n,
                  "},\"now\":{\"source\":\"%s\",\"sta\":%s,\"connected\":%s,\"ip\":\"%s\",\"rssi\":%d,"
                  "\"ap\":%s,\"apIp\":\"%s\",\"staSsid\":",
                  kSources[(int)st.source], st.staEnabled ? "true" : "false", st.staConnected ? "true" : "false",
                  st.staIp, st.rssi, st.apActive ? "true" : "false", st.apIp);
    n = jsonString(json, sizeof(json), n, st.staSsid);
    n += snprintf(json + n, sizeof(json) - n, ",\"apSsid\":");
    n = jsonString(json, sizeof(json), n, st.apSsid);
    snprintf(json + n, sizeof(json) - n, "}}");
    sendJson(c, json);
}

void WebServer::wifiPost(WiFiClient& c, const char* body) {
    char mode[8];
    WifiSettings ws;
    if (!formField(body, "mode", mode, sizeof(mode)) || (strcmp(mode, "ap") != 0 && strcmp(mode, "sta") != 0))
        return sendError(c, "bledny tryb");
    ws.ap = strcmp(mode, "ap") == 0;
    if (!formField(body, "ssid", ws.ssid, sizeof(ws.ssid)) || !ws.ssid[0])
        return sendError(c, "nazwa sieci: 1-32 znaki");
    if (!formField(body, "pass", ws.password, sizeof(ws.password))) ws.password[0] = 0;
    size_t pl = strlen(ws.password);
    if (pl && (pl < 8 || pl > 63)) return sendError(c, "haslo: puste albo 8-63 znaki");
    if (!Settings::saveWifi(ws)) return sendError(c, "blad zapisu");
    Serial.printf("[web] Zapisano WiFi: %s \"%s\", restart\n", ws.ap ? "AP" : "STA", ws.ssid);
    sendJson(c, "{\"ok\":true}");
    restartAt_ = millis() + 800;
}

void WebServer::scan(WiFiClient& c) {
    constexpr size_t kLen = 2048;
    char* json = (char*)malloc(kLen);
    if (!json) return sendError(c, "brak pamieci");
    if (Wifi::scanJson(json, kLen) < 0) sendError(c, "skanowanie niedostepne");
    else sendJson(c, json);
    free(json);
}

void WebServer::status(WiFiClient& c) {
    VirtualLcdStatus s = lcd_.status();
    char json[1400];
    snprintf(json, sizeof(json),
             "{\"driver\":\"%s\",\"on\":%s,\"inv\":%s,\"led\":%u,\"ledIgnored\":%s,\"cmds\":%lu,"
             "\"capture\":{\"bytes\":%lu,\"commands\":%lu,\"smRestarts\":%lu,\"resets\":%lu,\"overruns\":%lu,"
             "\"maxPending\":%lu,\"dmaRearms\":%lu,\"rxStalls\":%lu,\"loadPct\":%lu,\"peakLoadPct\":%lu,\"peakKBps\":%lu},"
             "\"web\":{\"clients\":%u,\"connects\":%lu,\"kicks\":%lu,\"silent\":%lu,\"ok\":%lu,\"err\":%lu,"
             "\"pixelKB\":%lu,\"wireKB\":%lu,\"ratio\":%.1f,\"sendKBps\":%lu,\"fullScreenMs\":%lu,\"fullScreenKB\":%lu},"
             "\"wifi\":{\"rssi\":%ld,\"ip\":\"%s\"},"
             "\"cpuMHz\":%lu,\"freeHeap\":%d,\"uptimeMs\":%lu}\n",
             s.driver, s.displayOn ? "true" : "false", s.inverted ? "true" : "false", s.backlightMeasured,
             s.backlightIgnored ? "true" : "false", (unsigned long)s.commands,
             (unsigned long)s.capture.bytes, (unsigned long)s.capture.commands, (unsigned long)s.capture.smRestarts,
             (unsigned long)s.capture.resets, (unsigned long)s.capture.overruns, (unsigned long)s.capture.maxPending,
             (unsigned long)s.capture.dmaRearms, (unsigned long)s.capture.rxStalls, (unsigned long)s.capture.loadPct,
             (unsigned long)s.capture.peakLoadPct, (unsigned long)s.capture.peakKBps, (unsigned)clientCount(), (unsigned long)connects_,
             (unsigned long)kicks_, (unsigned long)silentDrops_, (unsigned long)sentOk_, (unsigned long)sentErr_,
             (unsigned long)(pixelBytes_ / 1024), (unsigned long)(wireBytes_ / 1024),
             wireBytes_ ? (double)pixelBytes_ / wireBytes_ : 0.0,
             (unsigned long)(sendUs_ ? wireBytes_ * 1000 / sendUs_ : 0), (unsigned long)lastFullMs_,
             (unsigned long)lastFullKB_,
             (long)WiFi.RSSI(), WiFi.localIP().toString().c_str(), (unsigned long)(rp2040.f_cpu() / 1000000),
             rp2040.getFreeHeap(), (unsigned long)millis());
    // Doklejenie diagnostyki przeklaman: DISPOFF/SLPIN i miejsca restartow PIO.
    size_t n = strlen(json);
    if (n > 2 && json[n - 1] == '\n') n--;
    if (n > 1 && json[n - 1] == '}') {
        n--;
        n += snprintf(json + n, sizeof(json) - n, ",\"offCmds\":%lu,\"restartAt\":[", (unsigned long)s.offCommands);
        for (int i = 0; i < 25 && n < sizeof(json) - 16; i++)
            n += snprintf(json + n, sizeof(json) - n, "%s%lu", i ? "," : "", (unsigned long)s.capture.restartAt[i]);
        snprintf(json + n, sizeof(json) - n, "]}\n");
    }
    sendJson(c, json);
}

// --- WebSocket ---

size_t WebServer::clientCount() const {
    size_t n = 0;
    for (const auto& cl : clients_) n += cl.open;
    return n;
}

void WebServer::upgrade(WiFiClient& c, const char* key) {
    char src[128];
    snprintf(src, sizeof(src), "%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", key);
    uint8_t hash[20];
    sha1((const uint8_t*)src, strlen(src), hash);
    char accept[32];
    base64(hash, sizeof(hash), accept);
    c.printf("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
             "Sec-WebSocket-Accept: %s\r\n\r\n",
             accept);

    // Ta sama przegladarka (np. odswiezenie) - stare polaczenie nie jest potrzebne.
    uint32_t ip = ipOf(c);
    kickIp(ip, -1);
    // Wolne miejsce; gdy brak, rozlaczana jest najstarsza karta.
    size_t slot = kMaxClients;
    for (size_t i = 0; i < kMaxClients && slot == kMaxClients; i++)
        if (!clients_[i].open) slot = i;
    if (slot == kMaxClients) {
        slot = nextEvict_;
        nextEvict_ = (nextEvict_ + 1) % kMaxClients;
        closeClient(slot);
    }
    Client& cl = clients_[slot];
    cl.conn = c;
    cl.conn.setNoDelay(true);
    // Czas na zapis ramki do TCP. 200 ms bylo za malo: przy duzym ruchu (test, pelne klatki) WiFi potrafi
    // stac dluzej, niepelny zapis psuje strumien WebSocket i klient jest rozlaczany (licznik web.err).
    cl.conn.setTimeout(1500);
    cl.open = true;
    cl.ip = ip;
    cl.lastSeen = millis();
    connects_++;
    fullFrame_ = true;  // nowy klient potrzebuje calego obrazu (pozostali dostana go takze)
    Serial.printf("[web] Klient WebSocket polaczony, klientow: %u\n", (unsigned)clientCount());
}

void WebServer::kickIp(uint32_t ip, int keep) {
    static const char kBye[] = "{\"t\":\"bye\"}";
    for (size_t i = 0; i < kMaxClients; i++) {
        if (!clients_[i].open || (int)i == keep || clients_[i].ip != ip) continue;
        // Strona po "bye" nie laczy sie ponownie sama (inaczej dwie karty wyrzucalyby sie na zmiane).
        wsSend(i, 0x1, (const uint8_t*)kBye, sizeof(kBye) - 1);
        closeClient(i);
        kicks_++;
    }
}

void WebServer::closeClient(size_t i) {
    if (!clients_[i].open) return;
    clients_[i].conn.stop();
    clients_[i].open = false;
    Serial.printf("[web] Klient WebSocket rozlaczony, klientow: %u\n", (unsigned)clientCount());
}

void WebServer::readWs(size_t i) {
    WiFiClient& ws = clients_[i].conn;
    while (clients_[i].open && ws.available() >= 2) {
        uint8_t h[2];
        if (ws.readBytes(h, 2) != 2) return closeClient(i);
        uint8_t opcode = h[0] & 0x0F;
        uint64_t len = h[1] & 0x7F;
        if (len == 126) {
            uint8_t e[2];
            if (ws.readBytes(e, 2) != 2) return closeClient(i);
            len = e[0] << 8 | e[1];
        } else if (len == 127) {
            uint8_t e[8];
            if (ws.readBytes(e, 8) != 8) return closeClient(i);
            len = 0;
            for (int k = 0; k < 8; k++) len = len << 8 | e[k];
        }
        uint8_t mask[4] = {};
        if ((h[1] & 0x80) && ws.readBytes(mask, 4) != 4) return closeClient(i);

        char msg[128];
        size_t keep = len < sizeof(msg) - 1 ? (size_t)len : sizeof(msg) - 1;
        for (uint64_t k = 0; k < len; k++) {
            int b = -1;
            uint32_t start = millis();
            while ((b = ws.read()) < 0 && millis() - start < 200) {
            }
            if (b < 0) return closeClient(i);
            if (k < keep) msg[k] = (char)(b ^ mask[k & 3]);
        }
        msg[keep] = 0;
        clients_[i].lastSeen = millis();

        if (opcode == 0x8) return closeClient(i);
        if (opcode == 0x9) wsSend(i, 0xA, (const uint8_t*)msg, keep);  // ping -> pong
        if (opcode == 0x1) handleMessage(i, msg);
    }
}

// Zapis calej ramki mimo przestojow WiFi: niepelny zapis zepsulby strumien WebSocket, wiec dopisujemy reszte,
// dopoki polaczenie zyje (najwyzej kWriteDeadlineMs; pojedynczy write() czeka do setTimeout()).
static bool writeAll(WiFiClient& c, const uint8_t* p, size_t len) {
    constexpr uint32_t kWriteDeadlineMs = 5000;
    uint32_t start = millis();
    while (len) {
        size_t w = c.write(p, len);
        p += w;
        len -= w;
        if (len && (!c.connected() || millis() - start > kWriteDeadlineMs)) return false;
    }
    return true;
}

bool WebServer::wsSend(size_t i, uint8_t opcode, const uint8_t* payload, size_t len) {
    Client& cl = clients_[i];
    if (!cl.open) return false;
    uint8_t h[10];
    size_t hl = 0;
    h[hl++] = 0x80 | opcode;
    if (len < 126) {
        h[hl++] = len;
    } else if (len < 65536) {
        h[hl++] = 126;
        h[hl++] = len >> 8;
        h[hl++] = len;
    } else {
        h[hl++] = 127;
        for (int k = 7; k >= 0; k--) h[hl++] = (uint64_t)len >> (8 * k);
    }
    bool ok = writeAll(cl.conn, h, hl) && writeAll(cl.conn, payload, len);
    if (ok) {
        sentOk_++;
    } else {
        sentErr_++;
        closeClient(i);
    }
    return ok;
}

// Ta sama ramka do wszystkich klientow. false = nikt jej nie odebral.
bool WebServer::wsSendAll(uint8_t opcode, const uint8_t* payload, size_t len) {
    bool any = false;
    for (size_t i = 0; i < kMaxClients; i++)
        if (clients_[i].open) any |= wsSend(i, opcode, payload, len);
    return any;
}

void WebServer::handleMessage(size_t client, const char* msg) {
    if (strcmp(msg, "ping") == 0) {
        return;  // znak zycia - czas odnotowany w readWs
    } else if (strcmp(msg, "test") == 0) {
        lcd_.runTestPattern();
    } else if (strncmp(msg, "driver:", 7) == 0) {
        // "driver:ST7789" albo "driver:ST7789/135x240"
        char driver[16], res[16];
        splitDriver(msg + 7, driver, sizeof(driver), res, sizeof(res));
        if (lcd_.selectDriver(driver, res)) fullFrame_ = true;
    } else if (strncmp(msg, "loop:", 5) == 0) {
        if (!lcd_.loopback().start(msg + 5)) testSeq_ = 0;  // odeslij stary log - test nie wystartowal
    } else if (strncmp(msg, "demo:", 5) == 0) {
        // "demo:ST7789/135x240@20"
        char spec[40] = {}, driver[16], res[16];
        strncpy(spec, msg + 5, sizeof(spec) - 1);
        uint32_t mhz = 20;
        if (char* at = strchr(spec, '@')) {
            *at = 0;
            mhz = atoi(at + 1);
        }
        splitDriver(spec, driver, sizeof(driver), res, sizeof(res));
        if (!lcd_.demo().start(driver, res, mhz)) testSeq_ = 0;
    } else if (strcmp(msg, "hello") == 0) {
        sendDrivers(client);  // lista sterownikow tylko raz, przy polaczeniu
        fullFrame_ = true;
    } else if (strcmp(msg, "full") == 0) {
        fullFrame_ = true;
    }
}

// --- wysylanie obrazu i stanu ---

void WebServer::sendUpdates() {
    uint32_t now = millis();
    if (now - lastSend_ < kFrameIntervalMs) return;
    lastSend_ = now;

    if (fullFrame_) {
        fullFrame_ = false;
        lcd_.markAllDirty();
        lastInfo_ = 0;
        testSeq_ = UINT32_MAX;  // wyslij tez log testu
    }
    if (lastInfo_ == 0 || now - lastInfo_ >= 1000) {
        lastInfo_ = now ? now : 1;
        if (!sendInfo()) return;
    }
    if (lcd_.testLog().seq() != testSeq_ && !sendTestLog()) return;
    Rect r;
    if (lcd_.takeDirty(r) && !sendRegion(r)) lcd_.markAllDirty();
}

bool WebServer::sendInfo() {
    VirtualLcdStatus s = lcd_.status();
    char json[384];
    size_t n = snprintf(json, sizeof(json),
                        "{\"t\":\"info\",\"driver\":\"%s\",\"res\":\"%s\",\"w\":%u,\"h\":%u,\"round\":%s,\"on\":%s,\"inv\":%s,\"bl\":%u,"
                        "\"cmds\":%lu,\"bytes\":%lu,\"ovr\":%lu,\"resync\":%lu,\"resets\":%lu}",
                        s.driver, s.resolution, s.width, s.height, s.round ? "true" : "false", s.displayOn ? "true" : "false",
                        s.inverted ? "true" : "false", s.backlight, (unsigned long)s.commands,
                        (unsigned long)s.capture.bytes,
                        (unsigned long)s.capture.overruns, (unsigned long)s.capture.smRestarts,
                        (unsigned long)s.capture.resets);
    return wsSendAll(0x1, (const uint8_t*)json, n);
}

// {"t":"drivers","drivers":[...]} - lista sterownikow i rozdzielczosci, wysylana raz po "hello".
bool WebServer::sendDrivers(size_t client) {
    static char json[64 + kDriversJsonSize];  // tylko rdzen 0
    size_t n = append(json, sizeof(json), 0, "{\"t\":\"drivers\",\"drivers\":");
    n = driversJson(json, sizeof(json), n);
    n = append(json, sizeof(json), n, "}");
    return wsSend(client, 0x1, (const uint8_t*)json, n);
}

bool WebServer::sendTestLog() {
    testSeq_ = lcd_.testLog().seq();
    char log[1024];
    lcd_.testLog().copy(log, sizeof(log));
    // JSON: {"t":"test","run":bool,"en":bool,"log":"..."} z ucieczka znakow
    static char json[2200];
    int n = snprintf(json, sizeof(json), "{\"t\":\"test\",\"run\":%s,\"en\":%s,\"log\":\"",
                     lcd_.testLog().running() ? "true" : "false", lcd_.loopback().enabled() ? "true" : "false");
    for (const char* p = log; *p && n < (int)sizeof(json) - 8; p++) {
        if (*p == '"' || *p == '\\') {
            json[n++] = '\\';
            json[n++] = *p;
        } else if (*p == '\n') {
            json[n++] = '\\';
            json[n++] = 'n';
        } else if ((uint8_t)*p >= 0x20) {
            json[n++] = *p;
        }
    }
    n += snprintf(json + n, sizeof(json) - n, "\"}");
    return wsSendAll(0x1, (const uint8_t*)json, n);
}

bool WebServer::sendRegion(const Rect& r) {
    const uint32_t t0 = time_us_32();
    uint32_t wire = 0;
    const uint16_t w = r.width();
    uint16_t rowsPerFrame = kMaxPixelsPerFrame / w;
    if (rowsPerFrame == 0) rowsPerFrame = 1;
    for (uint32_t y = r.y0; y <= r.y1; y += rowsPerFrame) {
        uint16_t y1 = y + rowsPerFrame - 1 > r.y1 ? r.y1 : y + rowsPerFrame - 1;
        Rect part = {r.x0, (uint16_t)y, r.x1, y1};
        uint16_t header[4] = {part.x0, part.y0, part.width(), part.height()};
        const size_t count = (size_t)part.width() * part.height();
        uint16_t* pixels = (uint16_t*)(sendBuf_ + sizeof(header));
        lcd_.copyView(part, pixels);
        // Kompresja RLE, gdy cos daje (zdjecia i gradienty ida bez zmian).
        size_t words = rleEncode(pixels, count, (uint16_t*)(rleBuf_ + sizeof(header)));
        const uint8_t* frame = sendBuf_;
        size_t len = sizeof(header) + count * 2;
        if (words < count) {
            header[0] |= kRleFlag;
            memcpy(rleBuf_, header, sizeof(header));
            frame = rleBuf_;
            len = sizeof(header) + words * 2;
        } else {
            memcpy(sendBuf_, header, sizeof(header));
        }
        pixelBytes_ += count * 2;
        wireBytes_ += len;
        wire += len;
        if (!wsSendAll(0x2, frame, len)) return false;
    }
    const uint32_t us = time_us_32() - t0;
    sendUs_ += us;
    const VirtualLcdStatus s = lcd_.status();
    if ((uint32_t)r.width() * r.height() * 4 >= (uint32_t)s.width * s.height * 3) {
        lastFullMs_ = (us + 500) / 1000;
        lastFullKB_ = (wire + 512) / 1024;
    }
    return true;
}
