# Cahier des charges — Console BionX sur ESP32-C3 Super Mini

Version 1.0 — 23 octobre 2026

## 1. But et usage de ce document

Développer une console de vélo BionX personnalisée sur ESP32-C3 Super Mini : pilotage de l’assistance et de la régénération, lecture de la batterie et du moteur, affichage et odomètre. Prévoir les évolutions Wi-Fi et graphiques sans réécrire le cœur de contrôle.

Ce fichier décrit la cible fonctionnelle, pas des fonctions déjà présentes. Codex doit lire le dépôt réel et ses éventuels AGENTS.md avant de modifier le code. Avancer par incréments compilables et vérifiables. Ne pas interpréter ce document comme une demande de tout implémenter en une seule passe.

Les exigences marquées « confirmé » proviennent des choix du projet. Les valeurs « proposées » sont des points de départ à mesurer. Les éléments « à confirmer » ne doivent pas devenir des constantes présentées comme validées.

## 2. Décisions et inconnues

| Élément | État | Décision / contrainte |
|---|---|---|
| Carte | Confirmé | ESP32-C3 Super Mini ; vérifier la variante réelle |
| Framework | Confirmé | ESP-IDF natif, extension officielle ESP-IDF dans VS Code |
| Langage | Choix de départ | C pour le cœur ; C++ autorisé pour les bibliothèques qui le nécessitent |
| Arduino | Option | Ajouter comme composant uniquement si une dépendance précise le justifie |
| Moteur | Confirmé / référence à préciser | Moteur BionX avec contrôleur de puissance conservé dans le moteur |
| Console originale | Confirmé | Remplacée par notre ESP32 ; le projet ne remplace pas l’onduleur du moteur |
| Batterie | Confirmé / caractéristiques à préciser | Pack 10S ; chimie, capacité utilisable, tensions et courants à documenter |
| BMS actuellement ajouté | Déclaré par l’utilisateur | DALY basique 10S sans communication ; référence et fonctions exactes à vérifier |
| Électronique BionX batterie | Transition | Conservée pour les premiers essais si nécessaire ; retrait envisagé ensuite |
| Futur BMS communicant | Non choisi | DALY Smart / autre envisagé, aucun achat ni protocole exact confirmé |
| Courant batterie | Besoin annoncé | Environ 30 A ; distinguer continu, pointe et courant admissible des cellules |
| Écran | Intention | Petit écran rond, envisagé en OLED I2C ; contrôleur, résolution et bus non confirmés |
| Commande utilisateur | Intention | Encodeur rotatif et bouton ; référence et nombre de crans à préciser |
| CAN | Base de travail | CAN classique 125 kbit/s d’après les projets étudiés ; confirmer sur le vélo |
| Flash | Hypothèse de configuration | 4 Mio ; contrôler sur la carte reçue |

Ne pas imposer un écran SPI, LVGL ou un BMS particulier sans besoin établi. Un écran monochrome simple peut utiliser une bibliothèque plus légère que LVGL.

## 3. Périmètre et priorités

### P0 — Socle et observation

- Projet compilable pour `esp32c3`, configuration reproductible et console USB.
- Écoute passive CAN par défaut, capture horodatée, compteurs d’erreurs et pertes.
- Identification des composants et validation des registres sans commande motrice.
- Tests de décodage et limites sur ordinateur quand pertinents.

### P1 — Console locale et contrôle validé

- Mesures moteur/batterie avec validité et date de dernière réception.
- Réglage de niveaux d’assistance et de régène, démarrage en mode neutre.
- Gestion du frein et des défauts, limitation et surveillance des commandes.
- Écran, encodeur, distance de trajet et odomètre persistant.
- Une seule variante batterie opérationnelle suffit pour le premier incrément utilisable.

### P2 — Évolutions

- BMS communicant alternatif et retrait de l’électronique BionX lorsque validé.
- Estimation SOC par mesure tension/courant si le BMS ne fournit pas de jauge exploitable.
- Wi-Fi pour diagnostic/configuration, puis OTA et graphismes enrichis.

Hors périmètre initial : application mobile dédiée, cloud obligatoire, modification du firmware interne moteur, hausse des limites constructeur, contournement des protections batterie et conception d’un BMS maison.

## 4. État initial à ne pas surestimer

La base préparée contient une tâche de diagnostic TWAI, des lectures optionnelles, une console USB, un plan de GPIO et des partitions réservées à une future OTA. Elle ne contient pas encore le pilotage moteur, les drivers écran/encodeur/BMS, le SOC, l’odomètre, le réseau ou le service OTA.

