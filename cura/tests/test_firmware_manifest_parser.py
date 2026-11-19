# SPDX-License-Identifier: LGPL-3.0-or-later
"""El lector de manifiesto y el mapa del FIRMWARE (Marlin/src/lcd/extui/mks_ui/arquis_objects.cpp, compilado en el PC con
tests/firmware/arq_cli.cpp, ASan + UBSan) frente al parser de referencia de Cura y los vectores compartidos.

Tipo de prueba: SIMULADA en el PC con el codigo REAL del modulo (logica pura, sin Marlin ni LVGL). No prueba la placa ni la SD."""
import json
import os
import random
import shutil
import subprocess
import zlib

import pytest

from arquis_engines import manifest_engine as me
from arquis_engines.m486_engine import M486Options, process_gcode
from arquis_engines.object_engine import build_objects
from arquis_engines.validation_engine import gcode_object_stats
from real_gcode import derive_scene, load_reduced, split_like_cura

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
FW = os.path.join(ROOT, "tests", "firmware")
VEC = os.path.join(ROOT, "shared", "object_protocol", "test_vectors")
CHUNKS = (1, 7, 512, 100000)

pytestmark = pytest.mark.skipif(shutil.which("g++") is None, reason="necesita g++")


@pytest.fixture(scope="module")
def cli(tmp_path_factory):
    out = str(tmp_path_factory.mktemp("arq") / "arq_cli")
    subprocess.run([os.path.join(FW, "build_cli.sh"), out, "san"], check=True, capture_output=True)
    return out


def run_parse(cli, tmp_path, text, chunk=512, bed=(0, 0)):
    p = tmp_path / "f.gcode"
    p.write_bytes(text.encode("utf-8"))
    r = subprocess.run([cli, "parse", str(p), str(chunk), str(bed[0]), str(bed[1])], capture_output=True, text=True)
    assert r.returncode == 0, "ASan/UBSan o fallo del arnes: " + r.stderr[:500]
    return json.loads(r.stdout)


def ref(text, bed=None):
    r = me.parse_text(text, bed)
    errs = sorted({p.code for p in r.problems if p.severity == "error"})
    return r, errs


def same(fw, r, errs):
    if r.ok:
        return fw["status"] == "ok"
    if not errs:                                           # sin manifiesto
        return fw["status"] == "none"
    return fw["status"] == "bad" and sorted(fw["errors"]) == errs


def objects_match(fw, m):
    assert fw["print_id"] == "%08X" % m.pid and fw["n"] == len(m.objects) and fw["org"] == m.origin and fw["seq"] == m.seq
    assert fw["bed"] == [m.bed_w, m.bed_d, m.min_x, m.min_y] and fw["n_names"] == len(m.names)
    for fo, ro in zip(fw["objects"], sorted(m.objects, key=lambda o: o.id)):
        assert (fo["id"], fo["x"], fo["y"], fo["width"], fo["depth"], fo["shape"], fo["z0"], fo["ztop"], fo["order"], fo["instance"]) == \
               (ro.id, ro.cx, ro.cy, ro.w, ro.d, ro.shape, ro.z0, ro.ztop, ro.order, ro.inst)
        assert fo["name"] == m.names[ro.name_idx] and fo["label"] == m.display_name(ro)


def test_firmware_parser_reproduces_every_shared_vector_for_every_chunking(cli, tmp_path):
    v = json.load(open(os.path.join(VEC, "manifest_v1.json"), encoding="utf-8"))
    assert len(v["cases"]) >= 40
    for c in v["cases"]:
        e = c["expect"]
        bed = tuple(c["bed_check"]) if c["bed_check"] else (0, 0)
        for chunk in CHUNKS:
            fw = run_parse(cli, tmp_path, c["text"], chunk, bed)
            if e["ok"]:
                assert fw["status"] == "ok", (c["name"], chunk, fw)
                m = me.parse_text(c["text"], tuple(c["bed_check"]) if c["bed_check"] else None).manifest
                objects_match(fw, m)
            elif not e["errors"]:
                assert fw["status"] == "none", (c["name"], chunk, fw)
            else:
                assert fw["status"] == "bad" and sorted(fw["errors"]) == e["errors"], (c["name"], chunk, fw, e["errors"])


def _valid_text():
    v = json.load(open(os.path.join(VEC, "manifest_v1.json"), encoding="utf-8"))
    return next(c["text"] for c in v["cases"] if c["name"] == "valid_30_with_settings_tail")


