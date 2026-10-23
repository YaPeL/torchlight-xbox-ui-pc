/* The proxy's setup at load: the pad prompts' style by the connected pad.
 *
 * Prompts: the converter writes one folder per style (tools/install.py: OUT for Xbox, OUT-STYLE
 * for playstation3, playstation4, playstation5 and nintendo; tools/pad_prompts.py draws the icons).
 * With prompts=auto (the default) the connected pad's maker picks the folder: the HID devices'
 * paths name their USB vendor and product ("vid_054c&pid_09cc"), Sony's 054C (its model by the
 * product id) and Nintendo's 057E; anything else, or none, is Xbox. The recomp does the same through
 * SDL's controller types (its src/platform/platform_sdl.cpp). As there, the style is chosen at start
 * (the game loads its imagesets once).
 */

typedef unsigned long DWORD;
typedef int BOOL;
typedef void *HANDLE;
typedef void *HMODULE;
typedef void *FARPROC;

#define IMPORT __declspec(dllimport)
IMPORT HMODULE __stdcall LoadLibraryA(const char *name);
IMPORT FARPROC __stdcall GetProcAddress(HMODULE module, const char *name);
IMPORT HANDLE __stdcall CreateFileA(const char *name, DWORD access, DWORD share, void *security,
                                    DWORD disposition, DWORD flags, HANDLE templ);
IMPORT BOOL __stdcall WriteFile(HANDLE file, const void *data, DWORD size, DWORD *written, void *overlapped);
IMPORT BOOL __stdcall ReadFile(HANDLE file, void *data, DWORD size, DWORD *read, void *overlapped);
IMPORT BOOL __stdcall CloseHandle(HANDLE file);
IMPORT DWORD __stdcall GetFileAttributesA(const char *name);
IMPORT HANDLE __stdcall CreateThread(void *security, DWORD stack, DWORD (__stdcall *start)(void *), void *arg,
                                     DWORD flags, DWORD *id);

void x360_log(const char *s);

#define X360_VERSION "0.1.0"
#define UPDATE_HOST L"api.github.com"
#define UPDATE_PATH L"/repos/YaPeL/torchlight-xbox-ui-pc/releases/latest"
#define UPDATE_PAGE "https://github.com/YaPeL/torchlight-xbox-ui-pc/releases/latest"

static unsigned len(const char *s) { unsigned n = 0; while (s[n]) n++; return n; }
static char lower(char c) { return c >= 'A' && c <= 'Z' ? (char)(c + 32) : c; }
static int has(const char *hay, const char *needle) {
    unsigned i, j;
    for (i = 0; hay[i]; i++) {
        for (j = 0; needle[j] && lower(hay[i + j]) == needle[j]; j++) {}
        if (!needle[j]) return 1;
    }
    return 0;
}

/* ---- the pad's style ---- */

typedef struct { DWORD d1; unsigned short d2, d3; unsigned char d4[8]; } GUID_;
typedef struct { DWORD size; GUID_ cls; DWORD flags; unsigned long reserved; } IFACE_DATA;
typedef void *(__stdcall *get_class_devs_fn)(const GUID_ *cls, const char *enumerator, void *parent, DWORD flags);
typedef BOOL (__stdcall *enum_ifaces_fn)(void *set, void *info, const GUID_ *cls, DWORD index, IFACE_DATA *data);
typedef BOOL (__stdcall *iface_detail_fn)(void *set, IFACE_DATA *data, void *detail, DWORD size, DWORD *need, void *info);
typedef BOOL (__stdcall *destroy_list_fn)(void *set);

/* The style of a HID device's path, or 0. Sony's models by product id (SDL's database's): PS3
 * 0268; PS4 05c4, 09cc, 0ba0; PS5 0ce6, 0df2. */
static const char *style_of_path(const char *p) {
    if (has(p, "vid_054c")) {
        if (has(p, "pid_0268")) return "playstation3";
        if (has(p, "pid_05c4") || has(p, "pid_09cc") || has(p, "pid_0ba0")) return "playstation4";
        return "playstation5";
    }
    if (has(p, "vid_057e")) return "nintendo";
    return 0;
}

/* The connected pad's style: the first Sony or Nintendo HID device, else "xbox". */
const char *x360_pad_style(void) {
    static const GUID_ hid = {0x4D1E55B2, 0xF16F, 0x11CF, {0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30}};
    HMODULE m = LoadLibraryA("setupapi.dll");
    get_class_devs_fn get_devs = m ? (get_class_devs_fn)GetProcAddress(m, "SetupDiGetClassDevsA") : 0;
    enum_ifaces_fn enum_ifaces = m ? (enum_ifaces_fn)GetProcAddress(m, "SetupDiEnumDeviceInterfaces") : 0;
    iface_detail_fn detail_of = m ? (iface_detail_fn)GetProcAddress(m, "SetupDiGetDeviceInterfaceDetailA") : 0;
    destroy_list_fn destroy = m ? (destroy_list_fn)GetProcAddress(m, "SetupDiDestroyDeviceInfoList") : 0;
    const char *style = "xbox";
    void *set;
    DWORD i;
    if (!get_devs || !enum_ifaces || !detail_of || !destroy) return style;
    set = get_devs(&hid, 0, 0, 0x2 | 0x10);   /* DIGCF_PRESENT | DIGCF_DEVICEINTERFACE */
    if (!set || set == (void *)(long)-1) return style;
    for (i = 0; i < 256; i++) {
        IFACE_DATA data;
        struct { DWORD size; char path[512]; } detail;
        const char *s;
        data.size = sizeof data;
        if (!enum_ifaces(set, 0, &hid, i, &data)) break;
        detail.size = 5;   /* SP_DEVICE_INTERFACE_DETAIL_DATA_A's cbSize on x86 */
        if (!detail_of(set, &data, &detail, sizeof detail, 0, 0)) continue;
        detail.path[sizeof detail.path - 1] = 0;
        if ((s = style_of_path(detail.path)) != 0) { style = s; break; }
    }
    destroy(set);
    return style;
}

/* out_dir becomes out_dir-STYLE when that folder exists: setting is the ini's prompts= value
 * ("auto", "xbox", "playstation", "playstation3".."5", "nintendo"). */
void x360_prompt_folder(char *out_dir, unsigned size, const char *setting) {
    const char *style = setting && setting[0] && !has(setting, "auto") ? setting : x360_pad_style();
    char candidate[520];
    unsigned n = len(out_dir), k = len(style), i;
    if (setting && has(setting, "playstation") && len(setting) == 11) {
        const char *connected = x360_pad_style();
        style = has(connected, "playstation") ? connected : "playstation5";   /* the family: its model */
        k = len(style);
    }
    x360_log("x360 prompts: style "); x360_log(style); x360_log("\r\n");
    if (has(style, "xbox") || !n || n + 1 + k >= sizeof candidate || n + 1 + k >= size) return;
    for (i = 0; i < n; i++) candidate[i] = out_dir[i];
    candidate[n] = '-';
    for (i = 0; i <= k; i++) candidate[n + 1 + i] = style[i];
    if (GetFileAttributesA(candidate) == (DWORD)-1) {
        x360_log("x360 prompts: no converted folder for it, the Xbox prompts kept\r\n");
        return;
    }
    for (i = 0; candidate[i]; i++) out_dir[i] = candidate[i];
    out_dir[i] = 0;
}
