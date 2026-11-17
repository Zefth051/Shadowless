#!/usr/bin/env python3
"""Generates the built-in input overlay layouts (schema 1). Run: python3 -I generate_layouts.py
Ids are PHYSICAL key positions named after the US-QWERTY legend, so QWERTY and AZERTY layouts
share ids and differ only in labels. `evdev` is the Linux key code; `win` the Windows scan code
(0xE000 | code for extended keys); mouse buttons use `vk`."""
import json, os

EV = dict(esc=1, minus=12, equal=13, backspace=14, tab=15, lbracket=26, rbracket=27, enter=28, lctrl=29,
          semicolon=39, apostrophe=40, grave=41, lshift=42, backslash=43, comma=51, period=52, slash=53,
          rshift=54, lalt=56, space=57, caps=58, rctrl=97, ralt=100, up=103, left=105, right=106, down=108)
for i, c in enumerate("1234567890"): EV[c] = 2 + i
for i, c in enumerate("qwertyuiop"): EV[c] = 16 + i
for i, c in enumerate("asdfghjkl"): EV[c] = 30 + i
for i, c in enumerate("zxcvbnm"): EV[c] = 44 + i
for i in range(1, 11): EV[f"f{i}"] = 58 + i
EV["f11"], EV["f12"] = 87, 88
EXT = {"rctrl": 0xE01D, "ralt": 0xE038, "up": 0xE048, "left": 0xE04B, "right": 0xE04D, "down": 0xE050}

def key(name, x, y, w=1.0, h=1.0, label=None):
    e = {"id": f"key.{name}", "label": label if label is not None else (name.upper() if len(name) == 1 else name),
         "shape": "round", "x": x, "y": y, "w": w, "h": h, "evdev": EV[name], "win": EXT.get(name, EV[name])}
    return e

def row(names, x, y, widths=None, labels=None):
    out = []
    for i, n in enumerate(names):
        w = (widths or {}).get(n, 1.0)
        out.append(key(n, x, y, w, 1.0, (labels or {}).get(n)))
        x += w
    return out

def mouse(x, y):
    """Mouse body 3 wide, 4.4 tall: outline plus left/right/middle (+ wheel pulses)."""
    return [
        {"shape": "mouseBody", "x": x, "y": y, "w": 3.0, "h": 4.4},
        {"id": "mouse.left", "label": "", "shape": "mouseLeft", "x": x + .12, "y": y + .12, "w": 1.35, "h": 1.7, "evdev": 272, "vk": 1},
        {"id": "mouse.right", "label": "", "shape": "mouseRight", "x": x + 1.53, "y": y + .12, "w": 1.35, "h": 1.7, "evdev": 273, "vk": 2},
        {"id": "mouse.middle", "label": "", "shape": "round", "x": x + 1.3, "y": y + .45, "w": .4, "h": .9, "evdev": 274, "vk": 4},
    ]

def layout(id_, name, scope, elements):
    w = max(e["x"] + e["w"] for e in elements); h = max(e["y"] + e["h"] for e in elements)
    return {"schema": 1, "id": id_, "name": name, "scope": scope, "width": round(w, 3), "height": round(h, 3),
            "elements": elements}

G = 1.1  # key pitch: 1 unit key + .1 gap -> we simply offset by pitch below
def scale(els, k=1.1):
    out = []
    for e in els:
        e = dict(e)
        for f in ("x", "y"): e[f] = round(e[f] * k, 3)
        for f in ("w", "h"): e[f] = round(e[f] * k - (k - 1), 3)  # keep a (k-1) gap between neighbours
        out.append(e)
    return out

