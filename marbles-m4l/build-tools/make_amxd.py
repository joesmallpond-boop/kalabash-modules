#!/usr/bin/env python3
"""Builds DICEBAG.amxd (Max for Live MIDI effect) from the JS sources.

Usage: python3 make_amxd.py  -> writes ../build/DICEBAG.amxd and the .js files next to it.
"""
import json, os, struct

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
OUT = os.path.join(ROOT, "build")
os.makedirs(OUT, exist_ok=True)

# ---------------------------------------------------------------- JS assembly
engine = open(os.path.join(ROOT, "src", "dicebag_engine.js")).read()
engine = engine.replace("// @@TABLES@@",
                        open(os.path.join(HERE, "tables.js")).read() + open(os.path.join(HERE, "scales.js")).read())
open(os.path.join(OUT, "dicebag_engine.js"), "w").write(engine)
for f in ("dicebag_map.js", "dicebag_view.js"):
    open(os.path.join(OUT, f), "w").write(open(os.path.join(ROOT, "src", f)).read())

# ---------------------------------------------------------------- patch builder
boxes, lines = [], []
_n = [0]


def nid():
    _n[0] += 1
    return "obj-%d" % _n[0]


def box(maxclass, rect, pres=None, **kw):
    b = {"id": nid(), "maxclass": maxclass, "patching_rect": rect}
    if pres is not None:
        b["presentation"] = 1
        b["presentation_rect"] = pres
    b.update(kw)
    boxes.append({"box": b})
    return b["id"]


def obj(text, x, y, ins=1, outs=1, outtype=None, w=None):
    return box("newobj", [x, y, w or max(40, 7 * len(text) + 10), 22], text=text,
               numinlets=ins, numoutlets=outs, outlettype=outtype or [""] * outs)


def line(src, so, dst, di):
    lines.append({"patchline": {"source": [src, so], "destination": [dst, di]}})


PX = [20]  # patching x cursor for UI objects
def next_px():
    PX[0] += 70
    return PX[0]


FONT = {"fontname": "Ableton Sans Medium", "fontsize": 9.5}
ACCENT = [1.0, 0.62, 0.25, 1.0]
TXT = [0.86, 0.86, 0.86, 1.0]


def valueof(longname, shortname, ptype, **kw):
    v = {"parameter_longname": longname, "parameter_shortname": shortname, "parameter_type": ptype}
    v.update(kw)
    return {"valueof": v}


def dial(name, short, pres, lo=0.0, hi=1.0, init=0.5, unit=1, exp=1.0, steps=0):
    va = valueof(name, short, 0, parameter_mmin=lo, parameter_mmax=hi, parameter_initial=[init],
                 parameter_initial_enable=1, parameter_unitstyle=unit, parameter_exponent=exp)
    if steps:
        va["valueof"]["parameter_steps"] = steps
    return box("live.dial", [next_px(), 400, 41, 48], pres, numinlets=1, numoutlets=2,
               outlettype=["", "float"], parameter_enable=1, varname=name,
               saved_attribute_attributes=va, activedialcolor=ACCENT, **FONT)


def intdial(name, short, pres, lo, hi, init, unit=0):
    va = valueof(name, short, 1, parameter_mmin=lo, parameter_mmax=hi, parameter_initial=[init],
                 parameter_initial_enable=1, parameter_unitstyle=unit)
    return box("live.dial", [next_px(), 400, 41, 48], pres, numinlets=1, numoutlets=2,
               outlettype=["", "float"], parameter_enable=1, varname=name,
               saved_attribute_attributes=va, activedialcolor=ACCENT, **FONT)


def tab(name, short, pres, items, init=0):
    va = valueof(name, short, 2, parameter_enum=items, parameter_mmax=len(items) - 1,
                 parameter_initial=[init], parameter_initial_enable=1, parameter_unitstyle=10)
    return box("live.tab", [next_px(), 460, pres[2], pres[3]], pres, numinlets=1, numoutlets=3,
               outlettype=["", "", "float"], parameter_enable=1, varname=name, num_lines_patching=1,
               num_lines_presentation=1, saved_attribute_attributes=va, activebgoncolor=ACCENT, **FONT)


