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

static void *win(const char *name) { return win_of(PREFIX, name); }

#define MASTER "x360mm_"

static void prop(void *w, const char *name, const char *value) {
    object n, v;
    if (!w) return;
    string_ctor(&n, 0, name);
    string_utf8(&v, 0, value);
    __try { set_property(w, 0, &n, &v); } __except (1) { x360_log("x360 menu: setProperty threw\r\n"); }
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

/* ---- the 360's letter spacing ----
 * The 360 lays out and rasterises its fonts at 1280x720 and its frame is drawn 1.5 times larger at
 * 1080; PC rasterises at its own resolution, where FreeType's hinting rounds each advance
 * differently: PC's text came out 6-10% wider. Each 360 font (renamed X360_* by the converter, so
 * PC's own fonts are untouched) is laid out once at 1280x720 with CEGUI's own FreeType
 * (Font::notifyScreenResolution), its advances kept, then laid out again at the game's size,
 * which keeps the glyphs sharp, and every advance replaced by the 360's times height / 720. */

typedef struct { float w, h; } size2;
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
    if (!fm || !get_font || !notify || !glyph || height <= 0) return;
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
        __try {
            size2 s720 = {1280.0f, 720.0f}, real = {width, height};
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
            x360_log(" at 720, x100 "); x360_log_num((long)(patched_e[f] * 100)); x360_log(" now\r\n");
        } __except (1) {
            x360_log("x360 menu: the spacing threw\r\n");
        }
    }
}

/* ---- loading ---- */

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
#define GOG_DPS 0x004b8f10               /* CEquipment::DPS(), a float */
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
typedef unsigned char (__fastcall *can_equip_fn)(void *item, void *edx, void *character);
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

static void *inventory(void) { return hero ? *(void **)(hero + GOG_INVENTORY) : 0; }

#define MAX_LIST 128
static void *list[MAX_LIST];
static int list_slot[MAX_LIST];
static int list_count;

/* The 360's list (sub_82369FE8): the equipped items first, then the others, both read from the
 * last reference to the first; each in the filter's category or in any (6). */
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

