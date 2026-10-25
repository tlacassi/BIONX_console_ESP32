# Analyse de 0000235.TXT — 23 octobre 2026

## Verdict

Compatible avec le format de trames principal pour **observation passive**.
Le firmware actuel ne remplace pas encore la console d’origine : le transport
MCP2515 est uniquement récepteur, le réveil et les séquences maître ne sont pas
implémentés, et le dashboard utilise des données simulées. Aucune validation
matérielle ne résulte du rejeu hôte.

## Capture analysée

Snapshot : 692 555 octets, SHA-256
`b704a8db353e7da995a27dade87a34f5f908f6a72511b26497ddce3437844a91`.
Le fichier a changé de taille pendant la première lecture ; les résultats
ci-dessous correspondent au snapshot identifié par cette empreinte.

CL1000 HW 8.1x / FW 5.85, session 291, split 1, CAN **125 000 bit/s**.
27 679 trames, de 00:00:07.727 à 00:02:34.465, soit **146,738 s**.
Moyenne : environ 189 trames/s ; maximum par seconde entière : **204**.
Toutes les données observées font 2 ou 4 octets, avec préfixe 00.

Selon la [documentation CL1000](https://canlogger.csselectronics.com/clx000-docs/cl1000/log/index.html),
les identifiants sont hexadécimaux ; Type 0 signifie réception standard et
Type 8 émission standard. Le fichier comprend **25 038 réceptions et 2 641
émissions du logger**. Il ne constitue donc pas une capture entièrement passive
de la seule console d’origine. Le champ Type 8 ne signifie ni RTR ni CAN étendu.
Le logger indique aussi `Silent mode: false`.

Les 504 retours en arrière d’horodatage dans l’ordre des lignes interdisent de
supposer le fichier chronologiquement trié. Résolution : 1 ms. Ne pas attribuer
sans réserve chaque réponse à la requête précédente, même après tri : réponses
partagées sur 0x08, requêtes concurrentes et égalités temporelles.

## Rejeu dans le code réel

Commande reproductible, sans émission CAN :

```sh
python3 tools/check_cl1000.py captures/0000235.TXT
cmake --build build-unit
ctest --test-dir build-unit --output-on-failure
```

Le script compile `components/bionx_protocol/bionx_protocol.c` avec le compilateur
hôte, appelle `bionx_parse()` sur chaque trame et vérifie registre et valeur brute
big-endian. Il ne simule ni l’ordonnancement ESP32, ni le SPI, ni le bus physique.

| ID | Nombre | Comportement actuel |
|---|---:|---|
| 0x08 | 10 497 | Réponses, DLC 4, registre et valeur brute reconnus |
| 0x10 | 4 987 | Requêtes DLC 2 / écritures DLC 4 reconnues syntaxiquement |
| 0x20 | 11 620 | Requêtes DLC 2 / écritures DLC 4 reconnues syntaxiquement |
| 0x58 | 575 | Toujours `00 9C`, DLC 2 ; non décodé, conservé par capture brute |

**27 104 / 27 679 trames reconnues (97,92 %)** : 10 583 QUERY,
6 024 SET et 10 497 REPLY. Aucun rejet supplémentaire.
Les catégories QUERY/SET décrivent la syntaxe du code ; elles ne prouvent pas
le sens fonctionnel de chaque registre. Le rôle de 0x58 reste à identifier :
ne pas l’ajouter arbitrairement comme requête ou commande de réveil.
Les deux tests hôte existants `protocol` et `mcp2515` passent.

## Échanges à prendre en compte pour la future console

Périodes médianes entre occurrences, calculées par horodatages triés ; ce sont
des observations, pas des délais contractuels à recopier dans le firmware.

| Trame | Nombre | Observation |
|---|---:|---|
| 0x20 `00 11` | 1 293 | Lecture toutes les ~105 ms |
| 0x20 `00 14` | 1 293 | Lecture toutes les ~105 ms |
| 0x20 `00 21` | 2 723 | Lecture toutes les ~51 ms |
| 0x20 `00 09 xx xx` | 5 446 | Deux écritures par lecture 0x21 dans les exemples ; valeurs 0x0000–0x0040 |
| 0x20 `00 72` | 575 | Lecture toutes les ~253 ms |
| 0x10 `00 21 00 01` | 575 | Écriture périodique toutes les ~253 ms |
| 0x58 `00 9C` | 575 | Trame périodique toutes les ~253 ms |

Pour les doubles écritures 0x09, l’intervalle médian entre occurrences est 2 ms :
il reflète les paires, pas la période complète. Aucune unité ni fonction de
commande n’est déduite de la plage brute.

À la fin figurent également des écritures isolées :
0x20 `00 41 00 00` à 00:02:33.089,
0x20 `00 42 00 01` à 00:02:33.139,
0x10 `00 25 00 01` à 00:02:33.202.
Elles peuvent appartenir à un changement d’état ; cette capture seule ne
permet pas d’affirmer qu’il s’agit d’une extinction. Le fichier continue ensuite.
Il commence déjà avec des lectures périodiques : séquence complète de mise sous
tension, wake électrique et arrêt complet non établis.

Le diagnostic TWAI actif actuel interroge 0x20:20, 0x20:11,
0x10:3B, 0x10:3C, 0x10:30 et 0x10:BC, au maximum une requête toutes les
200 ms. Parmi ces registres, seuls 0x20:11 et 0x10:30/BC sont vus comme
lectures dans cette capture. L’absence des autres ne prouve pas leur invalidité,
mais cette trace ne les valide pas. Ce diagnostic ne reproduit pas les échanges
périodiques de la console et ne fait aucune écriture.

## Limites et suite

- Le débit 125 kbit/s correspond aux transports TWAI et MCP2515 configurés.
  Les tailles et identifiants observés sont supportés par le transport.
- Le journal observation est limité par défaut à **100 trames/s**, inférieur
  aux ~189 trames/s de cette capture : omissions attendues et comptées.
  Augmenter la limite ne garantit pas une capture sans perte. Mesurer les
  pertes de file et débordements MCP2515 avec l’écran actif.
- Les réponses 0x08 ne donnent pas leur émetteur. Le logger injectant des
  lectures, ne pas transformer les appariements temporels en certitudes.
- Aucun SOC, km/h, ampère, watt ou degré n’est validé par cette analyse :
  conserver les valeurs brutes tant que les conversions et le profil matériel
  ne sont pas confirmés par mesures indépendantes.
- Pour la suite : capture passive complète démarrage/arrêt et actions annotées
  (assistance, frein, rotation de roue), identification de 0x58, puis transport
  TX et machine de contrôle selon TODO. Le réveil électrique demande une mesure
  matérielle distincte. Aucun changement du firmware moteur ni flash effectué.

## Complément BionXtool

La [confrontation avec BionXtool](BIONXTOOL_ANALYSIS.md) apporte les noms
et échelles candidates des registres et renforce l’hypothèse d’arrêt en fin
de capture (batterie 0x25=1). La trame 0x58/009C DLC 2 reste à identifier.
