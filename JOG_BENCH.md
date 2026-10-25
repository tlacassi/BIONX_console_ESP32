# Essai encodeur, roue levée — 25 octobre 2026

Demande utilisateur : régler une petite vitesse par rotation, clic pour arrêter.
Il confirme roue levée et possibilité de débrancher la haute tension. Le frein
est préparé mais volontairement non raccordé. Ce mode de banc est distinct du
contrôle d'assistance par pédalage décrit dans ASSIST_BENCH.md.

## Commandes

- Au boot, après wake et init : cible zéro, commande zéro. Aucun démarrage
  sans nouvelle rotation de l'encodeur lorsque les mesures sont valides.
- Un sens augmente le niveau 0 → 1 → … → 10 ; l'autre la diminue. La direction
  physique dépend du câblage A/B ; vérifier le nombre affiché.
  Niveau0 : arrêt ; niveaux1–10 : cibles2,4,…20 km/h estimés.
- Clic : neutre immédiatement au prochain cycle, et contrôle du bouton encore
  juste avant émission. Le clic est prioritaire sur une rotation simultanée.
  Le vélo reste alimenté pour les mesures ; maintien 2 s demande son extinction.
- Sans rotation complète depuis 20 s : cible et commande zéro.
- Même en tournant : fin de session après 20 s, sans reprise automatique.
  Une nouvelle rotation explicite peut commencer une nouvelle session.
- Sur défaut de jog : clic puis relâchement acquitte si les données sont
  valides, la roue immobile et l’encodeur cohérent. La cible reste zéro et les
  crans reçus dans le même cycle sont ignorés ; nouvelle rotation nécessaire.
  Si les conditions échouent au relâchement, le défaut reste verrouillé ;
  aucune attente de récupération automatique. `reset` série reste disponible.
  Un défaut de transport CAN/MCP persistant n’est pas effacé par ce geste.


`Pbat` affiche la puissance électrique batterie échantillonnée : tension
A6/A7 (mV) × courant signé1E/1F (mA), en watts. Ce n’est pas la puissance
mécanique du moteur. Le `?` indique que calibration/signe/chemin du shunt
restent à valider sur ce montage (courant supposé passer par le shunt BionX).
Les quatre octets sont demandés en groupe en mode jog, période visée100 ms,
sans écriture de ces registres. La vitesse a priorité ; la mesure U/I publiée
reste celle du dernier groupe complet, sans mélanger une nouvelle tension
avec un ancien courant. Couple pédale demandé à4 Hz pour affichage, P14 à4 Hz. Paires HI/LO complètes, écart<100 ms, âge<500 ms et écart U/I≤200 ms
obligatoires ; sinon `Pbat -- W` et jog inhibé (défaut si déjà actif). Aucun remplacement par une puissance calculée
depuis la consigne. `max` conserve le maximum de vitesse estimée de la session.

L'affichage distingue JOG LIVE de JOG DRY RUN, cible et vitesse en km/h estimés (`~`), niveau et commande brute.
Une cible est ignorée si les mesures ne sont pas prêtes ; elle n'est pas mise en
attente pour déclencher un démarrage plus tard.

## Limites

Commande positive plafonnée au niveau sélectionné, maximum **8/64** au
démarrage, réduite à **4/64** dès que la vitesse est non nulle. Rampe1 brut
par200 ms. À la cible ou au-dessus : zéro/coasting, cible conservée pour
reprise en dessous. Aucun maintien positif imposé à la cible ni régène.
Après2 s sans vitesse détectée sous commande>=4 : défaut NO MOTION et zéro.
Ce contrôle détecte une absence de mouvement déclarée ; il ne mesure pas le
courant de phase ni un couple réel. Coupure OVERSPEED dès vitesse CAN
convertie>=25 km/h estimés ; commande zéro à la cible (au plus20 km/h).

