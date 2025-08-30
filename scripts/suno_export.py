#!/usr/bin/env python3
"""
Exporta todas tus canciones de Suno a archivos WAV.

Advertencias y ética:
- Usa esto solo para tus propios contenidos, respetando los Términos de Suno.
- Necesitas un token/cookie de sesión de tu cuenta (no intentes automatizar login).

Autenticación admitida (al menos una):
- Variable de entorno SUNO_BEARER: token Bearer (Authorization)
- Variable de entorno SUNO_COOKIE: valor completo del header Cookie de studio.suno.ai

Cómo obtener el token/cookie (resumen):
1) Abre studio.suno.ai (sesión iniciada) → DevTools → Network.
2) Filtra por "studio-api.suno.ai" y abre cualquier request autenticada.
3) Copia:
   - Header Authorization (si existe), y ponlo en SUNO_BEARER (sin la palabra Bearer, solo el token) o
   - Todo el header Cookie y ponlo en SUNO_COOKIE.

Limitaciones:
- Suno cambia endpoints con frecuencia. Este script intenta ser tolerante y fallar con mensajes útiles.
- Suno a veces ofrece audio solo en MP3. En ese caso convertimos a WAV (PCM 16‑bit) con ffmpeg si está instalado.

Uso rápido:
    SUNO_COOKIE="..." python scripts/suno_export.py --out exports/suno --concurrency 4

"""
from __future__ import annotations

import argparse
import concurrent.futures
import json
import os
import re
import sys
import time
from pathlib import Path
from typing import Dict, List, Optional

import requests


API_BASES = [
    # Orden de preferencia por estabilidad histórica
    "https://studio-api.suno.ai/api",  # v2/v3 endpoints suelen colgar aquí
]


class SunoClient:
    def __init__(self, bearer: Optional[str] = None, cookie: Optional[str] = None, timeout: int = 30):
        self.session = requests.Session()
        self.timeout = timeout
        self.base = None

        headers = {
            "Accept": "application/json, text/plain, */*",
            "User-Agent": "suno-exporter/1.0",
            "Origin": "https://studio.suno.ai",
            "Referer": "https://studio.suno.ai/",
        }
        if bearer:
            headers["Authorization"] = f"Bearer {bearer.strip()}"
        if cookie:
            headers["Cookie"] = cookie.strip()
        self.session.headers.update(headers)

        # Descubrir base que responde
        for base in API_BASES:
            try:
                r = self.session.get(base + "/v1/user", timeout=self.timeout)
                if r.status_code in (200, 401, 403):
                    self.base = base
                    break
            except requests.RequestException:
                continue
        if not self.base:
            raise RuntimeError("No se pudo contactar API de Suno. Verifica red.")

    def _get(self, path: str, params: Optional[dict] = None) -> requests.Response:
        url = self.base + path
        r = self.session.get(url, params=params, timeout=self.timeout)
        if r.status_code == 401:
            raise PermissionError("401 Unauthorized. Revisa SUNO_BEARER/SUNO_COOKIE.")
        r.raise_for_status()
        return r

    def get_details(self, clip_id: str) -> Optional[dict]:
        """Obtiene detalles de un clip/track por su ID usando varias rutas conocidas."""
        paths = [
            f"/v1/clips/{clip_id}",
            f"/v1/tracks/{clip_id}",
            f"/v1/songs/{clip_id}",
        ]
        for p in paths:
            try:
                r = self._get(p)
                if r.status_code == 200:
                    return r.json()
            except Exception:
                continue
        return None

    def list_my_songs(self, limit: int = 5000) -> List[dict]:
        """
        Intenta múltiples rutas de listado que Suno ha usado en distintas versiones.
        Devuelve una lista de 'clips' o 'tracks' con URLs de audio si están disponibles.
        """
        aggreg: List[dict] = []

        # Estrategia 1: /v1/user/<me>/clips o /v1/user/clips
        candidates = [
            ("/v1/user/clips", {}),
            ("/v1/clips?scope=me", {}),
            ("/v1/search", {"type": "clip", "scope": "me", "limit": str(limit)}),
            ("/v1/tracks", {"scope": "me", "limit": str(limit)}),
        ]

        for path, params in candidates:
            try:
                r = self._get(path, params=params if params else None)
                if r.status_code == 200:
                    data = r.json()
                    # Normalizar posible estructura
                    if isinstance(data, dict):
                        items = (
                            data.get("items")
                            or data.get("results")
                            or data.get("clips")
                            or data.get("data")
                            or []
                        )
                    elif isinstance(data, list):
                        items = data
                    else:
                        items = []

                    if items:
                        aggreg.extend(items)
                        # Si ya obtuvimos bastante, paramos
                        if len(aggreg) >= 1:
                            break
            except PermissionError:
                raise
            except Exception:
                continue

        # De-duplicar por id si existe
        seen = set()
        unique: List[dict] = []
        for it in aggreg:
            _id = it.get("id") or it.get("clip_id") or it.get("song_id") or it.get("uuid")
            if _id and _id not in seen:
                seen.add(_id)
                unique.append(it)
        return unique

    @staticmethod
    def extract_audio_url(item: dict) -> Optional[str]:
        """Intenta encontrar el URL directo del audio dentro del item."""
        # Campos comunes observados
        for key in [
            "audio_url",
            "audio",
            "audio_url_mp3",
            "mp3_url",
            "wav_url",
            "audio_url_wav",
        ]:
            url = item.get(key)
            if isinstance(url, str) and url.startswith("http"):
                return url

        # A veces vienen anidados
        media = item.get("media") or item.get("assets")
        if isinstance(media, dict):
            for key in ["audio_url", "mp3", "wav", "hls", "url"]:
                val = media.get(key)
                if isinstance(val, str) and val.startswith("http"):
                    return val
                if isinstance(val, dict):
                    for v in val.values():
                        if isinstance(v, str) and v.startswith("http"):
                            return v
        return None


