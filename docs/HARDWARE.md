# Matériel et câblage proposé — 23 octobre 2026

## Lecture du document

Les sections sont historiques. Le montage actuel est le banc HUZZAH32 à deux
SPI décrit plus bas ; le wake externe est installé et le pilotage moteur sur
roue levée est confirmé par l’utilisateur. Voir [BENCH_FINDINGS.md](BENCH_FINDINGS.md)
pour la synthèse courante et les inconnues encore ouvertes.

## Identifié ou déclaré

- ESP32-C3 Super Mini, flash 4 Mo identifiée par esptool.
- Écran GC9A01 SPI 240×240, câblage ci-dessous déjà utilisé par le dashboard.
- Moteur BionX 350 W et fonctionnement avec console G1 : déclarés par
  l’utilisateur ; modèle exact et version du contrôleur non identifiés.
- Électronique batterie BionX conservée : photos avec marquages
  `SMART CONNECT 5.1`, `SMC Rev5.1`, `2008-10`.
- MCP2515 choisi pour la liaison CAN : module précis, transceiver, alimentation
  et quartz non encore identifiés.
- Liaison vélo CAN : confirmée explicitement par l’utilisateur après examen
  des photos ; pas encore mesurée avec notre matériel.

Le doute initial CAN/I2C est levé par la confirmation utilisateur. Débit,
câblage du connecteur et échanges de cette variante restent à mesurer.

## Pinout proposé pour le vélo CAN confirmé

| Liaison | GPIO ESP32-C3 | État |
|---|---:|---|
| Écran RST | 0 | Câblage existant |
| Écran CS | 1 | Câblage existant |
| SPI MOSI : écran SDA/MOSI et MCP2515 SI | 3 | Partage proposé |
| SPI SCLK : écran SCL et MCP2515 SCK | 4 | Partage proposé |
| SPI MISO : MCP2515 SO | 5 | Proposé |
| MCP2515 CS | 6 | Proposé |
| MCP2515 INT | 7 | Proposé |
| Écran DC | 10 | Câblage existant |
| Entrée frein future | 20 | Réservée, interface électrique à définir |
| UART0 console TX | 21 | Occupé par le contournement console du SDK local |
| USB D− / D+ | 18 / 19 | Réservés USB |

GPIO2/8/9 restent exclus du plan. Aucun GPIO wake n’est attribué. Il ne reste
pas de paire libre pour l’encodeur dans ce plan : un futur expandeur pour les
entrées non critiques ou une révision du plan sera nécessaire. Le frein reste
une entrée directe, à valider avant commande moteur.

MOSI/SCLK sont partagés, les CS restent distincts. La fréquence MCP2515 doit
être configurée séparément de celle de l’écran. Les sorties SO et INT vers
l’ESP32 doivent être compatibles 3,3 V. La tension VCC du module ne peut pas
être déduite du seul nom MCP2515 : contrôler son schéma, son transceiver et ses
éventuels convertisseurs de niveaux avant de l’alimenter ou le connecter.
La masse côté logique doit être commune pour un module non isolé ; le côté
vélo suit le schéma du module, notamment si celui-ci est isolé.

CAN H/L, référence de masse et wake sur le connecteur vélo : non attribués.
Ne pas ajouter systématiquement la terminaison du module à un bus déjà terminé.

## Réveil batterie

L’utilisateur évoque une entrée wake à relier à GND. Hypothèse non vérifiée
pour cette carte : broche, tension au repos, polarité, impulsion ou maintien
restent inconnus. Ne pas relier une broche supposée wake au GND ni directement
à un GPIO ESP32. Identifier son comportement avec la console d’origine.
Un éventuel contact vers GND commandé par transistor/optocoupleur sera défini
seulement après cette identification. Le réveil électrique est distinct des
écritures CAN d’activation de la sortie de puissance.

## État logiciel et essais restants

- Le profil historique TWAI reste disponible. Le nouveau profil `sdkconfig.observe`
  utilise le MCP2515 en écoute seule ; pilote testé sur modèle SPI et compilé
  pour ESP32-C3, pas encore testé sur module physique.
