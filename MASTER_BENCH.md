# Maître CAN neutre et profil HUZZAH32 — 24 octobre 2026

## État courant — 25 octobre 2026

L’utilisateur confirme le **pilotage moteur fonctionnel sur roue levée**.
Le bilan à réutiliser pour le code final est [BENCH_FINDINGS.md](BENCH_FINDINGS.md).
Il distingue observations physiques, corrections testées et calibrations restantes.

Profil chargé : `build-encoder-jog/sdkconfig-live`, HUZZAH32. Dix crans2…20 km/h
estimés, zéro arrêt, survitesse25 estimés, pneu700×28C/config2136 mm provisoires.
Commande max8 bruts au démarrage/4 en mouvement, rampe200 ms et zéro à cible.
Clic arrêt/acquittement sous conditions ;20 s maximum/session, expiration100 ms.
Lecture/affichage pédalage confirmés par l’utilisateur ; U/I reste en diagnostic.
La force en N/Nm n’est pas calibrée. Reprise : [HANDOFF.md](HANDOFF.md).
Voir [JOG_BENCH.md](JOG_BENCH.md), [HARDWARE.md](HARDWARE.md) et les preuves/images
successives dans [VALIDATION.md](VALIDATION.md). Pas de validation routière ni
watchdog moteur sur perte CAN prouvé.

Les sections suivantes décrivent le **profil maître neutre** et l’historique ;
les anciens plafonds/délais ne remplacent pas les paramètres jog ci-dessus.

## Comportement de réveil externe et détection automatique

Depuis le 25 octobre, le profil maître attend une seconde sans trafic puis
entre en `WAIT_WAKE` : une lecture batterie HW (ID 0x10, `00 3B`) par seconde.
L'utilisateur réveille le vélo avec son bouton externe. Une réponse 0x08 au
registre 0x3B dans la fenêtre de 200 ms, avec HW attendu 0x33, déclenche
la séquence d'initialisation puis le runtime neutre. Aucun SET avant cette
réponse. Une trame quelconque ou une réponse tardive ne suffit pas.
Les réponses restent candidates, sans identifiant de transaction.

Les sondes one-shot sans succès TX sont tolérées uniquement en découverte ;
elles ne prouvent pas à elles seules que la batterie dort. Aucun reset MCP
ni retransmission matérielle automatique. TX error-passive seul est toléré
pendant l'attente ; RX error-passive, bus-off, SPI/mode et pertes restent
bloquants. Les compteurs TX d'échec restent visibles dans le journal.
Les séquences init/runtime/arrêt gardent leur comportement strict sur erreur.
La télémétrie marque une pause de 1 s après ses huit lectures ; les périodes
neutres (50 ms) et maintien batterie (250 ms) restent indépendantes.

Maintien 2 s encodeur ou `q` = arrêt et désactivation de la découverte.
Pas de redémarrage automatique après cet arrêt ou un défaut. Appui court
encodeur ou `s` = réarmement de l'attente après silence, si transport sain.
Arrêter dans WAIT_WAKE n'émet aucune séquence d'arrêt (aucun SET préalable).
GPIO21 n'est pas utilisé ; bouton wake externe indépendant.
**Assistance exclusivement nulle : aucun essai de rotation.**

L'écran affiche tension batterie, température moteur, SOC, état/défaut,
TX/RX, âge dernière réception, nombre de mesures fraîches et pertes/TEC/REC.
La dernière paire tension complète reste affichée pendant la lecture suivante,
jusqu'à expiration. Les conversions candidates portent un `?`.
Premier runtime confirmé sur environ 20 s après correction RX/TX :
NEUTRAL/OK, 8/8 mesures fraîches, ~39,6 V, 35 C, SOC75%, pertes=0,
TXerr=timeout=0. Voir VALIDATION.md pour les images et journaux exacts.

Dernière image flashée : télémétrie bornée, cache tension, runtime vérifié
sur 45 s : NEUTRAL/OK, TX=2115 RX=331, 8/8 mesures fraîches,
pertes=0, TEC/REC=0, TXerr=timeout=0 ; ~39,57 V / 36 C / SOC75%.
Cet intervalle ne valide pas encore le réveil depuis sommeil établi ni
l'extinction physique. La cause du NO REPLY d'un essai précédent reste inconnue.

