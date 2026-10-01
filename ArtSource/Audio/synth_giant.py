"""Procedural SFX for the giant cymbal monkey boss (zone F). Uses the helpers of synth_sfx.py."""
import math
import random

from synth_sfx import (SR, TAU, n, silence, mix, place, env_exp, adsr, osc, noise, lowpass, highpass, bandpass,
                       mul, gain, bell, ping, click, write, loopify, note)


def cymbal(sec, seed, bright=3500, decay=2.2):
    """Washy metallic crash: banded noise + inharmonic partials."""
    wash = mul(highpass(noise(sec, seed), bright), env_exp(n(sec), decay))
    body = [0.0] * n(sec)
    r = random.Random(seed)
    for _ in range(14):
        f = r.uniform(380, 7200)
        tone = osc(f, sec)
        e = env_exp(n(sec), decay * r.uniform(0.6, 1.8))
        a = r.uniform(0.05, 0.18)
        for i in range(len(body)):
            body[i] += tone[i] * e[i] * a
    return mix(gain(wash, 0.9), body)


def build():
    # big brass cymbal clash: hard transient + long shimmering wash
    cr = cymbal(2.6, 101, 3000, 1.6)
    place(cr, mix(click(0.02, 1200, 102), gain(ping(240, 0.25, 14), 0.6)), 0.0)
    write("SFX_CymbalCrash", cr, 0.95)

    # stomp: sub thump + debris rattle
    st = mix(gain(mul(osc(lambda t: 70 * math.exp(-3 * t) + 32, 1.0, "sine"), env_exp(n(1.0), 4.5)), 1.0),
             gain(mul(lowpass(noise(0.8, 103), 900), env_exp(n(0.8), 6)), 0.7),
             gain(mul(bandpass(noise(0.5, 104), 1500, 5000), env_exp(n(0.5), 9)), 0.25))
    write("SFX_GiantStomp", st, 0.95)

    # winding down: ratchet clicks slowing to a stop + a sighing spring
    wd = silence(1.9)
    at, gap = 0.0, 0.035
    while at < 1.6:
        place(wd, mix(click(0.012, 1800, int(at * 1000)), ping(900 - at * 300, 0.05, 60, 0.5)), at, 0.9)
        at += gap
        gap *= 1.12
    place(wd, gain(mul(osc(lambda t: 420 * math.exp(-0.9 * t), 1.6, "saw"), adsr(n(1.6), 0.1, 0.2, 0.5, 0.6)), 0.25), 0.1)
    write("SFX_GiantWindDown", lowpass(wd, 6000), 0.85)

    # rewind: fast ratchet accelerating upward
    rw = silence(1.7)
    at, gap = 0.0, 0.09
    while at < 1.55:
        place(rw, mix(click(0.01, 2200, 300 + int(at * 1000)), ping(700 + at * 900, 0.04, 70, 0.4)), at, 0.8)
        at += gap
        gap = max(0.018, gap * 0.9)
    write("SFX_GiantRewind", rw, 0.8)

    # key hit: big bell "DING" + metal crunch
    kh = mix(bell(note(-5), 2.4, 1.6, ((1, 1.0), (2.0, 0.6), (2.76, 0.45), (5.4, 0.2), (8.9, 0.1))),
             gain(mul(bandpass(noise(0.2, 105), 1200, 7000), env_exp(n(0.2), 18)), 0.6))
    write("SFX_KeyHit", kh, 0.95)

    # deflect: short high "ting"
    write("SFX_Deflect", mix(bell(1650, 0.6, 7, ((1, 1.0), (2.3, 0.4), (4.1, 0.2))), gain(click(0.01, 3000, 106), 0.5)), 0.7)

    # monkey screech: wobbling squeal with a chattering tail
    sc = mul(osc(lambda t: 1100 + 500 * math.sin(TAU * 7 * t) + 600 * math.exp(-3 * t), 1.1, "saw"), adsr(n(1.1), 0.02, 0.1, 0.8, 0.3))
    sc = bandpass(sc, 600, 5000)
    for k in range(5):
        place(sc, gain(mul(osc(lambda t, k=k: 1500 + 200 * k, 0.07, "square"), env_exp(n(0.07), 30)), 0.3), 0.75 + k * 0.08)
    write("SFX_MonkeyScreech", sc, 0.85)

    # cymbal whoosh (throw)
    wh = mul(bandpass(noise(0.6, 107), 400, 3000), adsr(n(0.6), 0.08, 0.1, 0.6, 0.3))
    write("SFX_CymbalWhoosh", wh, 0.7)

    # spinning cymbal loop: pulsing whoosh with a ringing tone
    sp = mix(gain(mul(bandpass(noise(1.0, 108), 500, 4000), [0.55 + 0.45 * math.sin(TAU * 9 * i / SR) for i in range(n(1.0))]), 0.8),
             gain(osc(lambda t: 2400 + 120 * math.sin(TAU * 9 * t), 1.0, "sine"), 0.12))
    write("SFX_CymbalSpin", loopify(sp, 0.1), 0.6, loop=True)

    # giant falls: creak, crash, cymbals clatter
    gf = silence(3.2)
    place(gf, gain(mul(osc(lambda t: 260 - 120 * t, 0.7, "saw"), adsr(n(0.7), 0.05, 0.1, 0.6, 0.2)), 0.3), 0.0)
    place(gf, mix(gain(mul(osc(lambda t: 55 * math.exp(-2 * t) + 30, 1.4, "sine"), env_exp(n(1.4), 3)), 1.0),
                  gain(mul(lowpass(noise(1.2, 109), 1200), env_exp(n(1.2), 4)), 0.8)), 0.75)
    place(gf, gain(cymbal(1.6, 110, 2500, 2.5), 0.7), 0.95)
    place(gf, gain(cymbal(1.2, 111, 3500, 3.0), 0.5), 1.35)
    write("SFX_GiantFall", gf, 0.95)

    # heavy clockwork tick loop of the giant (1 s, tick-tock)
    tk = silence(1.0)
    place(tk, mix(click(0.015, 900, 112), ping(520, 0.12, 30), gain(ping(1300, 0.06, 60), 0.4)), 0.0)
    place(tk, mix(click(0.015, 1100, 113), ping(440, 0.12, 30), gain(ping(1100, 0.06, 60), 0.4)), 0.5, 0.85)
    write("SFX_GiantTick", tk, 0.7, loop=True)


if __name__ == "__main__":
    build()
