# Assistance de banc — analyse et procédure (25 octobre 2026)

Ce document concerne le contrôleur de pédalage séparé et ses analyses, pas
l’application jog active. Pour le firmware courant, lire [HANDOFF.md](../HANDOFF.md).
La lecture pédalage est fonctionnelle selon l’utilisateur ; assistance depuis
ce capteur et calibration physique ne sont pas validées.

## Sources examinées

Lecture du code à deux commits identifiés ; aucune source copiée dans le
firmware. Le nouveau code reste GPL-3.0-only, comme le projet.

- [stancecoke/BionX_Minimal_Translator, d8f4fbb](https://github.com/stancecoke/BionX_Minimal_Translator/tree/d8f4fbb28d16435d4dd8182539962dab29c3d82b)
  : `Src/main.c`, notamment lignes 243–336, 349–434, 456–480.
- [inarighas/BionxConsole, 41b06f7](https://github.com/inarighas/BionxConsole/tree/41b06f7d1fc50b60cae9123ee55e801b9e330f54)
  : `sparkfun_bionx/sparkfun_bionx.ino`, notamment lignes 228–342,
  356–431 et pilote MCP2515.

### Minimal Translator

Le calcul côté console utilise le capteur interne filtré, le niveau choisi
et un gain ; une autre branche utilise un capteur externe et le PAS. La demande
signée est bornée à ±63 puis envoyée au registre moteur 0x09. Des entrées frein,
accélérateur et une régène avec limitation de tension existent.

Ce n'est pas une base de sécurité à reprendre telle quelle : dans la branche
interne, la disparition du signal conduit à une décroissance lente du filtre ;
une tension trop basse peut sauter l'envoi au lieu d'envoyer explicitement zéro.
La construction `MS.Voltage` emploie deux fois l'octet HI à la ligne 249.
Le traitement reçu s'appuie sur le registre, sans notre appariement temporel.
Le code examiné ne prouve aucun délai de coupure du contrôleur moteur sur CAN absent.

### BionxConsole

`pedinput()` transforme le couple moyenné et le niveau choisi en `spdout` ;
`regularCANmtr()` lit 0x21, écrit deux fois 0x09 puis lit vitesse et puissance.
Les niveaux inférieurs à 5 correspondent à des commandes négatives/régène ;
la variable « speed desired » n'établit pas une unité physique de vitesse.

La réception déclenche le démarrage sur n'importe quelle trame, et les mesures
ne possèdent pas de durée de validité. La voie d'émission applicative ignore
le résultat de `mcp2515_send_message`. Aucun verrouillage de défaut comparable
à celui du présent projet n'est démontré par ces fonctions.

Conclusion d'analyse : reprendre le principe effort → demande périodique,
mais pas les gains, délais de filtrage ou réactions aux erreurs. Aucun des
deux dépôts ne démontre la roue libre garantie en cas de rupture CAN.

## Contrôle livré

Profil séparé **banc uniquement**, sans régénération, accélérateur ou vitesse
cible. Le contrôleur calcule une proposition brute :

`min(4, max(0, couple_brut - zéro_calibré - 2) / 4)` (division entière).

Ce brut n'est ni un courant garanti, ni un nombre de watts. Le plafond de
4/64 est une fraction de la plage documentée de commande, pas une mesure de
puissance/couple. La lecture 0x14 reste un indicateur brut, pas des watts.
Le contrôleur générique historique avec régène reste indépendant et non utilisé.

- Calibration explicite `cal` : roue immobile et pédales entièrement déchargées,
  16 échantillons distincts, brut ≤8 et dispersion ≤2. Rien de conservé en NVS.
- `arm` seulement calibré, au repos, bouton relâché ; validité de l'armement 5 s.
- Maintenir ensuite le bouton encodeur pour un essai de 800 ms maximum.
  Le couple doit encore être au neutre à l'appui ; augmenter l'effort ensuite.
- Montée limitée à un point brut par 100 ms ; baisse de demande immédiate,
  sans prolongation par filtre. Relâchement = neutre et désarmement.
- Après chaque essai, nouveau `arm` nécessaire ; un bouton coincé ne répète
  pas les essais. Le maintien historique 2 s continue de demander l'extinction.
- Couple brut >64, saut entre deux mesures >16, vitesse brute ≥3, température
  ≥50 °C candidate, tension hors 30–42 V candidate : défaut verrouillé.
- Couple frais <150 ms, vitesse <250 ms, power-meter <500 ms, température et
  tension <2 s ; horloge incohérente ou boucle interrompue >100 ms : défaut.
- Une donnée reçue mais figée n'est pas nécessairement détectable comme capteur
  défectueux. Un effort statique n'établit pas une cadence : ce n'est pas un PAS
  routier. Le bouton maintenu et l'essai bref limitent le périmètre du banc.
- Contrôle et consigne expirables à 100 ms, vérification supplémentaire juste
  avant SPI, plafond indépendant, signe négatif interdit. Si la dernière
  vérification refuse une demande, elle est remplacée par une trame neutre.

Acquisition du profil banc : couple visé toutes les 50 ms, vitesse 100 ms,
power-meter 250 ms, autres mesures 1 s ; une requête à la fois. Délais réels
soumis au bus. Si un registre lent bloque la fraîcheur du couple, le contrôle
se met en défaut ; il ne réutilise pas une ancienne demande. Les périodes
neutres/commande (50 ms) et maintien batterie (250 ms) restent indépendantes.

## Ce que signifie « roue libre sur défaut »

La réaction logicielle est **assistance zéro, régène zéro, défaut verrouillé**.
Sur défaut protocolaire avec commande non nulle précédemment transmise,
le maître tente au plus deux trames neutres espacées, sans continuer l'assistance.
Une erreur sur cette tentative interrompt les tentatives ; aucun reset/réessai
infini ni réarmement automatique. Sur défaut transport/bus-off/pertes RX ou
autre maître détecté, aucun envoi supplémentaire n'est présumé utilisable.

Un CAN rompu ou un ESP arrêté ne peut pas envoyer zéro. Il faut donc mesurer
le timeout moteur et disposer d'une coupure matérielle indépendante. **Ni le
watchdog logiciel ESP, ni l'expiration d'une variable ne coupent physiquement
le moteur.** Le résultat réel de roue libre reste une validation matérielle.
Une confirmation TX CAN ne prouve pas l'application de la consigne par le moteur.

## Frein préparé, non raccordé pour les premiers tests

À la demande utilisateur, `BIONX_ASSIST_BRAKE=n` dans le profil livré : mode banc
à bouton maintenu, pas de frein inventé. Le futur frein utilise GPIO36,
contact normalement fermé vers GND au repos et ouvert au freinage. Résistance
externe de pull-up vers 3,3 V obligatoire ; GPIO36 n'en possède pas en interne.
Pas de tension externe directement appliquée à cette entrée.

Avec `BIONX_ASSIST_BRAKE=y`, il faut avoir observé au moins une ouverture puis
un retour fermé stable avant armement ; ouverture pendant l'essai coupe la
demande. Un fil coupé se comporte comme freinage. Un court-circuit à GND n'est
pas détectable par cette entrée simple. Le test sans frein exige le maintien
encodeur et la coupure matérielle indépendante ; il n'autorise pas un essai roulant.

## Profil et premiers essais

```sh
export IDF_PYTHON_CHECK_CONSTRAINTS=0
. /Users/theolacassin/esp/v5.2/esp-idf/export.sh
idf.py -B build-assist-bench -D IDF_TARGET=esp32 -D SDKCONFIG=build-assist-bench/sdkconfig -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.master;sdkconfig.huzzah32;sdkconfig.components-dual-spi;sdkconfig.master-dual-spi;sdkconfig.assist-bench' build
```

Le profil livré est **DRY RUN, TX=0** : la proposition varie mais la trame
moteur reste nulle. `BIONX_ASSIST_TX_VERIFIED` est désactivé. Ce verrou n'est
pas contournable par commande série, encodeur ou donnée reçue.

1. Sur banc, réveiller avec le bouton externe et attendre les mesures.
2. Pédales sans charge, roue immobile, envoyer `cal` + Entrée sur le port série.
3. Quand DISARMED et calibration terminée, envoyer `arm` + Entrée.
4. Appuyer et maintenir l'encodeur sans effort initial sur les pédales, puis
   charger doucement la pédale. Lire `Tq`, `base`, `want` (proposition), `tx`.
   En dry-run `tx` reste zéro. Relâcher : `want=0`, désarmement immédiat.
5. Après un défaut, éliminer la cause puis refaire explicitement `cal` et `arm`.
   `disarm` annule l'essai ; `q`/maintien 2 s demande aussi l'arrêt du vélo.

Avant activation non nulle : timeout moteur réellement vérifié, coupure de
puissance indépendante accessible, roue levée et zone libre, calibration
capteur et échelles/limites confirmées. Le passage en émission positive exige
une nouvelle configuration explicite, puis build et validation. Ne pas présenter
ce profil comme une assistance prête à rouler.
