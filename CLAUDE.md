# CLAUDE.md – VirtualLcd na Raspberry Pi Pico 2 W

Opis dla uzytkownika jest w README.md. Ten plik zbiera wiedze zdobyta przy tworzeniu projektu,
ktorej nie ma w README ani w kodzie – zeby latwo wrocic do tematu.

## Zasady pracy

- Rozmawiaj z uzytkownikiem po polsku.
- Kompiluj tylko lokalnie na tym komputerze, nigdy w chmurze.
- Nie wgrywaj firmware (`-t upload`) bez wyraznej prosby – wgrywa uzytkownik.
- Gdy kompilacja zglasza bledy, pokaz je i zapytaj przed zmiana kodu.
- Gdy uzytkownik prosi o analize, przedstaw wnioski i propozycje – kod zmieniaj dopiero po jego zgodzie.

## Commity git

- Nie rób commitów automatycznie — tylko na wyraźną prośbę użytkownika.
- Autorem commitów jest wyłącznie użytkownik: jego tożsamość z `git config user.name` / `user.email`
  (zgodna z kontem GitHub), żeby był widoczny w Contributors.
- Bez `Co-Authored-By` i bez żadnych linii przypisujących autorstwo Claude.
- `include/secrets.h` (haslo WiFi) nigdy do repozytorium – jest w `.gitignore`.

## Cel i stan (listopad 2026)

Pico 2 W podlaczone do SPI wyswietlacza innego mikrokontrolera udaje sterownik LCD (ILI9341, ST7789),
a obraz pokazuje na stronie WWW. Program na mikrokontrolerze i jego biblioteka (Adafruit, LovyanGFX,
TFT_eSPI...) dzialaja bez zmian. **Dalsza praca tylko w tej wersji (Pico 2 W)** – wersja ESP32-S3
w katalogu nadrzednym jest porzucona (powody nizej).

Dziala: ILI9341 i ST7789 przy 10/20/40 MHz (sprawdzone testem petli i prawdziwym ESP32-C3 z
Adafruit_ILI9341 i LovyanGFX – projekt `../../LcdTester`), rozdzielczosci paneli, obroty, pin LED (PWM),
strony /, /lcd, /wifi, /test, ustawienia w flashu, RLE obrazu do przegladarki.

Ostatni commit: `d2bb895`. Po nim niezacommitowane (stan dzialajacy, wgrany na plytke):
RLE + statystyki przesylu, emulacja pinu LED, petla rdzenia 1 w RAM, diagnostyka `restartAt`/`offCmds`,
poprawka liczenia `smRestarts`.

## Sprzet i piny

- Raspberry Pi Pico 2 W (RP2350 + CYW43439), **bez PSRAM** (520 KB SRAM). Zegar 250 MHz.
- Emulator: MOSI GP2, D/C GP3, SCK GP4, CS GP5 – **musza byc kolejne** (PIO czyta je od `in_base`);
  RST GP6 (sprawdzany programowo w petli rdzenia 1), LED/BL GP7 (**musi byc nieparzysty** – kanal B PWM).
- Nadajnik testow (test petli, demo): sprzetowe SPI1 GP10 (SCK), GP11 (MOSI), GP12 D/C, GP13 CS, GP14 RST.
  Przewody: GP10→GP4, GP11→GP2, GP12→GP3, GP13→GP5, GP14→GP6. Przy pracy z innym MCU odlaczyc.
- Z innym mikrokontrolerem: koniecznie wspolna masa; Pico nie toleruje 5 V (AVR wymaga konwertera).
- Zegar SPI1 nadajnika wynika z dzielnika zegara peryferiow – "40 MHz" daje ok. 33 MHz (log podaje faktyczny).

## Framework i budowanie

- PlatformIO, platforma maxgerhardt (`platform-raspberrypi.git`), `board = rpipico2w`,
  arduino-pico (earlephilhower), pico-sdk 2.3. `setup()/loop()` = rdzen 0, `setup1()/loop1()` = rdzen 1.