Les sections historiques ci-dessous décrivent la version précédente lorsqu'elles
mentionnent « aucune émission avant s » ou « pas de démarrage automatique ».

## Périmètre livré

Le profil `sdkconfig.master` active un maître MCP2515 **neutre pour le banc**.
Le profil observation reste écoute seule. Le profil maître nécessite toujours
l’identification du quartz, niveaux logiques, alimentation et terminaison du
module ; aucune de ces propriétés n’est devinée depuis une photo.

Démarrage à l’état OFF. `s` puis Entrée sur la console USB/série lance la séquence seulement
après une seconde sans trame reçue. `q` puis Entrée demande l’arrêt. Aucun armement de couple
n’est exposé : les écritures 0x09 restent nulles. Le MCP en mode normal peut
acquitter le trafic avant `s`, mais n’émet aucune trame applicative avant commande.
La console d’origine doit être retirée avant tout démarrage maître.

Le réveil électrique reste externe. Le système doit déjà pouvoir recevoir CAN.
Le chargeur LiPo de la Feather ne remplace ni le BMS ni le convertisseur du pack.

## Séquence

- Batterie : 0x20=0, 0x21=1, lectures versions et type, accessoire 0x22=0.
- Moteur : 0x02=0, 0x41=0, 0x42=1, lectures version/capteur/vitesse.
- Profil attendu : batterie HW 0x33, SW 0x69, type 3 ; moteur SW 0x60,
  type capteur 0. Ce sont les valeurs brutes de `start&stop.csv`.
- Lecture des quatre octets F9–FC. Refus de limites nulles ou supérieures aux
  plafonds conservés dans `bx_config_t` (9 000 / 45 000 bruts, aucune unité
  physique inventée). Aucun déverrouillage moteur si ces contrôles échouent.
- A5=AA sur moteur puis copie dynamique FB/FC vers 7A/7B et F9/FA vers 7C/7D.
  Le succès TX confirme la transmission CAN, pas l’application du registre.
- Runtime : doubles écritures 0x09=0, maintien batterie 0x21=1, lectures
  sérialisées couple/vitesse/power-meter/température/SOC estimé/jauge/voltage HI/LO.
- Arrêt demandé : deux écritures neutres, 0x41=0, 0x42=1, batterie 0x25=1.
  L’état OFF signifie séquence transmise, pas coupure électrique mesurée.

Une réponse est toujours candidate. Les lectures partagent 0x08 ; aucun numéro
de transaction. Les trames reçues visant un nœud moteur/batterie/console
indiquent un autre maître et provoquent un défaut. 0x58/009C seul reste UNKNOWN.

## Hypothèses de temps

| Paramètre | Défaut |
|---|---:|
| Confirmation TX MCP2515 | 20 ms |
| Attente de réponse candidate | 200 ms |
| Espacement écritures démarrage/arrêt | 20 ms |
| Espacement minimum runtime | 1 ms |
| Période demandée des paires neutres | 50 ms |
| Maintien demandé batterie | 250 ms |
| Expiration mesures | 2 s |
| Écart maximal des deux octets tension | 2 × timeout réponse |

Les périodes sont des objectifs soumis à la durée TX et à l’ordonnancement.
Le CSV ne les mesure pas. Les quatre principaux délais sont configurables via
menuconfig ; `bx_config_t` conserve les autres et valide leurs plages. Une
requête au plus est en attente. Pas de rafale pour rattraper des périodes ratées.
Un timeout de lecture entraîne un défaut, sans réessai automatique.

MCP2515 : un seul buffer TX, mode one-shot (pas de répétition autonome après
arbitrage perdu/erreur), contrôle TXREQ/TX0IF et erreurs. Bus-off, erreur SPI,
mode inattendu, pertes de file ou débordement RX inhibent le maître. Pas de
récupération automatique ; défaut transport → redémarrage manuel requis.
Un défaut purement protocolaire peut être réarmé par `s` après silence, mais
ne restaure ni commande antérieure ni armement du contrôleur.

