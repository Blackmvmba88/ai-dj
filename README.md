# 🎧 DJ-IA: DJ piloté par Intelligence Artificielle avec analyse spectrale avancée

DJ-IA est un système innovant de DJ virtuel piloté par intelligence artificielle, capable de générer et mixer de la musique en temps réel. En utilisant un Large Language Model (LLM) pour les décisions créatives, MusicGen pour la génération audio, et Demucs pour l'analyse et l'isolation spectrale, DJ-IA crée des sessions musicales dynamiques et évolutives dans différents styles.

⚠️ **IMPORTANT : Proof of Concept** ⚠️  
Ce projet est actuellement à l'état de preuve de concept (POC). Certains styles musicaux fonctionnent mieux que d'autres, et le code contient des sections inutilisées ou expérimentales qui n'ont pas encore été nettoyées. L'architecture globale est fonctionnelle mais continue d'évoluer.

À noter que parfois le calage entre les layers n'est pas toujours optimal. C'est un aspect qui sera amélioré dans les futures versions.

## Démonstration

🎬 **[Télécharger la démonstration vidéo avec son](./example/poc_ai_dj.mp4)**

![DJ-IA Banner](https://placehold.co/800x200/1a1a1a/FFFFFF?text=DJ-IA)

## ✨ Caractéristiques

- 🧠 IA générative pour les décisions DJ et la génération audio
- 🔊 **NOUVEAU : Analyse spectrale et séparation des stems pour un mixage intelligent**
- 🎭 **NOUVEAU : Sélection intelligente d'instruments complémentaires**
- 🎛️ Système de gestion des layers audio en temps réel
- 🎚️ Effets audio: filtres, panoramique, réverbération et delay
- 🎵 Support pour 10 styles musicaux différents
- 🔄 Transitions synchronisées et progression musicale naturelle
- 🗣️ Interventions vocales générées par TTS

## 🛠️ Installation

### Prérequis

- Python 3.10 ou supérieur
- Une carte graphique NVIDIA avec CUDA pour des performances optimales (recommandé)
- Au moins 8 Go de RAM
- Au moins 2 Go d'espace disque libre

### Configuration de l'environnement

```bash
# Créer et activer un environnement virtuel
python -m venv env

# Sur Windows
.\env\Scripts\activate

# Sur macOS/Linux
source env/bin/activate

# Installer les dépendances de base
pip install numpy==1.24.3

# Installer PyTorch avec CUDA
pip install torch==2.1.0 torchvision==0.16.0 torchaudio==2.1.0 --index-url https://download.pytorch.org/whl/cu118

# Installer les outils nécessaires
pip install setuptools wheel

# Installer les bibliothèques principales
pip install audiocraft pygame llama-cpp-python tqdm librosa

# Installer Demucs pour la séparation de sources
pip install demucs

# Installer les bibliothèques audio
pip install pyrubberband pedalboard soundfile sounddevice pyttsx3
```

## 🚀 Utilisation

### Lancement

```bash
python main.py --model-path "/chemin/vers/ton/modele/llm.gguf" --profile "techno_minimal" --output-dir "./output" --clean
```

### Paramètres

- `--model-path`: Chemin vers le modèle LLM (format GGUF recommandé)
- `--profile`: Style musical (voir ci-dessous)
- `--output-dir`: Dossier où sauvegarder les fichiers audio générés
- `--clean`: Nettoie les fichiers temporaires au démarrage (optionnel)

### Profils disponibles

1. **techno_minimal**: Techno minimaliste et profonde à 126 BPM
2. **experimental**: Sons expérimentaux et avant-gardistes à 130 BPM
3. **rock**: Éléments rock énergiques à 120 BPM
4. **hip_hop**: Beats hip-hop captivants à 90 BPM
5. **jungle_dnb**: Breakbeats rapides et basses profondes à 174 BPM
6. **dub**: Sons spacieux avec échos et réverbérations à 70 BPM
7. **deep_house**: Grooves house profonds et mélodiques à 124 BPM
8. **downtempo_ambient**: Paysages sonores atmosphériques et méditatifs à 85 BPM
9. **classical**: Réinterprétations électroniques de musique classique à 110 BPM
10. **trip_hop**: Beats lourds et mélodies mélancoliques à 95 BPM

## 🧩 Architecture du système

DJ-IA est composé de plusieurs modules:

- **LLM DJ Brain**: Prend les décisions créatives et détermine quels éléments audio ajouter/modifier
- **MusicGen**: Génère les samples audio en fonction des instructions du LLM
- **Demucs**: Analyse et sépare les samples en leurs composantes instrumentales (stems)
- **Spectral Analysis Engine**: Analyse intelligente pour éviter les chevauchements d'instruments
- **LayerManager**: Gère la lecture, le mixage et les effets des différentes couches audio
- **TTS Engine**: Génère des interventions vocales

Le système maintient en permanence un maximum de 3 layers simultanés, dont un seul élément rythmique à la fois pour garantir la cohérence du mix.

### 🎛️ Analyse spectrale et séparation de stems

Le système intègre désormais une technologie avancée d'analyse et de séparation spectrale:

1. **Analyse des échantillons**: Chaque sample généré est analysé avec Demucs pour identifier sa composition (batterie, basse, vocaux, etc.)
2. **Sélection intelligente de stems**: Le système sélectionne intelligemment les éléments complémentaires à ceux déjà présents dans le mix
3. **Évitement des chevauchements**: DJ-IA évite naturellement d'avoir plusieurs couches rythmiques simultanées
4. **Extraction ciblée**: Seul l'élément le plus pertinent pour le mix est extrait et utilisé
5. **Effets adaptés**: Les effets sont automatiquement optimisés pour le type d'instrument utilisé

### Limitations actuelles

- La qualité des samples générés varie selon le style musical
- Certains effets audio (comme la réverbération complexe) sont implémentés mais peu utilisés
- Les profils techno_minimal, hip_hop et deep_house donnent généralement les meilleurs résultats
- La performance dépend fortement de la puissance de votre GPU
- L'analyse spectrale ajoute un délai supplémentaire entre les décisions du DJ

## 📊 Comportement du DJ

Selon le profil choisi, DJ-IA adoptera différents comportements:

- **techno_minimal**: Construction progressive et hypnotique
- **experimental**: Contrastes audacieux et ruptures rythmiques
- **rock**: Progression énergique avec guitares et batterie
- **hip_hop**: Grooves syncopés et basses profondes
- **jungle_dnb**: Tempos rapides et breakbeats complexes
- **dub**: Espaces sonores profonds avec delays et échos
- **deep_house**: Progression fluide avec éléments jazzy et soulful
- **downtempo_ambient**: Évolutions lentes et atmosphériques avec textures immersives
- **classical**: Fusion d'éléments orchestraux avec des rythmiques modernes
- **trip_hop**: Ambiances cinématiques sombres avec beats lourds et scratches vinyles

## 🔧 Dépannage

### Problèmes connus

- **Erreurs CUDA**: Vérifiez que votre version de PyTorch correspond à votre version de CUDA
- **Audio saccadé**: Essayez d'augmenter la valeur du buffer audio dans le fichier `layer_manager.py`
- **Erreurs de mémoire**: Libérez de la RAM ou réduisez la taille du modèle LLM utilisé
- **Erreurs Demucs**: Assurez-vous que Demucs est correctement installé (`pip install demucs`)

## 🤝 Contribution

Les contributions sont les bienvenues! Voici comment vous pouvez contribuer:

1. Fork du projet
2. Création d'une branche pour votre fonctionnalité (`git checkout -b feature/amazing-feature`)
3. Commit de vos changements (`git commit -m 'Add some amazing feature'`)
4. Push vers la branche (`git push origin feature/amazing-feature`)
5. Ouverture d'une Pull Request

## 🙏 Remerciements

- [Audiocraft/MusicGen](https://github.com/facebookresearch/audiocraft) pour la génération audio
- [Demucs](https://github.com/facebookresearch/demucs) pour la séparation de sources audio
- [llama-cpp-python](https://github.com/abetlen/llama-cpp-python) pour l'inférence LLM optimisée
- [Pygame](https://www.pygame.org) pour la lecture audio
- [Librosa](https://librosa.org) pour le traitement audio
