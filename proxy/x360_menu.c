/* The 360's screens hosted in PC's GUI: the inventory slice (docs section 11).
 *
 * The converted layout (tools/convert_x360_ui.py, media/ui/x360/) is loaded once with the real
 * WindowManager::loadWindowLayout, attached to PC's GUI sheet and shown or hidden; PC's own
 * inventory is not opened. Its windows are scaled from the 360's 1280x720 to the game's height
 * (CEGUI 0.6 keeps absolute offsets in pixels; the fonts already scale). Everything runs on the
 * game's thread (called from proxy/pad.c, inside GetCursorPos, where the pad already calls the
 * game's functions). CEGUI's members are __thiscall: called as __fastcall with a dummy edx, as in
 * cegui_proxy.c. CEGUI::String is built and destroyed by the real DLL's own members.
 *
 * The screen follows the 360's CInventoryMenuXenon (read in the 360 image, section 11):
 * - the list (sub_82369FE8): the owner's inventory, the equipped items first, then the rest,
 *   each in its category (sub_82369C48, below) when it equals the filter or is 6 (any);
 * - the filters (sub_8236A488, from RB/LB): 0 to 6 exclusive, kept per filter with its selection;
 * - five rows (ItemTray0-4); the selection is framed by `moveframe` (inferred from its place);
 * - A on the selected row (sub_8236AA48): use it, equip it or take it off, through PC's own
 *   functions (the right click on an inventory slot, GOG 0x55e156); B closes.
 * GOG addresses are those of build 17870 (the GOG copy); the unit type IDs are the data's
 * (media/UNITTYPES.HIE, the same 162 IDs in the 360's pak and PC's).
 */

typedef void *FARPROC;
typedef void *HWND;
typedef struct { long left, top, right, bottom; } RECT;

void x360_log(const char *s);
void x360_log_num(long v);
const char *x360_text(const char *key);   /* the 360 code's texts, from the user's copy (proxy/cegui_proxy.c) */
FARPROC x360_need(const char *name);
__declspec(dllimport) HWND __stdcall GetActiveWindow(void);
/* The game's window, also while another has the focus (GetActiveWindow is then 0: the 360's
 * fonts were left unset when the user played with the mouse in another window). */
static HWND active_window(void) {
    static HWND cached;
    HWND w = GetActiveWindow();
    if (w) cached = w;
    return cached;
}
__declspec(dllimport) int __stdcall GetClientRect(HWND w, RECT *r);
__declspec(dllimport) unsigned long __stdcall GetModuleFileNameA(void *module, char *out, unsigned long size);
__declspec(dllimport) void *__stdcall CreateFileA(const char *name, unsigned long access, unsigned long share, void *security,
                                                  unsigned long disposition, unsigned long flags, void *templ);
__declspec(dllimport) int __stdcall WriteFile(void *file, const void *data, unsigned long size, unsigned long *written, void *overlapped);
__declspec(dllimport) int __stdcall CloseHandle(void *file);
__declspec(dllimport) int __stdcall ClientToScreen(HWND w, long *point);
__declspec(dllimport) int __stdcall SetCursorPos(int x, int y);
__declspec(dllimport) void *__stdcall SetCursor(void *cursor);
__declspec(dllimport) unsigned long __stdcall GetTickCount(void);
__declspec(dllimport) short __stdcall GetAsyncKeyState(int key);
__declspec(dllimport) void *__stdcall GetModuleHandleA(const char *name);
__declspec(dllimport) void *__stdcall GetProcAddress(void *module, const char *name);
__declspec(dllimport) int __stdcall VirtualProtect(void *addr, unsigned long size, unsigned long prot, unsigned long *old);
/* kernel32's functions looked up by name (no import), and wide strings' small helpers. */
static void *kernel32_fn(const char *name) { void *k = GetModuleHandleA("kernel32.dll"); return k ? GetProcAddress(k, name) : 0; }
static unsigned wlength(const unsigned short *s) { unsigned n = 0; while (s[n]) n++; return n; }
static void wappend(unsigned short *d, unsigned size, const char *ascii) {
    unsigned n = wlength(d);
    while (*ascii && n + 1 < size) d[n++] = (unsigned char)*ascii++;
    d[n] = 0;
}

/* Resolved in CEGUIBase_orig.dll; the build checks every name against the user's DLL. */
static FARPROC need(const char *name) { return x360_need(name); }

/* ---- CEGUI ---- */

typedef struct { __declspec(align(8)) unsigned char bytes[256]; } object;

typedef void *(__fastcall *string_ctor_fn)(void *self, void *edx, const char *text);
typedef void (__fastcall *string_dtor_fn)(void *self, void *edx);
typedef const char *(__fastcall *c_str_fn)(const void *self, void *edx);
typedef void *(__cdecl *singleton_fn)(void);
typedef void *(__fastcall *load_layout5_fn)(void *self, void *edx, const void *file, const void *prefix,
                                            const void *group, void *callback, void *userdata);
typedef void *(__fastcall *get_sheet_fn)(const void *self, void *edx);
typedef void (__fastcall *add_child_fn)(void *self, void *edx, void *child);
typedef void (__fastcall *set_bool_fn)(void *self, void *edx, int on);
typedef void (__fastcall *void_fn)(void *self, void *edx);
typedef void *(__fastcall *get_window_fn)(const void *self, void *edx, const void *name);
typedef unsigned char (__fastcall *present_fn)(const void *self, void *edx, const void *name);
typedef void (__fastcall *set_text_fn)(void *self, void *edx, const void *text);
typedef void (__fastcall *set_property_fn)(void *self, void *edx, const void *name, const void *value);
typedef void *(__fastcall *get_property_fn)(const void *self, void *edx, void *ret, const void *name);
typedef unsigned (__fastcall *child_count_fn)(const void *self, void *edx);
typedef void *(__fastcall *child_at_fn)(const void *self, void *edx, unsigned i);

static string_ctor_fn string_ctor, string_utf8;
static string_dtor_fn string_dtor;
static c_str_fn c_str;
static singleton_fn window_manager, system_ptr;
static load_layout5_fn load_layout5;
static get_sheet_fn get_gui_sheet;
static add_child_fn add_child;
static set_bool_fn set_visible;
static void_fn move_to_front;
static get_window_fn get_window;
static present_fn is_present;
static set_text_fn set_text;
static set_property_fn set_property;
static get_property_fn get_property;
static child_count_fn child_count;
static child_at_fn child_at;

static int resolve(void) {
    static int done, ok;
    if (done) return ok;
    done = 1;
    string_ctor = (string_ctor_fn)need("??0String@CEGUI@@QAE@PBD@Z");
    string_utf8 = (string_ctor_fn)need("??0String@CEGUI@@QAE@PBE@Z");
    string_dtor = (string_dtor_fn)need("??1String@CEGUI@@QAE@XZ");
    c_str = (c_str_fn)need("?c_str@String@CEGUI@@QBEPBDXZ");
    window_manager = (singleton_fn)need("?getSingletonPtr@?$Singleton@VWindowManager@CEGUI@@@CEGUI@@SAPAVWindowManager@2@XZ");
    system_ptr = (singleton_fn)need("?getSingletonPtr@?$Singleton@VSystem@CEGUI@@@CEGUI@@SAPAVSystem@2@XZ");
    load_layout5 = (load_layout5_fn)need(
        "?loadWindowLayout@WindowManager@CEGUI@@QAEPAVWindow@2@ABVString@2@00P6A_NPAV32@AAV42@2PAX@Z3@Z");
    get_gui_sheet = (get_sheet_fn)need("?getGUISheet@System@CEGUI@@QBEPAVWindow@2@XZ");
    add_child = (add_child_fn)need("?addChildWindow@Window@CEGUI@@QAEXPAV12@@Z");
    set_visible = (set_bool_fn)need("?setVisible@Window@CEGUI@@QAEX_N@Z");
    move_to_front = (void_fn)need("?moveToFront@Window@CEGUI@@QAEXXZ");
    get_window = (get_window_fn)need("?getWindow@WindowManager@CEGUI@@QBEPAVWindow@2@ABVString@2@@Z");
    is_present = (present_fn)need("?isWindowPresent@WindowManager@CEGUI@@QBE_NABVString@2@@Z");
    set_text = (set_text_fn)need("?setText@Window@CEGUI@@QAEXABVString@2@@Z");
    set_property = (set_property_fn)need("?setProperty@PropertySet@CEGUI@@QAEXABVString@2@0@Z");
    get_property = (get_property_fn)need("?getProperty@PropertySet@CEGUI@@QBE?AVString@2@ABV32@@Z");
    child_count = (child_count_fn)need("?getChildCount@Window@CEGUI@@QBEIXZ");
    child_at = (child_at_fn)need("?getChildAtIdx@Window@CEGUI@@QBEPAV12@I@Z");
    ok = string_ctor && string_utf8 && string_dtor && c_str && window_manager && system_ptr && load_layout5 &&
         get_gui_sheet && add_child && set_visible && move_to_front && get_window && is_present && set_text &&
         set_property && get_property && child_count && child_at;
    if (!ok) x360_log("x360 menu: a CEGUI function is missing\r\n");
    return ok;
}

static unsigned length(const char *s) { unsigned n = 0; while (s && s[n]) n++; return n; }

static void copy(char *out, unsigned size, const char *a, const char *b) {
    unsigned n = 0;
    while (a && *a && n + 1 < size) out[n++] = *a++;
    while (b && *b && n + 1 < size) out[n++] = *b++;
    out[n] = 0;
}

#define PREFIX "x360inv_"

/* A window of a layout by its prefix and its name in the layout (0 if absent). */
static void *win_of(const char *prefix, const char *name) {
    char full[96];
    object s;
    void *w = 0, *wm = window_manager();
    copy(full, sizeof full, prefix, name);
    string_ctor(&s, 0, full);
    if (wm && is_present(wm, 0, &s)) w = get_window(wm, 0, &s);
    string_dtor(&s, 0);
    return w;
}

static const char *cur_prefix = PREFIX;     /* the bag being drawn or acted on (see panels) */
static void *win(const char *name) { return win_of(cur_prefix, name); }

#define MASTER "x360mm_"

static void prop(void *w, const char *name, const char *value) {
    object n, v;
    if (!w) return;
    string_ctor(&n, 0, name);
    string_utf8(&v, 0, value);
    __try { set_property(w, 0, &n, &v); } __except (1) {
        x360_log("x360 menu: setProperty threw: "); x360_log(name); x360_log(" = "); x360_log(value); x360_log("\r\n");
    }
    string_dtor(&v, 0);
    string_dtor(&n, 0);
}

/* The property's value into out (empty if it throws). */
static void read_prop(void *w, const char *name, char *out, unsigned size) {
    object n, r;
    out[0] = 0;
    if (!w) return;
    string_ctor(&n, 0, name);
    __try {
        get_property(w, 0, &r, &n);
        copy(out, size, c_str(&r, 0), 0);
        string_dtor(&r, 0);
    } __except (1) {
        out[0] = 0;
    }
    string_dtor(&n, 0);
}

static void text(void *w, const char *utf8) {
    object s;
    if (!w) return;
    string_utf8(&s, 0, utf8);
    set_text(w, 0, &s);
    string_dtor(&s, 0);
}

static void show(void *w, int on) { if (w) set_visible(w, 0, on); }
/* The HUD only shows: every window of it lets the mouse through to PC's (its SkillSet1 is a
 * 1920x1080 window, and over a dialog of PC's it took the click on Ok: "x360 click: under the
 * mouse x360hud_SkillSet1", docs 14). PC's own HUD under it, transparent, is let through too, from
 * the 360's HUD's tick only: the layer is chosen at the game's start (x360_ui.ini) and cannot be
 * turned off while it runs, so PC's HUD is never left visible and unclickable. */
static void pass_mouse(void *w) {
    void *stack[256];
    int sp = 0;
    if (!w) return;
    stack[sp++] = w;
    while (sp > 0) {
        void *v = stack[--sp];
        unsigned c, n = child_count(v, 0);
        prop(v, "MousePassThroughEnabled", "True");
        for (c = 0; c < n && sp < 255; c++) stack[sp++] = child_at(v, 0, c);
    }
}
static void show_children(void *w, int on) {
    unsigned i, n = w ? child_count(w, 0) : 0;
    for (i = 0; i < n; i++) show(child_at(w, 0, i), on);
}

/* ---- numbers, without a CRT ---- */

static const char *parse_float(const char *s, float *out) {
    float v = 0, scale = 0.1f;
    int neg = 0;
    while (*s == ' ') s++;
    if (*s == '-') { neg = 1; s++; }
    while (*s >= '0' && *s <= '9') v = v * 10 + (float)(*s++ - '0');
    if (*s == '.') {
        s++;
        while (*s >= '0' && *s <= '9') { v += scale * (float)(*s++ - '0'); scale *= 0.1f; }
    }
    *out = neg ? -v : v;
    return s;
}

static char *put_int(char *p, long v) {
    char buf[12];
    int i = 0;
    if (v < 0) { *p++ = '-'; v = -v; }
    do { buf[i++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (i) *p++ = buf[--i];
    return p;
}

/* A float with three decimals (CEGUI's UDim scales and offsets). */
static char *put_float(char *p, float v) {
    long whole, frac;
    if (v < 0) { *p++ = '-'; v = -v; }
    whole = (long)v;
    frac = (long)((v - (float)whole) * 1000.0f + 0.5f);
    if (frac >= 1000) { whole++; frac -= 1000; }
    p = put_int(p, whole);
    *p++ = '.';
    *p++ = (char)('0' + frac / 100);
    *p++ = (char)('0' + frac / 10 % 10);
    *p++ = (char)('0' + frac % 10);
    return p;
}

/* "{{s,o},{s,o},{s,o},{s,o}}": the eight numbers (0 if it does not parse). */
static int parse_rect(const char *s, float v[8]) {
    int i = 0;
    while (*s && i < 8) {
        if ((*s >= '0' && *s <= '9') || *s == '-' || *s == '.') s = parse_float(s, &v[i++]);
        else s++;
    }
    return i == 8;
}

static void write_rect(char *out, const float v[8]) {
    char *p = out;
    int i;
    *p++ = '{';
    for (i = 0; i < 4; i++) {
        *p++ = '{';
        p = put_float(p, v[2 * i]);
        *p++ = ',';
        p = put_float(p, v[2 * i + 1]);
        *p++ = '}';
        if (i < 3) *p++ = ',';
    }
    *p++ = '}';
    *p = 0;
}

/* The size of an image named by an Image property's value ("set:NAME image:IMAGE"), from CEGUI's
 * own imageset (Image::getWidth, getHeight); 0 if it is not defined. */
static int image_size(const char *value, float *w, float *h) {
    typedef void *(__fastcall *get_set_fn)(const void *mgr, void *edx, const void *name);
    typedef const void *(__fastcall *get_image_fn)(const void *set, void *edx, const void *name);
    typedef float (__fastcall *image_float_fn)(const void *image, void *edx);
    static singleton_fn manager;
    static get_set_fn get_set;
    static get_image_fn get_image;
    static image_float_fn width, height;
    char set[64], image[64];
    const char *p;
    unsigned n;
    object a, b;
    int ok = 0;
    if (!manager) {
        manager = (singleton_fn)need("?getSingletonPtr@?$Singleton@VImagesetManager@CEGUI@@@CEGUI@@SAPAVImagesetManager@2@XZ");
        get_set = (get_set_fn)need("?getImageset@ImagesetManager@CEGUI@@QBEPAVImageset@2@ABVString@2@@Z");
        get_image = (get_image_fn)need("?getImage@Imageset@CEGUI@@QBEABVImage@2@ABVString@2@@Z");
        width = (image_float_fn)need("?getWidth@Image@CEGUI@@QBEMXZ");
        height = (image_float_fn)need("?getHeight@Image@CEGUI@@QBEMXZ");
    }
    if (!manager || !get_set || !get_image || !width || !height || !manager()) return 0;
    set[0] = image[0] = 0;
    for (p = value; *p; p++) {
        if (p[0] == 's' && p[1] == 'e' && p[2] == 't' && p[3] == ':') {
            for (n = 0; p[4 + n] && p[4 + n] != ' ' && n + 1 < sizeof set; n++) set[n] = p[4 + n];
            set[n] = 0;
        }
        if (p[0] == 'i' && p[1] == 'm' && p[2] == 'a' && p[3] == 'g' && p[4] == 'e' && p[5] == ':') {
            for (n = 0; p[6 + n] && p[6 + n] != ' ' && n + 1 < sizeof image; n++) image[n] = p[6 + n];
            image[n] = 0;
        }
    }
    if (!set[0] || !image[0]) return 0;
    string_ctor(&a, 0, set);
    string_ctor(&b, 0, image);
    __try {
        const void *img = get_image(get_set(manager(), 0, &a), 0, &b);
        *w = width(img, 0);
        *h = height(img, 0);
        ok = *w > 0 && *h > 0;
    } __except (1) { ok = 0; }
    string_dtor(&b, 0);
    string_dtor(&a, 0);
    return ok;
}

/* A window sized w x h (pixels), its place left to its alignments. */
static void set_size(void *w, float left, float top, float width, float height) {
    float v[8] = {0, left, 0, top, 0, left + width, 0, top + height};
    char value[128];
    write_rect(value, v);
    prop(w, "UnifiedAreaRect", value);
}

/* Every window's pixel offsets times k (the layouts are drawn for 720 rows). */
static void scale_tree(void *w, float k, int depth) {
    char value[128];
    float v[8];
    unsigned i, n;
    read_prop(w, "UnifiedAreaRect", value, sizeof value);
    if (parse_rect(value, v)) {
        v[1] *= k; v[3] *= k; v[5] *= k; v[7] *= k;
        write_rect(value, v);
        prop(w, "UnifiedAreaRect", value);
    }
    if (depth > 12) return;
    n = child_count(w, 0);
    for (i = 0; i < n; i++) scale_tree(child_at(w, 0, i), k, depth + 1);
}

/* A window's rect, moved by (dx, dy) pixels (already scaled). */
static void move_by(void *w, float dx, float dy) {
    char value[128];
    float v[8];
    read_prop(w, "UnifiedAreaRect", value, sizeof value);
    if (!parse_rect(value, v)) return;
    v[1] += dx; v[5] += dx; v[3] += dy; v[7] += dy;
    write_rect(value, v);
    prop(w, "UnifiedAreaRect", value);
}

/* The width in pixels of text in window w's font (Font::getTextExtent), 0 if unknown. */
static float text_width(void *w, const char *text) {
    typedef void *(__fastcall *get_font_fn)(const void *self, void *edx, int use_default);
    typedef float (__fastcall *extent_fn)(const void *font, void *edx, const object *text, float x_scale);
    static get_font_fn get_font;
    static extent_fn extent;
    object t;
    void *font;
    float r = 0;
    if (!get_font) get_font = (get_font_fn)need("?getFont@Window@CEGUI@@QBEPAVFont@2@_N@Z");
    if (!extent) extent = (extent_fn)need("?getTextExtent@Font@CEGUI@@QAEMABVString@2@M@Z");
    if (!w || !get_font || !extent || !(font = get_font(w, 0, 1))) return 0;
    string_utf8(&t, 0, text);
    __try { r = extent(font, 0, &t, 1.0f); } __except (1) { r = 0; }
    string_dtor(&t, 0);
    return r;
}

typedef struct { float w, h; } size2;

/* ---- the 360's letter spacing ----
 * The 360 lays out and rasterises its fonts at 1280x720 and its frame is drawn 1.5 times larger at
 * 1080; PC rasterises at its own resolution, where FreeType's hinting rounds each advance
 * differently: PC's text came out 6-10% wider. Each 360 font (renamed X360_* by the converter, so
 * PC's own fonts are untouched) is laid out once at 1280x720 with CEGUI's own FreeType
 * (Font::notifyScreenResolution), its advances kept, then laid out again at the size it had (its
 * own scaling, read from the object: CEGUI 0.6.2's Font keeps d_horzScaling, d_vertScaling just
 * before d_nativeHorzRes 1024, d_nativeVertRes 768), so its glyphs keep their size and sharpness,
 * and every advance is replaced by the 360's times height / 720 (the frame's scale against the
 * 360's 720 rows). The size it is laid out again for is PC's own Serif's: a font the scheme
 * creates is laid out for another size than the game's (a run: X360_Serif larger than PC's
 * Serif, from the same file), and PC's fonts were the ones the 360's screenshots matched. */

/* A font's scalings and native resolution: CEGUI 0.6.2's Font keeps d_horzScaling, d_vertScaling,
 * d_nativeHorzRes, d_nativeVertRes in a row (after the PropertySet base and three CEGUI::String).
 * The game sets its own fonts' native width to 1365 (a run: PC's Serif 1.406, 1.406, 1365x768,
 * though its file says 1024): its text is not stretched across. Returns the four floats. */
static float *font_scaling(void *font) {
    float *f = (float *)font;
    int i;
    for (i = 2; f && i < 300; i++)
        if (f[i + 1] == 768.0f && f[i] >= 1000.0f && f[i] <= 2000.0f &&
            f[i - 2] > 0.1f && f[i - 2] < 10.0f && f[i - 1] > 0.1f && f[i - 1] < 10.0f)
            return f + i - 2;
    return 0;
}

typedef void (__fastcall *notify_fn)(void *font, void *edx, const size2 *size);
typedef void *(__fastcall *glyph_fn)(void *font, void *edx, unsigned cp);
typedef void *(__fastcall *get_font_fn)(const void *fm, void *edx, const void *name);

#define FIRST_CP 0x20
#define LAST_CP 0x24f
static const char *const x360_fonts[] = {"X360_Serif", "X360_Serif14", "X360_SerifBig", "X360_SerifHuge",
                                         "X360_SerifSmall", "X360_FrizQuadrata", "X360_FrizQuadrataBig"};
#define FONTS (sizeof x360_fonts / sizeof x360_fonts[0])
static float advance_720[FONTS][LAST_CP - FIRST_CP + 1];
static float patched_e[FONTS];               /* the advance of 'e' as patched: a re-layout resets it */

/* The colour markup: this build's CEGUI draws "|cAARRGGBB" .. "|u" as a colour for a font given
 * its triggers (Font::setColourTrigger and setColourUnTrigger, with the String by value; PC sets
 * them on its own fonts, GOG 0x55fcf0 and 0x55fd1d). The converted X360_ fonts were created
 * without them: their text showed the markup (a name in gold in the 360's dialogs). */
typedef struct { unsigned char b[0x98]; } string_value;     /* CEGUI 0.6's String, by value */
typedef void (__fastcall *trigger_fn)(void *font, void *edx, string_value s);
static void set_colour_triggers(void *font) {
    static trigger_fn on, off;
    object s;
    string_value v;
    unsigned i;
    if (!on) on = (trigger_fn)need("?setColourTrigger@Font@CEGUI@@QAEXVString@2@@Z");
    if (!off) off = (trigger_fn)need("?setColourUnTrigger@Font@CEGUI@@QAEXVString@2@@Z");
    if (!on || !off || !font) return;
    /* Two-character strings stay in the String's own buffer (no heap): the callee destroys its
     * copy; ours is copied out and not destroyed. */
    string_ctor(&s, 0, "|c");
    for (i = 0; i < sizeof v.b; i++) v.b[i] = s.bytes[i];
    on(font, 0, v);
    string_ctor(&s, 0, "|u");
    for (i = 0; i < sizeof v.b; i++) v.b[i] = s.bytes[i];
    off(font, 0, v);
    /* Then PC sets the font's byte +0x424 to 1 (GOG 0x55fd26, after the two triggers; one class
     * for PC's fonts and the X360_ ones: the vtable is the same, the font diff). The diff read
     * only +0x200..+0x400, so it did not see it: likely the switch that turns the markup on. */
    *((unsigned char *)font + 0x424) = 1;
}

/* A diagnosis of the colour markup (docs 15, item 1): PC's SerifBig and the converted
 * X360_SerifBig, their first 0x300 bytes as dwords, where they differ (x360_ui.ini font_diff=1). */
int x360_font_diff;
static void font_diff(void) {
    singleton_fn fm_get = (singleton_fn)need("?getSingletonPtr@?$Singleton@VFontManager@CEGUI@@@CEGUI@@SAPAVFontManager@2@XZ");
    get_font_fn get_font = (get_font_fn)need("?getFont@FontManager@CEGUI@@QBEPAVFont@2@ABVString@2@@Z");
    void *fm = fm_get ? fm_get() : 0, *a = 0, *b = 0;
    object n;
    unsigned i;
    static int done;
    if (done || !fm || !get_font) return;
    done = 1;
    string_ctor(&n, 0, "SerifBig"); __try { a = get_font(fm, 0, &n); } __except (1) { a = 0; } string_dtor(&n, 0);
    string_ctor(&n, 0, "X360_SerifBig"); __try { b = get_font(fm, 0, &n); } __except (1) { b = 0; } string_dtor(&n, 0);
    if (!a || !b) { x360_log("x360 font diff: a font missing\r\n"); return; }
    x360_log("x360 font diff: PC's vtable "); x360_log_num(*(long *)a); x360_log(", X360_'s "); x360_log_num(*(long *)b); x360_log("\r\n");
    for (i = 0x200 / 4; i < 0x440 / 4; i++) {
        unsigned va = ((unsigned *)a)[i], vb = ((unsigned *)b)[i];
        if (va != vb) {
            x360_log("x360 font diff: +"); x360_log_num((long)(i * 4)); x360_log(" PC "); x360_log_num((long)va);
            x360_log(" X360 "); x360_log_num((long)vb); x360_log("\r\n");
        }
    }
}

static void match_spacing(float width, float height) {
    singleton_fn fm_get = (singleton_fn)need("?getSingletonPtr@?$Singleton@VFontManager@CEGUI@@@CEGUI@@SAPAVFontManager@2@XZ");
    get_font_fn get_font = (get_font_fn)need("?getFont@FontManager@CEGUI@@QBEPAVFont@2@ABVString@2@@Z");
    notify_fn notify = (notify_fn)need("?notifyScreenResolution@Font@CEGUI@@UAEXABVSize@2@@Z");
    glyph_fn glyph = (glyph_fn)need("?getGlyphData@Font@CEGUI@@IAEPBVFontGlyph@2@I@Z");
    void *fm = fm_get ? fm_get() : 0;
    unsigned f, cp;
    size2 pc;
    float *pc_scaling;
    if (!fm || !get_font || !notify || !glyph || height <= 0) return;
    {
        object name;
        void *serif = 0;
        string_ctor(&name, 0, "Serif");
        __try { serif = get_font(fm, 0, &name); } __except (1) { serif = 0; }
        string_dtor(&name, 0);
        pc_scaling = serif ? font_scaling(serif) : 0;
        if (pc_scaling) { pc.w = pc_scaling[0] * pc_scaling[2]; pc.h = pc_scaling[1] * pc_scaling[3]; }
        if (!pc_scaling) {
            /* A diagnosis: the floats of the object that look like scalings or resolutions. */
            unsigned i;
            x360_log("x360 menu: PC's Serif's scaling not found; object ");
            x360_log_num((long)serif);
            x360_log(", floats:");
            for (i = 0; serif && i < 400; i++) {
                float v = ((float *)serif)[i];
                if ((v > 0.2f && v < 8.0f) || (v > 300.0f && v < 5000.0f && v == (float)(long)v)) {
                    x360_log(" ["); x360_log_num((long)i); x360_log("]="); x360_log_num((long)(v * 1000)); x360_log("e-3");
                }
            }
            x360_log("\r\n");
            return;
        }
    }
    for (f = 0; f < FONTS; f++) {
        object name;
        void *font = 0;
        float *g;
        string_ctor(&name, 0, x360_fonts[f]);
        __try { font = get_font(fm, 0, &name); } __except (1) { font = 0; }
        string_dtor(&name, 0);
        if (!font) continue;
        {
            static void *triggered[FONTS];
            if (triggered[f] != font) { set_colour_triggers(font); triggered[f] = font; }
        }
        g = (float *)glyph(font, 0, 'e');
        if (patched_e[f] && g && g[1] == patched_e[f]) continue;          /* still as patched */
        {
            float *own = font_scaling(font);
            if (!own) { x360_log("x360 menu: a 360 font's scaling not found\r\n"); continue; }
        }
        __try {
            size2 s720 = {1280.0f, 720.0f}, real = pc;
            float *own = font_scaling(font);
            /* The game's native width for its fonts (1365): the 360's text is not stretched either. */
            own[2] = pc_scaling[2];
            own[3] = pc_scaling[3];
            if (!patched_e[f]) {
                notify(font, 0, &s720);
                for (cp = FIRST_CP; cp <= LAST_CP; cp++) {
                    g = (float *)glyph(font, 0, cp);     /* FontGlyph: the image, then the advance */
                    advance_720[f][cp - FIRST_CP] = g ? g[1] : 0;
                }
            }
            notify(font, 0, &real);
            for (cp = FIRST_CP; cp <= LAST_CP; cp++) {
                g = (float *)glyph(font, 0, cp);
                if (g && advance_720[f][cp - FIRST_CP] > 0) g[1] = advance_720[f][cp - FIRST_CP] * height / 720.0f;
            }
            g = (float *)glyph(font, 0, 'e');
            patched_e[f] = g ? g[1] : 0;
            x360_log("x360 menu: spacing of "); x360_log(x360_fonts[f]);
            x360_log(", 'e' x100 "); x360_log_num((long)(advance_720[f]['e' - FIRST_CP] * 100));
            x360_log(" at 720, x100 "); x360_log_num((long)(patched_e[f] * 100));
            x360_log(" now; laid out for "); x360_log_num((long)real.w); x360_log("x"); x360_log_num((long)real.h);
            x360_log(", native "); x360_log_num((long)own[2]); x360_log("x"); x360_log_num((long)own[3]);
            x360_log("\r\n");
        } __except (1) {
            x360_log("x360 menu: the spacing threw\r\n");
        }
    }
    if (x360_font_diff) font_diff();   /* after the triggers and the layout */
}

/* ---- loading ---- */

/* Every window under w drawn whole, not clipped to its parent (the character column: its windows
 * clipped by their parents were not drawn at all, those the layout marks ClippedByParent False
 * were). */
static void unclip_tree(void *w, int depth) {
    unsigned i, n;
    prop(w, "ClippedByParent", "False");
    if (depth > 12) return;
    n = child_count(w, 0);
    for (i = 0; i < n; i++) unclip_tree(child_at(w, 0, i), depth + 1);
}

/* PC's own interface (the GUI sheet's other children: the HUD, the portrait, the skill bar) is
 * hidden while the 360's screen is open, and what was visible is shown again on closing. The 360
 * keeps its own HUD (the orbs, the potions, the minimap) around its frame; PC's is another. */
typedef unsigned char (__fastcall *is_visible_fn)(const void *self, void *edx, int local_only);
#define MAX_HIDDEN 64
static void *hidden_pc[MAX_HIDDEN];
static int hidden_count;
static int is_ours(void *w);
static void *pc_tip_layout_root;   /* PC's tip window's root (x360_pc_layout_loaded, below) */

static unsigned pc_ui_back_at;   /* PC's sheets shown again then (a shop's close: hide_pc_ui's caller, pc_menus_guard) */
static void hide_pc_ui(int hide) {
    static is_visible_fn is_visible;
    static int logged;
    void *sys = system_ptr(), *sheet = sys ? get_gui_sheet(sys, 0) : 0;
    unsigned i, n;
    if (!is_visible) is_visible = (is_visible_fn)need("?isVisible@Window@CEGUI@@QBE_N_N@Z");
    if (!hide) {
        while (hidden_count > 0) set_visible(hidden_pc[--hidden_count], 0, 1);
        return;
    }
    if (!sheet || !is_visible) return;
    n = child_count(sheet, 0);
    for (i = 0; i < n; i++) {
        void *w = child_at(sheet, 0, i);
        int q, known = 0;
        if (is_ours(w) || !is_visible(w, 0, 1)) continue;
        /* Hidden again when PC shows it again (a shop's menus), kept once in the list: run B3's
         * side panels showed once the list, then filled with the same windows, stopped hiding. */
        for (q = 0; q < hidden_count; q++) if (hidden_pc[q] == w) { known = 1; break; }
        if (known) { set_visible(w, 0, 0); continue; }
        if (hidden_count >= MAX_HIDDEN) { set_visible(w, 0, 0); continue; }
        if (!logged) {
            typedef const void *(__fastcall *name_fn)(const void *self, void *edx);
            static name_fn gn;
            if (!gn) gn = (name_fn)need("?getName@Window@CEGUI@@QBEABVString@2@XZ");
            x360_log("x360 menu: hiding PC's "); x360_log(c_str(gn(w, 0), 0)); x360_log("\r\n");
        }
        set_visible(w, 0, 0);
        hidden_pc[hidden_count++] = w;
    }
    logged = 1;
}

static float k;                              /* the layouts' scale: the game's height / 768 */
static float moveframe_top;                  /* moveframe's top in the layout, scaled */

static void *load(const char *file, const char *prefix) {
    object f, p, g;
    void *root = 0, *wm, *sys, *sheet;
    if (!resolve()) return 0;
    wm = window_manager();
    sys = system_ptr();
    sheet = sys ? get_gui_sheet(sys, 0) : 0;
    if (!wm || !sheet) { x360_log("x360 menu: no window manager or GUI sheet yet\r\n"); return 0; }
    string_ctor(&f, 0, file);
    string_ctor(&p, 0, prefix);
    string_ctor(&g, 0, "");
    __try {
        root = load_layout5(wm, 0, &f, &p, &g, 0, 0);
        if (root) add_child(sheet, 0, root);
    } __except (1) {
        root = 0;
        x360_log("x360 menu: loadWindowLayout threw (see CEGUI.log)\r\n");
    }
    string_dtor(&g, 0);
    string_dtor(&p, 0);
    string_dtor(&f, 0);
    x360_log("x360 menu: loaded ");
    x360_log(file);
    x360_log(root ? "\r\n" : ": failed\r\n");
    return root;
}

/* ---- the game's model (GOG build 17870) ---- */

#define GOG_APP_GLOBAL 0x00c1ad64
#define GOG_ISA 0x00480760               /* CBaseUnit::ISA(int unit type) */
#define GOG_INVENTORY 0x404              /* CCharacter's CInventory* (OT +0x490) */
#define INV_REFS 0x1c                    /* CInventory: CEquipmentRef* array, count +0x20, capacity +0x24 */
#define REF_ITEM 0x8                     /* CEquipmentRef: the equipment, its slot at +0xc */
#define REF_SLOT 0xc
#define EQUIPPED_SLOTS 12                /* slots 0-11 are the equipped locations (OT isEquipmentEquipped) */
#define GOG_ICON_WINDOW 0x24c            /* CEquipment: its icon window (createIcon, GOG 0x4b9b70) */
#define GOG_CREATE_ICON 0x004b9b70       /* CEquipment::createIcon(CGameUI*, bool) */
#define GOG_IS_USEABLE 0x0053ea10        /* CItem::isUseable() */
#define GOG_USE_ITEM 0x005501b0          /* CGameUI::useItem(CLevel*, CEquipment*) */
#define GOG_EQUIP_FIRST_FREE 0x004e9fe0  /* CInventory::equipEquipmentIntoFirstFreeLocation(CEquipment*) */
#define GOG_CAN_EQUIP_SLOT (0x164 / 4)   /* CEquipment's virtual (CCharacter*): the right click's check */
typedef void *(__fastcall *known_spell_fn)(void *unit, void *edx, int i);   /* getKnownSpell, GOG 0x484ff0 */
#define GOG_COMPARISON_ITEMS 0x004e7b90  /* CInventory::getComparisonItems(CEquipment*, CEquipment**, CEquipment**) */
#define GOG_REMOVE_EQUIPMENT 0x004e9ca0  /* CInventory::removeEquipment(CEquipment*) */
#define GOG_PICKUP_EQUIPMENT 0x004e9a30  /* CInventory::pickupEquipment(CEquipment*, bool) */
#define GOG_RENDER_BEHIND 0x004885b0     /* CCharacter::setRenderBehind(bool) */
#define GOG_DROP_SOUND 0x004d2780        /* CEquipment::playDropSound(SceneNode*) */
#define GOG_ITEM_NAME 0x1b0              /* CEquipment: its name, a std::wstring (found by the probe) */
#define GOG_GOLD 0x3c4                   /* CCharacter: gold (giveGold, GOG 0x487f20), on the last owner */
#define GOG_OWNER 0x5b4                  /* CCharacter: the owner giveGold walks up to */
#define GOG_IDENTIFIED 0x2fc            /* CEquipment: identified (OT +0x348: getItemName, sellPrice) */
#define GOG_STACK 0x1e4                  /* CEquipment: the stack's count (OT +0x238, getStackSizeOfEquipment) */
/* An item's two prices: what a merchant asks for it (CEquipment::sellPrice *(OT)*, GOG 0x4b3a90:
 * PC's purchase checks the gold against it, 0x54f01f) and what a merchant pays for it
 * (CEquipment::buyPrice *(OT)*, 0x4b3b70: PC's sale gives it, 0x54ef2f). */
#define GOG_ASKED_PRICE 0x004b3a90
#define GOG_PAID_PRICE 0x004b3b70
#define GOG_DPS 0x004b8f10               /* CEquipment::DPS(), an int (fistp into eax) */
/* CEquipment's elemental damages and sockets, as getDamageBonus (OT) reads them (GOG 0x4b84d0):
 * the damage types (a std::vector's begin and end), their bonuses (begin), the socketed items (a
 * TArrayList: data, size). */
#define GOG_DAMAGE_TYPES 0x30c
#define GOG_DAMAGE_BONUSES 0x324
#define GOG_SOCKETED 0x3a0
#define GOG_EFFECT_MANAGER 0x17c         /* CEquipment: its CEffectManager (hasEffects, GOG 0x4b3c20) */
#define GOG_ARMOR 0x2ec                  /* CEquipment: its armour, read by PC's tooltip for an ARMOR (13), GOG 0x543d2f */
#define INV_SLOTS 0x18                   /* CInventory: its number of slots */
#define INV_PANE_STARTS 0x60             /* CInventory: the panes' first slots (a vector of int) */

typedef int (__fastcall *int_get_fn)(void *self, void *edx);
typedef float (__fastcall *float_get_fn)(void *self, void *edx);
typedef unsigned char (__fastcall *isa_fn)(void *unit, void *edx, int type);
typedef unsigned char (__fastcall *item_bool_fn)(void *item, void *edx);
typedef unsigned char (__fastcall *inv_item_fn)(void *inv, void *edx, void *item);
typedef void *(__fastcall *pickup_fn)(void *inv, void *edx, void *item, int flag);
typedef void (__fastcall *comparison_fn)(void *inv, void *edx, void *item, void **a, void **b);
typedef void (__fastcall *use_item_fn)(void *ui, void *edx, void *level, void *item);
/* CEquipment's virtual +0x164 (GOG 0x4b8660, ret 8): (CCharacter*, bool); the 360's +0x15C is
 * called with (hero, 1). One argument short unbalanced the stack (a crash in a run). */
typedef unsigned char (__fastcall *can_equip_fn)(void *item, void *edx, void *character, int flag);
typedef void (__fastcall *int_fn)(void *self, void *edx, int v);
typedef void (__fastcall *ptr_fn)(void *self, void *edx, void *p);
typedef void (__fastcall *create_icon_fn)(void *item, void *edx, void *ui, int flag);
/* A unit's voice (on its emitter, the character's +0x230) and a sound at a node (CGameUI's +0x13f8,
 * GOG 0x5366f0, ret 0x14), as PC plays them (the pet's command, GOG 0x54de5c; the shop, 0x54ef28). */
#define GOG_PLAY_SOUND 0x005368d0
#define GOG_UNIT_EMITTER 0x230
#define GOG_UI_SOUNDS 0x13f8
#define GOG_REQUEST_TIP 0x0055f570     /* CGameUI: a tip by its index, if unseen and tips are on; thiscall, ret 4 */
typedef void (__fastcall *request_tip_fn)(void *ui, void *edx, int index);
#define GOG_PLAY_SOUND_AT 0x005366f0
#define GOG_UNIT_NODE 0x58
typedef void (__fastcall *play_sound_fn)(void *emitter, void *edx, int id, float a, float b);
typedef void (__fastcall *play_sound_at_fn)(void *sounds, void *edx, int id, void *node, float a, float b, int flag);

static unsigned char *hero, *level, *ui, *the_client;

static int find_world(void) {
    unsigned char *app = *(unsigned char **)GOG_APP_GLOBAL;
    unsigned char *client = app ? *(unsigned char **)(app + 0x64) : 0;
    hero = client ? *(unsigned char **)(client + 0x2c) : 0;
    level = client ? *(unsigned char **)(client + 0x38) : 0;
    /* CGameUI: the client's +0x3c (its hover code, GOG 0x41b56b, calls CGameUI's methods on it);
     * its hero is at +0x1c (useItem reads it there). Checked before use. */
    the_client = client;
    ui = client ? *(unsigned char **)(client + 0x3c) : 0;
    if (ui && *(unsigned char **)(ui + 0x1c) != hero) ui = 0;
    return hero && level;
}

static int isa(void *unit, int type) { return ((isa_fn)GOG_ISA)(unit, 0, type) != 0; }
static int usable_now(unsigned char *item, unsigned char *user);
static void *unit_property(unsigned char *item, const unsigned short *key_text);
static int unit_name(unsigned char *item, char *out, unsigned size);

/* The 360's category of an item (sub_82369C48): 0 weapons, 1 armour, 2 jewellery, 3 potions and
 * scrolls, 4 gems, 5 fish, 6 any. Unit types: WEAPON 8, ARMOR 13, RING 17, NECKLACE 24, POTION 33,
 * SCROLL 42, QUESTITEM 103, SOCKETABLE 120, FISH 124, SPELL 129, MAP 172. */
static int category(void *item) {
    if (isa(item, 33) || isa(item, 129) || isa(item, 42) || isa(item, 172) || isa(item, 103)) return 3;
    if (isa(item, 8)) return 0;
    if (isa(item, 13)) return 1;
    if (isa(item, 17) || isa(item, 24)) return 2;
    if (isa(item, 120)) return 4;
    return isa(item, 124) ? 5 : 6;
}

static unsigned char *cur_owner;             /* the bag's owner: the hero, the pet, a merchant or a stash */
static int cur_select;                       /* the list is the gem's item select (the 360's mode 2) */
static int sock_open;                        /* the gem's item select is open */
/* A unit's CInventory*; the shared stash's (its chest of type 170) is the game's, kept apart from
 * the characters (PC's stash menu, GOG 0x5aac2d: 0x52dfe0's +0x8). */
#define GOG_SHARED_STASH 0x00e40900
/* The transmuter (CCombineMenu, CGameUI's +0x2e0): its four places are slots of the hero's own
 * inventory, their numbers at the menu's +0x7c (read when it opens; GOG 0x569663). */
static int at_transmute;                     /* PC's transmuter menu is open (shop: the transmuter) */
static int at_enchant;                       /* PC's enchant menu is open: its mode (0x15 enchanter, 0x19/0x1a gems) */
static int tm_slot[4], tm_known;
static int tm_from[4] = {-1, -1, -1, -1};   /* where X took the item in each place from, or -1 */
static void *tm_from_inv[4];                /* and from which bag (the hero's, the pet's) */
static unsigned char *tm_shop;               /* the transmuter unit while it is open */
static int is_tm_slot(int slot) {
    int j;
    for (j = 0; tm_known && j < 4; j++) if (tm_slot[j] == slot) return 1;
    return 0;
}
static void *owner_bag(unsigned char *owner) {
    if (!owner) return 0;
    if (at_transmute && owner == tm_shop) owner = hero;
    if (isa(owner, 170)) return *(unsigned char **)GOG_SHARED_STASH ? *(void **)(*(unsigned char **)GOG_SHARED_STASH + 8) : 0;
    return *(void **)(owner + GOG_INVENTORY);
}
static void *inventory(void) { return owner_bag(cur_owner ? cur_owner : hero); }

/* ---- the enchanter's rules (the 360's sub_822B5688, sub_822ABAC8, sub_82360EC0) ----
 * The game's settings (GOG 0x524d40, the 360's global at 0x835594C4 with the same offsets): the
 * price per enchantment +0x78, the disenchant chance's base +0x7c and maximum +0x80, the
 * enchantments' maximum +0x94, the chance added per enchantment +0x98 (PC fills them from
 * ENCHANTER_PRICE_PER_ENCHANT, _DISENCHANT_CHANCE, _MAX_DISENCHANT_CHANCE, _MAX_ENCHANTS,
 * _DISENCHANT_INCREASE: 0x525579-0x52577e). An item's enchantments are its +0x2f8 (the 360's
 * +792); its price is CEquipment::enchantPrice *(OT)* (GOG 0x4b2590, the 360's sub_822ABAC8:
 * PRICE_ENCHANT by level, plus the enchantments' share, at least 300). */
#define GOG_SETTINGS 0x00524d40
#define GOG_ENCHANT_PRICE 0x004b2590
#define GOG_ENCHANTS 0x2f8
typedef unsigned char *(__cdecl *settings_fn)(void);
typedef int (__fastcall *enchant_price_fn)(void *item, void *edx);
static int max_enchants(void) {
    unsigned char *g = ((settings_fn)GOG_SETTINGS)();
    return g ? *(int *)(g + 0x94) : 0;
}
/* Identified, a weapon, armour or trinket of the four kinds (ISA 8, 13, 17, 24), under the
 * maximum of enchantments (sub_822B5688). */
static int can_retire(unsigned char *item);
static int can_enchant(unsigned char *item) {
    if (!item || !item[GOG_IDENTIFIED]) return 0;
    if (!(isa(item, 8) || isa(item, 13) || isa(item, 17) || isa(item, 24))) return 0;
    return *(int *)(item + GOG_ENCHANTS) < max_enchants();
}
/* Gems in its sockets: what PC's gem pages ask before working (GOG 0x57c173: the sockets +0x39c
 * and the gems +0x3a4; the 360's sub_8236B1C8 asks its +924). */
static int has_gems(unsigned char *item) {
    return item && *(unsigned *)(item + 0x39c) && *(unsigned *)(item + 0x3a4);
}
static float disenchant_chance(unsigned char *item) {
    unsigned char *g = ((settings_fn)GOG_SETTINGS)();
    float v;
    if (!g) return 0;
    v = *(float *)(g + 0x7c) + (float)*(int *)(item + GOG_ENCHANTS) * *(float *)(g + 0x98);
    return v > *(float *)(g + 0x80) ? *(float *)(g + 0x80) : v;
}

#define MAX_LIST 128
static void *list[MAX_LIST];
static int list_slot[MAX_LIST];
static int list_count;

/* The 360's list (sub_82369FE8): the equipped items first, then the others, both read from the
 * last reference to the first; each in the filter's category or in any (6). */
static unsigned char *identify_scrolls(int *count);

static void build_list(int filter) {
    unsigned char *inv = (unsigned char *)inventory();
    void **refs;
    unsigned n, i;
    int pass;
    list_count = 0;
    if (!inv) return;
    refs = *(void ***)(inv + INV_REFS);
    n = *(unsigned *)(inv + INV_REFS + 4);
    /* The transmuter's column: the items in its four places, in their order, whatever the filter
     * (it has no tabs: town/05); the bags leave those places out. */
    if (at_transmute && cur_owner == tm_shop) {
        int j;
        for (j = 0; j < 4 && tm_known; j++)
            for (i = 0; i < n; i++) {
                unsigned char *ref = (unsigned char *)refs[i];
                if (ref && *(void **)(ref + REF_ITEM) && *(int *)(ref + REF_SLOT) == tm_slot[j] && list_count < MAX_LIST) {
                    list[list_count] = *(void **)(ref + REF_ITEM); list_slot[list_count] = tm_slot[j]; list_count++;
                }
            }
        return;
    }
    for (pass = 0; pass < 2; pass++) {
        for (i = n; i-- > 0;) {
            unsigned char *ref = (unsigned char *)refs[i];
            void *item;
            int slot, c;
            if (!ref || !(item = *(void **)(ref + REF_ITEM))) continue;
            slot = *(int *)(ref + REF_SLOT);
            if (is_tm_slot(slot)) continue;
            if ((slot < EQUIPPED_SLOTS) != (pass == 0)) continue;
            /* The gem's item select (the 360's list in mode 2 with +388, sub_82369FE8): no
             * filter; identified items (ISA 2, ITEM) with a free socket (+796; +916 against +924,
             * GOG's +0x2fc, +0x39c, +0x3a4). */
            if (cur_select) {
                unsigned char *e = (unsigned char *)item;
                if (!isa(e, 2) || !e[GOG_IDENTIFIED] || *(unsigned *)(e + 0x39c) == *(unsigned *)(e + 0x3a4)) continue;
            } else {
                c = category(item);
                if (c != filter && c != 6) continue;
            }
            if (list_count < MAX_LIST) { list[list_count] = item; list_slot[list_count] = slot; list_count++; }
        }
    }
}

/* ---- the item's name and icon ---- */

/* An MSVC (VC9) std::wstring at p: its text as UTF-8 into out; 0 if it does not look like one. */
static int wstring_utf8(const unsigned char *p, char *out, unsigned size) {
    unsigned len = *(const unsigned *)(p + 0x14), res = *(const unsigned *)(p + 0x18), i, n = 0;
    const unsigned short *s;
    if (len == 0 || len > 80 || res < len || res > 4096) return 0;
    s = res < 8 ? (const unsigned short *)(p + 4) : *(const unsigned short *const *)(p + 4);
    if (!s) return 0;
    for (i = 0; i < len; i++) {
        unsigned c = s[i];
        if (c < 0x20) return 0;
        if (c < 0x80) { if (n + 1 >= size) break; out[n++] = (char)c; }
        else if (c < 0x800) { if (n + 2 >= size) break; out[n++] = (char)(0xc0 | c >> 6); out[n++] = (char)(0x80 | (c & 0x3f)); }
        else { if (n + 3 >= size) break; out[n++] = (char)(0xe0 | c >> 12); out[n++] = (char)(0x80 | (c >> 6 & 0x3f)); out[n++] = (char)(0x80 | (c & 0x3f)); }
    }
    out[n] = 0;
    return 1;
}

/* The same for a long text (a description: lines kept, up to 1000 characters). */
static int wstring_utf8_long(const unsigned char *p, char *out, unsigned size) {
    unsigned len = *(const unsigned *)(p + 0x14), res = *(const unsigned *)(p + 0x18), i, n = 0;
    const unsigned short *s;
    if (len == 0 || len > 1000 || res < len || res > 8192) return 0;
    s = res < 8 ? (const unsigned short *)(p + 4) : *(const unsigned short *const *)(p + 4);
    if (!s) return 0;
    for (i = 0; i < len; i++) {
        unsigned c = s[i];
        if (c < 0x80) { if (n + 1 >= size) break; out[n++] = (char)c; }
        else if (c < 0x800) { if (n + 2 >= size) break; out[n++] = (char)(0xc0 | c >> 6); out[n++] = (char)(0x80 | (c & 0x3f)); }
        else { if (n + 3 >= size) break; out[n++] = (char)(0xe0 | c >> 12); out[n++] = (char)(0x80 | (c >> 6 & 0x3f)); out[n++] = (char)(0x80 | (c & 0x3f)); }
    }
    out[n] = 0;
    return 1;
}

typedef void *(__fastcall *wstr_ctor_fn)(void *self, void *edx, const unsigned short *text);
typedef struct { unsigned char bytes[28]; } wstring_by_value;   /* VC9 std::wstring */
typedef void (__fastcall *wstr_dtor_fn)(void *self, void *edx);
/* msvcp90's std::wstring constructor (from a wide C string) and destructor. */
static int wstring_functions(wstr_ctor_fn *ctor, wstr_dtor_fn *dtor) {
    static wstr_ctor_fn c;
    static wstr_dtor_fn d;
    if (!c || !d) {
        void *m = GetModuleHandleA("msvcp90.dll");
        c = m ? (wstr_ctor_fn)GetProcAddress(m, "??0?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QAE@PB_W@Z") : 0;
        d = m ? (wstr_dtor_fn)GetProcAddress(m, "??1?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QAE@XZ") : 0;
    }
    *ctor = c;
    *dtor = d;
    return c && d;
}

/* Where the item's display name is: found by the probe below (a run, 2026-10-16: "second blade"). */
static int name_offset = GOG_ITEM_NAME;

/* The probe: every std::wstring in the item's first 0x600 bytes, logged once, to find the name. */
static void probe_strings(unsigned char *item) {
    static int done;
    unsigned off;
    char buf[200];
    if (done) return;
    done = 1;
    x360_log("x360 inv: wstrings in the first item (");
    for (off = 0x40; off < 0x600; off += 4) {
        __try {
            if (wstring_utf8(item + off, buf, sizeof buf)) {
                x360_log(" +"); x360_log_num((long)off); x360_log(" \""); x360_log(buf); x360_log("\"");
            }
        } __except (1) {
        }
    }
    x360_log(" )\r\n");
}

/* The full name (prefix, name, suffix: "dagger with a suffix"), as the 360's rows write it
 * (its screenshots: "prefixed shoes"): the function PC's equipment tooltip calls for its name
 * line (GOG 0x4b3fe0 from 0x54289a: thiscall on the item, the std::wstring returned into its first
 * argument, then 0; ret 0x8). Its neighbour 0x4b4470, CEquipment::getFullItemName *(OT)*, builds
 * the line above it instead ("rare sword", "unidentified trinket"). */
#define GOG_FULL_ITEM_NAME 0x004b3fe0
typedef void *(__fastcall *full_name_fn)(void *item, void *edx, void *out, int flag);
static int full_item_name(unsigned char *item, char *out, unsigned size) {
    typedef void (__fastcall *wdtor_fn)(void *self, void *edx);
    static wdtor_fn wdtor;
    static int tried;
    unsigned char ws[32];
    int ok = 0;
    if (!tried) {
        void *m = GetModuleHandleA("msvcp90.dll");
        tried = 1;
        wdtor = m ? (wdtor_fn)GetProcAddress(m, "??1?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QAE@XZ") : 0;
    }
    if (!wdtor || !item[GOG_IDENTIFIED] || !(isa(item, 8) || isa(item, 13) || isa(item, 39))) return 0;
    __try {
        ((full_name_fn)GOG_FULL_ITEM_NAME)(item, 0, ws, 0);
        ok = wstring_utf8(ws, out, size);
        wdtor(ws, 0);
    } __except (1) { ok = 0; }
    return ok;
}

/* The name's colour, as PC's equipment tooltip picks it for the hero (GOG 0x5431a5-0x5438d9) and the
 * 360's rows by the same rule (sub_822BBCA8): PC's colour table (the singleton 0x524d40, wstrings
 * 0x1c apart) at +0x55c if 0x4818f0 says so; none (the default) unless the virtual +0x154 or
 * isa 0xa0; else +0x540 if the item's "SET" (0x4b28f0) is not empty (operator!= against the empty
 * string at 0xe5575c), +0x524 unique (isa 0x36), +0x508 rare (isa 0x37), +0x4ec otherwise.
 * 0 if none (the row keeps the 360's grey). */
#define GOG_COLOUR_TABLE 0x00524d40
#define GOG_ITEM_FLAG_COLOUR 0x004818f0
#define GOG_ITEM_SET 0x004b28f0
#define GOG_EMPTY_WSTRING 0x00e5575c
#define GOG_WSTRING_NE 0x00a85428      /* msvcp90's operator!= (two std::wstring), the import */
#define GOG_WSTRING_DTOR 0x00a85390    /* msvcp90's ~basic_string<wchar_t>, the import */
static int name_colour(unsigned char *item, char *out, unsigned size) {
    typedef unsigned char *(*table_fn)(void);
    typedef unsigned char (__fastcall *flag_fn)(void *self, void *edx);
    typedef void *(__fastcall *set_fn)(void *self, void *edx, void *out);
    typedef unsigned char (*ne_fn)(const void *a, const void *b);
    typedef void (__fastcall *dtor_fn)(void *self, void *edx);
    unsigned char *table, ws[32];
    int off = 0;
    out[0] = 0;
    if (!item[GOG_IDENTIFIED]) return 0;
    __try {
        table = ((table_fn)GOG_COLOUR_TABLE)();
        if (!table) return 0;
        if (((flag_fn)GOG_ITEM_FLAG_COLOUR)(item, 0)) off = 0x55c;
        else if (((flag_fn)(*(void ***)item)[0x154 / 4])(item, 0) || isa(item, 0xa0)) {
            void *got = ((set_fn)GOG_ITEM_SET)(item, 0, ws);
            unsigned char named = (*(ne_fn *)GOG_WSTRING_NE)(got, (const void *)GOG_EMPTY_WSTRING);
            (*(dtor_fn *)GOG_WSTRING_DTOR)(ws, 0);
            off = named ? 0x540 : isa(item, 0x36) ? 0x524 : isa(item, 0x37) ? 0x508 : 0x4ec;
        }
        if (!off || !wstring_utf8(table + off, out, size)) { out[0] = 0; return 0; }
    } __except (1) { out[0] = 0; return 0; }
    return 1;
}

static void item_name(unsigned char *item, char *out, unsigned size) {
    out[0] = 0;
    if (full_item_name(item, out, size)) return;
    if (name_offset) {
        int ok = 0;
        __try { ok = wstring_utf8(item + name_offset, out, size); } __except (1) { ok = 0; }
        if (ok) return;
    }
    copy(out, size, "?", 0);
}

/* The item's icon: the image of PC's own icon window (CEquipment::createIcon builds it from the
 * item's ICON in PC's data, on PC's itemicons imagesets), read from its "Image" property or its
 * first child's. */
static void item_icon(unsigned char *item, char *out, unsigned size) {
    void *icon;
    out[0] = 0;
    icon = *(void **)(item + GOG_ICON_WINDOW);
    if (!icon && ui) {
        __try { ((create_icon_fn)GOG_CREATE_ICON)(item, 0, ui, 0); } __except (1) {}
        icon = *(void **)(item + GOG_ICON_WINDOW);
    }
    if (!icon) return;
    read_prop(icon, "Image", out, size);
    if (!out[0] && child_count(icon, 0)) read_prop(child_at(icon, 0, 0), "Image", out, size);
}

/* ---- the screen ---- */

static void *inventory_root, *master_root;
static int inventory_failed, inventory_open;
static int filter;
static int selected[6], top[6];

/* The 360's two bags (CMasterHousingXenon's +0x104 and +0x108, sub_823702E0): the hero's and the
 * pet's, side by side; LB/RB past the first or the last filter moves the focus to the other bag
 * (to its last or first filter). The drawing and the actions work on the globals above, which
 * hold the bag selected by select_panel. */
#define PET_PREFIX "x360pet_"
#define SHOP_PREFIX "x360mer_"
#define GOG_GET_PET_FN 0x0040e720
#define SOCK_PREFIX "x360sock_"
static struct { const char *prefix; int is_pet, is_shop, filter, selected[6], top[6]; void *root; int is_select; } panels[4] = {
    {PREFIX, 0, 0, 0, {0}, {0}, 0, 0}, {PET_PREFIX, 1, 0, 0, {0}, {0}, 0, 0}, {SHOP_PREFIX, 0, 1, 0, {0}, {0}, 0, 0},
    {SOCK_PREFIX, 0, 0, 0, {0}, {0}, 0, 1}};   /* the gem's item select, in the message housing */
static int active_panel, focus_panel;
static unsigned char *shop;                  /* the merchant or the stash while PC's menu of it is open */
static int at_stash;                         /* shop is a stash's chest (the shared one of type 170) */
static unsigned char *the_pet(void) {
    typedef void *(__fastcall *pet_fn)(void *hero, void *edx, int index);
    return hero ? (unsigned char *)((pet_fn)GOG_GET_PET_FN)(hero, 0, 0) : 0;
}
/* The columns, left to right (LB and RB walk through them, round): the bag and the pet's bag; at a
 * merchant (CMerchantMenuXenon, sub_82378D30: its +0x10C, +0x108, +0x110) the merchant's first. */
/* At the enchanter the first column is his panel, not a list: the bags only are walked. */
static int columns(void) { return shop && !at_enchant ? 3 : 2; }
static int column_panel(int c) { return shop && !at_enchant ? (c ? c - 1 : 2) : c; }
static int panel_column(int p) { return shop && !at_enchant ? (p == 2 ? 0 : p + 1) : p; }
static void select_panel(int i) {
    int j;
    panels[active_panel].filter = filter;
    for (j = 0; j < 6; j++) { panels[active_panel].selected[j] = selected[j]; panels[active_panel].top[j] = top[j]; }
    active_panel = i;
    filter = panels[i].filter;
    for (j = 0; j < 6; j++) { selected[j] = panels[i].selected[j]; top[j] = panels[i].top[j]; }
    cur_prefix = panels[i].prefix;
    cur_owner = panels[i].is_shop ? shop : panels[i].is_pet ? the_pet() : hero;
    cur_select = panels[i].is_select;
}
#define ROWS 5
#define ROW_PITCH 92.0f                     /* ItemTray0-4 are 92 pixels apart in the layout */

static const char *const tab_window[6] = {"inventory_filter_weapons", "inventory_filter_armor",
    "inventory_filter_jewelry", "inventory_filter_potions", "inventory_filter_spells", "inventory_filter_fish"};
static const char *const tab_image[6] = {"tab_weapons", "tab_armor", "tab_jewelry", "tab_potions", "tab_gems", "tab_fish"};
static const char *const filter_name[6] = {"filter.0", "filter.1", "filter.2", "filter.3", "filter.4", "filter.5"};   /* x360_text keys */

static void row_name(char *out, const char *base, int row) {
    char n[2] = {(char)('0' + row), 0};
    copy(out, 48, base, n);
}

/* The icon's background (sub_82368980, its table at 0x8339FB08): red for a weapon, armour or
 * trinket the hero cannot equip (the item's virtual the right click checks, PC's +0x164); gold
 * for UNIQUE (54), blue for MAGIC (55), green for the other enchanted (RANDOMMAGIC 135); else
 * blank. The 360's gold is "traycolor_gold", which its imageset lacks; its yellow stands in. Its
 * set items (another colour, by the set's name) are not done. */
static const char *quality_image(unsigned char *item) {
    if ((isa(item, 8) || isa(item, 13) || isa(item, 39)) &&
        !((can_equip_fn)(*(void ***)item)[GOG_CAN_EQUIP_SLOT])(item, 0, hero, 1))
        return "set:X360_widgets2 image:traycolor_red";
    if (isa(item, 54)) return "set:X360_widgets2 image:traycolor_yellow";
    if (isa(item, 55)) return "set:X360_widgets2 image:traycolor_blue";
    if (isa(item, 135)) return "set:X360_widgets2 image:traycolor_green";
    return "set:X360_widgets2 image:traycolor_blank";
}

/* The comparison arrow (sub_82368980): the item's value against the one equipped in its place, up
 * if better, down if worse, none if equal or nothing to compare. Weapons by DPS (PC's DPS, the
 * equipped one from getComparisonItems); armour's value is not located yet, so no arrow there. */
static void compare_arrow(int row, unsigned char *item, int equipped) {
    char name[48];
    void *inv = panels[focus_panel].is_shop ? owner_bag(hero) : inventory(), *first = 0, *second = 0;
    int mine, theirs;
    row_name(name, "qualityarrow", row);
    if (equipped || !inv || !(isa(item, 8) || isa(item, 13)) || !item[GOG_IDENTIFIED]) { show(win(name), 0); return; }
    ((comparison_fn)GOG_COMPARISON_ITEMS)(inv, 0, item, &first, &second);
    if (!first || first == item) { show(win(name), 0); return; }
    if (isa(item, 8)) {
        mine = ((int_get_fn)GOG_DPS)(item, 0);
        theirs = ((int_get_fn)GOG_DPS)(first, 0);
    } else {
        mine = *(int *)(item + GOG_ARMOR);
        theirs = *(int *)((unsigned char *)first + GOG_ARMOR);
    }
    if (mine == theirs) { show(win(name), 0); return; }
    prop(win(name), "Image", mine > theirs ? "set:X360_widgets1 image:comparrison_arrow_up"
                                           : "set:X360_widgets1 image:comparrison_arrow_down");
    show(win(name), 1);
}

/* A row's details, as the 360's screenshots show them: the price with its coin (PC's sellPrice);
 * "DPS: n" for an identified weapon, "xN" for a stack; an unidentified item's name in red, "???"
 * and "Press A to identify" (the 360's strings). Hidden when empty, as sub_82368980 does. */
/* The count after "Effects:" (sub_822AFA10): the elemental damage bonuses above 0, the sizes of the
 * effect manager's three lists, and the same for each socketed item. */
static int effects_count(unsigned char *item) {
    int **types = (int **)(item + GOG_DAMAGE_TYPES), *bonus = *(int **)(item + GOG_DAMAGE_BONUSES);
    unsigned char *manager = *(unsigned char **)(item + GOG_EFFECT_MANAGER);
    unsigned char **sockets = *(unsigned char ***)(item + GOG_SOCKETED);
    int n = 0, i, count = (int)(types[1] - types[0]), socketed = *(int *)(item + GOG_SOCKETED + 4);
    for (i = 0; i < count; i++) if (bonus[i] > 0) n++;
    if (manager) n += *(int *)(manager + 0x1c) + *(int *)(manager + 0x2c) + *(int *)(manager + 0x3c);
    for (i = 0; i < socketed; i++) if (sockets[i]) n += effects_count(sockets[i]);
    return n;
}

static void fill_details(int row, unsigned char *item) {
    char name[48], buf[64], *p;
    int identified = item[GOG_IDENTIFIED] != 0, stack = *(int *)(item + GOG_STACK);
    /* The price (sub_822BC4B8, by the item's owner): what the merchant asks for its own items
     * (ISA 41, MERCHANT), what it pays for the others; none for an item of type 103, which the
     * merchant does not take (sub_8236B7B8; PC refuses it too, GOG 0x54eec2). Gold is hidden
     * when empty (sub_82368980). */
    buf[0] = 0;
    if (!isa(item, 103)) {
        p = put_int(buf, ((int_get_fn)(cur_owner && isa(cur_owner, 41) ? GOG_ASKED_PRICE : GOG_PAID_PRICE))(item, 0));
        *p = 0;
    }
    row_name(name, "Gold", row); text(win(name), buf); show(win(name), buf[0] != 0);
    buf[0] = 0;
    if (!identified) copy(buf, sizeof buf, "???", 0);
    else if (stack > 1) { buf[0] = 'x'; p = put_int(buf + 1, stack); *p = 0; }
    else if (isa(item, 8)) {
        copy(buf, sizeof buf, "DPS: ", 0);
        p = put_int(buf + 5, ((int_get_fn)GOG_DPS)(item, 0));
        *p = 0;
    } else if (isa(item, 13)) {
        /* The 360 (sub_82368980): its "armor" (the tooltip's word), a space, the value. */
        copy(buf, sizeof buf, "armor ", 0);
        p = put_int(buf + 6, *(int *)(item + GOG_ARMOR));
        *p = 0;
    }
    row_name(name, "DamageArmor", row); text(win(name), buf); show(win(name), buf[0] != 0);
    row_name(name, "Requirment", row);
    text(win(name), identified ? "" : x360_text("identify_hint"));
    prop(win(name), "TextColour", "FFFF2A2A");
    show(win(name), !identified);
    /* "Effects:" and the count (sub_82368980, the string at 0x820d0ef0) for an identified item
     * with any, not a CONSUMABLE (1); the 360 also hides it at a GAMBLER (127). */
    {
        int effects = identified && !isa(item, 1) ? effects_count(item) : 0;
        p = buf;
        if (effects) { copy(buf, sizeof buf, x360_text("effects"), 0); p = put_int(buf + length(buf), effects); }
        *p = 0;
        row_name(name, "Enchantments", row); text(win(name), buf); show(win(name), effects != 0);
    }
    row_name(name, "SlotText", row);
    {
        char colour[32];
        prop(win(name), "TextColour", !identified ? "FFFF2A2A"
                                      : name_colour(item, colour, sizeof colour) ? colour
                                      : "FFCCCACF");   /* the 360's grey (its image, near the tabs' names) */
    }
}

/* The row's icon at its image's proportions, as the 360's row fill (sub_82368980, 0x82368D14):
 * the image's width and height (W, H) times s = min(the frame's height / W, its width / H), the
 * frame being the row's QualityBk; s halved for a unit of type 0x78; centred in the frame (the
 * catalog's Slot0: 36 x 54 of a 54 x 80 frame, Centre and Centre). */
#define TYPE_HALF_ICON 0x78
static void row_icon_size(int row, unsigned char *item, const char *image) {
    char name[48], value[128];
    float v[8], w, h, fw, fh, s;
    void *slot;
    row_name(name, "QualityBk", row);
    read_prop(win(name), "UnifiedAreaRect", value, sizeof value);
    row_name(name, "Slot", row);
    slot = win(name);
    if (!slot || !parse_rect(value, v) || !image_size(image, &w, &h)) return;
    fw = v[5] - v[1];
    fh = v[7] - v[3];
    s = fh / w < fw / h ? fh / w : fw / h;
    if (isa(item, TYPE_HALF_ICON)) s *= 0.5f;
    set_size(slot, 0, 0, w * s, h * s);
    prop(slot, "HorizontalAlignment", "Centre");
    prop(slot, "VerticalAlignment", "Centre");
}

/* The bar's buttons as the 360's sub_8236E888 places them: a button with no text is hidden, one
 * with a text shown with it (sub_8236E7B0); each takes its icon's width plus, with a text, the
 * text's (A and X also 2.5); a gap of max(0, (0.9 x the tray's width - their sum) x 0.15) after
 * each one with a text; the group centred (x = half the gap - half the sum + 5): A, then Y, then
 * X, each at the widths of the ones before it. Its constants are pixels at 720 rows, so they are
 * scaled here by height / 720. */
static void place_buttons(const char *a, const char *y, const char *x) {
    void *ba = win("buttonA"), *by = win("buttonY"), *bx = win("buttonX"), *tray = win("buttontray");
    char value[128];
    float va[8], vy[8], vx[8], vt[8], icon, s = k * 768.0f / 720.0f, wa, wy, wx, gap, sum, left;
    if (!ba || !by || !bx || !tray) return;
    read_prop(tray, "UnifiedAreaRect", value, sizeof value);
    if (!parse_rect(value, vt)) return;
    read_prop(ba, "UnifiedAreaRect", value, sizeof value);
    if (!parse_rect(value, va)) return;
    read_prop(by, "UnifiedAreaRect", value, sizeof value);
    if (!parse_rect(value, vy)) return;
    read_prop(bx, "UnifiedAreaRect", value, sizeof value);
    if (!parse_rect(value, vx)) return;
    show(ba, a[0] != 0); show(by, y[0] != 0); show(bx, x[0] != 0);
    if (a[0]) text(win("buttonAtext"), a);
    if (y[0]) text(win("buttonYtext"), y);
    if (x[0]) text(win("buttonXtext"), x);
    icon = va[5] - va[1];
    wa = (a[0] ? text_width(win("buttonAtext"), a) + 2.5f * s : 0) + icon;
    wx = (x[0] ? text_width(win("buttonXtext"), x) + 2.5f * s : 0) + icon;
    wy = (y[0] ? text_width(win("buttonYtext"), y) : 0) + icon;
    gap = (0.9f * (vt[5] - vt[1]) - (wy + wx + wa)) * 0.15f;
    if (gap < 0) gap = 0;
    if (a[0]) wa += gap;
    if (y[0]) wy += gap;
    if (x[0]) wx += gap;
    sum = wa + wy + wx;
    left = gap * 0.5f - sum * 0.5f + 5.0f * s;
    va[1] = left; va[5] = left + icon;
    vy[1] = left + wa; vy[5] = vy[1] + icon;
    vx[1] = left + wa + wy; vx[5] = vx[1] + icon;
    write_rect(value, va); prop(ba, "UnifiedAreaRect", value);
    write_rect(value, vy); prop(by, "UnifiedAreaRect", value);
    write_rect(value, vx); prop(bx, "UnifiedAreaRect", value);
}

/* The section of a bag that an item goes to (PC's 0x4e8410 its type, 0x4e84a0 the type's section;
 * -1 for none). */
static int bag_section(void *inv, void *item) {
    typedef int (__fastcall *of_item_fn)(void *self, void *edx, void *item);
    typedef int (__fastcall *of_int_fn)(void *self, void *edx, int v);
    if (!inv || !item) return -1;
    return ((of_int_fn)0x004e84a0)(inv, 0, ((of_item_fn)0x004e8410)(inv, 0, item));
}

/* An item of a filter's category in a bag (0 for none). */
static void *of_category(void *bag, int filter) {
    unsigned char *inv = (unsigned char *)bag;
    void **refs;
    unsigned n, i;
    if (!inv) return 0;
    refs = *(void ***)(inv + INV_REFS);
    n = *(unsigned *)(inv + INV_REFS + 4);
    for (i = 0; i < n; i++) {
        unsigned char *ref = (unsigned char *)refs[i];
        void *item = ref ? *(void **)(ref + REF_ITEM) : 0;
        if (item && category(item) == filter) return item;
    }
    return 0;
}

static void refresh(void) {
    int row, i, counts[6];
    char name[48], buf[200];
    for (i = 0; i < 6; i++) { build_list(i); counts[i] = list_count; }
    build_list(filter);
    if (selected[filter] >= list_count) selected[filter] = list_count ? list_count - 1 : 0;
    if (selected[filter] < top[filter]) top[filter] = selected[filter];
    if (selected[filter] >= top[filter] + ROWS) top[filter] = selected[filter] - ROWS + 1;
    if (top[filter] < 0) top[filter] = 0;
    /* The focused column only has its "_rollover" images: its frame, its border and its filter's
     * tab (the catalog: menus/08-inventory's bag against the pet's, town/21-stash-1's columns). */
    /* The merchant's column has its own images (the catalog's town/01: backroundbase_merchant,
     * merchant_tab_*). */
    int is_shop = panels[active_panel].is_shop && !at_transmute;   /* the transmuter's: the plain images (town/05) */
    int is_tm = panels[active_panel].is_shop && at_transmute;
    for (i = 0; i < 6; i++) {
        char image[64];
        copy(image, sizeof image, is_shop ? "set:X360_widgets2 image:merchant_" : "set:X360_widgets2 image:", tab_image[i]);
        if (i == filter && active_panel == focus_panel)
            copy(image + length(image), sizeof image - length(image), "_rollover", 0);
        prop(win(tab_window[i]), "Image", image);
    }
    /* A column that is not the player's (the pet's, a merchant's) dims the tabs of the filters it
     * has nothing in to 0.25, but the focused column's own filter (sub_823654B8, 0x82365948: the
     * tab's count at its entry's +116; a transmuter's column, ISA 134, has no tabs). The 360's
     * stash is the hero's own (its +2548, sub_8236AD18), so its column is the player's: not
     * dimmed (town/21). */
    for (i = 0; i < 6; i++)
        prop(win(tab_window[i]), "Alpha", cur_owner != hero && !(at_stash && cur_owner == shop) && !counts[i] &&
                                          !(i == filter && active_panel == focus_panel) ? "0.25" : "1");
    prop(win("TopFrame"), "Image", is_shop ? (active_panel == focus_panel ? "set:X360_widgets2 image:backroundbase_merchant_rollover"
                                                                          : "set:X360_widgets2 image:backroundbase_merchant")
                                           : active_panel == focus_panel ? "set:X360_widgets2 image:backroundbase_inventory_rollover"
                                                                         : "set:X360_widgets2 image:backroundbase_inventory");
    prop(win("HiliteFrame"), "Image", active_panel == focus_panel ? "set:X360_widgets image:framea_rollover"
                                                                  : "set:X360_widgets image:framea");
    show(win("weaponHi"), 0); show(win("armorHi"), 0); show(win("trinketHi"), 0);
    show(win("potionHi"), 0); show(win("scrollHi"), 0); show(win("fishHi"), 0);
    /* The item select's filter line reads "Insert Into" (the catalog's in-game/11; 0x820D0C54). */
    text(win("filtername"), x360_text(cur_select ? "filter.insert" : is_tm ? "filter.transmute" : filter_name[filter]));
    /* The transmuter's column (sub_82367D38, ISA 134): no tabs, its line of help (the layout's
     * "transmutor", with the Y prompt under the rows) instead. */
    /* The item select's list has no filter: its tabs and the filter's name hidden. */
    for (i = 0; i < 6; i++) show(win(tab_window[i]), !is_tm && !cur_select);
    /* The merchant's name (sub_82367D38): the Gambler's (ISA 127, GAMBLER) or a merchant's; at a
     * stash its own (the catalog's town/21, 30). */
    text(win("characterName"), x360_text(is_tm ? "title.transmuter"
                                         : is_shop ? (at_stash ? (isa(cur_owner, 170) ? "title.sharedstash" : "title.stash")
                                                    : isa(cur_owner, 127) ? "title.gambler" : "title.merchant")
                                         : panels[active_panel].is_pet ? "bag.pet" : "bag.mine"));
    /* RB on the focused bag only (the catalog: the bag, the pet's bag, the stash's columns). */
    show(win("rightbumper"), active_panel == focus_panel && !is_tm && !cur_select);
    /* At the transmuter the focused bag has LB too (town/05). */
    show(win("leftbumper"), active_panel == focus_panel && at_transmute && !is_tm);
    show(win("transmutor"), is_tm);
    show(win("petintown"), 0);
    /* The 360 writes the owner's gold and "items/capacity" over the layout's samples ("12423",
     * "3/25"; sub_82367D38): PC's gold, and the bag's items and slots (the slots from the first
     * pane on; the 360 counts every reference against its own 126). */
    {
        unsigned char *owner = hero, *inv = (unsigned char *)inventory();
        char *p = buf;
        int n = 0;
        while (*(unsigned char **)(owner + GOG_OWNER)) owner = *(unsigned char **)(owner + GOG_OWNER);
        p = put_int(p, *(long *)(owner + GOG_GOLD));
        *p = 0;
        text(win("playergoldtext"), buf);
        /* The bag's gold, but at a menu of mode 7, 8, 9 or 10 (sub_82367D38, 0x82368620: the
         * stashes and the two gem pages; town/21, 02, 03). */
        show(win("playergoldwin"), cur_owner == hero && !at_stash && at_enchant != 0x19 && at_enchant != 0x1a && !cur_select);
        if (inv) {
            void **refs = *(void ***)(inv + INV_REFS);
            unsigned count = *(unsigned *)(inv + INV_REFS + 4), j;
            int *starts = *(int **)(inv + INV_PANE_STARTS);
            int first = starts ? starts[0] : EQUIPPED_SLOTS;
            int nsec = starts ? (int)((*(int **)(inv + INV_PANE_STARTS + 4)) - starts) : 0, end = *(long *)(inv + INV_SLOTS);
            /* PC's bag (three sections of 21, bag=pc): the focused item's section, as the one that
             * fills up (a pick-up looks for a slot in its item's section only, 0x4e85b0); on an
             * empty tab, the section of an item of the tab's category in the hero's bag (the
             * pet's tab empty); the 360's one bag (bag=360), or no such item, the whole bag. */
            if (nsec > 1 && !is_tm) {
                int sec = bag_section(inv, list_count ? list[selected[filter]] : of_category(owner_bag(hero), filter));
                if (sec >= 0 && sec < nsec) {
                    first = starts[sec];
                    end = sec + 1 < nsec ? starts[sec + 1] : end;
                }
            }
            for (j = 0; j < count; j++) {
                int slot = refs[j] ? *(int *)((unsigned char *)refs[j] + REF_SLOT) : -1;
                if (slot >= first && slot < end) n++;
            }
            p = put_int(buf, is_tm ? list_count : n);   /* the transmuter's: of its 4 (sub_82367D38) */
            *p++ = '/';
            p = put_int(p, is_tm ? 4 : end - first);
            *p = 0;
            text(win("inventorysize"), buf);
        }
    }
    show(win("uparrow"), top[filter] > 0);
    show(win("downarrow"), top[filter] + ROWS < list_count);
    for (row = 0; row < ROWS; row++) {
        int idx = top[filter] + row;
        void *tray;
        static int emptied[ROWS];            /* the shop column's trays shown empty at the transmuter */
        row_name(name, "ItemTray", row);
        tray = win(name);
        /* The transmuter's four places are drawn even empty: the tray at 0.6, nothing in it
         * (town/05: ItemTray0-3, their windows hidden). */
        if (is_tm && row < 4 && idx >= list_count) {
            show(tray, 1);
            prop(tray, "Image", "set:X360_widgets1 image:item_tray");
            prop(tray, "Alpha", "0.6");
            show_children(tray, 0);
            emptied[row] = 1;
            continue;
        }
        if (panels[active_panel].is_shop && emptied[row]) { show_children(tray, 1); emptied[row] = 0; }
        show(tray, idx < list_count);
        if (idx >= list_count) continue;
        item_name((unsigned char *)list[idx], buf, sizeof buf);
        row_name(name, "SlotText", row); text(win(name), buf);
        item_icon((unsigned char *)list[idx], buf, sizeof buf);
        row_name(name, "Slot", row); prop(win(name), "Image", buf);
        row_icon_size(row, (unsigned char *)list[idx], buf);
        row_name(name, "QualityBk", row); prop(win(name), "Image", quality_image((unsigned char *)list[idx]));
        row_name(name, "SlotText", row); show(win(name), 1);
        row_name(name, "Equipped", row); show(win(name), list_slot[idx] < EQUIPPED_SLOTS);
        /* The tray (sub_823652A0): item_tray, or item_tray_equipped for an equipped item; the
         * column's selected row at alpha 1.0 (focused or not: sub_82368980 tests the row's item
         * against the column's, sub_82369F60), the others at 0.9 if equipped, else 0.6. */
        prop(tray, "Image", list_slot[idx] < EQUIPPED_SLOTS ? "set:X360_widgets1 image:item_tray_equipped"
                                                            : "set:X360_widgets1 image:item_tray");
        prop(tray, "Alpha", idx == selected[filter] ? "1.0"
                            : list_slot[idx] < EQUIPPED_SLOTS ? "0.9" : "0.6");
        /* The details (sub_82368980); the quality is not filled yet, so hidden as the 360 hides
         * it when it has no text. */
        fill_details(row, (unsigned char *)list[idx]);
        row_name(name, "Quality", row); show(win(name), 0);
        compare_arrow(row, (unsigned char *)list[idx], list_slot[idx] < EQUIPPED_SLOTS);
        /* The sockets (sub_82368980, 0x823697F8): the row's two marks, the filled ones first
         * (socket_full, the image 17), then the empty (socket_empty, 16), the rest hidden; the
         * item's sockets +916 and the filled +924 on the 360, PC's +0x39c and +0x3a4 (0x4b9510).
         * Unidentified items show theirs too (in-game/11, town/02). */
        {
            unsigned char *e = (unsigned char *)list[idx];
            /* Read on what can be socketed only (weapons, armour, trinkets: ISA 8, 13, 39). */
            int equipment = isa(e, 8) || isa(e, 13) || isa(e, 39);
            unsigned total = equipment ? *(unsigned *)(e + 0x39c) : 0, filled = equipment ? *(unsigned *)(e + 0x3a4) : 0, q;
            for (q = 0; q < 2; q++) {
                void *w;
                row_name(name, q ? "socket1_" : "socket0_", row);
                w = win(name);
                if (q < filled) prop(w, "Image", "set:X360_widgets1 image:socket_full");
                else if (q < total) prop(w, "Image", "set:X360_widgets1 image:socket_empty");
                show(w, q < total && total <= 8);
            }
        }
    }
    /* The gold icons of the rows have generated names (i_506, ...): hidden with their text's parent. */
    {
        static const char *const gold_icons[ROWS] = {"i_506", "i_552", "i_524", "i_548", "i_513"};
        for (row = 0; row < ROWS; row++) show(win(gold_icons[row]), top[filter] + row < list_count);
    }
    /* The selection: moveframe framed around the selected row, its buttons under it. */
    {
        void *mf = win("moveframe");
        char value[128];
        float v[8];
        int sel_row = selected[filter] - top[filter];
        show(mf, list_count > 0 && active_panel == focus_panel);
        read_prop(mf, "UnifiedAreaRect", value, sizeof value);
        if (parse_rect(value, v)) move_by(mf, 0, moveframe_top + (float)sel_row * ROW_PITCH * k - v[3]);
        show(win("IDENTIFYWIN"), 0);
        show(win("buttonB"), 0);
        /* At a merchant (sub_8236C5C0) the frame's header says what X does: act.buy in the
         * merchant's column (TextColour FFFBE469), act.sell in the others (FFFF4800). */
        show(win("buyandsell"), shop && !at_stash && !at_transmute && !at_enchant);
        if (shop && !at_stash && !at_transmute && !at_enchant) {
            text(win("buyandselltext"), x360_text(panels[active_panel].is_shop ? "act.buy" : "act.sell"));
            prop(win("buyandselltext"), "TextColour", panels[active_panel].is_shop ? "FFFBE469" : "FFFF4800");
        }
        if (list_count) {
            unsigned char *item = (unsigned char *)list[selected[filter]];
            int equipped = list_slot[selected[filter]] < EQUIPPED_SLOTS, scrolls = 0;
            const char *a, *x, *y;
            char label[32];
            /* An unidentified item: the 360's "Identify (" + the scrolls + ")" (its screenshot,
             * "Identify(11)"). */
            label[0] = 0;
            if (!item[GOG_IDENTIFIED] && identify_scrolls(&scrolls) && scrolls > 0) {
                const char *p = x360_text("act.identify");
                char digits[12];
                int n = 0, d = 0, v = scrolls;
                while (*p && n < 20) label[n++] = *p++;
                label[n++] = ' ';
                label[n++] = '(';
                do { digits[d++] = (char)('0' + v % 10); v /= 10; } while (v && d < 11);
                while (d && n < 30) label[n++] = digits[--d];
                label[n++] = ')';
                label[n] = 0;
            }
            if (cur_select) {
                /* The item select (sub_8236C8D0, its mode not 0): A "Insert" only. */
                a = x360_text("act.insert"); x = ""; y = "";
            } else if (at_enchant) {
                /* At the enchanter (town/06): A act.enchant for an item he can enchant, nothing
                 * else (the pressbutton's rule, sub_82360EC0). */
                a = at_enchant == 0x15 && can_enchant(item) ? x360_text("act.enchant")
                    : at_enchant == 0x1b && can_retire(item) && !panels[active_panel].is_pet ? x360_text("act.retire") : ""; x = ""; y = "";   /* none at the gem pages (town/02, 03) */
            } else if (at_transmute) {
                /* At the transmuter (sub_8236C8D0, mode 5), in every column: X swaps; Y is the
                 * transmuter's own prompt under its rows. */
                a = ""; x = x360_text("act.swap"); y = "";
            } else if (at_stash && panels[active_panel].is_shop) {
                /* The stash's column (its menu's mode 7, the shared one's 8: sub_8236C8D0's plain
                 * bar): Y Drop, X Swap. A for an item in the stash is not done (the 360's acts
                 * for the column's owner, the hero, on an item of another inventory). */
                a = ""; x = x360_text("act.swap"); y = x360_text("act.drop");
            } else if (shop && !at_stash && !at_transmute && panels[active_panel].is_shop) {
                /* The merchant's column: X buys, nothing else (sub_8236C8D0, ISA 41). */
                a = ""; x = x360_text("act.buy"); y = "";
            } else if (shop && !at_stash && !at_transmute && !at_enchant) {
                /* A bag at a merchant (sub_8236C8D0 with the merchant's menu, mode 4): X sells; A
                 * identifies an unidentified item, unequips an equipped one, equips a weapon,
                 * armour or trinket (ISA 8, 13, 39; for an owner not of type 28, the pet, a
                 * trinket only), sockets a gem (120), else nothing; no Y. */
                x = x360_text("act.sell"); y = "";
                if (!item[GOG_IDENTIFIED]) a = label;
                else if (isa(item, 8) || isa(item, 13) || isa(item, 39))
                    a = equipped ? x360_text("act.unequip") : cur_owner == hero || isa(item, 39) ? x360_text("act.equip") : "";
                else if (isa(item, 120)) a = x360_text("act.socket");
                else a = "";
            } else {
                /* The bar as in the 360's screenshots: A by the item (Unequip, Equip, Use, Learn
                 * for a spell, Socket for a gem: in-game/10), Y Drop, X Swap (to the pet's bag);
                 * no Info. */
                a = label[0] ? label : x360_text(equipped ? "act.unequip"
                                                 : isa(item, 129) ? "act.learn"
                                                 : isa(item, 120) ? "act.socket"
                                                 : ((item_bool_fn)GOG_IS_USEABLE)(item, 0) ? (usable_now(item, (unsigned char *)cur_owner) ? "act.use" : "")
                                                 : "act.equip");
                x = x360_text("act.swap"); y = x360_text("act.drop");
            }
            place_buttons(a, y, x);
        }
    }
}

/* ---- the Info card ----
 * The 360's card (its screenshots: the item's type, name, "28 damage per second", "(2-handed)",
 * "Physical Damage: 11-22", over the pet's column at the selected row) is PC's equipment tooltip:
 * CGameUI::showEquipmentTooltip *(OT)* (GOG 0x541e80, ret 0x14: the character, the item, the
 * tooltip, the two panels it is compared with), the tooltip CGameUI's +0x2ac, its root window
 * +0x14 attached to +0x10 (PC detaches it with removeChildWindow when nothing is hovered, GOG
 * 0x55eb8e). It is filled by PC, then moved onto the GUI sheet, in the frame's third column at the
 * selected row's height.
 * Item comparison: the 360's bar shows "enable item compare" / "disable item compare" by its flag
 * (sub_8236C8D0, the global's +0x14c4) beside the d-pad's left-right image, which turns it. On,
 * the item is shown with what it would replace: PC's comparison tooltips (CGameUI's +0x2b0 and
 * +0x2b4, filled from getComparisonItems as PC's hover does, GOG 0x55eb18). Where the 360 puts
 * them (its screenshots, 1920x1080): the item's card to the right of the bag's list, over the pet's
 * column, wider than it, its top at the selected row's; the equipped item's card to the left of the
 * list, over the character's column, its right edge at the list's left edge and its top at the
 * row's; neither is kept inside the frame (the chest armour's runs past its bottom). */
#define GOG_SHOW_EQUIPMENT_TOOLTIP 0x00541e80
#define GOG_TOOLTIP 0x2ac                /* then the comparison tooltips at +0x2b0 and +0x2b4 */
typedef void (__fastcall *show_tooltip_fn)(void *ui, void *edx, void *character, void *item, void *tooltip, void *cmp1, void *cmp2);
static int compare_on;
static void *info_item;

/* CEGUI caches each window's screen rect and does not drop the children's when a frame is moved
 * under another parent (addChildWindow): their pixel rects came back as at the layout's load.
 * Window::notifyScreenAreaChanged drops them, recursively. */
static void fresh_rects(void *w) {
    static void_fn notify;
    if (!notify) notify = (void_fn)need("?notifyScreenAreaChanged@Window@CEGUI@@QAEXXZ");
    if (w && notify) notify(w, 0);
}

static void *parent_of(void *w) {
    typedef void *(__fastcall *parent_fn)(const void *self, void *edx);
    static parent_fn get_parent;
    if (!get_parent) get_parent = (parent_fn)need("?getParent@Window@CEGUI@@QBEPAV12@XZ");
    return w && get_parent ? get_parent(w, 0) : 0;
}

static unsigned char *card(int i) { return ui ? *(unsigned char **)(ui + GOG_TOOLTIP + 4 * i) : 0; }
static void *card_root(int i) { unsigned char *t = card(i); return t ? *(void **)(t + 0x14) : 0; }

static void hide_card(int i) {
    typedef void (__fastcall *remove_child_fn)(void *self, void *edx, void *child);
    static remove_child_fn remove_child;
    void *root = card_root(i), *parent;
    if (!remove_child) remove_child = (remove_child_fn)need("?removeChildWindow@Window@CEGUI@@QAEXPAV12@@Z");
    if (!root || !remove_child) return;
    parent = parent_of(root);
    if (parent) remove_child(parent, 0, root);
}

static void hide_info(void) { hide_card(0); hide_card(1); hide_card(2); info_item = 0; }

/* Card i (0 the item's, 1 and 2 the comparison's) filled by PC for item, compared against cards
 * cmp1 and cmp2, moved onto the sheet; its height in pixels, 0 if none. */
#define GOG_MERCHANT_MENU_UI 0x2d8         /* CGameUI's CMerchantMenu (the shop's section below) */
/* The first window under w whose name ends with suffix (PC's tooltips are made with a numbered
 * prefix: "gui__3497ItemName"). */
static void *descendant_ending(void *w, const char *suffix) {
    typedef const void *(__fastcall *name_fn)(const void *self, void *edx);
    static name_fn gn;
    void *stack[96];
    int sp = 0;
    unsigned n = length(suffix);
    if (!gn) gn = (name_fn)need("?getName@Window@CEGUI@@QBEABVString@2@XZ");
    if (!w || !gn) return 0;
    stack[sp++] = w;
    while (sp > 0) {
        void *v = stack[--sp];
        const char *name = c_str(gn(v, 0), 0);
        unsigned len = length(name), c, nc = child_count(v, 0);
        if (len >= n) {
            unsigned i = 0;
            while (i < n && name[len - n + i] == suffix[i]) i++;
            if (i == n && v != w) return v;
        }
        for (c = 0; c < nc && sp < 95; c++) stack[sp++] = child_at(v, 0, c);
    }
    return 0;
}
static void *desc_prop;                    /* PC's DESCRIPTION given the 360's text for a card */
static unsigned char desc_saved[28];       /* its own text meanwhile (a VC9 std::wstring) */
static void *(__fastcall *desc_assign)(void *self, void *edx, const unsigned short *text);
static const unsigned short *(__fastcall *desc_cstr)(const void *self, void *edx);
static void desc_restore(void) {
    wstr_ctor_fn wc;
    wstr_dtor_fn wd;
    if (!desc_prop) return;
    if (desc_assign && desc_cstr) desc_assign(desc_prop, 0, desc_cstr(desc_saved, 0));
    if (wstring_functions(&wc, &wd)) wd(desc_saved, 0);
    desc_prop = 0;
}

static float fill_card(int i, void *item, void *cmp1, void *cmp2) {
    void *root, *sys = system_ptr(), *sheet = sys ? get_gui_sheet(sys, 0) : 0;
    unsigned char *t = card(i);
    char value[128];
    float v[8];
    int saved31 = -1;
    if (!t || !sheet || !item) { hide_card(i); return 0; }
    /* PC rebuilds a card only when the item's GUID (its +0x8, kept at the tooltip's +0x8)
     * changes (GOG 0x54281f); the screen calls it on its own changes (an item identified, a stack
     * used), so the kept GUID is cleared first. */
    *(unsigned *)(t + 0x8) = 0xffffffffu;
    *(unsigned *)(t + 0xc) = 0xffffffffu;
    /* The 360's transmuter is its merchant menu in mode 5, so its card has the sell price
     * (sub_8233FE48: CGameUI's +920 virtual +0xc, the item not of type 103; town/05). PC's
     * tooltip asks its merchant menu the same (0x5471bc: the virtual +0xc, 0x58dcd0: open, or
     * +0x31 clear): at the transmuter +0x31 is cleared for the call and put back. */
    if (at_transmute && ui && *(unsigned char **)(ui + GOG_MERCHANT_MENU_UI)) {
        unsigned char *mm = *(unsigned char **)(ui + GOG_MERCHANT_MENU_UI);
        saved31 = mm[0x31];
        mm[0x31] = 0;
    }
    /* The 360's description where PC's differs (the converter's desc.<unit name>, "~" and the
     * 360's text, empty when it has none: PC's mouse texts, as the identify scroll's
     * "Right-click to use..."): PC's own DESCRIPTION property is given that text for the call,
     * so that PC lays the card out with it, and put back after it. */
    {
        typedef void *(__fastcall *wassign_fn)(void *self, void *edx, const unsigned short *text);
        typedef void *(__fastcall *wcopy_fn)(void *self, void *edx, const void *other);
        typedef const unsigned short *(__fastcall *wcstr_fn)(const void *self, void *edx);
        static const unsigned short key_desc[] = {'D', 'E', 'S', 'C', 'R', 'I', 'P', 'T', 'I', 'O', 'N', 0};
        static wassign_fn wassign;
        static wcopy_fn wcopy;
        static wcstr_fn wcstr;
        char name[96], key[112];
        const char *desc;
        if (!wassign) {
            void *m = GetModuleHandleA("msvcp90.dll");
            wassign = m ? (wassign_fn)GetProcAddress(m, "?assign@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QAEAAV12@PB_W@Z") : 0;
            wcopy = m ? (wcopy_fn)GetProcAddress(m, "??0?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QAE@ABV01@@Z") : 0;
            wcstr = m ? (wcstr_fn)GetProcAddress(m, "?c_str@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QBEPB_WXZ") : 0;
        }
        desc_prop = 0;
        if (wassign && wcopy && wcstr && unit_name((unsigned char *)item, name, sizeof name)) {
            copy(key, sizeof key, "desc.", name);
            desc = x360_text(key);
            if (desc[0] == '~' && (desc_prop = unit_property((unsigned char *)item, key_desc))) {
                static unsigned short wide[600];
                const unsigned char *u = (const unsigned char *)desc + 1;
                unsigned n = 0;
                while (*u && n < 599) {   /* UTF-8 to UTF-16 (the BMP) */
                    unsigned c = *u++;
                    if (c >= 0xe0 && u[0] && u[1]) { c = (c & 0x0f) << 12 | (u[0] & 0x3f) << 6 | (u[1] & 0x3f); u += 2; }
                    else if (c >= 0xc0 && u[0]) { c = (c & 0x1f) << 6 | (u[0] & 0x3f); u++; }
                    wide[n++] = (unsigned short)c;
                }
                wide[n] = 0;
                wcopy(desc_saved, 0, desc_prop);
                wassign(desc_prop, 0, wide);
            }
        }
        desc_assign = wassign;
        desc_cstr = wcstr;
    }
    __try {
        /* The card's character is the item's owner: for a merchant's item PC writes the price it
         * asks (its tooltip, GOG 0x546c9f: a character other than the hero, of type 41), as the
         * 360's card; for the bags' what it pays (0x5471bc, with the merchant's menu open). */
        ((show_tooltip_fn)GOG_SHOW_EQUIPMENT_TOOLTIP)(ui, 0, i == 0 && panels[focus_panel].is_shop && shop && !at_transmute ? shop : hero,
                                                      item, t, cmp1, cmp2);
    } __except (1) {
        desc_restore();
        x360_log("x360 inv: showEquipmentTooltip threw\r\n");
        if (saved31 >= 0) (*(unsigned char **)(ui + GOG_MERCHANT_MENU_UI))[0x31] = (unsigned char)saved31;
        return 0;
    }
    if (saved31 >= 0) (*(unsigned char **)(ui + GOG_MERCHANT_MENU_UI))[0x31] = (unsigned char)saved31;
    desc_restore();
    root = card_root(i);
    if (!root) { x360_log("x360 inv: the tooltip has no root\r\n"); return 0; }
    /* The 360's card names the item in its SerifBig (the catalog's 103ItemName); PC's layout has
     * FrizQuadrataBig there. Its other windows (Serif, and FrizQuadrataBig for the big DPS) are
     * the 360's already. */
    prop(descendant_ending(root, "ItemName"), "Font", "X360_SerifBig");
    /* The 360's card has its dividers' windows but draws nothing there (in-game/10, 30: the card's
     * background at their rows); PC's draw a white line. */
    prop(descendant_ending(root, "Divider"), "Alpha", "0");
    prop(descendant_ending(root, "Divider1"), "Alpha", "0");
    add_child(sheet, 0, root);
    set_visible(root, 0, 1);
    read_prop(root, "UnifiedAreaRect", value, sizeof value);
    return parse_rect(value, v) ? v[7] - v[3] : 0;
}

/* Card i put at (x, y), kept inside [x, right]. */
static void place_card(int i, float x, float right, float y) {
    void *root = card_root(i);
    char value[128];
    float v[8], w, h;
    if (!root) return;
    read_prop(root, "UnifiedAreaRect", value, sizeof value);
    if (!parse_rect(value, v)) return;
    w = v[5] - v[1]; h = v[7] - v[3];
    if (right > 0 && x + w > right) x = right - w;   /* a wide card kept inside the column */
    v[0] = v[2] = v[4] = v[6] = 0;
    v[1] = x; v[3] = y; v[5] = x + w; v[7] = y + h;
    write_rect(value, v);
    prop(root, "UnifiedAreaRect", value);
    move_to_front(root, 0);
}

/* Card i's width in pixels, 0 if none. */
static float card_width(int i) {
    void *root = card_root(i);
    char value[128];
    float v[8];
    if (!root) return 0;
    read_prop(root, "UnifiedAreaRect", value, sizeof value);
    return parse_rect(value, v) ? v[5] - v[1] : 0;
}

static void show_info(void *item, float y, float list_left) {
    typedef struct { float top, bottom, left, right; } rect6;   /* CEGUI 0.6's Rect */
    typedef rect6 *(__fastcall *pixel_rect_fn6)(const void *self, void *edx, rect6 *out);
    static pixel_rect_fn6 rect_of;
    /* The card goes over the column right of the focused one (the catalog: the bag's over the
     * pet's column, menus/08; the merchant's over the bag, town/07). */
    void *pet_top = win_of(panels[column_panel(panel_column(focus_panel) + 1)].prefix, "TopFrame");
    void *inv = inventory(), *first = 0, *second = 0;
    float x = 0, h0, h1 = 0, h2 = 0, gap = 4.0f * k, bottom = 0, w, y1;
    RECT cr;
    HWND hw = active_window();
    if (!item) { hide_info(); return; }
    if (!rect_of) rect_of = (pixel_rect_fn6)need("?getUnclippedPixelRect@Window@CEGUI@@QBE?AVRect@2@XZ");
    if (pet_top && rect_of) {
        rect6 r;
        fresh_rects(pet_top);
        rect_of(pet_top, 0, &r);
        /* 22 pixels at 1080 into the third column (the catalog: 103Base at 1194.6 over the pet's
         * TopFrame at 1172.7 in menus/08, 18 and 10, 1196.1 over 1174.1 in town/05), 25.1 into the
         * middle one (the merchant's card over the bag: 773.1 over 748.0 in town/01, 07, 09-11,
         * 18, 36-38) */
        /* ... measured on the visible border, PC's tooltip drawing its frame about 15 pixels
         * inside its root where the 360's is 22: the 360's border at the column's left - 0.7
         * (third column: 1172 against 1172.7) and + 3 (middle: 751 against 748.0); ours was 7-8
         * pixels right in both (QA run 2: 758; run B3: 1180), so the root goes 10.24 and 12.87
         * (base units) in. */
        x = r.left + (panel_column(focus_panel) + 1 == 1 ? 12.87f : 7.4f) * k;   /* QA run 8: the third column 4 more left */
    }
    /* The card's bottom kept 36 rows (of 720) above the screen's bottom: the 360's dumps
     * (in-game/17, 18: the card at the selected row's top, pushed up from row 5 on so that it ends
     * at 1026 of 1080, past the frame's bottom). */
    if (hw && GetClientRect(hw, &cr)) bottom = (float)cr.bottom - 36.0f * (float)cr.bottom / 720.0f;
    if (compare_on && inv && list_slot[selected[filter]] >= EQUIPPED_SLOTS)
        ((comparison_fn)GOG_COMPARISON_ITEMS)(inv, 0, item, &first, &second);
    if (first == item) first = 0;
    if (second == item) second = 0;
    h0 = fill_card(0, item, 0, 0);
    if (!h0) return;
    if (first) h1 = fill_card(1, first, card(0), 0); else hide_card(1);
    if (second) h2 = fill_card(2, second, card(0), card(1)); else hide_card(2);
    /* only the screen's bottom keeps them up */
    place_card(0, x, 0, bottom > 0 && y + h0 > bottom ? bottom - h0 : y);
    y1 = y;
    if (bottom > 0 && y1 + (h1 ? h1 : 0) + (h2 ? gap + h2 : 0) > bottom)
        y1 = bottom - (h1 ? h1 : 0) - (h2 ? gap + h2 : 0);
    w = card_width(1);
    if (h1) { place_card(1, list_left - 16.0f * k - w, 0, y1); y1 += h1 + gap; }
    w = card_width(2);
    if (h2) place_card(2, list_left - 16.0f * k - w, 0, y1);
    if (info_item != item) {
        char name[128];
        item_name((unsigned char *)item, name, sizeof name);
        x360_log(first ? "x360 inv: info with comparison, " : "x360 inv: info, ");
        x360_log(name);
        if (name_colour((unsigned char *)item, name, sizeof name)) { x360_log(" colour "); x360_log(name); }
        *put_int(name, effects_count((unsigned char *)item)) = 0;
        x360_log(" effects "); x360_log(name);
        x360_log("\r\n");
    }
    info_item = item;
}

/* ---- the character column (charactermenu_xenon, the frame's first column) ----
 * PC's own stats menu (STATSMENU.LAYOUT, kept by cegui_proxy.c) has windows of the same names; the
 * texts PC writes there are copied over: the same numbers PC shows. The 3D doll is not done. */
#define CHR_PREFIX "x360chr_"
#define PETCHR_PREFIX "x360petchr_"
extern void *x360_pc_stats_root;
static void *character_root;
static void *petchr_root;                 /* the pet's sheet (petmenu_xenon, in-game/19) in the character's column */
static int chr_on;                        /* the character's column is up (the inventory's or the Stats page) */
static void chr_show(int on);
static void pet_sheet_fill(unsigned char *pet);
typedef void *(__fastcall *child_search_fn)(const void *self, void *edx, const void *name);
typedef const void *(__fastcall *get_text_fn)(const void *self, void *edx);

typedef const void *(__fastcall *get_name_fn)(const void *self, void *edx);

/* PC's CStatsMenu (vtable 0xab87c4, GOG) writes its texts only in its update (the virtual +0x24,
 * CSubMenu::update(float), GOG 0x5b0d90); with its menu closed they keep the layout's samples
 * ("Name", "XP:0-0"). The instance is one of CGameUI's fields; its update is called before the
 * texts are copied. */
#define GOG_STATS_MENU_VTABLE 0x00ab87c4
static void *pc_stats_menu(void) {
    static void *found;
    unsigned i;
    if (found || !ui) return found;
    for (i = 0; i < 0x1000 / 4 && !found; i++) {
        void *p = 0;
        unsigned vt = 0;
        __try { p = ((void **)ui)[i]; vt = p ? *(unsigned *)p : 0; } __except (1) { vt = 0; }   /* no jump out of a __try */
        if (vt == GOG_STATS_MENU_VTABLE) found = p;
    }
    x360_log(found ? "x360 chr: PC's stats menu found\r\n" : "x360 chr: PC's stats menu not found\r\n");
    return found;
}

/* The character column's numbers, read as PC's own stats menu reads them (CStatsMenu's update,
 * GOG 0x5b0d90, its owner at +0x40): the getters PC calls (OT's names: CCharacter::strength
 * 0x487ec0, dexterity 0x487e60, magic 0x487d70, defense 0x487d00; the maximum HP 0x4901e0 and
 * mana 0x490330, base +0x398 and +0x3bc plus effects), and the fields it reads itself, there being
 * no getters for them: the level +0xf0, the experience +0x3c8, the fame +0x3cc and its level
 * +0x3d0, the name (a wstring) +0x420. The next level's experience and fame: the experience
 * table's singleton (0x5281d0) asked by level (0x528130) and fame level (0x528180). The fame
 * title is not done. */
#define GOG_STRENGTH 0x00487ec0
#define GOG_DEXTERITY 0x00487e60
#define GOG_MAGIC 0x00487d70
#define GOG_DEFENSE 0x00487d00
#define GOG_MAX_HP 0x004901e0
#define GOG_MAX_MANA 0x00490330
#define GOG_XP_TABLE 0x005281d0
#define GOG_XP_FOR_LEVEL 0x00528130
#define GOG_FAME_FOR_LEVEL 0x00528180
#define GOG_GAME_DATA 0x00524d40
typedef void *(__cdecl *table_fn)(void);
typedef int (__fastcall *int_arg_fn)(void *self, void *edx, int v);

static void fill_character(void) {
    static int logged;
    char buf[96], *p;
    int level, xp, xp_next = 0, fame_level, fame, fame_next = 0;
    void *table;
    /* The focused bag's character (the 360's two columns, sub_823702E0): the hero or the pet. */
    unsigned char *who = panels[focus_panel].is_pet ? (unsigned char *)the_pet() : hero;
    if (!character_root || !who) return;
    (void)pc_stats_menu;
    chr_show(chr_on);
    if (who != hero) { pet_sheet_fill(who); return; }
    __try {
        level = *(int *)(who + 0xf0);
        xp = *(int *)(who + 0x3c8);
        fame = *(int *)(who + 0x3cc);
        fame_level = *(int *)(who + 0x3d0);
        table = ((table_fn)GOG_XP_TABLE)();
        if (table) {
            xp_next = ((int_arg_fn)GOG_XP_FOR_LEVEL)(table, 0, level);
            fame_next = ((int_arg_fn)GOG_FAME_FOR_LEVEL)(table, 0, fame_level);
        }
    } __except (1) {
        x360_log("x360 chr: reading the character threw\r\n");
        return;
    }
    if (wstring_utf8(who + 0x420, buf, sizeof buf)) text(win_of(CHR_PREFIX, "characterName"), buf);
    /* The fame title, as PC's stats menu picks it (GOG 0x5b146e): the game data's singleton
     * (0x524d40) holds the titles, VC9 wstrings of 28 bytes from +0xc8, their count +0xcc, a bound
     * +0xd0; the title is the fame level's minus one, at most the last. */
    {
        char title[96];
        title[0] = 0;
        __try {
            unsigned char *data = ((table_fn)GOG_GAME_DATA)();
            if (data && fame_level > 0) {
                unsigned idx = (unsigned)(fame_level - 1), last = *(unsigned *)(data + 0xcc) - 1;
                unsigned char *titles = *(unsigned char **)(data + 0xc8);
                if (idx > last) idx = last;
                if (titles && idx < *(unsigned *)(data + 0xd0)) wstring_utf8(titles + 28 * idx, title, sizeof title);
            }
        } __except (1) {
            title[0] = 0;
            x360_log("x360 chr: reading the fame title threw\r\n");
        }
        text(win_of(CHR_PREFIX, "CharacterTitle"), title);
    }
    p = put_int(buf, level); *p = 0; text(win_of(CHR_PREFIX, "Level"), buf);
    p = put_int(buf, fame_level); *p = 0; text(win_of(CHR_PREFIX, "FameLevel"), buf);
    copy(buf, sizeof buf, "XP: ", 0); p = put_int(buf + 4, xp); *p++ = '/'; p = put_int(p, xp_next); *p = 0;
    text(win_of(CHR_PREFIX, "XP"), buf);
    copy(buf, sizeof buf, "Fame: ", 0); p = put_int(buf + 6, fame); *p++ = '/'; p = put_int(p, fame_next); *p = 0;
    text(win_of(CHR_PREFIX, "Fame"), buf);
    p = put_int(buf, ((int_get_fn)GOG_MAX_HP)(who, 0)); *p = 0; text(win_of(CHR_PREFIX, "HP"), buf);
    p = put_int(buf, ((int_get_fn)GOG_MAX_MANA)(who, 0)); *p = 0; text(win_of(CHR_PREFIX, "Mana"), buf);
    p = put_int(buf, ((int_get_fn)GOG_STRENGTH)(who, 0)); *p = 0; text(win_of(CHR_PREFIX, "strPts"), buf);
    p = put_int(buf, ((int_get_fn)GOG_DEXTERITY)(who, 0)); *p = 0; text(win_of(CHR_PREFIX, "dexPts"), buf);
    p = put_int(buf, ((int_get_fn)GOG_MAGIC)(who, 0)); *p = 0; text(win_of(CHR_PREFIX, "magicPts"), buf);
    p = put_int(buf, ((int_get_fn)GOG_DEFENSE)(who, 0)); *p = 0; text(win_of(CHR_PREFIX, "defPts"), buf);
    if (!logged) {
        /* A diagnosis: the numbers did not show in a run though they were read. */
        typedef unsigned char (__fastcall *visible_fn)(const void *self, void *edx, int local_only);
        visible_fn visible = (visible_fn)need("?isVisible@Window@CEGUI@@QBE_N_N@Z");
        static const char *const probe[] = {"HP", "Level", "characterName"};
        unsigned q;
        for (q = 0; q < 3; q++) {
            void *w = win_of(CHR_PREFIX, probe[q]);
            char v[160];
            x360_log("x360 chr: window "); x360_log(probe[q]); x360_log(w ? " found" : " missing");
            if (w) {
                read_prop(w, "Text", v, sizeof v); x360_log(", text \""); x360_log(v); x360_log("\"");
                x360_log(visible && visible(w, 0, 0) ? ", visible" : ", hidden");
                read_prop(w, "UnifiedAreaRect", v, sizeof v); x360_log(", rect "); x360_log(v);
                read_prop(w, "Font", v, sizeof v); x360_log(", font "); x360_log(v);
                read_prop(w, "TextColours", v, sizeof v); x360_log(", colours "); x360_log(v);
            }
            x360_log("\r\n");
        }
        {
            /* The column's tree: names and rects, three levels down from its root. */
            get_name_fn gn = (get_name_fn)need("?getName@Window@CEGUI@@QBEABVString@2@XZ");
            void *stack[64];
            int depth[64], sp = 0;
            stack[sp] = win_of(CHR_PREFIX, "TopFrame"); depth[sp++] = 0;
            while (sp > 0) {
                void *w = stack[--sp];
                int d = depth[sp];
                char v[128];
                unsigned c, nc = child_count(w, 0);
                read_prop(w, "UnifiedAreaRect", v, sizeof v);
                x360_log("x360 chr: tree "); x360_log_num(d); x360_log(" "); x360_log(c_str(gn(w, 0), 0));
                x360_log(" children "); x360_log_num((long)nc); x360_log(" "); x360_log(v); x360_log("\r\n");
                for (c = 0; c < nc && d < 4 && sp < 63; c++) { stack[sp] = child_at(w, 0, c); depth[sp++] = d + 1; }
            }
        }
        logged = 1;
        x360_log("x360 chr: level "); x360_log_num(level); x360_log(" xp "); x360_log_num(xp);
        x360_log("/"); x360_log_num(xp_next); x360_log(" fame "); x360_log_num(fame_level); x360_log(" ");
        x360_log_num(fame); x360_log("/"); x360_log_num(fame_next); x360_log("\r\n");
    }
}

/* The Info card for the hero's selected item, at its row's height. */
static void update_info(void) {
    typedef struct { float top, bottom, left, right; } rect6;
    typedef rect6 *(__fastcall *pixel_rect_fn6)(const void *self, void *edx, rect6 *out);
    static pixel_rect_fn6 rect_of;
    char name[48];
    void *tray;
    rect6 r;
    if (!inventory_open || panels[focus_panel].is_pet || !list_count || sock_open) { hide_info(); return; }
    if (!rect_of) rect_of = (pixel_rect_fn6)need("?getUnclippedPixelRect@Window@CEGUI@@QBE?AVRect@2@XZ");
    row_name(name, "ItemTray", selected[filter] - top[filter]);
    tray = win(name);
    if (!tray || !rect_of) return;
    fresh_rects(win("TopFrame"));
    rect_of(tray, 0, &r);
    show_info(list[selected[filter]], r.top, r.left);
}

/* Both bags, the focused one last (it stays selected for the actions). The pet's bag shows the
 * layout's "Your pet has gone to town." when there is no pet. */
/* ---- the enchanter's panel (enchantermenu_xenon, the frame's first column: town/06) ----
 * sub_82360EC0, for the item selected in the focused bag (the 360's +292):
 * - none: no price, no chance, no Enchant prompt;
 * - its enchantments at the maximum: the price red "Maximum enchantments reached" (0x820CF128),
 *   (the 360's red is markup in the text; here the window's TextColour),
 *   no chance, no prompt; one he cannot enchant: red "Cannot enchant item" (0x820CF18C);
 * - else the price (sub_822ABAC8), red when it is over the hero's gold, the disenchant chance
 *   "Disenchant chance" (0x820CF164) ": " N "%" (the settings' base and per-enchantment chance,
 *   at most their maximum), and the prompt.
 * The result line is cleared each time; the title is the enchanter's (title.enchanter). Its
 * other pages (getsocket, destroysocket, retire, enchantshrine) stay hidden. */
#define ENCH_PREFIX "x360ench_"
static void *ench_root;
static char ench_result[512];            /* the last result of the enchanter or a gem goblin, shown in their page */
/* PC's modal (CGameUI 0x54cad0, thiscall: title and text std::wstring by value, an int; callee
 * pops 0x3c) is called by the enchant act itself (0x57a800, at 0x57b6c9 and its other branches),
 * yet showed seconds later (QA runs 6, 20: after its 3D sign's fall). Hooked: during the act its
 * text is taken there and then for the 360's page and PC's modal not opened, the result at once as
 * the 360's (the user, QA run 20). Its first instructions (GOG's push -1; push imm32, 7 bytes;
 * Steam's mov eax, fs:[0]; push -1, 8 bytes) run from a copy. */
typedef struct { void *al; union { unsigned short buf[8]; unsigned short *ptr; } bx; unsigned size, res; } msvc_wstring;
static int ench_acting;
static unsigned char modal_tramp[16];
static void wide_to_utf8(const msvc_wstring *w, char *out, unsigned size) {
    const unsigned short *p = w->res >= 8 ? w->bx.ptr : w->bx.buf;
    unsigned i, o = 0;
    for (i = 0; p && i < w->size && o + 4 < size; i++) {
        unsigned c = p[i];
        if (c >= 0xd800 && c < 0xdc00 && i + 1 < w->size) { c = 0x10000 + ((c - 0xd800) << 10) + (p[i + 1] - 0xdc00); i++; }
        if (c < 0x80) out[o++] = (char)c;
        else if (c < 0x800) { out[o++] = (char)(0xc0 | c >> 6); out[o++] = (char)(0x80 | (c & 0x3f)); }
        else if (c < 0x10000) { out[o++] = (char)(0xe0 | c >> 12); out[o++] = (char)(0x80 | ((c >> 6) & 0x3f)); out[o++] = (char)(0x80 | (c & 0x3f)); }
        else { out[o++] = (char)(0xf0 | c >> 18); out[o++] = (char)(0x80 | ((c >> 12) & 0x3f)); out[o++] = (char)(0x80 | ((c >> 6) & 0x3f)); out[o++] = (char)(0x80 | (c & 0x3f)); }
    }
    out[o] = 0;
}
static void __fastcall modal_show_hook(void *gui, void *edx, msvc_wstring title, msvc_wstring body, int x) {
    typedef void (__fastcall *orig_fn)(void *gui, void *edx, msvc_wstring title, msvc_wstring body, int x);
    typedef void (__fastcall *dtor_fn)(void *self, void *edx);
    if (ench_acting) {
        dtor_fn dtor = *(dtor_fn *)0x00a85390;   /* msvcp90's ~basic_string<wchar_t>, as PC's own */
        wide_to_utf8(&body, ench_result, sizeof ench_result);
        dtor(&title, 0);
        dtor(&body, 0);
        x360_log("x360 enchanter: the result taken from the act, PC's modal not opened: ");
        x360_log(ench_result); x360_log("\r\n");
        return;
    }
    ((orig_fn)(void *)modal_tramp)(gui, edx, title, body, x);
}
/* The function's first instructions, which the hook moves to the copy: they differ between the
 * builds, whose exception frames are set up in another order (GOG's: push -1; push the handler;
 * Steam's: mov eax, fs:[0]; push -1; then push the handler). The handler's address is checked too. */
#define GOG_MODAL_HANDLER 0x0070ced2   /* the modal function's exception handler (its push imm32) */
#ifdef X360_TARGET_STEAM
static const unsigned char modal_want[9] = {0x64, 0xa1, 0x00, 0x00, 0x00, 0x00, 0x6a, 0xff, 0x68};
#define MODAL_MOVED 8                  /* mov eax, fs:[0]; push -1 */
#else
static const unsigned char modal_want[3] = {0x6a, 0xff, 0x68};
#define MODAL_MOVED 7                  /* push -1; push the handler */
#endif
static void modal_hook_install(void) {
    static int done;
    unsigned char *f = (unsigned char *)0x0054cad0;
    unsigned long old;
    unsigned i;
    if (done) return;
    done = 1;
    for (i = 0; i < sizeof modal_want; i++) if (f[i] != modal_want[i]) break;
    if (i < sizeof modal_want || *(unsigned *)(f + sizeof modal_want) != GOG_MODAL_HANDLER) {
        x360_log("x360 menu: PC's modal function not as expected, left\r\n");
        return;
    }
    for (i = 0; i < MODAL_MOVED; i++) modal_tramp[i] = f[i];
    modal_tramp[MODAL_MOVED] = 0xe9;
    *(int *)(modal_tramp + MODAL_MOVED + 1) = (int)(f + MODAL_MOVED) - (int)(modal_tramp + MODAL_MOVED + 5);
    if (!VirtualProtect(modal_tramp, sizeof modal_tramp, 0x40, &old)) return;
    if (!VirtualProtect(f, MODAL_MOVED, 0x40, &old)) return;
    f[0] = 0xe9;
    *(int *)(f + 1) = (int)(void *)modal_show_hook - (int)(f + 5);
    for (i = 5; i < MODAL_MOVED; i++) f[i] = 0x90;
    VirtualProtect(f, MODAL_MOVED, old, &old);
    x360_log("x360 menu: PC's modal hooked (the enchanter's result)\r\n");
}
static unsigned ench_pending;             /* when an act was done whose result PC's modal has yet to show */
static void ench_fill(unsigned char *item) {
    char buf[160], *p;
    /* The gem pages (the 360's sub_82360EC0, its modes 3 and 2): the goblin of type 132 (PC's
     * interaction 0x19) has the page the 360 titles "Destroy Gems" (getsocket, town/02), the one of
     * type 133 (0x1a) "Recover Gems" (destroysocket, town/03; the 360's sub_82376D48 by its +304:
     * 132 -> 10, 133 -> 9). Their A prompt shows for an item with gems (sub_8236B1C8). */
    /* The retirement (the 360's CEnchantMenuXenon mode 4, opened for the interaction 27 as PC's
     * 0x1b: sub_82360CD0 shows its panel 14, the layout's "retire": its title, the paragraph on the
     * heirloom, the warning and the Retire prompt, all the layout's own). */
    if (at_enchant == 0x1b) {
        show(win_of(ENCH_PREFIX, "enchant"), 0);
        show(win_of(ENCH_PREFIX, "getsocket"), 0);
        show(win_of(ENCH_PREFIX, "destroysocket"), 0);
        show(win_of(ENCH_PREFIX, "enchantshrine"), 0);
        show(win_of(ENCH_PREFIX, "retire"), 1);
        return;
    }
    if (at_enchant == 0x19 || at_enchant == 0x1a) {
        int get = at_enchant == 0x19;
        show(win_of(ENCH_PREFIX, "enchant"), 0);
        show(win_of(ENCH_PREFIX, "retire"), 0);
        show(win_of(ENCH_PREFIX, "getsocket"), get);
        show(win_of(ENCH_PREFIX, "destroysocket"), !get);
        /* The result lines start empty (the 360's catalog: town/02, 03; the layout has a sample)
         * and hold the last result until the next A (in-game/14). */
        text(win_of(ENCH_PREFIX, "getsocketresult"), get ? ench_result : "");
        text(win_of(ENCH_PREFIX, "destroysocketresult"), get ? "" : ench_result);
        show(win_of(ENCH_PREFIX, get ? "pressbuttongetsocket" : "pressbuttondestroysocket"), has_gems(item));
        return;
    }
    show(win_of(ENCH_PREFIX, "enchant"), 1);
    show(win_of(ENCH_PREFIX, "getsocket"), 0);
    show(win_of(ENCH_PREFIX, "destroysocket"), 0);
    show(win_of(ENCH_PREFIX, "retire"), 0);
    show(win_of(ENCH_PREFIX, "enchantshrine"), 0);
    text(win_of(ENCH_PREFIX, "title"), x360_text("title.enchanter"));
    /* The result, yellow in the layout, under the disenchant chance until the next A (in-game/22-24). */
    text(win_of(ENCH_PREFIX, "resultofenchant"), ench_result);
    {
        /* A diagnosis (QA run 21: the result taken, not seen): the window's text and visibility. */
        static char logged[64];
        if (ench_result[0] && ench_result[0] != logged[0]) {
            typedef unsigned char (__fastcall *vis_fn)(const void *self, void *edx, unsigned char local);
            static vis_fn vis;
            void *w = win_of(ENCH_PREFIX, "resultofenchant");
            char now[256];
            if (!vis) vis = (vis_fn)need("?isVisible@Window@CEGUI@@QBE_N_N@Z");
            read_prop(w, "Text", now, sizeof now);
            x360_log("x360 enchanter: the page's result window "); x360_log(w ? "found" : "missing");
            x360_log(w && vis && vis(w, 0, 0) ? ", shown" : ", not shown"); x360_log(", text: "); x360_log(now); x360_log("\r\n");
            logged[0] = ench_result[0];
        } else if (!ench_result[0]) logged[0] = 0;
    }
    if (!item) {
        text(win_of(ENCH_PREFIX, "priceofenchantment"), "");
        text(win_of(ENCH_PREFIX, "disenchantChance"), "");
        show(win_of(ENCH_PREFIX, "pressbutton"), 0);
        return;
    }
    if (!can_enchant(item)) {
        int at_max = item[GOG_IDENTIFIED] && *(int *)(item + GOG_ENCHANTS) >= max_enchants();
        prop(win_of(ENCH_PREFIX, "priceofenchantment"), "TextColour", "FFFF0000");
        text(win_of(ENCH_PREFIX, "priceofenchantment"), x360_text(at_max ? "ench.max" : "ench.cannot"));
        text(win_of(ENCH_PREFIX, "disenchantChance"), "");
        show(win_of(ENCH_PREFIX, "pressbutton"), 0);
        return;
    }
    {
        int price = ((enchant_price_fn)GOG_ENCHANT_PRICE)(item, 0);
        unsigned char *owner = hero;
        while (*(unsigned char **)(owner + GOG_OWNER)) owner = *(unsigned char **)(owner + GOG_OWNER);
        /* The 360's red is the text's "|cFFFF0000" .. "|u"; PC's CEGUI has no such markup, so
         * the window's colour (its layout's FFF7DF09 otherwise). */
        p = put_int(buf, price);
        *p = 0;
        prop(win_of(ENCH_PREFIX, "priceofenchantment"), "TextColour",
             *(int *)(owner + GOG_GOLD) < price ? "FFFF0000" : "FFF7DF09");
        text(win_of(ENCH_PREFIX, "priceofenchantment"), buf);
    }
    copy(buf, sizeof buf, x360_text("ench.chance"), ": ");
    p = put_int(buf + length(buf), (int)(disenchant_chance(item) + 0.5f));
    *p++ = '%'; *p = 0;
    text(win_of(ENCH_PREFIX, "disenchantChance"), buf);
    show(win_of(ENCH_PREFIX, "pressbutton"), 1);
}

static void refresh_all(void) {
    int c;
    for (c = 0; c < columns(); c++) {
        int p = column_panel(c);
        if (p == focus_panel || !panels[p].root) continue;
        select_panel(p);
        if (cur_owner) refresh();
        show(win("petintown"), !cur_owner);
        if (!cur_owner) { int r; char n[48]; for (r = 0; r < ROWS; r++) { row_name(n, "ItemTray", r); show(win(n), 0); } show(win("moveframe"), 0); }
    }
    select_panel(focus_panel);
    if (cur_owner) refresh();
    if (at_enchant) ench_fill(list_count ? (unsigned char *)list[selected[filter]] : 0);
    fill_character();
    text(win_of(MASTER, "itemcomparetext"), x360_text(compare_on ? "compare.disable" : "compare.enable"));
    update_info();
}

/* A on an unidentified item (the 360's "Press A to identify"): an identify scroll from the hero's
 * bag is spent and the item identified as PC does it (the identify effect's handler, GOG 0x4b388b:
 * +0x2fc set, CItem::destroyItemText 0x4d1cc0). The scroll is found by its unit's name, the 360's
 * text item.identify (no game text in the code); a stack loses one, the last one is taken out
 * and destroyed as PC destroys an
 * item (removeEquipment, then the virtual destructor +0 with 1). */
#define GOG_DESTROY_ITEM_TEXT 0x004d1cc0
/* The hero's first identify scroll stack, and in *count the scrolls of every stack. */
static int unit_named(unsigned char *item, const char *key);
static unsigned char *identify_scrolls(int *count) {
    unsigned char *inv = (unsigned char *)(hero ? *(void **)(hero + GOG_INVENTORY) : 0), *scroll = 0;
    void **refs;
    unsigned n, i;
    if (count) *count = 0;
    if (!inv) return 0;
    refs = *(void ***)(inv + INV_REFS);
    n = *(unsigned *)(inv + INV_REFS + 4);
    for (i = 0; i < n; i++) {
        unsigned char *ref = (unsigned char *)refs[i], *it;
        if (!ref || !(it = *(unsigned char **)(ref + REF_ITEM))) continue;
        if (!unit_named(it, "item.identify")) continue;   /* as usable_now tells the scroll */
        if (!scroll) scroll = it;
        if (count) *count += *(int *)(it + GOG_STACK);
    }
    return scroll;
}

static int identify(unsigned char *item) {
    unsigned char *inv = (unsigned char *)(hero ? *(void **)(hero + GOG_INVENTORY) : 0);
    unsigned char *scroll = identify_scrolls(0);
    if (!inv) return 0;
    if (!scroll) { x360_log("x360 inv: A: no identify scroll\r\n"); return 0; }
    item[GOG_IDENTIFIED] = 1;
    ((void_fn)GOG_DESTROY_ITEM_TEXT)(item, 0);
    if (*(int *)(scroll + GOG_STACK) > 1) {
        (*(int *)(scroll + GOG_STACK))--;
    } else {
        typedef void (__fastcall *destroy_fn)(void *self, void *edx, int flags);
        ((inv_item_fn)GOG_REMOVE_EQUIPMENT)(inv, 0, scroll);
        ((destroy_fn)(*(void ***)scroll)[0])(scroll, 0, 1);
    }
    x360_log("x360 inv: A: identified\r\n");
    return 1;
}

/* A on the selected row: what PC's right click on an inventory slot does (GOG 0x55e156): a
 * useable item is used (useItem); another is equipped into its first free place, or swapped
 * with what is there (getComparisonItems, removeEquipment, equip, the old one picked up into the
 * bag or dropped). An equipped item is taken off into the bag (removeEquipment, pickupEquipment);
 * if the bag is full, it goes back. */
static void sock_begin(unsigned char *gem);
static void hide_info(void);
/* ---- an item between bags is never left out of every one (the GPU agent's review, item 5) ----
 * Each move takes the item out with move_out, which remembers it and where it was, and ends with
 * move_done once it is somewhere (a bag, the ground, a socket, or destroyed as PC does). A put
 * that fails is answered with give_back (its slot, else its bag); one that cannot be undone
 * stops the game before any save (move_lost). The menus' pad input runs in __try
 * (x360_inventory_pad): an exception with a move under way, or a move left unfinished, stops the
 * game at once without touching the item (where it went is not known: it may be placed already,
 * or merged into a stack and freed), so no save is written and loading again has it. */
static struct { void *item, *inv; int slot; } in_move[2];   /* a two-handed swap moves two */
static void move_out(void *inv, void *item, int slot);
static void move_done(void) { in_move[0].item = in_move[1].item = 0; }
static int give_back(void *inv, int slot, void *item);
static void move_lost(void *item);
static void to_ground(void *item);
static void log_item_name(void *item);
static void move_cut_short(void);
extern int x360_save_held;
void x360_inventory_toggle(void);
/* Whether the item is the town portal scroll by its unit's name, as the 360's bag Use
 * (sub_8236BD80 at 0x8236BF68: the item's definition, +0x178, against the unit index's entry of
 * that name): its NAME property (PC's property bag at +0x178, read by 0x5c0190 as the item's
 * DESCRIPTION is at 0x4854dd; 0xd5d9a4 the empty default). */
static int unit_named(unsigned char *item, const char *key);
static int town_portal_scroll(unsigned char *item) { return unit_named(item, "item.townportal"); }
/* A property of the item's unit data (PC's property bag at +0x178, read by 0x5c0190 as the item's
 * DESCRIPTION is at 0x4854dd; 0xd5d9a4 the empty default): its std::wstring, or 0 when the unit
 * has none (the default is never handed out). */
static void *unit_property(unsigned char *item, const unsigned short *key_text) {
    typedef void *(__fastcall *prop_str_fn)(void *bag, void *edx, const void *key, const void *dflt);
    wstr_ctor_fn wctor;
    wstr_dtor_fn wdtor;
    unsigned char key[32];
    void *bag = *(void **)(item + 0x178), *r = 0;   /* GOG_ITEM_TEMPLATE */
    if (!bag || !wstring_functions(&wctor, &wdtor)) return 0;
    wctor(key, 0, key_text);
    __try { r = ((prop_str_fn)0x005c0190)(bag, 0, key, (const void *)0x00d5d9a4); } __except (1) { r = 0; }
    wdtor(key, 0);
    return r == (void *)0x00d5d9a4 ? 0 : r;
}

/* The item's unit name (its NAME), in UTF-8. */
static int unit_name(unsigned char *item, char *out, unsigned size) {
    static const unsigned short key_name[] = {'N', 'A', 'M', 'E', 0};
    void *r = unit_property(item, key_name);
    out[0] = 0;
    return r && wstring_utf8((const unsigned char *)r, out, size);
}

static int unit_named(unsigned char *item, const char *text_key) {
    char name[96];
    const char *want = x360_text(text_key);
    unsigned n, w = length(want);
    if (!want[0] || !unit_name(item, name, sizeof name)) return 0;
    n = length(name);
    while (n && name[n - 1] == ' ') n--;   /* the 360's text ends in spaces */
    while (w && want[w - 1] == ' ') w--;
    if (n != w) return 0;
    for (n = 0; n < w; n++) if (want[n] != name[n]) return 0;
    return 1;
}

/* Whether the item can be used now, as the 360's sub_822B9DA8 tells its bag (its Use prompt and
 * action): PC's item use (GOG 0x4b7470) refuses an item of one with no charges left (+0x1e4 the
 * stack, +0x1f0 the charges, -9999 endless) and asks the user, for each of the item's skills
 * (+0x17c: entries +0x28, count +0x2c, each one's skill +0x40), whether it can (the unit's virtual
 * +0x118 with the target, the skill and the entry): no, while a potion recharges (in-game/29).
 * The identify scroll has no Use on the 360 (sub_8236BD80 compares its unit name; in-game/30). */
static int usable_now(unsigned char *item, unsigned char *user) {
    typedef unsigned char (__fastcall *can_fn)(void *unit, void *edx, void *target, void *skill, void *entry);
    unsigned char *skills;
    unsigned i, n;
    int ok = 1;
    if (!item || !user || !((item_bool_fn)GOG_IS_USEABLE)(item, 0)) return 0;
    if (unit_named(item, "item.identify")) return 0;
    if (*(int *)(item + 0x1e4) <= 1 && *(int *)(item + 0x1f0) <= 0 && *(int *)(item + 0x1f0) != -9999) return 0;
    __try {
        skills = *(unsigned char **)(item + 0x17c);
        n = skills ? *(unsigned *)(skills + 0x2c) : 0;
        for (i = 0; i < n && ok; i++) {
            void *entry = (*(void ***)(skills + 0x28))[i];
            void *skill = entry ? *(void **)((unsigned char *)entry + 0x40) : 0;
            if (skill && !((can_fn)(*(void ***)user)[0x118 / 4])(user, 0, user, skill, entry)) ok = 0;
        }
    } __except (1) { ok = 1; }
    return ok;
}

static void act(void) {
    void *inv = inventory();
    unsigned char *item, *owner = (unsigned char *)cur_owner;   /* the 360's A acts for the bag's owner (+0x174) */
    int is_pet = panels[active_panel].is_pet;
    if (!inv || !list_count) return;
    item = (unsigned char *)list[selected[filter]];
    if (!item[GOG_IDENTIFIED]) { identify(item); return; }
    if (isa(item, 120)) { sock_begin(item); return; }   /* a gem: the item select (sub_8236B240) */
    if (list_slot[selected[filter]] < EQUIPPED_SLOTS) {
        move_out(inv, item, list_slot[selected[filter]]);
        if (((pickup_fn)GOG_PICKUP_EQUIPMENT)(inv, 0, item, 1)) x360_log("x360 inv: A: unequipped\r\n");
        else if (give_back(inv, list_slot[selected[filter]], item) || ((inv_item_fn)GOG_EQUIP_FIRST_FREE)(inv, 0, item))
            x360_log("x360 inv: A: bag full, kept equipped\r\n");
        else move_lost(item);
        move_done();
        return;
    }
    /* A spell in the pet's bag (the 360's sub_8236BD80, ISA 129: sub_822B9CA0, then sub_8236BB70):
     * the pet learns it when it can use it (sub_822B9DA8, then the use sub_822B9A60); otherwise,
     * with its places full (a pet, ISA 87, has two; the known spells counted by sub_821FD868, PC's
     * getKnownSpell 0x484ff0), the 360 opens its spell-replacing menu (CGameUI +6296), not ported;
     * else nothing, no sound. Here PC's own use with a user (GOG 0x54d980, thiscall on CGameUI:
     * level, item, user, target, pet; PC's right click on a menu owner's spell, 0x582150, gives
     * the owner for the last three; it takes the item out of its inventory, 0x4e9ca0). */
    if (is_pet && isa(item, 129)) {
        typedef void (__fastcall *use_by_fn)(void *ui, void *edx, void *level, void *item, void *user, void *target, void *pet);
        int known = 0, i;
        for (i = 0; i < 4; i++) {
            void *sp = 0;
            __try { sp = ((known_spell_fn)0x484ff0)(owner, 0, i); } __except (1) { sp = 0; }
            if (sp) known++;
        }
        if (usable_now(item, owner) && known < 2 && ui && level) {
            ((use_by_fn)0x0054d980)(ui, 0, level, item, owner, owner, owner);
            x360_log("x360 inv: A in the pet's bag: the pet learns the spell\r\n");
        } else if (known >= 2) {
            x360_log("x360 inv: A in the pet's bag: the pet knows two spells (the 360's replacing menu, not ported)\r\n");
        } else {
            /* The recomp's run (2026-10-28, a spell over the pet's level): no sound, text or
             * modal; only the item's card closes (the header's rebuild), until the row moves. */
            x360_log("x360 inv: A in the pet's bag: the pet cannot learn it\r\n");
            hide_info();
        }
        return;
    }
    if (is_pet && !((can_equip_fn)(*(void ***)item)[GOG_CAN_EQUIP_SLOT])(item, 0, owner, 1)) {
        /* The pet uses nothing and wears only what fits it (PC's canEquip for the pet). */
        x360_log("x360 inv: A in the pet's bag: the pet cannot wear it\r\n");
        return;
    }
    if (((item_bool_fn)GOG_IS_USEABLE)(item, 0)) {
        int portal = town_portal_scroll(item);
        if (!usable_now(item, owner)) { x360_log("x360 inv: A: not usable now (no Use on the 360)\r\n"); return; }
        if (ui) ((use_item_fn)GOG_USE_ITEM)(ui, 0, level, item);
        x360_log(ui ? "x360 inv: A: used\r\n" : "x360 inv: A: no CGameUI, not used\r\n");
        hide_info();   /* the 360's card closes after a use (in-game/29) */
        /* The 360 closes the bag after using the town portal scroll (sub_8236BD80: its menu's
         * Show(false), +0x1c with 0; in-game/27), and only then (a potion leaves it open, 29). */
        if (ui && portal && inventory_open) { x360_log("x360 inv: the town portal used: the bag closed\r\n"); x360_inventory_toggle(); }
        return;
    }
    if (((inv_item_fn)GOG_EQUIP_FIRST_FREE)(inv, 0, item)) {
        ((int_fn)GOG_RENDER_BEHIND)(owner, 0, 1);
        ((ptr_fn)GOG_DROP_SOUND)(item, 0, *(void **)(owner + 0x58));
        x360_log("x360 inv: A: equipped\r\n");
        return;
    }
    if (!((can_equip_fn)(*(void ***)item)[GOG_CAN_EQUIP_SLOT])(item, 0, owner, 1)) {
        x360_log("x360 inv: A: cannot equip\r\n");
        return;
    }
    {
        void *first = 0, *second = 0;
        ((comparison_fn)GOG_COMPARISON_ITEMS)(inv, 0, item, &first, &second);
        int equipped;
        if (!first && !(isa(item, 10) && second)) { x360_log("x360 inv: A: cannot equip\r\n"); return; }
        /* The old items out, the new one in; when it does not go in, the old ones back on. Then the
         * old items into the bag, or to the ground when it is full (as PC's swap drops them). */
        if (isa(item, 10) && second) move_out(inv, second, -1);
        if (first) move_out(inv, first, -1);
        equipped = ((inv_item_fn)GOG_EQUIP_FIRST_FREE)(inv, 0, item);
        if (!equipped) {
            /* Back on: nothing left for the bag below (one that does not go back goes there). */
            if (first && ((inv_item_fn)GOG_EQUIP_FIRST_FREE)(inv, 0, first)) first = 0;
            if (isa(item, 10) && second && ((inv_item_fn)GOG_EQUIP_FIRST_FREE)(inv, 0, second)) second = 0;
            x360_log("x360 inv: A: not equipped, the old items back on\r\n");
        } else {
            ((int_fn)GOG_RENDER_BEHIND)(owner, 0, 1);
            ((ptr_fn)GOG_DROP_SOUND)(item, 0, *(void **)(owner + 0x58));
        }
        if (first && !((pickup_fn)GOG_PICKUP_EQUIPMENT)(inv, 0, first, 1)) { to_ground(first); x360_log("x360 inv: A: the old item to the ground\r\n"); }
        if (isa(item, 10) && second && !((pickup_fn)GOG_PICKUP_EQUIPMENT)(inv, 0, second, 1)) {
            to_ground(second);
            x360_log("x360 inv: A: the old second item to the ground\r\n");
        }
        move_done();
        if (equipped) x360_log("x360 inv: A: swapped\r\n");
    }
}

#define GOG_GET_POSITION 0x00510eb0   /* CPositionableObject::getPosition(Vector3* out, bool) */
#define GOG_LEVEL_ADD_ITEM 0x004f5960 /* CLevel::addItem(CItem*, Vector3 const*, bool) */
#define GOG_ITEM_DROPPED_SLOT (0x198 / 4) /* CItem's virtual after addItem, as PC's drop does */
#define GOG_GET_PET 0x0040e720        /* the hero's pet by index (thiscall, int) */
typedef void *(__fastcall *get_position_fn)(void *self, void *edx, float *out, int flag);
typedef void (__fastcall *add_item_fn)(void *level, void *edx, void *item, const float *pos, int flag);
typedef void *(__fastcall *get_pet_fn)(void *hero, void *edx, int index);
static void shop_sound(int id);
static void hero_voice(int id);

/* Y: the item to the ground at the hero's feet, as PC drops an item that does not fit (GOG
 * 0x55e28a): removeEquipment, CLevel::addItem at the hero's position, the item's virtual +0x198. */
static void drop(void) {
    void *inv = inventory();
    unsigned char *item;
    float pos[3];
    if (!inv || !list_count) return;
    item = (unsigned char *)list[selected[filter]];
    /* An item of type 103 is not dropped (sub_82369E80: the 360's refusal, sub_82369E28). */
    if (isa(item, 103)) { shop_sound(24); hero_voice(49); return; }
    if (!level) return;
    move_out(inv, item, list_slot[selected[filter]]);
    ((get_position_fn)GOG_GET_POSITION)(hero, 0, pos, 1);
    ((add_item_fn)GOG_LEVEL_ADD_ITEM)(level, 0, item, pos, 1);
    move_done();
    ((void_fn)(*(void ***)item)[GOG_ITEM_DROPPED_SLOT])(item, 0);
    x360_log("x360 inv: Y: dropped\r\n");
}

/* X: the item to the pet's bag (the 360's "Swap" with the pet's column); back if it does not fit. */
static void swap_to_pet(void) {
    void *inv = inventory(), *pet, *pet_inv;
    unsigned char *item;
    if (!inv || !list_count) return;
    /* From the hero's bag to the pet's, or back from the pet's. */
    pet = panels[active_panel].is_pet ? (void *)hero : ((get_pet_fn)GOG_GET_PET)(hero, 0, 0);
    pet_inv = pet ? *(void **)((unsigned char *)pet + GOG_INVENTORY) : 0;
    if (!pet_inv) { x360_log("x360 inv: X: no pet\r\n"); return; }
    item = (unsigned char *)list[selected[filter]];
    move_out(inv, item, list_slot[selected[filter]]);
    if (((pickup_fn)GOG_PICKUP_EQUIPMENT)(pet_inv, 0, item, 1)) { move_done(); x360_log("x360 inv: X: to the pet\r\n"); return; }
    if (give_back(inv, list_slot[selected[filter]], item)) x360_log("x360 inv: X: the pet's bag is full\r\n");
    else move_lost(item);
    move_done();
}

/* ---- the merchant (CMerchantMenuXenon) ----
 * X in the merchant's column buys (sub_8236AD18 -> sub_8236B578), in a bag sells
 * (sub_8236B7B8), at once, with no question. Each step is done with PC's own function for it, as
 * PC's shop does it (CGameUI's slot click, GOG 0x54eeff and 0x54f003):
 * - buying: refused when the hero's gold is under the asked price or the item is of type 103 (the
 *   360's sub_82369E28: the sound 24 and the hero's voice 49); an endless stock item (a stack of 1
 *   with +0x1ff set; the 360's +0x1E4 and +0x1FF) is copied (GOG 0x5ff150, CGameUI's +0x1074, from
 *   the item's +0x178), another taken out of the merchant's bag; into the hero's bag, or, when it
 *   is full, back (the copy destroyed) with the voice 47; at the Gambler (ISA 127) identified (the
 *   360 sets +0x31C and remakes the text; here +0x2fc and destroyItemText), the journal's items
 *   gambled +1 (statistic 15, GOG 0x4d90f0) and, for type 54, the achievement 5 (GOG 0x5f9f90,
 *   0x5fa360, 0x5f9e70); the price taken (giveGold, GOG 0x487f20) and the sound 23;
 * - selling: refused for type 103; the sound 23, the paid price given, PC's items-sold count
 *   (GOG 0x4d90d0, which PC's sale keeps), the item out of its bag into the merchant's, or
 *   destroyed when that is full (the 360 too).
 * The sounds are CGameUI's (+0x13f8, GOG 0x5366f0) at the hero, the voices the hero's. */
#define GOG_GIVE_GOLD 0x00487f20
#define GOG_ENDLESS 0x1ff
#define GOG_ITEM_TEMPLATE 0x178
#define GOG_UI_ITEM_MAKER 0x1074
#define GOG_MAKE_ITEM 0x005ff150
#define GOG_COUNT_SOLD 0x004d90d0
#define GOG_COUNT_STAT 0x004d90f0
#define GOG_ACHIEVEMENTS 0x005f9f90
#define GOG_FIND_ACHIEVEMENT 0x005fa360
#define GOG_UNLOCK_ACHIEVEMENT 0x005f9e70
typedef void *(__fastcall *make_item_fn)(void *maker, void *edx, void *what, int a, int b, int c);
typedef void (__fastcall *count_stat_fn)(void *unit, void *edx, int stat, int n);
typedef void *(__cdecl *achievements_fn)(void);
typedef void *(__fastcall *find_achievement_fn)(void *self, void *edx, int id);
typedef void (__fastcall *destroy_fn)(void *self, void *edx, int flags);

static void shop_sound(int id) {
    void *sounds = ui ? *(void **)(ui + GOG_UI_SOUNDS) : 0;
    if (sounds) ((play_sound_at_fn)GOG_PLAY_SOUND_AT)(sounds, 0, id, *(void **)(hero + GOG_UNIT_NODE), 0.0f, 0.0f, 0);
}

static void hero_voice(int id) {
    void *emitter = *(void **)(hero + GOG_UNIT_EMITTER);
    if (emitter) ((play_sound_fn)GOG_PLAY_SOUND)(emitter, 0, id, 0.0f, *(float *)0x00a8ec04);
}

static void refuse(void) { shop_sound(24); hero_voice(49); }

static void buy(void) {
    unsigned char *stock = shop ? *(unsigned char **)(shop + GOG_INVENTORY) : 0;
    unsigned char *bag = *(unsigned char **)(hero + GOG_INVENTORY), *item, *got;
    int price;
    if (!stock || !bag || !list_count) return;
    item = (unsigned char *)list[selected[filter]];
    price = ((int_get_fn)GOG_ASKED_PRICE)(item, 0);
    if (*(int *)(hero + GOG_GOLD) < price || isa(item, 103)) { refuse(); x360_log("x360 shop: X: cannot buy\r\n"); return; }
    if (*(int *)(item + GOG_STACK) == 1 && item[GOG_ENDLESS])
        got = (unsigned char *)((make_item_fn)GOG_MAKE_ITEM)(*(void **)(ui + GOG_UI_ITEM_MAKER), 0,
                                                            *(void **)(item + GOG_ITEM_TEMPLATE), 0, 0, 0);
    else {
        got = item;
        move_out(stock, item, list_slot[selected[filter]]);
    }
    if (!got) return;
    if (!((pickup_fn)GOG_PICKUP_EQUIPMENT)(bag, 0, got, 1)) {
        if (got != item) ((destroy_fn)(*(void ***)got)[0])(got, 0, 1);
        else if (!give_back(stock, list_slot[selected[filter]], item)) move_lost(item);
        move_done();
        hero_voice(47);
        x360_log("x360 shop: X: the bag is full\r\n");
        {
            /* Which of the bag's sections the item goes to and how full it is (PC's 0x4e85b0 looks
             * for a free slot in the item's section only: 0x4e8410 its type, 0x4e84a0 the section,
             * the sections' first slots at +0x60; 0x4e7890 the item at a slot). QA run 20: weapons
             * refused with the bag at 23 of 63. */
            typedef int (__fastcall *of_item_fn)(void *self, void *edx, void *item);
            typedef int (__fastcall *of_int_fn)(void *self, void *edx, int v);
            typedef void *(__fastcall *at_fn)(void *self, void *edx, int slot);
            int type = ((of_item_fn)0x004e8410)(bag, 0, item);
            int sec = ((of_int_fn)0x004e84a0)(bag, 0, type);
            int *firsts = *(int **)(bag + 0x60), nsec = (int)((*(int **)(bag + 0x64)) - firsts);
            int first = sec >= 0 && sec < nsec ? firsts[sec] : -1, end = sec + 1 < nsec ? firsts[sec + 1] : -1, used = 0, sl;
            for (sl = first; first >= 0 && sl < end; sl++) if (((at_fn)0x004e7890)(bag, 0, sl)) used++;
            x360_log("x360 shop:   type "); x360_log_num(type); x360_log(" section "); x360_log_num(sec);
            x360_log(" of "); x360_log_num(nsec); x360_log(" slots "); x360_log_num(first); x360_log(".."); x360_log_num(end);
            x360_log(" used "); x360_log_num(used); x360_log("\r\n");
        }
        return;
    }
    move_done();
    if (isa(shop, 127)) {
        got[GOG_IDENTIFIED] = 1;
        ((void_fn)GOG_DESTROY_ITEM_TEXT)(got, 0);
        ((count_stat_fn)GOG_COUNT_STAT)(hero, 0, 15, 1);
        if (isa(got, 54)) {
            void *all = ((achievements_fn)GOG_ACHIEVEMENTS)();
            void *a = all ? ((find_achievement_fn)GOG_FIND_ACHIEVEMENT)(all, 0, 5) : 0;
            if (a) ((void_fn)GOG_UNLOCK_ACHIEVEMENT)(a, 0);
        }
    }
    ((int_fn)GOG_GIVE_GOLD)(hero, 0, -price);
    shop_sound(23);
    x360_log("x360 shop: X: bought\r\n");
}

static void sell(void) {
    unsigned char *stock = shop ? *(unsigned char **)(shop + GOG_INVENTORY) : 0, *item;
    void *inv = inventory();
    if (!stock || !inv || !list_count) return;
    item = (unsigned char *)list[selected[filter]];
    if (isa(item, 103)) { refuse(); x360_log("x360 shop: X: cannot sell\r\n"); return; }
    {
        /* The item into the merchant's bag (or destroyed when full, as the 360) before the gold: an
         * interrupted sale gives the item back, never the gold with it. */
        int paid = ((int_get_fn)GOG_PAID_PRICE)(item, 0);
        shop_sound(23);
        ((ptr_fn)GOG_COUNT_SOLD)(hero, 0, item);
        move_out(inv, item, list_slot[selected[filter]]);
        if (!((pickup_fn)GOG_PICKUP_EQUIPMENT)(stock, 0, item, 1)) ((destroy_fn)(*(void ***)item)[0])(item, 0, 1);
        move_done();
        ((int_fn)GOG_GIVE_GOLD)(hero, 0, paid);
    }
    x360_log("x360 shop: X: sold\r\n");
}

/* ---- the stash (CStashMenuXenon) ----
 * The 360's stash menu is of mode 7, the shared one's 8 (its +268, sub_82383DC8; mode 5 is the
 * transmuter's, 4 the merchant's): its bar is the plain one (sub_8236C8D0), Y drops
 * (sub_8236A810 -> sub_82369E80). X swaps (sub_8236AD18): from the stash's column the item
 * goes to the hero's bag, else the pet's, else to the ground at the hero's feet (sub_8236B8B0,
 * the sound 18 for a bag); from a bag into the stash (sub_8236BA20: the sound 18, or 24 and the
 * item kept when it does not fit); the stash is the hero's +2548 for mode 7, the game's for 8
 * (PC: the chest's +0x404, or the shared stash's). PC's pickupEquipment puts it in, as for the
 * pet's bag. */
static void stash_swap(void) {
    unsigned char *box = (unsigned char *)owner_bag(shop), *item, *pet = the_pet();
    void *inv = inventory();
    if (!box || !inv || !list_count) return;
    item = (unsigned char *)list[selected[filter]];
    move_out(inv, item, list_slot[selected[filter]]);
    if (panels[active_panel].is_shop) {
        if (((pickup_fn)GOG_PICKUP_EQUIPMENT)(owner_bag(hero), 0, item, 1) ||
            (pet && ((pickup_fn)GOG_PICKUP_EQUIPMENT)(owner_bag(pet), 0, item, 1))) {
            move_done();
            shop_sound(18);
            x360_log("x360 stash: X: taken\r\n");
            return;
        }
        to_ground(item);
        move_done();
        x360_log("x360 stash: X: taken, to the ground\r\n");
        return;
    }
    if (((pickup_fn)GOG_PICKUP_EQUIPMENT)(box, 0, item, 1)) { move_done(); shop_sound(18); x360_log("x360 stash: X: stored\r\n"); return; }
    if (!give_back(inv, list_slot[selected[filter]], item)) move_lost(item);
    move_done();
    shop_sound(24);
    x360_log("x360 stash: X: the stash is full\r\n");
}

/* ---- the transmuter (the 360's mode 5) ----
 * X in a bag puts the item into the first free of the transmuter's four places (sub_8236BA20:
 * the slots 15 to 18 of a column of type 134; the sound 18, or 24 when all are taken), here with
 * PC's own placing into a slot (GOG 0x4e9480, as its transmute puts its result back, 0x56a197);
 * X in the transmuter's column takes the item back as from a stash (sub_8236B8B0). Y in any
 * column transmutes (sub_8236A810 -> sub_82379058): PC's transmute, CCombineMenu's command 9
 * (its virtual +0x30, 0x56a490 -> 0x569550). */
/* The enchanter's: CEnchantMenu (CGameUI's +0x2dc, vtable 0xab6c0c, made at 0x562fb0), its owner
 * +0x2c, the character +0x30, open +0x38 (its virtual +0x10, 0x575950), its mode +0x90 (the
 * interaction CGameUI opened it for: 0x15 the enchanter, 0x557e23; the others its gem and
 * heirloom pages), opened with PC's inventory. Its command 9 (virtual +0x30, 0x57c900 ->
 * 0x57a800) does the mode's work on the item in the hero's slot 14 (0x57aea4): for 0x15 the
 * price checked and taken, the enchantment or the disenchant, PC's sounds. */
#define GOG_ENCHANT_MENU 0x2dc
#define ENCHANT_OWNER 0x2c
#define ENCHANT_OPEN 0x38
#define ENCHANT_MODE 0x90
#define ENCHANT_SLOT 14
#define GOG_ENCHANT_ACT 0x0057a800
#define GOG_COMBINE_MENU 0x2e0
#define GOG_ITEM_AT_SLOT 0x004e7890          /* CInventory: the item in a slot, or 0 */
#define GOG_PUT_AT_SLOT 0x004e9480           /* CInventory: the item into a slot (item, slot, 1) */
#define GOG_TRANSMUTE 0x00569550             /* CCombineMenu: transmute the items in its places */
typedef void *(__fastcall *item_at_fn)(void *inv, void *edx, int slot);
typedef void *(__fastcall *put_at_fn)(void *inv, void *edx, void *item, int slot, int flag);

static void move_out(void *inv, void *item, int slot) {
    int k = in_move[0].item ? 1 : 0;
    if (in_move[1].item) { move_cut_short(); k = 1; }   /* never: two at most */
    in_move[k].item = item; in_move[k].inv = inv; in_move[k].slot = slot;
    ((inv_item_fn)GOG_REMOVE_EQUIPMENT)(inv, 0, item);
}
/* The item back where it was: its slot when it is free, else anywhere in that bag. */
static int give_back(void *inv, int slot, void *item) {
    if (!inv || !item) return 0;
    if (slot >= 0 && !((item_at_fn)GOG_ITEM_AT_SLOT)(inv, 0, slot)) {
        ((put_at_fn)GOG_PUT_AT_SLOT)(inv, 0, item, slot, 1);
        if (((item_at_fn)GOG_ITEM_AT_SLOT)(inv, 0, slot) == item) return 1;
    }
    /* pickup's answer is the item or the stack it joined (a full merge frees the item: 0x4e9743). */
    return ((pickup_fn)GOG_PICKUP_EQUIPMENT)(inv, 0, item, 1) != 0;
}
static void to_ground(void *item) {
    float pos[3];
    if (!level || !hero) {   /* no level: back where it came from, if it can be */
        int k;
        for (k = 1; k >= 0; k--)
            if (in_move[k].item == item && give_back(in_move[k].inv, in_move[k].slot, item)) return;
        move_lost(item);
        return;
    }
    ((get_position_fn)GOG_GET_POSITION)(hero, 0, pos, 1);
    ((add_item_fn)GOG_LEVEL_ADD_ITEM)(level, 0, item, pos, 1);
    ((void_fn)(*(void ***)item)[GOG_ITEM_DROPPED_SLOT])(item, 0);
}
static void stop_unsaved(const char *why, int count);
/* Nowhere to put it: kept (never destroyed) and the game stopped before any save; loading the
 * character again has it where it was. */
static void move_lost(void *item) {
    x360_log("x360 inv: an item could not be put back after a move: ");
    if (item) log_item_name(item);
    x360_log("\r\n");
    stop_unsaved("An item could not be put back in a bag after a move. Nothing is saved, nothing is lost: press A "
                 "to close the game, then load this character again.", -1);
}
/* A move cut short (an exception, or a path that left it unfinished): the game stops before any
 * save, the item not touched. */
static void move_cut_short(void) {
    if (!in_move[0].item && !in_move[1].item) return;
    in_move[0].item = in_move[1].item = 0;
    x360_log("x360 inv: a move was cut short: the game stops, unsaved\r\n");
    stop_unsaved("A move between bags was interrupted. Nothing is saved, nothing is lost: press A to close the game, "
                 "then load this character again.", -1);
}

static void transmute_swap(void) {
    void *bag = owner_bag(hero);
    unsigned char *item;
    int j;
    if (!bag || !list_count || !tm_known) return;
    item = (unsigned char *)list[selected[filter]];
    if (panels[active_panel].is_shop) {           /* back from the transmuter */
        unsigned char *pet = the_pet();
        for (j = 0; j < 4; j++) if (tm_slot[j] == list_slot[selected[filter]]) tm_from[j] = -1;
        move_out(bag, item, list_slot[selected[filter]]);
        if (((pickup_fn)GOG_PICKUP_EQUIPMENT)(bag, 0, item, 1) ||
            (pet && ((pickup_fn)GOG_PICKUP_EQUIPMENT)(owner_bag(pet), 0, item, 1))) {
            move_done();
            shop_sound(18);
            x360_log("x360 transmuter: X: taken\r\n");
            return;
        }
        to_ground(item);
        move_done();
        x360_log("x360 transmuter: X: taken, to the ground\r\n");
        return;
    }
    for (j = 0; j < 4; j++) if (!((item_at_fn)GOG_ITEM_AT_SLOT)(bag, 0, tm_slot[j])) break;
    if (j == 4) { shop_sound(24); x360_log("x360 transmuter: X: its places are full\r\n"); return; }
    move_out(inventory(), item, list_slot[selected[filter]]);
    ((put_at_fn)GOG_PUT_AT_SLOT)(bag, 0, item, tm_slot[j], 1);
    if (((item_at_fn)GOG_ITEM_AT_SLOT)(bag, 0, tm_slot[j]) == item) { tm_from[j] = list_slot[selected[filter]]; tm_from_inv[j] = inventory(); }
    if (((item_at_fn)GOG_ITEM_AT_SLOT)(bag, 0, tm_slot[j]) != item) {
        /* Its place taken (or refused): the item back where it was, whatever is now there. */
        x360_log("x360 transmuter: X: not placed, back\r\n");
        if (!give_back(inventory(), list_slot[selected[filter]], item)) move_lost(item);
        move_done();
        return;
    }
    move_done();
    shop_sound(18);
    x360_log("x360 transmuter: X: placed\r\n");
}

/* The retirement's items (the 360's sub_8236B498: unit types 8 WEAPON, 13 ARMOR, 39 TRINKET, by
 * sub_821D7CE0; any other, nothing at all). */
static int can_retire(unsigned char *item) { return item && (isa(item, 8) || isa(item, 13) || isa(item, 39)); }
/* A at the retirement NPC: the 360 retires at once, no confirmation (sub_8236B498 -> sub_82361B00, mode 4:
 * the item out of the hero's bag, renamed after the hero the first time, its heirloom count up
 * and its bonuses, handed to the descendant, the hero marked retired, the menu closed and the game
 * left). PC's act does the same work on the item in the hero's slot 14 (CEnchantMenu's 0x57a800,
 * mode 0x1b at 0x57c42f: removeEquipment, the rename while +0x22c is 0, 0x4c0090, +0x2478 and
 * +0x247c, the hero's +0x9c4 set): the item is put there and the act called. RETIRING ENDS THE
 * CHARACTER: tested on copies only. */
#define X360_RETIRE_MSG 0x8036            /* WM_APP + 0x36: the retirement's act, run by keys_proc */
static unsigned char *retire_item;        /* the item put in slot 14 for the act */
static int retire_slot;
void x360_retire_act(void);
static void retire(void) {
    unsigned char *m = ui ? *(unsigned char **)(ui + GOG_ENCHANT_MENU) : 0, *item;
    void *bag = owner_bag(hero), *from = inventory();
    int slot;
    if (!m || !bag || !from || !list_count) return;
    if (!m[ENCHANT_OPEN] || hero[0x9c4]) { x360_log("x360 retire: A: the menu closed or the hero already retired: nothing\r\n"); return; }   /* never a second heirloom */
    item = (unsigned char *)list[selected[filter]];
    slot = list_slot[selected[filter]];
    if (!can_retire(item)) { x360_log("x360 retire: A: not a weapon, an armour or a trinket: nothing\r\n"); return; }
    if (from != bag) { x360_log("x360 retire: A: not the hero's bag: nothing\r\n"); return; }
    if (((item_at_fn)GOG_ITEM_AT_SLOT)(bag, 0, ENCHANT_SLOT)) { x360_log("x360 retire: A: slot 14 is taken\r\n"); return; }
    move_out(bag, item, slot);
    ((put_at_fn)GOG_PUT_AT_SLOT)(bag, 0, item, ENCHANT_SLOT, 1);
    if (((item_at_fn)GOG_ITEM_AT_SLOT)(bag, 0, ENCHANT_SLOT) != item) {
        if (!give_back(bag, slot, item)) move_lost(item);
        move_done();
        x360_log("x360 retire: A: not placed\r\n");
        return;
    }
    move_done();
    retire_item = item;
    retire_slot = slot;
    /* PC's Retire button runs its act from the window's messages, before the frame's update; run
     * from the frame listener, the act left PC between the descendant's creation and its
     * difficulty screen, which never came (the owner's runs: with pad=0, PC's own button, it came).
     * So the act is posted to the game's window and run from there (keys_proc), as the button's. */
    {
        __declspec(dllimport) int __stdcall PostMessageW(HWND w, unsigned msg, unsigned wparam, long lparam);
        extern HWND x360_keys_window(void);
        HWND w = x360_keys_window();
        if (w && PostMessageW(w, X360_RETIRE_MSG, 0, 0)) { x360_log("x360 retire: A: the act posted to the game's window\r\n"); return; }
    }
    x360_retire_act();
}
/* The retirement's act (from keys_proc as the window's messages are pumped; directly when there is
 * no window to post to): the item still in slot 14, the menu open, the hero not retired yet. */
void x360_retire_act(void) {
    unsigned char *m = ui ? *(unsigned char **)(ui + GOG_ENCHANT_MENU) : 0, *item = retire_item;
    void *bag = hero ? owner_bag(hero) : 0;
    retire_item = 0;
    if (!item || !m || !bag || !m[ENCHANT_OPEN] || hero[0x9c4] || ((item_at_fn)GOG_ITEM_AT_SLOT)(bag, 0, ENCHANT_SLOT) != item) {
        x360_log("x360 retire: the act not run (the menu, the hero or slot 14 changed)\r\n");
        if (item && bag && ((item_at_fn)GOG_ITEM_AT_SLOT)(bag, 0, ENCHANT_SLOT) == item) {
            move_out(bag, item, ENCHANT_SLOT);
            if (!give_back(bag, retire_slot, item)) move_lost(item);
            move_done();
        }
        return;
    }
    x360_log("x360 retire: the hero retired with "); log_item_name(item); x360_log("\r\n");
    ((void_fn)GOG_ENCHANT_ACT)(m, 0);
    /* PC's act takes the item out of slot 14 itself; one left there is put back. */
    if (((item_at_fn)GOG_ITEM_AT_SLOT)(bag, 0, ENCHANT_SLOT) == item) {
        move_out(bag, item, ENCHANT_SLOT);
        if (!give_back(bag, retire_slot, item)) move_lost(item);
        move_done();
        x360_log("x360 retire: PC's act left the item: put back, not retired\r\n");
    }
}

/* A at the enchanter: the selected item into the hero's slot 14, PC's own enchanting there
 * (CEnchantMenu's command 9), and back to its place (a bag's slot, an equipped one, the pet's). */
static void enchant(void) {
    unsigned char *m = ui ? *(unsigned char **)(ui + GOG_ENCHANT_MENU) : 0, *item, *got;
    void *bag = owner_bag(hero), *from = inventory();
    int slot;
    if (!m || !bag || !from || !list_count) return;
    item = (unsigned char *)list[selected[filter]];
    slot = list_slot[selected[filter]];
    if (at_enchant == 0x15 ? !can_enchant(item) : !has_gems(item)) { refuse(); x360_log("x360 enchanter: A: not for this item\r\n"); return; }
    if (((item_at_fn)GOG_ITEM_AT_SLOT)(bag, 0, ENCHANT_SLOT)) { x360_log("x360 enchanter: A: slot 14 is taken\r\n"); return; }
    move_out(from, item, slot);
    ((put_at_fn)GOG_PUT_AT_SLOT)(bag, 0, item, ENCHANT_SLOT, 1);
    if (((item_at_fn)GOG_ITEM_AT_SLOT)(bag, 0, ENCHANT_SLOT) != item) {
        if (!give_back(from, slot, item)) move_lost(item);
        move_done();
        x360_log("x360 enchanter: A: not placed\r\n");
        return;
    }
    move_done();
    ench_result[0] = 0;
    modal_hook_install();
    ench_acting = 1;
    ((void_fn)GOG_ENCHANT_ACT)(m, 0);
    ench_acting = 0;
    /* The 360 writes the result in the page, yellow, and the page stays open: the gem goblins' in
     * getsocketresult or destroysocketresult (in-game/14), the enchanter's in resultofenchant
     * (in-game/22-24). PC shows it in its modal (0x54cad0, called from the act: 0x57b6c9 and
     * others), which closes PC's menus a frame or two later (run B3: neither was done yet when the
     * act returned). So the frame tick waits for the modal (enchant_result_tick), up to 20 s. */
    ench_pending = GetTickCount();
    got = (unsigned char *)((item_at_fn)GOG_ITEM_AT_SLOT)(bag, 0, ENCHANT_SLOT);
    if (got) {
        move_out(bag, got, ENCHANT_SLOT);
        if (!give_back(from, slot, got) && !give_back(bag, ENCHANT_SLOT, got)) move_lost(got);
        move_done();
    }
    /* PC's act itself takes the item out of slot 14 and puts it back in the bag (0x4e9ca0 and
     * 0x4e9a30, at 0x57b619; dropped at the hero by 0x4f5960 only with the bag full). */
    x360_log(got ? "x360 enchanter: A: done, the item moved back from slot 14\r\n" : "x360 enchanter: A: done\r\n");
}

/* With no recipe for what is in its places, the 360's transmute (sub_82379058: its recipe
 * search's 0 sends it to 0x82379990) plays the sound 24 and redraws, no message: PC's does
 * nothing. Told here by its places unchanged after PC's (QA run 20). */
static void transmute(void) {
    unsigned char *m = ui ? *(unsigned char **)(ui + GOG_COMBINE_MENU) : 0;
    void *bag = hero ? owner_bag(hero) : 0, *was[4];
    int j, same = 1;
    if (!m) return;
    for (j = 0; j < 4; j++) was[j] = bag && tm_known ? ((item_at_fn)GOG_ITEM_AT_SLOT)(bag, 0, tm_slot[j]) : 0;
    ((void_fn)GOG_TRANSMUTE)(m, 0);
    for (j = 0; j < 4; j++) if ((bag && tm_known ? ((item_at_fn)GOG_ITEM_AT_SLOT)(bag, 0, tm_slot[j]) : 0) != was[j]) same = 0;
    if (same && bag && tm_known) { shop_sound(24); x360_log("x360 transmuter: Y: no recipe (the sound 24)\r\n"); return; }
    for (j = 0; j < 4; j++)   /* a changed place holds the result now, PC's own; an ingredient left keeps its origin */
        if ((bag && tm_known ? ((item_at_fn)GOG_ITEM_AT_SLOT)(bag, 0, tm_slot[j]) : 0) != was[j]) tm_from[j] = -1;
    x360_log("x360 transmuter: Y: transmuted\r\n");
}

/* The inventory, logged once per opening: the panes and every reference (a diagnosis). */
static void dump(void) {
    unsigned char *inv = (unsigned char *)inventory();
    void **refs;
    unsigned n, i;
    if (!inv) { x360_log("x360 inv: no inventory\r\n"); return; }
    refs = *(void ***)(inv + INV_REFS);
    n = *(unsigned *)(inv + INV_REFS + 4);
    x360_log("x360 inv: ui "); x360_log_num((long)ui);
    x360_log(" slots "); x360_log_num(*(long *)(inv + 0x18));
    x360_log(" pane starts");
    {
        int *b = *(int **)(inv + 0x60), *e = *(int **)(inv + 0x64);
        for (; b && b < e && b < *(int **)(inv + 0x60) + 16; b++) { x360_log(" "); x360_log_num(*b); }
    }
    x360_log(" refs "); x360_log_num((long)n); x360_log("\r\n");
    for (i = 0; i < n && i < 100; i++) {
        unsigned char *ref = (unsigned char *)refs[i], *item;
        char buf[128];
        if (!ref || !(item = *(unsigned char **)(ref + REF_ITEM))) continue;
        if (i == 0) probe_strings(item);
        x360_log("x360 inv:   slot "); x360_log_num(*(long *)(ref + REF_SLOT));
        x360_log(" cat "); x360_log_num(category(item));
        x360_log(" useable "); x360_log_num(((item_bool_fn)GOG_IS_USEABLE)(item, 0));
        x360_log(" identified "); x360_log_num(item[GOG_IDENTIFIED]);
        x360_log(" stack "); x360_log_num(*(long *)(item + GOG_STACK));
        x360_log(" price "); x360_log_num(((int_get_fn)GOG_PAID_PRICE)(item, 0));
        item_icon(item, buf, sizeof buf);
        x360_log(" icon \""); x360_log(buf); x360_log("\"\r\n");
    }
}

/* The layout's sample texts ("ggItemggNamegg", "99999", "3/25", "12423", "x12") never show: every
 * row's texts start empty and hidden, and the windows the 360 shows only in a mode start hidden. */
static void clear_samples(void) {
    static const char *const texts[] = {"SlotText", "DamageArmor", "Requirment", "Enchantments", "Quality",
                                        "Gold", "Equipped"};
    char name[48];
    unsigned row, i;
    for (row = 0; row < ROWS; row++) {
        for (i = 0; i < sizeof texts / sizeof texts[0]; i++) {
            row_name(name, texts[i], row);
            if (texts[i][0] != 'E' || texts[i][1] != 'q') text(win(name), "");   /* "Equipped" is a label */
            show(win(name), 0);
        }
        row_name(name, "ItemTray", row);
        show(win(name), 0);
    }
    text(win("playergoldtext"), "");
    text(win("inventorysize"), "");
    text(win("IDENTIFYTEXT"), "");
    show(win("IDENTIFYWIN"), 0);
    show(win("buyandsell"), 0);
    show(win("moveframe"), 0);
}

/* ---- the 3D doll (the 360's CharacterViewport) ----
 * PC's own inventory shows the hero in 3D: CInventoryMenu (CGameUI's +0x2cc, vtable 0xab73e0) keeps
 * the wardrobe's scene (+0x7b04) and camera ("WardrobeCam", +0x7b08), and on showing (its virtual
 * +0x1c, GOG 0x57da90) adds a viewport to the render window (+0x7b0c; RenderTarget's virtual +0x20,
 * addViewport(camera, Z 3, 0, 0, 1, 1)), kept at +0x7b10, sized with Viewport::setDimensions,
 * cleared black every frame, the camera's aspect set to the viewport's; on hiding it removes it
 * (the virtual +0x2c, removeViewport(3)) and zeroes +0x7b10 (its update, 0x580f80). The 360 does the
 * same with its own camera ("WardrobeCamXenon", sub_8233C534) and the CharacterViewport window's
 * rect (sub_8235AA28). Here PC's viewport is made and placed on the 360's window; the game draws
 * CEGUI after every viewport, so the column's texts and frames show over the doll, as on the 360. */
#define GOG_INVENTORY_MENU 0x2cc
#define GOG_INVENTORY_MENU_VTABLE 0x00ab73e0
#define INVMENU_CAMERA 0x7b08
#define INVMENU_TARGET 0x7b0c
#define INVMENU_VIEWPORT 0x7b10
#define IAT_VP_SET_DIMENSIONS 0x00a85fc4
#define IAT_VP_SET_BACKGROUND 0x00a85fd4
#define IAT_VP_SET_CLEAR 0x00a85fd8
#define IAT_VP_ACTUAL_WIDTH 0x00a85fe0
#define IAT_VP_ACTUAL_HEIGHT 0x00a85fdc
#define IAT_VP_SET_CAMERA 0x00a85f64
typedef struct { float top, bottom, left, right; } frect;   /* CEGUI 0.6's Rect */
typedef frect *(__fastcall *pixel_rect_fn)(const void *self, void *edx, frect *out);
typedef void *(__fastcall *add_viewport_fn)(void *target, void *edx, void *camera, int z, float l, float t, float w, float h);
typedef void (__fastcall *remove_viewport_fn)(void *target, void *edx, int z);
typedef void (__fastcall *set_dims_fn)(void *vp, void *edx, float l, float t, float w, float h);
typedef void (__fastcall *set_colour_fn)(void *vp, void *edx, const float *rgba);
typedef void (__fastcall *set_clear_fn)(void *vp, void *edx, int on, unsigned buffers);
typedef void (__fastcall *set_aspect_fn)(void *camera, void *edx, float ratio);

static unsigned char *pc_inventory_menu(void) {
    unsigned char *m = ui ? *(unsigned char **)(ui + GOG_INVENTORY_MENU) : 0;
    return m && *(unsigned *)m == GOG_INVENTORY_MENU_VTABLE ? m : 0;
}

#define DOLL_ZOOM_OUT 1.6f
/* The pet's: run B3 showed it about 2.2 times the 360's (in-game/19: the whole lynx, small, its feet
 * on the Stats header), its head under the header; a first value, to measure against 19. */
#define PET_DOLL_ZOOM_OUT 2.2f

/* tan and atan without a C library (the proxy links none), for angles under a right angle. */
static float tangent(float x) {
    float x2 = x * x, s = x, c = 1, t = x, u = 1;
    int i;
    for (i = 1; i < 8; i++) {
        t *= -x2 / ((2 * i) * (2 * i + 1)); s += t;
        u *= -x2 / ((2 * i - 1) * (2 * i)); c += u;
    }
    return s / c;
}

static float arctan(float x) {
    /* atan(x) = 2 atan(x / (1 + sqrt(1 + x^2))), halved until small, then its series. */
    float r = x, sum, t, r2;
    int i, halvings = 0;
    while ((r > 0.2f || r < -0.2f) && halvings < 8) {
        float q = 1 + r * r, root = q;
        for (i = 0; i < 12; i++) root = 0.5f * (root + q / root);
        r = r / (1 + root);
        halvings++;
    }
    r2 = r * r; sum = r; t = r;
    for (i = 1; i < 10; i++) { t *= -r2; sum += t / (2 * i + 1); }
    while (halvings-- > 0) sum *= 2;
    return sum;
}
typedef const float *(__fastcall *get_fovy_fn)(const void *frustum, void *edx);
typedef void (__fastcall *set_fovy_fn)(void *frustum, void *edx, const float *radian);
/* The pet's: PC's CPetMenu (CGameUI's +0x2d4, vtable 0xab7de0; made at GOG 0x5630c9) keeps its
 * own wardrobe the same way ("PetWardrobeCam", +0x7b28; its render window +0x7b2c; its viewport,
 * Z 4, at +0x7b30: its showing, GOG 0x5920c0). The 360 shows the focused bag's character in the
 * first column (sub_823702E0 calls sub_8235AA28 for the hero's column, +0x10c, and the pet's,
 * +0x110: the column shown, its character's model shown, the doll's viewport on its
 * CharacterViewport): with the pet's bag focused, the pet. */
#define GOG_PET_MENU 0x2d4
#define GOG_PET_MENU_VTABLE 0x00ab7de0
#define PETMENU_CAMERA 0x7b28
#define PETMENU_TARGET 0x7b2c
#define PETMENU_VIEWPORT 0x7b30

static unsigned char *pc_pet_menu(void) {
    unsigned char *m = ui ? *(unsigned char **)(ui + GOG_PET_MENU) : 0;
    return m && *(unsigned *)m == GOG_PET_MENU_VTABLE ? m : 0;
}

static float fovy_saved[2];
static int doll_shown = -1;                 /* -1 none, 0 the hero's, 1 the pet's */

/* The pet seen three-quarters as the 360's (in-game/19: the lynx side-on, its head to the right;
 * the 360's one wardrobe camera at (0, 1, 4.5) looking at (0, 1, 0), its pet's model turned).
 * PC's pet camera (CPetMenu's, made at GOG 0x5980e7) is at (0, 1, [0xaa75f4] = 3.5) looking at
 * (0, 1, 0), its pet facing it: the camera is turned PET_DOLL_YAW about that point while the
 * pet's sheet shows, and put back after. A first angle, to measure against 19. */
#define PET_DOLL_YAW (-55.0f * 3.14159265f / 180.0f)
static float x87_cos(float x) { float r; __asm__("fcos" : "=t"(r) : "0"(x)); return r; }
static float x87_sin2(float x) { float r; __asm__("fsin" : "=t"(r) : "0"(x)); return r; }
static void pet_camera_turn(void *camera, int turned) {
    typedef void (__fastcall *xyz_fn)(void *self, void *edx, float x, float y, float z);
    static xyz_fn set_pos, look_at;
    float d = *(float *)0x00aa75f4, a = turned ? PET_DOLL_YAW : 0.0f;
    if (!set_pos) {
        void *ogre = GetModuleHandleA("OgreMain.dll");
        set_pos = ogre ? (xyz_fn)GetProcAddress(ogre, "?setPosition@Camera@Ogre@@QAEXMMM@Z") : 0;
        look_at = ogre ? (xyz_fn)GetProcAddress(ogre, "?lookAt@Camera@Ogre@@QAEXMMM@Z") : 0;
    }
    if (!set_pos || !look_at || !camera || !(d > 0.5f && d < 50.0f)) return;
    set_pos(camera, 0, d * x87_sin2(a), 1.0f, d * x87_cos(a));
    look_at(camera, 0, 0.0f, 1.0f, 0.0f);
}

static void show_doll_of(int pet, int on) {
    static pixel_rect_fn rect_of;
    static get_fovy_fn fovy;
    static set_fovy_fn set_fovy;
    unsigned char *m = pet ? pc_pet_menu() : pc_inventory_menu();
    unsigned cam_at = pet ? PETMENU_CAMERA : INVMENU_CAMERA, target_at = pet ? PETMENU_TARGET : INVMENU_TARGET,
             vp_at = pet ? PETMENU_VIEWPORT : INVMENU_VIEWPORT;
    int z = pet ? 4 : 3;
    void *target, *camera, *vp;
    if (!fovy) {
        void *ogre = GetModuleHandleA("OgreMain.dll");
        fovy = ogre ? (get_fovy_fn)GetProcAddress(ogre, "?getFOVy@Frustum@Ogre@@UBEABVRadian@2@XZ") : 0;
        set_fovy = ogre ? (set_fovy_fn)GetProcAddress(ogre, "?setFOVy@Frustum@Ogre@@UAEXABVRadian@2@@Z") : 0;
    }
    if (!m) { x360_log(pet ? "x360 chr: no CPetMenu, no doll\r\n" : "x360 chr: no CInventoryMenu, no doll\r\n"); return; }
    target = *(void **)(m + target_at);
    camera = *(void **)(m + cam_at);
    vp = *(void **)(m + vp_at);
    if (!target || !camera) return;
    if (!on) {
        if (pet) pet_camera_turn(camera, 0);
        if (vp) { ((remove_viewport_fn)(*(void ***)target)[0x2c / 4])(target, 0, z); *(void **)(m + vp_at) = 0; }
        if (fovy_saved[pet] && set_fovy) { set_fovy(camera, 0, &fovy_saved[pet]); fovy_saved[pet] = 0; }
        return;
    }
    if (!rect_of) rect_of = (pixel_rect_fn)need("?getUnclippedPixelRect@Window@CEGUI@@QBE?AVRect@2@XZ");
    {
        void *view = win_of(CHR_PREFIX, "CharacterViewport");
        RECT r;
        HWND w = active_window();
        frect a;
        float W, H, rgba[4] = {0, 0, 0, 1};
        if (!view || !rect_of || !w || !GetClientRect(w, &r) || r.right <= r.left || r.bottom <= r.top) return;
        W = (float)(r.right - r.left);
        H = (float)(r.bottom - r.top);
        fresh_rects(win_of(CHR_PREFIX, "TopFrame"));
        rect_of(view, 0, &a);
        if (!vp) {
            vp = ((add_viewport_fn)(*(void ***)target)[0x20 / 4])(target, 0, camera, z, 0, 0, 1, 1);
            *(void **)(m + vp_at) = vp;
        }
        if (!vp) return;
        (*(set_dims_fn *)IAT_VP_SET_DIMENSIONS)(vp, 0, a.left / W, a.top / H, (a.right - a.left) / W, (a.bottom - a.top) / H);
        (*(set_colour_fn *)IAT_VP_SET_BACKGROUND)(vp, 0, rgba);
        (*(set_clear_fn *)IAT_VP_SET_CLEAR)(vp, 0, 1, 3);
        {
            int vw = (*(int_get_fn *)IAT_VP_ACTUAL_WIDTH)(vp, 0), vh = (*(int_get_fn *)IAT_VP_ACTUAL_HEIGHT)(vp, 0);
            if (vh > 0) ((set_aspect_fn)(*(void ***)camera)[0x138 / 4])(camera, 0, (float)vw / (float)vh);
        }
        (*(ptr_fn *)IAT_VP_SET_CAMERA)(vp, 0, camera);
        /* PC frames the doll for its own 166x192 viewport; in the 360's taller window it would fill
         * the height and lose its sides, so the camera's field of view is widened (tan of half
         * the angle times DOLL_ZOOM_OUT) while the screen is open, and given back on closing. */
        if (fovy && set_fovy && !fovy_saved[pet]) {
            fovy_saved[pet] = *fovy(camera, 0);
            {
                float wide = 2.0f * arctan((pet ? PET_DOLL_ZOOM_OUT : DOLL_ZOOM_OUT) * tangent(fovy_saved[pet] / 2.0f));
                set_fovy(camera, 0, &wide);
            }
        }
        if (pet) pet_camera_turn(camera, 1);
        x360_log(pet ? "x360 chr: pet doll viewport at " : "x360 chr: doll viewport at ");
        x360_log_num((long)a.left); x360_log(","); x360_log_num((long)a.top);
        x360_log(" "); x360_log_num((long)(a.right - a.left)); x360_log("x"); x360_log_num((long)(a.bottom - a.top)); x360_log("\r\n");
    }
}

/* The doll of the focused bag's character while the screen is open (on: -1 to close). */
static void show_doll(int on) {
    int want = on && !shop ? (panels[focus_panel].is_pet ? 1 : 0) : -1;   /* no doll at a merchant */
    if (want == doll_shown) return;
    if (doll_shown >= 0) show_doll_of(doll_shown, 0);
    if (want >= 0) show_doll_of(want, 1);
    doll_shown = want;
}

static void *hud_root, *pet_hud_root;
static int modal_shown;                   /* the 360's modal shows (no HUD under it, in-game/16) */
static unsigned overlay_closed_at;     /* when a tip, modal or NPC dialog of the 360's last closed */
static int qd_shown;                         /* the 360's NPC dialog is shown (its section, at the end) */
static int shop_closing;
static int is_ours(void *w) {
    typedef const void *(__fastcall *name_fn)(const void *self, void *edx);
    static name_fn gn;
    if (w) {
        /* Every window the port loads is named with an x360 prefix (the housing, the pet commands,
         * the gem select, the modal, the bars...: QA run 1, the pet commands hidden at once once
         * PC's interface was hidden every frame). */
        const char *nm;
        if (!gn) gn = (name_fn)need("?getName@Window@CEGUI@@QBEABVString@2@XZ");
        nm = gn ? c_str(gn(w, 0), 0) : 0;
        if (nm && nm[0] == 'x' && nm[1] == '3' && nm[2] == '6' && nm[3] == '0') return 1;
    }
    return w && (w == master_root || w == hud_root || w == pet_hud_root || w == inventory_root || w == panels[1].root || w == panels[2].root || w == ench_root || w == character_root || w == petchr_root ||
                 w == card_root(0) || w == card_root(1) || w == card_root(2));   /* the Info cards, PC's tooltips */
}

int x360_inventory_open(void) { return inventory_open; }

/* The pad's Back: the 360's inventory on or off. */
void x360_run_console(void);
void x360_pad_shutdown(void);   /* pad.c: the measurement's files written before a stop ends the process */

static int petcmd_open;
static void petcmd_show(int on);
static void sock_show(int on);
static void triggers_reset(void);
static void stats_load(void);
static void stats_commit(void);
static void skills_load(void);
static void quest_load(void);
static void *skills_root, *quest_root;
static void *skillmap_root;   /* the Skills page's mapping overlay, and whether it is open */
static int sk_map;
static void *stats_root, *journal_root;
static void hide_info(void);
static void update_info(void);

/* The master menu's pages, in the 360's order (LT to the left, RT to the right). */
static const char *const page_name[4] = {"page.inventory", "page.stats", "page.skills", "page.quest"};   /* x360_text keys */
static int page;

static void page_titles(void) {
    text(win_of(MASTER, "MenuName"), x360_text(!shop ? page_name[page] : at_enchant == 0x19 ? "title.destroygems" : at_enchant == 0x1a ? "title.recovergems" : at_enchant == 0x1b ? "title.retire" : at_enchant ? "title.enchanting" : at_transmute ? "title.transmuter" : !at_stash ? "title.merchant"
                                               : isa(shop, 170) ? "title.sharedstash" : "title.stash"));
    text(win_of(MASTER, "MenuLeft"), x360_text(page_name[(page + 3) % 4]));
    text(win_of(MASTER, "MenuRight"), x360_text(page_name[(page + 1) % 4]));
    /* At a merchant the frame has one page, no pages beside it and no pet commands (the
     * catalog's town/01-20: the title title.merchant, the Gambler's too). */
    show(win_of(MASTER, "MenuLeft"), !shop);
    show(win_of(MASTER, "MenuRight"), !shop);
    show(win_of(MASTER, "lefttrigger"), !shop);
    show(win_of(MASTER, "righttrigger"), !shop);
    /* "pet commands" (Start) and the item compare's toggle on the inventory's page only (the
     * catalog: menus/08 against 10). */
    show(win_of(MASTER, "PetCommands"), page == 0 && !shop);
    show(win_of(MASTER, "ToggleItemCompare"), page == 0);
}

/* The frame's layouts, loaded once: at the first opening, or before it when the game is entered
 * (x360_frame_tick, the 360's HUD shown): run B2 timed the frame's layouts at 394 ms at the first
 * opening, plus the pages'. */
static void plain_markup(void *w, int depth);
static void frame_load(void) {
    {
        RECT r;
        HWND w = active_window();
        if (resolve() && w && GetClientRect(w, &r) && r.bottom > r.top)
            match_spacing((float)(r.right - r.left), (float)(r.bottom - r.top));
    }
    if (!inventory_root && !inventory_failed) {
        unsigned t0 = GetTickCount();
        master_root = load("media/ui/x360/mastermenu_xenon.layout", MASTER);
        inventory_root = master_root ? load("media/ui/x360/inventorymenu_xenon.layout", PREFIX) : 0;
        panels[0].root = inventory_root;
        /* The pet's bag: the same layout again, in the frame's third column. */
        panels[1].root = inventory_root ? load("media/ui/x360/inventorymenu_xenon.layout", PET_PREFIX) : 0;
        /* The merchant's column at a shop: the same layout, in the character's column. */
        panels[2].root = inventory_root ? load("media/ui/x360/inventorymenu_xenon.layout", SHOP_PREFIX) : 0;
        /* The enchanter's panel in the same place (town/06: its column at the frame's left). */
        ench_root = inventory_root ? load("media/ui/x360/enchantermenu_xenon.layout", ENCH_PREFIX) : 0;
        character_root = inventory_root ? load("media/ui/x360/charactermenu_xenon.layout", CHR_PREFIX) : 0;
        /* The pet's sheet in the same column (the 360's +0x110 beside the hero's +0x10C). */
        petchr_root = inventory_root ? load("media/ui/x360/petmenu_xenon.layout", PETCHR_PREFIX) : 0;
        x360_log("x360 menu: the frame's layouts ms "); x360_log_num((int)(GetTickCount() - t0)); x360_log("\r\n");
        if (!inventory_root) inventory_failed = 1;
        else {
            RECT r;
            HWND w = active_window();
            void *mf;
            char value[128];
            float v[8];
            /* One factor, by height: the 360's fonts and imagesets are AutoScaled from 1024x768
             * (as PC's), so the fonts grow by height / 768; the layouts' pixel offsets with them
             * (the 360's screenshots: its 919-pixel frame is 1290 wide at 1080). */
            k = (w && GetClientRect(w, &r) && r.bottom > r.top) ? (float)(r.bottom - r.top) / 768.0f : 1.0f;
            scale_tree(master_root, k, 0);
            scale_tree(inventory_root, k, 0);
            if (panels[1].root) scale_tree(panels[1].root, k, 0);
            if (panels[2].root) scale_tree(panels[2].root, k, 0);
            if (ench_root) scale_tree(ench_root, k, 0);
            if (character_root) scale_tree(character_root, k, 0);
            if (character_root) unclip_tree(character_root, 0);
            if (petchr_root) scale_tree(petchr_root, k, 0);
            if (petchr_root) unclip_tree(petchr_root, 0);
            if (petchr_root) plain_markup(petchr_root, 0);   /* its "Pet" line is the 360's markup */
            /* The pet's name is white on the 360 (in-game/19): its layout's sample "|cFFB29560Name|u"
             * left gold by plain_markup, the 360's code writing the name plain. */
            if (petchr_root) prop(win_of(PETCHR_PREFIX, "characterName"), "TextColour", "FFFFFFFF");
            /* The 360's tabbed frame (mastermenu_xenon, centred) holds the screens in its
             * ContainerFrame, three columns: the character, the bag, the pet's bag. The 360's dump
             * of the screen (the catalog's menus/08-inventory) has the bag's TopFrame 299 (base
             * units, 280.313 at 720) from the frame's left and the pet's 602 (564.375). */
            {
                void *container = win_of(MASTER, "ContainerFrame"), *top = win("TopFrame");
                if (container && top) {
                    add_child(container, 0, top);
                    move_by(top, 299.0f * k, 0);
                }
                top = character_root ? win_of(CHR_PREFIX, "TopFrame") : 0;
                if (container && top) add_child(container, 0, top);
                top = petchr_root ? win_of(PETCHR_PREFIX, "TopFrame") : 0;
                if (container && top) add_child(container, 0, top);
                top = panels[1].root ? win_of(PET_PREFIX, "TopFrame") : 0;
                if (container && top) {
                    add_child(container, 0, top);
                    move_by(top, 602.0f * k, 0);
                }
                /* The merchant's at the frame's left, where the 360 shows it (town/01: x 325
                 * at 1080, the character's column). */
                top = panels[2].root ? win_of(SHOP_PREFIX, "TopFrame") : 0;
                if (container && top) add_child(container, 0, top);
                top = ench_root ? win_of(ENCH_PREFIX, "i_2631") : 0;
                if (container && top) add_child(container, 0, top);
            }
            /* The pages' names (the 360's strings, its image near 0x820d17d0): the page in the
             * middle, the one LT goes to on the left, RT's on the right (the catalog's menus/08 to
             * 12: Inventory, Stats, Skills, Quest, round). */
            page_titles();
            mf = win("moveframe");
            read_prop(mf, "UnifiedAreaRect", value, sizeof value);
            if (parse_rect(value, v)) moveframe_top = v[3];
            clear_samples();
            if (panels[1].root) { select_panel(1); clear_samples(); select_panel(0); }
            if (panels[2].root) { select_panel(2); clear_samples(); select_panel(0); }
            x360_log("x360 menu: scaled by x1000 "); x360_log_num((long)(k * 1000)); x360_log("\r\n");
        }
    }
    if (!inventory_root) return;
    {
        unsigned t0 = GetTickCount();
        int first = !stats_root;
        stats_load();
        skills_load();
        quest_load();
        if (first) { x360_log("x360 menu: the pages' layouts ms "); x360_log_num((int)(GetTickCount() - t0)); x360_log("\r\n"); }
    }
    /* Loaded before the first opening: kept hidden until it (the opening sets each one's). */
    if (!inventory_open) {
        void *const roots[] = {master_root, inventory_root, panels[1].root, panels[2].root, ench_root,
                               character_root, petchr_root, stats_root, journal_root, skills_root, quest_root};
        unsigned i;
        for (i = 0; i < sizeof roots / sizeof roots[0]; i++) if (roots[i]) set_visible(roots[i], 0, 0);
    }
}

static void shop_close(void);
static void set_page(int p);
/* The world frozen while the 360's inventory is open, as on the 360 (the owner, 2026-10-25: it
 * stops as with Start; the port's kept going). The client's own pause byte (+0x1017, written only
 * by its constructor, GOG 0x40fcba) is read first by its getIsPaused *(OT)* (GOG 0x40de90), which
 * its update asks before moving the world; set here while the inventory is open, cleared after
 * only if set here. Not CGameUI's +0x173d (PC's pause menu's togglePause *(OT)*): with it the
 * camera kept turning behind the inventory (the owner's run), PC's pause showing its world so. */
#define GOG_CLIENT_PAUSED 0x1017
static int inventory_paused;
static void inventory_pause(int on) {
    if (!the_client) return;
    if (on && !the_client[GOG_CLIENT_PAUSED]) { the_client[GOG_CLIENT_PAUSED] = 1; inventory_paused = 1; x360_log("x360 menu: the world paused under the inventory\r\n"); }
    else if (!on && inventory_paused) { the_client[GOG_CLIENT_PAUSED] = 0; inventory_paused = 0; }
}

void x360_inventory_toggle(void) {
    if (!find_world()) return;
    if (inventory_open && shop && !shop_closing) { shop_close(); return; }
    {
        /* Run B: the first opening was slow; its two parts timed (the console, the layouts). */
        unsigned t0 = GetTickCount();
        x360_run_console();   /* test runs: console= commands, once, at the first opening (the level is loaded) */
        if (GetTickCount() - t0 > 50) { x360_log("x360 menu: console ms "); x360_log_num((int)(GetTickCount() - t0)); x360_log("\r\n"); }
    }
    frame_load();
    if (!inventory_root) return;
    inventory_open = !inventory_open;
    /* Not at a shop: the enchanter's and gem goblins' results come after PC's 3D sign has dropped,
     * which a paused world may not animate (the review); the 360's own inventory, the one paused. */
    __try { inventory_pause(inventory_open && !shop); } __except (1) { inventory_paused = 0; }
    if (inventory_open) {
        /* At a merchant: its page, the merchant's column focused (town/01-20). */
        /* At the transmuter the bag is focused (town/05). */
        if (shop) { if (page) set_page(0); focus_panel = at_transmute || at_enchant ? 0 : 2; }
        else if (focus_panel == 2) focus_panel = 0;
        select_panel(0);
        dump();
        refresh_all();
        triggers_reset();
    } else if (page == 1) stats_commit();
    set_visible(master_root, 0, inventory_open);
    set_visible(inventory_root, 0, inventory_open);
    if (stats_root) set_visible(stats_root, 0, inventory_open && page == 1);
    if (journal_root) set_visible(journal_root, 0, inventory_open && page == 1);
    if (skills_root) set_visible(skills_root, 0, inventory_open && page == 2);
    if (sk_map && !(inventory_open && page == 2)) { sk_map = 0; show(skillmap_root, 0); }
    if (quest_root) set_visible(quest_root, 0, inventory_open && page == 3);
    if (panels[1].root) set_visible(panels[1].root, 0, inventory_open);
    /* The columns' places by the menu: the inventory's 299 and 602; a stash's 0, 303 and 606
     * (sub_823832B8, its constants at 0x820aa8bc; the catalog's town/21: 752 and 1178 at 1080); the
     * merchant's and the transmuter's -1, 300 and 603 (sub_823788D8: 0x820aa334, 0x820aa5d4,
     * 0x820aa87c; town/05 and 07: 748 and 1174). */
    {
        static float shifted, shop_shifted;
        float want = !inventory_open || !shop ? 0.0f : at_stash ? 4.0f : 1.0f;
        float want_shop = inventory_open && shop && !at_stash ? -1.0f : 0.0f;
        if (want != shifted) {
            float dx = (want - shifted) * k;
            move_by(win_of(PREFIX, "TopFrame"), dx, 0);
            if (panels[1].root) move_by(win_of(PET_PREFIX, "TopFrame"), dx, 0);
            shifted = want;
        }
        if (want_shop != shop_shifted && panels[2].root) {
            move_by(win_of(SHOP_PREFIX, "TopFrame"), (want_shop - shop_shifted) * k, 0);
            shop_shifted = want_shop;
        }
    }
    if (panels[2].root) set_visible(panels[2].root, 0, inventory_open && shop && !at_enchant);
    if (panels[2].root) show(win_of(SHOP_PREFIX, "TopFrame"), inventory_open && shop && !at_enchant);
    if (ench_root) set_visible(ench_root, 0, inventory_open && at_enchant);
    if (ench_root) show(win_of(ENCH_PREFIX, "i_2631"), inventory_open && at_enchant);
    /* Its first showing kept its frame's area from before it was moved into the ContainerFrame
     * (the stash's first opening: HiliteFrame drawn at the screen's corner, its texts clipped);
     * moving the column there and back has CEGUI work it out again. */
    if (panels[2].root && inventory_open && shop && !at_enchant) {
        move_by(win_of(SHOP_PREFIX, "TopFrame"), 1.0f, 0);
        move_by(win_of(SHOP_PREFIX, "TopFrame"), -1.0f, 0);
    }
    if (character_root) set_visible(character_root, 0, inventory_open);
    if (petchr_root) set_visible(petchr_root, 0, inventory_open);
    chr_show(page < 2 && !shop);
    page_titles();
    if (inventory_open) move_to_front(master_root, 0);
    /* At a shop's close PC's sheets come back 600 ms later: PC's merchant and inventory menus close
     * for about 330 ms (diag-shop-close), and shown at once they were seen sliding away at the
     * sides (the owner's runs of 2026-10-24). */
    if (!inventory_open && shop) pc_ui_back_at = (GetTickCount() + 600) | 1;
    else { pc_ui_back_at = 0; hide_pc_ui(inventory_open); }
    if (character_root) show_doll(inventory_open && !shop);
    if (!inventory_open) { hide_info(); if (petcmd_open) petcmd_show(0); if (sock_open) sock_show(0); }
    /* The 360's inventory asks for TIP_INVENTORY (index 0 in both games' CONTEXTTIPS) when it
     * opens (CInventoryMenuXenon's virtual +28, sub_8236F700 -> sub_82337A10(0): the hero's seen
     * byte +2471 and the setting, as PC's 0x55f570). PC asks for it when its own inventory opens
     * (0x57de89), which it does at a shop: there it is left to PC. */
    if (inventory_open && !shop && ui) ((request_tip_fn)GOG_REQUEST_TIP)(ui, 0, 0);
    x360_log(inventory_open ? "x360 menu: inventory shown\r\n" : "x360 menu: inventory hidden\r\n");
}

/* ---- the Stats page (RT from the inventory: the catalog's menus/10) ----
 * The 360's CStatsMenuXenon (vtable 0x8200145c) shows, beside the character's column,
 * statsmenu_xenon at 299 and journalmenu_xenon at 602 (sub_82385738, its constants at
 * 0x820aa5e0). Its windows are found through the table at 0x833fa020 (sub_82384338) and filled by
 * sub_82384738, with the points not yet spent added for the reading (sub_82383ff8) and taken off
 * again after it (sub_82384118):
 * - strPts .. defPts: the stat (GOG's getters 0x487ec0, 0x487e60, 0x487d70, 0x487d00); its colour
 *   by the stat against its base (+0x3ac, +0x3a8, +0x3b4, +0x3b0): FFc0c0ff above, FFFF0000
 *   below, FFFFFFFF even (sub_82384238);
 * - strDMG, dexDMG: "min-max" of the damage (GOG 0x499e10 and 0x492f60 with (1,0,0), (0,0,0); the
 *   colour by the maximum against (.., 1)); magicDMG: the lower non-zero minimum and the higher
 *   maximum of (0,1,0) and (1,1,0), against (0,1,1), (1,1,1);
 * - defArmor: "half-armor" (GOG 0x4901b0, 0x4900d0), against the base armor (0x490150);
 * - ptsToSpend: the points left; the resistances (GOG 0x48ffe0: poison 5, fire 2, ice 3,
 *   electric 4); strPtsAdded ..: "+" and the points added, shown when any;
 * - addPoint at 1 while points are left, else 0.25; removePoint at 1 while the stat has points
 *   added; spendPoints and the stat's help shown while either.
 * Its input (sub_82385E48): up and down the stat (0 to 3, sub_82385CC0: the window's
 * statwindow_base_rollover, pieceToMove kept at its offset from it, sub_823845C8); A adds a point
 * when one is left, Y takes one added back (sub_82385BF0). The points are spent when the page is
 * left (sub_82385738: sub_82383ff8, here PC's own +1 functions: GOG 0x488450, 0x488470, 0x4884b0,
 * 0x488490; their -1 at 0x4884d0, 0x4884f0, 0x488530, 0x488510). */
#define STATS_PREFIX "x360st_"
#define JOURNAL_PREFIX "x360jr_"
#define GOG_STAT_POINTS 0x3dc
typedef int (__fastcall *int_arg3_fn)(void *self, void *edx, int a, int b, int c);
typedef int (__fastcall *int_arg1_fn)(void *self, void *edx, int a);
typedef void (__fastcall *unit_void_fn)(void *self, void *edx);
/* The hero's stat functions (0 get, 1 add a point, 2 take one back), by stat: computed, not a
 * static table, so that Steam's build can move them with the executable (x360_reloc). */
static unsigned stat_fn(int kind, int i) {
    const unsigned get[4] = {0x487ec0, 0x487e60, 0x487d70, 0x487d00};
    const unsigned inc[4] = {0x488450, 0x488470, 0x4884b0, 0x488490};
    const unsigned dec[4] = {0x4884d0, 0x4884f0, 0x488530, 0x488510};
    return kind == 0 ? get[i] : kind == 1 ? inc[i] : dec[i];
}
static const unsigned stat_base[4] = {0x3ac, 0x3a8, 0x3b4, 0x3b0};
static const char *const stat_window[4] = {"strength", "dexterity", "magic", "defense"};
static const char *const stat_pts[4] = {"strPts", "dexPts", "magicPts", "defPts"};
static const char *const stat_added_window[4] = {"strPtsAdded", "dexPtsAdded", "magicPtsAdded", "defPtsAdded"};
static const char *const stat_help[4] = {"StrHelp", "DexHelp", "MagicHelp", "DefHelp"};
static int stat_sel, stat_added[4];
static float piece_dx, piece_dy;

static void stat_text(const char *name, int value, int diff) {
    char buf[16], *p = put_int(buf, value);
    *p = 0;
    text(win_of(STATS_PREFIX, name), buf);
    prop(win_of(STATS_PREFIX, name), "TextColour", diff > 0 ? "FFc0c0ff" : diff < 0 ? "FFFF0000" : "FFFFFFFF");
}

static void range_text(const char *name, int low, int high, int diff) {
    char buf[32], *p = put_int(buf, low);
    *p++ = '-';
    p = put_int(p, high);
    *p = 0;
    text(win_of(STATS_PREFIX, name), buf);
    prop(win_of(STATS_PREFIX, name), "TextColour", diff > 0 ? "FFc0c0ff" : diff < 0 ? "FFFF0000" : "FFFFFFFF");
}

static void stats_fill(void) {
    static const int resist_type[4] = {5, 2, 3, 4};
    static const char *const resist_window[4] = {"poisonRes", "fireRes", "iceRes", "eletricRes"};
    int i, n, remaining, can_add, can_remove, a, b, low, high, base_high;
    char buf[16], *p;
    if (!hero || !stats_root) return;
    for (i = 0; i < 4; i++) for (n = 0; n < stat_added[i]; n++) ((unit_void_fn)stat_fn(1, i))(hero, 0);
    for (i = 0; i < 4; i++) {
        int v = ((int_get_fn)stat_fn(0, i))(hero, 0);
        stat_text(stat_pts[i], v, v - *(int *)(hero + stat_base[i]));
    }
    for (i = 0; i < 2; i++) {
        int melee = i == 0;   /* strength: (1,0,x); dexterity: (0,0,x) */
        low = ((int_arg3_fn)0x499e10)(hero, 0, melee, 0, 0);
        high = ((int_arg3_fn)0x492f60)(hero, 0, melee, 0, 0);
        base_high = ((int_arg3_fn)0x492f60)(hero, 0, melee, 0, 1);
        range_text(i == 0 ? "strDMG" : "dexDMG", low, high, high - base_high);
    }
    a = ((int_arg3_fn)0x499e10)(hero, 0, 0, 1, 0);
    b = ((int_arg3_fn)0x499e10)(hero, 0, 1, 1, 0);
    low = !a ? b : !b ? a : a < b ? a : b;
    a = ((int_arg3_fn)0x492f60)(hero, 0, 0, 1, 0);
    b = ((int_arg3_fn)0x492f60)(hero, 0, 1, 1, 0);
    high = a > b ? a : b;
    a = ((int_arg3_fn)0x492f60)(hero, 0, 0, 1, 1);
    b = ((int_arg3_fn)0x492f60)(hero, 0, 1, 1, 1);
    base_high = a > b ? a : b;
    range_text("magicDMG", low, high, high - base_high);
    high = ((int_get_fn)0x4900d0)(hero, 0);
    range_text("defArmor", ((int_get_fn)0x4901b0)(hero, 0), high, high - ((int_get_fn)0x490150)(hero, 0));
    remaining = *(int *)(hero + GOG_STAT_POINTS);
    stat_text("ptsToSpend", remaining, 0);
    for (i = 0; i < 4; i++) stat_text(resist_window[i], ((int_arg1_fn)0x48ffe0)(hero, 0, resist_type[i]), 0);
    for (i = 0; i < 4; i++) {
        buf[0] = '+';
        p = put_int(buf + 1, stat_added[i]);
        *p = 0;
        text(win_of(STATS_PREFIX, stat_added_window[i]), buf);
        prop(win_of(STATS_PREFIX, stat_added_window[i]), "TextColour", "FFFFFFFF");
        show(win_of(STATS_PREFIX, stat_added_window[i]), stat_added[i] > 0);
    }
    can_add = remaining > 0;
    can_remove = stat_added[stat_sel] != 0;
    prop(win_of(STATS_PREFIX, "addPoint"), "Alpha", can_add ? "1" : "0.25");
    prop(win_of(STATS_PREFIX, "removePoint"), "Alpha", can_remove ? "1" : "0.25");
    show(win_of(STATS_PREFIX, "spendPoints"), can_add || can_remove);
    for (i = 0; i < 4; i++) show(win_of(STATS_PREFIX, stat_help[i]), i == stat_sel && (can_add || can_remove));
    for (i = 0; i < 4; i++) for (n = 0; n < stat_added[i]; n++) ((unit_void_fn)stat_fn(2, i))(hero, 0);
}

/* ---- the journal (the Stats page's third column) ----
 * The 360's CJournalXenon (vtable 0x820d11a4; its fill sub_82370628) makes two StaticTexts a line
 * in journalwindow, both 270 x 23 at 24 a line (768 rows; the catalog: {{0,0},{0,22.5 i},
 * {0,253.125},{0,22.5 i + 21.5625}} at 720), the label LeftAligned in FFFFFFFF and the value
 * RightAligned in FFFFE03D, both texts in "|cFFC8C8C8 .. |u" (so both drawn C8C8C8), AlwaysOnTop,
 * TopAligned: the
 * player's statistics (the 360's unit +0x720 + 4 i; GOG's +0x740, PC's own journal at
 * 0x586c00 reads them in the same order with the same labels), the first the time played as
 * "0hrs 44mins 34secs" (PC's: the float at +0x2c8, its seconds). */
#define GOG_STATISTICS 0x740
#define GOG_TIME_PLAYED 0x2c8


static void *create_window(const char *prefix, const char *name, const char *type);
static void *create_static(const char *prefix, const char *name) { return create_window(prefix, name, "ceguiWidget/StaticText"); }

static void *create_window(const char *prefix, const char *name, const char *type) {
    typedef void *(__fastcall *create_fn)(void *wm, void *edx, const object *type, const object *name, const object *prefix);
    static create_fn create;
    object t, n, pre;
    void *w = 0, *wm = window_manager();
    if (!create) create = (create_fn)need("?createWindow@WindowManager@CEGUI@@QAEPAVWindow@2@ABVString@2@00@Z");
    if (!create || !wm) return 0;
    string_ctor(&t, 0, type);
    string_ctor(&n, 0, name);
    string_ctor(&pre, 0, prefix);
    __try { w = create(wm, 0, &t, &n, &pre); } __except (1) { w = 0; }
    string_dtor(&pre, 0); string_dtor(&n, 0); string_dtor(&t, 0);
    return w;
}

static void journal_fill(void) {
    void *box = win_of(JOURNAL_PREFIX, "journalwindow");
    char name[32], value[160], *p;
    int i;
    if (!box || !hero) return;
    for (i = 0; i < 18; i++) {
        void *label, *val;
        copy(name, sizeof name, "jl_", 0); p = put_int(name + 3, i); *p = 0;
        label = win_of(JOURNAL_PREFIX, name);
        if (!label && (label = create_static(JOURNAL_PREFIX, name)) != 0) {
            add_child(box, 0, label);
            set_size(label, 0, 24.0f * k * (float)i, 270.0f * k, 23.0f * k);
            prop(label, "Font", "X360_Serif");
            prop(label, "AlwaysOnTop", "True");
            prop(label, "VertFormatting", "TopAligned");
            prop(label, "HorzTextFormatting", "LeftAligned");
            prop(label, "TextColour", "FFC8C8C8");
        }
        name[1] = 'v';
        val = win_of(JOURNAL_PREFIX, name);
        if (!val && (val = create_static(JOURNAL_PREFIX, name)) != 0) {
            add_child(box, 0, val);
            set_size(val, 0, 24.0f * k * (float)i, 270.0f * k, 23.0f * k);
            prop(val, "Font", "X360_Serif");
            prop(val, "AlwaysOnTop", "True");
            prop(val, "VertFormatting", "TopAligned");
            prop(val, "HorzTextFormatting", "RightAligned");
            prop(val, "TextColour", "FFC8C8C8");
        }
        /* The 360's texts are "|cFFC8C8C8" .. "|u", its CEGUI's colour markup, which PC's CEGUI
         * writes as it is: the text plain and the colour as the TextColour instead. */
        {   /* the 360's label (its key journal.N) and a colon */
            char key[16];
            copy(key, sizeof key, "journal.", 0);
            p = put_int(key + 8, i); *p = 0;
            copy(value, sizeof value, x360_text(key), ":");
            text(label, value);
        }
        value[0] = 0;
        p = value;
        if (i == 0) {
            long t = (long)*(float *)(hero + GOG_TIME_PLAYED);
            p = put_int(p, t / 3600); copy(p, 16, x360_text("time.hrs"), " "); p += length(p);
            p = put_int(p, t / 60 % 60); copy(p, 16, x360_text("time.mins"), " "); p += length(p);
            p = put_int(p, t % 60); copy(p, 16, x360_text("time.secs"), 0); p += length(p);
        } else {
            p = put_int(p, *(int *)(hero + GOG_STATISTICS + 4 * i));
        }
        *p = 0;
        text(val, value);
    }
}

/* The selected stat: its window's rollover image, pieceToMove at its offset from it. */
static void stats_select(void) {
    int i;
    char value[128];
    float v[8];
    for (i = 0; i < 4; i++)
        prop(win_of(STATS_PREFIX, stat_window[i]), "Image", i == stat_sel ? "set:X360_widgets1 image:statwindow_base_rollover"
                                                                           : "set:X360_widgets1 image:statwindow_base");
    read_prop(win_of(STATS_PREFIX, stat_window[stat_sel]), "UnifiedAreaRect", value, sizeof value);
    if (parse_rect(value, v)) {
        float p[8];
        void *piece = win_of(STATS_PREFIX, "pieceToMove");
        read_prop(piece, "UnifiedAreaRect", value, sizeof value);
        if (parse_rect(value, p)) move_by(piece, v[1] + piece_dx - p[1], v[3] + piece_dy - p[3]);
    }
    stats_fill();
}

/* The points added are spent when the page is left. */
static void stats_commit(void) {
    int i, n;
    if (!hero) return;
    for (i = 0; i < 4; i++) {
        for (n = 0; n < stat_added[i]; n++) ((unit_void_fn)stat_fn(1, i))(hero, 0);
        stat_added[i] = 0;
    }
}

static void stats_load(void) {
    void *container = win_of(MASTER, "ContainerFrame");
    char value[128];
    float s[8], p[8];
    if (stats_root || !container) return;
    stats_root = load("media/ui/x360/statsmenu_xenon.layout", STATS_PREFIX);
    journal_root = stats_root ? load("media/ui/x360/journalmenu_xenon.layout", JOURNAL_PREFIX) : 0;
    if (!stats_root) return;
    scale_tree(stats_root, k, 0);
    add_child(container, 0, stats_root);
    move_by(stats_root, 299.0f * k, 0);
    if (journal_root) {
        scale_tree(journal_root, k, 0);
        add_child(container, 0, journal_root);
        move_by(journal_root, 602.0f * k, 0);
        /* The stats' column over the journal's: its point box and help reach over it (the
         * catalog's menus/10). */
        move_to_front(stats_root, 0);
    }
    read_prop(win_of(STATS_PREFIX, "strength"), "UnifiedAreaRect", value, sizeof value);
    if (parse_rect(value, s)) {
        read_prop(win_of(STATS_PREFIX, "pieceToMove"), "UnifiedAreaRect", value, sizeof value);
        if (parse_rect(value, p)) { piece_dx = p[1] - s[1]; piece_dy = p[3] - s[3]; }
    }
}

static void stats_pad(unsigned pressed, int ly) {
    static int held;
    int dir = (pressed & 0x0001) || (ly > 500 && !held) ? -1 : (pressed & 0x0002) || (ly < -500 && !held) ? 1 : 0;
    held = ly > 500 || ly < -500;
    if (dir) {
        int s = stat_sel + dir;
        if (s < 0) s = 0;
        if (s > 3) s = 3;
        if (s != stat_sel) { stat_sel = s; stats_select(); }
    }
    if (pressed & 0x1000) {                                              /* A */
        int i, pending = 0;
        for (i = 0; i < 4; i++) pending += stat_added[i];
        if (hero && *(int *)(hero + GOG_STAT_POINTS) - pending > 0) { stat_added[stat_sel]++; stats_fill(); }
    }
    if (pressed & 0x8000 && stat_added[stat_sel] > 0) { stat_added[stat_sel]--; stats_fill(); }   /* Y */
}

/* A text set as the 360's would draw it: one "|cAARRGGBB" .. "|u" around it gives its colour (the
 * text plain); else as it is (see plain_markup below). */
static void marked_text(void *w, const char *t) {
    char value[600], colour[12];
    unsigned i, len;
    if (t && t[0] == '|' && t[1] == 'c' && length(t) > 10) {
        for (i = 0; i < 8; i++) colour[i] = t[2 + i];
        colour[8] = 0;
        copy(value, sizeof value, t + 10, 0);
        len = length(value);
        while (len && value[len - 1] == ' ') value[--len] = 0;
        if (len >= 2 && value[len - 2] == '|' && value[len - 1] == 'u') value[len - 2] = 0;
        text(w, value);
        prop(w, "TextColour", colour);
        return;
    }
    text(w, t);
}

/* The 360's CEGUI draws "|cAARRGGBB" .. "|u" in a text in that colour; PC's writes the marks as
 * they are. A layout text in that form (the Skills page's level tabs, "|cFFEAAA1520|u") is given
 * as its plain text and the colour as its TextColour. */
static void plain_markup(void *w, int depth) {
    char value[160], colour[12];
    unsigned i, n, len;
    typedef const void *(__fastcall *get_text_fn)(const void *self, void *edx);
    static get_text_fn get_text;
    if (!w || depth > 40) return;
    if (!get_text) get_text = (get_text_fn)need("?getText@Window@CEGUI@@QBEABVString@2@XZ");
    if (get_text) {
        const char *t = c_str(get_text(w, 0), 0);
        if (t && t[0] == '|' && t[1] == 'c' && length(t) > 12) {
            for (i = 0; i < 8; i++) colour[i] = t[2 + i];
            colour[8] = 0;
            copy(value, sizeof value, t + 10, 0);
            len = length(value);
            if (len >= 2 && value[len - 2] == '|' && value[len - 1] == 'u') value[len - 2] = 0;
            text(w, value);
            prop(w, "TextColour", colour);
        }
    }
    n = child_count(w, 0);
    for (i = 0; i < n; i++) plain_markup(child_at(w, 0, i), depth + 1);
}

/* A text with the 360's colour marks in it ("|cAARRGGBB" .. "|u" around a line, as the skills'
 * descriptions have them, from the game's data): PC's CEGUI writes the marks as they are, and one
 * window has one colour, so each paragraph gets a StaticText of its own under the window, in the
 * window's font and formatting, its colour the mark's or the window's, one under the other by
 * the lines CEGUI wraps it into (Font::getFormattedLineCount, getLineSpacing). */
typedef struct { float top, bottom, left, right; } cegui_rect;
static void rich_text(void *w, const char *prefix, const char *base, const char *textv) {
    typedef void *(__fastcall *get_font_fn)(const void *self, void *edx, unsigned char def);
    typedef unsigned (__fastcall *line_count_fn)(void *font, void *edx, const void *text, const cegui_rect *area, int fmt, float scale);
    typedef float (__fastcall *spacing_fn)(const void *font, void *edx, float scale);
    typedef cegui_rect *(__fastcall *rect_fn)(const void *self, void *edx, cegui_rect *out);
    static get_font_fn get_font;
    static line_count_fn line_count;
    static spacing_fn spacing;
    static rect_fn unclipped;
    char colour_base[24], fontname[48], name[64], para[400], colour[12], *p;
    const char *t = textv;
    void *font;
    cegui_rect r, area;
    float y = 0, h, sp;
    int n = 0, i;
    if (!w) return;
    if (!get_font) {
        get_font = (get_font_fn)need("?getFont@Window@CEGUI@@QBEPAVFont@2@_N@Z");
        line_count = (line_count_fn)need("?getFormattedLineCount@Font@CEGUI@@QAEIABVString@2@ABVRect@2@W4TextFormatting@2@M@Z");
        spacing = (spacing_fn)need("?getLineSpacing@Font@CEGUI@@QBEMM@Z");
        unclipped = (rect_fn)need("?getUnclippedPixelRect@Window@CEGUI@@QBE?AVRect@2@XZ");
    }
    if (!get_font || !line_count || !spacing || !unclipped) { text(w, textv); return; }
    read_prop(w, "TextColour", colour_base, sizeof colour_base);
    read_prop(w, "Font", fontname, sizeof fontname);
    font = get_font(w, 0, 1);
    unclipped(w, 0, &r);
    sp = font ? spacing(font, 0, 1.0f) : 0;
    text(w, "");
    while (*t && n < 12) {
        unsigned len = 0;
        void *pw;
        object str;
        colour[0] = 0;
        while (t[len] && t[len] != '\n') len++;
        if (len >= 10 && t[0] == '|' && t[1] == 'c') { for (i = 0; i < 8; i++) colour[i] = t[2 + i]; colour[8] = 0; }
        for (i = 0, p = para; (unsigned)i < len && p < para + sizeof para - 1; i++) {
            if (t[i] == '|' && t[i + 1] == 'c' && (unsigned)i + 10 <= len) { i += 9; continue; }
            if (t[i] == '|' && t[i + 1] == 'u') { i += 1; continue; }
            *p++ = t[i];
        }
        *p = 0;
        t += len;
        if (*t == '\n') t++;
        if (!para[0] && !*t) break;   /* the text's last newline */
        copy(name, sizeof name, base, "_rt");
        p = put_int(name + length(name), n); *p = 0;
        pw = win_of(prefix, name);
        if (!pw && (pw = create_window(prefix, name, "ceguiWidget/StaticText")) != 0) add_child(w, 0, pw);
        if (!pw) break;
        if (fontname[0]) prop(pw, "Font", fontname);
        prop(pw, "HorzTextFormatting", "WordWrapLeftAligned");
        prop(pw, "VertFormatting", "TopAligned");
        prop(pw, "TextColour", colour[0] ? colour : colour_base);
        text(pw, para[0] ? para : " ");
        area.left = 0; area.top = 0; area.right = r.right - r.left; area.bottom = 100000.0f;
        string_utf8(&str, 0, para[0] ? para : " ");
        h = sp * (float)(font ? line_count(font, 0, &str, &area, 4, 1.0f) : 1);   /* 4: WordWrapLeftAligned */
        string_dtor(&str, 0);
        set_size(pw, 0, y, r.right - r.left, h);
        show(pw, 1);
        y += h;
        n++;
    }
    for (; n < 12; n++) {   /* the paragraphs not used this time */
        copy(name, sizeof name, base, "_rt");
        p = put_int(name + length(name), n); *p = 0;
        show(win_of(prefix, name), 0);
    }
}

/* ---- the Skills page (the catalog's menus/11) ----
 * The 360's CSkillMenuXenon: skillsmenu_xenon in the frame. Its fill (sub_8235BE88) for each skill
 * of the player's skill manager (the player's +0x184; its list +0x2c on the 360, GOG's +0x3c and
 * count +0x40): the cell skillbg{tab}_{row}_{column} (the skill's +0xdc, +0xd8, +0xd4, the same
 * in GOG's CSkill), skills_window_passive for a passive (+0x38 == 4) else skills_window_active,
 * their _rollover for the selected cell; skillicon at 0.35 while the skill has no rank (+0xa4),
 * else 1; when the level it requires (GOG 0x5d5c00) is above the player's (+0xf0) and it has no
 * effective rank (+0xa8, GOG 0x5d7d40 computes it), the inactive icon (GOG 0x5d5950) and no
 * skilllvl; else the icon (GOG 0x5d5930) and skilllvl the effective rank. PanelName0..2: the
 * player's tab names (GOG 0x4a2220, 1 to 3); skillPointsValue the player's skill points (+0x3e0).
 * The selected skill (sub_8235D558): its icon, Rank "effective/maximum" (+0x188, else +0x68,
 * else 1), its name (GOG 0x5d59b0), its kind's text (GOG 0x5d68e0), "Always Enabled" for a passive,
 * its rank's effects (the 360's sub_823B07C8: "Inflicts 86% of Weapon DPS" ..; GOG 0x5d8c60, out, the
 * unit, the rank or -1 for the effective one, whose 0x5e0fc0 writes those lines; its neighbour
 * 0x5d88f0 gives the data's own description instead), its colour marks drawn by rich_text. The help box (sub_82381D18): centred under the
 * selected cell, 7 (720 rows) up into it; skill points; for a spell (+0x34 != 0) upgrade at 0.3
 * and map; with no rank: hidden without points, "Requires Level N" when the level is short, else
 * learn and map at 0.3; with a rank: upgrade (0.3 without points) and map (at 1 for a +0x38 == 3).
 * Input (sub_823827F8): the d-pad or stick moves over the cells (sub_8235E3A0 .. E6D0: columns and
 * tabs round, empty cells passed over), A learns (sub_8235E9D0's test: the level, the prerequisite
 * at +0x84 with a rank (GOG 0x5dd660 finds it), points, below the maximum; then PC's own: GOG
 * 0x488430 takes the point, 0x5de550 sets the rank, as PC's button at 0x5a58b0). */
#define SKILLS_PREFIX "x360sk_"
#define HUD_PREFIX_SK "x360hud_"
#define GOG_SKILL_MANAGER 0x184
#define GOG_SKILL_POINTS 0x3e0
#define GOG_HERO_LEVEL 0xf0
typedef const void *(__fastcall *skill_str_fn)(void *skill, void *edx);
typedef int (__fastcall *skill_int_fn)(void *skill, void *edx);
typedef void *(__fastcall *skill_out_fn)(void *skill, void *edx, void *out);
typedef void *(__fastcall *skill_desc_fn)(void *skill, void *edx, void *out, void *unit, int level);
typedef void *(__fastcall *tab_name_fn)(void *unit, void *edx, void *out, int tab);
typedef void *(__fastcall *find_skill_fn)(void *manager, void *edx, const void *name);
typedef void (__fastcall *set_skill_level_fn)(void *manager, void *edx, void *skill, int level);
typedef unsigned char (__fastcall *use_point_fn)(void *unit, void *edx);
static int sk_tab, sk_row, sk_col, sk_spells;
static float help_width;

/* ---- the skill's description as the 360 writes it (sub_823B07C8) ----
 * For the rank's data (the rank, or the effective one +0xa8 for -1/0; the ranks' vector +0x64,
 * their count +0x68; else +0x5c, as GOG 0x5d8c60): a cooldown line when the cooldown is over 0
 * (sub_823C0A60, GOG 0x5df280: +0x70 for a player, else +0x6c) "|cFF9180FF" cooldown ": " "%.1f"
 * " " seconds "|u \n"; a mana line when the cost (+0x4c) is over 0: the data's own text (the 360's
 * +288, GOG's +0xe8, as PC's tooltip 0x5d8d80), "\n|cFF9180FF" mana ": " the cost "|u\n"; the
 * weapon requirement (sub_823B0230) "|cFF9180FF" .. "|u \n"; the rank's effects (GOG 0x5d8c60, the
 * 360's sub_823C0AE0); a closing "\n". The requirement, from the first rank's data (+0x5c):
 * REQUIREMENT_RIGHT +8, REQUIREMENT_LEFT +0xc, DUELWIELD_REQUIRED +0x10 (GOG's loader near
 * 0x5dfb72): with the dual flag, the dual line and "\n"; then, with the dual flag and both hands
 * the same, req.both ": " the type " " req.weapons, else (with any hand) req.right ": " the type
 * " " req.weapon. The type's name (sub_823AFDB0): melee (60) single (9), two-handed (10) or plain;
 * ranged (35 or 163) the same; else any (the hierarchy's test, GOG 0x5fe190 as 0x480760 calls it).
 * The 360's capture test/04: "|cFF9180FFRight hand requires : melee Weapon|u \n" then the
 * effects. */
static char *put_str(char *p, char *end, const char *t) {
    while (*t && p < end) *p++ = *t++;
    return p;
}

static char *put_num(char *p, char *end, float v, int decimals) {
    char b[24], *q = put_float(b, decimals == 1 ? v + 0.05f : v);   /* three decimals; one rounded */
    *q = 0;
    q = b;
    while (*q && *q != '.') q++;
    if (*q == '.') {
        if (decimals == 1) q[2] = 0;
        else {   /* as an ostream's default: no trailing zeros */
            char *z = q + 4;
            while (z > q && (z[-1] == '0' || z[-1] == '.')) { z--; if (*z == '.') { *z = 0; break; } *z = 0; }
        }
    }
    return put_str(p, end, b);
}

static int type_is(int type, int of) {
    typedef unsigned char (__fastcall *type_isa_fn)(void *hierarchy, void *edx, int type, int of);
    void *h = hero ? *(void **)(hero + 0x60) : 0;
    return h && ((type_isa_fn)0x005fe190)(h, 0, type, of) != 0;
}

static const char *weapon_type_name(int t) {
    if (type_is(t, 60)) return x360_text(type_is(t, 9) ? "wt.melee1" : type_is(t, 10) ? "wt.melee2" : "wt.melee");
    if (type_is(t, 35) || type_is(t, 163))
        return x360_text(type_is(t, 9) ? "wt.ranged1" : type_is(t, 10) ? "wt.ranged2" : "wt.ranged");
    return x360_text("wt.any");
}

static unsigned char *skill_rank_data(unsigned char *sk, int level) {
    unsigned count = *(unsigned *)(sk + 0x68);
    unsigned char **ranks = *(unsigned char ***)(sk + 0x64);
    int i = (level == -1 || level == 0 ? *(int *)(sk + 0xa8) : level) - 1;
    if (count && ranks && i >= 0 && (unsigned)i < count && ranks[i]) return ranks[i];
    return count && ranks ? ranks[0] : *(unsigned char **)(sk + 0x5c);
}

static void skill_desc_360(unsigned char *sk, int level, const char *effects, char *out, unsigned size) {
    typedef float (__fastcall *cooldown_fn)(void *rank, void *edx);
    char *p = out, *end = out + size - 1;
    unsigned char *d = skill_rank_data(sk, level), *first = *(unsigned char **)(sk + 0x5c);
    float v;
    *out = 0;
    if (!first || !d) return;
    v = ((cooldown_fn)0x005df280)(d, 0);
    if (v > 0.0f) {
        p = put_str(p, end, "|cFF9180FF"); p = put_str(p, end, x360_text("skill.cooldown")); p = put_str(p, end, ": ");
        p = put_num(p, end, v, 1); p = put_str(p, end, " "); p = put_str(p, end, x360_text("skill.seconds"));
        p = put_str(p, end, "|u \n");
    }
    v = *(float *)(d + 0x4c);
    if (v > 0.0f) {
        char own[400];
        if (wstring_utf8_long(d + 0xe8, own, sizeof own)) p = put_str(p, end, own);
        p = put_str(p, end, "\n|cFF9180FF"); p = put_str(p, end, x360_text("skill.mana")); p = put_str(p, end, ": ");
        p = put_num(p, end, v, 0); p = put_str(p, end, "|u\n");
    }
    {
        int right = *(int *)(first + 8), left = *(int *)(first + 0xc), dual = first[0x10] != 0;
        if (dual || right || left) {
            p = put_str(p, end, "|cFF9180FF");
            if (dual) { p = put_str(p, end, x360_text("req.dual")); p = put_str(p, end, "\n"); }
            if (dual && right && left == right) {
                p = put_str(p, end, x360_text("req.both")); p = put_str(p, end, ": "); p = put_str(p, end, weapon_type_name(right));
                p = put_str(p, end, " "); p = put_str(p, end, x360_text("req.weapons"));
            } else {
                p = put_str(p, end, x360_text("req.right")); p = put_str(p, end, ": "); p = put_str(p, end, weapon_type_name(right));
                p = put_str(p, end, " "); p = put_str(p, end, x360_text("req.weapon"));
            }
            p = put_str(p, end, "|u \n");
        }
    }
    p = put_str(p, end, effects);
    if (p > out && p[-1] != '\n') p = put_str(p, end, "\n");
    *p = 0;
}

static void **skill_list(unsigned *count) {
    unsigned char *mgr = hero ? *(unsigned char **)(hero + GOG_SKILL_MANAGER) : 0;
    *count = mgr ? *(unsigned *)(mgr + 0x40) : 0;
    return mgr ? *(void ***)(mgr + 0x3c) : 0;
}

/* The image of an icon name: the first itemicons imageset that defines it (the 360's image lookup,
 * sub_8233DDE0, over its imagesets). */
static int icon_image(const char *icon, char *out, unsigned size) {
    typedef unsigned char (__fastcall *present_fn)(const void *self, void *edx, const void *name);
    typedef void *(__fastcall *get_set_fn)(const void *mgr, void *edx, const void *name);
    static singleton_fn manager;
    static present_fn set_present, image_defined;
    static get_set_fn get_set;
    char name[32], *p;
    int i, found = 0;
    object a, b;
    if (!manager) {
        manager = (singleton_fn)need("?getSingletonPtr@?$Singleton@VImagesetManager@CEGUI@@@CEGUI@@SAPAVImagesetManager@2@XZ");
        set_present = (present_fn)need("?isImagesetPresent@ImagesetManager@CEGUI@@QBE_NABVString@2@@Z");
        image_defined = (present_fn)need("?isImageDefined@Imageset@CEGUI@@QBE_NABVString@2@@Z");
        get_set = (get_set_fn)need("?getImageset@ImagesetManager@CEGUI@@QBEPAVImageset@2@ABVString@2@@Z");
    }
    if (!manager || !set_present || !image_defined || !get_set || !icon[0]) return 0;
    string_ctor(&b, 0, icon);
    for (i = 1; i <= 40 && !found; i++) {
        copy(name, sizeof name, "itemicons", 0);
        p = put_int(name + 9, i);
        *p = 0;
        string_ctor(&a, 0, name);
        __try {
            if (set_present(manager(), 0, &a) && image_defined(get_set(manager(), 0, &a), 0, &b)) {
                copy(out, size, "set:", name);
                copy(out + length(out), size - length(out), " image:", icon);
                found = 1;
            }
        } __except (1) {}
        string_dtor(&a, 0);
    }
    string_dtor(&b, 0);
    return found;
}

/* The character's column: the hero's sheet, or the pet's on the inventory's page with the pet's bag
 * focused (in-game/19: RB from the hero's bag; "<pet name> / Pet", its spells, HP/MP, the stats). */
static void chr_show(int on) {
    int pet = on && page == 0 && panels[focus_panel].is_pet && petchr_root;
    chr_on = on;
    if (character_root) show(win_of(CHR_PREFIX, "TopFrame"), on && !pet);
    if (petchr_root) show(win_of(PETCHR_PREFIX, "TopFrame"), pet);
}

static void skill_icon(void *skill, unsigned fn, char *out, unsigned size);
static void pet_sheet_fill(unsigned char *pet) {
    char buf[96], *p;
    int i;
    if (!petchr_root) return;
    if (wstring_utf8(pet + 0x420, buf, sizeof buf)) text(win_of(PETCHR_PREFIX, "characterName"), buf);
    p = put_int(buf, ((int_get_fn)GOG_MAX_HP)(pet, 0)); *p = 0; text(win_of(PETCHR_PREFIX, "HP"), buf);
    p = put_int(buf, ((int_get_fn)GOG_MAX_MANA)(pet, 0)); *p = 0; text(win_of(PETCHR_PREFIX, "Mana"), buf);
    p = put_int(buf, ((int_get_fn)GOG_STRENGTH)(pet, 0)); *p = 0; text(win_of(PETCHR_PREFIX, "strPts"), buf);
    p = put_int(buf, ((int_get_fn)GOG_DEXTERITY)(pet, 0)); *p = 0; text(win_of(PETCHR_PREFIX, "dexPts"), buf);
    p = put_int(buf, ((int_get_fn)GOG_MAGIC)(pet, 0)); *p = 0; text(win_of(PETCHR_PREFIX, "magicPts"), buf);
    p = put_int(buf, ((int_get_fn)GOG_DEFENSE)(pet, 0)); *p = 0; text(win_of(PETCHR_PREFIX, "defPts"), buf);
    /* Its two spells (the pet's known spells, getKnownSpell 0x484ff0 as the hero's): the icon and
     * name, or no icon and the 360's empty text. */
    for (i = 0; i < 2; i++) {
        unsigned char *sp = 0;
        char name[16];
        __try { sp = (unsigned char *)((known_spell_fn)0x484ff0)(pet, 0, i); } __except (1) { sp = 0; }
        copy(name, sizeof name, "spellIcon", 0); p = put_int(name + 9, i); *p = 0;
        if (sp) skill_icon(sp, 0x5d5950, buf, sizeof buf); else buf[0] = 0;
        prop(win_of(PETCHR_PREFIX, name), "Image", buf);
        copy(name, sizeof name, "SpellText", 0); p = put_int(name + 9, i); *p = 0;
        if (!sp || !wstring_utf8((const unsigned char *)((skill_str_fn)0x5d59b0)(sp, 0), buf, sizeof buf))
            copy(buf, sizeof buf, x360_text("pet.nospell"), 0);
        text(win_of(PETCHR_PREFIX, name), buf);
    }
}

static void skill_icon(void *skill, unsigned fn, char *out, unsigned size) {
    char icon[96];
    out[0] = 0;
    if (wstring_utf8((const unsigned char *)((skill_str_fn)fn)(skill, 0), icon, sizeof icon)) icon_image(icon, out, size);
}

/* CCharacter::getKnownSpell *(OT)* (GOG 0x484ff0; the 360's sub_821FD868): the player's spell i. */
static int sk_spell;

static void *selected_skill(void) {
    unsigned n, i;
    void **list = skill_list(&n);
    if (sk_spells) return hero ? ((known_spell_fn)0x484ff0)(hero, 0, sk_spell) : 0;
    for (i = 0; list && i < n; i++) {
        unsigned char *sk = (unsigned char *)list[i];
        if (!sk) continue;
        if (*(int *)(sk + 0xdc) == sk_tab && *(int *)(sk + 0xd8) == sk_row && *(int *)(sk + 0xd4) == sk_col) return sk;
    }
    return 0;
}

static int skill_max(unsigned char *sk) {
    int m = *(int *)(sk + 0x188);
    if (!m) m = *(int *)(sk + 0x68);
    return m ? m : 1;
}

static int skill_learnable(unsigned char *sk) {
    unsigned char *mgr = *(unsigned char **)(hero + GOG_SKILL_MANAGER);
    int ok = ((skill_int_fn)0x5d5c00)(sk, 0) <= *(int *)(hero + GOG_HERO_LEVEL);
    if (*(unsigned *)(sk + 0x84 + 0x14)) {    /* a prerequisite's name (a std::wstring) */
        unsigned char *pre = (unsigned char *)((find_skill_fn)0x5dd660)(mgr, 0, sk + 0x84);
        if (pre && *(int *)(pre + 0xa4) < 1) ok = 0;
    }
    if (*(int *)(hero + GOG_SKILL_POINTS) <= 0 || !ok) return 0;
    ((skill_int_fn)0x5d7d40)(sk, 0);
    return *(int *)(sk + 0xa8) < skill_max(sk);
}

static void skills_fill(void) {
    unsigned n, i;
    void **list = skill_list(&n);
    char name[48], buf[160], *p;
    int t, r, c;
    unsigned char *sel;
    void *cell = 0;
    wstr_ctor_fn wctor;
    wstr_dtor_fn wdtor;
    unsigned char ws[32];
    static const unsigned short empty[] = {0};
    if (!skills_root || !hero || !wstring_functions(&wctor, &wdtor)) return;
    /* Every cell hidden; the skills' shown. */
    for (t = 0; t < 3; t++) for (r = 0; r < 6; r++) for (c = 0; c < 2; c++) {
        copy(name, sizeof name, "skillbg", 0); p = put_int(name + 7, t); *p++ = '_'; p = put_int(p, r); *p++ = '_';
        p = put_int(p, c); *p = 0;
        show(win_of(SKILLS_PREFIX, name), 0);
    }
    for (t = 0; t < 3; t++) {
        wctor(ws, 0, empty);
        ((tab_name_fn)0x4a2220)(hero, 0, ws, t + 1);
        if (!wstring_utf8(ws, buf, sizeof buf)) buf[0] = 0;
        wdtor(ws, 0);
        copy(name, sizeof name, "PanelName", 0); p = put_int(name + 9, t); *p = 0;
        text(win_of(SKILLS_PREFIX, name), buf);
    }
    p = put_int(buf, *(int *)(hero + GOG_SKILL_POINTS)); *p = 0;
    text(win_of(SKILLS_PREFIX, "skillPointsValue"), buf);
    for (i = 0; list && i < n; i++) {
        unsigned char *sk = (unsigned char *)list[i];
        void *bg, *icon, *lvl;
        int passive, selected;
        if (!sk) continue;
        t = *(int *)(sk + 0xdc); r = *(int *)(sk + 0xd8); c = *(int *)(sk + 0xd4);
        if (t < 0 || t > 2 || r < 0 || r > 5 || c < 0 || c > 1) continue;
        copy(name, sizeof name, "skillbg", 0); p = put_int(name + 7, t); *p++ = '_'; p = put_int(p, r); *p++ = '_';
        p = put_int(p, c); *p = 0;
        bg = win_of(SKILLS_PREFIX, name);
        if (!bg) continue;
        show(bg, 1);
        passive = *(int *)(sk + 0x38) == 4;
        selected = !sk_spells && t == sk_tab && r == sk_row && c == sk_col;
        if (selected) cell = bg;
        prop(bg, "Image", passive ? (selected ? "set:X360_widgets1 image:skills_window_passive_rollover" : "set:X360_widgets2 image:skills_window_passive")
                                  : (selected ? "set:X360_widgets2 image:skills_window_active_rollover" : "set:X360_widgets2 image:skills_window_active"));
        copy(name, sizeof name, "skillicon", 0); p = put_int(name + 9, t); *p++ = '_'; p = put_int(p, r); *p++ = '_';
        p = put_int(p, c); *p = 0;
        icon = win_of(SKILLS_PREFIX, name);
        copy(name, sizeof name, "skilllvl", 0); p = put_int(name + 8, t); *p++ = '_'; p = put_int(p, r); *p++ = '_';
        p = put_int(p, c); *p = 0;
        lvl = win_of(SKILLS_PREFIX, name);
        show(icon, 1);
        prop(icon, "Alpha", *(int *)(sk + 0xa4) ? "1" : "0.35");
        ((skill_int_fn)0x5d7d40)(sk, 0);
        if (((skill_int_fn)0x5d5c00)(sk, 0) > *(int *)(hero + GOG_HERO_LEVEL) && *(int *)(sk + 0xa8) <= 0) {
            skill_icon(sk, 0x5d5950, buf, sizeof buf);
            prop(icon, "Image", buf);
            show(lvl, 0);
        } else {
            skill_icon(sk, 0x5d5930, buf, sizeof buf);
            prop(icon, "Image", buf);
            show(lvl, 1);
            p = put_int(buf, *(int *)(sk + 0xa8)); *p = 0;
            text(lvl, buf);
        }
    }
    /* The spells' row (sub_8235BE88's second loop): spellbg i, its _rollover when selected, and
     * spellIcon i: the spell's inactive icon, at 1 with a rank, else 0.35; none: no image. */
    for (t = 0; t < 4; t++) {
        unsigned char *sp = (unsigned char *)((known_spell_fn)0x484ff0)(hero, 0, t);
        void *bg, *icon;
        copy(name, sizeof name, "spellbg", 0); p = put_int(name + 7, t); *p = 0;
        bg = win_of(SKILLS_PREFIX, name);
        copy(name, sizeof name, "spellIcon", 0); p = put_int(name + 9, t); *p = 0;
        icon = win_of(SKILLS_PREFIX, name);
        prop(bg, "Image", sk_spells && sk_spell == t ? "set:X360_widgets2 image:spells_windowb_rollover"
                                                     : "set:X360_widgets2 image:spells_windowb");
        if (sk_spells && sk_spell == t) cell = bg;
        if (sp) {
            skill_icon(sp, 0x5d5950, buf, sizeof buf);
            prop(icon, "Image", buf);
            prop(icon, "Alpha", *(int *)(sp + 0xa4) ? "1" : "0.35");
        } else {
            prop(icon, "Image", "");
            prop(icon, "Alpha", "0.35");
        }
    }
    /* The selected skill's column. */
    sel = (unsigned char *)selected_skill();
    if (sel) {
        int m = skill_max(sel);
        skill_icon(sel, 0x5d5930, buf, sizeof buf);
        prop(win_of(SKILLS_PREFIX, "CurrentActiveSkillIconImage"), "Image", buf);
        prop(win_of(SKILLS_PREFIX, "CurrentActiveSkillFrame"), "Image", *(int *)(sel + 0x38) == 4 ? "set:X360_widgets2 image:skills_window_passive_lg"
                                                                                                 : "set:X360_widgets2 image:skills_window_active_lg");
        p = put_int(buf, *(int *)(sel + 0xa8)); *p++ = '/'; p = put_int(p, m); *p = 0;
        text(win_of(SKILLS_PREFIX, "RankTextVal"), buf);
        if (!wstring_utf8((const unsigned char *)((skill_str_fn)0x5d59b0)(sel, 0), buf, sizeof buf)) buf[0] = 0;
        text(win_of(SKILLS_PREFIX, "SkillNameText"), buf);
        show(win_of(SKILLS_PREFIX, "PassiveDescText"), *(int *)(sel + 0x38) == 4);
        wctor(ws, 0, empty);
        __try { ((skill_out_fn)0x5d68e0)(sel, 0, ws); } __except (1) {}
        if (!wstring_utf8(ws, buf, sizeof buf)) buf[0] = 0;
        wdtor(ws, 0);
        text(win_of(SKILLS_PREFIX, "TypeDescText"), buf);
        wctor(ws, 0, empty);
        __try { ((skill_desc_fn)0x5d8c60)(sel, 0, ws, hero, -1); } __except (1) {}
        {
            char eff[600], desc[1000];
            if (!wstring_utf8_long(ws, eff, sizeof eff)) eff[0] = 0;
            skill_desc_360(sel, -1, eff, desc, sizeof desc);
            rich_text(win_of(SKILLS_PREFIX, "SkillDescriptionText"), SKILLS_PREFIX, "SkillDescriptionText", desc);
        }
        wdtor(ws, 0);
        /* The next rank's text (sub_8235D558): under "Next Level" while the skill has a rank
         * below its maximum; else nothing. */
        {
            int eff = *(int *)(sel + 0xa8);
            char effects[600], desc[1000];
            desc[0] = 0;
            if (eff > 0 && eff < m) {
                wctor(ws, 0, empty);
                __try { ((skill_desc_fn)0x5d8c60)(sel, 0, ws, hero, eff + 1); } __except (1) {}
                if (!wstring_utf8_long(ws, effects, sizeof effects)) effects[0] = 0;
                wdtor(ws, 0);
                skill_desc_360(sel, eff + 1, effects, desc, sizeof desc);
            }
            show(win_of(SKILLS_PREFIX, "NextLevelHeader"), eff > 0 && eff < m);
            /* "Requires Level" N: the level the next rank asks (GOG 0x5d5c00, the 360's sub_823ADE08). */
            p = put_int(buf, ((skill_int_fn)0x5d5c00)(sel, 0)); *p = 0;
            text(win_of(SKILLS_PREFIX, "RequiredLevelVal"), buf);
            rich_text(win_of(SKILLS_PREFIX, "SkillDescriptionTextNext"), SKILLS_PREFIX, "SkillDescriptionTextNext", desc);
        }
    }
    /* The skill set's slots: the icons the HUD's same slots show (its SkillSlot4 to 7). */
    {
        static const char *const slot[4] = {"SkillSlot4", "SkillSlot5", "SkillSlot6", "SkillSlot7"};
        int j;
        for (j = 0; j < 4; j++) {
            char hudname[32], image[128];
            void *src, *dst, *icon;
            copy(hudname, sizeof hudname, "icon_", slot[j]);
            src = win_of(HUD_PREFIX_SK, hudname);
            dst = win_of(SKILLS_PREFIX, slot[j]);
            if (!dst) continue;
            copy(hudname, sizeof hudname, "icon_", slot[j]);
            icon = win_of(SKILLS_PREFIX, hudname);
            if (!icon && (icon = create_window(SKILLS_PREFIX, hudname, "ceguiWidget/StaticImage")) != 0) {
                add_child(dst, 0, icon);
                prop(icon, "UnifiedAreaRect", "{{0,0},{0,0},{1,0},{1,0}}");
            }
            image[0] = 0;
            if (src) read_prop(src, "Image", image, sizeof image);
            if (icon) {
                char area[128];
                prop(icon, "Image", image);
                show(icon, image[0] != 0);
                area[0] = 0;
                if (src) read_prop(src, "UnifiedAreaRect", area, sizeof area);   /* the HUD's size rule */
                if (area[0]) prop(icon, "UnifiedAreaRect", area);
            }
        }
    }
    /* The help box under the selected cell. */
    {
        void *box = win_of(SKILLS_PREFIX, "HelpSkillDialog");
        char value[128];
        float cv[8], pv[8], hv[8], s = k * 768.0f / 720.0f;
        int points = *(int *)(hero + GOG_SKILL_POINTS);
        show(box, 0);
        if (box && cell && sel) {
            void *parent = 0;
            typedef void *(__fastcall *parent_fn)(const void *w, void *edx);
            static parent_fn get_parent;
            if (!get_parent) get_parent = (parent_fn)need("?getParent@Window@CEGUI@@QBEPAV12@XZ");
            parent = get_parent ? get_parent(cell, 0) : 0;
            read_prop(cell, "UnifiedAreaRect", value, sizeof value);
            if (parse_rect(value, cv)) {
                pv[1] = pv[3] = 0;
                if (parent) { read_prop(parent, "UnifiedAreaRect", value, sizeof value); if (!parse_rect(value, pv)) pv[1] = pv[3] = 0; }
                read_prop(box, "UnifiedAreaRect", value, sizeof value);
                if (parse_rect(value, hv)) {
                    float x = pv[1] + cv[1] - (help_width - (cv[5] - cv[1])) * 0.5f;
                    float y = pv[3] + cv[3] + (cv[7] - cv[3]) - 7.0f * s;
                    set_size(box, x, y, hv[5] - hv[1], hv[7] - hv[3]);
                }
            }
            show(box, 1);
            move_to_front(box, 0);   /* over the cells (the 360's: last of i_1802's children) */
            show(win_of(SKILLS_PREFIX, "HideButtons"), 1);
            show(win_of(SKILLS_PREFIX, "HighLevel"), 0);
            p = put_int(buf, points); *p = 0;
            text(win_of(SKILLS_PREFIX, "SkillPts"), buf);
            if (*(int *)(sel + 0x34)) {
                show(win_of(SKILLS_PREFIX, "ALearnSkill"), 0);
                show(win_of(SKILLS_PREFIX, "ALevelUp"), 1); prop(win_of(SKILLS_PREFIX, "ALevelUp"), "Alpha", "0.3");
                show(win_of(SKILLS_PREFIX, "XMapSKill"), 1); prop(win_of(SKILLS_PREFIX, "XMapSKill"), "Alpha", "1");
            } else if (!*(int *)(sel + 0xa4)) {
                if (!points) show(box, 0);
                else if (((skill_int_fn)0x5d5c00)(sel, 0) > *(int *)(hero + GOG_HERO_LEVEL)) {
                    show(win_of(SKILLS_PREFIX, "HideButtons"), 0);
                    copy(buf, sizeof buf, x360_text("requires_level"), " ");
                    p = put_int(buf + length(buf), ((skill_int_fn)0x5d5c00)(sel, 0)); *p = 0;
                    text(win_of(SKILLS_PREFIX, "HighLevel"), buf);
                    show(win_of(SKILLS_PREFIX, "HighLevel"), 1);
                } else {
                    show(win_of(SKILLS_PREFIX, "ALearnSkill"), 1); prop(win_of(SKILLS_PREFIX, "ALearnSkill"), "Alpha", "1");
                    show(win_of(SKILLS_PREFIX, "ALevelUp"), 0);
                    show(win_of(SKILLS_PREFIX, "XMapSKill"), 1); prop(win_of(SKILLS_PREFIX, "XMapSKill"), "Alpha", "0.3");
                }
            } else {
                show(win_of(SKILLS_PREFIX, "ALearnSkill"), 0);
                show(win_of(SKILLS_PREFIX, "ALevelUp"), 1); prop(win_of(SKILLS_PREFIX, "ALevelUp"), "Alpha", points > 0 ? "1" : "0.3");
                show(win_of(SKILLS_PREFIX, "XMapSKill"), 1);
                prop(win_of(SKILLS_PREFIX, "XMapSKill"), "Alpha", *(int *)(sel + 0x38) == 3 ? "1" : "0.3");
            }
        }
    }
}

/* A cell with a skill at (tab, row, column). */
static int skill_at(int t, int r, int c) {
    unsigned n, i;
    void **list = skill_list(&n);
    for (i = 0; list && i < n; i++) {
        unsigned char *sk = (unsigned char *)list[i];
        if (sk && *(int *)(sk + 0xdc) == t && *(int *)(sk + 0xd8) == r && *(int *)(sk + 0xd4) == c) return 1;
    }
    return 0;
}

/* The spells' row under the tree: spell i stands under (tab, column) 0 (0,1), 1 (1,0), 2 (1,1),
 * 3 (2,0) (sub_8235E570, sub_8235E6D0). */
static const int spell_tab[4] = {0, 1, 1, 2}, spell_col[4] = {1, 0, 1, 0};

static void skills_move(int dx, int dy) {
    int tries;
    for (tries = 0; tries < 40; tries++) {
        if (sk_spells) {
            if (dx) { sk_spell = (sk_spell + (dx > 0 ? 1 : 3)) % 4; break; }
            if (dy) {   /* out of the row: up to the last row, down to the first */
                sk_spells = 0;
                sk_tab = spell_tab[sk_spell];
                sk_col = spell_col[sk_spell];
                sk_row = dy < 0 ? 5 : 0;
            }
        } else {
            if (dx > 0) { if (++sk_col > 1) { sk_col = 0; if (++sk_tab > 2) sk_tab = 0; } }
            if (dx < 0) { if (--sk_col < 0) { sk_col = 1; if (--sk_tab < 0) sk_tab = 2; } }
            if ((dy < 0 && --sk_row < 0) || (dy > 0 && ++sk_row > 5)) {
                sk_spells = 1;
                sk_spell = sk_tab == 0 ? 0 : sk_tab == 2 ? 3 : sk_col == 0 ? 1 : 2;
                break;
            }
        }
        if (skill_at(sk_tab, sk_row, sk_col)) break;
    }
    skills_fill();
}

/* The mapping of a skill to B, Y, LT or RT (X on the page). The 360: X (sub_823827F8, code 7)
 * opens it for a selected skill that is not passive (+0x38 != 4) and has an effective rank (+0xa8,
 * GOG 0x5d7d40 computes it): the help box hidden, the overlay skillsmap_xenon shown
 * (sub_823825F0(1)); X again closes it. With it open the sticks, A, B's close and LT/RT's tabs do
 * nothing; on the release of B, Y, LT or RT (the GamepadButtonUp handler, sub_82382C58) the skill
 * goes in that button's slot (sub_82382720: the same tests and +0x43 == 0 with +0x45 set; the
 * slot's skill GUID from the skill's +0x180, the slot's item cleared; the sound 30), and the
 * overlay closes; if the skill cannot go, the overlay stays. PC's own placing (GOG 0x540601, the
 * same tests and the sound 0x1e) is called: 0x40c8f0 with the slot. The slots are those the pad
 * fires (proxy/pad.c: B the hotbar key 3, Y 4, LT 5, RT 6; PC's SkillSlotN has the ID N-1,
 * GOG 0x562953); the 360's are B 4, Y 3, LT 5, RT 6, or 7, 2, 8, 9 with its second set, which
 * this port does not have. The overlay's slots show the HUD's icons, as the page's SkillSet1. */
#define SKILLMAP_PREFIX "x360skm_"
#define GOG_SET_SKILL_HOTKEY 0x0040c8f0   /* thiscall (the player), slot, GUID low, high; ret 0xc */
#define GOG_SKILL_RANK_NOW 0x005d7d40     /* the skill's effective rank, into its +0xa8 */
#define GOG_UI_HOTBAR_CHANGED 0x13d0       /* CGameUI's byte: its hotbar slots are redrawn */
static unsigned skillmap_refill_at;
typedef void (__fastcall *set_hotkey_fn)(void *unit, void *edx, int slot, unsigned lo, unsigned hi);
static int skill_mappable(unsigned char *sk) {
    if (!sk || *(int *)(sk + 0x38) == 4) return 0;
    ((void_fn)GOG_SKILL_RANK_NOW)(sk, 0);
    return *(unsigned *)(sk + 0xa8) > 0;
}

static void skillmap_fill(void) {
    static const char *const slot[4] = {"SkillSlot4", "SkillSlot5", "SkillSlot6", "SkillSlot7"};
    int j;
    for (j = 0; j < 4; j++) {
        char name[32], image[128];
        void *src, *dst, *icon;
        copy(name, sizeof name, "icon_", slot[j]);
        src = win_of(HUD_PREFIX_SK, name);
        copy(name, sizeof name, "Map", slot[j]);
        dst = win_of(SKILLMAP_PREFIX, name);
        if (!dst) continue;
        copy(name, sizeof name, "icon_Map", slot[j]);
        icon = win_of(SKILLMAP_PREFIX, name);
        if (!icon && (icon = create_window(SKILLMAP_PREFIX, name, "ceguiWidget/StaticImage")) != 0) {
            add_child(dst, 0, icon);
            prop(icon, "UnifiedAreaRect", "{{0,0},{0,0},{1,0},{1,0}}");
        }
        image[0] = 0;
        if (src) read_prop(src, "Image", image, sizeof image);
        if (icon) { prop(icon, "Image", image); show(icon, image[0] != 0); }
    }
}

static void skillmap_show(int on) {
    sk_map = on && skillmap_root;
    if (!skillmap_root) return;
    show(skillmap_root, sk_map);
    show(win_of(SKILLMAP_PREFIX, "MapSkillSet1"), 1);
    show(win_of(SKILLMAP_PREFIX, "MapSkillSet2"), 0);
    skills_fill();
    if (sk_map) { skillmap_fill(); show(win_of(SKILLS_PREFIX, "HelpSkillDialog"), 0); }
}

static int skillmap_assign(int slot) {
    unsigned char *sk = (unsigned char *)selected_skill();
    if (!skill_mappable(sk) || *(sk + 0x43) || !*(sk + 0x45)) return 0;
    ((set_hotkey_fn)GOG_SET_SKILL_HOTKEY)(hero, 0, slot, *(unsigned *)(sk + 0x180), *(unsigned *)(sk + 0x184));
    if (ui && *(void **)(ui + GOG_UI_SOUNDS))
        ((play_sound_at_fn)GOG_PLAY_SOUND_AT)(*(void **)(ui + GOG_UI_SOUNDS), 0, 0x1e, 0, 0.0f, 0.0f, 0);
    /* PC then marks its hotbar changed (GOG 0x540623, CGameUI's +0x13d0), which redraws its
     * slots; the 360 redraws the page at once (sub_82382720, its +1920's slot 8). The HUD mirrors
     * PC's slots, and the page's set is filled again once they have the new icon. */
    if (ui) *(ui + GOG_UI_HOTBAR_CHANGED) = 1;
    skillmap_refill_at = GetTickCount();
    return 1;
}

/* With the overlay open: B, Y, LT, RT on their release; X closes. Returns 1 while it is open. */
static int skillmap_pad(unsigned pressed, unsigned buttons) {
    static const struct { unsigned bit; int slot; } keys[] = {
        {0x2000, 2}, {0x8000, 3}, {0x10000, 4}, {0x20000, 5},
    };
    static unsigned last;
    unsigned released = last & ~buttons, i;
    last = buttons;
    if (skillmap_refill_at && GetTickCount() - skillmap_refill_at > 100) { skillmap_refill_at = 0; skills_fill(); }
    if (!sk_map) return 0;
    if (pressed & 0x4000) { skillmap_show(0); return 1; }
    if (pressed & 0x0030) { skillmap_show(0); return 0; }      /* Start, Back: as without it */
    for (i = 0; i < 4; i++) {
        if (pressed & keys[i].bit) skills_fill();        /* the 360's press: the help box redrawn */
        if ((released & keys[i].bit) && skillmap_assign(keys[i].slot)) { skillmap_show(0); break; }
    }
    return 1;
}

static void skills_pad(unsigned pressed, int lx, int ly) {
    static int held;
    int dx = (pressed & 0x0008) || (lx > 500 && !held) ? 1 : (pressed & 0x0004) || (lx < -500 && !held) ? -1 : 0;
    int dy = (pressed & 0x0002) || (ly < -500 && !held) ? 1 : (pressed & 0x0001) || (ly > 500 && !held) ? -1 : 0;
    held = lx > 500 || lx < -500 || ly > 500 || ly < -500;
    if (dx || dy) skills_move(dx, dy);
    if (pressed & 0x1000) {                                              /* A: learn */
        unsigned char *sk = (unsigned char *)selected_skill();
        if (sk && skill_learnable(sk) && ((use_point_fn)0x488430)(hero, 0) & 0xff) {
            ((set_skill_level_fn)0x5de550)(*(void **)(hero + GOG_SKILL_MANAGER), 0, sk, *(int *)(sk + 0xa4) + 1);
            skills_fill();
        }
    }
    if ((pressed & 0x4000) && skill_mappable((unsigned char *)selected_skill())) skillmap_show(1);   /* X */
}

static void skills_load(void) {
    void *container = win_of(MASTER, "ContainerFrame");
    char value[128];
    float v[8];
    if (skills_root || !container) return;
    skills_root = load("media/ui/x360/skillsmenu_xenon.layout", SKILLS_PREFIX);
    if (!skills_root) return;
    scale_tree(skills_root, k, 0);
    add_child(container, 0, skills_root);
    /* Moved once and back: CEGUI recomputes the descendants' rectangles it kept from before the
     * page was attached (else they are clipped where the page stood alone). */
    move_by(skills_root, 1, 0);
    move_by(skills_root, -1, 0);
    plain_markup(skills_root, 0);
    skillmap_root = load("media/ui/x360/skillsmap_xenon.layout", SKILLMAP_PREFIX);
    if (skillmap_root) {
        scale_tree(skillmap_root, k, 0);
        add_child(skills_root, 0, skillmap_root);
        plain_markup(skillmap_root, 0);
        show(skillmap_root, 0);
    }
    read_prop(win_of(SKILLS_PREFIX, "HelpSkillDialog"), "UnifiedAreaRect", value, sizeof value);
    if (parse_rect(value, v)) help_width = v[5] - v[1];
    /* What the 360's page keeps hidden (the catalog's menus/11): the second skill set and the
     * mapping's sets, the stat bonuses' lines (shown by its fill for a skill with them). */
    {
        static const char *const hidden[] = {"SkillSet2", "MapSkillSet1", "MapSkillSet2", "SkillTypeStr", "SkillTypeDex",
                                             "SkillTypeMag", "SkillTypeDef", "skillBonuses", "NextLevelHeader",
                                             "CurrentPassiveSkillFrame"};
        unsigned i;
        for (i = 0; i < sizeof hidden / sizeof hidden[0]; i++) show(win_of(SKILLS_PREFIX, hidden[i]), 0);
    }
}

/* ---- the Quest page (the catalog's menus/12) ----
 * The 360's CQuestMenuXenon: questmenu_xenon in the frame. Its list (sub_8237DF30): the quests of
 * the quest manager (GOG's: the player's +0x7b4, its list +0xc, count +0x10, as PC's own
 * CQuestMenu::updateLayout *(OT)* at 0x5a0330) that have a name and details. Its rows
 * (sub_8237E0B8): ItemTray0..5, the selected one load_chartext_window_rollover, the others
 * load_chartext_window_selected; SlotText the name (GOG 0x59f180, the quest's +0xb0, "|cFFFFBA00" .. "|u");
 * questcomplete shown for a quest complete (GOG 0x5e9200 with 1, the 360's sub_823CA628);
 * moveframe at the selected row. The selected quest (sub_8237E688): questtitle its name,
 * questdetails its details (GOG 0x5ea120, the 360's sub_823C7FE8), the reward boxes for XP, gold
 * and fame shown when not 0 ("XP:" N; GOG 0x5e8750, 0x5e8730, 0x5e8770, the quest's +0x150's
 * +0xc, +0x10, +0x14 as the 360's sub_823CBE88, BE28, BEE8). Up and down choose the quest. */
#define QUEST_PREFIX "x360qu_"
#define GOG_QUEST_MANAGER 0x7b4
typedef void *(__fastcall *quest_text_fn)(void *quest, void *edx, void *out);
typedef unsigned char (__fastcall *quest_complete_fn)(void *quest, void *edx, unsigned char flag);
static int quest_sel, quest_top;
static float qframe_dy;

static int quest_text(void *quest, unsigned fn, char *out, unsigned size) {
    wstr_ctor_fn wctor;
    wstr_dtor_fn wdtor;
    unsigned char ws[32];
    static const unsigned short empty[] = {0};
    out[0] = 0;
    if (!wstring_functions(&wctor, &wdtor)) return 0;
    wctor(ws, 0, empty);
    __try { ((quest_text_fn)fn)(quest, 0, ws); } __except (1) {}
    if (!wstring_utf8_long(ws, out, size)) out[0] = 0;
    wdtor(ws, 0);
    return out[0] != 0;
}

static int quest_list(void **out, int max) {
    unsigned char *mgr = hero ? *(unsigned char **)(hero + GOG_QUEST_MANAGER) : 0;
    void **data = mgr ? *(void ***)(mgr + 0xc) : 0;
    unsigned n = mgr ? *(unsigned *)(mgr + 0x10) : 0, i;
    int count = 0;
    char buf[600];
    for (i = 0; data && i < n && count < max; i++) {
        if (!data[i]) continue;
        if (!quest_text(data[i], 0x59f180, buf, sizeof buf) || !quest_text(data[i], 0x5ea120, buf, sizeof buf)) continue;
        out[count++] = data[i];
    }
    return count;
}

static void quest_fill(void) {
    void *quests[64];
    int n = quest_list(quests, 64), row;
    char name[32], buf[600], *p;
    if (!quest_root) return;
    if (quest_sel >= n) quest_sel = n ? n - 1 : 0;
    if (quest_sel < quest_top) quest_top = quest_sel;
    if (quest_sel >= quest_top + 6) quest_top = quest_sel - 5;
    for (row = 0; row < 6; row++) {
        int i = quest_top + row;
        void *tray;
        copy(name, sizeof name, "ItemTray", 0); p = put_int(name + 8, row); *p = 0;
        tray = win_of(QUEST_PREFIX, name);
        show(tray, i < n);
        if (i >= n) continue;
        prop(tray, "Image", i == quest_sel ? "set:X360_widgets1 image:load_chartext_window_rollover"
                                           : "set:X360_widgets1 image:load_chartext_window_selected");
        quest_text(quests[i], 0x59f180, buf, sizeof buf);
        copy(name, sizeof name, "SlotText", 0); p = put_int(name + 8, row); *p = 0;
        marked_text(win_of(QUEST_PREFIX, name), buf);
        copy(name, sizeof name, "questcomplete", 0); p = put_int(name + 13, row); *p = 0;
        show(win_of(QUEST_PREFIX, name), ((quest_complete_fn)0x5e9200)(quests[i], 0, 1) & 0xff);
    }
    /* The frame at the selected row. */
    {
        char value[128];
        float t[8], m[8];
        void *mf = win_of(QUEST_PREFIX, "moveframe");
        copy(name, sizeof name, "ItemTray", 0); p = put_int(name + 8, quest_sel - quest_top); *p = 0;
        read_prop(win_of(QUEST_PREFIX, name), "UnifiedAreaRect", value, sizeof value);
        if (parse_rect(value, t)) {
            read_prop(mf, "UnifiedAreaRect", value, sizeof value);
            if (parse_rect(value, m)) move_by(mf, 0, t[3] + qframe_dy - m[3]);
        }
        show(mf, n > 0);
    }
    if (n) {
        void *q = quests[quest_sel];
        const unsigned reward_fn[3] = {0x5e8750, 0x5e8730, 0x5e8770};
        static const char *const reward_box[3] = {"RewardXPBox", "RewardGoldBox", "RewardFameBox"};
        static const char *const reward_text[3] = {"RewardXP", "RewardGold", "RewardFame"};
        static const char *const reward_label[3] = {"reward.xp", "reward.gp", "reward.fame"};   /* x360_text keys */
        int r, any = 0;
        quest_text(q, 0x59f180, buf, sizeof buf);
        marked_text(win_of(QUEST_PREFIX, "questtitle"), buf);
        quest_text(q, 0x5ea120, buf, sizeof buf);
        text(win_of(QUEST_PREFIX, "questdetails"), buf);
        for (r = 0; r < 3; r++) {
            int v = ((skill_int_fn)reward_fn[r])(q, 0);
            show(win_of(QUEST_PREFIX, reward_box[r]), v != 0);
            copy(buf, sizeof buf, x360_text(reward_label[r]), ":");
            p = put_int(buf + length(buf), v); *p = 0;
            text(win_of(QUEST_PREFIX, reward_text[r]), buf);
            if (v) any = 1;
        }
        show(win_of(QUEST_PREFIX, "rewards"), any);
    } else {
        text(win_of(QUEST_PREFIX, "questtitle"), "");
        text(win_of(QUEST_PREFIX, "questdetails"), "");
        show(win_of(QUEST_PREFIX, "rewards"), 0);
    }
}

static void quest_load(void) {
    void *container = win_of(MASTER, "ContainerFrame");
    char value[128];
    float t[8], m[8];
    if (quest_root || !container) return;
    quest_root = load("media/ui/x360/questmenu_xenon.layout", QUEST_PREFIX);
    if (!quest_root) return;
    scale_tree(quest_root, k, 0);
    add_child(container, 0, quest_root);
    move_by(quest_root, 1, 0);
    move_by(quest_root, -1, 0);
    plain_markup(quest_root, 0);
    show(win_of(QUEST_PREFIX, "buttontray"), 0);
    read_prop(win_of(QUEST_PREFIX, "ItemTray0"), "UnifiedAreaRect", value, sizeof value);
    if (parse_rect(value, t)) {
        read_prop(win_of(QUEST_PREFIX, "moveframe"), "UnifiedAreaRect", value, sizeof value);
        if (parse_rect(value, m)) qframe_dy = m[3] - t[3];
    }
}

static void quest_pad(unsigned pressed, int ly) {
    static int held;
    int dir = (pressed & 0x0001) || (ly > 500 && !held) ? -1 : (pressed & 0x0002) || (ly < -500 && !held) ? 1 : 0;
    void *quests[64];
    int n = quest_list(quests, 64);
    held = ly > 500 || ly < -500;
    if (dir && quest_sel + dir >= 0 && quest_sel + dir < n) { quest_sel += dir; quest_fill(); }
}

/* The page shown: the inventory's bags, or the Stats page's two columns; the character's column
 * stays. */
static void set_page(int p) {
    if (page == 1 && p != 1) stats_commit();
    page = p;
    if (panels[0].root) show(win_of(PREFIX, "TopFrame"), page == 0);
    if (panels[1].root) show(win_of(PET_PREFIX, "TopFrame"), page == 0);
    if (stats_root) set_visible(stats_root, 0, page == 1);
    if (journal_root) set_visible(journal_root, 0, page == 1);
    if (skills_root) set_visible(skills_root, 0, page == 2);
    if (quest_root) set_visible(quest_root, 0, page == 3);
    /* The character's column (and its doll) on the inventory's and the Stats pages only. */
    chr_show(page < 2);
    show_doll(page < 2);
    page_titles();
    if (page == 1) { hide_info(); stat_sel = 0; stats_select(); journal_fill(); }
    else if (page == 2) {
        hide_info();
        sk_spells = 0; sk_tab = 0; sk_row = 0; sk_col = 0;
        if (!skill_at(0, 0, 0)) skills_move(1, 0); else skills_fill();
    } else if (page == 3) { hide_info(); quest_fill(); }
    else update_info();
}

/* ---- the pet's commands (Start in the inventory: the catalog's menus/09) ----
 * The 360's CMessagePetCommandsXenon (vtable 0x820d1a48) in its CMessageHousingXenon: the housing
 * (messagehousing_xenon: the screen darkened by its black at 0.7, its own close button hidden)
 * holds petcommands_xenon in its "parent". Its input (sub_8237B080): the stick or the d-pad left
 * and right turn the pet's stance (+0x1608 on the 360, GOG's +0x65c; 0 passive, 1 aggressive,
 * 2 defensive): right 0 -> 1 -> 2 -> 0, left back, then the pet's update (the 360's sub_8219c070,
 * GOG 0x4949d0, as PC's own stance buttons at 0x54ddfb, which also keep CGameUI's +0x1450); Y
 * sends the pet to town and closes: CCharacter::sendToTown *(OT)* (GOG 0x49a390, the level CGameUI's
 * +0x20) after PC's own test (the level's +0x15c, its +0x89: "Your pet cannot depart from here!"
 * otherwise) and its sound (0x5368d0, 0x2b, on CGameUI's +0x1c's +0x230), as PC's command at
 * 0x54de5c; B closes. Its drawing (sub_8237B648): each stance's image, pet_*_inactive but the
 * current one's, and the stance's name in stancetext. */
#define PETCMD_PREFIX "x360pc_"
#define HOUSING_PREFIX "x360mh_"
#define GOG_PET_STANCE 0x65c
#define GOG_UI_PET_STANCE 0x1450
#define GOG_PET_UPDATE 0x004949d0
#define GOG_SEND_TO_TOWN 0x0049a390
#define GOG_UI_LEVEL 0x20
typedef void (__fastcall *pet_update_fn)(void *pet, void *edx, int arg);
typedef void (__fastcall *send_to_town_fn)(void *pet, void *edx, void *level);
static void *housing_root, *petcmd_root;

static void petcmd_draw(void) {
    static const char *const window[3] = {"PetPassive", "PetAggressive", "PetDefensive"};
    static const char *const image[3] = {"pet_passive", "pet_aggressive", "pet_defensive"};
    static const char *const name[3] = {"stance.0", "stance.1", "stance.2"};   /* x360_text keys */
    unsigned char *pet = the_pet();
    int stance = pet ? *(int *)(pet + GOG_PET_STANCE) : 0, i;
    char value[96];
    for (i = 0; i < 3; i++) {
        copy(value, sizeof value, "set:X360_widgets2 image:", image[i]);
        if (i != stance) copy(value + length(value), sizeof value - length(value), "_inactive", 0);
        prop(win_of(PETCMD_PREFIX, window[i]), "Image", value);
    }
    text(win_of(PETCMD_PREFIX, "stancetext"), stance >= 0 && stance < 3 ? x360_text(name[stance]) : "");
}

/* The message housing, shared by the pet's commands and the gem's item select (both in its
 * "parent"). */
static int housing_load(void) {
    if (housing_root) return 1;
    housing_root = load("media/ui/x360/messagehousing_xenon.layout", HOUSING_PREFIX);
    if (!housing_root) return 0;
    scale_tree(housing_root, k, 0);
    show(win_of(HOUSING_PREFIX, "closebutton"), 0);
    set_visible(housing_root, 0, 0);
    return 1;
}
static void *sock_top;

static void petcmd_show(int on) {
    if (on && !petcmd_root) {
        if (!housing_load()) return;
        petcmd_root = load("media/ui/x360/petcommands_xenon.layout", PETCMD_PREFIX);
        if (!petcmd_root) return;
        scale_tree(petcmd_root, k, 0);
        add_child(win_of(HOUSING_PREFIX, "parent"), 0, petcmd_root);
    }
    if (!housing_root || !petcmd_root) return;
    petcmd_open = on;
    set_visible(petcmd_root, 0, on);
    if (on) show(win_of(HOUSING_PREFIX, "closebutton"), 0);
    if (sock_top && on) show(sock_top, 0);
    if (on) petcmd_draw();
    /* No Info card under it (the catalog's menus/09); back on closing. */
    if (on) hide_info(); else update_info();
    set_visible(housing_root, 0, on);
    if (on) move_to_front(housing_root, 0);
    x360_log(on ? "x360 menu: pet commands shown\r\n" : "x360 menu: pet commands hidden\r\n");
}

static void petcmd_pad(unsigned pressed, int lx) {
    static int held;
    unsigned char *pet = the_pet();
    int dir = (pressed & 0x0004) || (lx < -500 && !held) ? -1 : (pressed & 0x0008) || (lx > 500 && !held) ? 1 : 0;
    held = lx < -500 || lx > 500;
    if (pressed & 0x2000) { petcmd_show(0); return; }                  /* B */
    if (!pet) return;
    if (dir) {
        int stance = *(int *)(pet + GOG_PET_STANCE);
        stance = dir > 0 ? (stance == 0 ? 1 : stance == 1 ? 2 : 0) : (stance == 0 ? 2 : stance == 1 ? 0 : 1);
        *(int *)(pet + GOG_PET_STANCE) = stance;
        if (ui) *(int *)(ui + GOG_UI_PET_STANCE) = stance;
        ((pet_update_fn)GOG_PET_UPDATE)(pet, 0, 0);
        petcmd_draw();
    }
    if (pressed & 0x8000) {                                             /* Y */
        unsigned char *level_ = ui ? *(unsigned char **)(ui + GOG_UI_LEVEL) : 0;
        unsigned char *data = level_ ? *(unsigned char **)(level_ + 0x15c) : 0;
        if (!data || !data[0x89]) { x360_log("x360 menu: the pet cannot depart from here\r\n"); return; }
        {
            unsigned char *owner = *(unsigned char **)(ui + 0x1c);
            void *emitter = owner ? *(void **)(owner + 0x230) : 0;
            if (emitter) ((play_sound_fn)GOG_PLAY_SOUND)(emitter, 0, 0x2b, 0.0f, *(float *)0x00a8ec04);
        }
        ((send_to_town_fn)GOG_SEND_TO_TOWN)(pet, 0, level_);
        x360_log("x360 menu: the pet goes to town\r\n");
        petcmd_show(0);
    }
}

/* ---- the gem's item select (A on a gem) ----
 * The 360's A on a gem (sub_8236AA48 -> sub_8236B240), for a gem in the hero's or the pet's bag
 * (its +492 against theirs): CMessageItemSelectXenon (vtable 0x820D1904, the global UI's +6292)
 * opens (sub_8237ADF0 with 1: the sound 22) in messagehousing_xenon (sub_8237A770), holding a
 * CInventoryMenuXenon of the hero (sub_8237ACD8: its +384 = 2, +388 = 1) that lists the hero's
 * identified equipment with a free socket (sub_82369FE8); its bar has A "Insert" only
 * (sub_8236C8D0, 0x820D1070). Its pad (sub_8237AAF0): A keeps the selected item (+48) and closes;
 * B and the other buttons close with none (sub_8237ADF0 with 0: the sound 66). The inventory then
 * (sub_8236ECC0 -> sub_8236EDD8) sockets the gem in it: the gem out of its bag (sub_822E3218),
 * sub_822BAEF0 (GOG's 0x4b9510, CEquipment's sockets list: +0x3a0 grows while +0x39c is larger),
 * sub_822B5F78 (GOG 0x4c01b0), the owner's inventory recomputed (sub_822E0F08, sub_822E17A8,
 * sub_822E0648: GOG 0x4e7c80, 0x4e9110, 0x4e75d0), a sound; the list then shows the item's
 * category with it selected. With no item: the sound 24. PC's own socketing (a gem dropped on an
 * item, GOG 0x54f474-0x54f5d7) calls the same: playDropSound, 0x4b9510, 0x4c01b0, the UI's
 * sound 0x17, the owner's inventory's three; its gem was taken out of the bag when picked up. */
#define GOG_ADD_SOCKETABLE 0x004b9510   /* thiscall (the item), the gem; ret 4 */
#define GOG_SOCKETS_CHANGED 0x004c01b0  /* thiscall (the item) */
#define GOG_INV_RECOMPUTE_1 0x004e7c80  /* thiscall (CInventory), as after PC's socketing */
#define GOG_INV_RECOMPUTE_2 0x004e9110
#define GOG_INV_RECOMPUTE_3 0x004e75d0
typedef void (__fastcall *add_socketable_fn)(void *item, void *edx, void *gem);
static unsigned char *sock_gem;
static void *sock_gem_bag;

static void sock_draw(void) {
    int f = focus_panel;
    focus_panel = 3;          /* drawn as the focused list */
    select_panel(3);
    refresh();
    focus_panel = f;
    select_panel(f);
}

static void sock_show(int on) {
    if (on && !sock_top) {
        void *root;
        if (!housing_load()) return;
        root = load("media/ui/x360/inventorymenu_xenon.layout", SOCK_PREFIX);
        if (!root) return;
        scale_tree(root, k, 0);
        panels[3].root = root;
        sock_top = win_of(SOCK_PREFIX, "TopFrame");
        if (!sock_top) return;
        add_child(win_of(HOUSING_PREFIX, "parent"), 0, sock_top);
        set_visible(root, 0, 0);
        select_panel(3); clear_samples(); select_panel(focus_panel);
    }
    if (!sock_top) return;
    sock_open = on;
    if (petcmd_root && on) set_visible(petcmd_root, 0, 0);
    show(sock_top, on);
    /* The housing's own "Close" (B) shows with the item select (in-game/11), not with the pet's
     * commands. */
    show(win_of(HOUSING_PREFIX, "closebutton"), on);
    if (on) {
        /* Over the bag's column, 18.34 rows (of 720) higher (in-game/11: its TopFrame at 497.97,
         * 64.22 against the bag's 497.72, 82.56). */
        typedef struct { float top, bottom, left, right; } rect6;
        typedef rect6 *(__fastcall *pixel_rect_fn6)(const void *self, void *edx, rect6 *out);
        static pixel_rect_fn6 rect_of;
        void *bag = win_of(PREFIX, "TopFrame");
        if (!rect_of) rect_of = (pixel_rect_fn6)need("?getUnclippedPixelRect@Window@CEGUI@@QBE?AVRect@2@XZ");
        if (bag && rect_of) {
            rect6 b, t;
            fresh_rects(bag); fresh_rects(sock_top);
            rect_of(bag, 0, &b); rect_of(sock_top, 0, &t);
            move_by(sock_top, b.left - t.left, b.top - 18.34f * k * 768.0f / 720.0f - t.top);
            /* The housing's Close under it, centred (in-game/11: closebutton at 548.13, 649.81
             * of 720, 50.16 right of the TopFrame's left and 585.59 below its top; the layout
             * leaves it at the parent's corner). */
            {
                void *close = win_of(HOUSING_PREFIX, "closebutton");
                rect6 c;
                float s = k * 768.0f / 720.0f;
                if (close) {
                    fresh_rects(sock_top); fresh_rects(close);
                    rect_of(sock_top, 0, &t); rect_of(close, 0, &c);
                    move_by(close, t.left + 50.16f * s - c.left, t.top + 585.59f * s - c.top);
                }
            }
        }
        panels[3].selected[0] = panels[3].top[0] = 0; sock_draw(); hide_info();
    }
    else update_info();
    set_visible(housing_root, 0, on);
    if (on) move_to_front(housing_root, 0);
    shop_sound(on ? 22 : 66);
    x360_log(on ? "x360 menu: the gem's item select shown\r\n" : "x360 menu: the gem's item select hidden\r\n");
}

/* A on a gem in the focused bag (the hero's or the pet's). */
static void sock_begin(unsigned char *gem) {
    sock_gem = gem;
    sock_gem_bag = inventory();
    sock_show(1);
}

static void sock_finish(unsigned char *target) {
    unsigned char *gem = sock_gem;
    void *bag = sock_gem_bag, *inv = hero ? *(void **)(hero + GOG_INVENTORY) : 0;
    sock_gem = 0; sock_gem_bag = 0;
    sock_show(0);
    if (!target || !gem || !bag || !inv || *(unsigned *)(target + 0x39c) == *(unsigned *)(target + 0x3a4)) {
        shop_sound(24);
        x360_log("x360 inv: socket: no item\r\n");
        refresh_all();
        return;
    }
    move_out(bag, gem, -1);
    ((ptr_fn)GOG_DROP_SOUND)(gem, 0, *(void **)(hero + 0x58));
    ((add_socketable_fn)GOG_ADD_SOCKETABLE)(target, 0, gem);
    move_done();
    ((void_fn)GOG_SOCKETS_CHANGED)(target, 0);
    shop_sound(0x17);
    ((void_fn)GOG_INV_RECOMPUTE_1)(inv, 0);
    ((void_fn)GOG_INV_RECOMPUTE_2)(inv, 0);
    ((void_fn)GOG_INV_RECOMPUTE_3)(inv, 0);
    x360_log("x360 inv: socket: the gem is in the item\r\n");
    /* The list on the item's category, the item selected (sub_8236EDD8). */
    if (panels[focus_panel].is_pet) { focus_panel = 0; select_panel(0); }
    filter = category(target);
    if (filter > 5) filter = 0;
    build_list(filter);
    {
        int i;
        for (i = 0; i < list_count; i++) if (list[i] == target) { selected[filter] = i; break; }
    }
    refresh_all();
}

static void sock_pad(unsigned pressed, int ly) {
    static int held;
    int dir = (pressed & 0x0001) || (ly > 500 && !held) ? -1 : (pressed & 0x0002) || (ly < -500 && !held) ? 1 : 0;
    held = ly > 500 || ly < -500;
    if (pressed & 0x1000) {                                              /* A: Insert */
        unsigned char *t;
        select_panel(3); build_list(0);
        t = list_count ? (unsigned char *)list[selected[0]] : 0;
        select_panel(focus_panel);
        sock_finish(t);
        return;
    }
    /* B and the others close it with none; X and Y go to the list (sub_8236A810), not done. */
    if (pressed & ~(0x0001u | 0x0002u | 0x0004u | 0x0008u | 0x4000u | 0x8000u)) { sock_finish(0); return; }
    if (dir) {
        select_panel(3); build_list(0);
        if (selected[0] + dir >= 0 && selected[0] + dir < list_count) { selected[0] += dir; shop_sound(67); }
        select_panel(focus_panel);
        sock_draw();
    }
}

/* The triggers' buttons sway (the catalog's 08 to 12: righttriggerbutton at 1432.6 to 1439.2):
 * the 360 registers lefttriggerbutton and righttriggerbutton (sub_82376728) with speeds 1 and -1
 * in its animated windows (sub_82344BF0: its scale 20, its limit 5) and every frame
 * (sub_821A41A0) moves t by the speed times the frame's seconds, puts the window's x at its
 * place plus 20 t (pixels at 720 rows) and, past |t| = 5 / 20, sets t to the speed times that
 * and turns the speed round; opening the menu puts them back (sub_82376A20). */
static float trigger_t[2], trigger_v[2], trigger_base[2][8];
static unsigned trigger_last;
static int trigger_ready;

static void triggers_reset(void) {
    static const char *const name[2] = {"lefttriggerbutton", "righttriggerbutton"};
    char value[128];
    int i;
    for (i = 0; i < 2; i++) {
        if (!trigger_ready) {
            read_prop(win_of(MASTER, name[i]), "UnifiedAreaRect", value, sizeof value);
            if (!parse_rect(value, trigger_base[i])) return;
        }
        trigger_t[i] = 0;
        trigger_v[i] = i == 0 ? 1.0f : -1.0f;
        write_rect(value, trigger_base[i]);
        prop(win_of(MASTER, name[i]), "UnifiedAreaRect", value);
    }
    trigger_ready = 1;
    trigger_last = GetTickCount();
}

static void triggers_tick(void) {
    static const char *const name[2] = {"lefttriggerbutton", "righttriggerbutton"};
    unsigned now = GetTickCount();
    float dt = (float)(now - trigger_last) / 1000.0f, v[8], s = k * 768.0f / 720.0f;
    char value[128];
    int i, j;
    if (!trigger_ready || dt <= 0) return;
    if (dt > 0.1f) dt = 0.1f;
    trigger_last = now;
    for (i = 0; i < 2; i++) {
        trigger_t[i] += trigger_v[i] * dt;
        for (j = 0; j < 8; j++) v[j] = trigger_base[i][j];
        v[1] += trigger_t[i] * 20.0f * s;
        v[5] += trigger_t[i] * 20.0f * s;
        write_rect(value, v);
        prop(win_of(MASTER, name[i]), "UnifiedAreaRect", value);
        if (trigger_t[i] > 0.25f || trigger_t[i] < -0.25f) {
            trigger_t[i] = trigger_v[i] * 0.25f;
            trigger_v[i] = -trigger_v[i];
        }
    }
}

/* ---- the pause menu (Start in play: the catalog's menus/13 and 14) ----
 * The 360's COptionsMenu with optionsmenu: Pause Menu over the game, its buttons Options, Help,
 * Achievements, Leaderboards and Exit to Title, the focused one in its rollover image (the
 * catalog: Options focused, gem_button_small_rollover); up and down move the focus, A presses,
 * B or Start closes. Help opens howto_xenon (How To Play, B closes it). The trial's Unlock Full
 * Game and Gift a Friend are hidden. Options (the 360's settings screen), Achievements and
 * Leaderboards (Xbox Live) and Exit to Title do nothing yet (section 14). */
#define PAUSE_PREFIX "x360pm_"
static void *pc_window(const char *name);
#define HOWTO_PREFIX "x360ht_"
static void *pause_root, *howto_root;
static int pause_open, howto_open, pause_focus;
static const char *const pause_button[5] = {"i_416", "i_414", "i_404", "Leaderboards", "i_406"};

static void pause_draw(void) {
    int i;
    for (i = 0; i < 5; i++)
        prop(win_of(PAUSE_PREFIX, pause_button[i]), "NormalImage", i == pause_focus ? "set:X360_widgets1 image:gem_button_small_rollover"
                                                                                   : "set:X360_widgets2 image:gem_button_small");
}

static void pause_show(int on) {
    if (on && !pause_root) {
        pause_root = load("media/ui/x360/optionsmenu.layout", PAUSE_PREFIX);
        if (!pause_root) return;
        if (!k) { RECT r; HWND w = active_window(); k = (w && GetClientRect(w, &r) && r.bottom > r.top) ? (float)(r.bottom - r.top) / 768.0f : 1.0f; }
        scale_tree(pause_root, k, 0);
        /* The trial's buttons, hidden in the full game (the catalog's menus/13 has neither). */
        show(win_of(PAUSE_PREFIX, "UnlockFullGame"), 0);
        show(win_of(PAUSE_PREFIX, "invitefriend"), 0);
    }
    if (!pause_root) return;
    pause_open = on;
    /* The 360's pause stops the game (QA run 2: it did not). PC's pause (KEYMAP_PAUSE, CGameUI's
     * update at 0x55de74) is CGameUI's 0x541c60: it flips +0x173d (the flag its updates and label
     * loops test), and shows PC's "Paused" window (+0x9c), hidden here (the 360's menu says it). */
    if (ui && (ui[0x173d] != 0) != (on != 0)) {
        typedef void (__fastcall *self_fn)(void *self, void *edx);
        __try {
            ((self_fn)0x00541c60)(ui, 0);
            if (on && *(void **)(ui + 0x9c)) set_visible(*(void **)(ui + 0x9c), 0, 0);
        } __except (1) { x360_log("x360 menu: PC's pause threw\r\n"); }
    }
    if (on) { pause_focus = 0; pause_draw(); }
    set_visible(pause_root, 0, on);
    if (on) { prop(pause_root, "AlwaysOnTop", "True"); move_to_front(pause_root, 0); }
    x360_log(on ? "x360 menu: pause shown\r\n" : "x360 menu: pause hidden\r\n");
}

static void howto_show(int on) {
    if (on && !howto_root) {
        howto_root = load("media/ui/x360/howto_xenon.layout", HOWTO_PREFIX);
        if (!howto_root) return;
        scale_tree(howto_root, k, 0);
    }
    if (!howto_root) return;
    howto_open = on;
    set_visible(howto_root, 0, on);
    /* Over everything, the HUD's AlwaysOnTop windows too (the 360's veil, i_661 at 0.5, dims the
     * pet's HUD as the rest: the catalog's menus/14). */
    if (on) { prop(howto_root, "AlwaysOnTop", "True"); move_to_front(howto_root, 0); }
}

/* Start opens PC's own pause menu (COptionsMenu, CGameUI's +0x2e8: Settings, Exit Game, Return to
 * Game, used with the mouse), the user's choice for the first beta (2026-10-23: the 360's pause
 * menu with PC's screens behind it left a window with no way out). Its setOpen (the virtual +0x1c)
 * opens and closes it; PC pauses the game under it. */
static unsigned char *pc_pause_menu(void) {
    unsigned char *m;
    /* Only in play: at the title CGameUI is not whole. COptionsMenu is a CMenu, not a CSubMenu as
     * the shops' screens: its vtable (0xab7d78) has the open state at +0x18 (its onClick 0x591050
     * acts only then) and setOpen (thiscall, bool) at slot 6, 0x591200; slot 4 and 7 are others
     * (QA run 4: calling the shops' isOpen slot on it closed the game at the main menu). */
    if (!find_world() || !ui) return 0;
    m = *(unsigned char **)(ui + 0x2e8);
    return m && *(unsigned *)m == 0x00ab7d78 ? m : 0;
}
static int pc_pause_open(void) {
    unsigned char *m = pc_pause_menu();
    return m && m[0x18];
}
/* Asked by the pad (inside PC's GetCursorPos), done from the frame tick: the earlier try that
 * called 0x591200 from the pad's hook froze the game. 1 open, 2 close. */
static int pc_pause_request;
static void pc_pause_tick(void) {
    typedef void (__fastcall *open_fn)(void *self, void *edx, int open);
    unsigned char *m;
    int want = pc_pause_request;
    if (!want) return;
    pc_pause_request = 0;
    m = pc_pause_menu();
    if (!m || (m[0x18] != 0) == (want == 1)) return;
    ((open_fn)0x00591200)(m, 0, want == 1);
    x360_log(want == 1 ? "x360 menu: PC's pause menu opened\r\n" : "x360 menu: PC's pause menu closed\r\n");
}

int x360_pause_open(void) { return pause_open || howto_open || pc_pause_open(); }

/* The pad in play: Start opens the pause menu; while it (or How To Play) is open, its keys. */
static int x360_pc_settings_up;   /* PC's settings shown (pc_menus_nav_tick) */
static int x360_pc_die_up;        /* PC's resurrection screen has the pad (pc_menu_tick) */
int x360_pc_menu_up(void) { return x360_pc_settings_up || x360_pc_die_up; }
unsigned x360_pad_eaten;          /* buttons PC's menus used, kept from the play pad until let go (proxy/pad.c) */
void x360_pause_pad(unsigned pressed, int ly) {
    static int held;
    int dir = (pressed & 0x0001) || (ly > 500 && !held) ? -1 : (pressed & 0x0002) || (ly < -500 && !held) ? 1 : 0;
    held = ly > 500 || ly < -500;
    if (!resolve()) return;
    (void)dir;
    if (pc_pause_menu()) {
        int open = pc_pause_open();
        /* Not while PC's settings are up over it: B is their Cancel there (pc_menus_nav_tick). */
        if (!x360_pc_settings_up && ((pressed & 0x0010) || (open && (pressed & 0x2000)))) pc_pause_request = open ? 2 : 1;   /* Start; B closes it */
        return;
    }
    if (howto_open) {   /* the pause menu hidden under it (the catalog's menus/14), back after */
        if (pressed & 0x2000) { howto_show(0); if (pause_root) set_visible(pause_root, 0, 1); }
        return;
    }
    if (!pause_open) { if (pressed & 0x0010) pause_show(1); return; }
    if (pressed & (0x2000 | 0x0010)) { pause_show(0); return; }      /* B, Start */
    if (dir) { pause_focus = (pause_focus + (dir > 0 ? 1 : 4)) % 5; pause_draw(); }
    if (pressed & 0x1000) {                                            /* A */
        x360_log("x360 menu: pause A on "); x360_log_num(pause_focus); x360_log("\r\n");
        if (pause_focus == 1) { howto_show(1); if (howto_open) set_visible(pause_root, 0, 0); }
        /* Options and Exit to Title are PC's (the user's scope): PC's own pause menu
         * (COptionsMenu, CGameUI's +0x2e8; optionsmenu.layout: Settings, ExitGame,
         * ReturnToGame) is opened and its button pressed from the frame tick (pc_menu_tick):
         * the earlier tries did both at once from the pad's hook and froze the game. */
        if (pause_focus == 0 || pause_focus == 4) {
            extern int x360_pc_menu_request;
            x360_pc_menu_request = pause_focus == 0 ? 1 : 2;   /* 1 Settings, 2 ExitGame */
            pause_show(0);
        }
    }
}

/* A fingerprint of the inventory: its items, their slots and stacks. PC changes it outside our
 * buttons too (useItem takes a potion out while the hero drinks and puts the rest back; pickups),
 * and the 360's screen hears of every change (iInventoryListener): ours rebuilds when it moves. */
static unsigned fingerprint_of(unsigned char *inv) {
    void **refs;
    unsigned n, i, h = 2166136261u;
    if (!inv) return 0;
    refs = *(void ***)(inv + INV_REFS);
    n = *(unsigned *)(inv + INV_REFS + 4);
    for (i = 0; i < n && i < 256; i++) {
        unsigned char *ref = (unsigned char *)refs[i], *item;
        if (!ref || !(item = *(unsigned char **)(ref + REF_ITEM))) continue;
        h = (h ^ (unsigned)(unsigned long)item) * 16777619u;
        h = (h ^ *(unsigned *)(ref + REF_SLOT)) * 16777619u;
        h = (h ^ *(unsigned *)(item + GOG_STACK)) * 16777619u;
    }
    return h ^ n;
}

static unsigned fingerprint(void) {
    unsigned char *pet = the_pet();
    unsigned h = fingerprint_of(hero ? *(unsigned char **)(hero + GOG_INVENTORY) : 0);
    h = h * 31u + fingerprint_of(owner_bag(shop));
    return h * 31u + fingerprint_of(pet ? *(unsigned char **)(pet + GOG_INVENTORY) : 0);
}

/* ---- the shop: the 360's screen while PC's merchant menu is open ----
 * PC opens its CMerchantMenu (CGameUI's +0x2d8, vtable 0xab7b58) when the hero talks to a merchant
 * (CGameUI sets its owner, the virtual +0x18, then opens it, the virtual +0x1c with 1: GOG
 * 0x557d3f); its open flag is +0x30 (its virtual +0x10, 0x57e850), the merchant +0x28. While it is
 * open the 360's frame shows the merchant's column with the bags (CMerchantMenuXenon,
 * sub_82378D30); B closes both (PC's own close: the virtual +0x1c with 0, GOG 0x558413). */
#define GOG_MERCHANT_MENU 0x2d8
/* The stash's the same way: CStashMenu (CGameUI's +0x2e4, vtable 0xab8730), its owner the chest
 * the hero used (the hero's +0x290; CGameUI 0x5582c3 for the stash, 0x55832e for the shared one),
 * opened with PC's inventory; the 360's CStashMenuXenon (vtable 0x8200149c) puts the stash in the
 * first column (sub_82383630: its +0x104, the bag +0x100, the pet's +0x108). */
#define GOG_STASH_MENU 0x2e4
/* The transmuter's: CCombineMenu (CGameUI's +0x2e0, vtable 0xab2c4c, made at 0x562ff7), its owner
 * +0x4c (the transmuter the hero used: CGameUI 0x557ddf), the character +0x50, open +0x54 (its
 * virtual +0x10, 0x56a350), opened with PC's inventory too (0x557dcd). */
#define COMBINE_OWNER 0x4c
#define COMBINE_OPEN 0x54
#define COMBINE_SLOTS 0x7c
#define MENU_OPEN 0x30
#define MERCHANT_OWNER 0x28
#define MENU_SET_OPEN (0x1c / 4)
#define MENU_IS_OPEN (0x10 / 4)
/* PC opens its own inventory with the merchant's (CGameUI's +0x2cc, the virtual +0x10 its open
 * flag) and closes the merchant's when it closes it (GOG 0x54d465): it stays open under the frame,
 * its windows shown after the frame opened hidden with the others, its doll's viewport taken off;
 * it is closed with the shop. */
static void pc_inventory_close(void) {
    typedef unsigned char (__fastcall *is_open_fn)(void *self, void *edx);
    unsigned char *m = ui ? *(unsigned char **)(ui + GOG_INVENTORY_MENU) : 0;
    if (m && ((is_open_fn)(*(void ***)m)[MENU_IS_OPEN])(m, 0)) {
        ((set_bool_fn)(*(void ***)m)[MENU_SET_OPEN])(m, 0, 0);
        x360_log("x360 shop: PC's inventory closed\r\n");
    }
}

/* PC's transmuter, as it closes (CCombineMenu's setOpen, GOG 0x569220), gives each item in its
 * places back where its own map says it came from (0x568fe0: the map made as it opened;
 * the slot if it is free and at most 18, by 0x4ea200, else the bag by pickupEquipment). The items
 * the pad's X put there are not in that map as PC's drag puts them: an equipped sword was mapped
 * to its slot 0 when the transmuter opened, and closing left the hero two references to it, one
 * in slot 0 and one in the bag (the owner, 2026-10-25; the references logged around the close).
 * So those items are given back first, the same way PC does (removeEquipment, then 0x4ea200 to
 * the slot X took it from, else pickupEquipment), and PC finds the places empty. */
#define GOG_PUT_BACK_AT 0x004ea200          /* CInventory: the item into a slot (item, slot), as PC's transmuter gives it back */
static void transmuter_give_back(void) {
    typedef unsigned char (__fastcall *put_back_fn)(void *inv, void *edx, void *item, int slot);
    void *bag = owner_bag(hero);
    int j;
    if (!bag || !tm_known) return;
    for (j = 0; j < 4; j++) {
        unsigned char *item;
        int from = tm_from[j];
        tm_from[j] = -1;
        if (from < 0 || !(item = (unsigned char *)((item_at_fn)GOG_ITEM_AT_SLOT)(bag, 0, tm_slot[j]))) continue;
        move_out(bag, item, tm_slot[j]);
        if (tm_from_inv[j] && tm_from_inv[j] != bag) {   /* the pet's: back to its bag, its slot if free */
            if (give_back(tm_from_inv[j], from, item)) { x360_log("x360 transmuter: closing: an item given back to the pet's bag\r\n"); move_done(); continue; }
        }
        if (tm_from_inv[j] == bag && from <= 18 && !((item_at_fn)GOG_ITEM_AT_SLOT)(bag, 0, from) && ((put_back_fn)GOG_PUT_BACK_AT)(bag, 0, item, from)) {
            x360_log("x360 transmuter: closing: an item given back to its slot "); x360_log_num(from); x360_log("\r\n");
        } else if (((pickup_fn)GOG_PICKUP_EQUIPMENT)(bag, 0, item, 1)) {
            x360_log("x360 transmuter: closing: an item given back to the bag\r\n");
        } else {
            to_ground(item);
            x360_log("x360 transmuter: closing: an item given back to the ground (the bag full)\r\n");
        }
        move_done();
    }
}
/* Whoever closes PC's transmuter (the pad's B or Start through shop_close, a key, the game's
 * closing of every menu), the items are given back first: its setOpen (CCombineMenu's vtable
 * +0x1c, GOG 0x569220) is replaced in the vtable by this one, which gives them back on a close and
 * calls PC's. */
static set_bool_fn combine_set_open;
static void __fastcall combine_set_open_hook(void *self, void *edx, int on) {
    if (!on && ((unsigned char *)self)[COMBINE_OPEN]) __try { transmuter_give_back(); } __except (1) { move_cut_short(); }
    combine_set_open(self, edx, on);
}
static void combine_hook_install(void) {
    void **slot = (void **)0x00ab2c4c + MENU_SET_OPEN;   /* CCombineMenu's vtable */
    unsigned long old;
    if (combine_set_open) return;
    if (*slot != (void *)0x00569220) { x360_log("x360 transmuter: its setOpen not as read: the give-back on closing left out\r\n"); combine_set_open = (set_bool_fn)1; return; }
    if (!VirtualProtect(slot, 4, 0x04 /* PAGE_READWRITE */, &old)) return;
    combine_set_open = (set_bool_fn)*slot;
    *slot = (void *)combine_set_open_hook;
    VirtualProtect(slot, 4, old, &old);
    x360_log("x360 transmuter: its setOpen hooked (the pad's items given back on closing)\r\n");
}
static void shop_close(void) {
    unsigned char *m = ui ? *(unsigned char **)(ui + (at_enchant ? GOG_ENCHANT_MENU : at_transmute ? GOG_COMBINE_MENU : at_stash ? GOG_STASH_MENU : GOG_MERCHANT_MENU)) : 0;
    int open = m && (at_enchant ? m[ENCHANT_OPEN] : at_transmute ? m[COMBINE_OPEN] : m[MENU_OPEN]);
    shop_closing = 1;
    if (inventory_open) x360_inventory_toggle();
    shop_closing = 0;
    shop = 0;
    at_stash = 0;
    at_transmute = 0;
    at_enchant = 0;
    tm_shop = 0;
    if (open) ((set_bool_fn)(*(void ***)m)[MENU_SET_OPEN])(m, 0, 0);
    pc_inventory_close();
    x360_log("x360 shop: closed\r\n");
}

/* The 360 has a menu for the merchants and another for the stashes, each its own columns: the
 * left column's filter and rows are kept for each (the stash's not left at the merchant's). */
static void left_column_for(int stash) {
    static struct { int filter, selected[6], top[6]; } kept[3];
    static int kind = 0;
    int j;
    if (stash == kind) return;
    if (active_panel == 2) select_panel(0);   /* the column's state out of the globals first */
    kept[kind].filter = panels[2].filter;
    for (j = 0; j < 6; j++) { kept[kind].selected[j] = panels[2].selected[j]; kept[kind].top[j] = panels[2].top[j]; }
    kind = stash;
    panels[2].filter = kept[kind].filter;
    for (j = 0; j < 6; j++) { panels[2].selected[j] = kept[kind].selected[j]; panels[2].top[j] = kept[kind].top[j]; }
}

/* The window CEGUI has under the mouse (System::getWindowContainingMouse), its name and its
 * parents', logged after label. */
static void log_under_mouse(const char *label) {
    typedef void *(__fastcall *under_fn)(const void *self, void *edx);
    typedef const void *(__fastcall *name_fn)(const void *self, void *edx);
    typedef void *(__fastcall *parent_fn)(const void *self, void *edx);
    static under_fn under; static name_fn gn; static parent_fn gp;
    void *sys, *w;
    int depth = 0;
    if (!under) under = (under_fn)need("?getWindowContainingMouse@System@CEGUI@@QBEPAVWindow@2@XZ");
    if (!gn) gn = (name_fn)need("?getName@Window@CEGUI@@QBEABVString@2@XZ");
    if (!gp) gp = (parent_fn)need("?getParent@Window@CEGUI@@QBEPAV12@XZ");
    sys = system_ptr();
    if (!under || !gn || !gp || !sys) return;
    w = under(sys, 0);
    x360_log(label);
    for (; w && depth < 6; w = gp(w, 0), depth++) { x360_log(depth ? " < " : " "); x360_log(c_str(gn(w, 0), 0)); }
    if (!depth) x360_log(" nothing");
    x360_log("\r\n");
}

/* A diagnosis (PC's dialogs did not take a click, docs 14): at each left click, the window under
 * the mouse. */
static void log_click_window(void) {
    static int was_down;
    int down = (GetAsyncKeyState(0x01) & 0x8000) != 0;
    if (down == was_down) return;
    was_down = down;
    if (down) log_under_mouse("x360 click: under the mouse");
}

void x360_shop_tick(void) {
    log_click_window();
    unsigned char *m, *st, *tm, *en;
    if (!find_world() || !ui) return;
    m = *(unsigned char **)(ui + GOG_MERCHANT_MENU);
    st = *(unsigned char **)(ui + GOG_STASH_MENU);
    tm = *(unsigned char **)(ui + GOG_COMBINE_MENU);
    en = *(unsigned char **)(ui + GOG_ENCHANT_MENU);
    if (!shop && en && en[ENCHANT_OPEN] && *(unsigned char **)(en + ENCHANT_OWNER)) {
        static int logged_mode;
        int mode = *(int *)(en + ENCHANT_MODE);
        if (mode == 0x15 || mode == 0x19 || mode == 0x1a || mode == 0x1b) {
            if (inventory_open) x360_inventory_toggle();
            shop = *(unsigned char **)(en + ENCHANT_OWNER);
            at_enchant = mode;
            ench_result[0] = 0;
            x360_log(mode == 0x15 ? "x360 shop: PC's enchant menu opened (the enchanter)\r\n"
                     : mode == 0x1b ? "x360 shop: PC's enchant menu opened (the retirement NPC)\r\n"
                     : "x360 shop: PC's enchant menu opened (a gem goblin)\r\n");
            x360_inventory_toggle();
        } else if (mode != logged_mode) {
            logged_mode = mode;
            x360_log("x360 shop: PC's enchant menu opened in mode ");
            x360_log_num(mode);
            x360_log(", left to PC\r\n");
        }
    } else if (!shop && tm && tm[COMBINE_OPEN] && *(unsigned char **)(tm + COMBINE_OWNER)) {
        int j;
        if (inventory_open) x360_inventory_toggle();
        for (j = 0; j < 4; j++) { tm_slot[j] = ((int *)(tm + COMBINE_SLOTS))[j]; tm_from[j] = -1; tm_from_inv[j] = 0; }
        tm_known = 1;
        combine_hook_install();
        shop = tm_shop = *(unsigned char **)(tm + COMBINE_OWNER);
        at_transmute = 1;
        left_column_for(2);
        x360_log("x360 shop: PC's transmuter menu opened, its slots");
        for (j = 0; j < 4; j++) { x360_log(" "); x360_log_num(tm_slot[j]); }
        x360_log(isa(shop, 134) ? " (type 134)\r\n" : " (not of type 134)\r\n");
        x360_inventory_toggle();
    } else if (!shop && st && st[MENU_OPEN] && *(unsigned char **)(st + MERCHANT_OWNER)) {
        if (inventory_open) x360_inventory_toggle();
        shop = *(unsigned char **)(st + MERCHANT_OWNER);
        at_stash = 1;
        left_column_for(1);   /* 0 merchants, 1 stashes, 2 the transmuter */
        x360_log(isa(shop, 170) ? "x360 shop: PC's stash menu opened (the shared stash)\r\n" : "x360 shop: PC's stash menu opened\r\n");
        {
            /* Which bag the stash has (docs 21: the personal one's ctor branch, ISA 128's 42 or the
             * last one's, which bag=360 makes 50). */
            unsigned char *inv = (unsigned char *)owner_bag(shop);
            int *starts = inv ? *(int **)(inv + INV_PANE_STARTS) : 0;
            x360_log("x360 shop:   the stash: ISA 128 "); x360_log(isa(shop, 128) ? "yes" : "no");
            x360_log(", ISA 170 "); x360_log(isa(shop, 170) ? "yes" : "no");
            if (inv) {
                x360_log(", slots "); x360_log_num(*(long *)(inv + INV_SLOTS));
                x360_log(", sections "); x360_log_num(starts ? (long)((*(int **)(inv + INV_PANE_STARTS + 4)) - starts) : 0);
            }
            x360_log("\r\n");
        }
        x360_inventory_toggle();
    } else if (m && m[MENU_OPEN] && !shop && *(unsigned char **)(m + MERCHANT_OWNER)) {
        if (inventory_open) x360_inventory_toggle();
        shop = *(unsigned char **)(m + MERCHANT_OWNER);
        at_stash = 0;
        left_column_for(0);
        x360_log(isa(shop, 127) ? "x360 shop: PC's merchant menu opened (the Gambler)\r\n"
                                : "x360 shop: PC's merchant menu opened\r\n");
        x360_inventory_toggle();
    } else if (shop && inventory_open) {
        unsigned char *inv_menu = pc_inventory_menu();
        if (inv_menu && *(void **)(inv_menu + INVMENU_VIEWPORT)) show_doll_of(0, 0);
        hide_pc_ui(1);
    }
    if (at_stash) m = st;
    if (at_enchant && ench_pending) return;   /* PC's modal closes its menu: enchant_result_tick opens it again */
    if (shop && (at_enchant ? !(en && en[ENCHANT_OPEN]) : at_transmute ? !(tm && tm[COMBINE_OPEN]) : !(m && m[MENU_OPEN]))) {
        x360_log(at_enchant ? "x360 shop: PC's enchant menu closed\r\n" : at_transmute ? "x360 shop: PC's transmuter menu closed\r\n"
                 : at_stash ? "x360 shop: PC's stash menu closed\r\n" : "x360 shop: PC's merchant menu closed\r\n");
        shop_closing = 1;
        if (inventory_open) x360_inventory_toggle();
        shop_closing = 0;
        shop = 0;
        at_stash = 0;
        at_transmute = 0;
        at_enchant = 0;
        tm_shop = 0;
        /* PC closes its inventory with the merchant's menu, not with the enchant menu it closes
         * after an enchanting (run B: its inventory stayed open, over the 360's doll). */
        pc_inventory_close();
    }
}

/* The pad while the inventory is open: pressed, the buttons just pressed; ly, the stick (of 1000).
 * Up and down (the stick or the d-pad) move the selection, repeating while held; LB and RB the
 * filter; A acts on the row; B or Back closes. */
static void inventory_pad(unsigned pressed, unsigned buttons, int lx, int ly);
static int stop_asked;   /* a stop was decided (stop_unsaved): the menus take no input after it */
static unsigned short stop_wbody[700];   /* the stop's text (also declared below) */
void x360_inventory_pad(unsigned pressed, unsigned buttons, int lx, int ly) {
    static int logged;
    if (stop_asked) return;
    __try { inventory_pad(pressed, buttons, lx, ly); }
    __except (1) { if (!logged++) x360_log("x360 inv: exception in the menus' input (logged once)\r\n"); }
    __try { move_cut_short(); }
    __except (1) {
        /* The last-resort box needs a text: a fixed one if the stop's was not made. */
        static const char fallback[] = "A move between bags was interrupted. Nothing is saved, nothing is lost: the game closes.";
        unsigned i;
        x360_save_held = 1;
        if (!stop_wbody[0]) { for (i = 0; fallback[i] && i + 1 < 700; i++) stop_wbody[i] = (unsigned char)fallback[i]; stop_wbody[i] = 0; }
        stop_asked = 1;
        x360_log("x360 inv: exception stopping a cut move: saves held\r\n");
    }
}
static void inventory_pad(unsigned pressed, unsigned buttons, int lx, int ly) {
    static unsigned next_repeat;
    static int held_dir;
    unsigned now = GetTickCount();
    int dir = 0;
    static unsigned last_print, last_check;
    if (!inventory_open || !find_world()) return;
    triggers_tick();
    if (petcmd_open) { petcmd_pad(pressed, lx); return; }
    if (sock_open) { sock_pad(pressed, ly); return; }
    if (skillmap_pad(pressed, buttons)) return;               /* the Skills page's mapping open */
    if (pressed & 0x2000) {                                            /* B */
        if (shop) shop_close();
        else x360_inventory_toggle();
        return;
    }
    /* Start at an NPC's screen closes it, as Back (the owner's look at the 360, 2026-10-24; the
     * 360's frame update closes the open menus on Start, sub_8233E998). */
    if ((pressed & 0x0010) && shop) { shop_close(); return; }
    /* LT and RT: the page before and after (sub_82377990, sub_82377928); a merchant's frame has
     * one page. */
    if ((pressed & (0x10000 | 0x20000)) && !shop) {
        int step = pressed & 0x10000 ? 3 : 1, p = (page + step) % 4;
        set_page(p);
        return;
    }
    if (page == 1) { stats_pad(pressed, ly); return; }
    if (page == 2) { skills_pad(pressed, lx, ly); return; }
    if (page == 3) { quest_pad(pressed, ly); return; }
    if ((pressed & 0x0010) && !shop) { petcmd_show(1); return; }     /* Start: the pet's commands */
    if (now - last_check >= 100) {
        unsigned print = fingerprint();
        last_check = now;
        if (print != last_print) { last_print = print; refresh_all(); }
    }
    if (buttons & 0x0001 || ly > 500) dir = -1;                      /* up */
    if (buttons & 0x0002 || ly < -500) dir = 1;                      /* down */
    if (dir != held_dir) { held_dir = dir; next_repeat = 0; }
    if (dir && (!next_repeat || now >= next_repeat)) {
        int s = selected[filter] + dir;
        if (s >= 0 && s < list_count) selected[filter] = s;
        next_repeat = now + (next_repeat ? 120 : 350);
        refresh_all();
    }
    /* The 360's inventory (sub_8236FEF0; its jump table 0x8236FFE4, Runic's codes from XInput's
     * keys, sub_822DF308): RB and LB (10, 12) change the column without the filter
     * (sub_823702E0 with 0); the left stick's left and right (19, 20) change the filter, and past
     * the first or the last go to the column before or after with the last or the first filter
     * (0x82370180, sub_823653C8, sub_823702E0). The merchant's menu (sub_8237A0A8) sends the same
     * codes the same way. The transmuter's column has no filters: the stick goes to the next
     * column. */
    {
        static int lx_held;
        int fdir = !lx_held && lx > 500 ? 1 : !lx_held && lx < -500 ? -1 : 0;
        lx_held = lx > 500 || lx < -500;
        if (pressed & (0x0100 | 0x0200)) {                                /* LB, RB: the column */
            if (panels[1].root || shop) {
                focus_panel = column_panel((panel_column(focus_panel) + (pressed & 0x0100 ? columns() - 1 : 1)) % columns());
                select_panel(focus_panel); show_doll(1);
                shop_sound(67);
                refresh_all();
            }
        }
        if (fdir && at_transmute && panels[focus_panel].is_shop) {
            focus_panel = column_panel((panel_column(focus_panel) + (fdir < 0 ? columns() - 1 : 1)) % columns());
            select_panel(focus_panel); filter = fdir < 0 ? 5 : 0;
            refresh_all();
        } else if (fdir < 0) {                                             /* the stick left */
            if (filter > 0) filter--;
            else if (panels[1].root) {
                focus_panel = column_panel((panel_column(focus_panel) + columns() - 1) % columns());
                select_panel(focus_panel); filter = 5; show_doll(1);
            }
            shop_sound(67);
            refresh_all();
        } else if (fdir > 0) {                                             /* the stick right */
            if (filter < 5) filter++;
            else if (panels[1].root) {
                focus_panel = column_panel((panel_column(focus_panel) + 1) % columns());
                select_panel(focus_panel); filter = 0; show_doll(1);
            }
            shop_sound(67);
            refresh_all();
        }
    }
    if (pressed & 0x000c) { compare_on = !compare_on; refresh_all(); } /* d-pad left or right: item compare */
    /* A does nothing in the merchant's column (sub_8236AA48, ISA 41); Y nothing at a merchant
     * (sub_8236A810, the merchant's menu: mode 4); X buys or sells there (sub_8236AD18). */
    /* At the transmuter (mode 5) A shows nothing and is not done here; Y transmutes. */
    if ((pressed & 0x1000) && at_enchant == 0x1b) { retire(); refresh_all(); }               /* A at the retirement NPC */
    else if ((pressed & 0x1000) && at_enchant) { enchant(); refresh_all(); }                  /* A at the enchanter */
    else if ((pressed & 0x1000) && !panels[active_panel].is_shop && !at_transmute) { act(); refresh_all(); }    /* A */
    if ((pressed & 0x8000) && at_transmute) { transmute(); refresh_all(); }               /* Y */
    else if ((pressed & 0x8000) && (!shop || at_stash)) { drop(); refresh_all(); }       /* Y (a stash: mode 7/8) */
    if ((pressed & 0x4000) && !at_enchant) {                                             /* X (none at the enchanter) */
        if (at_transmute) transmute_swap();
        else if (at_stash) stash_swap();
        else if (!shop) swap_to_pet();
        else if (panels[active_panel].is_shop) buy();
        else sell();
        refresh_all();
    }
}

/* ---- test runs: the game's console commands ----
 * console= in x360_ui.ini (test runs only): each command, split at ';', is given once, at the first
 * opening of the inventory (run earlier, while the level loaded, the items never appeared), to
 * CConsole::executeCommandLine (OT's name; GOG 0x56cd00, ret 0x1c: a std::wstring by value, which
 * it destroys). The console is CGameUI's +0x13ec (created at 0x5636a9). Its commands include ITEM
 * name,count, SPAWNCLASS name,count,level, MONEY count, LEVELUP (the game's HELP lists them). */
extern char x360_console_commands[400];
#define GOG_CONSOLE 0x13ec
#define GOG_EXECUTE_COMMAND_LINE 0x0056cd00
typedef void (__fastcall *execute_fn)(void *console, void *edx, wstring_by_value command);
typedef void *(__fastcall *wctor_fn)(void *self, void *edx, const unsigned short *text);

void x360_run_console(void) {
    static int done;
    static wctor_fn wctor;
    unsigned short wide[200];
    const char *c = x360_console_commands;
    void *console;
    if (done || !c[0] || !find_world() || !ui) return;
    console = *(void **)(ui + GOG_CONSOLE);
    if (!wctor) {
        void *m = (void *)0;
        m = GetModuleHandleA("msvcp90.dll");
        wctor = m ? (wctor_fn)GetProcAddress(m, "??0?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QAE@PB_W@Z") : 0;
    }
    done = 1;
    if (!console || !wctor) { x360_log("x360 console: no console or no wstring constructor\r\n"); return; }
    while (*c) {
        unsigned n = 0;
        wstring_by_value cmd;
        while (*c && *c != ';' && n < 199) wide[n++] = (unsigned char)*c++;
        wide[n] = 0;
        if (*c == ';') c++;
        if (!n) continue;
        {
            /* Our own: X360PICKUP picks up what lies around (proxy/pad.c auto_pickup). */
            extern int x360_auto_pickup;
            static const unsigned short pick[] = {'X', '3', '6', '0', 'P', 'I', 'C', 'K', 'U', 'P', 0};
            unsigned q = 0;
            while (pick[q] && wide[q] == pick[q]) q++;
            if (!pick[q] && !wide[q]) { x360_auto_pickup = 40; continue; }
        }
        wctor(&cmd, 0, wide);
        x360_log("x360 console: running a command\r\n");
        __try { ((execute_fn)GOG_EXECUTE_COMMAND_LINE)(console, 0, cmd); } __except (1) { x360_log("x360 console: it threw\r\n"); }
    }
}

/* ---- the 360's HUD (hud.layout) ---- */

/* PC's window whose name is `name` or ends in "_" + name (the layouts' prefixes), under the GUI
 * sheet (0 if none). */
static void *pc_window(const char *name) {
    typedef const void *(__fastcall *name_fn)(const void *self, void *edx);
    static name_fn gn;
    void *sys = system_ptr(), *sheet = sys ? get_gui_sheet(sys, 0) : 0, *stack[256];
    int depth[256], sp = 0;
    unsigned nl = length(name);
    if (!gn) gn = (name_fn)need("?getName@Window@CEGUI@@QBEABVString@2@XZ");
    if (!sheet || !gn) return 0;
    stack[sp] = sheet; depth[sp++] = 0;
    while (sp > 0) {
        void *w = stack[--sp];
        int d = depth[sp];
        const char *wn = c_str(gn(w, 0), 0);
        unsigned wl = length(wn), c, n;
        if (is_ours(w)) continue;                       /* not into our own layouts */
        if (wl >= nl) {
            const char *a = wn + wl - nl, *b = name;
            while (*b && *a == *b) a++, b++;
            if (!*b && (wl == nl || wn[wl - nl - 1] == '_')) return w;
        }
        n = child_count(w, 0);
        for (c = 0; c < n && d < 12 && sp < 255; c++) { stack[sp] = child_at(w, 0, c); depth[sp++] = d + 1; }
    }
    return 0;
}

#define HUD_PREFIX "x360hud_"
#define PETHUD_PREFIX "x360pethud_"
static void *pc_hud_root;
/* In play: PC's HUD is up (the main menu has a hero and a level loaded behind it, but no HUD). */
static int hud_in_play;
int x360_in_play(void) { return hud_in_play; }

/* A bar of the 360's HUD filled as PC's own of the same name: PC shrinks the bar (a clipping
 * window) from its top, or for experience from its right, and moves its image ("...Sub") the
 * other way so the image stays put (a run: PlayerHealthBar {472.5, 23.5, 525.9, 130.8} and its
 * Sub {0, -8.1, 53.4, 107.2} at 0.93 of the hero's health). The fraction is the bar's height (or
 * width) over its image's; the 360's bar and image of the same names are set to it, from their
 * layout's full rects (full[8], scaled). */
static void set_bar(const char *prefix, const char *bar, const char *sub, int horizontal, float full[8], float f);

static void mirror_bar_of(const char *prefix, const char *bar, const char *sub, int horizontal, float full[8]) {
    void *pb = pc_window(bar), *ps = pc_window(sub);
    char value[128];
    float b[8], i[8], size;
    if (!pb || !ps) return;
    read_prop(pb, "UnifiedAreaRect", value, sizeof value);
    if (!parse_rect(value, b)) return;
    read_prop(ps, "UnifiedAreaRect", value, sizeof value);
    if (!parse_rect(value, i)) return;
    size = horizontal ? i[5] - i[1] : i[7] - i[3];
    set_bar(prefix, bar, sub, horizontal, full, size > 0 ? (horizontal ? b[5] - b[1] : b[7] - b[3]) / size : 0);
}

/* The 360 bar `bar` of `prefix` filled to the fraction f; `full`, its full rect, read once. */
static void set_bar(const char *prefix, const char *bar, const char *sub, int horizontal, float full[8], float f) {
    void *xb = win_of(prefix, bar), *xs = win_of(prefix, sub);
    char value[128];
    float v[8];
    int j;
    if (!xb) return;
    if (!full[0] && !full[1] && !full[5] && !full[7]) {          /* the 360 bar's full rect, once */
        read_prop(xb, "UnifiedAreaRect", value, sizeof value);
        if (!parse_rect(value, full)) return;
    }
    if (f < 0) f = 0;
    if (f > 1) f = 1;
    for (j = 0; j < 8; j++) v[j] = full[j];
    if (horizontal) v[5] = v[1] + (full[5] - full[1]) * f;
    else v[3] = v[7] - (full[7] - full[3]) * f;
    write_rect(value, v); prop(xb, "UnifiedAreaRect", value);
    if (!xs) return;                                  /* the 360's experience bar is its own image */
    read_prop(xs, "UnifiedAreaRect", value, sizeof value);
    if (!parse_rect(value, v)) return;
    if (horizontal) { float w = v[5] - v[1]; v[1] = 0; v[5] = w; }
    else { float h = v[7] - v[3]; v[3] = -(full[7] - full[3]) * (1 - f); v[7] = v[3] + h; }
    write_rect(value, v); prop(xs, "UnifiedAreaRect", value);
}

static void mirror_bar(const char *bar, const char *sub, int horizontal, float full[8]) {
    mirror_bar_of(HUD_PREFIX, bar, sub, horizontal, full);
}

/* A window of the 360's HUD shown as PC's of the same name, with its text; with own_only, as PC's
 * window itself is, its sheet's hiding not counted (the 360's inventory hides PC's sheets). */
static void mirror_text_shown(const char *prefix, const char *name, int own_only) {
    void *p = pc_window(name), *x = win_of(prefix, name);
    char value[256];
    static is_visible_fn vis;
    if (!vis) vis = (is_visible_fn)need("?isVisible@Window@CEGUI@@QBE_N_N@Z");
    if (!x) return;
    if (!p || !vis) { show(x, 0); return; }
    read_prop(p, "Text", value, sizeof value);
    prop(x, "Text", value);
    show(x, vis(p, 0, own_only ? 1 : 0));
}
static void mirror_text_of(const char *prefix, const char *name) { mirror_text_shown(prefix, name, 0); }

static void mirror_text(const char *name) { mirror_text_of(HUD_PREFIX, name); }

/* The large message (the place's title on entering a level): the 360 shows it at once and fades it
 * out. sub_8233E4F8(hud, text, seconds) sets the text, a hold (HUD +6116) of `seconds` and the
 * alpha (+6120) to 1.0; the HUD update (sub_821A83C8, 0x821AA3C0) counts the hold down by the
 * frame's time and then takes the alpha down by 2.0 a second (0x820AA33C), writing it to
 * TextColour (white) and DropTextColour (black), and hides the text at 0. The hold is PC's own
 * (the same game code passes it): while PC's text is shown the 360's is at 1.0, and when PC hides
 * it the 360's fades out over 0.5 s. */
static void large_message_tick(unsigned now) {
    void *p = pc_window("LargeMessageText"), *x = win_of(HUD_PREFIX, "LargeMessageText");
    char value[256], colour[16];
    static is_visible_fn vis;
    static unsigned fade_from;
    static int alpha_shown = -1;      /* the alpha last written, in 1/255 */
    int alpha;
    if (!vis) vis = (is_visible_fn)need("?isVisible@Window@CEGUI@@QBE_N_N@Z");
    if (!x) return;
    if (p && vis && vis(p, 0, 0)) {
        read_prop(p, "Text", value, sizeof value);
        prop(x, "Text", value);
        fade_from = 0;
        alpha = 255;
    } else {
        if (!fade_from && alpha_shown > 0) fade_from = now;
        alpha = fade_from ? 255 - (int)((now - fade_from) * 255 * 2 / 1000) : 0;
        if (alpha <= 0) { alpha = 0; fade_from = 0; }
    }
    if (alpha != alpha_shown) {
        static const char hex[] = "0123456789ABCDEF";
        int j;
        colour[0] = hex[alpha >> 4]; colour[1] = hex[alpha & 15]; colour[8] = 0;
        for (j = 2; j < 8; j++) colour[j] = 'F';
        prop(x, "TextColour", colour);
        for (j = 2; j < 8; j++) colour[j] = '0';
        prop(x, "DropTextColour", colour);
        alpha_shown = alpha;
    }
    show(x, alpha > 0);
}

/* A text of the 360's HUD set to the text of PC's window `from` (another name), shown as it. */
static void copy_text(const char *to, const char *from) {
    void *p = pc_window(from), *x = win_of(HUD_PREFIX, to);
    char value[64];
    if (!x) return;
    value[0] = 0;
    if (p) read_prop(p, "Text", value, sizeof value);
    prop(x, "Text", value);
}

/* The icon PC shows in its hotbar slot `pc_slot` ("SkillSlot3"), shown in the 360 HUD's slot
 * `slot` (a DefaultWindow the 360's code fills): a StaticImage of ours made in it once
 * (WindowManager::createWindow), given the image of the first window under PC's slot that has
 * one; hidden if none. */
static void mirror_icon(const char *slot, const char *pc_slot) {
    typedef void *(__fastcall *create_fn)(void *wm, void *edx, const object *type, const object *name, const object *prefix);
    static create_fn create;
    void *x = win_of(HUD_PREFIX, slot), *p = pc_window(pc_slot), *icon, *stack[16];
    char name[64], image[128];
    int sp = 0;
    if (!x) return;
    copy(name, sizeof name, "icon_", slot);
    icon = win_of(HUD_PREFIX, name);
    if (!icon) {
        object t, n, pre;
        void *wm = window_manager();
        if (!create) create = (create_fn)need("?createWindow@WindowManager@CEGUI@@QAEPAVWindow@2@ABVString@2@00@Z");
        if (!create || !wm) return;
        string_ctor(&t, 0, "ceguiWidget/StaticImage");
        string_ctor(&n, 0, name);
        string_ctor(&pre, 0, HUD_PREFIX);
        __try { icon = create(wm, 0, &t, &n, &pre); } __except (1) { icon = 0; }
        string_dtor(&pre, 0); string_dtor(&n, 0); string_dtor(&t, 0);
        if (!icon) { x360_log("x360 hud: createWindow failed\r\n"); return; }
        prop(icon, "UnifiedAreaRect", "{{0,0},{0,0},{1,0},{1,0}}");
        prop(icon, "FrameEnabled", "False");
        prop(icon, "BackgroundEnabled", "False");
        add_child(x, 0, icon);
    }
    image[0] = 0;
    if (p) stack[sp++] = p;
    while (sp > 0 && !image[0]) {
        void *w = stack[--sp];
        unsigned c, nc = child_count(w, 0);
        if (w != p) read_prop(w, "Image", image, sizeof image);
        for (c = 0; c < nc && sp < 16; c++) stack[sp++] = child_at(w, 0, c);
    }
    if (image[0]) {
        float w, h, side = 36.0f * k;   /* the 360's icon side (its CGameUI +0x384): the slot's 36 */
        prop(icon, "Image", image);
        /* As the 360's hotbar (sub_821F42A8, 0x821F4A5C): the icon as tall as the slot, as wide
         * as its image's proportions give, centred across (a potion's 24 x 36, a skill's 36). */
        if (image_size(image, &w, &h)) set_size(icon, (side - side * w / h) * 0.5f, 0, side * w / h, side);
    }
    show(icon, image[0] != 0);
}

/* The target's health. The 360's HUD update (sub_821A83C8) shows the hero's target while an
 * attack button is held, else the unit under the cursor (its +0x48), else the unit the hero last
 * attacked: the player's virtual +0x1cc, which its attack (sub_822919A8) sets through +0x1c8 for
 * 3.0 seconds (sub_822D7C08, the timer at +0xa00). PC's update (GOG 0x558750) does the first two
 * with the mouse, and with the pad there is nothing under it; PC clears its pointer every frame
 * (CGameUI's +0x2c: a unit put there was gone by the next tick), so the 360's HUD is filled here as
 * PC fills its own: the name (the unit's std::wstring at +0x420), "Level" and the level (+0xf0),
 * the health (+0x394, a float) over the maximum (GOG 0x4901e0). The unit is kept in a safe pointer
 * (PC's assignment, GOG 0x4944e0) so that it reads 0 once the unit is gone. */
#define GOG_SAFE_ASSIGN 0x004944e0   /* TSafePointer assignment: thiscall (the pointer), the unit */
#define GOG_HERO_TARGET 0x290        /* CCharacter: its target */
#define GOG_UI_MOUSE_OVER 0x2c       /* CGameUI: the unit under the mouse (a safe pointer) */
#define GOG_PERFORMING_ATTACK_LOOSE 0x004863b0
#define GOG_UNIT_NAME 0x420          /* CCharacter: its shown name, a std::wstring (PC's 0x5589a5) */
#define GOG_UNIT_LEVEL 0xf0          /* CCharacter: its level (PC's 0x5588e8) */
#define GOG_UNIT_HEALTH 0x394        /* CCharacter: its health, a float (GOG 0x487e00) */
#define GOG_MAX_HEALTH 0x004901e0
#define GOG_UNIT_DESCRIPTION 0x004854a0   /* its DESCRIPTION (the unit's data, +0x178) into a wstring */
#define GOG_UNIT_CHAMPION 0x64e          /* CCharacter: a champion (PC's 0x5589ff; the 360's +0x63a) */
#define GOG_PLAYING_ANIMATION 0x00482740 /* thiscall (unit, const std::wstring &): that animation is on */
#define TYPE_MIMIC 0xa7
typedef void *(__fastcall *safe_assign_fn)(void *pointer, void *edx, void *unit);
typedef int (__fastcall *performing_fn)(void *unit, void *edx);
typedef void *(__fastcall *unit_wstring_fn)(void *unit, void *edx, void *out);
typedef int (__fastcall *playing_fn)(void *unit, void *edx, const void *name);

/* The 360 shows no target for a mimic still in its idle animation (sub_821A83C8: ISA 0xa7 and
 * "MIMICIDLE" playing, sub_822801d0); PC's update has the same test (GOG 0x558a8f, 0x482740). */
static int hidden_mimic(void *unit) {
    static const unsigned short idle[] = {'M', 'I', 'M', 'I', 'C', 'I', 'D', 'L', 'E', 0};
    wstr_ctor_fn ctor;
    wstr_dtor_fn dtor;
    unsigned char ws[32];
    int on = 0;
    if (!isa(unit, TYPE_MIMIC) || !wstring_functions(&ctor, &dtor)) return 0;
    ctor(ws, 0, idle);
    __try { on = ((playing_fn)GOG_PLAYING_ANIMATION)(unit, 0, ws) & 0xff; } __except (1) { on = 0; }
    dtor(ws, 0);
    return on;
}

/* The unit's description (the 360's TargetDescription: its data's DESCRIPTION, a monster's kind,
 * an NPC's role), as UTF-8; "" if it has none. */
static void unit_description(void *unit, char *out, unsigned size) {
    static const unsigned short empty[] = {0};
    wstr_ctor_fn ctor;
    wstr_dtor_fn dtor;
    unsigned char ws[32];
    out[0] = 0;
    if (!wstring_functions(&ctor, &dtor)) return;
    ctor(ws, 0, empty);
    __try {
        ((unit_wstring_fn)GOG_UNIT_DESCRIPTION)(unit, 0, ws);
        if (!wstring_utf8(ws, out, size)) out[0] = 0;
    } __except (1) { out[0] = 0; }
    dtor(ws, 0);
}

static struct { void *unit; int handle; } attacked = {0, -1};
static unsigned attacked_until;

/* The 360's player virtual +0x1c8 (sub_822D7C08): the unit attacked, kept 3.0 seconds. */
void x360_hud_attacked(void *unit) {
    if (!unit) return;
    if (attacked.unit != unit) {
        ((safe_assign_fn)GOG_SAFE_ASSIGN)(&attacked, 0, unit);
        x360_log("x360 hud: target shown\r\n");
    }
    attacked_until = GetTickCount() + 3000;
}

static void target_tick(void) {
    static float bar[8];
    static int champion_shown = -1;
    unsigned now = GetTickCount();
    unsigned char *unit;
    void *x = win_of(HUD_PREFIX, "TargetHealth"), *pc = pc_window("TargetHealth");
    static void *pc_faded;
    char buf[128], *p;
    int max;
    if (pc != pc_faded) { pc_faded = pc; if (pc) prop(pc, "Alpha", "0"); }   /* PC's own, for the mouse */
    if (!x || !hero) return;
    unit = *(unsigned char **)(hero + GOG_HERO_TARGET);
    if (unit && (((performing_fn)GOG_PERFORMING_ATTACK_LOOSE)(hero, 0) & 0xff)) x360_hud_attacked(unit);
    if (attacked.unit && (int)(now - attacked_until) >= 0) ((safe_assign_fn)GOG_SAFE_ASSIGN)(&attacked, 0, 0);
    /* The 360's choice (sub_821A83C8, 0x821A9124-0x821A9678, the recomp agent's reading): while
     * an attack or skill button is held, the unit the player acts on (its +644, GOG's hero
     * target); else the pad's target (its HUD's +72, an NPC near too); none under a menu or a
     * modal; then, unless that is an NPC (type 104), the player's combat target when it has one
     * (its virtual +460; here the unit attacked, kept 3 s as before). PC's unit under the mouse
     * first, for the mouse. */
    unit = ui ? *(unsigned char **)(ui + GOG_UI_MOUSE_OVER) : 0;
    if (!unit) {
        void *x360_pad_unit_target(void);
        unsigned x360_pad_buttons(void);
        int x360_overlay_open(void);
        if (x360_pad_buttons() & (0x2000 | 0x4000 | 0x8000 | 0x10000 | 0x20000)) unit = *(unsigned char **)(hero + GOG_HERO_TARGET);
        if (!unit) unit = (unsigned char *)x360_pad_unit_target();
        if (!(inventory_open || shop || qd_shown || modal_shown || x360_overlay_open() || x360_pause_open() || x360_pc_die_up || x360_pc_settings_up) &&
            (!unit || !isa(unit, 104)) && attacked.unit) unit = (unsigned char *)attacked.unit;   /* no call on the unit under a screen: the check below clears it */
        /* Under a screen nothing (the owner: the bar over PC's pause menu); the 360's pause
         * covers its HUD. */
    }
    {
        int x360_overlay_open(void);
        if (inventory_open || shop || qd_shown || modal_shown || x360_overlay_open() || x360_pause_open() || x360_pc_die_up || x360_pc_settings_up) unit = 0;
    }
    if (unit && hidden_mimic(unit)) unit = 0;
    show(x, unit != 0);
    if (!unit) return;
    if (!wstring_utf8(unit + GOG_UNIT_NAME, buf, sizeof buf)) buf[0] = 0;
    text(win_of(HUD_PREFIX, "TargetName"), buf); show(win_of(HUD_PREFIX, "TargetName"), 1);
    /* A champion's name in gold: the 360's colour (1, 0.88, 0.24, its catalog's FFFFE03D), as PC's
     * own update (the constants at GOG 0xab2150, 0xab2154). */
    if (unit[GOG_UNIT_CHAMPION] != champion_shown) {
        champion_shown = unit[GOG_UNIT_CHAMPION];
        prop(win_of(HUD_PREFIX, "TargetName"), "TextColour", champion_shown ? "FFFFE03D" : "FFFFFFFF");
    }
    copy(buf, sizeof buf, x360_text("level"), " ");
    p = put_int(buf + length(buf), *(int *)(unit + GOG_UNIT_LEVEL));
    *p = 0;
    text(win_of(HUD_PREFIX, "TargetLevel"), buf); show(win_of(HUD_PREFIX, "TargetLevel"), 1);
    unit_description(unit, buf, sizeof buf);
    text(win_of(HUD_PREFIX, "TargetDescription"), buf); show(win_of(HUD_PREFIX, "TargetDescription"), 1);
    max = ((int_get_fn)GOG_MAX_HEALTH)(unit, 0);
    /* The bar's size as the 360's update sets it (sub_821A83C8, 0x821A95E4): the health over the
     * maximum, at most 1, times the full width (220 at 768 rows, rounded in the 360's 720-row
     * pixels), 84 high; at (1, 1) in its parent (i_780), which clips it (the catalog: in-game/01
     * to 03, {{0,0.9375},{0,0.9375},{0,1+w},{0,79.6875}} at 720). */
    {
        float f = max > 0 ? *(float *)(unit + GOG_UNIT_HEALTH) / (float)max : 0, w720;
        if (f > 1) f = 1;
        if (f < 0) f = 0;
        w720 = (float)(long)(220.0f * 0.9375f * f + 0.5f);
        set_size(win_of(HUD_PREFIX, "TargetHealthBar"), k, k, w720 * k / 0.9375f, 84.0f * k);
    }
    {
        /* A test aid: the first frame a unit of each kind is shown, a dump (target-enemy,
         * target-champion, target-npc), as the catalog's in-game/01-03; x360_ui.ini dump_targets=1. */
        extern int x360_dump_targets;
        void x360_ui_dump_now(const char *name);
        static void *dumped_unit;
        static int done[3];
        if (x360_dump_targets && unit != dumped_unit && *(float *)(unit + GOG_UNIT_HEALTH) > 0) {
            int kind = unit == the_pet() ? 2 : unit[GOG_UNIT_CHAMPION] ? 1 : 0;   /* in-game/03 is the pet */
            dumped_unit = unit;
            if (!done[kind]) {
                done[kind] = 1;
                x360_ui_dump_now(kind == 1 ? "target-champion" : kind == 2 ? "target-npc" : "target-enemy");
                x360_log("x360 hud: dumped the target bar\r\n");
            }
        }
    }
    (void)bar;
}

/* The minimap where the 360 draws it. The 360's CAutomap::setVisible (sub_822E6090) places its
 * viewport, out of its fullscreen mode, in the bottom right corner: a square of 200 (its +0x50,
 * from the constructor sub_822E3A30) times the screen's scale (height / 768, the scale the HUD's
 * layouts get here; its captures: 187 x 187 at 1045, 466 in 1280 x 720), 50 from the right edge
 * and 70 from the bottom (the constants at 0x820aa388 and 0x820aa570). PC's (CAutomap::setVisible
 * *(OT)*, GOG 0x4ea6b0, called on toggling it) puts it top right; the level keeps it at +0x160
 * (GOG 0x4fc062), its viewport at +0x28, shown +0x3c, fullscreen +0x65. Here the corner's viewport
 * is placed again as the 360's at once after the pad toggles it (R3) and every frame from the frame
 * listener, before the frame is drawn (PC sizes it only when it is toggled; placed from the HUD's
 * tick alone, every 50 ms, it showed top right for an instant after R3: the owner, 2026-10-25). */
#define GOG_LEVEL_AUTOMAP 0x160
#define AUTOMAP_VIEWPORT 0x28
#define AUTOMAP_SHOWN 0x3c
#define AUTOMAP_FULLSCREEN 0x65

static int class_is(void *object, const char *name) {
    unsigned char *col = object ? *(unsigned char **)(*(unsigned char **)object - 4) : 0;
    unsigned char *td = col ? *(unsigned char **)(col + 12) : 0;
    const char *b = td ? (const char *)(td + 8) : "";
    while (*b && *b == *name) b++, name++;
    return !*b && !*name;
}

static void minimap_tick_body(void);

/* While a level is left or loaded, its pointer (and the automap's) may not be valid yet: skipped. */
static void minimap_tick(void) {
    __try { minimap_tick_body(); } __except (1) {}
}

static void minimap_tick_body(void) {
    unsigned char *map = level ? *(unsigned char **)(level + GOG_LEVEL_AUTOMAP) : 0;
    void *vp;
    HWND w = active_window();
    RECT r;
    float W, H, s;
    if (!map || !class_is(map, ".?AVCAutomap@@")) return;
    vp = *(void **)(map + AUTOMAP_VIEWPORT);
    if (!vp || !map[AUTOMAP_SHOWN] || map[AUTOMAP_FULLSCREEN]) return;
    if (!w || !GetClientRect(w, &r) || r.right <= r.left || r.bottom <= r.top) return;
    W = (float)(r.right - r.left);
    H = (float)(r.bottom - r.top);
    s = H / 768.0f;
    (*(set_dims_fn *)IAT_VP_SET_DIMENSIONS)(vp, 0, 1.0f - (200.0f + 50.0f) * s / W, 1.0f - (200.0f + 70.0f) * s / H,
                                            200.0f * s / W, 200.0f * s / H);
}

/* The 360's HUD in play: hud.layout on the GUI sheet, PC's bottom HUD hidden (still updated by
 * PC), the orbs and the experience bar filled as PC's, the level's name, the messages and the
 * target's health as PC shows them. Called with the pad's poll (the game's thread). */
static void hud_tick(void);
static const char *hud_stage = "";

/* The HUD's tick, guarded: while a level loads, the pointers it reads may not be set yet. */
void x360_ui_dump_poll(void);

static void add_frame_listener(void);
/* PC's bottom HUD and pet HUD made transparent as PC loads their layouts, at the game's start: the
 * HUD tick did it only once play was entered, so they showed for its first frames (the owner saw
 * them flash at each load). PC changes their visibility, never their alpha. */
/* PC's tip window's root (tipmenu.layout, loaded once at the game's start): made transparent as it
 * loads, so PC's tip is never drawn before the 360's takes its place (the owner saw it for a
 * moment); tip_tick shows it again only when it is not a tip (the settings' confirmations). */
/* PC's main menu leaves its 3D view's first column and row uncovered at 16:10 (the Steam Deck: a
 * blue line, PC's own, without the mod too; the owner asked for it gone): two 1-pixel black lines
 * of PC's own image (GuiLook/BlackLine, its HUD's borders') at its left and top, under its root. */
/* The two lines, made under root with the windows' prefix; 0 if not made. */
static int one_pixel_borders(void *root, const char *prefix, void **made);
static void main_menu_borders(void *root) {
    if (one_pixel_borders(root, "x360mmb_", 0)) x360_log("x360 menu: the main menu's 1-pixel borders added\r\n");
}
static int one_pixel_borders(void *root, const char *prefix, void **made) {
    typedef void *(__fastcall *create_fn)(void *wm, void *edx, const void *type, const void *name, const void *prefix);
    static create_fn create;
    static const char *const names[2] = {"LeftLine", "TopLine"};
    static const char *const rects[2] = {"{{0,0},{0,0},{0,1},{1,0}}", "{{0,0},{0,0},{1,0},{0,1}}"};
    void *wm = window_manager();
    int b;
    if (!create) create = (create_fn)need("?createWindow@WindowManager@CEGUI@@QAEPAVWindow@2@ABVString@2@00@Z");
    if (!create || !wm || !root) return 0;
    for (b = 0; b < 2; b++) {
        object t, nm, pre;
        void *w;
        string_ctor(&t, 0, "GuiLook/StaticImage");
        string_ctor(&nm, 0, names[b]);
        string_ctor(&pre, 0, prefix);
        __try { w = create(wm, 0, &t, &nm, &pre); } __except (1) { w = 0; }
        string_dtor(&pre, 0); string_dtor(&nm, 0); string_dtor(&t, 0);
        if (!w) { x360_log("x360 menu: a 1-pixel border not made\r\n"); return 0; }
        prop(w, "UnifiedAreaRect", rects[b]);
        prop(w, "Image", "set:GuiLook image:BlackLine");
        prop(w, "FrameEnabled", "False");
        prop(w, "BackgroundEnabled", "False");
        prop(w, "MousePassThroughEnabled", "True");
        prop(w, "AlwaysOnTop", "True");
        add_child(root, 0, w);
        if (made) made[b] = w;
    }
    return 1;
}

void x360_pc_layout_loaded(const char *file, void *root) {
    static const char *const hidden[] = {"BOTTOMHUD.LAYOUT", "PETHUD.LAYOUT", "TIPMENU.LAYOUT", 0};
    unsigned n = length(file), i;
    if (!resolve()) return;
    {
        static const char mm[] = "MAINMENUFRAME.LAYOUT";
        unsigned m = sizeof mm - 1, j;
        int same = n >= m;
        for (j = 0; same && j < m; j++) {
            char c = file[n - m + j];
            if (c >= 'a' && c <= 'z') c = (char)(c - 32);
            same = c == mm[j];
        }
        if (same) __try { main_menu_borders(root); } __except (1) { x360_log("x360 menu: exception adding the main menu's borders\r\n"); }
    }
    for (i = 0; hidden[i]; i++) {
        unsigned m = length(hidden[i]), j;
        int same = n >= m;
        for (j = 0; same && j < m; j++) {
            char c = file[n - m + j];
            if (c >= 'a' && c <= 'z') c = (char)(c - 32);
            same = c == hidden[i][j];
        }
        if (!same) continue;
        if (i == 2) {   /* the tip: not let through to the mouse (the settings' confirmations use it) */
            /* Only when tip_tick will run (the pad's frame), which shows it again where needed. */
            extern int x360_pad_running;
            if (!x360_pad_running) continue;
            __try { prop(root, "Alpha", "0"); } __except (1) { return; }
            pc_tip_layout_root = root;
        } else {
            __try { prop(root, "Alpha", "0"); pass_mouse(root); } __except (1) { return; }
            if (i == 0) {
                /* Its four 1-pixel black borders kept: PC's 3D view leaves its first row and column
                 * uncovered at 16:10 (the Steam Deck's 1280x800: a blue line at the top and the left,
                 * seen in PC's own main menu too), and these cover it in play; made transparent
                 * with the rest, the line showed (the owner's Deck). */
                static const char *const borders[] = {"LeftBorder", "RightBorder", "TopBorder", "BottomBorder"};
                unsigned b;
                for (b = 0; b < 4; b++) {
                    void *w = descendant_ending(root, borders[b]);
                    __try { prop(w, "InheritsAlpha", "False"); prop(w, "Alpha", "1"); } __except (1) { }
                }
            }
        }
        x360_log("x360 hud: PC's "); x360_log(hidden[i]); x360_log(" transparent as it loads\r\n");
    }
}

void x360_hud_tick(void) {
    static int logged;
    add_frame_listener();
    x360_ui_dump_poll();
    __try { hud_tick(); }
    __except (1) {
        if (logged < 5) { x360_log("x360 hud: exception in the tick at "); x360_log(hud_stage); x360_log("\r\n"); logged++; }
    }
}

/* The "A Interact" prompt (the 360's HUD update sub_821A83C8, 0x821AB364-0x821AB518, the recomp
 * agent's reading): hidden each frame, then shown over the unit that is the pad's current target
 * (the HUD's +72) when the player can interact with it (sub_821B8D70), no menu or modal is up, the
 * unit is not dead and not the player's own (its pet); its text is the layout's own. Here: the
 * pad's candidate from its second pass (any alignment: an NPC, a stash, the retirement NPC; not an enemy),
 * not the pet, with no 360 screen, dialog or modal open. The 360 places it at the unit's position
 * 1.0 up and 0.4 aside, projected and kept 26 pixels inside the screen (pool 0x820AA2C0 +64, +140,
 * +1608); the anchor below (interact_prompt). */
/* A world point on the screen, in the window's pixels, by Ogre's camera (the client's +0x10, its
 * +8, as proxy/pad.c reads it): its projection and view matrices; 0 behind the camera. */
static int world_to_screen(const float *p, float sw, float sh, float *x, float *y) {
    typedef const float *(__fastcall *mat_fn)(const void *self, void *edx);
    static mat_fn view, proj;
    static int looked;
    unsigned char *ctl = the_client ? *(unsigned char **)(the_client + 0x10) : 0;
    void *cam = ctl ? *(void **)(ctl + 8) : 0;
    const float *v, *q;
    float e[4], c[4];
    int i;
    if (!looked) {
        void *ogre = GetModuleHandleA("OgreMain.dll");
        looked = 1;
        view = ogre ? (mat_fn)GetProcAddress(ogre, "?getViewMatrix@Camera@Ogre@@UBEABVMatrix4@2@XZ") : 0;
        proj = ogre ? (mat_fn)GetProcAddress(ogre, "?getProjectionMatrix@Frustum@Ogre@@UBEABVMatrix4@2@XZ") : 0;
    }
    if (!cam || !view || !proj) return 0;
    v = view(cam, 0);
    q = proj(cam, 0);
    for (i = 0; i < 4; i++) e[i] = v[i * 4] * p[0] + v[i * 4 + 1] * p[1] + v[i * 4 + 2] * p[2] + v[i * 4 + 3];   /* row-major, as Ogre's */
    for (i = 0; i < 4; i++) c[i] = q[i * 4] * e[0] + q[i * 4 + 1] * e[1] + q[i * 4 + 2] * e[2] + q[i * 4 + 3] * e[3];
    if (c[3] <= 1e-4f) return 0;
    *x = (c[0] / c[3] + 1.0f) * 0.5f * sw;
    *y = (1.0f - c[1] / c[3]) * 0.5f * sh;
    return 1;
}
/* The "A Interact" prompt (the 360's HUD update sub_821A83C8, 0x821AB364-0x821AB518, the recomp
 * agent's reading): hidden each frame, then shown over the unit that is the pad's current target
 * (the HUD's +72) when the player can interact with it (sub_821B8D70), no menu or modal is up, the
 * unit is not dead and not the player's own (its pet); its text is the layout's own. Here: the
 * pad's candidate from its second pass (any alignment: an NPC, a stash, the retirement NPC; not an enemy),
 * not the pet, with no 360 screen, dialog or modal open. Placed as the 360's: the unit's position
 * 1.0 up and 0.4 aside on x and z (pool 0x820AA2C0 +64, +140), projected (sub_8233E1C0), its
 * top left there, kept between 0 and the screen less 26 pixels (of 720; +68, +1608). */
static int pc_pause_open(void);
int x360_pause_open(void);
static void interact_prompt(void) {
    typedef struct { float top, bottom, left, right; } rect6;
    typedef rect6 *(__fastcall *pixel_rect_fn6)(const void *self, void *edx, rect6 *out);
    typedef void *(__fastcall *parent_fn)(const void *self, void *edx);
    void *x360_pad_interact_unit(void);
    int x360_overlay_open(void);
    static pixel_rect_fn6 rect_of;
    static parent_fn gp;
    void *w = win_of(HUD_PREFIX, "TargetInteractHover"), *par;
    unsigned char *u = (unsigned char *)x360_pad_interact_unit();
    rect6 pr;
    float pos[3], x, y, sw, sh, m;
    HWND hw = active_window();
    RECT r;
    if (!w) return;
    if (!rect_of) rect_of = (pixel_rect_fn6)need("?getUnclippedPixelRect@Window@CEGUI@@QBE?AVRect@2@XZ");
    if (!gp) gp = (parent_fn)need("?getParent@Window@CEGUI@@QBEPAV12@XZ");
    if (!u || !hero || u == hero || u == the_pet() || inventory_open || shop || qd_shown || modal_shown || x360_overlay_open() ||
        pc_pause_open() || x360_pause_open() || x360_pc_die_up || x360_pc_settings_up || !rect_of || !gp ||
        !(par = gp(w, 0)) || !hw || !GetClientRect(hw, &r) || r.right <= r.left) { show(w, 0); return; }
    sw = (float)(r.right - r.left);
    sh = (float)(r.bottom - r.top);
    ((get_position_fn)GOG_GET_POSITION)(u, 0, pos, 1);
    {
        /* sub_821B8D70(player, unit): within 2.0 on the ground (2.75 while the player stands:
         * its +508 0; here the stick at rest), and an interactive unit: the 360's registry
         * (sub_821E3C48, not ported) or one of the types 130 ENCHANTER, 175 RETIREMENTCHARACTER,
         * 132 and 133 the gem goblins, 134 COMBINER, 41 MERCHANT, 128 STASH, 170 SHAREDSTASH, 104 NPC. */
        static const int types[] = {130, 175, 132, 133, 134, 41, 128, 170, 104};
        int x360_pad_lx(void), x360_pad_ly(void);
        float hp[3], dx, dz, reach;
        int t, ok = 0;
        for (t = 0; t < 9 && !ok; t++) ok = isa(u, types[t]);
        ((get_position_fn)GOG_GET_POSITION)(hero, 0, hp, 1);
        dx = pos[0] - hp[0];
        dz = pos[2] - hp[2];
        {
            int sx = x360_pad_lx(), sy = x360_pad_ly();   /* raw (of 1000): at rest under the 360's dead zone, about 240 */
            reach = sx * sx + sy * sy < 240 * 240 ? 2.75f : 2.0f;
        }
        if (!ok || dx * dx + dz * dz > reach * reach) { show(w, 0); return; }
    }
    {
        /* The 360's anchor (sub_821AB3F0.., the projection sub_8233E1C0 with its offset, the recomp
         * agent's reading): pos + (0.4, 1.0 - unit+348, 0.4) + the camera's up axis (its orientation's
         * Y, unit length). The 360's +348 measured on the recomp (2026-10-25): 0.8 for the units that
         * stand (merchants, stashes, the retirement NPC), 0.3 for the ones that walk;
         * PC's unit has it at the same +348 (logged on GOG: the same 0.8 and 0.3). The owner's capture
         * at the stash: the prompt within some 30 pixels of the 360's at 1080 (its camera not quite
         * the same). */
        typedef float *(__fastcall *vec_fn)(const void *self, void *edx, float *out);
        static vec_fn derived_up;
        static int looked;
        unsigned char *ctl = the_client ? *(unsigned char **)(the_client + 0x10) : 0;
        void *cam = ctl ? *(void **)(ctl + 8) : 0;
        float up[3] = {0, 1, 0};
        if (!looked) {
            void *ogre = GetModuleHandleA("OgreMain.dll");
            looked = 1;
            derived_up = ogre ? (vec_fn)GetProcAddress(ogre, "?getDerivedUp@Camera@Ogre@@QBE?AVVector3@2@XZ") : 0;
        }
        if (derived_up && cam) derived_up(cam, 0, up);
        pos[0] += 0.4f + up[0];
        pos[1] += 1.0f - *(float *)(u + 348) + up[1];
        pos[2] += 0.4f + up[2];
    }
    if (!world_to_screen(pos, sw, sh, &x, &y)) { show(w, 0); return; }
    m = 26.0f * k;
    if (x < 0) x = 0;
    if (x > sw - m) x = sw - m;
    if (y < 0) y = 0;
    if (y > sh - m) y = sh - m;
    rect_of(par, 0, &pr);
    {   /* the window's own size, its layout's: kept, only moved */
        rect6 wr;
        rect_of(w, 0, &wr);
        set_size(w, x - pr.left, y - pr.top, wr.right - wr.left, wr.bottom - wr.top);
    }
    show(w, 1);
}

static void hud_tick(void) {
    static unsigned last;
    static int failed;
    static float health[8], mana[8], xp[8];
    static int faded;
    unsigned now = GetTickCount();
    if (failed || now - last < 50) return;
    last = now;
    /* Out of play (the main menu after "Exit to menu", a load) the 360's HUD is hidden; PC's
     * windows are searched again on the way back, as PC may have made new ones. */
    hud_stage = "resolve";
    if (!resolve() || !pc_window("PlayerHealthRoot")) {
        { void *l = win_of("x360pb_", "LeftLine"), *t = win_of("x360pb_", "TopLine"); if (l) show(l, 0); if (t) show(t, 0); }
        if (hud_root) show(hud_root, 0);
        if (pet_hud_root) show(pet_hud_root, 0);
        hud_in_play = 0;
        return;
    }
    hud_in_play = 1;
    {
        /* PC's 3D view leaves its first row and column uncovered at 16:10 (the Deck); PC's bottom
         * HUD's borders cover them in play (section 42), but PC hides that HUD in a cinematic and
         * the blue line showed there (the owner's Deck): the same two 1-pixel lines on the GUI
         * sheet, shown while in play. */
        static void *lines[2];
        static int tried;
        void *sys = system_ptr(), *sheet = sys ? get_gui_sheet(sys, 0) : 0;
        if (!tried && sheet) { tried = 1; if (one_pixel_borders(sheet, "x360pb_", lines)) x360_log("x360 hud: the 1-pixel borders over the 3D view added\r\n"); }
        (void)lines;
        { void *l = win_of("x360pb_", "LeftLine"), *t = win_of("x360pb_", "TopLine"); if (l) show(l, 1); if (t) show(t, 1); }   /* by name each time (the review) */
    }
    if (!hud_root) {
        HWND w = active_window();
        RECT r;
        pc_hud_root = pc_window("PlayerHealthRoot");
        if (!pc_hud_root) return;                       /* PC's HUD not loaded yet */
        hud_root = load("media/ui/x360/hud.layout", HUD_PREFIX);
        if (!hud_root) { failed = 1; return; }
        if (!k) k = (w && GetClientRect(w, &r) && r.bottom > r.top) ? (float)(r.bottom - r.top) / 768.0f : 1.0f;
        scale_tree(hud_root, k, 0);
        pass_mouse(hud_root);
        if (w && GetClientRect(w, &r) && r.bottom > r.top)   /* the 360's fonts, before any menu opens */
            match_spacing((float)(r.right - r.left), (float)(r.bottom - r.top));
        x360_log("x360 hud: shown, PC's bottom HUD transparent\r\n");
    }
    /* PC shows its bottom HUD again each frame, so it is made transparent instead (its children
     * inherit the alpha); PC still updates it, and its bars are read from it. */
    {
        void *p = pc_window("PlayerHealthRoot");
        /* Unseen, it takes no click either: with the 360's HUD letting the mouse through, a click
         * on the bottom bar opened PC's skills (its SkillsButton under the mouse). */
        if (p != pc_hud_root) { pc_hud_root = p; prop(pc_hud_root, "Alpha", "0"); pass_mouse(pc_hud_root); }
        if (!faded) { prop(pc_hud_root, "Alpha", "0"); pass_mouse(pc_hud_root); faded = 1; }
    }
    hud_stage = "bars";
    show(hud_root, !qd_shown && !modal_shown);     /* no HUD under a dialog or a modal (town/13, in-game/16) */
    mirror_bar("PlayerHealthBar", "PlayerHealthBarSub", 0, health);
    mirror_bar("PlayerManaBar", "PlayerManaBarSub", 0, mana);
    mirror_bar("ExperienceBar", "ExperienceBarSub", 1, xp);
    /* The potions' counts: LB and RB press PC's hotbar keys 1 and 2 (proxy/pad.c), so the counts
     * are those PC writes on those slots (SkillText1, SkillText2). */
    copy_text("POTIONCOUNT", "SkillText1");
    copy_text("MANACOUNT", "SkillText2");
    /* Not done yet: the skills on the pad's buttons (the layout's
     * samples hidden). */
    hud_stage = "interact";
    interact_prompt();
    /* The icons on the pad's buttons, from the hotbar keys proxy/pad.c presses for them: LB 1,
     * RB 2, B 3, Y 4, LT 5, RT 6. The 360's first set (SkillSet1: Y SkillSlot4, B 5, LT 6, RT 7);
     * its second set (SkillSet2, the same places) hidden. */
    mirror_icon("SkillSlot1", "SkillSlot1");
    mirror_icon("SkillSlot2", "SkillSlot2");
    mirror_icon("SkillSlot5", "SkillSlot3");
    mirror_icon("SkillSlot4", "SkillSlot4");
    mirror_icon("SkillSlot6", "SkillSlot5");
    mirror_icon("SkillSlot7", "SkillSlot6");
    show(win_of(HUD_PREFIX, "SkillSet2"), 0);
    {
        static const char *const texts[] = {"SkillText3", "SkillText4", "SkillText5", "SkillText6", "SkillText7",
                                            "SkillText8", "SkillText9", "SkillText10", 0};
        int j;
        for (j = 0; texts[j]; j++) prop(win_of(HUD_PREFIX, texts[j]), "Text", "");
    }
    hud_stage = "plus";
    /* The bag's "+" (AvailableSkillsIcon, its glow AvailableSkillsGlowIcon over it): the 360's HUD
     * update (sub_821A83C8, 0x821A9C90-0x821A9D50) shows the plus while the player has skill points
     * (+968, GOG's +0x3e0) or stat points (+964, GOG's +0x3dc) unspent, and hides both otherwise;
     * meanwhile a timer (HUD +6260) counts the frame's time down from 1.0 s (0x820AA300) and at 0
     * flips a flag (+6264) that hides or shows the glow, so it blinks once a second. */
    {
        static unsigned blink_at;
        static int flag;
        int points = find_world() && hero && (*(int *)(hero + GOG_SKILL_POINTS) > 0 || *(int *)(hero + GOG_STAT_POINTS) > 0);
        show(win_of(HUD_PREFIX, "AvailableSkillsIcon"), points);
        if (!points) blink_at = 0;
        else if (!blink_at || now - blink_at >= 1000) { blink_at = now; flag = !flag; }
        show(win_of(HUD_PREFIX, "AvailableSkillsGlowIcon"), points && !flag);
    }
    hud_stage = "pet";
    /* The pet's: the 360's pethud.layout, PC's made transparent, its bars and text as PC's. */
    {
        static void *pet_root, *pc_pet;
        static float pet_health[8], pet_mana[8];
        static is_visible_fn vis;
        if (!vis) vis = (is_visible_fn)need("?isVisible@Window@CEGUI@@QBE_N_N@Z");
        void *p = pc_window("PetHud");
        if (p && !pet_root) {
            pet_root = load("media/ui/x360/pethud.layout", PETHUD_PREFIX);
            if (pet_root) { scale_tree(pet_root, k, 0); pass_mouse(pet_root); pet_hud_root = pet_root; }
        }
        if (p != pc_pet) { pc_pet = p; if (pc_pet) { prop(pc_pet, "Alpha", "0"); pass_mouse(pc_pet); } }
        if (!pc_pet && pet_root) show(pet_root, 0);
        if (pet_root && vis && pc_pet) {
            show(pet_root, vis(pc_pet, 0, 0) && !qd_shown && !modal_shown);
            mirror_bar_of(PETHUD_PREFIX, "PetHealthBar", "PetHealth", 1, pet_health);
            mirror_bar_of(PETHUD_PREFIX, "PetManaBar", "PetMana", 1, pet_mana);
            mirror_text_of(PETHUD_PREFIX, "FleeingText");
            /* The stance: only the current one's button shown, the others left to pethud_base's
             * grey ones (the catalog's menus/07: PetDefensive shown, PetAggressive and PetPassive
             * not; PC's update does the same with its own, GOG 0x5598c5: the pet's +0x65c,
             * 0 passive, 1 aggressive, 2 defensive). */
            {
                unsigned char *pet = the_pet();
                int stance = pet ? *(int *)(pet + GOG_PET_STANCE) : -1;
                show(win_of(PETHUD_PREFIX, "PetPassive"), stance == 0);
                show(win_of(PETHUD_PREFIX, "PetAggressive"), stance == 1);
                show(win_of(PETHUD_PREFIX, "PetDefensive"), stance == 2);
            }
        }
    }
    /* The place's name stays with the 360's inventory open, as on the 360 (the owner, 2026-10-25). */
    /* PC's own hiding counted otherwise; while the port hides PC's sheets (a 360 screen, and the
     * 600 ms after a shop until they come back) only the window's own (the owner: the name went
     * off and on again as an NPC's menu closed). */
    mirror_text_shown(HUD_PREFIX, "LevelName", inventory_open || pc_ui_back_at || hidden_count);
    large_message_tick(now);
    mirror_text("SmallMessageText");
    hud_stage = "target";
    target_tick();
    hud_stage = "minimap";
    minimap_tick();
    hud_stage = "done";
}

/* ---- NPC dialogs (the 360's CQuestDialogMenu: town/04, 13, 14, 27-29, 42) ----
 * PC's quest dialog (CQuestDialogMenu, vtable 0xab7ff0) and the 360's questdialogmenu.layout have
 * the same windows (Portrait, DialogTextFrame/DialogText, Ok, Accept, Decline, RewardGold/XP/Fame
 * and their Box, ItemBkg1-3/ItemSlot1-3). While PC's is shown the 360's is shown over it, its
 * windows mirrored from PC's each frame, PC's made transparent; the HUD is hidden (town/13).
 * PC's modal dialog stops the GetCursorPos polls that drive the pad's ticks, and CEGUI is drawn
 * only when something changes, so this runs from an Ogre FrameListener of the proxy's, added to
 * PC's Root (OgreMain's Root::addFrameListener), once a frame.
 * The text rises (the 360's sub_82357C40, each frame with the frame's time): t grows to
 * T = max(the text's characters * 0.015, 5.0) seconds (sub_823568D0: its +344 times 0x820aa7d8,
 * at least 0x820aa464), the text's top at (1 - t/T) * (its height + 50) pixels (0x820aa388) below
 * its frame's top, at the 360's 720 rows.
 * The pad: A presses PC's Accept when shown, else its Ok; B its Decline when shown. X held makes
 * the text rise faster (sub_82192640, 0x821929c4: the dialog's frame time times [r29+268], r29 the
 * constant pool 0x820AA2C0, its +0x10C 10.0; the mods agent's static reading). x360_ui.ini's
 * dialog_fast_scroll overrides it. */
int x360_dialog_fast_scroll = 10;
int x360_dump_targets;                       /* x360_ui.ini dump_targets=1: a dump at the first target of each kind */
int x360_dialog_pc;                          /* x360_ui.ini dialog_pc=1: PC's dialog left visible (a diagnosis) */
/* The 360's colour markup kept (the names in gold, QA run B with the font's +0x424 set); x360_ui.ini
 * dialog_markup=0 takes it out (the words kept). It was off by default until 2026-10-23, so an
 * installer's ini without the key lost the gold. */
int x360_dialog_markup = 1;
char x360_dialog_font[32];                   /* x360_ui.ini dialog_font=NAME: the text's font */
#define QD_PREFIX "x360qd_"
static void *qd_root, *pc_qd_text_frame;
static char pc_qd_prefix[48];
static int qd_failed;
static unsigned qd_last, qd_prev_buttons;
static float qd_T, qd_t;
unsigned x360_pad_buttons(void);

/* The first window under the GUI sheet whose name ends in `suffix`, not of the proxy's layouts,
 * whose layout also has the windows `needs` (a list ended by 0, or none); its name's prefix (PC's
 * layouts' counter, "20_") into prefix_out. */
static void *find_pc_by_suffix(const char *suffix, const char *const *needs, char *prefix_out, unsigned size) {
    typedef const void *(__fastcall *name_fn)(const void *self, void *edx);
    static name_fn gn;
    void *sys = system_ptr(), *sheet = sys ? get_gui_sheet(sys, 0) : 0, *stack[256];
    int sp = 0;
    unsigned n = length(suffix);
    if (!gn) gn = (name_fn)need("?getName@Window@CEGUI@@QBEABVString@2@XZ");
    if (!sheet || !gn) return 0;
    stack[sp++] = sheet;
    while (sp > 0) {
        void *v = stack[--sp];
        const char *name = c_str(gn(v, 0), 0);
        unsigned len = length(name), c, nc;
        if (name[0] == 'x' && name[1] == '3' && name[2] == '6' && name[3] == '0') continue;
        if (len > n && len - n < size) {
            unsigned i = 0;
            while (i < n && name[len - n + i] == suffix[i]) i++;
            if (i == n) {
                for (i = 0; i < len - n; i++) prefix_out[i] = name[i];
                prefix_out[len - n] = 0;
                unsigned q = 0;
                while (needs && needs[q] && win_of(prefix_out, needs[q])) q++;
                if (!needs || !needs[q]) return v;
            }
        }
        nc = child_count(v, 0);
        for (c = 0; c < nc && sp < 255; c++) stack[sp++] = child_at(v, 0, c);
    }
    prefix_out[0] = 0;   /* no candidate had its needs: none (the review: a failed one's was kept) */
    return 0;
}

/* PC's quest dialog's DialogTextFrame. */
/* With its Portrait: PC's cinematic menu has a DialogTextFrame too (QA run 18: a new character's
 * intro took it, the quest dialog then never found). */
static void *find_pc_dialog(void) {
    static const char *const needs[] = {"Portrait", 0};
    return find_pc_by_suffix("DialogTextFrame", needs, pc_qd_prefix, sizeof pc_qd_prefix);
}

/* Whether PC's window prefix+name is visible, and its Frame shown and still under the GUI sheet
 * (PC closes its menus by taking them off the sheet: they stay "visible"). */
static int pc_shown_in(const char *prefix, const char *name) {
    typedef unsigned char (__fastcall *vis_fn)(const void *self, void *edx, unsigned char local);
    static vis_fn vis;
    void *w = win_of(prefix, name);
    if (!vis) vis = (vis_fn)need("?isVisible@Window@CEGUI@@QBE_N_N@Z");
    return w && vis && vis(w, 0, 0);
}

static int pc_attached(const char *prefix, const char *name) {
    typedef void *(__fastcall *parent_fn)(const void *self, void *edx);
    static parent_fn gp;
    void *sys = system_ptr(), *sheet = sys ? get_gui_sheet(sys, 0) : 0, *w = win_of(prefix, name);
    int depth = 0;
    if (!gp) gp = (parent_fn)need("?getParent@Window@CEGUI@@QBEPAV12@XZ");
    for (; w && gp && depth < 32; w = gp(w, 0), depth++) if (w == sheet) return pc_shown_in(prefix, name);
    return 0;
}

/* A left click at the centre of PC's window prefix+name (the mouse PC's own buttons take). */
static void press_pc(const char *prefix, const char *name) {
    typedef struct { float top, bottom, left, right; } rect6;
    typedef rect6 *(__fastcall *pixel_rect_fn6)(const void *self, void *edx, rect6 *out);
    static pixel_rect_fn6 rect_of;
    void x360_inject_left(int down, float x, float y);
    void *w = win_of(prefix, name);
    rect6 r;
    if (!rect_of) rect_of = (pixel_rect_fn6)need("?getUnclippedPixelRect@Window@CEGUI@@QBE?AVRect@2@XZ");
    if (!w || !rect_of) return;
    rect_of(w, 0, &r);
    x360_inject_left(1, (r.left + r.right) * 0.5f, (r.top + r.bottom) * 0.5f);
    x360_inject_left(0, (r.left + r.right) * 0.5f, (r.top + r.bottom) * 0.5f);
    x360_log("x360: pressed PC's "); x360_log(prefix); x360_log(name); x360_log("\r\n");
}

static int pc_shown(const char *name) { return pc_shown_in(pc_qd_prefix, name); }

/* PC's cinematic menu (cinematicmenu.layout), its prefix: Skip by DialogTextFrame. Attached, not
 * only found: PC takes a closed menu off the sheet (as the settings' search). No other layout of
 * PC's has a window ending in Skip. */
static char cin_prefix[64];
static int cin_shown;            /* the 360's frame shown over PC's story screen */

static int pc_cinematic_up(void) {
    static const char *const needs[] = {"DialogTextFrame", 0};
    static unsigned last_search;
    unsigned now = GetTickCount();
    /* Once found, PC's menu is kept (it is taken off the sheet and put back, not made again): its
     * parent chain is the test, and the tree is walked again only if its Skip is gone. */
    if (cin_prefix[0] && win_of(cin_prefix, "Skip")) return pc_attached(cin_prefix, "Skip");
    if (now - last_search < 250) return 0;   /* the tree's walk at most four times a second */
    last_search = now;
    find_pc_by_suffix("Skip", needs, cin_prefix, sizeof cin_prefix);
    return cin_prefix[0] && pc_attached(cin_prefix, "Skip");
}

/* Start during a cinematic skips it (the owner's look at the 360, 2026-10-24): PC's cinematic
 * menu's Skip, a left click at its centre (press_pc). */
void x360_cinematic_skip(void) {
    int x360_overlay_open(void);
    if (qd_shown || x360_overlay_open()) return;   /* a talk, a modal, fishing, a tip: Start does nothing there, and no Escape reaches PC's screen under them (the release review) */
    if (cin_shown) return;                          /* the story screen's own buttons (cinematic_bar_tick) */
    if (pc_cinematic_up()) { press_pc(cin_prefix, "Skip"); return; }
    /* In play no Skip is shown: PC's Escape skips it, as the 360's Start (the owner, 2026-10-24),
     * posted to the game's window (keys_proc lets it through while the cinematic holds). */
    {
        __declspec(dllimport) int __stdcall PostMessageW(HWND w, unsigned msg, unsigned wparam, long lparam);
        extern HWND x360_keys_window(void);
        HWND w = x360_keys_window();
        if (w) {
            PostMessageW(w, 0x100, 0x1b, 0x00010001);           /* WM_KEYDOWN Esc (scan code 1) */
            PostMessageW(w, 0x101, 0x1b, (long)0xc0010001);     /* WM_KEYUP */
            x360_log("x360 menu: Start in a cinematic, Escape posted\r\n");
        } else x360_log("x360 menu: Start in a cinematic, no Skip and no game window\r\n");
    }
}

/* The story screens (PC's cinematic menu with its Skip shown: a new hero's intro, the acts'
 * pages): the 360's frame over PC's page. The 360's CCinematicMenu (sub_82349BA8) loads
 * media/ui/cinematic.layout and looks up only Skip, DialogText and Background; its frame is the
 * layout's own: main_topborder with its ends at the top, main_bottomborder (1024 x 93, bottom,
 * centred) with its ends at the bottom and in it the A button with the layout's text. Its pad
 * (sub_82349E80): any button continues (the menu's +26; code 10 also writes the cinematics'
 * +6024 and +6028, not followed). Here PC's page and text stay (its Background and DialogText);
 * the 360's black backdrop, page and text are left out, PC's bars and Skip made transparent, and
 * A, B, X, Y and Start press PC's Skip. */
#define CIN_PREFIX "x360cin_"
static void cinematic_bar_tick(void) {
    static void *root;
    static int failed;
    static unsigned prev;
    static const char *const pc_bars[] = {"TopBarB", "TopBarLeft", "TopBarRight", "BottomBarB", "BottomBarLeft",
                                          "BottomBarRight", "Skip", 0};
    unsigned buttons, pressed;
    int j;
    int x360_overlay_open(void), x360_pause_open(void);
    if (failed) return;
    if (!pc_cinematic_up() || qd_shown || x360_overlay_open() || x360_pause_open() || inventory_open) {
        if (cin_shown) {   /* PC's bars back, for whatever shows the menu now */
            cin_shown = 0;
            show(root, 0);
            for (j = 0; pc_bars[j]; j++) prop(win_of(cin_prefix, pc_bars[j]), "Alpha", "1");
        }
        return;
    }
    if (!root) {
        RECT r;
        HWND hw = active_window();
        root = load("media/ui/x360/cinematic.layout", CIN_PREFIX);
        if (!root) { failed = 1; return; }   /* PC's own bars stay */
        if (!k) k = (hw && GetClientRect(hw, &r) && r.bottom > r.top) ? (float)(r.bottom - r.top) / 768.0f : 1.0f;
        scale_tree(root, k, 0);
        pass_mouse(root);
        prop(win_of(CIN_PREFIX, "i_947"), "Image", "");   /* the black backdrop: PC's page shows */
        show(win_of(CIN_PREFIX, "Background"), 0);
        show(win_of(CIN_PREFIX, "i_962"), 0);             /* the text: PC's */
        show(win_of(CIN_PREFIX, "Skip"), 0);
        show(root, 0);
    }
    if (!cin_shown) {
        RECT r;
        HWND hw = active_window();
        if (hw && GetClientRect(hw, &r) && r.bottom > r.top)   /* the 360's spacing, as the tip's */
            match_spacing((float)(r.right - r.left), (float)(r.bottom - r.top));
        cin_shown = 1;
        show(root, 1);
        for (j = 0; pc_bars[j]; j++) prop(win_of(cin_prefix, pc_bars[j]), "Alpha", "0");
        move_to_front(root, 0);
        prev = x360_pad_buttons();      /* a button held from before does not act */
        x360_log("x360 cinematic: the 360's frame over PC's story screen\r\n");
    }
    buttons = x360_pad_buttons();
    pressed = buttons & ~prev;
    prev = buttons;
    if (pressed & (0x1000 | 0x2000 | 0x4000 | 0x8000 | 0x0010)) {
        x360_log("x360 cinematic: a button, PC's Skip\r\n");
        press_pc(cin_prefix, "Skip");
    }
}

/* The first non-empty Image of w or a window under it. */
static void first_image_under(void *w, char *out, unsigned size) {
    void *stack[64];
    int sp = 0;
    out[0] = 0;
    if (!w) return;
    stack[sp++] = w;
    while (sp > 0 && !out[0]) {
        void *v = stack[--sp];
        unsigned c, n = child_count(v, 0);
        read_prop(v, "Image", out, size);
        for (c = n; c-- > 0 && sp < 63;) stack[sp++] = child_at(v, 0, c);
    }
}

/* The reward slot i's sockets (1-3), from PC's overlay image over that slot (QuestRewardSocketsO's
 * windows, in the order the dump showed: slot 1, 2, 3 at the slots' places): "one..", "two..". */
static int pc_reward_sockets(int i) {
    void *o = pc_window("QuestRewardSocketsO"), *slot = win_of(pc_qd_prefix, i == 1 ? "ItemSlot1" : i == 2 ? "ItemSlot2" : "ItemSlot3");
    unsigned c, n = o ? child_count(o, 0) : 0;
    char want[128], have[128], img[96];
    read_prop(slot, "UnifiedAreaRect", want, sizeof want);
    for (c = 0; c < n; c++) {
        void *w = child_at(o, 0, c);
        read_prop(w, "UnifiedAreaRect", have, sizeof have);
        {
            unsigned q = 0;
            while (want[q] && want[q] == have[q]) q++;
            if (want[q] || have[q]) continue;            /* not over this slot */
        }
        read_prop(w, "Image", img, sizeof img);
        {
            const char *p = img;
            while (*p && !(p[0] == 'i' && p[1] == 'm' && p[2] == 'a' && p[3] == 'g' && p[4] == 'e' && p[5] == ':')) p++;
            if (*p) p += 6;
            if (p[0] == 'o' && p[1] == 'n' && p[2] == 'e') return 1;
            if (p[0] == 't' && p[1] == 'w' && p[2] == 'o') return 2;
            if (p[0] == 't' && p[1] == 'h' && p[2] == 'r') return 3;
        }
        return 0;
    }
    return 0;
}

static void qd_mirror_text(const char *name) {
    char v[160];
    read_prop(win_of(pc_qd_prefix, name), "Text", v, sizeof v);
    text(win_of(QD_PREFIX, name), v);
}

/* A left click on PC's button (its centre, the mouse injected into CEGUI). */
static void qd_press(const char *name) { press_pc(pc_qd_prefix, name); }

/* The quest marker over the NPC ("?", "!") hidden while its dialog is up, as the 360's (town/42
 * against 43). PC's markers are effects on the unit by name, "QUEST COMPLETE", "QUEST INCOMPLETE",
 * "QUEST GIVING", "QUEST PASSIVE" (PC's own wide strings at 0xabad3c, 0xabad18, 0xabacfc,
 * 0xabace0), each taken off by 0x482710 (the unit, the name), as the quest manager's update
 * 0x5f0f50 does before it puts the one of the NPC's state back; that update (the manager
 * *0xf25320, as 0x5efa20 returns it) is called again when the dialog closes. */
void *x360_talked_unit;
static void *marker_hidden_on;
static void quest_marker(int hide) {
    typedef void (__fastcall *unit_str_fn)(void *unit, void *edx, const void *name);
    typedef void (__fastcall *update_fn)(void *manager, void *edx, void *unit);
    const unsigned names[] = {0x00abad3c, 0x00abad18, 0x00abacfc, 0x00abace0};
    wstr_ctor_fn wctor;
    wstr_dtor_fn wdtor;
    unsigned char ws[32];
    unsigned i;
    if (hide) {
        void *u = x360_talked_unit;
        if (!u || !wstring_functions(&wctor, &wdtor)) return;
        __try {
            for (i = 0; i < 4; i++) {
                wctor(ws, 0, (const unsigned short *)names[i]);
                ((unit_str_fn)0x00482710)(u, 0, ws);
                wdtor(ws, 0);
            }
            marker_hidden_on = u;
            x360_log("x360 dialog: the NPC's quest marker hidden\r\n");
        } __except (1) { x360_log("x360 dialog: hiding the quest marker threw\r\n"); }
    } else if (marker_hidden_on) {
        void *manager = *(void **)0x00f25320;
        __try { if (manager) ((update_fn)0x005f0f50)(manager, 0, marker_hidden_on); }
        __except (1) { x360_log("x360 dialog: the quest marker's update threw\r\n"); }
        marker_hidden_on = 0;
        x360_talked_unit = 0;
    }
}

static int qd_reward;   /* the reward item the stick selected (the 360's +464), -1 none */
static int qd_card;     /* the 360's item card shown for a reward (fill_card's card 0) */
static void *qd_card_item;   /* its item */
/* The selected slot drawn over the highlight (the 360's capture: the gold frame around, the item's
 * icon over it; the rollover image's centre is opaque, so the slot comes in front with its own
 * background image off, given back when the selection goes). */
static void *qd_lifted;
static char qd_lifted_image[96];
static void qd_lift(void *bkg) {
    if (qd_lifted == bkg) return;
    if (qd_lifted) prop(qd_lifted, "Image", qd_lifted_image);
    qd_lifted = bkg;
    if (!bkg) return;
    read_prop(bkg, "Image", qd_lifted_image, sizeof qd_lifted_image);
    prop(bkg, "Image", "");
    move_to_front(bkg, 0);
}
static void nav_point(float x, float y);   /* below: the hidden cursor moved */
/* The reward item i of the quest PC's dialog offers, 0 if none: PC's own mouse-over (GOG 0x59be40,
 * CQuestDialogMenu::handle_MouseOver *(OT)*) takes CGameUI's +0x2fc, its dialog at +0x98, that
 * dialog's quest at +8, the quest's rewards at +0x150 and their items' list from 0x5f4760
 * (getRewardItems *(OT)*: the data, the count at +4, the capacity at +8). */
static void *qd_reward_item(int i) {
    typedef unsigned char *(__fastcall *list_fn)(void *self, void *edx);
    void *found = 0;
    __try {
        unsigned char *menu = ui ? *(unsigned char **)(ui + 0x2fc) : 0;
        if (menu && *(unsigned *)menu != 0x00ab7ff0) menu = 0;   /* CQuestDialogMenu's vtable */
        unsigned char *dlg = menu ? *(unsigned char **)(menu + 0x98) : 0;
        unsigned char *quest = dlg ? *(unsigned char **)(dlg + 8) : 0;
        unsigned char *rw = quest ? *(unsigned char **)(quest + 0x150) : 0;
        unsigned char *list = rw ? ((list_fn)0x005f4760)(rw, 0) : 0;
        if (list && i >= 0 && i < *(int *)(list + 4) && (unsigned)i < *(unsigned *)(list + 8)) found = (*(void ***)list)[i];
    } __except (1) { found = 0; }
    return found;
}
static void qd_show(int on) {
    if (on == qd_shown) return;
    qd_reward = -1;
    if (qd_card) { hide_card(0); qd_card = 0; }
    qd_lift(0);
    quest_marker(on);
    qd_shown = on;
    if (!on) overlay_closed_at = GetTickCount();
    /* A button held from before does not act (run B2: the A that talked, held, accepted the
     * quest at once), as the tip and the modal. */
    if (on) qd_prev_buttons = x360_pad_buttons();
    set_visible(qd_root, 0, on && !x360_dialog_pc);
    prop(win_of(pc_qd_prefix, "Frame"), "Alpha", on && !x360_dialog_pc ? "0" : "1");
    if (hud_root) set_visible(hud_root, 0, !on);
    if (pet_hud_root) set_visible(pet_hud_root, 0, !on);
    if (on) move_to_front(qd_root, 0);
    x360_log(on ? "x360 dialog: shown over PC's\r\n" : "x360 dialog: hidden\r\n");
}

/* The pad's A on an NPC (proxy/pad.c): PC's dialog is looked for at every frame for a while,
 * not every 250 ms (run B: PC's showed for a moment the first time). */
int x360_tip_open(void);   /* the tip has the pad over the 360's other screens (tip_tick) */
static unsigned dialog_expected_at;
void x360_dialog_expect(void) { dialog_expected_at = GetTickCount(); }

static void dialog_tick(void) {
    static unsigned last_search;
    unsigned now = GetTickCount(), buttons, pressed;
    char v[2048];
    static char quest_prefix[48], plain_prefix[48];
    static void *plain_text;
    const char *active;
    if (qd_failed) return;
    /* PC has two NPC dialogs: questdialogmenu (a portrait, rewards) and dialogmenu (Title,
     * DialogText, Ok, Accept, Decline; run A: an NPC with a quest opened it). The 360's image
     * names only questdialogmenu.uilayout (its one NPC dialog layout loaded, 0x820CF924), so
     * the 360's is shown over either; dialogmenu's DialogText is found as the one with a Title
     * beside it (the quest dialog's has none). */
    if ((!pc_qd_text_frame || !plain_text) && (now - last_search > 250 || now - dialog_expected_at < 10000)) {
        static const char *const plain_needs[] = {"Title", 0};
        last_search = now;
        if (!pc_qd_text_frame && (pc_qd_text_frame = find_pc_dialog()) != 0)
            copy(quest_prefix, sizeof quest_prefix, pc_qd_prefix, 0);
        if (!plain_text) plain_text = find_pc_by_suffix("DialogText", plain_needs, plain_prefix, sizeof plain_prefix);
    }
    /* PC closes its dialog by taking it off the GUI sheet (it stays "visible"): open while its
     * Frame is visible and under the sheet. */
    active = pc_qd_text_frame && pc_attached(quest_prefix, "Frame") ? quest_prefix
           : plain_text && pc_attached(plain_prefix, "Frame") ? plain_prefix : 0;
    if (!active) { if (qd_shown) qd_show(0); qd_last = 0; return; }
    if (!qd_shown) copy(pc_qd_prefix, sizeof pc_qd_prefix, active, 0);
    if (!qd_root) {
        RECT r;
        HWND hw = active_window();
        qd_root = load("media/ui/x360/questdialogmenu.layout", QD_PREFIX);
        if (!qd_root) { qd_failed = 1; return; }
        if (!k) k = (hw && GetClientRect(hw, &r) && r.bottom > r.top) ? (float)(r.bottom - r.top) / 768.0f : 1.0f;
        scale_tree(qd_root, k, 0);
        pass_mouse(qd_root);
        set_visible(qd_root, 0, 0);
        x360_log("x360 dialog: loaded questdialogmenu.layout\r\n");
    }
    if (!qd_shown) {
        /* The 360's letter spacing and native width (match_spacing), which the inventory's
         * opening did before: a dialog can come first (the review: its text 1.3 times wider). */
        RECT r;
        HWND hw = active_window();
        if (hw && GetClientRect(hw, &r) && r.bottom > r.top)
            match_spacing((float)(r.right - r.left), (float)(r.bottom - r.top));
    }
    qd_show(1);
    /* The windows, as PC's. */
    /* The NPC's portrait as PC's, and none where PC shows none (the owner, 2026-10-25: town NPCs
     * without one, on the 360 too, showed the last one's, an empty Image not replacing it; PC's
     * plain dialogmenu has no Portrait at all). */
    read_prop(win_of(pc_qd_prefix, "Portrait"), "Image", v, sizeof v);
    {
        int has = v[0] && pc_shown("Portrait");
        if (has) prop(win_of(QD_PREFIX, "Portrait"), "Image", v);
        show(win_of(QD_PREFIX, "Portrait"), has);
    }
    read_prop(win_of(pc_qd_prefix, "DialogText"), "Text", v, sizeof v);
    {
        /* A new text (its hash): the rise starts again. */
        static unsigned seen;
        unsigned i, chars = 0, h = 2166136261u;
        for (i = 0; v[i]; i++) h = (h ^ (unsigned char)v[i]) * 16777619u;
        if (h != seen) {
            seen = h;
            for (i = 0; v[i]; i++) if ((v[i] & 0xc0) != 0x80) chars++;
            /* The 360's colour markup (|cAARRGGBB .. |u) is drawn as text by this CEGUI's
             * StaticText: taken out, the words kept (their colour is open, docs 14). */
            if (!x360_dialog_markup) {
                unsigned r = 0, w2 = 0;
                while (v[r]) {
                    if (v[r] == '|' && v[r + 1] == 'c') { unsigned j = 2; while (j < 10 && v[r + j]) j++; r += j; continue; }
                    if (v[r] == '|' && v[r + 1] == 'u') { r += 2; continue; }
                    v[w2++] = v[r++];
                }
                v[w2] = 0;
            }
            if (x360_dialog_font[0]) prop(win_of(QD_PREFIX, "DialogText"), "Font", x360_dialog_font);
            text(win_of(QD_PREFIX, "DialogText"), v);
            qd_T = (float)chars * 0.015f;
            if (qd_T < 5.0f) qd_T = 5.0f;
            qd_t = 0;
        }
    }
    show(win_of(QD_PREFIX, "Ok"), pc_shown("Ok"));
    show(win_of(QD_PREFIX, "Accept"), pc_shown("Accept"));
    show(win_of(QD_PREFIX, "Decline"), pc_shown("Decline"));
    show(win_of(QD_PREFIX, "Fast Scroll"), 1);
    {
        static const char *const box[3] = {"RewardGoldBox", "RewardXPBox", "RewardFameBox"};
        static const char *const val[3] = {"RewardGold", "RewardXP", "RewardFame"};
        int i, items = 0;
        for (i = 0; i < 3; i++) {
            show(win_of(QD_PREFIX, box[i]), pc_shown(box[i]));
            qd_mirror_text(val[i]);
        }
        for (i = 1; i <= 3; i++) {
            char bkg[16], slot[16];
            int on;
            copy(bkg, sizeof bkg, "ItemBkg", 0); bkg[7] = (char)('0' + i); bkg[8] = 0;
            copy(slot, sizeof slot, "ItemSlot", 0); slot[8] = (char)('0' + i); slot[9] = 0;
            on = pc_shown(bkg);
            items += on;
            show(win_of(QD_PREFIX, bkg), on);
            /* PC draws the item's icon in a window under its ItemSlot (icon__N, its child with
             * the Image: a dump of the quest NPC's offer); the first image under it is the icon. */
            first_image_under(win_of(pc_qd_prefix, slot), v, sizeof v);
            prop(win_of(QD_PREFIX, slot), "Image", v);
            {
                /* The icon square, as the 360 draws it (the owner's capture: not stretched over the
                 * slot's 40x65 window): the window made as tall as it is wide, its middle kept. */
                static unsigned char squared[4];
                void *sw = win_of(QD_PREFIX, slot);
                char rr[128];
                float q[8], wdt, hgt;
                if (sw && !squared[i]) {
                    read_prop(sw, "UnifiedAreaRect", rr, sizeof rr);
                    if (parse_rect(rr, q) && (wdt = q[5] - q[1]) > 0 && (hgt = q[7] - q[3]) > wdt) {
                        q[3] += (hgt - wdt) * 0.5f;
                        q[7] = q[3] + wdt;
                        write_rect(rr, q);
                        prop(sw, "UnifiedAreaRect", rr);
                    }
                    squared[i] = 1;
                }
            }
            {
                /* Its empty sockets: PC's overlay over the slot (QuestRewardSocketsO, "onesocketglow",
                 * "twosocketglow"); the 360's EmptySocketN_0 and _1 shown for as many. */
                char e[24];
                int n = on ? pc_reward_sockets(i) : 0, q;
                for (q = 0; q < 2; q++) {
                    copy(e, sizeof e, "EmptySocket", 0);
                    e[11] = (char)('0' + i); e[12] = '_'; e[13] = (char)('0' + q); e[14] = 0;
                    show(win_of(QD_PREFIX, e), q < n);
                }
            }
        }
        /* The left stick's left and right select a reward item (the 360's pad, sub_82357ED0: codes
         * 19 and 20 move +464 by one; sub_82358058 wraps it from -1, none, to the last and back,
         * puts ItemHighlight (+440) on the slot (+408) and the item in the item tooltip there;
         * the d-pad does nothing). Here the highlight on the 360's slot, and the cursor (hidden)
         * on PC's slot, where PC shows that item's card as its hover does (the owner, 2026-10-24:
         * the stick did nothing). */
        {
            int x360_pad_lx(void);
            static int held;
            int lx = x360_tip_open() ? 0 : x360_pad_lx(), dir = lx < -500 ? -1 : lx > 500 ? 1 : 0;
            if (dir && !held && items > 0) {
                qd_reward += dir;
                if (qd_reward < -1) qd_reward = items - 1;
                if (qd_reward >= items) qd_reward = -1;
                if (qd_card) { hide_card(0); qd_card = 0; }
                if (qd_reward < 0) qd_lift(0);
                if (qd_reward >= 0) {
                    typedef struct { float top, bottom, left, right; } rect6;
                    typedef rect6 *(__fastcall *pixel_rect_fn6)(const void *self, void *edx, rect6 *out);
                    static pixel_rect_fn6 rect_of;
                    char slot[16], r[128];
                    void *item = qd_reward_item(qd_reward), *bkg;
                    float h;
                    rect6 br;
                    copy(slot, sizeof slot, "ItemBkg", 0); slot[7] = (char)('1' + qd_reward); slot[8] = 0;
                    bkg = win_of(QD_PREFIX, slot);
                    read_prop(bkg, "UnifiedPosition", r, sizeof r);   /* its place only, as the 360 (+248) */
                    prop(win_of(QD_PREFIX, "ItemHighlight"), "UnifiedPosition", r);
                    qd_lift(bkg);
                    /* The item's card, the 360's (the inventory's, fill_card), where the 360 draws
                     * it (the owner's captures of the 360 and the port, 2026-10-24, 1920x1080): its
                     * right edge 6 % of the slot's width into the slot, its middle at the slot's;
                     * its size is its text's (the 360's in Spanish there, wider). */
                    if (!rect_of) rect_of = (pixel_rect_fn6)need("?getUnclippedPixelRect@Window@CEGUI@@QBE?AVRect@2@XZ");
                    h = item ? fill_card(0, item, 0, 0) : 0;
                    if (h > 0) { qd_card = 1; qd_card_item = item; }   /* shown by fill_card: tracked whatever follows */
                    if (h > 0 && bkg && rect_of) {
                        fresh_rects(bkg);
                        rect_of(bkg, 0, &br);
                        place_card(0, br.left + 0.06f * (br.right - br.left) - card_width(0), 0, (br.top + br.bottom) * 0.5f - h * 0.5f);
                    }
                    nav_point(1.0f, 1.0f);   /* the hidden cursor off PC's slots: no hover card of PC's too */
                    if (!item) x360_log("x360 dialog: no item for that reward\r\n");
                }
                x360_log("x360 dialog: reward item "); x360_log_num(qd_reward); x360_log("\r\n");
            }
            held = dir != 0;
            /* The selection dropped when the slots or their item change under it (the talk's next
             * page or quest): no card left on an item no longer offered. */
            if (qd_reward >= items || (qd_card && qd_reward_item(qd_reward) != qd_card_item)) {
                qd_reward = -1;
                if (qd_card) { hide_card(0); qd_card = 0; }
                qd_lift(0);
            }
        }
        show(win_of(QD_PREFIX, "ItemHighlight"), qd_reward >= 0);
        if (qd_card && card_root(0)) move_to_front(card_root(0), 0);   /* the card over the 360's dialog */
        /* "Rewards" with the stick's prompt when there are items (town/04; not town/13). */
        show(win_of(QD_PREFIX, "RewardsTooltip"), items > 0);
    }
    /* The pad. */
    buttons = x360_pad_buttons();
    pressed = buttons & ~qd_prev_buttons;
    qd_prev_buttons = buttons;
    if (x360_tip_open()) buttons = pressed = 0;   /* a tip shown over it has the pad (drawn in front: the owner's run) */
    /* The frame's time, faster with X held. */
    if (qd_last) qd_t += (float)(now - qd_last) / 1000.0f * (buttons & 0x4000 ? (float)x360_dialog_fast_scroll : 1.0f);
    qd_last = now;
    /* Start and Back do nothing in a talk (the owner's look at the 360, 2026-10-24: so in a quest's
     * talk; a common talk is closed by them there, but its first pages look the same as a quest's,
     * Ok only, and the port has no sign of which it is yet). proxy/pad.c keeps them from the pause
     * and the inventory. */
    if (pressed & 0x1000) qd_press(pc_shown("Accept") ? "Accept" : "Ok");    /* A */
    if ((pressed & 0x2000) && pc_shown("Decline")) qd_press("Decline");       /* B */
    /* The rising text. */
    {
        void *t = win_of(QD_PREFIX, "DialogText");
        float vr[8], s = k * 768.0f / 720.0f, h, elapsed = qd_t, y;
        char rect[128];
        read_prop(t, "UnifiedAreaRect", rect, sizeof rect);
        if (t && parse_rect(rect, vr)) {
            h = vr[7] - vr[3];
            if (elapsed > qd_T) elapsed = qd_T;
            y = (1.0f - elapsed / qd_T) * (h + 50.0f * s);
            vr[3] = y; vr[7] = y + h;
            write_rect(rect, vr);
            prop(t, "UnifiedAreaRect", rect);
        }
    }
}

/* ---- the labels over objects (in-game/04-08) ----
 * The engine labels the objects near (a chest, a shrine, a lever, a fishing hole) with a window
 * each on the GUI sheet, placed over the object: PC's of type GuiLook/ItemText (GOG 0x55aa5e),
 * the 360's ceguiWidget/ItemTextButton (its look draws the a_button 20 pixels left of the text
 * when the window is enabled). The 360 enables the label of what A would use, in white
 * (FFFFFFFF), and disables the others, in grey (FFCCCCCC) (the catalog's 07: "A Chest" with
 * "healing shrine" grey behind). Here each of PC's labels is given the 360's look and font,
 * and the one whose text is the candidate's name (pad.c's) enabled. */
void *x360_pad_candidate(void);
#define GOG_UNIT_LABEL_NAME 0x420           /* a unit's shown name, a std::wstring (also the target bar's) */
static int screen_w(void) { RECT r; HWND w = active_window(); return w && GetClientRect(w, &r) ? r.right - r.left : 1920; }
static int screen_h(void) { RECT r; HWND w = active_window(); return w && GetClientRect(w, &r) ? r.bottom - r.top : 1080; }
static void labels_tick(void) {
    typedef const void *(__fastcall *str_fn)(const void *self, void *edx);
    typedef void (__fastcall *set_str_fn)(void *self, void *edx, const void *s);
    static str_fn get_type;
    static set_str_fn set_look;
    void *sys = system_ptr(), *sheet = sys ? get_gui_sheet(sys, 0) : 0, *ingame = 0;
    unsigned i, n;
    char want[96];
    unsigned char *cand = (unsigned char *)x360_pad_candidate();
    /* The candidate's label: PC's label loop (GOG 0x55a8db-0x55b248, over the level's units) keeps
     * each unit's label window at its +0x198 (made if 0 at 0x55a9ef, placed by setPosition at
     * 0x55b21b after the projection 0x541800), so the label is the candidate's own, not one found
     * by its text and the screen's centre. */
    void *cand_label = cand ? *(void **)(cand + 0x198) : 0;
    if (!get_type) get_type = (str_fn)need("?getType@Window@CEGUI@@QBEABVString@2@XZ");
    if (!set_look) set_look = (set_str_fn)need("?setLookNFeel@Window@CEGUI@@UAEXABVString@2@@Z");
    if (!sheet || !get_type || !set_look) return;
    /* The 360 labels the items on the floor near the hero, all of them (in-game/20, 25); PC only
     * the one under the mouse, or all while its show-items key (KEYMAP_SHOWITEMS, Alt) is down or
     * its "TOGGLE DROPPED ITEMS" action is on (CGameUI's update 0x5578e0, its label loop at
     * 0x55a942: the key's state against the toggle's). The toggle's state is the input object's
     * (CGameUI +0x3c) entry [0xf60c90] of its vector at +0x80 (0x5c53d0), 1 when on (0x557af8):
     * set on, so the pad's player sees them as on the 360. */
    if (ui) {
        unsigned char *inp = *(unsigned char **)(ui + 0x3c);
        int idx = *(int *)0x00f60c90;
        int **vec = inp ? (int **)(inp + 0x80) : 0;
        static int logged_toggle;
        if (vec && vec[0] && vec[1] > vec[0] && idx >= 0 && idx < vec[1] - vec[0] && vec[0][idx] != 1) {
            vec[0][idx] = 1;
            if (logged_toggle < 3) { x360_log("x360 labels: PC's dropped items toggle set on\r\n"); logged_toggle++; }
        }
    }
    /* With the toggle on PC labels every unit of its two lists on the screen (the level's +0x7c,
     * the items, and +0x80, the objects: chests, shrines, exits; GOG 0x55a8e0, 0x55b263). The 360
     * (sub_821A83C8's label loop at 0x821AA9FC, the mods agent's reading) labels a unit when it
     * is the pad's target (unless of type 30), or when its distance to the hero is under the root
     * of 20.0 (const pool 0x820AA2C0 +0x244: 4.47) and it is not of type 29 (the breakables): the
     * others are made transparent here (QA run 1: the mine's entrance labelled from far off). */
    if (level && hero) {
        static const unsigned lists[] = {0x7c, 0x80};
        float hp[3], up[3];
        unsigned li;
        ((get_position_fn)GOG_GET_POSITION)(hero, 0, hp, 1);
        for (li = 0; li < 2; li++) {
            unsigned char *lst = *(unsigned char **)(level + lists[li]);
            unsigned char *node = lst ? *(unsigned char **)lst : 0;
            int guard = 0;
            for (; node && guard < 4096; node = *(unsigned char **)(node + 4), guard++) {
                unsigned char *u = *(unsigned char **)node;
                void *lw = u ? *(void **)(u + 0x198) : 0;
                float dx, dy, dz;
                int near_, on;
                char a[16];
                if (!lw) continue;
                ((get_position_fn)GOG_GET_POSITION)(u, 0, up, 1);
                dx = up[0] - hp[0]; dy = up[1] - hp[1]; dz = up[2] - hp[2];
                near_ = dx * dx + dy * dy + dz * dz < 20.0f;
                on = u == cand ? !isa(u, 30) : near_ && !isa(u, 29);
                read_prop(lw, "Alpha", a, sizeof a);
                if (on ? a[0] == '0' && a[1] != '.' : !(a[0] == '0' && a[1] != '.'))
                    prop(lw, "Alpha", on ? "1" : "0");
            }
        }
    }
    want[0] = 0;
    if (cand && !wstring_utf8(cand + GOG_UNIT_LABEL_NAME, want, sizeof want))
        if (!wstring_utf8(cand + GOG_ITEM_NAME, want, sizeof want)) want[0] = 0;
    {
        typedef const void *(__fastcall *name_fn)(const void *self, void *edx);
        static name_fn gn;
        if (!gn) gn = (name_fn)need("?getName@Window@CEGUI@@QBEABVString@2@XZ");
        n = gn ? child_count(sheet, 0) : 0;
        for (i = 0; i < n && !ingame; i++) {
            void *c = child_at(sheet, 0, i);
            const char *nm = c_str(gn(c, 0), 0);
            if (nm && nm[0] == 'I' && nm[1] == 'n' && nm[2] == 'g') ingame = c;   /* "Ingame UI Sheet" */
        }
    }
    {
        /* PC's labels: children of the sheet's first child (Ingame UI Sheet) of type GuiLook/ItemText. */
        void *root = ingame, *best = 0;
        float best_d = 1e30f;
        unsigned m = root ? child_count(root, 0) : 0, j;
        int pass;
        for (pass = 0; pass < 2; pass++)
        for (j = 0; j < m; j++) {
            void *w = child_at(root, 0, j);
            const char *type = c_str(get_type(w, 0), 0);
            char t[96];
            int on;
            static int logged;
            if (!type || type[0] != 'G' || type[1] != 'u' || length(type) != 16) continue;   /* GuiLook/ItemText */
            read_prop(w, "LookNFeel", t, sizeof t);
            if (t[0] != 'c') {   /* not yet ceguiWidget/ItemTextButton: setLookNFeel once (it remakes the window's imagery) */
                object look;
                string_ctor(&look, 0, "ceguiWidget/ItemTextButton");
                __try { set_look(w, 0, &look); } __except (1) { x360_log("x360 labels: setLookNFeel threw\r\n"); }
                string_dtor(&look, 0);
                prop(w, "Font", "X360_Serif");
                pass_mouse(w);
                if (logged < 8) {
                    read_prop(w, "Text", t, sizeof t);
                    x360_log("x360 labels: a label of PC's given the 360's look: "); x360_log(t); x360_log("\r\n");
                    logged++;
                }
            }
            /* The 360's label height (17.27 at 720: 26 pixels at 1080), its centre kept: PC's engine
             * makes it 40 high at 1080; the look's A button is a fraction of it. */
            {
                char rect[128];
                float vr[8], h = 17.27f * (float)screen_h() / 720.0f, mid;
                read_prop(w, "UnifiedAreaRect", rect, sizeof rect);
                if (parse_rect(rect, vr) && vr[0] == 0 && vr[2] == 0 && (vr[7] - vr[3] > h + 0.5f || vr[7] - vr[3] < h - 0.5f)) {
                    mid = (vr[3] + vr[7]) * 0.5f;
                    vr[3] = mid - h * 0.5f; vr[7] = mid + h * 0.5f;
                    write_rect(rect, vr);
                    prop(w, "UnifiedAreaRect", rect);
                }
            }
            read_prop(w, "Text", t, sizeof t);
            if (cand_label) {   /* the candidate's own label: exact */
                if (pass == 0) continue;
                on = w == cand_label;
                goto write_state;
            }
            on = want[0] && t[0] && length(t) == length(want);
            if (on) { unsigned q; for (q = 0; t[q]; q++) if (t[q] != want[q]) { on = 0; break; } }
            /* Two of the same name (two chests): only the one nearest the screen's centre, where
             * the hero is and so the candidate, nearest of them (the review: three "Chest" lit). */
            if (on) {
                char rect[128];
                float vr[8], dx, dy, d;
                read_prop(w, "UnifiedAreaRect", rect, sizeof rect);
                d = 1e9f;
                if (parse_rect(rect, vr)) {
                    dx = (vr[1] + vr[5]) * 0.5f - (float)screen_w() * 0.5f;
                    dy = (vr[3] + vr[7]) * 0.5f - (float)screen_h() * 0.5f;
                    d = dx * dx + dy * dy;
                }
                if (pass == 0) { if (d < best_d) { best_d = d; best = w; } continue; }
                on = w == best;
            } else if (pass == 0) continue;
        write_state:
            {
                /* The state written only when it changes (a window's last, by its pointer). */
                static void *seen[64];
                static signed char state[64];
                static char own[64][12];          /* PC's colour for the label (its rarity's), as first seen */
                unsigned q, slot = 64;
                for (q = 0; q < 64; q++) if (seen[q] == w) { slot = q; break; }
                if (slot == 64) for (q = 0; q < 64; q++) if (!seen[q]) { slot = q; seen[q] = w; state[q] = -1; own[q][0] = 0; break; }
                if (slot == 64) { slot = (unsigned)(GetTickCount() & 63); seen[slot] = w; state[slot] = -1; own[slot][0] = 0; }
                if (state[slot] == -1) {
                    char c[32];
                    read_prop(w, "TextColour", c, sizeof c);
                    copy(own[slot], sizeof own[slot], c, 0);
                }
                if (state[slot] != on) {
                    /* The 360 (in-game/07, 20, 25): A only on the label of what A would take; a
                     * label of a colour of its own (a unique's gold) keeps it either way, a plain
                     * one is white for the candidate and grey (FFCCCCCC) otherwise. */
                    int plain = !own[slot][0] || (own[slot][2] == 'F' && own[slot][3] == 'F' && own[slot][4] == 'F' && own[slot][5] == 'F'
                                                  && own[slot][6] == 'F' && own[slot][7] == 'F') || (own[slot][2] == 'C' && own[slot][3] == 'C');
                    state[slot] = (signed char)on;
                    prop(w, "Disabled", on ? "False" : "True");
                    prop(w, "TextColour", !plain ? own[slot] : on ? "FFFFFFFF" : "FFCCCCCC");
                }
            }
        }
    }
}

/* ---- fishing (in-game/09) ----
 * PC's CFishingMenu (CGameUI's +0x31c, vtable 0xab72a0, made at 0x563478; open +0x18) and the
 * 360's fishing.layout size their ring by the same rule (the 360's sub_821FFE90, read with gdb on
 * the recomp; PC's 0x57c997): of the player ([[menu+0x1c]+0x1c]), f = sin(phase +0x7a0) * 0.2 +
 * base +0x79c, or 1.0 when the fish bites (+0x7ac > 0); the ring's lower right corner at f * 128
 * (0xab72b8; the 360's 0x820AA2C0 base) pixels of the 360's 720 rows, its upper left kept (the
 * 360 sets the area's maximum only). While PC's menu is open the 360's layout is shown over it,
 * PC's made transparent; A gives PC's menu its command 0xe, B its command 6 (its virtual +0x14,
 * 0x57d060: the player's +0x170, or 0x494ab0 and the player's +0x174 with 2). */
#define GOG_FISHING_MENU 0x31c
static float x87_sin(float x) { float r; __asm__("fsin" : "=t"(r) : "0"(x)); return r; }   /* as PC: fsin */
#define FISH_PREFIX "x360fish_"
static void *fish_root;
static int fish_shown;
static void fishing_tick(void) {
    typedef unsigned char (__fastcall *cmd_fn)(void *self, void *edx, int cmd);
    static unsigned prev;
    unsigned char *m = ui ? *(unsigned char **)(ui + GOG_FISHING_MENU) : 0, *player;
    int open = m && m[0x18] && *(unsigned char **)(m + 0x1c) && *(unsigned char **)(*(unsigned char **)(m + 0x1c) + 0x1c);
    unsigned buttons, pressed;
    if (!open) {
        if (fish_shown) { fish_shown = 0; set_visible(fish_root, 0, 0); overlay_closed_at = GetTickCount(); x360_log("x360 fishing: hidden\r\n"); }
        return;
    }
    if (!fish_root) {
        fish_root = load("media/ui/x360/fishing.layout", FISH_PREFIX);
        if (!fish_root) return;
        if (!k) k = (float)screen_h() / 768.0f;
        scale_tree(fish_root, k, 0);
        pass_mouse(fish_root);
        x360_log("x360 fishing: loaded fishing.layout\r\n");
    }
    if (!fish_shown) {
        void *pc = pc_window("FishingRing"), *root = pc;
        typedef void *(__fastcall *parent_fn)(const void *self, void *edx);
        static parent_fn gp;
        if (!gp) gp = (parent_fn)need("?getParent@Window@CEGUI@@QBEPAV12@XZ");
        while (root && gp && gp(root, 0) && gp(gp(root, 0), 0)) root = gp(root, 0);   /* PC's layout's root */
        if (pc) prop(pc_window("FishingRing"), "Alpha", "0");
        if (pc_window("FishingRingTarget")) prop(pc_window("FishingRingTarget"), "Alpha", "0");
        if (pc_window("FishingButton")) prop(pc_window("FishingButton"), "Alpha", "0");
        if (pc_window("Base")) prop(pc_window("Base"), "Alpha", "0");
        fish_shown = 1;
        set_visible(fish_root, 0, 1);
        move_to_front(fish_root, 0);
        x360_log("x360 fishing: shown over PC's\r\n");
    }
    /* PC's ripples: the 360's layout has none and its code names only FishingRing (its image's
     * one fishing window name); kept transparent each frame, in case PC animates their alpha. */
    if (pc_window("FishingRipples")) prop(pc_window("FishingRipples"), "Alpha", "0");
    if (pc_window("FishingRipples2")) prop(pc_window("FishingRipples2"), "Alpha", "0");
    player = *(unsigned char **)(*(unsigned char **)(m + 0x1c) + 0x1c);
    {
        float f = 0.2f * x87_sin(*(float *)(player + 0x7a0)) + *(float *)(player + 0x79c), size;
        void *ring = win_of(FISH_PREFIX, "FishingRing");
        char rect[128];
        float vr[8];
        if (*(float *)(player + 0x7ac) > 0) f = 1.0f;
        size = f * 128.0f * (float)screen_h() / 720.0f;
        read_prop(ring, "UnifiedAreaRect", rect, sizeof rect);
        if (ring && parse_rect(rect, vr)) {
            vr[4] = vr[6] = 0;
            vr[5] = size; vr[7] = size;
            write_rect(rect, vr);
            prop(ring, "UnifiedAreaRect", rect);
        }
    }
    buttons = x360_pad_buttons();
    pressed = buttons & ~prev;
    prev = buttons;
    if (x360_tip_open()) buttons = pressed = 0;   /* a tip shown over it has the pad (drawn in front: the owner's run) */
    if (pressed & 0x1000) { ((cmd_fn)(*(void ***)m)[5])(m, 0, 0xe); x360_log("x360 fishing: A (PC's command 0xe)\r\n"); }
    if (pressed & 0x2000) { ((cmd_fn)(*(void ***)m)[5])(m, 0, 6); x360_log("x360 fishing: B (PC's command 6)\r\n"); }
}

/* Once a frame: the screens that must run when PC's dialogs stop the pad's polls. An
 * Ogre::FrameListener of the proxy's (its virtuals, as PC's own at GOG 0xabbac4: frameStarted,
 * frameRenderingQueued, frameEnded, the destructor), added to PC's Root from the HUD's tick. */
void x360_frame_tick(void);
static unsigned char __fastcall fl_started(void *self, void *edx, const void *evt) { x360_frame_tick(); return 1; }
static unsigned char __fastcall fl_other(void *self, void *edx, const void *evt) { return 1; }
static void *__fastcall fl_dtor(void *self, void *edx, unsigned flags) { return self; }
static void *fl_vtable[4] = {(void *)fl_started, (void *)fl_other, (void *)fl_other, (void *)fl_dtor};
static struct { void **vt; } frame_listener = {fl_vtable};
static void add_frame_listener(void) {
    typedef void *(__cdecl *root_fn)(void);
    typedef void (__fastcall *add_fn)(void *root, void *edx, void *listener);
    static int done;
    void *ogre, *root;
    root_fn get_root;
    add_fn add;
    if (done) return;
    ogre = GetModuleHandleA("OgreMain.dll");
    get_root = ogre ? (root_fn)GetProcAddress(ogre, "?getSingletonPtr@?$Singleton@VRoot@Ogre@@@Ogre@@SAPAVRoot@2@XZ") : 0;
    add = ogre ? (add_fn)GetProcAddress(ogre, "?addFrameListener@Root@Ogre@@QAEXPAVFrameListener@2@@Z") : 0;
    root = get_root ? get_root() : 0;
    if (!root || !add) return;
    add(root, 0, &frame_listener);
    done = 1;
    x360_log("x360 menu: frame listener added\r\n");
}

/* ---- PC's tips (the 360's CTipMenu, tipmenu.layout) ----
 * Both builds have a CTipMenu (the 360's loads media/ui/tipmenu.uilayout, sub_823587C8). PC's
 * (tipmenu.layout: Frame, Title, Dialog, Ok, ShowTips) is shown after a screen's first closing and
 * in town at a new game. The 360's window is shown over PC's while that is open (as the NPC
 * dialog, PC's made transparent), Title and Dialog as PC's. Its pad (sub_823589A0, on
 * GamepadButtonDown): B (6) closes it (the menu's virtual +28); Y (8) turns the tips off (the
 * settings' sub_821FA030 with 0, then saved) and closes it; the other buttons do nothing. Here B
 * presses PC's Ok, and Y presses PC's ShowTips when it is selected, then Ok: PC's own controls
 * turn the option off and close. */
#define TIP_PREFIX "x360tip_"
static void *tip_root;
static char pc_tip_prefix[48];
static int tip_failed, tip_shown;
static unsigned tip_closed_at;   /* the press that closed a tip does not reach the inventory under it */

/* A tip of the 360's has the pad (or closed less than 0.3 s ago): the inventory it shows over gets
 * no buttons then (the 360's CTipMenu keeps the buttons it gets, sub_823589A0 returns 1). */
int x360_tip_open(void) { return tip_shown || GetTickCount() - tip_closed_at < 300; }

typedef void *(__fastcall *parent_of_fn)(const void *self, void *edx);
/* The GUI sheet's child that holds PC's tip window (PC's Top UI Sheet), 0 when it is not attached. */
static void *pc_tip_sheet(void) {
    static parent_of_fn gp;
    void *sys = system_ptr(), *sheet = sys ? get_gui_sheet(sys, 0) : 0, *top = win_of(pc_tip_prefix, "Frame"), *up;
    int depth = 0;
    if (!gp) gp = (parent_of_fn)need("?getParent@Window@CEGUI@@QBEPAV12@XZ");
    if (!sheet || !gp) return 0;
    while (top && depth++ < 32) {
        up = gp(top, 0);
        if (up == sheet) return top;
        top = up;
    }
    return 0;
}

/* PC's tip shown: attached, and its own window shown. Its sheet's visibility is not asked: the
 * 360's screens hide PC's sheets while they are open, and PC shows the inventory's tip as the
 * 360's inventory opens (TIP_INVENTORY), into the hidden sheet (QA run of 2026-10-24). */
static int pc_tip_up(void) {
    typedef unsigned char (__fastcall *vis_fn)(const void *self, void *edx, unsigned char local);
    static vis_fn vis;
    void *frame = win_of(pc_tip_prefix, "Frame");
    if (!vis) vis = (vis_fn)need("?isVisible@Window@CEGUI@@QBE_N_N@Z");
    return frame && vis && pc_tip_sheet() && vis(frame, 0, 1);
}

/* A click on PC's tip's button. Its sheet, if a 360 screen hid it, is shown and brought in front
 * for the click only (synchronous: CEGUI's injected mouse), then hidden again and the inventory
 * brought in front again. */
static void press_tip(const char *name) {
    typedef unsigned char (__fastcall *vis_fn)(const void *self, void *edx, unsigned char local);
    typedef void *(__fastcall *get_target_fn)(const void *self, void *edx);
    typedef void (__fastcall *set_target_fn)(void *self, void *edx, void *w);
    static vis_fn vis;
    static get_target_fn get_target;
    static set_target_fn set_target;
    static parent_of_fn gp;
    void *top = pc_tip_sheet(), *sys = system_ptr(), *target = 0, *up;
    int was_hidden, depth = 0;
    if (!vis) vis = (vis_fn)need("?isVisible@Window@CEGUI@@QBE_N_N@Z");
    if (!get_target) get_target = (get_target_fn)need("?getModalTarget@System@CEGUI@@QBEPAVWindow@2@XZ");
    if (!set_target) set_target = (set_target_fn)need("?setModalTarget@System@CEGUI@@QAEXPAVWindow@2@@Z");
    if (!gp) gp = (parent_of_fn)need("?getParent@Window@CEGUI@@QBEPAV12@XZ");
    /* A window opened over the tip (PC's modal, as the mod's notice: the owner's run of
     * 2026-10-24, where the tip's clicks closed nothing) must not take the click: the tip's sheet
     * in front for it, and another window's CEGUI modal target, if any, let go for the click
     * only. Which window got it is logged (the cause is not settled). */
    if (sys && get_target && set_target && gp) {
        target = get_target(sys, 0);
        for (up = target; up && depth++ < 32; up = gp(up, 0))
            if (up == top) { target = 0; break; }
        if (target) { set_target(sys, 0, 0); x360_log("x360 tip: another window was modal, let go for the click\r\n"); }
    }
    was_hidden = top && vis && !vis(top, 0, 1);
    if (was_hidden) set_visible(top, 0, 1);
    if (top) move_to_front(top, 0);
    press_pc(pc_tip_prefix, name);
    log_under_mouse("x360 tip: the click went to");
    if (was_hidden) set_visible(top, 0, 0);
    if (inventory_open && master_root) move_to_front(master_root, 0);
    if (target && !get_target(sys, 0)) set_target(sys, 0, target);
}

/* A tip up: the 360's shown, or PC's window shown and not yet seen by tip_tick (which runs after
 * the notice and the stop). PC's modal is not opened over it: it would take the tip's clicks. */
static int tip_up_now(void) { return x360_tip_open() || (pc_tip_prefix[0] && pc_tip_up()); }

/* PC's tip window seen (alpha 1) or not (0): its root, made transparent as it loaded, and its Frame. */
static void pc_tip_alpha(const char *a) {
    prop(win_of(pc_tip_prefix, "Frame"), "Alpha", a);   /* the layout's root (by name: never a stale pointer) */
}

static void tip_tick(void) {
    static unsigned last_search, prev;
    static void *pc_tip;
    unsigned now = GetTickCount(), buttons, pressed;
    char v[2048];
    if (tip_failed) return;
    /* Its window found at once when its layout was seen loading (its root, "<prefix>Frame"). */
    if (!pc_tip && pc_tip_layout_root) {
        typedef const void *(__fastcall *name_fn)(const void *self, void *edx);
        static name_fn gn;
        const char *name;
        unsigned len;
        if (!gn) gn = (name_fn)need("?getName@Window@CEGUI@@QBEABVString@2@XZ");
        name = gn ? c_str(gn(pc_tip_layout_root, 0), 0) : 0;
        len = name ? length(name) : 0;
        if (len > 5 && len - 5 < sizeof pc_tip_prefix && name[len - 5] == 'F' && name[len - 4] == 'r' &&
            name[len - 3] == 'a' && name[len - 2] == 'm' && name[len - 1] == 'e') {
            unsigned i;
            for (i = 0; i < len - 5; i++) pc_tip_prefix[i] = name[i];
            pc_tip_prefix[len - 5] = 0;
            pc_tip = win_of(pc_tip_prefix, "ShowTips");
        }
    }
    if (!pc_tip && now - last_search > 250) { last_search = now; pc_tip = find_pc_by_suffix("ShowTips", 0, pc_tip_prefix, sizeof pc_tip_prefix); }
    if (!pc_tip) return;
    if (!pc_tip_up()) {
        if (tip_shown) {
            tip_shown = 0;
            overlay_closed_at = tip_closed_at = GetTickCount();
            set_visible(tip_root, 0, 0);
        }
        pc_tip_alpha("0");   /* ready unseen for the next one */
        return;
    }
    if (!tip_root) {
        RECT r;
        HWND hw = active_window();
        tip_root = load("media/ui/x360/tipmenu.layout", TIP_PREFIX);
        if (!tip_root) { tip_failed = 1; pc_tip_alpha("1"); return; }   /* PC's, seen and usable then */
        if (!k) k = (hw && GetClientRect(hw, &r) && r.bottom > r.top) ? (float)(r.bottom - r.top) / 768.0f : 1.0f;
        scale_tree(tip_root, k, 0);
        pass_mouse(tip_root);
        set_visible(tip_root, 0, 0);
        prop(tip_root, "AlwaysOnTop", "True");   /* over the 360's inventory, which asks for its tip */
    }
    /* Out of play (the title, the main menu's settings: their confirmations use the tip's
     * layout) PC's window is left as it is, used with the mouse (QA runs 3, 4: the 360's tip over
     * the settings' window left no way out). */
    {
        /* Not a tip: its text is none of PC's tips (the settings' confirmations; the main menu has
         * a hero and a level loaded, so being in play does not tell them apart). */
        const char *x360_text_key_of(const char *prefix, const char *value);
        char t[2048];
        read_prop(win_of(pc_tip_prefix, "Dialog"), "Text", t, sizeof t);
        if (!x360_text_key_of("pctip.", t)) {
            if (tip_shown) { tip_shown = 0; set_visible(tip_root, 0, 0); overlay_closed_at = GetTickCount(); }
            pc_tip_alpha("1");
            return;
        }
    }
    if (!tip_shown) {
        RECT r;
        HWND hw = active_window();
        if (hw && GetClientRect(hw, &r) && r.bottom > r.top)   /* the 360's spacing, as the dialog's */
            match_spacing((float)(r.right - r.left), (float)(r.bottom - r.top));
        tip_shown = 1;
        set_visible(tip_root, 0, 1);
        pc_tip_alpha("0");
        move_to_front(tip_root, 0);
        prev = x360_pad_buttons();      /* a button held from before does not act */
    }
    move_to_front(tip_root, 0);   /* each frame: the inventory, shown in the same frame, comes after */
    read_prop(win_of(pc_tip_prefix, "Title"), "Text", v, sizeof v);
    text(win_of(TIP_PREFIX, "Title"), v);
    read_prop(win_of(pc_tip_prefix, "Dialog"), "Text", v, sizeof v);
    {
        /* The 360's text of the same tip (its GLOBALS' CONTEXTTIPS, written for the pad: the
         * converter's --pc-pak), found by PC's text; PC's when there is none (TIP_HOWTOATTACK). */
        const char *x360_text_key_of(const char *prefix, const char *value);
        const char *id = x360_text_key_of("pctip.", v);
        char key[48];
        static const char *logged = (const char *)1;
        if (id) {
            copy(key, sizeof key, "tip.", id);
            if (x360_text(key)[0]) copy(v, sizeof v, x360_text(key), 0);
        }
        if (id != logged) {
            logged = id;
            x360_log("x360 tip: "); x360_log(id ? id : "PC's text not among PC's tips"); x360_log("\r\n");
        }
        /* The tips the 360 never shows: its only tip call, sub_82337A10(client, id), has 16 callers
         * passing the ids 0-4, 6-9, 11, 16, 19, 20 and 23 (and one from data, sub_8223F5C0); the
         * ids of the merchant, the sockets, the gambler, the enchanter, the transmuter, the pet's
         * inventory, the stash, fishing, the shared stash and how to attack are passed by none (the
         * recomp agent's reading; the names by the 360's table from TIP_INVENTORY at 0x820AC130;
         * the owner saw none at the stash, the enchanter and the transmuter on the 360). PC's are
         * closed at once, never shown. */
        if (id) {
            static const char *logged_close;
            static const char *const never[] = {"TIP_MERCHANT", "TIP_SOCKETABLE", "TIP_GAMBLE", "TIP_ENCHANT", "TIP_TRANSMUTE",
                                                "TIP_PETINVENTORY", "TIP_STASH", "TIP_FISHING", "TIP_SHAREDSTASH",
                                                "TIP_HOWTOATTACK", 0};
            int t;
            /* The inventory's tip is the 360's for its own inventory only (its caller 8236F700, an
             * inventory menu): PC opens its inventory with a shop's menu and the tip with it (the
             * owner, 2026-10-25: at a gem goblin; none there on the 360). */
            int at_a_shop = 0;
            if (ui) {
                typedef unsigned char (__fastcall *is_open_fn)(void *self, void *edx);
                static const unsigned menus[] = {0x2d8, 0x2dc, 0x2e0, 0x2e4};
                int q;
                for (q = 0; q < 4; q++) {
                    unsigned char *m = *(unsigned char **)(ui + menus[q]);
                    if (m && ((is_open_fn)(*(void ***)m)[MENU_IS_OPEN])(m, 0)) at_a_shop = 1;
                }
            }
            for (t = at_a_shop ? -1 : 0; t < 0 || never[t]; t++) {
                const char *n = t < 0 ? "TIP_INVENTORY" : never[t];
                unsigned c = 0;
                while (n[c] && id[c] == n[c]) c++;
                if (n[c] || id[c]) continue;
                set_visible(tip_root, 0, 0);
                tip_shown = 0;   /* the next tip PC queues behind it is shown as new (the review) */
                press_tip("Ok");
                if (logged_close != id) { logged_close = id; x360_log("x360 tip: closed, the 360 never shows it\r\n"); }
                return;
            }
        }
    }
    text(win_of(TIP_PREFIX, "Dialog"), v);
    buttons = x360_pad_buttons();
    pressed = buttons & ~prev;
    prev = buttons;
    if (pressed & 0x8000) {                                              /* Y: tips off */
        read_prop(win_of(pc_tip_prefix, "ShowTips"), "Selected", v, sizeof v);
        if (v[0] == 'T' || v[0] == 't') press_tip("ShowTips");
        press_tip("Ok");
    } else if (pressed & 0x2000) press_tip("Ok");                        /* B */
}

/* ---- PC's modal (the 360's CModalMenu, modalmenu.layout) ----
 * PC's modalmenu.layout and the 360's have the same names (Title, Dialog, Ok, Accept, Decline);
 * PC shows it at a hardcore death and for the fishing's result ("Fishing", fishing-catch.png).
 * The 360's (loaded by sub_823532D0, its pad sub_82353758 on GamepadButtonDown): A (5) sends the menu's command 9 when its window +272 is visible, else 10;
 * B (6) sends 8 (the menu's virtual +36); PC's CModalMenu keeps Ok, Accept, Decline (+0x6c..+0x74).
 * Here, over PC's (made transparent, as the tip): A presses PC's Accept when shown, else its Ok;
 * B its Decline when shown (the 360 sends 8 also when it is not; what that does with Ok alone
 * is not read). The buttons shown as PC's. */
void x360_dump_after(const char *name, unsigned ms);
#define MODAL_PREFIX "x360modal_"
static void *modal_root;
static char pc_modal_prefix[48];
static int pc_modal_menu_open(void) {
    unsigned char *m = ui ? *(unsigned char **)(ui + 0x304) : 0;
    return m && *(unsigned *)m == 0x00ab7d04 && m[0x18];
}
/* PC's modal frame drawn its parchment and chains though made transparent (QA run 5): it is moved
 * off the screen while the 360's shows, and put back for the moment its button is pressed (a click
 * at its centre) and when it closes. */
static void modal_frame_away(int away) {
    static char kept[128];
    static int is_away;
    void *f = win_of(pc_modal_prefix, "Frame");
    if (!f || away == is_away) return;
    if (away) {
        float v[8];
        char moved[128];
        read_prop(f, "UnifiedAreaRect", kept, sizeof kept);
        if (!parse_rect(kept, v)) return;
        v[1] += 10000.0f; v[5] += 10000.0f;
        write_rect(moved, v);
        prop(f, "UnifiedAreaRect", moved);
    } else prop(f, "UnifiedAreaRect", kept);
    fresh_rects(f);   /* the buttons' cached pixel rects follow (press_pc reads them) */
    is_away = away;
}
static int modal_failed;

/* After an enchanting or a gem goblin's act: PC's modal taken when it shows (its text into the
 * page, its Ok pressed) and PC's enchant menu opened again with its type (CEnchantMenu 0x575990,
 * thiscall (open, type); its setOpen 0x576540 passes 0x15), so the 360's page stays as it was. */
/* PC's modal texts break their lines themselves ("you\n15 Gold\nfrom the sale of items."); the
 * 360's flow in the window (in-game/16): a single line break made a space, a blank line kept. */
static void flow_lines(char *t) {
    char *c;
    for (c = t; *c; c++) {
        if (*c == '\r') *c = ' ';
        else if (*c == '\n') {
            if (c[1] == '\n' || (c > t && c[-1] == '\n')) continue;
            *c = ' ';
        }
    }
}

int x360_modal_take(char *out, unsigned size);
static unsigned modal_skip_until;          /* PC's modal pressed by enchant_result_tick: not shown as the 360's meanwhile */
static void enchant_result_tick(void) {
    typedef void (__fastcall *open_type_fn)(void *self, void *edx, int open, int type);
    unsigned char *en = ui ? *(unsigned char **)(ui + GOG_ENCHANT_MENU) : 0;
    if (!ench_pending) return;
    if (!at_enchant || !en) { ench_pending = 0; return; }
    /* PC's modal comes when its 3D sign has dropped (QA run 6: over 2 s after the act; QA run 20:
     * over 6 s). Past the wait it shows as the 360's modal instead. */
    if (GetTickCount() - ench_pending > 20000) {
        ench_pending = 0;
        x360_log("x360 enchanter: no modal from PC in 20 s\r\n");
        return;
    }
    /* Taken by the modal hook during the act (at once), else from PC's modal when it shows. */
    if (ench_result[0]) x360_log("x360 enchanter: the result shown in the page\r\n");
    else if (!x360_modal_take(ench_result, sizeof ench_result)) return;
    else { modal_skip_until = GetTickCount() + 500; x360_log("x360 enchanter: the result taken from PC's modal\r\n"); }
    flow_lines(ench_result);
    ench_pending = 0;
    if (!en[ENCHANT_OPEN]) {
        ((open_type_fn)0x00575990)(en, 0, 1, at_enchant);
        x360_log(en[ENCHANT_OPEN] ? "x360 enchanter: PC's enchant menu opened again\r\n"
                                  : "x360 enchanter: PC's enchant menu did not open again\r\n");
    }
    pc_inventory_close();
    if (inventory_open) refresh_all();
}

static void modal_tick(void) {
    static unsigned last_search, prev;
    static void *pc_modal;
    unsigned now = GetTickCount(), buttons, pressed;
    char v[2048];
    if (modal_failed) return;
    /* PC's modal: of PC's layouts with an Accept (combine, dialog, enchant, quest dialog), the
     * only one with Ok and Dialog too. */
    if (!pc_modal && now - last_search > 250) {
        static const char *const needs[] = {"Ok", "Dialog", 0};
        last_search = now;
        pc_modal = find_pc_by_suffix("Accept", needs, pc_modal_prefix, sizeof pc_modal_prefix);
    }
    if (!pc_modal) return;
    if (ench_pending || (modal_skip_until && (int)(modal_skip_until - now) > 0)) return;   /* an enchanting's result: its page takes it (0: none asked; a 0 compared would read as the future after 24.8 days of uptime) */
    /* PC's modal only while CGameUI's CModalMenu (+0x304, vtable 0xab7d04, a CMenu: its open
     * byte +0x18) is open: the settings' screen has windows of the same names (QA run 5: the
     * bars over the settings). */
    if (!pc_attached(pc_modal_prefix, "Frame") || !pc_modal_menu_open()) {
        if (modal_shown) {
            modal_shown = 0;
            overlay_closed_at = GetTickCount();
            set_visible(modal_root, 0, 0);
            modal_frame_away(0);
            prop(win_of(pc_modal_prefix, "Frame"), "Alpha", "1");
        }
        return;
    }
    if (!modal_root) {
        RECT r;
        HWND hw = active_window();
        modal_root = load("media/ui/x360/modalmenu.layout", MODAL_PREFIX);
        if (!modal_root) { modal_failed = 1; return; }
        if (!k) k = (hw && GetClientRect(hw, &r) && r.bottom > r.top) ? (float)(r.bottom - r.top) / 768.0f : 1.0f;
        scale_tree(modal_root, k, 0);
        pass_mouse(modal_root);
        set_visible(modal_root, 0, 0);
    }
    if (!modal_shown) {
        RECT r;
        HWND hw = active_window();
        if (hw && GetClientRect(hw, &r) && r.bottom > r.top)
            match_spacing((float)(r.right - r.left), (float)(r.bottom - r.top));
        modal_shown = 1;
        set_visible(modal_root, 0, 1);
        prop(win_of(pc_modal_prefix, "Frame"), "Alpha", "0");
        modal_frame_away(1);
        move_to_front(modal_root, 0);
        prev = x360_pad_buttons();
        {
            /* A test aid: a UI dump 0.5 s after it shows (modal-1, ...): run B2 showed a
             * parchment with chains behind the 360's modal, which the 360 has not (in-game/16);
             * PC's modalmenu.layout is transparent whole, so the dump shows what draws it. */
            static int modals;
            char name[16] = "modal-";
            modals++;
            name[6] = (char)('0' + modals % 10);
            name[7] = 0;
            x360_dump_after(name, 500);
        }
    }
    read_prop(win_of(pc_modal_prefix, "Title"), "Text", v, sizeof v);
    text(win_of(MODAL_PREFIX, "Title"), v);
    read_prop(win_of(pc_modal_prefix, "Dialog"), "Text", v, sizeof v);
    flow_lines(v);
    text(win_of(MODAL_PREFIX, "Dialog"), v);
    show(win_of(MODAL_PREFIX, "Ok"), pc_shown_in(pc_modal_prefix, "Ok"));
    show(win_of(MODAL_PREFIX, "Accept"), pc_shown_in(pc_modal_prefix, "Accept"));
    show(win_of(MODAL_PREFIX, "Decline"), pc_shown_in(pc_modal_prefix, "Decline"));
    buttons = x360_pad_buttons();
    pressed = buttons & ~prev;
    prev = buttons;
    if (x360_tip_open()) buttons = pressed = 0;   /* a tip shown over it has the pad (drawn in front: the owner's run) */
    if (pressed & 0x1000) { modal_frame_away(0); press_pc(pc_modal_prefix, pc_shown_in(pc_modal_prefix, "Accept") ? "Accept" : "Ok"); }   /* A */
    if ((pressed & 0x2000) && pc_shown_in(pc_modal_prefix, "Decline")) { modal_frame_away(0); press_pc(pc_modal_prefix, "Decline"); }   /* B */
}

/* ---- the bars at the screen's top and bottom (the 360's CInteractiveMenu, interactivemenu.layout:
 * TopBar and BottomBar, 10 % each, set:Loading image:background) ----
 * The 360 counts its open modal, NPC dialog, death, settings and leaderboards menus (each one's
 * setOpen, the virtual +0x1c, adds or takes one at the UI's +5612: CModalMenu sub_82353528,
 * CQuestDialogMenu sub_82355668, ...) and sub_82343DF8 shows the bars when the count is 1 and hides
 * them at 0, hiding its HUD (the UI's +60) while they show (town/04, in-game/16). Of those the port
 * shows the modal and the NPC dialog. */
#define BARS_PREFIX "x360bars_"
static void bars_place(float y_top, float y_bottom) {
    /* The bars' rects in screen fractions (the layout's 10 % each, no offsets), only their y moved. */
    char rect[128];
    float vr[8] = {0, 0, 0, 0, 1, 0, 0, 0};
    vr[2] = y_top; vr[6] = y_top + 0.1f;
    write_rect(rect, vr);
    prop(win_of(BARS_PREFIX, "TopBar"), "UnifiedAreaRect", rect);
    vr[2] = y_bottom; vr[6] = y_bottom + 0.1f;
    write_rect(rect, vr);
    prop(win_of(BARS_PREFIX, "BottomBar"), "UnifiedAreaRect", rect);
}
static void bars_tick(void) {
    static void *root;
    static int failed, on;
    static unsigned last;
    static float t;                              /* the slide's time left, s (the 360's +32) */
    unsigned now = GetTickCount();
    float dt = last ? (float)(now - last) / 1000.0f : 0.0f, q;
    int want = modal_shown || qd_shown;
    last = now;
    if (failed) return;
    if (want != on) {
        if (!root) {
            RECT r;
            HWND hw = active_window();
            root = load("media/ui/x360/interactivemenu.layout", BARS_PREFIX);
            if (!root) { failed = 1; return; }
            if (!k) k = (hw && GetClientRect(hw, &r) && r.bottom > r.top) ? (float)(r.bottom - r.top) / 768.0f : 1.0f;
            scale_tree(root, k, 0);
            pass_mouse(root);
        }
        on = want;
        t = 0.2f;   /* the 360's setOpen 0x82353068: 0.2 s either way (closing: the bar's height 0.1 times 2) */
        if (on) { set_visible(root, 0, 1); move_to_front(root, 0); }
        if (hud_root) set_visible(hud_root, 0, !on);
        if (pet_hud_root) set_visible(pet_hud_root, 0, !on);
        x360_log(on ? "x360 bars: shown\r\n" : "x360 bars: hidden\r\n");
    }
    if (!root || t <= 0.0f) return;
    /* The 360's tick 0x821FFD50: q = t * 5, from 1 to 0; opening, TopBar's y -0.1 q and BottomBar's
     * 0.9 + 0.1 q (sliding in from beyond the screen's edges); closing, the reverse, the menu
     * hidden at the end. */
    t -= dt;
    q = t * 5.0f;
    if (q < 0.0f) q = 0.0f;
    if (on) bars_place(-0.1f * q, 0.9f + 0.1f * q);
    else bars_place(-0.1f * (1.0f - q), 0.9f + 0.1f * (1.0f - q));
    if (!on && q <= 0.0f) set_visible(root, 0, 0);
}

/* PC's modal taken without showing it (the gem goblins' result, written in their page as the
 * 360 does): its Dialog's text, then its Ok pressed. */
int x360_modal_take(char *out, unsigned size) {
    static const char *const needs[] = {"Ok", "Dialog", 0};
    if (!pc_modal_prefix[0]) find_pc_by_suffix("Accept", needs, pc_modal_prefix, sizeof pc_modal_prefix);
    if (!pc_modal_prefix[0] || !pc_attached(pc_modal_prefix, "Frame")) return 0;
    read_prop(win_of(pc_modal_prefix, "Dialog"), "Text", out, size);
    modal_frame_away(0);
    press_pc(pc_modal_prefix, pc_shown_in(pc_modal_prefix, "Accept") ? "Accept" : "Ok");
    return 1;
}

/* Whether a tip, modal or NPC dialog of the 360's has the pad (or closed less than 0.3 s ago, so
 * the press that closed it does not reach the world too): the 360's handlers keep the buttons they
 * get (sub_823589A0 and sub_82353758 return 1), so proxy/pad.c does not act in the world then. */
int x360_overlay_open(void) {
    /* The fishing too (QA run 8: B that cancelled it also fired the skill mapped to B). */
    return tip_shown || modal_shown || qd_shown || fish_shown || GetTickCount() - overlay_closed_at < 300;
}

/* A test aid: a UI dump `ms` after a call (proxy/pad.c asks for one after A talks to a unit, to see
 * which of PC's menus opened); taken from the frame tick, which runs under PC's dialogs too. */
static char dump_after_name[32];
static unsigned dump_after_at;
void x360_dump_after(const char *name, unsigned ms) {
    copy(dump_after_name, sizeof dump_after_name, name, 0);
    dump_after_at = GetTickCount() + ms;
}

/* PC's own screens kept shut (the user: neither by their hotkeys, I, C, S, J, Q, P, which also took
 * the 360's doll away): CGameUI's CInventoryMenu +0x2cc, CStatsMenu +0x2d0, CPetMenu +0x2d4,
 * CSkillMenu +0x30c, CJournalMenu +0x310, CQuestMenu +0x314 (made at GOG 0x562d96-0x563445, named
 * by their RTTI), closed by their setOpen as they open (a shop's own are the shop's code's); and
 * PC's interface hidden every frame while the 360's frame is open. */
static void pc_menus_guard(void) {
    typedef unsigned char (__fastcall *is_open_fn)(void *self, void *edx);
    static const unsigned at[] = {GOG_INVENTORY_MENU, 0x2d0, GOG_PET_MENU, 0x30c, 0x310, 0x314};
    unsigned i;
    int closed = 0;
    if (!ui) return;
    if (inventory_open || pc_ui_back_at) hide_pc_ui(1);   /* during the delay too: what PC shows again stays hidden */
    if (!inventory_open && pc_ui_back_at && (int)(GetTickCount() - pc_ui_back_at) >= 0) { pc_ui_back_at = 0; hide_pc_ui(0); }
    if (shop) return;
    /* A shop's menu opens PC's inventory with it (the merchant, enchanter, transmuter, stash): left
     * to the shop's code (QA run 1: closing it closed the shop before the port saw it). */
    {
        static const unsigned shops[] = {0x2d8, 0x2dc, 0x2e0, 0x2e4};
        for (i = 0; i < sizeof shops / sizeof shops[0]; i++) {
            unsigned char *m = *(unsigned char **)(ui + shops[i]);
            if (m && ((is_open_fn)(*(void ***)m)[MENU_IS_OPEN])(m, 0)) return;
        }
    }
    for (i = 0; i < sizeof at / sizeof at[0]; i++) {
        unsigned char *m = *(unsigned char **)(ui + at[i]);
        if (!m || !((is_open_fn)(*(void ***)m)[MENU_IS_OPEN])(m, 0)) continue;
        ((set_bool_fn)(*(void ***)m)[MENU_SET_OPEN])(m, 0, 0);
        x360_log("x360 menu: PC's own screen at CGameUI + "); x360_log_num((long)at[i]); x360_log(" closed as it opened\r\n");
        closed = 1;
    }
    /* PC's closing takes its doll's viewport (the 360's doll uses the same camera and target): made again. */
    if (closed && inventory_open && doll_shown >= 0) { doll_shown = -1; show_doll(page < 2); }
}

/* R3: the 360 cycles its map with RS, the corner minimap, the overlay over the play area, none, and
 * round (in-game/31-33; GUITOGGLEAUTOMAP). PC's KEYMAP_AUTOMAP does the same three (CGameUI's update
 * at 0x55d380: unless CGameUI's +0x173d, 0x4ee7c0 on its +0x20, whose automap +0x160, by its +0x3c
 * shown and +0x65 overlay, goes corner -> overlay (0x4ea910 with 1) -> off (0x4ea6b0 with 0) ->
 * corner, keeping the state in the setting [0xf60c94]): called for R3. */
void x360_automap_cycle(void) {
    typedef void (__fastcall *self_fn)(void *self, void *edx);
    void *holder;
    if (!find_world() || !ui) return;
    holder = *(void **)(ui + 0x20);
    if (!holder || ui[0x173d]) return;
    __try { ((self_fn)0x004ee7c0)(holder, 0); x360_log("x360: R3, the map's next state\r\n"); }
    __except (1) { x360_log("x360: R3, the map's toggle threw\r\n"); }
    minimap_tick();   /* placed at once: PC's toggle puts it top right, drawn there for a frame (the owner) */
}

/* The 360's pause menu's Options and Exit to Title, done by PC's own pause menu: opened by its
 * setOpen (the virtual +0x1c) in one tick, its button pressed once it shows (a left click at its
 * centre, as PC's other buttons), within 2 s. */
int x360_pc_menu_request;
static char pc_options_prefix[64];
static void pc_menu_tick(void) {
    static unsigned since;
    static int opened;
    unsigned char *m;
    if (!x360_pc_menu_request || !ui) { since = 0; opened = 0; return; }
    if (!since) since = GetTickCount();
    if (GetTickCount() - since > 2000) {
        x360_log("x360 menu: PC's pause menu did not show its button in 2 s\r\n");
        x360_pc_menu_request = 0; since = 0; opened = 0;
        return;
    }
    m = *(unsigned char **)(ui + 0x2e8);
    if (!m) { x360_pc_menu_request = 0; return; }
    if (*(unsigned *)m != 0x00ab7d78) { x360_pc_menu_request = 0; return; }
    if (!opened) {
        ((void (__fastcall *)(void *, void *, int))0x00591200)(m, 0, 1);   /* COptionsMenu's setOpen, slot 6 */
        opened = 1;
        x360_log("x360 menu: PC's pause menu opened\r\n");
        return;
    }
    {
        static const char *const needs[] = {"ExitGame", "Settings", 0};
        const char *button = x360_pc_menu_request == 1 ? "Settings" : "ExitGame";
        if (!pc_options_prefix[0] || !win_of(pc_options_prefix, "ReturnToGame"))
            find_pc_by_suffix("ReturnToGame", needs, pc_options_prefix, sizeof pc_options_prefix);
        if (!pc_options_prefix[0] || !pc_shown_in(pc_options_prefix, button)) return;
        press_pc(pc_options_prefix, button);
        x360_pc_menu_request = 0; since = 0; opened = 0;
    }
}

/* PC's hotkeys for its own screens, off: CGameUI's update tests each key (its action's binding,
 * 0x5c53d0, pressed, 0x4e71b0) and opens the screen; each test's jump is made to always pass the
 * opening by (the user: the menus showing and closing at once were not a fix). KEYMAP_INVENTORY
 * [0xf60c0c] at 0x55d206 (with its HUD button's flag), KEYMAP_SKILLS [0xf60c00] at 0x55d282,
 * KEYMAP_JOURNAL [0xf60bf8] at 0x55d2a4, KEYMAP_QUESTS [0xf60bfc] at 0x55d2c5, KEYMAP_STATS
 * [0xf60c04] at 0x55d2e7, KEYMAP_PET [0xf60c08] at 0x55d318. Each patched only if its two bytes
 * are as read. */
static void hotkeys_off(void) {
    const struct { unsigned at; unsigned char was[2], now[2]; } p[] = {   /* not static: Steam's build moves the addresses (x360_reloc) */
        {0x0055d206, {0x75, 0x08}, {0xeb, 0x16}},
        {0x0055d282, {0x74, 0x07}, {0xeb, 0x07}},
        {0x0055d2a4, {0x74, 0x07}, {0xeb, 0x07}},
        {0x0055d2c5, {0x74, 0x07}, {0xeb, 0x07}},
        {0x0055d2e7, {0x75, 0x08}, {0xeb, 0x16}},
        {0x0055d318, {0x75, 0x08}, {0xeb, 0x16}},
    };
    static int done;
    unsigned i, n = 0;
    if (done) return;
    done = 1;
    for (i = 0; i < sizeof p / sizeof p[0]; i++) {
        unsigned char *f = (unsigned char *)p[i].at;
        unsigned long old;
        if (f[0] != p[i].was[0] || f[1] != p[i].was[1]) continue;
        if (!VirtualProtect(f, 2, 0x40, &old)) continue;
        f[0] = p[i].now[0]; f[1] = p[i].now[1];
        VirtualProtect(f, 2, old, &old);
        n++;
    }
    x360_log("x360 menu: PC's screen hotkeys off: "); x360_log_num((long)n); x360_log(" of 6\r\n");
}

/* PC's menu keys swallowed in play (the owner, 2026-10-24): with the 360's interface the pad opens
 * the screens (Start, Back, R3), and PC's keys opened or animated PC's own under them (the space
 * bar's close-all showed PC's menus moving behind the 360's). Their key messages never reach the
 * game's window procedure (CGame::WndProc *(OT)* feeds CGameUI's and CKeyManager's keyEvent from
 * WM_KEYDOWN, WM_KEYUP, WM_CHAR, WM_SYSKEYDOWN, WM_SYSKEYUP) while in play and PC's settings are
 * not open (the pad's way to them, where the keyboard keeps working), the console is not shown
 * (typed into) and no cinematic holds (PC's keys skip it). A press's repeats and release go as
 * the press went (no key left down in PC's state across the changes); out of play, or when the
 * frame tick stops saying so, nothing is swallowed. The window is the game's (its title), taken
 * once. The keys: KEYMAP_INVENTORY I,
 * PET P, STATS C, SKILLS S, QUESTS Q, JOURNAL J, OPTIONS Esc, PAUSE Enter, CLOSEALL the space bar,
 * AUTOMAP A (the 360's map is on R3); the six of hotkeys_off are kept off there too. The others
 * (the hotbar's 1 to 0, F1 to F12, Shift, Alt, Tab, W, the skills' cycle, the zooms, the console)
 * work as on PC. One address of the executable, the cinematic's test (ported to Steam's). */
__declspec(dllimport) long __stdcall SetWindowLongW(HWND w, int index, long value);
__declspec(dllimport) long __stdcall SetWindowLongA(HWND w, int index, long value);
__declspec(dllimport) long __stdcall CallWindowProcW(long proc, HWND w, unsigned msg, unsigned wparam, long lparam);
__declspec(dllimport) long __stdcall CallWindowProcA(long proc, HWND w, unsigned msg, unsigned wparam, long lparam);
__declspec(dllimport) int __stdcall IsWindowUnicode(HWND w);
__declspec(dllimport) long __stdcall GetWindowLongW(HWND w, int index);
__declspec(dllimport) long __stdcall GetWindowLongA(HWND w, int index);
__declspec(dllimport) int __stdcall GetWindowTextA(HWND w, char *text, int size);
static long keys_old_proc;       /* the game window's own procedure: subclassed once, never again */
static HWND keys_window;         /* the game's window (keys_off) */
static int keys_unicode;
static int keys_open;            /* set each frame by keys_off: in play, no PC settings, console or cinematic */
static unsigned keys_open_at;    /* when keys_off last said so: a stale state (out of play) opens nothing */
static unsigned keys_down;       /* the menu keys whose press was swallowed: their repeats and release are too */
static int keys_console_key;     /* the console's key went down: look for its window at once */
HWND x360_keys_window(void) { return keys_old_proc ? keys_window : 0; }   /* only once subclassed: what is posted to it reaches keys_proc */
static int key_index(unsigned vk) {
    static const unsigned char keys[] = {'I', 'P', 'C', 'S', 'Q', 'J', 'A', 0x1b, 0x0d, 0x20};
    int i;
    for (i = 0; i < (int)sizeof keys; i++) if (vk == keys[i]) return i;
    return -1;
}
static long __stdcall keys_proc(HWND w, unsigned msg, unsigned wparam, long lparam) {
    if (msg == X360_RETIRE_MSG) { __try { x360_retire_act(); } __except (1) { x360_log("x360 retire: exception in the act\r\n"); } return 0; }
    int open = keys_open && GetTickCount() - keys_open_at < 2000;   /* a long frame (a load's) is not stale */
    int k = (msg == 0x100 || msg == 0x101 || msg == 0x102) ? key_index(wparam) : -1;
    if (msg == 0x0008) keys_down = 0;   /* WM_KILLFOCUS: no release will come for what is held */
    if (msg == 0x100 && wparam == 0xc0) keys_console_key = 1;   /* KEYMAP_CONSOLEKEY_PRESS (`) */
    if (k >= 0 && msg == 0x100) {        /* WM_KEYDOWN: a new press (lParam bit 30 clear) or a repeat */
        if (!(lparam & 0x40000000) || !(keys_down & (1u << k))) {   /* a press, or a repeat after a focus loss */
            if (open) keys_down |= 1u << k; else keys_down &= ~(1u << k);
        }
        if (keys_down & (1u << k)) return 0;   /* the press and its repeats, as the press went */
    }
    if (k >= 0 && msg == 0x101 && (keys_down & (1u << k))) { keys_down &= ~(1u << k); return 0; }   /* WM_KEYUP */
    if (k >= 7 && msg == 0x102 && (keys_down & (1u << k))) return 0;   /* WM_CHAR of a swallowed Esc, Enter, space */
    return keys_unicode ? CallWindowProcW(keys_old_proc, w, msg, wparam, lparam)
                        : CallWindowProcA(keys_old_proc, w, msg, wparam, lparam);
}
static void keys_off(void) {
    static int tried;
    static void *console;
    static char console_prefix[48];
    int console_up = 0, cinematic = 0;
    if (!tried && hud_in_play) {
        /* The game's window, once: the active window in play, its title the game's (600 frames'
         * tries, then given up, said in the log). */
        static int tries;
        HWND w = active_window();
        char title[64];
        if (++tries > 600) { tried = 1; x360_log("x360 menu: PC's menu keys: no window titled Torchlight, not swallowed\r\n"); }
        else if (w && GetWindowTextA(w, title, sizeof title) > 0 && title[0] == 'T' && title[1] == 'o' && title[2] == 'r' &&
                 title[3] == 'c' && title[4] == 'h') {
            long old = 0;
            tried = 1;
            keys_unicode = IsWindowUnicode(w);
            keys_window = w;
            if ((keys_unicode ? GetWindowLongW(w, -4) : GetWindowLongA(w, -4)) != (long)keys_proc)   /* never onto itself */
                old = keys_unicode ? SetWindowLongW(w, -4 /* GWL_WNDPROC */, (long)keys_proc) : SetWindowLongA(w, -4, (long)keys_proc);
            keys_old_proc = old;
            x360_log(old ? "x360 menu: PC's menu keys swallowed in play, window " : "x360 menu: PC's menu keys: the window not subclassed, ");
            x360_log_num((long)w);
            x360_log(keys_unicode ? " (Unicode)\r\n" : " (ANSI)\r\n");
        }
    }
    if (!keys_old_proc) return;
    /* The console (Runic's developer console, a CEGUI window): typed into with the keyboard. */
    {
        static unsigned last_search;
        unsigned now = GetTickCount();
        if (console && !win_of(console_prefix, "WindowsLook/Console")) console = 0;
        if (!console && (now - last_search >= 1000 || keys_console_key)) {   /* the whole tree: once a second, or at the console's key */
            last_search = now;
            keys_console_key = 0;
            console = find_pc_by_suffix("WindowsLook/Console", 0, console_prefix, sizeof console_prefix);
        }
    }
    if (console) {
        typedef unsigned char (__fastcall *vis_fn)(const void *self, void *edx, unsigned char local);
        static vis_fn vis;
        if (!vis) vis = (vis_fn)need("?isVisible@Window@CEGUI@@QBE_N_N@Z");
        console_up = vis && vis(console, 0, 0);
    }
    /* A cinematic (CGameUI's test, 0x541db0; proxy/pad.c): PC's own keys skip it. */
    if (ui) cinematic = ((unsigned char (__fastcall *)(void *, void *))0x00541db0)(ui, 0) != 0;
    keys_open = hud_in_play && !x360_pc_settings_up && !console_up && !cinematic;
    keys_open_at = GetTickCount();
}

/* PC's menus' 3D boards (QA runs 5, 6: the parchment with chains behind the 360's modal, the
 * boards at the shops' sides; neither a CEGUI window, they stayed with PC's interface hidden and
 * its modal frame moved away). A CMenu keeps its board at +0x38 (the modal, CGameUI's +0x304;
 * CMenu's update 0x574477 animates the entity at the board's +0x5c with Entity::_updateAnimation),
 * a shop's screen elsewhere (boards_tick's table, each read in its update). The entity is made invisible
 * (Ogre's MovableObject::setVisible, exported): the modal's and the shops' for good (the 360's
 * screens show every one of them; the owner's run of 2026-10-24: made visible again as the shop
 * closed, PC's boards were seen after a merchant). */
static void board_visible(unsigned char *menu, unsigned at, int visible) {
    /* QA run 8: the entity alone made invisible left the boards seen. Now its scene node, with all
     * it carries (SceneNode::setVisible with cascade), never the scene's root (a node with no
     * parent). What is found is logged once per menu field. */
    typedef void (__fastcall *set_visible_fn)(void *self, void *edx, int visible);
    typedef void (__fastcall *node_visible_fn)(void *self, void *edx, int visible, int cascade);
    typedef void *(__fastcall *get_ptr_fn)(const void *self, void *edx);
    static set_visible_fn set_vis;
    static node_visible_fn node_vis;
    static get_ptr_fn parent_scene_node, node_parent;
    static void *entity_vtable;
    static unsigned logged[16];
    static int nlogged;
    unsigned char *board;
    void *entity, *node = 0, *up = 0;
    int i, seen = 0;
    if (!set_vis) {
        void *ogre = GetModuleHandleA("OgreMain.dll");
        set_vis = ogre ? (set_visible_fn)GetProcAddress(ogre, "?setVisible@MovableObject@Ogre@@UAEX_N@Z") : 0;
        node_vis = ogre ? (node_visible_fn)GetProcAddress(ogre, "?setVisible@SceneNode@Ogre@@UAEX_N0@Z") : 0;
        parent_scene_node = ogre ? (get_ptr_fn)GetProcAddress(ogre, "?getParentSceneNode@MovableObject@Ogre@@UBEPAVSceneNode@2@XZ") : 0;
        node_parent = ogre ? (get_ptr_fn)GetProcAddress(ogre, "?getParent@Node@Ogre@@UBEPAV12@XZ") : 0;
        entity_vtable = ogre ? GetProcAddress(ogre, "??_7Entity@Ogre@@6BShadowCaster@1@@")   /* the vtable at offset 0: MovableObject derives from ShadowCaster first (QA run 9: the board was refused under 6B@) */ : 0;
    }
    if (!set_vis || !entity_vtable || !menu) return;
    __try {
        board = *(unsigned char **)(menu + at);
        entity = board ? *(void **)(board + 0x5c) : 0;
        for (i = 0; i < nlogged; i++) if (logged[i] == (unsigned)(unsigned long)menu + at) seen = 1;
        if (!seen && nlogged < 16) {
            logged[nlogged++] = (unsigned)(unsigned long)menu + at;
            x360_log("x360 boards: menu vtable "); x360_log_num((long)*(unsigned *)menu);
            x360_log(" board "); x360_log_num((long)board);
            x360_log(" entity "); x360_log_num((long)entity);
            x360_log(entity && *(void **)entity == entity_vtable ? " an Entity" : " not an Entity's vtable");
            if (entity) { x360_log(" vtable "); x360_log_num((long)*(unsigned *)entity); }
            x360_log("\r\n");
        }
        if (!entity || *(void **)entity != entity_vtable) return;
        set_vis(entity, 0, visible);
        if (parent_scene_node && node_vis && node_parent) {
            node = parent_scene_node(entity, 0);
            up = node ? node_parent(node, 0) : 0;
            if (node && up) node_vis(node, 0, visible, 1);
        }
    } __except (1) { }
}
/* The minimap under the 360's modal: the 360 hides it there (in-game/16; its shops keep it, town/07).
 * PC's automap (CGameUI's +0x20, its +0x160; shown at its +0x3c) hidden by PC's own call
 * (0x4ea6b0 with 0, as KEYMAP_AUTOMAP's 0x4ee7c0 does) while the modal shows, if it was shown, and
 * shown again after. */
static void modal_minimap(int hide) {
    typedef void (__fastcall *show_fn)(void *self, void *edx, int on);
    static unsigned char *hidden;
    unsigned char *holder, *am;
    if (!ui) return;
    holder = *(unsigned char **)(ui + 0x20);
    am = holder ? *(unsigned char **)(holder + 0x160) : 0;
    if (hide) {
        if (hidden || !am || !am[0x3c]) return;
        __try { ((show_fn)0x004ea6b0)(am, 0, 0); hidden = am; } __except (1) { }
    } else if (hidden) {
        __try { if (am == hidden) ((show_fn)0x004ea6b0)(am, 0, 1); } __except (1) { }
        hidden = 0;
        minimap_tick();   /* shown again top right by PC: placed at once */
    }
}

static void boards_tick(void) {
    /* Each shop screen's board, where its update (its vtable's slot 9) animates it: CInventoryMenu
     * 0x580f80 its +0x7b20, CMerchantMenu 0x58ac50 +0x48, CEnchantMenu 0x5752d0 +0x50,
     * CCombineMenu 0x5656a0 +0x6c, CStashMenu 0x5aadb0 +0x48; the menu's vtable checked first. */
    const struct { unsigned at, vtable, board; } shops[] = {   /* not static: Steam's vtables move with the executable */
        {0x2cc, 0x00ab73e0, 0x7b20}, {0x2d8, 0x00ab7b58, 0x48}, {0x2dc, 0x00ab6c0c, 0x50},
        {0x2e0, 0x00ab2c4c, 0x6c}, {0x2e4, 0x00ab8730, 0x48},
    };
    static int modal_hidden;
    static unsigned char *shop_logged[sizeof shops / sizeof shops[0]];
    unsigned i;
    int want_modal = modal_shown;   /* the minimap's */
    if (!ui) return;
    modal_minimap(want_modal);
    {
        /* PC's modal's board kept hidden always (the 360's modal shows every one of PC's modals):
         * hidden only while the 360's showed, its fall before it and its rise after it were seen
         * (the owner, the pet's return). */
        unsigned char *m = *(unsigned char **)(ui + 0x304);
        if (m && *(unsigned *)m == 0x00ab7d04) {
            board_visible(m, 0x38, 0);
            if (!modal_hidden) x360_log("x360 modal: PC's board hidden for good\r\n");
            modal_hidden = 1;
        }
    }
    /* The shops' boards kept hidden always, as the modal's: the 360's shop frame shows every one
     * of PC's shop menus, and shown again as it closed they stayed seen after it (the owner's run
     * of 2026-10-24, a merchant). Each menu's logged once. CInventoryMenu's too (the owner's
     * decision, 2026-10-24: shown again after a shop, it was seen sliding away), though the 360's
     * inventory draws its doll through that menu's scene (show_doll_of): QA checks the doll. */
    for (i = 0; i < sizeof shops / sizeof shops[0]; i++) {
        unsigned char *m = *(unsigned char **)(ui + shops[i].at);
        if (!m || *(unsigned *)m != shops[i].vtable) continue;
        board_visible(m, shops[i].board, 0);
        if (shop_logged[i] != m) { shop_logged[i] = m; x360_log("x360 shop: a shop's board hidden for good\r\n"); }
    }
}

/* PC's own bars (its CInteractiveMenu, CGameUI's +0x318, interactivemenu.layout: Root, TopBar,
 * BottomBar) shown with its settings (QA run 5): made transparent; the port shows the 360's where
 * the 360 does. */
static void pc_bars_off(void) {
    /* By CGameUI's CInteractiveMenu itself (+0x318, vtable 0xab731c; its root window at +0xc,
     * checked by its name), not by the windows' names: PC's main menu frame has windows of the
     * same names (QA run 7: the main menu made transparent). */
    typedef const void *(__fastcall *name_fn)(const void *self, void *edx);
    static name_fn gn;
    unsigned char *m;
    void *root;
    const char *nm;
    unsigned n;
    if (!ui || !find_world()) return;
    m = *(unsigned char **)(ui + 0x318);
    if (!m || *(unsigned *)m != 0x00ab731c) return;
    root = *(void **)(m + 0xc);
    if (!gn) gn = (name_fn)need("?getName@Window@CEGUI@@QBEABVString@2@XZ");
    if (!root || !gn) return;
    nm = c_str(gn(root, 0), 0);
    n = nm ? length(nm) : 0;
    if (n < 4 || nm[n - 4] != 'R' || nm[n - 3] != 'o' || nm[n - 2] != 'o' || nm[n - 1] != 't') return;
    {
        char a[16];
        read_prop(root, "Alpha", a, sizeof a);   /* written only when not already 0: a write redraws (QA run 11's slowness) */
        if (!(a[0] == '0' && a[1] != '.')) prop(root, "Alpha", "0");
    }
}

/* PC narrows its world's viewport while one of its side panels is open (a shop's: CGameUI's
 * 0x541090 asks each of its panels, the vector at +0x16b8, their virtuals +8 and +0xc; the client's
 * update at 0x40276c then sets the viewport inside them, and the mouse's and labels' projections,
 * 0x411b8f, 0x541886, use the same test). The 360's frame covers the screen with the world seen
 * whole around it, and the strips PC leaves out keep the last picture of PC's panels (QA runs 1, 2:
 * the merchant's sides). While the 360's frame is open at a shop, 0x541090 answers no (its first
 * bytes made "xor al, al; ret"; put back when it closes). */
/* bag=360 in x360_ui.ini (an install option, the user's): the hero's and the pet's bag as the
 * 360's. The 360 gives a unit with a bag one section of 50 slots for every item (sub_822883F0:
 * sub_822E0308 with 50, but 0 for type 170). PC's unit ctor 0x49f700 gives the kind 0x57 (the pet)
 * three of 21 (0x49f85f: CInventory 0x4e9820 with 21, its first section from slot 19; 0x49f880,
 * 0x49f88f: addSection (OT) 0x4e9a00 for the types 1 and 2 with 21 each), the kinds 0x29 and 0x80
 * 42, 0xaa none, any other one of 21 (0x49f8ee); the hero's ctor 0x4dbc40 runs it, then adds the
 * types 1 and 2 with 21 each itself (0x4dbc8d, 0x4dbc9c). Patched at load, before any unit: 50 for
 * the two first sections of 21, the four adds left out (their 8 bytes of arguments taken off the
 * stack). An item
 * of no section's type goes to the first (0x4e8410), as on the 360. Saves keep slot numbers: items
 * past the 50 are lost with it on, and placed oddly with it off again (the user's choice). */
#define GOG_ADD_SECTION 0x004e9a00   /* addSection (OT), the two calls' target */
static int calls_add_section(const unsigned char *c) {
    return c[0] == 0xe8 && (unsigned)(unsigned long)(c + 5) + *(const unsigned *)(c + 1) == GOG_ADD_SECTION;
}
void x360_bag_patch(void) {
    static const unsigned char skip[5] = {0x83, 0xc4, 0x08, 0x90, 0x90};   /* add esp,8 */
    /* The pushes of 21 made 50: the pet's branch (kind 0x57) and the other units' (the hero's,
     * whose ctor 0x4dbc40 runs the base 0x49f700 then adds its own two sections: QA run 25 found
     * the hero still at 82 slots, 19 40 61, with the pet at 50). The four addSection calls left
     * out: the pet's two and the hero's two. */
    unsigned char *size[2] = {(unsigned char *)0x0049f85f, (unsigned char *)0x0049f8ee};
    unsigned char *calls[4] = {(unsigned char *)0x0049f880, (unsigned char *)0x0049f88f,
                               (unsigned char *)0x004dbc8d, (unsigned char *)0x004dbc9c};
    unsigned long old;
    unsigned i, k;
    for (i = 0; i < 2; i++)
        if (size[i][0] != 0x6a || size[i][1] != 0x15) { x360_log("x360 bag: PC's code not as expected, PC's bag kept\r\n"); return; }
    for (i = 0; i < 4; i++)
        if (!calls_add_section(calls[i]) || (i >= 2 && (calls[i][-2] != 0x6a || calls[i][-1] != i - 1))) {   /* the hero's: push 1, push 2 right before */
            x360_log("x360 bag: PC's code not as expected, PC's bag kept\r\n");
            return;
        }
    for (i = 0; i < 2; i++) {
        if (!VirtualProtect(size[i], 2, 0x40, &old)) return;
        size[i][1] = 50;
        VirtualProtect(size[i], 2, old, &old);
    }
    for (i = 0; i < 4; i++) {
        if (!VirtualProtect(calls[i], 5, 0x40, &old)) return;
        for (k = 0; k < 5; k++) calls[i][k] = skip[k];
        VirtualProtect(calls[i], 5, old, &old);
    }
    x360_log("x360 bag: the 360's bag (one section of 50), the hero's and the pet's\r\n");
}

/* ---- the proxy's own black screen (the owner: a Windows box showed blank under Proton, and on the
 * Deck's gamescope neither it nor Notepad may show) ----
 * A full-screen StaticImage of the converted UI's black image (set:X360_widgets2 image:black, the
 * layouts' own), always on top, a text over it and a line with what A does. While it shows the pad
 * reaches nothing else (pad.c's poll asks x360_black_screen_take); A or Enter acts: the game ends
 * (unsaved) or the screen goes. It shows at the title as in play (the GUI sheet is there in both). */
static void *bs_back, *bs_text, *bs_hint;
static unsigned bs_t3;                 /* when it went with A still held */
static int bs_state;                   /* 0 none, 1 asked, 2 shown, 3 gone but A still held (QA run g1-3b:
                                          * the held A reached the title's Continue the next frame) */
static int bs_close, bs_act, bs_failed;
static char bs_utf8[1400];
static void (*bs_after[2])(void);      /* when it goes (also when it ends the game) */
static void black_screen(const char *utf8, int close_game, void (*after)(void)) {
    if (bs_state == 1 || bs_state == 2) {
        /* Another one at the same time (the title's warning and a stop's text left by the last
         * game): one screen with both, neither lost (the review). */
        unsigned n = length(bs_utf8);
        if (n + 2 < sizeof bs_utf8) { bs_utf8[n++] = '\n'; bs_utf8[n++] = '\n'; bs_utf8[n] = 0; }
        copy(bs_utf8 + n, sizeof bs_utf8 - n, utf8, 0);
        bs_close = bs_close || close_game;
        if (!bs_after[1]) bs_after[1] = after;
        bs_act = 0;     /* an A for the earlier text does not close the new one unseen */
        bs_state = 1;   /* laid out again */
    } else {
        copy(bs_utf8, sizeof bs_utf8, utf8, 0);
        bs_close = close_game;
        bs_after[0] = after;
        bs_after[1] = 0;
        bs_act = 0;
        bs_state = 1;
    }
    bs_failed = 0;
    x360_log("x360: the black screen asked for: "); x360_log(utf8); x360_log("\r\n");
}
static void black_screen_done(void) {
    if (bs_after[0]) bs_after[0]();
    if (bs_after[1] && bs_after[1] != bs_after[0]) bs_after[1]();
}
int x360_black_screen_take(unsigned buttons) {   /* pad.c: 1 while it shows (the pad is its own) */
    static unsigned last;
    int edge = (buttons & 0x1000) && !(last & 0x1000);
    last = buttons;
    /* A let go, or held over 2 s (a stuck button must not leave the pad dead). */
    if (bs_state == 3) { if (!(buttons & 0x1000) || GetTickCount() - bs_t3 > 2000) bs_state = 0; return bs_state == 3; }
    if (bs_state == 1 && bs_back) return 1;   /* laid out again while it shows: still its own */
    if (bs_state != 2) return 0;
    if (edge) bs_act = 1;
    return 1;
}
unsigned long x360_read_text(void *f, char *buf, unsigned long size, int *full);   /* cegui_proxy.c: line ends made LF */
__declspec(dllimport) void *__stdcall GetCurrentProcess(void);
__declspec(dllimport) int __stdcall TerminateProcess(void *process, unsigned code);
static void black_screen_tick(void) {
    static int enter_was;
    int enter = (GetAsyncKeyState(0x0d) & 0x8000) != 0;
    if (bs_state == 1) {
        void *sys, *sheet;
        RECT r;
        HWND w = active_window();
        float width = 1920.0f, height = 1080.0f;
        if (!resolve()) return;
        sys = system_ptr();
        sheet = sys ? get_gui_sheet(sys, 0) : 0;
        if (!sheet) return;                       /* not yet: the next frame */
        if (w && GetClientRect(w, &r) && r.right > 0 && r.bottom > 0) { width = (float)r.right; height = (float)r.bottom; }
        if (!bs_back) {
            bs_back = create_window("x360bs_", "back", "ceguiWidget/StaticImage");
            bs_text = create_window("x360bs_", "text", "ceguiWidget/StaticText");
            bs_hint = create_window("x360bs_", "hint", "ceguiWidget/StaticText");
            if (!bs_back || !bs_text || !bs_hint) {
                bs_back = bs_text = bs_hint = 0;   /* all three or none: never a screen without its text */
                bs_failed = 1; bs_state = 0;
                x360_log("x360: the black screen could not be made\r\n");
                return;
            }
            prop(bs_back, "Image", "set:X360_widgets2 image:black");
            prop(bs_back, "FrameEnabled", "False");
            prop(bs_back, "BackgroundEnabled", "False");
            prop(bs_back, "AlwaysOnTop", "True");
            add_child(bs_back, 0, bs_text);
            add_child(bs_back, 0, bs_hint);
            {
                /* Only the properties this scheme's StaticText has (FrameEnabled and
                 * BackgroundEnabled threw, QA run g1-4); the text itself is laid out by rich_text,
                 * as the dialog's paragraphs: WordWrapLeftAligned, each one's height measured. */
                void *list[2]; int i;
                list[0] = bs_text; list[1] = bs_hint;
                for (i = 0; i < 2; i++) {
                    prop(list[i], "Font", "X360_Serif");
                    prop(list[i], "TextColour", "FFFFFFFF");
                }
            }
        }
        add_child(sheet, 0, bs_back);
        set_size(bs_back, 0, 0, width, height);
        set_size(bs_text, width * 0.15f, height * 0.25f, width * 0.7f, height * 0.4f);
        set_size(bs_hint, width * 0.15f, height * 0.7f, width * 0.7f, height * 0.08f);
        rich_text(bs_text, "x360bs_", "body", bs_utf8);
        rich_text(bs_hint, "x360bs_", "hintline", bs_close ? "A: close the game" : "A: continue");
        set_visible(bs_back, 0, 1);
        move_to_front(bs_back, 0);
        bs_state = 2;
        x360_log("x360: the black screen shows\r\n");
    }
    if (bs_state == 2) {
        move_to_front(bs_back, 0);
        if (enter && !enter_was && GetActiveWindow()) bs_act = 1;   /* Enter only with the game in front */
        if (bs_act) {
            bs_act = 0;
            if (bs_close) {
                x360_log("x360: the black screen closed: the game ends, unsaved\r\n");
                black_screen_done();
                x360_pad_shutdown(), TerminateProcess(GetCurrentProcess(), 1);
            }
            set_visible(bs_back, 0, 0);
            bs_state = 3;   /* the pad kept until A is let go */
            bs_t3 = GetTickCount();
            x360_log("x360: the black screen closed\r\n");
            black_screen_done();
        }
    }
    enter_was = enter;
}

/* ---- the save held back while a stop is pending (docs 21) ----
 * Every save of the character goes through 0x417380 (thiscall, 2 arguments, ret 8: its 9 callers
 * are the game's; it writes backup.tmp with "wb" at 0x41762d, renames it into the .svt at
 * 0x417fad and writes sharedstash.bin through 0x52e3f0, whose one caller is 0x4173ce, in it). The
 * other files opened "wb" or "w" are the settings' and logs'. While x360_save_held is set the
 * function returns at once: nothing of the character or the shared stash is written. Its first
 * instructions (GOG's push -1; push the handler, 7 bytes; Steam's mov eax, fs:[0]; push -1, 8)
 * run from a copy, as for the modal function. */
#define GOG_SAVE_GAME 0x00417380
#define GOG_SAVE_HANDLER 0x0060a15a    /* the save function's exception handler (its push imm32) */
#ifdef X360_TARGET_STEAM
static const unsigned char save_want[9] = {0x64, 0xa1, 0x00, 0x00, 0x00, 0x00, 0x6a, 0xff, 0x68};
#define SAVE_MOVED 8
#else
static const unsigned char save_want[3] = {0x6a, 0xff, 0x68};
#define SAVE_MOVED 7
#endif
static unsigned char save_tramp[16];
int x360_save_held;                    /* also x360_ui.ini hold_saves=1: a test of the hold, off by default */
int x360_test_save_hook_fails;         /* x360_ui.ini test_save_hook_fails=1: a test, as if the pattern differed */
int x360_test_load_hook_fails;         /* x360_ui.ini test_load_hook_fails=1: a test, as if the loader's call differed */
static int load_items_aside(void);
static int __fastcall save_game_hook(void *self, void *edx, int a, int b) {
    typedef int (__fastcall *orig_fn)(void *self, void *edx, int a, int b);
    if (x360_save_held) { x360_log("x360 load: a save refused (a stop is pending)\r\n"); return 0; }
    /* From the first item kept aside until every one is placed: a save then would leave them out
     * (the report's case: the game left during the wait for the pet). */
    if (load_items_aside()) { x360_log("x360 load: a save refused (items are kept aside, not yet placed)\r\n"); return 0; }
    return ((orig_fn)(void *)save_tramp)(self, edx, a, b);
}
static int save_hook_install(void) {
    unsigned char *f = (unsigned char *)GOG_SAVE_GAME;
    unsigned long old;
    unsigned i;
    if (x360_test_save_hook_fails) { x360_log("x360 load: test_save_hook_fails=1: the save function taken as not as expected\r\n"); return 0; }
    for (i = 0; i < sizeof save_want; i++) if (f[i] != save_want[i]) break;
    if (i < sizeof save_want || *(unsigned *)(f + sizeof save_want) != GOG_SAVE_HANDLER) {
        x360_log("x360 load: PC's save function not as expected, saves not held back\r\n");
        return 0;
    }
    for (i = 0; i < SAVE_MOVED; i++) save_tramp[i] = f[i];
    save_tramp[SAVE_MOVED] = 0xe9;
    *(int *)(save_tramp + SAVE_MOVED + 1) = (int)(f + SAVE_MOVED) - (int)(save_tramp + SAVE_MOVED + 5);
    if (!VirtualProtect(save_tramp, sizeof save_tramp, 0x40, &old)) return 0;
    if (!VirtualProtect(f, SAVE_MOVED, 0x40, &old)) return 0;
    f[0] = 0xe9;
    *(int *)(f + 1) = (int)(void *)save_game_hook - (int)(f + 5);
    for (i = 5; i < SAVE_MOVED; i++) f[i] = 0x90;
    VirtualProtect(f, SAVE_MOVED, old, &old);
    return 1;
}

/* ---- items placed again at load (the owner's plan, docs 21) ----
 * PC's loader of a unit's saved data puts each saved item at its saved slot (0x4e9480 at
 * 0x4acd4b) and destroys it when that fails: a slot the bag lacks for its type (69 to 81 with the
 * 360's bag, another type's section with PC's). The call goes through load_put_hook: for the
 * hero's (ISA 28) and the pet's (ISA 87) bag slots (19 on; the equipped ones are left to PC), an
 * item that PC refuses goes to the first empty slot of its type's section; with none it is kept
 * aside (not destroyed: the loader is answered with the item, which it only reads for a gambler,
 * ISA 127) and, at the first frame in play, put into the other one's bag (the pet's for the
 * hero's, the hero's for the pet's). One that fits nowhere stops the game before anything is saved
 * (the save is written on leaving the game or a level, not at load: QA run 28), with a message.
 * A slot PC accepts is kept: loading twice moves nothing the second time. */
#define GOG_LOAD_PUT_CALL 0x004acd4b   /* in the loader's loop (0x4acc3d), the call of 0x4e9480 */
typedef int (__fastcall *load_put_fn)(void *inv, void *edx, void *item, int slot, int flag);
#define LOAD_ASIDE_MAX 64
static struct { void *item, *owner, *inv; int slot; } load_aside[LOAD_ASIDE_MAX];
static int load_aside_count, load_moved, load_stopped;
int x360_test_no_stop;                 /* x360_ui.ini test_no_stop=1: a test, items kept aside never decided */
static int load_overflow;              /* items past LOAD_ASIDE_MAX: kept alive (not tracked), the game stopped */
static int load_puts;                  /* every item PC's loader put (logged per phase: which loads run when) */
/* No save hook (the owner, after QA run g1-3): the load hook only stops. At the first item of the
 * player's load that PC would destroy, the item is kept (the loader answered with it) and the game
 * ends at once, before any save (Torchlight saves on closing, Alt+F4 too): its text in
 * x360_stop.txt, shown on the black screen at the next start. Nothing is kept aside: no save
 * could be held back. */
static int load_cut_only;
static int write_stop_file(const char *text);
static void utf8_to_wide(const char *text, unsigned short *w, unsigned size);
static void last_resort_box(const unsigned short *text);
static int load_items_aside(void) {
    int k;
    if (load_overflow) return 1;
    for (k = 0; k < load_aside_count; k++) if (load_aside[k].item) return 1;
    return 0;
}
static int load_aside_left(void) {
    int k, n = 0;
    for (k = 0; k < load_aside_count; k++) if (load_aside[k].item) n++;
    return n + load_overflow;
}
/* A load the player asked for: set when a menu out of play gives way to none (Continue, Load, a
 * new game pressed), cleared when a menu shows again or play is left. The title's own loads of the
 * last character, before its menu shows or after leaving play, are not the player's: left to PC
 * (their copy is not saved: QA run 29). */
static int load_armed;

static void log_item_name(void *item) {
    char name[96];
    name[0] = 0;
    __try { unit_name((unsigned char *)item, name, sizeof name); } __except (1) { name[0] = 0; }
    x360_log(name[0] ? name : "(no name)");
}

/* The first empty slot of the item's section in a bag, or -1. */
static int empty_slot_of_section(unsigned char *inv, void *item) {
    typedef void *(__fastcall *at_fn)(void *self, void *edx, int slot);
    int *starts = *(int **)(inv + INV_PANE_STARTS), nsec, sec, first, end, sl;
    if (!starts) return -1;
    nsec = (int)((*(int **)(inv + INV_PANE_STARTS + 4)) - starts);
    sec = bag_section(inv, item);
    if (sec < 0 || sec >= nsec) return -1;
    first = starts[sec];
    end = sec + 1 < nsec ? starts[sec + 1] : *(int *)(inv + INV_SLOTS);
    for (sl = first; sl < end; sl++) if (!((at_fn)GOG_ITEM_AT_SLOT)(inv, 0, sl)) return sl;
    return -1;
}

/* An item kept aside whose unit is gone: destroyed, as PC's loader would have (its vtable's first
 * entry with 1, 0x4acd5b). */
static void destroy_aside(int k) {
    typedef void (__fastcall *dtor_fn)(void *self, void *edx, int flags);
    void *item = load_aside[k].item;
    load_aside[k].item = 0;
    if (item) __try { ((dtor_fn)(*(void ***)item)[0])(item, 0, 1); } __except (1) { x360_log("x360 load: exception dropping an item\r\n"); }
}

static int __fastcall load_put_hook(void *inv, void *edx, void *item, int slot, int flag) {
    load_put_fn put = (load_put_fn)GOG_PUT_AT_SLOT;
    unsigned char *owner;
    int r = put(inv, 0, item, slot, flag), to;
    load_puts++;
    if (r || !inv || !item || slot < EQUIPPED_SLOTS) return r;
    owner = *(unsigned char **)((unsigned char *)inv + 0x14);   /* CInventory's owner (0x4e953f reads it) */
    if (!load_armed) {
        x360_log("x360 load: the title's load: "); log_item_name(item); x360_log(" from slot "); x360_log_num(slot);
        x360_log(" left to PC (that copy is not saved)\r\n");
        return r;
    }
    if (!owner || !(isa(owner, 28) || isa(owner, 87))) {
        x360_log("x360 load: PC drops "); log_item_name(item); x360_log(" from slot "); x360_log_num(slot);
        x360_log(owner ? " (a unit not of type 28 or 87)\r\n" : " (no owner)\r\n");
        return r;
    }
    if (load_cut_only) {
        x360_log("x360 load: no save hook: "); log_item_name(item); x360_log(" from slot "); x360_log_num(slot);
        x360_log(" would be destroyed: the game ends now, unsaved\r\n");
        {
            static const char cut[] = "The game was closed by the mod while a character loaded: an item of it does not fit the bag "
                                      "chosen at install, and with this version of the game the mod cannot hold the save back. "
                                      "Nothing was saved, nothing was lost. Install again with the bag that character was saved with.";
            if (!write_stop_file(cut)) {   /* nowhere to leave it for the next start: a box now (it shows on Windows) */
                unsigned short w[600];
                utf8_to_wide(cut, w, 600);
                last_resort_box(w);
            }
        }
        x360_pad_shutdown(), TerminateProcess(GetCurrentProcess(), 1);
        return (int)item;   /* not reached */
    }
    to = empty_slot_of_section((unsigned char *)inv, item);
    if (to >= 0 && (r = put(inv, 0, item, to, flag)) != 0) {
        load_moved++;
        x360_log("x360 load: "); log_item_name(item); x360_log(isa(owner, 87) ? " (the pet's)" : " (the hero's)");
        x360_log(" from slot "); x360_log_num(slot); x360_log(" to "); x360_log_num(to); x360_log("\r\n");
        return r;
    }
    {
        /* The same bag and slot again: another load of the same save (the title loads the last
         * character too, QA run 29): the earlier item kept aside is that unit's, gone with it. */
        int k;
        for (k = 0; k < load_aside_count; k++)
            if (load_aside[k].item && load_aside[k].inv == inv && load_aside[k].slot == slot) {
                destroy_aside(k);
                load_aside[k].item = item;
                load_aside[k].owner = owner;
                x360_log("x360 load: "); log_item_name(item); x360_log(" from slot "); x360_log_num(slot);
                x360_log(": kept aside again (another load), the earlier copy dropped\r\n");
                return (int)item;
            }
    }
    if (load_aside_count < LOAD_ASIDE_MAX) {
        load_aside[load_aside_count].item = item;
        load_aside[load_aside_count].owner = owner;
        load_aside[load_aside_count].inv = inv;
        load_aside[load_aside_count].slot = slot;
        load_aside_count++;
        x360_log("x360 load: "); log_item_name(item); x360_log(isa(owner, 87) ? " (the pet's)" : " (the hero's)");
        x360_log(" from slot "); x360_log_num(slot); x360_log(": no room in its section, kept aside\r\n");
        return (int)item;
    }
    /* More than can be tracked: not destroyed (the loader is answered with the item, never placed),
     * and the game stops in play before any save. */
    load_overflow++;
    x360_log("x360 load: too many items kept aside: "); log_item_name(item); x360_log(" kept, the game will stop\r\n");
    return (int)item;
}

__declspec(dllimport) void *__stdcall GetCurrentProcess(void);
__declspec(dllimport) int __stdcall TerminateProcess(void *process, unsigned code);

/* In play: the items kept aside into the other one's bag; any left stops the game, unsaved. The
 * pet is waited for without a clock (a slow machine must not stop the game): until it exists, or
 * the world is ready without it (the 360's HUD up, then 60 frames, counted, not timed); then, with
 * no pet, the game stops with a message that says so. Nothing is done out of play (the title loads
 * the last character as well, QA run 29: its items were kept aside and placed twice). */
/* The stop: saves held back at once; the text into x360_stop.txt (stop_path: LOCALAPPDATA\\x360ui) and the log; PC's
 * own modal opened with it (0x54cad0 on CGameUI, as PC's callers do: its title and text, wide
 * strings by value), which the 360's modal shows (modal_tick) and which takes the pad, keyboard
 * and mouse as PC's modal does; when it closes (A, Ok), TerminateProcess. A Windows box showed
 * blank under Proton (QA runs 34, 35): not used. */
static int stop_state;                 /* 0 none, 1 PC's modal asked for, 2 it showed, 3 waiting for another modal to close, 4 the black screen */
static int stop_asked;                 /* a stop was decided (also declared above, for the menus' input) */
static unsigned stop_t0;               /* when (GetTickCount): the modal is waited for STOP_MODAL_MS */
#define STOP_MODAL_MS 15000          /* from the stop's modal asked for */
#define STOP_WAIT_MS 120000          /* from the stop, while another modal is open (state 3) */
#define STOP_BOX_MS 30000
static unsigned short stop_wbody[700];
static unsigned short stop_file[600];    /* x360_stop.txt, where it was written (wide: any user name), or empty */
int x360_test_stop_modal_fails;        /* x360_ui.ini test_stop_modal_fails=1: a test, PC's modal never shows */

static void utf8_to_wide(const char *text, unsigned short *w, unsigned size) {
    /* UTF-8 to UTF-16 (a character's name may have any letter). */
    const unsigned char *u = (const unsigned char *)text;
    unsigned o = 0;
    while (*u && o + 1 < size) {
        unsigned c = *u++;
        if (c >= 0xe0 && u[0] && u[1]) { c = (c & 0x0f) << 12 | (u[0] & 0x3f) << 6 | (u[1] & 0x3f); u += 2; }
        else if (c >= 0xc0 && u[0]) { c = (c & 0x1f) << 6 | (u[0] & 0x3f); u++; }
        w[o++] = (unsigned short)c;
    }
    w[o] = 0;
}

/* PC's own modal (0x54cad0 on CGameUI, as PC's callers do: its title and text, wide strings by
 * value), which the 360's modal shows. 1 when asked for. */
static int open_pc_modal(const char *title, const unsigned short *wbody) {
    unsigned short wtitle[64];
    unsigned i;
    int done = 0;
    for (i = 0; i < 63 && title[i]; i++) wtitle[i] = (unsigned char)title[i];
    wtitle[i] = 0;
    __try {
        typedef void (__fastcall *ctor_fn)(void *self, void *edx, const unsigned short *s);
        typedef void (__fastcall *modal_fn)(void *gui, void *edx, msvc_wstring title, msvc_wstring body, int x);
        msvc_wstring t, b;
        ctor_fn ctor = *(ctor_fn *)0x00a85418;   /* msvcp90's basic_string<wchar_t>(const wchar_t *), as PC's callers */
        if (ui) {
            ctor(&t, 0, wtitle);
            ctor(&b, 0, wbody);
            ((modal_fn)0x0054cad0)(ui, 0, t, b, 0);   /* it takes the strings (by value) and destroys them */
            done = 1;
        }
    } __except (1) { done = 0; x360_log("x360: exception opening PC's modal\r\n"); }
    return done;
}

/* The last resort where PC's modal did not show: x360_stop.txt in Notepad, else a Windows box (it
 * may be blank under Proton, QA
 * runs 34, 35, and behind the game's window): the game minimized, on top, and closed by itself after 30 s so a box
 * that cannot be seen does not keep the game open forever. user32's, looked up (no import). */
static void last_resort_box(const unsigned short *text) {
    /* First the stop's text file in Notepad: another process, which draws (QA run g1-2: a box
     * from the game showed no text under Proton). The box only when Notepad cannot start. */
    if (stop_file[0]) {
        typedef int (__stdcall *create_process_fn)(const unsigned short *app, unsigned short *cmd, void *pa, void *ta, int inherit,
                                                   unsigned long flags, void *env, const unsigned short *dir, void *si, void *pi);
        create_process_fn run = (create_process_fn)kernel32_fn("CreateProcessW");
        unsigned short cmd[660];
        unsigned long si[17] = {68}, pi[4] = {0};   /* STARTUPINFOW (cb 68), PROCESS_INFORMATION */
        unsigned n;
        cmd[0] = 0;
        wappend(cmd, 660, "notepad.exe \"");
        n = wlength(cmd);
        { unsigned k; for (k = 0; stop_file[k] && n + 2 < 660; k++) cmd[n++] = stop_file[k]; cmd[n] = 0; }
        wappend(cmd, 660, "\"");
        if (run && run(0, cmd, 0, 0, 0, 0, 0, 0, si, pi)) {
            CloseHandle((void *)pi[0]); CloseHandle((void *)pi[1]);
            x360_log("x360 load: the stop's text opened in Notepad\r\n");
            return;
        }
        x360_log("x360 load: Notepad did not start: a Windows box\r\n");
    }
    typedef int (__stdcall *box_timeout_fn)(void *w, const unsigned short *t, const unsigned short *c, unsigned type, unsigned short lang, unsigned ms);
    typedef int (__stdcall *box_fn)(void *w, const unsigned short *t, const unsigned short *c, unsigned type);
    static const unsigned short caption[] = {'T','o','r','c','h','l','i','g','h','t',0};
    void *u = GetModuleHandleA("user32.dll");
    box_timeout_fn bt = u ? (box_timeout_fn)GetProcAddress(u, "MessageBoxTimeoutW") : 0;
    box_fn b = u ? (box_fn)GetProcAddress(u, "MessageBoxW") : 0;
    const unsigned type = 0x10 | 0x1000 | 0x10000 | 0x40000;   /* MB_ICONERROR, MB_SYSTEMMODAL, MB_SETFOREGROUND, MB_TOPMOST */
    {
        /* The game's window out of the way first: a box behind a full-screen window is not seen. */
        typedef int (__stdcall *show_fn)(void *w, int how);
        show_fn show = u ? (show_fn)GetProcAddress(u, "ShowWindow") : 0;
        void *w = (void *)active_window();
        if (show && w) show(w, 6);   /* SW_MINIMIZE */
    }
    if (bt) bt(0, text, caption, type, 0, STOP_BOX_MS);
    else if (b) b(0, text, caption, type);
}

/* A message for the player that does not stop the game: shown in PC's modal once in play. */
static char notice_utf8[1400];
static int notice_pending;
static void queue_notice(const char *text) {   /* appended to any notice not shown yet */
    unsigned n = length(notice_utf8);
    if (n && n + 2 < sizeof notice_utf8) { notice_utf8[n++] = '\n'; notice_utf8[n++] = '\n'; notice_utf8[n] = 0; }
    copy(notice_utf8 + n, sizeof notice_utf8 - n, text, 0);
    notice_pending = 1;
    x360_log("x360: a notice for the player: "); x360_log(text); x360_log("\r\n");
}
static void notice_tick(void) {
    static unsigned short wide[1400];
    /* Not while a tip is up (the owner's run of 2026-10-24: the new game's welcome tip, then the
     * notice over it, and neither closed): shown once the tip is closed. */
    if (!notice_pending || !hud_in_play || !ui || stop_asked || pc_modal_menu_open() || tip_up_now()) return;
    utf8_to_wide(notice_utf8, wide, sizeof wide / sizeof wide[0]);
    if (open_pc_modal("The mod", wide)) { notice_pending = 0; notice_utf8[0] = 0; x360_log("x360: the notice shows\r\n"); }
}
/* x360_ui.ini's test options found at start (proxy/cegui_proxy.c): told on screen once in play. */
void x360_test_options_notice(const char *names) {
    char text[400];
    copy(text, sizeof text, "Test options are on in x360_ui.ini (", names);
    copy(text + length(text), sizeof text - length(text), "). They are for test runs on copies and may hold "
         "saves back or move the save folder: remove those lines, or install again.", 0);
    queue_notice(text);
}

/* x360_stop.txt (stop_path): the stop's text, kept until the player has seen it (the stop's
 * modal or black screen closed by them); one left at the next start is shown then, at the title. */
static char stop_utf8[700];
/* Its place: %LOCALAPPDATA%\x360ui\x360_stop.txt (the game's folder may be Program Files, which the
 * game may not write and Windows would redirect into VirtualStore); the uninstallers remove the
 * folder. Beside the game when that cannot be written. Wide paths throughout (a user name the ANSI
 * code page cannot hold, the review); kernel32's W functions, looked up. */
static int stop_path(unsigned short *path, unsigned size, int beside_game) {
    typedef unsigned long (__stdcall *getenv_fn)(const unsigned short *name, unsigned short *out, unsigned long size);
    typedef int (__stdcall *mkdir_fn)(const unsigned short *name, void *security);
    typedef unsigned long (__stdcall *module_fn)(void *module, unsigned short *out, unsigned long size);
    static const unsigned short var[] = {'L','O','C','A','L','A','P','P','D','A','T','A',0};
    unsigned long n;
    path[0] = 0;
    if (!beside_game) {
        getenv_fn env = (getenv_fn)kernel32_fn("GetEnvironmentVariableW");
        mkdir_fn mkdir = (mkdir_fn)kernel32_fn("CreateDirectoryW");
        n = env ? env(var, path, size - 32) : 0;
        if (!n || n >= size - 32 || !mkdir) { path[0] = 0; return 0; }
        wappend(path, size, "\\x360ui");
        mkdir(path, 0);   /* there already: fine */
        wappend(path, size, "\\x360_stop.txt");
        return 1;
    } else {
        module_fn mod = (module_fn)kernel32_fn("GetModuleFileNameW");
        n = mod ? mod(0, path, size - 20) : 0;
        if (!n) return 0;
        while (n && path[n - 1] != '\\' && path[n - 1] != '/') n--;
        path[n] = 0;
        wappend(path, size, "x360_stop.txt");
        return 1;
    }
}
static int write_text_w(const unsigned short *path, const char *text) {
    typedef void *(__stdcall *create_fn)(const unsigned short *name, unsigned long access, unsigned long share, void *sec,
                                         unsigned long disp, unsigned long flags, void *tmpl);
    create_fn create = (create_fn)kernel32_fn("CreateFileW");
    void *f = create ? create(path, 0x40000000, 0, 0, 2, 0x80, 0) : (void *)-1;   /* GENERIC_WRITE, CREATE_ALWAYS */
    unsigned long done = 0;
    int ok;
    if (f == (void *)-1) return 0;
    ok = WriteFile(f, text, length(text), &done, 0) && done == length(text);
    CloseHandle(f);
    return ok;
}
/* Written where it can be: 1 if anywhere (stop_file then names it), 0 if nowhere (logged). */
static int write_stop_file(const char *text) {
    unsigned short path[600];
    int i;
    for (i = 0; i < 2; i++)
        if (stop_path(path, 600, i) && write_text_w(path, text)) {
            unsigned k;
            for (k = 0; k < 600 && (stop_file[k] = path[k]) != 0; k++) {}
            if (i) x360_log("x360 load: x360_stop.txt written beside the game (LOCALAPPDATA could not be written)\r\n");
            return 1;
        }
    stop_file[0] = 0;
    x360_log("x360 load: x360_stop.txt could not be written anywhere\r\n");
    return 0;
}
static void stop_seen(void) {   /* seen: removed where it was, and from the other place too */
    typedef int (__stdcall *del_fn)(const unsigned short *name);
    del_fn del = (del_fn)kernel32_fn("DeleteFileW");
    unsigned short path[600];
    int i;
    if (!del) return;
    if (stop_file[0]) del(stop_file);
    for (i = 0; i < 2; i++) if (stop_path(path, 600, i)) del(path);
}
static void stop_unsaved(const char *why, int count) {
    char text[700], name[96], *p = text;
    x360_save_held = 1;
    if (stop_asked) { x360_log("x360 load: a second stop ignored (one is under way)\r\n"); return; }
    stop_asked = 1;
    stop_t0 = GetTickCount();
    name[0] = 0;
    __try { if (hero && !wstring_utf8(hero + 0x420, name, sizeof name)) name[0] = 0; } __except (1) { name[0] = 0; }   /* the shown name (GOG_UNIT_NAME), not the class's */
    copy(p, sizeof text, name[0] ? name : "This character", ": ");
    p += length(p);
    if (count >= 0) p = put_int(p, count);
    copy(p, sizeof text - (unsigned)(p - text), why, 0);
    x360_log("x360 load: the stop's text: "); x360_log(text); x360_log("\r\n");
    write_stop_file(text);
    copy(stop_utf8, sizeof stop_utf8, text, 0);
    utf8_to_wide(text, stop_wbody, sizeof stop_wbody / sizeof stop_wbody[0]);
    stop_state = 3;   /* opened by stop_tick once no other modal is open (closing that one must not end the game) */
}
static void stop_tick(void) {
    if (stop_state == 3 && !pc_modal_menu_open() && !tip_up_now()) {   /* a tip closed first; STOP_WAIT_MS still holds */
        stop_state = x360_test_stop_modal_fails ? 1 : open_pc_modal("The bag", stop_wbody);   /* the test: asked, never shown */
        stop_t0 = GetTickCount();   /* its own 15 s, from now */
        if (!stop_state) x360_log("x360 load: PC's modal could not be opened: saves held back, the text in x360_stop.txt\r\n");
    }
    if (stop_state == 1 && pc_modal_menu_open() && !x360_test_stop_modal_fails) { stop_state = 2; x360_log("x360 load: the stop's modal shows\r\n"); }
    /* Never playing on unsaved without knowing: PC's modal not opened, or not shown in
     * STOP_MODAL_MS, gives the text on the proxy's black screen (A ends the game); if that cannot
     * be made or does not show in 5 s, x360_stop.txt in Notepad or a Windows box, then the game
     * ends (nothing saved; the file shows again at the next start). */
    if (stop_asked && stop_state != 2 && stop_state != 4 &&
        (stop_state == 0 || GetTickCount() - stop_t0 > (stop_state == 3 ? STOP_WAIT_MS : STOP_MODAL_MS))) {
        x360_log("x360 load: the stop's modal did not show: the black screen\r\n");
        black_screen(stop_utf8[0] ? stop_utf8 : "The game stops: nothing is saved, nothing is lost.", 1, stop_seen);
        stop_state = 4;
        stop_t0 = GetTickCount();
    }
    if (stop_state == 4 && bs_state != 2 && (bs_failed || GetTickCount() - stop_t0 > 5000)) {
        x360_log("x360 load: the black screen did not show: Notepad or a Windows box, then the game ends, unsaved\r\n");
        last_resort_box(stop_wbody);
        x360_pad_shutdown(), TerminateProcess(GetCurrentProcess(), 1);
    }
    if (stop_state == 2 && !pc_modal_menu_open()) {
        x360_log("x360 load: the stop's modal closed: the game ends, unsaved\r\n");
        stop_seen();
        x360_pad_shutdown(), TerminateProcess(GetCurrentProcess(), 1);   /* no code of the game runs after: no save */
    }
}

static void load_aside_tick(void) {
    static int frames_ready, pet_logged;
    int k, left = 0, need_pet = 0;
    unsigned char *pet = the_pet();
    static int was_in_play;
    {
        /* Which loads run when (the review's question: does a level change load the hero's items
         * again with the load not armed?): the items PC's loader put since the last log. */
        static int logged_puts;
        if (load_puts != logged_puts) {
            x360_log("x360 load: PC's loader put "); x360_log_num(load_puts - logged_puts);
            x360_log(load_armed ? " item(s), the load armed, " : " item(s), the load not armed, ");
            x360_log(hud_in_play ? "in play\r\n" : "out of play\r\n");
            logged_puts = load_puts;
        }
    }
    if (was_in_play && !hud_in_play) { load_armed = 0; x360_log("x360 load: play left: the load not armed\r\n"); }
    was_in_play = hud_in_play;
    if (!hud_in_play) { frames_ready = 0; return; }   /* the title or a load: the units may be the title's */
    if (pet && !pet_logged) { pet_logged = 1; x360_log("x360 load: the pet is there, "); x360_log_num(frames_ready); x360_log(" frames into play\r\n"); }
    if (frames_ready < 1000000) frames_ready++;
    if (!load_aside_count || load_stopped || x360_test_no_stop) return;
    /* Only the items of the hero's and the pet's bags in play; others are of units gone (the
     * title's copy of the character). */
    for (k = 0; k < load_aside_count; k++) {
        void *own = load_aside[k].owner;
        if (!load_aside[k].item) continue;
        if ((own == hero || (pet && own == pet)) && load_aside[k].inv == owner_bag(own)) continue;
        x360_log("x360 load: "); log_item_name(load_aside[k].item);
        x360_log(" from slot "); x360_log_num(load_aside[k].slot); x360_log(": its unit is gone, dropped\r\n");
        destroy_aside(k);
    }
    for (k = 0; k < load_aside_count; k++) if (load_aside[k].item && !isa(load_aside[k].owner, 87)) need_pet = 1;
    if (load_overflow) {
        load_stopped = 1;
        x360_log("x360 load: more items kept aside than can be placed: the game stops, unsaved\r\n");
        stop_unsaved("Too many items do not fit in the bag chosen at install to be placed. Nothing is saved, nothing is "
                     "lost: press A to close the game. Then install again with the other bag, load this character, make "
                     "room (sell, stash or drop items), save, and change the bag.", -1);
        return;
    }
    if (need_pet && !pet) {
        if (frames_ready < 60) return;
        load_stopped = 1;
        x360_log("x360 load: the world is ready and the hero has no pet: the game stops, unsaved\r\n");
        stop_unsaved(" item(s) do not fit in the bag chosen at install, and the pet, where they would go, is not "
                     "there. Nothing is saved, nothing is lost: press A to close the game. Then install again with the "
                     "other bag, load this character, make room (sell, stash or drop items), save, and change the bag.",
                     load_aside_left());
        return;
    }
    for (k = 0; k < load_aside_count; k++) {
        void *item = load_aside[k].item;
        unsigned char *other = isa(load_aside[k].owner, 87) ? hero : pet;
        unsigned char *bag = other ? (unsigned char *)owner_bag(other) : 0;
        int to = bag ? empty_slot_of_section(bag, item) : -1;
        if (!item) continue;
        if (to >= 0 && ((load_put_fn)GOG_PUT_AT_SLOT)(bag, 0, item, to, 1)) {
            x360_log("x360 load: "); log_item_name(item); x360_log(other == pet ? " into the pet's bag, slot " : " into the hero's bag, slot ");
            x360_log_num(to); x360_log("\r\n");
            load_aside[k].item = 0;
            load_moved++;
        } else left++;
    }
    if (!left) { load_aside_count = 0; return; }
    load_stopped = 1;
    for (k = 0; k < load_aside_count; k++) {
        if (!load_aside[k].item) continue;
        x360_log("x360 load: fits nowhere: "); log_item_name(load_aside[k].item);
        x360_log(isa(load_aside[k].owner, 87) ? " (the pet's) from slot " : " (the hero's) from slot ");
        x360_log_num(load_aside[k].slot); x360_log("\r\n");
    }
    x360_log("x360 load: "); x360_log_num(left); x360_log(" item(s) fit nowhere: the game stops, unsaved\r\n");
    stop_unsaved(" item(s) do not fit in the bag or the pet's with the bag chosen at install. Nothing is saved, "
                 "nothing is lost: press A to close the game. Then install again with the other bag, load this "
                 "character, make room (sell, stash or drop items), save, and change the bag.", left);
}

/* The save's hook first. With it, the load hook keeps aside and places again; without it, the load
 * hook only stops (load_cut_only). Without the load hook nothing protects the items (PC destroys at
 * load what does not fit): a black screen at the title says so before any character is loaded. */
static const char hooks_off_text[] =
    "The mod cannot protect the items of a character with this version of the game: when a character is "
    "loaded, the game deletes the items that do not fit the bag chosen at install. Install again with PC's "
    "bag before loading a character. Characters saved with the 360's bag can lose items with the original game "
    "too: load and save each with the mod's PC's bag first.";
void x360_load_hook_install(void) {
    unsigned char *c = (unsigned char *)GOG_LOAD_PUT_CALL;
    unsigned long old;
    int saves = save_hook_install();
    if (saves) x360_log("x360 load: PC's save function hooked (held back while items are aside or a stop is pending)\r\n");
    else x360_log("x360 load: no save hook: the load hook only stops\r\n");
    if (x360_test_load_hook_fails || c[0] != 0xe8 || (unsigned)(unsigned long)(c + 5) + *(unsigned *)(c + 1) != GOG_PUT_AT_SLOT ||
        !VirtualProtect(c, 5, 0x40, &old)) {
        x360_log("x360 load: PC's loader not as expected, items not protected: a warning at the title\r\n");
        black_screen(hooks_off_text, 0, 0);
        return;
    }
    load_cut_only = !saves;
    *(int *)(c + 1) = (int)(void *)load_put_hook - (int)(c + 5);
    VirtualProtect(c, 5, old, &old);
    x360_log(saves ? "x360 load: saved items placed again when their slot does not fit\r\n"
                   : "x360 load: the load hook in its stop-only mode\r\n");
}

/* A stop's text left by the last run (x360_stop.txt): the player did not see it (the game ended
 * at once, or the stop's screens did not show). Shown at the title, then the file goes. */
void x360_stop_file_check(void) {
    typedef void *(__stdcall *create_fn)(const unsigned short *name, unsigned long access, unsigned long share, void *sec,
                                         unsigned long disp, unsigned long flags, void *tmpl);
    static char textbuf[1200];
    unsigned short path[600];
    create_fn create = (create_fn)kernel32_fn("CreateFileW");
    void *f = (void *)-1;
    unsigned long n;
    int i;
    for (i = 0; i < 2 && f == (void *)-1; i++)
        if (stop_path(path, 600, i) && create) f = create(path, 0x80000000, 1, 0, 3, 0x80, 0);   /* GENERIC_READ, OPEN_EXISTING */
    if (f == (void *)-1) return;
    copy(textbuf, sizeof textbuf, "The last game was closed by the mod, and this was not shown then:\n\n", 0);
    n = length(textbuf);
    x360_read_text(f, textbuf + n, sizeof textbuf - n, 0);
    CloseHandle(f);
    { unsigned k; for (k = 0; k < 600 && (stop_file[k] = path[k]) != 0; k++) {} }
    x360_log("x360 load: a stop's text from the last game: shown at the title\r\n");
    black_screen(textbuf, 0, stop_seen);
}

static void side_panels_test(int off) {
    /* Both of CGameUI's side tests: 0x541090 (one side's panels, their virtual +0xc) and 0x541130
     * (the other's, +8); 0x549b70, both, calls them (QA run 10: with only the first patched the world
     * stayed narrowed, its strips the background's blue once the boards were hidden). */
    const unsigned tests[2] = {0x00541090, 0x00541130};
    static const unsigned char no[3] = {0x32, 0xc0, 0xc3};
    static unsigned char saved[2][3];
    static int patched;
    unsigned long old;
    unsigned t, i;
    if (off == patched || patched < 0) return;
    for (t = 0; t < 2; t++) {
        unsigned char *f = (unsigned char *)tests[t];
        if (off && !(f[0] == 0x53 && f[1] == 0x56 && f[2] == 0x8b)) { x360_log("x360 menu: a side-panel test not as expected, left\r\n"); patched = -1; return; }
    }
    for (t = 0; t < 2; t++) {
        unsigned char *f = (unsigned char *)tests[t];
        if (!VirtualProtect(f, 3, 0x40 /* PAGE_EXECUTE_READWRITE */, &old)) continue;
        if (off) { for (i = 0; i < 3; i++) { saved[t][i] = f[i]; f[i] = no[i]; } }
        else for (i = 0; i < 3; i++) f[i] = saved[t][i];
        VirtualProtect(f, 3, old, &old);
    }
    patched = off;
    x360_log(off ? "x360 menu: PC's side panels told closed (the world's viewport whole)\r\n"
                 : "x360 menu: PC's side panels test put back\r\n");
}

/* PC's own menus with the pad (the user, for the first beta): the main menu (mainmenuframe: Continue
 * Last, New Game, Continue Game, Settings, Exit Game; and the screens after it, character load and
 * create, difficulty) and, in play, PC's pause menu (optionsmenu: Settings, Exit Game, Return to
 * Game). Their visible buttons (PC's windows of a Button type, not the port's) in a circular list
 * from the top one down: up and down (the d-pad or the left stick) move the focus, shown as PC
 * shows a hover (the CEGUI cursor, hidden, put on the button's centre); A clicks it there. Not in
 * PC's settings screen (Apply, Cancel), used with the mouse, which shows there; hidden elsewhere. */
#define NAV_MAX 64
static void *nav_list[NAV_MAX], *nav_editing;
static int nav_count, nav_focus;
/* The windows the pad goes to, by their type's end: buttons, edit boxes (A clicks one: its caret,
 * the keyboard types the name), check boxes (difficulty's Hardcore), the character load's slots
 * (QA run 16), and the settings' sliders and drop-downs (left and right change them, QA run 17). */
enum { NAV_NONE, NAV_BUTTON, NAV_EDIT, NAV_CHECK, NAV_SLOT, NAV_SLIDER, NAV_COMBO };
static int nav_kind_of(const char *type) {
    static const char *const kinds[] = {"Button", "Editbox", "Checkbox", "CharacterSlot", "Slider", "Combobox"};
    unsigned q, k = type ? length(type) : 0;
    for (q = 0; q < 6; q++) {
        unsigned m = length(kinds[q]), c = 0;
        if (k < m) continue;
        while (c < m && type[k - m + c] == kinds[q][c]) c++;
        if (c == m) return (int)q + 1;
    }
    return NAV_NONE;
}
static int nav_kind(void *w) {
    typedef const void *(__fastcall *str_fn)(const void *self, void *edx);
    static str_fn get_type;
    if (!get_type) get_type = (str_fn)need("?getType@Window@CEGUI@@QBEABVString@2@XZ");
    return w && get_type ? nav_kind_of(c_str(get_type(w, 0), 0)) : NAV_NONE;
}
/* A window's rectangle in pixels; a drop-down's is its edit box's (CEGUI's Combobox spans its
 * list too: its centre was below, the pad passed it by, QA run 18). */
typedef struct { float top, bottom, left, right; } nav_rect_t;
static void nav_rect(void *w, void *out) {
    typedef nav_rect_t *(__fastcall *pixel_rect_fn6)(const void *self, void *edx, nav_rect_t *out);
    static pixel_rect_fn6 rect_of;
    nav_rect_t *r = (nav_rect_t *)out;
    if (!rect_of) rect_of = (pixel_rect_fn6)need("?getUnclippedPixelRect@Window@CEGUI@@QBE?AVRect@2@XZ");
    if (!rect_of) return;
    rect_of(w, 0, r);
    if (nav_kind(w) == NAV_COMBO) {
        unsigned c, n = child_count(w, 0);
        for (c = 0; c < n; c++) {
            void *e = child_at(w, 0, c);
            if (nav_kind(e) == NAV_EDIT) { rect_of(e, 0, r); break; }
        }
    }
}
/* Left or right (step -1, 1) on a slider (its click step, else a tenth) or a drop-down (the item
 * before or after, accepted as a click on it would: its text, PC's handler). */
static void nav_change(void *w, int kind, int step) {
    typedef float (__fastcall *getf_fn)(const void *self, void *edx);
    typedef void (__fastcall *setf_fn)(void *self, void *edx, float v);
    typedef unsigned (__fastcall *count_fn)(const void *self, void *edx);
    typedef void *(__fastcall *item_fn)(const void *self, void *edx);
    typedef unsigned (__fastcall *index_fn)(const void *self, void *edx, const void *item);
    typedef void (__fastcall *select_fn)(void *self, void *edx, unsigned i, unsigned char on);
    typedef void (__fastcall *args_fn)(void *self, void *edx, void *window);
    typedef void (__fastcall *accept_fn)(void *self, void *edx, void *args);
    static getf_fn cur, max, click;
    static setf_fn set;
    static count_fn count;
    static item_fn selected_fn;
    static index_fn index_of;
    static select_fn select;
    static args_fn args_ctor;
    static accept_fn accepted;
    typedef void *(__fastcall *item_at_fn)(const void *self, void *edx, unsigned i);
    typedef const void *(__fastcall *item_text_fn)(const void *self, void *edx);
    typedef void (__fastcall *set_text_fn)(void *self, void *edx, const void *text);
    static item_at_fn item_at;
    static item_text_fn item_text;
    static set_text_fn set_text;
    if (!cur) {
        cur = (getf_fn)need("?getCurrentValue@Slider@CEGUI@@QBEMXZ");
        max = (getf_fn)need("?getMaxValue@Slider@CEGUI@@QBEMXZ");
        click = (getf_fn)need("?getClickStep@Slider@CEGUI@@QBEMXZ");
        set = (setf_fn)need("?setCurrentValue@Slider@CEGUI@@QAEXM@Z");
        count = (count_fn)need("?getItemCount@Combobox@CEGUI@@QBEIXZ");
        selected_fn = (item_fn)need("?getSelectedItem@Combobox@CEGUI@@QBEPAVListboxItem@2@XZ");
        index_of = (index_fn)need("?getItemIndex@Combobox@CEGUI@@QBEIPBVListboxItem@2@@Z");
        select = (select_fn)need("?setItemSelectState@Combobox@CEGUI@@QAEXI_N@Z");
        args_ctor = (args_fn)need("??0WindowEventArgs@CEGUI@@QAE@PAVWindow@1@@Z");
        accepted = (accept_fn)need("?onListSelectionAccepted@Combobox@CEGUI@@MAEXAAVWindowEventArgs@2@@Z");
        item_at = (item_at_fn)need("?getListboxItemFromIndex@Combobox@CEGUI@@QBEPAVListboxItem@2@I@Z");
        item_text = (item_text_fn)need("?getText@ListboxItem@CEGUI@@QBEABVString@2@XZ");
        set_text = (set_text_fn)need("?setText@Window@CEGUI@@QAEXABVString@2@@Z");
    }
    if (kind == NAV_SLIDER && cur && max && click && set) {
        float m = max(w, 0), d = click(w, 0), v;
        if (d <= 0.0f) d = m * 0.1f;
        v = cur(w, 0) + d * (float)step;
        set(w, 0, v < 0.0f ? 0.0f : v > m ? m : v);
    } else if (kind == NAV_COMBO && count && selected_fn && index_of && select && args_ctor && accepted) {
        unsigned n = count(w, 0), i;
        void *item = selected_fn(w, 0);
        unsigned char args[32];
        if (!n) return;
        i = item ? index_of(w, 0, item) : 0;
        if (step < 0 && i == 0) return;
        if (step > 0 && i + 1 >= n) return;
        select(w, 0, i + step, 1);
        /* Its text as the list's click sets it (setItemSelectState alone leaves the box's text). */
        if (item_at && item_text && set_text && (item = item_at(w, 0, i + step)) != 0) set_text(w, 0, item_text(item, 0));
        args_ctor(args, 0, w);
        accepted(w, 0, args);
    }
}
/* Nintendo's layout's checkbox in PC's settings screen (the owner: as the recomp's, enabled only
 * with a Nintendo pad found and not behind Steam Input; its text says it needs a restart). Made
 * once per screen in the free row under the resolution's, of PC's own checkbox type, in its parent; its
 * state saved to x360_settings.ini when changed (proxy/setup.c). */
int x360_swap_setting(void);
void x360_swap_setting_write(int on);
int x360_pad_is_nintendo(void);
int x360_pad_steam_input(void);
static void swap_checkbox_tick(const char *prefix) {
    typedef void *(__fastcall *parent_fn)(const void *self, void *edx);
    typedef unsigned char (__fastcall *is_sel_fn)(const void *self, void *edx);
    typedef void (__fastcall *set_sel_fn)(void *self, void *edx, unsigned char on);
    typedef void (__fastcall *set_en_fn)(void *self, void *edx, unsigned char on);
    static parent_fn parent_of;
    static is_sel_fn is_sel;
    static set_sel_fn set_sel;
    static set_en_fn set_en;
    static int last_state = -1, enabled;
    /* Placed in the free row between the resolution's and the music's (QA run 41: below the last
     * checkbox it covered one, the rows there being side by side): the second row's checkbox moved
     * down twice the rows' step. */
    void *box = win_of(prefix, "x360SwapAB"), *last = win_of(prefix, "Rimlights"), *above = win_of(prefix, "Fullscreen");
    if (!parent_of) parent_of = (parent_fn)need("?getParent@Window@CEGUI@@QBEPAV12@XZ");
    if (!is_sel) is_sel = (is_sel_fn)need("?isSelected@Checkbox@CEGUI@@QBE_NXZ");
    if (!set_sel) set_sel = (set_sel_fn)need("?setSelected@Checkbox@CEGUI@@QAEX_N@Z");
    if (!set_en) set_en = (set_en_fn)need("?setEnabled@Window@CEGUI@@QAEX_N@Z");
    if (!parent_of || !is_sel || !set_sel || !set_en || !last) return;
    if (!box) {
        char a[160], b[160];
        float va[8], vb[8];
        void *parent = parent_of(last, 0);
        if (!parent || !(box = create_window(prefix, "x360SwapAB", "GuiLook/Checkbox"))) {
            x360_log("x360 prompts: the settings' checkbox not made\r\n");
            return;
        }
        add_child(parent, 0, box);
        read_prop(last, "UnifiedAreaRect", a, sizeof a);
        read_prop(above ? above : last, "UnifiedAreaRect", b, sizeof b);
        if (parse_rect(a, va) && parse_rect(b, vb)) {
            int i;
            float step[8];
            for (i = 0; i < 8; i++) step[i] = va[i] - vb[i];
            for (i = 0; i < 8; i++) if (i == 2 || i == 3 || i == 6 || i == 7) va[i] += 2.0f * step[i];   /* two rows further down */
            write_rect(a, va);
            prop(box, "UnifiedAreaRect", a);
        }
        enabled = x360_pad_is_nintendo();
        text(box, x360_pad_steam_input() ? "Nintendo layout (set it in Steam Input)" : "Nintendo layout (needs a restart)");
        set_sel(box, 0, (unsigned char)(x360_swap_setting() != 0));
        set_en(box, 0, (unsigned char)enabled);
        if (!enabled) prop(box, "Alpha", "0.5");
        last_state = x360_swap_setting() != 0;
        x360_log(enabled ? "x360 prompts: the settings' checkbox made\r\n" : "x360 prompts: the settings' checkbox made, disabled (no Nintendo pad)\r\n");
        return;
    }
    if (enabled && (int)(is_sel(box, 0) != 0) != last_state) {
        last_state = is_sel(box, 0) != 0;
        x360_swap_setting_write(last_state);
    }
}

static void nav_collect(void *w, int depth, int only_under_pause) {
    typedef const void *(__fastcall *str_fn)(const void *self, void *edx);
    typedef unsigned char (__fastcall *vis_fn)(const void *self, void *edx, unsigned char local);
    static str_fn get_type, gn;
    static vis_fn vis;
    unsigned i, n;
    const char *nm, *type;
    if (!w || depth > 30 || nav_count >= NAV_MAX) return;
    if (!get_type) get_type = (str_fn)need("?getType@Window@CEGUI@@QBEABVString@2@XZ");
    if (!gn) gn = (str_fn)need("?getName@Window@CEGUI@@QBEABVString@2@XZ");
    if (!vis) vis = (vis_fn)need("?isVisible@Window@CEGUI@@QBE_N_N@Z");
    if (!get_type || !gn || !vis) return;
    nm = c_str(gn(w, 0), 0);
    if (nm && nm[0] == 'x' && nm[1] == '3' && nm[2] == '6' && nm[3] == '0') return;   /* the port's */
    /* Out of play, not PC's in-game sheet: its HUD buttons stay visible while a game loads (QA run
     * 14: 19 of them listed for a moment). */
    if (!only_under_pause && nm && nm[0] == 'I' && nm[1] == 'n' && nm[2] == 'g' && nm[3] == 'a' && nm[4] == 'm' && nm[5] == 'e' && nm[6] == ' ') return;
    if (!vis(w, 0, 1)) return;   /* hidden here: nothing under it shows */
    type = c_str(get_type(w, 0), 0);
    if (type && depth > 0) {
        unsigned k = length(type);
        int kind = nav_kind_of(type);
        (void)k;
        if (kind) {
            nav_list[nav_count++] = w;
            /* A slot holds windows too: the load's DeleteConfirm, its DeleteConfirmButton and Cancel. */
            if (kind != NAV_SLOT) return;
        }
    }
    n = child_count(w, 0);
    for (i = 0; i < n; i++) nav_collect(child_at(w, 0, i), depth + 1, only_under_pause);
}
static int pc_pause_open(void);
static unsigned char *pc_pause_menu(void);
int x360_pad_ly(void);
int x360_pad_lx(void);
/* PC reads Windows' cursor each frame (0x4e7330: GetCursorPos, ScreenToClient on its window) and
 * injects it into CEGUI, then a 1 px move (0x4016c0) for the hover: a position injected alone lasts
 * one frame (QA run 12). So Windows' cursor is put on (x, y), the window's client pixels, and PC
 * hovers the button there itself; CEGUI is told the same at once for a click in this frame. */
/* CEGUI's tooltip (System's default__auto_tooltip__, shown over PC's character slots and classes):
 * the mouse leaves a window over the screen's background, which takes the tooltip off (its
 * target none, fading with its text); the pad's cursor jumps from window to window, so the tooltip
 * went with it onto one without a text (Back) and its empty frame showed while it faded (the
 * owner, 2026-10-25). Before the cursor moves, a shown tooltip is made inactive at once (hidden,
 * no target): the next window's own shows after the hover time, as with the mouse. */
static void nav_tooltip_off(void) {
    typedef void *(__fastcall *tip_fn)(const void *self, void *edx);
    typedef void (__fastcall *target_fn)(void *self, void *edx, void *w);
    typedef void (__fastcall *self_fn)(void *self, void *edx);
    typedef unsigned char (__fastcall *vis_fn)(const void *self, void *edx, unsigned char local);
    static tip_fn get_tip;
    static target_fn set_target;
    static self_fn inactive;
    static vis_fn vis;
    void *sys = system_ptr(), *tip;
    if (!get_tip) {
        get_tip = (tip_fn)need("?getDefaultTooltip@System@CEGUI@@QBEPAVTooltip@2@XZ");
        set_target = (target_fn)need("?setTargetWindow@Tooltip@CEGUI@@QAEXPAVWindow@2@@Z");
        inactive = (self_fn)need("?switchToInactiveState@Tooltip@CEGUI@@IAEXXZ");
        vis = (vis_fn)need("?isVisible@Window@CEGUI@@QBE_N_N@Z");
    }
    if (!sys || !get_tip || !set_target || !inactive || !vis) return;
    tip = get_tip(sys, 0);
    if (!tip || !vis(tip, 0, 1)) return;
    set_target(tip, 0, 0);
    inactive(tip, 0);
}
static void nav_point(float x, float y) {
    void x360_inject_left(int down, float x, float y);
    HWND w = GetActiveWindow();
    long pt[2];
    __try { nav_tooltip_off(); } __except (1) { x360_log("x360 menu: exception taking CEGUI's tooltip off\r\n"); }
    pt[0] = (long)x; pt[1] = (long)y;
    if (w && ClientToScreen(w, pt)) SetCursorPos((int)pt[0], (int)pt[1]);
    x360_inject_left(-1, x, y);
}
/* A click on PC's window at (x, y): as the mouse's, a WM_LBUTTONDOWN and WM_LBUTTONUP to the game's
 * window, whose handler (CGameUI's, GOG 0x53ff90: injectMouseButtonDown and Up, the game's only
 * calls of them) injects it into CEGUI as the messages are pumped, before the frame's update.
 * Injected from the frame listener instead, PC's main menu closed (CMainMenu's onClick, its
 * setOpen(false) taking its root off at once *(OT)*) and the frame was drawn before the next
 * screen opened: its title and bottom bar gone for an instant (the owner, 2026-10-25). Injected
 * here only when the game's window is not the active one or the press cannot be posted. */
static void nav_click(float x, float y) {
    __declspec(dllimport) int __stdcall PostMessageW(HWND w, unsigned msg, unsigned wparam, long lparam);
    void x360_inject_left(int down, float x, float y);
    HWND w = GetActiveWindow();
    long lp = ((long)y & 0xffff) << 16 | ((long)x & 0xffff);
    nav_point(x, y);
    if (w && PostMessageW(w, 0x0201 /* WM_LBUTTONDOWN */, 1 /* MK_LBUTTON */, lp)) {
        if (!PostMessageW(w, 0x0202 /* WM_LBUTTONUP */, 0, lp)) PostMessageW(w, 0x0202, 0, lp);   /* never a press left held */
        return;
    }
    x360_inject_left(1, x, y);
    x360_inject_left(0, x, y);
}
/* Windows' cursor, PC's own (CEGUI's is not the one shown): PC's 0x547ca0 (thiscall) sets it, as
 * the mouse moves, to its resource cursors 0x6a-0x6d (SetCursor(LoadCursorW(GetModuleHandleW(0),
 * id)) by its +0x106c), or none when its +0x60 is set. Hooked: none either while nav_cursor_off
 * (QA run 14: hiding CEGUI's cursor left this one shown). The hook's first 7 bytes (push esi;
 * mov esi,ecx; cmp dword [esi+0x60],0) run from a copy, then its je onwards in place. */
static volatile int nav_cursor_off;
static unsigned char cursor_tramp[16];
static void __fastcall cursor_hook(void *self) {
    typedef void (__fastcall *orig_fn)(void *self);
    if (nav_cursor_off) { SetCursor(0); return; }
    ((orig_fn)(void *)cursor_tramp)(self);
}
static void cursor_hook_install(void) {
    static const unsigned char want[7] = {0x56, 0x8b, 0xf1, 0x83, 0x7e, 0x60, 0x00};
    static int done;
    unsigned char *f = (unsigned char *)0x00547ca0;
    unsigned long old;
    unsigned i;
    if (done) return;
    done = 1;
    for (i = 0; i < 7; i++) if (f[i] != want[i]) { x360_log("x360 menu: PC's cursor function not as expected, left\r\n"); return; }
    for (i = 0; i < 7; i++) cursor_tramp[i] = f[i];
    cursor_tramp[7] = 0xe9;
    *(int *)(cursor_tramp + 8) = (int)(f + 7) - (int)(cursor_tramp + 12);
    if (!VirtualProtect(cursor_tramp, sizeof cursor_tramp, 0x40, &old)) return;
    if (!VirtualProtect(f, 7, 0x40, &old)) return;
    f[0] = 0xe9;
    *(int *)(f + 1) = (int)(void *)cursor_hook - (int)(f + 5);
    f[5] = f[6] = 0x90;
    VirtualProtect(f, 7, old, &old);
    x360_log("x360 menu: PC's cursor hooked\r\n");
}
static void pc_menus_nav_tick(void) {
    typedef struct { float top, bottom, left, right; } rect6;
    typedef rect6 *(__fastcall *pixel_rect_fn6)(const void *self, void *edx, rect6 *out);
    typedef void (__fastcall *self_fn)(void *self, void *edx);
    typedef void *(__cdecl *single_fn)(void);
    void x360_inject_left(int down, float x, float y);
    int x360_in_play(void);
    static pixel_rect_fn6 rect_of;
    typedef unsigned char (__fastcall *cvis_fn)(const void *self, void *edx);
    static self_fn cursor_hide, cursor_show;
    static cvis_fn cursor_vis;
    static single_fn cursor_get;
    static unsigned prev, last_collect, next_repeat, last_search;
    static int held_dir, settings_shown = -1;
    static char settings_prefix[64];
    static const char *const settings_needs[] = {"Cancel", 0};
    static char create_prefix[64];
    static const char *const create_needs[] = {"Vanquisher", 0};
    static char die_prefix[64];
    static const char *const die_needs[] = {"Resurrect2", 0};
    static char wp_prefix[64];
    static const char *const wp_needs[] = {"Button10", "Cancel", 0};
    int create = 0, die = 0, wp = 0;
    unsigned buttons = x360_pad_buttons(), pressed = buttons & ~prev, now = GetTickCount();
    int in_play = x360_in_play(), settings = 0, dir = 0, i, j;
    void *sys = system_ptr(), *sheet = sys ? get_gui_sheet(sys, 0) : 0;
    rect6 r[NAV_MAX];
    prev = buttons;
    if (!rect_of) rect_of = (pixel_rect_fn6)need("?getUnclippedPixelRect@Window@CEGUI@@QBE?AVRect@2@XZ");
    if (!cursor_get) {
        cursor_get = (single_fn)need("?getSingletonPtr@?$Singleton@VMouseCursor@CEGUI@@@CEGUI@@SAPAVMouseCursor@2@XZ");
        cursor_hide = (self_fn)need("?hide@MouseCursor@CEGUI@@QAEXXZ");
        cursor_show = (self_fn)need("?show@MouseCursor@CEGUI@@QAEXXZ");
        cursor_vis = (cvis_fn)need("?isVisible@MouseCursor@CEGUI@@QBE_NXZ");
    }
    if (!sheet || !rect_of) return;
    /* PC's settings screen: its Apply and Cancel shown. */
    /* The searches walk PC's whole GUI: every 250 ms at most (the review); the checks each frame. */
    if (now - last_search > 250 && (!settings_prefix[0] || !win_of(settings_prefix, "Apply") || !create_prefix[0] || !win_of(create_prefix, "Destroyer") ||
                                    !die_prefix[0] || !win_of(die_prefix, "Resurrect1") || !wp_prefix[0] || !win_of(wp_prefix, "Choice10"))) {
        last_search = now;
        if (!settings_prefix[0] || !win_of(settings_prefix, "Apply")) find_pc_by_suffix("Apply", settings_needs, settings_prefix, sizeof settings_prefix);
        if (!create_prefix[0] || !win_of(create_prefix, "Destroyer")) find_pc_by_suffix("Destroyer", create_needs, create_prefix, sizeof create_prefix);
        if (!die_prefix[0] || !win_of(die_prefix, "Resurrect1")) find_pc_by_suffix("Resurrect1", die_needs, die_prefix, sizeof die_prefix);
        if (!wp_prefix[0] || !win_of(wp_prefix, "Choice10")) find_pc_by_suffix("Choice10", wp_needs, wp_prefix, sizeof wp_prefix);
    }
    /* Attached to the sheet, not only visible: PC closes it by taking it off (QA run 13: the mouse
     * stayed shown and the pad off after Settings was left). */
    settings = settings_prefix[0] && pc_attached(settings_prefix, "Apply");
    /* And PC's character creation (its name is typed): the mouse shown there too (the user). Told by
     * its Destroyer, always there (its CreatePlayer shows once a name is typed: QA run 16). */
    create = create_prefix[0] && pc_attached(create_prefix, "Destroyer");
    /* PC's resurrection screen (diemenu.layout: Resurrect1-3, ExitGame), shown when the hero dies:
     * its buttons for the pad as the pause menu's (the owner's run of 2026-10-24: the pad could not
     * leave it; the 360's own screen is not ported). Told by ExitGame, shown whatever choices it
     * offers; not while a screen of the 360's is over it. The play pad gets nothing meanwhile
     * (x360_pc_menu_up: no move, skill, pause or inventory). */
    die = in_play && die_prefix[0] && pc_attached(die_prefix, "ExitGame") && !x360_pause_open() && !inventory_open && !x360_overlay_open();
    /* PC's waypoint portals (waypointmenu.layout: Frame, Button1-10 with Choice1-10, Cancel), opened
     * at a waypoint once the first boss is dead: PC's screen with the pad, as the resurrection's
     * (the owner, 2026-10-24: kept PC's for now; the pad could not move in it). B its Cancel. */
    wp = !die && in_play && wp_prefix[0] && pc_attached(wp_prefix, "Cancel") && !x360_pause_open() && !inventory_open && !x360_overlay_open();
    x360_pc_die_up = die || wp;
    x360_pc_settings_up = settings;
    if (settings) __try { swap_checkbox_tick(settings_prefix); } __except (1) { x360_log("x360 prompts: exception in the settings' checkbox\r\n"); }
    cursor_hook_install();
    if (nav_cursor_off != !(settings || create)) {
        nav_cursor_off = !(settings || create);
        x360_log(nav_cursor_off ? "x360 menu: Windows' cursor off\r\n" : "x360 menu: Windows' cursor on\r\n");
        if (nav_cursor_off) SetCursor(0);
    }
    if (cursor_get && cursor_hide && cursor_show && cursor_vis && cursor_get()) {
        /* Kept so each frame: PC shows its cursor again on its own. */
        if ((cursor_vis(cursor_get(), 0) != 0) != (settings || create)) (settings || create ? cursor_show : cursor_hide)(cursor_get(), 0);
        if (settings != settings_shown) {
            settings_shown = settings;
            x360_log(settings ? "x360 menu: the mouse shown (PC's settings)\r\n" : "x360 menu: the mouse hidden\r\n");
        }
    }
    if (in_play && !pc_pause_open() && !settings && !die && !wp) { nav_count = 0; return; }
    /* Collected every frame: no pointer kept from one frame to the next (the review: a window
     * destroyed meanwhile, as a load ends, was used). */
    (void)last_collect;
    {
        void *was = nav_count > 0 && nav_focus < nav_count ? nav_list[nav_focus] : 0;
        int n_before = nav_count;
        last_collect = now;
        nav_count = 0;
        /* In play, PC's pause menu's own windows only (CMenu's root at +0xc): PC's HUD buttons are
         * visible there, made transparent. */
        if (settings) {
            /* PC's settings' own windows: from Apply up to its last ancestor of its prefix (in play
             * it is on PC's in-game sheet, with the HUD's buttons). */
            typedef void *(__fastcall *parent_fn)(const void *self, void *edx);
            typedef const void *(__fastcall *str_fn)(const void *self, void *edx);
            static parent_fn gp;
            static str_fn gn;
            void *top = win_of(settings_prefix, "Apply"), *up;
            unsigned pl = length(settings_prefix), c;
            if (!gp) gp = (parent_fn)need("?getParent@Window@CEGUI@@QBEPAV12@XZ");
            if (!gn) gn = (str_fn)need("?getName@Window@CEGUI@@QBEABVString@2@XZ");
            while (top && gp && gn && (up = gp(top, 0)) != 0) {
                const char *un = c_str(gn(up, 0), 0);
                for (c = 0; c < pl && un[c] == settings_prefix[c]; c++) {}
                if (c < pl) break;
                top = up;
            }
            if (top) nav_collect(top, 0, 1);
        } else if (die || wp) {
            nav_collect(win_of(die ? die_prefix : wp_prefix, "Frame"), 0, 1);   /* its own windows only: PC's HUD buttons are on its sheet */
        } else if (in_play) {
            static unsigned char *logged_pm;
            unsigned char *pm = pc_pause_menu();
            if (pm && *(void **)(pm + 0xc)) nav_collect(*(void **)(pm + 0xc), 0, 1);
            if (!nav_count && pm != logged_pm) {
                logged_pm = pm;
                x360_log("x360 menu: no button found in PC's pause menu, its root ");
                x360_log_num(pm ? (long)*(void **)(pm + 0xc) : -1); x360_log("\r\n");
            }
        } else nav_collect(sheet, 0, 0);
        /* A popup over the screen (a slot holding windows of the list: the load's DeleteConfirm):
         * only its windows while it is open (QA run 17). */
        {
            typedef unsigned char (__fastcall *anc_fn)(const void *self, void *edx, const void *w);
            static anc_fn is_under;
            if (!is_under) is_under = (anc_fn)need("?isAncestor@Window@CEGUI@@QBE_NPBV12@@Z");
            for (i = 0; i < nav_count && is_under; i++) {
                int under = 0, keep = 0;
                for (j = 0; j < nav_count; j++) if (j != i && is_under(nav_list[j], 0, nav_list[i])) under++;
                if (!under) continue;
                for (j = 0; j < nav_count; j++) if (j != i && is_under(nav_list[j], 0, nav_list[i])) nav_list[keep++] = nav_list[j];
                nav_count = keep;
                break;
            }
        }
        /* From the top one down, then left to right. */
        for (i = 0; i < nav_count; i++) nav_rect(nav_list[i], &r[i]);
        for (i = 1; i < nav_count; i++)
            for (j = i; j > 0 && (r[j].top < r[j - 1].top - 4.0f || (r[j].top < r[j - 1].top + 4.0f && r[j].left < r[j - 1].left)); j--) {
                rect6 t = r[j]; void *tw = nav_list[j];
                r[j] = r[j - 1]; nav_list[j] = nav_list[j - 1];
                r[j - 1] = t; nav_list[j - 1] = tw;
            }
        int found = 0;
        nav_focus = 0;
        for (i = 0; i < nav_count; i++) if (nav_list[i] == was) { nav_focus = i; found = 1; }
        if (!in_play && n_before > 0 && nav_count == 0) load_armed = 1;   /* a menu gave way: the player's load (the load hook) */
        else if (nav_count > 0 && !in_play) load_armed = 0;
        if (nav_count != n_before) {
            typedef const void *(__fastcall *str_fn)(const void *self, void *edx);
            static str_fn gn;
            if (!gn) gn = (str_fn)need("?getName@Window@CEGUI@@QBEABVString@2@XZ");
            x360_log("x360 menu: PC's buttons for the pad "); x360_log_num(nav_count);
            x360_log(die ? " (resurrection):" : wp ? " (waypoints):" : in_play ? " (pause):" : ":");
            for (i = 0; i < nav_count && gn; i++) { x360_log(" "); x360_log(c_str(gn(nav_list[i], 0), 0)); }
            x360_log("\r\n");
        }
        /* A new screen (the focused window gone): its first, the cursor on it; not on the screens
         * used with the mouse, where the user moves it. */
        if (nav_count && !found && !(settings || create)) {
            nav_point((r[nav_focus].left + r[nav_focus].right) * 0.5f, (r[nav_focus].top + r[nav_focus].bottom) * 0.5f);
        }
    }
    if (!nav_count) { nav_editing = 0; return; }
    /* A, B, X, Y: the settings' (in play); PC's pause menu's B stays x360_pause_pad's. */
    if (settings) x360_pad_eaten |= pressed & (0x1000 | 0x2000 | 0x4000 | 0x8000);
    {
        /* A direction (1 up, 2 down, 3 left, 4 right): the d-pad, or the left stick held (its
         * stronger axis, repeating). */
        int ly = x360_pad_ly(), lx = x360_pad_lx();
        int stick = (ly > 500 || ly < -500 || lx > 500 || lx < -500)
                        ? ((ly < 0 ? -ly : ly) >= (lx < 0 ? -lx : lx) ? (ly > 0 ? 1 : 2) : (lx < 0 ? 3 : 4)) : 0;
        if (pressed & 0x0001) dir = 1;
        else if (pressed & 0x0002) dir = 2;
        else if (pressed & 0x0004) dir = 3;
        else if (pressed & 0x0008) dir = 4;
        else if (stick && (stick != held_dir || (int)(now - next_repeat) >= 0)) { dir = stick; next_repeat = now + (stick != held_dir ? 400 : 180); }
        held_dir = stick;
    }
    /* A slider or a drop-down is changed after an A on it (left and right then change it; A, B, up
     * or down end that): the d-pad otherwise moves on past it (QA run 18: left and right always
     * changing it trapped the focus there). */
    if (nav_editing && (nav_editing != nav_list[nav_focus] || (pressed & (0x1000 | 0x2000)) || dir == 1 || dir == 2)) {
        pressed &= ~(0x1000u | 0x2000u);   /* that A or B ends the change only */
        nav_editing = 0;
        x360_log("x360 menu: changing a setting ended\r\n");
    } else if (nav_editing && (dir == 3 || dir == 4)) {
        nav_change(nav_list[nav_focus], nav_kind(nav_list[nav_focus]), dir == 3 ? -1 : 1);
        dir = 0;
    } else if ((pressed & 0x1000) && (nav_kind(nav_list[nav_focus]) == NAV_SLIDER || nav_kind(nav_list[nav_focus]) == NAV_COMBO)) {
        nav_editing = nav_list[nav_focus];
        pressed &= ~0x1000u;
        x360_log("x360 menu: changing a setting (left, right)\r\n");
    }
    if (dir) {
        /* The nearest window that way (its centre's distance along it, plus twice the distance across:
         * PC's main menu is a row, its pause menu a column, its character creation two columns and its
         * name boxes, QA runs 13-15); none that way, the farthest the other way on the same line
         * (around). */
        float fx = (r[nav_focus].left + r[nav_focus].right) * 0.5f, fy = (r[nav_focus].top + r[nav_focus].bottom) * 0.5f;
        float best = 1e30f, wrap = -1e30f;
        int pick = -1, around = -1;
        for (i = 0; i < nav_count; i++) {
            float x = (r[i].left + r[i].right) * 0.5f - fx, y = (r[i].top + r[i].bottom) * 0.5f - fy;
            float along = dir == 1 ? -y : dir == 2 ? y : dir == 3 ? -x : x;
            float across = dir <= 2 ? (x < 0 ? -x : x) : (y < 0 ? -y : y);
            if (i == nav_focus) continue;
            if (along > 4.0f) { if (along + 2.0f * across < best) { best = along + 2.0f * across; pick = i; } }
            else if (along < -4.0f && across < 40.0f && -along - 2.0f * across > wrap) { wrap = -along - 2.0f * across; around = i; }
        }
        if (pick < 0) pick = around;
        if (pick >= 0) {
            float x = (r[pick].left + r[pick].right) * 0.5f, y = (r[pick].top + r[pick].bottom) * 0.5f;
            nav_focus = pick;
            nav_point(x, y);
            /* A character slot is selected as the focus comes (its highlight, PC's own, shows it; as
             * the 360's list, where moving selects). */
            if (nav_kind(nav_list[pick]) == NAV_SLOT) nav_click(x, y);
        }
    }
    if ((pressed & 0x2000) && !nav_editing && !(in_play && !settings && !wp)) {
        /* B: the screen's way back, its Cancel (settings, the delete popup) or Back (creation,
         * difficulty, load), clicked (the user, QA run 19). In play PC's pause menu is closed by
         * x360_pause_pad. */
        typedef const void *(__fastcall *str_fn)(const void *self, void *edx);
        static str_fn gn;
        static const char *const backs[] = {"Cancel", "Back"};
        int b, hit = -1;
        if (!gn) gn = (str_fn)need("?getName@Window@CEGUI@@QBEABVString@2@XZ");
        for (b = 0; b < 2 && hit < 0 && gn; b++)
            for (i = 0; i < nav_count && hit < 0; i++) {
                const char *nm = c_str(gn(nav_list[i], 0), 0);
                unsigned nl = length(nm), bl = length(backs[b]), c = 0;
                if (nl < bl) continue;
                while (c < bl && nm[nl - bl + c] == backs[b][c]) c++;
                if (c == bl && (nl == bl || nm[nl - bl - 1] == '_')) hit = i;
            }
        if (hit >= 0) {
            float x = (r[hit].left + r[hit].right) * 0.5f, y = (r[hit].top + r[hit].bottom) * 0.5f;
            nav_click(x, y);
            x360_log("x360 menu: B on PC's "); x360_log(c_str(gn(nav_list[hit], 0), 0)); x360_log("\r\n");
        }
    }
    if ((pressed & 0x1000) && nav_kind(nav_list[nav_focus]) != NAV_SLIDER && nav_kind(nav_list[nav_focus]) != NAV_COMBO) {
        float x = (r[nav_focus].left + r[nav_focus].right) * 0.5f, y = (r[nav_focus].top + r[nav_focus].bottom) * 0.5f;
        nav_click(x, y);
        last_collect = 0;   /* the next screen's buttons */
        x360_log("x360 menu: A on PC's button\r\n");
    }
}

unsigned x360_frames;   /* the game's frames, counted by the frame listener (proxy/pad.c: one destination a frame) */
void x360_frame_tick(void) {
    static int logged, preloaded;
    void x360_pad_frame(void);
    x360_frames++;
    x360_pad_frame();   /* pad_trace: the frame's time (proxy/pad.c) */
    __try { black_screen_tick(); }
    __except (1) {
        bs_state = 0; bs_failed = 1;
        x360_log("x360: exception on the black screen\r\n");
        __try { if (bs_back) set_visible(bs_back, 0, 0); } __except (1) {}   /* never a screen left over the game */
    }
    __try { pc_menus_nav_tick(); } __except (1) { x360_log("x360 menu: exception in PC's menus' pad\r\n"); }
    if (!find_world()) return;
    minimap_tick();   /* the corner minimap where the 360 draws it, before this frame is drawn */
    /* The frame's layouts loaded as the game is entered, once the 360's HUD is up (it is on the
     * in-game sheet), not at the first Back (run B2: the user asked for no wait there). */
    if (!preloaded && hud_root) {
        preloaded = 1;
        __try { frame_load(); } __except (1) { x360_log("x360 menu: exception loading the frame\r\n"); }
    }
    if (dump_after_at && (int)(GetTickCount() - dump_after_at) >= 0) {
        void x360_ui_dump_now(const char *name);
        dump_after_at = 0;
        x360_ui_dump_now(dump_after_name);
        x360_log("x360: dumped the UI after a talk\r\n");
    }
    x360_ui_dump_poll();                     /* each frame: the HUD's tick, which polls it too, stops under a dialog or without input */
    __try { hotkeys_off(); } __except (1) { x360_log("x360 menu: exception turning PC's hotkeys off\r\n"); }
    __try { keys_off(); } __except (1) { x360_log("x360 menu: exception swallowing PC's menu keys\r\n"); }
    __try { load_aside_tick(); } __except (1) { x360_log("x360 load: exception placing the items kept aside\r\n"); }
    __try { stop_tick(); } __except (1) { x360_log("x360 load: exception in the stop\r\n"); }
    __try { notice_tick(); } __except (1) { x360_log("x360: exception showing a notice\r\n"); }
    __try { pc_pause_tick(); } __except (1) { x360_log("x360 menu: exception in PC's pause menu\r\n"); }
    __try { pc_menu_tick(); } __except (1) { x360_pc_menu_request = 0; x360_log("x360 menu: exception opening PC's pause menu\r\n"); }
    __try { boards_tick(); } __except (1) { x360_log("x360: exception hiding PC's boards\r\n"); }
    __try { pc_bars_off(); } __except (1) { x360_log("x360 bars: exception hiding PC's\r\n"); }
    {
        /* While the 360's shop frame is open, and 2 s after it closes: PC's merchant still closing
         * narrowed the world, its strips blue with the boards hidden for good (the owner's run of
         * 2026-10-24). Not always: the same tests hold PC's game-state changes back while a side
         * panel is open (its client update, 0x41ba36), which must keep PC's rule (the review). */
        static unsigned last_at_shop;   /* the last frame with the shop frame open (0: none yet) */
        int at_shop = inventory_open && shop;
        if (at_shop) last_at_shop = GetTickCount() | 1;
        else if (last_at_shop && GetTickCount() - last_at_shop > 2000) last_at_shop = 0;
        /* By the last frame at the shop, not by a change seen once (the owner's run: given back at
         * once as a shop closed, the world's width moved, two blue columns). */
        /* Not given back out of play: there PC's menus are not updated and a closing one (the
         * enchant menu and the inventory after the retirement, which leaves play at once)
         * never ends, so the tests held PC's next state, the descendant's difficulty screen,
         * forever (the owner's runs; the panels logged not closed: CInventoryMenu, CEnchantMenu).
         * Given back once in play again, where their closing ends. */
        if (hud_in_play || last_at_shop)
        __try { side_panels_test(last_at_shop != 0); } __except (1) { x360_log("x360 menu: exception in the side panels' test\r\n"); }
    }
    __try { pc_menus_guard(); } __except (1) { x360_log("x360 menu: exception in the PC menus' guard\r\n"); }
    __try { labels_tick(); } __except (1) { x360_log("x360 labels: exception\r\n"); }
    /* The tip first: on the frame it shows, the screens under it already leave it the pad. */
    __try { tip_tick(); } __except (1) { x360_log("x360 tip: exception\r\n"); }
    __try { fishing_tick(); } __except (1) { x360_log("x360 fishing: exception\r\n"); }
    __try { cinematic_bar_tick(); } __except (1) { x360_log("x360 cinematic: exception\r\n"); }
    __try { enchant_result_tick(); } __except (1) { ench_pending = 0; x360_log("x360 enchanter: exception taking the result\r\n"); }
    __try { modal_tick(); } __except (1) { x360_log("x360 modal: exception\r\n"); }
    __try { bars_tick(); } __except (1) { x360_log("x360 bars: exception\r\n"); }
    __try { dialog_tick(); }
    __except (1) { if (logged < 5) { x360_log("x360 dialog: exception in the frame tick\r\n"); logged++; } }
}
