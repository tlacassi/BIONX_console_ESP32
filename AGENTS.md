# Reprise du projet BionX

Avant toute modification, lire [HANDOFF.md](HANDOFF.md), puis
[project_requirements.md](project_requirements.md) et les documents pertinents
indiqués dans le handoff. Le code, le profil effectivement compilé et les
preuves datées priment sur les sections historiques.

Mettre à jour TODO.md avant un incrément firmware, puis y consigner les résultats.
Séparer implémentation, tests logiciels et observation physique utilisateur.
Consigner les nouveaux résultats dans VALIDATION.md et actualiser le bilan
BENCH_FINDINGS.md lorsqu’une conclusion change.

Le banc actif est HUZZAH32/ESP32, pas la carte ESP32-C3 cible historique.
Les profils neutre, jog et assistance pédale ont des autorisations distinctes.
Ne pas transformer une lecture de capteur en activation de l’assistance pédale.
Ne pas lancer automatiquement un mouvement moteur pour valider le firmware.
L’ouverture du port série peut réinitialiser la carte : préserver d’abord un
éventuel écran de premier défaut et tenir compte de l’essai physique en cours.
