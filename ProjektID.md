# Projekt: odczyt ID i rejestrów z emulowanego LCD

Stan: projekt, bez zmian w kodzie (15.11.2026).

**Priorytet: udoskonalenie, nie warunek działania.** Programy i biblioteki używane do tej pory działają poprawnie
z niepodłączonym MISO, więc emulator bez odczytów spełnia swoje zadanie. Odczyt ID to wstęp do ogólniejszego
zwracania danych przez emulowany LCD (rejestry stanu, później RAMRD).

**Funkcja wyłączana w ustawieniach** (strona Wyświetlacz, zapis w `LcdSettings`), **domyślnie wyłączona**:
GP8 zostaje wtedy wejściem w stanie wysokiej impedancji (bez pull-up, bez nadawania), maszyna PIO nadająca
i kanały DMA nie są uruchamiane, a w 3-wire Pico nigdy nie przejmuje linii SDA. Obecne zachowanie (tylko słuchanie)
pozostaje domyślne i bezpieczne dla każdego układu.

## Cel

Dziś Pico tylko **słucha** SPI. Wiele programów przy starcie czyta z wyświetlacza ID albo rejestry stanu
(np. TFT_eSPI `readcommand8()`, Adafruit `readcommand8()`, diagnostyka „Display Power Mode”) i bez odpowiedzi
dostaje 0x00 albo 0xFF. Celem jest, żeby Pico odpowiadało jak prawdziwy sterownik: ID układu i rejestry
liczone z bieżącego stanu (MADCTL, COLMOD, uśpienie, włączenie ekranu).

## Etapy

| Etap | Zakres | Warunek zakończenia |
|------|--------|---------------------|
| 1 | Odpowiedź przez **osobną linię MISO (4-wire)**: maszyna PIO nadająca, tabela odpowiedzi, rejestry stanu w `MipiDcsDriver`, ID dla **ILI9341** | LcdTester „Odczytaj ID” (MISO) daje OK dla wszystkich kroków ILI9341; test pętli czyta ID przy 10 MHz |
| 2 | Tryb **3-wire**: odpowiedź tą samą linią SDA (MOSI), przejmowanie linii tylko na czas odpowiedzi | Odczyt ID ST7789 w 3-wire bez konfliktu na linii (oscyloskop / analizator) |
| 3… | **Po kolei każdy sterownik** – tabela `kReads` według tego, czy dany LCD zwraca ID i jak (4-wire / 3-wire, bity dummy, komendy producenta) | Dla każdego sterownika: odczyt z jego typową biblioteką |
| później | `0x2E` RAMRD – odczyt pikseli z bufora obrazu (`readPixel`, `readRect`) | Strumień danych, a nie kilka bajtów – osobny projekt |

Kolejność sterowników w etapie 3 do ustalenia; propozycja: ST7789 → GC9A01 → ST7735/R/S.

## Sprzęt

### 4-wire – osobne MISO (etap 1)

- ILI9341 ma osobną linię SDO (MISO). Na Pico potrzebny jest jeden dodatkowy pin wyjściowy MISO.
- Pin MISO jest **w stanie wysokiej impedancji**, gdy CS = 1 i gdy Pico nie odpowiada. Nadaje tylko w czasie odpowiedzi.
- Wewnętrzny **pull-up** (ok. 50–80 kΩ) trzyma „1” na wolnej linii – nieznana komenda odczytu zwraca wtedy 0xFF,
  jak prawdziwy układ. **Nie używać wewnętrznego pull-down** – RP2350 ma erratum E9 dotyczące właśnie pull-down.
- Rezystor szeregowy niepotrzebny, gdy MISO nie jest współdzielone z innym urządzeniem SPI (np. kartą SD).
  Dla bezpieczeństwa: najsłabsza wydajność prądowa pinu (2 mA, `gpio_set_drive_strength`).
- Gdy MISO jest wspólne z kartą SD: Pico odpowiada wyłącznie przy CS LCD = 0 (sprawdzane w PIO), rezystor 330 Ω zalecany.

Piny (ustalone 15.11.2026):

| Sygnał | Pin Pico | Uwagi |
|--------|----------|-------|
| MISO emulatora (wyjście) | GP8 | nie koliduje z GP2–GP7 i nadajnikiem testów |
| RX nadajnika testów (SPI1) | GP28 | SPI1 RX może być na GP8, GP12, GP28; GP12 zajęty (D/C nadajnika) |

Test pętli: dodatkowy przewód GP8 → GP28.

### 3-wire – wspólna linia SDA (etap 2)

- ST7735, ST7789, GC9A01 w modułach zwykle pracują w 3-wire: odczyt idzie **tą samą linią SDA**. Biblioteka przełącza
  swój pin MOSI na wejście (TFT_eSPI `TFT_SDA_READ`), a Pico na czas odpowiedzi przejmuje linię (pin GP2 jako wyjście).
