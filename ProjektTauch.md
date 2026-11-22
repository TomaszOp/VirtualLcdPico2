# Projekt: emulacja panelu dotykowego

Stan: projekt, bez zmian w kodzie (15.11.2026).

**Priorytet: udoskonalenie.** Zależy od [ProjektID.md](ProjektID.md): wspólne MISO (GP8) i maszyna PIO nadająca
z tablicą odpowiedzi (DMA) są potrzebne także tutaj. **Funkcja wyłączana w ustawieniach, domyślnie wyłączona** –
wtedy wszystkie piny dotyku są wejściami w stanie wysokiej impedancji, a Pico nic nie nadaje.

## Cel

Program na mikrokontrolerze czyta dotyk tak jak z prawdziwego modułu (XPT2046 przez SPI albo kontroler
pojemnościowy przez I2C), a „palcem” jest mysz albo dotyk na stronie Podgląd. Biblioteki (TFT_eSPI, XPT2046_Touchscreen,
LovyanGFX, Adafruit_FT6206, sterowniki CST816S) działają bez zmian.

## Kontrolery dotyku

| Kontroler | Typ | Magistrala | Typowo przy |
|-----------|-----|------------|-------------|
| XPT2046 (ADS7843, TSC2046) | rezystancyjny | SPI | ILI9341 2,4–3,2″, płytki „CYD” (ESP32-2432S028) |
| CST816S | pojemnościowy | I2C | okrągłe GC9A01 1,28″ |
| FT6236 / FT6206 / FT6336 | pojemnościowy | I2C | ILI9341 / ST7789 z dotykiem pojemnościowym |
| GT911 | pojemnościowy | I2C | większe panele – później |

## XPT2046 (SPI)

### Protokół (z dokumentacji i bibliotek – szczegóły do sprawdzenia w datasheet)

- **Bajt kontrolny** na DIN: `S A2 A1 A0 MODE SER/DFR PD1 PD0`. Bit S = 1 to start – XPT2046 rozpoznaje komendę
  po pierwszej jedynce na DIN, **nie ma linii D/C**.
  - Kanał (A2..A0, tryb różnicowy SER/DFR = 0): X = `101` (0xD0), Y = `001` (0x90), Z1 = `011` (0xB0), Z2 = `100` (0xC0).
  - MODE = 0: 12 bitów (biblioteki zawsze 12).
  - PD1..PD0: `00` – uśpienie między pomiarami z włączonym PENIRQ (0xD0, 0x90…); `01` (0xD1, 0x91…) używa
    XPT2046_Touchscreen w trakcie serii pomiarów, a na końcu wysyła 0xD0, żeby włączyć PENIRQ.
- **Odpowiedź na DOUT**: po 8 taktach bajtu kontrolnego **1 takt BUSY (0)**, potem **12 bitów wyniku (MSB pierwszy)**,
  potem zera. Biblioteki czytają 16 bitów (`transfer16`) i przesuwają `>> 3` (TFT_eSPI, XPT2046_Touchscreen).
- **Nakładanie (tryb 16 taktów na pomiar)**: biblioteki wysyłają następny bajt kontrolny w tych samych 16 taktach,
  w których odbierają poprzedni wynik (`transfer16(0xD1)` zwraca wynik poprzedniej komendy). Emulator musi więc
  rozpoznawać nową komendę na DIN w trakcie nadawania wyniku na DOUT. **Do sprawdzenia w datasheet**: dokładne
  wyrównanie bitu S następnej komendy względem bitów wyniku.
- **Nacisk**: Z1 i Z2; TFT_eSPI i XPT2046_Touchscreen liczą `z = Z1 + 4095 − Z2` i uznają dotyk powyżej progu
  (ok. 300–400). Bez dotyku: Z1 ≈ 0, Z2 ≈ 4095 → z ≈ 0.
- **PENIRQ (T_IRQ)**: stan niski, gdy panel jest dotknięty (przy PD = 00). Część bibliotek najpierw sprawdza ten pin.
- Zegar: biblioteki czytają wolno – TFT_eSPI `SPI_TOUCH_FREQUENCY` 2,5 MHz, XPT2046_Touchscreen 2 MHz.

### Dlaczego PIO, a nie sprzętowe SPI slave Pico

- SPI slave musi mieć odpowiedź w FIFO **przed** komendą, a XPT2046 odpowiada 1 takt po bajcie kontrolnym
  i wynik zależy od kanału w tej komendzie.
- SPI slave RP2350 (PL022) w trybie 0 (CPHA = 0) wymaga zmiany CS po każdej ramce – biblioteki trzymają T_CS nisko
  przez całą serię. (Do potwierdzenia w datasheet RP2350, rozdział SPI.)
- PIO robi to samo co przy odczycie ID: liczy bity od zbocza CS, rozpoznaje bajt kontrolny (bit S), oddaje kanał
  do tablicy odpowiedzi (DMA) i nadaje BUSY + 12 bitów. Wspólny kod z ProjektID, inny tylko program PIO
  (start po bicie S zamiast po D/C = 0).

### Magistrala: wspólna z LCD albo osobna – oba warianty

- **Wspólna (domyślna)**: SCK, MOSI, MISO jak LCD, osobny **T_CS**. Tak łączy TFT_eSPI i większość modułów ILI9341
  z dotykiem. Wymaga MISO (GP8) z ProjektID. Przechwytywanie LCD pomija ruch przy CS LCD = 1, więc odczyt dotyku
  mu nie przeszkadza.
