# Apports de BionXtool

Analyse du 23 octobre 2026, référence : commit
[f8cca39cb3cb8deab9f929d769c8209f23de8a4d](https://github.com/buxtronix/BionXtool/commit/f8cca39cb3cb8deab9f929d769c8209f23de8a4d).
Lecture de README, PROTOCOL.md, registers.h, BionXtool.c et début de
bike-on-off.txt. Aucun code externe importé, aucune émission CAN effectuée.

Sources :
- [Registres](https://github.com/buxtronix/BionXtool/blob/f8cca39cb3cb8deab9f929d769c8209f23de8a4d/registers.h)
- [Protocole et séquences](https://github.com/buxtronix/BionXtool/blob/f8cca39cb3cb8deab9f929d769c8209f23de8a4d/PROTOCOL.md)
- [Implémentation](https://github.com/buxtronix/BionXtool/blob/f8cca39cb3cb8deab9f929d769c8209f23de8a4d/BionXtool.c)
- [Capture démarrage/arrêt décodée](https://github.com/buxtronix/BionXtool/blob/f8cca39cb3cb8deab9f929d769c8209f23de8a4d/bike-on-off.txt)

## Correspondances avec notre capture

Ces significations proviennent du projet tiers. Elles fournissent des hypothèses
documentées pour notre matériel, pas une calibration ou validation sur banc.

| Cible | Registre | Sens et conversion indiqués par registers.h |
|---|---|---|
| Moteur 0x20 | 0x09 | Assistance signée, facteur 1,5625 % ; 0x40 correspondrait à 100 % |
| Moteur | 0x11 | Vitesse de rotation, facteur 9,091 rpm ; conversion km/h avec circonférence roue |
| Moteur | 0x12 | Révolutions depuis dernière lecture, remise à zéro à la lecture |
| Moteur | 0x14 | Indicateur de puissance, facteur 1,5625 %, pas des watts |
| Moteur | 0x16 | Température, °C |
| Moteur | 0x21 | Signal capteur de couple, facteur 1,5625 %, pas directement des N·m |
| Moteur | 0x72/73 | Tension 12 V en deux registres, facteur 0,001 V |
| Moteur | 0x41 | Limite 3 km/h, implémentation incertaine dans le commentaire source |
| Moteur | 0x42 | Direction d’assistance |
| Batterie 0x10 | 0x21 | Activation vPower ; commentaire source encore interrogatif |
| Batterie | 0x25 | Demande d’extinction par écriture de 1 |
| Batterie | 0x30 | SOC estimé à partir de tension, en % |
| Batterie | 0x61 | Jauge grossière, facteur 6,6667 % |
| Batterie | 0xBC | SOC du gas gauge, % |
| Batterie | 0xA6/A7 | Tension batterie, deux octets répartis entre registres, facteur 0,001 V |
| Batterie | 0xD3/D4 | Courant gas gauge, facteur indiqué 0,002141 A ; signe à valider |
| Batterie | 0xD5/D6 | Capacité LMD, facteur 0,002142 Ah |

Exemples de notre log : réponse 0x61=0x0F compatible avec une jauge à ~100 %,
et 0xBC=0x5A avec 90 %. Cela illustre des estimateurs distincts ; ne pas les
fusionner silencieusement. La sémantique tierce rend plausible l’interprétation
d’arrêt de notre fin de log (0x41=0, 0x42=1 puis batterie 0x25=1), sans prouver
une coupure électrique : des échanges restent enregistrés ensuite.

## Nouveaux éléments pour l’architecture

Le projet distingue adresses normales batterie/moteur 0x10/0x20 et adresses
0x50/0x60 utilisées par le diagnostic ; console esclave 0x48, retour diagnostic
0x58. La console peut être forcée esclave via son registre 0xD1=1.
Notre parseur limité à 0x08/0x10/0x20 ne couvre donc pas tout ce mode diagnostic.
Notre diagnostic normal n’est pas nécessairement erroné : il suit la voie
console normale et attend 0x08, alors que getValue() de BionXtool attend 0x58.

**0x58/009C reste non résolu** : BionXtool nomme 0x58 BIB (et réponse console),
mais ses réponses documentées font 4 octets. Nos 575 trames ont 2 octets.
Le décodeur tiers classifie toute trame DLC 2 comme QUERY : ce seul comportement
ne prouve ni l’origine ni la fonction de notre trame. Ne pas conclure que c’est
une lecture de température batterie 0x9C simplement parce que ce registre existe.

La séquence de démarrage du projet comporte identification des composants,
activation batterie, initialisation moteur et synchronisation des limites de
courant batterie vers moteur (batterie F9–FC, moteur 7A–7D). Sa capture contient
aussi les doubles écritures d’assistance 0x09 vues dans notre log.
La documentation ne fournit pas la loi complète assistance/couple/frein, ni le
comportement moteur sur perte console. Le wake CAN décrit suppose déjà une
électronique alimentée : cela n’identifie pas notre réveil électrique.

## Points à conserver ou corriger dans notre future implémentation

- Les valeurs fonctionnelles courantes sont décrites comme des octets :
  `[00, registre, 00, valeur]`. Les mesures multioctets se reconstruisent via
  plusieurs registres HI/LO. Notre champ raw 16 bits conserve correctement les
  données mais ne doit pas devenir une conversion physique 16 bits générique.
- Conserver attribution de réponse candidate et expiration. decodeFrame() tiers
  utilise la dernière cible par registre et une heuristique de dictionnaire sans
  expiration : insuffisant pour piloter le moteur sur une capture mêlant maîtres.
- Traiter séparément valeur zéro et erreur : getValue() tiers retourne zéro dans
  les deux cas. Notre télémétrie doit conserver validité et âge.
- Prévoir signe explicite pour assistance/régène et courant. Le formateur tiers
  F_PCT_ASSIST multiplie une valeur non signée malgré la mention signed du 0x09.
- Utiliser registers.h et le code avec prudence : PROTOCOL.md simplifie certaines
  unités (rpm sans facteur pour 0x11, tension normalisée indiquée en % alors que
  le header et getVoltageValue() proposent une autre conversion).
- Déverrouillage différent selon nœud : moteur A5, clé AA et fenêtre annoncée
  30 s ; batterie 71, valeur correspondant à une durée. Ne pas généraliser la
  clé moteur à la batterie. Aucune de ces écritures ajoutée à notre firmware.

BionXtool est un outil Linux/SocketCAN de diagnostic, pas un firmware complet
remplaçant la console. Ses captures et sa table sont utiles à notre phase 2,
mais le transport MCP2515 TX, contrôle, frein et essais restent nécessaires.

## Mise en œuvre ultérieure

Le [maître neutre pour le banc](MASTER_BENCH.md) implémente maintenant le TX
MCP2515, les séquences et la télémétrie candidate. Les constats ci-dessus sur
leur absence décrivent l’état avant cet incrément. Les validations matérielles
et la liaison du contrôleur à une commande non nulle restent à réaliser.