La compilation cible et les essais matériels n’étaient pas exécutés au moment de rédaction. Codex doit d’abord vérifier cet état et les API de la version d’ESP-IDF réellement installée. Une inspection du JSON ou du partitionnement ne remplace pas un build.

## 5. Architecture attendue

Séparer les responsabilités par modules ; ne pas créer une tâche par petit module sans nécessité :

| Module logique | Responsabilité |
|---|---|
| board / configuration | Broches, variante carte, options et paramètres validés |
| can_transport | Driver TWAI, RX/TX bornés, état bus et compteurs |
| bionx_protocol | Encodage/décodage sans dépendance à l’écran ni au réseau |
| battery | Interface commune, validité et capacités ; backend BionX ou futur BMS |
| control | Machine à états, limites, assistance, régène et arbitrage frein |
| inputs | Encodeur, bouton, frein et anti-rebond |
| ui | Affichage, menus, animations et indication des défauts |
| odometer / storage | Distance, persistance versionnée et récupération |
| network / ota | Options futures, isolées du contrôle |
| diagnostics | Logs bornés, statistiques, raisons d’arrêt et version firmware |

Le C3 est monocœur : toutes les tâches partagent le CPU. Aucune écriture directe d’état moteur depuis l’UI ou le réseau. Transmettre des intentions via des queues bornées ; le contrôle applique les limites. Échanger les mesures via snapshots cohérents, queues ou synchronisation courte. Une UI lente, une connexion Wi-Fi ou un client de diagnostic ne doit pas bloquer le contrôle ni saturer le CAN.

Réserver l’allocation dynamique à l’initialisation autant que possible. Les piles réseau et bibliothèques peuvent allouer : mesurer leur coût et traiter les échecs plutôt que promettre une absence absolue de heap.

## 6. Interfaces matérielles

Plan proposé, modifiable dans menuconfig et à confronter au schéma de la carte :

| Fonction | GPIO proposé |
|---|---:|
| TWAI TX vers TXD du transceiver | 4 |
| TWAI RX depuis RXD du transceiver | 5 |
| Écran I2C SDA / SCL | 0 / 1 |
| UART ESP TX / RX vers BMS RX / TX | 21 / 20 |
| Encodeur A / B | 6 / 7 |
| Bouton encodeur | 10 |

GPIO18/19 restent affectés à l’USB. Éviter GPIO2/8/9 pour les périphériques dans ce profil de départ. Le frein n’a pas encore de GPIO réservé : il faut résoudre ce point avant d’autoriser l’assistance, éventuellement réviser le plan ou ajouter un expandeur pour les seules entrées non critiques.

Un transceiver CAN externe est requis ; vérifier alimentation, compatibilité logique 3,3 V, référence de masse et terminaisons du bus. Vérifier les niveaux et la référence de masse du BMS avant de connecter l’UART. UART TTL, RS485 et CAN ne sont pas interchangeables. La tension pack n’est jamais appliquée directement à la carte.

Prévoir les limites de puissance, tension et température dans une configuration explicite, sans reprendre automatiquement les valeurs d’une autre batterie.

## 7. Protocole BionX

- Séparer écoute seule, lectures actives et commande ; démarrer en écoute seule.
- Refuser les trames mal formées : identifiant, format standard/étendu, RTR, DLC, registre et plage de valeur.
- Documenter chaque registre utilisé : source, unité, signe, échelle, endian, profil matériel et preuve observée.
- Les réponses peuvent partager l’identifiant 0x08 sans numéro de transaction. Une requête à la fois réduit l’ambiguïté sans l’éliminer. Ne pas attribuer une réponse tardive ou issue d’une autre console avec une certitude inventée.
- Un seul maître actif pendant la validation. Expirer les requêtes et mesures ; traiter les réponses inattendues sans modifier la commande.
- Borner temps d’attente, débit de requêtes et taille des files. Un bus muet ne doit pas provoquer de boucle active ou de blocage permanent.
- Documenter la séquence de mise sous tension/réveil/initialisation/arrêt pour chaque profil réellement pris en charge.
- Le réveil électrique et le déverrouillage de registres sont distincts. Aucune écriture protégée ou persistante non justifiée.
- Valider le comportement autonome du moteur si la console cesse d’émettre ; envoyer zéro ne suffit pas si le bus est déjà coupé.

