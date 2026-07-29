#!/usr/bin/env python3
"""API-Doku-Generator fuer die Ruby-Bindings (SADS Kap. 19: "API First").

Liest src/RubyVM.cpp und src/RubyRgss.cpp, extrahiert alle
  mrb_define_module(_function)/mrb_define_class/mrb_define_method(_all)-
Aufrufe (auch mehrzeilig) und schreibt docs/API.md — die oeffentliche,
stabile Ruby-API der Engine. Bei jeder Aenderung an den Bindings:

    python3 tools/generate_api_docs.py

Keine Abhaengigkeiten ausser Python 3 (stdlib).
"""
import re
import sys
import collections

SOURCES = ["src/RubyVM.cpp", "src/RubyRgss.cpp"]
OUT = "docs/API.md"

RE_MODULE = re.compile(
    r'struct\s+RClass\*\s*(\w+)\s*=\s*mrb_define_module\(\s*mMrb\s*,\s*"([^"]+)"')
RE_CLASS = re.compile(
    r'(?:struct\s+RClass\*\s*(\w+)\s*=\s*)?mrb_define_class\(\s*mMrb\s*,\s*"([^"]+)"')
RE_DEFINE = re.compile(
    r'mrb_define_(module_function|class_function|method|method_all)\('
    r'\s*mMrb\s*,\s*(\w+)\s*,\s*"([^"]+)"\s*,\s*(\w+)\s*,\s*'
    r'((?:[^()]|\([^()]*\))*)\)',
    re.S)
RE_ARGS_NUM = re.compile(r'MRB_ARGS_(REQ|OPT|NONE|ANY|BLOCK)\s*\(\s*(\d*)\s*\)')


def norm(text: str) -> str:
    """Mehrzeilige Aufrufe auf eine Zeile bringen."""
    return re.sub(r'\s+', ' ', text)


def args_repr(argstr: str) -> str:
    req, opt = 0, 0
    for kind, num in RE_ARGS_NUM.findall(argstr):
        if kind == 'REQ':
            req += int(num or 0)
        elif kind == 'OPT':
            opt += int(num or 0)
        elif kind in ('ANY', 'BLOCK'):
            opt = -1
    if opt < 0:
        return "beliebig" if req == 0 else f"{req}+"
    if req and opt:
        return f"{req}..{req + opt}"
    if opt:
        return f"0..{opt}"
    return str(req)


def main() -> int:
    text = ""
    for src in SOURCES:
        try:
            with open(src, encoding='utf-8') as f:
                text += "\n" + f.read()
        except FileNotFoundError:
            print(f"warn: {src} nicht gefunden", file=sys.stderr)

    var2name = {}   # C++-Variable -> Ruby-Modul/Klasse
    kindof = {}     # C++-Variable -> 'modul'|'klasse'
    members = collections.defaultdict(list)  # Ruby-Name -> [(art, name, arity)]

    for m in RE_MODULE.finditer(text):
        var2name[m.group(1)] = m.group(2)
        kindof[m.group(1)] = 'modul'
    for m in RE_CLASS.finditer(text):
        if m.group(1):
            var2name[m.group(1)] = m.group(2)
            kindof[m.group(1)] = 'klasse'

    flat = norm(text)
    for m in RE_DEFINE.finditer(flat):
        art, var, name, _func, argstr = m.groups()
        ruby_obj = var2name.get(var)
        if not ruby_obj:
            continue
        art_de = ('Modulfunktion' if art == 'module_function'
                  else 'Klassenfunktion' if art == 'class_function'
                  else 'Methode')
        members[ruby_obj].append((art_de, name, args_repr(argstr)))

    lines = []
    lines.append("# Oeffentliche Ruby-API — RPG Maker 3D")
    lines.append("")
    lines.append("> **Automatisch generiert** aus den `mrb_define_*`-Bindungen")
    lines.append("> (`src/RubyVM.cpp`, `src/RubyRgss.cpp`) via")
    lines.append("> `tools/generate_api_docs.py`. Nicht von Hand editieren!")
    lines.append(">")
    lines.append("> SADS Kap. 19/21: Nur was hier steht, ist die oeffentliche,")
    lines.append("> stabile Schnittstelle; Ruby hat keinen Direktzugriff auf")
    lines.append("> interne C++-Objekte (Zugriff ausschliesslich ueber diese API).")
    lines.append("")
    total = sum(len(v) for v in members.values())
    lines.append(f"**{len(members)} Module/Klassen, {total} Funktionen/Methoden.**")
    lines.append("")

    for obj in sorted(members):
        lines.append(f"## `{obj}`")
        lines.append("")
        lines.append("| Art | Name | Argumente |")
        lines.append("|---|---|---|")
        seen = set()
        for art, name, arity in members[obj]:
            if (art, name) in seen:
                continue
            seen.add((art, name))
            lines.append(f"| {art} | `{name}` | {arity} |")
        lines.append("")

    with open(OUT, 'w', encoding='utf-8') as f:
        f.write("\n".join(lines))
    print(f"OK: {OUT} — {len(members)} Module/Klassen, {total} Eintraege")
    return 0


if __name__ == "__main__":
    sys.exit(main())
