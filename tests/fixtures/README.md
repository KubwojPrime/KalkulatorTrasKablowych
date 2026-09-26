# Projekty referencyjne (dane syntetyczne)

Wartości oczekiwane wyprowadzono niezależnie od funkcji kalkulatora:

- mixed: pole = 2×10²+20² = 600 mm²; 600/5000 = 12%; masa kabli =
  (2×100+500)/1000 = 0,7 kg/m; trasa = 2+0,2+(0,5+2×0,3)/2 = 2,75 kg/m;
  komplet = 3,45 kg/m; ogień = 2×1,5+2,5 = 5,5 MJ/m (w tym 2,5 oszacowane).
- missing: pole = 5² = 25 mm²; wypełnienie = 0,5%; masa i ogień nieznane;
  znane sumy wynoszą 0, ale nie oznaczają kompletnego wyniku zerowego.
- empty: brak kabli; masa trasy = 1,2+0,3+(1+0,5×0,4)/1,5 = 2,3 kg/m.

Testy porównują te wartości, wykonują obieg XLSX i eksport DXF, a następnie
sprawdzają odzyskanie również niedokończonej edycji i pochodzenia danych BAKS.
Nie jest to weryfikacja danych producentów ani zatwierdzenie projektu budowlanego.
