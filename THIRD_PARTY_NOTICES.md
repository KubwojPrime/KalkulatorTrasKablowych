# Informacje o komponentach zewnętrznych

## Qt 6

Projekt korzysta dynamicznie z modułów Qt Core, Gui, Widgets i Sql.
Qt jest dostępne na zasadach licencji komercyjnej albo odpowiednich licencji open
source, w tym GNU Lesser General Public License v3. Sposób dystrybucji programu musi
spełniać warunki wariantu wybranego przez właściciela projektu.

Strona licencji: <https://www.qt.io/licensing/>

W planowanej bezpłatnej dystrybucji własnościowej należy używać dynamicznego
linkowania z modułami dostępnymi na LGPLv3, przekazać użytkownikowi wymagane
informacje i tekst licencji oraz nie ograniczać praw do wymiany bibliotek Qt.

## QXlsx 1.5.1.1

Biblioteka odczytu i zapisu XLSX, Copyright 2017-,
<https://github.com/QtExcel/QXlsx>. Licencja MIT.

Pełny tekst jest dostarczany w `third_party/QXlsx/LICENSE` i kopiowany do katalogu
`licenses` paczki Windows.

## pdfplumber 0.11.9 (narzędzie deweloperskie)

Generator danych katalogowych w `tools` korzysta z biblioteki pdfplumber
(<https://github.com/jsvine/pdfplumber>), udostępnianej na licencji MIT.
Biblioteka i jej zależności nie są dołączane do pliku wykonywalnego aplikacji.

## MinGW-w64 / GCC runtime

Wersja Windows budowana MinGW może zawierać biblioteki uruchomieniowe GCC i MinGW-w64.
Ich pliki licencyjne należy dołączyć do paczki produkcyjnej zgodnie z wersją
toolchaina użytego do kompilacji.

## NSIS (narzędzie do tworzenia instalatora)

Instalator Windows jest generowany przez Nullsoft Scriptable Install System
(NSIS), Copyright (C) 1999-2026 Contributors, na warunkach licencji zlib/libpng.
Skrypt projektu używa kompresji zlib. NSIS nie jest linkowany z właściwą aplikacją.

Tekst licencji i dokumentacja: <https://nsis.sourceforge.io/Docs/AppendixI.html>.