Sources de départ à relire et confronter aux captures :
- https://github.com/stancecoke/BionX_Minimal_Translator
- https://github.com/inarighas/BionxConsole
- https://github.com/buxtronix/BionXtool

Conserver licences et attributions ; ne pas copier de code à licence incompatible. Enregistrer le commit source utilisé lors d’une reprise de code.

## 8. Contrôle et machine à états

États suggérés : INITIALISATION, DIAGNOSTIC, ATTENTE_COMPOSANTS, PRET_NEUTRE, ASSISTANCE, REGENERATION, DEFAUT, ARRET. Les transitions doivent être explicites et testables. Une autre organisation équivalente est acceptable si les conditions sont documentées.

### Assistance

- Niveaux proposés : 0 à 4, valeurs exactes et courbes configurables après validation.
- Un niveau représente une consigne d’assistance, pas un pourcentage de watts garanti.
- Déterminer et valider la source de demande conducteur : capteur de couple BionX, cadence ou autre signal identifié. Ne pas transformer un niveau choisi à l’écran en accélération constante.
- Pas de commande positive avant identification des composants requis, mesures valides, entrée frein validée et armement explicite pour le banc.
- Appliquer saturation, rampes et limites électriques/thermiques et de vitesse pertinentes, avant encodage CAN.
- Au reset : consigne neutre ; ne jamais restaurer une commande de couple active depuis la NVS.

### Frein et régénération

- Le frein inhibe immédiatement toute demande d’assistance au prochain cycle de contrôle.
- Une entrée frein absente ou non validée bloque l’autorisation d’assistance initiale.
- Niveaux de régène proposés : 0 à 4, réglables indépendamment de l’assistance.
- La régène doit respecter les limites de recharge du pack, les tensions cellules disponibles, la température et les capacités moteur/BMS.
- Désactiver la régène si la charge est interdite ou si les informations nécessaires à sa limitation sont indisponibles/périmées ; définir un profil conservateur validé si le BMS ne communique pas.
- Vérifier le retour de courant sur le port moteur et le comportement lors d’une coupure du BMS. La coupure matérielle ne doit pas être le mécanisme normal de fin de régène.

### Défauts

- Perte CAN, bus-off, BMS indisponible, mesure critique expirée, valeur impossible ou défaut batterie : inhiber les commandes non nulles selon une politique documentée.
- En cas de liaison disponible, demander le neutre ; en cas de rupture, s’appuyer sur un comportement moteur préalablement mesuré et validé.
- Après récupération du bus : rester au neutre et exiger un réarmement explicite, sans retour automatique à la précédente commande.
- Aucune dépendance au Wi-Fi ou à un téléphone pour arrêter l’assistance.

## 9. Batterie et SOC

Prévoir une interface batterie exposant au minimum : tension pack, courant signé, SOC, température, défauts et autorisation de charge/décharge lorsqu’ils sont disponibles. Pour chaque champ : valeur, unité, validité, âge et origine. Ajouter tensions cellules min/max et écart quand disponibles.

Trois profils possibles, à implémenter uniquement au besoin :
1. Batterie/électronique BionX d’origine : télémétrie et démarrage documentés.
2. BMS communicant alternatif : protocole exact et révision documentés.
3. BMS autonome sans communication : mesure externe tension/courant ; protections cellules assurées par le matériel, pas par l’estimation de l’ESP32.

Ne pas mélanger des télémétries de deux backends ni passer silencieusement d’un profil à l’autre.

Pour un SOC local : intégrer le courant bidirectionnel (consommation et régène/recharge), capacité utilisable configurable, convention de signe unique, calibration d’offset et compensation du temps réel. Réinitialiser proprement l’intégration après un trou de mesure. Recaler à pleine charge sur des critères confirmés ; ne pas déduire 100 % d’un seul échantillon de tension.

La tension au repos peut fournir une estimation initiale propre à la chimie, avec incertitude explicite. La tension en charge ne constitue pas une jauge précise. Si les charges hors tension ESP32 ne sont pas mesurées, invalider/recaler le SOC au redémarrage au lieu de conserver un chiffre supposé exact.

Calculer la puissance batterie en watts à partir de V × A avec convention de signe documentée. Ne pas afficher directement un « power meter » moteur en watts sans conversion validée.

## 10. Affichage et entrées

Écran principal : vitesse en km/h, SOC en %, niveau d’assistance, niveau de régène, état frein/défaut, puissance si valide et distance du trajet. Écran secondaire : odomètre, tension, courant, température et diagnostic des liaisons selon disponibilité.

