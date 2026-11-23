# VirtualLcd – Raspberry Pi Pico 2 W

## Do czego to jest

VirtualLcd to **wirtualny wyświetlacz LCD**. Raspberry Pi Pico 2 W podłącza się do magistrali SPI
w miejsce prawdziwego wyświetlacza (ILI9341, ST7789, GC9A01, ST7735…). Pico odbiera komendy i piksele
tak jak sterownik LCD, a obraz pokazuje na stronie WWW, w przeglądarce na komputerze albo telefonie.

Program na mikrokontrolerze (ESP32, RP2040, STM32…) i jego biblioteka graficzna (Adafruit_GFX, TFT_eSPI,
LovyanGFX…) **działają bez żadnych zmian**. Nie trzeba mieć fizycznego wyświetlacza.

Do czego się przydaje:
- pisanie i uruchamianie programu z wyświetlaczem, gdy wyświetlacza nie ma albo jest za mały, żeby
  przyjrzeć się szczegółom (podgląd w powiększeniu 1–4×),
- sprawdzanie, co biblioteka faktycznie wysyła: obrót, przesunięcie okna (colstart/rowstart), inwersja,
  kolejność kolorów RGB/BGR, wyłączanie ekranu, jasność podświetlenia,
- pokazywanie obrazu z urządzenia innym osobom (do 3 przeglądarek naraz),
- testy wydajności SPI do 40 MHz (statystyki przechwytywania i przepełnień).

## Panel WWW

Adres Pico podaje log szeregowy przy starcie (albo router). Bez skonfigurowanej sieci Pico tworzy
otwarty punkt dostępu **VirtualLCD**, a panel jest pod http://192.168.4.1/. Wszystkie strony mają
u góry menu: Podgląd, Wyświetlacz, WiFi i Test.

Panel jest po polsku albo po angielsku – przełącznik **PL / EN** jest w górnej belce (na wąskim ekranie
widać tylko przycisk drugiego języka). Wybór jest zapisywany w urządzeniu, więc po restarcie strony od razu
są w zapisanym języku, w każdej przeglądarce. Log testów na stronie Test zostaje po polsku.

### Podgląd (`/`)

![Podgląd](doc/img/podglad.png)

Obraz emulowanego wyświetlacza na żywo, odświeżany przez WebSocket (tylko zmienione fragmenty,
z kompresją RLE).
- Obrót i powiększenie według ustawień ze strony Wyświetlacz („Dopasuj” = do wielkości okna przeglądarki).
- Pod obrazem: stan połączenia, model i rozdzielczość, stan ekranu (włączony/wyłączony, inwersja)
  i zmierzona jasność podświetlenia.
- Podgląd odwzorowuje stan wyświetlacza: wyłączony ekran (DISPOFF, uśpienie) jest czarny, inwersja kolorów
  jest widoczna, a obraz jest przyciemniany według podświetlenia z pinu LED. Okrągły panel (GC9A01) ma okrągłą maskę.
- Odświeżenie strony albo przejście na inną stronę panelu zamyka poprzednie połączenie tej samej przeglądarki.

Na zrzucie widać obraz testowy (paski kolorów, skala szarości, znacznik orientacji w lewym górnym rogu).

### Wyświetlacz (`/lcd`)

![Wyświetlacz](doc/img/wyswietlacz.png)

Ustawienia emulowanego wyświetlacza:
- **Model LCD** – lista rozwijana z emulowanymi sterownikami. Musi się zgadzać z biblioteką w programie mikrokontrolera.
- **Rozdzielczość** – panele dostępne dla wybranego sterownika (np. ST7789: 240×320, 240×240, 135×240…).
  Każdy ma swoje okno w pamięci sterownika, takie jak przesunięcie ustawiane w bibliotece. Gdy sterownik ma
  jedną rozdzielczość, lista jest nieaktywna.
- **Pomiń pin LED** – podświetlenie zawsze 100 %. Bez zaznaczenia podgląd jest przyciemniany według sygnału
  na GP7 (stan stały albo PWM). Obok widać aktualnie zmierzoną wartość.
- **Obrót** i **Skalowanie** – tylko dla podglądu na stronie Podgląd.
- **Obraz testowy** – Pico samo rysuje obraz testowy w wybranym panelu, bez drugiego mikrokontrolera.
- **Zapisz** – zapis do flasha. Model i rozdzielczość działają od razu, a wszystkie ustawienia są
  wczytywane przy starcie. Przycisk jest aktywny tylko przy niezapisanych zmianach.

### WiFi (`/wifi`)

![WiFi](doc/img/wifi.png)

- **Stan połączenia** – skąd pochodzą ustawienia (strona, `secrets.h`, awaryjny punkt dostępu), nazwa sieci,
  adres IP i siła sygnału.
