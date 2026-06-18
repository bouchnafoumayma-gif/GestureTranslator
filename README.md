# GestureTranslator 👁️✋

**GestureTranslator** est une application de bureau interactive et performante développée en **C++** avec le framework **Qt 6** et la bibliothèque de vision par ordinateur **OpenCV**. 

L'objectif de ce projet est de concevoir une Interface Homme-Machine (IHM) intelligente capable de capturer, traiter et traduire en temps réel les gestes de la main (nombre de doigts levés) en texte et en synthèse vocale (Text-To-Speech). L'application intègre une gestion multi-langue, des filtres contextuels (Général, Santé, Restaurant) ainsi qu'un mode apprentissage pédagogique.

---

## 🚀 Fonctionnalités Clés

* **Traitement Vidéo en Temps Réel** : Capture fluide du flux de la webcam et isolation d'une zone d'intérêt (ROI) dédiée à l'analyse.
* **Pipeline Algorithmique OpenCV** : Conversion en niveaux de gris, réduction du bruit (Flou Gaussien), binarisation adaptative (Seuillage d'Otsu) et détection géométrique des doigts via l'enveloppe convexe et les défauts de convexité (`convexityDefects`).
* **Filtrage Intelligent du Signal** : Intégration d'un tampon de stabilité (`stabilityCounter`) pour éliminer les micro-tremblements de la main et fiabiliser la traduction avant déclenchement.
* **Base de Données Embarquée** : Moteur **SQLite** gérant dynamiquement les correspondances de vocabulaire selon le contexte et la langue choisis.
* **Accessibilité Audio** : Module de synthèse vocale intégré pour verbaliser instantanément le résultat de la traduction.
* **Mode Apprentissage Interactif** : Module guidé utilisant un système de temporisation (`QTimer`) pour aider l'utilisateur à apprendre et maîtriser la formation des signes.

---

## 🛠️ Technologies & Bibliothèques

* **Langage** : C++ (Norme moderne)
* **Framework IHM** : Qt 6 (Modules : Core, GUI, Widgets, Multimedia, SQL, TextToSpeech)
* **Vision par Ordinateur** : OpenCV 4.x
* **Base de Données** : SQLite 3

---

```text
GestureTranslator/
├── main.cpp                  # Point d'entrée de l'application
├── mainwindow.cpp            # Logique métier, pipeline OpenCV et requêtes SQL
├── mainwindow.h              # Déclarations des slots, signaux et structures de données
├── mainwindow.ui             # Conception graphique de l'interface utilisateur
├── gestures.db               # Base de données SQLite embarquée (tables des gestes)
├── ressources.qrc            # Fichier de ressources Qt (images modèles, icônes)
└── images/                   # Actifs visuels intégrés à l'exécutable
## 📐 Architecture du Pipeline de Traitement

Le traitement d'une frame vidéo suit les étapes rigoureuses suivantes :
1. **Extraction de la ROI** : Isolation d'une sous-matrice centrale pour optimiser les temps de calcul.
2. **Prétraitement** : Passage en nuances de gris et application d'un filtre passe-bas (Flou Gaussien) pour éliminer le bruit numérique.
3. **Binarisation** : Analyse de l'histogramme des pixels pour séparer dynamiquement la main de l'arrière-plan (Méthode d'Otsu).
4. **Analyse Géométrique** : Extraction du contour majeur, calcul de l'enveloppe convexe, filtrage des angles par triangulation mathématique et comptage des sommets.
