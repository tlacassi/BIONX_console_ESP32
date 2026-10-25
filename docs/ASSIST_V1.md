# V1 — console locale, assistance ×2 sans régénération

Développement du 25 octobre 2026 sur HUZZAH32 / ESP32 classique, GC9A01 et
MCP2515 à deux SPI. C'est une V1 expérimentale pour essais supervisés roue
levée. Le câblage reste celui de [HARDWARE.md](HARDWARE.md).
Le profil jog précédemment chargé n'est pas remplacé sans flash explicite.

## Arrêt mémorisé et reprise

Un appui coupe immédiatement, puis l'écran indique `STOPPED: CLICK`. Le
relâchement de ce même appui ne réarme pas. Un **nouveau clic court**, roue
immobile, pédales déchargées et données saines, relance le zéro puis l'armement
automatique. Si ces conditions ne sont pas réunies, le clic est refusé sans
reprise différée. Maintien2 s conserve l'extinction. Un défaut verrouillé n'est
pas effacé par ce clic. Le libellé WAIT ZERO de l'ancienne version pouvait
masquer cet état STOPPED ; les logs cal16/auto0/STOPPED le distinguent.

## Choix explicite de la mesure de courant

Deux sources batterie sont disponibles dans la configuration V1 :

| Source | Registres | Conversion candidate |
|---|---|---|
| Shunt BionX (défaut) |10:1E/1F|16 bits signés×0,001 A|
| Gas gauge batterie |10:D3/D4|16 bits signés×0,002141 A|

Option `BIONX_CURRENT_GAS`, overlay `sdkconfig.current-gas` à ajouter après
`sdkconfig.assist-v1` lors d'une nouvelle configuration. Dans une configuration
existante, utiliser menuconfig : les defaults ne remplacent pas forcément un
choix déjà enregistré. Un redémarrage du profil sélectionné repart avec des
paires vierges. Aucune bascule automatique, aucun mélange d'octets/sources.
La paire choisie remplace les lectures de courant, sans augmenter leur nombre.
Les contrôles fraîcheur, ordre, skew, U/I et signe sont conservés ; le log
`power_diag src=BAT_GG_D3D4` ou `BAT_SHUNT_1E1F` identifie la source.

D3/D4 est documenté dans BionXtool et Minimal Translator, mais support réel,
signe, latence et calibration restent à mesurer sur ce pack. Le moteur fournit
un indicateur14 en pourcentage ; aucun registre de courant moteur en ampères
n'est confirmé dans ces deux tables. Ne pas convertir14 en watts ni utiliser
les registres de limites moteur7A–7D comme une mesure de courant.

Le choix gas gauge est implémenté/testé et sa branche ESP32 compilée ; il n'a
pas encore été essayé sur le vélo. L'image actuellement flashée utilise encore
le shunt. Prochain essai utile : profil diagnostic gas gauge puis comparaison
repos/impulsion, avant de déclarer des watts fiables.

## Profil de banc sans limite de vitesse — 25 octobre2026

Sur demande explicite utilisateur, la V1 ne verrouille plus un défaut à25 km/h
et ne réduit plus l'assistance entre20 et25. La vitesse reste affichée et sa
fraîcheur reste obligatoire ; seules les valeurs hors format octet sont refusées.
Les profils jog et ancien banc pédale ne changent pas. Cap8/64, enveloppe≤400 ms,
arrêt bouton/frein, température/tension, NO MOTION, expiration et défaut CAN
restent actifs. Les seuils de vitesse cités plus bas sont historiques.

## Réponse impulsionnelle et diagnostic — 25 octobre2026

Demande utilisateur après essai : répondre à un coup de pédale court.
Montée désormais1 point brut/50 ms ; le pic de demande corrigée du zéro
alimente une enveloppe qui perd1 point/50 ms, expirant au plus400 ms après
le pic (maximum8). Une nouvelle impulsion mesurée peut renouveler la demande.
Cette brève décroissance remplace la baisse immédiate sur simple retour du
couple à zéro. **Bouton, frein, défaut, donnée périmée et survitesse coupent
immédiatement**, y compris pendant l'enveloppe. Les mesures doivent continuer
à arriver fraîches ; aucune prolongation si le capteur cesse de répondre.
Le filtre final TX vérifie la même enveloppe et les mêmes expirations.

Diagnostic : `pedal_rx` journalise les changements de couple ; `drive_tx`
confirme les commandes moteur effectivement transmises, y compris retour0.
`assist_diag` et `power_diag` donnent1 Hz état, âges, demande, sortie, émission,
U/I bruts/cohérence, watts/courant exacts et retour de mise à jour UI.
Un affichage-0 W a été expliqué par courant FF/FF, interprété-1 mA, soit
-0,039 W vers39,2 V. Validité physique du registre/shunt restant à confirmer.
Lors du premier essai détaillé, la survitesse a verrouillé le contrôle avant
les pics couple2…14 ; la récupération CAN seule ne réarme pas le contrôle.
Pour l'essai roue levée, repartir à l'arrêt et garder une vitesse faible.

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