- `components/board_spi` initialise une seule fois SPI2. En observation, MOSI=3,
  SCLK=4 et MISO=5 ; écran CS=1, MCP CS=6, INT=7. Les doublons et GPIO réservés
  sont refusés au démarrage. Le profil dashboard seul conserve MISO=-1.
- Observation : buffers écran de deux lignes et profondeur SPI de 1, soit
  960 octets de pixels par transfert (384 us à 20 MHz hors overhead). Cela
  réduit le blocage, sans garantir une capture sans perte. Mesurer les
  débordements RX sur le module, qui ne dispose que de deux buffers matériels.
- Quartz pris en charge : 8/16/20 MHz, 125 kbit/s, 16 TQ, point d’échantillonnage
  75 %. Aucun quartz n’est présélectionné ; SPI MCP fixé à 1 MHz pour débuter.
- `BIONX_MCP_HARDWARE_VERIFIED` est désactivé et le quartz à 0 par défaut.
  Leur absence empêche l’initialisation du MCP ; les renseigner seulement
  d’après le module réel. Aucune détection automatique de sa tension ou quartz.
- GPIO20 est réservé au futur frein mais est aussi le RX UART0 par défaut du
  SDK local : réaffecter/désactiver ce RX avant d’implémenter l’entrée frein.
- Commencer par l’identification des broches puis l’écoute passive avec la console
  d’origine. Pas de commande moteur dans le firmware actuel.

La future console maître devra émettre : le transport TX borné, les lectures
actives, l’initialisation, les échanges périodiques et le contrôle figurent dans
`TODO.md`. Le présent pilote MCP2515 n’expose encore aucune API d’émission.

