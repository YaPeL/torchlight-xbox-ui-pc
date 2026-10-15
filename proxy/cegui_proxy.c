/* The proxy CEGUIBase.dll (x86): every export but three is forwarded by name to the real DLL,
 * renamed CEGUIBase_orig.dll; the three below are thunks that log the call and call the real one.
 *
 *   SchemeManager::loadScheme(const String& filename, const String& resourceGroup)
 *   WindowManager::loadWindowLayout(const String& filename, bool generateRandomPrefix)
 *   WindowManager::loadWindowLayout(const String& filename, const String& name_prefix,
 *                                   const String& resourceGroup, PropertyCallback*, void* userdata)
 *
 * They are __thiscall members of MSVC-built classes. A __fastcall function with a dummy second
 * argument has the same convention (this in ecx, edx unused, the callee pops the stack
 * arguments), so the thunks are plain C and need no MSVC C++ ABI. CEGUI::String is only read,
 * through the real DLL's String::c_str(). No CRT: kernel32 only, plus msvcr90's SEH handler.
 * The log is x360_ui.log next to this DLL. The export table is made by tools/build_proxy.py from
 * the user's own CEGUIBase.dll.
 *
 * x360_ui.ini next to this DLL (optional; without it the proxy only logs):
 *   mode=scheme                                 after PC loads GuiLookSkin.scheme, load the 360 one
 *   out=Z:\home\me\torchlight-gog\x360ui        the converter's OUT (tools/convert_x360_ui.py)
 * In mode=scheme, OUT is added to Ogre's world resource group (a FileSystem location, recursive),
 * so the converted files' own paths (media/ui/x360/...) resolve, and then
 * media/ui/x360/ceguiWidgets.scheme is loaded once, inside __try: if it throws, the game's own
 * call still returns as it would have.
 */

typedef unsigned long DWORD;
typedef int BOOL;
typedef void *HANDLE;
typedef void *HMODULE;
typedef void *FARPROC;

#define IMPORT __declspec(dllimport)
IMPORT HMODULE __stdcall LoadLibraryA(const char *name);
IMPORT FARPROC __stdcall GetProcAddress(HMODULE module, const char *name);
IMPORT DWORD __stdcall GetModuleFileNameA(HMODULE module, char *out, DWORD size);
IMPORT HANDLE __stdcall CreateFileA(const char *name, DWORD access, DWORD share, void *security,
                                    DWORD disposition, DWORD flags, HANDLE templ);
IMPORT BOOL __stdcall WriteFile(HANDLE file, const void *data, DWORD size, DWORD *written, void *overlapped);
IMPORT DWORD __stdcall GetTickCount(void);
IMPORT BOOL __stdcall DisableThreadLibraryCalls(HMODULE module);
IMPORT HMODULE __stdcall GetModuleHandleA(const char *name);
IMPORT BOOL __stdcall ReadFile(HANDLE file, void *data, DWORD size, DWORD *read, void *overlapped);
IMPORT BOOL __stdcall CloseHandle(HANDLE file);

#define REAL_DLL "CEGUIBase_orig.dll"
#define GENERIC_READ 0x80000000UL
#define GENERIC_WRITE 0x40000000UL
#define FILE_SHARE_READ 1UL
#define CREATE_ALWAYS 2UL
#define OPEN_EXISTING 3UL
#define INVALID_HANDLE ((HANDLE)(long)-1)

/* clang may emit calls to these even without a CRT. */
void *memset(void *d, int c, unsigned n) { unsigned char *p = d; while (n--) *p++ = (unsigned char)c; return d; }
void *memcpy(void *d, const void *s, unsigned n) {
    unsigned char *p = d; const unsigned char *q = s; while (n--) *p++ = *q++; return d;
}

static HMODULE self_module;
static HANDLE log_file = INVALID_HANDLE;
static HMODULE real;

/* ---- the log ---- */

