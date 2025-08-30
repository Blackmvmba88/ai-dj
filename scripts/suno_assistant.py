#!/usr/bin/env python3
"""
Suno Assistant: setup + export + preparación SoundCloud.

Objetivos
- Guardar de forma segura el token/cookie de Suno en el llavero del sistema (keyring).
- Detectar el SO (macOS/Windows/Linux) y ejecutar la exportación automática.
- Preparar CSV + descripciones para subir a SoundCloud manualmente (cumpliendo TOS).

Requisitos
    pip install keyring requests

Uso rápido
    # 1) Guardar tu token/cookie en el llavero del sistema
    python scripts/suno_assistant.py setup

    # 2) Exportar WAVs y generar manifiesto
    python scripts/suno_assistant.py export --out exports/suno --concurrency 4

    # 3) Preparar CSV + descripciones para SoundCloud
    python scripts/suno_assistant.py prep-soundcloud --manifest exports/suno/suno_export_manifest.json \
        --audio-dir exports/suno --out exports/soundcloud

"""
from __future__ import annotations

import argparse
import csv
import json
import os
import platform
import subprocess
import sys
from pathlib import Path
from typing import Dict, List, Optional
import getpass

try:
    import keyring  # type: ignore
except Exception:  # pragma: no cover
    keyring = None


SERVICE = "suno_export"


def get_os() -> str:
    s = platform.system().lower()
    if s.startswith("darwin") or s.startswith("mac") or s == "darwin":
        return "mac"
    if s.startswith("win"):
        return "win"
    return "linux"


def open_folder(path: Path) -> None:
    try:
        system = get_os()
        if system == "mac":
            subprocess.run(["open", str(path)], check=False)
        elif system == "win":
            os.startfile(str(path))  # type: ignore[attr-defined]
        else:
            subprocess.run(["xdg-open", str(path)], check=False)
    except Exception:
        pass


def store_secret(name: str, value: str) -> None:
    if not keyring:
        raise RuntimeError("keyring no disponible. Instala 'pip install keyring'.")
    keyring.set_password(SERVICE, name, value)


def load_secret(name: str) -> Optional[str]:
    if not keyring:
        return None
    try:
        return keyring.get_password(SERVICE, name)
    except Exception:
        return None


def validate_cookie_header(cookie_header: str) -> bool:
    """Intenta validar el cookie/token haciendo ping al endpoint /v1/user."""
    try:
        import requests

        headers = {
            "Accept": "application/json, text/plain, */*",
            "User-Agent": "suno-assistant/1.0",
            "Origin": "https://studio.suno.ai",
            "Referer": "https://studio.suno.ai/",
            "Cookie": cookie_header,
        }
        url = "https://studio-api.suno.ai/api/v1/user"
        r = requests.get(url, headers=headers, timeout=20)
        return r.status_code in (200, 403) or r.status_code == 401  # 200 ok; 401/403 indican endpoint válido
    except Exception:
        return False


def cookiejar_to_header(cj) -> str:
    parts: List[str] = []
    try:
        for c in cj:
            try:
                dom = (c.domain or "").lstrip(".")
            except Exception:
                dom = ""
            if dom.endswith("suno.ai"):
                parts.append(f"{c.name}={c.value}")
    except Exception:
        pass
    return "; ".join(dict.fromkeys(parts))


def grab_cookie_from_browser(preferred: Optional[str] = None) -> Optional[str]:
    """Intenta leer cookies de tu navegador para studio.suno.ai usando browser-cookie3.
    Nota: Safari no está soportado por browser-cookie3.
    """
    try:
        import browser_cookie3 as bc3  # type: ignore
    except Exception:
        print("Instala 'pip install browser-cookie3' para auto-detectar cookies del navegador.")
        return None

    browsers_order = [
        preferred,
        "chrome",
        "brave",
        "edge",
        "chromium",
        "opera",
        "vivaldi",
        "firefox",
    ]
    browsers_order = [b for b in browsers_order if b]

    for b in browsers_order:
        try:
            if b == "chrome":
                cj = bc3.chrome(domain_name="suno.ai")
            elif b == "brave":
                cj = bc3.brave(domain_name="suno.ai")
            elif b == "edge":
                cj = bc3.edge(domain_name="suno.ai")
            elif b == "chromium":
                cj = bc3.chromium(domain_name="suno.ai")
            elif b == "opera":
                cj = bc3.opera(domain_name="suno.ai")
            elif b == "vivaldi":
                cj = bc3.vivaldi(domain_name="suno.ai")
            elif b == "firefox":
                cj = bc3.firefox(domain_name="suno.ai")
            else:
                continue

            header = cookiejar_to_header(cj)
            if header:
                return header
        except Exception:
            continue
    return None


