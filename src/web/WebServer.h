#pragma once

#include <WiFi.h>
#include <stddef.h>
#include <stdint.h>

#include "FrameBuffer.h"

class VirtualLcd;

// Serwer WWW na WiFiServer (lwIP): strony "/" (podglad), "/lcd" (ustawienia wyswietlacza), "/wifi", "/test",
// API ustawien "/api/...", diagnostyka "/status" i WebSocket "/ws" dla kilku kart przegladarki naraz
// (te same dane dla wszystkich). Protokol jak w wersji ESP32-S3, wiec strony sa te same:
//   obraz: binarne ramki [x, y, w, h (uint16 LE)] + piksele RGB565 LE, tylko zmieniony obszar;
//          bit 14 w x (kRleFlag) = piksele skompresowane RLE (format w WebServer.cpp, rleEncode),
//   stan:  tekstowe ramki JSON co sekunde i po kazdej zmianie.
// Wszystko dziala na rdzeniu 0 w poll() - wywolywanym z petli i w trakcie testow.
// Strony sa po polsku albo angielsku - jezyk wybrany przelacznikiem w gornej belce jest zapisany w flashu
// (Settings::saveUi) i wczytywany w begin(), wiec juz pierwsza strona po starcie jest w zapisanym jezyku.
class WebServer {
public:
    explicit WebServer(VirtualLcd& lcd) : lcd_(lcd) {}

    bool begin(uint16_t port);
    void poll();

private:
    static constexpr size_t kMaxPixelsPerFrame = 8 * 1024;  // 16 KB pikseli w jednej ramce WS
    static constexpr uint32_t kFrameIntervalMs = 40;
    static constexpr uint16_t kRleFlag = 0x4000;
    static constexpr size_t kMaxClients = 3;
    // Strony wysylaja "ping" co 2 s; klient milczacy dluzej jest uznawany za martwy (np. karta zamknieta bez
    // pozegnania) - inaczej wysylanie do niego blokowaloby pozostalych.
    static constexpr uint32_t kClientTimeoutMs = 8000;

    struct Client {
        WiFiClient conn;
        bool open = false;
        uint32_t ip = 0;
        uint32_t lastSeen = 0;
    };

    struct Request {
        char method[8];
        char path[64];
        char wsKey[64];
        bool upgrade;
        char body[512];
    };

    void handleHttp(WiFiClient& c);
    bool readRequest(WiFiClient& c, Request& r);
    void sendPage(WiFiClient& c, const char* html);
    void sendJson(WiFiClient& c, const char* json, const char* status = "200 OK");
    void sendError(WiFiClient& c, const char* message);
    void settingsGet(WiFiClient& c);
    void settingsPost(WiFiClient& c, const char* body);
    void wifiGet(WiFiClient& c);
    void wifiPost(WiFiClient& c, const char* body);
    void scan(WiFiClient& c);
    void status(WiFiClient& c);
    void langGet(WiFiClient& c);
    void langPost(WiFiClient& c, const char* body);
    // Tekst w jezyku stron (komunikaty bledow API).
    const char* tr(const char* pl, const char* en) const { return english_ ? en : pl; }

    void upgrade(WiFiClient& c, const char* key);
    // Zamyka polaczenia WebSocket z adresu ip (z wyjatkiem keep): stara karta po odswiezeniu lub przejsciu
    // na inna strone tej samej przegladarki.
    void kickIp(uint32_t ip, int keep);
    void closeClient(size_t i);
    size_t clientCount() const;
    void readWs(size_t i);
    void handleMessage(size_t client, const char* msg);
    void sendUpdates();
    bool sendInfo();
    bool sendDrivers(size_t client);
    bool sendTestLog();
    bool sendRegion(const Rect& r);
    bool wsSend(size_t i, uint8_t opcode, const uint8_t* payload, size_t len);
    bool wsSendAll(uint8_t opcode, const uint8_t* payload, size_t len);

    VirtualLcd& lcd_;
    WiFiServer* server_ = nullptr;
    Client clients_[kMaxClients];
    size_t nextEvict_ = 0;
    bool inPoll_ = false;
    bool fullFrame_ = false;
    bool english_ = false;  // jezyk stron: z UiSettings, zmieniany przez POST /api/lang
    uint32_t lastSend_ = 0;
    uint32_t lastInfo_ = 0;
    uint32_t testSeq_ = 0;
    uint32_t restartAt_ = 0;  // != 0: restart po wyslaniu odpowiedzi (zmiana WiFi)
    uint8_t* sendBuf_ = nullptr;
    uint8_t* rleBuf_ = nullptr;  // naglowek + skompresowane piksele (najgorszy przypadek nieco wiekszy niz surowe)

    // Diagnostyka
    uint32_t sentOk_ = 0;
    uint32_t sentErr_ = 0;
    // Przesyl obrazu: bajty pikseli przed i po kompresji, czas wysylania, ostatni pelny ekran.
    uint64_t pixelBytes_ = 0;
    uint64_t wireBytes_ = 0;
    uint64_t sendUs_ = 0;
    uint32_t lastFullMs_ = 0;   // czas wyslania ostatniego obszaru >= 3/4 ekranu
    uint32_t lastFullKB_ = 0;   // jego rozmiar po kompresji
    uint32_t connects_ = 0;
    uint32_t kicks_ = 0;
    uint32_t silentDrops_ = 0;
};
