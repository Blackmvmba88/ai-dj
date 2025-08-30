Suno Assistant (multiplataforma)

Objetivo
- Guardar tu token/cookie de Suno en el llavero del sistema (Keychain macOS / Credential Manager Windows / Secret Service Linux).
- Ejecutar la exportación con un comando, detectando tu sistema y abriendo la carpeta al final.
- Preparar CSV + descripciones para subir a SoundCloud sin automatizar la web (cumple TOS).

Instalación

```bash
pip install keyring requests
```

Setup de credenciales (una sola vez)

Opción A — automática (lee tu cookie del navegador):

```bash
pip install browser-cookie3
python scripts/suno_assistant.py setup --auto --browser chrome   # o brave/edge/firefox/...
```

Opción B — manual (pegar Bearer o Cookie):

```bash
python scripts/suno_assistant.py setup
# Pega tu Bearer o Cookie (entrada oculta)
```

Exportar WAVs

```bash
python scripts/suno_assistant.py export --out exports/suno --concurrency 4 --open
```

Preparar paquete para SoundCloud

```bash
python scripts/suno_assistant.py prep-soundcloud \
  --manifest exports/suno/suno_export_manifest.json \
  --audio-dir exports/suno \
  --out exports/soundcloud \
  --artwork-dir path/a/tus/portadas   # opcional
```

Salida
- WAVs en `exports/suno/`
- Manifiesto `suno_export_manifest.json` y `suno_export_manifest.csv`
- CSV para SoundCloud: `exports/soundcloud/soundcloud_upload.csv`
- Descripciones por pista: `exports/soundcloud/descriptions/<id>.txt`

Notas
- El asistente no automatiza la web de SoundCloud; solo te deja listo el contenido.
- Puedes abrir la carpeta de salida automáticamente con `--open` en `export`.

Sync automático (cada 5 minutos)

```bash
# Solo descargar lo nuevo según el estado en la misma carpeta
python scripts/suno_assistant.py sync --out exports/suno --interval 300 --only-new --open-on-first

# Detener: Ctrl + C
```

Detalles del sync
- Usa el estado guardado en `exports/suno/suno_export_state.json` para no repetir descargas.
- Puedes ajustar `--interval` (segundos) y `--concurrency`.

Arrancar solo al iniciar sesión (macOS LaunchAgent)

```bash
# Instalar LaunchAgent (sync cada 5 min, solo nuevos)
python scripts/suno_assistant.py install-macos-agent \
  --out exports/suno \
  --interval 300 \
  --only-new

# Ver logs: Console.app o `tail -f ~/Library/Logs/suno_sync.out.log`

# Desinstalar
python scripts/suno_assistant.py uninstall-macos-agent
```
