# TODO — console BionX CAN sur MCP2515

## V1 automatique et émission moteur — 25 octobre2026

État courant : profil live flashé, zéro/armement automatiques, anneau moteur14,
barre couple21. Tests9/9 et2/2, build sans warning, empreintes flash vérifiées.
Aucune commande de test envoyée par l'assistant ; retour physique utilisateur
sur armement et assistance attendu. Les sections suivantes gardent l'historique.

Utilisateur confirme boot, interface, SOC et vitesse ; demande explicite de
zéro/armement automatiques, anneau puissance, barre couple et émission moteur.
- [x] Automatiser la calibration stable au boot et l'armement au neutre ; garder arrêt/défaut bloquants.
- [x] Anneau = indicateur moteur14, barre = couple21 ; masquer les données périmées.
- [x] Tester transitions automatiques et jauges ; compiler le profil live.
- [x] Flasher le profil live demandé, sans déclencher d'effort ni commande positive de test.
- [x] Consigner les résultats et publier les commits.

## V1 assistance ×2 — travail demandé le 25 octobre 2026

- [x] Initialiser Git et publier une base fidèle du banc sur le dépôt utilisateur.
- [x] Ranger documentation et captures, préserver les références et profils existants.
- [x] Relire les sources tierces ; définir le gain brut ×2 sans inventer une calibration physique.
- [x] Ajouter un profil V1 séparé : découverte automatique au boot, un mode, aucune régène.
- [x] Restaurer le dashboard initial avec données réelles, péremption, puissance batterie et état de contrôle.
- [x] Tester commande/arrêt/défauts/polling et interface ; compiler HUZZAH32 et vérifier régressions.
- [x] Documenter limites, procédure et résultats ; committer et publier les incréments.

Preuves V1 : tests9/9 contrôle et2/2 LVGL, captures rejouées sans nouveau
rejet, builds ESP32 dry/live et jog sans warning ; empreintes et détails dans
[VALIDATION](docs/VALIDATION.md). Commits publiés sur `feature/assist-v1`.

- [ ] Essais physiques V1 après autorisation de flash : calibration, pédalage et arrêt.
- [ ] Calibrer le gain physique, vitesse et U/I ; frein et arrêt sur perte CAN/reset.

Aucun flash demandé dans cet incrément. Le profil jog chargé reste inchangé.

## État courant pour la reprise — 25 octobre 2026

Lire [HANDOFF.md](HANDOFF.md) et [BENCH_FINDINGS.md](docs/BENCH_FINDINGS.md).

- [x] Pilotage moteur au banc confirmé par l’utilisateur.
- [x] Lecture/affichage capteur pédalage jugés fonctionnels par l’utilisateur.
- [x] Dernier profil : build-encoder-jog/sdkconfig-live, dix crans2–20 km/h estimés,
  survitesse25, affichage Ped raw/max/âge, tests8/8 et flash vérifié.
- [ ] Relevés capteur repos/effort, calibration physique, vitesse réelle et U/I.
- [ ] Frein et comportement sur perte CAN/reset ; contrôle final/usage routier.

Le reste du fichier est un **journal chronologique** : les mentions « dernier »,
« actuel », « non testé » et cases ouvertes se rapportent à leur incrément.
Le statut courant ci-dessus prime ; les anciennes preuves ne sont pas réécrites.

Objectif complet : remplacer la console par un maître CAN bidirectionnel.
L’écoute seule est un mode de diagnostic initial, pas l’architecture finale.

Demandé le 23 octobre 2026 : traiter les tâches ci-dessous dans l’ordre et
mettre à jour leur état avec les preuves de validation.

## Phase 1 — observation (logiciel terminé, matériel non testé)

1. [x] Inspecter le socle, le plan GPIO et les contraintes du SPI partagé.
   État initial : transport TWAI, aucun MCP2515 ni réveil électrique implémenté.
2. [x] Définir les trames CAN et isoler le protocole BionX ; tests hôte de
   validation des identifiants, formats, DLC et réponses candidates.
   Preuve : `ctest --test-dir build-unit`, test `protocol` réussi.
3. [x] Centraliser le bus SPI écran/MCP2515 et valider les conflits GPIO.
   Conserver le profil écran existant et borner les transactions écran.
   Preuve : build dashboard réussi, sans avertissement. Validation GPIO au
   démarrage ; observation utilise deux lignes par transfert et profondeur 1.
