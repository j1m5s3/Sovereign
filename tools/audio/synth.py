"""Sovereign's sound, made by script (the art toolchain's rule: nothing downloaded): effects, ambience and music,
synthesised with numpy and written as 16-bit WAV files that tools/audio/ue_import_audio.py brings into
unreal/Content/Audio/ as sound assets.

  blender -b --factory-startup --python tools/audio/synth.py -- art/build/audio [name ...]

Instruments: a plucked string (Karplus-Strong, for lute, harp and kithara), a breathy pipe (sine partials with
vibrato and noise), a bowed pad (detuned partials, slow swell), a frame drum and a bell (inharmonic partials).
Every sound passes through a convolution hall (decaying noise), so they share one room. Music is composed by a
seeded generator: modal phrases over a drone, call and answer, cadences home.
"""
import math
import os
import sys
import wave

import numpy as np

SR = 44100
rng = np.random.default_rng(1)


# ------------------------------------------------------------------------------------------- building blocks

def t_axis(seconds):
    return np.arange(int(seconds * SR)) / SR


def env(n, attack=0.005, release=0.1, hold=None):
    """Attack-hold-release envelope over n samples (exponential release)."""
    e = np.ones(n)
    a = max(1, int(attack * SR))
    e[:a] = np.linspace(0, 1, a)
    r = max(1, int(release * SR))
    start = n - r if hold is None else min(n - r, int(hold * SR))
    if start > 0:
        e[start:] *= np.exp(-np.linspace(0, 6, n - start))
    return e


def fft_lowpass(x, cutoff, slope=2.0):
    """A smooth low-pass in the frequency domain (fast for long signals)."""
    X = np.fft.rfft(x)
    f = np.fft.rfftfreq(len(x), 1 / SR)
    X *= 1 / (1 + (f / cutoff) ** (2 * slope)) ** 0.5
    return np.fft.irfft(X, len(x))


def fft_highpass(x, cutoff, slope=2.0):
    X = np.fft.rfft(x)
    f = np.fft.rfftfreq(len(x), 1 / SR)
    X *= 1 - 1 / (1 + (f / cutoff) ** (2 * slope)) ** 0.5
    return np.fft.irfft(X, len(x))


def hall(x, seconds=2.2, wet=0.28, damp=3500.0):
    """Convolution with a decaying noise tail, darker as it fades: one shared room for everything."""
    n = int(seconds * SR)
    tail = rng.standard_normal(n) * np.exp(-np.linspace(0, 7, n))
    tail = fft_lowpass(tail, damp)
    tail[: int(0.012 * SR)] = 0  # pre-delay
    tail /= np.sqrt(np.sum(tail ** 2))
    size = len(x) + n
    nfft = 1 << (size - 1).bit_length()
    y = np.fft.irfft(np.fft.rfft(x, nfft) * np.fft.rfft(tail, nfft), nfft)[:size]
    out = np.zeros(size)
    out[: len(x)] = x * (1 - wet)
    out += y * wet * 2.2
    return out


def pluck(freq, seconds, bright=0.6, decay=0.996):
    """Karplus-Strong plucked string, computed a period at a time."""
    n = int(seconds * SR)
    p = max(2, int(SR / freq))
    burst = rng.uniform(-1, 1, p)
    burst = bright * burst + (1 - bright) * np.convolve(burst, np.ones(4) / 4, mode="same")
    out = np.zeros(n + p)
    out[:p] = burst
    prev = burst
    for start in range(p, n + p, p):
        block = 0.5 * (prev + np.roll(prev, 1)) * decay
        end = min(start + p, n + p)
        out[start:end] = block[: end - start]
        prev = block
    y = out[:n]
    return y * env(n, 0.002, min(0.3, seconds * 0.3))


def pipe(freq, seconds, vibrato=5.0, breath=0.08):
    t = t_axis(seconds)
    vib = 1 + 0.006 * np.sin(2 * math.pi * vibrato * t) * np.clip(t * 2, 0, 1)
    phase = 2 * math.pi * freq * np.cumsum(vib) / SR
    y = np.sin(phase) + 0.25 * np.sin(2 * phase) + 0.08 * np.sin(3 * phase)
    noise = fft_lowpass(rng.standard_normal(len(t)), freq * 3) * breath
    return (y * 0.7 + noise) * env(len(t), 0.06, 0.15)


