# Checklista wydania 1.0

## Blokujące przed 1.0.0

- [x] Jedno źródło numeru wersji dla programu, EXE, CI i nazw paczek.
- [x] Przenośna paczka ZIP oraz instalator all-users wymagający administratora.
- [x] Automatyczny test startu pliku EXE i instalatora.
- [x] Testy zgodności starszego schematu i odrzucania uszkodzonych XLSX.
- [x] Test wydajności na pełnym katalogu około 25 tys. rekordów.
- [x] Pisemna licencja offline bez zależności od serwera autoryzacji.
- [x] Trzy projekty referencyjne z niezależnie wyprowadzonymi wynikami i testem obiegu XLSX/DXF.
- [x] Autosave, odzyskiwanie i ostrzeganie przed utratą pracy; pamięć folderu eksportu.
- [x] Ograniczony podgląd dużych list bez obcinania obliczeń.
- [x] Testy aktualizacji instalacji ze znacznikiem i bez niego oraz zachowania plików użytkownika.
- [x] Blokada publikacji niepodpisanych artefaktów CI.
- [x] Automatyczny test integralności źródeł i wygenerowanego katalogu w CI.
- [ ] Test wszystkich głównych operacji GUI na Windows 10 i Windows 11.
- [ ] Przegląd treści EULA przez prawnika przed dystrybucją komercyjną.
- [x] Early Access: podpis cyfrowy EXE i instalatora samopodpisanym certyfikatem
  z jawnym odciskiem i instrukcją zaufania.
- [x] Świadomie zachowany SSC i licencja offline zgodnie z decyzją autora.
- [ ] Odbiór RC na docelowym Windows oraz otwarcie DXF w używanym CAD.
- [ ] Potwierdzenie kompletu źródeł i licencji zależności do szerokiej dystrybucji (docs/third-party-rights.md).

## Procedura wydania

1. Ustawić wersję w `CMakeLists.txt` i uzupełnić `CHANGELOG.md`.
2. Uruchomić `scripts/verify-source-materials.ps1` oraz
   `tools/verify_catalog_dataset.py`.
3. Uruchomić kompilację Release, wszystkie testy i `scripts/package-windows.ps1`.
4. Sprawdzić pliki ZIP, instalator i `SHA256SUMS-<wersja>.txt` na czystym Windows.
5. Utworzyć tag `v<wersja>` albo `v<wersja>-rc.<n>`, poczekać na CI i użyć
   `scripts/publish-release.ps1` do publikacji podpisanych artefaktów.
6. Po publikacji sprawdzić pobranie, instalację, import przykładowego projektu,
   obliczenia, eksport XLSX oraz deinstalację.

## Po 1.0

Testy w rzeczywistych projektach są prowadzone po publikacji 1.0. Błędy bez
zmiany formatu danych są poprawiane w wersjach 1.0.x. Zmiany niezgodne wstecznie
wymagają wersji 1.x/2.0 oraz migracji plików XLSX.
