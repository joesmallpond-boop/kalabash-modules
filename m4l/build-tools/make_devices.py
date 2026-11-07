#!/usr/bin/env python3
"""Generates the KNOTWORK, ALLOY, FOGBANK and ABACUS Max for Live devices.

Each device is a Live-native UI (DICEBAG style) around a compiled external
(kb.knotwork~ etc.) that runs the same DSP cores as the VST3/AU plugins.
Output: m4l/build/<NAME>.amxd
"""
import os
from m4l_lib import Device, BG1, BG2, DIM, U_FLOAT, U_ST, U_HZ, U_DB, U_INT

OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "build")
os.makedirs(OUT, exist_ok=True)
DIVS = ["Off", "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/16T"]
CREDIT = "kalabash  -  DSP after Mutable Instruments %s by Emilie Gillet (MIT)"


def footer(d, x, w, module):
    d.label(CREDIT % module, [x, 154, w, 12], 7.5, DIM)


# ------------------------------------------------------------------ KNOTWORK (Plaits)
def knotwork():
    d = Device("instrument")
    ext = d.setup("kb.knotwork~", 1)
    models = ["VA", "Waveshp", "2-op FM", "Formant", "Additive", "Wavetbl", "Chords", "Speech",
              "Swarm", "Noise", "Particle", "String", "Modal", "Kick", "Snare", "Hi-hat",
              "VA+VCF", "PhaseDst", "6-op I", "6-op II", "6-op III", "Terrain", "StrMach", "Chip"]
    d.panel([2, 2, 404, 165], BG1)
    d.header("MODEL", [8, 4, 120, 14])
    d.label("pitched  /  noise + perc  /  extended", [190, 5, 210, 13], 8, DIM)
    d.tab("model", "Model", "Model", [8, 21, 392, 54], models, 0, lines=3)
    d.dial("tune", "Frequency", "Freq", [8, 82, 41, 48], -12, 12, 0, U_ST)
    d.dial("harmonics", "Harmonics", "Harm", [58, 82, 41, 48], 0, 1, 0.5)
    d.dial("timbre", "Timbre", "Timbre", [108, 82, 41, 48], 0, 1, 0.5)
    d.dial("morph", "Morph", "Morph", [158, 82, 41, 48], 0, 1, 0.5)
    d.dial("cents", "Fine", "Fine", [208, 82, 41, 48], -50, 50, 0, U_FLOAT)
    d.dial("a4", "A4 Reference", "A4", [258, 82, 41, 48], 415, 466, 440, U_HZ)
    d.label("octave", [312, 80, 60, 12], 8, DIM)
    d.menu("octave", "Octave", "Oct", [312, 93, 88, 15], ["-4", "-3", "-2", "-1", "0", "+1", "+2", "+3", "+4"], 4)
    d.toggle("tuned", "Tuned", "TUNED", [312, 113, 88, 17], 1)
    footer(d, 8, 396, "Plaits")

    d.panel([410, 2, 170, 165], BG2)
    d.header("ENV  /  LPG", [416, 4, 120, 14])
    d.dial("timbreAmt", "Env > Timbre", "E>Tmb", [416, 22, 41, 48], -1, 1, 0)
    d.dial("fmAmt", "Env > Freq", "E>Frq", [470, 22, 41, 48], -1, 1, 0)
    d.dial("morphAmt", "Env > Morph", "E>Mrph", [524, 22, 41, 48], -1, 1, 0)
    d.dial("decay", "LPG Decay", "Decay", [416, 82, 41, 48], 0, 1, 0.5)
    d.dial("colour", "LPG Colour", "Colour", [470, 82, 41, 48], 0, 1, 0.5)
    d.label("amp", [524, 80, 50, 12], 8, DIM)
    d.tab("amp", "Amp Mode", "Amp", [524, 93, 50, 45], ["Pluck", "Gate", "Drone"], 0, lines=3)

    d.panel([584, 2, 186, 165], BG1)
    d.header("VOICE", [590, 4, 120, 14])
    d.label("voices", [590, 22, 50, 12], 8, DIM)
    d.menu("voices", "Voices", "Voices", [590, 35, 50, 15], [str(i) for i in range(1, 9)], 3)
    d.dial("glide", "Glide", "Glide", [646, 22, 41, 48], 0, 1, 0)
    d.dial("velTimbre", "Vel > Timbre", "V>Tmb", [696, 22, 41, 48], 0, 1, 0)
    d.dial("aux", "Out / Aux", "Aux", [590, 82, 41, 48], 0, 1, 0)
    d.dial("level", "Level", "Level", [646, 82, 41, 48], 0, 1.5, 0.8)
    d.label("bend", [696, 82, 40, 12], 8, DIM)
    d.numbox("bend", "Bend Range", "Bend", [696, 95, 40, 15], 0, 24, 2)
    d.meters(ext, [748, 22, 16, 108])
    return d.write(os.path.join(OUT, "KNOTWORK.amxd"), 774,
                   "KNOTWORK by kalabash - macro oscillator. DSP from Mutable Instruments Plaits by Emilie Gillet (MIT).",
                   ["kb.knotwork~.mxo"])


