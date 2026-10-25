# Console BionX ESP32

Console CAN pour le banc **Adafruit HUZZAH32 / ESP32 classique**, écran rond
GC9A01 et MCP2515 sur deux bus SPI. Les profils ESP32-C3 historiques restent
séparés. Licence GPL-3.0-only.

**V1 expérimentale :** initialisation automatique au boot, dashboard d'origine,
un mode d'assistance au signal pédale ×2, sans régénération, puissance électrique
batterie et commande CAN visibles. Calibration à vide puis armement local ;
aucun départ moteur au simple boot. Le gain physique et les watts restent à
calibrer. Procédure : [ASSIST_V1](docs/ASSIST_V1.md).

## État vérifié

Le pilotage moteur roue levée, l'écran/encodeur, le démarrage/extinction et la
lecture pédale du profil jog précédent sont confirmés par l'utilisateur.
La V1 est un nouvel incrément logiciel, sans flash ni validation physique.
Frein et arrêt moteur sur perte CAN/reset restent à mesurer avant usage roulant.

- [Point de reprise et image actuellement chargée](HANDOFF.md)
- [Travail et preuves par incrément](TODO.md)
- [Bilan des essais précédents](docs/BENCH_FINDINGS.md)
- [Journal de validation](docs/VALIDATION.md)
- [Matériel et câblage](docs/HARDWARE.md)
- [Exigences initiales et périmètre V1](project_requirements.md)

## Compiler et tester

Environnement utilisé : ESP-IDF local5.2-dirty. Les dépendances applicatives
sont verrouillées dans `dependencies.lock`. Le SDK local modifié n'est pas
une référence reproductible sur une installation propre.

```sh
export IDF_PYTHON_CHECK_CONSTRAINTS=0
. /Users/theolacassin/esp/v5.2/esp-idf/export.sh

cmake -S tests/unit -B build-unit
cmake --build build-unit
ctest --test-dir build-unit --output-on-failure

cmake -S tests/host -B build-host
cmake --build build-host
ctest --test-dir build-host --output-on-failure
```

Commandes complètes des deux builds V1 dans [ASSIST_V1](docs/ASSIST_V1.md).
Le profil standard calcule à blanc ; l'overlay `sdkconfig.assist-v1-bench-live`
compile une image d'émission distincte pour le banc supervisé.
Aucun de ces builds n'effectue de flash. Le nom historique du binaire contient
`c3`, mais les profils HUZZAH32 ciblent ESP32.

## Organisation

| Dossier | Contenu |
|---|---|
| `main/` | Intégration maître, entrées et applications séparées |
| `components/` | Protocole, transport, contrôle, écran et dashboard |
| `tests/unit/` | Tests C sans matériel |
| `tests/host/` | Dashboard avec le véritable LVGL et rendu RGB565 |
| `docs/` | Procédures, analyses et preuves datées |
| `captures/` | Captures CAN originales, conservées sans modification |
| `tools/` | Rejeu des captures, sans émission CAN |
| `sdkconfig.*` | Profils source ; configurations générées dans `build*/` |

Les sorties `build*/` et composants téléchargés `managed_components/` sont
ignorés. [Ancien guide détaillé](docs/LEGACY_SETUP.md) conservé pour les profils
C3, écran, diagnostic et observation ; ses états historiques ne remplacent
pas le [handoff](HANDOFF.md).

## Git

`main` conserve la base importée du banc. `feature/assist-v1` contient les
incréments V1 et le rangement, avec commits documentés et publiés sur
[le dépôt](https://github.com/tlacassi/BIONX_console_ESP32).