Une valeur absente/périmée doit être indiquée par `--` ou un état explicite, jamais remplacée par un zéro plausible. Les avertissements restent visibles malgré les animations. Les écritures écran sont bornées et n’occupent pas la tâche de contrôle.

Encoder : anti-rebond et décodage quadrature, bornage des niveaux, détection clic/appui long sans blocage. Proposer le comportement des menus lorsque l’écran et l’encodeur sont choisis. Une erreur d’entrée ne peut pas produire un niveau hors plage.

LVGL est une option pour une interface riche ; choisir bibliothèque, buffers, fréquence et éventuel rafraîchissement partiel après identification de l’écran. Éviter les grandes images et allocations par frame sur C3.

## 11. Vitesse, distance et persistance

- Documenter la conversion de la vitesse brute moteur ; circonférence de roue configurable et mesurée.
- Intégrer la distance avec un temps monotone et une résolution suffisante, idéalement un compteur entier en unité fine.
- Ne pas accumuler de distance avec une vitesse périmée, un intervalle non couvert ou un saut invraisemblable.
- Distinguer distance trajet réinitialisable et odomètre total non réinitialisable dans l’interface normale.
- Stockage NVS versionné, contrôles de cohérence, traitement des erreurs et stratégie compatible avec l’endurance flash.
- Proposer puis mesurer la périodicité de sauvegarde et la perte maximale en cas de coupure ; ne pas écrire à chaque cycle.
- Ne pas garantir une sauvegarde à l’extinction sans détection de coupure et réserve d’énergie matérielles.
- Ne pas effacer automatiquement toute la NVS en cas d’erreur ; conserver un chemin de récupération explicite.

## 12. Temps réel et ressources

Valeurs de départ proposées, à confirmer par captures et mesures : contrôle toutes les 10 ms, UI entre 5 et 20 Hz selon écran, télémétrie batterie de l’ordre de 1 à 5 Hz selon protocole. La cadence des commandes moteur doit suivre le protocole validé, pas une valeur arbitraire.

Les délais d’expiration sont propres aux signaux et dérivés des périodes observées avec une marge documentée. Utiliser une horloge monotone ; gérer débordements et conversions d’unités.

Mesurer : heap libre et minimum, taille image, marge de piles, temps du cycle de contrôle, occupation CAN et pertes RX. Les logs doivent être limités en production. Aucune réception CAN dans une ISR avec printf ou opération bloquante. Pas de core pinning supposant deux cœurs.

## 13. Wi-Fi et OTA : préparer sans surcharger le MVP

- Fonctions optionnelles, désactivées au départ.
- Wi-Fi pour consulter les mesures, exporter des diagnostics et configurer les paramètres autorisés.
- Pas d’endpoint réseau de couple brut ; les demandes passent par le contrôle et ses vérifications.
- Secrets absents des sources et logs ; accès aux paramètres protégé et configuration initiale explicite.
- Déconnexion ou panne réseau sans impact sur le contrôle local.
- OTA uniquement vélo immobile, commande inhibée et alimentation jugée suffisante.
- Deux slots réservés ne constituent pas une implémentation OTA. Prévoir validation d’image, politique d’authenticité et récupération/rollback après échec d’amorçage avant activation.
- Revoir la taille des partitions avec le binaire réel ; documenter migrations et compatibilité des paramètres persistants.

## 14. Validation et critères d’acceptation

| Étape | Preuve attendue |
|---|---|
| Build | Build ESP32-C3 réel, version exacte d’ESP-IDF et commande consignées |
| Démarrage | Console USB, reset répété, aucun ordre moteur spontané |
| Observation | Capture horodatée, absence d’émission en mode passif |
| Décodage | Tests de trames valides, DLC incorrect, RTR, mauvais ID, signe/endianness et données hors plage |
| Contrôle | Tests états/transitions, saturations, frein prioritaire, expiration et absence d’autorisation |
| Panne liaison | Bus silencieux/coupé, bus-off et retour liaison : arrêt et réarmement vérifiés |
| Batterie | Défauts, charge interdite, SOC élevé et mesure invalide inhibent la régène selon politique |
| Odomètre | Test d’une distance connue, coupures et récupération persistante sans saut aberrant |
| UI | Entrées rapides, anti-rebond, périphérique absent sans blocage contrôle |
| Charge CPU | UI/réseau sollicités sans dépassement non traité du budget contrôle |
| Banc | Premiers ordres non nuls roue levée, limitation conservatrice et arrêt accessible |

