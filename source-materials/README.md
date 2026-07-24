# Materiały źródłowe producentów

Ten katalog przechowuje niezmienione dokumenty pobrane z oficjalnych stron
producentów. Służą one jako ślad audytowy dla danych wprowadzanych do katalogu
aplikacji.

Stan archiwum na 2026-07-24:

- 24 pliki źródłowe: 14 PDF i 10 migawek HTML;
- 5 producentów;
- 2208 stron PDF;
- 235 961 741 bajtów (225,03 MiB).

Wygenerowany zestaw `2026-07-24.1` zawiera 25 065 wariantów. Raport zachowuje
cztery odrzucone wiersze BITNER: dwa bez parametrów liczbowych i dwa z podaną
przez katalog, fizycznie niespójną średnicą `106 mm`. Nie wprowadzono korekt
opartych na domyśle.

Szczegółowe adresy URL, daty pobrania, liczby stron, rozmiary i skróty SHA-256
znajdują się w pliku [`manifest.json`](manifest.json).

## Struktura

```text
source-materials/
  telefonika/
  elpar/
  bitner/
  cobicabling/
  baks/
  manifest.json
  extraction-report.json
  extraction-rejects.json
  SHA256SUMS.txt
```

## Weryfikacja integralności

Z katalogu głównego projektu:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\verify-source-materials.ps1
```

Skrypt sprawdza obecność każdego pliku, sygnaturę PDF albo HTML, rozmiar oraz
SHA-256 względem manifestu. Zmiana choćby jednego bajtu powoduje błąd.

## Zasady użycia danych w programie

Każdy rekord katalogowy w aplikacji wskazuje:

1. producenta;
2. dokładny plik z tego katalogu;
3. numer strony PDF;
4. oznaczenie kabla i wariant/przekrój z tabeli;
5. datę pozyskania źródła i status weryfikacji;
6. informację, czy parametr jest wartością dokładną, przybliżoną lub zakresem.

Dla średnicy zewnętrznej używanej we wzorze `ilość × D²` należy przyjmować
największą wartość producenta, jeśli katalog podaje tolerancję albo zakres.
Masa musi pochodzić bezpośrednio z tabeli producenta i zachowywać jednostkę
`kg/km`.

Klasa CPR, odporność ogniowa i obciążenie ogniowe są różnymi danymi.
Dla części wariantów TELE-FONIKA katalog podaje ciepło spalania w `kWh/m`;
zapis w aplikacji powstaje wyłącznie przez przeliczenie jednostek `1 kWh =
3,6 MJ`. Nie wolno obliczać `MJ/m` z klasy CPR ani wpisywać zera w miejsce
brakującej wartości. Pozostałe dane wymagają odrębnej karty producenta,
raportu badawczego albo pisemnego potwierdzenia producenta.

Dla produktów BAKS rekord zachowuje rolę elementu w zestawie, symbol, numer
katalogowy, masę i jednostkę `kg/m` albo `kg/szt.`. Pręty gwintowane są
normalizowane do `kg/m zwieszenia` przez podzielenie masy katalogowej odcinka
przez jego długość. Dane masowe nie zastępują doboru nośności, powłoki,
zamocowań, rozstawu podpór ani wymagań E30/E60/E90.

## Zakres i ograniczenia źródeł

- **TELE-FONIKA Kable**: aktualny katalog elektroenergetyczny 2026, katalog
  telekomunikacyjny 2025 oraz przewodnik CPR. Katalogi zawierają tabele średnic,
  mas i klasy reakcji na ogień. Część tabel katalogu elektroenergetycznego
  zawiera również ciepło spalania w `kWh/m`.
- **ELPAR**: katalogi elektroenergetyczne, telekomunikacyjne, sterownicze oraz
  bezhalogenowe, a także dwa materiały projektowe. Główne katalogi zawierają
  średnice i masy.
- **BITNER**: pełny katalog 2025 i materiał BiTLAN. Pełny katalog zawiera
  średnice, masy i informacje CPR. Warstwa tekstowa PDF ma miejscami niestandardowe
  kodowanie, dlatego tabele należy weryfikować również wizualnie.
- **CobiCabling**: katalog polski oraz nowszy katalog angielski 2025. Katalog
  angielski zawiera średnice zewnętrzne kabli sieciowych i klasy CPR, ale nie
  zapewnia kompletnej masy `kg/km` dla wszystkich pozycji.
- **BAKS**: dział PDF katalogu 2024/25 dla koryt i pokryw oraz niezmienione
  migawki aktualnych kart online 2026 dla drabinek i elementów montażowych.
  Baza obejmuje wybrane popularne warianty, a każda masa wskazuje konkretny
  plik/stronę lub kartę online.

## Ekstrakcja i kontrola

`tools/extract_cable_catalogs.py` skanuje źródła podstawowe, zachowuje
pochodzenie każdego wariantu, przy tolerancji średnicy wybiera maksimum i nie
zamienia braków masy lub `MJ/m` na zero. Niejednoznaczne wiersze trafiają do
`extraction-rejects.json`, a wszystkie rekordy automatyczne pozostają oznaczone
jako nieweryfikowane. Zależność ekstraktora jest przypięta w
`tools/requirements.txt` (pdfplumber, licencja MIT). Kontrola bez ponownego
czytania plików PDF:

```powershell
python .\tools\verify_catalog_dataset.py
```

## Aktualizacje

Nie należy nadpisywać istniejących plików nowszą treścią pod tym samym
identyfikatorem. Nowe wydanie zapisuje się pod nową nazwą, dodaje jako osobny
wpis manifestu i ponownie weryfikuje. Dzięki temu istniejące obliczenia można
odtworzyć względem wersji źródła użytej w dniu projektu.

## Prawa autorskie

Dokumenty pozostają własnością ich producentów i są przechowywane jako
materiał dowodowy w prywatnym repozytorium projektu. Przed publicznym
udostępnieniem repozytorium lub dalszą redystrybucją PDF należy sprawdzić
warunki publikacji każdego producenta. Manifest i skróty można publikować
niezależnie jako informację o pochodzeniu danych.

Pliki PDF są wersjonowane przez Git LFS. Po sklonowaniu repozytorium należy
mieć zainstalowane Git LFS i wykonać `git lfs pull`.
