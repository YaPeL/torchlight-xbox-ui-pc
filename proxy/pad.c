/* Prototype: the pad moves the hero, by a virtual mouse (pad=1 in x360_ui.ini).
 *
 * The 360 build turns the left stick into a point about 2 world units ahead of the hero, in the
 * camera's frame, and sends the hero there (docs/xbox-ui-on-pc.md, section 9). PC sends the hero
 * toward the cursor while the left button is held. So the prototype does the 360's thing in
 * screen space: while the stick is out of its dead zone, Torchlight.exe's GetCursorPos returns a
 * point RADIUS pixels from the centre of the game's window, in the stick's direction, and its
 * GetAsyncKeyState says the left button is down. A on the pad is a left click where the virtual
 * cursor is. Nothing of the executable's code is patched: only its import table entries for
 * those two functions point here.
 *
 * Input: XInput (xinput1_3.dll, pad 0); or, with pad_file=PATH, a text file "LX LY BUTTONS"
 * (sticks from -1 to 1 as numbers times 1000, buttons as XInput's decimal mask), re-read every
 * 50 ms, so a test can drive it without a hand on the pad.
 */

typedef unsigned long DWORD;
typedef unsigned short WORD;
typedef int BOOL;
typedef void *HANDLE;
typedef void *HMODULE;
typedef void *HWND;
typedef void *FARPROC;
typedef struct { long x, y; } POINT;
typedef struct { long left, top, right, bottom; } RECT;

#define IMPORT __declspec(dllimport)
IMPORT HMODULE __stdcall GetModuleHandleA(const char *name);
IMPORT HMODULE __stdcall LoadLibraryA(const char *name);
IMPORT FARPROC __stdcall GetProcAddress(HMODULE module, const char *name);
IMPORT BOOL __stdcall VirtualProtect(void *addr, DWORD size, DWORD prot, DWORD *old);
IMPORT DWORD __stdcall GetTickCount(void);
IMPORT HANDLE __stdcall CreateFileA(const char *name, DWORD access, DWORD share, void *security,
                                    DWORD disposition, DWORD flags, HANDLE templ);
IMPORT BOOL __stdcall ReadFile(HANDLE file, void *data, DWORD size, DWORD *read, void *overlapped);
IMPORT BOOL __stdcall CloseHandle(HANDLE file);
IMPORT HWND __stdcall GetActiveWindow(void);
IMPORT BOOL __stdcall GetClientRect(HWND w, RECT *r);
IMPORT BOOL __stdcall ClientToScreen(HWND w, POINT *p);
IMPORT BOOL __stdcall ScreenToClient(HWND w, POINT *p);
IMPORT BOOL __stdcall PostMessageW(HWND w, unsigned msg, unsigned wparam, long lparam);
#define IMPORT_SCREEN_TO_CLIENT(w, p) ((w) ? ScreenToClient((w), (p)) : 0)

void x360_log(const char *s);
void x360_log_num(long v);
void x360_inject_left(int down, float x, float y);

typedef BOOL(__stdcall *get_cursor_pos_fn)(POINT *p);
typedef short(__stdcall *get_async_key_state_fn)(int vk);
typedef DWORD(__stdcall *xinput_get_state_fn)(DWORD pad, void *state);

static get_cursor_pos_fn real_get_cursor_pos;
static get_async_key_state_fn real_get_async_key_state;
static xinput_get_state_fn xinput_get_state;
static int xinput_tried;
static char pad_file[400];

/* The pad, as last read. */
static int lx, ly;             /* -1000..1000, y up */
static unsigned buttons;       /* XInput mask: A 0x1000, B 0x2000, X 0x4000, Y 0x8000 */
static unsigned a_pressed_at;  /* GetTickCount of A's last press, for a short synthetic click */
static unsigned last_poll;
static int held;               /* the virtual left button, as last injected */
static int edges_by_message;   /* 1: the edges are WM_LBUTTONDOWN/UP posted to the window; 0: CEGUI */
static int direct;             /* option b: no virtual button; the proxy calls setDestination */
static int in_probe;           /* inside our own call of findWorldLocation: the cursor is virtual */
static unsigned last_order;    /* GetTickCount of the last setDestination */

