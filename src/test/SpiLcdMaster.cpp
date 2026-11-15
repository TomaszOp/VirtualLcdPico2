#include "SpiLcdMaster.h"

#include <string.h>
#include <strings.h>

#include <Arduino.h>
#include <stdlib.h>

#include "drivers/Gc9a01Driver.h"
#include "drivers/Ili9341Driver.h"
#include "drivers/St7735Driver.h"
#include "drivers/St7735RDriver.h"
#include "drivers/St7735SDriver.h"
#include "drivers/St7789Driver.h"
#include "hardware/gpio.h"

// Sekwencja jak w Adafruit_ILI9341 (komendy producenta testuja pomijanie parametrow).
static const LcdInitCmd kIli9341Init[] = {
    {0x01, 0, {}, 150},                                  // SWRESET
    {0xEF, 3, {0x03, 0x80, 0x02}, 0},
    {0xCF, 3, {0x00, 0xC1, 0x30}, 0},
    {0xED, 4, {0x64, 0x03, 0x12, 0x81}, 0},
    {0xE8, 3, {0x85, 0x00, 0x78}, 0},
    {0xCB, 5, {0x39, 0x2C, 0x00, 0x34, 0x02}, 0},
    {0xF7, 1, {0x20}, 0},
    {0xEA, 2, {0x00, 0x00}, 0},
    {0xC0, 1, {0x23}, 0},                                // PWCTR1
    {0xC1, 1, {0x10}, 0},                                // PWCTR2
    {0xC5, 2, {0x3E, 0x28}, 0},                          // VMCTR1
    {0xC7, 1, {0x86}, 0},                                // VMCTR2
    {0x36, 1, {0x48}, 0},                                // MADCTL = MX | BGR
    {0x37, 1, {0x00}, 0},                                // VSCRSADD
    {0x3A, 1, {0x55}, 0},                                // COLMOD 16 bit
    {0xB1, 2, {0x00, 0x18}, 0},                          // FRMCTR1
    {0xB6, 3, {0x08, 0x82, 0x27}, 0},                    // DFUNCTR
    {0xF2, 1, {0x00}, 0},
    {0x26, 1, {0x01}, 0},                                // GAMMASET
    {0xE0, 15, {0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00}, 0},
    {0xE1, 15, {0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F}, 0},
    {0x11, 0, {}, 150},                                  // SLPOUT
    {0x29, 0, {}, 150},                                  // DISPON
};

// Sekwencja jak w Adafruit_ST7789.
static const LcdInitCmd kSt7789Init[] = {
    {0x01, 0, {}, 150},                                  // SWRESET
    {0x11, 0, {}, 10},                                   // SLPOUT
    {0x3A, 1, {0x55}, 10},                               // COLMOD 16 bit
    {0x36, 1, {0x00}, 0},                                // MADCTL
    {0x2A, 4, {0x00, 0x00, 0x00, 0xF0}, 0},              // CASET
    {0x2B, 4, {0x00, 0x00, 0x01, 0x40}, 0},              // RASET
    {0x21, 0, {}, 10},                                   // INVON
    {0x13, 0, {}, 10},                                   // NORON
    {0x29, 0, {}, 10},                                   // DISPON
};

// Sekwencja jak w Adafruit_GC9A01A: odblokowanie rejestrow producenta (0xFE, 0xEF) i ich ustawienia
// (tu skrocone, parametry przykladowe - emulator je pomija), MADCTL = MX | BGR, COLMOD 16 bit, INVON.
static const LcdInitCmd kGc9a01Init[] = {
    {0xEF, 0, {}, 0},                                    // INREGEN2
    {0xEB, 1, {0x14}, 0},
    {0xFE, 0, {}, 0},                                    // INREGEN1
    {0xEF, 0, {}, 0},
    {0xEB, 1, {0x14}, 0},
    {0x84, 1, {0x40}, 0},
    {0x88, 1, {0x0A}, 0},
    {0x89, 1, {0x21}, 0},
    {0x8B, 1, {0x80}, 0},
    {0xB6, 2, {0x00, 0x00}, 0},                          // Display Function Control (SS = GS = 0)
    {0x36, 1, {0x48}, 0},                                // MADCTL = MX | BGR
    {0x3A, 1, {0x05}, 0},                                // COLMOD 16 bit
    {0x90, 4, {0x08, 0x08, 0x08, 0x08}, 0},
    {0xBD, 1, {0x06}, 0},
    {0xFF, 3, {0x60, 0x01, 0x04}, 0},
    {0xC3, 1, {0x13}, 0},                                // napiecia
    {0xC4, 1, {0x13}, 0},
    {0xC9, 1, {0x22}, 0},
    {0xF0, 6, {0x45, 0x09, 0x08, 0x08, 0x26, 0x2A}, 0},  // gamma
    {0xF1, 6, {0x43, 0x70, 0x72, 0x36, 0x37, 0x6F}, 0},
    {0x62, 12, {0x18, 0x0D, 0x71, 0xED, 0x70, 0x70, 0x18, 0x0F, 0x71, 0xEF, 0x70, 0x70}, 0},
    {0x35, 0, {}, 0},                                    // TEON
    {0x21, 0, {}, 0},                                    // INVON (panel IPS)
    {0x11, 0, {}, 120},                                  // SLPOUT
    {0x29, 0, {}, 20},                                   // DISPON
};