4. [x] Implémenter le MCP2515 en écoute seule : choix explicite du quartz,
   initialisation vérifiée, RX standard/étendu/RTR et compteurs d’erreurs/pertes.
   Tester le pilote sur un modèle SPI hôte et compiler pour ESP32-C3.
   Preuves : tests `protocol` et `mcp2515` réussis, build ESP32-C3 réussi.
5. [x] Ajouter le profil observation : RX prioritaire, file de capture bornée,
   journal horodaté et écran de diagnostic sans données simulées.
   Implémenté : état configuration requise si module/quartz non renseignés,
   capture limitée en débit avec omissions comptées séparément des pertes RX.
6. [x] Exécuter les tests hôte et builds observation/dashboard/diagnostic
   existant ; corriger les erreurs et avertissements introduits.
   Preuves : 2 tests protocole/pilote + 1 test dashboard réussis ; les trois
   profils ESP32-C3 compilent. Chemin MCP activé également compilé, sans flash.
7. [x] Documenter build, câblage proposé, paramètres à confirmer et procédure
   d’essai vélo ; mettre à jour README, HARDWARE et VALIDATION.

## Phase 2 — intégration active et validation matérielle

8. [ ] Identifier le module MCP2515 : quartz, transceiver, niveaux logiques,
   alimentation et terminaisons. Puis choisir son quartz dans menuconfig.
9. [ ] Vérifier l’écoute passive et les pertes sur banc CAN, écran sollicité,
   puis capturer le vélo avec la console d’origine (démarrage et extinction).
10. [x] Ajouter le transport bidirectionnel (logiciel, banc restant) et le mode maître explicite :
    TX borné, ACK/erreurs/bus-off, tests de pannes, lectures actives sérialisées
    et réponses candidates expirables. Aucun second maître actif en parallèle.
11. [ ] Identifier et mesurer le wake batterie : broche, référence, tension,
    polarité, impulsion/maintien. Définir son interface électrique avant code GPIO.
12. [x] Implémenter les séquences maître (mode neutre pour le banc) démarrage/arrêt et les échanges
    périodiques réellement requis (maintien en fonctionnement, lectures et
    paramètres volatils), après confrontation aux captures de cette variante.
13. [ ] Valider les registres, conversions et délais pour cette variante
    moteur/BMS d’origine ; ajouter la télémétrie avec validité/expiration.
14. [x] Implémenter puis tester la machine de contrôle (tests hôte), le frein prioritaire,
    les limites, le réarmement et la perte console avant commande non nulle.
15. [ ] Essais de remplacement de console roue levée, arrêt accessible et
    limitations conservatrices ; aucune validation roulante à ce stade.

Les points 8–15 incluent des mesures/identifications absentes du dépôt.
Le profil observation ne doit envoyer aucune trame ni réveiller la batterie.
Le nouveau profil maître est distinct, émet uniquement après commande explicite
et conserve une assistance nulle. Pas de flash d’un
profil MCP2515 tant que son alimentation et son quartz ne sont pas identifiés.

## Capture d’origine reçue — 0000235.TXT

Analyse et rejeu effectués : [rapport](docs/LOG_ANALYSIS_0000235.md),
`python3 tools/check_cl1000.py`. 27 104 trames reconnues sur 27 679 ;
575 trames 0x58/009C restent sans sémantique décodée. Deux tests hôte passent.
La capture contient des émissions du logger et ne valide pas à elle seule
les points 9, 12 ou 13 : démarrage/arrêt complets, conversions et essais
matériels restent à établir. Le journal limité à 100 trames/s omettrait
une partie du trafic observé (~189 trames/s).

## Incrément firmware après analyse BionXtool

- [x] Reconnaître cibles diagnostic 0x48/0x50/0x60 et réponses 0x58 DLC 4,
  sans mélanger réponses diagnostic et console normale.
- [x] Conserver 0x58/009C DLC 2 comme UNKNOWN, sans sémantique inventée.
- [x] Ajouter noms et conversions candidates des registres mono-octet,
  signe assistance, refus des valeurs hors plage et absence d’inférence
  d’origine pour les réponses partagées. HI/LO restent bruts.
- [x] Étendre les lectures TWAI de diagnostic aux registres documentés ;
  exclure la lecture destructive du compteur moteur 0x12.
