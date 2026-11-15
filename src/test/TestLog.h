#pragma once

#include <stddef.h>
#include <stdint.h>

#include <atomic>

#include "pico/critical_section.h"

// Wspolny log testow uruchamianych ze strony (test petli, demo) i blokada nadajnika SPI:
// naraz moze dzialac tylko jeden test, bo korzystaja z tych samych pinow.
class TestLog {
public:
    TestLog() { critical_section_init(&lock_); }

    // Zajmuje nadajnik i czysci log. false = inny test juz trwa.
    bool begin();
    void end();
    bool running() const { return running_; }

    void log(const char* fmt, ...) __attribute__((format(printf, 2, 3)));

    // Numer kolejny zmienia sie po kazdym wpisie i zmianie stanu - serwer WWW wie, kiedy wyslac log.
    uint32_t seq() const { return seq_; }
    void copy(char* out, size_t len);

private:
    critical_section_t lock_;
    char text_[1024] = {};
    size_t len_ = 0;
    std::atomic<uint32_t> seq_{0};
    std::atomic<bool> running_{false};
};
