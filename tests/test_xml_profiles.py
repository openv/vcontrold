#!/usr/bin/env python3
"""Validate invariants shared by the shipped XML controller profiles."""

import pathlib
import sys
import xml.etree.ElementTree as ET


def fail(message):
    print(message, file=sys.stderr)
    return False


def validate(profile):
    config_path = profile / "vcontrold.xml"
    commands_path = profile / "vito.xml"
    config = ET.parse(config_path).getroot()
    commands = ET.parse(commands_path).getroot()

    device_ids = {
        node.attrib["ID"] for node in commands.findall("./devices/device")
    }
    selected = config.find("./unix/config/device")
    if selected is None or selected.attrib.get("ID") not in device_ids:
        return fail(f"{profile}: selected device is not defined")

    units = {
        node.findtext("abbrev") for node in config.findall("./units/unit")
    }
    names = set()
    getters = {}
    ok = True
    strict = profile.name == "20c8"
    for command in commands.findall("./commands/command"):
        name = command.attrib.get("name", "")
        if not name or name in names:
            ok = fail(f"{profile}: duplicate or empty command name {name!r}")
        names.add(name)
        address = command.findtext("addr")
        length = command.findtext("len")
        unit = command.findtext("unit")
        if strict and (not address or not length):
            ok = fail(f"{profile}: {name} lacks address or length")
        if unit and unit not in units:
            ok = fail(f"{profile}: {name} references unknown unit {unit}")
        key = (address, length, unit)
        if name.startswith("get"):
            getters[key] = name
        elif (strict and name.startswith("set") and
              name != "setVitoSystemzeit"):
            if key not in getters:
                ok = fail(
                    f"{profile}: {name} has no preceding matching getter")

    return ok


def main():
    source = pathlib.Path(__file__).resolve().parents[1]
    profiles = sorted(
        path.parent for path in (source / "xml").glob("*/vcontrold.xml")
    )
    return 0 if all(validate(profile) for profile in profiles) else 1


if __name__ == "__main__":
    raise SystemExit(main())
