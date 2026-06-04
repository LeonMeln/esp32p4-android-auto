#!/usr/bin/env python3
"""
Offline code generator for the on-device VESC controller-config tables.

Parses the official VESC Tool parameter metadata
(res/config/<ver>/parameters_{mcconf,appconf}.xml) and emits C source under
components/vesc_config/generated/ describing every config parameter: its type,
transmit (vTx) encoding, editor bounds, enum names, default value, grouping
tree (tabs -> subgroups, with ::sep:: separators) and serialize order. It also
computes the firmware config *signature* (crc32c over the serialize-order
metadata) and bakes it in, so the device can self-check that its table matches
the firmware it talks to.

This script is NOT compiled into the firmware. Re-run it when bumping the set
of supported firmware versions. See CLAUDE.md ("Воспроизведение игнорируемых
артефактов") for where the vesc_tool sources live.

Usage:
    python3 tools/gen_vesc_config.py \
        --vesc-root ~/Downloads/vesc_tool-master \
        --out components/vesc_config/generated \
        --versions 6.05,6.06,7.00
"""

import argparse
import math
import os
import struct
import sys
import xml.etree.ElementTree as ET

# ---------------------------------------------------------------------------
# CFG_T / VESC_TX_T (must mirror vesc_config_types.h and VESC Tool datatypes.h)
# ---------------------------------------------------------------------------
CFG_T_DOUBLE, CFG_T_INT, CFG_T_QSTRING, CFG_T_ENUM, CFG_T_BOOL, CFG_T_BITFIELD = 1, 2, 3, 4, 5, 6

# VESC_TX_T integer -> serialized byte count for INT/DOUBLE params.
TX_INT_BYTES = {1: 1, 2: 1, 3: 2, 4: 2, 5: 4, 6: 4}   # UINT8 INT8 UINT16 INT16 UINT32 INT32
TX_DOUBLE16, TX_DOUBLE32, TX_DOUBLE32_AUTO = 7, 8, 9

FLAG_EDIT_PCT = 0x01
FLAG_SHOW_DISP = 0x02
FLAG_TRANSMIT = 0x04


def crc32c(data: bytes) -> int:
    """Castagnoli CRC-32C, reflected poly 0x82F63B78 — matches Utility::crc32c."""
    crc = 0xFFFFFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ (0x82F63B78 if (crc & 1) else 0)
    return crc ^ 0xFFFFFFFF


def _text(el, tag, default=None):
    c = el.find(tag)
    if c is not None and c.text is not None:
        return c.text
    return default


class Param:
    __slots__ = ("name", "type", "vtx", "vtx_scale", "editor_scale", "long_name",
                 "suffix", "decimals", "mn", "mx", "step", "def_is_double",
                 "def_d", "def_i", "enum_names", "flags")

    def __init__(self, el):
        self.name = el.tag
        self.type = int(_text(el, "type", "0"))
        self.vtx = int(_text(el, "vTx", "0"))                 # default VESC_TX_UNDEFINED
        self.vtx_scale = float(_text(el, "vTxDoubleScale", "1"))
        self.editor_scale = float(_text(el, "editorScale", "1"))
        self.long_name = _text(el, "longName", "") or ""
        self.suffix = _text(el, "suffix", "") or ""
        self.enum_names = [e.text or "" for e in el.findall("enumNames")]

        transmit = int(_text(el, "transmittable", "1"))
        edit_pct = int(_text(el, "editAsPercentage", "0"))
        show_disp = int(_text(el, "showDisplay", "0"))
        self.flags = ((FLAG_EDIT_PCT if edit_pct else 0)
                      | (FLAG_SHOW_DISP if show_disp else 0)
                      | (FLAG_TRANSMIT if transmit else 0))

        if self.type == CFG_T_DOUBLE:
            self.mn = float(_text(el, "minDouble", "0"))
            self.mx = float(_text(el, "maxDouble", "0"))
            self.step = float(_text(el, "stepDouble", "1"))
            self.decimals = int(_text(el, "editorDecimalsDouble", "2"))
            self.def_is_double = True
            self.def_d = float(_text(el, "valDouble", "0"))
            self.def_i = 0
        elif self.type == CFG_T_INT:
            self.mn = float(_text(el, "minInt", "0"))
            self.mx = float(_text(el, "maxInt", "0"))
            self.step = float(_text(el, "stepInt", "1"))
            self.decimals = 0
            self.def_is_double = False
            self.def_d = 0.0
            self.def_i = int(_text(el, "valInt", "0"))
        else:  # ENUM / BOOL / BITFIELD / QSTRING / undefined
            self.mn = self.mx = self.step = 0.0
            self.decimals = 0
            self.def_is_double = False
            self.def_d = 0.0
            self.def_i = int(_text(el, "valInt", "0"))

    def wire_bytes(self):
        if self.type == CFG_T_DOUBLE:
            return 2 if self.vtx == TX_DOUBLE16 else 4
        if self.type == CFG_T_INT:
            return TX_INT_BYTES.get(self.vtx, 0)
        if self.type in (CFG_T_ENUM, CFG_T_BOOL, CFG_T_BITFIELD):
            return 1
        return 0  # QSTRING never appears in serialize order


