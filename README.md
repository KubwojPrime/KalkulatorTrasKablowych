# Kalkulator Tras Kablowych

Interfejs programu wykorzystuje spójny motyw ciemny o wysokim kontraście.

Natywna aplikacja Windows do obliczania:

- wypełnienia trasy według konserwatywnego wzoru `Σ(ilość × D²)`;
- masy kabli na metr bieżący;
- opcjonalnej masy koryta, pokrywy i zawieszeń;
- obciążenia ogniowego kabli w `MJ/m`;
- schematycznego ułożenia kabli od największej średnicy;
- importu i eksportu projektu oraz raportu w formacie XLSX.

Projekt jest na etapie pierwszego działającego kamienia milowego. Startowa baza zawiera
jedynie kilka zweryfikowanych pozycji demonstracyjnych ELPAR. Rozszerzenie katalogów
TELE-FONIKA, ELPAR, Bitner, CobiCabling i BAKS jest osobnym, kontrolowanym strumieniem
danych — każdy rekord musi mieć źródło i datę weryfikacji.

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

Obciążenie ogniowe:

```text
obciążenie ogniowe [MJ/m] = Σ(ilość × wartość katalogowa kabla [MJ/m])
```

Brak wartości `MJ/m` nie jest traktowany jako zero. Raport wyraźnie oznacza wynik
niepełny. Klasa CPR nie jest przeliczana na obciążenie ogniowe.

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
