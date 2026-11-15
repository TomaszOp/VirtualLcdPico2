#include "Backlight.h"

#include <Arduino.h>

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"

bool Backlight::begin(int pin) {
    if (pin < 0) return false;
    if (pwm_gpio_to_channel(pin) != PWM_CHAN_B) {
        Serial.printf("[led] GP%d nie jest kanalem B PWM - wybierz nieparzysty GPIO\n", pin);
        return false;
    }
    pin_ = pin;
    slice_ = pwm_gpio_to_slice_num(pin);
    gpio_set_function(pin, GPIO_FUNC_PWM);
    gpio_pull_down(pin);  // bez polaczenia: brak podswietlenia (chyba ze pin pomijany w ustawieniach)
    pwm_config c = pwm_get_default_config();
    pwm_config_set_clkdiv_mode(&c, PWM_DIV_B_HIGH);
    pwm_config_set_clkdiv_int(&c, kClkDiv);
    pwm_init(slice_, &c, true);
    windowStartUs_ = time_us_32();
    Serial.printf("[led] Pin LED: GP%d (PWM %u)\n", pin, slice_);
    return true;
}

void Backlight::poll() {
    if (pin_ < 0) return;
    uint32_t now = time_us_32();
    uint32_t elapsed = now - windowStartUs_;
    if (elapsed < kWindowMs * 1000) return;
    uint32_t high = pwm_get_counter(slice_);
    pwm_set_counter(slice_, 0);
    windowStartUs_ = now;
    // Licznik 16-bit przy zegarze/256 starcza na ok. 65 ms (250 MHz). Dluzsze okno (spozniony odczyt)
    // mogloby sie przekrecic i dac falszywe przyciemnienie - takie okno jest pomijane.
    if (elapsed > kMaxWindowUs) return;
    // Takty licznika w calym oknie przy stanie wysokim przez caly czas.
    uint64_t full = (uint64_t)clock_get_hz(clk_sys) / kClkDiv * elapsed / 1000000;
    uint32_t pct = full ? (uint32_t)((uint64_t)high * 100 + full / 2) / full : 0;
    percent_ = pct > 100 ? 100 : pct;
}