// ST7735: okno (CASET/RASET), BGR i INVON zaleza od panelu - test ustawia je po sekwencji (SpiLcdMaster::init).

// ST7735 - jak Adafruit initB() (Bcmd).
static const LcdInitCmd kSt7735Init[] = {
    {0x01, 0, {}, 50},                                   // SWRESET
    {0x11, 0, {}, 120},                                  // SLPOUT
    {0x3A, 1, {0x05}, 10},                               // COLMOD 16 bit
    {0xB1, 3, {0x00, 0x06, 0x03}, 10},                   // FRMCTR1
    {0x36, 1, {0xC8}, 0},                                // MADCTL = MX | MY | BGR
    {0xB6, 2, {0x15, 0x02}, 0},                          // DISSET5 (w ST7735 nie jest to kierunek skanowania)
    {0xB4, 1, {0x00}, 0},                                // INVCTR
    {0xC0, 2, {0x02, 0x70}, 10},                         // PWCTR1
    {0xC1, 1, {0x05}, 0},                                // PWCTR2
    {0xC2, 2, {0x01, 0x02}, 0},                          // PWCTR3
    {0xC5, 2, {0x3C, 0x38}, 10},                         // VMCTR1
    {0xFC, 2, {0x11, 0x15}, 0},                          // PWCTR6
    {0xE0, 16, {0x09, 0x16, 0x09, 0x20, 0x21, 0x1B, 0x13, 0x19, 0x17, 0x15, 0x1E, 0x2B, 0x04, 0x05, 0x02, 0x0E}, 0},
    {0xE1, 16, {0x0B, 0x14, 0x08, 0x1E, 0x22, 0x1D, 0x18, 0x1E, 0x1B, 0x1A, 0x24, 0x2B, 0x06, 0x06, 0x02, 0x0F}, 10},
    {0x13, 0, {}, 10},                                   // NORON
    {0x29, 0, {}, 100},                                  // DISPON
};

// ST7735R - jak Adafruit initR() (Rcmd1 + Rcmd3).
static const LcdInitCmd kSt7735RInit[] = {
    {0x01, 0, {}, 150},                                  // SWRESET
    {0x11, 0, {}, 120},                                  // SLPOUT
    {0xB1, 3, {0x01, 0x2C, 0x2D}, 0},                    // FRMCTR1..3
    {0xB2, 3, {0x01, 0x2C, 0x2D}, 0},
    {0xB3, 6, {0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D}, 0},
    {0xB4, 1, {0x07}, 0},                                // INVCTR
    {0xC0, 3, {0xA2, 0x02, 0x84}, 0},                    // PWCTR1..5
    {0xC1, 1, {0xC5}, 0},
    {0xC2, 2, {0x0A, 0x00}, 0},
    {0xC3, 2, {0x8A, 0x2A}, 0},
    {0xC4, 2, {0x8A, 0xEE}, 0},
    {0xC5, 1, {0x0E}, 0},                                // VMCTR1
    {0x20, 0, {}, 0},                                    // INVOFF
    {0x36, 1, {0xC8}, 0},                                // MADCTL = MX | MY | BGR
    {0x3A, 1, {0x05}, 0},                                // COLMOD 16 bit
    {0xE0, 16, {0x02, 0x1C, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2D, 0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10}, 0},
    {0xE1, 16, {0x03, 0x1D, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D, 0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10}, 0},
    {0x13, 0, {}, 10},                                   // NORON
    {0x29, 0, {}, 100},                                  // DISPON
};

