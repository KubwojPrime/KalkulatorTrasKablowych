# Oszacowanie obciążenia ogniowego kabli

## Status wyniku

Wartość z karty producenta ma zawsze pierwszeństwo. Program wykonuje
oszacowanie tylko wtedy, gdy:

- producent nie podał `MJ/m`;
- z oznaczenia można rozpoznać materiał izolacji lub powłoki;
- dostępna jest średnica zewnętrzna albo masa kabla.

Każdy taki wynik jest oznaczony `*` i ma zapisany opis podstawy. Jest to
konserwatywne oszacowanie do wstępnej analizy, a nie wynik badania
kalorymetrycznego ani wartość deklarowana przez producenta.

Nie wolno używać oszacowania jako jedynej podstawy do rezygnacji z detekcji,
systemu zasysającego lub innych zabezpieczeń przeciwpożarowych. Taką decyzję
należy potwierdzić z projektantem zabezpieczeń ppoż. albo rzeczoznawcą.

## Metoda

Program rozpoznaje obecnie sześć grup materiałowych:

| Grupa | Współczynnik masowy `qₘ` | Współczynnik objętościowy `qᵥ` |
|---|---:|---:|
| PVC | 25 MJ/kg | 35 MJ/l |
| PE | 40 MJ/kg | 45 MJ/l |
| XLPE | 40 MJ/kg | 45 MJ/l |
| LSZH / bezhalogenowa | 40 MJ/kg | 45 MJ/l |
| guma / elastomer | 30 MJ/kg | 40 MJ/l |
| PUR / poliuretan | 30 MJ/kg | 40 MJ/l |

Współczynniki są celowo zaokrąglone w górę i zawierają margines projektowy.
Nie są wartościami normowymi przypisanymi do każdego wyrobu o danej nazwie
materiału.

Jeżeli oznaczenie przekroju jest jednoznaczne, program odejmuje pole i masę
rozpoznanych żył. Dla układów parowych lub niejednoznacznych nie wykonuje tego
odjęcia, co zwiększa wynik.

```text
A kabla = π × D² / 4
A niemetaliczna = A kabla - A rozpoznanych żył
Q geometria = A niemetaliczna [mm²] × qᵥ [MJ/l] / 1000

m niemetaliczna = masa kabla - masa rozpoznanych żył
Q masa = m niemetaliczna [kg/km] × qₘ [MJ/kg] / 1000

Q* = max(Q geometria, Q masa)
```

Dla żył miedzianych przyjęto `8,96 kg/(km·mm²)`, a dla aluminiowych
`2,70 kg/(km·mm²)`. Jeżeli którejś metody nie można zastosować, używana jest
druga. Jeśli nie można zastosować żadnej, `MJ/m` pozostaje nieznane.

## Podstawa techniczna i ograniczenia

- [ISO 1716:2018](https://www.iso.org/standard/70177.html) opisuje oznaczanie
  ciepła spalania brutto w kalorymetrze bombowym. Pomiar konkretnego wyrobu
  pozostaje dokładniejszy od oszacowania.
- [NIST Technical Note 1453, tabela 4](https://nvlpubs.nist.gov/nistpubs/Legacy/TN/nbstechnicalnote1453.pdf)
  podaje dla badanych komponentów kabla między innymi `18,36 MJ/kg` dla
  powłoki, `23,39 MJ/kg` dla izolacji żyły i `17,00 MJ/kg` dla wypełnienia.
- [NIST — test XLPE](https://www.nist.gov/node/1839876) przyjmuje
  `40 MJ/kg`, a zmierzona efektywna wartość wyniosła około `37,5 MJ/kg`.
- [NIST — test wyrobu gumowego](https://www.nist.gov/el/fcd/transient-combustion-calorimetry-tcc/test082gardenhoser2)
  używa wartości `30 MJ/kg`.

Rzeczywista receptura PVC, PE/XLPE, mieszaniny LSZH lub elastomeru zależy od
producenta i może zawierać plastyfikatory, wypełniacze mineralne, ekrany,
pancerze oraz inne składniki. Z tego powodu współczynniki materiałowe nie są
zamiennikiem danych `MJ/m` z karty konkretnego kabla.