static void item_name(unsigned char *item, char *out, unsigned size) {
    out[0] = 0;
    if (name_offset) {
        __try { if (wstring_utf8(item + name_offset, out, size)) return; } __except (1) {}
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
        float dps = ((float_get_fn)GOG_DPS)(item, 0);
        copy(buf, sizeof buf, "DPS: ", 0);
        p = put_int(buf + 5, (long)(dps + 0.5f));
        *p = 0;
    }
    row_name(name, "DamageArmor", row); text(win(name), buf); show(win(name), buf[0] != 0);
    row_name(name, "Requirment", row);
    text(win(name), identified ? "" : "Press A to identify");
    prop(win(name), "TextColour", "FFFF2A2A");
    show(win(name), !identified);
    row_name(name, "SlotText", row);
    prop(win(name), "TextColour", identified ? "FFFFFFFF" : "FFFF2A2A");
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
    text(win("characterName"), "My Bag");
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
        show(win("playergoldwin"), 1);
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
        row_name(name, "QualityBk", row); prop(win(name), "Image", "set:X360_widgets2 image:traycolor_blank");
        row_name(name, "SlotText", row); show(win(name), 1);
        row_name(name, "Equipped", row); show(win(name), list_slot[idx] < EQUIPPED_SLOTS);
        /* The tray (sub_823652A0): item_tray, or item_tray_equipped for an equipped item; the
         * selected row at alpha 1.0, the others at 0.9 if equipped, else 0.6. */
        prop(tray, "Image", list_slot[idx] < EQUIPPED_SLOTS ? "set:X360_widgets1 image:item_tray_equipped"
                                                            : "set:X360_widgets1 image:item_tray");
        prop(tray, "Alpha", idx == selected[filter] ? "1.0" : list_slot[idx] < EQUIPPED_SLOTS ? "0.9" : "0.6");
        /* Not filled yet (the 360's sub_82368980), so hidden as the 360 hides each when it has no
         * text: damage and armour, requirements, enchantments, quality, price, sockets, the
         * comparison arrow. */
        fill_details(row, (unsigned char *)list[idx]);
        row_name(name, "Enchantments", row); show(win(name), 0);
        row_name(name, "Quality", row); show(win(name), 0);
        row_name(name, "qualityarrow", row); show(win(name), 0);
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
        show(mf, list_count > 0);
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
            text(win("buttonAtext"), a);
        }
    }
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
    item = (unsigned char *)list[selected[filter]];
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
    if (!((can_equip_fn)(*(void ***)item)[GOG_CAN_EQUIP_SLOT])(item, 0, hero)) {
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
    pet = ((get_pet_fn)GOG_GET_PET)(hero, 0, 0);
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

int x360_inventory_open(void) { return inventory_open; }

/* The pad's Back: the 360's inventory on or off. */
void x360_inventory_toggle(void) {
    if (!find_world()) return;
    {
        RECT r;
        HWND w = GetActiveWindow();
        if (resolve() && w && GetClientRect(w, &r) && r.bottom > r.top)
            match_spacing((float)(r.right - r.left), (float)(r.bottom - r.top));
    }
    if (!inventory_root && !inventory_failed) {
        master_root = load("media/ui/x360/mastermenu_xenon.layout", MASTER);
        inventory_root = master_root ? load("media/ui/x360/inventorymenu_xenon.layout", PREFIX) : 0;
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
            /* The 360's tabbed frame (mastermenu_xenon, centred) holds the screens in its
             * ContainerFrame, three columns of 301: the character, the bag, the pet's bag. The bag
             * goes in the second (the screenshots: its frame at (747, 127) of 1080). */
            {
                void *container = win_of(MASTER, "ContainerFrame"), *top = win("TopFrame");
                if (container && top) {
                    add_child(container, 0, top);
                    move_by(top, 301.0f * k, 0);
                }
            }
            /* Not working yet, so not shown: the LT/RT tabs, pet commands, item comparison. */
            show(win_of(MASTER, "lefttrigger"), 0);
            show(win_of(MASTER, "righttrigger"), 0);
            show(win_of(MASTER, "MenuLeft"), 0);
            show(win_of(MASTER, "MenuRight"), 0);
            show(win_of(MASTER, "PetCommands"), 0);
            show(win_of(MASTER, "ToggleItemCompare"), 0);
            text(win_of(MASTER, "MenuName"), "Inventory");   /* the 360's string (its image, 0x820d17d0) */
            mf = win("moveframe");
            read_prop(mf, "UnifiedAreaRect", value, sizeof value);
            if (parse_rect(value, v)) moveframe_top = v[3];
            clear_samples();
            x360_log("x360 menu: scaled by x1000 "); x360_log_num((long)(k * 1000)); x360_log("\r\n");
        }
    }
    if (!inventory_root) return;
    inventory_open = !inventory_open;
    if (inventory_open) {
        dump();
        refresh();
    }
    set_visible(master_root, 0, inventory_open);
    set_visible(inventory_root, 0, inventory_open);
    if (inventory_open) move_to_front(master_root, 0);
    x360_log(inventory_open ? "x360 menu: inventory shown\r\n" : "x360 menu: inventory hidden\r\n");
}

/* A fingerprint of the inventory: its items, their slots and stacks. PC changes it outside our
 * buttons too (useItem takes a potion out while the hero drinks and puts the rest back; pickups),
 * and the 360's screen hears of every change (iInventoryListener): ours rebuilds when it moves. */
static unsigned fingerprint(void) {
    unsigned char *inv = (unsigned char *)inventory();
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
        if (print != last_print) { last_print = print; refresh(); }
    }
    if (buttons & 0x0001 || ly > 500) dir = -1;                      /* up */
    if (buttons & 0x0002 || ly < -500) dir = 1;                      /* down */
    if (dir != held_dir) { held_dir = dir; next_repeat = 0; }
    if (dir && (!next_repeat || now >= next_repeat)) {
        int s = selected[filter] + dir;
        if (s >= 0 && s < list_count) selected[filter] = s;
        next_repeat = now + (next_repeat ? 120 : 350);
        refresh();
    }
    if (pressed & 0x0100 && filter > 0) { filter--; refresh(); }       /* LB */
    if (pressed & 0x0200 && filter < 5) { filter++; refresh(); }       /* RB */
    if (pressed & 0x1000) { act(); refresh(); }                        /* A */
    if (pressed & 0x8000) { drop(); refresh(); }                       /* Y */
    if (pressed & 0x4000) { swap_to_pet(); refresh(); }                /* X */
}