// ST7735S - typowa sekwencja producenta (moduly z Chin, przyklady do 0,96" 80x160).
static const LcdInitCmd kSt7735SInit[] = {
    {0x11, 0, {}, 120},                                  // SLPOUT
    {0xB1, 3, {0x05, 0x3C, 0x3C}, 0},                    // FRMCTR1..3
    {0xB2, 3, {0x05, 0x3C, 0x3C}, 0},
    {0xB3, 6, {0x05, 0x3C, 0x3C, 0x05, 0x3C, 0x3C}, 0},
    {0xB4, 1, {0x03}, 0},                                // INVCTR
    {0xC0, 3, {0x28, 0x08, 0x04}, 0},                    // PWCTR1..5
    {0xC1, 1, {0xC0}, 0},
    {0xC2, 2, {0x0D, 0x00}, 0},
    {0xC3, 2, {0x8D, 0x2A}, 0},
    {0xC4, 2, {0x8D, 0xEE}, 0},
    {0xC5, 1, {0x1A}, 0},                                // VMCTR1
    {0x36, 1, {0xC8}, 0},                                // MADCTL = MX | MY | BGR
    {0xE0, 16, {0x04, 0x22, 0x07, 0x0A, 0x2E, 0x30, 0x25, 0x2A, 0x28, 0x26, 0x2E, 0x3A, 0x00, 0x01, 0x03, 0x13}, 0},
    {0xE1, 16, {0x04, 0x16, 0x06, 0x0D, 0x2D, 0x26, 0x23, 0x27, 0x27, 0x25, 0x2D, 0x3B, 0x00, 0x01, 0x04, 0x13}, 0},
    {0x3A, 1, {0x05}, 0},                                // COLMOD 16 bit
    {0x29, 0, {}, 20},                                   // DISPON
};

static const LcdProfile kProfiles[] = {
    {Ili9341Driver::kName, kIli9341Init, sizeof(kIli9341Init) / sizeof(LcdInitCmd), 240, 320, 0x48, 0x28},
    {St7789Driver::kName, kSt7789Init, sizeof(kSt7789Init) / sizeof(LcdInitCmd), 240, 320, 0x00, 0x60},
    {Gc9a01Driver::kName, kGc9a01Init, sizeof(kGc9a01Init) / sizeof(LcdInitCmd), 240, 240, 0x48, 0x28},
    // ST7735: rotacja 1 = MY | MV (+ BGR) jak w Adafruit
    {St7735Driver::kName, kSt7735Init, sizeof(kSt7735Init) / sizeof(LcdInitCmd), 132, 162, 0xC8, 0xA8},
    {St7735RDriver::kName, kSt7735RInit, sizeof(kSt7735RInit) / sizeof(LcdInitCmd), 132, 162, 0xC8, 0xA8},
    {St7735SDriver::kName, kSt7735SInit, sizeof(kSt7735SInit) / sizeof(LcdInitCmd), 132, 162, 0xC8, 0xA8},
};

const LcdProfile* LcdProfile::find(const char* driver) {
    for (const auto& p : kProfiles)
        if (strcasecmp(p.driver, driver) == 0) return &p;
    return nullptr;
}

size_t LcdProfile::count() { return sizeof(kProfiles) / sizeof(kProfiles[0]); }

const LcdProfile& LcdProfile::at(size_t i) { return kProfiles[i]; }

void (*SpiLcdMaster::idleHook_)(void*) = nullptr;
void* SpiLcdMaster::idleArg_ = nullptr;

void SpiLcdMaster::waitMs(uint32_t ms) {
    uint32_t start = millis();
    do {
        idle();
        sleep_us(200);
    } while (millis() - start < ms);
}

