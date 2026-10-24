"""Synthetic data for checking a release's executables where there is no Python (the release
workflow's clean containers): a 360 copy, a GOG game in a Wine prefix with a save, and a Steam
game, made with the tests' builders. Nothing of the game: every file is made here.
Usage: release_fixture.py DIR"""
import os
import shutil
import sys
import zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from test_installer_cpp import ConvertCppTest, _real_cegui_dll  # noqa: E402
from test_tools import fake_exe  # noqa: E402

GOG_TIMESTAMP, STEAM_TIMESTAMP = 0x4FFC6815, 0x4B5F7519


def make(root):
    os.makedirs(root)
    pak, pc, image = ConvertCppTest().paks(root)
    with zipfile.ZipFile(pak, "a") as z:
        z.writestr("media/UI/mastermenu_xenon.layout", "<GUILayout />")
    x360 = os.path.join(root, "x360")
    os.makedirs(x360)
    os.replace(pak, os.path.join(x360, "pak.zip"))
    with open(image, "rb") as f:
        data = bytearray(f.read())
    data[0:2] = b"MZ"
    with open(os.path.join(x360, "default.bin"), "wb") as f:
        f.write(data)
    os.remove(image)
    games = {"gog": os.path.join(root, "prefix", "drive_c", "GOG Games", "Torchlight"),
             "steam": os.path.join(root, "steam", "Torchlight")}
    for store, game in games.items():
        os.makedirs(game)
        fake_exe(os.path.join(game, "Torchlight.exe"), GOG_TIMESTAMP if store == "gog" else STEAM_TIMESTAMP)
        _real_cegui_dll(os.path.join(game, "CEGUIBase.dll"))
        shutil.copyfile(pc, os.path.join(game, "Pak.zip"))
    os.remove(pc)
    saves = os.path.join(root, "prefix", "drive_c", "users", "me", "AppData", "Roaming", "runic games", "torchlight", "save")
    os.makedirs(saves)
    with open(os.path.join(saves, "0.svt"), "wb") as f:
        f.write(b"a synthetic save")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    make(sys.argv[1])