/* GOG build 17870 only (docs/xbox-ui-on-pc.md, section 9): the game client and its members.
 * Names from OpenTorchlight's symbols of the Linux build (research/original-symbols.txt);
 * the addresses found in GOG's Torchlight.exe by shape, callers and callees. */
#define GOG_APP_GLOBAL 0x00c1ad64        /* the application object; +0x64 its CGameClient */
#define GOG_FIND_WORLD_LOCATION 0x00411d60 /* CGameClient::findWorldLocation(Vector3&, bool) */
#define GOG_GET_POSITION 0x00510eb0      /* CPositionableObject::getPosition(bool) */
#define GOG_SET_DESTINATION 0x00494b30   /* CCharacter::setDestination(CLevel&, float, float) */
#define GOG_SET_TARGET 0x004949d0        /* CCharacter::setTarget(CCharacter*) */
#define GOG_SET_TARGET_ITEM 0x00494ab0   /* CCharacter::setTargetItem(CItem*) */
#define GOG_FIND_CHARACTER 0x005006c0    /* CLevel::findCharacterWithinView(Matrix4 const&, EAlignment,
                                            float min, float half angle, float range, bool best,
                                            CCharacter* exclude, CSkill*) */
#define GOG_FIND_ITEM 0x00500a50         /* CLevel::findItemWithinView(Matrix4 const&, float half
                                            angle, float range) */

typedef unsigned char(__fastcall *find_world_fn)(void *client, void *edx, float *out, int flag);
typedef float *(__fastcall *get_position_fn)(void *unit, void *edx, float *out, int flag);
typedef void(__fastcall *set_destination_fn)(void *hero, void *edx, void *level, float x, float z);
typedef void(__fastcall *set_target_fn)(void *hero, void *edx, void *target);
typedef void *(__fastcall *find_character_fn)(void *level, void *edx, const float *matrix, int alignment,
                                             float min, float half_angle, float range, int best,
                                             void *exclude, void *skill);
typedef void *(__fastcall *find_item_fn)(void *level, void *edx, const float *matrix, float half_angle,
                                        float range);

/* The 360's numbers (docs/xbox-ui-on-pc.md, section 9a). */
#define TARGET_HALF_ANGLE 30.0f
#define TARGET_MIN 0.5f
#define TARGET_RANGE 7.0f
#define TARGET_RANGE_ANY 3.5f
#define ITEM_HALF_ANGLE 60.0f
#define ITEM_RANGE 3.0f
#define ALIGN_ENEMY 5
#define ALIGN_ANY 3

static float face_x = 0.0f, face_z = 1.0f; /* the hero's facing: the stick's last direction, in the world */
static void *candidate, *item_candidate;    /* the 360's +0x1BC and +0x1C4 */
static unsigned aim_until;                  /* the virtual cursor aims ahead of the hero until then */
static int aim_lx = 0, aim_ly = 1000;       /* the stick's last direction, on screen */
static unsigned prev_buttons;
#define BUTTON_LT 0x10000u                  /* the triggers, as bits of our own (XInput gives bytes) */
#define BUTTON_RT 0x20000u

/* A press or release of the left button at (x, y), client pixels. */
static void edge(HWND w, int down, long x, long y) {
    if (edges_by_message && w)
        PostMessageW(w, down ? 0x0201 /* WM_LBUTTONDOWN */ : 0x0202 /* WM_LBUTTONUP */, down ? 1 /* MK_LBUTTON */ : 0,
                     (long)((y & 0xffff) << 16 | (x & 0xffff)));
    else
        x360_inject_left(down, (float)x, (float)y);
}

#define DEAD_ZONE 240          /* of 1000: XInput's 7849 of 32767 */
#define RADIUS_PERMILLE 140    /* of the window's height: the point ahead of the hero */
#define CLICK_MS 120

