# Historia zmian

Wszystkie istotne zmiany projektu są opisywane w tym pliku. Projekt używa
wersjonowania semantycznego.

## [1.0.0-rc.1] - 2026-09-26

- Autosave z odzyskiwaniem projektu, zachowaniem BAKS i niedokończonych edycji.
- Zapamiętywanie folderu eksportu i pytanie o niezapisane zmiany.
- Atomowy zapis XLSX; ograniczenie podglądu do 5000 kabli bez obcinania obliczeń.
- Rozpoznawanie istniejącej instalacji po metadanych EXE, również bez znacznika.
- Deinstalacja wyłącznie plików paczki; zachowanie plików użytkownika.
- Trzy projekty wzorcowe i automatyczne testy sesji GUI oraz aktualizacji.
- Kontrolowana publikacja podpisanych paczek także dla stabilnych tagów.
- Rozbudowana EULA offline i instrukcja praw do bibliotek zewnętrznych.

## [0.7.0] - 2026-09-25

- Eksport edytowalnego przekroju i tabeli kabli do DXF, skala 1:1 w mm.
- Numery pozycji kabli na rysunku, warstwy CAD i czerwone oznaczenie przepełnienia.
- Wspólna geometria podglądu i eksportu, bez obcinania ilości do 1000 sztuk.
- Testy eksportu, geometrii, polskich znaków i ochrony pliku przy błędnych danych.

## [0.6.0] - w przygotowaniu

### Dodano

- instalator Windows NSIS obok przenośnego archiwum ZIP;
- test uruchomienia aplikacji i gotowej paczki w trybie bez interfejsu;
- testy importu starszego schematu XLSX oraz ręcznie uszkodzonych plików;
- kontrolę czasu ładowania i filtrowania pełnego katalogu kabli;
- sumy SHA-256 artefaktów wydania i automatyczną publikację po tagu.
- opcjonalne podpisywanie Authenticode pliku EXE i instalatora certyfikatem z
  lokalnego magazynu Windows;
- eksport publicznego certyfikatu i odcisku dla wydań Early Access.
- wybór katalogu instalacji z ochroną niepustych obcych katalogów;
- wybór skrótu w menu Start i opcjonalnego skrótu na pulpicie;
- poprawne kodowanie Unicode treści EULA w instalatorze.
- poprawiono walidację katalogu instalacji: komunikat pojawia się dopiero po
  kliknięciu „Dalej”, a nie podczas każdego kliknięcia lub wpisywania ścieżki.
- poprawiono rozpoznawanie istniejącego pustego katalogu przez pomijanie
  technicznych wpisów `.` i `..`; dodano odpowiadający test regresji.
- instalator i deinstalator wymagają uprawnień administratora, domyślnie używają
  64-bitowego `Program Files` oraz rejestrują aplikację i skróty dla wszystkich
  użytkowników w `HKLM`.

### Zmieniono

- numer wersji z CMake jest jedynym źródłem wersji dla EXE, CI i paczek;
- model licencyjny jest całkowicie offline i opiera się na pisemnej licencji;
- import XLSX odrzuca nieprawidłowe, ujemne lub nieskończone wartości zamiast
  zastępować je po cichu wartościami domyślnymi.
