# Kalkulator Tras Kablowych

Interfejs programu wykorzystuje spójny motyw ciemny o wysokim kontraście.

Natywna aplikacja Windows do obliczania:

- wypełnienia trasy według konserwatywnego wzoru `Σ(ilość × D²)`;
- masy kabli na metr bieżący;
- masy koryta lub drabinki, pokrywy i elementów zawieszenia dobranych
  z katalogu BAKS albo wprowadzonych ręcznie;
- obciążenia ogniowego kabli w `MJ/m`;
- schematycznego ułożenia kabli od największej średnicy;
- importu i eksportu projektu oraz raportu w formacie XLSX.

Katalog obsługuje wyszukiwanie tokenowe z logiką AND (np. `YKY 3 x 2,5`)
oraz filtry producenta, typu/rodziny kabla, izolacji lub powłoki, klasy CPR
i odporności ogniowej (np. `PH90`). Separatory `x`, `×` i `G` oraz zapis
dziesiętny z przecinkiem lub kropką są traktowane równoważnie.

Wersjonowana baza kabli jest generowana z oficjalnych katalogów TELE-FONIKA,
ELPAR, BITNER i CobiCabling. Każdy rekord zachowuje producenta, wariant, kod
katalogowy, dokładny plik i stronę PDF, adres źródłowy oraz metodę ekstrakcji.
Zestaw `2026-07-24.1` zawiera 25 065 wariantów: 13 525 BITNER, 8 616 ELPAR,
2 896 TELE-FONIKA Kable i 28 CobiCabling. Rekordy pozyskane automatycznie mają
status „do weryfikacji”.

Wbudowany konfigurator BAKS zawiera 55 popularnych wariantów koryt H42, drabinek
H60/H100, pokryw, wysięgników, podstaw, zacisków i prętów gwintowanych. Użytkownik
buduje rzeczywisty zestaw na punkt podparcia, podaje liczbę elementów, wysokość
zwieszenia i rozstaw podpór. Każda pozycja zachowuje symbol, numer katalogowy,
masę, jednostkę oraz źródło. Szczegóły modelu opisano w
[`docs/baks-route-mass.md`](docs/baks-route-mass.md).

## Materiały źródłowe

Oficjalne katalogi TELE-FONIKI, ELPAR, BITNER, CobiCabling i BAKS są przechowywane
w katalogu [`source-materials`](source-materials/README.md). Manifest zawiera
oryginalne adresy URL, datę pobrania, liczbę stron, rozmiar i SHA-256 każdego
pliku. Integralność całego archiwum można sprawdzić poleceniem:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\verify-source-materials.ps1
```

Spójność wygenerowanego zestawu danych i jego powiązanie z archiwum można
sprawdzić poleceniem:

```powershell
python .\tools\verify_catalog_dataset.py
```

Dla części wariantów TELE-FONIKA katalog podaje ciepło spalania w `kWh/m`;
program przelicza je jednostkowo przez `3,6` na `MJ/m`. Gdy producent nie podał
tej wartości, program może wykonać konserwatywne oszacowanie dla rozpoznanego
materiału izolacji/powłoki. Każde takie oszacowanie jest oznaczone `*`, zachowuje
opis przyjętej metody i nie zastępuje danych producenta. CPR nie jest używane
jako zamiennik. Szczegóły i ograniczenia opisano w
[`docs/fire-load-estimation.md`](docs/fire-load-estimation.md).

## Zasady obliczeń

Wypełnienie:

```text
pole zarezerwowane = Σ(ilość × D²)
pole trasy = szerokość wewnętrzna × wysokość wewnętrzna
wypełnienie [%] = 100 × pole zarezerwowane / pole trasy
```

Obciążenie masowe:

```text
masa kabli [kg/m] = Σ(ilość × masa katalogowa [kg/km] / 1000)
masa podpór [kg/m] =
  (masa bazowa podpory + wysokość zwieszenia × masa elementów pionowych) / rozstaw podpór
masa kompletna = kable + koryto + pokrywa + podpory
```

W trybie katalogowym BAKS:

```text
masa bazowa podpory = Σ(ilość × masa elementu BAKS [kg/szt.])
masa elementów pionowych =
  Σ(ilość × masa odcinka BAKS [kg/szt.] / długość odcinka [m])
```

Brak masy katalogowej nie jest traktowany jako zero. Pokazana suma jest wtedy
oznaczona jako wartość minimalna.

Obciążenie ogniowe:

```text
obciążenie ogniowe [MJ/m] =
  Σ(ilość × wartość producenta lub oznaczone * oszacowanie kabla [MJ/m])
```

Brak wartości `MJ/m` i brak możliwości wiarygodnego oszacowania nie są traktowane
jako zero. Raport oddziela część potwierdzoną, oszacowaną `*` i nadal nieznaną.
Klasa CPR nie jest przeliczana na obciążenie ogniowe.

## Aktualizacja katalogu aplikacji

Po dodaniu lub zmianie oficjalnych materiałów:

```powershell
python -m pip install -r .\tools\requirements.txt
python .\tools\extract_cable_catalogs.py
python .\tools\verify_catalog_dataset.py
```

Generator zapisuje dane aplikacji w `resources/catalog-seed.json`, pełne
podsumowanie w `source-materials/extraction-report.json` oraz odrzucone,
niejednoznaczne wiersze w `source-materials/extraction-rejects.json`.

## Budowanie na Windows

Wymagania:

- CMake 3.24 lub nowszy;
- Qt 6.5 lub nowszy z modułami Core, Gui, Widgets, Sql i Network;
- MinGW 64-bit albo MSVC;
- podmoduł QXlsx.

```powershell
git submodule update --init --recursive
cmake --preset windows-debug
cmake --build --preset windows-debug --parallel 4
ctest --preset windows-debug
```

Lokalne presety wskazują Qt 6.11.0 i MinGW 13.1 z domyślnej instalacji
`C:\Qt`. Inne środowisko może przekazać własne `CMAKE_PREFIX_PATH` i kompilator.

Paczka Windows:

```powershell
.\scripts\package-windows.ps1
```

Archiwum powstaje w `release\KalkulatorTrasKablowych-<wersja>-win64.zip`.

## Kontrola dostępu

Domyślna kompilacja deweloperska nie wymaga serwera licencyjnego. Wersję
dystrybucyjną z możliwością cofnięcia dostępu buduje się przykładowo tak:

```powershell
cmake -S . -B build/licensed -G "MinGW Makefiles" `
  -DCMAKE_PREFIX_PATH=C:/Qt/6.11.0/mingw_64 `
  -DKTK_LICENSE_REQUIRED=ON `
  -DKTK_LICENSE_ENDPOINT=https://licencje.example.com/v1/verify `
  -DKTK_OFFLINE_GRACE_HOURS=24
```

Kontrakt serwera i ograniczenia mechanizmu opisano w
[`docs/license-and-access.md`](docs/license-and-access.md).

## Status odpowiedzialności

Program jest narzędziem wspierającym projektowanie. Nie zastępuje oceny projektanta,
rzeczoznawcy ds. zabezpieczeń przeciwpożarowych ani sprawdzenia aktualnych kart
producenta, norm, aprobat i warunków zastosowania konkretnego systemu trasowego.
