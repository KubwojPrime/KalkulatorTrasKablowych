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

Wbudowany konfigurator BAKS zawiera 96 popularnych wariantów, w tym 41 korytek
KCJ/KCOJ H42, H50, H60, H80, H100 i H110, a także pozostałe koryta, drabinki,
pokrywy, wysięgniki, podstawy, zaciski i pręty gwintowane. Użytkownik
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
- Qt 6.5 lub nowszy z modułami Core, Gui, Widgets i Sql;
- MinGW 64-bit albo MSVC;
- podmoduł QXlsx;
- NSIS 3.x do utworzenia instalatora (nie jest wymagany dla samej kompilacji).

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

Skrypt pobiera numer wersji bezpośrednio z konfiguracji CMake, uruchamia testy
oraz test startu gotowej paczki. Powstają:

- `release\KalkulatorTrasKablowych-<wersja>-win64.zip`;
- `release\KalkulatorTrasKablowych-<wersja>-win64-setup.exe`;
- `release\SHA256SUMS-<wersja>.txt`.

Przy pakowaniu z certyfikatem Early Access skrypt dodaje także publiczny plik
`.cer` i tekstowy opis podmiotu, ważności oraz odcisków certyfikatu. Klucz
prywatny nie jest kopiowany do paczki ani repozytorium.

Bez zainstalowanego NSIS można zbudować tylko paczkę przenośną poleceniem
`.\scripts\package-windows.ps1 -SkipInstaller`.

Pełny lokalny test instalacji systemowej należy uruchomić w PowerShellu
otwartym jako administrator. W procesie bez podwyższonych uprawnień wykonywany
jest test paczki przenośnej, a test instalatora jest odkładany do GitHub CI,
gdzie pozostaje obowiązkową bramką wydania.

## Eksport rysunku CAD

W zakładce „3. Wyniki i przekrój” wybierz „Eksportuj przekrój i tabelę do DXF…”.
Ta sama funkcja jest dostępna w menu Plik. Eksport zapisuje edytowalny DXF
(AutoCAD 2007, UTF-8), w milimetrach, w skali 1:1, bez dodatkowych bibliotek CAD.
Obrys trasy i okręgi kabli mają wspólny układ z podglądem: największe średnice
są układane jako pierwsze. Numery w okręgach odpowiadają pozycjom tabeli.
Kable wystające poza obrys są czerwone, na warstwie PRZEPELNIENIE.

Tabela pod przekrojem zawiera producenta, oznaczenie, kod, ilość, średnicę,
masę jednostkową, obciążenie ogniowe kabla i CPR. Jest zbudowana z edytowalnych
linii i tekstów, a nie z obiektu tabeli AutoCAD. Zachowuje brakujące dane
i oznaczenia oszacowań (*). Eksport odrzuca błędne ilości/średnice i więcej
niż 100 000 sztuk, zamiast zapisywać niepełny rysunek. DXF można otworzyć
w programie CAD i zapisać jako DWG.

## Instalator Windows

Instalator prowadzi użytkownika przez standardowe ekrany:

1. powitanie;
2. treść EULA i obowiązkową akceptację licencji;
3. wybór katalogu instalacji;
4. wybór skrótów;
5. postęp instalacji i ekran końcowy z możliwością uruchomienia programu.

Instalator prosi system Windows o uprawnienia administratora. Domyślna
instalacja dla wszystkich użytkowników jest wykonywana do
`%ProgramFiles%\Kalkulator Tras Kablowych`, ale użytkownik może wskazać inny
katalog. Ze względów bezpieczeństwa instalator przyjmuje tylko katalog pusty
albo rozpoznany katalog wcześniejszej instalacji — zapobiega to usunięciu
obcych plików przez deinstalator.

Skrót w menu Start dla wszystkich użytkowników jest domyślnie włączony. Skrót
na wspólnym pulpicie jest opcjonalny. Instalator zapisuje wpis aplikacji w
64-bitowej gałęzi `HKLM`, rejestruje program na liście zainstalowanych aplikacji
Windows i tworzy wymagający administratora deinstalator w wybranym katalogu.

Wydania Early Access używają samopodpisanego certyfikatu Authenticode. Publiczny
certyfikat, jego odciski oraz instrukcja weryfikacji są dołączone do wydania.
Szczegóły opisano w
[`docs/early-access-signature.md`](docs/early-access-signature.md).

## Licencja i dostęp

Aplikacja działa całkowicie offline. Nie ma serwera aktywacyjnego, telemetrii ani
technicznej blokady zainstalowanej kopii. Dostęp jest udzielany i może być
cofnięty pisemnie na zasadach `EULA.txt`. Model i jego ograniczenia opisano w
[`docs/license-and-access.md`](docs/license-and-access.md).

Numer wersji w aplikacji, właściwościach EXE, nazwach paczek i instalatorze
pochodzi z deklaracji `project(... VERSION ...)` w `CMakeLists.txt`. Wysłanie tagu
`v<wersja>` uruchamia publikację stabilnych paczek oraz sum SHA-256 w GitHub
Releases. Tagi `v<wersja>-early-access.<numer>` przechodzą ten sam pełny build i
testy, ale podpisane artefakty są publikowane ręcznie jako pre-release.

## Status odpowiedzialności

Program jest narzędziem wspierającym projektowanie. Nie zastępuje oceny projektanta,
rzeczoznawcy ds. zabezpieczeń przeciwpożarowych ani sprawdzenia aktualnych kart
producenta, norm, aprobat i warunków zastosowania konkretnego systemu trasowego.