- [x] Tests hôte protocole/MCP2515 et rejeu des 27 679 trames :
  27 104 décodées, 575 UNKNOWN, aucun rejet de format.

Transport MCP2515 TX, séquences de démarrage/arrêt, assemblage HI/LO avec
validité/expiration et télémétrie réelle restent à implémenter/valider selon
la phase 2. Aucune écriture CAN ni commande de réveil ajoutée.

## Capture locale démarrage/arrêt — 24 octobre 2026

`start&stop.csv` analysé : [rapport](docs/START_STOP_ANALYSIS.md).
Séquence CAN démarrage/arrêt et copie des limites batterie → moteur observées
sur notre système. Runtime exclusivement immobile, assistance nulle. 1 690
trames rejouées dans le parseur C : 1 648 décodées, 42 UNKNOWN, aucun rejet.
L’outil `tools/check_cl1000.py` accepte désormais cet export CSV.
Tous les timestamps CSV valent zéro : délais/cadences, silence après arrêt et
wake électrique restent non validés. Les points matériels de phase 2 restent ouverts.

## Implémentation phase 2 — 24 octobre 2026

Voir [MASTER_BENCH.md](docs/MASTER_BENCH.md) pour le périmètre, les hypothèses et
les builds. Ce relevé remplace les mentions historiques « TX non implémenté ».

- [x] MCP2515 normal explicite, TX one-shot borné, confirmation TX0IF,
  erreurs/arbitrage perdu/bus-off et abandon, sans reprise automatique.
- [x] Maître démarré/arrêté par `s`/`q`, une lecture à la fois, réponses
  candidates expirables, détection de trafic d’un autre maître.
- [x] Démarrage de la capture locale, vérification du profil, copie dynamique
  des limites brutes batterie vers moteur ; refus des valeurs non prévues.
- [x] Runtime neutre et arrêt, délais provisoires configurables.
- [x] Mesures brutes horodatées et expirables, tension HI/LO avec contrôle de
  fraîcheur/ordre/écart, écran réel de diagnostic maître avec `--` si périmé.
- [x] Contrôleur indépendant testé : frein prioritaire, limites, rampe,
  demande conducteur bornante, régène autorisée, armement neutre et réarmement.
  Aucune sortie non nulle reliée au CAN tant que les entrées/profil sont non validés.
- [x] Profil séparé Adafruit HUZZAH32 Feather ESP32, pinout proposé, C3 conservé.
- [ ] Point 8 : quartz/transceiver/niveaux/terminaison à identifier matériellement.
- [ ] Point 9 : essai de notre MCP et mesure des pertes ; capture console reçue.
- [ ] Point 11 : mesure wake électrique, aucune broche pilotée par hypothèse.
- [ ] Point 13 : calibration des conversions et délais sur le vélo.
- [ ] Point 14 matériel : entrée frein, courant, loi couple/assistance et arrêt
  moteur sur perte console à valider avant liaison du contrôleur au CAN non nul.
- [ ] Point 15 : essais roue levée, sans validation roulante revendiquée.

## Test combiné HUZZAH32 — en diagnostic

- [x] Profil test écran/encodeur/MCP passif créé, compilé et flashé.
- [ ] Affichage physique : utilisateur signale écran noir ; SDA/SCL raccordés
  vers MO/SCK, alimentation/CS/DC/RST à vérifier.
- [ ] Encodeur : A1(GPIO25)/A0(GPIO26)/SCL(GPIO22), masse commune à vérifier ;
  aucune rotation ou clic encore observé sur les fenêtres de capture série.
- [ ] MCP : timeout, CANSTAT lu à 00 après reset SPI ; vérifier SPI/alimentation.
  Ni le CAN H/L ni le mode maître ne sont validés par ce test.

Voir `COMPONENT_TEST.md` et le relevé dans `VALIDATION.md`.

Écran isolé à la demande utilisateur : profil `build-screen-test` compilé,
flashé et exécuté à 1 MHz. Motifs confirmés côté série, écran toujours noir
selon l’utilisateur ; contrôle physique VCC/GND/BL et des fils nécessaire.
Encodeur et MCP laissés de côté pour le moment. Dernier firmware flashé :
**test écran seul**, pas mode maître.

Écran confirmé fonctionnel par l’utilisateur après correction de pins décalées.
MCP débranché ; poursuite avec le profil `sdkconfig.encoder-test` (sans CAN).

