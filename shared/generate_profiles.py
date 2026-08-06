# Compiles every profile in profiles/ into the packed header + frame bytes the
# DE1 consumes, generating src/profiles.h. The profiles listed in
# profiles/gnat_profiles.txt make up the default enabled set that the button
# cycles through, the full set is configurable from the config page.
#
# This is a port of the de1app compilation pipeline:
#   - profile.tcl  pressure_to_advanced_list / flow_to_advanced_list
#   - binary.tcl   de1_packed_shot
# Keep it in sync with upstream if the frame format ever changes.

import glob
import json
import os
import sys
import unicodedata

PROFILE_DIR = os.path.join("profiles")
CONFIG_FILE = os.path.join(PROFILE_DIR, "gnat_profiles.txt")
OUTPUT_FILE = os.path.join("src", "profiles.h")

# de1app defaults (machine.tcl) for settings the conversions reference but
# individual profile files may omit
DEFAULTS = {
    "espresso_temperature_steps_enabled": 0,
    "temp_bump_time_seconds": 2,
    "espresso_temperature": 92,
    "espresso_decline_time": 25,
    "preinfusion_time": 5,
    "espresso_hold_time": 10,
    "flow_profile_hold": 2,
    "flow_profile_decline": 1.2,
    "pressure_end": 4,
    "espresso_pressure": 9.2,
    "preinfusion_flow_rate": 4,
    "preinfusion_stop_pressure": 4,
    "maximum_flow": 0,
    "maximum_flow_range_default": 1.0,
    "maximum_pressure": 0,
    "maximum_pressure_range_default": 0.9,
}


# ---------------------------------------------------------------------------
# tcl parsing: profile files are flat "key value key value ..." tcl lists,
# with braces for grouping and no substitution
# ---------------------------------------------------------------------------
def tcl_list(text):
    items = []
    i = 0
    n = len(text)
    while i < n:
        while i < n and text[i] in " \t\n\r":
            i += 1
        if i >= n:
            break
        if text[i] == "{":
            depth = 1
            i += 1
            start = i
            while i < n and depth > 0:
                if text[i] == "{":
                    depth += 1
                elif text[i] == "}":
                    depth -= 1
                i += 1
            if depth != 0:
                raise ValueError("unbalanced braces in tcl list")
            items.append(text[start : i - 1])
        else:
            start = i
            while i < n and text[i] not in " \t\n\r":
                i += 1
            items.append(text[start:i])
    return items


def tcl_dict(text):
    items = tcl_list(text)
    if len(items) % 2 != 0:
        raise ValueError("odd number of elements in tcl dict")
    return dict(zip(items[0::2], items[1::2]))


def parse_profile(path):
    # profile files are "key value" per line, but values can be multi-line
    # brace groups, so parse the whole file as one tcl dict
    with open(path, "r", encoding="utf-8") as f:
        return tcl_dict(f.read())


def num(settings, key):
    val = settings.get(key, DEFAULTS.get(key, 0))
    if val in ("", None):
        return 0.0
    return float(val)


# ---------------------------------------------------------------------------
# pressure/flow -> advanced step list (port of profile.tcl)
# ---------------------------------------------------------------------------
def step(**props):
    return props