# ------------------------------------------------------------------ ALLOY (Elements)
def alloy():
    d = Device("instrument")
    ext = d.setup("kb.alloy~", 1)
    d.panel([2, 2, 352, 165], BG1)
    d.header("EXCITER", [8, 4, 120, 14])
    d.dial("contour", "Contour", "Contour", [8, 22, 41, 48], 0, 1, 0.5)
    d.dial("bow", "Bow Level", "Bow", [58, 22, 41, 48], 0, 1, 0.0)
    d.dial("blow", "Blow Level", "Blow", [108, 22, 41, 48], 0, 1, 0.0)
    d.dial("strike", "Strike Level", "Strike", [158, 22, 41, 48], 0, 1, 0.8)
    d.dial("flow", "Flow", "Flow", [208, 22, 41, 48], 0, 1, 0.5)
    d.dial("mallet", "Mallet", "Mallet", [258, 22, 41, 48], 0, 1, 0.5)
    d.dial("bowTimbre", "Bow Timbre", "Bow T", [58, 82, 41, 48], 0, 1, 0.5)
    d.dial("blowTimbre", "Blow Timbre", "Blow T", [108, 82, 41, 48], 0, 1, 0.5)
    d.dial("strikeTimbre", "Strike Timbre", "Strk T", [158, 82, 41, 48], 0, 1, 0.5)
    d.toggle("play", "Play", "PLAY", [306, 30, 42, 30], 0)
    d.dial("velStrength", "Vel > Strength", "V>Str", [258, 82, 41, 48], 0, 1, 1.0)
    d.label("latching gate", [302, 64, 50, 24], 8, DIM)
    footer(d, 8, 344, "Elements")

    d.panel([358, 2, 352, 165], BG2)
    d.header("RESONATOR", [364, 4, 120, 14])
    d.dial("coarse", "Coarse", "Coarse", [364, 22, 41, 48], -30, 30, 0, U_ST)
    d.dial("fine", "Fine", "Fine", [414, 22, 41, 48], -50, 50, 0, U_FLOAT)
    d.dial("fm", "FM Amount", "FM", [464, 22, 41, 48], -1, 1, 0)
    d.dial("geometry", "Geometry", "Geom", [514, 22, 41, 48], 0, 1, 0.5)
    d.dial("brightness", "Brightness", "Bright", [564, 22, 41, 48], 0, 1, 0.5)
    d.dial("damping", "Damping", "Damp", [614, 22, 41, 48], 0, 1, 0.5)
    d.dial("position", "Position", "Pos", [364, 82, 41, 48], 0, 1, 0.3)
    d.dial("space", "Space", "Space", [414, 82, 41, 48], 0, 2, 0.4)
    d.dial("a4", "A4 Reference", "A4", [464, 82, 41, 48], 415, 466, 440, U_HZ)
    d.label("resonator", [514, 80, 80, 12], 8, DIM)
    d.tab("model", "Resonator", "Model", [514, 93, 140, 16], ["Modal", "String", "Strings"], 0)
    d.toggle("tuned", "Tuned", "TUNED", [514, 114, 68, 17], 1)
    d.toggle("ominous", "Ominous", "OMINOUS", [586, 114, 68, 17], 0)
    d.meters(ext, [664, 22, 16, 108])

    d.panel([714, 2, 300, 165], BG1)
    d.header("MOD", [720, 4, 60, 14])
    d.menu("modSrc", "Mod Source", "Src", [760, 5, 90, 15], ["Velocity", "Mod wheel", "Aftertouch", "LFO", "Random"], 0)
    d.dial("lfoRate", "LFO Rate", "LFO", [966, 22, 41, 48], 0, 1, 0.3)
    mods = [("mBowTimbre", "Mod > Bow Timbre", "Bow T"), ("mFlow", "Mod > Flow", "Flow"),
            ("mBlowTimbre", "Mod > Blow Timbre", "Blow T"), ("mMallet", "Mod > Mallet", "Mallet"),
            ("mStrikeTimbre", "Mod > Strike Timbre", "Strk T"), ("mGeometry", "Mod > Geometry", "Geom"),
            ("mBrightness", "Mod > Brightness", "Bright"), ("mDamping", "Mod > Damping", "Damp"),
            ("mPosition", "Mod > Position", "Pos"), ("mSpace", "Mod > Space", "Space")]
    for i, (p, ln, sn) in enumerate(mods):
        x = 720 + (i % 5) * 48
        y = 22 if i < 5 else 82
        d.dial(p, ln, sn, [x, y, 41, 48], -1, 1, 0)
    d.dial("level", "Level", "Level", [966, 82, 41, 48], 0, 1.5, 0.8)
    d.numbox("bend", "Bend Range", "Bend", [966, 136, 41, 15], 0, 24, 2)
    return d.write(os.path.join(OUT, "ALLOY.amxd"), 1018,
                   "ALLOY by kalabash - modal synthesis voice. DSP from Mutable Instruments Elements by Emilie Gillet (MIT).",
                   ["kb.alloy~.mxo"])