def compact(azerty=False):
    L = {"q": "A", "w": "Z", "a": "Q", "z": "W", "1": "&", "2": "é", "3": "\"", "4": "'", "5": "("} if azerty else {}
    els = []
    els += [key("esc", 0, 0, 1, 1, "Esc")]
    els += row(list("12345"), 1.25, 0, labels=L)
    els += row(["tab"] + list("qwert"), 0, 1.05, {"tab": 1.5}, {**L, "tab": "Tab"})
    els += row(list("asdf"), 1.5, 2.1, labels=L)
    els += row(["lshift"] + list("zxc"), 0, 3.15, {"lshift": 2.25}, {**L, "lshift": "Shift"})
    els += row(["lctrl", "lalt", "space"], 0, 4.2, {"lctrl": 1.25, "lalt": 1.25, "space": 3.5}, {"lctrl": "Ctrl", "lalt": "Alt", "space": ""})
    return els

def full(azerty=False):
    L = {"q": "A", "w": "Z", "a": "Q", "z": "W", "m": ",", "comma": ";", "period": ":", "slash": "!", "semicolon": "M",
         "1": "&", "2": "é", "3": "\"", "4": "'", "5": "(", "6": "-", "7": "è", "8": "_", "9": "ç", "0": "à"} if azerty else {}
    S = {"backspace": "⌫", "tab": "Tab", "caps": "Caps", "enter": "Enter", "lshift": "Shift", "rshift": "Shift",
         "lctrl": "Ctrl", "rctrl": "Ctrl", "lalt": "Alt", "ralt": "Alt", "space": "", "grave": "`", "minus": "-", "equal": "=",
         "lbracket": "[", "rbracket": "]", "backslash": "\\", "semicolon": ";", "apostrophe": "'", "comma": ",", "period": ".", "slash": "/", "esc": "Esc"}
    S.update(L)
    els = [key("esc", 0, 0, 1, 1, "Esc")]
    els += row([f"f{i}" for i in range(1, 13)], 2, 0, labels={f"f{i}": f"F{i}" for i in range(1, 13)})
    els += row(["grave"] + list("1234567890") + ["minus", "equal", "backspace"], 0, 1.25, {"backspace": 2}, S)
    els += row(["tab"] + list("qwertyuiop") + ["lbracket", "rbracket", "backslash"], 0, 2.25, {"tab": 1.5, "backslash": 1.5}, S)
    els += row(["caps"] + list("asdfghjkl") + ["semicolon", "apostrophe", "enter"], 0, 3.25, {"caps": 1.75, "enter": 2.25}, S)
    els += row(["lshift"] + list("zxcvbnm") + ["comma", "period", "slash", "rshift"], 0, 4.25, {"lshift": 2.25, "rshift": 2.75}, S)
    els += row(["lctrl", "lalt", "space", "ralt", "rctrl"], 0, 5.25, {"lctrl": 1.25, "lalt": 1.25, "space": 6.25, "ralt": 1.25, "rctrl": 1.25}, S)
    return els

def arrows(x, y):
    sym = {"up": "↑", "left": "←", "down": "↓", "right": "→"}
    return [key("up", x + 1, y, 1, 1, sym["up"]), key("left", x, y + 1, 1, 1, sym["left"]),
            key("down", x + 1, y + 1, 1, 1, sym["down"]), key("right", x + 2, y + 1, 1, 1, sym["right"])]

