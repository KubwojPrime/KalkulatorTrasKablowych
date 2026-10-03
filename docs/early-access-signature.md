# Podpis cyfrowy Early Access

Pliki wykonywalne wydań Early Access są podpisane samopodpisanym certyfikatem
Authenticode. Podpis pozwala wykryć zmianę pliku po publikacji, ale Windows nie
ufa automatycznie temu certyfikatowi, ponieważ nie został wydany przez publiczny
urząd certyfikacji.

## Bezpieczna weryfikacja

1. Pobierz instalator, plik `.cer`, opis certyfikatu `.txt` i plik `SHA256SUMS`
   wyłącznie z tego samego wydania GitHub.
2. Porównaj SHA-256 wszystkich plików z `SHA256SUMS`.
3. Otwórz właściwości instalatora, kartę `Podpisy cyfrowe`, i sprawdź, czy podpis
   istnieje oraz czy odcisk certyfikatu odpowiada opisowi w wydaniu.
4. Nie instaluj certyfikatu jako zaufanego, jeśli odciski są inne.

## Opcjonalne zaufanie na komputerze testowym

Administrator firmy może rozprowadzić publiczny plik `.cer` do magazynów
`Zaufane główne urzędy certyfikacji` oraz `Zaufani wydawcy`. Należy zrobić to
wyłącznie na komputerach przeznaczonych do testów Early Access i dopiero po
niezależnym potwierdzeniu odcisku certyfikatu.

Zaufanie certyfikatowi samopodpisanemu oznacza zaufanie wszystkim plikom
podpisanym odpowiadającym mu kluczem prywatnym do czasu wycofania certyfikatu.
Publiczny plik `.cer` nie zawiera klucza prywatnego i sam nie umożliwia
podpisywania plików.