Test encodeur isolé flashé (MCP désactivé). Rotation/appuis initiaux non détectés.
Test direct GPIO22→GND : états appuyé/relâché et 2 clics observés ; entrée bouton
logicielle validée. Câblage/contacts SW/commun et voies A/B encore à vérifier.

## Confirmation utilisateur — écran et encodeur

- [x] Écran : affichage fonctionnel après correction du pinout.
- [x] Encodeur : utilisateur confirme rotation et bouton fonctionnels après
  correction du pinout. Confirmation manuelle ; pas de mesure de pertes à haute vitesse.
- [ ] MCP : toujours déconnecté et non validé ; ancien timeout à réexaminer
  avec le câblage réel avant tout test CAN.

Dernier firmware : `build-encoder-test`, écran + encodeur, MCP désactivé.

## MCP : communication SPI confirmée

- [x] Reset, écriture/relecture timing et passage écoute seule validés sur
  le MCP reconnecté (CANSTAT 80→60, init ESP_OK, quartz déclaré 8 MHz).
- [x] Partage SPI fiabilisé par IO synchrone et verrou commun ; statistiques
  publiées sans attendre RX. Dernier essai combiné 30 s sans assertion ni timeout.
- [x] Réception CAN réelle confirmée sur vélo/console allumés, test MCP isolé.
- [x] Réduire les pertes massives : dernier essai 4 512 trames / 30 s,
  zéro perte de file ; trois événements de débordement matériel.
- [x] Capture du banc sans perte détectée : profil deux SPI, 13 878 trames
  en environ 90 s, drops=0/ov=0/malformed=0/SPI errors=0.
  Ce résultat borne la validation à cet essai, pas à toutes les conditions.
- [x] INT34 : fronts reçus et notifications RX mesurées sur le vélo réel.

État précédent : test isolé `BIONX_MCP_SERIAL_ONLY=y`. Le test combiné
optimisé ci-dessous le remplace.

## Optimisation RX et partage SPI — 24 octobre 2026

- [x] RX passive : deux transactions par trame avec READ RX BUFFER ; santé
  toutes les 10 ms, contrôles complets conservés en mode actif. Tests hôte 3/3.
- [x] Buffers SPI internes pour échanges courts ; IO écran synchrone, verrou
  commun récursif, une ligne LCD par transfert. Écran 20 MHz/MCP SPI 2 MHz
  confirmés fonctionnels par l’utilisateur (réactivité améliorée).
- [x] Tests hôte 3/3 ; builds composants HUZZAH32, observation ESP32-C3 et
  maître HUZZAH32 réussis sans avertissement. Seul le test composants passif
  est flashé. Mesure finale : 4 512 reçues, drops=0, ov=3 en environ 30 s.

Firmware conservé : `build-components/bionx_c3_supermini.bin`, profil composants
HUZZAH32, LCD 20 MHz/MCP SPI 2 MHz, INT34 + secours 1 ms, CAN listen-only.
Statistiques copiées sous section critique ; affinité CPU automatique.

## Débordements résiduels — 25 octobre 2026

- [x] Impressions `can_perf` déplacées hors RX ; compteurs publiés et lus
  par le consommateur, avec malformed/SPI errors.
- [x] Tests hôte 3/3 et builds partagés ESP32-C3/HUZZAH32 vérifiés.
  Mesure combinée 9 111 reçues/60 s, drops=0, ov=3 ; isolée aux mêmes
  réglages 9 240 reçues/60 s, drops=0, ov=0.
- [x] Buffer RX libéré avant diagnostics passifs ; tests couvrant deux
  buffers pleins, fautes SPI et contrôles de mode passif/actif.
- [x] Profil HUZZAH32 deux bus indépendants codé et compilé sans avertissement :
  LCD SPI2, MCP SPI3 SI=23/SCK=4, SO/CS/INT inchangés ; verrous indépendants,
  validation GPIO/doublons/encodeur, repli propre si initialisation SPI3 échoue.
- [x] Déplacement des deux fils confirmé par l’utilisateur ; profil
  `sdkconfig.components-dual-spi` flashé, empreintes vérifiées.
- [x] SPI3 validé après vérification/reconnexion utilisateur : init ESP_OK,
  13 878 trames / 90 s sans perte de file ni débordement ; écran actif.
  L’utilisateur confirme le fonctionnement très réactif de l’ensemble.

