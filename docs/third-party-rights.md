# Biblioteki zewnętrzne: prawa i wymiana

Aplikacja korzysta z dynamicznych bibliotek Qt 6 na LGPLv3. Teksty LGPLv3 i
GPLv3 znajdują się w `licenses`. Prawa do bibliotek są niezależne od EULA
aplikacji. Wolno je modyfikować, zastępować i wykonywać inżynierię wsteczną
w zakresie potrzebnym do debugowania takich modyfikacji.

Zamknij aplikację, wykonaj kopię jej folderu i w kopii zastąp odpowiednie
`Qt6*.dll` oraz odpowiadające im wtyczki (`platforms`, `sqldrivers`, itd.).
Użyj kompatybilnej wersji ABI, architektury x64 i tego samego toolchaina MinGW.
Uruchom EXE z tej kopii. Program nie wymaga podpisu autora dla bibliotek.
QXlsx używa też prywatnych nagłówków Qt, więc bezpiecznym punktem wyjścia jest
dokładnie ta sama wersja Qt z własnymi poprawkami. Numer wersji DLL można
sprawdzić we właściwościach pliku; nie należy mieszać zestawów bibliotek.

Źródła Qt i informacje o warunkach:

- https://download.qt.io/archive/qt/ — archiwa odpowiednich wersji (podfolder `single` lub `submodules`);
- https://code.qt.io/ — repozytoria modułów;
- https://www.qt.io/development/open-source-lgpl-obligations;
- https://www.gnu.org/licenses/lgpl-3.0.html.

Dystrybutor musi zapewnić dostęp do odpowiadających dystrybuowanym bibliotekom
źródeł na warunkach ich licencji, także w razie modyfikowania Qt. Powyższe linki
identyfikują upstream; nie zastępują obowiązków dystrybutora. Przed szeroką
dystrybucją należy utrwalić źródła dokładnego zestawu bibliotek i informacje
o licencjach jego zależności. Ta czynność pozostaje punktem odbioru wydania.

QXlsx: https://github.com/QtExcel/QXlsx — MIT; tekst w `licenses/QXlsx-MIT.txt`.
GCC/MinGW: teksty runtime oraz wyjątku GCC w `licenses`.
NSIS: https://nsis.sourceforge.io/Docs/AppendixI.html.

EULA jest projektem warunków autora, a nie opinią prawną ani potwierdzeniem
zgodności każdej formy dystrybucji. Przegląd prawny nie został wykonany.
