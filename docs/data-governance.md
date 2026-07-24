# Zasady utrzymania danych katalogowych

1. Każdy parametr musi wskazywać publiczną kartę, deklarację producenta albo
   zatwierdzony dokument otrzymany od producenta.
2. Wartości przybliżone są oznaczane jako przybliżone. Do rezerwy geometrycznej
   używana jest największa dostępna średnica zewnętrzna.
3. CPR, odporność ogniowa i obciążenie ogniowe są osobnymi polami.
4. Brak `MJ/m` pozostaje brakiem danych. Nie wolno zastępować go zerem ani
   wyprowadzać z klasy CPR.
5. Aktualizacja nie nadpisuje historii użytej w istniejącym projekcie. Eksport XLSX
   zachowuje wartości wykorzystane w obliczeniu oraz źródło.
6. Automatyczny import z katalogu powinien kończyć się kolejką ręcznej weryfikacji.
7. Dane producentów i systemów trasowych wymagają sprawdzenia zasad ich dalszego
   wykorzystania oraz publikacji.
8. Oficjalne dokumenty źródłowe są wersjonowane w `source-materials`; każdy plik
   ma zapisany URL, datę pobrania, rozmiar, liczbę stron i SHA-256.
9. Rekord katalogowy wskazuje dokładny plik, stronę PDF i wariant tabeli.
10. Aktualizacja źródła tworzy nowy plik i wpis manifestu. Nie nadpisuje źródła
    użytego przez istniejące projekty.
11. Integralność archiwum sprawdza `scripts/verify-source-materials.ps1`.