class GroupEntry:
    __slots__ = ("param_name", "sep_label")  # exactly one is set

    def __init__(self, param_name=None, sep_label=None):
        self.param_name = param_name
        self.sep_label = sep_label


class Subgroup:
    def __init__(self, name):
        self.name = name
        self.entries = []  # list[GroupEntry]


class Group:
    def __init__(self, name):
        self.name = name
        self.subgroups = []  # list[Subgroup]


def parse_kind(xml_path):
    root = ET.parse(xml_path).getroot()

    params = []
    name_to_idx = {}
    for el in root.find("Params"):
        idx = len(params)
        p = Param(el)
        name_to_idx[p.name] = idx
        params.append(p)

    ser = [e.text for e in root.find("SerOrder")]

    groups = []
    grouping_el = root.find("Grouping")
    if grouping_el is not None:
        for g_el in grouping_el.findall("group"):
            g = Group(_text(g_el, "groupName", "") or "")
            for sg_el in g_el.findall("subgroup"):
                sg = Subgroup(_text(sg_el, "subgroupName", "") or "")
                sp = sg_el.find("subgroupParams")
                if sp is not None:
                    for prm in sp.findall("param"):
                        name = prm.text or ""
                        if name.startswith("::sep::"):
                            sg.entries.append(GroupEntry(sep_label=name[len("::sep::"):]))
                        else:
                            sg.entries.append(GroupEntry(param_name=name))
                g.subgroups.append(sg)
            groups.append(g)

    return params, name_to_idx, ser, groups


def signature(params, name_to_idx, ser):
    parts = []
    for name in ser:
        p = params[name_to_idx[name]]
        parts.append(name)
        parts.append(str(p.type))
        parts.append(str(p.vtx))
        parts.extend(p.enum_names)
    return crc32c("".join(parts).encode("utf-8"))


def serialized_size(params, name_to_idx, ser):
    total = 4  # uint32 signature
    for name in ser:
        total += params[name_to_idx[name]].wire_bytes()
    return total


# ---------------------------------------------------------------------------
# Byte-exact default-blob serializer (mirrors ConfigParams::getParamSerial +
# VByteArray). Used for the host cross-check in --self-test.
# ---------------------------------------------------------------------------
def _f32(x):
    return struct.unpack("f", struct.pack("f", float(x)))[0]


def _round_half_away(x):
    return math.ceil(x - 0.5) if x < 0 else math.floor(x + 0.5)


def _append_be(buf, value, nbytes, signed):
    mask = (1 << (8 * nbytes)) - 1
    buf += (int(value) & mask).to_bytes(nbytes, "big", signed=False)


def _append_f32_auto(buf, number):
    number = _f32(number)
    if abs(number) < 1.5e-38:
        number = 0.0
    fr, e = math.frexp(number)
    fr = _f32(fr)
    fr_abs = abs(fr)
    fr_s = 0
    if fr_abs >= 0.5:
        fr_s = int(_f32(_f32(_f32(fr_abs - 0.5) * 2.0) * 8388608.0)) & 0xFFFFFFFF
        e += 126
    res = ((e & 0xFF) << 23) | (fr_s & 0x7FFFFF)
    if fr < 0:
        res |= 1 << 31
    _append_be(buf, res, 4, False)


