#include "DemoSender.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "Font5x7.h"
#include "LoopbackTester.h"
#include <Arduino.h>
#include <stdlib.h>

#include "VirtualLcd.h"

namespace {

constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) { return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); }

constexpr uint16_t BLACK = 0x0000;
constexpr uint16_t WHITE = 0xFFFF;
constexpr uint16_t RED = rgb(255, 0, 0);
constexpr uint16_t GREEN = rgb(0, 220, 0);
constexpr uint16_t BLUE = rgb(40, 90, 255);
constexpr uint16_t YELLOW = rgb(255, 230, 0);
constexpr uint16_t CYAN = rgb(0, 230, 230);
constexpr uint16_t MAGENTA = rgb(230, 0, 230);
constexpr uint16_t ORANGE = rgb(255, 140, 0);
constexpr uint16_t NAVY = rgb(0, 0, 70);
constexpr uint16_t FACE = rgb(25, 30, 40);
constexpr uint16_t GRAY = rgb(120, 120, 120);

const uint8_t* glyphFor(char c) {
    if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
    for (const auto& g : kFont5x7)
        if (g.ch == c) return g.cols;
    return kFont5x7[0].cols;  // nieznany znak = spacja
}

}  // namespace

bool DemoSender::start(const char* driver, const char* resolution, uint32_t clockMhz) {
    if (!enabled() || !LcdProfile::find(driver) || !VirtualLcd::findPanel(driver, resolution) || !log_.begin())
        return false;
    strncpy(driver_, driver, sizeof(driver_) - 1);
    strncpy(res_, resolution ? resolution : "", sizeof(res_) - 1);
    clockMhz_ = clockMhz ? clockMhz : 20;
    pending_ = true;
    return true;
}

void DemoSender::runPending() {
    if (!pending_) return;
    pending_ = false;
    run();
    log_.end();
}

void DemoSender::run() {
    const LcdProfile& p = *LcdProfile::find(driver_);
    const PanelSize& panel = *VirtualLcd::findPanel(driver_, res_);
    W_ = panel.width;
    H_ = panel.height;
    clockSize_ = W_ - 10 < H_ - 90 ? W_ - 10 : H_ - 90;
    if (clockSize_ > 180) clockSize_ = 180;
    if (clockSize_ < 40) clockSize_ = 40;
    if (!LoopbackTester::checkWires(pins_, capturePins_, log_)) {
        log_.log("KONIEC: popraw polaczenia");
        return;
    }
    if (!lcd_.selectDriver(p.driver, panel.name)) {
        log_.log("BLAD: brak sterownika %s", p.driver);
        return;
    }
    sprite_ = (uint16_t*)malloc((size_t)clockSize_ * clockSize_ * 2);
    uint32_t actualHz = sprite_ ? master_.open(pins_, clockMhz_ * 1000000) : 0;
    if (!actualHz) {
        log_.log("BLAD: brak pamieci albo SPI nie wystartowal");
        free(sprite_);
        sprite_ = nullptr;
        return;
    }
    VirtualLcdStatus before = lcd_.status();
    log_.log("Demo %s %s @ %.1f MHz", p.driver, panel.name, actualHz / 1e6);

    master_.hardwareReset();
    master_.init(p, panel);
    const uint8_t madctl = LcdProfile::madctlFor(p.madctlRot0, panel);
    master_.command(0x36, &madctl, 1);  // rotacja 0: pion
    master_.setOrigin(panel.x, panel.y);      // jak "colstart/rowstart" w bibliotekach

    rectangles(p);
    SpiLcdMaster::waitMs(1500);
    texts(p, (actualHz + 500000) / 1000000);
    SpiLcdMaster::waitMs(2500);
    clock(p);

    master_.select(false);
    uint32_t sent = master_.bytesSent();
    master_.close();
    free(sprite_);
    sprite_ = nullptr;
    SpiLcdMaster::waitMs(100);  // przechwytywanie konczy dekodowanie

    VirtualLcdStatus s = lcd_.status();
    uint32_t received = s.capture.bytes - before.capture.bytes;
    log_.log("Bajty: wyslane %lu, odebrane %lu%s, ekran %s, przepelnienia %lu", (unsigned long)sent,
             (unsigned long)received, received == sent ? " (zgodne)" : " (ROZNICA)", s.displayOn ? "wl." : "WYL.",
             (unsigned long)(s.capture.overruns - before.capture.overruns));
    log_.log("KONIEC demo");
}

