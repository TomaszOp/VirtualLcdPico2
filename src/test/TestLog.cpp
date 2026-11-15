#include "TestLog.h"

#include <Arduino.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

bool TestLog::begin() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) return false;
    critical_section_enter_blocking(&lock_);
    len_ = 0;
    text_[0] = 0;
    critical_section_exit(&lock_);
    seq_++;
    return true;
}

void TestLog::end() {
    running_ = false;
    seq_++;  // strona dostaje stan "zakonczony"
}

void TestLog::log(const char* fmt, ...) {
    char line[200];
    va_list args;
    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    Serial.printf("[test] %s\n", line);
    critical_section_enter_blocking(&lock_);
    size_t n = strlen(line);
    if (len_ + n + 2 < sizeof(text_)) {
        memcpy(text_ + len_, line, n);
        len_ += n;
        text_[len_++] = '\n';
        text_[len_] = 0;
    }
    critical_section_exit(&lock_);
    seq_++;
}

void TestLog::copy(char* out, size_t len) {
    critical_section_enter_blocking(&lock_);
    strncpy(out, text_, len - 1);
    out[len - 1] = 0;
    critical_section_exit(&lock_);
}
