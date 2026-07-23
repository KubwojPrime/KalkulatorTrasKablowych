# Bezpłatna dystrybucja i kontrola dostępu

## Model

Aplikacja pozostaje własnościowa, ale właściciel może udostępniać jej skompilowane
kopie bez pobierania opłat. Qt jest linkowane dynamicznie na warunkach LGPLv3,
a QXlsx na warunkach MIT.

Możliwość cofnięcia dostępu jest cechą aplikacji, nie licencji bibliotek. Produkcyjna
kompilacja może wymagać zdalnej autoryzacji przy starcie:

```text
POST /v1/verify
Content-Type: application/json

{
  "licenseKey": "...",
  "installationId": "...",
  "application": "KalkulatorTrasKablowych",
  "version": "0.1.0"
}
```

Odpowiedź:

```json
{
  "allowed": true,
  "message": "Dostęp aktywny"
}
```

Ustawienie `allowed: false` cofa dostęp dla klucza przy następnym sprawdzeniu.
W razie braku sieci ostatnie pozytywne potwierdzenie działa przez skonfigurowany
okres offline. Jawna odpowiedź odmowna usuwa ten okres.

## Ograniczenia

- Serwer licencyjny nie jest jeszcze częścią repozytorium.
- Obecny mechanizm sprawdza dostęp przy uruchomieniu.
- Zabezpieczenie klienta nigdy nie jest absolutne; zdeterminowany użytkownik może
  próbować modyfikować plik wykonywalny.
- Natychmiastowe cofnięcie dostępu wymaga cyklicznej kontroli podczas działania i
  stałego połączenia, co pogarsza odporność aplikacji na awarie sieci.
- Produkcyjne wdrożenie powinno używać podpisanych tokenów, pinowania domeny,
  podpisu pliku wykonywalnego, dziennika decyzji i zgodnej z RODO polityki danych.

Przed udostępnianiem programu klientom należy przygotować regulamin/EULA i politykę
prywatności oraz poddać je przeglądowi prawnemu.
