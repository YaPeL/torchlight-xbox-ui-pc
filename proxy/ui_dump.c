/* PC's CEGUI window tree written as JSON, in the format of the 360 catalog's dumps
 * ("torchlight-ui-dump 1", ~/torchlight-gog/screens/ref/catalog/README.md), so a screen of the
 * GOG game can be compared window by window with the 360's (tools/compare_ui.py).
 *
 * A dump is asked for with a file: when x360_dump.req exists next to this DLL, its first line is
 * the dump's name; the tree is written to x360_dump_NAME.json next to it and the request file is
 * deleted. It runs on the game's thread (x360_ui_dump_poll, from proxy/x360_menu.c's HUD tick).
 *
 * Everything is read through CEGUI's exported getters (checked in GOG's CEGUIBase.dll, build
 * 17870): Window::getUnclippedPixelRect and getPixelRect return a Rect whose four floats are, in
 * memory, top, bottom, left, right (Rect(left, top, right, bottom) at 0x1008bdf0 stores its second
 * argument at +0, the fourth at +4, the first at +8, the third at +0xc; getWidth at 0x10001aa0 is
 * [+0xc] - [+8]). isVisible(true) is the window's own flag, isVisible(false) with its parents'.
 */

typedef void *FARPROC;
typedef void *HANDLE;
typedef void *HWND;
typedef unsigned long DWORD;
typedef struct { long left, top, right, bottom; } RECT;

void x360_log(const char *s);
void x360_log_num(long v);
FARPROC x360_need(const char *name);
__declspec(dllimport) HANDLE __stdcall CreateFileA(const char *name, DWORD access, DWORD share, void *security,
                                                   DWORD disposition, DWORD flags, HANDLE templ);
__declspec(dllimport) int __stdcall WriteFile(HANDLE file, const void *data, DWORD size, DWORD *written, void *ov);
__declspec(dllimport) int __stdcall ReadFile(HANDLE file, void *data, DWORD size, DWORD *read, void *ov);
__declspec(dllimport) int __stdcall CloseHandle(HANDLE file);
__declspec(dllimport) int __stdcall DeleteFileA(const char *name);
__declspec(dllimport) DWORD __stdcall GetModuleFileNameA(void *module, char *out, DWORD size);
__declspec(dllimport) void *__stdcall GetModuleHandleA(const char *name);
__declspec(dllimport) DWORD __stdcall GetTickCount(void);
__declspec(dllimport) HWND __stdcall GetActiveWindow(void);
__declspec(dllimport) int __stdcall GetClientRect(HWND w, RECT *r);

#define INVALID ((HANDLE)(long)-1)

typedef struct { __declspec(align(8)) unsigned char bytes[256]; } object;
typedef struct { float top, bottom, left, right; } rect;

typedef void *(__fastcall *string_ctor_fn)(void *self, void *edx, const char *text);
typedef void (__fastcall *string_dtor_fn)(void *self, void *edx);
typedef const char *(__fastcall *c_str_fn)(const void *self, void *edx);
typedef void *(__cdecl *singleton_fn)(void);
typedef void *(__fastcall *get_sheet_fn)(const void *self, void *edx);
typedef const void *(__fastcall *get_string_fn)(const void *self, void *edx);
typedef unsigned char (__fastcall *get_flag_fn)(const void *self, void *edx, unsigned char arg);
typedef unsigned char (__fastcall *get_bool_fn)(const void *self, void *edx);
typedef float (__fastcall *get_float_fn)(const void *self, void *edx);
typedef rect *(__fastcall *get_rect_fn)(const void *self, void *edx, rect *out);
typedef void *(__fastcall *get_font_fn)(const void *self, void *edx, unsigned char use_default);
typedef unsigned (__fastcall *child_count_fn)(const void *self, void *edx);
typedef void *(__fastcall *child_at_fn)(const void *self, void *edx, unsigned i);
typedef void *(__fastcall *get_property_fn)(const void *self, void *edx, void *ret, const void *name);
typedef unsigned char (__fastcall *has_property_fn)(const void *self, void *edx, const void *name);

