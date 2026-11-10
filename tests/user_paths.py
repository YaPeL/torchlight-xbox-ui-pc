"""The user's own folders kept out of the tests (imported by test_tools, before anything else runs).

A test on Windows once backed up the real saves in %APPDATA%\\runic games\\torchlight\\save
(InstallTest.test_backs_up_the_saves_of_the_games_prefix, through install.save_folders). Here:
- the variables that name the user's folders (APPDATA, LOCALAPPDATA, USERPROFILE, HOME, HOMEDRIVE,
  HOMEPATH) point to a temporary folder for the whole run, the installers run by the tests too;
- writing, removing or moving anything in the real ones fails the test that tried
  (RealUserPathError), except the system's temporary folder and this repository;
- check() (each test module's tearDownModule) fails when the real game's and mod's folders changed
  during the run, as a subprocess could do it out of reach of the patches.
"""
import builtins
import io
import os
import shutil
import tempfile

REPO = os.path.realpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
TEMP = os.path.realpath(tempfile.gettempdir())
VARIABLES = ("APPDATA", "LOCALAPPDATA", "USERPROFILE", "HOME", "HOMEDRIVE", "HOMEPATH")


class RealUserPathError(AssertionError):
    pass


def _real_folders():
    found = [os.environ.get(v) for v in ("APPDATA", "LOCALAPPDATA", "USERPROFILE", "HOME")]
    found.append(os.path.expanduser("~"))
    return sorted({os.path.realpath(f) for f in found if f})


def _watched():
    """The folders whose content must not change: the game's and the mod's in the real profile."""
    appdata, local = os.environ.get("APPDATA"), os.environ.get("LOCALAPPDATA")
    return ([os.path.join(appdata, "runic games")] if appdata else []) + \
        ([os.path.join(local, "x360ui")] if local else [])


def _inside(path, folder):
    return path == folder or path.startswith(folder.rstrip(os.sep) + os.sep)


def _snapshot(folders):
    state = {}
    for folder in folders:
        for root, dirs, files in os.walk(folder):
            for name in dirs + files:
                p = os.path.join(root, name)
                try:
                    s = os.stat(p)
                    state[p] = (s.st_size, s.st_mtime_ns)
                except OSError:
                    state[p] = None
        state[folder] = os.path.isdir(folder)
    return state


_installed = False
REAL = []
WATCHED = []
_before = {}


def guard(path):
    """Raises when path is in the user's real folders (the temporary folder and the repository
    excepted)."""
    if isinstance(path, int) or path is None:
        return
    p = os.path.realpath(os.fspath(path))
    if _inside(p, TEMP) or _inside(p, REPO) or _inside(p, FAKE):
        return
    for folder in REAL:
        if _inside(p, folder):
            raise RealUserPathError(f"a test touched the user's real folder: {p}")


def _wrap(module, name, positions=(0,), keywords=()):
    real = getattr(module, name)

    def wrapper(*args, **kwargs):
        for i in positions:
            if i < len(args):
                guard(args[i])
        for k in keywords:
            if k in kwargs:
                guard(kwargs[k])
        return real(*args, **kwargs)
    wrapper.__wrapped__ = real
    setattr(module, name, wrapper)


def _wrap_open(module):
    real = module.open

    def wrapper(file, mode="r", *args, **kwargs):
        if any(c in str(mode) for c in "wax+"):
            guard(file)
        return real(file, mode, *args, **kwargs)
    wrapper.__wrapped__ = real
    module.open = wrapper


def _wrap_os_open():
    real = os.open
    writes = os.O_WRONLY | os.O_RDWR | os.O_CREAT | os.O_TRUNC | os.O_APPEND

    def wrapper(path, flags, *args, **kwargs):
        if flags & writes:
            guard(path)
        return real(path, flags, *args, **kwargs)
    wrapper.__wrapped__ = real
    os.open = wrapper


FAKE = os.path.realpath(tempfile.mkdtemp(prefix="x360ui-tests-home-"))


def install():
    global _installed
    if _installed:
        return
    _installed = True
    REAL[:] = _real_folders()
    WATCHED[:] = _watched()
    _before.update(_snapshot(WATCHED))
    for v in ("APPDATA", "LOCALAPPDATA"):
        os.makedirs(os.path.join(FAKE, v), exist_ok=True)
    os.environ.update({"APPDATA": os.path.join(FAKE, "APPDATA"), "LOCALAPPDATA": os.path.join(FAKE, "LOCALAPPDATA"),
                       "USERPROFILE": FAKE, "HOME": FAKE})
    drive, rest = os.path.splitdrive(FAKE)
    if os.name == "nt":
        os.environ.update({"HOMEDRIVE": drive, "HOMEPATH": rest})
    _wrap_open(builtins)
    _wrap_open(io)
    _wrap_os_open()
    for name in ("makedirs", "mkdir", "remove", "unlink", "rmdir", "utime", "chmod"):
        _wrap(os, name)
    for name in ("rename", "replace"):
        _wrap(os, name, (0, 1), ("src", "dst"))
    for name in ("copyfile", "copy", "copy2", "copytree"):
        _wrap(shutil, name, (1,), ("dst",))
    _wrap(shutil, "move", (0, 1), ("src", "dst"))
    _wrap(shutil, "rmtree", (0,), ("path",))


def check():
    """Fails when the real game's or mod's folders changed during the run."""
    after = _snapshot(WATCHED)
    if after != _before:
        changed = sorted(k for k in set(after) | set(_before) if after.get(k) != _before.get(k))
        raise RealUserPathError("the user's real folders changed during the tests: " + ", ".join(changed[:10]))