- **Osobna**: np. płytka CYD ma XPT2046 na własnym SPI. Osobne SCK / DIN / DOUT / T_CS (+ T_IRQ), druga maszyna PIO,
  ten sam kod odpowiedzi.

## Kontrolery pojemnościowe (I2C)

Prostsze: Pico ma sprzętowy **I2C slave** (pico-sdk `i2c_slave`), a I2C sam wstrzymuje zegar (clock stretching),
więc nie ma problemu z czasem. Odpowiedzi z rejestrów w pamięci, aktualizowanych przy każdym „dotyku” ze strony.
Dodatkowo wyjście **INT** (impuls albo stan niski przy dotyku).

### CST816S (adres 0x15) – do sprawdzenia w datasheet / bibliotekach

| Rejestr | Znaczenie |
|---------|-----------|
| 0x01 | gest (0 = brak, 1–4 przesunięcia, 5 klik, 0x0B podwójny, 0x0C długi) |
| 0x02 | liczba palców (0/1) |
| 0x03 / 0x04 | X: bity 11..8 w 0x03 (bity 7..6 = zdarzenie), bity 7..0 w 0x04 |
| 0x05 / 0x06 | Y: analogicznie |
| 0xA7 | ChipID (0xB4 CST816S, 0xB5 CST816T, 0xB6 CST816D) |
| 0xA9 | wersja firmware |
| 0xFE | wyłączenie automatycznego uśpienia |

Biblioteki czytają zwykle 6 bajtów od 0x01. Prawdziwy CST816S usypia się i nie odpowiada na I2C bez dotyku –
emulator może odpowiadać zawsze (bezpieczniejsze dla bibliotek).

### FT6236 / FT6206 (adres 0x38) – do sprawdzenia w datasheet / bibliotekach

| Rejestr | Znaczenie |
|---------|-----------|
| 0x02 | TD_STATUS – liczba punktów (0..2) |
| 0x03 / 0x04 | P1: zdarzenie (bity 7..6), X bity 11..8; X bity 7..0 |
| 0x05 / 0x06 | P1: ID dotyku (bity 7..4), Y bity 11..8; Y bity 7..0 |
| 0x09–0x0E | P2 (drugi punkt) |
| 0x80 | próg (TH_GROUP) – zapisywany przez bibliotekę, emulator zapamiętuje |
| 0xA3 | CHIPID (0x06 FT6206, 0x36 FT6236, 0x64 FT6336) |
| 0xA8 | VENDID (0x11 FocalTech) – Adafruit_FT6206 sprawdza go przy `begin()` |

GT911 (adres 0x5D/0x14, rejestry 16-bitowe, konfiguracja z sumą kontrolną) – później, osobno.

## Skąd brać dotyk

- Kliknięcie lub przeciągnięcie myszą (albo palcem na telefonie) na stronie Podgląd → WebSocket → Pico.
  Nowe wiadomości np. `touch:x,y` (współrzędne panelu po odwróceniu obrotu podglądu) i `touch:up`.
- **XPT2046**: przeliczenie pikseli na surowe wartości ADC (typowo ok. 200–3900) z ustawialną kalibracją:
  zakresy X/Y, zamiana osi, odwrócenie osi; Z stała wartość (np. z ≈ 1000 przy dotyku). Możliwy import 5 liczb
  kalibracji TFT_eSPI (`calData`).
- **I2C**: współrzędne w pikselach panelu w orientacji natywnej (tak raportuje kontroler), opcjonalnie zamiana / odwrócenie osi.
- Na podglądzie: znacznik miejsca dotyku i licznik odczytów dotyku (widać, czy program w ogóle pyta).

## Etapy

| Etap | Zakres |
|------|--------|
| 0 | ProjektID etap 1: wspólne MISO (GP8), PIO nadające, tablica odpowiedzi DMA |
| 1 | XPT2046 na wspólnej magistrali + T_IRQ, dotyk z podglądu, kalibracja na stronie Wyświetlacz |
| 2 | XPT2046 na osobnej magistrali (druga maszyna PIO) |
| 3 | CST816S i FT6236 przez I2C slave + INT |
| później | GT911, gesty (CST816S), dwa punkty (FT6236) |

## Piny (propozycja – do potwierdzenia)

Zajęte: GP2–GP7 (LCD), GP8 (MISO, ProjektID), GP10–GP14 i GP28 (nadajnik testów). GP0/GP1 zostawione na UART.

| Sygnał | Pin | Uwagi |
|--------|-----|-------|
| T_CS (wspólna magistrala) | GP9 | obok pinów LCD |
| T_IRQ / PENIRQ (wyjście) | GP15 | stan niski = dotyk |
| Osobna magistrala: T_CLK, T_DIN, T_DOUT, T_CS, T_IRQ | GP16, GP17, GP18, GP19, GP20 | kolejne GPIO (wygodne dla PIO) |
| I2C (SDA, SCL), INT | GP26, GP27, GP22 | I2C1; nie koliduje z osobną magistralą |

## Otwarte pytania

1. Jaki dotyk mają moduły, z którymi będzie używany emulator (XPT2046, CST816S, FT6236…)?
2. Piny T_CS i T_IRQ – czy propozycja GP9 / GP15 pasuje do płytki?
3. Którą biblioteką testować etap 1 (TFT_eSPI `getTouch`, XPT2046_Touchscreen, LovyanGFX `getTouch`)?
   LcdTester ma już LovyanGFX – sprawdzić, czy ma test dotyku.