uint32_t SpiLcdMaster::open(const LoopbackPins& pins, uint32_t clockHz) {
    close();
    pins_ = pins;
    bytesSent_ = 0;
    originX_ = originY_ = 0;
    txBuf_ = (uint8_t*)malloc(kTxBufSize);
    if (!txBuf_) return 0;

    // GP0-7 i GP16-23 to SPI0, GP8-15 i GP24-29 to SPI1.
    spi_ = ((pins.sck >> 3) & 1) ? spi1 : spi0;
    uint32_t actual = spi_init(spi_, clockHz);
    spi_set_format(spi_, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(pins.sck, GPIO_FUNC_SPI);
    gpio_set_function(pins.mosi, GPIO_FUNC_SPI);

    const int outs[] = {pins.dc, pins.cs, pins.rst};
    for (int p : outs) {
        if (p < 0) continue;
        gpio_init(p);
        gpio_set_dir(p, GPIO_OUT);
        gpio_put(p, 1);
    }
    return actual;
}

void SpiLcdMaster::close() {
    if (spi_) {
        spi_deinit(spi_);
        spi_ = nullptr;
    }
    if (pins_.sck >= 0) {
        const int all[] = {pins_.sck, pins_.mosi, pins_.dc, pins_.cs, pins_.rst};
        for (int p : all) {
            if (p < 0) continue;
            gpio_init(p);  // z powrotem wejscie, zeby nie przeszkadzac innemu nadajnikowi
            gpio_set_dir(p, GPIO_IN);
        }
        pins_ = {-1, -1, -1, -1, -1};
    }
    free(txBuf_);
    txBuf_ = nullptr;
}

void SpiLcdMaster::hardwareReset() {
    if (pins_.rst >= 0) {
        gpio_put(pins_.rst, 0);
        waitMs(10);
        gpio_put(pins_.rst, 1);
        waitMs(120);
    }
    select(true);
}

uint8_t LcdProfile::madctlFor(uint8_t madctl, const PanelSize& panel) {
    if (panel.bgr < 0) return madctl;
    return panel.bgr ? madctl | 0x08 : madctl & ~0x08;
}

void SpiLcdMaster::init(const LcdProfile& p, const PanelSize& panel) {
    for (size_t i = 0; i < p.initCount; i++) {
        command(p.init[i].cmd, p.init[i].params, p.init[i].len);
        if (p.init[i].delayMs) waitMs(p.init[i].delayMs);
    }
    // Cechy panelu (ST7735: ten sam uklad z roznymi szybami) - tak jak biblioteka po wyborze modulu.
    if (panel.bgr >= 0) {
        uint8_t madctl = LcdProfile::madctlFor(p.madctlRot0, panel);
        command(0x36, &madctl, 1);
    }
    if (panel.inverted >= 0) command(panel.inverted ? 0x21 : 0x20);
}

void SpiLcdMaster::select(bool active) { gpio_put(pins_.cs, active ? 0 : 1); }

void SpiLcdMaster::command(uint8_t cmd, const uint8_t* params, size_t len) {
    gpio_put(pins_.dc, 0);
    spi_write_blocking(spi_, &cmd, 1);  // czeka, az bajt wyjdzie - dopiero potem zmiana D/C
    bytesSent_++;
    gpio_put(pins_.dc, 1);
    if (len) data(params, len);
}

void SpiLcdMaster::data(const uint8_t* bytes, size_t len) {
    while (len) {
        size_t n = len < kTxBufSize ? len : kTxBufSize;
        spi_write_blocking(spi_, bytes, n);
        bytesSent_ += n;
        bytes += n;
        len -= n;
        if (len) idle();
    }
}

void SpiLcdMaster::window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    x0 += originX_;
    x1 += originX_;
    y0 += originY_;
    y1 += originY_;
    uint8_t ca[4] = {(uint8_t)(x0 >> 8), (uint8_t)x0, (uint8_t)(x1 >> 8), (uint8_t)x1};
    uint8_t ra[4] = {(uint8_t)(y0 >> 8), (uint8_t)y0, (uint8_t)(y1 >> 8), (uint8_t)y1};
    command(0x2A, ca, 4);
    command(0x2B, ra, 4);
}

void SpiLcdMaster::fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if (w == 0 || h == 0) return;
    window(x, y, x + w - 1, y + h - 1);
    command(0x2C);
    size_t total = (size_t)w * h * 2;
    size_t fill = total < kTxBufSize ? total : kTxBufSize;
    for (size_t i = 0; i < fill; i += 2) {
        txBuf_[i] = color >> 8;
        txBuf_[i + 1] = color;
    }
    while (total) {
        size_t n = total < kTxBufSize ? total : kTxBufSize;
        data(txBuf_, n);
        total -= n;
    }
}

void SpiLcdMaster::pushPixels(const uint16_t* pixels, size_t count) {
    while (count) {
        size_t n = count < kTxBufSize / 2 ? count : kTxBufSize / 2;
        for (size_t i = 0; i < n; i++) {
            txBuf_[i * 2] = pixels[i] >> 8;
            txBuf_[i * 2 + 1] = pixels[i];
        }
        data(txBuf_, n * 2);
        pixels += n;
        count -= n;
    }
}

void SpiLcdMaster::pushImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t* pixels) {
    window(x, y, x + w - 1, y + h - 1);
    command(0x2C);
    pushPixels(pixels, (size_t)w * h);
}