def test_firmware_parser_matches_reference_on_random_mutations(cli, tmp_path):
    rnd = random.Random(486)
    base = _valid_text()
    lines = base.split("\n")
    bi = lines.index(me.BEGIN)
    n_checked = 0
    for it in range(450):
        L = list(lines)
        kind = rnd.choice(["flip", "flip", "del", "dup", "swap", "trunc", "cr", "ins", "num"])
        i = rnd.randrange(max(0, bi - 1), len(L))
        if kind == "flip":
            s = L[i]
            if s:
                j = rnd.randrange(len(s)); L[i] = s[:j] + rnd.choice("0123456789ABCDEF -;:xZ\t") + s[j + 1:]
        elif kind == "del":
            del L[i]
        elif kind == "dup":
            L.insert(i, L[i])
        elif kind == "swap" and i + 1 < len(L):
            L[i], L[i + 1] = L[i + 1], L[i]
        elif kind == "trunc":
            L = L[:i]
        elif kind == "cr":
            L[i] = L[i] + rnd.choice(["\r", "\r\r", " ", "\t"])
        elif kind == "ins":
            L.insert(i, rnd.choice([";ARQ:X 1", ";ARQ:O 0", ";ARQ:M 40 q", "", ";ARQUIS_OBJECTS_BEGIN", ";ARQUIS_OBJECTS_END CRC=00000000", ";ARQ:H " + "z" * 120]))
        elif kind == "num":
            s = L[i]
            j = next((k for k, ch in enumerate(s) if ch.isdigit()), None)
            if j is not None:
                L[i] = s[:j] + rnd.choice(["99999", "70000", "-1", "40000", "300000000000", "0"]) + s[j + 1:]
        text = "\n".join(L)
        bed = rnd.choice([None, (3000, 3000), (4500, 4500)])
        r, errs = ref(text, bed)
        for chunk in (rnd.choice(CHUNKS), 512):
            fw = run_parse(cli, tmp_path, text, chunk, bed or (0, 0))
            assert same(fw, r, errs), "mutacion %d (%s) trozo %d: firmware=%s referencia ok=%s errores=%s" % (it, kind, chunk, fw.get("status"), r.ok, errs) + \
                " fw_errors=%s\n" % fw.get("errors") + "\n".join(text.split("\n")[bi:bi + 4])
            if r.ok:
                objects_match(fw, r.manifest)
        n_checked += 1
    assert n_checked == 450


def _rebuilt(body, pid, n, trailer_pid=None):
    """Bloque con END y trailer CORRECTOS para un cuerpo ya mutado: asi el parser llega a las validaciones semanticas."""
    crc = me.crc32_lines(body)
    return "\n".join([me.BEGIN] + body + ["%s CRC=%08X" % (me.END, crc)]) + "\n" + "%s CRC=%08X PID=%08X N=%d\n" % (me.TRAILER, crc, pid if trailer_pid is None else trailer_pid, n)


def test_firmware_parser_matches_reference_on_structural_mutations_with_valid_crc(cli, tmp_path):
    """Muta CAMPOS del manifiesto y recalcula el CRC: ejercita E_ID_*, E_SIZE, E_Z, E_ROT, E_OUTSIDE_BED, E_ORDER, E_NAME_*, E_CAP, E_COUNT..."""
    rnd = random.Random(30)
    base = _valid_text()
    head = base[:base.index(me.BEGIN)]
    blk = base[base.index(me.BEGIN):]
    body = blk.split("\n")[1:blk.split("\n").index(next(l for l in blk.split("\n") if l.startswith(me.END)))]
    seen = set()
    for it in range(600):
        B = list(body)
        for _ in range(rnd.choice([1, 1, 2, 3])):
            i = rnd.randrange(len(B))
            f = B[i].split(" ")
            kind = rnd.choice(["field", "field", "del", "dup", "swap"])
            if kind == "del":
                del B[i]
            elif kind == "dup":
                B.insert(i, B[i])
            elif kind == "swap" and i + 1 < len(B):
                B[i], B[i + 1] = B[i + 1], B[i]
            elif len(f) > 2:
                k = rnd.randrange(2, len(f))
                if f[k].lstrip("-").isdigit():
                    f[k] = str(rnd.choice([0, 1, 2, 29, 30, 31, 32, 33, 359, 360, 3000, 3001, 32767, 32768, 65535, 65536, -1, -32768, -32769, 99999999999, int(f[k]) + 1, int(f[k]) - 1]))
                elif f[k].startswith(("N=", "CAP=", "SEQ=", "V=", "BED=", "MIN=")):
                    key, _, val = f[k].partition("=")
                    f[k] = key + "=" + rnd.choice(["0", "1", "2", "30", "31", "32", "33", "3000,3000", "70000,3000", "-5,-5", "40000,0", "9"])
                elif f[k] in ("C", "S", "R"):
                    f[k] = rnd.choice(["C", "S", "R", "X", "c"])
                B[i] = " ".join(f)
        pid = rnd.choice([int(me.parse_text(base).manifest.pid), 0xDEADBEEF])
        n = rnd.choice([30, 30, 29, 31])
        text = head + _rebuilt(B, pid, n)
        bed = rnd.choice([None, (3000, 3000), (2200, 2200)])
        r, errs = ref(text, bed)
        seen.add(tuple(errs))
        fw = run_parse(cli, tmp_path, text, rnd.choice(CHUNKS), bed or (0, 0))
        assert same(fw, r, errs), "estructural %d: firmware=%s %s | referencia ok=%s %s\n%s" % (it, fw.get("status"), fw.get("errors"), r.ok, errs, "\n".join(B[:3]))
        if r.ok:
            objects_match(fw, r.manifest)
    assert len(seen) >= 20, "el fuzz estructural debe ejercitar muchas combinaciones de errores distintas (%d)" % len(seen)