- **Rezystor szeregowy 330 Ω w linii SDA zalecany** – ogranicza prąd do ok. 10 mA, gdy obie strony nadają naraz.

Kiedy dochodzi do konfliktu na linii:
- **Przejście nadawca → odbiorca**: mikrokontroler wysyła komendę i przełącza pin na wejście; jeśli zrobi to za późno
  albo Pico zacznie za wcześnie, przez kilka–kilkadziesiąt ns obie strony sterują linią.
- **Koniec odczytu**: biblioteka podnosi CS i od razu nadaje, a Pico jeszcze nie zwolniło linii.
- **Fałszywy odczyt**: Pico uzna zwykłą komendę za odczyt i nadaje przeciw mikrokontrolerowi przez cały bajt –
  najgorszy przypadek, niszczy też dane obrazu. Dlatego odpowiedź tylko dla komend z tabeli i tylko przy CS = 0.
- **Restart którejś strony** w trakcie transmisji.

W 4-wire konflikt powstaje tylko przy błędzie (wspólne MISO z kartą SD, pomyłka w okablowaniu) i jest nieszkodliwy
dla mikrokontrolera, który tę linię tylko czyta.

W 3-wire przechwytywanie zobaczy na MOSI własną odpowiedź Pico (D/C = 1) – rdzeń 1 musi pomijać te bajty
(od komendy odczytu do CS = 1), żeby nie trafiły do sterownika jako dane.

## Czas odpowiedzi – główny problem

Odpowiedź musi wyjść na linię zaraz po bajcie komendy, po określonej liczbie **taktów dummy**:

| Sterownik | Komendy | Takty dummy | Pewność |
|-----------|---------|-------------|---------|
| ILI9341 (4-wire) | 0x04, 0x09, 0xD3 | 8 (bajt dummy) | z dokumentacji / bibliotek, do sprawdzenia |
| ILI9341 (4-wire) | 0x0A–0x0F | 0–8 | **do sprawdzenia** (diagramy odczytu w datasheet) |
| ST7789, ST7735S, GC9A01 | 0x04, 0x09 (24/32 bity) | 1 bit | do sprawdzenia |
| ST7789, ST7735S, GC9A01 | 0x0A–0x0F, 0xDA–0xDC (8 bitów) | 0 | do sprawdzenia |

- Przy bajcie dummy (8 taktów, przy 10 MHz = 800 ns) zdążyłby rdzeń 1, ale **przy 1 bicie dummy (100 ns przy 10 MHz)
  oprogramowanie nie zdąży** – rdzeń 1 widzi bajty z opóźnieniem pierścienia DMA i pętli `poll()`.
- Biblioteki czytają wolniej niż piszą (TFT_eSPI `SPI_READ_FREQUENCY`, zwykle 6–20 MHz) – nie trzeba celować w 40 MHz.

### Proponowane rozwiązanie: PIO nadające + tablica odpowiedzi przez DMA

1. **Osobna maszyna PIO (drugi blok PIO – blok z przechwytywaniem ma zajęte 25 z 32 instrukcji)** czyta te same
   piny SCK, MOSI, D/C, CS i sama liczy bity. Po bajcie z D/C = 0 (komenda) wstawia bajt komendy do RX FIFO
   i czeka na odpowiedź (`pull`).
2. **Dwa kanały DMA jako tablica przeglądowa**: kanał A bierze bajt komendy z RX FIFO i wpisuje go jako adres
   odczytu kanału B (`tabela + komenda × rozmiar wpisu`), kanał B kopiuje wpis z tablicy do TX FIFO maszyny.
   Opóźnienie kilkadziesiąt ns – mieści się w 1 bicie dummy przy częstotliwościach odczytu bibliotek.
3. **Wpis tablicy** (256 komend): nagłówek (liczba taktów dummy, liczba bitów odpowiedzi; 0 = to nie jest odczyt)
   + do 32 bitów danych. Maszyna po nagłówku 0 wraca do liczenia bajtów; inaczej odczekuje takty dummy,
   ustawia pin jako wyjście, nadaje bity na zbocze opadające SCK (tryb 0), zwalnia linię i czeka na CS = 1.
4. **Rdzeń 1 utrzymuje tablicę**: wpisy ID ze sterownika, a rejestry stanu aktualizuje przy zmianie
   (np. zapis MADCTL → wpis 0x0B), więc odpowiedź jest gotowa, zanim ktoś zapyta.

Do sprawdzenia w etapie 1: najkrótszy czas od ostatniego bitu komendy do pierwszego bitu odpowiedzi
(pomiar analizatorem przy 10 i 20 MHz) i zachowanie, gdy CS zostaje niskie między transakcjami (TFT_eSPI).

