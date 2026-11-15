#include "MipiDcsDriver.h"

#include "FrameBuffer.h"
#include "pico.h"

void MipiDcsDriver::reset() {
    cmd_ = 0;
    paramIndex_ = 0;
    colStart_ = rowStart_ = 0;
    colEnd_ = panel_.width - 1;
    rowEnd_ = panel_.height - 1;
    col_ = row_ = 0;
    scanMirrorX_ = scanMirrorY_ = false;
    setMadctl(0);
    bytesPerPixel_ = 3;  // po resecie 18 bit (COLMOD 0x66)
    pixelBytes_ = 0;
    displayOn_ = false;
    sleeping_ = true;
    invertOn_ = false;
    if (fb_) {
        fb_->fill(0);
        fb_->markAllDirty();
    }
}

void __not_in_flash_func(MipiDcsDriver::command)(uint8_t cmd) {
    cmd_ = cmd;
    paramIndex_ = 0;
    pixelBytes_ = 0;
    switch (cmd) {
        case CMD_SWRESET: reset(); break;
        case CMD_SLPIN: sleeping_ = true; break;
        case CMD_SLPOUT: sleeping_ = false; break;
        case CMD_INVOFF: invertOn_ = false; break;
        case CMD_INVON: invertOn_ = true; break;
        case CMD_DISPOFF: displayOn_ = false; break;
        case CMD_DISPON: displayOn_ = true; break;
        case CMD_RAMWR: startRamWrite(true); break;
        case CMD_RAMWRC: startRamWrite(false); break;
        default: break;
    }
}

void __not_in_flash_func(MipiDcsDriver::data)(const uint8_t* bytes, size_t len) {
    if (cmd_ == CMD_RAMWR || cmd_ == CMD_RAMWRC) {
        writePixels(bytes, len);
        return;
    }
    for (size_t i = 0; i < len; i++) {
        onParam(cmd_, paramIndex_, bytes[i]);
        if (paramIndex_ < 255) paramIndex_++;
    }
}

void __not_in_flash_func(MipiDcsDriver::onParam)(uint8_t cmd, uint8_t index, uint8_t value) {
    if (index < sizeof(params_)) params_[index] = value;
    switch (cmd) {
        case CMD_CASET:
            if (index == 1) colStart_ = (params_[0] << 8) | params_[1];
            if (index == 3) colEnd_ = (params_[2] << 8) | params_[3];
            break;
        case CMD_RASET:
            if (index == 1) rowStart_ = (params_[0] << 8) | params_[1];
            if (index == 3) rowEnd_ = (params_[2] << 8) | params_[3];
            break;
        case CMD_MADCTL:
            if (index == 0) setMadctl(value);
            break;
        case CMD_COLMOD:
            // dolne 3 bity: 5 = 16 bit, 6 = 18 bit
            if (index == 0) bytesPerPixel_ = ((value & 0x07) == 0x06) ? 3 : 2;
            break;
        default: break;
    }
}

void MipiDcsDriver::setScanMirror(bool x, bool y) {
    scanMirrorX_ = x;
    scanMirrorY_ = y;
    setMadctl(madctl_);
}

void MipiDcsDriver::setMadctl(uint8_t value) {
    madctl_ = value;
    mv_ = value & MADCTL_MV;
    mirrorX_ = (((value & MADCTL_MX) != 0) != panel_.mirrorX) != scanMirrorX_;
    mirrorY_ = (((value & MADCTL_MY) != 0) != panel_.mirrorY) != scanMirrorY_;
    swapRB_ = ((value & MADCTL_BGR) != 0) != panel_.bgr;
}

void MipiDcsDriver::startRamWrite(bool restart) {
    if (restart) {
        col_ = colStart_;
        row_ = rowStart_;
    }
    pixelBytes_ = 0;
}

inline void __not_in_flash_func(MipiDcsDriver::putPixel)(uint16_t color) {
    // Adres logiczny (kolumna, wiersz) -> pamiec panelu wg MADCTL.
    uint16_t x = mv_ ? row_ : col_;
    uint16_t y = mv_ ? col_ : row_;
    if (x < panel_.width && y < panel_.height) {
        if (mirrorX_) x = panel_.width - 1 - x;
        if (mirrorY_) y = panel_.height - 1 - y;
        if (swapRB_) color = (color >> 11) | (color & 0x07E0) | (color << 11);
        fb_->setPixel(x, y, color);
    }
    if (col_ >= colEnd_) {
        col_ = colStart_;
        row_ = (row_ >= rowEnd_) ? rowStart_ : row_ + 1;
    } else {
        col_++;
    }
}