# ------------------------------------------------------------------ FOGBANK (Clouds)
def fogbank():
    d = Device("audio")
    ext = d.setup("kb.fogbank~", 2)
    d.panel([2, 2, 262, 165], BG1)
    d.header("GRAIN", [8, 4, 120, 14])
    d.dial("position", "Position", "Pos", [8, 22, 41, 48], 0, 1, 0.5)
    d.dial("size", "Size", "Size", [58, 22, 41, 48], 0, 1, 0.5)
    d.dial("pitch", "Pitch", "Pitch", [108, 22, 41, 48], -24, 24, 0, U_ST)
    d.dial("density", "Density", "Dens", [158, 22, 41, 48], 0, 1, 0.7)
    d.dial("texture", "Texture", "Text", [208, 22, 41, 48], 0, 1, 0.5)
    d.label("mode", [8, 78, 60, 12], 8, DIM)
    d.tab("mode", "Mode", "Mode", [8, 91, 250, 16], ["Granular", "Stretch", "Loop dly", "Spectral"], 0)
    d.label("quality / buffer", [8, 111, 100, 12], 8, DIM)
    d.menu("quality", "Quality", "Qual", [8, 124, 170, 15], ["16-bit stereo 1s", "16-bit mono 2s", "8-bit stereo 4s", "8-bit mono 8s"], 0)
    d.toggle("pitchSnap", "Pitch Snap", "SEMI", [184, 123, 74, 17], 1)
    footer(d, 8, 254, "Clouds")

    d.panel([268, 2, 112, 165], BG2)
    d.header("BLEND", [274, 4, 80, 14])
    d.dial("dryWet", "Dry/Wet", "Dry/Wet", [274, 22, 41, 48], 0, 1, 0.5)
    d.dial("spread", "Stereo Spread", "Spread", [326, 22, 41, 48], 0, 1, 0.5)
    d.dial("feedback", "Feedback", "Fdbk", [274, 82, 41, 48], 0, 1, 0.0)
    d.dial("reverb", "Reverb", "Reverb", [326, 82, 41, 48], 0, 1, 0.3)

    d.panel([384, 2, 214, 165], BG1)
    d.header("CONTROL", [390, 4, 80, 14])
    d.toggle("freeze", "Freeze", "FREEZE", [390, 24, 72, 30], 0)
    d.pulse("trig", "TRIG", [468, 24, 60, 30])
    d.label("trig sync", [390, 62, 70, 12], 8, DIM)
    d.menu("clock", "Trigger Sync", "Sync", [390, 75, 72, 15], DIVS, 0)
    d.dial("inGain", "In Gain", "In", [468, 62, 41, 48], -18, 18, 0, U_DB)
    d.dial("outGain", "Out Gain", "Out", [518, 62, 41, 48], -18, 12, 0, U_DB)
    d.meters(ext, [574, 22, 16, 108])
    return d.write(os.path.join(OUT, "FOGBANK.amxd"), 602,
                   "FOGBANK by kalabash - granular texture processor. DSP from Mutable Instruments Clouds by Emilie Gillet (MIT).",
                   ["kb.fogbank~.mxo"])


