# Masa trasy i elementów montażowych BAKS

## Zakres

Konfigurator BAKS służy do obliczenia masy własnej systemu trasy przypadającej
na metr jej długości. Baza `resources/baks-catalog.json` zawiera 96 wybranych
popularnych produktów:

- korytka KGR i KGL/KCL H42;
- 41 wariantów korytek KCJ/KCOJ H42, H50, H60, H80, H100 i H110;
- drabinki DKP H60 i DUP H100;
- pokrywy PKL i PKJ;
- wysięgniki WS, WPL i WWS/WWSO;
- podstawę sufitową PSDDN, wieszak pręta WPV i zacisk ZS/ZSO;
- pręty gwintowane PGM8, PGM10 i PGM12.

Każdy rekord wskazuje symbol, numer katalogowy, masę, jednostkę, plik źródłowy,
stronę PDF (jeżeli dotyczy) i aktualną kartę online producenta.

## Model obliczeniowy

Elementy mają jedną z czterech ról:

1. `route` - koryto lub drabinka, masa w `kg/m` trasy;
2. `cover` - pokrywa, masa w `kg/m` trasy;
3. `fixed` - element stały, masa w `kg/szt.` na jeden punkt podparcia;
4. `vertical` - odcinek elementu pionowego o masie w `kg/szt.` i znanej
   długości katalogowej.

Program oblicza:

```text
M_stałe [kg/podporę] = Σ(ilość × masa katalogowa [kg/szt.])

M_pionowe [kg/m zwieszenia] =
  Σ(ilość × masa katalogowa odcinka [kg/szt.] / długość odcinka [m])

M_trasy [kg/m] =
  M_koryta/drabinki + M_pokrywy
  + (M_stałe + wysokość zwieszenia × M_pionowe) / rozstaw podpór

M_instalacji [kg/m] = M_trasy + M_kabli
```

Przykład: dwa pręty `PGM10/3` o katalogowej masie `1,50 kg/szt.` i długości
`3 m` dają `2 × 1,50 / 3 = 1,00 kg/m zwieszenia`.

## XLSX i odtwarzalność

Eksport tworzy arkusz `Zestaw BAKS`. Zawiera on pełną migawkę wybranych
produktów, a nie jedynie wynik liczbowy. Import odtwarza symbol, numer
katalogowy, ilość, masę, jednostkę i źródło użyte w historycznym projekcie.

Ręczna zmiana któregokolwiek z czterech pól masowych w GUI usuwa powiązanie
z zestawem BAKS, aby raport nie przedstawiał nieaktualnego pochodzenia danych.

## Ograniczenia

Obliczenie obejmuje wyłącznie elementy faktycznie dodane przez użytkownika.
Śruby, nakrętki, łączniki i inne drobne części bez podanej przez producenta
masy nie są dopisywane automatycznie ani szacowane. Konfigurator masowy nie
sprawdza:

- dopuszczalnego obciążenia i ugięcia;
- zgodności materiałowej i powłoki antykorozyjnej;
- kompletności systemu zamocowania;
- klasy zachowania funkcji E30/E60/E90;
- wymagań instrukcji montażowej dla konkretnego zastosowania.

Dobór konstrukcyjny i pożarowy musi zostać zweryfikowany względem aktualnej
dokumentacji BAKS przez projektanta.
