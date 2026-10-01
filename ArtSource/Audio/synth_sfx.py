"""Procedural SFX for Windup Crew (stdlib only). Writes 16-bit mono WAVs to ./wav/."""
import math
import os
import random
import struct
import wave

SR = 44100
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "wav")
os.makedirs(OUT, exist_ok=True)
rnd = random.Random(1234)
TAU = 2 * math.pi


def n(sec):
    return int(sec * SR)


def silence(sec):
    return [0.0] * n(sec)


def mix(*tracks):
    length = max(len(t) for t in tracks)
    out = [0.0] * length
    for t in tracks:
        for i, v in enumerate(t):
            out[i] += v
    return out


def place(buf, src, at_sec, gain=1.0):
    start = n(at_sec)
    if start + len(src) > len(buf):
        buf.extend([0.0] * (start + len(src) - len(buf)))
    for i, v in enumerate(src):
        buf[start + i] += v * gain
    return buf


def env_exp(length, decay):
    return [math.exp(-decay * i / SR) for i in range(length)]


def adsr(length, a=0.01, d=0.05, s=0.7, r=0.1):
    out = []
    na, nd, nr = n(a), n(d), n(r)
    for i in range(length):
        if i < na:
            e = i / max(1, na)
        elif i < na + nd:
            e = 1 - (1 - s) * (i - na) / max(1, nd)
        elif i < length - nr:
            e = s
        else:
            e = s * max(0.0, (length - i) / max(1, nr))
        out.append(e)
    return out


def osc(freq_fn, sec, shape="sine", phase=0.0):
    out = []
    p = phase
    for i in range(n(sec)):
        t = i / SR
        f = freq_fn(t) if callable(freq_fn) else freq_fn
        p += TAU * f / SR
        if shape == "sine":
            v = math.sin(p)
        elif shape == "square":
            v = 1.0 if math.sin(p) >= 0 else -1.0
        elif shape == "saw":
            v = 2.0 * ((p / TAU) % 1.0) - 1.0
        elif shape == "tri":
            x = (p / TAU) % 1.0
            v = 4 * x - 1 if x < 0.5 else 3 - 4 * x
        else:
            v = 0.0
        out.append(v)
    return out


def noise(sec, seed=None):
    r = random.Random(seed) if seed is not None else rnd
    return [r.uniform(-1, 1) for _ in range(n(sec))]


def lowpass(x, cutoff):
    out = []
    y = 0.0
    for i, v in enumerate(x):
        c = cutoff(i / SR) if callable(cutoff) else cutoff
        a = 1 - math.exp(-TAU * c / SR)
        y += a * (v - y)
        out.append(y)
    return out


def highpass(x, cutoff):
    lp = lowpass(x, cutoff)
    return [a - b for a, b in zip(x, lp)]


def bandpass(x, lo, hi):
    return lowpass(highpass(x, lo), hi)


def mul(x, e):
    return [a * b for a, b in zip(x, e)]


def gain(x, g):
    return [v * g for v in x]


def bell(freq, sec, decay=4.0, partials=((1, 1.0), (2.76, 0.5), (5.4, 0.25), (8.93, 0.12))):
    out = [0.0] * n(sec)
    for ratio, amp in partials:
        tone = osc(freq * ratio, sec)
        e = env_exp(len(tone), decay * (1 + ratio * 0.3))
        for i in range(len(out)):
            out[i] += tone[i] * e[i] * amp
    return out


def ping(freq, sec=0.15, decay=30.0, amp=1.0):
    t = osc(freq, sec)
    return gain(mul(t, env_exp(len(t), decay)), amp)


def click(sec=0.012, bright=3000, seed=None):
    x = noise(sec, seed)
    x = highpass(x, bright)
    return mul(x, env_exp(len(x), 400))


def karplus(freq, sec, damp=0.996):
    period = int(SR / freq)
    buf = [rnd.uniform(-1, 1) for _ in range(period)]
    out = []
    for i in range(n(sec)):
        v = buf[i % period]
        nxt = buf[(i + 1) % period]
        buf[i % period] = damp * 0.5 * (v + nxt)
        out.append(v)
    return out


def normalize(x, peak=0.9):
    m = max(1e-9, max(abs(v) for v in x))
    return [v * peak / m for v in x]


