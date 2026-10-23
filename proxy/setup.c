/* The proxy's setup at load: the pad prompts' style by the connected pad, and the update check.
 *
 * Prompts: the converter writes one folder per style (tools/install.py: OUT for Xbox, OUT-STYLE
 * for playstation3, playstation4, playstation5 and nintendo; tools/pad_prompts.py draws the icons).
 * With prompts=auto (the default) the connected pad's maker picks the folder: the HID devices'
 * paths name their USB vendor and product ("vid_054c&pid_09cc"), Sony's 054C (its model by the
 * product id) and Nintendo's 057E; anything else, or none, is Xbox. The recomp does the same through
 * SDL's controller types (its src/platform/platform_sdl.cpp). As there, the style is chosen at start
 * (the game loads its imagesets once).
 *
 * Update check: a thread asks GitHub for the latest release of this mod (its tag) and writes it to
 * x360_update.txt next to the DLL; at the next start, if that tag is newer than this build's
 * version and the user has not declined it, a message box offers the download page. update_check=0
 * in x360_ui.ini turns it off. Nothing else is sent.
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

/* ---- the update check ---- */

static char state_path[520];

/* 1 when tag a (v1.2.3) is newer than b. */
static int newer(const char *a, const char *b) {
    int i;
    if (*a == 'v' || *a == 'V') a++;
    if (*b == 'v' || *b == 'V') b++;
    for (i = 0; i < 4; i++) {
        long x = 0, y = 0;
        while (*a >= '0' && *a <= '9') x = x * 10 + (*a++ - '0');
        while (*b >= '0' && *b <= '9') y = y * 10 + (*b++ - '0');
        if (x != y) return x > y;
        if (*a == '.') a++;
        if (*b == '.') b++;
    }
    return 0;
}

/* The value of KEY= in the state file's text, into out. */
static void state_value(const char *text, const char *key, char *out, unsigned size) {
    unsigned k = len(key), n = 0;
    const char *p = text;
    out[0] = 0;
    while (*p) {
        unsigned i;
        for (i = 0; i < k && p[i] == key[i]; i++) {}
        if (i == k && p[k] == '=') {
            p += k + 1;
            while (*p && *p != '\r' && *p != '\n' && n + 1 < size) out[n++] = *p++;
            out[n] = 0;
            return;
        }
        while (*p && *p != '\n') p++;
        if (*p) p++;
    }
}

static void state_read(char *text, unsigned size) {
    HANDLE f = CreateFileA(state_path, 0x80000000UL, 1, 0, 3, 0, 0);
    DWORD got = 0;
    text[0] = 0;
    if (f == (HANDLE)(long)-1) return;
    ReadFile(f, text, size - 1, &got, 0);
    CloseHandle(f);
    text[got] = 0;
}

static void state_write(const char *latest, const char *skip) {
    HANDLE f = CreateFileA(state_path, 0x40000000UL, 0, 0, 2, 0, 0);
    DWORD w;
    if (f == (HANDLE)(long)-1) return;
    WriteFile(f, "latest=", 7, &w, 0); WriteFile(f, latest, len(latest), &w, 0); WriteFile(f, "\r\n", 2, &w, 0);
    if (skip[0]) { WriteFile(f, "skip=", 5, &w, 0); WriteFile(f, skip, len(skip), &w, 0); WriteFile(f, "\r\n", 2, &w, 0); }
    CloseHandle(f);
}

typedef void *(__stdcall *http_open_fn)(const unsigned short *agent, DWORD access, const unsigned short *proxy,
                                        const unsigned short *bypass, DWORD flags);
typedef void *(__stdcall *http_connect_fn)(void *session, const unsigned short *host, unsigned short port, DWORD reserved);
typedef void *(__stdcall *http_request_fn)(void *connect, const unsigned short *verb, const unsigned short *path,
                                           const unsigned short *version, const unsigned short *referrer,
                                           const unsigned short **types, DWORD flags);
typedef BOOL (__stdcall *http_send_fn)(void *request, const unsigned short *headers, DWORD headers_len, void *opt,
                                       DWORD opt_len, DWORD total, unsigned long context);
typedef BOOL (__stdcall *http_receive_fn)(void *request, void *reserved);
typedef BOOL (__stdcall *http_read_fn)(void *request, void *buffer, DWORD size, DWORD *read);
typedef BOOL (__stdcall *http_close_fn)(void *handle);
typedef int (__stdcall *message_box_fn)(void *window, const char *text, const char *caption, unsigned type);
typedef void *(__stdcall *shell_execute_fn)(void *window, const char *verb, const char *file, const char *params,
                                            const char *dir, int show);