static int parse_int(const char **s) {
    int v = 0, neg = 0;
    while (**s == ' ' || **s == '\t') (*s)++;
    if (**s == '-') { neg = 1; (*s)++; }
    while (**s >= '0' && **s <= '9') v = v * 10 + (*(*s)++ - '0');
    return neg ? -v : v;
}

static void poll(void) {
    unsigned now = GetTickCount(), old = buttons;
    if (now - last_poll < 50 && last_poll) return;
    last_poll = now;
    if (pad_file[0]) {
        char text[64];
        DWORD got = 0;
        const char *p = text;
        HANDLE f = CreateFileA(pad_file, 0x80000000UL, 7, 0, 3, 0, 0);
        if (f == (HANDLE)(long)-1) return;
        ReadFile(f, text, sizeof text - 1, &got, 0);
        CloseHandle(f);
        text[got] = 0;
        lx = parse_int(&p);
        ly = parse_int(&p);
        buttons = (unsigned)parse_int(&p);
    } else {
        struct { DWORD packet; WORD buttons; unsigned char lt, rt; short lx, ly, rx, ry; } st;
        if (!xinput_tried) {
            HMODULE m = LoadLibraryA("xinput1_3.dll");
            xinput_tried = 1;
            xinput_get_state = m ? (xinput_get_state_fn)GetProcAddress(m, "XInputGetState") : 0;
            x360_log(xinput_get_state ? "pad: XInput found\r\n" : "pad: no XInput\r\n");
        }
        {
            static long last_result = -1;
            long result = xinput_get_state ? (long)xinput_get_state(0, &st) : -2;
            if (result != last_result) {
                /* 0: pad 0 connected; 1167 (ERROR_DEVICE_NOT_CONNECTED): none. */
                x360_log("pad: XInputGetState(0) = ");
                x360_log_num(result);
                x360_log("\r\n");
                last_result = result;
            }
            if (result != 0) { lx = ly = 0; buttons = 0; return; }
        }
        lx = st.lx * 1000 / 32767;
        ly = st.ly * 1000 / 32767;
        buttons = st.buttons | (st.lt > 64 ? BUTTON_LT : 0) | (st.rt > 64 ? BUTTON_RT : 0);
    }
    {
        /* Transitions only: the buttons' mask, and the stick in or out of its dead zone. */
        static unsigned logged_buttons;
        static int logged_active;
        int active = lx * lx + ly * ly > DEAD_ZONE * DEAD_ZONE;
        if (buttons != logged_buttons) {
            x360_log("pad: buttons ");
            x360_log_num((long)buttons);
            x360_log("\r\n");
            logged_buttons = buttons;
        }
        if (active != logged_active) {
            x360_log(active ? "pad: stick out " : "pad: stick back ");
            x360_log_num(lx);
            x360_log(" ");
            x360_log_num(ly);
            x360_log("\r\n");
            logged_active = active;
        }
    }
    if ((buttons & 0x1000) && !(old & 0x1000)) a_pressed_at = now;
}

static int stick_active(void) { return lx * lx + ly * ly > DEAD_ZONE * DEAD_ZONE; }

static int clicking(void) { return a_pressed_at && GetTickCount() - a_pressed_at < CLICK_MS; }

static void order(void);

