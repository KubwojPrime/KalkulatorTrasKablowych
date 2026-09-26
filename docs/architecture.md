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

## Model katalogu

Katalog rozdziela:

- producenta i rodzinę;
- pełne oznaczenie oraz kod katalogowy;
- średnicę używaną do obliczeń (maksimum przy podanej tolerancji lub zakresie);
- masę;
- obciążenie ogniowe `MJ/m` wraz z metodą i źródłem;
- klasę CPR;
- datę źródła, status weryfikacji oraz pełne pochodzenie rekordu.

Zestaw JSON ma własną wersję. SQLite synchronizuje rekordy oficjalne przy
pierwszym uruchomieniu nowej wersji zestawu, pozostawiając wpisy użytkownika.
Brak masy i brak `MJ/m` są przechowywane jako wartości nieznane, nigdy jako zero.

Katalog BAKS powinien być oddzielnym modułem danych systemu trasowego: koryta,
pokrywy, łączniki i zestawy zawieszeń. Nie należy mieszać rekordów tras z rekordami
kabli.
