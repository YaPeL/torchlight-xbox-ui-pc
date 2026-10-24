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
 * The log is x360_ui.log in %LOCALAPPDATA%\x360ui (x360_data_path). The export table is made by tools/build_proxy.py from
 * the user's own CEGUIBase.dll.
 *
 * x360_ui.ini next to this DLL (optional; without it the proxy only logs):
 *   mode=scheme                                 after PC loads GuiLookSkin.scheme, load the 360 one
 *   out=Z:\home\me\torchlight-gog\x360ui        the converter's OUT (tools/convert_x360_ui.py)
 *   trace=1                                     also log, once each, the window names the game's
 *                                               own code (Torchlight.exe) looks up, the properties
 *                                               it reads and the texts it sets
 *   pad=1                                       the pad moves the hero (proxy/pad.c, a prototype)
 *   pad_file=Z:\path\pad.txt                    its input from a file instead of XInput (tests)
 *   pad_edges=message                           the virtual button's edges as window messages
 *   pad_mode=direct                             option b: the proxy calls setDestination (GOG only)
 *   pad_face=0                                  the old movement, setDestination only (to compare)
 *   pad_log=1                                   the pad's detailed diagnosis in the log (costly)
 *   pad_replay=Z:\path\route.txt               a recorded stick instead of XInput (a test; pad.c)
 *   pad_trace=1                                 the hero's path to x360_trace.csv (a test; pad.c)
 *   console=ITEM <item name>,3;MONEY 100        game console commands run once in play (test runs)
 *   dialog_fast_scroll=10                       how many times faster a dialog's text rises with X
 *                                               held (the 360's factor is not measured yet; 4 by default)
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
IMPORT BOOL __stdcall VirtualProtect(void *address, DWORD size, DWORD protect, DWORD *old);
IMPORT DWORD __stdcall GetFileAttributesA(const char *name);

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

/* The MSVC ABI expects this symbol in any image that uses floating point (no CRT here). */
int _fltused = 0;

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

/* ---- the mod's own files (x360_ui.log, x360_settings.ini; x360_stop.txt in x360_menu.c) ----
 * In %LOCALAPPDATA%\x360ui (under Wine or Proton, the prefix's): the game's folder may be Program
 * Files, which the game cannot write and Windows would redirect into VirtualStore, where no
 * uninstaller looks. Beside this DLL only when there is no LOCALAPPDATA or its folder cannot be
 * made. Wide paths (any user name); kernel32's W functions, looked up (no import). */
typedef unsigned short wchar16;
static FARPROC kernel32_proc(const char *name) {
    HMODULE k = GetModuleHandleA("kernel32.dll");
    return k ? GetProcAddress(k, name) : 0;
}
static void wcat16(wchar16 *d, unsigned size, const char *ascii) {
    unsigned n = 0;
    while (d[n]) n++;
    while (*ascii && n + 1 < size) d[n++] = (unsigned char)*ascii++;
    d[n] = 0;
}
int x360_data_path(const char *name, wchar16 *out, unsigned size) {
    typedef DWORD (__stdcall *getenv_fn)(const wchar16 *n, wchar16 *o, DWORD s);
    typedef BOOL (__stdcall *mkdir_fn)(const wchar16 *n, void *sec);
    typedef DWORD (__stdcall *attr_fn)(const wchar16 *n);
    typedef DWORD (__stdcall *module_fn)(HMODULE m, wchar16 *o, DWORD s);
    static const wchar16 var[] = {'L','O','C','A','L','A','P','P','D','A','T','A',0};
    getenv_fn env = (getenv_fn)kernel32_proc("GetEnvironmentVariableW");
    mkdir_fn mkdir = (mkdir_fn)kernel32_proc("CreateDirectoryW");
    attr_fn attr = (attr_fn)kernel32_proc("GetFileAttributesW");
    module_fn mod = (module_fn)kernel32_proc("GetModuleFileNameW");
    DWORD n = env ? env(var, out, size - 48) : 0;
    if (n && n < size - 48 && mkdir && attr) {
        wcat16(out, size, "\\x360ui");
        mkdir(out, 0);
        DWORD at = attr(out);
        if (at != (DWORD)-1 && (at & 0x10)) {   /* FILE_ATTRIBUTE_DIRECTORY */
            wcat16(out, size, "\\");
            wcat16(out, size, name);
            return 1;
        }
    }
    n = mod ? mod(self_module, out, size - 48) : 0;
    while (n && out[n - 1] != '\\' && out[n - 1] != '/') n--;
    out[n] = 0;
    wcat16(out, size, name);
    return 0;
}
/* CreateFileW on x360_data_path(name). */
HANDLE x360_data_open(const char *name, DWORD access, DWORD share, DWORD disposition) {
    typedef HANDLE (__stdcall *create_fn)(const wchar16 *n, DWORD a, DWORD s, void *sec, DWORD d, DWORD f, HANDLE t);
    create_fn create = (create_fn)kernel32_proc("CreateFileW");
    wchar16 path[600];
    path[0] = 0;
    x360_data_path(name, path, 600);
    return create && path[0] ? create(path, access, share, 0, disposition, 0x80, 0) : INVALID_HANDLE;
}

static void open_log(void) {
    int local;
    wchar16 path[600];
    path[0] = 0;
    local = x360_data_path("x360_ui.log", path, 600);
    log_file = x360_data_open("x360_ui.log", GENERIC_WRITE, FILE_SHARE_READ, CREATE_ALWAYS);
    put("x360_ui proxy CEGUIBase.dll: loaded\r\n");
    put(local ? "the log in %LOCALAPPDATA%\\x360ui\r\n" : "the log beside the DLL (no LOCALAPPDATA)\r\n");
}

/* For proxy/pad.c. */
void x360_log(const char *s) { put(s); }

void x360_log_num(long v) {
    if (v < 0) { put("-"); v = -v; }
    put_dec((unsigned long)v);
}

static void stamp(void) { put("["); put_dec(GetTickCount()); put("] "); }

/* ---- the configuration ---- */

static int mode_scheme, x360_done, trace, pad, pad_edges_msg, pad_direct, pad_old_face, pad_log;
static char pad_file[400];
/* console=CMD;CMD...: game console commands run once in play (test runs only; proxy/x360_menu.c). */
char x360_console_commands[400];
static char out_dir[400];

/* The texts the 360's code puts in its screens, read by the converter from the user's 360 image
 * into OUT/x360_strings.txt (tools/convert_x360_ui.py --image): key=value lines, UTF-8. The
 * repository holds only the keys; a key not found gives "". */
static char texts[196608];                   /* the 360 code's texts and both games' tips (--pc-pak) */
static const char *text_key[1200], *text_value[1200];
static int texts_count, texts_loaded;

static void load_texts(void) {
    char path[520];
    DWORD got = 0, i, n = 0;
    HANDLE f;
    char *line;
    texts_loaded = 1;
    if (!out_dir[0]) return;
    for (i = 0; out_dir[i] && n < 480; i++) path[n++] = out_dir[i];
    memcpy(path + n, "\\x360_strings.txt", 18);
    f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (f == INVALID_HANDLE) { put("x360: no x360_strings.txt (convert with --image)\r\n"); return; }
    ReadFile(f, texts, sizeof texts - 1, &got, 0);
    CloseHandle(f);
    texts[got] = 0;
    for (line = texts; *line && texts_count < 1200;) {
        char *end = line, *eq = 0;
        while (*end && *end != '\n') { if (*end == '=' && !eq) eq = end; end++; }
        if (*end) *end++ = 0;
        if (end > line && end[-1] == '\r') end[-1] = 0;
        if (eq) { *eq = 0; text_key[texts_count] = line; text_value[texts_count++] = eq + 1; }
        line = end;
    }
}

const char *x360_text(const char *key) {
    int i;
    if (!texts_loaded) load_texts();
    for (i = 0; i < texts_count; i++) {
        const char *a = text_key[i], *b = key;
        while (*a && *a == *b) a++, b++;
        if (!*a && !*b) return text_value[i];
    }
    return "";
}

/* The key, past `prefix`, of the first text equal to `value` ("pctip." and PC's tip text give
 * its TIP_ID); 0 if none. */
const char *x360_text_key_of(const char *prefix, const char *value) {
    int i;
    if (!texts_loaded) load_texts();
    for (i = 0; i < texts_count; i++) {
        const char *k = text_key[i], *p = prefix, *a = text_value[i], *b = value;
        while (*p && *k == *p) k++, p++;
        if (*p) continue;
        while (*a && *a == *b) a++, b++;
        if (!*a && !*b) return k;
    }
    return 0;
}


static int starts(const char *s, const char *prefix) {
    while (*prefix) if (*s++ != *prefix++) return 0;
    return 1;
}

/* prompts=auto|xbox|playstation|playstation3|playstation4|playstation5|nintendo (proxy/setup.c);
 * bag=360 makes the hero's and pet's bag the 360's (x360_bag_patch, proxy/x360_menu.c). */
static char prompts_setting[24] = "auto";
static int bag_360;
/* save_root=DIR (a diagnosis for tests on copies, off by default): the folder the game takes as
 * the user's application data (SHGetSpecialFolderPathW with CSIDL_APPDATA, called by the game's
 * one wrapper 0x5cc6c0), so its "Runic Games\Torchlight" (saves, settings) is DIR's and the
 * user's own is not read or written. DIR: a Windows path in ASCII that exists. */
static char save_root[260];
void x360_prompt_folder(char *out_dir, unsigned size, const char *setting);
void x360_bag_patch(void);
void x360_load_hook_install(void);

void x360_test_options_notice(const char *names);
static void read_config(void) {
    static char text[4096];   /* static: over a page on the stack needs __chkstk, which is in the CRT */
    char path[520], *line, test_names[160];
    DWORD n = GetModuleFileNameA(self_module, path, 500), got = 0;
    HANDLE f;
    while (n && path[n - 1] != '\\' && path[n - 1] != '/') n--;
    memcpy(path + n, "x360_ui.ini", 12);
    f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (f == INVALID_HANDLE) { put("no x360_ui.ini: mode log\r\n"); return; }
    ReadFile(f, text, sizeof text - 1, &got, 0);
    CloseHandle(f);
    text[got] = 0;
    if (got == sizeof text - 1) put("x360_ui.ini over 4 KB: the rest is not read\r\n");
    test_names[0] = 0;
    for (line = text; *line;) {
        char *end = line;
        while (*end && *end != '\r' && *end != '\n') end++;
        if (*end) *end++ = 0;
        {
            /* The test options (runs on copies): named in a notice on screen (x360_menu.c). */
            static const char *const tests[] = {"save_root=", "hold_saves=", "pad_detect=", "console=", "test_", "pad_replay=", "pad_trace=", 0};
            int k;
            for (k = 0; tests[k]; k++)
                if (starts(line, tests[k])) {
                    const char *c = line;
                    unsigned m = length(test_names);
                    if (m && m + 2 < sizeof test_names) { test_names[m++] = ','; test_names[m++] = ' '; }
                    while (*c && *c != '=' && m + 1 < sizeof test_names) test_names[m++] = *c++;
                    test_names[m] = 0;
                    break;
                }
        }
        if (starts(line, "mode=scheme")) mode_scheme = 1;
        if (starts(line, "trace=1")) trace = 1;
        if (starts(line, "pad=1")) pad = 1;
        if (starts(line, "pad_edges=message")) pad_edges_msg = 1;
        if (starts(line, "pad_mode=direct")) pad_direct = 1;
        if (starts(line, "pad_face=0")) pad_old_face = 1;
        if (starts(line, "pad_log=1")) pad_log = 1;
        if (starts(line, "pad_trace=1")) { extern int x360_pad_trace; x360_pad_trace = 1; }
        if (starts(line, "pad_replay=")) {
            extern char x360_pad_replay[400];
            if (length(line + 11) < 400) memcpy(x360_pad_replay, line + 11, length(line + 11) + 1);
        }
        if (starts(line, "dialog_pc=1")) { extern int x360_dialog_pc; x360_dialog_pc = 1; }
        if (starts(line, "font_diff=1")) { extern int x360_font_diff; x360_font_diff = 1; }
        if (starts(line, "dump_targets=1")) { extern int x360_dump_targets; x360_dump_targets = 1; }
        if (starts(line, "dialog_markup=0")) { extern int x360_dialog_markup; x360_dialog_markup = 0; }
        if (starts(line, "dialog_font=") && length(line + 12) < 32) { extern char x360_dialog_font[32]; memcpy(x360_dialog_font, line + 12, length(line + 12) + 1); }
        if (starts(line, "dialog_fast_scroll=")) {
            extern int x360_dialog_fast_scroll;
            const char *v = line + 19;
            int n = 0;
            while (*v >= '0' && *v <= '9') n = n * 10 + (*v++ - '0');
            if (n > 0) x360_dialog_fast_scroll = n;
        }
        if (starts(line, "pad_file=") && length(line + 9) < sizeof pad_file) memcpy(pad_file, line + 9, length(line + 9) + 1);
        if (starts(line, "out=") && length(line + 4) < sizeof out_dir) memcpy(out_dir, line + 4, length(line + 4) + 1);
        if (starts(line, "prompts=") && length(line + 8) < sizeof prompts_setting) memcpy(prompts_setting, line + 8, length(line + 8) + 1);
        if (starts(line, "bag=360")) bag_360 = 1;
        if (starts(line, "pad_detect=") && length(line + 11) < 24) { extern char x360_pad_detect_test[24]; memcpy(x360_pad_detect_test, line + 11, length(line + 11) + 1); }
        if (starts(line, "hold_saves=1")) { extern int x360_save_held; x360_save_held = 1; }   /* a test: no save at all */
        if (starts(line, "test_save_hook_fails=1")) { extern int x360_test_save_hook_fails; x360_test_save_hook_fails = 1; }
        if (starts(line, "test_load_hook_fails=1")) { extern int x360_test_load_hook_fails; x360_test_load_hook_fails = 1; }
        if (starts(line, "test_no_stop=1")) { extern int x360_test_no_stop; x360_test_no_stop = 1; }
        if (starts(line, "test_stop_modal_fails=1")) { extern int x360_test_stop_modal_fails; x360_test_stop_modal_fails = 1; }
        if (starts(line, "save_root=") && length(line + 10) < sizeof save_root) memcpy(save_root, line + 10, length(line + 10) + 1);
        if (starts(line, "console=") && length(line + 8) < sizeof x360_console_commands)
            memcpy(x360_console_commands, line + 8, length(line + 8) + 1);
        line = end;
        while (*line == '\r' || *line == '\n') line++;
    }
    if (test_names[0]) { put("test options on: "); put(test_names); put("\r\n"); }
    if (test_names[0] && mode_scheme) x360_test_options_notice(test_names);
    put(mode_scheme ? "mode scheme, out \"" : "mode log, out \""); put(out_dir); put("\"");
    put(trace ? ", trace\r\n" : "\r\n");
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

/* For proxy/x360_menu.c: a member of the real DLL by its mangled name. */
FARPROC x360_need(const char *name) { return need(name); }

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

    /* The pad's prompts: the converted folder of the connected pad's style, if there is one
     * (here, not in DllMain: the HID enumeration loads setupapi.dll). */
    x360_prompt_folder(out_dir, sizeof out_dir, prompts_setting);
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

/* PC's stats menu's root (STATSMENU.LAYOUT, loaded at start): proxy/x360_menu.c reads the texts
 * PC writes there for the 360's character column. */
void *x360_pc_stats_root;

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
    if (r && file && ends_with_nocase(real_c_str(file, 0), "STATSMENU.LAYOUT")) x360_pc_stats_root = r;
    if (r && file && mode_scheme) { void x360_pc_layout_loaded(const char *file, void *root); x360_pc_layout_loaded(real_c_str(file, 0), r); }
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

/* ---- the trace (trace=1): what the game's code asks of its windows ---- */

typedef void *(__fastcall *get_child_fn)(const void *self, void *edx, const void *name);
typedef void *(__fastcall *get_property_fn)(const void *self, void *edx, void *ret, const void *name);
typedef void (__fastcall *set_text_fn)(void *self, void *edx, const void *text);
typedef const void *(__fastcall *get_name_fn)(const void *self, void *edx);

static get_child_fn real_get_child, real_child_search;
static get_property_fn real_get_property;
static set_text_fn real_set_text;
static get_name_fn real_get_name;
static unsigned long exe_lo, exe_hi;
static unsigned long seen[8192];

static void resolve_trace(void) {
    unsigned char *base;
    if (real_get_child) return;
    resolve();
    real_get_child = (get_child_fn)need("?getChild@Window@CEGUI@@QBEPAV12@ABVString@2@@Z");
    real_child_search = (get_child_fn)need("?recursiveChildSearch@Window@CEGUI@@QBEPAV12@ABVString@2@@Z");
    real_get_property = (get_property_fn)need("?getProperty@PropertySet@CEGUI@@QBE?AVString@2@ABV32@@Z");
    real_set_text = (set_text_fn)need("?setText@Window@CEGUI@@QAEXABVString@2@@Z");
    real_get_name = (get_name_fn)need("?getName@Window@CEGUI@@QBEABVString@2@XZ");
    base = (unsigned char *)GetModuleHandleA(0);
    exe_lo = (unsigned long)base;
    exe_hi = exe_lo + *(DWORD *)(base + *(DWORD *)(base + 0x3c) + 0x50);
}

static unsigned long fnv(unsigned long h, const char *s) {
    while (s && *s) h = (h ^ (unsigned char)*s++) * 16777619UL;
    return h;
}

/* True the first time this (kind, window, name, found) is seen. */
static int first(const char *kind, const char *window, const char *name, int found) {
    unsigned long h = fnv(fnv(fnv(2166136261UL + found, kind), window), name) | 1;
    unsigned i = h & 8191, n;
    for (n = 0; n < 8192; n++, i = (i + 1) & 8191) {
        if (seen[i] == h) return 0;
        if (!seen[i]) { seen[i] = h; return 1; }
    }
    return 0;
}

static int from_game(void *ret) {
    unsigned long r = (unsigned long)ret;
    return trace && r >= exe_lo && r < exe_hi;
}

static const char *name_of(const void *window) {
    return window && real_get_name ? real_c_str(real_get_name(window, 0), 0) : "(null)";
}

static void *child_lookup(const char *kind, get_child_fn real_fn, const void *self, const void *name, void *ret) {
    void *r = real_fn(self, 0, name);
    if (from_game(ret)) {
        const char *window = name_of(self), *wanted = real_c_str(name, 0);
        if (first(kind, window, wanted, r != 0)) {
            stamp(); put(kind); put(" in \""); put(window); put("\": \""); put(wanted);
            put(r ? "\" found\r\n" : "\" NOT FOUND\r\n");
        }
    }
    return r;
}

void *__fastcall hook_get_child(const void *self, void *edx, const void *name) {
    resolve_trace();
    return child_lookup("getChild", real_get_child, self, name, __builtin_return_address(0));
}

void *__fastcall hook_child_search(const void *self, void *edx, const void *name) {
    resolve_trace();
    return child_lookup("recursiveChildSearch", real_child_search, self, name, __builtin_return_address(0));
}

void *__fastcall hook_get_property(const void *self, void *edx, void *ret, const void *name) {
    void *r;
    resolve_trace();
    r = real_get_property(self, 0, ret, name);
    if (from_game(__builtin_return_address(0))) {
        /* PropertySet is Window's first base: the same address, for a window. */
        const char *window = name_of(self), *prop = real_c_str(name, 0);
        if (first("getProperty", window, prop, 1)) {
            stamp(); put("getProperty \""); put(prop); put("\" of \""); put(window); put("\" = \"");
            put(real_c_str(r, 0)); put("\"\r\n");
        }
    }
    return r;
}

void __fastcall hook_set_text(void *self, void *edx, const void *text) {
    resolve_trace();
    if (from_game(__builtin_return_address(0))) {
        const char *window = name_of(self);
        if (first("setText", window, "", 1)) {
            char clip[81];
            const char *t = real_c_str(text, 0);
            unsigned n = length(t);
            if (n > 80) n = 80;
            memcpy(clip, t, n);
            clip[n] = 0;
            stamp(); put("setText \""); put(window); put("\" = \""); put(clip); put("\"\r\n");
        }
    }
    real_set_text(self, 0, text);
}

typedef unsigned char (__fastcall *inject_fn)(void *self, void *edx, unsigned code);
static inject_fn real_key_down, real_key_up, real_char;

static unsigned char injected(const char *what, inject_fn *real, const char *name, void *self, unsigned code,
                              void *ret) {
    unsigned char r;
    if (!*real) { resolve_trace(); *real = (inject_fn)need(name); }
    r = (*real)(self, 0, code);
    if (from_game(ret)) {
        stamp(); put(what); put(" "); put_dec(code); put(r ? " consumed\r\n" : " not consumed\r\n");
    }
    return r;
}

unsigned char __fastcall hook_key_down(void *self, void *edx, unsigned code) {
    return injected("injectKeyDown", &real_key_down, "?injectKeyDown@System@CEGUI@@QAE_NI@Z", self, code,
                    __builtin_return_address(0));
}

unsigned char __fastcall hook_key_up(void *self, void *edx, unsigned code) {
    return injected("injectKeyUp", &real_key_up, "?injectKeyUp@System@CEGUI@@QAE_NI@Z", self, code,
                    __builtin_return_address(0));
}

unsigned char __fastcall hook_char(void *self, void *edx, unsigned code) {
    return injected("injectChar", &real_char, "?injectChar@System@CEGUI@@QAE_NI@Z", self, code,
                    __builtin_return_address(0));
}

void x360_pad_start(const char *file, int by_message, int direct, int face, int log);

/* For proxy/pad.c: a left button press or release at (x, y) in the window's pixels, injected into
 * CEGUI as the game injects real ones, so the game's own handlers (the click on the world) run. */
typedef void *(__cdecl *system_get_fn)(void);
typedef unsigned char (__fastcall *inject_xy_fn)(void *self, void *edx, float x, float y);
typedef unsigned char (__fastcall *inject_button_fn)(void *self, void *edx, int button);

void x360_inject_left(int down, float x, float y) {
    static system_get_fn get_system;
    static inject_xy_fn inject_position;
    static inject_button_fn inject_down, inject_up;
    void *system;
    if (!get_system) {
        resolve();
        get_system = (system_get_fn)need("?getSingletonPtr@?$Singleton@VSystem@CEGUI@@@CEGUI@@SAPAVSystem@2@XZ");
        inject_position = (inject_xy_fn)need("?injectMousePosition@System@CEGUI@@QAE_NMM@Z");
        inject_down = (inject_button_fn)need("?injectMouseButtonDown@System@CEGUI@@QAE_NW4MouseButton@2@@Z");
        inject_up = (inject_button_fn)need("?injectMouseButtonUp@System@CEGUI@@QAE_NW4MouseButton@2@@Z");
    }
    if (!get_system || !inject_position || !inject_down || !inject_up) return;
    system = get_system();
    if (!system) return;
    inject_position(system, 0, x, y);
    if (down < 0) return;   /* the position only: the hover of the window there */
    if (down) inject_down(system, 0, 0 /* LeftButton */);
    else inject_up(system, 0, 0);
}

/* The build of Torchlight.exe this proxy was compiled for (tools/build_proxy.py --target): the
 * addresses in the sources are GOG's build 17870; for Steam's build 17685 the build substitutes
 * them from proxy/addresses_steam.txt and defines X360_TARGET_STEAM. In any other executable, or
 * in this one loaded away from its preferred base (Steam's has relocations), every part that
 * touches the executable's code or data (the pad, the 360's screens, the bag patch) stays off:
 * the converted scheme and layouts load through CEGUI and Ogre by name only. */
#ifdef X360_TARGET_STEAM
#define X360_EXE_TIMESTAMP 0x4b5f7519UL
#define X360_TARGET "Steam's build 17685"
#else
#define X360_EXE_TIMESTAMP 0x4ffc6815UL
#define X360_TARGET "GOG's build 17870"
#endif
#define X360_EXE_BASE (4UL << 20)   /* the image base both builds are linked for */

/* 1 if the process's executable is the build this proxy was made for, at its preferred base. */
int x360_exe_is_target(void) {
    unsigned char *base = (unsigned char *)GetModuleHandleA(0);
    unsigned char *nt;
    if (!base || base[0] != 'M' || base[1] != 'Z') return 0;
    nt = base + *(DWORD *)(base + 0x3c);
    if (nt[0] != 'P' || nt[1] != 'E' || nt[2] || nt[3]) return 0;
    return *(DWORD *)(nt + 8) == X360_EXE_TIMESTAMP && (DWORD)base == X360_EXE_BASE;
}

static unsigned short save_root_w[260];
typedef BOOL (__stdcall *special_folder_fn)(void *window, unsigned short *path, int csidl, BOOL create);
static special_folder_fn real_special_folder;
static BOOL __stdcall special_folder_hook(void *window, unsigned short *path, int csidl, BOOL create) {
    if ((csidl & 0xff) == 0x1a) {                /* CSIDL_APPDATA */
        unsigned i;
        for (i = 0; save_root_w[i]; i++) path[i] = save_root_w[i];
        path[i] = 0;
        return 1;
    }
    return real_special_folder(window, path, csidl, create);
}

static int same_name(const char *a, const char *b) {   /* ASCII, case-insensitive */
    for (; *a && *b; a++, b++) {
        char x = *a >= 'a' && *a <= 'z' ? *a - 32 : *a, y = *b >= 'a' && *b <= 'z' ? *b - 32 : *b;
        if (x != y) return 0;
    }
    return *a == *b;
}

/* The executable's import of SHELL32!SHGetSpecialFolderPathW replaced (found in its import table,
 * by name: the same in every build). */
static void save_root_install(void) {
    unsigned char *base = (unsigned char *)GetModuleHandleA(0), *nt;
    DWORD *desc, i, old;
    if (!save_root[0]) return;
    for (i = 0; save_root[i]; i++) {
        if ((unsigned char)save_root[i] >= 0x80) { put("save_root: not ASCII, not used\r\n"); return; }
        save_root_w[i] = (unsigned char)save_root[i];
    }
    save_root_w[i] = 0;
    if (GetFileAttributesA(save_root) == 0xffffffff) { put("save_root: no such folder, not used\r\n"); return; }
    nt = base + *(DWORD *)(base + 0x3c);
    for (desc = (DWORD *)(base + *(DWORD *)(nt + 0x80)); desc[3]; desc += 5) {
        DWORD *names = (DWORD *)(base + (desc[0] ? desc[0] : desc[4])), *slots = (DWORD *)(base + desc[4]);
        if (!same_name((const char *)(base + desc[3]), "SHELL32.dll")) continue;
        for (i = 0; names[i]; i++) {
            if (names[i] & 0x80000000) continue;
            if (!same_name((const char *)(base + names[i] + 2), "SHGetSpecialFolderPathW")) continue;
            if (!VirtualProtect(&slots[i], 4, 0x04, &old)) return;
            real_special_folder = (special_folder_fn)slots[i];
            slots[i] = (DWORD)special_folder_hook;
            VirtualProtect(&slots[i], 4, old, &old);
            put("save_root: the game's application data folder is "); put(save_root); put("\r\n");
            return;
        }
    }
    put("save_root: SHGetSpecialFolderPathW not imported, not used\r\n");
}

void *x360_self_module(void) { return self_module; }

BOOL __stdcall DllMain(HMODULE module, DWORD reason, void *reserved) {
    if (reason == 1) {
        self_module = module;
        DisableThreadLibraryCalls(module);
        open_log();
        read_config();
        put("built for " X360_TARGET "\r\n");
        if (!x360_exe_is_target()) {
            unsigned char *base = (unsigned char *)GetModuleHandleA(0);
            put("Torchlight.exe is not " X360_TARGET " at its preferred base (its timestamp ");
            put_hex(base ? *(DWORD *)(base + *(DWORD *)(base + 0x3c) + 8) : 0);
            put(", base "); put_hex((unsigned long)base);
            put("): the pad, the 360's screens and the bag patch stay off\r\n");
            pad = 0;
            bag_360 = 0;
        }
        save_root_install();                        /* before the game asks for its folder */
        if (pad) x360_pad_start(pad_file, pad_edges_msg, pad_direct, !pad_old_face, pad_log);
        if (bag_360 && mode_scheme) x360_bag_patch();   /* before any unit is made */
        if (mode_scheme && x360_exe_is_target()) {
            void x360_stop_file_check(void);
            x360_load_hook_install();   /* either bag: a save of the other one placed again */
            x360_stop_file_check();     /* a stop's text the last game did not show */
        }
    }
    return 1;
}
