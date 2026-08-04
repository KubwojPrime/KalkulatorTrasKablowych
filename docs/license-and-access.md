# Licencja pisemna i praca offline

## Model dla wersji 1.0

Aplikacja jest własnościowa i może być bezpłatnie udostępniana wskazanym osobom
lub firmom. Prawo używania wynika z pisemnej zgody autora oraz z warunków
`EULA.txt`. Program nie wymaga konta, serwera aktywacyjnego ani połączenia z
Internetem.

Wersja 1.0 nie:

- wysyła klucza licencyjnego ani identyfikatora instalacji;
- wykonuje zdalnej kontroli dostępu;
- posiada telemetrii ani automatycznego sprawdzania aktualizacji;
- pozwala autorowi technicznie wyłączyć już zainstalowanej kopii.

## Cofnięcie zgody

Autor może cofnąć udzieloną licencję w formie pisemnej. Jest to mechanizm umowny:
adresat ma obowiązek zaprzestać korzystania z aplikacji i usunąć kopie zgodnie z
otrzymanym zawiadomieniem. Autor może też odmówić przekazywania kolejnych wersji.

Brak technicznej blokady jest świadomą decyzją. Upraszcza wdrożenie w firmach,
eliminuje zależność od dostępności serwera i nie wymaga gromadzenia danych
użytkowników.

## Dane lokalne

Katalog aplikacji jest kopiowany do lokalnej bazy SQLite użytkownika. Projekty
XLSX są zapisywane wyłącznie w lokalizacji wskazanej przez użytkownika. Program
nie przesyła tych danych. Kopie zapasowe i kontrola dostępu do plików należą do
organizacji używającej programu.

## Komponenty zewnętrzne

Qt jest linkowane dynamicznie na warunkach LGPLv3, QXlsx jest używane na
warunkach MIT, a instalator powstaje przy użyciu NSIS na warunkach zlib/libpng.
Paczka zawiera wymagane informacje oraz teksty licencji. Własnościowa EULA nie
ogranicza praw użytkownika wynikających z licencji komponentów zewnętrznych.

Przed szeroką dystrybucją komercyjną treść EULA powinna zostać sprawdzona przez
prawnika właściwego dla jurysdykcji autora i odbiorców.
