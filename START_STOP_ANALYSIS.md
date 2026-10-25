# Capture locale démarrage / arrêt — 24 octobre 2026

Fichier : `start&stop.csv`, 82 869 octets, SHA-256
`920b684e663ca50424e8291e7a82598f25cea7c67bff257981b4d599eab869f6`.
Contexte utilisateur : console d’origine, démarrage, fonctionnement immobile,
puis arrêt. Interprétations des registres : `BIONXTOOL_ANALYSIS.md`.

## Qualité et portée

1 690 trames, toutes standard et marquées Rx sur bus 0 ; DLC 2 ou 4.
**Tous les horodatages valent zéro** : seule la succession des lignes est
exploitable. Aucune période, durée de démarrage ou latence ne peut être déduite.
Le CSV ne contient ni débit CAN ni champ RTR explicite. Rx décrit le point de
vue du captureur, pas l’identité physique de l’émetteur ; cela ne prouve pas
l’absence d’autres participants actifs.

Rejeu dans le parseur C existant :

```sh
python3 tools/check_cl1000.py 'start&stop.csv'
```

Résultat : 452 SET, 598 QUERY, 598 REPLY et 42 UNKNOWN (0x58/009C),
aucun rejet de format. Les horodatages de rejeu CSV sont synthétiques à zéro :
ce test ne valide pas l’appariement temporel. Le rejeu CL1000 reste identique.

## 1. Démarrage — lignes 2 à 32

| Lignes CSV | Échanges constatés |
|---|---|
| 2–3 | Batterie 0x20=0, puis 0x21=1 (activation vPower selon source tierce) |
| 4–7 | Révisions batterie : matériel 0x33 (51), logiciel 0x69 (105), valeurs brutes |
| 8–10 | Accessoire 0x22=0 ; type batterie 0x3D répond 3 |
| 11–13 | Moteur 0x02=0, 0x41=0, direction 0x42=1 |
| 14–17 | Logiciel moteur 0x20 répond 0x60 (96) ; type capteur 0x6C répond 0 |
| 18–19 | Vitesse moteur 0x11 répond 0 |
| 20–27 | Lecture limites batterie F9=23, FA=28, FB=AF, FC=C8 (hex) |
| 28 | Déverrouillage moteur A5=AA |
| 29–32 | Copie vers moteur 7A=AF, 7B=C8, 7C=23, 7D=28 |

La correspondance de copie est directement observée :
- batterie F9/FA → moteur 7C/7D : 0x2328 = 9 000 brut ;
- batterie FB/FC → moteur 7A/7B : 0xAFC8 = 45 000 brut.

La source tierce les nomme limites charge/décharge. Cette capture démontre
la copie des octets mais ne calibre pas leur unité physique. Ne pas figer ces
valeurs pour une autre batterie. Elle confirme la séquence CAN, pas le signal
électrique qui a préalablement réveillé/alimenté les composants.

## 2. Fonctionnement immobile — lignes 33 à 1688

- 198 lectures couple moteur 0x21, toutes à zéro.
- 396 écritures assistance 0x09, toutes à zéro : exactement deux par lecture
  couple dans cette capture. Aucun exemple d’assistance positive ou de régène.
- 96 lectures vitesse 0x11 et 95 lectures power meter 0x14 : toutes à zéro,
  incluant la première vitesse lue durant le démarrage.
- 11 lectures compteur moteur 0x12 à zéro ; la source le décrit comme remis
  à zéro par la lecture. Notre polling diagnostic continue de l’exclure.
- 11 lectures température moteur 0x16 : 31 brut, candidat 31 °C.
- 43 écritures batterie 0x21=1 au total, dont une au démarrage : activation
  répétée pendant le runtime. Nécessité et timeout non déterminés.
- Jauge batterie 0x61 : 13 brut (candidat ~86,7 %), 22 fois.
- SOC estimé batterie 0x30 : 82 brut (candidat 82 %), 11 fois.
  Les jauges diffèrent : ne pas présenter leurs valeurs comme interchangeables.
- Aucun accès A6/A7 ni BC/D3–D6, contrairement à l’ancien log enrichi de requêtes
  du CL1000. Ces lectures ne sont donc pas requises par la console sur ce runtime.
- 42 trames 0x58, DLC 2, données 00 9C : elles apparaissent également ici.
  Cela renforce l’hypothèse d’un échange habituel du système, sans établir sa
  fonction ni son émetteur. Ne pas le transformer en heartbeat validé.

Les 598 réponses ont chacune un candidat dans l’ordre des lignes par registre
et dernière requête moteur/batterie. Cette observation n’apporte aucun numéro
de transaction et ne supprime pas les ambiguïtés sur un bus avec plusieurs maîtres.

## 3. Extinction — lignes 1689 à 1691

Après des écritures d’assistance nulles et des lectures vitesse/puissance nulles :

1. Moteur 0x20 : `00 41 00 00`.
2. Moteur 0x20 : `00 42 00 01`.
3. Batterie 0x10 : `00 25 00 01`.

Le fichier s’arrête immédiatement après la demande d’extinction batterie.
Avec le contexte utilisateur, la séquence d’arrêt CAN est désormais observée
sur notre système. La coupure électrique effective, son délai et le silence
postérieur ne sont pas mesurables dans ce fichier sans timestamps ni durée
post-capture. Le libellé 0x41 reste incertain dans la source tierce.

## Conséquences pour le firmware

Le parseur actuel accepte tout le format observé, en gardant les 42 trames
0x58 comme UNKNOWN. Aucun correctif de parsing supplémentaire requis.

Pour la phase maître : cette capture fournit une séquence propre à notre
système, les révisions brutes et la preuve de synchronisation des limites.
L’implémentation devra lire les limites réelles, contrôler les réponses et les
expirations, puis copier les octets conformément au profil validé. Une loi
assistance/couple/frein, les délais et le comportement sur perte console restent
à identifier ; le runtime ici est exclusivement neutre.

Prochaine preuve utile : réexporter avec timestamps réels et conserver plusieurs
secondes après la demande d’arrêt. Mesurer séparément le wake électrique.
Aucune modification de la commande moteur ou du firmware à partir de ce log.
