# Materiały źródłowe producentów

Ten katalog przechowuje niezmienione dokumenty pobrane z oficjalnych stron
producentów. Służą one jako ślad audytowy dla danych wprowadzanych do katalogu
aplikacji.

Stan archiwum na 2026-07-24:

- 13 plików PDF;
- 4 producentów;
- 1999 stron;
- 144 565 988 bajtów (137,87 MiB).

Szczegółowe adresy URL, daty pobrania, liczby stron, rozmiary i skróty SHA-256
znajdują się w pliku [`manifest.json`](manifest.json).

## Struktura

```text
source-materials/
  telefonika/
  elpar/
  bitner/
  cobicabling/
  manifest.json
  SHA256SUMS.txt
```

## Weryfikacja integralności

Z katalogu głównego projektu:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\verify-source-materials.ps1
```

Skrypt sprawdza obecność każdego pliku, sygnaturę `%PDF-`, rozmiar oraz
SHA-256 względem manifestu. Zmiana choćby jednego bajtu powoduje błąd.

## Zasady użycia danych w programie

Każdy rekord katalogowy w aplikacji powinien wskazywać:

1. producenta;
2. dokładny plik z tego katalogu;
3. numer strony PDF;
4. oznaczenie kabla i wariant/przekrój z tabeli;
5. datę weryfikacji;
6. informację, czy parametr jest wartością dokładną, przybliżoną lub zakresem.

Dla średnicy zewnętrznej używanej we wzorze `ilość × D²` należy przyjmować
największą wartość producenta, jeśli katalog podaje tolerancję albo zakres.
Masa musi pochodzić bezpośrednio z tabeli producenta i zachowywać jednostkę
`kg/km`.

Klasa CPR, odporność ogniowa i obciążenie ogniowe są różnymi danymi.
W obecnym zestawie nie znaleziono bezpośrednich wartości kaloryczności w
`MJ/m`. Nie wolno obliczać `MJ/m` z klasy CPR ani wpisywać zera w miejsce
brakującej wartości. Takie dane wymagają odrębnej karty producenta, raportu
badawczego albo pisemnego potwierdzenia producenta.

## Zakres i ograniczenia źródeł

- **TELE-FONIKA Kable**: aktualny katalog elektroenergetyczny 2026, katalog
  telekomunikacyjny 2025 oraz przewodnik CPR. Katalogi zawierają tabele średnic,
  mas i klasy reakcji na ogień.
- **ELPAR**: katalogi elektroenergetyczne, telekomunikacyjne, sterownicze oraz
  bezhalogenowe, a także dwa materiały projektowe. Główne katalogi zawierają
  średnice i masy.
- **BITNER**: pełny katalog 2025 i materiał BiTLAN. Pełny katalog zawiera
  średnice, masy i informacje CPR. Warstwa tekstowa PDF ma miejscami niestandardowe
  kodowanie, dlatego tabele należy weryfikować również wizualnie.
- **CobiCabling**: katalog polski oraz nowszy katalog angielski 2025. Katalog
  angielski zawiera średnice zewnętrzne kabli sieciowych i klasy CPR, ale nie
  zapewnia kompletnej masy `kg/km` dla wszystkich pozycji.

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
