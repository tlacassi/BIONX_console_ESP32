# Trouvailles du banc BionX — base pour le code final

Mis à jour le 25 octobre 2026. Ce bilan décrit le montage HUZZAH32 actuel ;
il ne valide ni le matériel ESP32-C3 cible, ni un usage routier.
Pour reprendre le développement : [HANDOFF.md](../HANDOFF.md).
Les essais détaillés et images flashées figurent dans [VALIDATION.md](VALIDATION.md).

## Résultats physiques rapportés par l’utilisateur

| Résultat | Ce qui est confirmé | Limite |
|---|---|---|
| Réveil/init | Bouton wake externe, console originale déconnectée ; démarrage et données cohérentes rapportés | L’amélioration du wake avec l’ESP est observée, son mécanisme n’est pas prouvé |
| Extinction | Extinction fonctionnelle rapportée | Ne remplace pas une coupure indépendante de puissance |
| Télémétrie | Tension, température et SOC jugés cohérents | Calibration des conversions non réalisée |
| Pilotage moteur | Rotation roue levée via encodeur ; utilisateur confirme explicitement le pilotage fonctionnel | Ni validation roulante ni validation de toutes les protections |
| Capteur pédalage | Utilisateur répond « ça marche bien » après ajout de la lecture/affichage CAN | Aucun relevé brut fourni ; zéro, signe, dynamique et échelle physique à mesurer |
| Watts | Calcul électrique U×I implémenté | Courant/shunt/calibration et comparaison à une mesure indépendante non validés |

Ne pas transformer une compilation, un test simulé ou une commande zéro en
preuve d’arrêt physique. Un défaut logiciel donne une demande de neutre ;
si CAN/ESP ne répond plus, son effet moteur n’est pas établi.

## Montage à conserver comme référence

- Banc actuel : Adafruit HUZZAH32 / ESP32 classique ; le nom historique de
  l’image `bionx_c3_supermini.bin` ne désigne pas la carte réellement flashée.
- Écran GC9A01 sur SPI2 : MOSI18, SCK5, CS14, DC32, RST27.
- MCP2515 sur SPI3 : SI23, SCK4, SO19, CS33, INT34. CAN125 kbit/s,
  quartz déclaré8 MHz, SPI2 MHz. Deux bus séparés ont été retenus.
- Encodeur A25/B26/SW22, commun GND, pull-ups internes, capture par IRQ.
- Wake : bouton externe vers GND déclaré par l’utilisateur ; GPIO21 inutilisé.
- Frein prévu mais non raccordé pendant ces essais. Coupure haute tension
  indépendante accessible confirmée par l’utilisateur, pas testée par l’ESP.
- Pneu déclaré700×28C. Circonférence configurée2136 mm, valeur de table
  provisoire, pas mesure réelle. Chemin du courant via shunt BionX supposé
  d’après la réponse utilisateur ; à confirmer explicitement avant calibration.

Voir [HARDWARE.md](HARDWARE.md) pour le détail et l’historique du câblage.
La référence exacte du transceiver et ses caractéristiques électriques ne
sont pas devenues connues simplement parce que des échanges CAN ont réussi.

## Protocole et conversions retenus

Le maître découvre par requête batterie0x10 registre3B, une fois par seconde.
Seule une réponse attendue et à temps déclenche l’init : une trame quelconque
ne suffit pas. Profil observé : batterie HW33/SW69/type3, moteur SW60/sensor0.
Init et arrêt suivent la capture locale [start&stop.csv](../captures/start&stop.csv).
Les réponses sur ID08 n’ont pas d’identifiant de transaction : conserver une
seule lecture en attente, le registre attendu et la fenêtre temporelle.

| Lecture/commande | Utilisation | Statut de l’échelle |
|---|---|---|
| Moteur20/09 | Consigne positive répétée, deux trames par période50 ms | Fraction brute/64 décrite dans les références ; pas un couple, courant ou wattage garanti |
| Moteur20/11 | Vitesse | Facteur candidat9,091 rpm/brut ; km/h = rpm × circonférence_mm ×0,00006 |
| Moteur20/21 | Indicateur de couple/pédalage | Affichage brut ; facteur tiers1,5625 % non calibré, aucune conversion N ou Nm |
| Moteur20/14 | Indicateur de puissance | Ce registre restait à0 pendant les essais rapportés ; ne pas le convertir arbitrairement en watts |
| Moteur20/16 | Température | Conversion candidate1 °C/brut |
| Batterie10/A6,A7 | Tension batterie | Paire HI/LO, mV candidats |
| Batterie10/1E,1F | Courant shunt | Paire16 bits signée, mA candidats |
| Pbat | Tension × courant | Puissance électrique batterie, pouvant inclure les auxiliaires ; pas puissance mécanique à la roue |

