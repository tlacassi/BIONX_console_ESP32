# Test des trois composants — HUZZAH32

Profil `sdkconfig.components`, combiné à `sdkconfig.observe` et
`sdkconfig.huzzah32`. Écran : SPI18/5, RST27, CS14, DC32.
MCP : SO19, SI18, SCK5, CS33, quartz déclaré 8 MHz, alimentation déclarée 3,3 V.
Encodeur A25/B26 ; bouton déplacé de GPIO39 vers **GPIO22** avec pull-up interne.
Pas de résistance ajoutée déclarée par l’utilisateur.

Le test combiné utilise INT34 et un timer de secours de 1 ms. Le profil
isolé peut fonctionner sans INT, par polling toutes les 250 µs. Configuration
listen-only, jamais TX.
Les trois rectangles affichés doivent être rouge, vert, bleu de gauche à droite.
Rotation : compteur signé par cycle quadrature complet ; inverser A/B inverse
le sens. `jumps` compte les sauts illégaux et peut révéler des lectures manquées.
Bouton actif à GND, anti-rebond 25 ms ; `Clicks` compte les appuis.

```sh
idf.py -B build-components -D IDF_TARGET=esp32 -D SDKCONFIG=build-components/sdkconfig -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.observe;sdkconfig.huzzah32;sdkconfig.components' build
idf.py -B build-components -p /dev/cu.usbserial-016138BC -b 115200 flash monitor
```

Ce profil correspond uniquement au montage déclaré pour ce test. Il active
l’initialisation MCP avec ces paramètres ; les profils génériques continuent
de laisser sa configuration matérielle non vérifiée.

SPI OK sans trames signifie que le contrôleur répond et accepte la configuration,
mais ne valide pas le transceiver ni le câblage CAN H/L. Aucun trafic CAN n’est
nécessaire pour réussir l’initialisation SPI. Un timeout d’initialisation doit
être diagnostiqué côté SPI/alimentation/reset/quartz, avant le bus CAN.

GPIO22 est maintenant occupé par le bouton : l’ancien plan I2C 23/22 n’est plus
libre tel quel. Les autres profils n’utilisent pas encore l’encodeur.

## Correspondance des noms imprimés

| Module / signal | Broche imprimée Feather | GPIO |
|---|---|---:|
| Écran SCL et MCP SCK | SCK | 5 |
| Écran SDA et MCP SI | MOSI | 18 |
| MCP SO | MISO | 19 |
| Encodeur A | A1 | 25 |
| Encodeur B | A0 | 26 |
| Bouton encodeur | SCL | 22 |

Sur le GC9A01, SDA/SCL sont les signaux **SPI**, pas I2C. Ne pas les relier
aux broches SDA/SCL de la Feather : cela correspondrait à GPIO23/22, alors
que ce firmware utilise GPIO18/5. Le bouton occupe déjà GPIO22.

## Isolation de l’écran

À la demande utilisateur, l’encodeur et le MCP sont laissés de côté. Le profil
`sdkconfig.screen-test` utilise uniquement l’écran à **1 MHz**, avec un aplat
rouge, vert, bleu puis blanc toutes les deux secondes. CS33 est maintenu haut
pour désélectionner le MCP connecté au même bus ; aucune initialisation MCP,
lecture encodeur ou émission CAN. Les sept fils écran restent nécessaires :
VCC/GND, SDA→MO, SCL→SCK, CS14, DC32 et RST27.

```sh
idf.py -B build-screen-test -D IDF_TARGET=esp32 -D SDKCONFIG=build-screen-test/sdkconfig -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.dashboard;sdkconfig.huzzah32;sdkconfig.screen-test' build
idf.py -B build-screen-test -p /dev/cu.usbserial-016138BC -b 115200 flash monitor
```

L’initialisation logicielle de l’écran ne garantit pas qu’il est alimenté ni
qu’il reçoit les commandes SPI. Si le rétroéclairage reste totalement éteint,
mesurer directement VCC/GND sur le module et identifier sa broche BL éventuelle.

## Test écran + encodeur, MCP débranché

Après correction de broches décalées, l’utilisateur confirme l’écran fonctionnel.
Le profil `sdkconfig.encoder-test` désactive toute initialisation MCP et conserve
l’écran à 1 MHz. A/CLK→A1(GPIO25), B/DT→A0(GPIO26), SW→SCL(GPIO22), commun→GND.
Les trois entrées utilisent leurs pull-ups internes ; bouton actif à GND.
L’écran affiche position, A/B, état du bouton, clics et transitions illégales.
Un pas de position correspond à un cycle quadrature complet (4 transitions),
pas nécessairement à un cran selon le modèle d’encodeur.

