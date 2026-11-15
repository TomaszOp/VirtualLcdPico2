#pragma once

#include <stddef.h>
#include <stdint.h>

#include "drivers/LcdDriver.h"
#include "hardware/spi.h"

// Piny nadajnika testowego - musza byc inne niz piny emulatora, laczy sie je przewodami.
// SCK i MOSI musza nalezec do jednego sprzetowego SPI (np. SPI1: SCK GP10, TX GP11).
struct LoopbackPins {
    int sck;   // polaczyc z SpiPins.sck
    int mosi;  // z SpiPins.mosi
    int dc;    // z SpiPins.dc
    int cs;    // z SpiPins.cs
    int rst;   // z SpiPins.rst, -1 = brak
};

struct LcdInitCmd {
    uint8_t cmd;
    uint8_t len;
    uint8_t params[16];  // najdluzsze: gamma ST7735 (0xE0, 0xE1)
    uint16_t delayMs;
};

// Sekwencja startowa i ustawienia typowej biblioteki dla danego sterownika.
struct LcdProfile {
    const char* driver;
    const LcdInitCmd* init;
    size_t initCount;
    uint16_t width;
    uint16_t height;
    uint8_t madctlRot0;  // MADCTL dla rotacji 0 (jak w bibliotece Adafruit)
    uint8_t madctlRot1;  // MADCTL dla rotacji 1 (pozioma, obrot w prawo)

    static const LcdProfile* find(const char* driver);
    // MADCTL z bitem BGR wg panelu (gdy panel go okresla), jak ustawilaby biblioteka.
    static uint8_t madctlFor(uint8_t madctl, const PanelSize& panel);
    static size_t count();
    static const LcdProfile& at(size_t i);
};

// Nadajnik (sprzetowe SPI w trybie master) udajacy biblioteke LCD na mikrokontrolerze z programem.
// Transmisja w stylu TFT_eSPI: CS trzymany nisko przez cala sekwencje,
// D/C przelaczany miedzy komenda a danymi.
class SpiLcdMaster {
public:
    static constexpr size_t kTxBufSize = 4096;

    ~SpiLcdMaster() { close(); }

    // Zwraca rzeczywisty zegar SPI (dzielnik z zegara peryferiow) albo 0 przy bledzie.
    uint32_t open(const LoopbackPins& pins, uint32_t clockHz);
    void close();

    // Wywolywane miedzy porcjami danych i w czasie czekania - rdzen 0 obsluguje wtedy WiFi i strone.
    static void setIdleHook(void (*hook)(void*), void* arg) {
        idleHook_ = hook;
        idleArg_ = arg;
    }
    static void idle() {
        if (idleHook_) idleHook_(idleArg_);
    }
    static void waitMs(uint32_t ms);

    void hardwareReset();  // RST, potem CS nisko
    void init(const LcdProfile& p, const PanelSize& panel);
    void select(bool active);  // CS

    void command(uint8_t cmd, const uint8_t* params = nullptr, size_t len = 0);
    void data(const uint8_t* bytes, size_t len);
    // Przesuniecie okna panelu w pamieci sterownika, dodawane w window() (jak colstart/rowstart w bibliotekach).
    void setOrigin(uint16_t x, uint16_t y) {
        originX_ = x;
        originY_ = y;
    }
    void window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
    void fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
    // Piksele RGB565 (zamiana na big-endian przy wysylce).
    void pushPixels(const uint16_t* pixels, size_t count);
    void pushImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t* pixels);

    uint8_t* txBuf() { return txBuf_; }
    uint32_t bytesSent() const { return bytesSent_; }

private:
    LoopbackPins pins_ = {-1, -1, -1, -1, -1};
    spi_inst_t* spi_ = nullptr;
    uint8_t* txBuf_ = nullptr;
    static void (*idleHook_)(void*);
    static void* idleArg_;
    uint32_t bytesSent_ = 0;
    uint16_t originX_ = 0, originY_ = 0;
};
