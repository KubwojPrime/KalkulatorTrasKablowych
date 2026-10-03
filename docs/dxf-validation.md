# Kontrola eksportu DXF

Eksport wykorzystuje pełny pusty dokument DXF 2007 (AC1021) z poprawnymi
tabelami, blokami przestrzeni modelu/papieru, układami i słownikami.
Szablon `resources/dxf-template.txt` wygenerowano narzędziem deweloperskim
ezdxf 1.4.4 (MIT). Aplikacja C++ nie uruchamia Pythona ani ezdxf.
Geometria i tabela kabli pozostają edytowalnymi obiektami LINE/CIRCLE/TEXT.

Odtworzenie szablonu:

```powershell
python -m pip install ezdxf==1.4.4
python tools/dxf_support.py generate resources/dxf-template.txt
```

Szablon ma właściciela modelu `17`; uchwyty generowanych obiektów zaczynają się
od `10000` (hex), a HANDSEED jest rezerwowany powyżej wszystkich obiektów.
Generator sprawdza te założenia. Plik jest osadzony w zasobach aplikacji.

Test regresyjny eksportuje projekt z trzema kablami, przepełnieniem, polskimi
znakami, brakującymi danymi i oszacowanym obciążeniem ogniowym.
`tools/dxf_support.py validate <plik-testowy.dxf>` najpierw kontroluje surowe
rekordy, następnie odczytuje plik zwykłym czytnikiem (nie trybem recover),
wymaga braku błędów i napraw audytora oraz sprawdza geometrię i treść.
Walidator dotyczy tego konkretnego projektu testowego, nie dowolnej listy kabli.

Poprzednie testy przepuszczały niepełne tabele i brak właścicieli obiektów,
ponieważ tolerancyjny czytnik uzupełniał część braków podczas wczytywania.
Kontrola surowych rekordów wykrywa stary format przed taką normalizacją.

Zgodność z konkretną przeglądarką internetową CAD wymaga testu w tym serwisie;
poprawny audyt pliku nie stanowi dowodu wsparcia formatu przez każdy serwis.
