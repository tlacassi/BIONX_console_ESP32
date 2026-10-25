# BionX — ESP32-C3 Super Mini / Adafruit HUZZAH32

**Reprise agent : lire [HANDOFF.md](HANDOFF.md).**

Projet ESP-IDF natif préparé pour ESP32-C3 Super Mini, sans Arduino requis.
**Observation CAN, diagnostic, dashboard et pilotage MCP2515 sur banc roue levée.**

[Bilan des trouvailles pour le code final](BENCH_FINDINGS.md) : pilotage moteur
et lecture/affichage du capteur de pédalage confirmés par l’utilisateur.
Calibrations physiques et sécurité routière restent à valider séparément.

Nouveau : [maître et profil Adafruit HUZZAH32](MASTER_BENCH.md).
Dernier firmware chargé : [test encodeur roue levée](JOG_BENCH.md),
`build-encoder-jog` (sdkconfig-live), faible commande positive autorisée par
l'utilisateur pour ce banc. Clic neutre, expirations ; pas de validation roulante.
[Analyse des dépôts et contrôle de pédalage](ASSIST_BENCH.md) séparés.
Banc HUZZAH32 actuel : **deux bus SPI**, MCP SI→SDA23 et SCK→A5/GPIO4,
profil passif `sdkconfig.components-dual-spi` après `sdkconfig.components`.
Le profil maître neutre est `build-master-dual-spi` : bouton wake externe,
requête batterie toutes les secondes puis initialisation et runtime neutre
sur réponse attendue. Écran : tension, température moteur, SOC et diagnostics.
Maintien encodeur 2 s pour arrêter ; appui court pour réarmer après arrêt.
Voir [MASTER_BENCH.md](MASTER_BENCH.md) pour les defaults de compilation.
Les anciens profils partagés correspondent au câblage précédent.
Séquence démarrage/arrêt, TX borné, télémétrie expirante et tests de contrôle
sont implémentés. Le profil neutre garde une assistance nulle ; le profil jog
actif permet la rotation au banc. Le wake est un bouton externe, sans GPIO.
Compilation ESP32-C3 réussie le 23 octobre 2026 avec l’ESP-IDF local
**v5.2-dirty**. Flash USB effectué sur ESP32-C3 révision 0.4, flash 4 Mo.
Voir le [rapport de validation](VALIDATION.md) pour les outils, résultats et limites.
Les exigences fonctionnelles sont dans [project_requirements.md](project_requirements.md).

Analyse de la capture d’origine : [0000235.TXT](LOG_ANALYSIS_0000235.md).
Rejeu hôte : 97,92 % des trames décodées ; observation compatible au niveau
format ; ce rejeu seul ne valide pas le remplacement de console.

## Décodage issu des captures et de BionXtool

Le protocole reconnaît aussi les cibles de diagnostic 0x48/0x50/0x60 et les
réponses 0x58 sur 4 octets. Les trames 0x58 sur 2 octets restent `UNKNOWN`.
Une réponse 0x58 ne satisfait pas une lecture normale attendant 0x08.
Les journaux ajoutent des noms de registres et des conversions marquées
`candidate` (rpm, %, °C) lorsque la cible est connue. Les réponses passives
restent d’origine inconnue ; aucune attribution automatique par registre.
Voir les sources et limites dans [BIONXTOOL_ANALYSIS.md](BIONXTOOL_ANALYSIS.md).

Le diagnostic TWAI optionnel interroge désormais vitesse, indicateur de puissance,
température, capteur de couple et jauges batterie distinctes, ainsi que les deux
registres de tension pack. Ces derniers restent bruts, sans assemblage de mesures
prises à des instants différents. Le compteur moteur 0x12, remis à zéro à la
lecture selon la source, est volontairement absent du polling. Avec 12 lectures
espacées de 200 ms, chaque registre revient au mieux toutes les 2,4 s : ceci
reste un diagnostic, pas une boucle de contrôle.

L’assistance signée est interprétée sur un octet, avec refus des valeurs hors
plage ; aucune commande d’assistance n’est ajoutée. Les unités du projet tiers
restent à confirmer sur le vélo. Le journal conserve sa limite de 100 lignes/s
pour borner la sortie série ; les omissions sont comptées, et ce profil ne
promet pas une capture intégrale des ~189 trames/s observées.

Test matériel HUZZAH32 : [écran, MCP et encodeur](COMPONENT_TEST.md).