void __not_in_flash_func(MipiDcsDriver::writePixels)(const uint8_t* bytes, size_t len) {
    if (!fb_ || len == 0) return;
    uint16_t startRow = row_;
    bool wrapped = false;
    uint16_t prevRow = row_;

    size_t i = 0;
    // Szybko: 16 bit, cale okno w pamieci - kolejne piksele wiersza logicznego leza w buforze obrazu
    // co staly krok: +-1 (bez zamiany osi) albo +-szerokosc (MV, np. rotacja 1 i 3 w bibliotekach).
    const uint16_t W = panel_.width, H = panel_.height;
    const bool inside = mv_ ? (colEnd_ < H && rowEnd_ < W) : (colEnd_ < W && rowEnd_ < H);
    if (bytesPerPixel_ == 2 && inside) {
        if (pixelBytes_ == 1 && len > 0) {  // dokoncz piksel zaczety w poprzedniej porcji
            putPixel((pixelBuf_[0] << 8) | bytes[0]);
            pixelBytes_ = 0;
            i = 1;
        }
        const int step = mv_ ? (mirrorY_ ? -(int)W : (int)W) : (mirrorX_ ? -1 : 1);
        while (len - i >= 2) {
            if (col_ > colEnd_ || row_ > rowEnd_) break;
            size_t run = (len - i) / 2;
            size_t left = colEnd_ - col_ + 1;
            if (run > left) run = left;
            uint16_t mx = mv_ ? row_ : col_, my = mv_ ? col_ : row_;
            if (mirrorX_) mx = W - 1 - mx;
            if (mirrorY_) my = H - 1 - my;
            uint16_t* dst = fb_->row(my) + mx;
            const uint8_t* b = bytes + i;
            if (swapRB_) {
                for (size_t k = 0; k < run; k++, b += 2, dst += step) {
                    uint16_t c = (b[0] << 8) | b[1];
                    *dst = (c >> 11) | (c & 0x07E0) | (c << 11);
                }
            } else {
                for (size_t k = 0; k < run; k++, b += 2, dst += step) *dst = (b[0] << 8) | b[1];
            }
            i += run * 2;
            col_ += run;
            if (col_ > colEnd_) {
                col_ = colStart_;
                if (row_ >= rowEnd_) {
                    row_ = rowStart_;
                    wrapped = true;
                } else {
                    row_++;
                }
            }
        }
        prevRow = row_;
    }
    for (; i < len; i++) {
        pixelBuf_[pixelBytes_++] = bytes[i];
        if (pixelBytes_ < bytesPerPixel_) continue;
        pixelBytes_ = 0;
        uint16_t color;
        if (bytesPerPixel_ == 2) {
            color = (pixelBuf_[0] << 8) | pixelBuf_[1];
        } else {
            color = ((pixelBuf_[0] & 0xF8) << 8) | ((pixelBuf_[1] & 0xFC) << 3) | (pixelBuf_[2] >> 3);
        }
        putPixel(color);
        if (row_ < prevRow) wrapped = true;
        prevRow = row_;
    }

    // Zmieniony obszar: okno CASET/RASET od wiersza startowego, przeliczone na pamiec panelu.
    uint16_t c0 = colStart_, c1 = colEnd_;
    uint16_t r0 = wrapped ? rowStart_ : startRow, r1 = wrapped ? rowEnd_ : row_;
    uint16_t x0 = mv_ ? r0 : c0, x1 = mv_ ? r1 : c1;
    uint16_t y0 = mv_ ? c0 : r0, y1 = mv_ ? c1 : r1;
    if (x1 >= panel_.width) x1 = panel_.width - 1;
    if (y1 >= panel_.height) y1 = panel_.height - 1;
    if (x0 > x1 || y0 > y1) return;
    if (mirrorX_) { uint16_t t = panel_.width - 1 - x1; x1 = panel_.width - 1 - x0; x0 = t; }
    if (mirrorY_) { uint16_t t = panel_.height - 1 - y1; y1 = panel_.height - 1 - y0; y0 = t; }
    fb_->markDirty({x0, y0, x1, y1});
}

// --- obraz testowy (bez drugiego mikrokontrolera) ---

void MipiDcsDriver::send(uint8_t cmd, const uint8_t* params, size_t len) {
    command(cmd);
    if (len) data(params, len);
}

void MipiDcsDriver::testFillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    uint8_t ca[4] = {(uint8_t)(x >> 8), (uint8_t)x, (uint8_t)((x + w - 1) >> 8), (uint8_t)(x + w - 1)};
    uint8_t ra[4] = {(uint8_t)(y >> 8), (uint8_t)y, (uint8_t)((y + h - 1) >> 8), (uint8_t)(y + h - 1)};
    send(CMD_CASET, ca, 4);
    send(CMD_RASET, ra, 4);
    command(CMD_RAMWR);
    uint8_t line[64];
    for (size_t i = 0; i < sizeof(line); i += 2) {
        line[i] = color >> 8;
        line[i + 1] = color;
    }
    size_t total = (size_t)w * h * 2;
    while (total) {
        size_t n = total < sizeof(line) ? total : sizeof(line);
        data(line, n);
        total -= n;
    }
}

void MipiDcsDriver::testPattern(const PanelSize& panel) {
    static const uint16_t bars[] = {0xFFFF, 0xFFE0, 0x07FF, 0x07E0, 0xF81F, 0xF800, 0x001F, 0x0000};
    const uint16_t ox = panel.x, oy = panel.y, w = panel.width, h = panel.height;
    const uint16_t barW = w / 8;
    for (int i = 0; i < 8; i++) testFillRect(ox + i * barW, oy, i == 7 ? w - 7 * barW : barW, h * 2 / 3, bars[i]);
    // szare paski na dole
    for (int i = 0; i < 16; i++) {
        uint8_t v = i * 2;
        uint16_t gray = (v << 11) | ((v * 2) << 5) | v;
        uint16_t x0 = i * w / 16, x1 = (i + 1) * w / 16;
        testFillRect(ox + x0, oy + h * 2 / 3, x1 - x0, h - h * 2 / 3, gray);
    }
    // czerwony znacznik w lewym gornym rogu - kontrola orientacji (na okraglym panelu blizej srodka, w kole)
    const uint16_t m = panel.round ? w / 5 : 4;
    testFillRect(ox + m, oy + m, 20, 20, 0xF800);
    testFillRect(ox + m + 4, oy + m + 4, 12, 12, 0xFFFF);
}
