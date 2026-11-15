#include "VirtualLcd.h"

#include <Arduino.h>
#include <string.h>
#include <strings.h>

#include "drivers/Gc9a01Driver.h"
#include "drivers/Ili9341Driver.h"
#include "drivers/St7735Driver.h"
#include "drivers/St7735RDriver.h"
#include "drivers/St7735SDriver.h"
#include "drivers/St7789Driver.h"

namespace {

struct DriverEntry {
    const char* name;
    LcdDriver* (*create)();
    const PanelSize* panels;
    size_t panelCount;
};

template <size_t N>
constexpr size_t countOf(const PanelSize (&)[N]) {
    return N;
}

// Rejestr sterownikow - nowy LCD = nowa klasa (z lista kPanels) + wpis tutaj.
const DriverEntry kDrivers[] = {
    {Ili9341Driver::kName, []() -> LcdDriver* { return new Ili9341Driver(); }, Ili9341Driver::kPanels,
     countOf(Ili9341Driver::kPanels)},
    {St7789Driver::kName, []() -> LcdDriver* { return new St7789Driver(); }, St7789Driver::kPanels,
     countOf(St7789Driver::kPanels)},
    {Gc9a01Driver::kName, []() -> LcdDriver* { return new Gc9a01Driver(); }, Gc9a01Driver::kPanels,
     countOf(Gc9a01Driver::kPanels)},
    {St7735Driver::kName, []() -> LcdDriver* { return new St7735Driver(); }, St7735Driver::kPanels,
     countOf(St7735Driver::kPanels)},
    {St7735RDriver::kName, []() -> LcdDriver* { return new St7735RDriver(); }, St7735RDriver::kPanels,
     countOf(St7735RDriver::kPanels)},
    {St7735SDriver::kName, []() -> LcdDriver* { return new St7735SDriver(); }, St7735SDriver::kPanels,
     countOf(St7735SDriver::kPanels)},
};

const DriverEntry* findDriver(const char* name) {
    for (const auto& e : kDrivers)
        if (name && strcasecmp(e.name, name) == 0) return &e;
    return nullptr;
}

}  // namespace

VirtualLcd::VirtualLcd() : web_(*this), loopback_(*this, testLog_), demo_(*this, testLog_) { mutex_init(&lock_); }

bool VirtualLcd::beginCore0(const VirtualLcdConfig& config) {
    config_ = config;
    // Pamiec obrazu na najwiekszy sterownik od razu - pozniejsza zmiana sterownika jej nie przydziela.
    size_t maxPixels = 0;
    for (const auto& e : kDrivers) {
        std::unique_ptr<LcdDriver> d(e.create());
        size_t px = (size_t)d->nativeWidth() * d->nativeHeight();
        if (px > maxPixels) maxPixels = px;
    }
    if (!frameBuffer_.reserve(maxPixels)) return false;
    {
        mutex_enter_blocking(&lock_);
        bool ok = selectDriverLocked(config.driver, config.resolution) || selectDriverLocked(kDrivers[0].name, nullptr);
        mutex_exit(&lock_);
        if (!ok) return false;
    }
    ignoreBacklight_ = config.ignoreBacklight;
    backlight_.begin(config.backlightPin);
    loopback_.setPins(config.loopbackPins, config.pins);
    demo_.setPins(config.loopbackPins, config.pins);
    SpiLcdMaster::setIdleHook([](void* arg) { ((VirtualLcd*)arg)->web_.poll(); }, this);
    return web_.begin(config.webPort);
}

void VirtualLcd::loopCore0() {
    // Pomiar LED na rdzeniu 0: okna spoznione przez wysylanie obrazu (> 60 ms) Backlight pomija.
    backlight_.poll();
    web_.poll();
    // Testy dzialaja tutaj, a nie w obsludze strony - w trakcie wywoluja web_.poll() przez SpiLcdMaster::idle().
    loopback_.runPending();
    demo_.runPending();
}