static string_ctor_fn string_ctor;
static string_dtor_fn string_dtor;
static c_str_fn c_str;
static singleton_fn system_ptr;
static get_sheet_fn get_gui_sheet;
static get_string_fn get_name, get_type, get_text;
static get_flag_fn is_visible;
static get_bool_fn is_active;
static get_float_fn get_alpha, get_effective_alpha;
static get_rect_fn unclipped_rect, clipped_rect;
static get_font_fn get_font;
static child_count_fn child_count;
static child_at_fn child_at;
static get_property_fn get_property;
static has_property_fn has_property;

/* Resolved in CEGUIBase_orig.dll; the build checks every name against the user's DLL. */
static FARPROC need(const char *name) { return x360_need(name); }

static int resolve(void) {
    static int done, ok;
    if (done) return ok;
    done = 1;
    string_ctor = (string_ctor_fn)need("??0String@CEGUI@@QAE@PBD@Z");
    string_dtor = (string_dtor_fn)need("??1String@CEGUI@@QAE@XZ");
    c_str = (c_str_fn)need("?c_str@String@CEGUI@@QBEPBDXZ");
    system_ptr = (singleton_fn)need("?getSingletonPtr@?$Singleton@VSystem@CEGUI@@@CEGUI@@SAPAVSystem@2@XZ");
    get_gui_sheet = (get_sheet_fn)need("?getGUISheet@System@CEGUI@@QBEPAVWindow@2@XZ");
    get_name = (get_string_fn)need("?getName@Window@CEGUI@@QBEABVString@2@XZ");
    get_type = (get_string_fn)need("?getType@Window@CEGUI@@QBEABVString@2@XZ");
    get_text = (get_string_fn)need("?getText@Window@CEGUI@@QBEABVString@2@XZ");
    is_visible = (get_flag_fn)need("?isVisible@Window@CEGUI@@QBE_N_N@Z");
    is_active = (get_bool_fn)need("?isActive@Window@CEGUI@@QBE_NXZ");
    get_alpha = (get_float_fn)need("?getAlpha@Window@CEGUI@@QBEMXZ");
    get_effective_alpha = (get_float_fn)need("?getEffectiveAlpha@Window@CEGUI@@QBEMXZ");
    unclipped_rect = (get_rect_fn)need("?getUnclippedPixelRect@Window@CEGUI@@QBE?AVRect@2@XZ");
    clipped_rect = (get_rect_fn)need("?getPixelRect@Window@CEGUI@@QBE?AVRect@2@XZ");
    get_font = (get_font_fn)need("?getFont@Window@CEGUI@@QBEPAVFont@2@_N@Z");
    child_count = (child_count_fn)need("?getChildCount@Window@CEGUI@@QBEIXZ");
    child_at = (child_at_fn)need("?getChildAtIdx@Window@CEGUI@@QBEPAV12@I@Z");
    get_property = (get_property_fn)need("?getProperty@PropertySet@CEGUI@@QBE?AVString@2@ABV32@@Z");
    has_property = (has_property_fn)need("?isPropertyPresent@PropertySet@CEGUI@@QBE_NABVString@2@@Z");
    ok = string_ctor && string_dtor && c_str && system_ptr && get_gui_sheet && get_name && get_type && get_text &&
         is_visible && is_active && get_alpha && get_effective_alpha && unclipped_rect && clipped_rect && get_font &&
         child_count && child_at && get_property && has_property;
    if (!ok) x360_log("x360 dump: a CEGUI function is missing\r\n");
    return ok;
}

/* ---- the output file, buffered ---- */

static HANDLE out = INVALID;
static char buf[32768];
static unsigned used;

static void flush(void) {
    DWORD done;
    if (out != INVALID && used) WriteFile(out, buf, used, &done, 0);
    used = 0;
}

static void put_char(char c) {
    if (used == sizeof buf) flush();
    buf[used++] = c;
}