static BOOL __stdcall pad_get_cursor_pos(POINT *p) {
    BOOL r = real_get_cursor_pos(p);
    HWND w;
    RECT rc;
    poll();
    if (direct) {
        if (!in_probe) {
            order();
            if (!(aim_until && GetTickCount() < aim_until)) return r;
        }
    }
    if (!(stick_active() || (buttons & 0x1000) || clicking() || (direct && aim_until && GetTickCount() < aim_until))) {
        if (held) {
            /* Released: the button goes up where the real cursor is. */
            POINT c = *p;
            HWND w2 = GetActiveWindow();
            IMPORT_SCREEN_TO_CLIENT(w2, &c);
            held = 0;
            edge(w2, 0, c.x, c.y);
        }
        return r;
    }
    w = GetActiveWindow();
    if (!w || !GetClientRect(w, &rc)) return r;
    {
        long cx = (rc.right - rc.left) / 2, cy = (rc.bottom - rc.top) / 2;
        long radius = (rc.bottom - rc.top) * RADIUS_PERMILLE / 1000;
        long len2 = (long)lx * lx + (long)ly * ly;
        POINT q;
        q.x = cx;
        q.y = cy;
        if (len2 > DEAD_ZONE * DEAD_ZONE) {
            /* The direction only, at a fixed distance, as the 360 does (2 world units). */
            long len = 1;
            while (len * len < len2) len++;
            q.x = cx + lx * radius / len;
            q.y = cy - ly * radius / len;
        } else if (direct) {
            /* Aiming a skill with the stick at rest: ahead of the hero, the stick's last direction. */
            long l2 = (long)aim_lx * aim_lx + (long)aim_ly * aim_ly, len = 1;
            while (len * len < l2) len++;
            q.x = cx + aim_lx * radius / len;
            q.y = cy - aim_ly * radius / len;
        }
        {
            /* The press and the release are injected (the game starts a move or an attack on the
             * button's down event); while held, the game polls GetAsyncKeyState and the cursor. */
            int now_held = !direct && (stick_active() || clicking());
            if (now_held != held) {
                held = now_held;
                edge(w, held, q.x, q.y);
            }
        }
        ClientToScreen(w, &q);
        *p = q;
    }
    return 1;
}

static short __stdcall pad_get_async_key_state(int vk) {
    short r = real_get_async_key_state(vk);
    if (vk == 1 && !direct) {
        poll();
        if (stick_active() || clicking()) return (short)0x8000;
    }
    return r;
}

/* Point one import of the executable (by DLL and function name) at a replacement. */
static void *patch_import(const char *dll, const char *name, void *replacement) {
    unsigned char *base = (unsigned char *)GetModuleHandleA(0);
    unsigned char *nt = base + *(DWORD *)(base + 0x3c);
    DWORD dir = *(DWORD *)(nt + 0x80);
    DWORD *desc;
    for (desc = (DWORD *)(base + dir); desc[3] || desc[4]; desc += 5) {
        const char *d = (const char *)(base + desc[3]);
        const char *a = d, *b = dll;
        DWORD *names, *slots;
        while (*a && *b && (*a | 32) == (*b | 32)) a++, b++;
        if (*a || *b) continue;
        names = (DWORD *)(base + (desc[0] ? desc[0] : desc[4]));
        slots = (DWORD *)(base + desc[4]);
        for (; *names; names++, slots++) {
            const char *n, *m;
            if (*names & 0x80000000UL) continue;
            n = (const char *)(base + *names + 2);
            m = name;
            while (*n && *n == *m) n++, m++;
            if (!*n && !*m) {
                DWORD old;
                void *prev = (void *)*slots;
                VirtualProtect(slots, 4, 0x04 /* PAGE_READWRITE */, &old);
                *slots = (DWORD)replacement;
                VirtualProtect(slots, 4, old, &old);
                return prev;
            }
        }
    }
    return 0;
}

/* Option b, once per 100 ms while the stick is out: the world point under the stick's screen
 * point (the game's own findWorldLocation, with the virtual cursor only for that call) gives the
 * camera-relative direction; the hero is sent 2 world units that way, as the 360 does. */
static void note(int what, const char *s) {
    static int said[4];
    if (!said[what]) { said[what] = 1; x360_log(s); }
}

static float root(float v) {
    float g = v > 1 ? v / 2 : 1;
    int i;
    if (v <= 0) return 0;
    for (i = 0; i < 12; i++) g = 0.5f * (g + v / g);
    return g;
}

/* The 360's choice of target, every 100 ms (section 9a): an enemy in a 60-degree cone ahead of
 * the hero within 7 units, else anyone within 3.5; an item in a 120-degree cone within 3. */
/* The hero's own world matrix (Ogre's row-major Matrix4): its rotation is kept at +0xAC in GOG's
 * build (a 4x4 with a zero translation; read in a dump, section 9), its position at +0x70. */