bool VirtualLcd::beginCore1() {
    bool ok = capture_.begin(config_.pins, this);
    core1Ready_ = ok;
    return ok;
}

// Petla rdzenia 1 w RAM: kod z flasha konkurowalby o wspolny cache z rdzeniem 0 (WiFi, kompresja obrazu).
void __not_in_flash_func(VirtualLcd::loopCore1)() {
    capture_.poll();
    if (request_ != Request::None) serviceRequest();
}

size_t VirtualLcd::driverCount() { return sizeof(kDrivers) / sizeof(kDrivers[0]); }

const char* VirtualLcd::driverName(size_t index) { return index < driverCount() ? kDrivers[index].name : nullptr; }

size_t VirtualLcd::panelCount(size_t driverIndex) { return driverIndex < driverCount() ? kDrivers[driverIndex].panelCount : 0; }

const PanelSize& VirtualLcd::panel(size_t driverIndex, size_t panelIndex) {
    return kDrivers[driverIndex].panels[panelIndex];
}

const PanelSize* VirtualLcd::findPanel(const char* driver, const char* resolution) {
    const DriverEntry* e = findDriver(driver);
    if (!e) return nullptr;
    for (size_t i = 0; i < e->panelCount; i++)
        if (resolution && strcasecmp(e->panels[i].name, resolution) == 0) return &e->panels[i];
    return &e->panels[0];
}

bool VirtualLcd::selectDriver(const char* name, const char* resolution) {
    strncpy(requestRes_, resolution ? resolution : "", sizeof(requestRes_) - 1);
    requestRes_[sizeof(requestRes_) - 1] = 0;
    return request(Request::SelectDriver, name);
}

void VirtualLcd::runTestPattern() { request(Request::TestPattern, nullptr); }

bool VirtualLcd::selectDriverLocked(const char* name, const char* resolution) {
    for (const auto& entry : kDrivers) {
        if (strcasecmp(entry.name, name) != 0) continue;
        const PanelSize* panel = findPanel(entry.name, resolution);
        std::unique_ptr<LcdDriver> driver(entry.create());
        if (frameBuffer_.width() != driver->nativeWidth() || frameBuffer_.height() != driver->nativeHeight()) {
            if (!frameBuffer_.begin(driver->nativeWidth(), driver->nativeHeight())) return false;
        }
        driver->attach(&frameBuffer_);
        driver->setPanel(*panel);
        driver->reset();
        driver_ = std::move(driver);
        panel_ = panel;
        commands_ = 0;
        frameBuffer_.markAllDirty();
        Serial.printf("[vlcd] Sterownik %s, panel %s (okno %u,%u w pamieci %ux%u)\n", entry.name, panel->name,
                      panel->x, panel->y, frameBuffer_.width(), frameBuffer_.height());
        return true;
    }
    Serial.printf("[vlcd] Nieznany sterownik: %s\n", name);
    return false;
}

// Zlecenie dla rdzenia 1 - goraca sciezka dekodowania nie potrzebuje wtedy blokady.
bool VirtualLcd::request(Request r, const char* driverName) {
    if (!core1Ready_) {
        if (r != Request::SelectDriver) return false;
        mutex_enter_blocking(&lock_);
        bool ok = selectDriverLocked(driverName, requestRes_);
        mutex_exit(&lock_);
        return ok;
    }
    if (request_ != Request::None) return false;  // poprzednie zlecenie jeszcze trwa
    strncpy(requestDriver_, driverName ? driverName : "", sizeof(requestDriver_) - 1);
    requestResult_ = false;
    __dmb();
    request_ = r;
    uint32_t start = millis();
    while (request_ != Request::None) {
        if (millis() - start > 2000) {
            request_ = Request::None;
            Serial.println("[vlcd] Rdzen 1 nie obsluzyl zlecenia");
            return false;
        }
        tight_loop_contents();
    }
    return requestResult_;
}