def sanitize_filename(name: str) -> str:
    name = re.sub(r"[\r\n\t]", " ", name)
    name = re.sub(r"[\\/:*?\"<>|]", "_", name)
    return name.strip()[:200] or "untitled"


def ensure_ffmpeg() -> Optional[str]:
    from shutil import which

    exe = which("ffmpeg")
    return exe


def pick_one(d: dict, keys: List[str]) -> Optional[object]:
    for k in keys:
        if k in d and d[k] not in (None, ""):
            return d[k]
    return None


def flatten_tags(val) -> List[str]:
    if isinstance(val, list):
        return [str(x) for x in val]
    if isinstance(val, str):
        # CSV-style or space-separated
        parts = re.split(r"[,;]", val)
        return [p.strip() for p in parts if p.strip()]
    return []


def extract_metadata_from_item(item: dict) -> Dict[str, Optional[str]]:
    """Heurística para extraer lyrics/style/weirdness/genre/persona.
    Acepta estructuras variadas: campos de primer nivel y/o `metadata`, `meta`, `song`, `clip`.
    """
    md: Dict[str, Optional[str]] = {
        "lyrics": None,
        "style": None,
        "weirdness": None,
        "genre": None,
        "persona": None,
        "persona_used": None,
        "tags": None,
        "prompt": None,
    }

    # Candidatos de primer nivel
    md["lyrics"] = pick_one(item, ["lyrics", "lyric", "song_text"]) or md["lyrics"]
    md["style"] = pick_one(item, ["style", "musical_style"]) or md["style"]
    w = pick_one(item, ["weirdness", "weirdness_slider", "randomness"])  # puede ser numérico
    if w is not None:
        md["weirdness"] = str(w)
    md["genre"] = pick_one(item, ["genre"]) or md["genre"]
    md["persona"] = pick_one(item, ["persona", "voice", "singer", "artist_persona"]) or md["persona"]
    md["prompt"] = pick_one(item, ["prompt", "gpt_description_prompt"]) or md["prompt"]
    tags_val = pick_one(item, ["tags"]) or []
    tags = flatten_tags(tags_val)

    # Buscar en metadata anidada
    nested = pick_one(item, ["metadata", "meta", "song", "clip", "params"]) or {}
    if isinstance(nested, dict):
        md["lyrics"] = md["lyrics"] or pick_one(nested, ["lyrics", "lyric", "song_text"])  # type: ignore
        md["style"] = md["style"] or pick_one(nested, ["style", "musical_style"])  # type: ignore
        w2 = pick_one(nested, ["weirdness", "weirdness_slider", "randomness"])  # type: ignore
        if w2 is not None and md["weirdness"] is None:
            md["weirdness"] = str(w2)
        md["genre"] = md["genre"] or pick_one(nested, ["genre"])  # type: ignore
        md["persona"] = md["persona"] or pick_one(nested, ["persona", "voice", "singer", "artist_persona"])  # type: ignore
        md["prompt"] = md["prompt"] or pick_one(nested, ["prompt", "gpt_description_prompt"])  # type: ignore
        tags_val2 = pick_one(nested, ["tags"]) or []
        tags.extend(flatten_tags(tags_val2))

    # Derivar desde tags tipo "genre: techno" o "style: dark"
    for t in tags:
        low = t.lower()
        if low.startswith("genre:") and not md["genre"]:
            md["genre"] = t.split(":", 1)[1].strip() or None
        if low.startswith("style:") and not md["style"]:
            md["style"] = t.split(":", 1)[1].strip() or None
        if low.startswith("persona:") and not md["persona"]:
            md["persona"] = t.split(":", 1)[1].strip() or None

    if tags:
        md["tags"] = ", ".join(dict.fromkeys(tags))  # únicos, orden estable

    # Persona usada: verdadero si hay valor en persona o tag relacionado
    md["persona_used"] = "true" if (md["persona"] and str(md["persona"]).strip()) else "false"
    return md


