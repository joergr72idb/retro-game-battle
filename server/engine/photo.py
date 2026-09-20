"""
Visitor photo + platform logo fetching. Ported verbatim (aside from using
config.* instead of module-level constants) from
retro-tron-battle/server/tron_server.py.
"""

import ftplib
import os
import urllib.parse
import urllib.request

import config


def fetch_photo_ftp_blocking(pin: str):
    pin = (pin or "").strip()
    if not pin or pin.upper() in ("NONE", "-"):
        return None

    os.makedirs(config.PHOTO_CACHE_DIR, exist_ok=True)
    cached = os.path.join(config.PHOTO_CACHE_DIR, pin + ".jpg")
    if os.path.exists(cached):
        return cached

    remote_path = f"{pin}/{config.HTTP_PHOTO_FILENAME}"
    try:
        ftp = ftplib.FTP()
        ftp.connect(config.FTP_HOST, config.FTP_PORT, timeout=8)
        ftp.login(config.FTP_USER, config.FTP_PASS)
        if config.FTP_REMOTE_DIR:
            ftp.cwd(config.FTP_REMOTE_DIR)
        with open(cached, "wb") as f:
            ftp.retrbinary(f"RETR {remote_path}", f.write)
        ftp.quit()
        return cached
    except Exception as e:
        print(f"[ftp] couldn't fetch photo for PIN '{pin}' ({remote_path}): {e}")
        try:
            os.remove(cached)
        except OSError:
            pass
    return None


def fetch_photo_http_blocking(pin: str):
    pin = (pin or "").strip()
    if not pin or pin.upper() in ("NONE", "-"):
        return None

    os.makedirs(config.PHOTO_CACHE_DIR, exist_ok=True)
    cached = os.path.join(config.PHOTO_CACHE_DIR, pin + ".jpg")
    if os.path.exists(cached):
        return cached

    url = f"{config.HTTP_PHOTO_BASE_URL}/{urllib.parse.quote(pin)}/{config.HTTP_PHOTO_FILENAME}"
    try:
        req = urllib.request.Request(url, headers={"User-Agent": "RetroGameBattle/1.0"})
        with urllib.request.urlopen(req, timeout=8) as resp:
            data = resp.read()
        with open(cached, "wb") as f:
            f.write(data)
        return cached
    except Exception as e:
        print(f"[http-photo] couldn't fetch photo for PIN '{pin}' from {url}: {e}")
    return None


def fetch_photo_blocking(pin: str):
    if config.PHOTO_SOURCE.upper() == "HTTP":
        return fetch_photo_http_blocking(pin)
    return fetch_photo_ftp_blocking(pin)


def logo_path_for(platform: str):
    for base in config.LOGO_NAME_CANDIDATES.get(platform, []):
        for ext in config.LOGO_EXTENSIONS:
            p = os.path.join(config.LOGO_DIR, base + ext)
            if os.path.exists(p):
                return p
    return None


def event_logo_path():
    for ext in config.LOGO_EXTENSIONS:
        p = os.path.join(config.LOGO_DIR, "logo" + ext)
        if os.path.exists(p):
            return p
    return None