def serialize_defaults(params, name_to_idx, ser):
    buf = bytearray()
    _append_be(buf, signature(params, name_to_idx, ser), 4, False)
    for name in ser:
        p = params[name_to_idx[name]]
        if p.type == CFG_T_DOUBLE:
            if p.vtx == TX_DOUBLE16:
                _append_be(buf, _round_half_away(p.def_d * p.vtx_scale), 2, False)
            elif p.vtx == TX_DOUBLE32:
                _append_be(buf, _round_half_away(p.def_d * p.vtx_scale), 4, False)
            else:  # DOUBLE32_AUTO
                _append_f32_auto(buf, p.def_d)
        elif p.type == CFG_T_INT:
            _append_be(buf, p.def_i, TX_INT_BYTES[p.vtx], False)
        elif p.type in (CFG_T_ENUM, CFG_T_BOOL, CFG_T_BITFIELD):
            _append_be(buf, p.def_i, 1, False)
    return bytes(buf)


# ---------------------------------------------------------------------------
# C emitter
# ---------------------------------------------------------------------------
class StrPool:
    """Deduplicated, NUL-terminated string pool. Offset 0 is the empty string."""

    def __init__(self):
        self._buf = bytearray(b"\x00")     # offset 0 == ""
        self._off = {"": 0}

    def add(self, s):
        s = s if s is not None else ""
        if s in self._off:
            return self._off[s]
        off = len(self._buf)
        self._off[s] = off
        self._buf += s.encode("utf-8") + b"\x00"
        return off

    def emit(self, var):
        # Emit as a C string with explicit \xNN escapes so embedded NULs and
        # any UTF-8 bytes survive verbatim.
        out = [f"static const char {var}[{len(self._buf)}] =\n"]
        line = '    "'
        col = 0
        for b in self._buf:
            if b == 0:
                # Three-digit octal: unambiguous regardless of the next char
                # (a bare "\0" would merge with a following digit into one
                # octal escape, e.g. "\0" + "45" -> '\045' == '%').
                line += "\\000"
            elif b == ord('"'):
                line += '\\"'
            elif b == ord('\\'):
                line += "\\\\"
            elif 0x20 <= b < 0x7F:
                line += chr(b)
            else:
                # hex escape; close+reopen the literal to avoid swallowing the
                # following char into the escape (e.g. "\xABc").
                line += f'\\x{b:02x}""'
            col += 1
            if col >= 24:
                out.append(line + '"\n')
                line = '    "'
                col = 0
        out.append(line + '";\n')
        return "".join(out)


def _cfloat(x):
    x = float(x)
    if x == int(x) and abs(x) < 1e15:
        return f"{int(x)}.0f"
    return f"{x!r}f"


def _cdouble(x):
    x = float(x)
    if x == int(x) and abs(x) < 1e15:
        return f"{int(x)}.0"
    return repr(x)


