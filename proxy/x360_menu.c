/* The 360's screens hosted in PC's GUI (stage 4, the inventory slice, docs section 11).
 *
 * The converted layout (tools/convert_x360_ui.py, media/ui/x360/) is loaded once with the real
 * WindowManager::loadWindowLayout, attached to PC's GUI sheet and shown or hidden; PC's own
 * inventory is not opened. Everything runs on the game's thread (called from proxy/pad.c, inside
 * GetCursorPos, where the pad already calls the game's functions). CEGUI's members are __thiscall:
 * called as __fastcall with a dummy edx, as in cegui_proxy.c. CEGUI::String is built and destroyed
 * by the real DLL's own members, in opaque storage.
 */

typedef void *FARPROC;

void x360_log(const char *s);
void x360_log_num(long v);
FARPROC x360_need(const char *name);

/* Resolved in CEGUIBase_orig.dll; the build checks every name against the user's DLL. */
static FARPROC need(const char *name) { return x360_need(name); }

typedef struct { __declspec(align(8)) unsigned char bytes[256]; } object;

typedef void *(__fastcall *string_ctor_fn)(void *self, void *edx, const char *text);
typedef void (__fastcall *string_dtor_fn)(void *self, void *edx);
typedef void *(__cdecl *singleton_fn)(void);
typedef void *(__fastcall *load_layout5_fn)(void *self, void *edx, const void *file, const void *prefix,
                                            const void *group, void *callback, void *userdata);
typedef void *(__fastcall *get_sheet_fn)(const void *self, void *edx);
typedef void (__fastcall *add_child_fn)(void *self, void *edx, void *child);
typedef void (__fastcall *set_bool_fn)(void *self, void *edx, int on);
typedef void (__fastcall *void_fn)(void *self, void *edx);

static string_ctor_fn string_ctor;
static string_dtor_fn string_dtor;
static singleton_fn window_manager, system_ptr;
static load_layout5_fn load_layout5;
static get_sheet_fn get_gui_sheet;
static add_child_fn add_child;
static set_bool_fn set_visible;
static void_fn move_to_front;

static int resolve(void) {
    static int done, ok;
    if (done) return ok;
    done = 1;
    string_ctor = (string_ctor_fn)need("??0String@CEGUI@@QAE@PBD@Z");
    string_dtor = (string_dtor_fn)need("??1String@CEGUI@@QAE@XZ");
    window_manager = (singleton_fn)need("?getSingletonPtr@?$Singleton@VWindowManager@CEGUI@@@CEGUI@@SAPAVWindowManager@2@XZ");
    system_ptr = (singleton_fn)need("?getSingletonPtr@?$Singleton@VSystem@CEGUI@@@CEGUI@@SAPAVSystem@2@XZ");
    load_layout5 = (load_layout5_fn)need(
        "?loadWindowLayout@WindowManager@CEGUI@@QAEPAVWindow@2@ABVString@2@00P6A_NPAV32@AAV42@2PAX@Z3@Z");
    get_gui_sheet = (get_sheet_fn)need("?getGUISheet@System@CEGUI@@QBEPAVWindow@2@XZ");
    add_child = (add_child_fn)need("?addChildWindow@Window@CEGUI@@QAEXPAV12@@Z");
    set_visible = (set_bool_fn)need("?setVisible@Window@CEGUI@@QAEX_N@Z");
    move_to_front = (void_fn)need("?moveToFront@Window@CEGUI@@QAEXXZ");
    ok = string_ctor && string_dtor && window_manager && system_ptr && load_layout5 && get_gui_sheet &&
         add_child && set_visible && move_to_front;
    if (!ok) x360_log("x360 menu: a CEGUI function is missing\r\n");
    return ok;
}

/* The layout's root, loaded and attached once (0: not yet, or it failed). */
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

static void *inventory;
static int inventory_failed, inventory_open;

int x360_inventory_open(void) { return inventory_open; }

/* The pad's Back: the 360's inventory on or off. */
void x360_inventory_toggle(void) {
    if (!inventory && !inventory_failed) {
        inventory = load("media/ui/x360/inventorymenu_xenon.layout", "x360inv_");
        if (!inventory) inventory_failed = 1;
    }
    if (!inventory) return;
    inventory_open = !inventory_open;
    set_visible(inventory, 0, inventory_open);
    if (inventory_open) move_to_front(inventory, 0);
    x360_log(inventory_open ? "x360 menu: inventory shown\r\n" : "x360 menu: inventory hidden\r\n");
}
