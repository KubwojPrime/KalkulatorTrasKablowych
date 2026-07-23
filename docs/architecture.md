# Architektura

## Moduły

- `domain` — model projektu i deterministyczne obliczenia;
- `data` — wersjonowany katalog kabli w SQLite, inicjalizowany z kontrolowanego JSON;
- `io` — import i eksport XLSX z numerem wersji schematu;
- `ui` — interfejs Qt Widgets i schemat przekroju trasy;
- `licensing` — opcjonalna brama zdalnej autoryzacji.

Rdzeń obliczeniowy nie zależy od interfejsu ani XLSX, dzięki czemu może być
testowany na zestawach referencyjnych.

## Wizualizacja

Każda sztuka kabla jest reprezentowana okręgiem o średnicy zewnętrznej `D`.
Instancje są sortowane stabilnie, malejąco według `D`, a następnie układane
warstwami od dna trasy. Kolor czerwony oznacza wyjście poza geometryczny obrys.

Wizualizacja jest schematem obliczeniowym. Nie uwzględnia promieni gięcia,
uchwytów, przegród, odstępów EMC ani szczegółowej technologii montażu.

## Rozwój katalogów

Docelowo katalog powinien rozdzielać:

- producenta i rodzinę;
- pełne oznaczenie oraz kod katalogowy;
- średnicę nominalną, minimalną i maksymalną;
- masę;
- obciążenie ogniowe `MJ/m` wraz z metodą i źródłem;
- CPR oraz niezależne parametry odporności ogniowej;
- datę źródła, datę weryfikacji i status wycofania.

Katalog BAKS powinien być oddzielnym modułem danych systemu trasowego: koryta,
pokrywy, łączniki i zestawy zawieszeń. Nie należy mieszać rekordów tras z rekordami
kabli.