def dualsense():
    def b(id_, label, shape, x, y, w, h, **codes):
        e = {"id": id_, "label": label, "shape": shape, "x": x, "y": y, "w": w, "h": h}; e.update(codes); return e
    return [
        {"shape": "padBody", "x": 0, "y": 1.2, "w": 11.0, "h": 6.4},
        b("pad.l2", "L2", "round", 0.9, 0, 2.2, .9, evdev=312), b("pad.r2", "R2", "round", 7.9, 0, 2.2, .9, evdev=313),
        b("pad.l1", "L1", "round", 0.9, .95, 2.2, .75, evdev=310), b("pad.r1", "R1", "round", 7.9, .95, 2.2, .75, evdev=311),
        b("pad.dpad.up", "↑", "round", 2.0, 2.4, .8, .8, hat="ABS_HAT0Y<0"), b("pad.dpad.left", "←", "round", 1.2, 3.2, .8, .8, hat="ABS_HAT0X<0"),
        b("pad.dpad.right", "→", "round", 2.8, 3.2, .8, .8, hat="ABS_HAT0X>0"), b("pad.dpad.down", "↓", "round", 2.0, 4.0, .8, .8, hat="ABS_HAT0Y>0"),
        b("pad.north", "△", "circle", 8.2, 2.4, .9, .9, evdev=307), b("pad.west", "□", "circle", 7.4, 3.2, .9, .9, evdev=308),
        b("pad.east", "○", "circle", 9.0, 3.2, .9, .9, evdev=305), b("pad.south", "✕", "circle", 8.2, 4.0, .9, .9, evdev=304),
        b("pad.select", "", "round", 3.9, 2.3, .55, .8, evdev=314), b("pad.start", "", "round", 6.55, 2.3, .55, .8, evdev=315),
        b("pad.mode", "PS", "circle", 5.0, 4.2, 1.0, 1.0, evdev=316),
        b("pad.l3", "L3", "circle", 3.4, 5.1, 1.5, 1.5, evdev=317), b("pad.r3", "R3", "circle", 6.1, 5.1, 1.5, 1.5, evdev=318),
    ]

def mouse_with(els, gap=1.0):
    """Keyboard + mouse side by side, vertically centred on each other (the taller one sets the height)."""
    w = max(e["x"] + e["w"] for e in els)
    top = min(e["y"] for e in els); h = max(e["y"] + e["h"] for e in els) - top
    mouse_h = 4.4
    shift_kb, shift_mouse = (0, (h - mouse_h) / 2) if h >= mouse_h else ((mouse_h - h) / 2, 0)
    els = [dict(e, y=round(e["y"] - top + shift_kb, 3)) for e in els]
    return els + mouse(round(w + gap, 3), round(shift_mouse, 3))

def full_with_side_column(gap=.8):
    """Full keyboard with mouse stacked above the arrow cluster in one right-hand column, so the silhouette
    stays compact (about 3:1) instead of a long strip; the column is centred on the keyboard's height."""
    kb = scale(full())
    w = max(e["x"] + e["w"] for e in kb); h = max(e["y"] + e["h"] for e in kb)
    arr = scale(arrows(0, 0))                      # 3.3 x 2.2
    aw = max(e["x"] + e["w"] for e in arr); ah = max(e["y"] + e["h"] for e in arr)
    colw = max(3.0, aw); stack = 4.4 + .5 + ah
    top = (h - stack) / 2 if h > stack else 0
    x0 = w + gap
    out = kb + mouse(round(x0 + (colw - 3.0) / 2, 3), round(top, 3))
    out += [dict(e, x=round(e["x"] + x0 + (colw - aw) / 2, 3), y=round(e["y"] + top + 4.4 + .5, 3)) for e in arr]
    return out

OUT = os.path.dirname(os.path.abspath(__file__))
SETS = [
    layout("qwerty", "QWERTY Keyboard + Mouse", "gameplay", mouse_with(scale(compact()))),
    layout("qwerty-full", "QWERTY Keyboard + Mouse (Full)", "full", full_with_side_column()),
    layout("arrows", "Arrow Keys + Mouse", "gameplay", mouse_with(scale(arrows(0, 1.1)), 1.0)),
    layout("azerty", "AZERTY Keyboard + Mouse", "gameplay", mouse_with(scale(compact(True)))),
    layout("dualsense", "Dualsense Controller (placeholder)", "pad", dualsense()),
]
for s in SETS:
    path = os.path.join(OUT, f"{s['id']}.json")
    head = {k: v for k, v in s.items() if k != "elements"}
    with open(path, "w", encoding="utf-8") as f:
        f.write(json.dumps(head, ensure_ascii=False)[:-1] + ',\n "elements": [\n')
        f.write(",\n".join("  " + json.dumps(e, ensure_ascii=False) for e in s["elements"]))
        f.write("\n ]}\n")
print("wrote", len(SETS), "layouts")
