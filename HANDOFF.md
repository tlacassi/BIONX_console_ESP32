# Point de reprise — V1 du25 octobre2026

## Demande et travail livré

L'utilisateur demande une V1 : lancement de l'initialisation dès le boot ESP,
interface ronde d'origine, un seul mode d'assistance ×2, sans régénération,
affichage de puissance ; rangement et publication sur son dépôt avec commits
réguliers. Tout le travail est sur `feature/assist-v1` du dépôt
https://github.com/tlacassi/BIONX_console_ESP32 ; `main` conserve la base importée.

Lire [README](README.md), [procédure V1](docs/ASSIST_V1.md),
[exigences](project_requirements.md), [TODO](TODO.md),
[bilan](docs/BENCH_FINDINGS.md) et [validation](docs/VALIDATION.md).
Les procédures/analyses sont maintenant dans `docs/`, les captures originales
inchangées dans `captures/`. Profils et entrypoints restent à la racine.

## Dernier état — sans limite vitesse, choix courant préparé

Image flashée V1 live sans défaut25 ni réduction20–25 :646128 octets,
SHA256 `051790b62c00cbf54eaaea222ba34843c18e305ad35e8d6923ece4e439f83ffe`.
Source courant encore shunt1E/1F. Capture laissée ouverte, session exec4428,
fichier `/tmp/bionx-v1-no-speed.log`, processus `/tmp/bionx-live-diagnostic.py`.
Lire ce log sans rouvrir le port. Dernier relevé STOPPED/NO REPLY batterieA7 ;
origine non établie, pas de réarmement de contrôle envoyé par l'assistant.

Code courant alternatif gas gaugeD3/D4 ajouté, option BIONX_CURRENT_GAS /
sdkconfig.current-gas ; conversion signée candidate0,002141 A/raw distincte du
shunt0,001. Aucun nouveau flash après cet ajout, GG non essayé physiquement.
Sources primaires ne confirment pas de registre courant moteur mesuré.
Prochaine étape : essai lecture GG, comparaison sous impulsion puis choix de
source et calibration ; garder anneau14 en pourcentage séparé des watts.

## Historique : diagnostic et réponse au coup de pédale

Port série **laissé ouvert à la demande utilisateur** par le processus
`/tmp/bionx-live-diagnostic.py`, session exec7907. Lire d'abord
`/tmp/bionx-v1-pedal-pulse.log` ; ne pas rouvrir le port ni flasher pendant
son essai. Capture initialement armée/NEUTRAL/OK, cmd0 sans effort assistant.

Firmware chargé : `build-assist-v1-live/sdkconfig`, V1_TX=y,646240 octets,
SHA256 `e486babc7231c16f0ad05f6bcef68b8b06d14e51f9b47ac23a0d462bc06f7d3f`.
Flash/hash vérifiés. Réponse demandée à l'impulsion : montée1/50 ms, enveloppe
qui décroît1/50 ms et expire≤400 ms, cap8, défaut/frein/bouton/péremption
coupent immédiatement. Le filtre TX autorise la même enveloppe. Tests9/9 et
2/2 UI, build sans warning. Essai reçu : TX confirmé jusqu'à7/64 puis0, P14=5 ; utilisateur rapporte
brève hausse de jauge. Nouveau RANGE ensuite. Pas de mesure mécanique indépendante.
Courant FF/FF reste figé malgré activité moteur : watts non crédibles comme
mesure moteur, vérifier registre/shunt/chemin du courant avant correction.

