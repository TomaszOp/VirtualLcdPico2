#include "FrameBuffer.h"

#include <Arduino.h>
#include <stdlib.h>

FrameBuffer::FrameBuffer() { critical_section_init(&lock_); }

FrameBuffer::~FrameBuffer() { end(); }

bool FrameBuffer::reserve(size_t pixels) {
    if (pixels <= capacity_) return true;
    end();
    pixels_ = (uint16_t*)malloc(pixels * sizeof(uint16_t));
    if (!pixels_) {
        Serial.printf("[fb] Brak pamieci na bufor %u B\n", (unsigned)(pixels * sizeof(uint16_t)));
        return false;
    }
    capacity_ = pixels;
    return true;
}

bool FrameBuffer::begin(uint16_t width, uint16_t height) {
    if (!reserve((size_t)width * height)) return false;
    width_ = width;
    height_ = height;
    fill(0);
    markAllDirty();
    return true;
}

void FrameBuffer::end() {
    free(pixels_);
    pixels_ = nullptr;
    capacity_ = 0;
    width_ = height_ = 0;
    dirty_ = false;
}

void FrameBuffer::fill(uint16_t color) {
    size_t n = (size_t)width_ * height_;
    for (size_t i = 0; i < n; i++) pixels_[i] = color;
}

void FrameBuffer::markDirty(const Rect& r) {
    critical_section_enter_blocking(&lock_);
    if (!dirty_) {
        dirtyRect_ = r;
        dirty_ = true;
    } else {
        if (r.x0 < dirtyRect_.x0) dirtyRect_.x0 = r.x0;
        if (r.y0 < dirtyRect_.y0) dirtyRect_.y0 = r.y0;
        if (r.x1 > dirtyRect_.x1) dirtyRect_.x1 = r.x1;
        if (r.y1 > dirtyRect_.y1) dirtyRect_.y1 = r.y1;
    }
    critical_section_exit(&lock_);
}

void FrameBuffer::markAllDirty() {
    if (width_ == 0 || height_ == 0) return;
    markDirty({0, 0, (uint16_t)(width_ - 1), (uint16_t)(height_ - 1)});
}

bool FrameBuffer::takeDirty(Rect& out) {
    critical_section_enter_blocking(&lock_);
    bool d = dirty_;
    out = dirtyRect_;
    dirty_ = false;
    critical_section_exit(&lock_);
    return d;
}