- `tools/pioasm.py` (extra_script) generuje `*.pio.h` z `src/capture/*.pio` przed kompilacja.
- `board_build.filesystem_size = 64k` – partycja LittleFS na ustawienia.
- 250 MHz: arduino-pico dla RP2350 **nie podnosi napiecia** – robi to konstruktor globalny w `main.cpp`
  (`vreg_set_voltage(1,20 V)`, dziala przed `main()`, czyli przed zmiana zegara). Od 250 MHz arduino-pico
  samo zwalnia magistrale do CYW43 (dzielnik 4 zamiast 2) – WiFi troche wolniejsze.
- `printf` w arduino-pico **nie wypisuje float** (`%f` daje pusty tekst) – stad w `/status` pole `"ratio":`
  jest puste i psuje JSON (do poprawy: liczba calkowita, np. `ratioX10`).
- Wgrywanie: `upload_protocol = picotool`; za pierwszym razem tryb BOOTSEL.

## Przechwytywanie SPI (najwazniejsza wiedza)

### Program PIO (`spi_capture.pio`)
- Petla rozwinieta (25 instrukcji z 32): `wait 0 pin 3` (CS nisko na poczatku kazdego bajtu), potem 8×
  (`wait 1 pin 2`, `in pins, 2`, `wait 0 pin 2`). Na kazde narastajace zbocze SCK wsuwane sa MOSI i D/C.
  3 instrukcje na bit – przy 250 MHz zapas do ok. 60 MHz SPI. Tryb SPI 0.
- Autopush co 16 bitow = 1 slowo na bajt SPI. Uklad bitow: bity 15,14 = D/C,MOSI bitu 7 ... bity 1,0 = bitu 0.
  Bajt = bity parzyste; D/C bajtu = bit 1 (ostatni bit).
- CS sprawdzany tylko na poczatku bajtu – zegar innych urzadzen przy CS=1 jest pomijany.
- Filtr szpilek w PIO (podwojne `wait`) **odrzucony**: przy 40 MHz probka MOSI wypadalaby tuz przed
  opadajacym zboczem (brak marginesu).

### DMA i dekoder (rdzen 1)
- DMA 16-bit z RX FIFO do pierscienia 32 KB (`channel_config_set_ring`, max 2^15 B – wiecej sie nie da
  jednym kanalem). Licznik transferow 0x0FFFFFFF, ponowne uzbrojenie w `poll()` gdy kanal stanie
  (raz na ~270 mln bajtow – daje wtedy 1 `rxStalls`). Zuzycie DMA liczone z rejestru `transfer_count`,
  co daje dokladne wykrywanie przepelnien (`pending > ring`).
- Dekoder: tablica 64 KB (slowo → bajt) – jeden odczyt na bajt. Bajty danych leca ciasna petla do bufora 2 KB,
  komenda najpierw oddaje zebrane dane sterownikowi. Hot path (`PioSpiCapture::poll`, `VirtualLcd::onData/
  onCommand`, `MipiDcsDriver::command/data/onParam/putPixel/writePixels`, `loopCore1`) jest w RAM
  (`__not_in_flash_func`) – kod z flasha konkuruje o cache XIP z rdzeniem 0 (WiFi, RLE) i to bylo widac
  jako przeciazenie.
- Przy 40 MHz i LovyanGFX (wiele krotkich komend, ok. 1 na 45 bajtow, 30 pelnych klatek/s = 4,6 MB/s)
  obciazenie rdzenia 1 w szczycie ok. 76 %, pojedyncze przepelnienia przy dlugich seriach (fillScreen).
- `smRestarts`: gdy CS=1, a PIO stoi w srodku bajtu – restart SM. PC = offset+1 (czeka na pierwsze
  zbocze nowego bajtu) **to nie blad** (po ostatnim bajcie CS byl jeszcze chwile nisko) – nie restartujemy.
  Wczesniej to dawalo 45 tys. falszywych alarmow.

