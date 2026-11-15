#pragma once

#include "SpiCapture.h"
#include "hardware/pio.h"

// Przechwytywanie SPI na RP2350: PIO sklada bity (MOSI + D/C na kazde zbocze SCK),
// DMA przepisuje slowa 16-bit (1 slowo = 1 bajt SPI) do pierscienia w SRAM,
// rdzen 1 zamienia je na bajty i komendy dla sterownika LCD.
class PioSpiCapture : public SpiCapture {
public:
    bool begin(const SpiPins& pins, SpiSink* sink) override;
    void poll() override;
    SpiCaptureStats stats() const override { return stats_; }

private:
    static constexpr uint32_t kRingBits = 15;                // pierscien 32 KB (wymog DMA: potega 2, wyrownany)
    static constexpr uint32_t kRingWords = (1u << kRingBits) / 2;
    static constexpr uint32_t kMaxTransfers = 0x0FFFFFFF;    // licznik transferow DMA w jednym uzbrojeniu
    static constexpr size_t kDataChunk = 2048;

    void armDma();
    uint64_t dmaTotal() const;  // ile slow DMA zapisalo od startu
    void flushData();

    SpiPins pins_ = {};
    SpiSink* sink_ = nullptr;
    PIO pio_ = nullptr;
    uint sm_ = 0;
    uint offset_ = 0;
    int dmaChannel_ = -1;
    uint64_t dmaBase_ = 0;  // slowa z poprzednich uzbrojen DMA
    uint64_t readTotal_ = 0;
    bool lastRst_ = true;
    uint8_t csStuck_ = 0;
    uint32_t busyUs_ = 0;       // czas dekodowania w biezacym oknie 1 s
    uint32_t windowStart_ = 0;
    uint32_t windowBytes_ = 0;

    uint8_t data_[kDataChunk];
    size_t dataLen_ = 0;
    SpiCaptureStats stats_ = {};
};
