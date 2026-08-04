# Historia zmian

Wszystkie istotne zmiany projektu są opisywane w tym pliku. Projekt używa
wersjonowania semantycznego.

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

### Zmieniono

- numer wersji z CMake jest jedynym źródłem wersji dla EXE, CI i paczek;
- model licencyjny jest całkowicie offline i opiera się na pisemnej licencji;
- import XLSX odrzuca nieprawidłowe, ujemne lub nieskończone wartości zamiast
  zastępować je po cichu wartościami domyślnymi.