def emit_table(prefix, params, name_to_idx, ser, groups, fw_major, fw_minor, kind):
    pool = StrPool()
    out = []

    # Enum-name offset table (flat). Each enum param slices it via (off, count).
    enum_offsets = []        # list of pool offsets
    param_enum_slice = {}    # param index -> (start, count)
    for i, p in enumerate(params):
        if p.enum_names:
            start = len(enum_offsets)
            for en in p.enum_names:
                enum_offsets.append(pool.add(en))
            param_enum_slice[i] = (start, len(p.enum_names))

    # Pre-add the strings the param table references so the pool is stable.
    for p in params:
        pool.add(p.name)
        pool.add(p.long_name)
        pool.add(p.suffix)

    # --- params[] ---
    param_lines = []
    for i, p in enumerate(params):
        eoff, ecnt = param_enum_slice.get(i, (0, 0))
        if p.def_is_double:
            defc = f"{{ .d = {_cdouble(p.def_d)} }}"
        else:
            defc = f"{{ .i = {p.def_i} }}"
        param_lines.append(
            "    { "
            f".name_off={pool.add(p.name)}, .longname_off={pool.add(p.long_name)}, "
            f".suffix_off={pool.add(p.suffix)}, "
            f".min={_cfloat(p.mn)}, .max={_cfloat(p.mx)}, .step={_cfloat(p.step)}, "
            f".editor_scale={_cfloat(p.editor_scale)}, .vtx_scale={_cdouble(p.vtx_scale)}, "
            f".def={defc}, .enum_off={eoff}, .enum_count={ecnt}, "
            f".type={p.type}, .vtx={p.vtx}, .decimals={p.decimals}, "
            f".flags=0x{p.flags:02x} }},"
            f"  /* {i}: {p.name} */"
        )

    # --- ser_order[] ---
    ser_idx = [name_to_idx[n] for n in ser]

    # --- grouping (flattened) ---
    group_entries = []   # list[(param_idx, sep_off)]
    subgroups = []       # list[(name_off, first_entry, entry_count)]
    group_arr = []       # list[(name_off, first_sub, sub_count)]
    for g in groups:
        first_sub = len(subgroups)
        for sg in g.subgroups:
            first_entry = len(group_entries)
            for e in sg.entries:
                if e.sep_label is not None:
                    group_entries.append((-1, pool.add(e.sep_label)))
                else:
                    pidx = name_to_idx.get(e.param_name)
                    if pidx is None:
                        sys.stderr.write(
                            f"  warn: grouping param '{e.param_name}' not in Params; skipped\n")
                        continue
                    group_entries.append((pidx, 0))
            subgroups.append((pool.add(sg.name), first_entry,
                              len(group_entries) - first_entry))
        group_arr.append((pool.add(g.name), first_sub, len(subgroups) - first_sub))

    sig = signature(params, name_to_idx, ser)
    size = serialized_size(params, name_to_idx, ser)

    # ---- assemble file ----
    out.append(pool.emit(f"{prefix}_str_pool"))
    out.append("")
    if enum_offsets:
        out.append(f"static const uint32_t {prefix}_enum_off[{len(enum_offsets)}] = {{")
        out.append("    " + ", ".join(str(o) for o in enum_offsets))
        out.append("};")
    else:
        out.append(f"static const uint32_t {prefix}_enum_off[1] = {{ 0 }};")
    out.append("")

    out.append(f"static const vc_param_t {prefix}_params[{len(params)}] = {{")
    out.extend(param_lines)
    out.append("};")
    out.append("")

    out.append(f"static const uint16_t {prefix}_ser[{len(ser_idx)}] = {{")
    for i in range(0, len(ser_idx), 16):
        out.append("    " + ", ".join(str(x) for x in ser_idx[i:i + 16]) + ",")
    out.append("};")
    out.append("")

    out.append(f"static const vc_group_entry_t {prefix}_gentries[{len(group_entries)}] = {{")
    for pidx, soff in group_entries:
        out.append(f"    {{ {pidx}, {soff} }},")
    out.append("};")
    out.append("")

    out.append(f"static const vc_subgroup_t {prefix}_subgroups[{len(subgroups)}] = {{")
    for noff, fe, ec in subgroups:
        out.append(f"    {{ {noff}, {fe}, {ec} }},")
    out.append("};")
    out.append("")

    out.append(f"static const vc_group_t {prefix}_groups[{len(group_arr)}] = {{")
    for noff, fs, sc in group_arr:
        out.append(f"    {{ {noff}, {fs}, {sc} }},")
    out.append("};")
    out.append("")

    out.append(f"const vc_table_t {prefix} = {{")
    out.append(f"    .str_pool = {prefix}_str_pool,")
    out.append(f"    .enum_name_offsets = {prefix}_enum_off,")
    out.append(f"    .params = {prefix}_params, .param_count = {len(params)},")
    out.append(f"    .ser_order = {prefix}_ser, .ser_count = {len(ser_idx)},")
    out.append(f"    .groups = {prefix}_groups, .group_count = {len(group_arr)},")
    out.append(f"    .subgroups = {prefix}_subgroups, .subgroup_count = {len(subgroups)},")
    out.append(f"    .group_entries = {prefix}_gentries, .group_entry_count = {len(group_entries)},")
    out.append(f"    .expected_signature = 0x{sig:08X}u,")
    out.append(f"    .fw_major = {fw_major}, .fw_minor = {fw_minor}, .kind = {kind},")
    out.append("};")
    out.append("")

    return "\n".join(out), sig, size