### Historia wydajnosci (co pomoglo)
1. Szybka sciezka pikseli w sterowniku dzialala tylko bez MV – `setRotation(1)` (MV) szlo wolno → 99 %
   i artefakty. Teraz krok ±1 albo ±szerokosc dla wszystkich rotacji.
2. Tablica 64 KB + petla odcinkami pierscienia + kod w RAM + 250 MHz: 99 % → 72 %.
3. Pomiar LED na rdzeniu 1 (kod z flasha w kazdej petli) podniosl znow do 99 % – przeniesiony na rdzen 0.

## Sterowniki LCD

- `MipiDcsDriver`: CASET/RASET/RAMWR/RAMWRC, MADCTL, COLMOD 16/18 bit, INVON/OFF, DISPON/OFF, SLPIN/OUT.
- Odwzorowanie MADCTL → pamiec (FrameBuffer = obraz jak na panelu): najpierw zamiana osi (MV:
  x = wiersz, y = kolumna), potem lustra na osiach pamieci. Zgodne z Adafruit (rotacja 1 = obrot w prawo
  dla obu sterownikow).
- Cechy paneli (`PanelTraits`): ILI9341 – panel BGR i lustro X (Adafruit w rotacji 0 wysyla MX|BGR);
  ST7789 – RGB, bez lustra, panel IPS odwrocony (biblioteki wysylaja INVON).
- **ILI9341 0xB6 (Display Function Control), 2. parametr: SS (0x20) odwraca poziomo, GS (0x40) pionowo,
  niezaleznie od MADCTL.** Adafruit wysyla `08 82 27`, LovyanGFX `08 C2 27` (GS=1) i w rotacji 0 MADCTL bez MX –
  bez obslugi B6 obraz z LovyanGFX byl w lustrze. Przy LovyanGFX rotacja 0 jest obrocona o 180° wzgledem Adafruit.
- Rozdzielczosci: lista `kPanels` w kazdej klasie sterownika – okno w pamieci (x, y, w, h) jak colstart/rowstart
  bibliotek. ST7789 135×240: x=52 jak TFT_eSPI (Adafruit uzywa 53 – przesuniecie o 1 kolumne).
  Test petli liczy przesuniecie w rotacji 1: kolumny od `panel.y`, wiersze od `W - panel.x - szerokosc`.
- GC9A01 (dodany 15.11.2026, jeszcze nie sprawdzony z prawdziwa biblioteka): cechy jak ILI9341 (BGR, lustro X,
  Adafruit_GC9A01A w rotacji 0 wysyla MX|BGR) + IPS jak ST7789 (INVON); obsluga 0xB6 SS/GS jak w ILI9341.
  Panel okragly: `PanelSize::round` -> pole `round` w info WS -> podglad z `border-radius:50%`.
  Do sprawdzenia z TFT_eSPI/LovyanGFX, czy w rotacji 0 obraz nie jest w lustrze.
- ST7735 (dodany 15.11.2026, nie sprawdzony z prawdziwa biblioteka): `St7735Base` (pamiec 132x162, panel w lustrze
  X i Y - biblioteki w rotacji 0 wysylaja MX|MY, stad `PanelTraits::mirrorY`) + wersje ukladu `St7735Driver`,
  `St7735RDriver`, `St7735SDriver` (osobne pliki, tylko kName; roznia sie sekwencja startowa w testach).
  Rozdzielczosci w `St7735Base::kPanels` (dziedziczone): 128x160 (2,1 BGR), 128x128 (2,3 BGR), 80x160 (26,1 RGB IPS).
  Cechy zalezne od szyby sa w `PanelSize::bgr/inverted` (-1 = cecha sterownika), sterownik je bierze w `setPanel()`,
  a nadajnik testow po sekwencji wysyla MADCTL (`LcdProfile::madctlFor`) i INVON wg panelu.
  Niepewne: wiersz okna 1.44" (Adafruit 3, inne 1, zmieniaja go przy obrocie), okno 0.96" (TFT_eSPI 26,1 /
  Adafruit 24,0) i RGB/BGR modulow 0.96". Moduly red/black tab (okno 0,0, RGB) = kolejny wpis w kPanels.
  `LcdInitCmd::params` ma 16 bajtow (gamma ST7735). Test petli liczy oczekiwane bajty z `SpiLcdMaster::bytesSent()`.