def pressure_to_advanced(s):
    steps = []
    if num(s, "espresso_temperature_steps_enabled") == 1:
        first_len = num(s, "temp_bump_time_seconds")
        second_len = max(num(s, "preinfusion_time") - first_len, 0)
        temps = [num(s, "espresso_temperature_%d" % i) for i in range(4)]
    else:
        first_len = 0
        second_len = num(s, "preinfusion_time")
        temps = [num(s, "espresso_temperature")] * 4

    if first_len > 0:
        steps.append(
            step(temperature=temps[0], sensor="coffee", pump="flow", transition="fast",
                 flow=num(s, "preinfusion_flow_rate"), seconds=first_len, volume=0,
                 exit_if=1, exit_type="pressure_over",
                 exit_value=num(s, "preinfusion_stop_pressure")))
    if second_len > 0:
        steps.append(
            step(temperature=temps[1], sensor="coffee", pump="flow", transition="fast",
                 flow=num(s, "preinfusion_flow_rate"), seconds=second_len, volume=0,
                 exit_if=1, exit_type="pressure_over",
                 exit_value=num(s, "preinfusion_stop_pressure")))

    limiter = {}
    if num(s, "maximum_flow") != 0:
        limiter = dict(max_flow_or_pressure=num(s, "maximum_flow"),
                       max_flow_or_pressure_range=num(s, "maximum_flow_range_default"))

    hold_time = num(s, "espresso_hold_time")
    if hold_time > 0:
        if hold_time > 3:
            steps.append(
                step(temperature=temps[2], sensor="coffee", pump="pressure",
                     transition="fast", pressure=num(s, "espresso_pressure"), seconds=3,
                     volume=0, exit_if=0))
            hold_time -= 3
        steps.append(
            step(temperature=temps[2], sensor="coffee", pump="pressure", transition="fast",
                 pressure=num(s, "espresso_pressure"), seconds=hold_time, volume=0,
                 exit_if=0, **limiter))

    decline_time = num(s, "espresso_decline_time")
    if decline_time > 0:
        if hold_time < 3 and decline_time > 3:
            steps.append(
                step(temperature=temps[3], sensor="coffee", pump="pressure",
                     transition="fast", pressure=num(s, "espresso_pressure"), seconds=3,
                     volume=0, exit_if=0))
            decline_time -= 3
        steps.append(
            step(temperature=temps[3], sensor="coffee", pump="pressure",
                 transition="smooth", pressure=num(s, "pressure_end"),
                 seconds=decline_time, volume=0, exit_if=0, **limiter))

    if not steps:
        steps.append(step(temperature=90, sensor="coffee", pump="flow",
                          transition="smooth", flow=0, seconds=0, volume=0, exit_if=0))
    return steps


def flow_to_advanced(s):
    steps = []
    if num(s, "espresso_temperature_steps_enabled") == 1:
        first_len = num(s, "temp_bump_time_seconds")
        second_len = max(num(s, "preinfusion_time") - first_len, 0)
        temps = [num(s, "espresso_temperature_%d" % i) for i in range(4)]
    else:
        first_len = 0
        second_len = num(s, "preinfusion_time")
        temps = [num(s, "espresso_temperature")] * 4

    if first_len > 0:
        steps.append(
            step(temperature=temps[0], sensor="coffee", pump="flow", transition="fast",
                 flow=num(s, "preinfusion_flow_rate"), seconds=first_len, volume=0,
                 exit_if=1, exit_type="pressure_over",
                 exit_value=num(s, "preinfusion_stop_pressure")))
    if second_len > 0:
        steps.append(
            step(temperature=temps[1], sensor="coffee", pump="flow", transition="fast",
                 flow=num(s, "preinfusion_flow_rate"), seconds=second_len, volume=0,
                 exit_if=1, exit_type="pressure_over",
                 exit_value=num(s, "preinfusion_stop_pressure")))

    limiter = {}
    if num(s, "maximum_pressure") != 0:
        limiter = dict(max_flow_or_pressure=num(s, "maximum_pressure"),
                       max_flow_or_pressure_range=num(s, "maximum_pressure_range_default"))

    if num(s, "espresso_hold_time") > 0:
        steps.append(
            step(temperature=temps[2], sensor="coffee", pump="flow", transition="fast",
                 flow=num(s, "flow_profile_hold"), seconds=num(s, "espresso_hold_time"),
                 volume=0, exit_if=0, **limiter))
        steps.append(
            step(temperature=temps[3], sensor="coffee", pump="flow", transition="smooth",
                 flow=num(s, "flow_profile_decline"),
                 seconds=num(s, "espresso_decline_time"), volume=0, exit_if=0, **limiter))

    if not steps:
        steps.append(step(temperature=90, sensor="coffee", pump="flow",
                          transition="smooth", flow=0, seconds=0, volume=0, exit_if=0))
    return steps