- **Tryb** – połączenie z istniejącą siecią (STA) albo własny punkt dostępu (AP).
- **Nazwa sieci** z przyciskiem **Szukaj** (lista sieci w pobliżu) i **hasło** (puste = sieć otwarta).
- **Zapisz i uruchom ponownie** – po restarcie strona czeka na urządzenie i podpowiada nowy adres.
- **Usuń zapisane ustawienia** (powrót do `secrets.h` albo punktu dostępu VirtualLCD) i **Uruchom ponownie**.
- Opis kolejności, w jakiej ustawienia WiFi są próbowane przy starcie.

### Test (`/test`)

![Test](doc/img/test.png)

Strona do sprawdzania emulatora bez drugiego mikrokontrolera:
- U góry: szybka zmiana emulowanego LCD (model + rozdzielczość, bez zapisu), skala i obrót podglądu, obraz testowy.
- Podgląd z liczbami: komendy, bajty SPI, przepełnienia, urwane bajty, resety.
- **Demo** – nadajnik SPI tego samego Pico rysuje prostokąty, napisy i animowany zegar dla wybranego
  wyświetlacza i zegara SPI (10, 20 albo 40 MHz).
- **Test pętli zwrotnej** – inicjalizacja jak w bibliotece, pełny obraz, w którym każdy piksel jest inny,
  i prostokąt w rotacji 1. Potem porównanie z tym, co odebrał emulator: błędne piksele, liczba bajtów,
  stan ekranu. Do wyboru wybrany zegar, wszystkie zegary (10, 20, 40 MHz) albo wszystkie modele.
- Log testu w czarnym polu na dole.

Demo i test wymagają przewodów nadajnika (opis niżej w „Test pętli zwrotnej i demo”).

### Diagnostyka (`/status`)

JSON dla komputera (np. `curl http://<adres>/status`): liczniki przechwytywania (bajty, komendy, przepełnienia,
szczytowe obciążenie rdzenia 1 i przepływność), połączenia WWW (klienci, rozłączenia, błędy wysyłki,
przepływność), WiFi (RSSI), wolna pamięć i czas pracy.

## Jak to działa

**PIO** składa bity sprzętowo. Na każde zbocze SCK wsuwa MOSI i D/C, a po 8 bitach oddaje jedno słowo 16-bit na bajt SPI.
CS jest sprawdzany na początku każdego bajtu, więc zegar innych urządzeń na wspólnej szynie jest pomijany.

## Framework