## Démarrer
Le gestionnaire de composants a été mis à jour en **2.4.11** dans l’environnement
Python du SDK : les versions 1.5.1 et 1.5.3 échouaient à lire les règles des
versions LVGL récentes examinées par le résolveur. Les composants retenus restent
verrouillés dans `main/idf_component.yml` et `dependencies.lock`.
Avec le SDK local 5.2, charger l’environnement ainsi (ses anciennes contraintes
Python imposaient le gestionnaire 1.x) :

```sh
export IDF_PYTHON_CHECK_CONSTRAINTS=0
. /Users/theolacassin/esp/v5.2/esp-idf/export.sh
```

Cela désactive la vérification des contraintes Python pour ce terminal ; aucun
fichier du SDK n’a été modifié. Sur une nouvelle installation, installer
`idf-component-manager==2.4.11` dans l’environnement Python ESP-IDF.

ESP-IDF **5.4.x** reste la version de départ annoncée, mais n’a pas été testé ici.
Le SDK disponible et utilisé est **v5.2-dirty**, également sélectionné dans VS Code.
Son état modifié ne constitue pas une dépendance verrouillée ; une compilation
sur une distribution propre sera nécessaire pour établir la référence reproductible.
Utiliser l’extension officielle Espressif dans VS Code.
Ouvrir ce dossier comme racine du workspace et utiliser un terminal ESP-IDF :

```sh
idf.py set-target esp32c3
idf.py menuconfig
idf.py build
idf.py -p PORT flash monitor
```

Les tâches VS Code nécessitent un environnement ESP-IDF chargé ; les commandes
Build / Flash de l’extension peuvent aussi être utilisées directement.
Dans ce dossier, seul le réglage du SDK VS Code est présent ; aucune tâche
personnalisée n’est fournie. Pour charger le SDK local depuis un terminal :

```sh
. /Users/theolacassin/esp/v5.2/esp-idf/export.sh
idf.py build
idf.py size
```

Si Ninja relance CMake en boucle, vérifier les dates des fichiers : l’import
initial contenait des horodatages futurs, corrigés avant le build réussi.
Premier flash si nécessaire : maintenir BOOT, appuyer puis relâcher RESET,
puis relâcher BOOT. Console par USB-C natif (pas par UART du BMS).
Quitter le moniteur avec Ctrl+]. Vérifier que la carte possède bien 4 Mo de flash.

## Anciens brochages C3 et profils associés

Le [suivi matériel et pinout MCP2515 proposé](HARDWARE.md) tient compte des
photos moteur/batterie et de l’écran branché. Le CAN est confirmé par
l’utilisateur ; les inconnues matérielles et le wake externe sont détaillés
dans HARDWARE.md. Ce paragraphe conserve l’ancien plan C3.
Ce nouveau pinout est implémenté dans le profil observation, mais n’a pas été
testé sur module CAN ; le tableau CAN initial ci-dessous n’est plus applicable
tel quel. Avancement détaillé : [TODO.md](TODO.md).
### Montage C3 historique (ne pas appliquer au banc HUZZAH32 actuel)

Écran **GC9A01 240×240 SPI** : VCC → 3V3, GND → GND,
RST → GPIO0, CS → GPIO1, DC → GPIO10, MOSI → GPIO3, SCLK → GPIO4.
Le pilote écran est utilisé par le profil dashboard décrit ci-dessous.
Ce montage entre en conflit avec le plan initial ci-dessous : GPIO4 était CAN
TX et GPIO10 le bouton encodeur. Le plan complet devra être revu avant CAN/UI.
Avec cet écran branché, utiliser le profil de test USB isolé :

```sh
idf.py -B build-usb -D SDKCONFIG=build-usb/sdkconfig -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.usb-test' build
idf.py -B build-usb -p /dev/cu.usbmodem213301 -b 115200 flash
```

Ce profil ignore le plan initial et ne démarre aucun driver CAN/écran/BMS.
Contournement du démarrage défectueux en USB seule du SDK local : UART0
principal (GPIO21 TX), USB Serial/JTAG secondaire. GPIO21 n’est donc pas
disponible pour le BMS dans ce profil de test. Le profil normal reste distinct
et ne doit pas être flashé avec ce câblage d’écran.

### Plan initial à réviser

Tous les GPIO sont configurables dans `BionX console` via menuconfig.
Les affectations réservées sont vérifiées au démarrage pour éviter les doublons.

