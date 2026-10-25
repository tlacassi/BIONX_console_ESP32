# Point de reprise — 25 octobre 2026

## État confirmé et dernière demande

L’utilisateur confirme le pilotage moteur sur roue levée, puis répond
« ça marche bien » après ajout de la lecture du capteur de pédalage.
Lecture/affichage du capteur jugés fonctionnels ; aucun relevé chiffré,
zéro sous charge connue, signe, gain ou calibration N/Nm n’a été fourni.
Dernière demande : vérifier la documentation pour le prochain agent.
Cette passe ne modifie ni ne reflashe le firmware.

Lire ensuite :

1. [project_requirements.md](project_requirements.md) : cible fonctionnelle.
2. [BENCH_FINDINGS.md](BENCH_FINDINGS.md) : acquis et leçons pour le code final.
3. [JOG_BENCH.md](JOG_BENCH.md) : commandes et limites du profil réellement actif.
4. [HARDWARE.md](HARDWARE.md) : montage actuel et historique des broches.
5. [TODO.md](TODO.md) : état courant en tête, puis journal des incréments.
6. [VALIDATION.md](VALIDATION.md) : preuves datées, images et résultats successifs.

Les sections anciennes « actuel », « dernier firmware », « non flashé » dans
les journaux décrivent leur date, pas l’état courant ci-dessous. Les tableaux
C3/SPI partagé ne sont pas le brochage du banc HUZZAH32 à deux SPI.

## Firmware réellement chargé

- Carte : Adafruit HUZZAH32, ESP32 classique, écran GC9A01 et MCP2515 séparés.
- Répertoire `build-encoder-jog`, config `build-encoder-jog/sdkconfig-live`.
- Image `build-encoder-jog/bionx_c3_supermini.bin`, **509520 octets**.
  Le nom C3 est historique : cette image est compilée pour ESP32.
- SHA256 : `7aba6b67f6c0ce945dffb3d94d70c32d8f4a2d1d56fc837b1439a9c5c7b517ca`.
- Dernier flash : profil jog live avec affichage pédalage, hash vérifié.
- Dernier port utilisé : `/dev/cu.usbserial-016138BC` ; le réidentifier si besoin,
  sans ouvrir de moniteur pendant une rotation ni perdre un défaut figé.
- Flags actifs : HUZZAH32, CAN_MASTER, ASSIST_BENCH, ENCODER_JOG, JOG_TX.
  `ASSIST_BENCH` est une dépendance du jog ; il ne signifie pas que le
  contrôleur de pédalage est actif. Le jog fournit l’implémentation bench_app.
- `CONFIG_BIONX_JOG_WHEEL_MM=2136`, valeur de table provisoire pour700×28C.
- Defaults exacts : `sdkconfig.defaults;sdkconfig.master;sdkconfig.huzzah32;sdkconfig.components-dual-spi;sdkconfig.master-dual-spi;sdkconfig.assist-bench;sdkconfig.encoder-jog;sdkconfig.encoder-jog-live`.

Écran SPI2 : MOSI18/SCK5/CS14/DC32/RST27. MCP SPI3 : SI23/SCK4/SO19/CS33/INT34,
SPI2 MHz, CAN125 kbit/s, quartz déclaré8 MHz. Encodeur25/26/22, commun GND.
Wake externe vers GND, aucun wake GPIO. Frein ESP non câblé ; utilisateur
confirme une coupure haute tension indépendante accessible sur le banc.

## Comportement courant

Boot à zéro, sonde batterie HW toutes les secondes ; réponse attendue → init.
Console originale déconnectée. Encodeur : niveau0 arrêt, niveaux1–10 demandent
2,4,…20 km/h estimés. Clic coupe ; après défaut jog, clic puis relâchement
acquitte seulement si roue immobile et données saines ; nouvelle rotation
nécessaire. Pas de reprise différée. Défaut CAN/MCP persistant reste bloquant.
Maintien2 s : extinction. Sans action20 s ou durée de session20 s : zéro.

Consigne brute max8 au démarrage/4 en mouvement, rampe1 brut/200 ms, zéro
à cible et reprise en dessous ; aucune régène. Survitesse25 km/h estimés,
NO MOTION après2 s sans vitesse détectée sous commande>=4. Commande valable
100 ms, vitesse fraîche<150 ms, garde-fous température/tension/données actifs.
La coupure CAN ne garantit pas l’arrêt si le moteur garde sa dernière consigne.