Référence du banc au 25 octobre : `build-components-dual-spi`, overlay
`sdkconfig.components-dual-spi`, SI23/SCK4/SO19/CS33/INT34 pour le MCP.
Écran SPI2 inchangé. Aucun maître actif flashé pendant cette validation.

## Commande encodeur du maître — 25 octobre 2026

- [x] Ajouter appui court au relâchement = démarrer, maintien 2 s = arrêt,
  anti-rebond 25 ms ; bouton maintenu au boot sans action jusqu'au relâchement.
- [x] Préparer le profil maître HUZZAH32 avec le câblage deux SPI actuel.
- [x] Tester les gestes sur hôte et compiler le profil actif sans flash.
  Preuve : 4/4 tests hôte passent ; build-master-dual-spi ESP32 réussi.
- [ ] Identifier tension/polarité/interface wake avant pilotage GPIO21.
- [ ] Valider réveil, lectures et extinction sur le vélo après retrait console.

## Premier maître sur vélo — 25 octobre 2026

- [x] Ajouter diagnostics lisibles écran/série : tension, température, SOC,
  état/erreur explicites, progression, TX/RX et pertes.
- [x] Compiler maître deux SPI et rejouer les tests hôte.
  Preuve : 4/4 tests ; image ESP32 vérifiée par esptool image_info.
- [x] Charger le profil maître : boot OFF silencieux 1 s, puis WAIT_WAKE
  et requêtes périodiques selon la nouvelle demande utilisateur.
- [ ] Essai utilisateur : bouton wake externe, appui court encodeur, mesures,
  appui long arrêt. Console d'origine déclarée débranchée ; couple toujours nul.

Flash maître bloqué par revue automatique : autorisation explicite demandée.
Aucun flash effectué ; le dernier firmware passif reste sur la carte.

## Wake externe et détection automatique — 25 octobre 2026

- [x] Ajouter WAIT_WAKE : lecture batterie HW 0x3B toutes les secondes,
  démarrage uniquement sur réponse candidate attendue ; aucun SET avant réponse.
- [x] Tolérer les échecs TX d'une sonde sans destinataire pendant WAIT_WAKE,
  conserver SPI/mode/bus-off/pertes comme défauts bloquants.
- [x] Tester attente, réponses invalides/tardives, init automatique, arrêt
  sans redémarrage automatique ; confirmer toutes les consignes à zéro.
- [x] Compiler, vérifier image ESP32/HUZZAH32, flasher (autorisation reçue).
- [x] Observer boot, initialisation automatique et échanges sur vélo ; résultats
  consignés dans VALIDATION.md (45 s, aucun défaut).
- [ ] Confirmer réveil depuis sommeil établi et extinction physique par appui long.

Premier runtime actif stable après correction de famine du verrou RX/TX :
20 s, NEUTRAL/OK, 2616 TX / 1878 RX, fresh=8/8, pertes=0,
TEC/REC=0, txerr=0, timeout=0. Valeurs candidates ~39,57 V, 35 C, SOC75%.
Arrêt réel et réveil depuis batterie effectivement endormie restent à vérifier.

Version finale flashée : télémétrie avec pause 1 s par cycle, cache tension.
45 s de série : NEUTRAL/OK, TX=2115 RX=331, 8/8 mesures fraîches,
pertes=0, TEC/REC=0, txerr=timeout=0, ~39,57 V / 36 C / SOC75%.
L'utilisateur confirme que la batterie semble communiquer. Aucun essai de rotation.

## Contrôle d'assistance de banc et sécurité — 25 octobre 2026

- [x] Analyser les deux dépôts demandés à des commits identifiés ; distinguer
  loi de commande, limites et sécurités réellement présentes.
- [x] Définir un contrôleur simple positif seulement, neutre au démarrage,
  armement explicite, frein prioritaire, entrées fraîches et plausibles,
  rampe/plafond bas et défaut verrouillé ; aucune régénération sur défaut.
- [x] Intégrer les acquisitions rapides et la demande au maître ; sur défaut,
  tenter le neutre uniquement si transport utilisable, sans prétendre qu'une
  rupture CAN permet de transmettre une coupure.