| Fonction | GPIO | État |
|---|---:|---|
| CAN TX vers TXD transceiver | 4 | Initialisé |
| CAN RX depuis RXD transceiver | 5 | Initialisé |
| Écran I2C SDA | 0 | Réservé, driver à ajouter |
| Écran I2C SCL | 1 | Réservé, driver à ajouter |
| ESP UART TX vers BMS RX | 21 | Réservé, driver à ajouter |
| ESP UART RX depuis BMS TX | 20 | Réservé, driver à ajouter |
| Encodeur A / B | 6 / 7 | Réservé, driver à ajouter |
| Bouton encodeur | 10 | Réservé, driver à ajouter |

GPIO2/8/9 sont exclus de ce plan car liés au démarrage ; GPIO8 est souvent
la LED et GPIO9 le bouton BOOT. GPIO18/19 restent dédiés à l’USB.
Les variantes Super Mini doivent être confrontées à leur schéma réel.
Ce plan réserve un écran I2C ; un écran SPI demandera une nouvelle allocation.

## Matériel externe
- Transceiver CAN avec logique compatible 3,3 V, alimentation selon sa fiche.
  L’ESP32-C3 possède le contrôleur TWAI mais pas le transceiver CAN H/L.
- Masse de référence et terminaisons adaptées au bus existant ; ne pas ajouter
  systématiquement une résistance si le bus est déjà terminé.
- Alimentation régulée adaptée au pack ; jamais la tension pack sur la carte.
- UART du BMS : vérifier niveaux, référence de masse, protocole et isolation
  avant connexion. Ne pas confondre UART TTL et RS485.

## Comportement des profils de diagnostic historiques
CAN classique 125 kbit/s. Par défaut : écoute seule, aucune trame envoyée.
Le mode actif optionnel lit des registres moteur/batterie, sans écrire de couple,
de paramètres protégés ni de commande de réveil. Il attend un seul retour à
la fois. Les réponses partagent potentiellement un identifiant et ne portent
pas de numéro de transaction : l’appariement reste candidat, en particulier
avec des réponses tardives. Ne pas utiliser avec une autre console maître.
En cas de défaut CAN ou échec TX, les requêtes restent désactivées jusqu’au
redémarrage manuel. Le journal synchrone est destiné au diagnostic à faible
charge ; ce n’est pas un enregistreur garanti sans perte.
Le watchdog surveille le fonctionnement général/idle, pas encore la fraîcheur
fonctionnelle de futures commandes moteur.

## Évolutions prévues, non implémentées
- Dashboard de conduite complet avec télémétrie réelle (le profil maître dispose
  déjà d’un écran de diagnostic et de mesures expirables).
- Encodeur, état partagé et échanges par queues entre contrôle/UI/réseau.
- BMS DALY ou autre : choix dépendant du modèle et protocole exacts.
- Wi-Fi et configuration, puis OTA. Le partitionnement réserve deux slots
  de 1,5 Mio et 896 Kio de stockage ; aucun service OTA ni système de fichiers
  n’est lancé. Ajuster les tailles si le futur firmware dépasse 1,5 Mio.
- Odomètre avec persistance NVS, SOC, puissance et commande moteur validés.

Le C3 est monocœur et sans PSRAM sur cette carte typique : choisir des buffers
et animations modestes, puis mesurer RAM et temps CPU avec Wi-Fi actif.
Arduino pourra être ajouté comme composant si nécessaire ; verrouiller alors
un couple de versions Arduino/ESP-IDF officiellement compatible.

## Dashboard animé — mode DEMO

Le profil est **désactivé par défaut**. Il démarre l’écran déclaré ci-dessus,
le port LVGL et des données simulées ; CAN, BMS et encodeur ne sont pas démarrés.
L’ancienne validation des broches CAN est remplacée dans ce mode par celle des
cinq broches écran. Aucun changement du câblage communiqué n’est requis.

Après chargement du terminal ESP-IDF comme indiqué plus haut :

```sh
idf.py -B build-dashboard -D SDKCONFIG=build-dashboard/sdkconfig -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.dashboard' build
idf.py -B build-dashboard size
idf.py -B build-dashboard -p /dev/cu.usbmodem213301 -b 115200 flash monitor
```

