# V1 — console locale, assistance ×2 sans régénération

Développement du 25 octobre 2026 sur HUZZAH32 / ESP32 classique, GC9A01 et
MCP2515 à deux SPI. C'est une V1 expérimentale pour essais supervisés roue
levée. Le câblage reste celui de [HARDWARE.md](HARDWARE.md).
Le profil jog précédemment chargé n'est pas remplacé sans flash explicite.

## Mise à jour automatique — 25 octobre2026

L'utilisateur confirme le rendu, SOC et vitesse puis demande explicitement
la suppression des clics de calibration/armement et l'émission moteur.
Au boot, pédales déchargées et roue immobile : attente de mesures saines,
calibration16 réponses distinctes stables (≤8, dispersion≤2), puis armement
automatique au neutre. Un zéro instable/chargé recommence la fenêtre sans
commande. Un faible effort statique stable peut ressembler à un offset :
le logiciel ne peut pas prouver seul que les pédales sont déchargées.

Une fois ASSIST X2 affiché, l'effort pédale peut commander le moteur dans le
profil live. Le plafond8/64, rampe, défauts et absence de régène sont conservés.
Appui bouton = arrêt bloquant, même pendant le zéro ; pas de reprise automatique
après stop ou défaut. Un nouveau boot recommence l'automatisation ; `cal` puis
`arm` restent des commandes de diagnostic disponibles, pas une étape normale.

Anneau = registre moteur14, facteur candidat100/64 %, affiché MTR…%?.
Aucune substitution par la commande ou la vitesse si cette mesure manque.
Ce registre a déjà renvoyé0 pendant une rotation : un anneau vide ne prouve pas
l'absence d'assistance. CMD n/64 et watts batterie restent séparés.
Barre horizontale cyan = capteur21 brut0–64, valeur TQ…/64, sans conversion N/Nm.
Ces deux jauges sont vidées et marquées-- si la mesure est périmée.
La vitesse centrale est conservée. Les étapes manuelles ci-dessous décrivent
la V1 précédente et ne sont plus nécessaires au boot normal.

## Historique : démarrage et commandes manuels

L'écran d'origine (anneau, vitesse centrale, batterie et puissance) apparaît au
boot. Après une seconde de silence CAN, l'ESP tente une lecture HW batterie
chaque seconde. Une réponse attendue déclenche l'initialisation et la copie
des limites batterie existantes. L'alimentation de l'ESP ne prouve pas que la
batterie est éveillée : le bouton wake externe peut rester nécessaire. Aucun
GPIO ne commande ce wake. Console originale déconnectée, un seul maître.

1. Lever la roue, garder la coupure haute tension indépendante accessible,
   ne poser aucun effort sur les pédales et attendre les mesures.
2. À `CLICK ZERO`, clic puis relâchement : calibration sur 16 réponses couple
   distinctes et fraîches, roue immobile. Valeur ≤8, dispersion ≤2. `cal` série
   fait la même chose. Laisser les pédales entièrement déchargées.
3. À `CLICK ARM`, nouveau clic puis relâchement au neutre : armement. `arm`
   série est équivalent. L'armement ne produit aucune commande à lui seul.
4. Charger doucement la pédale après armement. Un effort statique peut suffire :
   le capteur de couple n'est pas un détecteur de cadence. Aucun PAS ajouté.
5. Appuyer sur le bouton : arrêt de l'assistance dès la lecture du niveau bas,
   vérifié aussi juste avant émission. Le relâchement de ce geste ne réarme pas.
   Nouvelle calibration puis nouvel armement nécessaires. Rotation sans effet.
6. Maintien 2 s ou `q`/`stop` : séquence d'extinction existante. Après arrêt,
   clic ou `s` relance la découverte, sans restaurer une demande moteur.

Après défaut contrôle, éliminer la cause, puis refaire calibration et armement.
Un défaut transport reste bloquant, sans récupération positive automatique.
Le zéro est volontairement volatil : aucun couple actif restauré depuis NVS.

## Loi de commande et limites

`effort = max(0, capteur_0x21 - zéro_calibré - 2)`

`demande = min(8, 2 × effort)`

Il s'agit de deux fois le **signal brut corrigé**, pas de deux fois une force
mesurée en N, un couple en Nm ou une puissance humaine en W. Les deux registres
ont une échelle candidate en pourcentage ; leur rapport physique doit être
calibré sur le vélo. La saturation à8/64 et la rampe limitent volontairement
la demande pendant cette première validation. Aucun gain d'un autre vélo
n'est présenté comme calibré ici. Le facteur ×2 reste unique, sans menu de modes.

- Rampe montante : un point brut par200 ms ; baisse immédiate.
- Réduction linéaire de la demande entre20 et25 km/h candidats ; défaut à25.
  Circonférence2136 mm provisoire pour700×28C, facteur9,091 rpm/brut.
- Température moteur <50 °C, batterie30–42 V : conversions et bornes candidates.
- Couple/vitesse frais <150 ms ; indicateur moteur14 <500 ms ; température et
  tension <2 s. Puissance batterie U/I cohérente obligatoire, <500 ms.