static unsigned length(const char *s) { unsigned n = 0; while (s && s[n]) n++; return n; }

static void put(const char *s) {
    DWORD done;
    if (log_file != INVALID_HANDLE && s) WriteFile(log_file, s, length(s), &done, 0);
}

static void put_hex(unsigned long v) {
    char buf[11] = "0x";
    int i;
    for (i = 0; i < 8; i++) buf[2 + i] = "0123456789abcdef"[(v >> (28 - 4 * i)) & 15];
    buf[10] = 0;
    put(buf);
}

static void put_dec(unsigned long v) {
    char buf[12];
    int i = 11;
    buf[i] = 0;
    do { buf[--i] = (char)('0' + v % 10); v /= 10; } while (v);
    put(buf + i);
}

static void open_log(void) {
    char path[520];
    DWORD n = GetModuleFileNameA(self_module, path, 500);
    while (n && path[n - 1] != '\\' && path[n - 1] != '/') n--;
    memcpy(path + n, "x360_ui.log", 12);
    log_file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
    put("x360_ui proxy CEGUIBase.dll: loaded\r\n");
}

static void stamp(void) { put("["); put_dec(GetTickCount()); put("] "); }

/* ---- the configuration ---- */

static int mode_scheme, x360_done;
static char out_dir[400];

static int starts(const char *s, const char *prefix) {
    while (*prefix) if (*s++ != *prefix++) return 0;
    return 1;
}

static void read_config(void) {
    char path[520], text[1024], *line;
    DWORD n = GetModuleFileNameA(self_module, path, 500), got = 0;
    HANDLE f;
    while (n && path[n - 1] != '\\' && path[n - 1] != '/') n--;
    memcpy(path + n, "x360_ui.ini", 12);
    f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (f == INVALID_HANDLE) { put("no x360_ui.ini: mode log\r\n"); return; }
    ReadFile(f, text, sizeof text - 1, &got, 0);
    CloseHandle(f);
    text[got] = 0;
    for (line = text; *line;) {
        char *end = line;
        while (*end && *end != '\r' && *end != '\n') end++;
        if (*end) *end++ = 0;
        if (starts(line, "mode=scheme")) mode_scheme = 1;
        if (starts(line, "out=") && length(line + 4) < sizeof out_dir) memcpy(out_dir, line + 4, length(line + 4) + 1);
        line = end;
        while (*line == '\r' || *line == '\n') line++;
    }
    put(mode_scheme ? "mode scheme, out \"" : "mode log, out \""); put(out_dir); put("\"\r\n");
}

/* ---- the real DLL ---- */

typedef const char *(__fastcall *c_str_fn)(const void *s, void *edx);
typedef void *(__fastcall *load_scheme_fn)(void *self, void *edx, const void *file, const void *group);
typedef void *(__fastcall *load_layout2_fn)(void *self, void *edx, const void *file, int random_prefix);
typedef void *(__fastcall *load_layout5_fn)(void *self, void *edx, const void *file, const void *prefix,
                                            const void *group, void *callback, void *userdata);

static c_str_fn real_c_str;
static load_scheme_fn real_load_scheme;
static load_layout2_fn real_load_layout2;
static load_layout5_fn real_load_layout5;

static FARPROC need(const char *name) {
    FARPROC f;
    if (!real) real = LoadLibraryA(REAL_DLL);
    f = real ? GetProcAddress(real, name) : 0;
    if (!f) { put("FATAL: not found in " REAL_DLL ": "); put(name); put("\r\n"); }
    return f;
}