// --- 1. prostokaty ---

void DemoSender::rectangles(const LcdProfile& p) {
    uint64_t t0 = time_us_64();
    master_.fillRect(0, 0, W_, H_, BLACK);
    seed_ = 12345;
    const int count = 40;
    for (int i = 0; i < count; i++) {
        const uint16_t maxW = W_ / 2 < 90 ? W_ / 2 : 90, maxH = H_ / 2 < 90 ? H_ / 2 : 90;
        uint16_t w = 10 + random() % maxW;
        uint16_t h = 10 + random() % maxH;
        uint16_t x = random() % (W_ - w);
        uint16_t y = random() % (H_ - h);
        uint16_t color = rgb(random() % 256, random() % 256, random() % 256);
        master_.fillRect(x, y, w, h, color);
        if (i % 2 == 0) {  // ramka co drugiego prostokata
            master_.fillRect(x, y, w, 1, WHITE);
            master_.fillRect(x, y + h - 1, w, 1, WHITE);
            master_.fillRect(x, y, 1, h, WHITE);
            master_.fillRect(x + w - 1, y, 1, h, WHITE);
        }
        SpiLcdMaster::waitMs(25);  // zeby na stronie bylo widac, jak sie pojawiaja
    }
    log_.log("1. prostokaty: %d w %lu ms", count, (unsigned long)((time_us_64() - t0) / 1000));
}

// --- 2. napisy ---

void DemoSender::texts(const LcdProfile& p, uint32_t clockMhz) {
    uint64_t t0 = time_us_64();
    master_.fillRect(0, 0, W_, H_, NAVY);
    master_.fillRect(0, 0, W_, 40, rgb(0, 60, 140));

    auto center = [&](uint16_t y, const char* t, uint8_t size, uint16_t fg, uint16_t bg) {
        while (size > 1 && textWidth(t, size) > W_) size--;  // maly panel - mniejsza czcionka
        if (y + 8 * size > H_) return;
        uint16_t w = textWidth(t, size);
        if (w > W_) return;
        drawText((W_ - w) / 2, y, t, size, fg, bg);
    };
    center(8, "VIRTUAL LCD", 3, YELLOW, rgb(0, 60, 140));

    char line[32];
    snprintf(line, sizeof(line), "%s %luMHZ", p.driver, (unsigned long)clockMhz);
    center(52, line, 2, CYAN, NAVY);

    struct Item {
        const char* text;
        uint16_t color;
    };
    static const Item items[] = {{"CZERWONY", RED},      {"ZIELONY", GREEN}, {"NIEBIESKI", BLUE},
                                 {"ZOLTY", YELLOW},      {"FIOLETOWY", MAGENTA}, {"POMARANCZOWY", ORANGE},
                                 {"BIALY", WHITE}};
    uint16_t y = 84;
    for (const auto& it : items) {
        center(y, it.text, 2, it.color, NAVY);
        y += 24;
    }
    center(y + 6, "MALY TEKST 5X7: 0123456789", 1, GRAY, NAVY);
    center(y + 22, "ODWROCONE KOLORY", 2, NAVY, WHITE);
    log_.log("2. napisy w %lu ms", (unsigned long)((time_us_64() - t0) / 1000));
}

void DemoSender::drawText(uint16_t x, uint16_t y, const char* text, uint8_t size, uint16_t fg, uint16_t bg) {
    if (size < 1) size = 1;
    if (size > 4) size = 4;
    const uint16_t cw = 6 * size, ch = 8 * size;
    for (; *text; text++, x += cw) {
        const uint8_t* cols = glyphFor(*text);
        for (uint16_t py = 0; py < ch; py++) {
            for (uint16_t px = 0; px < cw; px++) {
                uint16_t gx = px / size, gy = py / size;
                bool on = gx < 5 && gy < 7 && (cols[gx] >> gy) & 1;
                glyph_[py * cw + px] = on ? fg : bg;
            }
        }
        master_.pushImage(x, y, cw, ch, glyph_);
    }
}

size_t DemoSender::strlenSafe(const char* s) { return s ? strlen(s) : 0; }

// --- 3. zegar ---