def run_export(out: Path, concurrency: int, limit: int = 0) -> int:
    env = dict(os.environ)
    # Prioridad: Bearer > Cookie
    bearer = load_secret("SUNO_BEARER")
    cookie = load_secret("SUNO_COOKIE")
    if bearer:
        env["SUNO_BEARER"] = bearer
    if cookie and not bearer:
        env["SUNO_COOKIE"] = cookie

    cmd = [
        sys.executable,
        str(Path(__file__).parent / "suno_export.py"),
        "--out",
        str(out),
        "--concurrency",
        str(concurrency),
    ]
    if limit > 0:
        cmd += ["--limit", str(limit)]
    print("Ejecutando:", " ".join(cmd))
    return subprocess.call(cmd, env=env)


SC_GENRE_MAP = {
    # Mapeo básico a los géneros de SoundCloud
    "techno": "Techno",
    "house": "House",
    "deep house": "House",
    "electronic": "Electronic",
    "edm": "Electronic",
    "ambient": "Ambient",
    "trance": "Trance",
    "drum and bass": "Drum & Bass",
    "dnb": "Drum & Bass",
    "dubstep": "Dubstep",
    "trap": "Trap",
    "hip hop": "Hip-hop & Rap",
    "hip-hop": "Hip-hop & Rap",
    "hip-hop & rap": "Hip-hop & Rap",
    "rap": "Hip-hop & Rap",
    "pop": "Pop",
    "rock": "Rock",
    "experimental": "Experimental",
    "latin": "Latin",
    "reggaeton": "Reggaeton",
}


def normalize_genre(g: str) -> str:
    if not g:
        return "Electronic"
    k = g.strip().lower()
    return SC_GENRE_MAP.get(k, g if g[0].isupper() else g.title())


def build_description(row: Dict[str, str]) -> str:
    parts: List[str] = []
    if row.get("style"):
        parts.append(f"Style: {row['style']}")
    if row.get("genre"):
        parts.append(f"Genre: {row['genre']}")
    if row.get("persona"):
        parts.append(f"Persona: {row['persona']}")
    if row.get("weirdness"):
        parts.append(f"Weirdness: {row['weirdness']}")
    if row.get("prompt"):
        parts.append("")
        parts.append("Prompt:")
        parts.append(row["prompt"]) 
    if row.get("lyrics"):
        parts.append("")
        parts.append("Lyrics:")
        parts.append(row["lyrics"]) 
    return "\n".join(parts).strip()


