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
 * through the real DLL's String::c_str(). No CRT: kernel32 only. The log is x360_ui.log next to
 * this DLL. The export table is made by tools/build_proxy.py from the user's own CEGUIBase.dll.
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

#define REAL_DLL "CEGUIBase_orig.dll"
#define GENERIC_WRITE 0x40000000UL
#define FILE_SHARE_READ 1UL
#define CREATE_ALWAYS 2UL
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

/* ---- the thunks (exported under the members' mangled names by the .def) ---- */

void *__fastcall hook_load_scheme(void *self, void *edx, const void *file, const void *group) {
    void *r;
    resolve();
    stamp(); put_str("loadScheme ", file); put_str(" group ", group); put("\r\n");
    r = real_load_scheme(self, 0, file, group);
    stamp(); put("loadScheme -> "); put_hex((unsigned long)r); put("\r\n");
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
    }
    return 1;
}