void DemoSender::clock(const LcdProfile& p) {
    master_.fillRect(0, 0, W_, H_, BLACK);
    const uint16_t sx = (W_ - clockSize_) / 2, sy = 40;
    drawText((W_ - textWidth("ZEGAR", 2)) / 2, 10, "ZEGAR", 2, WHITE, BLACK);
    uint8_t timeSize = 3;
    while (timeSize > 1 && textWidth("00:00:00", timeSize) > W_) timeSize--;

    const uint64_t t0 = time_us_64();
    const float startSec = 10 * 3600 + 8 * 60;  // 10:08:00
    int frames = 0;
    char time[16], last[16] = "";
    for (;;) {
        uint64_t elapsed = time_us_64() - t0;
        if (elapsed > 5000000) break;
        // Przyspieszony czas: 12 s na sekunde - wskazowka sekundowa robi pelny obrot w 5 s.
        float now = startSec + elapsed / 1e6f * 12.0f;
        float sec = fmodf(now, 60.0f);
        float min = fmodf(now / 60.0f, 60.0f);
        float hour = fmodf(now / 3600.0f, 12.0f);
        renderClockFace(hour, min, sec);
        master_.pushImage(sx, sy, clockSize_, clockSize_, sprite_);

        int t = (int)now;
        snprintf(time, sizeof(time), "%02d:%02d:%02d", t / 3600 % 24, t / 60 % 60, t % 60);
        if (strcmp(time, last) != 0) {
            uint16_t ty = sy + clockSize_ + 16;
            if (ty + 8 * timeSize <= H_ && textWidth(time, timeSize) <= W_)
                drawText((W_ - textWidth(time, timeSize)) / 2, ty, time, timeSize, GREEN, BLACK);
            strcpy(last, time);
        }
        frames++;
        SpiLcdMaster::idle();  // strona dostaje klatki zegara na biezaco
    }
    log_.log("3. zegar: %d klatek w 5 s (%.1f fps)", frames, frames / 5.0f);
}

void DemoSender::renderClockFace(float hours, float minutes, float seconds) {
    const int n = clockSize_;
    const float c = (n - 1) / 2.0f, r = c - 2;
    for (int i = 0; i < n * n; i++) sprite_[i] = BLACK;
    // tarcza
    for (int y = 0; y < n; y++) {
        for (int x = 0; x < n; x++) {
            float d = hypotf(x - c, y - c);
            if (d <= r) sprite_[y * n + x] = d >= r - 3 ? GRAY : FACE;
        }
    }
    // podzialka godzinowa
    for (int i = 0; i < 12; i++) {
        float a = i * (float)M_PI / 6;
        float len = i % 3 == 0 ? 16 : 9;
        spriteLine(c + sinf(a) * (r - 6), c - cosf(a) * (r - 6), c + sinf(a) * (r - 6 - len),
                   c - cosf(a) * (r - 6 - len), i % 3 == 0 ? 3 : 1, WHITE);
    }
    auto hand = [&](float turns, float len, int thick, uint16_t color) {
        float a = turns * 2 * (float)M_PI;
        spriteLine(c - sinf(a) * 10, c + cosf(a) * 10, c + sinf(a) * len, c - cosf(a) * len, thick, color);
    };
    hand((hours + minutes / 60) / 12, r * 0.5f, 5, WHITE);
    hand((minutes + seconds / 60) / 60, r * 0.75f, 3, CYAN);
    hand(seconds / 60, r * 0.85f, 1, RED);
    spriteDot((int)c, (int)c, 5, RED);
}

void DemoSender::spriteLine(float x0, float y0, float x1, float y1, int thickness, uint16_t color) {
    const int n = clockSize_;
    float dx = x1 - x0, dy = y1 - y0;
    int steps = (int)fmaxf(fabsf(dx), fabsf(dy)) + 1;
    int half = thickness / 2;
    for (int i = 0; i <= steps; i++) {
        int x = (int)lroundf(x0 + dx * i / steps), y = (int)lroundf(y0 + dy * i / steps);
        for (int oy = -half; oy <= half; oy++)
            for (int ox = -half; ox <= half; ox++) {
                int px = x + ox, py = y + oy;
                if (px >= 0 && py >= 0 && px < n && py < n) sprite_[py * n + px] = color;
            }
    }
}

void DemoSender::spriteDot(int cx, int cy, int r, uint16_t color) {
    const int n = clockSize_;
    for (int y = -r; y <= r; y++)
        for (int x = -r; x <= r; x++)
            if (x * x + y * y <= r * r && cx + x >= 0 && cy + y >= 0 && cx + x < n && cy + y < n)
                sprite_[(cy + y) * n + cx + x] = color;
}

uint32_t DemoSender::random() {
    seed_ = seed_ * 1103515245u + 12345u;
    return seed_ >> 8;
}