def prep_soundcloud(manifest: Path, audio_dir: Path, out_dir: Path, artwork_dir: Optional[Path] = None) -> Path:
    out_dir.mkdir(parents=True, exist_ok=True)
    desc_dir = out_dir / "descriptions"
    desc_dir.mkdir(parents=True, exist_ok=True)

    data = json.loads(manifest.read_text(encoding="utf-8"))
    if not isinstance(data, list):
        raise RuntimeError("Manifiesto JSON inesperado.")

    csv_path = out_dir / "soundcloud_upload.csv"
    with open(csv_path, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(
            f,
            fieldnames=[
                "title",
                "genre",
                "tags",
                "description_path",
                "audio_path",
                "artwork_path",
                "privacy",
                "downloads",
            ],
        )
        writer.writeheader()

        for it in data:
            title = it.get("title") or "untitled"
            genre = normalize_genre(it.get("genre", "Electronic"))

            tags_parts: List[str] = []
            if it.get("style"):
                tags_parts.append(str(it["style"]))
            if it.get("persona_used") == "true" and it.get("persona"):
                tags_parts.append(str(it["persona"]))
            if it.get("weirdness"):
                tags_parts.append(f"weirdness:{it['weirdness']}")
            if it.get("tags"):
                tags_parts.extend([x.strip() for x in str(it["tags"]).split(",") if x.strip()])
            tags = " ".join(dict.fromkeys(tags_parts))

            desc = build_description(it)
            desc_file = desc_dir / f"{it.get('id','noid')}.txt"
            desc_file.write_text(desc, encoding="utf-8")

            audio_name = it.get("suggested_filename") or f"{title}__{it.get('id','noid')}.wav"
            audio_path = audio_dir / audio_name
            art_path = ""
            if artwork_dir and artwork_dir.exists():
                # Heurística: usar la misma base que el título si existe
                base = Path(str(title)).stem
                candidate = artwork_dir / f"{base}.jpg"
                if candidate.exists():
                    art_path = str(candidate)
                else:
                    # primer JPG/PNG del directorio
                    for ext in ("*.jpg", "*.jpeg", "*.png"):
                        files = list(artwork_dir.glob(ext))
                        if files:
                            art_path = str(files[0])
                            break

            writer.writerow(
                {
                    "title": title,
                    "genre": genre,
                    "tags": tags,
                    "description_path": str(desc_file),
                    "audio_path": str(audio_path),
                    "artwork_path": art_path,
                    "privacy": "public",
                    "downloads": "false",
                }
            )

    return csv_path


def main(argv: Optional[List[str]] = None) -> int:
    p = argparse.ArgumentParser(description="Suno Assistant (setup/export/soundcloud prep)")
    sub = p.add_subparsers(dest="cmd", required=True)

    p_setup = sub.add_parser("setup", help="Guarda token/cookie en el llavero")
    p_setup.add_argument("--bearer", default=None, help="Token Bearer (opcional si pegas cookie)")
    p_setup.add_argument("--cookie", default=None, help="Cookie completa (opcional si pasas bearer)")
    p_setup.add_argument("--auto", action="store_true", help="Intentar leer cookie desde el navegador (browser-cookie3)")
    p_setup.add_argument("--browser", default=None, help="Forzar navegador: chrome|brave|edge|chromium|opera|vivaldi|firefox")

    p_export = sub.add_parser("export", help="Exportar WAVs desde Suno usando llavero")
    p_export.add_argument("--out", default="exports/suno", help="Directorio destino")
    p_export.add_argument("--concurrency", type=int, default=4)
    p_export.add_argument("--limit", type=int, default=0)
    p_export.add_argument("--open", action="store_true", help="Abrir carpeta al terminar")

    p_prep = sub.add_parser("prep-soundcloud", help="Generar CSV + descripciones para SoundCloud")
    p_prep.add_argument("--manifest", required=True, help="Ruta a suno_export_manifest.json")
    p_prep.add_argument("--audio-dir", default="exports/suno", help="Carpeta con WAVs")
    p_prep.add_argument("--out", default="exports/soundcloud", help="Salida")
    p_prep.add_argument("--artwork-dir", default=None, help="Carpeta con portadas (opcional)")

    p_check = sub.add_parser("check", help="Verifica si hay token/cookie y los valida con la API")
    p_check.add_argument("--verbose", action="store_true", help="Muestra códigos de respuesta")

    p_sync = sub.add_parser("sync", help="Ejecuta export en bucle cada N segundos")
    p_sync.add_argument("--out", default="exports/suno", help="Carpeta de salida")
    p_sync.add_argument("--concurrency", type=int, default=4)
    p_sync.add_argument("--limit", type=int, default=0)
    p_sync.add_argument("--interval", type=int, default=300, help="Segundos entre corridas (por defecto 300 = 5min)")
    p_sync.add_argument("--only-new", action="store_true", help="Pedir solo nuevos (requiere estado en --out)")
    p_sync.add_argument("--open-on-first", action="store_true", help="Abrir carpeta tras la primera corrida")

    # macOS LaunchAgent installer
    p_inst = sub.add_parser("install-macos-agent", help="Instala LaunchAgent para sync periódico (macOS)")
    p_inst.add_argument("--out", default="exports/suno", help="Carpeta de salida")
    p_inst.add_argument("--interval", type=int, default=300)
    p_inst.add_argument("--label", default="com.suno.sync", help="Etiqueta del LaunchAgent")
    p_inst.add_argument("--only-new", action="store_true")
    p_inst.add_argument("--concurrency", type=int, default=4)
    p_inst.add_argument("--limit", type=int, default=0)

    p_uninst = sub.add_parser("uninstall-macos-agent", help="Desinstala LaunchAgent (macOS)")
    p_uninst.add_argument("--label", default="com.suno.sync")

    args = p.parse_args(argv)

    if args.cmd == "setup":
        if not keyring:
            print("Instala 'pip install keyring' para usar el llavero.", file=sys.stderr)
            return 2
        bearer = args.bearer
        cookie = args.cookie
        if args.auto and not bearer and not cookie:
            cookie = grab_cookie_from_browser(preferred=args.browser)
            if cookie:
                print("Cookie obtenida desde el navegador.")
            else:
                print("No se pudo obtener cookie automáticamente.")
        if not bearer and not cookie:
            print("Pega uno de los dos (Authorization Bearer o Cookie). Puedes relanzar con --bearer o --cookie.")
            try:
                bearer = getpass.getpass("Bearer (enter para omitir): ").strip() or None
            except Exception:
                bearer = input("Bearer (enter para omitir): ").strip() or None
            if not bearer:
                try:
                    cookie = getpass.getpass("Cookie (pega valor completo): ").strip() or None
                except Exception:
                    cookie = input("Cookie (pega valor completo): ").strip() or None
        if bearer:
            store_secret("SUNO_BEARER", bearer)
            print("Guardado SUNO_BEARER en el llavero del sistema.")
        if cookie and not bearer:
            store_secret("SUNO_COOKIE", cookie)
            print("Guardado SUNO_COOKIE en el llavero del sistema.")
            ok = validate_cookie_header(cookie)
            if ok:
                print("Cookie validada con /v1/user.")
            else:
                print("Advertencia: no se pudo validar la cookie ahora. La exportación lo reintentará.")
        if not bearer and not cookie:
            print("No se guardó nada.")
        return 0

    if args.cmd == "export":
        code = run_export(Path(args.out), args.concurrency, args.limit)
        if args.open:
            open_folder(Path(args.out))
        return code

    if args.cmd == "prep-soundcloud":
        artwork_dir = Path(args.artwork_dir) if args.artwork_dir else None
        csv_path = prep_soundcloud(Path(args.manifest), Path(args.audio_dir), Path(args.out), artwork_dir)
        print("CSV listo:", csv_path)
        open_folder(Path(args.out))
        return 0

    if args.cmd == "check":
        bearer = load_secret("SUNO_BEARER")
        cookie = load_secret("SUNO_COOKIE")
        has_bearer = bool(bearer)
        has_cookie = bool(cookie)
        print(f"Bearer guardado: {'sí' if has_bearer else 'no'}")
        print(f"Cookie guardada: {'sí' if has_cookie else 'no'}")
        ok = False
        code = None
        try:
            import requests
            hdrs = {
                "Accept": "application/json, text/plain, */*",
                "User-Agent": "suno-assistant/1.0",
                "Origin": "https://studio.suno.ai",
                "Referer": "https://studio.suno.ai/",
            }
            if bearer:
                hdrs["Authorization"] = f"Bearer {bearer}"
            elif cookie:
                hdrs["Cookie"] = cookie
            r = requests.get("https://studio-api.suno.ai/api/v1/user", headers=hdrs, timeout=20)
            code = r.status_code
            ok = code in (200, 403) or code == 401
        except Exception:
            ok = False
        print(f"Validez API: {'OK' if ok else 'NO'}" + (f" (HTTP {code})" if args.verbose and code else ""))
        return 0 if ok else 1

    if args.cmd == "sync":
        from time import sleep
        first = True
        print(f"Iniciando sync cada {args.interval}s. Ctrl+C para detener.")
        while True:
            export_args = [
                str(Path(args.out)),
                str(args.concurrency),
                str(args.limit),
            ]
            # Usamos run_export pero agregamos flags adicionales pasando variables de entorno
            env = dict(os.environ)
            bearer = load_secret("SUNO_BEARER")
            cookie = load_secret("SUNO_COOKIE")
            if bearer:
                env["SUNO_BEARER"] = bearer
            if cookie and not bearer:
                env["SUNO_COOKIE"] = cookie

            cmd = [
                sys.executable,
                str(Path(__file__).parent / "suno_export.py"),
                "--out", str(Path(args.out)),
                "--concurrency", str(args.concurrency),
            ]
            if args.limit > 0:
                cmd += ["--limit", str(args.limit)]
            if args.only-new:
                cmd += ["--only-new"]
            print("\n—"*20)
            print("[SYNC] Ejecutando:", " ".join(cmd))
            code = subprocess.call(cmd, env=env)
            if first and args.open-on-first:
                open_folder(Path(args.out))
            first = False
            # Espera
            sleep(max(10, args.interval))

    if args.cmd == "install-macos-agent":
        if get_os() != "mac":
            print("Este comando es solo para macOS.", file=sys.stderr)
            return 2
        label = args.label
        launch_agents = Path.home() / "Library" / "LaunchAgents"
        launch_agents.mkdir(parents=True, exist_ok=True)
        plist_path = launch_agents / f"{label}.plist"

        repo_root = Path(__file__).resolve().parent.parent
        python_exec = sys.executable
        program = [
            python_exec,
            str(Path(__file__).parent / "suno_assistant.py"),
            "sync",
            "--out", str(Path(args.out).resolve()),
            "--interval", str(args.interval),
            "--concurrency", str(args.concurrency),
        ]
        if args.limit > 0:
            program += ["--limit", str(args.limit)]
        if args.only_new:
            program += ["--only-new"]

        log_dir = Path.home() / "Library" / "Logs"
        stdout_log = str(log_dir / "suno_sync.out.log")
        stderr_log = str(log_dir / "suno_sync.err.log")

        # Construir plist
        from plistlib import dumps as plist_dumps
        plist_dict = {
            "Label": label,
            "ProgramArguments": program,
            "WorkingDirectory": str(repo_root),
            "StartInterval": int(args.interval),
            "RunAtLoad": True,
            "StandardOutPath": stdout_log,
            "StandardErrorPath": stderr_log,
            # No KeepAlive: dejamos que StartInterval lo ejecute
        }
        plist_path.write_bytes(plist_dumps(plist_dict))

        # Cargar
        try:
            subprocess.run(["launchctl", "unload", str(plist_path)], check=False)
        except Exception:
            pass
        r = subprocess.run(["launchctl", "load", str(plist_path)], check=False)
        if r.returncode != 0:
            # Intento moderno
            try:
                uid = os.getuid()
                subprocess.run(["launchctl", "bootstrap", f"gui/{uid}", str(plist_path)], check=False)
            except Exception:
                pass
        print(f"LaunchAgent instalado: {plist_path}")
        print("Ver logs en Console.app o en ~/Library/Logs/suno_sync.*.log")
        return 0

    if args.cmd == "uninstall-macos-agent":
        if get_os() != "mac":
            print("Este comando es solo para macOS.", file=sys.stderr)
            return 2
        label = args.label
        plist_path = Path.home() / "Library" / "LaunchAgents" / f"{label}.plist"
        try:
            subprocess.run(["launchctl", "unload", str(plist_path)], check=False)
        except Exception:
            pass
        if plist_path.exists():
            plist_path.unlink()
        print(f"LaunchAgent desinstalado: {label}")
        return 0

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