def advanced_to_steps(s):
    steps = []
    for raw in tcl_list(s["advanced_shot"]):
        props = tcl_dict(raw)

        def p(key, default=0.0):
            val = props.get(key, default)
            if val in ("", None):
                return default
            return float(val)

        converted = step(
            temperature=p("temperature"),
            sensor=props.get("sensor", "coffee"),
            pump=props.get("pump", "pressure"),
            transition=props.get("transition", "fast"),
            pressure=p("pressure"),
            flow=p("flow"),
            seconds=p("seconds"),
            volume=p("volume"),
            exit_if=int(p("exit_if")),
        )
        if converted["exit_if"] == 1:
            exit_type = props.get("exit_type", "")
            converted["exit_type"] = exit_type
            converted["exit_value"] = p("exit_%s" % exit_type)
        if p("max_flow_or_pressure") != 0:
            converted["max_flow_or_pressure"] = p("max_flow_or_pressure")
            converted["max_flow_or_pressure_range"] = p("max_flow_or_pressure_range")
        steps.append(converted)
    return steps


# ---------------------------------------------------------------------------
# packing (port of binary.tcl de1_packed_shot)
# ---------------------------------------------------------------------------
FLAG_CTRLF = 0x01
FLAG_DOCOMPARE = 0x02
FLAG_DC_GT = 0x04
FLAG_DC_COMPF = 0x08
FLAG_TMIXTEMP = 0x10
FLAG_INTERPOLATE = 0x20
FLAG_IGNORELIMIT = 0x40


def u8p4(val):
    return round(min(val, 16) * 16)


def u8p1(val):
    return round(min(val, 128) * 2)


def f8_1_7(val):
    if val >= 12.75:
        return round(min(val, 127)) | 128
    return round(val * 10)


def u10p0(val):
    return round(val) | 1024


def pack_step(idx, props):
    flag = FLAG_IGNORELIMIT
    if props["pump"] == "flow":
        flag |= FLAG_CTRLF
        set_val = props.get("flow", 0)
    else:
        set_val = props.get("pressure", 0)

    if props["sensor"] == "water":
        flag |= FLAG_TMIXTEMP
    if props["transition"] == "smooth":
        flag |= FLAG_INTERPOLATE

    trigger_val = 0
    if props["exit_if"] == 1:
        exit_flags = {
            "pressure_under": FLAG_DOCOMPARE,
            "pressure_over": FLAG_DOCOMPARE | FLAG_DC_GT,
            "flow_under": FLAG_DOCOMPARE | FLAG_DC_COMPF,
            "flow_over": FLAG_DOCOMPARE | FLAG_DC_GT | FLAG_DC_COMPF,
        }
        exit_type = props.get("exit_type", "")
        if exit_type in exit_flags:
            flag |= exit_flags[exit_type]
            trigger_val = props.get("exit_value", 0)

    max_vol = u10p0(props.get("volume", 0))
    return bytes([
        idx,
        flag,
        u8p4(set_val),
        u8p1(props["temperature"]),
        f8_1_7(props["seconds"]),
        u8p4(trigger_val),
        (max_vol >> 8) & 0xFF,
        max_vol & 0xFF,
    ])


def pack_ext_step(idx, props):
    return bytes([
        idx + 32,
        u8p4(props["max_flow_or_pressure"]),
        u8p4(props.get("max_flow_or_pressure_range", 0)),
        0, 0, 0, 0, 0,
    ])


def compile_profile(settings):
    profile_type = settings.get("settings_profile_type", "settings_2c")
    if profile_type == "settings_2a":
        steps = pressure_to_advanced(settings)
        preinfuse_count = sum(1 for s in steps if s.get("exit_if") == 1)
    elif profile_type == "settings_2b":
        steps = flow_to_advanced(settings)
        preinfuse_count = sum(1 for s in steps if s.get("exit_if") == 1)
    elif profile_type in ("settings_2c", "settings_2c2"):
        steps = advanced_to_steps(settings)
        count_start = settings.get("final_desired_shot_volume_advanced_count_start", "")
        preinfuse_count = int(float(count_start)) if count_start != "" else 0
    else:
        raise ValueError("unsupported profile type: %s" % profile_type)

    if len(steps) > 20:
        raise ValueError("too many frames: %d" % len(steps))

    header = bytes([1, len(steps), preinfuse_count, 0, u8p4(6)])

    frames = [pack_step(i, s) for i, s in enumerate(steps)]
    frames += [pack_ext_step(i, s) for i, s in enumerate(steps)
               if s.get("max_flow_or_pressure", 0) != 0]
    # tail frame, MaxTotalVolume disabled
    frames.append(bytes([len(steps), 0, 0, 0, 0, 0, 0, 0]))

    return header, frames