def pad(freq, seconds, partials=6, detune=0.004):
    t = t_axis(seconds)
    y = np.zeros(len(t))
    for k in range(1, partials + 1):
        for d in (-detune, 0, detune):
            y += np.sin(2 * math.pi * freq * k * (1 + d) * t + rng.uniform(0, 6.28)) / (k ** 1.3)
    swell = np.clip(t / (seconds * 0.35), 0, 1) * np.clip((seconds - t) / (seconds * 0.35), 0, 1)
    return y / 6 * swell


def drum(seconds=0.6, pitch=90.0, skin=0.4):
    t = t_axis(seconds)
    body = np.sin(2 * math.pi * pitch * t * (1 + 0.6 * np.exp(-t * 30))) * np.exp(-t * 9)
    slap = fft_lowpass(rng.standard_normal(len(t)), 1800) * np.exp(-t * 40) * skin
    return body + slap


def bell(freq, seconds=2.5):
    t = t_axis(seconds)
    y = np.zeros(len(t))
    for ratio, amp, dec in [(1.0, 1.0, 1.6), (2.0, 0.5, 2.4), (2.76, 0.4, 3.0), (5.4, 0.25, 5.0), (8.9, 0.12, 8.0)]:
        y += amp * np.sin(2 * math.pi * freq * ratio * t) * np.exp(-t * dec)
    return y * env(len(t), 0.002, 0.2)


def noise_burst(seconds, cutoff=3000.0, decay=20.0, high=None):
    t = t_axis(seconds)
    n = fft_lowpass(rng.standard_normal(len(t)), cutoff)
    if high:
        n = fft_highpass(n, high)
    return n * np.exp(-t * decay)


def mix(length, *parts):
    """Lay (offset seconds, signal, gain) parts into one buffer."""
    out = np.zeros(int(length * SR))
    for at, sig, gain in parts:
        i = int(at * SR)
        j = min(len(out), i + len(sig))
        if j > i:
            out[i:j] += sig[: j - i] * gain
    return out


def note(semitones_from_a4):
    return 440.0 * 2 ** (semitones_from_a4 / 12)