static void resolve(void) {
    if (real_load_scheme) return;
    real_c_str = (c_str_fn)need("?c_str@String@CEGUI@@QBEPBDXZ");
    real_load_layout2 = (load_layout2_fn)need(
        "?loadWindowLayout@WindowManager@CEGUI@@QAEPAVWindow@2@ABVString@2@_N@Z");
    real_load_layout5 = (load_layout5_fn)need(
        "?loadWindowLayout@WindowManager@CEGUI@@QAEPAVWindow@2@ABVString@2@00P6A_NPAV32@AAV42@2PAX@Z3@Z");
    real_load_scheme = (load_scheme_fn)need("?loadScheme@SchemeManager@CEGUI@@QAEPAVScheme@2@ABVString@2@0@Z");
}

static void put_str(const char *label, const void *s) {
    put(label);
    put("\"");
    put(s && real_c_str ? real_c_str(s, 0) : "(null)");
    put("\"");
}

/* ---- the 360 scheme ---- */

/* Opaque storage for an MSVC-built std::string (28 bytes in VC9) or CEGUI::String (152 bytes in
 * CEGUI 0.6), built and destroyed only by their own DLLs' exported members. */
typedef struct { __declspec(align(8)) unsigned char bytes[256]; } object;

typedef void *(__fastcall *ctor_fn)(void *self, void *edx, const char *text);
typedef void (__fastcall *dtor_fn)(void *self, void *edx);
typedef const char *(__fastcall *std_c_str_fn)(const void *s, void *edx);
typedef void *(__cdecl *singleton_fn)(void);
typedef const void *(__fastcall *world_group_fn)(void *self, void *edx);
typedef void (__fastcall *add_location_fn)(void *self, void *edx, const void *name, const void *type,
                                           const void *group, int recursive);
typedef unsigned char (__fastcall *exists_fn)(void *self, void *edx, const void *group, const void *name);

static FARPROC sym(const char *dll, const char *name) {
    HMODULE m = GetModuleHandleA(dll);
    FARPROC f = m ? GetProcAddress(m, name) : 0;
    if (!f) { put("not found in "); put(dll); put(": "); put(name); put("\r\n"); }
    return f;
}

#define STD_STRING "?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@"

static void load_x360(void *scheme_manager) {
    ctor_fn std_ctor = (ctor_fn)sym("msvcp90.dll", "??0" STD_STRING "QAE@PBD@Z");
    dtor_fn std_dtor = (dtor_fn)sym("msvcp90.dll", "??1" STD_STRING "QAE@XZ");
    std_c_str_fn std_c_str = (std_c_str_fn)sym("msvcp90.dll", "?c_str@" STD_STRING "QBEPBDXZ");
    singleton_fn rgm_get = (singleton_fn)sym("OgreMain.dll", "?getSingleton@ResourceGroupManager@Ogre@@SAAAV12@XZ");
    world_group_fn world = (world_group_fn)sym("OgreMain.dll",
        "?getWorldResourceGroupName@ResourceGroupManager@Ogre@@QBEABV" STD_STRING "XZ");
    add_location_fn add = (add_location_fn)sym("OgreMain.dll",
        "?addResourceLocation@ResourceGroupManager@Ogre@@QAEXABV" STD_STRING "00_N@Z");
    exists_fn exists = (exists_fn)sym("OgreMain.dll",
        "?resourceExists@ResourceGroupManager@Ogre@@QAE_NABV" STD_STRING "0@Z");
    ctor_fn cegui_ctor = (ctor_fn)need("??0String@CEGUI@@QAE@PBD@Z");
    dtor_fn cegui_dtor = (dtor_fn)need("??1String@CEGUI@@QAE@XZ");
    object location, filesystem, probe, file, group;
    void *rgm;
    const void *world_group;
    void *r = 0;

    if (!(std_ctor && std_dtor && rgm_get && world && add && exists && cegui_ctor && cegui_dtor)) return;
    if (!out_dir[0]) { put("mode scheme without out=: nothing loaded\r\n"); return; }
    rgm = rgm_get();
    world_group = world(rgm, 0);
    stamp(); put("world resource group \""); put(std_c_str ? std_c_str(world_group, 0) : "?"); put("\"\r\n");

    std_ctor(&probe, 0, "media/ui/x360probe/probe.txt");
    stamp(); put("before: mods probe media/ui/x360probe/probe.txt exists: ");
    put(exists(rgm, 0, world_group, &probe) ? "yes\r\n" : "no\r\n");
    std_dtor(&probe, 0);

    std_ctor(&location, 0, out_dir);
    std_ctor(&filesystem, 0, "FileSystem");
    std_ctor(&probe, 0, "media/ui/x360/ceguiWidgets.scheme");
    __try {
        add(rgm, 0, &location, &filesystem, world_group, 1);
        stamp(); put("added resource location \""); put(out_dir); put("\"; the scheme exists: ");
        put(exists(rgm, 0, world_group, &probe) ? "yes\r\n" : "no\r\n");
    } __except (1) {
        stamp(); put("addResourceLocation threw\r\n");
    }
    std_dtor(&probe, 0);
    std_dtor(&filesystem, 0);
    std_dtor(&location, 0);

    cegui_ctor(&file, 0, "media/ui/x360/ceguiWidgets.scheme");
    cegui_ctor(&group, 0, "");
    __try {
        stamp(); put("loadScheme (360) \"media/ui/x360/ceguiWidgets.scheme\"\r\n");
        r = real_load_scheme(scheme_manager, 0, &file, &group);
        stamp(); put("loadScheme (360) -> "); put_hex((unsigned long)r); put("\r\n");
    } __except (1) {
        stamp(); put("loadScheme (360) threw; see CEGUI.log\r\n");
    }
    cegui_dtor(&group, 0);
    cegui_dtor(&file, 0);
}