- Strona /lcd: model LCD i rozdzielczosc to listy rozwijane wypelniane z `drivers` w `/api/settings` –
  nowy sterownik pojawia sie na stronach sam. Przez WebSocket lista idzie raz, po "hello" klienta
  (`{"t":"drivers"}`), a nie w kazdym `info`; bufor listy `kDriversJsonSize` = 2 KB.
- Nowy sterownik: klasa (zwykle pochodna `MipiDcsDriver`) z `kName` i `kPanels` + wpis w `kDrivers` w `VirtualLcd.cpp`;
  dla testu petli i demo profil w `kProfiles` (`test/SpiLcdMaster.cpp`) z nazwa `Klasa::kName` (bez literalu).

## Pin LED (podswietlenie)

- Licznik PWM w trybie `PWM_DIV_B_HIGH` liczy takty przy stanie wysokim (dziala dla PWM i stanu stalego),
  dzielnik 256, okno 50 ms. 16-bitowy licznik przy 250 MHz/256 starcza na ok. 65 ms – **okna dluzsze niz
  60 ms sa pomijane** (rdzen 0 potrafi stac 100–200 ms przy wysylaniu obrazu; przepelnienie dawalo losowe
  przyciemnienia).
- Opcja "Pomin pin LED" domyslnie wlaczona – bez przewodu pin czyta 0 i obraz bylby czarny.

## Strona WWW i przesyl

- `WiFiServer` + wlasny WebSocket (SHA1 z biblioteki `Hash`, base64 wlasny). Do 3 klientow; odswiezenie
  strony zamyka stare polaczenie tej samej przegladarki ("bye"); klient bez "ping" przez 8 s jest rozlaczany.
- Testy (petla, demo) dzialaja w petli rdzenia 0 (`runPending`), a w trakcie wywoluja `web_.poll()` przez
  `SpiLcdMaster::idle()` – strona dostaje obraz na zywo. `poll()` ma ochrone przed rekurencja.
- Ramka obrazu: naglowek uint16 [x, y, w, h] LE + piksele RGB565 LE; **bit 14 w x = RLE**:
  slowo z bitem 15 = powtorzenie ((s&0x7FFF)+1) × nastepne slowo; bez bitu 15 = (s+1) pikseli kopii.
  Powtorzenia od 3 pikseli; gdy RLE nic nie daje – piksele surowe. Fragmenty po 8 K pikseli.
- Zapis ramki WS (`writeAll`): WiFi potrafi stanac na 1-4 s przy duzym ruchu. Dawniej `setTimeout(200)` + zamkniecie
  przy niepelnym zapisie dawalo "Brak polaczenia" co kilka sekund testu (licznik `web.err`). Teraz timeout 1,5 s
  i dopisywanie reszty ramki do 5 s - podglad chwile stoi, ale polaczenie zostaje.
- Bufor obrazu przydzielany raz na najwiekszy sterownik (`FrameBuffer::reserve`) - malloc przy zmianie sterownika
  zawodzil po fragmentacji (GC9A01 -> ILI9341).
- Zmierzone: WiFi Pico 2 W ok. **0,75–0,85 MB/s**, RLE srednio 4–5× na typowych ekranach.
- **Pasy przy pelnych, za kazdym razem innych klatkach to rwanie po stronie WWW**, nie blad przechwytywania:
  klatka 150 KB idzie ok. 180 ms, fragmenty sa kopiowane na biezaco, wiec pochodza z roznych klatek.
  Proba "migawki" (kopia calego obszaru naraz, tablica dekodera zmniejszona do 2×256 B) **pogorszyla** obraz
  i zostala cofnieta. Mozliwe kierunki na przyszlosc: migawka na granicy klatki (RAMWR od (0,0)) bez blokowania
  dekodera i jako opcja do wlaczenia, albo podglad w polowie rozdzielczosci przy szybkich zmianach.