`Ped raw… max… …ms` : moteur0x21, brut actuel, pic depuis boot, âge. Lecture
visée250 ms, stale à1 s. Indicateur seulement : aucun pédalage ne démarre
l’assistance. `Pbat` = U×I batterie signé, pas puissance mécanique moteur.
Ni watts ni vitesse ni force ne sont calibrés indépendamment.

## Où travailler

| Besoin | Sources principales |
|---|---|
| Protocole/metadata | `components/bionx_protocol/bionx_protocol.[ch]` |
| Init/arrêt, polling prioritaire et ensembles U/I cohérents | `components/bionx_master/bionx_master.[ch]` |
| Jog, conversion km/h, clic acquittement et garde-fous | `components/bionx_master/bionx_jog.[ch]` |
| Capture IRQ, Gray, file de crans et clic bref | `components/bionx_master/bionx_encoder.h` |
| Entrées, affichage pédalage, autorisation finale TX | `main/jog_app.c` |
| Boucle principale, UI générale, premier défaut TX figé | `main/bionx_master_app.c` |
| Transport MCP one-shot, mutex RX/TX | `components/mcp2515/` |
| Contrôle pédale séparé, non activé dans ce jog | `components/bionx_master/bionx_bench.[ch]`, `main/bench_app.c`, `ASSIST_BENCH.md` |
| Ancien contrôleur non utilisé par l’application jog | `components/bionx_master/bionx_control.[ch]` |

Ne pas réintroduire les défauts corrigés : acquisition encodeur par polling,
plafonds divergents entre contrôleur et filtre TX, vitesse affamée par polling
circulaire, calcul U/I mélangeant des générations, maintien de couple fixe à
la cible sur roue déchargée. Voir les preuves dans BENCH_FINDINGS/VALIDATION.

## Reproduire les vérifications

Dernier résultat logiciel : **8/8 tests hôte**, y compris charge télémétrie avec
réponses retardées1/6/11 ms et conversion km/h. Build et flash réussis, sans
warning. Ces résultats ne constituent pas une mesure physique de sécurité.

```sh
cmake -S tests/unit -B build-unit
cmake --build build-unit
ctest --test-dir build-unit --output-on-failure
```

Environnement local et build du profil existant :

```sh
export IDF_PYTHON_CHECK_CONSTRAINTS=0
. /Users/theolacassin/esp/v5.2/esp-idf/export.sh
idf.py -B build-encoder-jog build
```

Pour recréer le build/config, utiliser la commande complète de JOG_BENCH.md,
pas un autre profil C3 ou un ancien SDKCONFIG. Vérifier les flags et l’image
avant tout flash. Derniers journaux :
`/tmp/bionx-jog-pedal-display-{build,flash}.log` (temporaires, pas archives fiables).
La trace durable est VALIDATION.md et le hash ci-dessus.

## Ce qui reste à faire selon la prochaine demande

- Relever Ped raw au repos et sous efforts reproductibles ; vérifier zéro,
  signe, dynamique, filtrage et conversion physique avant assistance pédale.
- Mesurer le tour de roue et contrôler le facteur vitesse CAN. À2136 mm,
  la quantification donne un premier bin au-dessus25 vers25,63 km/h estimés ;
  le système ne garantit pas une vitesse mécanique maximale exacte de25.
- Vérifier le chemin du courant/shunt et comparer U/I à une mesure indépendante.
- Raccorder/tester frein, caractériser coupure CAN/reset ESP et arrêt matériel.
- Concevoir et tester le contrôle final avec ces résultats. Le jog validé
  n’est pas une assistance routière validée et ses plafonds ne sont pas des watts.

Ne pas rouvrir un chantier matériel sur la seule base d’un ancien DATA FAULT :
relever la première cause précise (SPEED OLD, U/I INVALID, LINK/MASTER, TX…)
et conserver son contexte avant reset. Les références exactes du transceiver
et certaines propriétés électriques restent inconnues ; cela ne permet pas
d’attribuer rétroactivement les défauts déjà reproduits dans le logiciel.
