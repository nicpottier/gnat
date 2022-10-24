Import("env")
import os
import shutil

def populate_gh_pages(source, target, env):
    # Get the version number from the build environment.
    version = os.environ.get('VERSION', "")

    # Clean up the version number
    if version == "":
        # When no version is specified default to "0.0.1" 
        version = "v0.0.1"

    build_dir = os.path.join(".pio", "build")
    gh_pages_dir = os.path.join(build_dir, "gh-pages")
    assets_dir = os.path.join(gh_pages_dir, "assets")
    firmware_dir = os.path.join(assets_dir, "firmwares", version)
    release_dir = os.path.join(gh_pages_dir, "_releases")
    bin_dir = os.path.join(build_dir, env["PIOENV"])

    os.makedirs(firmware_dir, exist_ok=True)
    os.makedirs(release_dir, exist_ok=True)

    # 1) copy our built binary into our firmware dir
    shutil.copy(str(target[0]), firmware_dir)
    print(f"Wrote built binary: {str(target[0])} to {firmware_dir}")

    # 2) generate a manifest file for this env for use by gh-pages
    MANIFEST_JSON = """
    {
        "name": "GNAT",
        "version": "$VERSION",
        "new_install_skip_erase": false,
        "builds": [
            {
                "chipFamily": "ESP32",
                "parts": [
                    {
                        "path": "https://nicpottier.github.io/gnat/assets/esp32/bootloader_dio_40m.bin",
                        "offset": 4096
                    },
                    {
                        "path": "https://nicpottier.github.io/gnat/assets/esp32/partitions.bin",
                        "offset": 32768
                    },
                    {
                        "path": "https://nicpottier.github.io/gnat/assets/esp32/boot_app0.bin",
                        "offset": 57344
                    },
                    {
                        "path": "https://nicpottier.github.io/gnat/assets/firmwares/$VERSION/$PROGNAME.bin",
                        "offset": 65536
                    }
                ]
            }
        ]
    }
    """

    manifest_json = MANIFEST_JSON
    manifest_json = manifest_json.replace("$VERSION", version)
    manifest_json = manifest_json.replace("$PROGNAME", env["PROGNAME"])

    manifest_path = os.path.join(firmware_dir, env["PROGNAME"] + ".json")
    with open(manifest_path, "w") as f:
        f.write(manifest_json)

    print(f"Wrote manifest: {manifest_path}")

    # 3) create a release markdown file for jekyll to use
    RELEASE_MD = """---
name: gnat-$VERSION
version: $VERSION
platforms:
    - ttgo
    - m5stick
    - ttgo-s3
---
"""

    release_md = RELEASE_MD
    release_md = release_md.replace("$VERSION", version)

    # output a gh-pages release md file as well
    release_path = os.path.join(release_dir, f"gnat_{version}.md")
    with open(release_path, "w") as f:
        f.write(release_md)

    print(f"Wrote release md: {release_path}")

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", populate_gh_pages)