def write(path, x, peak=0.89):
    x = np.asarray(x, dtype=np.float64)
    x = x - np.mean(x)
    m = np.max(np.abs(x)) or 1.0
    x = x / m * peak
    fade = min(len(x) // 4, int(0.01 * SR))
    if fade > 0:
        x[-fade:] *= np.linspace(1, 0, fade)
    data = (x * 32767).astype("<i2")
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(data.tobytes())


# ------------------------------------------------------------------------------------------- effects

def sfx_click():
    return mix(0.12, (0, noise_burst(0.05, 6000, 120, high=1500), 0.6), (0, np.sin(2 * math.pi * 1400 * t_axis(0.05)) * np.exp(-t_axis(0.05) * 90), 0.5))


def sfx_select():
    return mix(0.25, (0, pluck(note(3), 0.25, 0.8), 0.8), (0.04, pluck(note(10), 0.2, 0.8), 0.5))


def sfx_end_turn():
    return hall(mix(2.2, (0, bell(note(-9)), 0.9), (0.0, bell(note(-2)), 0.4)), 2.0, 0.3)


def sfx_new_turn():
    return hall(mix(1.2, (0, pluck(note(-5), 1.0), 0.7), (0.09, pluck(note(2), 1.0), 0.6), (0.18, pluck(note(7), 1.0), 0.6)), 1.6, 0.3)


def sfx_found_city():
    # A brassy fanfare from pipes in fifths over a drum roll.
    roll = mix(1.0, *[(k * 0.06, drum(0.3, 110, 0.6), 0.3 + 0.05 * k) for k in range(8)])
    horn = mix(2.4, (0.45, pipe(note(-14), 0.5, 0, 0.02), 0.6), (0.45, pipe(note(-7), 0.5, 0, 0.02), 0.5),
               (0.95, pipe(note(-9), 1.3, 4, 0.02), 0.7), (0.95, pipe(note(-2), 1.3, 4, 0.02), 0.5), (0.95, pipe(note(-21), 1.3, 0, 0.02), 0.5))
    return hall(mix(2.6, (0, roll, 1.0), (0, horn, 1.0)), 2.4, 0.3)


def sfx_research():
    return hall(mix(1.6, *[(k * 0.11, bell(note(n), 1.4), 0.5) for k, n in enumerate((3, 7, 10, 15))]), 2.0, 0.35)


def sfx_civic():
    return hall(mix(1.8, *[(k * 0.14, pluck(note(n), 1.2, 0.5), 0.7) for k, n in enumerate((-2, 2, 5, 9, 14))]), 2.0, 0.35)


def sfx_notify():
    return hall(mix(0.8, (0, bell(note(15), 0.7), 0.6), (0.07, bell(note(22), 0.6), 0.35)), 1.2, 0.25)


def sfx_clash():
    """Swords: a bright metallic ring over a struck thud."""
    t = t_axis(0.8)
    ring = sum(np.sin(2 * math.pi * f * t) * np.exp(-t * d) for f, d in [(2350, 9), (3470, 12), (5120, 16), (1730, 7)])
    return hall(mix(0.9, (0, noise_burst(0.15, 7000, 35, high=800), 0.8), (0, ring, 0.25), (0, drum(0.3, 70, 0.8), 0.7)), 1.2, 0.2)


def sfx_bow():
    t = t_axis(0.5)
    twang = np.sin(2 * math.pi * 180 * t * (1 - 0.15 * t)) * np.exp(-t * 14)
    whoosh = fft_highpass(fft_lowpass(rng.standard_normal(len(t)), 3500), 600) * np.sin(np.pi * np.clip(t / 0.45, 0, 1)) * 0.5
    return hall(mix(0.7, (0, twang, 0.8), (0.05, whoosh, 0.7)), 1.0, 0.2)


def sfx_gunshot():
    return hall(mix(1.0, (0, noise_burst(0.5, 5000, 18), 1.0), (0, drum(0.4, 55, 0.2), 0.9)), 1.6, 0.25)


def sfx_cannon():
    return hall(mix(1.8, (0, noise_burst(1.2, 1600, 5), 1.0), (0, drum(1.0, 38, 0.1), 1.0)), 2.2, 0.3)


def sfx_march():
    """Footfalls in step: a moving army."""
    steps = [(k * 0.27, fft_lowpass(drum(0.12, 60 + 8 * (k % 2), 0.9), 900), 0.5 + 0.05 * (k % 3)) for k in range(4)]
    return mix(1.2, *steps)


def sfx_hooves():
    pattern = [0.0, 0.08, 0.2, 0.28, 0.5, 0.58, 0.7, 0.78]
    return mix(1.0, *[(p, fft_lowpass(drum(0.08, 120, 1.0), 2500), 0.6) for p in pattern])


def sfx_war():
    beats = [(k * 0.42, drum(0.7, 55, 0.3), 1.0 if k % 2 == 0 else 0.7) for k in range(6)]
    horn = pipe(note(-26), 2.0, 2, 0.03)
    return hall(mix(3.0, *beats, (0.8, horn, 0.5)), 2.5, 0.3)


def sfx_era():
    chord = [(-21, 0.5), (-14, 0.4), (-9, 0.35), (-2, 0.3)]
    swell = mix(4.0, *[(0, pad(note(n), 3.8), g) for n, g in chord])
    return hall(mix(4.5, (0, swell, 1.0), (1.4, bell(note(3), 2.5), 0.4), (1.6, bell(note(10), 2.5), 0.3)), 3.0, 0.35)


def sfx_victory():
    seq = [(-9, 0.0), (-5, 0.3), (-2, 0.6), (3, 0.9)]
    horns = mix(4.0, *[(at, pipe(note(n), 1.6 if k == 3 else 0.35, 4, 0.02), 0.6) for k, (n, at) in enumerate(seq)])
    return hall(mix(5.0, (0, horns, 1.0), (0.9, pad(note(-21), 3.5), 0.4), (0.9, pad(note(-14), 3.5), 0.3)), 3.0, 0.35)


def sfx_defeat():
    seq = [(-9, 0.0), (-12, 0.5), (-14, 1.0), (-17, 1.5)]
    return hall(mix(4.0, *[(at, pad(note(n), 1.6), 0.5) for n, at in seq]), 3.0, 0.4)


def sfx_build():
    hits = [(k * 0.22, fft_lowpass(noise_burst(0.1, 4000, 50), 3000) + drum(0.1, 200, 0.4) * 0.3, 0.6) for k in range(3)]
    return hall(mix(0.9, *hits), 0.8, 0.2)


def sfx_coins():
    t = t_axis(0.25)
    return hall(mix(0.8, *[(k * 0.06, np.sin(2 * math.pi * (4200 + 300 * k) * t) * np.exp(-t * 30), 0.35) for k in range(5)]), 0.8, 0.2)


# ------------------------------------------------------------------------------------------- ambience (loops)

def loopable(x, xfade=2.0):
    """Cross-fades the end into the start so the loop has no seam."""
    n = int(xfade * SR)
    head, tail = x[:n].copy(), x[-n:].copy()
    w = np.linspace(0, 1, n)
    x = x[:-n].copy()
    x[:n] = tail * (1 - w) + head * w
    return x


def amb_country(seconds=48.0):
    """Wind through grass and, now and then, birds."""
    t = t_axis(seconds)
    gust = 0.6 + 0.4 * np.sin(2 * math.pi * t / 11.0) * np.sin(2 * math.pi * t / 4.3 + 1.0)
    wind = fft_lowpass(rng.standard_normal(len(t)), 700) * gust
    out = wind * 0.5
    for k in range(18):
        at = rng.uniform(0, seconds - 1.5)
        base = rng.uniform(2400, 4200)
        tt = t_axis(rng.uniform(0.25, 0.6))
        chirps = np.zeros(len(tt))
        for c in range(rng.integers(2, 5)):
            s, e = int(c * len(tt) / 4), int((c + 0.6) * len(tt) / 4)
            seg = tt[: e - s]
            chirps[s:e] += np.sin(2 * math.pi * (base + 900 * np.sin(2 * math.pi * 12 * seg)) * seg) * np.sin(np.pi * np.linspace(0, 1, e - s))
        out = out + mix(seconds, (at, chirps, rng.uniform(0.04, 0.09)))
    return loopable(hall(out, 1.5, 0.15)[: len(t)])


def amb_town(seconds=40.0):
    """A market's murmur: many soft voices (formant noise), footsteps, a cart, a distant hammer."""
    t = t_axis(seconds)
    murmur = np.zeros(len(t))
    for f0 in (300, 520, 900, 1400, 2300):
        band = fft_highpass(fft_lowpass(rng.standard_normal(len(t)), f0 * 1.4), f0 * 0.7)
        murmur += band * (0.6 + 0.4 * np.sin(2 * math.pi * t * rng.uniform(0.2, 1.1) + rng.uniform(0, 6)))
    out = murmur * 0.4
    for k in range(40):
        out = out + mix(seconds, (rng.uniform(0, seconds - 0.2), fft_lowpass(drum(0.08, 90, 1.0), 1500), rng.uniform(0.05, 0.12)))
    for k in range(6):
        at = rng.uniform(0, seconds - 1.0)
        out = out + mix(seconds, *[(at + j * 0.5, noise_burst(0.06, 5000, 60) + drum(0.06, 300, 0.5) * 0.2, 0.08) for j in range(3)])
    return loopable(hall(out, 1.4, 0.2)[: len(t)])


def amb_battle(seconds=30.0):
    """Clamour: shouts (formant bursts), clashes, drums."""
    t = t_axis(seconds)
    roar = fft_lowpass(rng.standard_normal(len(t)), 900) * (0.5 + 0.3 * np.sin(2 * math.pi * t / 5.0))
    out = roar * 0.5
    clash = sfx_clash()
    for k in range(45):
        out = out + mix(seconds, (rng.uniform(0, seconds - 1), clash, rng.uniform(0.08, 0.25)))
    for k in range(int(seconds / 0.6)):
        out = out + mix(seconds, (k * 0.6, drum(0.4, 60, 0.3), 0.25 if k % 2 == 0 else 0.15))
    return loopable(hall(out, 1.8, 0.25)[: len(t)])


# ------------------------------------------------------------------------------------------- music

MODES = {"dorian": [0, 2, 3, 5, 7, 9, 10], "mixolydian": [0, 2, 4, 5, 7, 9, 10], "major": [0, 2, 4, 5, 7, 9, 11],
         "aeolian": [0, 2, 3, 5, 7, 8, 10], "pentatonic": [0, 2, 4, 7, 9]}


def compose(seed, mode, root, bpm, bars, lead, bass_pulse=True, drums=True, pads=True):
    """A seeded modal piece: four-bar phrases (call, answer, call varied, cadence) over a drone and slow chords."""
    r = np.random.default_rng(seed)
    scale = MODES[mode]
    beat = 60.0 / bpm
    length = bars * 4 * beat + 6.0
    parts = []
    degree = 0

    def pitch(deg, octave=0):
        o, d = divmod(deg, len(scale))
        return root + 12 * (o + octave) + scale[d]

    motif = [int(x) for x in r.integers(-2, 3, 6)]
    for bar in range(bars):
        phrase_pos = bar % 4
        chord_root = [0, 3, 4, 0][phrase_pos] if mode != "pentatonic" else [0, 2, 3, 0][phrase_pos]
        start = bar * 4 * beat
        if pads and bar % 2 == 0:
            for d in (chord_root, chord_root + 2, chord_root + 4):
                parts.append((start, pad(note(pitch(d, -1)), 8 * beat + 1.0, 5), 0.13))
        if bass_pulse:
            parts.append((start, pluck(note(pitch(chord_root, -2)), 4 * beat, 0.3, 0.998), 0.35))
            parts.append((start + 2 * beat, pluck(note(pitch(chord_root + 4, -2)), 2 * beat, 0.3, 0.998), 0.25))
        if drums:
            for b in range(4):
                parts.append((start + b * beat, drum(0.5, 80 if b % 2 == 0 else 120, 0.5), 0.32 if b == 0 else 0.16))
            if r.random() < 0.5:
                parts.append((start + 3.5 * beat, drum(0.3, 140, 0.6), 0.12))
        # The melody: the motif in call bars, a free answer, and a cadence home on the fourth bar.
        tpos = start
        steps = [1, 1, 0.5, 0.5, 1] if phrase_pos != 3 else [1, 1, 2]
        for k, dur in enumerate(steps):
            if phrase_pos in (0, 2):
                degree = int(np.clip(degree + motif[k % len(motif)] + (1 if phrase_pos == 2 and k == 0 else 0), -3, 9))
            elif phrase_pos == 1:
                degree = int(np.clip(degree + r.integers(-2, 3), -3, 9))
            else:
                degree = [2, 1, 0][min(k, 2)] if k < 3 else 0
            f = note(pitch(degree, 0))
            seconds = dur * beat
            if lead == "pluck":
                parts.append((tpos, pluck(f, seconds * 1.8, 0.55, 0.997), 0.55))
            elif lead == "pipe":
                parts.append((tpos, pipe(f, seconds * 0.95, 5.0, 0.06), 0.45))
            else:  # "harp": a pluck doubled an octave up
                parts.append((tpos, pluck(f, seconds * 2.0, 0.7, 0.997), 0.45))
                parts.append((tpos, pluck(f * 2, seconds * 1.5, 0.8, 0.996), 0.18))
            tpos += seconds
        if phrase_pos == 3:
            degree = 0
            if r.random() < 0.4:
                motif = [int(x) for x in r.integers(-2, 3, 6)]
    return hall(mix(length, *parts), 3.0, 0.32)


def music_ancient():
    return compose(11, "dorian", -14, 76, 40, "pluck")


def music_classical():
    return compose(23, "mixolydian", -12, 84, 40, "pipe", drums=True)


def music_medieval():
    return compose(37, "aeolian", -16, 70, 36, "harp", drums=False)


def music_modern():
    return compose(53, "major", -10, 92, 44, "harp", bass_pulse=True, drums=True)


def music_menu():
    return compose(5, "pentatonic", -15, 64, 24, "harp", drums=False)


SOUNDS = {
    "SFX_Click": sfx_click, "SFX_Select": sfx_select, "SFX_EndTurn": sfx_end_turn, "SFX_NewTurn": sfx_new_turn,
    "SFX_FoundCity": sfx_found_city, "SFX_Research": sfx_research, "SFX_Civic": sfx_civic, "SFX_Notify": sfx_notify,
    "SFX_Clash": sfx_clash, "SFX_Bow": sfx_bow, "SFX_Gunshot": sfx_gunshot, "SFX_Cannon": sfx_cannon, "SFX_March": sfx_march,
    "SFX_Hooves": sfx_hooves, "SFX_War": sfx_war, "SFX_Era": sfx_era, "SFX_Victory": sfx_victory, "SFX_Defeat": sfx_defeat,
    "SFX_Build": sfx_build, "SFX_Coins": sfx_coins,
    "AMB_Country": amb_country, "AMB_Town": amb_town, "AMB_Battle": amb_battle,
    "MUS_Menu": music_menu, "MUS_Ancient": music_ancient, "MUS_Classical": music_classical, "MUS_Medieval": music_medieval,
    "MUS_Modern": music_modern,
}


def main():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    out = os.path.abspath(args[0] if args else os.path.join("art", "build", "audio"))
    wanted = args[1:] or list(SOUNDS)
    os.makedirs(out, exist_ok=True)
    for name in wanted:
        x = SOUNDS[name]()
        write(os.path.join(out, name + ".wav"), x, peak=0.7 if name.startswith(("MUS", "AMB")) else 0.89)
        print("sound %s: %.1f s" % (name, len(x) / SR))


if __name__ == "__main__":
    main()