- [x] Tester sur hôte les défauts et limites, compiler les profils concernés.
  Preuve : 5/5 tests, builds banc dry-run et maître neutre ; branches optionnelles
  TX/frein compilées séparément, non activées dans le profil livré.
- [x] Documenter procédure de banc, prérequis matériels et limites restantes.
  Preuve : ASSIST_BENCH.md, commits sources identifiés et limites explicites.
- [ ] Valider frein, calibration couple et timeout moteur avant commande non nulle.

Confirmation utilisateur avant cet incrément : réveil fiable par bouton externe,
valeurs cohérentes et extinction fonctionnelle. Cela valide ces fonctions sur
son montage ; le timeout d'arrêt moteur sur perte CAN n'est pas encore mesuré.
Le frein reste non raccordé à sa demande pour les premiers tests ; le profil
banc livré est en calcul à blanc, TX non nul verrouillé.

## Test encodeur roue levée — demande utilisateur du 25 octobre 2026

- [x] Identifier le statut TX et refaire une capture système rallumé :
  TXCTRL50 sur essais précédents ; 30 s rapides sans erreur après rallumage,
  extinction cause plausible sans preuve unique. Aucun défaut masqué.
- [x] Ajouter un mode de banc encodeur : petites vitesses cibles, plafond
  faible de commande, clic neutre immédiat, expiration sans action,
  données fraîches et défaut verrouillé. Aucun redémarrage automatique.
- [x] Tests hôte et compilation ; relever les paramètres réellement livrés.
  Preuve : 6/6 tests ; build ESP32/HUZZAH32, paramètres dans JOG_BENCH.md.
- [x] Charger le mode de banc autorisé et laisser toute rotation à l'utilisateur.
  Preuve : flash/hash vérifiés, boot JOG LIVE READY, target=speed=cmd=0,
  15 s sans défaut ; aucune consigne positive déclenchée par l'assistant.

Utilisateur confirme roue levée et possibilité de débrancher la haute tension.
Il demande explicitement un test de rotation à faibles vitesses avec encodeur.
Le timeout moteur sur perte CAN reste non mesuré ; arrêt logiciel non garanti
si la liaison est coupée. La coupure physique indépendante reste le recours.

Profil désormais flashé : build-encoder-jog (sdkconfig-live), JOG_TX=y pour
le banc explicitement autorisé. Le contrôle par pédalage reste séparé et son
verrou TX_VERIFIED reste désactivé. Retour matériel sur rotation et clic attendu.

## Premier mouvement encodeur : défaut verrouillé à diagnostiquer

- [x] Utilisateur confirme un premier mouvement moteur puis arrêt.
- [x] Relever le motif JOG LIVE FAULT et l'état maître à l'écran, avant reboot.
  Photo : ENCODER, maître NEUTRAL/OK ; défaut du contrôle encodeur isolé.
- [x] Corriger l'acquisition compatible avec ce défaut, conserver limites et neutre.
  IRQ A/B + clic, file bornée ; validation matérielle des gestes restant à faire.

Aucun port série rouvert pendant ce relevé : son ouverture réinitialise
la carte et ferait perdre l'état du premier défaut. Aucun réarmement lancé.

## Correction acquisition encodeur — photo du premier défaut

- [x] Photo : ENCODER, maître NEUTRAL/OK, Target0 speed0 cmd0,
  fresh8/8, pertes0, TEC0 REC0 ; preuve conservée avant reboot.
- [x] Décoder A25/B26 sur interruptions, mémoriser les crans en file bornée
  et les clics brefs ; conserver les défauts sur transition invalide/overflow.
- [x] Tester rebonds, inversions, plusieurs crans entre deux cycles, clic et
  incohérences ; compiler puis flasher le même profil de banc autorisé.
  Preuve : 7/7 tests, build sans warning, flash/hash vérifiés ; boot12 s
  JOG LIVE READY, cible/commande0, EncErr0 Ov0 et CAN sans erreur.
- [ ] Valider la rotation et le clic matériellement après correction.

## Arrêt au début de rotation : TX ERROR (nouvelle photo)

- [x] Photo : JOG DATA FAULT + maître FAULT/TX ERROR ; EncErr0 Ov0,
  TX1124 RX580, fresh0/8, Loss0 ; mesures invalidées après défaut transport.
- [x] Conserver et afficher la première cause TX détaillée (CTRL/INTF/EFLG,
  TEC/REC et erreur), sans réessai positif ni relèvement de seuil.