def menu(name, short, pres, items, init=0):
    va = valueof(name, short, 2, parameter_enum=items, parameter_mmax=len(items) - 1,
                 parameter_initial=[init], parameter_initial_enable=1)
    return box("live.menu", [next_px(), 500, pres[2], pres[3]], pres, numinlets=1, numoutlets=3,
               outlettype=["", "", "float"], parameter_enable=1, varname=name,
               saved_attribute_attributes=va, **FONT)


def toggle(name, short, pres, init=0, text=None):
    va = valueof(name, short, 2, parameter_enum=["off", "on"], parameter_mmax=1,
                 parameter_initial=[init], parameter_initial_enable=1)
    return box("live.text", [next_px(), 530, pres[2], pres[3]], pres, numinlets=1, numoutlets=2,
               outlettype=["", ""], parameter_enable=1, varname=name, mode=1,
               text=text or short, texton=text or short, saved_attribute_attributes=va,
               activebgoncolor=ACCENT, **FONT)


def numbox(name, short, pres, lo, hi, init, unit=0):
    va = valueof(name, short, 1, parameter_mmin=lo, parameter_mmax=hi, parameter_initial=[init],
                 parameter_initial_enable=1, parameter_unitstyle=unit)
    return box("live.numbox", [next_px(), 560, pres[2], pres[3]], pres, numinlets=1, numoutlets=2,
               outlettype=["", "float"], parameter_enable=1, varname=name,
               saved_attribute_attributes=va, **FONT)


def label(text, pres, size=9.0, color=TXT, bold=False):
    return box("comment", [next_px(), 600, pres[2], pres[3]], pres, text=text, numinlets=1, numoutlets=0,
               textcolor=color, fontname="Ableton Sans Bold" if bold else "Ableton Sans Medium", fontsize=size)


def panel(pres, color):
    return box("panel", [next_px(), 640, pres[2], pres[3]], pres, numinlets=1, numoutlets=0,
               bgcolor=color, rounded=6, background=1, mode=0)


# ---------------------------------------------------------------- engine plumbing
engine_id = obj("js dicebag_engine.js", 600, 160, 1, 3, ["", "", ""], w=160)
thisdev = obj("live.thisdevice", 40, 20, 1, 3, ["bang", "int", "int"])
lb_t = obj("t b b", 40, 50, 1, 2, ["bang", "bang"])
line(thisdev, 0, lb_t, 0)

hb = obj("metro 1 @active 1", 200, 20, 2, 1, ["bang"])
cpu = obj("cpuclock", 200, 50, 1, 1, ["float"])
pms = obj("prepend ms", 200, 80, 1, 1, [""])
line(hb, 0, cpu, 0); line(cpu, 0, pms, 0); line(pms, 0, engine_id, 0)

tick = obj("metro 128n @active 1", 340, 20, 2, 1, ["bang"])
tr = obj("transport", 340, 50, 2, 9, ["int", "int", "float", "float", "float", "", "int", "float", ""])
pk = obj("pack 0 0 0.", 340, 80, 3, 1, [""])
ppos = obj("prepend pos", 340, 110, 1, 1, [""])
line(tick, 0, tr, 0)
line(tr, 0, pk, 0); line(tr, 1, pk, 1); line(tr, 2, pk, 2)
line(pk, 0, ppos, 0); line(ppos, 0, engine_id, 0)

mi = obj("midiin", 480, 20, 1, 1, ["int"])
pmidi = obj("prepend midi", 480, 50, 1, 1, [""])
line(mi, 0, pmidi, 0); line(pmidi, 0, engine_id, 0)
mo = obj("midiout", 600, 220, 1, 0, [])
line(engine_id, 0, mo, 0)


def bind(ui_id, pname):
    p = obj("prepend " + pname, PX[0], 700, 1, 1, [""])
    line(ui_id, 0, p, 0)
    line(p, 0, engine_id, 0)
    line(lb_t, 1, ui_id, 0)   # re-send stored values when the device loads
    return p


# ---------------------------------------------------------------- UI (presentation, 169 px high)
H = 169
BG1 = [0.16, 0.16, 0.17, 1.0]
BG2 = [0.20, 0.20, 0.21, 1.0]