# ------------------------------------------------------------------ ABACUS (Beads)
def abacus():
    d = Device("audio")
    ext = d.setup("kb.abacus~", 2)
    d.panel([2, 2, 212, 165], BG1)
    d.header("GRAIN", [8, 4, 120, 14])
    for i, (p, ln, sn, init) in enumerate([("time", "Time", "Time", 0.3), ("size", "Size", "Size", 0.5),
                                           ("shape", "Shape", "Shape", 0.5), ("pitch", "Pitch", "Pitch", 0.5)]):
        d.dial(p, ln, sn, [8 + i * 50, 22, 41, 48], 0, 1, init)
        d.dial(p + "Amt", ln + " Random", "Rnd", [8 + i * 50, 82, 41, 48], -1, 1, 0)
    d.label("random:  left = peaky S&H,  right = smooth drift", [8, 136, 204, 12], 7.5, DIM)
    footer(d, 8, 204, "Beads")

    d.panel([218, 2, 160, 165], BG2)
    d.header("GENERATION", [224, 4, 120, 14])
    d.dial("density", "Density", "Density", [224, 22, 41, 48], 0, 1, 0.7)
    d.pulse("seed", "SEED", [274, 24, 50, 28], 40)
    d.toggle("latch", "Seed Latch", "LATCH", [328, 24, 44, 28], 1)
    d.label("seed clock", [274, 58, 70, 12], 8, DIM)
    d.menu("clock", "Seed Clock", "Clock", [274, 71, 98, 15], DIVS, 0)
    d.toggle("synth", "Synth Mode", "SYNTH", [274, 92, 98, 17], 0)
    d.label("SIZE fully up = delay mode", [224, 136, 150, 12], 7.5, DIM)

    d.panel([382, 2, 160, 165], BG1)
    d.header("MIX", [388, 4, 80, 14])
    d.dial("feedback", "Feedback", "Fdbk", [388, 22, 41, 48], 0, 1, 0.0)
    d.dial("dryWet", "Dry/Wet", "Dry/Wet", [438, 22, 41, 48], 0, 1, 0.5)
    d.dial("reverb", "Reverb", "Reverb", [488, 22, 41, 48], 0, 1, 0.2)
    d.dial("inGain", "In Gain", "In", [388, 82, 41, 48], -18, 18, 0, U_DB)
    d.dial("outGain", "Out Gain", "Out", [438, 82, 41, 48], -18, 12, 0, U_DB)

    d.panel([546, 2, 150, 165], BG2)
    d.header("BUFFER", [552, 4, 80, 14])
    d.toggle("freeze", "Freeze", "FREEZE", [552, 24, 70, 28], 0)
    d.toggle("mono", "Mono Buffer", "MONO 2x", [552, 58, 70, 17], 0)
    d.label("quality", [552, 80, 70, 12], 8, DIM)
    d.tab("quality", "Quality", "Qual", [552, 93, 110, 64],
          ["Digital 48k", "Cloudy 32k", "Clean tape", "Dirty tape"], 0, lines=4)
    d.meters(ext, [672, 22, 16, 108])
    return d.write(os.path.join(OUT, "ABACUS.amxd"), 700,
                   "ABACUS by kalabash - granular texture synthesizer. DSP from Mutable Instruments Beads by Emilie Gillet (MIT).",
                   ["kb.abacus~.mxo"])


if __name__ == "__main__":
    for f in (knotwork, alloy, fogbank, abacus):
        print(f.__name__, "boxes/lines", f())