Wariant zapasowy dla samego ILI9341 (bajt dummy): rdzeń 1 wpisuje odpowiedź do TX FIFO po zdekodowaniu komendy,
a maszyna PIO sprawdza FIFO na granicy każdego bajtu. Prostsze, ale nie zadziała dla ST77xx.

## Dane w sterownikach

ID trzymane w klasie sterownika, obok `kName` i `kPanels`:

```cpp
struct ReadReg {
    uint8_t cmd;
    uint8_t dummyBits;  // takty dummy przed odpowiedzia
    uint8_t len;        // bajty odpowiedzi (1..4)
    uint8_t data[4];
};

class St7789Driver : public MipiDcsDriver {
    static constexpr ReadReg kReads[] = {
        {0x04, 1, 3, {0x85, 0x85, 0x52}},  // RDDID
        {0xDA, 0, 1, {0x85}},              // RDID1
        {0xDB, 0, 1, {0x85}},              // RDID2
        {0xDC, 0, 1, {0x52}},              // RDID3
    };
```

- **`MipiDcsDriver` odpowiada sam na wspólne rejestry liczone ze stanu**: 0x09 RDDST (status), 0x0A RDDPM (power mode:
  booster, SLPOUT, normal mode, DISPON), 0x0B RDDMADCTL (ostatni MADCTL), 0x0C RDDCOLMOD (ostatni COLMOD).
  Sterownik dopisuje tylko swoje ID i komendy producenta.
- **Komendy producenta zależne od stanu** – np. ILI9341: `0xD9` (nieudokumentowana) ustawia indeks bajtu, który zwróci
  następny odczyt `0xD3`; Adafruit i TFT_eSPI tak czytają `readcommand8(reg, index)`. Sterownik obsługuje to
  w `onParam()` i aktualizuje wpis tablicy.
- **Nieznana komenda odczytu**: Pico nie nadaje, linia zostaje w wysokiej impedancji, pull-up daje 0xFF – najbliżej
  prawdziwego układu. Odpowiedź tylko dla komend z tabeli (ochrona przed fałszywym odczytem w 3-wire).
- Nowy sterownik bez `kReads` = brak odpowiedzi na ID, ale rejestry stanu z `MipiDcsDriver` działają.

## ID sterowników

Wartości z pamięci (datasheety, źródła bibliotek) – **każdą sprawdzić w datasheet przed wpisaniem do kodu**.

| Sterownik | Komenda | Odpowiedź | Uwagi | Status |
|-----------|---------|-----------|-------|--------|
| ILI9341 | 0xD3 RDID4 | (dummy) 00 93 41 | wersja IC, model 9341; z `0xD9` – po jednym bajcie | prawie pewne |
| ILI9341 | 0x04 RDDID | (dummy) ID1 ID2 ID3 | wartości zależne od producenta modułu, często 00 00 00 | do sprawdzenia |
| ST7789(V) | 0x04 RDDID | 85 85 52 | 1 bit dummy | prawie pewne, zgodne z LcdTester |
| ST7789(V) | 0xDA / 0xDB / 0xDC | 85 / 85 / 52 | bez dummy | prawie pewne, zgodne z LcdTester |
| ST7735S | 0x04 RDDID | 7C 89 F0 | 1 bit dummy | prawie pewne, zgodne z LcdTester |
| ST7735S | 0xDA / 0xDB / 0xDC | 7C / 89 / F0 | bez dummy | zgodne z LcdTester, do sprawdzenia w datasheet |
| ST7735, ST7735R | 0x04, 0xDA–0xDC | prawdopodobnie jak ST7735S | | **niesprawdzone** |
| GC9A01 | 0x04 RDDID | 00 9A 01 (?) | w dokumentacji „ID2 = 9A, ID3 = 01” | **niesprawdzone** |

## Weryfikacja: LcdTester – „Odczytaj ID”

Etap 1 (i potem każdy sterownik) sprawdzany programem `../../LcdTester` (ESP32-C3). Do rysowania używa LovyanGFX,
ale **odczyt ID ma własny** (`src/LcdIdReader.cpp`, kroki w `src/LcdModels.cpp`), bez LovyanGFX:

- **bit-bang ok. 100 kHz** (pół okresu 5 µs), tryb 0: bit ustawiany przy SCK = 0, czytany 5 µs po zboczu narastającym,
- linia odczytu: **MISO** (pull-up po stronie ESP) albo **SDA/MOSI w 3-wire** (opcja „Odczyt ID przez SDA”,
  ESP przełącza swój MOSI na wejście z pull-up po bajcie komendy),