```sh
idf.py -B build-encoder-test -D IDF_TARGET=esp32 -D SDKCONFIG=build-encoder-test/sdkconfig -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.observe;sdkconfig.huzzah32;sdkconfig.encoder-test' build
idf.py -B build-encoder-test -p /dev/cu.usbserial-016138BC -b 115200 flash monitor
```

Le profil réutilise le support SPI du mode observation, mais l’option
`BIONX_TEST_MCP` est désactivée : aucun démarrage MCP, tâche RX ou commande CAN.
L’échantillonnage est effectué au-dessus de la priorité LVGL ; `jumps` peut
signaler des transitions manquées lors d’une rotation rapide.

## MCP reconnecté, CAN libre

Réactiver `BIONX_TEST_MCP=y` via `sdkconfig.components` conserve le polling
passif, quartz 8 MHz et écran à 20 MHz (profil optimisé). Le CAN peut rester débranché pour ce test
SPI : reset, relecture des registres de timing et mode listen-only sont vérifiés.
Les statistiques sont maintenant publiées dans une copie protégée par une
courte section critique : leur lecture ne prend plus le verrou du contrôleur
MCP. Le consommateur et l’écran ne peuvent donc pas ralentir RX pour les lire.


## Transport SPI optimisé

Le test combiné utilise maintenant un IO écran synchrone, sous le même verrou
récursif que le MCP. Il ne mélange plus commandes polling et pixels queued.
Un transfert écran est limité à une ligne ; le MCP garde le verrou pendant
la lecture complète d’une trame. Les échanges MCP de quatre octets ou moins
utilisent les buffers internes du pilote SPI.

En écoute seule, READ RX BUFFER lit et acquitte le message en une commande.
CANINTF/EFLG restent lus à chaque passage ; mode et TEC/REC sont contrôlés
toutes les 10 ms. Le chemin actif conserve ses contrôles complets par lecture.
L’option `BIONX_MCP_USE_INT` active INT34 et un timer de secours de 1 ms.
Sans cette option, un timer réveille la réception toutes les 250 µs.
Le journal `can_perf` mesure les fronts INT et la durée des lectures.

Le profil `sdkconfig.components` demande LCD 20 MHz et MCP SPI 2 MHz, sans
changer le quartz CAN de 8 MHz ni le débit de 125 kbit/s. Le profil générique
MCP reste à 1 MHz. L’écran montre aussi `ov`, le cumul des événements de
débordement matériel (pas un nombre exact de trames perdues).
Les compteurs s’actualisent toutes les 500 ms, les actions encodeur/bouton
sont prioritaires à l’affichage (tentative toutes les 100 ms).

Pour les résultats physiques et les limites, voir la dernière section de
[VALIDATION.md](VALIDATION.md). Le profil isolé `sdkconfig.mcp-serial-test`
reste disponible : le charger après `sdkconfig.components` pour désactiver
l’initialisation écran tout en conservant les mêmes fréquences.

Après une seconde sans réception, le statut passe à `RX IDLE` plutôt que
de laisser `RX OK` affiché sur la seule base d’un ancien compteur non nul.


## Diagnostic et séparation des bus — 25 octobre 2026

La tâche RX n’imprime plus ses performances ; `can_perf` est journalisé par
le consommateur. Les valeurs polls/durée moyenne/maximale/INT sont cumulées
depuis le démarrage. Les compteurs `bad` et `spierr` sont distincts des
débordements et pertes de file. En passif, la libération du buffer RX passe
avant les diagnostics dus ; leur résultat est vérifié avant livraison.

Le test isolé, aux mêmes réglages MCP que le test combiné, a reçu 9 240
trames en une minute sans débordement, contre trois débordements sur
9 111 trames avec écran. Le profil deux bus est donc préparé pour un essai
avec l’écran actif et le MCP indépendant : voir le brochage et la commande
de compilation dans [MASTER_BENCH.md](MASTER_BENCH.md). Son activation
exige les deux déplacements de fils décrits ; le profil partagé conserve
le câblage précédent. Le test ne transmet aucune commande CAN.


Le câblage deux bus est désormais confirmé par l’utilisateur. Après
vérification/reconnexion, l’initialisation MCP sur SPI3 réussit (ESP_OK),
les trames reviennent et l’utilisateur confirme que l’écran et l’encodeur
sont très réactifs. Firmware actif :
`build-components-dual-spi/bionx_c3_supermini.bin`.
