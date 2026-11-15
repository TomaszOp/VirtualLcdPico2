#pragma once

#include <memory>

#include "Backlight.h"
#include "FrameBuffer.h"
#include "capture/PioSpiCapture.h"
#include "drivers/LcdDriver.h"
#include "pico/mutex.h"
#include "test/DemoSender.h"
#include "test/LoopbackTester.h"
#include "test/TestLog.h"
#include "web/WebServer.h"

struct VirtualLcdConfig {
    SpiPins pins;
    const char* driver;      // nazwa sterownika, np. "ILI9341"
    const char* resolution;  // rozdzielczosc panelu z listy sterownika, np. "135x240"; pusta = pierwsza
    uint16_t webPort;
    int backlightPin;        // pin LED wyswietlacza (nieparzysty GPIO), -1 = brak
    bool ignoreBacklight;    // pomin pin LED - podswietlenie zawsze 100 %
    LoopbackPins loopbackPins;  // nadajnik testow (test petli, demo); sck = -1: wylaczony
};

struct VirtualLcdStatus {
    const char* driver;
    const char* resolution;
    uint16_t width;   // widoczne okno panelu
    uint16_t height;
    uint16_t offsetX;  // polozenie okna w pamieci obrazu sterownika
    uint16_t offsetY;
    bool round;  // okragly panel
    bool displayOn;
    bool inverted;
    uint8_t backlight;          // jasnosc podgladu 0..100 % (100, gdy pin LED pomijany)
    uint8_t backlightMeasured;  // zmierzona na pinie LED
    bool backlightIgnored;
    uint32_t commands;
    uint32_t offCommands;  // odebrane DISPOFF (0x28) i SLPIN (0x10) - diagnostyka przeklaman
    SpiCaptureStats capture;
};

// Centralny punkt: wybiera sterownik, laczy przechwytywanie SPI z buforem obrazu
// i udostepnia go serwerowi WWW.
//
// Podzial rdzeni RP2350:
//   rdzen 0 - WiFi, serwer WWW, testy (beginCore0 / loopCore0),
//   rdzen 1 - tylko przechwytywanie i dekodowanie SPI (beginCore1 / loopCore1).
class VirtualLcd : public SpiSink {
public:
    VirtualLcd();

    bool beginCore0(const VirtualLcdConfig& config);
    void loopCore0();
    bool beginCore1();
    void loopCore1();

    // Zmiana emulowanego sterownika i rozdzielczosci w locie (wykonuje ja rdzen 1 miedzy porcjami danych).
    // resolution pusta/nullptr albo nieznana = pierwsza z listy sterownika.
    bool selectDriver(const char* name, const char* resolution = nullptr);
    static size_t driverCount();
    static const char* driverName(size_t index);
    static size_t panelCount(size_t driverIndex);
    static const PanelSize& panel(size_t driverIndex, size_t panelIndex);
    // Rozdzielczosc z listy sterownika; nullptr = nieznany sterownik. Nieznana rozdzielczosc = pierwsza.
    static const PanelSize* findPanel(const char* driver, const char* resolution);

    VirtualLcdStatus status();
    // Dla WWW: tylko widoczne okno panelu, wspolrzedne wzgledem okna.
    bool takeDirty(Rect& out);
    void markAllDirty() { frameBuffer_.markAllDirty(); }
    void copyView(const Rect& r, uint16_t* dst);
    // Cala pamiec obrazu sterownika (test petli zwrotnej).
    void copyRegion(const Rect& r, uint16_t* dst);

    // Obraz testowy wyslany przez aktualny sterownik, bez drugiego mikrokontrolera.
    void runTestPattern();
    void setBacklightIgnored(bool ignore) { ignoreBacklight_ = ignore; }
    LoopbackTester& loopback() { return loopback_; }
    DemoSender& demo() { return demo_; }
    TestLog& testLog() { return testLog_; }
    WebServer& web() { return web_; }

    // SpiSink - wywolania tylko z rdzenia 1
    void onReset() override;
    void onCommand(uint8_t cmd) override;
    void onData(const uint8_t* bytes, size_t len) override;

private:
    enum class Request : uint8_t { None, SelectDriver, TestPattern };

    bool selectDriverLocked(const char* name, const char* resolution);
    bool request(Request r, const char* driverName);
    void serviceRequest();

    VirtualLcdConfig config_ = {};
    mutex_t lock_;  // tylko na czas zmiany sterownika (bufor obrazu moze zmienic rozmiar)
    std::unique_ptr<LcdDriver> driver_;
    const PanelSize* panel_ = nullptr;
    FrameBuffer frameBuffer_;
    PioSpiCapture capture_;
    Backlight backlight_;
    volatile bool ignoreBacklight_ = true;
    WebServer web_;
    TestLog testLog_;
    LoopbackTester loopback_;
    DemoSender demo_;
    uint32_t commands_ = 0;
    uint32_t offCommands_ = 0;
    volatile bool core1Ready_ = false;
    volatile Request request_ = Request::None;
    char requestDriver_[16] = {};
    char requestRes_[16] = {};
    volatile bool requestResult_ = false;
};