# ---------------------------------------------------------------------------
# output
# ---------------------------------------------------------------------------
def c_bytes(data):
    return ",".join("0x%02X" % b for b in data)


def c_str(text):
    return text.replace("\\", "\\\\").replace('"', '\\"')


def to_ascii(text):
    # the display fonts only cover ascii, fold accents (é -> e) and drop the rest
    folded = unicodedata.normalize("NFKD", text)
    return folded.encode("ascii", "ignore").decode()


def main():
    with open(CONFIG_FILE, "r", encoding="utf-8") as f:
        defaults = [line.strip() for line in f
                    if line.strip() and not line.startswith("#")]

    compiled = []
    for path in sorted(glob.glob(os.path.join(PROFILE_DIR, "*.tcl"))):
        stem = os.path.splitext(os.path.basename(path))[0]
        try:
            settings = parse_profile(path)
            header, frames = compile_profile(settings)
        except Exception as e:
            sys.stderr.write("skipping %s: %s\n" % (path, e))
            continue

        title = settings.get("profile_title", stem)
        compiled.append((title, stem, header, frames))

    compiled.sort(key=lambda p: p[0].lower())

    # the default enabled set, from gnat_profiles.txt, matched by filename or title
    default_mask = bytearray((len(compiled) + 7) // 8)
    for name in defaults:
        for idx, (title, stem, _, _) in enumerate(compiled):
            if name in (title, stem):
                default_mask[idx // 8] |= 1 << (idx % 8)
                break
        else:
            sys.stderr.write("default profile not found: %s\n" % name)
            sys.exit(1)

    # the first entry in gnat_profiles.txt is the default profile
    default_profile = 0
    for idx, (title, stem, _, _) in enumerate(compiled):
        if defaults[0] in (title, stem):
            default_profile = idx + 1
            break

    out = []
    out.append("// generated by shared/generate_profiles.py, do not edit!")
    out.append("#pragma once")
    out.append("")
    out.append("const int profile_count = %d;" % len(compiled))
    out.append("const int profile_mask_bytes = %d;" % len(default_mask))
    out.append("const uint8_t profile_default_mask[] = {%s};" % c_bytes(default_mask))
    out.append("// the default profile, 1-based, first entry of gnat_profiles.txt")
    out.append("const int profile_default = %d;" % default_profile)
    out.append("")

    for idx, (title, stem, header, frames) in enumerate(compiled):
        out.append("// %s" % title)
        out.append("const uint8_t profile_%d_header[] = {%s};" % (idx, c_bytes(header)))
        out.append("const uint8_t profile_%d_frames[][8] = {" % idx)
        for frame in frames:
            out.append("    {%s}," % c_bytes(frame))
        out.append("};")
        out.append("")

    out.append("typedef struct {")
    out.append("  const char* name;")
    out.append("  const uint8_t* header;")
    out.append("  const uint8_t (*frames)[8];")
    out.append("  int frameCount;")
    out.append("} Profile;")
    out.append("")
    out.append("const Profile profiles[] = {")
    for idx, (title, stem, header, frames) in enumerate(compiled):
        out.append('    {"%s", profile_%d_header, profile_%d_frames, %d},' %
                   (c_str(to_ascii(title)), idx, idx, len(frames)))
    out.append("};")
    out.append("")

    # names as json for the config page
    names_json = json.dumps([title for title, _, _, _ in compiled], ensure_ascii=False)
    out.append('const char profiles_json[] = "%s";' % c_str(names_json))
    out.append("")

    with open(OUTPUT_FILE, "w", encoding="utf-8") as f:
        f.write("\n".join(out))

    total = sum(len(h) + sum(len(f) for f in fr) for _, _, h, fr in compiled)
    print("wrote %s: %d profiles, %d bytes of frames, %d defaults" %
          (OUTPUT_FILE, len(compiled), total, len(defaults)))


main()