Conversion : registre moteur11, facteur tiers candidat9,091 rpm/brut, puis
km/h=rpm×circonférence_mm×0,00006. Circonférence configurable via
`CONFIG_BIONX_JOG_WHEEL_MM` ;0 ou <1000 mm inhibe la commande.
Profil live :2136 mm provisoires pour pneu déclaré700×28C, source
[table CatEye](https://www.cateye.com/intl/support/manual/data/doc/CC-PA100W_HP_ENG_n_v4.pdf).
Mesure du tour réel et validation du facteur CAN requises pour calibration.
Affichage `~` : valeurs estimées, pas une garantie de vitesse réelle.
Avec2136 mm, un brut vaut~1,165 km/h ; bins21≈24,467 et22≈25,632.
Le premier bin détecté au-dessus du seuil est22. Résolution, latence et
inertie empêchent de garantir mécaniquement un plafond exact de25 km/h.
Aucune commande de freinage/régène n’est ajoutée.

Défauts : vitesse âgée de ≥150 ms, indicateur puissance ≥500 ms, tension/temp
≥2 s, tension hors30–42 V candidate, température ≥50 C candidate, indicateur
puissance >16 brut, boucle bloquée >100 ms, transition encodeur incohérente,
CAN/MCP/SPI/overflow/défaut protocolaire. Pas de reprise automatique du couple.
La consigne expire après100 ms sans renouvellement. Le bouton est vérifié au
plus près de l'émission ; une demande refusée est remplacée par zéro.

Un neutre logiciel ne coupe rien si CAN/ESP ne fonctionnent plus. Le timeout
moteur n'a pas encore été mesuré : la coupure haute tension indépendante doit
rester accessible. Roue levée, zone libre ; ne pas utiliser ce profil en roulant.
Aucun essai de rotation n'est déclenché automatiquement par l'assistant.

## État du CAN avant cet essai

Des échecs TX ont été relevés avec TXCTRL=0x50 (ABTF/TXERR), MERRF=1, pendant
un test antérieur. L'utilisateur indique ensuite qu'il rallume le système.
Capture continue après rallumage : `/tmp/bionx-assist-powered-comparison.log`,
30 s, init puis NEUTRAL/OK, TX2163/RX1013, zéro erreur TX/timeout/perte,
fresh8/8, ~39,52 V /34 C /SOC74%. Cela confirme les lectures rapides sur cet
intervalle ; l'extinction est une cause plausible des échecs précédents,
pas une preuve de leur cause unique. Aucun recâblage ni sécurité désactivée.

## Builds

Profil par défaut : encodeur calcul à blanc, `BIONX_JOG_TX=n`.
Profil live : overlay séparé `sdkconfig.encoder-jog-live`, autorisation
explicite de ce banc avec coupure HV confirmée ; ce flag n'affirme pas que le
watchdog moteur est validé. Le contrôle par pédalage garde son verrou propre.

```sh
export IDF_PYTHON_CHECK_CONSTRAINTS=0
. /Users/theolacassin/esp/v5.2/esp-idf/export.sh
idf.py -B build-encoder-jog -D IDF_TARGET=esp32 -D SDKCONFIG=build-encoder-jog/sdkconfig-live -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.master;sdkconfig.huzzah32;sdkconfig.components-dual-spi;sdkconfig.master-dual-spi;sdkconfig.assist-bench;sdkconfig.encoder-jog;sdkconfig.encoder-jog-live' build
```

Ne pas confondre compilation, observation de consigne nulle et validation
matérielle d'arrêt après une commande non nulle. L’utilisateur confirme la rotation et le pilotage au banc. La caractérisation
indépendante des arrêts/frein/perte CAN reste à faire.

## Historique : correction après le premier mouvement

La photo utilisateur montre ENCODER, maître NEUTRAL/OK, Target0 speed0 cmd0,
8/8 mesures fraîches et pertes0. Le polling A/B dans la boucle CAN/écran pouvait
manquer les états intermédiaires. Ce mécanisme est reproduit sur hôte ; il
reste une explication compatible avec la photo, pas une mesure électrique.

La nouvelle acquisition utilise les interruptions A25/B26 et bouton22. Chaque
cycle Gray complet devient un cran dans une file bornée de16 ; plusieurs crans
entre deux cycles applicatifs sont traités dans l'ordre. Les rebonds/inversions
valides s'annulent. Un clic bref est mémorisé même s'il est déjà relâché au
prochain cycle ; il efface les crans en attente et reste prioritaire avant TX.
Une transition deux bits invalide ou un overflow garde le défaut verrouillé.
Les compteurs `EncErr` et `Ov` sont affichés. Aucun changement de plafond,
cible, expiration, frein ou règle de neutre. Le service GPIO ISR existant du
MCP est partagé sans être désinstallé ; en cas d'échec d'initialisation, le
mode maître applicatif ne démarre pas.

7/7 tests hôte passent, dont les séquences complètes entre deux lectures,
rebonds, inversion, cycle partiel, clic bref, incohérence et overflow.
Ce point décrit l’état à cette étape. Depuis, l’utilisateur confirme le
pilotage moteur au banc ; aucune caractérisation de sécurité routière n’est déduite.

## Écran de défaut TX détaillé

En cas d'émission échouée, l'écran générique est remplacé par la **première**
erreur figée : classe (ARBITRATION / TX BUS ERROR / TX TIMEOUT / SPI ERROR /
CAN MODE / BUS OFF), code SDK, ID, registre, valeur, CTRL, INTF, EFLG, mode,
TEC/REC. Les sondes sans réponse en WAIT_WAKE ne figent pas cette vue.
Relever cet écran avant reboot ; ouvrir un moniteur série peut réinitialiser
la carte. Cet ajout ne réessaie pas de commande ni ne relâche les protections.

Diagnostic DATA : première cause et âge figés à l’écran (`SPEED OLD`,
`P14 OLD`, `U/I INVALID`, `LINK/MASTER`, etc.), sans réarmement automatique.

## Lecture du capteur de pédalage

Utilisateur confirme le pilotage moteur, puis le bon fonctionnement de
la lecture/affichage du capteur ; aucun relevé chiffré n’a encore été fourni. `Ped rawN maxM Ams` : registre moteur0x21, brut courant,
maximum depuis boot et âge. Lecture visée250 ms (sérialisée), stale à1 s.
Aucune conversion en force/Nm, ni déclenchement d’assistance depuis ce capteur.
Observer valeur au repos et sous effort pour établir la corrélation physique.
Voir [BENCH_FINDINGS.md](BENCH_FINDINGS.md) pour le bilan destiné au code final.