## Télémétrie et contrôle

Snapshot propriétaire de la tâche maître. Chaque mesure a un timestamp de
réception et une validité ; absence/péremption affichée `--`. L’écran distingue
SOC estimé et jauge interne dans le modèle ; il affiche le SOC estimé. Valeurs
physiques marquées `?`, car les échelles restent à confirmer sur ce vélo.
La tension requiert deux octets frais, ordonnés et assez proches. Elle reste
candidate, sans atomicité garantie de la mesure côté BMS.

`bionx_control.c` implémente séparément frein prioritaire, armement au neutre,
expiration, limites électriques/thermiques/vitesse, rampes et sens signé.
Les entrées exigent un profil validé, un frein valide et des mesures fraîches.
La demande positive est bornée par une demande conducteur préalablement calibrée.
La régène exige autorisation de charge et tension disponible. Les défauts
nécessitent un réarmement explicite. Ce contrôleur est testé sur hôte mais
**n’est pas relié à une émission non nulle dans l’application maître** :
ni frein électrique, ni source de courant calibrée, ni loi couple/assistance
ni délai d’arrêt moteur sur perte CAN n’ont été validés matériellement.

## Builds séparés

Charger l’environnement ESP-IDF local selon README avant ces commandes.
Ne pas réutiliser un dossier de build C3 pour ESP32. Le nom de binaire
historique `bionx_c3_supermini.bin` est conservé ; le profil HUZZAH32 produit
bien une image pour ESP32, malgré ce nom.

```sh
# C3 : maître neutre, écran au câblage actuel C3
idf.py -B build-master -D SDKCONFIG=build-master/sdkconfig -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.master' build

# HUZZAH32 : maître neutre avec nouveau brochage proposé
idf.py -B build-huzzah32 -D IDF_TARGET=esp32 -D SDKCONFIG=build-huzzah32/sdkconfig -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.master;sdkconfig.huzzah32' build

# HUZZAH32 : observation passive pour commencer
idf.py -B build-huzzah-observe -D IDF_TARGET=esp32 -D SDKCONFIG=build-huzzah-observe/sdkconfig -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.observe;sdkconfig.huzzah32' build
```

Avant utilisation matérielle : menuconfig dans le dossier choisi, renseigner le
quartz mesuré et la vérification du module. Les profils fournis les laissent
à 0 / désactivé et n’initialisent donc pas le MCP. Aucun flash automatique.

## Brochage HUZZAH32 proposé

| Fonction | GPIO |
|---|---:|
| SPI MOSI / SCLK / MISO | 18 / 5 / 19 |
| Écran RST / CS / DC | 27 / 14 / 32 |
| MCP2515 CS / INT | 33 / 34 |
| Frein futur | 36 |
| Encodeur A / B / bouton (test actuel) | 25 / 26 / 22 |
| Wake futur, via interface identifiée | 21 |
| UART BMS futur TX / RX | 17 / 16 |
| I2C | À réattribuer : GPIO22 utilisé par le bouton |

GPIO34/36/39 sont des entrées seules, sans pull-up interne : résistances
externes adaptées nécessaires. GPIO35 est relié à la mesure batterie Feather,
GPIO1/3 restent USB-série. GPIO5 est une broche de strapping utilisée pour SPI
sur cette carte : ne pas imposer un niveau de démarrage incompatible via le
module externe. Le validateur refuse GPIO0/2/12/15, flash 6–11 et GPIO35.
Le pinout de la photo n’est pas déduit fil par fil ; le câblage doit être adapté
explicitement au profil et vérifié avant flash.

