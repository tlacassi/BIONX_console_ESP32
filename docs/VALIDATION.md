# Validation du socle — 23 octobre 2026

Ce fichier est un journal chronologique. Pour l’état courant et l’image réellement
chargée, lire [HANDOFF.md](../HANDOFF.md). Les mentions « non flashé », « prochain »
ou « restant » ci-dessous décrivent leur étape historique ; voir les ajouts récents
en fin de fichier et le bilan [BENCH_FINDINGS.md](BENCH_FINDINGS.md).

## Complément : observation MCP2515 (non flashée)

### Implémenté

Phase 1 de `TODO.md`, tâches 1–7 terminées : protocole CAN/BionX indépendant,
bus SPI partagé, pilote MCP2515 silencieux, file de capture statique de 64
trames, notification GPIO INT avec repli par interrogation et écran de
diagnostic à 1 Hz sans télémétrie fictive. Aucun code wake ou consigne moteur.
Le transport bidirectionnel et le mode console maître sont les étapes suivantes,
pas une fonction présente dans ce premier pilote MCP2515.

Le profil opt-in `sdkconfig.observe` conserve le câblage écran et ajoute
MISO=5, MCP CS=6 et INT=7 sur SPI2, avec MOSI=3/SCLK=4 partagés. Le propriétaire
unique du bus est `board_spi`, avec validation des doublons et GPIO réservés.
L’écran transfère au maximum deux lignes de pixels par transaction en observation
(960 octets, 384 us à 20 MHz hors overhead), avec profondeur de file 1.
Ses deux buffers DMA totalisent 1 920 octets. Ces bornes réduisent le blocage
SPI sans prouver une absence de perte sur un bus chargé.

Le pilote est une implémentation originale d’après la fiche Microchip MCP2515
DS20001801J, sans reprise d’un pilote Arduino. Quartz 8/16/20 MHz, débit fixé
à 125 kbit/s, 16 TQ, échantillonnage 75 %, SPI 1 MHz. Initialisation contrôlée
par lectures de retour des CNF et CANSTAT ; aucun passage en mode normal ni
instruction de transmission. Le mode inattendu ou l’échec SPI verrouille RX
à l’arrêt jusqu’au redémarrage manuel. L’ISR ne fait que notifier la tâche.

Le parseur préserve les deux octets bruts et distingue requête/écriture/réponse,
sans inventer d’unité ni de source à une réponse partagée `0x08`. Le diagnostic
TWAI existant réutilise maintenant la validation des réponses candidates et
refuse une réponse lue après son échéance. L’horodatage est pris avant le log.

### Tests exécutés

- Tests hôte `tests/unit` : **2/2 réussis** (protocole et modèle SPI MCP2515).
  Trames standard/étendues, RTR, DLC 0/8 et refus 9–15, identifiants limites,
  ordre des octets, réponses hors fenêtre, origine ambiguë, quartz non pris
  en charge, calcul indépendant du débit, lecture des deux buffers, effacement
  des indicateurs de débordement, composant absent, mode figé et CNF corrompu.
- Injection d’une erreur à chaque transaction SPI d’initialisation puis de
  réception : retour d’erreur et arrêt validés. Le modèle refuse toute
  instruction RTS/chargement TX, toute écriture de buffer TX et tout passage
  en mode normal. Ce modèle ne reproduit pas la couche physique CAN.
- Régression UI hôte `tests/host` : **1/1 réussie**.
- Builds ESP32-C3 avec ESP-IDF v5.2-dirty et gestionnaire 2.4.11 : observation,
  dashboard et diagnostic historique **réussis**, sans avertissement compilateur
  détecté dans les journaux finaux.
- Vérification supplémentaire du chemin MCP activé : build et édition de liens
  réussis avec un quartz **fictif de test de 8 MHz** et la confirmation matérielle
  activée temporairement dans la configuration générée, jamais flashée. Taille
  538 992 octets (`0x83970`), D/IRAM statique 103 972 octets (32,4 %).
- Puis restauration du profil livré : matériel **non vérifié**, quartz **0**,
  observation activée, et recompilation réussie. Binaire 534 768 octets
  (`0x828f0`), D/IRAM statique 102 006 octets (31,7 %), marge du slot 66 %.
  La différence de taille vient de l’élimination du chemin non accessible par
  le compilateur tant que la vérification matérielle est désactivée.
- SHA-256 du binaire non configuré livré dans `build-observe/` :
  `280537dfa5a1990318c45d68db02ac65d6f7ab7c6a81b6d4ffb7478ddc6e5c13`.
- Logs : `/tmp/bionx-unit.log`, `/tmp/bionx-ui-regression.log`,
  `/tmp/bionx-observe-build.log`, `/tmp/bionx-observe-configured-build.log`,
  `/tmp/bionx-dashboard-regression.log`, `/tmp/bionx-normal-regression.log`.

### Limites et procédure suivante — non exécutée

Aucun flash effectué pendant cet incrément : la carte reste sur le dashboard
précédemment flashé. Le module MCP2515, ses niveaux logiques et son quartz
ne sont pas identifiés. Aucun essai RX matériel, SPI partagé en charge,
interruption GPIO réelle, saturation de file, débit de journal ou réveil batterie
n’a été exécuté. Les tests hôte du modèle ne les remplacent pas.

1. Identifier alimentation, transceiver, niveaux SO/INT, quartz et terminaison
   du module ; vérifier le connecteur vélo et la référence de masse.
2. Renseigner le quartz réel et la confirmation matérielle dans menuconfig,
   reconstruire puis flasher le profil observation. Vérifier les modes de
   configuration et d’écoute avec retour SPI ; confirmer l’absence d’émission
   avec un analyseur CAN externe.
3. Sur un banc CAN à 125 kbit/s comportant deux autres nœuds actifs pour les
   ACK, injecter trames connues, rafales, standard/étendu/RTR et bus muet.
   Comparer capture et compteurs avec l’analyseur. Répéter écran sollicité et
   console série ralentie/déconnectée ; mesurer marges de piles et latence RX.
4. Distinguer les pertes exactes de file logicielle des événements matériels
   RX0OVR/RX1OVR (bornes inférieures) et des omissions volontaires du journal.
   L’horodatage est celui de lecture SPI et l’ordre RX0/RX1 ne garantit pas
   l’ordre chronologique des trames sur le fil.
5. Le MCP2515 listen-only peut capturer des trames endommagées ; son TEC/REC
   est désactivé dans ce mode. Des compteurs nuls ne prouvent donc pas un bus
   sain et le parseur structurel ne prouve pas un CRC CAN correct.
6. Avec la console G1 d’origine comme seul maître, enregistrer allumage,
   identification, niveaux, pédalage, frein et extinction. Puis confronter ces
   observations aux sources avant les étapes TX/maître/wake/contrôle de TODO.

