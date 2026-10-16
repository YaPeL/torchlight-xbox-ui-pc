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
FARPROC x360_need(const char *name);
__declspec(dllimport) HWND __stdcall GetActiveWindow(void);
__declspec(dllimport) int __stdcall GetClientRect(HWND w, RECT *r);
__declspec(dllimport) unsigned long __stdcall GetTickCount(void);
__declspec(dllimport) void *__stdcall GetModuleHandleA(const char *name);
__declspec(dllimport) void *__stdcall GetProcAddress(void *module, const char *name);

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
    for (i = 0; i < n && hidden_count < MAX_HIDDEN; i++) {
        void *w = child_at(sheet, 0, i);
        if (is_ours(w) || !is_visible(w, 0, 1)) continue;
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
#define GOG_SELL_PRICE 0x004b3a90        /* CEquipment::sellPrice() */
#define GOG_DPS 0x004b8f10               /* CEquipment::DPS(), an int (fistp into eax) */
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

static unsigned char *hero, *level, *ui;

static int find_world(void) {
    unsigned char *app = *(unsigned char **)GOG_APP_GLOBAL;
    unsigned char *client = app ? *(unsigned char **)(app + 0x64) : 0;
    hero = client ? *(unsigned char **)(client + 0x2c) : 0;
    level = client ? *(unsigned char **)(client + 0x38) : 0;
    /* CGameUI: the client's +0x3c (its hover code, GOG 0x41b56b, calls CGameUI's methods on it);
     * its hero is at +0x1c (useItem reads it there). Checked before use. */
    ui = client ? *(unsigned char **)(client + 0x3c) : 0;
    if (ui && *(unsigned char **)(ui + 0x1c) != hero) ui = 0;
    return hero && level;
}

static int isa(void *unit, int type) { return ((isa_fn)GOG_ISA)(unit, 0, type) != 0; }

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

static unsigned char *cur_owner;             /* the bag's owner: the hero, or the pet */
static void *inventory(void) {
    unsigned char *owner = cur_owner ? cur_owner : hero;
    return owner ? *(void **)(owner + GOG_INVENTORY) : 0;
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
    for (pass = 0; pass < 2; pass++) {
        for (i = n; i-- > 0;) {
            unsigned char *ref = (unsigned char *)refs[i];
            void *item;
            int slot, c;
            if (!ref || !(item = *(void **)(ref + REF_ITEM))) continue;
            slot = *(int *)(ref + REF_SLOT);
            if ((slot < EQUIPPED_SLOTS) != (pass == 0)) continue;
            c = category(item);
            if (c != filter && c != 6) continue;
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
#define GOG_GET_PET_FN 0x0040e720
static struct { const char *prefix; int is_pet, filter, selected[6], top[6]; void *root; } panels[2] = {
    {PREFIX, 0, 0, {0}, {0}, 0}, {PET_PREFIX, 1, 0, {0}, {0}, 0}};
static int active_panel, focus_panel;
static unsigned char *the_pet(void) {
    typedef void *(__fastcall *pet_fn)(void *hero, void *edx, int index);
    return hero ? (unsigned char *)((pet_fn)GOG_GET_PET_FN)(hero, 0, 0) : 0;
}
static void select_panel(int i) {
    int j;
    panels[active_panel].filter = filter;
    for (j = 0; j < 6; j++) { panels[active_panel].selected[j] = selected[j]; panels[active_panel].top[j] = top[j]; }
    active_panel = i;
    filter = panels[i].filter;
    for (j = 0; j < 6; j++) { selected[j] = panels[i].selected[j]; top[j] = panels[i].top[j]; }
    cur_prefix = panels[i].prefix;
    cur_owner = panels[i].is_pet ? the_pet() : hero;
}
#define ROWS 5
#define ROW_PITCH 92.0f                     /* ItemTray0-4 are 92 pixels apart in the layout */

static const char *const tab_window[6] = {"inventory_filter_weapons", "inventory_filter_armor",
    "inventory_filter_jewelry", "inventory_filter_potions", "inventory_filter_spells", "inventory_filter_fish"};
static const char *const tab_image[6] = {"tab_weapons", "tab_armor", "tab_jewelry", "tab_potions", "tab_gems", "tab_fish"};
static const char *const filter_name[6] = {"Weapons", "Armor", "Jewelry", "Miscellaneous", "Socketables", "Fish"};

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
    void *inv = inventory(), *first = 0, *second = 0;
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
static void fill_details(int row, unsigned char *item) {
    char name[48], buf[64], *p;
    int identified = item[GOG_IDENTIFIED] != 0, stack = *(int *)(item + GOG_STACK);
    p = put_int(buf, ((int_get_fn)GOG_SELL_PRICE)(item, 0));
    *p = 0;
    row_name(name, "Gold", row); text(win(name), buf); show(win(name), 1);
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
    text(win(name), identified ? "" : "Press A to identify");
    prop(win(name), "TextColour", "FFFF2A2A");
    show(win(name), !identified);
    row_name(name, "SlotText", row);
    {
        char colour[32];
        prop(win(name), "TextColour", !identified ? "FFFF2A2A"
                                      : name_colour(item, colour, sizeof colour) ? colour
                                      : "FFCCCACF");   /* the 360's grey (its image, near the tabs' names) */
    }
}

static void refresh(void) {
    int row, i;
    char name[48], buf[200];
    build_list(filter);
    if (selected[filter] >= list_count) selected[filter] = list_count ? list_count - 1 : 0;
    if (selected[filter] < top[filter]) top[filter] = selected[filter];
    if (selected[filter] >= top[filter] + ROWS) top[filter] = selected[filter] - ROWS + 1;
    if (top[filter] < 0) top[filter] = 0;
    for (i = 0; i < 6; i++) {
        char image[64];
        copy(image, sizeof image, "set:X360_widgets2 image:", tab_image[i]);
        if (i == filter) copy(image + length(image), sizeof image - length(image), "_rollover", 0);
        prop(win(tab_window[i]), "Image", image);
    }
    show(win("weaponHi"), 0); show(win("armorHi"), 0); show(win("trinketHi"), 0);
    show(win("potionHi"), 0); show(win("scrollHi"), 0); show(win("fishHi"), 0);
    text(win("filtername"), filter_name[filter]);
    text(win("characterName"), panels[active_panel].is_pet ? "Pet Bag" : "My Bag");
    show(win("rightbumper"), 1);
    show(win("transmutor"), 0);
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
        show(win("playergoldwin"), !panels[active_panel].is_pet);
        if (inv) {
            void **refs = *(void ***)(inv + INV_REFS);
            unsigned count = *(unsigned *)(inv + INV_REFS + 4), j;
            int *starts = *(int **)(inv + INV_PANE_STARTS);
            int first = starts ? starts[0] : EQUIPPED_SLOTS;
            for (j = 0; j < count; j++)
                if (refs[j] && *(int *)((unsigned char *)refs[j] + REF_SLOT) >= first) n++;
            p = put_int(buf, n);
            *p++ = '/';
            p = put_int(p, *(long *)(inv + INV_SLOTS) - first);
            *p = 0;
            text(win("inventorysize"), buf);
        }
    }
    show(win("uparrow"), top[filter] > 0);
    show(win("downarrow"), top[filter] + ROWS < list_count);
    for (row = 0; row < ROWS; row++) {
        int idx = top[filter] + row;
        void *tray;
        row_name(name, "ItemTray", row);
        tray = win(name);
        show(tray, idx < list_count);
        if (idx >= list_count) continue;
        item_name((unsigned char *)list[idx], buf, sizeof buf);
        row_name(name, "SlotText", row); text(win(name), buf);
        item_icon((unsigned char *)list[idx], buf, sizeof buf);
        row_name(name, "Slot", row); prop(win(name), "Image", buf);
        row_name(name, "QualityBk", row); prop(win(name), "Image", quality_image((unsigned char *)list[idx]));
        row_name(name, "SlotText", row); show(win(name), 1);
        row_name(name, "Equipped", row); show(win(name), list_slot[idx] < EQUIPPED_SLOTS);
        /* The tray (sub_823652A0): item_tray, or item_tray_equipped for an equipped item; the
         * selected row at alpha 1.0, the others at 0.9 if equipped, else 0.6. */
        prop(tray, "Image", list_slot[idx] < EQUIPPED_SLOTS ? "set:X360_widgets1 image:item_tray_equipped"
                                                            : "set:X360_widgets1 image:item_tray");
        prop(tray, "Alpha", idx == selected[filter] && active_panel == focus_panel ? "1.0"
                            : list_slot[idx] < EQUIPPED_SLOTS ? "0.9" : "0.6");
        /* Not filled yet (the 360's sub_82368980), so hidden as the 360 hides each when it has no
         * text: damage and armour, requirements, enchantments, quality, price, sockets, the
         * comparison arrow. */
        fill_details(row, (unsigned char *)list[idx]);
        row_name(name, "Enchantments", row); show(win(name), 0);
        row_name(name, "Quality", row); show(win(name), 0);
        compare_arrow(row, (unsigned char *)list[idx], list_slot[idx] < EQUIPPED_SLOTS);
        row_name(name, "socket0_", row); show(win(name), 0);
        row_name(name, "socket1_", row); show(win(name), 0);
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
        show(win("buyandsell"), 0);
        /* The bar as in the 360's screenshots: A by the item (Unequip, Equip, Use, Learn for a
         * spell), Y Drop, X Swap (to the pet's bag); no Info. */
        show(win("buttonB"), 0);
        show(win("buttonY"), 1);
        show(win("buttonX"), 1);
        text(win("buttonYtext"), "Drop");
        text(win("buttonXtext"), "Swap");
        if (list_count) {
            unsigned char *item = (unsigned char *)list[selected[filter]];
            const char *a = list_slot[selected[filter]] < EQUIPPED_SLOTS ? "Unequip"
                            : isa(item, 129) ? "Learn"
                            : ((item_bool_fn)GOG_IS_USEABLE)(item, 0) ? "Use" : "Equip";
            int scrolls = 0;
            char label[32];
            /* An unidentified item: the 360's "Identify (" + the scrolls + ")" (its screenshot,
             * "Identify(11)"). */
            if (!item[GOG_IDENTIFIED] && identify_scrolls(&scrolls) && scrolls > 0) {
                const char *p = "Identify (";
                char digits[12];
                int n = 0, d = 0, v = scrolls;
                while (*p && n < 20) label[n++] = *p++;
                do { digits[d++] = (char)('0' + v % 10); v /= 10; } while (v && d < 11);
                while (d && n < 30) label[n++] = digits[--d];
                label[n++] = ')';
                label[n] = 0;
                a = label;
            }
            text(win("buttonAtext"), a);
            /* The 360's bar (its screenshot: "A Identify(11)  Y Drop  X Swap"): each button after
             * the text of the one before it. The layout's gap after "Unequip" (A at -112, Y at -29,
             * X at 26, the icons 15 wide, the texts 15 in) is kept after the measured texts. */
            {
                void *ba = win("buttonA"), *by = win("buttonY"), *bx = win("buttonX");
                char value[128];
                float va[8], vy[8], vx[8], wa = text_width(win("buttonAtext"), a), wy = text_width(win("buttonYtext"), "Drop");
                if (ba && by && bx && wa > 0 && wy > 0) {
                    read_prop(ba, "UnifiedAreaRect", value, sizeof value);
                    if (parse_rect(value, va)) {
                        float gap = 10.0f * k, icon = va[5] - va[1];
                        read_prop(by, "UnifiedAreaRect", value, sizeof value);
                        if (parse_rect(value, vy)) {
                            vy[1] = va[1] + icon + wa + gap; vy[5] = vy[1] + icon;
                            write_rect(value, vy); prop(by, "UnifiedAreaRect", value);
                            read_prop(bx, "UnifiedAreaRect", value, sizeof value);
                            if (parse_rect(value, vx)) {
                                vx[1] = vy[1] + icon + wy + gap; vx[5] = vx[1] + icon;
                                write_rect(value, vx); prop(bx, "UnifiedAreaRect", value);
                            }
                        }
                    }
                }
            }
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
static float fill_card(int i, void *item, void *cmp1, void *cmp2) {
    void *root, *sys = system_ptr(), *sheet = sys ? get_gui_sheet(sys, 0) : 0;
    unsigned char *t = card(i);
    char value[128];
    float v[8];
    if (!t || !sheet || !item) { hide_card(i); return 0; }
    /* PC rebuilds a card only when the item's GUID (its +0x8, kept at the tooltip's +0x8)
     * changes (GOG 0x54281f); the screen calls it on its own changes (an item identified, a stack
     * used), so the kept GUID is cleared first. */
    *(unsigned *)(t + 0x8) = 0xffffffffu;
    *(unsigned *)(t + 0xc) = 0xffffffffu;
    __try {
        ((show_tooltip_fn)GOG_SHOW_EQUIPMENT_TOOLTIP)(ui, 0, hero, item, t, cmp1, cmp2);
    } __except (1) {
        x360_log("x360 inv: showEquipmentTooltip threw\r\n");
        return 0;
    }
    root = card_root(i);
    if (!root) { x360_log("x360 inv: the tooltip has no root\r\n"); return 0; }
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
    void *pet_top = win_of(PET_PREFIX, "TopFrame"), *inv = inventory(), *first = 0, *second = 0;
    float x = 0, h0, h1 = 0, h2 = 0, gap = 4.0f * k, bottom = 0, w, y1;
    RECT cr;
    HWND hw = GetActiveWindow();
    if (!item) { hide_info(); return; }
    if (!rect_of) rect_of = (pixel_rect_fn6)need("?getUnclippedPixelRect@Window@CEGUI@@QBE?AVRect@2@XZ");
    if (pet_top && rect_of) {
        rect6 r;
        fresh_rects(pet_top);
        rect_of(pet_top, 0, &r);
        x = r.left + 8.0f * k;
    }
    if (hw && GetClientRect(hw, &cr)) bottom = (float)cr.bottom;
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
        x360_log("\r\n");
    }
    info_item = item;
}

/* ---- the character column (charactermenu_xenon, the frame's first column) ----
 * PC's own stats menu (STATSMENU.LAYOUT, kept by cegui_proxy.c) has windows of the same names; the
 * texts PC writes there are copied over: the same numbers PC shows. The 3D doll is not done. */
#define CHR_PREFIX "x360chr_"
extern void *x360_pc_stats_root;
static void *character_root;
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
    if (!character_root || !hero) return;
    (void)pc_stats_menu;
    __try {
        level = *(int *)(hero + 0xf0);
        xp = *(int *)(hero + 0x3c8);
        fame = *(int *)(hero + 0x3cc);
        fame_level = *(int *)(hero + 0x3d0);
        table = ((table_fn)GOG_XP_TABLE)();
        if (table) {
            xp_next = ((int_arg_fn)GOG_XP_FOR_LEVEL)(table, 0, level);
            fame_next = ((int_arg_fn)GOG_FAME_FOR_LEVEL)(table, 0, fame_level);
        }
    } __except (1) {
        x360_log("x360 chr: reading the hero threw\r\n");
        return;
    }
    if (wstring_utf8(hero + 0x420, buf, sizeof buf)) text(win_of(CHR_PREFIX, "characterName"), buf);
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
    p = put_int(buf, ((int_get_fn)GOG_MAX_HP)(hero, 0)); *p = 0; text(win_of(CHR_PREFIX, "HP"), buf);
    p = put_int(buf, ((int_get_fn)GOG_MAX_MANA)(hero, 0)); *p = 0; text(win_of(CHR_PREFIX, "Mana"), buf);
    p = put_int(buf, ((int_get_fn)GOG_STRENGTH)(hero, 0)); *p = 0; text(win_of(CHR_PREFIX, "strPts"), buf);
    p = put_int(buf, ((int_get_fn)GOG_DEXTERITY)(hero, 0)); *p = 0; text(win_of(CHR_PREFIX, "dexPts"), buf);
    p = put_int(buf, ((int_get_fn)GOG_MAGIC)(hero, 0)); *p = 0; text(win_of(CHR_PREFIX, "magicPts"), buf);
    p = put_int(buf, ((int_get_fn)GOG_DEFENSE)(hero, 0)); *p = 0; text(win_of(CHR_PREFIX, "defPts"), buf);
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
    if (!inventory_open || panels[focus_panel].is_pet || !list_count) { hide_info(); return; }
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
static void refresh_all(void) {
    int other = 1 - focus_panel;
    if (panels[1].root) {
        select_panel(other);
        if (cur_owner) refresh();
        show(win("petintown"), !cur_owner);
        if (!cur_owner) { int r; char n[48]; for (r = 0; r < ROWS; r++) { row_name(n, "ItemTray", r); show(win(n), 0); } show(win("moveframe"), 0); }
    }
    select_panel(focus_panel);
    if (cur_owner) refresh();
    fill_character();
    text(win_of(MASTER, "itemcomparetext"), compare_on ? "disable item compare" : "enable item compare");
    update_info();
}

/* A on an unidentified item (the 360's "Press A to identify"): an identify scroll from the hero's
 * bag is spent and the item identified as PC does it (the identify effect's handler, GOG 0x4b388b:
 * +0x2fc set, CItem::destroyItemText 0x4d1cc0). The scroll is found by its name (GOG's English
 * "identify scroll"); a stack loses one, the last one is taken out and destroyed as PC destroys an
 * item (removeEquipment, then the virtual destructor +0 with 1). */
#define GOG_DESTROY_ITEM_TEXT 0x004d1cc0
/* The hero's first identify scroll stack, and in *count the scrolls of every stack. */
static unsigned char *identify_scrolls(int *count) {
    unsigned char *inv = (unsigned char *)(hero ? *(void **)(hero + GOG_INVENTORY) : 0), *scroll = 0;
    void **refs;
    unsigned n, i;
    char name[64];
    if (count) *count = 0;
    if (!inv) return 0;
    refs = *(void ***)(inv + INV_REFS);
    n = *(unsigned *)(inv + INV_REFS + 4);
    for (i = 0; i < n; i++) {
        unsigned char *ref = (unsigned char *)refs[i], *it;
        if (!ref || !(it = *(unsigned char **)(ref + REF_ITEM))) continue;
        item_name(it, name, sizeof name);
        {
            const char *a = name, *b = "identify scroll";
            while (*a && *a == *b) a++, b++;
            if (*a || *b) continue;
        }
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
static void act(void) {
    void *inv = inventory();
    unsigned char *item;
    if (!inv || !list_count) return;
    if (panels[active_panel].is_pet) { x360_log("x360 inv: A in the pet's bag: not done yet\r\n"); return; }
    item = (unsigned char *)list[selected[filter]];
    if (!item[GOG_IDENTIFIED]) { identify(item); return; }
    if (list_slot[selected[filter]] < EQUIPPED_SLOTS) {
        ((inv_item_fn)GOG_REMOVE_EQUIPMENT)(inv, 0, item);
        if (!((pickup_fn)GOG_PICKUP_EQUIPMENT)(inv, 0, item, 1)) {
            ((inv_item_fn)GOG_EQUIP_FIRST_FREE)(inv, 0, item);
            x360_log("x360 inv: A: bag full, kept equipped\r\n");
        } else {
            x360_log("x360 inv: A: unequipped\r\n");
        }
        return;
    }
    if (((item_bool_fn)GOG_IS_USEABLE)(item, 0)) {
        if (ui) ((use_item_fn)GOG_USE_ITEM)(ui, 0, level, item);
        x360_log(ui ? "x360 inv: A: used\r\n" : "x360 inv: A: no CGameUI, not used\r\n");
        return;
    }
    if (((inv_item_fn)GOG_EQUIP_FIRST_FREE)(inv, 0, item)) {
        ((int_fn)GOG_RENDER_BEHIND)(hero, 0, 1);
        ((ptr_fn)GOG_DROP_SOUND)(item, 0, *(void **)(hero + 0x58));
        x360_log("x360 inv: A: equipped\r\n");
        return;
    }
    if (!((can_equip_fn)(*(void ***)item)[GOG_CAN_EQUIP_SLOT])(item, 0, hero, 1)) {
        x360_log("x360 inv: A: cannot equip\r\n");
        return;
    }
    {
        void *first = 0, *second = 0;
        ((comparison_fn)GOG_COMPARISON_ITEMS)(inv, 0, item, &first, &second);
        if (isa(item, 10) && second) ((inv_item_fn)GOG_REMOVE_EQUIPMENT)(inv, 0, second);
        if (first) {
            ((inv_item_fn)GOG_REMOVE_EQUIPMENT)(inv, 0, first);
            if (((inv_item_fn)GOG_EQUIP_FIRST_FREE)(inv, 0, item)) {
                ((int_fn)GOG_RENDER_BEHIND)(hero, 0, 1);
                ((ptr_fn)GOG_DROP_SOUND)(item, 0, *(void **)(hero + 0x58));
            }
            if (!((pickup_fn)GOG_PICKUP_EQUIPMENT)(inv, 0, first, 1)) x360_log("x360 inv: A: the old item did not fit\r\n");
        }
        if (isa(item, 10) && second && !((pickup_fn)GOG_PICKUP_EQUIPMENT)(inv, 0, second, 1))
            x360_log("x360 inv: A: the old second item did not fit\r\n");
        x360_log("x360 inv: A: swapped\r\n");
    }
}

#define GOG_GET_POSITION 0x00510eb0   /* CPositionableObject::getPosition(Vector3* out, bool) */
#define GOG_LEVEL_ADD_ITEM 0x004f5960 /* CLevel::addItem(CItem*, Vector3 const*, bool) */
#define GOG_ITEM_DROPPED_SLOT (0x198 / 4) /* CItem's virtual after addItem, as PC's drop does */
#define GOG_GET_PET 0x0040e720        /* the hero's pet by index (thiscall, int) */
typedef void *(__fastcall *get_position_fn)(void *self, void *edx, float *out, int flag);
typedef void (__fastcall *add_item_fn)(void *level, void *edx, void *item, const float *pos, int flag);
typedef void *(__fastcall *get_pet_fn)(void *hero, void *edx, int index);

/* Y: the item to the ground at the hero's feet, as PC drops an item that does not fit (GOG
 * 0x55e28a): removeEquipment, CLevel::addItem at the hero's position, the item's virtual +0x198. */
static void drop(void) {
    void *inv = inventory();
    unsigned char *item;
    float pos[3];
    if (!inv || !list_count) return;
    item = (unsigned char *)list[selected[filter]];
    ((inv_item_fn)GOG_REMOVE_EQUIPMENT)(inv, 0, item);
    ((get_position_fn)GOG_GET_POSITION)(hero, 0, pos, 1);
    ((add_item_fn)GOG_LEVEL_ADD_ITEM)(level, 0, item, pos, 1);
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
    ((inv_item_fn)GOG_REMOVE_EQUIPMENT)(inv, 0, item);
    if (((pickup_fn)GOG_PICKUP_EQUIPMENT)(pet_inv, 0, item, 1)) { x360_log("x360 inv: X: to the pet\r\n"); return; }
    ((pickup_fn)GOG_PICKUP_EQUIPMENT)(inv, 0, item, 1);
    x360_log("x360 inv: X: the pet's bag is full\r\n");
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
        x360_log(" price "); x360_log_num(((int_get_fn)GOG_SELL_PRICE)(item, 0));
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
static float fovy_saved;

static void show_doll(int on) {
    static pixel_rect_fn rect_of;
    static get_fovy_fn fovy;
    static set_fovy_fn set_fovy;
    if (!fovy) {
        void *ogre = GetModuleHandleA("OgreMain.dll");
        fovy = ogre ? (get_fovy_fn)GetProcAddress(ogre, "?getFOVy@Frustum@Ogre@@UBEABVRadian@2@XZ") : 0;
        set_fovy = ogre ? (set_fovy_fn)GetProcAddress(ogre, "?setFOVy@Frustum@Ogre@@UAEXABVRadian@2@@Z") : 0;
    }
    unsigned char *m = pc_inventory_menu();
    void *target, *camera, *vp;
    if (!m) { x360_log("x360 chr: no CInventoryMenu, no doll\r\n"); return; }
    target = *(void **)(m + INVMENU_TARGET);
    camera = *(void **)(m + INVMENU_CAMERA);
    vp = *(void **)(m + INVMENU_VIEWPORT);
    if (!target || !camera) return;
    if (!on) {
        if (vp) { ((remove_viewport_fn)(*(void ***)target)[0x2c / 4])(target, 0, 3); *(void **)(m + INVMENU_VIEWPORT) = 0; }
        if (fovy_saved && set_fovy) { set_fovy(camera, 0, &fovy_saved); fovy_saved = 0; }
        return;
    }
    if (!rect_of) rect_of = (pixel_rect_fn)need("?getUnclippedPixelRect@Window@CEGUI@@QBE?AVRect@2@XZ");
    {
        void *view = win_of(CHR_PREFIX, "CharacterViewport");
        RECT r;
        HWND w = GetActiveWindow();
        frect a;
        float W, H, rgba[4] = {0, 0, 0, 1};
        if (!view || !rect_of || !w || !GetClientRect(w, &r) || r.right <= r.left || r.bottom <= r.top) return;
        W = (float)(r.right - r.left);
        H = (float)(r.bottom - r.top);
        fresh_rects(win_of(CHR_PREFIX, "TopFrame"));
        rect_of(view, 0, &a);
        if (!vp) {
            vp = ((add_viewport_fn)(*(void ***)target)[0x20 / 4])(target, 0, camera, 3, 0, 0, 1, 1);
            *(void **)(m + INVMENU_VIEWPORT) = vp;
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
        if (fovy && set_fovy && !fovy_saved) {
            fovy_saved = *fovy(camera, 0);
            {
                float wide = 2.0f * arctan(DOLL_ZOOM_OUT * tangent(fovy_saved / 2.0f));
                set_fovy(camera, 0, &wide);
            }
        }
        x360_log("x360 chr: doll viewport at "); x360_log_num((long)a.left); x360_log(","); x360_log_num((long)a.top);
        x360_log(" "); x360_log_num((long)(a.right - a.left)); x360_log("x"); x360_log_num((long)(a.bottom - a.top)); x360_log("\r\n");
    }
}

static int is_ours(void *w) {
    return w && (w == master_root || w == inventory_root || w == panels[1].root || w == character_root ||
                 w == card_root(0) || w == card_root(1) || w == card_root(2));   /* the Info cards, PC's tooltips */
}

int x360_inventory_open(void) { return inventory_open; }

/* The pad's Back: the 360's inventory on or off. */
void x360_run_console(void);

void x360_inventory_toggle(void) {
    if (!find_world()) return;
    x360_run_console();   /* test runs: console= commands, once, at the first opening (the level is loaded) */
    {
        RECT r;
        HWND w = GetActiveWindow();
        if (resolve() && w && GetClientRect(w, &r) && r.bottom > r.top)
            match_spacing((float)(r.right - r.left), (float)(r.bottom - r.top));
    }
    if (!inventory_root && !inventory_failed) {
        master_root = load("media/ui/x360/mastermenu_xenon.layout", MASTER);
        inventory_root = master_root ? load("media/ui/x360/inventorymenu_xenon.layout", PREFIX) : 0;
        panels[0].root = inventory_root;
        /* The pet's bag: the same layout again, in the frame's third column. */
        panels[1].root = inventory_root ? load("media/ui/x360/inventorymenu_xenon.layout", PET_PREFIX) : 0;
        character_root = inventory_root ? load("media/ui/x360/charactermenu_xenon.layout", CHR_PREFIX) : 0;
        if (!inventory_root) inventory_failed = 1;
        else {
            RECT r;
            HWND w = GetActiveWindow();
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
            if (character_root) scale_tree(character_root, k, 0);
            if (character_root) unclip_tree(character_root, 0);
            /* The 360's tabbed frame (mastermenu_xenon, centred) holds the screens in its
             * ContainerFrame, three columns of 301: the character, the bag, the pet's bag. The bag
             * goes in the second (the screenshots: its frame at (747, 127) of 1080). */
            {
                void *container = win_of(MASTER, "ContainerFrame"), *top = win("TopFrame");
                if (container && top) {
                    add_child(container, 0, top);
                    move_by(top, 301.0f * k, 0);
                }
                top = character_root ? win_of(CHR_PREFIX, "TopFrame") : 0;
                if (container && top) add_child(container, 0, top);
                top = panels[1].root ? win_of(PET_PREFIX, "TopFrame") : 0;
                if (container && top) {
                    add_child(container, 0, top);
                    move_by(top, 602.0f * k, 0);
                }
            }
            /* Not working yet, so not shown: the LT/RT tabs, pet commands. */
            show(win_of(MASTER, "lefttrigger"), 0);
            show(win_of(MASTER, "righttrigger"), 0);
            show(win_of(MASTER, "MenuLeft"), 0);
            show(win_of(MASTER, "MenuRight"), 0);
            show(win_of(MASTER, "PetCommands"), 0);
            text(win_of(MASTER, "MenuName"), "Inventory");   /* the 360's string (its image, 0x820d17d0) */
            mf = win("moveframe");
            read_prop(mf, "UnifiedAreaRect", value, sizeof value);
            if (parse_rect(value, v)) moveframe_top = v[3];
            clear_samples();
            if (panels[1].root) { select_panel(1); clear_samples(); select_panel(0); }
            x360_log("x360 menu: scaled by x1000 "); x360_log_num((long)(k * 1000)); x360_log("\r\n");
        }
    }
    if (!inventory_root) return;
    inventory_open = !inventory_open;
    if (inventory_open) {
        select_panel(0);
        dump();
        refresh_all();
    }
    set_visible(master_root, 0, inventory_open);
    set_visible(inventory_root, 0, inventory_open);
    if (panels[1].root) set_visible(panels[1].root, 0, inventory_open);
    if (character_root) set_visible(character_root, 0, inventory_open);
    if (inventory_open) move_to_front(master_root, 0);
    hide_pc_ui(inventory_open);
    if (character_root) show_doll(inventory_open);
    if (!inventory_open) hide_info();
    x360_log(inventory_open ? "x360 menu: inventory shown\r\n" : "x360 menu: inventory hidden\r\n");
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
    return h * 31u + fingerprint_of(pet ? *(unsigned char **)(pet + GOG_INVENTORY) : 0);
}

/* The pad while the inventory is open: pressed, the buttons just pressed; ly, the stick (of 1000).
 * Up and down (the stick or the d-pad) move the selection, repeating while held; LB and RB the
 * filter; A acts on the row; B or Back closes. */
void x360_inventory_pad(unsigned pressed, unsigned buttons, int ly) {
    static unsigned next_repeat;
    static int held_dir;
    unsigned now = GetTickCount();
    int dir = 0;
    static unsigned last_print, last_check;
    if (!inventory_open || !find_world()) return;
    if (pressed & 0x2000) { x360_inventory_toggle(); return; }       /* B */
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
    /* LB/RB: the filter; past the ends, the other bag (sub_8236A488, sub_823702E0). */
    if (pressed & 0x0100) {                                            /* LB */
        if (filter > 0) filter--;
        else if (panels[1].root) { focus_panel = 1 - focus_panel; select_panel(focus_panel); filter = 5; }
        refresh_all();
    }
    if (pressed & 0x0200) {                                            /* RB */
        if (filter < 5) filter++;
        else if (panels[1].root) { focus_panel = 1 - focus_panel; select_panel(focus_panel); filter = 0; }
        refresh_all();
    }
    if (pressed & 0x000c) { compare_on = !compare_on; refresh_all(); } /* d-pad left or right: item compare */
    if (pressed & 0x1000) { act(); refresh_all(); }                    /* A */
    if (pressed & 0x8000) { drop(); refresh_all(); }                   /* Y */
    if (pressed & 0x4000) { swap_to_pet(); refresh_all(); }            /* X */
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
typedef struct { unsigned char bytes[28]; } wstring_by_value;   /* VC9 std::wstring */
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