- [ ] Vérifier référence/alimentation du transceiver et comportement au démarrage
  moteur avec les informations utilisateur ; pas de nouveau test positif automatique.

Diagnostic TX détaillé compilé (7/7 tests) et flashé dans le même profil.
Aucun réessai positif automatique, aucun seuil modifié ; les registres et la
trame de la première erreur restent figés en RAM jusqu'au redémarrage.
Utilisateur rappelle que le CAN fonctionne : ne pas conclure à une panne
matérielle depuis le seul message générique, diagnostiquer le statut précis.

## Jog : délai demandé de 20 secondes

- [x] Porter le délai sans cran et la durée maximale de session à 20 s.
- [x] Vérifier les bornes temporelles, compiler et flasher le profil jog live.
  Preuve : 7/7 tests, build réussi, flash/hash vérifiés (journal /tmp/bionx-jog-20s-flash.log).
- [ ] Confirmer le comportement sur roue levée après flash.

## Jog : quatre crans et indicateur puissance

- [x] Ajouter quatre niveaux, plafond 4 bruts, petites cibles de vitesse conservées.
- [x] Afficher le registre puissance en pourcentage candidat, masquer si périmé.
- [x] Tests 7/7, build et flash jog live réussis, hash vérifié.
- [ ] Valider les quatre niveaux et l’affichage puissance sur le matériel.

## Jog : maintien cible et filtre émission cohérent

- [x] Corriger le filtre TX resté à 2, qui désarmait aux commandes 3–4.
- [x] Maintien 1 brut à la cible, zéro au-dessus ; conserver arrêt/caps/délai.
- [x] Tests 7/7, build sans warning et flash/hash vérifiés.
- [ ] Confirmer maintien et quatre niveaux sur roue levée.

## Jog : huit crans et diagnostic puissance

- [x] Huit niveaux/plafond8, cible vitesse1–2 conservée, filtre final cohérent.
- [x] Vérifier 0x14, lecture50 ms et affichage brut/pic pendant session.
  Références : CAN-Registers.h et traitement RxData[3] dans Minimal Translator ;
  indicateur %, pas de conversion en watts connue.
- [x] Tests7/7, build sans warning, flash/hash vérifiés du jog live.
- [ ] Confirmer les huit niveaux et les valeurs P14/peak sur le matériel.

## Overspeed et puissance électrique instantanée

- [x] Remplacer maintien positif à cible par coasting/reprise, plafond diagnostic2.
- [x] Lire courant signé1E/1F et calculer U×I sur paires fraîches/cohérentes.
- [x] Afficher Pbat en W (pas puissance mécanique), indisponible si périmé.
- [x] Tests8/8, build sans warning, flash/hash vérifiés du jog live.
- [ ] Confirmer chemin shunt, signe/calibration et régulation physique.

## DATA FAULT rapide après ajout des watts

- [x] Reproduction hôte : réponses11 ms → âge vitesse165 ms (>150 ms),
  réponses16 ms →245 ms ; ordonnanceur circulaire surchargeable par les lectures U/I.
- [x] Prioriser vitesse et finir les groupes U/I ; réduire lectures inutiles au jog.
  Publication U/I sur dernier ensemble complet, curseur équitable séparé.
- [x] Figer le motif détaillé du premier DATA FAULT, sans relâcher les seuils.
- [x] Tests8/8 avec latences1/6/11 ms, build sans warning, flash/hash vérifiés.
- [ ] Vérifier DATA détaillé et stabilité sur matériel ; plafond2 maintenu.

## Acquittement au clic et démarrage roue

- [x] Clic/release acquitte un défaut jog si données saines/roue arrêtée, sans reprise.
- [x] Rampe jusqu’à8 au démarrage, plafond4 en mouvement, coast à cible ; seuil3 conservé.
- [x] Défaut sans mouvement2 s sous commande>=4 ; tests8/8, build sans warning, flash/hash vérifiés.
- [ ] Validation des gestes et rotation sur matériel.

## Palier de vitesse supplémentaire — demande utilisateur

- [x] Cibles1/2/3 aux niveaux1–3/4–6/7–8, coupure survitesse4.
- [x] Conserver commande8/4, coast à cible, clic, données et temporisations.
- [x] Bornes testées (8/8), build sans warning, flash/hash vérifiés du jog live.
- [ ] Confirmer vitesse et stabilité physiques sur roue levée.