def fade(x, fin=0.002, fout=0.01):
    a, b = n(fin), n(fout)
    out = list(x)
    for i in range(min(a, len(out))):
        out[i] *= i / max(1, a)
    for i in range(min(b, len(out))):
        out[-1 - i] *= i / max(1, b)
    return out


def loopify(x, xfade=0.08):
    """Crossfade the tail into the head so the sample loops seamlessly."""
    k = n(xfade)
    body = x[:-k]
    tail = x[-k:]
    for i in range(k):
        w = i / k
        body[i] = body[i] * w + tail[i] * (1 - w)
    return body


def write(name, x, peak=0.9, loop=False):
    if not loop:
        x = fade(x)
    x = normalize(x, peak)
    path = os.path.join(OUT, name + ".wav")
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(b"".join(struct.pack("<h", int(max(-1, min(1, v)) * 32767)) for v in x))
    print("wrote", name, round(len(x) / SR, 2), "s")


def note(semitone_from_a4):
    return 440.0 * 2 ** (semitone_from_a4 / 12.0)


# ------------------------------------------------------------------------------------------
def build():
    # clockwork tick: tiny metallic tick
    t = mix(click(0.01, 4000, 1), ping(3800, 0.04, 120, 0.35), ping(2100, 0.05, 90, 0.25))
    write("SFX_Tick", t, 0.7)

    # winding ratchet click (pawl over teeth)
    c = silence(0.09)
    for k, at in enumerate((0.0, 0.025, 0.05)):
        place(c, mix(click(0.008, 2500, 10 + k), ping(2300 - k * 120, 0.05, 80, 0.5), ping(5200, 0.02, 200, 0.2)), at)
    write("SFX_Click", c, 0.85)

    write("SFX_RhythmDing", bell(1318.5, 0.7, 5.0), 0.7)
    write("SFX_KeyGrab", mix(ping(900, 0.12, 40, 0.8), ping(1400, 0.08, 60, 0.5), click(0.01, 1500, 3)), 0.8)

    # creak: rusty saw squeak
    def creak_f(t):
        return 170 - 60 * t + 18 * math.sin(TAU * 7 * t) + rnd.uniform(-6, 6)
    cr = osc(creak_f, 0.55, "saw")
    cr = bandpass(cr, 300, 2500)
    write("SFX_Creak", mul(cr, adsr(len(cr), 0.05, 0.1, 0.8, 0.2)), 0.6)

    # slip: freewheeling ratchet "drrrrk" decelerating
    s = silence(1.0)
    at = 0.0
    rate = 45.0
    k = 0
    while at < 0.95:
        place(s, mix(click(0.006, 2000, 100 + k), ping(1900, 0.03, 120, 0.4)), at, 1.0 - at * 0.6)
        at += 1.0 / rate
        rate = max(8.0, rate * 0.94)
        k += 1
    write("SFX_Slip", s, 0.85)

    # stretch (pulling the slingshot): rising rubbery tone
    st = osc(lambda t: 140 + 260 * t * t, 0.7, "saw")
    st = lowpass(st, 900)
    st = mul(st, [0.6 + 0.4 * math.sin(TAU * 18 * i / SR) for i in range(len(st))])
    write("SFX_Stretch", mul(st, adsr(len(st), 0.05, 0.1, 0.9, 0.1)), 0.55)

    # sling: twang + whoosh
    tw = karplus(98, 0.8, 0.995)
    wh = bandpass(noise(0.5, 7), 400, 3000)
    wh = mul(wh, adsr(len(wh), 0.02, 0.1, 0.6, 0.3))
    write("SFX_Sling", mix(gain(tw, 1.0), gain(wh, 0.5)), 0.9)

    # launch: spring boing
    def boing_f(t):
        return 260 + 160 * math.sin(TAU * 11 * t) * math.exp(-4 * t) + 120 * t
    bo = osc(boing_f, 0.6, "sine")
    bo = mul(bo, env_exp(len(bo), 5))
    bo2 = osc(lambda t: boing_f(t) * 2.01, 0.6, "sine")
    bo2 = mul(bo2, env_exp(len(bo2), 9))
    write("SFX_Launch", mix(bo, gain(bo2, 0.35), gain(bandpass(noise(0.35, 8), 600, 4000), 0.15)), 0.85)

    # thud
    th = osc(lambda t: 95 - 40 * t, 0.3, "sine")
    th = mul(th, env_exp(len(th), 14))
    tn = lowpass(noise(0.12, 9), 500)
    tn = mul(tn, env_exp(len(tn), 40))
    write("SFX_Thud", mix(th, gain(tn, 0.7), gain(ping(620, 0.08, 50), 0.25)), 0.85)

    gu = osc(lambda t: 420 + 220 * math.sin(TAU * 14 * t) * math.exp(-8 * t), 0.3, "sine")
    write("SFX_GetUp", mul(gu, env_exp(len(gu), 9)), 0.6)

    sp = lowpass(noise(0.4, 11), lambda t: 2500 * math.exp(-6 * t) + 150)
    sp = mul(sp, env_exp(len(sp), 7))
    write("SFX_Splat", mix(sp, gain(osc(lambda t: 300 - 400 * t, 0.25, "sine"), 0.4)), 0.85)

    # pop: rupture
    p = mix(gain(mul(noise(0.25, 12), env_exp(n(0.25), 18)), 1.0),
            gain(mul(osc(lambda t: 220 * math.exp(-6 * t) + 40, 0.5, "sine"), env_exp(n(0.5), 6)), 1.0))
    for k in range(9):
        place(p, ping(rnd.uniform(1800, 4200), 0.2, rnd.uniform(25, 45), 0.25), 0.08 + k * rnd.uniform(0.04, 0.09))
    write("SFX_Pop", p, 0.95)

    # suck into the dust bin
    su = noise(0.7, 13)
    su = lowpass(su, lambda t: 300 + 3500 * t)
    su = mul(su, adsr(len(su), 0.05, 0.2, 0.8, 0.15))
    write("SFX_Suck", mix(su, gain(mul(osc(lambda t: 600 - 450 * t, 0.7, "tri"), env_exp(n(0.7), 3)), 0.3)), 0.85)

    write("SFX_Bonk", mix(ping(185, 0.25, 18), ping(530, 0.15, 30, 0.5), click(0.01, 1500, 14)), 0.85)
    write("SFX_BonkMetal", bell(310, 0.9, 4.0, ((1, 1.0), (2.27, 0.6), (3.89, 0.4), (5.93, 0.25))), 0.85)
    write("SFX_Step", mix(ping(1600, 0.05, 90, 0.6), ping(3100, 0.03, 140, 0.3), click(0.006, 3000, 15)), 0.5)
    jp = osc(lambda t: 320 + 900 * t, 0.16, "sine")
    write("SFX_Jump", mul(jp, env_exp(len(jp), 16)), 0.55)
    write("SFX_Warn", mul(osc(1800, 0.09, "square"), adsr(n(0.09), 0.002, 0.01, 0.8, 0.02)), 0.35)

    wd = silence(0.9)
    at, gap, k = 0.0, 0.05, 0
    while at < 0.8:
        place(wd, mix(click(0.007, 1800, 200 + k), ping(1700 - at * 900, 0.04, 90, 0.4)), at)
        at += gap
        gap *= 1.18
        k += 1
    write("SFX_WindDown", wd, 0.8)

    write("SFX_Grab", mix(click(0.01, 900, 16), ping(420, 0.06, 60, 0.6)), 0.6)
    pk = silence(0.2)
    place(pk, ping(660, 0.1, 30), 0.0)
    place(pk, ping(990, 0.12, 30), 0.07)
    write("SFX_Pickup", pk, 0.6)
    tr = bandpass(noise(0.35, 17), 500, 2500)
    write("SFX_Throw", mul(tr, adsr(len(tr), 0.03, 0.05, 0.7, 0.2)), 0.6)

    dl = silence(1.0)
    for k, semi in enumerate((3, 7, 10, 15)):
        place(dl, bell(note(semi), 0.6, 6.0), k * 0.09)
    write("SFX_Deliver", dl, 0.85)

    asm = silence(0.6)
    place(asm, mix(click(0.01, 1200, 18), ping(800, 0.08, 50)), 0.0)
    place(asm, mix(click(0.01, 1200, 19), ping(950, 0.08, 50)), 0.12)
    place(asm, bell(1567.98, 0.4, 7.0), 0.22)
    write("SFX_Assemble", asm, 0.8)

    rv = silence(1.1)
    for k, semi in enumerate((-2, 2, 5, 10)):
        place(rv, bell(note(semi), 0.5, 6.0), k * 0.08)
    place(rv, mul(osc(lambda t: 300 + 200 * math.sin(TAU * 12 * t) * math.exp(-5 * t), 0.5), env_exp(n(0.5), 5)), 0.35, 0.6)
    write("SFX_Revive", rv, 0.85)

    # music box loop: original 16-bar-ish tune, 120 bpm eighths
    melody = [12, 16, 19, 16, 21, 19, 16, 14, 12, 14, 16, 19, 17, 16, 14, 12,
              9, 12, 16, 12, 17, 16, 14, 12, 11, 14, 17, 14, 16, 12, 7, None]
    step = 0.25
    mb = silence(len(melody) * step + 1.0)
    for k, semi in enumerate(melody):
        if semi is None:
            continue
        f = note(semi + 3)  # around C5
        place(mb, bell(f, 1.0, 5.5, ((1, 1.0), (3.0, 0.25), (4.97, 0.12), (8.1, 0.05))), k * step, 0.8)
        if k % 4 == 0:
            place(mb, bell(f / 2, 1.0, 4.0, ((1, 1.0), (3.0, 0.2))), k * step, 0.35)
    mb = mb[:n(len(melody) * step + 0.08)]
    write("SFX_MusicBox", loopify(mb, 0.08), 0.7, loop=True)

    # vacuum motor loop
    vl = mix(gain(lowpass(osc(lambda t: 118 + 3 * math.sin(TAU * 2 * t), 2.1, "saw"), 900), 0.6),
             gain(bandpass(noise(2.1, 21), 200, 1800), 0.5),
             gain(osc(236, 2.1, "sine"), 0.2))
    write("SFX_VacuumLoop", loopify(vl, 0.1), 0.6, loop=True)
    sl = mix(gain(bandpass(noise(2.1, 22), 800, 5000), 0.7), gain(osc(lambda t: 1250 + 30 * math.sin(TAU * 5 * t), 2.1, "sine"), 0.15))
    write("SFX_SuctionLoop", loopify(sl, 0.1), 0.6, loop=True)

    bs = mul(osc(lambda t: 700 * math.exp(-1.2 * t) + 120 + 60 * math.sin(TAU * 6 * t), 1.2, "square"), env_exp(n(1.2), 2.0))
    bs = lowpass(bs, 1800)
    for k in range(5):
        place(bs, mul(osc(lambda t, k=k: 2500 + 900 * math.sin(TAU * (9 + k) * t), 0.12), env_exp(n(0.12), 20)), 0.3 + k * 0.15, 0.3)
    write("SFX_BossStun", bs, 0.8)

    hp = mix(gain(mul(highpass(noise(0.5, 23), 2500), adsr(n(0.5), 0.01, 0.1, 0.6, 0.3)), 0.6), gain(ping(140, 0.2, 20), 0.8))
    write("SFX_Hatch", hp, 0.8)

    cp = mix(gain(mul(noise(0.3, 24), env_exp(n(0.3), 12)), 0.4),
             gain(mul(osc(60, 0.3, "square"), env_exp(n(0.3), 9)), 0.4),
             gain(mul(osc(lambda t: 900 - 1500 * t, 0.25, "sine"), env_exp(n(0.25), 10)), 0.7))
    write("SFX_CellPull", cp, 0.85)

    al = []
    for k in range(4):
        al += mul(osc(880 if k % 2 == 0 else 660, 0.22, "square"), adsr(n(0.22), 0.005, 0.02, 0.8, 0.03))
    write("SFX_Alarm", lowpass(al, 3000), 0.6)

    bh = mix(gain(mul(bandpass(noise(0.25, 25), 1500, 6000), env_exp(n(0.25), 15)), 0.8), gain(ping(160, 0.2, 18), 0.8))
    write("SFX_BrushHit", bh, 0.8)
    ej = mix(gain(mul(lowpass(noise(0.4, 26), 1200), env_exp(n(0.4), 9)), 0.8),
             gain(mul(osc(lambda t: 500 - 700 * t, 0.3, "sine"), env_exp(n(0.3), 8)), 0.6))
    write("SFX_Eject", ej, 0.85)
    dk = mix(gain(mul(osc(lambda t: 200 + 1000 * t, 1.0, "sine"), adsr(n(1.0), 0.05, 0.2, 0.7, 0.3)), 0.6),
             gain(mul(highpass(noise(1.0, 27), 4000), [0.2 + 0.2 * math.sin(TAU * 13 * i / SR) for i in range(n(1.0))]), 0.3))
    write("SFX_Dock", dk, 0.7)
    bd = mul(osc(lambda t: 650 * math.exp(-1.6 * t) + 45, 1.6, "saw"), adsr(n(1.6), 0.01, 0.2, 0.8, 0.4))
    bd = lowpass(bd, 1400)
    for k in range(6):
        place(bd, click(0.02, 800, 30 + k), 0.9 + k * 0.1, 0.5)
    write("SFX_BossDown", bd, 0.85)
    bw = silence(1.0)
    for k, f in enumerate((660, 880, 1320)):
        place(bw, mul(osc(f, 0.1, "square"), adsr(n(0.1), 0.005, 0.02, 0.8, 0.02)), k * 0.14, 0.4)
    place(bw, mul(lowpass(osc(lambda t: 60 + 200 * t, 0.6, "saw"), 700), adsr(n(0.6), 0.2, 0.1, 0.8, 0.1)), 0.42, 0.7)
    write("SFX_BossWake", bw, 0.8)

    lv = silence(0.35)
    place(lv, mix(click(0.015, 700, 40), ping(300, 0.1, 40)), 0.0)
    place(lv, mix(click(0.015, 700, 41), ping(220, 0.15, 30)), 0.15)
    write("SFX_Lever", lv, 0.85)
    brg = mix(gain(bandpass(osc(lambda t: 140 - 50 * t + 10 * math.sin(TAU * 9 * t), 1.0, "saw"), 200, 1800), 0.6))
    write("SFX_Bridge", mul(brg, adsr(len(brg), 0.05, 0.2, 0.7, 0.3)), 0.6)
    cf = mul(bandpass(noise(0.9, 42), 200, 1500), adsr(n(0.9), 0.05, 0.2, 0.6, 0.5))
    write("SFX_ClothFall", cf, 0.6)
    dr = bandpass(noise(0.6, 43), 300, 1600)
    dr = mul(dr, [0.5 + 0.5 * abs(math.sin(TAU * 23 * i / SR)) for i in range(len(dr))])
    write("SFX_Drawer", mul(dr, adsr(len(dr), 0.03, 0.1, 0.8, 0.15)), 0.6)
    write("SFX_Toast", mix(ping(1046, 0.12, 25, 0.6), ping(1568, 0.15, 25, 0.4)), 0.4)
    write("SFX_UIClick", mix(click(0.008, 2500, 44), ping(1200, 0.04, 80, 0.5)), 0.5)

    # win fanfare
    win = silence(2.0)
    for k, semi in enumerate((3, 7, 10, 15)):
        tone = mix(osc(note(semi), 0.35, "saw"), gain(osc(note(semi) * 2, 0.35, "saw"), 0.3))
        tone = mul(lowpass(tone, 2500), adsr(len(tone), 0.01, 0.05, 0.7, 0.1))
        place(win, tone, k * 0.13, 0.6)
    chord = [0.0] * n(1.2)
    for semi in (3, 7, 10, 15):
        tone = mul(lowpass(osc(note(semi), 1.2, "saw"), 2200), adsr(n(1.2), 0.02, 0.2, 0.6, 0.5))
        chord = mix(chord, gain(tone, 0.3))
    place(win, chord, 0.55)
    write("SFX_Win", win, 0.85)

    fail = silence(2.4)
    for k, semi in enumerate((-2, -3, -4, -5)):
        dur = 0.4 if k < 3 else 1.1
        tone = osc(lambda t, s=semi: note(s - 12) * (1 + (0.02 * math.sin(TAU * 5 * t) if dur > 1 else 0)), dur, "saw")
        tone = mul(lowpass(tone, 900), adsr(len(tone), 0.03, 0.05, 0.8, 0.15))
        place(fail, tone, k * 0.42, 0.7)
    write("SFX_Fail", fail, 0.85)

    ch = silence(4.5)
    for k in range(3):
        place(ch, bell(220, 2.5, 1.2, ((1, 1.0), (2.0, 0.5), (2.76, 0.4), (5.4, 0.15))), k * 1.2)
    write("SFX_ClockChime", ch, 0.9)
    write("SFX_ClockTick", mix(ping(700, 0.08, 45), lowpass(mul(noise(0.03, 45), env_exp(n(0.03), 120)), 2000)), 0.5)


if __name__ == "__main__":
    build()