def download_file(session: requests.Session, url: str, dest: Path, timeout: int = 60) -> None:
    with session.get(url, stream=True, timeout=timeout) as r:
        r.raise_for_status()
        dest.parent.mkdir(parents=True, exist_ok=True)
        with open(dest, "wb") as f:
            for chunk in r.iter_content(chunk_size=1 << 14):
                if chunk:
                    f.write(chunk)


def convert_to_wav(src: Path, dst: Path) -> None:
    import subprocess

    cmd = [
        "ffmpeg",
        "-y",
        "-i",
        str(src),
        "-acodec",
        "pcm_s16le",
        "-ac",
        "2",
        "-ar",
        "44100",
        str(dst),
    ]
    subprocess.run(cmd, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)


def main(argv: Optional[List[str]] = None) -> int:
    p = argparse.ArgumentParser(description="Exporta tus temas de Suno a WAV")
    p.add_argument("--out", default="exports/suno", help="Directorio de salida")
    p.add_argument("--concurrency", type=int, default=4, help="Descargas paralelas")
    p.add_argument("--limit", type=int, default=0, help="Limitar número de temas (0 = todos)")
    p.add_argument("--urls-file", default=None, help="Archivo .txt con URLs de audio (una por línea)")
    p.add_argument("--from-json", default=None, help="JSON con objetos que contengan audio_url/mp3_url/wav_url")
    p.add_argument("--state-file", default=None, help="Ruta a archivo de estado para deduplicar (por defecto en la carpeta --out)")
    p.add_argument("--only-new", action="store_true", help="Descargar solo items nuevos según --state-file")
    args = p.parse_args(argv)

    items: List[dict] = []
    client: Optional[SunoClient] = None

    # Modo 1: URLs directas desde archivo
    if args.urls_file:
        txt = Path(args.urls_file).read_text(encoding="utf-8")
        for i, line in enumerate(txt.splitlines(), 1):
            u = line.strip()
            if u and u.startswith("http"):
                items.append({"title": f"suno_{i:04d}", "id": str(i), "audio_url": u})

    # Modo 2: JSON con objetos que incluyan URLs de audio
    elif args.from_json:
        data = json.loads(Path(args.from_json).read_text(encoding="utf-8"))
        if isinstance(data, dict):
            data = data.get("items") or data.get("results") or data.get("clips") or data.get("data") or []
        if isinstance(data, list):
            items.extend(data)
        else:
            print("JSON no contiene una lista reconocible.", file=sys.stderr)
            return 2

    # Modo 3: API oficial/no documentada con cookie o bearer
    else:
        bearer = os.getenv("SUNO_BEARER")
        cookie = os.getenv("SUNO_COOKIE")
        if not bearer and not cookie:
            print("ERROR: Define SUNO_BEARER o SUNO_COOKIE en el entorno, o usa --urls-file / --from-json.", file=sys.stderr)
            return 2

        client = SunoClient(bearer=bearer, cookie=cookie)
        print("Conectando y listando tus canciones…")
        items = client.list_my_songs()
        if not items:
            print("No se encontraron canciones. ¿La cuenta tiene creaciones?", file=sys.stderr)
            return 1
        if args.limit > 0:
            items = items[: args.limit]

    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)

    # Estado de descargas previas
    state_path = Path(args.state_file) if args.state_file else (out_dir / "suno_export_state.json")
    downloaded_ids: set[str] = set()
    if state_path.exists():
        try:
            st = json.loads(state_path.read_text(encoding="utf-8"))
            if isinstance(st, dict) and isinstance(st.get("downloaded_ids"), list):
                downloaded_ids = set(str(x) for x in st["downloaded_ids"])  # type: ignore
        except Exception:
            pass
    if args.only-new:
        # filtrar items ya descargados
        def _id_of(d):
            return str(d.get("id") or d.get("clip_id") or d.get("uuid") or "")
        items = [it for it in items if _id_of(it) and _id_of(it) not in downloaded_ids]

    ffmpeg_path = ensure_ffmpeg()
    sess = client.session if client else requests.Session()

    def handle_item(it: dict) -> str:
        title = it.get("title") or it.get("name") or it.get("prompt") or "untitled"
        song_id = it.get("id") or it.get("clip_id") or it.get("uuid") or str(int(time.time() * 1000))
        safe = sanitize_filename(f"{title}__{song_id}")

        url = client.extract_audio_url(it) if client else (it.get("audio_url") or it.get("mp3_url") or it.get("wav_url"))
        if not url:
            # A veces el objeto de la lista no trae las URLs; intentar detalle si hay endpoint
            # Heurística: /v1/clips/<id>
            if client:
                for path in (f"/v1/clips/{song_id}", f"/v1/tracks/{song_id}"):
                    try:
                        r = client._get(path)
                        data = r.json()
                        url = client.extract_audio_url(data)
                        if url:
                            break
                    except Exception:
                        pass
        if not url:
            return f"SKIP (sin URL): {title}"

        # Elegir extensión según URL
        ext = "wav" if ".wav" in url.lower() else "mp3" if ".mp3" in url.lower() else "audio"
        tmp_path = out_dir / f"{safe}.{ext}"
        wav_path = out_dir / f"{safe}.wav"

        if wav_path.exists():
            return f"EXISTE: {wav_path.name}"

        try:
            download_file(sess, url, tmp_path)
        except Exception as e:
            return f"ERROR descarga: {title} → {e}"

        if tmp_path.suffix.lower() == ".wav":
            # Ya es WAV
            return f"OK: {tmp_path.name}"

        # Convertir a WAV si ffmpeg está disponible
        if ffmpeg_path:
            try:
                convert_to_wav(tmp_path, wav_path)
                tmp_path.unlink(missing_ok=True)
                return f"OK (convertido): {wav_path.name}"
            except Exception as e:
                return f"ERROR ffmpeg: {title} → {e}"
        else:
            return f"OK (MP3): {tmp_path.name} — instala ffmpeg para WAV"

    results: List[str] = []
    manifest: List[Dict[str, str]] = []
    newly_downloaded_ids: set[str] = set()
    with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, args.concurrency)) as ex:
        futs = [ex.submit(handle_item, it) for it in items]
        for f in concurrent.futures.as_completed(futs):
            msg = f.result()
            results.append(msg)
            print(msg)
    # Guardar manifiesto con lo que encontramos (no solo lo descargado)
    try:
        manifest_path = out_dir / "suno_export_manifest.json"
        csv_path = out_dir / "suno_export_manifest.csv"
        for it in items:
            # Traer detalles si es posible para enriquecer metadatos
            full = dict(it)
            sid = str(it.get("id") or it.get("clip_id") or it.get("uuid") or "").strip()
            if client and sid and (not any(k in it for k in ("lyrics", "style", "weirdness", "genre", "persona"))):
                details = client.get_details(sid)
                if isinstance(details, dict):
                    # merge superficial
                    full = {**details, **full}

            md = extract_metadata_from_item(full)
            title = str(full.get("title") or full.get("name") or full.get("prompt") or "").strip()
            audio = SunoClient.extract_audio_url(full) or it.get("audio_url") or ""
            filename = sanitize_filename(f"{title or 'untitled'}__{sid or 'noid'}.wav")
            row = {
                "id": sid,
                "title": title,
                "audio_url": audio,
                "lyrics": md.get("lyrics") or "",
                "style": md.get("style") or "",
                "weirdness": md.get("weirdness") or "",
                "genre": md.get("genre") or "",
                "persona": md.get("persona") or "",
                "persona_used": md.get("persona_used") or "",
                "tags": md.get("tags") or "",
                "prompt": md.get("prompt") or "",
                "suggested_filename": filename,
            }
            manifest.append(row)

            # Marcar como descargado si el archivo WAV existe
            if (out_dir / filename).exists() and sid:
                newly_downloaded_ids.add(sid)

        # JSON
        manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")

        # CSV simple
        try:
            import csv

            with open(csv_path, "w", newline="", encoding="utf-8") as f:
                writer = csv.DictWriter(
                    f,
                    fieldnames=[
                        "id",
                        "title",
                        "genre",
                        "style",
                        "weirdness",
                        "persona_used",
                        "persona",
                        "lyrics",
                        "prompt",
                        "tags",
                        "audio_url",
                        "suggested_filename",
                    ],
                )
                writer.writeheader()
                for row in manifest:
                    writer.writerow(row)
        except Exception:
            pass

        # Persistir estado actualizado
        try:
            if newly_downloaded_ids:
                all_ids = sorted(set(downloaded_ids) | set(newly_downloaded_ids))
                state_path.write_text(json.dumps({"downloaded_ids": all_ids}, ensure_ascii=False, indent=2), encoding="utf-8")
        except Exception:
            pass
    except Exception:
        pass

    print("—" * 60)
    print(f"Completado. Archivos en: {out_dir}")
    print("Se escribió manifest: suno_export_manifest.json y CSV opcional")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