static void put(const char *s) { while (s && *s) put_char(*s++); }

static void put_int(long v) {
    char d[12];
    int i = 0;
    if (v < 0) { put_char('-'); v = -v; }
    do { d[i++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (i) put_char(d[--i]);
}

/* A number with up to three decimals, as the 360's dumps write them. */
static void put_number(float v) {
    long whole, frac;
    int digits = 3;
    if (v != v) { put("null"); return; }
    if (v < 0) { put_char('-'); v = -v; }
    whole = (long)v;
    frac = (long)((v - (float)whole) * 1000.0f + 0.5f);
    if (frac >= 1000) { whole++; frac -= 1000; }
    put_int(whole);
    if (!frac) return;
    while (frac % 10 == 0) { frac /= 10; digits--; }
    put_char('.');
    if (digits >= 3 && frac < 100) put_char('0');
    if (digits >= 2 && frac < 10) put_char('0');
    put_int(frac);
}

/* A JSON string of UTF-8 text. */
static void put_string(const char *s) {
    put_char('"');
    for (; s && *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') { put_char('\\'); put_char((char)c); }
        else if (c == '\n') put("\\n");
        else if (c == '\r') put("\\r");
        else if (c == '\t') put("\\t");
        else if (c < 0x20) { put("\\u00"); put_char("0123456789abcdef"[c >> 4]); put_char("0123456789abcdef"[c & 15]); }
        else put_char((char)c);
    }
    put_char('"');
}

static void put_key(const char *key) { put_string(key); put_char(':'); }

/* ---- reading a window ---- */

static float scale_1080p;

/* The window's name without its layout's prefix: the proxy's own ("x360inv_", "x360hud_"...) or a
 * layout instance number ("43_", "103"), as the catalog's base_name. */
static const char *base_name(const char *name) {
    const char *p = name;
    if (p[0] == 'x' && p[1] == '3' && p[2] == '6' && p[3] == '0') {
        const char *q = p + 4;
        while (*q >= 'a' && *q <= 'z') q++;
        if (*q == '_' && q[1]) return q + 1;
    }
    while (*p >= '0' && *p <= '9') p++;
    if (p != name && *p == '_' && p[1]) return p + 1;
    if (p != name && *p) return p;
    return name;
}

/* The class's RTTI name (".?AVPushButton@CEGUI@@"), or "". */
static const char *class_name(void *object_) {
    const char *name = "";
    __try {
        void **vtable = *(void ***)object_;
        unsigned char *locator = (unsigned char *)vtable[-1];
        unsigned char *type = *(unsigned char **)(locator + 12);
        name = (const char *)(type + 8);
        if (name[0] != '.') name = "";
    } __except (1) {
        name = "";
    }
    return name;
}

static void put_property(void *w, const char *name, char *value, unsigned size) {
    object n, r;
    unsigned i;
    value[0] = 0;
    string_ctor(&n, 0, name);
    __try {
        if (has_property(w, 0, &n)) {
            const char *s;
            get_property(w, 0, &r, &n);
            s = c_str(&r, 0);
            for (i = 0; s && s[i] && i + 1 < size; i++) value[i] = s[i];
            value[i] = 0;
            string_dtor(&r, 0);
        } else {
            value[0] = 1;   /* absent */
            value[1] = 0;
        }
    } __except (1) {
        value[0] = 1;
        value[1] = 0;
    }
    string_dtor(&n, 0);
}

static void put_rect(const char *key, const rect *r, float k) {
    put_key(key);
    put_char('{');
    put_key("left"); put_number(r->left * k); put_char(',');
    put_key("top"); put_number(r->top * k); put_char(',');
    put_key("right"); put_number(r->right * k); put_char(',');
    put_key("bottom"); put_number(r->bottom * k); put_char(',');
    put_key("width"); put_number((r->right - r->left) * k); put_char(',');
    put_key("height"); put_number((r->bottom - r->top) * k);
    put_char('}');
}

static const char *const PROPERTIES[] = {
    "AlwaysOnTop", "ClippedByParent", "Disabled", "HorizontalAlignment", "HorzTextFormatting", "InheritsAlpha",
    "TextColour", "UnifiedAreaRect", "VertFormatting", "VerticalAlignment", "tabOrder"};
static const char *const IMAGES[] = {"Image", "NormalImage", "HoverImage", "PushedImage", "DisabledImage"};

/* "set:NAME image:IMAGE" into {"imageset": NAME, "image": IMAGE}. */
static void put_image(const char *value) {
    const char *set = 0, *image = 0, *p;
    unsigned n;
    char part[128];
    for (p = value; *p; p++) {
        if (p[0] == 's' && p[1] == 'e' && p[2] == 't' && p[3] == ':') set = p + 4;
        if (p[0] == 'i' && p[1] == 'm' && p[2] == 'a' && p[3] == 'g' && p[4] == 'e' && p[5] == ':') image = p + 6;
    }
    put_char('{');
    put_key("imageset");
    for (n = 0; set && set[n] && set[n] != ' ' && n + 1 < sizeof part; n++) part[n] = set[n];
    part[n] = 0;
    put_string(part);
    put_char(',');
    put_key("image");
    for (n = 0; image && image[n] && n + 1 < sizeof part; n++) part[n] = image[n];
    part[n] = 0;
    put_string(part);
    put_char('}');
}

static unsigned windows_written;

static void put_window(void *w, int depth) {
    char value[512];
    const char *name;
    rect r;
    unsigned i, n, first;
    void *font;
    windows_written++;
    put_char('{');
    name = c_str(get_name(w, 0), 0);
    put_key("name"); put_string(name); put_char(',');
    put_key("base_name"); put_string(base_name(name)); put_char(',');
    put_key("type"); put_string(c_str(get_type(w, 0), 0)); put_char(',');
    put_key("class"); put_string(class_name(w)); put_char(',');
    put_key("visible"); put(is_visible(w, 0, 1) ? "true" : "false"); put_char(',');
    put_key("shown"); put(is_visible(w, 0, 0) ? "true" : "false"); put_char(',');
    put_key("active"); put(is_active(w, 0) ? "true" : "false"); put_char(',');
    unclipped_rect(w, 0, &r);
    put_rect("rect", &r, 1); put_char(',');
    put_rect("rect_1080p", &r, scale_1080p); put_char(',');
    clipped_rect(w, 0, &r);
    put_rect("clipped", &r, 1); put_char(',');
    put_rect("clipped_1080p", &r, scale_1080p); put_char(',');
    put_key("alpha"); put_number(get_alpha(w, 0)); put_char(',');
    put_key("effective_alpha"); put_number(get_effective_alpha(w, 0)); put_char(',');
    /* The font in use (the default one when the window names none), by its Name property. */
    put_key("font");
    value[0] = 0;
    font = get_font(w, 0, 1);
    if (font) put_property(font, "Name", value, sizeof value);
    put_string(value[0] == 1 ? "" : value);
    put_char(',');
    put_key("text"); put_string(c_str(get_text(w, 0), 0)); put_char(',');
    put_key("images"); put_char('{');
    for (i = 0, first = 1; i < sizeof IMAGES / sizeof IMAGES[0]; i++) {
        put_property(w, IMAGES[i], value, sizeof value);
        if (value[0] == 1 || !value[0]) continue;
        if (!first) put_char(',');
        first = 0;
        put_key(IMAGES[i]);
        put_image(value);
    }
    put("},");
    put_key("properties"); put_char('{');
    for (i = 0, first = 1; i < sizeof PROPERTIES / sizeof PROPERTIES[0]; i++) {
        put_property(w, PROPERTIES[i], value, sizeof value);
        if (value[0] == 1) continue;
        if (!first) put_char(',');
        first = 0;
        put_key(PROPERTIES[i]);
        put_string(value);
    }
    put("},");
    put_key("children"); put_char('[');
    n = depth < 64 ? child_count(w, 0) : 0;
    for (i = 0; i < n; i++) {
        if (i) put_char(',');
        put_window(child_at(w, 0, i), depth + 1);
    }
    put("]}");
}

/* ---- the request ---- */

static void folder_path(char *path, unsigned size, const char *a, const char *b) {
    DWORD n = GetModuleFileNameA(GetModuleHandleA("CEGUIBase.dll"), path, size - 80);
    unsigned i;
    while (n && path[n - 1] != '\\' && path[n - 1] != '/') n--;
    for (i = 0; a && a[i] && n + 1 < size; i++) path[n++] = a[i];
    for (i = 0; b && b[i] && n + 1 < size; i++) path[n++] = b[i];
    path[n] = 0;
}

static void dump(const char *name) {
    char path[600];
    void *sys, *sheet;
    RECT client = {0, 0, 0, 0};
    float height;
    folder_path(path, sizeof path, "x360_dump_", name);
    {
        unsigned n = 0;
        while (path[n]) n++;
        if (n + 6 < sizeof path) { path[n] = '.'; path[n + 1] = 'j'; path[n + 2] = 's'; path[n + 3] = 'o'; path[n + 4] = 'n'; path[n + 5] = 0; }
    }
    sys = system_ptr();
    sheet = sys ? get_gui_sheet(sys, 0) : 0;
    if (!sheet) { x360_log("x360 dump: no GUI sheet\r\n"); return; }
    GetClientRect(GetActiveWindow(), &client);
    height = (float)(client.bottom - client.top);
    scale_1080p = height > 0 ? 1080.0f / height : 1;
    out = CreateFileA(path, 0x40000000UL, 1, 0, 2, 0, 0);
    if (out == INVALID) { x360_log("x360 dump: cannot write "); x360_log(path); x360_log("\r\n"); return; }
    used = 0;
    windows_written = 0;
    put("{\"format\":\"torchlight-ui-dump 1\",\"source\":\"gog\",");
    put_key("name"); put_string(name); put_char(',');
    put_key("frame"); put_int((long)GetTickCount()); put_char(',');
    put_key("display"); put("{\"width\":"); put_int(client.right - client.left);
    put(",\"height\":"); put_int(client.bottom - client.top); put("},");
    put_key("scale_to_1080p"); put_number(scale_1080p); put_char(',');
    put("\"menus\":[],");
    put_key("windows"); put_char('[');
    __try {
        put_window(sheet, 0);
    } __except (1) {
        x360_log("x360 dump: exception while reading the tree\r\n");
    }
    put("]}\n");
    flush();
    CloseHandle(out);
    out = INVALID;
    x360_log("x360 dump: wrote "); x360_log(path); x360_log(", windows "); x360_log_num((long)windows_written);
    x360_log("\r\n");
}

/* Called every HUD tick (about every 50 ms): looks for the request file at most every 250 ms. */
void x360_ui_dump_poll(void) {
    static unsigned last;
    char path[600], name[80];
    unsigned now = GetTickCount(), i;
    DWORD got = 0;
    HANDLE f;
    if (now - last < 250 && last) return;
    last = now;
    folder_path(path, sizeof path, "x360_dump.req", 0);
    f = CreateFileA(path, 0x80000000UL, 1, 0, 3, 0, 0);
    if (f == INVALID) return;
    ReadFile(f, name, sizeof name - 1, &got, 0);
    CloseHandle(f);
    DeleteFileA(path);
    name[got] = 0;
    /* The name: letters, digits, '-' and '_' only, up to the first other character. */
    for (i = 0; name[i] && ((name[i] >= 'a' && name[i] <= 'z') || (name[i] >= 'A' && name[i] <= 'Z') ||
                            (name[i] >= '0' && name[i] <= '9') || name[i] == '-' || name[i] == '_'); i++) {}
    name[i] = 0;
    if (!name[0]) { name[0] = 'u'; name[1] = 'i'; name[2] = 0; }
    if (!resolve()) return;
    dump(name);
}