# T section
panel([2, 2, 252, 165], BG1)
label("t  ·  TIME", [8, 4, 120, 14], 10, ACCENT, True)
bind(tab("Clock", "Clock", [130, 5, 118, 16], ["Free", "Live sync"], 0), "clock")
bind(dial("Rate", "Rate", [8, 24, 41, 48], 0, 1, 0.5), "rate")
bind(menu("Division", "Div", [8, 76, 46, 15], ["1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/16T", "1/4.", "1/8."], 3), "division")
bind(dial("Jitter", "Jitter", [58, 24, 41, 48], 0, 1, 0.0), "jitter")
bind(dial("t Bias", "Bias", [108, 24, 41, 48], 0, 1, 0.5), "tbias")
bind(dial("Gate Length", "Gate", [158, 24, 41, 48], 0, 1, 0.5), "gatelen")
bind(dial("Gate Random", "G.Rnd", [206, 24, 41, 48], 0, 1, 0.0), "gaternd")
label("t model", [60, 76, 60, 13], 8.5)
bind(menu("t Model", "Model", [60, 90, 120, 15], ["Coin toss", "Random ratio", "Drums", "Independent coins",
                                                  "Fixed divider", "Three states", "Markov"], 0), "tmodel")
label("range", [186, 76, 60, 13], 8.5)
bind(tab("t Range", "Range", [186, 90, 62, 15], ["/4", "x1", "x4"], 1), "trange")
label("notes from t1 / t2 / t3", [8, 112, 160, 13], 8.5)
bind(toggle("Lane 1", "t1", [8, 127, 36, 16], 1, "t1 > X1"), "lane1")
bind(toggle("Lane 2", "t2", [48, 127, 36, 16], 0, "t2 > X2"), "lane2")
bind(toggle("Lane 3", "t3", [88, 127, 36, 16], 1, "t3 > X3"), "lane3")
bind(toggle("Notes", "Notes", [128, 127, 38, 16], 1, "NOTES"), "notes")
bind(toggle("Thru", "Thru", [170, 127, 36, 16], 1, "THRU"), "thru")
bind(intdial("Velocity", "Vel", [210, 112, 41, 48], 1, 127, 100), "vel")
bind(toggle("Y Velocity", "Y>Vel", [8, 147, 50, 16], 0, "Y > VEL"), "yvel")
bind(numbox("Channel", "Ch", [62, 147, 36, 16], 1, 16, 1), "channel")

# DEJA VU section
panel([258, 2, 128, 165], BG2)
label("DÉJÀ VU", [264, 4, 100, 14], 10, ACCENT, True)
bind(dial("Deja Vu", "Deja Vu", [264, 24, 52, 58], 0, 1, 0.5), "dejavu")
bind(dial("Length", "Length", [326, 24, 41, 48], 0, 1, 0.5), "length")
label("t", [264, 90, 12, 13], 9, TXT, True)
bind(tab("Deja Vu t", "DV t", [278, 90, 102, 16], ["off", "on", "lock"], 0), "tdv")
label("X", [264, 112, 12, 13], 9, TXT, True)
bind(tab("Deja Vu X", "DV X", [278, 112, 102, 16], ["off", "on", "lock"], 0), "xdv")
label("noon = locked loop\nright = shuffle loop", [264, 134, 120, 28], 8, [0.7, 0.7, 0.7, 1.0])

# X section
panel([390, 2, 246, 165], BG1)
label("X  ·  VOLTAGES", [396, 4, 140, 14], 10, ACCENT, True)
bind(dial("Spread", "Spread", [396, 24, 41, 48], 0, 1, 0.5), "spread")
bind(dial("X Bias", "Bias", [444, 24, 41, 48], 0, 1, 0.5), "xbias")
bind(dial("Steps", "Steps", [492, 24, 41, 48], 0, 1, 0.5), "steps")
bind(intdial("Root", "Root", [540, 24, 41, 48], 0, 127, 48, 8), "root")
bind(toggle("Ext", "EXT", [588, 30, 42, 18], 0, "EXT"), "ext")
label("scale", [396, 76, 60, 13], 8.5)
bind(menu("Scale", "Scale", [396, 90, 112, 15], ["Major", "Minor", "Pentatonic", "Pelog", "Raag Bhairav", "Raag Shri"], 0), "scale")
label("range", [514, 76, 60, 13], 8.5)
bind(tab("X Range", "Range", [514, 90, 116, 15], ["2 oct", "5 oct", "±5 oct"], 0), "xrange")
label("diversity", [396, 110, 60, 13], 8.5)
bind(tab("X Mode", "Mode", [396, 124, 112, 15], ["same", "X2 vs", "tilt"], 0), "xmode")
label("X clock", [514, 110, 60, 13], 8.5)
bind(menu("X Clock", "XClk", [514, 124, 116, 15], ["t1 / t2 / t3", "all from t1", "all from t2", "all from t3"], 0), "xclock")
label("STEPS: left = smooth, right = quantise", [396, 146, 236, 13], 8, [0.7, 0.7, 0.7, 1.0])