static int ends_with_nocase(const char *s, const char *tail) {
    unsigned n = length(s), m = length(tail), i;
    if (n < m) return 0;
    for (i = 0; i < m; i++) {
        char a = s[n - m + i], b = tail[i];
        if (a >= 'a' && a <= 'z') a -= 32;
        if (b >= 'a' && b <= 'z') b -= 32;
        if (a != b) return 0;
    }
    return 1;
}

/* ---- the thunks (exported under the members' mangled names by the .def) ---- */

void *__fastcall hook_load_scheme(void *self, void *edx, const void *file, const void *group) {
    void *r;
    resolve();
    stamp(); put_str("loadScheme ", file); put_str(" group ", group); put("\r\n");
    r = real_load_scheme(self, 0, file, group);
    stamp(); put("loadScheme -> "); put_hex((unsigned long)r); put("\r\n");
    if (mode_scheme && !x360_done && file && ends_with_nocase(real_c_str(file, 0), "GUILOOKSKIN.SCHEME")) {
        x360_done = 1;
        load_x360(self);
    }
    return r;
}

void *__fastcall hook_load_layout2(void *self, void *edx, const void *file, int random_prefix) {
    void *r;
    resolve();
    stamp(); put_str("loadWindowLayout ", file); put(random_prefix & 0xff ? " random prefix\r\n" : "\r\n");
    r = real_load_layout2(self, 0, file, random_prefix);
    stamp(); put("loadWindowLayout -> "); put_hex((unsigned long)r); put("\r\n");
    return r;
}

void *__fastcall hook_load_layout5(void *self, void *edx, const void *file, const void *prefix,
                                   const void *group, void *callback, void *userdata) {
    void *r;
    resolve();
    stamp(); put_str("loadWindowLayout ", file); put_str(" prefix ", prefix); put_str(" group ", group);
    put("\r\n");
    r = real_load_layout5(self, 0, file, prefix, group, callback, userdata);
    stamp(); put("loadWindowLayout -> "); put_hex((unsigned long)r); put("\r\n");
    return r;
}

BOOL __stdcall DllMain(HMODULE module, DWORD reason, void *reserved) {
    if (reason == 1) {
        self_module = module;
        DisableThreadLibraryCalls(module);
        open_log();
        read_config();
    }
    return 1;
}