- `/status` (JSON) – glowne narzedzie diagnostyki, czytane z komputera przez curl (Pico pod 192.168.100.138
  w sieci uzytkownika): `capture.overruns`, `maxPending`, `peakLoadPct`, `peakKBps`, `rxStalls`, `smRestarts`,
  `restartAt[]`, `offCmds`, `web.sendKBps`, `led`.

## Strony PL / EN (23.11.2026)

- Kazda strona istnieje w dwoch wersjach: polska w `Pages.h` / `TestHtml.h`, angielska w `PagesEn.h` /
  `TestHtmlEn.h` (te same id elementow i logika JS). Czesci niezalezne od jezyka sa w makrach `VLCD_CSS_*`
  i `VLCD_JS_MAIN_VIEW` / `VLCD_JS_TEST_VIEW`. **Zmiana tekstu albo logiki strony = zmiana w obu wersjach.**
- Jezyk wybiera serwer (`english_` w `WebServer`, wczytany w `begin()` z `/ui.bin`) - bez JS po stronie
  przegladarki, wiec pierwsza strona po starcie jest juz w zapisanym jezyku. Przelacznik PL/EN w `VLCD_NAV`
  wysyla `POST /api/lang` (lang=pl|en, zapis tylko przy zmianie) i przeladowuje strone; `GET /api/lang`
  sprawdza strona przywrocona z pamieci przegladarki (wstecz). Strony maja `Cache-Control: no-cache`.
- Komunikaty bledow API sa tlumaczone przez `tr(pl, en)`; log testow (z firmware) zostaje po polsku.
- Waski ekran: przy <=480 px widac tylko przycisk drugiego jezyka, przy <=400 px znika logo (polskie nazwy
  zakladek ledwo sie mieszcza). Przy 320 px belka byla za szeroka juz przed zmiana.

## Ustawienia (LittleFS)

- Pliki `/lcd.bin`, `/wifi.bin` i `/ui.bin` (`UiSettings`, jezyk stron): [magic "VLCD"][sizeof struktury][struktura]. Zmiana rozmiaru struktury =
  stary plik ignorowany (wartosci domyslne). **Nie zmieniac magic** – uniewazniloby tez WiFi.
  `LcdSettings` ma `reserved[4]` na przyszlosc.
- Zapis do flasha wstrzymuje rdzen 1 (arduino-pico) – tylko po "Zapisz" na stronie.
- WiFi przy starcie: zapisane → `secrets.h` → otwarty AP "VirtualLCD" 192.168.4.1. CYW43 nie laczy sie
  z siecia pracujac jako AP – ponowna proba co 3 min, tylko gdy nikt nie jest podlaczony do AP.

## Dlaczego nie ESP32-S3 (wiedza z wersji porzuconej)

- Zwykly SPI slave nie widzi D/C. LCD_CAM (kamera) zapisuje 1 bajt na kazde zbocze SCK – przy 40 MHz to
  40 mln bajtow/s do zdekodowania programowo; limit ok. 20 MHz.
- Pulapki S3: VSYNC trzeba podac odwrocony (jak sterownik kamery IDF); EOF przychodzi przy opadajacym CS
  nastepnej transakcji; pierwsze 2 probki po starcie potrafia zginac (resynchronizacja na zmianie D/C);
  IDF 5.5 nie wola handlera WS przy handshake; domyslne `-Og` i FREERTOS_HZ=100 trzeba zmienic.
- PIO w RP2350 sklada bity sprzetowo – stad przejscie na Pico (40 MHz bez problemu).

## Pomysly na przyszlosc

- Polaczenie z projektem HDMI (Feather RP2350 HSTX ma 8 MB PSRAM, ale nie ma WiFi).
- Kolejne sterowniki (ST7735, ILI9488, SSD1306/SH1106), przewijanie (VSCRDEF/VSCRSADD), odczyt ID przez MISO.
- Test petli dla wszystkich 4 rotacji.
- Poprawka pola `ratio` w `/status`.
