import sounddevice as sd
import soundfile as sf
import os
import numpy as np
import threading
import time
import gc


class AudioLayerSync:
    """AudioLayer avec sounddevice + nettoyage automatique"""

    # Nettoyage automatique
    _cleanup_thread = None
    _cleanup_running = False
    _last_cleanup = time.time()

    def __init__(
        self,
        layer_id: str,
        file_path: str,
        channel_id: int,
        midi_manager,
        volume: float = 0.9,
        pan: float = 0.0,
        measures: int = 1,
    ):
        self.layer_id = layer_id
        self.file_path = file_path
        self.midi_manager = midi_manager
        self.volume = volume * 0.8
        self.pan = pan

        # Audio data
        self.audio_data = None
        self.sample_rate = None
        self.length_seconds = 0

        # État simple
        self.is_armed = False

        # Démarrer le nettoyage auto si pas encore fait
        if not AudioLayerSync._cleanup_running:
            AudioLayerSync._start_cleanup_thread()

        # Charger l'audio
        try:
            self.audio_data, self.sample_rate = sf.read(self.file_path, always_2d=True)
            self.length_seconds = len(self.audio_data) / self.sample_rate
            
            # Convertir en stéréo si nécessaire
            if self.audio_data.shape[1] == 1:
                self.audio_data = np.tile(self.audio_data, (1, 2))
            
            # Appliquer volume et pan une fois pour toutes
            self._apply_volume_and_pan()
            
            print(f"🎵 Layer '{self.layer_id}' chargé ({self.length_seconds:.2f}s)")

        except Exception as e:
            print(f"❌ Erreur chargement {self.layer_id}: {e}")
            self.audio_data = None
            
        # Compatibilité avec LayerManager
        self.sound_object = True if self.audio_data is not None else None

    @classmethod
    def _start_cleanup_thread(cls):
        """Démarre le thread de nettoyage automatique"""
        cls._cleanup_running = True
        cls._cleanup_thread = threading.Thread(target=cls._cleanup_loop, daemon=True)
        cls._cleanup_thread.start()
        print("🧹 Thread de nettoyage automatique démarré")

    @classmethod 
    def _cleanup_loop(cls):
        """Boucle de nettoyage - Juste GC, pas de reset audio"""
        while cls._cleanup_running:
            time.sleep(30)  # Nettoyage toutes les 30 secondes
            
            try:
                gc.collect()
                
                current_time = time.time()
                if current_time - cls._last_cleanup > 60:
                    cls._last_cleanup = current_time
                
            except Exception as e:
                print(f"⚠️ Erreur nettoyage: {e}")

    @classmethod
    def _stop_cleanup(cls):
        """Arrête le nettoyage (pour shutdown propre)"""
        cls._cleanup_running = False


    def _apply_volume_and_pan(self):
        """Applique volume et pan directement sur les données"""
        if self.audio_data is None:
            return
            
        # Appliquer le pan
        if self.pan != 0:
            if self.pan > 0:  # Pan vers la droite
                self.audio_data[:, 0] *= (1.0 - self.pan)  # Réduire le canal gauche
            else:  # Pan vers la gauche
                self.audio_data[:, 1] *= (1.0 + self.pan)  # Réduire le canal droit
        
        # Appliquer le volume
        self.audio_data *= self.volume

    def play(self):
        """Arme le layer"""
        if self.audio_data is None:
            print(f"❌ Impossible d'armer {self.layer_id} - fichier non chargé")
            return

        self.is_armed = True
        self.midi_manager.add_listener(self)
        
        print(f"🎼 Layer '{self.layer_id}' armé - attente du prochain beat 1...")

    def stop(self, fadeout_ms: int = 0, cleanup: bool = True):
        """Arrête le layer"""
        self.is_armed = False
        self.midi_manager.remove_listener(self)
        
        # Stop sounddevice si il joue encore
        sd.stop()
        
        print(f"⏹️  Layer '{self.layer_id}' arrêté")

        # Cleanup fichiers temporaires
        if cleanup and self.file_path and os.path.exists(self.file_path):
            is_temp_file = any(
                marker in self.file_path
                for marker in ["_loop_", "_fx_", "temp_", "_orig_"]
            )

            if is_temp_file:
                try:
                    os.remove(self.file_path)
                    print(f"🗑️  Fichier temporaire supprimé: {self.file_path}")
                except (PermissionError, OSError) as e:
                    print(f"Impossible de supprimer {self.file_path}: {e}")

    def on_midi_event(self, event_type: str, measure: int = None):
        """Callback MIDI - Simplicité + nettoyage occasionnel"""
        
        if event_type == "measure_start" and self.is_armed:
            
            # Debug + nettoyage manuel occasionnel
            if measure and measure % 100 == 0:  # Toutes les 100 mesures
                print(f"🧹 Nettoyage manuel mesure {measure}")
                gc.collect()
                sd.stop()
                time.sleep(0.05)
            
            try:
                sd.stop()
                sd.wait()
                sd.play(self.audio_data, samplerate=self.sample_rate)
                
            except Exception as e:
                print(f"❌ Erreur play {self.layer_id}: {e}")

        elif event_type == "stop":
            sd.stop()

    def set_volume(self, volume: float):
        """Ajuste le volume"""
        self.volume = np.clip(volume, 0.0, 1.0) * 0.8
        self._apply_volume_and_pan()  # Recalcule les données

    def set_pan(self, pan: float):
        """Ajuste le panoramique"""
        # Recharger les données originales d'abord
        original_data, _ = sf.read(self.file_path, always_2d=True)
        if original_data.shape[1] == 1:
            original_data = np.tile(original_data, (1, 2))
        
        self.audio_data = original_data.copy()
        self.pan = np.clip(pan, -1.0, 1.0)
        self._apply_volume_and_pan()  # Recalcule avec nouveau pan

    def is_loaded(self):
        """Vérifie si l'audio est chargé"""
        return self.audio_data is not None

    # Propriétés pour compatibilité debug
    @property
    def is_playing(self):
        """Compatibilité - toujours False car one-shot"""
        return False
    
    @property 
    def sample_position(self):
        """Compatibilité - toujours 0 car one-shot"""
        return 0