- komenda przy D/C = 0, potem D/C = 1 i odczyt przy **CS trzymanym nisko** od komendy do końca odczytu,
- przed odczytami: reset sprzętowy (RST) i `0x01` SWRESET,
- każdy krok porównywany z oczekiwanym ID w wyrównaniu wg dokumentacji i **przesuniętym o 1 bit**
  („OK (przesunięte o 1 bit)” – klony różnią się taktem dummy).

Sposoby odczytu ILI9341 (trzy sposoby ID + status):

| Krok | Przebieg na magistrali | Takty dummy | Oczekiwane | Co musi zrobić emulator |
|------|------------------------|-------------|------------|--------------------------|
| RDID4 metoda D9 | 3×: `D9` (osobny CS), dana `0x11`/`0x12`/`0x13` (osobny CS), potem `D3` i 9 bitów | 0 | 00, 93, 41 (po jednym bajcie) | zapamiętać parametr D9 (także przez CS = 1), na `D3` oddać od razu bajt nr (parametr − 0x10) odpowiedzi `00 93 41` |
| RDID4 bezpośrednio | `D3`, potem 32 bity | 8 | 00 93 41 | bajt dummy, potem 3 bajty |
| RDDID | `04`, potem 25 bitów | 1 | (tylko informacyjnie) | 1 bit dummy + 3 bajty; prawdziwy ILI9341 zwykle 00 00 00 |
| RDDST | `09`, potem 33 bity | 1 | (informacyjnie) | 1 bit dummy + 4 bajty stanu |

ST7789 i ST7735/ST7735S:

| Krok | Takty dummy | Oczekiwane ST7789 | Oczekiwane ST7735 |
|------|-------------|-------------------|-------------------|
| `04` RDDID | 1 | 85 85 52 | 7C 89 F0 |
| `DA` / `DB` / `DC` RDID1–3 | 0 | 85 / 85 / 52 | 7C / 89 / F0 |
| `09` RDDST | 1 | (informacyjnie) | (informacyjnie) |

**Wnioski dla projektu:**
- Przy 0 taktach dummy (`DA`–`DC`, metoda D9) pierwszy bit odpowiedzi musi być na linii **już przed pierwszym zboczem
  narastającym po komendzie** – Pico wystawia go zaraz po ostatnim zboczu opadającym bajtu komendy.
- Przy 100 kHz czasu jest dużo (5 µs na pół bitu), więc LcdTester **nie sprawdzi** zapasu czasowego. Rozwiązanie PIO + DMA
  i tak jest potrzebne dla bibliotek czytających z kilkoma MHz; czas sprawdzić osobno (nadajnik testów z SPI1 przy 10–20 MHz).
- LcdTester rozpoznaje też inne sterowniki po parze bajtów (np. `9A 01` = GC9A01) – przyda się w etapie 3.

## Jak biblioteki czytają (do sprawdzenia w źródłach przed etapem 1)

- **Adafruit_ILI9341** `readcommand8(reg, index)`: wysyła `0xD9` z parametrem `0x10 + index`, potem `reg` i czyta 1 bajt.
- **TFT_eSPI** `readcommand8/16/32()`: dla ILI9341 ta sama sztuczka z `0xD9`; dla ST77xx w 3-wire przełącza SDA na wejście
  (`TFT_SDA_READ`), czyta z obniżonym zegarem `SPI_READ_FREQUENCY`.
- **LovyanGFX** `readCommand()`: `dummy_read_bits` w konfiguracji panelu (np. 1 dla ST7789, 8 dla ILI9341) –
  dobre źródło liczby taktów dummy dla każdego sterownika.

## Dodatki

- **Ustawiane ID** na stronie Wyświetlacz: „domyślne” albo wpisane ręcznie, żeby emulować klony (np. ST7789 z innym ID)
  i sprawdzić, jak program reaguje na „zły” wyświetlacz. Zapis w `LcdSettings` (jest `reserved[4]`, ale ID może
  wymagać zmiany rozmiaru struktury – wtedy stare ustawienia wracają do domyślnych).
- **Diagnostyka**: licznik odczytów i ostatnia komenda odczytu w `/status` i na stronie Test – od razu widać,
  czego program pyta przy starcie.
- **Test pętli**: nadajnik (SPI1 z RX na GP28) czyta ID i rejestry stanu i porównuje z tabelą sterownika.
- **RAMRD 0x2E** – później, osobny projekt (strumień pikseli z bufora, konwersja do 18 bit jak w prawdziwym układzie).

## Otwarte pytania

1. Czy MISO w docelowym układzie będzie współdzielone z kartą SD?
2. Liczba wolnych kanałów DMA (przechwytywanie zajmuje 1) i maszyn PIO na RP2350 – sprawdzić przy etapie 1.