Les tests automatisés doivent couvrir les calculs et décisions, sans reproduire simplement le code. Pour le matériel absent, fournir une procédure et marquer « non exécuté ». Ne jamais qualifier le système de validé pour rouler sur la seule base des tests hôte.

## 15. Ordre de travail pour Codex

1. Lire ce document, AGENTS.md éventuel, README et code ; vérifier l’état réel du dépôt et les dépendances.
2. Compiler le socle actuel sur ESP32-C3 ; corriger seulement les blocages pertinents et consigner les outils exacts.
3. Isoler le protocole du transport, ajouter tests utiles et diagnostics ; rester en écoute seule.
4. Identifier les références manquantes et capturer le vélo ; continuer les travaux indépendants de ces inconnues.
5. Implémenter télémétrie et profil batterie choisi, avec validité et expiration.
6. Implémenter machine à états et limitations testables avant toute commande non nulle.
7. Valider le frein, les arrêts et le comportement moteur sur perte console, puis essais au banc.
8. Ajouter UI/encodeur et odomètre ; vérifier les ressources en charge.
9. Ajouter BMS alternatif, Wi-Fi et OTA comme incréments distincts lorsque demandés.

À chaque incrément : résumer les modifications, les tests exécutés, leurs résultats et les limites restantes ; mettre à jour le README. Choisir les détails logiciels réversibles de manière autonome. Demander une information seulement si elle bloque une décision matérielle/protocolaire précise, sans inventer la réponse ni suspendre les autres travaux.

Livrables attendus : sources, configurations, dépendances verrouillées lorsque pertinentes, tests utiles, câblage confirmé, procédure build/flash et rapport de validation. Ce cahier des charges complète les instructions du dépôt ; le nom du fichier ne garantit pas sa lecture automatique par Codex. Le référencer explicitement dans le prompt de démarrage ou dans AGENTS.md.

## 16. Acquis du banc à reprendre — 25 octobre 2026

Lire [HANDOFF.md](HANDOFF.md) pour l’image réellement chargée, les commandes
et le point de reprise, puis [BENCH_FINDINGS.md](docs/BENCH_FINDINGS.md) pour les
observations, correctifs et inconnues. Les sections1–15 définissent la cible
fonctionnelle et décrivent aussi l’état initial ; elles ne constituent pas
un statut matériel courant.

L’utilisateur confirme le pilotage moteur sur banc HUZZAH32, roue levée,
puis le bon fonctionnement de la lecture/affichage du capteur moteur0x21.
Ce jalon ne valide ni les critères routiers ni le portage ESP32-C3.
Aucun relevé chiffré du capteur n’a été fourni : zéro, signe, dynamique et
échelle restent à mesurer. Une valeur brute/% ne doit pas être présentée
comme force en N ou couple en Nm. La puissance électrique batterie U×I
n’est pas la puissance mécanique du moteur. Ne pas recopier les paramètres
jog dans le contrôle final sans traiter calibration, frein et perte de CAN.

## 17. Incrément V1 demandé le25 octobre2026

Sur le banc HUZZAH32 réellement utilisé : découverte et initialisation CAN
automatiques au boot, interface ronde d'origine alimentée par données réelles,
un mode pédale ×2 et aucune régénération. Le rapport ×2 porte sur le signal
brut corrigé du zéro ; un doublement physique de couple ne peut pas être
affirmé sans calibration. Calibration à vide et armement local restent explicites
pour cette première version expérimentale. Puissance affichée = U×I batterie,
commande CAN séparée ; aucune puissance mécanique inventée depuis le niveau.
Procédure, saturation/rampe, profils calcul à blanc/émission et limites :
[ASSIST_V1](docs/ASSIST_V1.md). Les exigences routières restent ouvertes.

## 18. Mise à jour V1 automatique demandée le25 octobre2026

Utilisateur confirme le boot, l'interface, SOC et vitesse. Il demande
zéro/armement sans clic, anneau de puissance moteur, petite barre couple et
activation de l'assistance sur le banc roue levée. La calibration automatique
attend16 mesures distinctes, stables et basses à l'arrêt puis arme au neutre ;
les pédales doivent être déchargées. Stops/défauts restent bloquants, sans
reprise automatique. Anneau = indicateur moteur14 candidat, barre = couple21
brut, commande et watts batterie séparés. Plafond8/64 et absence de régène
conservés. Cette demande remplace l'armement manuel au boot de la section17,
sans transformer les limites physiques encore inconnues en validations.