# Y section
panel([640, 2, 108, 165], BG2)
label("Y  ·  SMOOTH", [646, 4, 100, 14], 10, ACCENT, True)
bind(dial("Y Rate", "Y Div", [646, 24, 41, 48], 0, 1, 0.34), "ydiv")
bind(dial("Y Spread", "Y Spr", [698, 24, 41, 48], 0, 1, 0.5), "yspread")
bind(dial("Y Bias", "Y Bias", [646, 90, 41, 48], 0, 1, 0.5), "ybias")
bind(dial("Y Steps", "Y Stp", [698, 90, 41, 48], 0, 1, 0.0), "ysteps")
label("never affected by déjà vu", [646, 146, 100, 13], 7.5, [0.7, 0.7, 0.7, 1.0])

# View
view = box("jsui", [700, 300, 140, 160], [752, 4, 150, 161], filename="dicebag_view.js",
           numinlets=1, numoutlets=0, border=0)
line(engine_id, 2, view, 0)

# Modulation outputs with MAP buttons
panel([906, 2, 268, 165], BG1)
label("MODULATION  ·  click MAP, then any parameter", [912, 4, 260, 14], 9.5, ACCENT, True)
rid = obj("route mod", 600, 260, 1, 2, ["", ""])
line(engine_id, 1, rid, 0)
pmod = obj("prepend mod", 600, 290, 1, 1, [""])
line(rid, 0, pmod, 0)
for i, nm in enumerate(["X1", "X2", "X3", "Y"]):
    y = 22 + i * 36
    mj = obj("js dicebag_map.js %d" % i, 900 + i * 180, 360, 3, 4, ["", "", "", ""], w=140)
    line(pmod, 0, mj, 0)
    initobj = box("message", [900 + i * 180, 330, 40, 22], text="init", numinlets=2, numoutlets=1, outlettype=[""])
    line(lb_t, 0, initobj, 0)
    line(initobj, 0, mj, 0)
    # MAP / clear buttons (not Live parameters)
    mapb = box("live.text", [900 + i * 180, 300, 40, 18], [912, y, 38, 16], numinlets=1, numoutlets=2,
               outlettype=["", ""], parameter_enable=0, mode=0, text="MAP", texton="MAP", varname="map%d" % i,
               activebgoncolor=ACCENT, **FONT)
    clr = box("live.text", [945 + i * 180, 300, 20, 18], [952, y, 16, 16], numinlets=1, numoutlets=2,
              outlettype=["", ""], parameter_enable=0, mode=0, text="x", texton="x", varname="clr%d" % i, **FONT)
    line(mapb, 0, mj, 1)
    line(clr, 0, mj, 2)
    # LED feedback onto the MAP button while waiting for a click
    setb = obj("prepend set", 900 + i * 180, 420, 1, 1, [""])
    line(mj, 3, setb, 0)
    line(setb, 0, mapb, 0)
    # target label
    lab = label("%s  —  not mapped" % nm, [972, y, 200, 14], 8.5)
    line(mj, 1, lab, 0)
    # min / max (Live parameters, like the LFO's range)
    mn = box("live.numbox", [900 + i * 180, 450, 44, 15], [972, y + 15, 44, 15], numinlets=1, numoutlets=2,
             outlettype=["", "float"], parameter_enable=1, varname="%s Min" % nm,
             saved_attribute_attributes=valueof("%s Min" % nm, "Min", 0, parameter_mmin=0.0, parameter_mmax=100.0,
                                                parameter_initial=[0.0], parameter_initial_enable=1,
                                                parameter_unitstyle=5), **FONT)
    mx = box("live.numbox", [950 + i * 180, 450, 44, 15], [1022, y + 15, 44, 15], numinlets=1, numoutlets=2,
             outlettype=["", "float"], parameter_enable=1, varname="%s Max" % nm,
             saved_attribute_attributes=valueof("%s Max" % nm, "Max", 0, parameter_mmin=0.0, parameter_mmax=100.0,
                                                parameter_initial=[100.0], parameter_initial_enable=1,
                                                parameter_unitstyle=5), **FONT)
    for src, msg in ((mn, "min"), (mx, "max")):
        sc = obj("/ 100.", 900 + i * 180, 480, 2, 1, ["float"])
        pp = obj("prepend " + msg, 900 + i * 180, 510, 1, 1, [""])
        line(src, 0, sc, 0); line(sc, 0, pp, 0); line(pp, 0, mj, 0)
        line(lb_t, 1, src, 0)
    on = box("live.text", [1000 + i * 180, 450, 40, 15], [1072, y + 15, 34, 15], numinlets=1, numoutlets=2,
             outlettype=["", ""], parameter_enable=1, mode=1, text="ON", texton="ON", varname="%s On" % nm,
             saved_attribute_attributes=valueof("%s On" % nm, "%s On" % nm, 2, parameter_enum=["off", "on"],
                                                parameter_mmax=1, parameter_initial=[1], parameter_initial_enable=1),
             activebgoncolor=ACCENT, **FONT)
    pa = obj("prepend active", 1000 + i * 180, 480, 1, 1, [""])
    line(on, 0, pa, 0); line(pa, 0, mj, 0); line(lb_t, 1, on, 0)
    # live.remote~ : values -> left inlet, "id N" -> right inlet
    rt = obj("route id", 900 + i * 180, 540, 1, 2, ["", ""])
    pid = obj("prepend id", 900 + i * 180, 570, 1, 1, [""])
    rem = obj("live.remote~", 900 + i * 180, 600, 2, 0, [])
    line(mj, 0, rt, 0)
    line(rt, 0, pid, 0); line(pid, 0, rem, 1)
    line(rt, 1, rem, 0)
    # persistent target path (stored in the Live set)
    pat = box("newobj", [900 + i * 180, 630, 150, 22], text="pattr dicebag_target_%d" % (i + 1),
              numinlets=1, numoutlets=3, outlettype=["", "", ""], parameter_enable=1,
              varname="dicebag_target_%d" % (i + 1),
              saved_attribute_attributes=valueof("Map Target %d" % (i + 1), "Target %d" % (i + 1), 3,
                                                 parameter_invisible=1))
    prs = obj("prepend restore", 900 + i * 180, 660, 1, 1, [""])
    line(mj, 2, pat, 0)
    line(pat, 0, prs, 0)
    line(prs, 0, mj, 0)


