#!/usr/bin/env python3
# Syncs the stock profile library from the de1app repo into profiles/.
# Run manually when you want to pick up new or updated profiles:
#
#   python3 shared/sync_profiles.py
#
# Which profiles actually get compiled into the firmware is controlled by
# profiles/gnat_profiles.txt, see shared/generate_profiles.py.

import json
import os
import urllib.parse
import urllib.request

REPO_API = "https://api.github.com/repos/decentespresso/de1app/contents/de1plus/profiles"
RAW_BASE = "https://raw.githubusercontent.com/decentespresso/de1app/master/de1plus/profiles/"
PROFILE_DIR = os.path.join(os.path.dirname(__file__), "..", "profiles")


def main():
    os.makedirs(PROFILE_DIR, exist_ok=True)

    with urllib.request.urlopen(REPO_API) as resp:
        listing = json.load(resp)

    names = sorted(f["name"] for f in listing if f["name"].endswith(".tcl"))
    for name in names:
        url = RAW_BASE + urllib.parse.quote(name)
        with urllib.request.urlopen(url) as resp:
            data = resp.read()
        with open(os.path.join(PROFILE_DIR, name), "wb") as f:
            f.write(data)
        print("synced %s (%d bytes)" % (name, len(data)))

    print("synced %d profiles" % len(names))


if __name__ == "__main__":
    main()
