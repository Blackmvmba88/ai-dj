import json
import time
from llama_cpp import Llama


class DJAILL:
    """Interface avec le LLM qui joue le rôle du DJ"""

    def __init__(self, model_path, profile_name, config):
        """
        Initialise l'interface avec le LLM

        Args:
            model_path (str): Chemin vers le modèle GMA-4B
            profile_name (str): Nom du profil DJ à utiliser
            config (dict): Configuration globale
        """
        # Charger le profil DJ
        from config.dj_profiles import DJ_PROFILES

        self.profile = DJ_PROFILES[profile_name]

        # Initialiser le modèle LLM
        self.model = Llama(
            model_path=model_path,
            n_ctx=4096,  # Contexte suffisamment grand
            n_gpu_layers=-1,  # Utiliser tous les layers GPU disponibles
            n_threads=4,  # Ajuster selon CPU
            verbose=False,
        )

        # État interne du DJ
        self.session_state = {
            "tempo": self.profile["default_tempo"],
            "current_key": "C minor",  # Clé par défaut
            "energy_level": 3,  # 1-10
            "phase": "intro",  # intro, build, drop, breakdown, outro
            "active_samples": [],
            "session_duration": 0,
            "last_action_time": 0,
            "history": [],  # Historique des décisions pour contexte
            "approaching_max_layers": False,
        }

        self.system_prompt = self.profile["system_prompt"]

    def _build_prompt(self):
        """Construit le prompt pour le LLM basé sur l'état actuel"""

        # Historique des dernières actions (limité)
        history_text = "\n".join(
            [
                f"- {action['action_type']}: {json.dumps(action['parameters'])}"
                for action in self.session_state["history"][-5:]  # 5 dernières actions
            ]
        )

        # Informations détaillées sur les layers actifs (avec leurs stems extraits)
        active_layers = self.session_state.get("active_layers", {})
        active_layers_details = []

        for layer_id, layer_info in active_layers.items():
            layer_type = layer_info.get("type", "unknown")
            layer_key = layer_info.get("key", "unknown")
            layer_volume = layer_info.get("volume", 0.8)

            # Information cruciale: le stem utilisé s'il a été extrait par Demucs
            used_stem = layer_info.get("used_stem")
            stem_info = f" (stem extrait: {used_stem})" if used_stem else ""

            spectral_profile = layer_info.get("spectral_profile", {})
            profile_info = ""
            if spectral_profile:
                # Sélectionner les composants principaux
                main_components = sorted(
                    spectral_profile.items(), key=lambda x: x[1], reverse=True
                )[
                    :2
                ]  # Les 2 composants les plus importants
                if main_components:
                    components_str = ", ".join(
                        [f"{k}: {v:.1%}" for k, v in main_components]
                    )
                    profile_info = f" [analyse spectrale: {components_str}]"

            active_layers_details.append(
                f"• {layer_id}: {layer_type}{stem_info}, tonalité: {layer_key}, volume: {layer_volume}{profile_info}"
            )

        active_layers_text = (
            "\n".join(active_layers_details)
            if active_layers_details
            else "Aucun layer actif"
        )

        # Construction du prompt utilisateur
        user_prompt = f"""
    État actuel du mix:
    - Tempo: {self.session_state.get('tempo', self.profile['default_tempo'])} BPM
    - Tonalité actuelle: {self.session_state.get('current_key', 'C minor')}
    - Phase: {self.session_state.get('phase', 'intro')}
    - Durée: {self.session_state.get('session_duration', 0)} secondes

    Layers actifs en détail:
    {active_layers_text}

    Historique récent:
    {history_text}

    Décide maintenant de la prochaine action pour maintenir un set cohérent.
    IMPORTANT: Prends en compte les stems extraits et les analyses spectrales pour tes décisions.
    Par exemple, si tu vois "stem extrait: drums" pour un layer précédent, ajuste ta stratégie en conséquence.
    Réponds UNIQUEMENT en format JSON comme spécifié.
    """
        return user_prompt

    def get_next_decision(self):
        """Obtient la prochaine décision du DJ IA"""

        # Mise à jour du temps de session
        current_time = time.time()
        if self.session_state["last_action_time"] > 0:
            elapsed = current_time - self.session_state["last_action_time"]
            self.session_state["session_duration"] += elapsed
        self.session_state["last_action_time"] = current_time

        # Générer la réponse du LLM
        user_prompt = self._build_prompt()
        print("\n🧠 Génération AI-DJ prompt...")
        response = self.model.create_chat_completion(
            [
                {"role": "system", "content": self.system_prompt},
                {"role": "user", "content": user_prompt},
            ]
        )
        print("✅ Génération terminée !")
        try:
            # Extraire et parser la réponse JSON
            response_text = response["choices"][0]["message"]["content"]
            # Trouver le JSON dans la réponse (au cas où le modèle ajoute du texte autour)
            import re

            json_match = re.search(r"({.*})", response_text, re.DOTALL)
            if json_match:
                json_str = json_match.group(1)
                decision = json.loads(json_str)
            else:
                # Fallback si pas de JSON trouvé
                decision = {
                    "action_type": "sample",  # Action par défaut
                    "parameters": {"type": "techno_kick", "intensity": 5},
                    "reasoning": "Fallback: Pas de réponse JSON valide",
                }
        except (json.JSONDecodeError, KeyError) as e:
            print(f"Erreur de parsing de la réponse: {e}")
            print(f"Réponse brute: {response_text}")
            # Décision par défaut en cas d'erreur
            decision = {
                "action_type": "sample",
                "parameters": {"type": "techno_kick", "intensity": 5},
                "reasoning": f"Erreur: {str(e)}",
            }

        # Enregistrer la décision dans l'historique
        self.session_state["history"].append(decision)

        return decision