Une mesure multi-octets doit être publiée comme ensemble complet. Pour Pbat,
conserver ensemble la tension et le courant utilisés ; une nouvelle tension
ne remplace pas la moitié d’un ancien calcul. Âge maximal500 ms, écart HI/LO
<100 ms, décalage U/I≤200 ms actuellement. Une donnée absente/périmée n’est
pas un zéro mesuré. L’affichage `?` ou `~` signale les conversions à calibrer.

Sources de protocole analysées, pas reprises aveuglément :
[Minimal Translator](https://github.com/stancecoke/BionX_Minimal_Translator)
(d8f4fbb28d16435d4dd8182539962dab29c3d82b),
[BionxConsole](https://github.com/inarighas/BionxConsole)
(41b06f7d1fc50b60cae9123ee55e801b9e330f54).
Voir [ASSIST_BENCH.md](ASSIST_BENCH.md) et [BIONXTOOL_ANALYSIS.md](BIONXTOOL_ANALYSIS.md).
Ces références calculent une demande côté console ; elles ne prouvent pas
le comportement de sécurité du contrôleur moteur sur perte de CAN.

## Défauts rencontrés et leçons à garder

- **ENCODER** avec CAN sain : le polling A/B pouvait manquer des transitions.
  IRQ, décodage Gray, file bornée de crans et mémorisation du clic bref ont
  remplacé cette acquisition. Rebonds, inversions, overflow et transitions
  incohérentes sont couverts par les tests ; une incohérence reste bloquante.
- **Arrêt sans erreur** après ajout de crans : le filtre final était resté à2
  alors que le contrôleur pouvait demander4. Partager les plafonds entre
  contrôleur, configuration maître et autorisation juste avant émission.
- **DATA FAULT** peut être secondaire à un défaut transport. Conserver la
  première erreur TX, la trame et CTRL/INTF/EFLG/TEC/REC au lieu de déduire une
  panne matérielle du seul message générique. Pas de réessai positif automatique.
- **DATA FAULT sous charge télémétrie** reproduit sur hôte : tour circulaire
  de dix lectures et réponses11 ms → vitesse âgée165 ms, au-delà du seuil150.
  Priorité vitesse, curseur équitable séparé, groupe U/I et publication complète
  corrigent les scénarios simulés1/6/11 ms. Cela ne garantit pas tous les délais
  physiques ; une expiration réelle doit encore couper.
- **OVERSPEED** après maintien positif à la cible : un couple fixe à la cible
  peut encore accélérer une roue déchargée. La cause exacte du dépassement
  rapporté n’a pas été capturée. Maintien imposé retiré ; commande zéro à cible,
  reprise en dessous, sans régène. La stabilité réelle reste à caractériser.
- **Commande trop faible** : des niveaux affichés différents étaient équivalents
  sous un plafond diagnostic2/64. Distinguer la cible vitesse de la commande
  brute et afficher les deux ; le niveau affiché n’est pas la puissance moteur.

## Profil de banc courant

`build-encoder-jog/sdkconfig-live`, overlay `sdkconfig.encoder-jog-live`.
Zéro au boot, dix crans positifs2…20 km/h estimés, niveau0 arrêt, survitesse25
km/h estimés. Commande progressive1 brut/200 ms, maximum8 au démarrage et4
en mouvement ; zéro à cible. Clic coupe ; sur défaut jog, clic/relâchement
acquitte uniquement si roue arrêtée, données saines et encodeur cohérent.
Nouvelle rotation obligatoire, pas de départ sur l’acquittement. Un défaut
CAN/MCP persistant reste bloquant. Aucun acquittement différé si refus.

Durée20 s sans action et20 s maximum/session ; consigne moteur valable100 ms.
Absence de vitesse détectée2 s sous commande≥4 : NO MOTION. Vitesse fraîche
<150 ms, P14<500 ms, température/tension<2 s ; autres bornes dans le code et
[JOG_BENCH.md](JOG_BENCH.md). Les seuils physiques restent candidats.

Affichage pédalage : `Ped raw`, maximum depuis boot et âge ; lecture visée250 ms,
`stale` à1 s. Cet indicateur ne déclenche aucune assistance au pédalage.
Les tests hôte vérifient aussi que cette charge ne dégrade pas les scénarios
simulés de vitesse/U/I. L’utilisateur confirme le bon fonctionnement de cette lecture/affichage ;
les relevés chiffrés repos/effort et la calibration restent à effectuer.

## À reprendre et à mesurer pour le code final

Réutiliser le protocole séparé du transport, les machines à états, l’expiration
des commandes, les snapshots cohérents, les IRQ encodeur, les diagnostics
figés et les tests. Les durées et commandes du jog sont des choix de banc,
pas une spécification automatique de l’assistance finale.

Restent à faire : calibration vitesse/circonférence et couple (zéro sans charge,
réponse à un effort connu, signe et gain), mesure indépendante U/I, chemin du
shunt, frein réel et coupure matérielle, comportement/watchdog moteur sur perte
CAN ou reset ESP, régulation stable et essais de charge. Établir explicitement
les plafonds physiques avant le contrôle d’assistance en circulation.
