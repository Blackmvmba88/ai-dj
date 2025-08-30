#!/usr/bin/env python3
import os
import sys
import psutil
import requests
import subprocess
from pathlib import Path
import logging

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)

class AIdjMonitor:
    def __init__(self):
        self.base_dir = Path(__file__).parent.absolute()
        self.vst_dir = self.base_dir / "vst"
        self.server_url = "http://localhost:8000"
        self.env_dir = self.base_dir / "obsidian-env"

    def check_environment(self):
        """Verifica el ambiente virtual y dependencias"""
        logging.info("Verificando ambiente virtual...")
        
        if not self.env_dir.exists():
            logging.error("❌ Ambiente virtual no encontrado")
            return False
            
        # Verifica activación del ambiente
        if not hasattr(sys, 'real_prefix') and not sys.base_prefix != sys.prefix:
            logging.warning("⚠️ Ambiente virtual no está activado")
            return False
            
        return True

    def check_dependencies(self):
        """Verifica las dependencias principales"""
        required_packages = [
            'torch',
            'fastapi',
            'uvicorn',
            'llama-cpp-python',
            'stable-audio-tools',
            'librosa'
        ]
        
        logging.info("Verificando dependencias...")
        
        missing = []
        for package in required_packages:
            try:
                __import__(package)
                logging.info(f"✅ {package} instalado")
            except ImportError:
                missing.append(package)
                logging.error(f"❌ {package} no encontrado")
        
        return len(missing) == 0

    def check_vst_build(self):
        """Verifica la compilación del VST"""
        logging.info("Verificando compilación del VST...")
        
        build_dir = self.vst_dir / "build"
        if not build_dir.exists():
            logging.error("❌ Directorio build no encontrado")
            return False
            
        # Busca archivos VST3
        vst_files = list(build_dir.rglob("*.vst3"))
        if not vst_files:
            logging.error("❌ No se encontraron archivos VST3 compilados")
            return False
            
        logging.info(f"✅ VST compilado encontrado: {vst_files[0].name}")
        return True

    def check_server(self):
        """Verifica si el servidor Neural está funcionando"""
        logging.info("Verificando servidor Neural...")
        
        try:
            response = requests.get(f"{self.server_url}/health")
            if response.status_code == 200:
                logging.info("✅ Servidor Neural respondiendo")
                return True
        except requests.exceptions.ConnectionError:
            logging.error("❌ Servidor Neural no responde")
            
        # Busca proceso del servidor
        for proc in psutil.process_iter(['name', 'cmdline']):
            if 'python' in proc.info['name'] and 'server_interface.py' in str(proc.info['cmdline']):
                logging.info(f"✅ Proceso del servidor encontrado (PID: {proc.pid})")
                return True
                
        return False

    def check_models(self):
        """Verifica si los modelos necesarios están descargados"""
        logging.info("Verificando modelos...")
        
        models_dir = self.base_dir / "models"
        if not models_dir.exists():
            logging.error("❌ Directorio de modelos no encontrado")
            return False
            
        # Verifica archivos de modelo (ajusta según los modelos específicos)
        required_models = [
            "model.pt",
            "config.json"
        ]
        
        missing = []
        for model in required_models:
            if not (models_dir / model).exists():
                missing.append(model)
                logging.error(f"❌ Modelo {model} no encontrado")
            else:
                logging.info(f"✅ Modelo {model} presente")
                
        return len(missing) == 0

    def check_disk_space(self):
        """Verifica espacio en disco"""
        logging.info("Verificando espacio en disco...")
        
        disk = psutil.disk_usage(self.base_dir)
        gb_free = disk.free / (1024**3)
        
        if gb_free < 5:
            logging.warning(f"⚠️ Poco espacio libre: {gb_free:.1f}GB")
            return False
            
        logging.info(f"✅ Espacio libre suficiente: {gb_free:.1f}GB")
        return True

    def run_checks(self):
        """Ejecuta todas las verificaciones"""
        checks = {
            "Ambiente Virtual": self.check_environment(),
            "Dependencias": self.check_dependencies(),
            "Compilación VST": self.check_vst_build(),
            "Servidor Neural": self.check_server(),
            "Modelos": self.check_models(),
            "Espacio en Disco": self.check_disk_space()
        }
        
        print("\n=== Resumen de Verificación ===")
        all_passed = True
        for check, passed in checks.items():
            status = "✅ PASS" if passed else "❌ FAIL"
            print(f"{check}: {status}")
            all_passed = all_passed and passed
            
        return all_passed

if __name__ == "__main__":
    monitor = AIdjMonitor()
    success = monitor.run_checks()
    
    if success:
        print("\n✨ Todo está correctamente instalado y funcionando!")
        sys.exit(0)
    else:
        print("\n⚠️ Se encontraron problemas en la instalación.")
        print("Revisa los logs anteriores para más detalles.")
        sys.exit(1)