/* The latest release's tag from GitHub's API ("tag_name":"v0.2.0"), or "" on any failure. */
static void fetch_latest(char *tag, unsigned size) {
    HMODULE m = LoadLibraryA("winhttp.dll");
    http_open_fn open_ = m ? (http_open_fn)GetProcAddress(m, "WinHttpOpen") : 0;
    http_connect_fn connect_ = m ? (http_connect_fn)GetProcAddress(m, "WinHttpConnect") : 0;
    http_request_fn request_ = m ? (http_request_fn)GetProcAddress(m, "WinHttpOpenRequest") : 0;
    http_send_fn send_ = m ? (http_send_fn)GetProcAddress(m, "WinHttpSendRequest") : 0;
    http_receive_fn receive_ = m ? (http_receive_fn)GetProcAddress(m, "WinHttpReceiveResponse") : 0;
    http_read_fn read_ = m ? (http_read_fn)GetProcAddress(m, "WinHttpReadData") : 0;
    http_close_fn close_ = m ? (http_close_fn)GetProcAddress(m, "WinHttpCloseHandle") : 0;
    static const unsigned short agent[] = {'t','o','r','c','h','l','i','g','h','t','-','x','b','o','x','-','u','i',0};
    static const unsigned short headers[] = {'A','c','c','e','p','t',':',' ','a','p','p','l','i','c','a','t','i','o','n','/',
                                             'v','n','d','.','g','i','t','h','u','b','+','j','s','o','n',0};
    static char body[65536];
    unsigned n = 0, k = 0;
    void *s = 0, *c = 0, *r = 0;
    const char *p;
    tag[0] = 0;
    if (!open_ || !connect_ || !request_ || !send_ || !receive_ || !read_ || !close_) return;
    s = open_(agent, 0, 0, 0, 0);
    c = s ? connect_(s, UPDATE_HOST, 443, 0) : 0;
    r = c ? request_(c, L"GET", UPDATE_PATH, 0, 0, 0, 0x00800000 /* WINHTTP_FLAG_SECURE */) : 0;
    if (r && send_(r, headers, (DWORD)-1, 0, 0, 0, 0) && receive_(r, 0)) {
        DWORD got;
        while (n + 1 < sizeof body && read_(r, body + n, sizeof body - 1 - n, &got) && got) n += got;
    }
    body[n] = 0;
    if (r) close_(r);
    if (c) close_(c);
    if (s) close_(s);
    for (p = body; *p; p++) {
        const char *q = "\"tag_name\"";
        unsigned i;
        for (i = 0; q[i] && p[i] == q[i]; i++) {}
        if (q[i]) continue;
        p += i;
        while (*p == ' ' || *p == ':') p++;
        if (*p != '"') return;
        p++;
        while (*p && *p != '"' && k + 1 < size) tag[k++] = *p++;
        tag[k] = 0;
        return;
    }
}

static DWORD __stdcall update_thread(void *unused) {
    char text[512], latest[64], skip[64], fetched[64];
    (void)unused;
    state_read(text, sizeof text);
    state_value(text, "latest", latest, sizeof latest);
    state_value(text, "skip", skip, sizeof skip);
    /* Last time's answer first: offered once per new version. */
    if (latest[0] && newer(latest, X360_VERSION) && !(skip[0] && !newer(latest, skip))) {
        HMODULE u = LoadLibraryA("user32.dll"), sh = LoadLibraryA("shell32.dll");
        message_box_fn box = u ? (message_box_fn)GetProcAddress(u, "MessageBoxA") : 0;
        shell_execute_fn open_page = sh ? (shell_execute_fn)GetProcAddress(sh, "ShellExecuteA") : 0;
        char msg[256];
        static const char a[] = "Torchlight Xbox UI: version ", b[] = " is out (this is " X360_VERSION ").\n\nOpen its download page?";
        unsigned i = 0, j;
        for (j = 0; a[j] && i + 1 < sizeof msg; j++) msg[i++] = a[j];
        for (j = 0; latest[j] && i + 1 < sizeof msg; j++) msg[i++] = latest[j];
        for (j = 0; b[j] && i + 1 < sizeof msg; j++) msg[i++] = b[j];
        msg[i] = 0;
        if (box) {
            int answer = box(0, msg, "Torchlight Xbox UI", 0x4 | 0x40 | 0x40000 | 0x10000);   /* YESNO, INFORMATION, TOPMOST, SETFOREGROUND */
            if (answer == 6 && open_page) open_page(0, "open", UPDATE_PAGE, 0, 0, 1);
            for (j = 0; latest[j]; j++) skip[j] = latest[j];   /* asked once for this version */
            skip[j] = 0;
            state_write(latest, skip);
        }
    }
    fetch_latest(fetched, sizeof fetched);
    if (fetched[0]) {
        state_write(fetched, skip);
        x360_log("x360 update: the latest release is "); x360_log(fetched); x360_log(" (this is " X360_VERSION ")\r\n");
    } else x360_log("x360 update: no answer from GitHub\r\n");
    return 0;
}

/* dir: the DLL's folder, with its trailing separator. */
void x360_update_start(const char *dir) {
    unsigned i, n = len(dir);
    const char *name = "x360_update.txt";
    if (n + 16 >= sizeof state_path) return;
    for (i = 0; i < n; i++) state_path[i] = dir[i];
    for (i = 0; name[i]; i++) state_path[n + i] = name[i];
    state_path[n + i] = 0;
    CreateThread(0, 0, update_thread, 0, 0, 0);
}