W = 1176
patcher = {
    "patcher": {
        "fileversion": 1,
        "appversion": {"major": 8, "minor": 6, "revision": 0, "architecture": "x64", "modernui": 1},
        "classnamespace": "box",
        "rect": [100.0, 100.0, 1400.0, 800.0],
        "openinpresentation": 1,
        "default_fontsize": 10.0,
        "default_fontname": "Arial",
        "gridsize": [8.0, 8.0],
        "devicewidth": float(W),
        "description": "DICEBAG by kalabash - random sampler, sequencer and modulator. Algorithms ported from Marbles by Emilie Gillet (MIT licence).",
        "digest": "DICEBAG",
        "tags": "",
        "boxes": boxes,
        "lines": lines,
        "dependency_cache": [
            {"name": "dicebag_engine.js", "bootpath": ".", "type": "TEXT", "implicit": 1},
            {"name": "dicebag_map.js", "bootpath": ".", "type": "TEXT", "implicit": 1},
            {"name": "dicebag_view.js", "bootpath": ".", "type": "TEXT", "implicit": 1},
        ],
        "latency": 0,
        "is_mpe": 0,
        "oscreceiveudpport": 0,
        "minimum_live_version": "",
        "minimum_max_version": "",
        "platform_compatibility": 0,
        "autosave": 0,
    }
}

data = json.dumps(patcher, indent=1).encode("utf-8") + b"\x00"
blob = (b"ampf" + struct.pack("<I", 4) + b"mmmm" + b"meta" + struct.pack("<I", 4) + b"\x00\x00\x00\x00"
        + b"ptch" + struct.pack("<I", len(data)) + data)
open(os.path.join(OUT, "DICEBAG.amxd"), "wb").write(blob)
# also a plain .maxpat for opening/freezing in Max directly
open(os.path.join(OUT, "DICEBAG.maxpat"), "w").write(json.dumps(patcher, indent=1))
print("boxes", len(boxes), "lines", len(lines), "bytes", len(blob))
