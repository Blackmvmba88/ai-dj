Suno → WAV (export masivo)

Resumen:
- Descarga todos tus temas de Suno y los convierte a WAV listos para SoundCloud.
- Requiere copiar tu cookie/token de sesión (no pedimos tu password).

1) Instalar dependencias

```bash
python -m pip install -U requests

# Para conversión a WAV desde MP3 (recomendado)
# macOS (Homebrew):
brew install ffmpeg
# Ubuntu/Debian:
sudo apt-get update && sudo apt-get install -y ffmpeg
# Windows (winget):
winget install Gyan.FFmpeg
```

2) Obtener cookie/token de Suno

1. Abre https://studio.suno.ai y asegúrate de estar logueado.
2. Abre DevTools (Chrome/Edge: Ctrl+Shift+I o Cmd+Opt+I) → pestaña Network.
3. Recarga la página y filtra por “studio-api.suno.ai”.
4. Abre cualquiera de las requests autenticadas.
5. En “Request Headers”:
   - Si ves `Authorization: Bearer <TOKEN>` copia solo el `<TOKEN>` y exporta como `SUNO_BEARER`.
   - Si no, copia TODO el valor del header `cookie` (botón derecho → Copy value) y úsalo en `SUNO_COOKIE`.

3) Ejecutar export

```bash
# Usando cookie completa
SUNO_COOKIE="<pega_aquí_tu_cookie_completa>" \
python scripts/suno_export.py --out exports/suno --concurrency 4

# O usando token Bearer (si existe)
SUNO_BEARER="<tu_token_bearer>" \
python scripts/suno_export.py --out exports/suno --concurrency 4
```

El script:
- Lista tus temas.
- Descarga audio (WAV si Suno lo ofrece; si es MP3, lo convierte a WAV si tienes ffmpeg).
- Escribe `suno_export_manifest.json` y `suno_export_manifest.csv` con metadatos clave:
  - `lyrics`, `style`, `weirdness`, `genre`, `persona` (y `persona_used`), `tags`, `prompt`, `audio_url`, `suggested_filename`.

Alternativas/fallbacks

- Si la API de Suno cambia y el listado falla, puedes pasar URLs directas:

```bash
# Un URL por línea en urls.txt (pueden ser .mp3 o .wav del CDN de Suno)
python scripts/suno_export.py --urls-file urls.txt --out exports/suno
```

- Si tienes un JSON con objetos que contengan `audio_url` / `mp3_url` / `wav_url`:

```bash
python scripts/suno_export.py --from-json mis_songs.json --out exports/suno
```

Notas éticas y de uso

- Descarga solo tus propias creaciones y respeta los Términos de Suno.
- Este proyecto no intenta eludir medidas de acceso ni recolecta credenciales.

Campos y cómo se infieren

- `lyrics`: busca `lyrics/lyric/song_text` en el objeto o en `metadata/meta`.
- `style`: toma `style/musical_style` o el tag `style: ...`.
- `weirdness`: toma `weirdness/weirdness_slider/randomness` (se guarda como texto).
- `genre`: toma `genre` o el tag `genre: ...`.
- `persona`: busca `persona/voice/singer/artist_persona` o tag `persona: ...`; `persona_used` será `true` si se detecta valor.
- `tags`: lista unificada en texto (si Suno provee tags list/string).
- Si la lista inicial no trae todo, el script intenta pedir detalle del clip: `/v1/clips/{id}`, `/v1/tracks/{id}` o `/v1/songs/{id}`.