Sources : [Adafruit HUZZAH32](https://www.adafruit.com/product/3405),
[brochage officiel](https://learn.adafruit.com/adafruit-huzzah32-esp32-feather/pinouts),
[datasheet MCP2515 DS20001801K](https://ww1.microchip.com/downloads/aemDocuments/documents/APID/ProductDocuments/DataSheets/MCP2515-Family-Data-Sheet-DS20001801K.pdf).

Test matériel des trois composants : [COMPONENT_TEST.md](COMPONENT_TEST.md).
Le bouton initialement proposé sur GPIO39 a été déplacé sur GPIO22 afin
d’utiliser un pull-up interne ; INT34 reste non utilisé par le test en polling.


## Banc composants : transport partagé optimisé

Le profil `sdkconfig.components` est un test **passif**, distinct du maître :
LCD 20 MHz, MCP SPI 2 MHz, quartz CAN 8 MHz, CAN 125 kbit/s, INT34 avec
secours périodique de 1 ms. Aucun recâblage du bus SPI ni émission CAN.
Sans INT (`BIONX_MCP_USE_INT=n`), le secours passe à 250 µs.

Les deux clients utilisent des transferts SPI synchrones sous verrou commun ;
la lecture d’une trame est indivisible vis-à-vis du LCD, limité à une ligne.
Les statistiques sont publiées sans attente du verrou RX ; aucune affinité
CPU n’est imposée. Le profil maître conserve ses contrôles actifs complets.
Voir [COMPONENT_TEST.md](COMPONENT_TEST.md) et les derniers résultats dans
[VALIDATION.md](VALIDATION.md) avant de confondre test passif et maître validé.


## Deux bus SPI indépendants — 25 octobre 2026

Profil `sdkconfig.components-dual-spi`, chargé après `sdkconfig.components`.
L’écran reste sur SPI2 ; le MCP utilise SPI3 avec un verrou indépendant.
Le profil partagé reste disponible, mais il ne correspond plus au câblage
ci-dessous après déplacement des deux fils.

| Fil du MCP | Ancienne broche Feather | Nouvelle broche Feather | GPIO |
|---|---|---|---:|
| SI / MOSI | MO / MOSI | SDA | 23 |
| SCK | SCK | A5 | 4 |

Effectuer ces déplacements hors tension. Garder l’écran sur MO/MOSI18 et
SCK5 ; MCP SO reste sur MISO19, CS33 et INT34. Encodeur inchangé. Les labels
A5/GPIO4 et SDA/GPIO23 sont confirmés par le
[brochage Adafruit](https://learn.adafruit.com/adafruit-huzzah32-esp32-feather/pinouts).
GPIO21 reste réservé au futur wake ; SDA23 est désormais occupé par le MCP.

```sh
idf.py -B build-components-dual-spi -D IDF_TARGET=esp32 -D SDKCONFIG=build-components-dual-spi/sdkconfig -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.observe;sdkconfig.huzzah32;sdkconfig.components;sdkconfig.components-dual-spi' build
idf.py -B build-components-dual-spi -p /dev/cu.usbserial-016138BC -b 115200 flash
```

Le firmware journalise les deux plans SPI au démarrage. Il valide les GPIO
réservés, doublons et collisions avec l’encodeur avant toute initialisation.
Les transferts LCD peuvent être regroupés par dix lignes : ils ne prennent
plus le verrou du MCP. Les fréquences restent LCD 20 MHz/MCP SPI 2 MHz,
CAN listen-only à 125 kbit/s avec quartz déclaré 8 MHz.


**Référence du banc validée le 25 octobre :**
`build-components-dual-spi/bionx_c3_supermini.bin` flashé, écran et encodeur
confirmés très réactifs par l’utilisateur. Capture CAN avec écran actif :
13 878 trames en environ 90 s, zéro perte de file, zéro débordement matériel,
zéro trame malformée et zéro erreur SPI signalée. Voir VALIDATION.md pour
le hash de l’image et les limites de cet essai.

L’overlay `sdkconfig.components-dual-spi` définit le routage SPI ; il ne
sélectionne pas le rôle CAN. Pour une future compilation du maître avec ce
câblage, l’ajouter après ses defaults HUZZAH32. Les essais ci-dessus restent
strictement passifs et ne valident pas encore le maître sur le vélo.

## Commande encodeur — 25 octobre 2026

Profil maître pour le câblage deux bus actuel :

```sh
export IDF_PYTHON_CHECK_CONSTRAINTS=0
. /Users/theolacassin/esp/v5.2/esp-idf/export.sh
idf.py -B build-master-dual-spi -D IDF_TARGET=esp32 -D SDKCONFIG=build-master-dual-spi/sdkconfig -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.master;sdkconfig.huzzah32;sdkconfig.components-dual-spi;sdkconfig.master-dual-spi' build
```

Bouton existant SW sur GPIO22/SCL, commun GND, pull-up interne :
appui court au relâchement démarre depuis OFF/FAULT ; maintien de 2 s
arrête dès le seuil, y compris pendant STARTING. Anti-rebond 25 ms,
aucun démarrage au relâchement d'un appui long. Au boot, un bouton déjà
maintenu est ignoré jusqu'à un relâchement stable. Appui court en marche
sans effet. Les commandes série s/q restent disponibles. Le démarrage
conserve la condition d'une seconde sans trafic et l'inhibition sur panne
transport. Aucun démarrage différé si la condition n'est pas satisfaite.

Le wake est déclaré actif par mise à la masse par l'utilisateur. GPIO21
est réservé à la commande d'une interface transistor/opto ; aucune sortie
wake implémentée tant que tension à vide, durée impulsion/maintien et
interface ne sont pas identifiées. Ne pas connecter le fil wake directement
au GPIO. Ce build nécessite encore un réveil externe avant la séquence CAN.
Les écritures assistance restent nulles. Pas de flash effectué pour cet ajout.

Variante discutée : réveil par contact mécanique, sans alimentation ESP.
Un contact indépendant peut relier wake à GND si une impulsion suffit ;
le SW actuel est déjà connecté à GPIO22 et ne doit pas recevoir directement
le fil wake. Un bouton à deux contacts indépendants ou une isolation permet
la détection par l'ESP sans exposer son entrée à la tension wake. Si l'ESP
est alimenté seulement après le wake, le premier appui tenu au boot sera
ignoré par le logiciel actuel : un nouvel appui court sera nécessaire pour
lancer le CAN. Aucun auto-start au boot ajouté par hypothèse.

## Premier essai actif préparé — 25 octobre 2026

Utilisateur : console d'origine débranchée, bouton wake externe installé,
interdiction de faire tourner le moteur. GPIO21 reste non piloté.
Le maître conserve la séquence CAN neutre et le démarrage explicite encodeur.
Un garde indépendant dans l'envoi applicatif refuse toute écriture moteur
0x09 non nulle. Les tests vérifient également toutes les émissions 0x09.

Écran : tension batterie, température moteur, SOC, état et défaut lisible,
TX/RX, âge dernière réception, mesures fraîches sur 8, pertes (somme des
événements file/overflow, pas un décompte exact de trames perdues), TEC/REC,
étape et cible/registre attendu. Les valeurs périmées affichent -- ; les
conversions physiques candidates portent ?. TX indique une transmission
confirmée, pas la prise en compte d'un registre par le vélo. NEUTRAL indique
séquence terminée, pas validation physique du moteur. OFF indique séquence
d'arrêt transmise, pas coupure électrique mesurée.

Procédure prévue : bouton wake externe, attendre une seconde, appui court
encodeur ; observer STARTING puis NEUTRAL et les réponses. Maintien 2 s
encodeur pour demander la séquence d'arrêt. Aucun test de rotation demandé.
Le profil démarre OFF sans trame applicative ; le mode normal MCP peut ACK.

Flash non effectué : revue automatique demandant autorisation explicite.
La compatibilité image ESP32/HUZZAH32 a été vérifiée avec esptool image_info.

## Contrôle de pédalage préparé — 25 octobre 2026

Voir [ASSIST_BENCH.md](ASSIST_BENCH.md) pour l'analyse des deux dépôts,
le profil séparé, les commandes cal/arm et les réactions aux défauts.
Le profil courant neutre garde son garde zéro ; le nouveau `sdkconfig.assist-bench`
calcule la demande mais garde TX=0 jusqu'à validation des prérequis matériels.
Frein préparé sur GPIO36, non raccordé à la demande utilisateur pour le banc.
L'utilisateur a confirmé le réveil par appui court externe, les valeurs
cohérentes et l'extinction fonctionnelle avant cet incrément d'assistance.