void VirtualLcd::serviceRequest() {
    Request r = request_;
    if (r == Request::None) return;
    if (r == Request::SelectDriver) {
        mutex_enter_blocking(&lock_);
        requestResult_ = selectDriverLocked(requestDriver_, requestRes_);
        mutex_exit(&lock_);
    } else if (r == Request::TestPattern) {
        if (driver_ && panel_) driver_->testInit(*panel_);
        requestResult_ = true;
    }
    __dmb();
    request_ = Request::None;
}

VirtualLcdStatus VirtualLcd::status() {
    // Pola czytane bez synchronizacji z dekodowaniem - tylko do wyswietlenia.
    VirtualLcdStatus s = {};
    s.capture = capture_.stats();
    s.driver = driver_ ? driver_->name() : "";
    s.resolution = panel_ ? panel_->name : "";
    s.width = panel_ ? panel_->width : frameBuffer_.width();
    s.height = panel_ ? panel_->height : frameBuffer_.height();
    s.offsetX = panel_ ? panel_->x : 0;
    s.offsetY = panel_ ? panel_->y : 0;
    s.round = panel_ && panel_->round;
    s.displayOn = driver_ && driver_->displayOn();
    s.inverted = driver_ && driver_->inverted();
    s.commands = commands_;
    s.offCommands = offCommands_;
    s.backlightMeasured = backlight_.measured();
    s.backlightIgnored = ignoreBacklight_ || backlight_.pin() < 0;
    s.backlight = s.backlightIgnored ? 100 : s.backlightMeasured;
    return s;
}

bool VirtualLcd::takeDirty(Rect& out) {
    Rect r;
    if (!frameBuffer_.takeDirty(r) || !panel_) return false;
    // Przyciecie do widocznego okna panelu i przeliczenie na wspolrzedne okna.
    const PanelSize& p = *panel_;
    int x0 = r.x0 > p.x ? r.x0 : p.x, y0 = r.y0 > p.y ? r.y0 : p.y;
    int x1 = r.x1 < p.x + p.width - 1 ? r.x1 : p.x + p.width - 1;
    int y1 = r.y1 < p.y + p.height - 1 ? r.y1 : p.y + p.height - 1;
    if (x0 > x1 || y0 > y1) return false;
    out = {(uint16_t)(x0 - p.x), (uint16_t)(y0 - p.y), (uint16_t)(x1 - p.x), (uint16_t)(y1 - p.y)};
    return true;
}

void VirtualLcd::copyView(const Rect& r, uint16_t* dst) {
    const PanelSize* p = panel_;
    if (!p) return;
    copyRegion({(uint16_t)(r.x0 + p->x), (uint16_t)(r.y0 + p->y), (uint16_t)(r.x1 + p->x), (uint16_t)(r.y1 + p->y)},
               dst);
}

void VirtualLcd::copyRegion(const Rect& r, uint16_t* dst) {
    mutex_enter_blocking(&lock_);
    if (r.x1 < frameBuffer_.width() && r.y1 < frameBuffer_.height()) {
        const uint16_t* src = frameBuffer_.pixels();
        const uint16_t w = r.width();
        for (uint16_t y = r.y0; y <= r.y1; y++) {
            memcpy(dst, src + y * frameBuffer_.width() + r.x0, w * sizeof(uint16_t));
            dst += w;
        }
    }
    mutex_exit(&lock_);
}

void VirtualLcd::onReset() {
    if (driver_) driver_->reset();
}

void __not_in_flash_func(VirtualLcd::onCommand)(uint8_t cmd) {
    commands_++;
    if (cmd == 0x28 || cmd == 0x10) offCommands_++;
    if (driver_) driver_->command(cmd);
}

void __not_in_flash_func(VirtualLcd::onData)(const uint8_t* bytes, size_t len) {
    if (driver_) driver_->data(bytes, len);
}
