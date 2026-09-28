"""
Chip-style sound effects for the pygame display - ported from
retro-tron-battle/server/tron_server.py (same sounds, same triggers in
display/pygame_loop.py).
"""

import array
import math
import random


def _pulse(freq_fn, dur, sr, duty=0.5, vol=0.5, decay=0.0):
    """Pulse wave; freq_fn(t) -> Hz, decay = exponential fade rate (1/s)."""
    out = []
    ph = 0.0
    for i in range(int(sr * dur)):
        t = i / sr
        ph = (ph + freq_fn(t) / sr) % 1.0
        out.append((vol if ph < duty else -vol) * math.exp(-decay * t))
    return out


def _noise(clock_fn, dur, sr, vol=0.5, decay=0.0, seed=1):
    """Noise that picks a new random level clock_fn(t) times a second -
    high clock = hiss, low clock = rumble (the classic explosion trick)."""
    rng = random.Random(seed)
    out = []
    acc = 0.0
    cur = 0.0
    for i in range(int(sr * dur)):
        t = i / sr
        acc += clock_fn(t) / sr
        if acc >= 1.0:
            acc -= int(acc)
            cur = rng.uniform(-1.0, 1.0)
        out.append(cur * vol * math.exp(-decay * t))
    return out


def _sine(freq_fn, dur, sr, vol=0.5, decay=0.0):
    out = []
    ph = 0.0
    for i in range(int(sr * dur)):
        t = i / sr
        ph += 2 * math.pi * freq_fn(t) / sr
        out.append(math.sin(ph) * vol * math.exp(-decay * t))
    return out


def _mix(*parts):
    n = max(len(p) for p in parts)
    return [sum(p[i] for p in parts if i < len(p)) for i in range(n)]


def _note(n):
    """MIDI note number -> Hz (60 = middle C)."""
    return 440.0 * 2 ** ((n - 69) / 12)


def _fanfare(sr, win):
    out = []
    if win:
        # C-E-G-C run up, then a SID-style arpeggiated major chord
        for n in (72, 76, 79, 84):
            out += _pulse(lambda t, f=_note(n): f, 0.09, sr, duty=0.25, vol=0.35)
        chord = [_note(n) for n in (84, 88, 91)]
        out += _pulse(lambda t: chord[int(t / 0.03) % 3], 0.9, sr, duty=0.25, vol=0.35, decay=2.5)
    else:
        # sad slide down, then a low arpeggiated minor chord
        for n in (67, 66, 65, 64):
            out += _pulse(lambda t, f=_note(n): f, 0.16, sr, duty=0.5, vol=0.3)
        chord = [_note(n) for n in (52, 55, 59)]
        out += _pulse(lambda t: chord[int(t / 0.04) % 3], 0.8, sr, duty=0.5, vol=0.3, decay=3.0)
    return out