#define GOG_UNIT_ROTATION 0xac
static void hero_matrix(unsigned char *hero, float *m) {
    const float *r = (const float *)(hero + GOG_UNIT_ROTATION);
    float pos[3];
    int i;
    ((get_position_fn)GOG_GET_POSITION)(hero, 0, pos, 1);
    for (i = 0; i < 16; i++) m[i] = r[i];
    m[3] = pos[0];
    m[7] = pos[1];
    m[11] = pos[2];
}

static void *any_candidate;                 /* anyone near, for A: talk, use */

static void choose_targets(unsigned char *hero, unsigned char *level) {
    float m[16];
    void *c, *a, *it;
    hero_matrix(hero, m);
    c = ((find_character_fn)GOG_FIND_CHARACTER)(level, 0, m, ALIGN_ENEMY, TARGET_MIN, TARGET_HALF_ANGLE,
                                                TARGET_RANGE, 1, hero, 0);
    a = c ? c : ((find_character_fn)GOG_FIND_CHARACTER)(level, 0, m, ALIGN_ANY, TARGET_MIN, TARGET_HALF_ANGLE,
                                                        TARGET_RANGE_ANY, 1, hero, 0);
    it = ((find_item_fn)GOG_FIND_ITEM)(level, 0, m, ITEM_HALF_ANGLE, ITEM_RANGE);
    if (c != candidate) {
        x360_log("pad: enemy candidate ");
        x360_log_num((long)c);
        x360_log("\r\n");
        candidate = c;
    }
    any_candidate = a;
    if (it != item_candidate) {
        x360_log("pad: item candidate ");
        x360_log_num((long)it);
        x360_log("\r\n");
        item_candidate = it;
    }
}

/* Diagnostics for X with no candidate: the same search, wide and far, and the hero's facing. */
static unsigned char *diag_level;
static void diagnose(unsigned char *hero) {
    float m[16];
    void *c;
    hero_matrix(hero, m);
    x360_log("pad: X with no enemy in the cone; facing ");
    x360_log_num((long)(m[2] * 1000)); x360_log(" "); x360_log_num((long)(m[10] * 1000));
    c = ((find_character_fn)GOG_FIND_CHARACTER)(diag_level, 0, m, ALIGN_ENEMY, 0.0f, 180.0f, 20.0f, 1, hero, 0);
    x360_log("; nearest enemy within 20: "); x360_log_num((long)c);
    x360_log("\r\n");
}

static void post_key(unsigned vk) {
    HWND w = GetActiveWindow();
    if (!w) return;
    PostMessageW(w, 0x0100 /* WM_KEYDOWN */, vk, 1);
    PostMessageW(w, 0x0101 /* WM_KEYUP */, vk, (long)0xC0000001UL);
}

/* The buttons, on their press (the 360's scheme, section 9a): X attacks the candidate, A picks up
 * the item or talks to the candidate, LB/RB and B, Y, LT, RT press the hotbar's keys 1 to 6. */
static void buttons_pressed(unsigned char *hero, unsigned pressed) {
    static const struct { unsigned bit; unsigned key; const char *name; } keys[] = {
        {0x0100, '1', "LB"}, {0x0200, '2', "RB"}, {0x2000, '3', "B"}, {0x8000, '4', "Y"},
        {BUTTON_LT, '5', "LT"}, {BUTTON_RT, '6', "RT"},
    };
    unsigned i;
    if ((pressed & 0x4000) && !candidate && diag_level) diagnose(hero);
    if ((pressed & 0x4000) && candidate) {
        x360_log("pad: X: setTarget\r\n");
        ((set_target_fn)GOG_SET_TARGET)(hero, 0, candidate);
    }
    if (pressed & 0x1000) {
        if (item_candidate) {
            x360_log("pad: A: setTargetItem\r\n");
            ((set_target_fn)GOG_SET_TARGET_ITEM)(hero, 0, item_candidate);
        } else if (any_candidate) {
            x360_log("pad: A: setTarget (interact)\r\n");
            ((set_target_fn)GOG_SET_TARGET)(hero, 0, any_candidate);
        }
    }
    for (i = 0; i < sizeof keys / sizeof keys[0]; i++)
        if (pressed & keys[i].bit) {
            x360_log("pad: ");
            x360_log(keys[i].name);
            x360_log(": hotbar key\r\n");
            aim_until = GetTickCount() + 400;
            post_key(keys[i].key);
        }
}