PlatformIO + [arduino-pico](https://github.com/earlephilhower/arduino-pico) (earlephilhower), platforma maxgerhardt:
- pełne pico-sdk 2.x (PIO, DMA, `pio_claim_free_sm_and_add_program_for_gpio_range`),
- WiFi CYW43439 przez lwIP (`WiFi`, `WiFiServer`),
- oba rdzenie: `setup()/loop()` na rdzeniu 0, `setup1()/loop1()` na rdzeniu 1,
- pioasm w pakiecie – `tools/pioasm.py` generuje `*.pio.h` przed kompilacją.

## Podział pracy

| Rdzeń | Zadanie |
|-------|---------|
| 0 | WiFi, serwer HTTP + WebSocket, test pętli zwrotnej, demo |
| 1 | tylko przechwytywanie: odczyt pierścienia DMA, dekodowanie, sterownik LCD |

Zegar CPU 250 MHz (`board_build.f_cpu`, napięcie rdzenia 1,20 V ustawiane w `main.cpp`).
Pętla PIO to 3 instrukcje na bit, więc zapas do ok. 60 MHz SPI.

## Połączenie

| Mikrokontroler z programem | Pico 2 W |
|----------------------------|----------|
| MOSI / SDA / DIN           | GP2      |
| D/C (RS, A0)               | GP3      |
| SCK / CLK                  | GP4      |
| CS                         | GP5      |
| RST (opcjonalnie)          | GP6      |
| LED / BL (opcjonalnie)     | GP7      |
| GND                        | GND      |

MOSI, D/C, SCK, CS muszą być kolejnymi GPIO (tak czyta je PIO). Poziomy 3,3 V (Pico nie toleruje 5 V).
Tryb SPI 0. Wspólna masa jest konieczna.

### Test pętli zwrotnej i demo

Nadajnik testowy to sprzętowe SPI1 tego samego Pico. Przewody:
GP10→GP4 (SCK), GP11→GP2 (MOSI), GP12→GP3 (D/C), GP13→GP5 (CS), GP14→GP6 (RST).
Mikrokontroler z programem musi być wtedy odłączony.

Zegar SPI nadajnika powstaje z dzielnika zegara peryferiów, więc „40 MHz” może wyjść np. 33,3 MHz –
log podaje rzeczywistą wartość.

## Uruchomienie

1. Opcjonalnie skopiuj `include/secrets.h.example` do `include/secrets.h` i wpisz dane WiFi
   (`WIFI_AP 1` = własny punkt dostępu, `0` lub brak = połączenie z siecią).
2. `pio run -t upload` (Pico w trybie BOOTSEL za pierwszym razem), potem `pio device monitor`.
3. Otwórz adres z logu. Diagnostyka: `http://<adres>/status`.

Kolejność ustawień WiFi przy starcie: zapisane na stronie (flash) → `secrets.h` → otwarty punkt
dostępu **VirtualLCD** (http://192.168.4.1/). Gdy sieć z danego kroku nie wystartuje (najwyżej 15 s),
próbowany jest następny. Przy punkcie dostępu VirtualLCD urządzenie co 3 minuty próbuje połączyć się
z siecią, ale tylko gdy nikt nie jest podłączony do punktu dostępu (CYW43 nie robi obu rzeczy naraz).

## Strony

| Adres | Zawartość |
|-------|-----------|
| `/` | podgląd obrazu (obrót i skalowanie wg ustawień ze strony Wyświetlacz) |
| `/lcd` | model LCD, rozdzielczość, obrót, skalowanie, obraz testowy, „Zapisz” (flash, wczytywane przy starcie) |
| `/wifi` | stan sieci, tryb STA/AP, SSID (z wyszukiwaniem), hasło (puste = sieć otwarta) |
| `/test` | test pętli zwrotnej, demo przez SPI, statystyki przechwytywania |
| `/status` | diagnostyka w JSON |

Do 3 kart przeglądarki naraz dostaje ten sam obraz. Przez WebSocket idą: lista sterowników raz po połączeniu
(`drivers`), stan co sekundę (`info`), log testów (`test`) i fragmenty obrazu (binarnie, RGB565, opcjonalnie RLE).

## Ustawienia

Zapisywane w plikach `/lcd.bin`, `/wifi.bin` i `/ui.bin` (język panelu) na LittleFS – partycja 64 KB na końcu flasha
(`board_build.filesystem_size`). Zapis do flasha na chwilę wstrzymuje rdzeń 1, więc w tym czasie
przechwytywanie SPI stoi – dzieje się to tylko po kliknięciu „Zapisz” na stronie albo zmianie języka.
Wgranie nowego programu nie kasuje ustawień (partycja jest poza obszarem programu).

## Budowa

| Plik | Rola |
|------|------|
| `VirtualLcd` | centralna klasa: rejestr sterowników `kDrivers`, bufor obrazu, przechwytywanie (rdzeń 1), WWW i testy (rdzeń 0) |
| `drivers/*` | sterowniki LCD: `MipiDcsDriver` (wspólne komendy) i klasy pochodne z listą paneli `kPanels` |
| `capture/spi_capture.pio` | program PIO: MOSI + D/C na zbocze SCK, bajt po CS |
| `capture/PioSpiCapture` | PIO + DMA (pierścień 32 KB) + dekodowanie tablicą |
| `web/WebServer` | HTTP + WebSocket na `WiFiServer`: strony, API ustawień, do 3 klientów |
| `web/Pages`, `web/TestHtml` | strony `/`, `/lcd`, `/wifi` i `/test` po polsku + wspólne style i skrypty (makra) |
| `web/PagesEn`, `web/TestHtmlEn` | te same strony po angielsku |
| `net/Wifi` | WiFi: STA, AP, awaryjny AP, wyszukiwanie sieci |
| `Settings` | ustawienia na LittleFS (wyświetlacz, WiFi, język panelu) |
| `test/SpiLcdMaster` | nadajnik: sprzętowe SPI, profile `kProfiles` z sekwencjami startowymi bibliotek |
| `test/LoopbackTester`, `test/DemoSender` | test pętli zwrotnej i demo |

## Rozdzielczości

Każda klasa sterownika ma listę `kPanels` (np. `St7789Driver.h`): nazwa, szerokość, wysokość i położenie
widocznego okna w pamięci sterownika – takie przesunięcie biblioteki dodają w CASET/RASET
(„colstart/rowstart”). Podgląd na stronie pokazuje tylko to okno.

| Sterownik | Rozdzielczości |
|-----------|----------------|
| ILI9341 | 240×320 |
| ST7789 | 240×320, 240×280, 240×240, 172×320, 170×320, 135×240 (okno x=52 jak TFT_eSPI; Adafruit używa 53), 76×284 |
| GC9A01 | 240×240, okrągły (podgląd z okrągłą maską) |
| ST7735, ST7735R, ST7735S | 128×160 „green tab” (okno 2,1, BGR), 128×128 (okno 2,3 jak Adafruit 144GREENTAB, BGR), 80×160 IPS (okno 26,1 jak TFT_eSPI/LovyanGFX – Adafruit używa 24,0; RGB, INVON) |

Nowa rozdzielczość = nowy wpis w `kPanels` danego sterownika. Test pętli i demo rysują w oknie wybranej
rozdzielczości, tak jak robiłaby to biblioteka.

## Pin LED (podświetlenie)

GP7 mierzy sygnał podświetlenia: stan stały albo PWM (licznik PWM RP2350 zlicza takty przy stanie wysokim,
okno 50 ms). Podgląd na stronie jest przyciemniany według wypełnienia. Na stronie Wyświetlacz można
zaznaczyć „Pomiń pin LED” – wtedy podświetlenie jest zawsze 100 % (domyślnie, bo bez połączenia pin czyta 0).
Pin LED musi być nieparzystym GPIO (kanał B plastra PWM).

## Jak dodać nowy LCD

Lista sterowników na stronach WWW (Wyświetlacz, Test) jest budowana z firmware, więc HTML się nie zmienia.

### 1. Klasa sterownika – `src/drivers/<Nazwa>Driver.h`

Większość wyświetlaczy SPI jest zgodna z MIPI DCS (CASET/RASET/RAMWR, MADCTL, COLMOD, INVON, DISPON) –
wtedy klasa dziedziczy po `MipiDcsDriver` i podaje tylko stałe:

```cpp
#pragma once

#include "MipiDcsDriver.h"

class Ili9488Driver : public MipiDcsDriver {
public:
    // Nazwa sterownika - takze na stronie, w ustawieniach i w profilach testow (test/SpiLcdMaster.cpp).
    static constexpr const char* kName = "ILI9488";

    // Rozdzielczosci paneli (pierwsza = domyslna): nazwa, szerokosc, wysokosc, okno x, y w pamieci
    // [, okragly, bgr, inverted - cechy zalezne od szyby, -1 = cecha sterownika].
    static constexpr PanelSize kPanels[] = {
        {"320x480", 320, 480, 0, 0},
    };

    // Pamiec obrazu (szerokosc, wysokosc), panel BGR, lustro X, IPS (odwrocone kolory), [lustro Y].
    Ili9488Driver() : MipiDcsDriver({320, 480, true, true, false}) {}

    const char* name() const override { return kName; }

    // Obraz testowy z przycisku na stronie: minimalna sekwencja jak w bibliotece.
    void testInit(const PanelSize& panel) override { ... testPattern(panel); }
};
```

Cechy panelu (`PanelTraits`) dobiera się tak, żeby to, co biblioteka wysyła w rotacji 0, dało prosty obraz:
- `bgr` – biblioteka ustawia bit BGR w MADCTL (panel ma kolejność BGR),
- `mirrorX` / `mirrorY` – biblioteka w rotacji 0 wysyła MX / MY (panel zamontowany w lustrze),
- `inverted` – biblioteka wysyła INVON (panel IPS).

Komendy producenta, które zmieniają obraz (np. ILI9341 0xB6 – kierunek skanowania), obsługuje się w `onParam()`.
Gdy kilka wersji układu ma te same komendy (ST7735, ST7735R, ST7735S), wspólna logika i lista paneli
idą do klasy bazowej (`St7735Base`), a każda wersja to osobny mały plik z `kName`.

### 2. Rejestr sterowników – `src/VirtualLcd.cpp`

Nagłówek w `#include` i jeden wpis w `kDrivers`:

```cpp
{Ili9488Driver::kName, []() -> LcdDriver* { return new Ili9488Driver(); }, Ili9488Driver::kPanels,
 countOf(Ili9488Driver::kPanels)},
```

Bufor obrazu (RGB565, 2 B na piksel) jest przydzielany przy starcie na największy sterownik. Dziś to
240×320 = 150 KB, a wolnego RAM zostaje ok. 125 KB – pamięć 320×480 (300 KB) już by się nie zmieściła
i wymagałaby np. bufora o mniejszej głębi kolorów.

### 3. Profil testowy – `src/test/SpiLcdMaster.cpp` (dla demo i testu pętli)

Sekwencja startowa jak w typowej bibliotece (`LcdInitCmd`: komenda, liczba parametrów, do 16 parametrów,
opóźnienie ms) i wpis w `kProfiles`:

```cpp
{Ili9488Driver::kName, kIli9488Init, sizeof(kIli9488Init) / sizeof(LcdInitCmd), 320, 480, 0x48, 0x28},
//                                                                  pamiec obrazu, MADCTL rot. 0, rot. 1
```

Bez profilu sterownik działa normalnie, tylko demo i test pętli go nie obsłużą.

### 4. Sprawdzenie

- `pio run` – kompilacja.
- Strona Test → test pętli zwrotnej dla nowego modelu (wszystkie zegary) – musi dać „OK”.
- Z prawdziwym mikrokontrolerem i biblioteką: obrót 0–3, kolory (RGB/BGR), inwersja i przesunięcie okna.