## Jog en km/h : dix crans, cible20 et survitesse25

- [x] Configurer roue700×28C, circonférence provisoire2136 mm (CatEye), configurable.
- [x] Dix crans positifs2…20 km/h, zéro arrêt ; survitesse25 km/h estimés.
- [x] Afficher cible/vitesse km/h estimés, préserver tous les autres garde-fous.
- [x] Tests conversions/bornes8/8, build sans warning, flash/hash vérifiés.
- [ ] Mesurer tour réel et valider conversion CAN/vitesse/coupure sur le banc.

## Lecture capteur pédalage et consolidation pour code final

- [x] Utilisateur confirme pilotage moteur fonctionnel sur ce banc (pas validation routière).
- [x] Afficher moteur0x21 brut/max/âge, lecture visée250 ms sans assistance pédale.
- [x] Consolider dans BENCH_FINDINGS.md, lié README/cahier des charges/master/matériel.
- [x] Tests8/8, charge simulée1/6/11 ms, build sans warning, flash/hash vérifiés.
- [x] Retour utilisateur : lecture/affichage capteur « ça marche bien ».
- [ ] Relever valeurs repos/effort et calibrer ; aucun relevé chiffré fourni.

## Audit de passation documentaire — 25 octobre 2026

- [x] Retour utilisateur ajouté : lecture/affichage capteur fonctionne ; calibration distincte.
- [x] Créer HANDOFF.md et AGENTS.md, avec ordre de lecture, profil réel, hash,
  limites, brochage, fichiers utiles, commandes et prochaines mesures.
- [x] Distinguer l’état courant des historiques README/TODO/VALIDATION/JOG/master.
- [x] Restaurer project_requirements.md après écrasement accidentel documentaire :
  quinze sections originales récupérées, état courant ajouté en section16.
- [x] Vérifier onze documents/liens locaux, sections1–16, identité du cahier,
  flags réels, constantes de contrôle, hash et taille du binaire existant.

Documentation uniquement : aucun source/config firmware changé, aucun build,
flash ou mouvement moteur exécuté pendant cet audit.

## Flash V1 calcul à blanc — autorisé le25 octobre2026

Utilisateur : « go, je suis bien roue levée en securité », après proposition
explicite du profil calcul à blanc.
- [x] Recompiler et vérifier ESP32/HUZZAH32, V1_TX désactivé et image.
- [x] Flasher build-assist-v1 sur HUZZAH32, vérifier les empreintes.
- [x] Capturer le boot sans commande utilisateur et consigner les résultats.

Preuve :643664 octets, SHA256 db189abb652b285cfe9bdd558ddd1f16716cc9dfe34817b8a3fd75b49111f5d0,
flash/hash vérifiés ;20 s de capture, init puis NEUTRAL/OK,8/8 mesures fraîches,
commande0, aucune perte. Boot ROM : erreur lecture1000 transitoire puis reset
RTCWDT et démarrage réussi, cause non établie. Validation visuelle/calibration
et rapport ×2 à poursuivre par l'utilisateur ; aucun armement envoyé par l'assistant.

## Diagnostic assistance et watts — 25 octobre2026

Utilisateur ne perçoit pas d'assistance et watts absents. Demande de capture
série continue pendant son pédalage ; port ouvert sans reset volontaire,
mais boot effectivement relancé à l'ouverture.
- [x] Capturer pédale/demande/commande et défauts, conserver le premier motif.
- [x] Identifier le zéro affiché : FF/FF courant→-1 mA→-0,039 W, UI ESP_OK ; diagnostics détaillés flashés.
  Validité physique de ce registre de courant à confirmer sous commande effective.
- [ ] Corriger seulement les causes établies, tester et documenter avant reflash.

## Réponse au coup de pédale — demande pendant diagnostic

Utilisateur confirme absence d'entraînement et demande accélération/décélération
sur une impulsion courte.
- [x] Enveloppe décroissante bornée400 ms et attaque50 ms, plafond8 conservé.
- [x] Arrêt immédiat sur bouton/frein/données périmées/survitesse et filtre TX cohérent.
- [x] Tests impulsion, expiration, coupure et limites ; build/flash puis capture ouverte.
- [ ] Retour physique utilisateur sur la réponse au petit coup de pédale.