Référence du pilote : [fiche MCP2515 Microchip](https://ww1.microchip.com/downloads/en/DeviceDoc/MCP2515-Stand-Alone-CAN-Controller-with-SPI-20001801J.pdf).

## Complément : dashboard LVGL 9

### Retour visuel et correction d’orientation

L’utilisateur a fourni une photo : le dashboard apparaît effectivement sur
l’écran physique. Avec le connecteur de l’écran en haut, DEMO est en haut,
TOUR en bas et les lettres sont réfléchies verticalement, sans inversion de
leur ordre gauche/droite. Le profil `sdkconfig.dashboard` active maintenant
`CONFIG_BIONX_LCD_MIRROR_Y=y` ; X et l’échange X/Y restent désactivés.
Le blanc et le cyan visibles ne suffisent pas à identifier un défaut de format
couleur : BGR, inversion et permutation RGB565 sont conservés pour isoler la
correction. Confirmation visuelle après ce nouveau flash encore nécessaire.
Les résultats et le hash ci-dessous décrivent la première version, avant cette
correction d’orientation.

Build de la correction : réussi, sans avertissement compilateur ; taille
inchangée à 659 840 octets. Flash à 115200 bauds : réussi, hashes vérifiés.
SHA-256 de cette version :
`cf5c146cc39aa622750b97f75eda9a7abf88a5a3119eb1172f2edd933678838e`.
Logs : `/tmp/bionx-dashboard-orientation-build.log` et
`/tmp/bionx-dashboard-orientation-flash.log`.
Capture USB de 12 s après ce flash : heartbeat à 10 s, heap/minimum
229 816 octets, marge de pile principale 4 556 octets, aucune mise à jour
abandonnée ni panic observé. Confirmation visuelle de l’orientation corrigée
toujours attendue ; couleurs inchangées.

### Implémenté

- Modules `display` (esp_lcd / GC9A01 / port LVGL), `dashboard` (widgets et
  interpolation) et `dashboard_demo` (échantillons sans dépendance LVGL).
- Profil opt-in `sdkconfig.dashboard`, configuration générée isolée dans
  `build-dashboard/`. Aucun driver véhicule démarré dans ce profil.
- Écran SPI avec le câblage communiqué : RST=0, CS=1, DC=10, MOSI=3, SCLK=4.
  SPI mode 0 à 20 MHz, BGR, inversion activée, RGB565 avec permutation des
  octets par le port LVGL. Rotation neutre, réglable dans menuconfig.
- Deux buffers DMA 240×20 RGB565 : 19 200 octets au total ; pool LVGL 48 Kio.
  Pas de framebuffer complet sur la cible. Le framebuffer hôte n’est qu’un
  outil de test, non lié au firmware.
- Données à 10 Hz, timer UI de 33 ms ; textes sur buffers fixes, objets créés
  à l’initialisation, accès depuis la tâche principale protégés par le verrou
  LVGL (attente maximale 20 ms). Log des mises à jour abandonnées et de la heap.
- Dépendances verrouillées : LVGL 9.2.2, esp_lvgl_port 2.6.2,
  esp_lcd_gc9a01 2.0.4, cmake_utilities 0.5.3 ; licences conservées dans les
  composants téléchargés (MIT pour LVGL, Apache-2.0 pour les composants Espressif).

### Outils et blocages

Le gestionnaire ESP-IDF 1.5.1, puis 1.5.3, échouait à analyser une clause `&&`
dans les manifests de versions candidates du registre. Il a été mis à jour en
**2.4.11** dans l’environnement Python existant. Les contraintes Python
historiques du SDK 5.2 imposent 1.x ; `IDF_PYTHON_CHECK_CONSTRAINTS=0` est
nécessaire dans ce terminal avant `export.sh`. Le SDK n’a pas été modifié.
Cette adaptation de l’environnement est documentée dans le README.

Le gestionnaire 2.x inspecte les processus CMake via psutil : les builds ont dû
être exécutés hors du bac à sable macOS. Un premier avertissement de troncature
de chaîne a été corrigé par un buffer vitesse suffisamment grand. Les derniers
builds cible, hôte et profil normal ne présentent pas d’avertissement compilateur.

### Testé

```sh
export IDF_PYTHON_CHECK_CONSTRAINTS=0
. /Users/theolacassin/esp/v5.2/esp-idf/export.sh
idf.py -B build-dashboard -D SDKCONFIG=build-dashboard/sdkconfig -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.dashboard' build
idf.py -B build-dashboard size
idf.py build
cmake -S tests/host -B build-host
cmake --build build-host
ctest --test-dir build-host --output-on-failure
idf.py -B build-dashboard -p /dev/cu.usbmodem213301 -b 115200 flash
```

- Build dashboard et profil normal : réussis. Le profil normal a été compilé
  uniquement, jamais flashé avec cet écran branché.
- Binaire dashboard : **659 840 octets** (`0xa1180`), 58 % de marge dans le
  slot de 1,5 Mio. D/IRAM statique : **118 552 octets** (36,9 %).
- SHA-256 flashé :
  `1bc17e6d6c25666fa00c45cb7a74383e5c3e34f2e365f6a1d30571bdb9d00b17`.
- Tests hôte : réussis. Cycle complet de 24 s, bornes vitesse/puissance,
  démarrage à 78 %, décrément du SOC, temps extrême, entrées invalides,
  timeout de verrou, appels d’initialisation incorrects et géométrie des textes
  dans le cercle. Le vrai moteur LVGL est utilisé, avec stubs uniquement pour
  les API système ESP et le verrou (pas de test de concurrence réelle sur hôte).
- Aperçu natif RGB565 rendu et inspecté : vitesse 35,0, TOUR, 78 %, 500 W,
  repère DEMO ; lisibilité et absence de texte coupé vérifiées sur l’aperçu.
- Pool LVGL hôte après le cycle : 28 % utilisés, plus grand bloc libre
  32 816 octets ; cela ne mesure pas les allocations DMA/piles de la carte.
- Flash autorisé explicitement par l’utilisateur : réussi, hashes vérifiés,
  reset esptool effectué. La première connexion a échoué car un moniteur série
  VS Code occupait le port ; il a été fermé avant la relance réussie.
- Captures USB : messages `DEMO alive` à environ 40/50 puis 80/90 s après
  démarrage, heap libre et minimum **229 816 octets**, marge de pile de la tâche
  principale **4 556 octets**, **0** mise à jour UI abandonnée. Aucun panic ou
  redémarrage dans ces fenêtres. Le heartbeat implique que les initialisations
  écran et dashboard ont retourné ESP_OK ; leurs logs initiaux n’ont pas été
  capturés. Une tentative de reset par RTS seul entre les deux captures n’a
  pas réinitialisé le compteur : aucun second démarrage validé par cet essai.
- Logs temporaires : `/tmp/bionx-dashboard-build.log`,
  `/tmp/bionx-dashboard-flash.log`, `/tmp/bionx-dashboard-monitor.log`,
  `/tmp/bionx-dashboard-boot.log` (seconde capture, sans reset effectif).
  Le port est refermé à la fin de chaque capture bornée.

### Encore à confirmer

La photo utilisateur confirme l’affichage et le rétroéclairage de la première
version, avec un miroir vertical. L’orientation après correction, les couleurs
et la fluidité restent à confirmer par observation de l’écran.
Les 20–30 FPS restent un objectif, sans mesure du taux de trames effectif.
Marge de pile LVGL, charge CPU et essais longs non mesurés. Aucun essai
CAN/BMS/frein/moteur exécuté ; toutes les valeurs du dashboard sont fictives.
La connexion de données réelles demandera validité/expiration et adaptation de
l’indication DEMO avant usage. Aucun réseau ni service OTA ajouté.

## Complément : flash et essai USB sur carte

La première section de résultats ci-dessous décrit le build initial, avant
branchement de la carte. Un essai matériel USB a ensuite été exécuté :

- Port : `/dev/cu.usbmodem213301`.
- Identification esptool : ESP32-C3 QFN32 révision v0.4, flash intégrée XMC
  4 Mo, quartz 40 MHz.
- Montage déclaré : écran GC9A01 240×240 SPI, RST=0, CS=1, DC=10,
  MOSI=3, SCLK=4, alimentation 3,3 V. Aucun test d’affichage effectué.
- Ajout de `CONFIG_BIONX_USB_TEST_ONLY` : aucun driver CAN/écran/BMS démarré,
  heartbeat une fois par seconde. Build séparé `build-usb/`, avec
  `sdkconfig.usb-test` en complément de `sdkconfig.defaults`.
- Premier flash : écriture et hashes vérifiés, mais panic avant `app_main`.
  Le SDK local appelle `esp_rom_uart_tx_wait_idle(CONFIG_ESP_CONSOLE_UART_NUM)`
  sans garde dans `components/esp_system/port/soc/esp32c3/clk.c` alors que le
  numéro vaut -1 en USB seule ; le panic observé est dans `uart_tx_flush`,
  avec l’argument 0xff. Le problème n’est pas corrigé dans le SDK.
- Contournement limité au profil USB : UART0 principal, USB Serial/JTAG
  secondaire. Cela utilise GPIO21 TX et interdit son usage BMS dans ce profil.
  Le profil normal garde sa configuration USB seule et reste à corriger avant
  flash matériel ; le build seul ne détecte pas ce problème.
- Deux tentatives du second flash ont échoué avec erreur de transfert ; un
  processus résiduel du moniteur détenait encore le port. Après fermeture,
  flash à 115200 bauds réussi, hashes de chaque zone vérifiés, reset effectué.
- Console USB lue pendant 12 s : heartbeats successifs de 8 à 19 s,
  sans panic ni redémarrage dans cette fenêtre. Port fermé après capture.
- Binaire flashé : `build-usb/bionx_c3_supermini.bin`, `0x2c970`
  (182 640 octets), SHA-256
  `75c9ecfd842fdd1944b4ebf3baee98630bf00d383d57b0cb01a988945bb4ff41`.
- Profil normal recompilé avec succès après ajout du choix de test
  (avant le dernier changement de texte du heartbeat) ; aucun flash de ce profil.
- Logs de session : `/tmp/bionx-usb-flash-retry.log`,
  `/tmp/bionx-usb-monitor.log` ; logs ESP-IDF dans `build-usb/log/`.

Le montage écran occupe GPIO4 (ancien CAN TX), GPIO10 (ancien bouton), et les
anciennes réservations I2C 0/1. Le plan de broches doit être revu avant d’activer
les périphériques. Seuls flash et fonctionnement USB du profil isolé ont été
testés ; CAN, écran, BMS et commande moteur restent non validés.

## Périmètre inspecté

Lecture de `project_requirements.md`, du README, des sources `main/`, du
partitionnement, des fichiers CMake et du réglage VS Code. Aucun `AGENTS.md`
trouvé dans le projet ou ses répertoires parents. Le fichier
`CAHIER_DES_CHARGES.md` affiché dans le contexte IDE n’est pas présent sur disque.
Ce dossier ne contient pas de `.git` : pas de branche, historique ou diff Git
disponible. Aucune initialisation Git effectuée.

## Outils réellement utilisés

- SDK : `/Users/theolacassin/esp/v5.2/esp-idf`, `ESP-IDF v5.2-dirty`.
- HEAD SDK : `11eaf41b37267ad7709c0899c284e3683d2f0b5e`.
  De nombreux fichiers locaux sont signalés modifiés ; ce hash seul ne permet
  donc pas de reproduire exactement le SDK. Aucun fichier du SDK modifié ici.
- Compilateur RISC-V : GCC 13.2.0, `esp-13.2.0_20230928`.
- Python du SDK : 3.11.6 ; CMake 3.24.0 ; Ninja 1.11.1 ; esptool.py 4.7.0.
- Version 5.4.x annoncée dans le projet : non installée, non testée.

## Blocage corrigé

Les fichiers importés portaient des dates futures (notamment
`CMakeLists.txt`, daté du 23 octobre à 23:33, heure Zurich, alors que le build
se déroulait vers 22:49). Ninja signalait `build.ninja older than most recent
input` et relançait CMake en boucle. Les dates futures des fichiers du projet
hors `build/` ont été ramenées à l’heure courante, sans changement de contenu.
La compilation a ensuite abouti sans modification des sources C ou des API.

Le README a été corrigé pour refléter le SDK installé et le résultat réel.
Un `.gitignore` exclut les sorties de compilation et configurations générées ;
`sdkconfig.defaults` reste la configuration source.

## Vérifications exécutées

```sh
. /Users/theolacassin/esp/v5.2/esp-idf/export.sh
idf.py --version
idf.py build
idf.py build size
```

Le premier build a généré `sdkconfig` depuis `sdkconfig.defaults` : cible
`esp32c3`, flash configurée 4 Mio, console USB Serial/JTAG, pas de console
secondaire, `CONFIG_BIONX_ACTIVE` désactivé. Ce sont des paramètres de build,
pas des caractéristiques mesurées sur la carte.

- Build complet : réussi, bootloader, partition-table et application générés.
- Relance `idf.py build size` : réussie, sans boucle de reconfiguration.
- Rapport statique : D/IRAM utilisée 52 298 octets (16,3 %) ; ce chiffre
  ne mesure ni la heap minimale ni les marges de piles en fonctionnement.
- Taille application : `0x2cd10` = 183 568 octets.
- Vérification ESP-IDF du slot : réussi ; slot `0x180000`, marge
  `0x1532f0` (88 % affichés par l’outil).
- Binaire : `build/bionx_c3_supermini.bin`.
- SHA-256 : `4f888b41cbc8b00c579e4ad4324f72c0321032c06d003f079ef1c88836a09e6d`.
- Journaux de compilation : `build/log/` (fichiers générés, non versionnés).

## État fonctionnel

| Élément | Implémentation constatée | Validation |
|---|---|---|
| Socle C3 / FreeRTOS / TWAI | Présent, compile | Démarrage non testé |
| Écoute passive 125 kbit/s | Mode par défaut, capture avec temps monotone | Absence physique d’émission et débit réel non mesurés |
| GPIO | Réservation et détection de doublons au démarrage | Schéma et tests matériels à confirmer |
| Lectures actives optionnelles | Une attente à la fois, timeout, arrêt des requêtes sur défaut détecté | Protocole et réponses candidates non validés sur vélo |
| Diagnostic CAN | Trames horodatées, arrêt actif sur erreur TX/état bus | Compteurs d’erreurs/pertes non exposés ; journal synchrone potentiellement perdant |
| Décodage métier | Affichage brut de réponses candidates | Pas de télémétrie avec unités, validité et expiration ; pas de tests hôte |
| Contrôle moteur / frein | Absent ; aucune commande de couple implémentée | Non testé |
| Écran / encodeur / BMS | Broches réservées uniquement | Drivers absents |
| SOC / odomètre / stockage applicatif | Absent | Non testé |
| Wi-Fi / OTA | Services absents ; deux slots réservés | Aucune validation OTA |

L’appariement actif reste ambigu (réponse tardive, identifiant partagé).
Le watchdog général ne surveille pas une future commande fonctionnelle.
La séparation transport/protocole et les tests de trames restent le prochain
incrément logiciel ; aucune commande motrice n’a été ajoutée dans cette passe.

## Procédure matérielle — non exécutée

1. Confirmer variante C3, flash, alimentation, schéma GPIO et transceiver
   compatible 3,3 V ; confirmer masse et terminaisons du bus.
2. Garder les requêtes actives désactivées. Flasher explicitement avec
   `idf.py -p PORT flash monitor`, puis tester plusieurs resets et la console USB.
3. Sur banc CAN maîtrisé, injecter des trames connues à 125 kbit/s ; comparer
   identifiants, DLC, données, horodatages et pertes avec un analyseur externe.
   Vérifier avec cet analyseur l’absence d’émission en écoute seule.
4. Tester bus muet, déconnexion, saturation et défauts. Ajouter les compteurs
   manquants avant de qualifier la capture pour une charge soutenue.
5. Confirmer les registres et profils matériels par captures. Avant toute
   assistance future : entrée frein validée, limites batterie, armement,
   arrêts et comportement moteur sur perte de console mesurés.

Ce build ne valide ni le câblage, ni le protocole BionX, ni un usage roulant.

## Incrément BionXtool — 23 octobre 2026

Référence tierce et limites : `BIONXTOOL_ANALYSIS.md`.
Décodeur étendu au trafic diagnostic, 0x58 DLC 2 conservé UNKNOWN,
conversions candidates mono-octet et lectures TWAI enrichies sans écritures.

Vérifications effectuées avec ESP-IDF local v5.2-dirty :

- `cmake --build build-unit` et `ctest --test-dir build-unit --output-on-failure` :
  2/2 réussis, incluant signe/plages, HI/LO non convertis, adresses diagnostic
  et refus d’une réponse 0x58 dans une transaction attendant 0x08.
- `python3 tools/check_cl1000.py` : 27 104 trames décodées, 575 UNKNOWN,
  aucun rejet, même empreinte de capture que le rapport initial.
- `idf.py -B build-observe build` : réussi, aucun avertissement/erreur dans
  le journal, image 0x82eb0 octets.
- `idf.py -B build build` : réussi, aucun avertissement/erreur dans le
  journal, image 0x2cdb0 octets.

Aucun flash ni test matériel effectué. Pas de validation physique des unités,
du signe courant ou du fonctionnement moteur. Le dashboard reste simulé.

## Maître neutre et HUZZAH32 — 24 octobre 2026

Implémentation et hypothèses détaillées dans `MASTER_BENCH.md`.

- `ctest --test-dir build-unit --output-on-failure` : **3/3 réussis**.
  Protocole, modèle SPI MCP2515 passif/actif et maître/contrôle.
  Cas couverts : confirmation TX, pending/abort, erreur/arbitrage perdu,
  absence de confirmation, bus-off, maintien passif sans TX ; démarrage,
  copie des limites, runtime neutre, arrêt, échec TX, timeout/retard de réponse,
  mauvais profil/limites, présence d’un autre maître, horloge régressive,
  fraîcheur/assemblage tension ; armement, frein, limites, rampes, régène,
  expiration, réarmement et NaN.
- `ctest --test-dir build-host --output-on-failure` : **dashboard 1/1 réussi**.
- Rejeu `start&stop.csv` : 452 SET, 598 QUERY, 598 REPLY, 42 UNKNOWN ;
  aucun rejet, horodatages CSV synthétiques, sans validation de timing.
- Builds ESP-IDF local v5.2-dirty : C3 maître, HUZZAH32 maître ESP32,
  C3 observation, C3 dashboard et C3 diagnostic réussis, aucun avertissement
  ou erreur de compilation dans les journaux de validation.
- Le chemin MCP actif HUZZAH32 a également été compilé avec une configuration
  synthétique quartz 16 MHz / vérification activée, uniquement dans le dossier
  de build. Les paramètres 0 / non vérifié ont été restaurés et recompilés.
  Cela ne constitue pas une identification du module réel.
- Une tentative additionnelle AddressSanitizer/UBSan est restée bloquée sans
  diagnostic et a été interrompue ; aucun résultat de sanitizer n’est revendiqué.

Aucun flash effectué. Initialisation matérielle, ACK réel, stress RX/SPI/écran,
commande série en situation réelle, réveil électrique, calibration et arrêt
moteur sur perte de liaison restent non testés. Les cases logicielles de TODO
ne valent pas validation de banc. Aucune émission d’assistance non nulle n’est
reliée au contrôleur dans l’application fournie.

## Premier essai physique des composants HUZZAH32 — 24 octobre 2026

Profil `build-components` / `sdkconfig.components` : ESP32-D0WDQ6 rev1.0
identifié sur `/dev/cu.usbserial-016138BC` (CP2104). Adaptateur CANable sur un
autre port, non utilisé pour flasher. Build réussi, flash à 115200 réussi avec
vérification des empreintes ; application de test démarre.

Déclarations utilisateur : modules alimentés à 3,3 V, MCP quartz 8 MHz,
pas de pull-ups ajoutés. Rotation A25/B26 avec pull-ups internes, bouton22
avec pull-up interne ; MCP passif par polling sans INT34.

Résultats encore non satisfaisants :
- Pilote écran initialisé, mais écran signalé noir par l’utilisateur.
  Cela n’est pas une validation de l’écran : le SPI écran ne répond pas.
- Diagnostic MCP : dix lectures CANSTAT (0x0E) = 00 après reset SPI,
  puis ESP_ERR_TIMEOUT. Aucune erreur retournée par le contrôleur SPI ESP32,
  mais aucune preuve de communication correcte avec le composant.
- Fenêtres de capture : A/B=11, compteur0, bouton relâché et clics0 ;
  utilisateur indique avoir tourné/cliqué. Câblage effectif et alimentation
  restent à vérifier, aucune validation de l’encodeur revendiquée.
- Utilisateur a raccordé écran SDA→MO et SCL→SCK pendant le diagnostic.
  Correspondances des noms imprimés détaillées dans `COMPONENT_TEST.md`.

Aucune émission CAN du test. Le défaut MCP arrive avant toute capture de bus.
Les trois composants restent en diagnostic ; les validations matérielles de la
TODO restent ouvertes. Les captures série sont temporaires dans /tmp.

### Isolation de l’écran seul

À la demande utilisateur, test combiné remplacé sur la carte par le profil
`build-screen-test` : GC9A01 seul à 1 MHz, sans lecture encodeur ni initialisation
MCP. CS33 maintenu haut. Build corrigé puis réussi sans avertissement, flash
réussi avec vérification des empreintes. Série : initialisation écran réussie,
puis motifs RED/GREEN/BLUE/WHITE observés sans panic pendant la capture.
L’utilisateur confirme que l’écran reste noir. **Affichage physique non validé**.
Les journaux prouvent l’exécution du test sur ESP32, pas la réception des
commandes SPI ou l’alimentation/rétroéclairage du module. Prochaine vérification :
VCC/GND directement sur module, broche BL éventuelle et câblage CS/DC/RST.

## Écran confirmé, test encodeur isolé — 24 octobre 2026

L’utilisateur confirme que l’écran fonctionne après correction de broches
décalées ; le premier échec d’affichage ne constitue donc pas une panne de
l’écran. MCP déconnecté à sa demande.

Profil `build-encoder-test` compilé sans avertissement et flashé avec vérification
des empreintes. Écran à 1 MHz, A25/B26 et bouton22 avec pull-ups internes,
aucune initialisation MCP ni tâche CAN (confirmé côté série).
Première fenêtre de test : rotation/appuis déclarés par l’utilisateur, mais
position0, A/B11, bouton1, clics0 dans les logs. Encodeur non encore validé.
Test direct GPIO22 vers GND demandé pour isoler la lecture de l’entrée du
câblage/contacts de l’encodeur.

Test direct d’entrée : capture série montre bouton GPIO22 passant de 1 à 0
puis à 1, compteur atteignant 2 clics. Lecture logicielle de cette entrée et
anti-rebond fonctionnels dans ce test. Cela ne valide pas encore les contacts
ou le branchement SW/commun de l’encodeur lui-même, ni ses voies A/B.

### Confirmation finale utilisateur — écran et encodeur

L’utilisateur signale un problème de pinout corrigé puis confirme que tout
fonctionne dans le test écran + encodeur : affichage, rotation et bouton.
Cette confirmation clôt le diagnostic de ces fonctions sur le montage actuel ;
les entrées immobiles précédemment observées ne sont pas une panne logicielle
démontrée. Les pertes à rotation rapide restent non mesurées. MCP déconnecté,
aucune validation SPI/CAN supplémentaire ni test maître effectué.

## MCP reconnecté, CAN non branché — 24 octobre 2026

Test combiné `build-components` recompilé sans avertissement et flashé avec
vérification des empreintes. Quartz déclaré 8 MHz, alimentation déclarée 3,3 V.
Reset SPI : CANSTAT=80 ; timing relu conforme par le pilote ; passage à
CANSTAT=60, init ESP_OK et LISTEN ONLY 125 kbit/s. Liaison SPI bidirectionnelle
Feather ↔ MCP2515 confirmée sur ce montage.

L’ancien délai de verrouillage des statistiques (5 ms) expirait pendant les
transferts écran à 1 MHz, sans échec SPI du MCP. Délai porté à 100 ms uniquement
pour le test des composants, et statut STATS BUSY distinct d’une panne du MCP.
Après ce premier correctif, capture série 18 s : 18 snapshots SPI OK et
65 STATS BUSY ; RX/drops/débordements nuls sur les snapshots disponibles,
aucun défaut RX ou échec d’initialisation observé. Le délai accru seul ne
suffit donc pas à garantir la disponibilité des statistiques. Une seconde
correction évite les rafraîchissements de texte identique et limite l’UI à
1 Hz pendant le test MCP, sans modifier l’horloge écran de 1 MHz.

Le CAN est débranché : ni transceiver H/L, débit réel, réception de trames,
ACK/TX ni interruption INT34 validés. Test en polling, aucune émission CAN.
Dernier firmware flashé : test combiné écran + encodeur + MCP passif.


## CAN du vélo reçu, MCP isolé — 24 octobre 2026

Vélo et console d’origine allumés, confirmation utilisateur. Firmware
`build-components/bionx_c3_supermini.bin` compilé puis flashé avec
`BIONX_MCP_SERIAL_ONLY=y` (profil reproductible `sdkconfig.mcp-serial-test`).
Pas d’initialisation ni de rafraîchissement écran, CAN écoute seule à
125 kbit/s avec quartz déclaré 8 MHz. Init ESP_OK, CANSTAT 80 puis 60.

Capture série environ 18 s : 2 205 trames reçues, zéro perte dans la file
logicielle, 883 événements de débordement matériel. Ce compteur ne donne
pas le nombre exact de trames perdues. Aucun panic/assert ni STATS BUSY.
Exemples reçus : `020 [2] 00 11`, `020 [4] 00 09 00 00`,
`010 [2] 00 32`, `008 [4] 00 32 00 D1`, `058 [2] 00 9C`.
Preuve locale : `/tmp/bionx-mcp-isolated-serial.log`.

Liaison SPI et réception du CAN réel confirmées ; capture exhaustive non
validée. Aucune émission CAN, ACK/TX et INT34 non testés. Le partage SPI
avec l’écran reste non résolu : l’essai combiné a donné des statistiques
indisponibles, une assertion SDK avec transactions polling, puis une
relecture CNF invalide avec transactions queued et réservation du bus.
Le test isolé remplace ce dernier firmware combiné sur la carte.


## Optimisation SPI/RX — 24 octobre 2026

Modifications : IO GC9A01 synchrone (commande/pixels en polling, callback
LVGL après fin effective), verrou SPI récursif commun, une ligne par transfert.
Lecture RX passive par READ RX BUFFER (acquittement RXnIF matériel en fin de
commande), deux transactions par trame hors santé. Mode/TEC/REC toutes les
10 ms en passif ; contrôles actifs complets conservés. Buffers SPI internes
pour les commandes de quatre octets ou moins. INT optionnel avec timer de
secours (1 ms avec INT, 250 µs sans). Statistiques publiées indépendamment
du mutex RX ; aucun timeout d’acquisition nécessaire pour leur lecture.
LCD 20 MHz, MCP SPI 2 MHz sur le profil composants ; quartz CAN toujours
8 MHz et débit CAN 125 kbit/s. Pas d’émission CAN.

Tests hôte : 3/3 réussis, y compris RX standard/étendu/RTR, deux buffers,
DLC invalide consommé, détection différée de changement de mode, erreurs
SPI sur chaque étape du chemin rapide et bus-off actif.
L’utilisateur confirme l’écran lisible et une meilleure réactivité de
l’encodeur à LCD 20 MHz / MCP SPI 2 MHz.

Mesures intermédiaires (événements de débordement, pas trames perdues) :
- Avant : 2 205 reçues / 883 événements en environ 18 s, MCP isolé.
- Synchrone et réveils rapides : 4 443 / 180 en environ 30 s, combiné.
- LCD 20 MHz/MCP 2 MHz, sans INT : 4 464 / 38 sur environ 30 s.
- Avec INT34 : 4 540 / 1 sur environ 30 s, zéro perte de file,
  mais deux anciens timeouts de statistiques. Fronts INT réellement comptés.
- Essai d’affinité CPU imposée : dégradation (482 pertes de file, sept
  débordements en 40 s) ; réglage retiré. La version conservée laisse
  l’ordonnanceur répartir les tâches.

Le mécanisme de copie des statistiques remplace les anciens délais de 5/100 ms.
INT34 est effectivement utilisé et mesuré dans le dernier test combiné ;
le profil isolé sans INT reste disponible. La transmission/ACK et le mode
maître n’ont pas été testés physiquement lors de ces essais.

### Version finale conservée

Compilation sans avertissement : `build-components` (composants HUZZAH32),
`build-observe` (ESP32-C3 observation) et `build-huzzah32` (maître HUZZAH32).
Seul le profil composants passif a été flashé, empreintes vérifiées par esptool.
Image : `build-components/bionx_c3_supermini.bin` (cible matérielle ESP32).
SHA-256 : `48faec8913aeec92c0285777d1db2b9157814063d37ac45ef998eb66ee05a5f8`.

Capture finale environ 30 s, vélo et console d’origine actifs :
**4 512 trames reçues, zéro perte de file logicielle, trois événements de
débordement matériel**. Aucun assert/panic, RX latched off, échec
d’initialisation ni STATS BUSY. Init ESP_OK. À 25 s : 3 867 fronts INT ;
lecture RX moyenne 93–95 µs sur les fenêtres de cinq secondes, maximum
observé 899 µs. Ces durées incluent les attentes d’accès au bus partagé.
Preuve locale : `/tmp/bionx-final-unpinned.log`.

La fréquence des débordements est fortement réduite par rapport au point
de départ, mais la capture n’est **pas exhaustive** : les trois événements
résiduels restent à investiguer. Un événement ne permet pas de déduire
le nombre exact de trames perdues. Aucun résultat ne valide ici émission,
ACK, remplacement de console ou fonctionnement maître sur le vélo.


## Poursuite des débordements résiduels — 25 octobre 2026

Retrait des impressions `can_perf` de la tâche RX : les mesures cumulatives
(polls, durée totale/maximale, fronts INT) sont publiées, puis journalisées par
le consommateur. Ajout des compteurs malformed/SPI errors au diagnostic.
Premier essai combiné sur une minute : 9 169 reçues, drops=0, ov=8,
malformed=0, SPI errors=0. Ce changement seul ne supprime pas les pertes.
Preuve locale : `/tmp/bionx-no-rxlog-validation.log`.

En passif, une trame présente est désormais lue/acquittée avant les contrôles
mode/TEC/REC dus. La vérification reste exécutée avant livraison de la trame ;
un défaut de mode empêche donc toujours sa livraison. Les chemins actifs
conservent leur contrôle avant consommation. Tests hôte 3/3 réussis, avec
cas deux buffers pleins, faute de mode en passif/actif et fautes SPI à
chaque étape du chemin passif avec contrôle de santé dû.

Essai combiné suivant : 9 111 reçues, drops=0, ov=3 sur environ une minute,
malformed=0/SPI errors=0 ; aucun panic/assert, défaut RX ou STATS BUSY.
Maximum de lecture observé : 806 µs.
Preuve locale : `/tmp/bionx-rx-priority-validation.log`.

Comparaison MCP seul, **mêmes réglages** (SPI 2 MHz, INT34 + secours 1 ms),
LCD non initialisé : **9 240 reçues, drops=0, ov=0 sur environ une minute**,
malformed=0/SPI errors=0. Maximum de lecture : 295 µs.
Preuve locale : `/tmp/bionx-isolated-compare-validation.log`.
Ces mesures désignent le partage SPI/LCD comme piste principale ; une minute
sans événement ne garantit pas à elle seule une capture exhaustive permanente.

Un profil optionnel `sdkconfig.components-dual-spi` est préparé pour comparer
l’écran actif sur SPI2 et le MCP sur SPI3. Il exige SI→SDA23 et SCK→A5/GPIO4,
avec SO19/CS33/INT34 inchangés. Aucun firmware maître actif n’est flashé.


Compilation finale de cette étape : quatre profils réussis sans avertissement
(composants partagés HUZZAH32, observation ESP32-C3, maître HUZZAH32 et
composants deux bus HUZZAH32). Le profil deux bus n’est pas encore flashé :
confirmation du recâblage attendue. Dernier flash effectif à cet instant :
`build-mcp-isolated/bionx_c3_supermini.bin`, MCP passif sans rafraîchissement
écran, aux mêmes réglages SPI 2 MHz/INT34 que le test combiné.


### Premier essai à deux bus

L’utilisateur confirme avoir déplacé SI→SDA23 et SCK→A5/GPIO4. Flash
`build-components-dual-spi/bionx_c3_supermini.bin` réussi avec vérification
d’empreintes. Les deux bus s’initialisent : LCD SPI2 MOSI18/SCK5, MCP SPI3
SI23/SCK4/SO19. GC9A01 initialisé à 20 MHz, buffers DMA 9 600 octets.

Le MCP ne répond cependant pas au reset : CANSTAT=00, SPI errors=0,
initialisation ESP_ERR_TIMEOUT avant tout essai CAN. Le plan logiciel et
l’absence de réaffectation de ces GPIO dans l’application ont été vérifiés ;
contacts et éventuels ponts breadboard vers les signaux LCD sont à contrôler.
Preuve locale : `/tmp/bionx-dual-validation.log`.

Le dernier firmware flashé est désormais le profil **composants deux bus**,
pas le test MCP isolé. La réception CAN avec écran actif sur cette nouvelle
configuration n’est pas encore validée. Aucun maître actif n’a été flashé.


### Validation finale des deux bus — 25 octobre 2026

Après vérification/reconnexion utilisateur, le même profil initialise
correctement le MCP sur SPI3 : ESP_OK, écoute seule à 125 kbit/s. Premier
essai de 15 s : 2 227 trames, drops=0, ov=0, rotations et dix clics reçus.
La cause exacte du premier timeout n’a pas été mesurée ; il ne se reproduit
pas lors de ces redémarrages. Diagnostic GPIO en lecture seule ajouté sur
échec d’initialisation uniquement, jamais dans la tâche RX.

Dernière image compilée puis flashée et vérifiée :
`build-components-dual-spi/bionx_c3_supermini.bin`.
SHA-256 : `e3b1b06534a6d41361bad813a126efedcd49d9ba608bf67266c407f3a45f38a0`.
Cible HUZZAH32 ESP32 ; LCD SPI2 MOSI18/SCK5, MCP SPI3 SI23/SCK4/SO19,
CS33/INT34. LCD 20 MHz, MCP SPI 2 MHz ; buffers LCD DMA 9 600 octets.
Les deux bus et leurs verrous sont indépendants, sans affinité CPU imposée.

**Capture finale : 13 878 trames reçues sur environ 90 s ; drops=0, ov=0,
malformed=0 et SPI errors=0.** Trafic présent jusqu’au dernier échantillon
à 90 076 ms, aucun RX IDLE durant la capture. Aucun panic/assert, défaut RX,
échec d’initialisation ou STATS BUSY. À 85 s : 13 246 fronts INT ; moyenne
cumulative des lectures environ 83 µs, maximum observé 614 µs.
Preuve locale : `/tmp/bionx-dual-final-validation.log`.

L’utilisateur confirme que l’écran et l’encodeur fonctionnent et que
l’ensemble est « super réactif ». Ce profil est conservé comme référence
du banc. Les quatre profils compilent sans avertissement ; les trois tests
hôte passent. Ce résultat valide cet essai passif, pas une garantie
d’exhaustivité permanente ni le pilotage maître sur le vélo.

## Maître deux SPI et bouton — 25 octobre 2026

`build-master-dual-spi` compilé avec les defaults documentés dans
MASTER_BENCH.md : cible ESP32, maître actif, bouton22, MCP SPI3 SI23/SCK4,
quartz 8 MHz, SPI 2 MHz. Build réussi, sans warning compilateur ; image
0x76df0 octets. Tests hôte 4/4 : protocole, MCP, maître/contrôle, bouton.
Le test bouton couvre rebond, appui court, seuil long, unicité de l'arrêt,
absence de démarrage au relâchement long et bouton tenu au boot.
Aucun flash ; aucune validation physique TX, wake ou extinction.

### Diagnostics maître et interdiction de rotation — 25 octobre 2026

Build `build-master-dual-spi` réussi sans warning compilateur, tests 4/4.
Image : 499 392 octets ; SHA-256
`bc3b839e3563f2ac6d4d9dc7eba3540a05c36bbba24f947dd3230995db784f7e`.
`esptool image_info` : type ESP32, checksum/hash valides ; sdkconfig cible
ESP32/HUZZAH32. Le nom historique C3 du fichier ne décrit pas sa cible.
Toutes les émissions d'assistance testées sont nulles ; garde applicatif
supplémentaire avant transport. Contrôleur de couple non relié à l'application.

Tentative de flash rejetée avant exécution par la revue automatique :
maître CAN actif sur véhicule, demande d'autorisation explicite et vérification
compatibilité cible. Cette dernière est faite ; aucun flash ni essai TX réel.
Affichage physique, mesures, séquence démarrage et extinction restent à valider.

### Premier flash WAIT_WAKE — 25 octobre 2026

Autorisation explicite reçue : « vas y tu peux flash ». Profil
`build-master-dual-spi` chargé sur `/dev/cu.usbserial-016138BC`, hashes flash
vérifiés. Image initiale WAIT_WAKE 500 336 octets, SHA-256
`3421c362242d81eeaa527c47ce669b3092d60bbd80282812a5ed5078ffc6e772`.
Tests hôte 4/4, dont découverte sans réponse/échec TX, réponse tardive/erronée,
profil incorrect, autre maître, init automatique et arrêt sans redémarrage.
MCP : échecs de sondes répétés puis succès sans reset ; error-passive TX toléré
uniquement en découverte, bus-off fatal ; annulation sonde sans invalider le MCP.

Observation réelle dans `/tmp/bionx-master-wake-monitor.log` : boot ESP32,
MCP NORMAL one-shot 125 kbit/s ; OFF TX=0 RX=0 puis réponse HW et STARTING,
NEUTRAL à 2273 ms avec TX=28 RX=14, 3/8 mesures fraîches. À 2389 ms, arrêt
RX après timeout TX ; FAULT TX ERROR, TX=32 RX=16, pertes=0, TEC/REC=0,
un timeout. Init/profil/copie des limites ont donc abouti, mais le runtime
stable, les mesures physiques et l'extinction ne sont pas encore validés.

Correction : lecture finale du statut TX après expiration du budget de polling
avant d'abandonner ; aucune retransmission. Cela traite le cas d'un retour
d'ordonnancement tardif où TX était déjà terminé, sans déclarer un succès
si TXREQ reste actif. La cause matérielle du premier timeout n'est pas prouvée.

### Runtime stable après correction RX/TX

Le second flash (SHA-256
`612c9629b9e05737b545fbf3ce9c9bafd7b222f4c3a796305ff23774a3e53f7d`)
a encore échoué : TX deadline au registre batterie 0x21, compteur timeout
publié resté à zéro, correspondant à l'impossibilité de reprendre le verrou.
La lecture finale de statut seule ne résolvait donc pas le problème.

Correction suivante : en mode actif seulement, RX cède un tick sans verrou
avant de consommer la notification suivante. Les notifications timer déjà en
attente pouvaient sinon faire boucler RX et priver TX d'accès au verrou sur
l'autre CPU. Le chemin passif conserve son ordonnancement précédent. Ajout
maxpoll au diagnostic pour suivre le temps de possession du contrôleur.

Image testée ensuite : 500 672 octets, SHA-256
`b973ea032d7cd0e2bb496f307be4954cf7b13d14edb4a880e57c04c885b795cc`.
Flash/hash vérifiés ; log `/tmp/bionx-master-wake-monitor-v3.log`.
Sur environ 20 s : démarrage auto puis NEUTRAL/OK continu, dernier relevé
TX=2616 RX=1878, fresh=8/8, pertes=0, TEC=REC=0, txerr=timeout=0,
maxpoll=454 us. Valeurs candidates : ~39,57 V, 35 C, SOC75%.
Cela confirme les échanges de ce banc pendant cet intervalle, pas la
calibration physique des valeurs ni le réveil depuis un sommeil établi.
Aucune rotation commandée ; absence de rotation à confirmer par utilisateur.

Amélioration de tension : conserver la dernière paire HI/LO complète pendant
le rafraîchissement suivant, avec les mêmes vérifications d'ordre, skew et
expiration des deux octets. Test hôte couvre la conservation entre deux paires.

### Conservation de la tension et cadence de télémétrie

Image avec cache de paire : 500 784 octets, SHA-256
`8bfe69cb3e363f2c370a728cb2cecae03dd93e3d8f95af98400e137b5b59e19f`.
Flash vérifié, log `/tmp/bionx-master-final-validation.log` : init automatique,
NEUTRAL, tension continue ~39,57–39,59 V, moteur36 C, SOC75%, fresh=8/8.
À ~12,85 s : FAULT NO REPLY sur batterie0x30, TX=1549 RX=1103,
pertes=0, TEC/REC=0, TXerr=timeout TX=0. Aucun réessai automatique.
L'origine de cette réponse manquante n'est pas établie ; question utilisateur
sur une éventuelle action physique durant l'essai restée en attente.

Le polling de télémétrie a ensuite été borné : une pause de 1 s après un
cycle complet des huit lectures. Les délais de réponses et les défauts restent
inchangés, ainsi que les périodes neutres et de maintien batterie. Il s'agit
d'une cadence de banc pour les données affichées, pas d'une boucle d'assistance.
Tests hôte 4/4 et nouveau build réussi sans warning compilateur.

### Image finale flashée et essai 45 s — 25 octobre 2026

Profil `build-master-dual-spi`, image ESP32/HUZZAH32 500 832 octets,
SHA-256 `3d770db1b826c66b760be6bedd178fee49493ac8e31e9b849a80737db5389449`.
Flash à 115200 sur `/dev/cu.usbserial-016138BC`, hashes vérifiés, reset effectué.
Télémétrie avec pause de 1 s par cycle et cache de paire tension activés.
Build sans warning compilateur, 4/4 tests hôte réussis.

Preuve : `/tmp/bionx-master-paced-validation.log`, fenêtre d'environ 45 s.
Démarrage automatique après réponse batterie et NEUTRAL/OK continu.
Dernier relevé à 44 903 ms : TX=2115 RX=331, fresh=8/8, pertes=0,
EFLG=00, TEC=REC=0, txerr=timeout TX=0, maxpoll=440 us.
Tension candidate ~39,57 V, température moteur36 C, SOC75%.
Aucun FAULT, timeout, panic ou assertion observé dans cette fenêtre.
L'utilisateur indique « la batterie a l'air de parler là » pendant l'essai.
La cadence réduite tient sur cet intervalle ; elle ne prouve pas à elle seule
la cause du NO REPLY précédent ni une absence permanente de réponse manquante.

Implémenté/observé : init CAN, maintien neutre, lectures et diagnostics.
Toujours à confirmer : calibration au multimètre/thermomètre, affichage physique
complet, réveil depuis batterie réellement endormie et extinction électrique.
Aucune commande d'assistance non nulle dans l'application, ni essai de rotation.

## Contrôle de banc : logiciel et barrières — 25 octobre 2026

Dépôts analysés à commits identifiés dans ASSIST_BENCH.md. Code propre du
contrôleur `bionx_bench`, aucune reprise de source externe. L'utilisateur a
confirmé avant cet incrément le réveil fonctionnel, les valeurs cohérentes et
l'extinction. Il demande le frein préparé mais non connecté pour les premiers tests.

5/5 tests hôte : protocole, MCP2515, maître/contrôleur historique, bouton,
bench_safety. Ce dernier couvre calibration à échantillons distincts,
refus d'armement chargé, limites/rampes, relâchement, fin de burst et armement,
expiration/invalidité de mesures, horloge, frein, sauts de couple, verrouillage
des défauts et autorisation finale. Intégration maître : acquisition rapide,
refus d'une valeur négative encodée ou >4, expiration commande, neutre immédiat
sur arrêt, deux tentatives neutres sur défaut protocolaire, interruption après
échec et absence de réessai automatique sur transport indisponible.

Builds ESP32/HUZZAH32 séparés : `build-assist-bench` et
`build-master-dual-spi`, réussis sans warning compilateur. Les branches
conditionnelles TX vérifiée et frein36 ont également été compilées avec le
compilateur ESP32 et -Werror, sans les activer dans l'image livrée.

Image banc calcul à blanc : 504 752 octets ; SHA-256
`211512aeb3ea824fa6d502b0795086e4bac7faef6bec340ae8923295fe60a6ae`.
`esptool image_info` confirme ESP32, checksum et hash valides.
Config livrée : BIONX_ASSIST_BENCH=y, ASSIST_TX_VERIFIED non défini,
ASSIST_BRAKE non défini. Aucune commande série ne peut lever ce verrou TX.

Essai supplémentaire ASan/UBSan non concluant : exécutable bloqué sans
résultat, interrompu ; un programme instrumenté vide subit aussi un timeout.
Il s'agit donc d'une limitation de vérification dans cet environnement ;
aucun succès sanitizer n'est revendiqué. Les cinq tests non instrumentés passent.

Toujours non validés physiquement : loi couple/gain, bornes et unités des
limites, contact frein, coupure matérielle et watchdog moteur sur perte CAN.
La sortie positive reste désactivée. Le logiciel ne garantit pas une roue libre
physique si le CAN, l'ESP ou l'étage de puissance est défaillant.

## CAN alimenté et test encodeur demandé — 25 octobre 2026

Le profil calcul à blanc a été flashé, hash vérifié, puis observé. Plusieurs
échecs TX : TXCTRL50, TXINTF80 (ABTF/TXERR et MERRF), aucun couple demandé.
Ce statut ne désigne pas une simple perte d'arbitrage ; il ne permet pas à
lui seul d'identifier une cause électrique. L'utilisateur signale ensuite
qu'il rallume le système. Les ouvertures du port série réinitialisent aussi
la carte sur ce banc : éviter de rouvrir un moniteur pendant une rotation.

Capture après rallumage `/tmp/bionx-assist-powered-comparison.log` : 30 s,
init puis NEUTRAL/OK continu, TX2163 RX1013, fresh8/8, pertes=0,
TEC/REC=0, TXerr=timeout=0, maxpoll391 us. Aucun recâblage ni défaut désactivé.
L'extinction est une cause plausible des échecs antérieurs, pas une preuve unique.

Utilisateur demande ensuite pilotage par rotation encodeur avec clic neutre,
confirme roue levée et possibilité de débrancher la haute tension. Ce contexte
autorise le profil de banc dédié, sans prétendre que le watchdog moteur est
mesuré. Pedal-assist et jog sont deux modes distincts et ne partagent pas
leur autorisation TX. Le frein reste désactivé à sa demande pour cet essai.

6/6 tests hôte passent. Nouveau jog_safety : cible initiale zéro, rotation
ignorée sans données, plafonds cible/commande2, rampe, baisse de cible,
consigne zéro à la cible, clic prioritaire, expiration3 s, durée10 s malgré
rotations, données périmées/hors plage, sur-vitesse, frein, erreur encodeur,
horloge, défaut verrouillé et réarmement explicite uniquement stationnaire.
Builds encodeur dry-run et live ESP32/HUZZAH32 réussis sans warning compilateur.

Image finale flashée : `build-encoder-jog/bionx_c3_supermini.bin`,
configuration `build-encoder-jog/sdkconfig-live`, 504 768 octets, SHA-256
`596e9e5c1aa949dc4b7c05237e81ad468e08816ecfe54e4c7bbf6d72be407c94`.
Cible ESP32 vérifiée, checksum/hash image valides ; flash à115200 sur
`/dev/cu.usbserial-016138BC`, empreintes des données vérifiées, reset effectué.
Config JOG_TX=y, ASSIST_TX_VERIFIED et ASSIST_BRAKE non définis.

Capture `/tmp/bionx-jog-live-validation.log`, environ15 s : init/NEUTRAL OK,
JOG LIVE READY, Target0 speed0 cmd0 sur toute la fenêtre. Dernier relevé
à14894 ms : TX1079 RX560, fresh8/8, pertes=0, TEC/REC=0,
TXerr=timeout=0, maxpoll358 us, ~39,50 V/36 C/SOC74% (conversions candidates).
Aucune commande positive capturée et aucune rotation déclenchée par l'assistant.
Prêt pour le geste utilisateur ; rotation, arrêt physique par clic et timeout
moteur sur perte CAN restent des validations matérielles à effectuer.

### Premier retour de rotation utilisateur

L'utilisateur indique que le moteur a un peu tourné puis s'est arrêté, que
l'encodeur ne permet plus de le relancer, et que l'écran affiche JOG LIVE FAULT.
Pas de reboot à ce stade. Le défaut jog est verrouillé par construction ;
une nouvelle rotation ou un clic ne l'efface pas. Le motif précis et l'état
maître sont demandés à l'écran avant toute modification. Le port série n'est
pas rouvert car son ouverture réinitialise la carte sur ce montage.
Cette observation confirme un premier mouvement, pas encore la cause de
l'arrêt ni la validation matérielle du clic ou d'une perte CAN.

### Photo du défaut ENCODER et correction d'acquisition

Photo utilisateur : JOG LIVE FAULT / ENCODER ; Target0 speed0 cmd0,
NEUTRAL/OK, batterie39,50 V candidate, moteur36 C candidat, SOC74%,
TX4317/RX2253, RX3 ms, fresh8/8, pertes0, TEC0 REC0. Le défaut enregistré
est celui de l'encodeur ; le relevé courant du maître est sain. Cela ne prouve
pas à lui seul l'origine électrique ou logicielle exacte de la transition.

Hypothèse reproduite : en polling, 00→10→11 entre deux lectures apparaît
comme00→11 et déclenche le défaut. Nouvelle acquisition sur interruptions
GPIO25/26 et22, file de16 crans complets, clic mémorisé et prioritaire,
rebonds/inversions cohérents annulés ; transition impossible/overflow toujours
bloquants. L'autorisation finale consulte aussi le stop/défaut IRQ en attente.
Un défaut de dernière seconde est verrouillé avant la substitution par neutre.

7/7 tests hôte réussis (nouveau encoder_capture). Build du même profil
`build-encoder-jog`, sdkconfig-live, réussi sans warning compilateur.
Image505424 octets, SHA-256
`e69aa91f2dc4b47c7411c92992336191aff3ba78e477a64909722fc5f00c6297`.
Limites cible/commande, expirations et absence de frein conservées. Aucune
sécurité supprimée ni commande de rotation envoyée par l'assistant.

Flash de la correction IRQ effectué sur `/dev/cu.usbserial-016138BC` à115200,
empreintes vérifiées et reset. Log `/tmp/bionx-jog-irq-validation.log`, environ12 s :
init puis NEUTRAL/OK, JOG LIVE READY, Target0 speed0 cmd0, EncErr0 Ov0.
Relevé complet à11402 ms : TX810 RX422, 8/8 mesures fraîches, pertes0,
TEC/REC0, TXerr/timeout0, maxpoll411 us. Aucun FAULT ou panic observé.
Le test porte sur boot/communication à commande zéro, sans manipulation
encodeur par l'assistant. Les mouvements et le clic utilisateur restent à valider.

### Photo suivante : DATA FAULT est secondaire à TX ERROR

Après le premier mouvement très bref, nouvelle photo : JOG LIVE FAULT,
DATA FAULT, Target0 speed0 cmd0, EncErr0 Ov0 ; maître FAULT/TX ERROR,
TX1124/RX580, RX34780 ms, fresh0/8, pertes0, TEC0 REC0. Les mesures ont
été invalidées par le défaut du transport : cette photo ne démontre pas
quelle mesure ou quel composant serait défaillant. L'utilisateur rappelle
que le CAN fonctionne ; aucun recâblage n'est effectué ni présumé nécessaire.

Mise à jour purement diagnostique : conserver dans la tâche maître la première
trame TX échouée, l'erreur SDK et le snapshot MCP après le retour d'envoi.
Les erreurs de sonde WAIT_WAKE sont exclues. Les compteurs TEC/REC sont relus
sur statut d'erreur avant l'abandon ; auparavant l'affichage pouvait conserver
le relevé RX antérieur. L'écran de défaut montre type, code, ID/registre/valeur,
CTRL/INTF/EFLG/mode et TEC/REC figés. Pas de retransmission ni baisse de sécurité.

Décodage vérifié sur Microchip MCP2515 DS20001801K, registre3-1, p18 :
ABTF bit6, MLOA bit5, TXERR bit4 ; sections3.4/3.6 pour le mode one-shot.
La classe TX BUS ERROR ne distingue pas à elle seule ACK, bit, forme, etc.
Source : https://ww1.microchip.com/downloads/aemDocuments/documents/APID/ProductDocuments/DataSheets/MCP2515-Family-Data-Sheet-DS20001801K.pdf

7/7 tests passent, dont classement TXCTRL60 arbit., TXCTRL50 erreur émission,
timeout, SPI et bus-off. Build réussi sans warning compilateur.
Image506304 octets, SHA-256
`50b42285c8b142e18580caa03ce697f9287670d118e314db8175d34908fd3473`.
Même profil build-encoder-jog/sdkconfig-live flashé à115200 sur le port HUZZAH32,
hashes vérifiés et reset effectué. Aucun mouvement déclenché par l'assistant.
Aucune réouverture série après ce flash : laisser le prochain défaut visible
sans risquer un reset du port. La cause exacte reste à relever au nouvel écran.

## 2026-10-25 — Jog : délai 20 secondes

Demande utilisateur après TIMEOUT avec état toujours READY : délai sans cran
et limite totale de session portés à 20 s. Clic, fraîcheur des mesures,
limites de commande/vitesse et verrouillage des défauts conservés.
Tests hôte : 7/7, bornes de délai à 20 000 000 us vérifiées.
Build et flash du profil build-encoder-jog (JOG_TX=y) réussis, hash flash vérifié.
Image bionx_c3_supermini.bin, 506304 octets, SHA256
2c2423155390575d7d5fa1c0b7fb7f77d8167fe7077a1979238527116a284153.
Journaux /tmp/bionx-jog-20s-{build,flash}.log. Aucun mouvement lancé par logiciel
de test ; comportement matériel après ce flash restant à confirmer.

## 2026-10-25 — Quatre niveaux de jog et indicateur puissance

Demande utilisateur : davantage de crans et de commande, puissance à l’écran.
Niveaux 1–4 : cible brute (niveau+1)/2, plafond de commande niveau/64,
rampe 1 brut/200 ms. Cibles 1–2 et défaut vitesse >=3 conservés.
Clic, fraîcheur, seuil puissance, délai/session20 s et absence de reprise automatique conservés.
Affichage P : registre0x14 ×1,5625 en % candidat, jamais présenté en watts ;
masqué si ancien de >=500 ms.
Tests 7/7 ; build sans warning ; flash jog live et hash vérifiés.
Image build-encoder-jog/bionx_c3_supermini.bin, 506496 octets.
SHA256 e71866f43868ad70b3c8d7fa21c95c34a98da0fd9707d231aeb4a673c969cb0c.
Journaux /tmp/bionx-jog-4levels-{build,flash}.log.
Aucun mouvement déclenché par l’assistant ; essai physique restant à confirmer.

## 2026-10-25 — Maintien à la cible et correction du filtre final jog

Arrêt sans erreur après1–2 s rapporté après passage à quatre niveaux.
Inspection : bench_app_tx_allowed refusait encore data[3]>2, déclenchant
le disarm générique aux commandes3–4. Défaut logiciel avéré ; corrélation
avec le symptôme physique à confirmer. Le filtre et le contrôle partagent
désormais BJ_MAX_LEVEL=4.
À égalité de vitesse : réduction à1 brut, reprise depuis zéro via rampe200 ms ;
au-dessus : zéro, sans perdre la cible. Autorisation finale plafonnée à1
à égalité. Clic/défauts/fraîcheur/vitesse>=3/session20 s conservés.
Tests7/7 : maintien3 s à cible, autorisation, dépassement, défaut horloge,
plus limites/délais existants. Build sans warning ; flash/hash vérifiés
du profil build-encoder-jog, image bionx_c3_supermini.bin (506576 octets).
SHA256 1fee1e25d17127b5e72192c261d84b0de2c280d33b1295b5fff42f8955d6eef3.
Journaux /tmp/bionx-jog-hold-{build,flash}.log.
Aucun mouvement lancé par l’assistant ; maintien physique non encore validé.

## 2026-10-25 — Huit crans, diagnostic indicateur puissance nul

Utilisateur demande plus de crans et signale puissance affichée0.
Huit niveaux : commande plafonnée au niveau (max8/64), cible brute1
aux niveaux1–4, cible2 aux niveaux5–8. Rampe200 ms et maintien1
à cible conservés. Plafond maximum maître8, réglage8 seulement au profil jog,
profil pédalage reste4 et profil normal0. Filtre final partagé avec BJ_MAX_LEVEL.
Clic, délai/session20 s, seuils/fraîcheur/overspeed et verrouillage inchangés.
Références Minimal Translator Inc/CAN-Registers.h et Src/main.c :
0x14 est power_meter (% candidat1,5625), lu dans RxData[3]. Aucun
registre de mesure courant/watts identifié dans ces références moteur.
Lecture0x14 accélérée250→50 ms pour fast_poll, affichage P14 brut + pic
depuis début de session, stale après500 ms ; zéro non remplacé par estimation.
Cause physique du zéro non déterminée, mesure watts non disponible.
Tests7/7 : huit niveaux/caps/descente, maître accepte8/refuse9, expiration
vers zéro ; garde-fous existants passent. Build sans warning et flash/hash vérifiés.
Image build-encoder-jog/bionx_c3_supermini.bin, 506656 octets, SHA256
c8d284c444d478a0893f99aed28cfa1d00667db6e3c8d3b2002fecf5d9326d99.
Journaux /tmp/bionx-jog-8levels-{build,flash}.log.
Aucun mouvement initié par l’assistant ; validation physique restante.

## 2026-10-25 — Overspeed signalé, watts électriques batterie

Utilisateur signale OVERSPEED et demande watts instantanés. Seuil logiciel
vitesse brute>=3 conservé. Le maintien positif imposé à la cible pouvait
accélérer une roue déchargée ; cause exacte non prouvée faute de capture
vitesse/commande au déclenchement. Retrait de ce maintien : zéro à cible,
reprise en dessous sans désarmement. Plafond diagnostic2/64, plafond1 dès
vitesse non nulle, rampe200 ms. Plafond maître, contrôleur et filtre final
partagent BJ_MAX_DRIVE. Huit positions conservées mais certaines équivalentes
avec ce plafond réduit. Vmax brut mémorisé pendant la session/défaut.

Recherche élargie à la batterie : Minimal Translator Inc/CAN-Registers.h
(REG_BATTERY_STATUS_CELLPACK_CURRENT_HI/LO) décrit1E/1F signé en mA,
lecture shunt sans calibration. L’indicateur moteur14 n’est pas converti
arbitrairement en watts. Réponse utilisateur « oui » interprétée explicitement
comme courant passant par shunt BionX ; chemin physique et calibration restent
à confirmer. Le précédent constat d’absence de registre courant concernait
la référence moteur et était incomplet pour la batterie.

Calcul Pbat=U(A6/A7)*I(1E/1F), signé, unités V et A ; puissance électrique
batterie incluant éventuellement auxiliaires, jamais puissance mécanique roue.
Lecture additionnelle activée uniquement par cfg.measure_power (jog).
Demandes U/I100 ms, hi/lo complets non réutilisés pour le courant,
écart hi/lo<100 ms, âge<500 ms, décalage U/I<=200 ms et U30–42 V requis.
Sinon Pbat indisponible et jog inhibé/défaut si actif. Expiration U/I aussi
contrôlée avant émission. Affichage W? et I signé : calibration/signe non
validés sur ce montage. Zéro réel non remplacé par estimation depuis consigne.

Tests8/8 : watts positifs/négatifs, ordre/skew/expiration, hi partiel,
absence des nouvelles requêtes si désactivées, coasting à cible sans perte
de session, reprise sous cible, réduction à1 si vitesse1, seuil3 maintenu.
Build sans warning et flash/hash vérifiés du profil build-encoder-jog,
bionx_c3_supermini.bin, 507360 octets, SHA256
147a506f8a527fb834837fdfb60ac45b08f256e75b6e53ba0e728ee4444c1357.
Journaux /tmp/bionx-jog-watts-{build,flash}.log. Aucun mouvement initié
par l’assistant ; mesure CAN réelle, signe/calibration et stabilité à confirmer.

## 2026-10-25 — DATA FAULT après ajout U/I : ordonnanceur et publication

Utilisateur rapporte DATA FAULT rapide et commande faible. Plafond2 conservé
après OVERSPEED ; aucune augmentation de commande ni relâchement de seuil.
Simulation hôte avec tick5 ms et réponses sérialisées retardées : ancien
round-robin à10 lectures donne vitesse âgée165 ms pour latence11 ms,
245 ms pour16 ms (>seuil150 ms). Cause logicielle reproduite ; attribution
au défaut physique utilisateur restant à confirmer.

Correction mode measure_power seulement : priorité vitesse50 ms, curseur
de tour général indépendant pour éviter la famine après requête prioritaire,
groupe tensionHI/LO puis courantHI/LO (vitesse peut interrompre), couple
pédale inutilisé au jog ralenti1 Hz et P14 à4 Hz. Les délais de validité
restent identiques. Publication puissance depuis snapshot U/I complet,
indépendant d’une nouvelle tension encore sans nouveau courant. Ancienne
publication pouvait invalider temporairement le calcul entre deux lectures.

Tests permanents dans electrical_power : réponses1/6/11 ms pendant5 s,
après établissement vitesse fraîche<150 ms et U/I valide sans trou, watts
attendus39,5. Avec délais plus élevés (simulation16–26 ms), expiration U/I
reste possible ; elle doit encore arrêter, aucun contournement ajouté.
Diagnostic figé du premier BJ_DATA : LINK/MASTER, SPEED OLD, P14 OLD,
TEMP OLD, VOLT OLD, U/I INVALID et plages, avec âge/valeurs en log.

8/8 tests passent, build sans warning et flash/hash vérifiés du profil
build-encoder-jog, bionx_c3_supermini.bin (508464 octets), SHA256
411035f7c4f002c4cdc466174b5b474b3764502e407100e5d6e52499cd0763ae.
Journaux /tmp/bionx-jog-datafix-{build,flash}.log. Aucun mouvement lancé
par l’assistant ; correction du symptôme physique à confirmer sur le banc.

## 2026-10-25 — Clic acquitte le jog ; couple de démarrage augmenté

Demande explicite : effacer défaut au clic, permettre une rotation effective.
Clic pendant BJ_FAULT arme uniquement un acquittement au relâchement.
Au relâchement : données valides, vitesse0 et encodeur cohérent requis ;
cible/output restent0, rotation concomitante ignorée dans core et app.
Si conditions refusées, attente d’un nouveau clic, pas d’acquittement différé.
Clic en RUNNING garde priorité arrêt. Les défauts CAN/MCP persistants restent
bloquants : le geste ne reset pas le transport. Réarmement série conservé.

Plafond partagé BJ_MAX_DRIVE8, commande plafonnée au niveau (1–8) ;
BJ_ROLLING_DRIVE4 dès vitesse non nulle, rampe1 brut/200 ms, zéro à cible
et reprise sous cible. Seuil overspeed3, télémétrie, expiration100 ms,
délai/session20 s, diagnostic DATA/U/I et absence de régène conservés.
Défaut BJ_STALL/NO MOTION si aucune vitesse détectée depuis2 s et
commande précédente>=4 ; cible et commande annulées. Validation finale TX
vérifie aussi cet âge. Ce garde-fou ne remplace pas un capteur de courant
phase et la stabilité mécanique reste à confirmer.

Tests8/8 : huit niveaux jusqu’à8, réduction rolling4, coasting/reprise,
acquittement au relâchement, aucun départ sur geste combiné, refus si donnée
invalide/roue mobile/encodeur mauvais, aucune récupération différée,
NO MOTION à2 s et garde-fous existants. Build sans warning ; flash/hash
vérifiés du profil build-encoder-jog, image bionx_c3_supermini.bin,
508688 octets, SHA256
97574dfb7dac040f3a19737c9d584ff97de16f71c78e626dc0cf7ba8d90f5cd7.
Journaux /tmp/bionx-jog-ack-start-{build,flash}.log. Aucun mouvement
lancé par l’assistant ; essais de clic et rotation restant à confirmer.

## 2026-10-25 — Palier de vitesse supplémentaire demandé

Utilisateur confirme amélioration puis demande plus vite. Cibles brutes1/2/3
pour niveaux1–3/4–6/7–8 ; limite cible BJ_MAX_TARGET3, coupure
BJ_OVERSPEED_RAW4 (au lieu de3) annoncée avant modification. Facteur tiers
9,091 rpm/brut donne ~27 rpm cible maximale candidate, calibration non validée.
Coasting à cible, aucun maintien positif imposé, plafonds commande8/4,
rampe200 ms, clic stop/acquittement sous conditions, données/U/I, défaut
sans mouvement et délais20 s/100 ms conservés. Profil exclusivement banc
roue levée ; dépassement par inertie possible, aucune garantie mécanique.

Tests8/8 : correspondance des huit positions, autorisation rolling à vitesse2
pour cible3, zéro à vitesse3 sans perte de session, défaut à vitesse4,
garde-fous et temporisations existants. Build sans warning et flash/hash
vérifiés du profil build-encoder-jog, bionx_c3_supermini.bin (508704 octets).
SHA256 737d8871a00a3b88584ac25948ee921db4e863c61620aa9a899a4342308095c3.
Journaux /tmp/bionx-jog-speed3-{build,flash}.log. Aucun mouvement lancé
par l’assistant ; vitesse réelle et stabilité à confirmer sur le matériel.

## 2026-10-25 — Dix crans0–20 km/h, survitesse25 km/h estimés

Demande utilisateur explicite de changer l’échelle : dix crans positifs de
2 km/h (niveaux1–10→2,4,…20), niveau0 arrêt, OVERSPEED>=25 km/h.
Pneu précisé par utilisateur :700C puis28, interprété700×28C.
Référence primaire CatEye CC-PA100W : circonférence indicative2136 mm
https://www.cateye.com/intl/support/manual/data/doc/CC-PA100W_HP_ENG_n_v4.pdf
Circonférence mesurée non fournie ; valeur table annoncée provisoire.

Ajout CONFIG_BIONX_JOG_WHEEL_MM, défaut0 inhibiteur, overlay live2136.
Même valeur dans sdkconfig-live réellement compilé. bj_set_wheel autorisé
uniquement en IDLE sans sortie, plage1000–4000 mm ; reset/acquittement conserve
la configuration. Conversion candidate9,091 rpm/brut, vitesse en milli-km/h
entier64 bits arrondi supérieur. Cible2*niveau en km/h, comparaisons de cible
et survitesse/final TX dans les mêmes unités. Commande max8 au démarrage,
rolling4, rampe200 ms, coasting à cible, temporisations et protections U/I,
NO MOTION et clic acquittement conservés. Affichage L/10, T et V~km/h,
max~km/h et commande brute ; vitesse périmée affichée--.

Avec2136 mm, raw21→24,468 et22→25,633 km/h arrondis : coupure dès le
premier bin>=25, aucun plafond mécanique exact garanti (quantification,
conversion candidate, circonférence et latence/inertie non calibrées).
Profil demeure réservé au banc roue levée.

Tests8/8 : dix positions/target20/caps, coast raw18 au-dessus20, raw21 coast
sans défaut et raw22 OVERSPEED, conservation roue à l’acquittement,
configuration absente/invalide inhibitrice, conversions et garde-fous existants.
Build sans warning et flash/hash vérifiés du profil build-encoder-jog,
image bionx_c3_supermini.bin (509280 octets), SHA256
66c1c0beee33c877dc7590fa9510b4e735d2dd4a697c51d5b1668b7891cede0c.
Journaux /tmp/bionx-jog-kmh-{build,flash}.log. Aucun mouvement initié
par l’assistant ; mesures physiques de vitesse/calibration restant à faire.

## 2026-10-25 — Pilotage confirmé, affichage pédalage, bilan pour code final

Utilisateur confirme explicitement le pilotage moteur fonctionnel sur ce banc,
puis demande la lecture de force pédale et la consolidation des trouvailles.
Cette confirmation ne vaut pas validation de vitesse réelle, watts, frein,
watchdog CAN ou usage routier. Nouveau bilan BENCH_FINDINGS.md, lié depuis
README, MASTER_BENCH, HARDWARE, JOG_BENCH et project_requirements ; ancien
résumé master de plafonds2/délais3/10 s remplacé par l’état réel courant.
Journal historique conservé, confirmations et inconnues distinguées.

Affichage moteur0x21 depuis BX_TORQUE : Ped raw, maximum depuis boot,
âge en ms ; donnée périmée>=1 s marquée stale. Requête couple visée250 ms
au lieu de1 s en mode measure_power, priorité vitesse et snapshots U/I
conservés. Aucun changement à l’autorisation de couple : appui pédale ne
commande pas le moteur dans le jog. Aucune conversion N/Nm non calibrée.

Tests8/8 : réponse simulée0x21=37 correctement enregistrée, lectures
répétées ; tests de charge avec latences1/6/11 ms et fraîcheur vitesse/U/I
passent avec le nouveau polling. Build sans warning ; liens locaux du bilan
et documents principaux vérifiés sans manque. Flash/hash vérifiés du profil
build-encoder-jog/sdkconfig-live, bionx_c3_supermini.bin (509520 octets).
SHA256 7aba6b67f6c0ce945dffb3d94d70c32d8f4a2d1d56fc837b1439a9c5c7b517ca.
Journaux /tmp/bionx-jog-pedal-display-{build,flash}.log. Aucun mouvement
lancé par l’assistant ; affichage physique et réponse sous effort à confirmer.

## 2026-10-25 — Confirmation capteur et audit de reprise agent

Utilisateur répond « ça marche bien » après ajout de la lecture/affichage du
capteur de pédalage. Confirmation fonctionnelle consignée, sans inventer de
valeurs repos/effort ou de calibration N/Nm. HANDOFF.md et AGENTS.md créés ;
README, bilan, cahier, TODO, JOG, master, matériel et assistance séparée
renvoient vers l’état courant ou le distinguent des étapes historiques.

L’audit a détecté une erreur d’édition documentaire : project_requirements.md
avait été remplacé par un autre document. Restauration des quinze sections
originales v1.0 depuis la lecture complète conservée dans le journal du
23 octobre (session01a16a85-48f2-7be3-9c31-d82751ae4e79,
réponse outil ligne22). Le début est identique à la lecture du25 octobre
jusqu’à sa limite (1978 caractères). Texte original récupéré :243 lignes,
19442 caractères. Ajout séparé de la section16 pour les acquis récents ;
aucune réécriture des exigences1–15 restaurées. Le cahier est de nouveau
un cahier des charges distinct d’ASSIST_BENCH.md.

Contrôles documentaires réussis : liens locaux de onze documents, présence
et ordre des sections1–16, conservation intégrale du texte récupéré,
flags HUZZAH32/jog live/circonférence2136, limites10 niveaux/20 km/h cible/
25 estimés/commande8–4 vérifiées dans les sources, hash et taille du binaire
existant identiques au handoff. Dernier firmware reste l’image pédalage
7aba6b67f6c0ce945dffb3d94d70c32d8f4a2d1d56fc837b1439a9c5c7b517ca,
509520 octets. Aucun source/config firmware modifié, aucun build/flash,
aucun mouvement ni nouvel essai physique pendant cet audit. Les8/8 tests
mentionnés dans le handoff sont ceux de la dernière livraison firmware.


## 2026-10-25 — V1 assistance ×2 et publication Git

Demande utilisateur : init au boot, interface originelle, un mode pédale×2,
aucune régène, puissance visible ; GitHub, commits réguliers et rangement.
Aucun dépôt Git local préexistant, dépôt distant vide accessible via Git.
Base importée sur `main`, incréments sur `feature/assist-v1`, publiés sur
https://github.com/tlacassi/BIONX_console_ESP32 . Le connecteur GitHub répondait404 ;
Git direct a permis lecture/écriture. `docs/` et `captures/` organisés,
liens Markdown locaux vérifiés, profils source conservés à la racine.

Sources tierces relues aux mêmes commits que ASSIST_BENCH (main.c et
sparkfun_bionx.ino), aucun code importé. Contrôle V1 indépendant : calibration
explicite16 échantillons distincts, armement local stationnaire au neutre,
`min(8,2*max(0,torque-zero-2))`, rampe200 ms, baisse immédiate, réduction
20–25 km/h candidats, aucune commande négative. Arrêt bouton vérifié à la
boucle et avant TX, consigne100 ms, fraîcheur couple/vitesse150 ms, défauts
verrouillés ; NO MOTION après2 s sans vitesse sous commande≥4. Frein36
optionnel conservé, absent dans les profils livrés. Aucun armement au boot.

Dashboard original repris : anneau, vitesse, SOC ; mode fixe ASSIST X2,
watts électriques batterie signés depuis les paires U/I cohérentes, marqueur
BAT W? et vitesse estimée km/h~. CMD est la dernière commande dont le transport
confirme l'émission, suivie après substitution éventuelle par neutre. Ce
suivi d'émission n'établit pas la réponse physique du moteur. Valeurs absentes
ou périmées masquées, watchdog d'affichage500 ms sans update ; motif de défaut
préservé. Pas d'inférence de watts depuis le capteur ou le niveau de commande.

Vérifications logicielles exécutées :

- `cmake -S tests/unit -B build-unit`, build, ctest : **9/9 réussis**.
  Gain/saturation, calibration refusée sous charge ou sur réponses dupliquées,
  armement, rampe, chute immédiate, seuil vitesse, frein, données anciennes,
  horloge, saut couple, NO MOTION et défaut verrouillé. Tests existants conservés.
- Ordonnanceur V1 : vitesse et couple prioritaires50 ms ; scénarios sérialisés
  à latences1/6/11 ms sur5 s gardent couple/vitesse frais et puissance U/I
  valide. Profil jog testé en parallèle avec sa cadence couple précédente.
- `cmake -S tests/host -B build-host`, build, ctest : **2/2 réussis**,
  dashboard démo et compilation conditionnelle V1. Vrai LVGL : validité,
  NaN/refus, verrou occupé, watts signés±1400, géométrie circulaire, états,
  péremption et conservation du motif défaut. Rendu240×240 inspecté visuellement.
  Un débordement à quatre chiffres corrigé par police16 pour la puissance réelle.
- Rejeux `tools/check_cl1000.py` :27679 trames,575 UNKNOWN, aucun rejet ;
  CSV start&stop :1690 trames,42 UNKNOWN, aucun rejet. Hashes inchangés.
- ESP-IDF local5.2-dirty : builds **V1 dry-run, V1 live et jog réussis sans
  warning compilateur**. Gestionnaire de composants nécessite accès psutil
  hors sandbox ; aucun fichier SDK modifié. Branche optionnelle frein36 de
  assist_app compilée avec-Werror, sans activation dans les profils livrés.
- `esptool --chip esp32 image_info` de la V1 live : checksum et hash valides.

Images construites, nom historique `bionx_c3_supermini.bin`, cible ESP32 :

| Profil | Taille | SHA256 fichier |
|---|---:|---|
| build-assist-v1 (TX=0) |643664|961465f82d5f1e5065435a69490e88637f4953c635bb054c37efdda2d4b8a48b|
| build-assist-v1-live (V1_TX=y) |643888|db2ce015acafc014da0cc105596a84e5266f59f9f3b9c843c809d8c9b33f39dc|
| build-encoder-jog (régression, non flashé) |509680|8050a5f5b30c4f43f3d67ba3e27db0cfa01ad9fe3734f83d6eb479331f6b9963|

Logs temporaires `/tmp/bionx-v1-{build,live-build,jog-build,unit-build,ui-build,replay}.log`.
Les hashes peuvent changer après un nouveau commit/build car la version Git
est intégrée dans l'image ; ils identifient précisément les fichiers vérifiés ici.

**Aucun flash, aucune ouverture série ni nouvel essai physique.** Le firmware
chargé connu reste le jog509520 octets/hash7aba6b67… du précédent essai ; le
rebuild jog local n'est pas ce firmware flashé. Le gain physique×2 n'est pas
calibré, capteur de couple sans cadence, vitesse et watts candidats, frein non
raccordé, timeout moteur sur perte CAN/reset non mesuré. Ni ce build ni ces
tests ne valident un usage roulant. Procédure : [ASSIST_V1](ASSIST_V1.md).


## 2026-10-25 — Flash V1 calcul à blanc autorisé, boot et CAN

Autorisation utilisateur « go, je suis bien roue levée en securité » suite à
la proposition explicite du profil calcul à blanc. Rebuild build-assist-v1
réussi sans warning ; config ESP32/HUZZAH32, ASSIST_V1=y, V1_TX non défini.
Image643664 octets, SHA256 fichier
`db189abb652b285cfe9bdd558ddd1f16716cc9dfe34817b8a3fd75b49111f5d0`.
Version source cdea92d, TODO modifié pour suivre l'opération ; nouveau hash
lié notamment à la version Git embarquée. esptool image_info checksum/hash valides.

Port libre vérifié, flash115200 sur /dev/cu.usbserial-016138BC réussi ;
ESP32-D0WDQ6 rev1.0 identifié, application/bootloader/partitions/ota_data
empreintes vérifiées. Reset esptool puis capture série20 s avec reset RTS
contrôlé, aucune commande série de calibration/armement/assistance envoyée.
Port fermé à l'issue. Log durable : [capture boot](logs/2026-10-25-v1-dry-boot.txt).

Au début : ROM `flash read err, 1000`, puis RTCWDT_RTC_RESET et démarrage
normal. Cause non établie ; pas de boucle de reset observée après démarrage
applicatif dans cette fenêtre. Ne pas présenter cette capture comme plusieurs
boots sans anomalie. Dashboard init réussi à764 ms, GC9A01/LVGL actifs.
WAIT_WAKE à~1,8 s, cinq sondes TX échouées avant réponse batterie ; échecs
tolérés en découverte, cause physique non attribuée. STARTING à6,9 s puis
NEUTRAL/OK à7,4 s. Runtime ensuite stable jusqu'à19,5 s : TX1421/RX923,
fresh8/8, pertes0, EFLG0, TEC/REC0, timeout0 ; txerr5 reste le cumul des sondes,
sans augmentation en runtime. Valeurs candidates39,25–39,27 V,33 °C,71 %.

Contrôle CLICK ZERO, pédale0, want0/cmd0 ; dernière ligne ZERO PEDALS,
calibration locale apparemment commencée pendant la capture, origine du geste
non confirmée. Aucune calibration ou armement déclenché par l'assistant.
Le verrou V1_TX reste fermé même si l'utilisateur arme ce profil.
Affichage physique, calibration aboutie et réponse pédale×2 encore à confirmer
par l'utilisateur. Aucun essai de rotation ni validation de frein/perte CAN.


## 2026-10-25 — Zéro/armement automatiques, jauges et flash moteur demandé

Utilisateur confirme « ça boot, l'interface est là », SOC et vitesse visibles.
Il demande explicitement calibration/armement automatiques, anneau puissance,
barre couple et émission moteur. Banc roue levée confirmé dans le tour précédent.

Logiciel : zéro automatique dès mesures complètes/saines à l'arrêt,16 réponses
couple distinctes≤8 et dispersion≤2, puis armement au neutre. Fenêtre initiale
recommencée si signal chargé/instable/périmé. Un arrêt au bouton, y compris
pendant le zéro, annule l'automatisme ; défaut après armement reste verrouillé.
Aucune reprise automatique après arrêt/défaut. Un nouveau boot peut recommencer
le zéro puis armer. Un effort statique faible et stable ne peut pas être
distingué sûrement d'un offset : pédales déchargées requises au boot.
Gain brut×2, plafond8/64, rampe200 ms, baisse immédiate, temporisations,
NO MOTION, contrôles U/I et aucune régène conservés.

UI : anneau depuis moteur14 et pourcentage candidat100/64 ; petite barre
horizontale cyan depuis couple21, TQ brut0–64. Mesures masquées si périmées,
aucune substitution par vitesse ou commande. Watts batterie et CMD transport
restent séparés. Le14 a déjà renvoyé0 malgré rotation ; ne pas interpréter
l'anneau comme une mesure mécanique garantie. Rendu hôte inspecté visuellement.

Tests :9/9 unitaires et2/2 LVGL passent. Nouveaux scénarios : séquence boot
sans clic et sans demande pendant zéro/armement, effort après armement,
stop/fault sans reprise, bouton maintenu au boot, signal chargé et zéro instable.
Jauges vérifiées par vraies valeurs LVGL : vitesse changée sans modifier
l'anneau moteur, couple indépendant, masquage/péremption et géométrie.
Build live ESP32 sans warning, image_info checksum/hash valides.

Commit implémentation f9a3796 publié. Flash build-assist-v1-live sur
/dev/cu.usbserial-016138BC à115200, cible ESP32-D0WDQ6 rev1.0, ASSIST_V1=y,
V1_TX=y, ENCODER_JOG désactivé. Image644592 octets, SHA256 fichier
`bb1ec62cd8ee0f7d3e908f42d3faed9c99baa9503eaf975ecf64d0e1ab82e8bd`. Application/bootloader/partitions/ota_data écrits et empreintes
vérifiées, reset esptool exécuté. [Log flash](logs/2026-10-25-v1-auto-live-flash.txt).
Aucun port série rouvert après flash, aucune calibration/commande d'effort
émise par l'assistant. L'effort éventuel appartient à l'essai utilisateur.
Retour matériel du zéro/armement et de la réponse moteur encore attendu.
Ce flash ne valide ni frein, ni calibration physique du gain, ni arrêt sur
perte CAN/reset. La commande peut désormais devenir positive après auto-armement.