Fermer tout autre moniteur avant le flash. Le port peut changer après
rebranchement. Pour configurer : `idf.py -B build-dashboard menuconfig`.
`BionX console` permet de régler la fréquence SPI, l’inversion et les axes.
Aucune orientation antérieure n’était implémentée. Après observation de la
photo du module, le profil active `CONFIG_BIONX_LCD_MIRROR_Y` pour corriger le
miroir vertical ; X et l’échange X/Y restent désactivés. SPI 20 MHz et cette
correction restent à vérifier sur le rendu physique après flash. Aucun GPIO de
rétroéclairage n’a été déclaré ; le logiciel n’en commande pas.

Pour désactiver la démo, revenir au profil `build-usb` sans périphériques
(compatible avec l’écran branché). Le profil normal `build/` conserve le plan
CAN initial : ne pas le flasher avec le câblage écran actuel. Son démarrage USB
seul reste affecté par le défaut du SDK local documenté dans `VALIDATION.md`.

Le profil dashboard reprend le contournement UART0 principal / USB secondaire
testé sur la carte. GPIO21 est donc utilisé comme console TX et n’est pas libre
pour un BMS dans ce profil.

L’interface indique une vitesse de 0 à 35 km/h (arc gradué jusqu’à 45),
une batterie démarrant à 78 % et perdant 1 % toutes les deux minutes,
une puissance simulée de 0 à 500 W et les modes ECO/TOUR/SPORT toutes les 8 s.
Le repère **DEMO** reste affiché. Ce ne sont pas des mesures du vélo.

- `components/display` : `esp_lcd`, driver GC9A01 2.0.4, SPI DMA RGB565 et port
  LVGL 2.6.2 ; LVGL est fixé à 9.2.2. Deux buffers partiels de 240×20 pixels
  utilisent **19 200 octets** au total, sans framebuffer plein écran.
- `components/dashboard` : widgets créés une fois, textes sur buffers fixes,
  interpolation toutes les 33 ms et fondu de mode ; pas d’allocation applicative
  répétée dans le rafraîchissement. LVGL gère ses allocations internes dans un
  pool de 48 Kio. La cadence visée est 20–30 FPS, pas une cadence garantie.
- `dashboard_demo_sample()` : générateur pur à temps monotone, appelé à 10 Hz
  depuis la tâche principale. Pour des données réelles, remplacer cet appel
  par un snapshot complet puis appeler `dashboard_update()`, qui prend le
  verrou LVGL avec un délai borné. Ne pas appeler depuis une ISR. Un producteur
  critique devra passer par une queue bornée vers une tâche UI.

L’interface actuelle exige des valeurs valides ; elle refuse NaN, vitesse hors
0–45, SOC hors 0–100 et modes inconnus. Avant d’afficher de vraies mesures,
ajouter leur validité/expiration et remplacer le repère DEMO ; ce POC n’est pas
une implémentation de la télémétrie métier ni du contrôle.

Le build dashboard réserve environ **118,6 ko de D/IRAM statique** (36,9 %).
Ce chiffre inclut le pool LVGL mais pas les buffers DMA et piles alloués au
démarrage. Les logs donnent la heap après initialisation et toutes les 10 s ;
les résultats matériels et limites sont consignés dans `VALIDATION.md`.

Tests hôte, après téléchargement des composants par le build ESP-IDF :

```sh
cmake -S tests/host -B build-host
cmake --build build-host
ctest --test-dir build-host --output-on-failure
```

Le test utilise le vrai moteur de rendu LVGL et un écran mémoire : cycle de
24 s, bornes, entrées invalides, timeout du verrou et textes dans le cercle.
Il produit `build-host/dashboard.rgb565` (240×240, RGB565 little endian).
Cela ne remplace ni les essais SPI ni la confirmation visuelle sur l’écran.