Cause du dernier essai sans moteur : vitesse dépassant25 km/h candidats avant
les pics de couple (jusqu'à14) ; RANGE verrouillé, aucune commande positive
confirmée. Les watts affichés-0 W viennent de FF/FF→-1 mA→-0,039 W, UI ESP_OK.
Mesure physique shunt non validée. La batterie a aussi cessé de répondre à
une lecture, NO REPLY ; récupération CAN ne réarme pas le contrôleur.
Traces bornées : `pedal_rx`, `drive_tx`, `assist_diag`, `power_diag`.

## Historique : V1 live automatique

Demande explicite utilisateur après confirmation du rendu, SOC et vitesse :
suppression des clics, anneau moteur, barre couple et émission d'assistance.
Profil `build-assist-v1-live/sdkconfig`, ESP32/HUZZAH32, ASSIST_V1=y et V1_TX=y.
Image644592 octets, SHA256 `bb1ec62cd8ee0f7d3e908f42d3faed9c99baa9503eaf975ecf64d0e1ab82e8bd`.
Flash115200/hash vérifiés. Zéro et armement automatiques au boot seulement,
pédales déchargées, roue immobile et mesures saines. Plafond8/64, gain brut×2,
rampe, expirations et défauts conservés ; aucune régène. Appui bouton coupe
sans reprise automatique. Aucun effort/commande positive de test de l'assistant.
Pas de capture série après ce flash pour ne pas réinitialiser un essai utilisateur.
Armement/réponse moteur de ce nouvel incrément à confirmer par l'utilisateur.

Anneau : mesure moteur14 (0–64, % candidat), pas vitesse ni consigne.
Barre cyan et TQ : capteur couple21 brut0–64. Vitesse centrale, CMD réellement
émise et puissance électrique batterie séparées. Un registre14 nul peut rester
nul malgré rotation, comme déjà observé ; aucune puissance mécanique inventée.

## Historique : V1 calcul à blanc

Flash autorisé et exécuté le25 octobre2026 sur HUZZAH32, profil
`build-assist-v1/sdkconfig`, V1_TX désactivé,643664 octets. SHA256
`db189abb652b285cfe9bdd558ddd1f16716cc9dfe34817b8a3fd75b49111f5d0`.
Flash/hash vérifiés ; capture20 s : découverte, init puis NEUTRAL/OK,8/8 frais,
commande0, pertes0. Anomalie ROM transitoire `flash read err,1000` avant reset
RTCWDT puis boot réussi ; cause non établie. Pas de nouveau reset à provoquer
pendant les essais utilisateur. Affichage/calibration/réponse pédale à confirmer.
Log et détails : [VALIDATION](docs/VALIDATION.md).

## Historique avant ce flash : jog précédent

**Aucun flash, port série ouvert ou mouvement initié pendant l'incrément V1.**
Dernier flash connu : `build-encoder-jog/sdkconfig-live`, ESP32/HUZZAH32,
image historique `bionx_c3_supermini.bin`,509520 octets, SHA256
`7aba6b67f6c0ce945dffb3d94d70c32d8f4a2d1d56fc837b1439a9c5c7b517ca`.
Le dossier jog a été recompilé pour vérifier les régressions : son binaire
local a changé, il ne représente plus cette image précédemment flashée.
Dernier port connu `/dev/cu.usbserial-016138BC`, à réidentifier sans ouvrir
un moniteur pendant un essai ou avant préservation d'un premier défaut.

Jog chargé : dix crans2…20 km/h estimés, plafond commande8 au démarrage/4
en mouvement, zéro à cible, clic arrêt/acquittement sous conditions,20 s de
session, aucune régène. Pilotage et lecture pédale confirmés par utilisateur.
Voir [JOG_BENCH](docs/JOG_BENCH.md) pour ce profil distinct de la V1.

## Images V1 initiales avant le flash calcul à blanc

- `build-assist-v1/sdkconfig` : calcul à blanc, initialisation CAN réelle,
  aucune assistance émise ;643664 octets, SHA256
  `961465f82d5f1e5065435a69490e88637f4953c635bb054c37efdda2d4b8a48b`.
- `build-assist-v1-live/sdkconfig` : émission de banc supervisé,643888 octets,
  SHA256 `db2ce015acafc014da0cc105596a84e5266f59f9f3b9c843c809d8c9b33f39dc`.
- Cible réelle **ESP32**, nom C3 du binaire historique. Ne pas flasher sur C3.
- Découverte batterie automatique après silence1 s ; init seulement sur réponse
  HW attendue, bouton wake externe conservé. Aucun pilotage wake GPIO.
- Clic sans effort = calibration16 réponses distinctes ; nouveau clic = armement
  stationnaire au neutre. Le boot ne lance pas le moteur.
- `min(8, 2*max(0,pédale-zéro-2))`, rampe1/200 ms, baisse immédiate,
  aucune régène, réduction20–25 km/h candidats, défaut à25, arrêt bouton.
- Un ×2 brut ne prouve pas un doublement du couple physique. Force/cadence,
  watts et vitesse non calibrés ; frein ESP non câblé ; timeout moteur sur perte
  CAN/reset toujours non mesuré. Profil réservé au banc roue levée avec coupure HV.
- Dashboard d'origine avec données réelles et `--` si périmées : watts batterie
  signés U×I (`BAT W?`), commande CAN effectivement émise (`CMD n/64`),
  vitesse estimée, SOC, état et raison de défaut. Aucune puissance mécanique inventée.

## Montage inchangé

HUZZAH32, écran SPI2 MOSI18/SCK5/CS14/DC32/RST27 ; MCP SPI3 SI23/SCK4/SO19,
CS33/INT34, quartz déclaré8 MHz, SPI2 MHz, CAN125 kbit/s. Encodeur25/26/22,
commun GND ; bouton wake externe, GPIO21 inutilisé. Frein36 optionnel avec
pull-up externe3,3 V et contact normalement fermé, non câblé sur le banc.

## Sources et vérifications

- Contrôle V1 pur : `components/bionx_master/bionx_assist.[ch]`.
- Entrées, commandes locales/série et filtre ultime : `main/assist_app.c`.
- Intégration CAN et suivi émission réelle : `main/bionx_master_app.c`.
- Ordonnanceur/paires U/I : `components/bionx_master/bionx_master.[ch]`.
- Dashboard partagé démo/V1 : `components/dashboard/dashboard.[ch]`.
- L'ancien banc pédale, le jog et le contrôleur historique restent séparés.

Tests9/9 contrôle/protocole et2/2 LVGL ; captures rejouées sans nouveau rejet ;
builds V1 dry/live et jog sans warning. Branche frein V1 compilée avec-Werror,
non activée. Procédures et commandes exactes dans ASSIST_V1 et VALIDATION.
Les tests et builds ne constituent pas une validation matérielle de la V1.

Prochaine étape : essais calcul à blanc déjà flashé, puis profil live seulement
sur autorisation explicite. Essais
utilisateur roue levée : boot/wake, calibration, armement, réponse pédale,
arrêt bouton, mesures U/I et validation indépendante des sécurités restantes.