Références : [MCP2515 Microchip](https://www.microchip.com/en-us/product/MCP2515),
[SPI partagé ESP-IDF 5.2](https://docs.espressif.com/projects/esp-idf/en/v5.2/esp32c3/api-reference/peripherals/spi_master.html),
[périmètre CAN BionXtool](https://github.com/buxtronix/BionXtool#hardware-prerequisites).

## Extension HUZZAH32 et maître neutre — 24 octobre 2026

La photo utilisateur montre une Adafruit HUZZAH32 Feather classique (micro-USB,
sérigraphie HUZZAH32). Le profil séparé `sdkconfig.huzzah32` utilise ESP32,
avec brochage proposé et contraintes détaillés dans [MASTER_BENCH.md](MASTER_BENCH.md).
Le C3 et ses profils sont conservés. La photo n’identifie pas le quartz/niveaux
logiques du module CAN et ne valide pas les liaisons fil par fil.

La Feather a un chargeur pour petite LiPo monocellule ; aucun raccordement
direct du pack vélo à BAT/USB/3V. Le pinout proposé libère frein, encodeur,
wake et UART BMS sans les activer. INT34/frein36/bouton39 nécessitent des
pull-ups externes si ces signaux en demandent (pas de pull-up interne).

Le MCP2515 expose désormais une API TX et un profil normal explicite ; les
mentions antérieures « aucune API émission » décrivent l’ancien incrément.
Le profil observation reste passif. Le maître neutre transmet après `s`, sans
réveil électrique ; le module reste désactivé dans les defaults non vérifiés.

## Montage de test déclaré — 24 octobre 2026

HUZZAH32 connectée, écran/MCP/encodeur selon le plan, alimentation déclarée
3,3 V pour tous les modules, quartz MCP déclaré 8 MHz, pas de pull-ups ajoutés.
Bouton encodeur déplacé vers GPIO22 avec pull-up interne ; A25/B26 avec
pull-ups internes. MCP interrogé par polling, sans utiliser INT34.
Voir [COMPONENT_TEST.md](COMPONENT_TEST.md). GPIO22 n’est donc plus libre pour I2C.

Premier test physique : flash et démarrage ESP32 réussis, écran signalé noir
par l’utilisateur, initialisation MCP en timeout ; les pilotes écran s’initialisent
mais cela ne valide pas son affichage. Rotation/bouton non encore validés.

L’utilisateur confirme ensuite que l’écran fonctionne après correction de
broches décalées. MCP déconnecté pour le moment ; test écran + encodeur isolé.

Écran et encodeur : utilisateur confirme désormais le fonctionnement après
correction du pinout (rotation et bouton inclus). Brochage du profil de test :
A1/GPIO25, A0/GPIO26, SCL/GPIO22 pour SW, commun GND. MCP toujours déconnecté.

MCP reconnecté : échange SPI matériel confirmé (reset CANSTAT80, timing relu
conforme, écoute seule CANSTAT60, init ESP_OK). Écran actif à 1 MHz pendant
le test, quartz MCP déclaré 8 MHz. CAN H/L libres : transceiver et réception
CAN restent non validés ; INT34 non utilisé, fonctionnement en polling.


Dernier état du banc (24 octobre 2026) : écran, encodeur et MCP fonctionnent
ensemble sur le brochage HUZZAH32 déclaré. L’utilisateur confirme l’affichage
et la réactivité améliorée à LCD 20 MHz/MCP SPI 2 MHz. INT34 est utilisé :
fronts et réveils RX mesurés. Aucun recâblage supplémentaire pour ces correctifs.
CAN réel reçu à 125 kbit/s, quartz déclaré 8 MHz, écoute seule. Dernière
capture : 4 512 trames / environ 30 s, drops=0, trois événements de
débordement matériel. La capture exhaustive et la transmission restent
non validées. Voir la dernière section de VALIDATION.md pour l’image exacte.


### Montage actuel : deux bus — 25 octobre 2026

L’utilisateur a déplacé les deux fils MCP : SI/MOSI→SDA/GPIO23 et
SCK→A5/GPIO4. SO/MISO19, CS33 et INT34 restent en place ; écran toujours
MOSI18/SCK5/CS14/DC32/RST27. Encodeur A25/B26/SW22 inchangé.
LCD sur SPI2, MCP sur SPI3 ; GPIO21 reste libre pour le wake futur.
Le profil actuel est `sdkconfig.components-dual-spi` après `sdkconfig.components`.
Les profils partagés exigent l’ancien câblage et ne doivent pas être utilisés
tels quels avec ces deux fils déplacés.

Après un premier timeout puis vérification/reconnexion, le MCP répond de
nouveau : init ESP_OK, CAN réel reçu. L’utilisateur confirme maintenant
un affichage fonctionnel et un ensemble « super réactif ».

Validation finale du montage deux bus : 13 878 trames / environ 90 s,
drops=0, ov=0, malformed=0, SPI errors=0 avec écran actif. Le profil
`build-components-dual-spi` est flashé et conservé pour le banc.

Wake (25 octobre) : utilisateur confirme activation par liaison à GND.
Tension du fil libre et nécessité impulsion/maintien encore inconnues.
GPIO21 réservé pour une interface de commutation vers GND, pas pour une
liaison directe au fil batterie. Aucun pilotage wake activé à ce stade.
Le maître peut désormais utiliser SW22/GND (bouton existant) pour ses gestes.

### Maître neutre avec wake externe — 25 octobre 2026

Utilisateur confirme bouton wake externe installé et console d'origine retirée.
Aucun fil wake connecté/piloté par le firmware ; GPIO21 reste libre.
Profil maître deux SPI flashé avec autorisation explicite. Bouton encodeur
SW22 inchangé : maintien 2 s arrête, appui court réarme la découverte.
Au boot, requête HW batterie chaque seconde puis init neutre sur réponse.
Après correction d'ordonnancement RX/TX, échanges réels stables environ 20 s,
mesures candidates ~39,6 V / moteur 35 C / SOC 75 %, aucune perte observée.
Le réveil depuis un sommeil établi, la coupure réelle après arrêt et la
calibration des mesures restent à confirmer ; aucune rotation commandée.