- Boucle/consigne100 ms maximum ; ultime vérification avant transport.
- Couple >64 ou saut montant >16 : défaut ; relâchement rapide coupe immédiatement.
- Aucune vitesse pendant2 s sous commande≥4 : défaut NO MOTION.
- Pas de consigne négative, pas de régénération sur frein ou défaut.

L'acquisition V1 donne priorité vitesse50 ms puis couple50 ms, puis termine
les groupes U/I et les lectures secondaires. Une seule requête en attente.
Les objectifs de période ne garantissent pas les délais physiques ; une
expiration reste bloquante. La charge simulée1/6/11 ms est testée.

## Ce que montre l'écran

- `ASSIST X2` : mode unique ; état bas `CLICK ZERO`, `CLICK ARM`, `ASSIST X2`
  ou raison de défaut. En calcul à blanc : `DRY RUN`.
- Vitesse et SOC CAN ; `--` si absents/périmés. Vitesse encore estimée.
- `BAT W?` : puissance électrique batterie signée U×I, pas la puissance
  mécanique à la roue. Le `?` rappelle que shunt, signe et calibration restent
  à vérifier. Les auxiliaires peuvent être inclus. Une mesure zéro reste zéro.
- `CMD n/64` : dernière commande moteur dont le transport CAN a confirmé
  l'émission ; ce n'est pas une mesure de puissance moteur. Le moteur peut
  encore avoir une autre réponse physique. En défaut maître : `CMD --`.
- `NO REGEN` : aucune consigne de régénération. Un wattage négatif reçu reste
  affiché comme mesure signée ; il ne signifie pas que l'ESP a demandé la régène.

L'interface invalide les valeurs après500 ms sans mise à jour de l'application.
Les premières erreurs TX détaillées restent journalisées côté série. L'ouverture
du port peut réinitialiser la carte ; relever l'écran avant toute connexion.

## Profils et compilation

Charger l'environnement ESP-IDF comme décrit dans le README, puis depuis
la racine du dépôt :

```sh
# Profil de calcul à blanc : initialisation CAN réelle, assistance TX toujours0.
idf.py -B build-assist-v1 -D IDF_TARGET=esp32 -D SDKCONFIG=build-assist-v1/sdkconfig -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.master;sdkconfig.huzzah32;sdkconfig.components-dual-spi;sdkconfig.master-dual-spi;sdkconfig.assist-bench;sdkconfig.assist-v1' build

# Image distincte d'essai moteur, uniquement banc supervisé avec coupure HV.
idf.py -B build-assist-v1-live -D IDF_TARGET=esp32 -D SDKCONFIG=build-assist-v1-live/sdkconfig -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.master;sdkconfig.huzzah32;sdkconfig.components-dual-spi;sdkconfig.master-dual-spi;sdkconfig.assist-bench;sdkconfig.assist-v1;sdkconfig.assist-v1-bench-live' build
```

Le binaire porte encore le nom historique `bionx_c3_supermini.bin`, mais ces
profils sont pour **ESP32/HUZZAH32**. Ne pas les flasher sur ESP32-C3.
Les autorisations du jog et de l'ancien banc pédale ne lèvent pas le verrou
`BIONX_V1_TX`. Aucun flash automatique ni essai moteur initié par l'assistant.

## Restant à mesurer avant usage roulant

Frein ESP non câblé sur le banc actuel. L'option frein36 existante est prise en
charge : contact normalement fermé vers GND, pull-up externe3,3 V, ouverture
puis retour fermé stable requis avant armement. L'absence de frein câblé est
une limite explicite du profil expérimental, pas une validation du frein.

Calibrer le capteur et le gain physique, la circonférence/vitesse, U/I et leur
signe ; raccorder et tester le frein ; mesurer l'arrêt moteur sur perte CAN et
reset ESP. Un CAN rompu ne transmet pas zéro. La coupure indépendante reste
nécessaire ; aucun test hôte ne prouve la roue libre physique. Ni validation
routière, ni odomètre, ni réseau/OTA ne sont revendiqués pour cette V1.

## Références relues

[Minimal Translator](https://github.com/stancecoke/BionX_Minimal_Translator/tree/d8f4fbb28d16435d4dd8182539962dab29c3d82b)
et [BionxConsole](https://github.com/inarighas/BionxConsole/tree/41b06f7d1fc50b60cae9123ee55e801b9e330f54)
relus aux commits documentés : calcul côté console effort→demande, registre09,
doubles écritures et télémétrie. Aucune source copiée. Voir
[ASSIST_BENCH.md](ASSIST_BENCH.md) et [BIONXTOOL_ANALYSIS.md](BIONXTOOL_ANALYSIS.md)
pour l'analyse et ses limites. Pas de PI de vitesse : la V1 suit l'effort,
avec saturation, rampe et réduction en vitesse, sans poursuivre une vitesse cible.