Références d’intégration : [LVGL avec ESP-IDF](https://lvgl.io/docs/open/integration/chip_vendors/espressif/add_lvgl_to_esp32_idf_project),
[port Espressif](https://github.com/espressif/esp-bsp/tree/master/components/esp_lvgl_port).

## Observation CAN MCP2515 — premier incrément

Ce mode écoute le vélo avec sa console d’origine encore présente. Il ne
transmet aucune trame (pas d’ACK en listen-only), ne réveille pas la batterie et
n’active aucune puissance. La future console de remplacement sera un maître
CAN bidirectionnel : étapes suivantes dans [TODO.md](TODO.md).

Avec le terminal ESP-IDF chargé comme indiqué au début du README :

```sh
idf.py -B build-observe -D SDKCONFIG=build-observe/sdkconfig -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.observe' build
idf.py -B build-observe menuconfig
idf.py -B build-observe build size
```

Dans `BionX console`, choisir le quartz **réel** du MCP2515 (8, 16 ou
20 MHz) et activer la confirmation matérielle **après vérification** de son
alimentation, niveaux logiques, câblage et terminaisons. Les valeurs livrées
sont « matériel non vérifié » et quartz inconnu (`0`) : le code refuse alors
de démarrer le MCP et l’écran indique une erreur de configuration.

Le [pinout](HARDWARE.md) conserve l’écran et partage MOSI=3/SCLK=4 ; MCP
MISO=5, CS=6, INT=7. La fréquence CAN configurée est 125 kbit/s, celle du SPI
MCP 1 MHz. La présence de deux buffers RX matériels impose un essai de charge.
En observation, l’écran utilise deux buffers DMA de deux lignes, soit
**1 920 octets** au total ; son affichage de diagnostic est mis à jour à 1 Hz.

Après identification et câblage du matériel uniquement :

```sh
idf.py -B build-observe -p PORT -b 115200 flash monitor
```

Fermer tout autre moniteur série. Ce profil n’a pas été flashé pendant son
développement : le module MCP2515 exact n’est pas encore identifié.

La tâche RX prioritaire vide le contrôleur avec un budget de huit trames par
passage, puis cède le CPU. Une file statique de 64 trames sépare RX et journal.
Une panne SPI ou un changement de mode inattendu arrête RX jusqu’au redémarrage
manuel ; aucune récupération automatique ne réactive un mode maître.

Les lignes `CAN` donnent temps monotone en microsecondes, identifiant,
standard/étendu, DATA/RTR, DLC et octets. Les annotations QUERY/SET/REPLY
décrivent les trames **observées**, pas des écritures effectuées par l’ESP.
Une réponse `0x08` reste d’origine inconnue. Aucun registre n’est converti en
km/h, %, W ou A à ce stade ; l’écran indique télémétrie `--`.

Les compteurs distinguent :

- `rx` : trames lues et structurellement valides ; `malformed` : DLC invalide.
- `queue_drop` : trames perdues à l’entrée de la file logicielle pleine.
- `ov0/ov1` : événements de débordement matériel observés et acquittés ; ce sont
  des bornes inférieures des pertes, pas un nombre exact de trames perdues.
- `log_omitted` : trames délibérément omises du journal série selon
  `BIONX_CAPTURE_LOG_RATE` (100 lignes/s par défaut ; 0 coupe le journal brut).
- `spi_err`, `mode_fault`, dernier EFLG/TEC/REC et âge de la dernière réception.

Le temps enregistré est celui de la lecture SPI, pas de l’arrivée sur le fil.
Deux buffers peuvent être lus dans un ordre différent de l’ordre du bus.
Le listen-only du MCP2515 peut recevoir des trames endommagées et désactive
ses compteurs d’erreurs CAN : **TEC/REC à zéro ne prouvent pas un bus sain**,
et le parseur structurel ne garantit pas l’intégrité CRC. Le journal synchrone
n’est pas un enregistreur sans perte ; un consommateur série bloqué peut arrêter
son propre affichage mais la tâche RX continue et compte les pertes de file.

Tests protocole et pilote, sans ESP-IDF ni matériel :

```sh
cmake -S tests/unit -B build-unit
cmake --build build-unit
ctest --test-dir build-unit --output-on-failure
```

Le modèle SPI teste configuration, débit, mode silencieux, buffers RX,
identifiants/RTR/DLC, débordements, erreurs injectées et absence d’instructions
TX. Il ne simule ni la couche électrique, ni les arbitrages SPI/FreeRTOS réels.
Les résultats et la procédure banc/vélo figurent dans [VALIDATION.md](VALIDATION.md).

## Origine et licence
GPL-3.0-only. Sources du protocole et attributions :
- stancecoke : https://github.com/stancecoke/BionX_Minimal_Translator
- Ali Saghiran (2018) : https://github.com/inarighas/BionxConsole
- Documentation carte : https://github.com/apache/nuttx/blob/master/Documentation/platforms/risc-v/esp32c3/boards/esp32c3-supermini/index.rst
- USB : https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32c3/api-guides/usb-serial-jtag-console.html