def test_firmware_parser_never_trusts_a_cut_or_garbled_tail(cli, tmp_path):
    base = _valid_text()
    for cut in range(0, len(base), 997):
        t = base[:cut]
        r, errs = ref(t)
        assert same(run_parse(cli, tmp_path, t, 61), r, errs), cut
    rnd = random.Random(7)
    for _ in range(40):
        raw = bytearray(base.encode("utf-8"))
        for _ in range(rnd.randint(1, 30)):
            raw[rnd.randrange(len(raw))] = rnd.randrange(256)
        p = tmp_path / "g.gcode"
        p.write_bytes(bytes(raw))
        out = subprocess.run([cli, "parse", str(p), str(rnd.choice(CHUNKS)), "0", "0"], capture_output=True, text=True)
        assert out.returncode == 0, out.stderr[:400]                  # sin caidas ni UB con bytes cualesquiera


def test_crc32_matches_zlib(cli):
    rnd = random.Random(1)
    for n in (0, 1, 3, 16, 100, 255):
        data = bytes(rnd.randrange(256) for _ in range(n))
        out = subprocess.run([cli, "batch"], input="crc32 " + " ".join(map(str, data)) + "\n", capture_output=True, text=True).stdout.strip()
        assert out == "%08X" % (zlib.crc32(data) & 0xFFFFFFFF)


def _batch(cli, cmds):
    r = subprocess.run([cli, "batch"], input="\n".join(cmds) + "\n", capture_output=True, text=True)
    assert r.returncode == 0, r.stderr[:400]
    return r.stdout.splitlines()


def test_bed_map_and_state_match_all_shared_vectors(cli):
    v = json.load(open(os.path.join(VEC, "bed_map_v1.json"), encoding="utf-8"))
    cmds, exp = [], []
    def bv(c):
        b, w = c["bed"], c["view"]
        return " ".join(map(str, b)) + " " + " ".join(str(x) for x in w[:5]) + " %d %d %d" % (1 if w[5] is not None else 0, w[5] or 0, w[6] or 0)
    for c in v["cases"]:
        fn = c["fn"]
        if fn == "div_round":
            cmds.append("div_round %d %d" % tuple(c["in"])); exp.append(str(c["out"]))
        elif fn == "to_px":
            cmds.append("to_px %s %d %d" % (bv(c), *c["in"])); exp.append("%d %d" % tuple(c["out"]))
        elif fn == "marker":
            i, cx, cy, w, d, sh = c["in"]
            cmds.append("marker %s %d %d %d %d %d %s" % (bv(c), i, cx, cy, w, d, sh)); exp.append(" ".join(map(str, c["out"])))
        elif fn == "pick":
            ms = c["markers"]
            cmds.append("pick %d %s %d %d" % (len(ms), " ".join(" ".join(map(str, m)) for m in ms), *c["in"])); exp.append(str(c["out"]))
        elif fn == "object_state":
            k = c["in"]
            cmds.append("object_state %d %d %d %d %d %d %d %d %d" % (k["available"], k["trusted"], k["canceled"], k["seq"], k["z0"], k["ztop"], k["order"], k["z_print"], k["active_order"]))
            exp.append(str(c["out"]))
    out = _batch(cli, cmds)
    assert len(out) == len(exp) > 380
    bad = [(cmds[i], out[i], exp[i]) for i in range(len(exp)) if out[i] != exp[i]]
    assert not bad, bad[:5]


def _real_manifest(name):
    chunks = split_like_cura(load_reduced(name))
    scene = derive_scene(chunks)
    res = process_gcode(chunks, scene, M486Options(bed=(0, 0, 450, 450), expected_bed=None))
    assert res.status == "ok" and res.manifest is not None
    return "".join(res.chunks), res.manifest


@pytest.mark.parametrize("name", ["real_3_distintos", "real_4_iguales"])
def test_firmware_reads_the_manifest_of_real_cura_gcode_and_rejects_the_wrong_bed(cli, tmp_path, name):
    text, man = _real_manifest(name)
    fw = run_parse(cli, tmp_path, text, 512, (4500, 4500))                      # el perfil de esos G-code: CR-10 Max 450 x 450
    assert fw["status"] == "ok"
    objects_match(fw, me.parse_text(text).manifest)
    wrong = run_parse(cli, tmp_path, text, 512, (3000, 3000))                   # la maquina real: 300 x 300  =>  NO se acepta
    assert wrong["status"] == "bad" and wrong["errors"] == ["E_BED_MISMATCH"]
    crlf = run_parse(cli, tmp_path, text.replace("\n", "\r\n"), 200, (4500, 4500))
    assert crlf["status"] == "ok"
