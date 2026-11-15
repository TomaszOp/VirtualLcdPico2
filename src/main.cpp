// VirtualLcd na Raspberry Pi Pico 2 W - Pico udaje wyswietlacz SPI, obraz na stronie WWW.
//
// Rdzen 0: WiFi, serwer WWW, testy.  Rdzen 1: tylko przechwytywanie i dekodowanie SPI.

#include <Arduino.h>

#include "Settings.h"
#include "hardware/vreg.h"
#include "VirtualLcd.h"
#include "net/Wifi.h"

// secrets.h jest opcjonalny: WIFI_SSID, WIFI_PASSWORD, WIFI_AP (1 = punkt dostepu, 0/brak = polaczenie z siecia).
// Ustawienia zapisane na stronie WiFi (flash) maja pierwszenstwo.
#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif
#ifndef WIFI_AP
#define WIFI_AP 0
#endif

// Polaczenie z mikrokontrolerem, na ktorym dziala program z LCD (+ wspolna masa).
// MOSI, D/C, SCK, CS musza byc kolejnymi GPIO - tak czyta je PIO.
static constexpr SpiPins kPins = {
    .mosi = 2,
    .dc = 3,
    .sck = 4,
    .cs = 5,
    .rst = 6,
};

// Pin LED (podswietlenie) wyswietlacza - musi byc nieparzystym GPIO (kanal B licznika PWM).
static constexpr int kBacklightPin = 7;

// Nadajnik testow (test petli zwrotnej i demo, sprzetowe SPI1). Polaczyc przewodami z pinami powyzej:
// GP10->GP4 (SCK), GP11->GP2 (MOSI), GP12->GP3 (D/C), GP13->GP5 (CS), GP14->GP6 (RST).
// Mikrokontroler z programem musi byc wtedy odlaczony. Testy uruchamia sie ze strony WWW.
static constexpr LoopbackPins kLoopbackPins = {
    .sck = 10,
    .mosi = 11,
    .dc = 12,
    .cs = 13,
    .rst = 14,
};

// Napiecie rdzenia 1,20 V przed podniesieniem zegara do 250 MHz (arduino-pico robi to dopiero w main(),
// a dla RP2350 nie podnosi napiecia samo). Konstruktor globalny dziala przed main().
__attribute__((constructor)) static void raiseCoreVoltage() {
    vreg_set_voltage(VREG_VOLTAGE_1_20);
    busy_wait_at_least_cycles(150000 * 2);  // ok. 2 ms przy 150 MHz na ustalenie napiecia
}

static VirtualLcd s_lcd;
static volatile bool s_core0Ready = false;

void setup() {
    Serial.begin(115200);
    uint32_t start = millis();
    while (!Serial && millis() - start < 2000) delay(10);  // chwila na otwarcie monitora USB

    Wifi::start(WIFI_SSID, WIFI_PASSWORD, WIFI_AP != 0, 15000);

    static LcdSettings lcd;
    Settings::loadLcd(lcd);
    VirtualLcdConfig config = {};
    config.pins = kPins;
    config.driver = lcd.driver;
    config.resolution = lcd.resolution;
    config.backlightPin = kBacklightPin;
    config.ignoreBacklight = lcd.ignoreBacklight;
    config.webPort = 80;
    config.loopbackPins = kLoopbackPins;
    if (!s_lcd.beginCore0(config)) Serial.println("[main] Start VirtualLcd nie powiodl sie");
    s_core0Ready = true;
}

void loop() {
    Wifi::poll();
    s_lcd.loopCore0();
}

void setup1() {
    while (!s_core0Ready) delay(1);
    if (!s_lcd.beginCore1()) Serial.println("[main] Przechwytywanie SPI nie wystartowalo");
}

void loop1() { s_lcd.loopCore1(); }