def build_sounds(pygame, volume):
    """Returns {name: pygame.mixer.Sound}, or {} if there's no usable audio
    device - the game then simply runs silent."""
    try:
        if not pygame.mixer.get_init():
            pygame.mixer.init()
        sr, size, channels = pygame.mixer.get_init()
    except Exception as e:
        print(f"[sound] no audio available ({e}) - running silent")
        return {}
    if size != -16:
        print(f"[sound] unexpected mixer format {size} - running silent")
        return {}

    raw = {
        # versus: photos slide in (rising pulse sweep)
        "slide": _pulse(lambda t: 150 + 1500 * t * t, 0.5, sr, duty=0.25, vol=0.25, decay=1.5),
        # versus: "VS" slams in (noise burst getting darker + low thud)
        "slam": _mix(_noise(lambda t: 9000 * math.exp(-6 * t) + 300, 0.7, sr, vol=0.4, decay=5.0),
                     _sine(lambda t: 40 + 80 * math.exp(-12 * t), 0.5, sr, vol=0.55, decay=6.0)),
        # countdown 3, 2, 1 and the GO
        "beep": _pulse(lambda t: 660, 0.14, sr, vol=0.3, decay=6.0),
        "go": _pulse(lambda t: 1320 if t < 0.08 else 1760, 0.45, sr, vol=0.3, decay=3.0),
        # game over: explosion, noise clock falling = pitch dropping
        "crash": _noise(lambda t: 5000 * math.exp(-3 * t) + 150, 1.4, sr, vol=0.6, decay=2.2, seed=7),
        "win": _fanfare(sr, True),
        "draw": _fanfare(sr, False),
    }

    sounds = {}
    for name, samples in raw.items():
        fade = min(len(samples) // 2, int(sr * 0.004))  # 4ms edges, no clicks
        for i in range(fade):
            samples[i] *= i / fade
            samples[-1 - i] *= i / fade
        pcm = array.array("h")
        for s in samples:
            v = int(max(-1.0, min(1.0, s)) * 32767)
            pcm.extend([v] * channels)
        snd = pygame.mixer.Sound(buffer=pcm.tobytes())
        snd.set_volume(volume)
        sounds[name] = snd
    return sounds


# --- Attract-mode music: an original chiptune in a dark, heroic minor-key
# 80s sci-fi mood (not a copy of any film theme). Three SID-style voices -
# pulsing octave bass, fast-arpeggio chords, pulse lead with delayed
# vibrato - plus noise/sine drums. 16 bars at 112 BPM, ~34 s. ---

# (arpeggio chord tones, bass root) per bar - D minor with a borrowed A major
_TUNE_BARS = [
    ((62, 65, 69), 38), ((62, 65, 69), 38), ((58, 62, 65), 34), ((57, 61, 64), 33),   # intro
    ((62, 65, 69), 38), ((58, 62, 65), 34), ((55, 58, 62), 31), ((57, 61, 64), 33),
    ((62, 65, 69), 38), ((53, 57, 60), 29), ((60, 64, 67), 36), ((57, 61, 64), 33),
    ((58, 62, 65), 34), ((60, 64, 67), 36), ((57, 61, 64), 33), ((62, 65, 69), 38),
]
# lead melody as (MIDI note or None for a rest, length in beats), from bar 1
_TUNE_LEAD = [(None, 16),
    (69, 2), (74, 1), (76, 1),   (77, 3), (76, .5), (74, .5),
    (74, 2), (70, 1), (67, 1),   (69, 3), (73, 1),
    (74, 2), (77, 1), (81, 1),   (81, 3), (79, .5), (77, .5),
    (76, 2), (79, 1), (76, 1),   (73, 2), (76, 1), (81, 1),
    (82, 3), (81, 1),            (79, 2), (76, 1), (72, 1),
    (81, 2), (79, 1), (76, 1),   (74, 4),
]
_TUNE_BPM = 112


def _env(samples, sr, attack=0.005, release=0.03):
    a = min(len(samples), int(sr * attack))
    r = min(len(samples), int(sr * release))
    for i in range(a):
        samples[i] *= i / a
    for i in range(r):
        samples[-1 - i] *= i / r
    return samples


def _add(buf, sr, start, samples):
    o = int(start * sr)
    for i, s in enumerate(samples[:len(buf) - o]):
        buf[o + i] += s


def build_music(pygame, volume):
    """Returns the attract tune as a pygame.mixer.Sound, or None without
    audio. Takes a second or two of pure-Python synthesis at startup."""
    try:
        sr, size, channels = pygame.mixer.get_init()
    except Exception:
        return None
    if size != -16:
        return None
    beat = 60.0 / _TUNE_BPM
    total = len(_TUNE_BARS) * 4 * beat
    buf = [0.0] * int(sr * (total + 1.5))  # + room for the last chord to ring

    kick = _sine(lambda t: 45 + 110 * math.exp(-30 * t), 0.2, sr, vol=0.55, decay=16)
    snare = _noise(lambda t: 7000, 0.16, sr, vol=0.22, decay=22, seed=3)
    hat = _noise(lambda t: 16000, 0.04, sr, vol=0.07, decay=70, seed=5)

    for b, (chord, root) in enumerate(_TUNE_BARS):
        t0 = b * 4 * beat
        last = b == len(_TUNE_BARS) - 1
        # pulsing octave bass in 8ths (the final bar holds one long root)
        if last:
            _add(buf, sr, t0, _env(_pulse(lambda t, f=_note(root): f, 4 * beat, sr, vol=0.2, decay=0.6), sr, release=0.4))
        else:
            for k in range(8):
                f = _note(root + (12 if k % 2 else 0))
                _add(buf, sr, t0 + k * beat / 2,
                     _env(_pulse(lambda t, f=f: f, beat / 2 * 0.9, sr, vol=0.18, decay=4.0), sr))
        # fast-arpeggio chord, SID style (one voice cycling 1/25 s per note)
        tones = [_note(n) for n in chord]
        dur = 4 * beat + (1.2 if last else 0)
        _add(buf, sr, t0, _env(_pulse(lambda t, fs=tones: fs[int(t / 0.04) % 3], dur, sr,
                                      duty=0.25, vol=0.07, decay=1.2 if last else 0.25), sr, release=0.05))
        # drums from bar 5 on: kick on 1+3, snare on 2+4, off-beat hats
        if b >= 4 and not last:
            for k in range(4):
                _add(buf, sr, t0 + k * beat, kick if k % 2 == 0 else snare)
                _add(buf, sr, t0 + k * beat + beat / 2, hat)
        elif last:
            _add(buf, sr, t0, kick)

    # lead: pulse wave, vibrato fading in after 0.15 s like a held brass note
    t = 0.0
    for n, beats in _TUNE_LEAD:
        dur = beats * beat
        if n is not None:
            f0 = _note(n)
            fn = lambda x, f0=f0: f0 * (1 + 0.012 * min(1.0, max(0.0, (x - 0.15) * 4)) * math.sin(2 * math.pi * 5.5 * x))
            _add(buf, sr, t, _env(_pulse(fn, dur * 0.95, sr, duty=0.3, vol=0.16, decay=0.35), sr, release=0.06))
        t += dur

    peak = max(abs(s) for s in buf) or 1.0
    pcm = array.array("h")
    for s in buf:
        v = int(s / peak * 0.9 * 32767)
        pcm.extend([v] * channels)
    snd = pygame.mixer.Sound(buffer=pcm.tobytes())
    snd.set_volume(volume)
    return snd
