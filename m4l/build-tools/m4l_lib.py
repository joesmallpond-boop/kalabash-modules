"""Tiny builder for Max for Live devices (.amxd) in the kalabash / DICEBAG style.

A Device collects boxes + patchlines; helpers create Live UI objects bound to an
external's named parameters ("<name> <value>" messages), styled like DICEBAG:
dark section panels, orange headings, Live's own dials/tabs/menus.
"""
import json
import struct

ACCENT = [1.0, 0.62, 0.25, 1.0]
TXT = [0.86, 0.86, 0.86, 1.0]
DIM = [0.68, 0.68, 0.68, 1.0]
BG1 = [0.16, 0.16, 0.17, 1.0]
BG2 = [0.20, 0.20, 0.21, 1.0]
FONT = {"fontname": "Ableton Sans Medium", "fontsize": 9.5}

# parameter_unitstyle values
U_INT, U_FLOAT, U_TIME, U_HZ, U_DB, U_PCT, U_PAN, U_ST, U_MIDI = 0, 1, 2, 3, 4, 5, 6, 7, 8


def valueof(longname, shortname, ptype, **kw):
    v = {"parameter_longname": longname, "parameter_shortname": shortname, "parameter_type": ptype}
    v.update(kw)
    return {"valueof": v}


class Device:
    def __init__(self, kind):
        assert kind in ("midi", "instrument", "audio")
        self.kind = kind
        self.boxes, self.lines = [], []
        self._n = 0
        self._px = 20
        self.target = None      # object id receiving "<param> <value>"
        self.loadbang = None    # [t b b] fired by live.thisdevice
        self.names = set()

    # ---- low level
    def nid(self):
        self._n += 1
        return "obj-%d" % self._n

    def box(self, maxclass, rect, pres=None, **kw):
        b = {"id": self.nid(), "maxclass": maxclass, "patching_rect": rect}
        if pres is not None:
            b["presentation"] = 1
            b["presentation_rect"] = pres
        b.update(kw)
        self.boxes.append({"box": b})
        return b["id"]

    def obj(self, text, x, y, ins=1, outs=1, outtype=None, w=None):
        return self.box("newobj", [x, y, w or max(40, 7 * len(text) + 10), 22], text=text,
                        numinlets=ins, numoutlets=outs, outlettype=outtype if outtype is not None else [""] * outs)

    def line(self, src, so, dst, di):
        self.lines.append({"patchline": {"source": [src, so], "destination": [dst, di]}})

    def px(self):
        self._px += 60
        return self._px

    def _unique(self, name):
        assert name not in self.names, "duplicate parameter name " + name
        self.names.add(name)

    # ---- binding
    def bind(self, ui, pname, reload=True):
        p = self.obj("prepend " + pname, self._px, 760, 1, 1, [""])
        self.line(ui, 0, p, 0)
        self.line(p, 0, self.target, 0)
        if reload:
            self.line(self.loadbang, 1, ui, 0)
        return ui

    # ---- styled UI
    def panel(self, pres, color=BG1):
        return self.box("panel", [self.px(), 900, pres[2], pres[3]], pres, numinlets=1, numoutlets=0,
                        bgcolor=color, rounded=6, background=1, mode=0)

    def label(self, text, pres, size=9.0, color=TXT, bold=False):
        return self.box("comment", [self.px(), 940, pres[2], pres[3]], pres, text=text, numinlets=1, numoutlets=0,
                        textcolor=color, fontname="Ableton Sans Bold" if bold else "Ableton Sans Medium",
                        fontsize=size)

    def header(self, text, pres):
        return self.label(text, pres, 10, ACCENT, True)

    def dial(self, pname, longname, short, pres, lo=0.0, hi=1.0, init=0.5, unit=U_FLOAT, exp=1.0, steps=0):
        self._unique(longname)
        va = valueof(longname, short, 0, parameter_mmin=lo, parameter_mmax=hi, parameter_initial=[init],
                     parameter_initial_enable=1, parameter_unitstyle=unit, parameter_exponent=exp)
        if steps:
            va["valueof"]["parameter_steps"] = steps
        ui = self.box("live.dial", [self.px(), 600, 41, 48], pres, numinlets=1, numoutlets=2,
                      outlettype=["", "float"], parameter_enable=1, varname=longname,
                      saved_attribute_attributes=va, activedialcolor=ACCENT, **FONT)
        return self.bind(ui, pname)

    def idial(self, pname, longname, short, pres, lo, hi, init, unit=U_INT):
        self._unique(longname)
        va = valueof(longname, short, 1, parameter_mmin=lo, parameter_mmax=hi, parameter_initial=[init],
                     parameter_initial_enable=1, parameter_unitstyle=unit)
        ui = self.box("live.dial", [self.px(), 600, 41, 48], pres, numinlets=1, numoutlets=2,
                      outlettype=["", "float"], parameter_enable=1, varname=longname,
                      saved_attribute_attributes=va, activedialcolor=ACCENT, **FONT)
        return self.bind(ui, pname)

    def tab(self, pname, longname, short, pres, items, init=0, lines=1):
        self._unique(longname)
        va = valueof(longname, short, 2, parameter_enum=items, parameter_mmax=len(items) - 1,
                     parameter_initial=[init], parameter_initial_enable=1, parameter_unitstyle=10)
        ui = self.box("live.tab", [self.px(), 660, pres[2], pres[3]], pres, numinlets=1, numoutlets=3,
                      outlettype=["", "", "float"], parameter_enable=1, varname=longname,
                      num_lines_patching=lines, num_lines_presentation=lines,
                      saved_attribute_attributes=va, activebgoncolor=ACCENT, **FONT)
        return self.bind(ui, pname)

    def menu(self, pname, longname, short, pres, items, init=0):
        self._unique(longname)
        va = valueof(longname, short, 2, parameter_enum=items, parameter_mmax=len(items) - 1,
                     parameter_initial=[init], parameter_initial_enable=1)
        ui = self.box("live.menu", [self.px(), 690, pres[2], pres[3]], pres, numinlets=1, numoutlets=3,
                      outlettype=["", "", "float"], parameter_enable=1, varname=longname,
                      saved_attribute_attributes=va, **FONT)
        return self.bind(ui, pname)

    def toggle(self, pname, longname, text, pres, init=0):
        self._unique(longname)
        va = valueof(longname, text, 2, parameter_enum=["off", "on"], parameter_mmax=1,
                     parameter_initial=[init], parameter_initial_enable=1)
        ui = self.box("live.text", [self.px(), 720, pres[2], pres[3]], pres, numinlets=1, numoutlets=2,
                      outlettype=["", ""], parameter_enable=1, varname=longname, mode=1,
                      text=text, texton=text, saved_attribute_attributes=va,
                      activebgoncolor=ACCENT, **FONT)
        return self.bind(ui, pname)

    def pulse(self, pname, text, pres, ms=30):
        """Momentary button: sends '<pname> 1', then '<pname> 0' after `ms` milliseconds."""
        ui = self.box("live.text", [self.px(), 820, pres[2], pres[3]], pres, numinlets=1, numoutlets=2,
                      outlettype=["", ""], parameter_enable=0, mode=0, text=text, texton=text,
                      varname="btn_" + pname, activebgoncolor=ACCENT, **FONT)
        x = self._px
        t = self.obj("t b b", x, 850, 1, 2, ["bang", "bang"])
        on = self.box("message", [x, 880, 50, 22], text="%s 1" % pname, numinlets=2, numoutlets=1, outlettype=[""])
        dl = self.obj("delay %d" % ms, x + 60, 880, 2, 1, ["bang"])
        off = self.box("message", [x + 60, 910, 50, 22], text="%s 0" % pname, numinlets=2, numoutlets=1,
                       outlettype=[""])
        self.line(ui, 0, t, 0)
        self.line(t, 1, on, 0)
        self.line(t, 0, dl, 0)
        self.line(dl, 0, off, 0)
        self.line(on, 0, self.target, 0)
        self.line(off, 0, self.target, 0)
        return ui

    def numbox(self, pname, longname, short, pres, lo, hi, init, unit=U_INT, ptype=1):
        self._unique(longname)
        va = valueof(longname, short, ptype, parameter_mmin=lo, parameter_mmax=hi, parameter_initial=[init],
                     parameter_initial_enable=1, parameter_unitstyle=unit)
        ui = self.box("live.numbox", [self.px(), 790, pres[2], pres[3]], pres, numinlets=1, numoutlets=2,
                      outlettype=["", "float"], parameter_enable=1, varname=longname,
                      saved_attribute_attributes=va, **FONT)
        return self.bind(ui, pname)

    def meters(self, src, pres):
        """Stereo output meters fed from object `src` outlets 0/1."""
        x, y, w, h = pres
        for ch in (0, 1):
            m = self.box("live.meter~", [self.px(), 980, 5, 100], [x + ch * (w / 2 + 1), y, w / 2 - 1, h],
                         numinlets=1, numoutlets=1, outlettype=["list"])
            self.line(src, ch, m, 0)

    # ---- plumbing
    def setup(self, external, ins):
        """Creates the DSP external and live.thisdevice; returns the external's id."""
        self.target = self.obj(external, 600, 300, max(1, ins), 2, ["signal", "signal"], w=160)
        td = self.obj("live.thisdevice", 40, 20, 1, 3, ["bang", "int", "int"])
        self.loadbang = self.obj("t b b", 40, 50, 1, 2, ["bang", "bang"])
        self.line(td, 0, self.loadbang, 0)
        out = self.obj("plugout~", 600, 380, 2, 2, ["signal", "signal"])
        self.line(self.target, 0, out, 0)
        self.line(self.target, 1, out, 1)
        if self.kind == "audio":
            pin = self.obj("plugin~", 600, 220, 2, 2, ["signal", "signal"])
            self.line(pin, 0, self.target, 0)
            self.line(pin, 1, self.target, 1)
        else:
            mi = self.obj("midiin", 400, 220, 1, 1, ["int"])
            pm = self.obj("prepend midi", 400, 250, 1, 1, [""])
            self.line(mi, 0, pm, 0)
            self.line(pm, 0, self.target, 0)
        return self.target

    # ---- output
    def write(self, path, width, description, deps=()):
        patcher = {
            "patcher": {
                "fileversion": 1,
                "appversion": {"major": 8, "minor": 6, "revision": 0, "architecture": "x64", "modernui": 1},
                "classnamespace": "box",
                "rect": [100.0, 100.0, 1400.0, 1100.0],
                "openinpresentation": 1,
                "default_fontsize": 10.0,
                "default_fontname": "Arial",
                "gridsize": [8.0, 8.0],
                "devicewidth": float(width),
                "description": description,
                "digest": "",
                "tags": "",
                "boxes": self.boxes,
                "lines": self.lines,
                "dependency_cache": [{"name": d, "type": "iLaX", "implicit": 1} for d in deps],
                "latency": 0,
                "is_mpe": 0,
                "autosave": 0,
            }
        }
        data = json.dumps(patcher, indent=1).encode("utf-8") + b"\x00"
        code = {"midi": b"mmmm", "instrument": b"iiii", "audio": b"aaaa"}[self.kind]
        blob = (b"ampf" + struct.pack("<I", 4) + code + b"meta" + struct.pack("<I", 4) + b"\x00\x00\x00\x00"
                + b"ptch" + struct.pack("<I", len(data)) + data)
        with open(path, "wb") as f:
            f.write(blob)
        with open(path.replace(".amxd", ".maxpat"), "w") as f:
            json.dump(patcher, f, indent=1)
        return len(self.boxes), len(self.lines)
