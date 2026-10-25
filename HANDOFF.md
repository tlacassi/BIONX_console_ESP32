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

## Firmware actuellement chargé : V1 calcul à blanc

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