def ver_to_major_minor(ver):
    a, b = ver.split(".")
    return int(a), int(b)


def sym_ver(ver):
    return ver.replace(".", "_")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--vesc-root", default=os.path.expanduser("~/Downloads/vesc_tool-master"))
    ap.add_argument("--out", default="components/vesc_config/generated")
    ap.add_argument("--versions", default="6.05,6.06,7.00")
    ap.add_argument("--self-test", action="store_true",
                    help="print signatures, sizes and default-blob hashes")
    args = ap.parse_args()

    versions = [v.strip() for v in args.versions.split(",") if v.strip()]
    os.makedirs(args.out, exist_ok=True)

    index = []  # (major, minor, mc_sym, app_sym)
    summary = []

    for ver in versions:
        major, minor = ver_to_major_minor(ver)
        svr = sym_ver(ver)
        file_body = [
            "/* AUTO-GENERATED by tools/gen_vesc_config.py — DO NOT EDIT. */",
            f"/* Source: res/config/{ver}/parameters_{{mcconf,appconf}}.xml */",
            "#include \"vesc_config/vesc_config_types.h\"",
            "",
        ]
        for kind_name, kind_id in (("mcconf", 0), ("appconf", 1)):
            xml_path = os.path.join(args.vesc_root, "res", "config", ver,
                                    f"parameters_{kind_name}.xml")
            if not os.path.isfile(xml_path):
                sys.exit(f"missing {xml_path}")
            params, n2i, ser, groups = parse_kind(xml_path)
            prefix = f"vc_table_{svr}_{kind_name}"
            body, sig, size = emit_table(prefix, params, n2i, ser, groups,
                                         major, minor, kind_id)
            file_body.append(body)
            summary.append((ver, kind_name, sig, size, len(params), len(ser)))
            if args.self_test:
                blob = serialize_defaults(params, n2i, ser)
                print(f"  {ver} {kind_name:8s} sig=0x{sig:08X} size={size:4d}B "
                      f"params={len(params):3d} ser={len(ser):3d} "
                      f"defblob={len(blob)}B crc32c=0x{crc32c(blob):08X}")
            index.append((major, minor, f"vc_table_{svr}_{kind_name}"))

        out_path = os.path.join(args.out, f"vc_table_{svr}.c")
        with open(out_path, "w") as f:
            f.write("\n".join(file_body))
        print(f"wrote {out_path}")

    # index file
    by_ver = {}
    for ver in versions:
        major, minor = ver_to_major_minor(ver)
        svr = sym_ver(ver)
        by_ver[(major, minor)] = (f"vc_table_{svr}_mcconf", f"vc_table_{svr}_appconf")

    idx = [
        "/* AUTO-GENERATED by tools/gen_vesc_config.py — DO NOT EDIT. */",
        "#include \"vesc_config/vesc_config_types.h\"",
        "",
    ]
    for (major, minor), (mc, app) in by_ver.items():
        idx.append(f"extern const vc_table_t {mc};")
        idx.append(f"extern const vc_table_t {app};")
    idx.append("")
    idx.append(f"const vc_version_t g_vc_versions[{len(by_ver)}] = {{")
    for (major, minor), (mc, app) in sorted(by_ver.items()):
        idx.append(f"    {{ {major}, {minor}, &{mc}, &{app} }},")
    idx.append("};")
    idx.append(f"const size_t g_vc_version_count = {len(by_ver)};")
    idx.append("")
    with open(os.path.join(args.out, "vc_tables_index.c"), "w") as f:
        f.write("\n".join(idx))
    print(f"wrote {os.path.join(args.out, 'vc_tables_index.c')}")

    print("\nSummary (ver, kind, signature, blob bytes, #params, #ser):")
    for ver, kind, sig, size, np_, ns in summary:
        print(f"  {ver:5s} {kind:8s} 0x{sig:08X}  {size:4d}B  {np_:3d}  {ns:3d}")


if __name__ == "__main__":
    main()