static void order(void) {
    unsigned now = GetTickCount();
    unsigned char *app, *client, *hero, *level;
    float hit[3], pos[3], dx, dz, len2, len;
    static int was_active;
    if (in_probe) return;
    app = *(unsigned char **)GOG_APP_GLOBAL;
    client = app ? *(unsigned char **)(app + 0x64) : 0;
    hero = client ? *(unsigned char **)(client + 0x2c) : 0;
    level = client ? *(unsigned char **)(client + 0x38) : 0;
    if (hero && level) {
        unsigned pressed = buttons & ~prev_buttons;
        prev_buttons = buttons;
        if (stick_active() && !was_active) {
            /* Moving the stick drops the target, as on the 360. */
            ((set_target_fn)GOG_SET_TARGET)(hero, 0, 0);
            ((set_target_fn)GOG_SET_TARGET_ITEM)(hero, 0, 0);
        }
        was_active = stick_active();
        diag_level = level;
        if (pressed) buttons_pressed(hero, pressed);
        if ((buttons & 0x4000) && candidate && now - last_order >= 100) {
            /* X held: keep attacking the candidate, and no move orders meanwhile. */
            ((set_target_fn)GOG_SET_TARGET)(hero, 0, candidate);
        }
        if (now - last_order >= 100) choose_targets(hero, level);
    }
    if (!stick_active() || (buttons & 0x4000) || now - last_order < 100) {
        if (now - last_order >= 100) last_order = now;
        return;
    }
    aim_lx = lx;
    aim_ly = ly;
    if (!hero || !level) { note(1, "pad: no client, hero or level\r\n"); return; }
    last_order = now;
    in_probe = 1;
    if (!((find_world_fn)GOG_FIND_WORLD_LOCATION)(client, 0, hit, hero[0x20a])) {
        in_probe = 0;
        note(2, "pad: findWorldLocation found nothing\r\n");
        return;
    }
    in_probe = 0;
    note(3, "pad: setDestination called\r\n");
    ((get_position_fn)GOG_GET_POSITION)(hero, 0, pos, 1);
    dx = hit[0] - pos[0];
    dz = hit[2] - pos[2];
    len2 = dx * dx + dz * dz;
    if (len2 < 1e-6f) return;
    len = root(len2); /* a square root without the CRT */
    face_x = dx / len;
    face_z = dz / len;
    ((set_destination_fn)GOG_SET_DESTINATION)(hero, 0, level, pos[0] + 2.0f * dx / len, pos[2] + 2.0f * dz / len);
}

void x360_pad_start(const char *file, int by_message, int direct_mode) {
    edges_by_message = by_message;
    direct = direct_mode;
    if (file && file[0]) {
        unsigned n = 0;
        while (file[n] && n < sizeof pad_file - 1) { pad_file[n] = file[n]; n++; }
        pad_file[n] = 0;
    }
    real_get_cursor_pos = (get_cursor_pos_fn)patch_import("user32.dll", "GetCursorPos", (void *)pad_get_cursor_pos);
    real_get_async_key_state =
        (get_async_key_state_fn)patch_import("user32.dll", "GetAsyncKeyState", (void *)pad_get_async_key_state);
    x360_log(real_get_cursor_pos && real_get_async_key_state ? "pad: imports patched" : "pad: imports NOT patched");
    x360_log(direct ? ", direct (setDestination)" : edges_by_message ? ", edges as window messages" : ", edges into CEGUI");
    x360_log(pad_file[0] ? ", input from " : ", input from XInput\r\n");
    if (pad_file[0]) { x360_log(pad_file); x360_log("\r\n"); }
}
