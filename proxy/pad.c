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
IMPORT BOOL __stdcall QueryPerformanceCounter(long long *v);
IMPORT BOOL __stdcall QueryPerformanceFrequency(long long *v);
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
#define GOG_STOP_PATHING 0x00486150     /* CCharacter::stopPathing() */
#define GOG_PERFORMING_ATTACK 0x004863b0 /* CCharacter::performingAttackLoose() */
#define GOG_PERFORMING_SKILL 0x00486550  /* CCharacter::performingSkill() */
#define GOG_IN_ATTACK_RANGE 0x00492160   /* CCharacter::inAttackRange(EATTACK_RANGE_TYPE) */
#define GOG_ATTACK 0x00491bc0            /* CCharacter::attack() */
#define GOG_SET_MODE_SLOT (0x174 / 4)    /* virtual, (int): clickLeft passes 4 before chasing */
#define GOG_CHASE_SLOT (0x1ac / 4)       /* virtual, (float 0, CLevel*): clickLeft's next call */
#define GOG_PICKUP_SLOT (0x1b4 / 4)      /* virtual, (float 0, CLevel*): clickLeft on an item, after mode 7 */
#define GOG_HOLD_POSITION 0x20a         /* byte: PC's hold-position key (Shift); with it set,
                                            setDestination attacks toward the point instead of walking */
#define GOG_SET_TARGET_DIRECTION 0x00486180 /* CCharacter::setTargetDirection(float x, float z) */
#define GOG_SET_HIGHLIGHTED_SLOT (0x100 / 4) /* virtual CCharacter::setHighlighted(bool): the unit and its equipment */
#define GOG_LEVEL_UNITS 0x5c              /* the level's unit list: [[level+0x5c]] the first node {unit, next} */
#define GOG_ALIVE 0x00485780             /* CCharacter::alive() */
#define GOG_ALIGNMENT_MATCHES 0x004eec80 /* cdecl (alignment, unit): findCharacterWithinView's filter */
#define GOG_UNIT_OK 0x0040e6e0           /* thiscall bool: findCharacterWithinView's last filter */
#define GOG_SIGHT_UNOBSTRUCTED 0x004fdd60 /* CLevel::sightUnobstructed(Vector3 const*, Vector3 const*, bool) */
#define GOG_GET_PET 0x0040e720           /* the hero's pet by index (thiscall, int) */
#define GOG_ISA 0x00480760               /* CBaseUnit::ISA(type), thiscall */
#define GOG_LEVEL_ITEMS 0x68             /* the level's item list: [[level+0x68]] the first node {item, next} */
#define GOG_FIND_ITEM 0x00500a50         /* CLevel::findItemWithinView(Matrix4 const&, float half
                                            angle, float range) */

typedef unsigned char(__fastcall *find_world_fn)(void *client, void *edx, float *out, int flag);
typedef float *(__fastcall *get_position_fn)(void *unit, void *edx, float *out, int flag);
typedef void(__fastcall *set_destination_fn)(void *hero, void *edx, void *level, float x, float z);
typedef void(__fastcall *set_target_fn)(void *hero, void *edx, void *target);
typedef void(__fastcall *void_fn)(void *hero, void *edx);
typedef void(__fastcall *set_dir_fn)(void *hero, void *edx, float x, float z);
typedef unsigned char(__fastcall *bool_fn)(void *hero, void *edx);
typedef unsigned char(__fastcall *bool_int_fn)(void *hero, void *edx, int arg);
typedef void(__fastcall *int_fn)(void *hero, void *edx, int arg);
typedef void(__fastcall *float_ptr_fn)(void *hero, void *edx, float f, void *p);

/* Diagnostics: what our own code did since the last search. */
static unsigned dest_calls, drop_calls, log_until;
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
#define BEHIND 1.5f           /* the 360's second pass: its origin 1.5 behind the hero */
#define ITEM_HALF_ANGLE 60.0f
#define ITEM_RANGE 3.0f
#define ALIGN_ENEMY 5
#define ALIGN_ANY 3

static float face_x = 0.0f, face_z = 1.0f; /* the hero's facing: the stick's last direction, in the world */
static void *candidate, *item_candidate;    /* the 360's +0x1BC and +0x1C4 */
static unsigned aim_until;                  /* the virtual cursor aims ahead of the hero until then */
static int aim_lx = 0, aim_ly = 1000;       /* the stick's last direction, on screen */
static unsigned prev_buttons;
void x360_inventory_toggle(void);
int x360_inventory_open(void);
void x360_inventory_pad(unsigned pressed, unsigned buttons, int ly);
#define BUTTON_BACK 0x0020u                 /* XInput's Back */
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
#define POLL_MS 4              /* below one frame at the game's 200 fps cap */
#define SEARCH_MS 15           /* the candidates: about every frame at 60 fps, as the 360 */

static int parse_int(const char **s) {
    int v = 0, neg = 0;
    while (**s == ' ' || **s == '\t') (*s)++;
    if (**s == '-') { neg = 1; (*s)++; }
    while (**s >= '0' && **s <= '9') v = v * 10 + (*(*s)++ - '0');
    return neg ? -v : v;
}

static void poll(void) {
    unsigned now = GetTickCount(), old = buttons;
    /* The 360 reads the pad every frame; GetCursorPos comes several times a frame. */
    if (now - last_poll < POLL_MS && last_poll) return;
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
static void *highlighted;                   /* the unit we highlighted, as the 360 marks its candidate */

static int in_list(unsigned char *level, int list, void *unit) {
    void **node = *(void ***)*(void **)(level + list);
    int n = 0;
    for (; node && n < 100000; node = (void **)node[1], n++)
        if (node[0] == unit) return 1;
    return 0;
}

static void highlight(unsigned char *level, void *unit) {
    typedef void(__fastcall *set_bool_fn)(void *self, void *edx, int on);
    if (unit == highlighted) return;
    if (highlighted && in_list(level, GOG_LEVEL_UNITS, highlighted))
        ((set_bool_fn)(*(void ***)highlighted)[GOG_SET_HIGHLIGHTED_SLOT])(highlighted, 0, 0);
    highlighted = unit;
    if (unit) ((set_bool_fn)(*(void ***)unit)[GOG_SET_HIGHLIGHTED_SLOT])(unit, 0, 1);
}

/* The 360 highlights the item A would pick up too (CItem::setHighlighted: the same slot +0x100). */
static void *highlighted_item;
static const char *class_of(void *object);
static int derives_from(void *object, const char *name);
static void highlight_item(unsigned char *level, void *item) {
    typedef void(__fastcall *set_bool_fn)(void *self, void *edx, int on);
    if (item == highlighted_item) return;
    if (highlighted_item && in_list(level, GOG_LEVEL_ITEMS, highlighted_item))
        ((set_bool_fn)(*(void ***)highlighted_item)[GOG_SET_HIGHLIGHTED_SLOT])(highlighted_item, 0, 0);
    highlighted_item = item;
    /* Barrels stay the item candidate (X breaks them) but the 360 does not highlight them. */
    if (item && !derives_from(item, ".?AVCBreakable@@"))
        ((set_bool_fn)(*(void ***)item)[GOG_SET_HIGHLIGHTED_SLOT])(item, 0, 1);
}

/* The 360's choice of target (sub_821B58C0, read in section 9e): every unit that passes PC's
 * own filters (those of findCharacterWithinView, GOG 0x5006c0), within 0.5 at any angle or within
 * 30 degrees of the hero's facing and 7 units, with a clear line of sight beyond 0.5; the nearest.
 * (The 360 also prefers units of a kind sub_822A48D0 tells, not read yet.) */
typedef unsigned char(__cdecl *align_fn)(int alignment, void *unit);
typedef void *(__fastcall *get_pet_fn)(void *hero, void *edx, int index);
typedef unsigned char(__fastcall *sight_fn)(void *level, void *edx, const float *from, const float *to, int flag);

static void *unit_in_view(unsigned char *hero, unsigned char *level, const float *m, int alignment, float range) {
    void **node = *(void ***)*(void **)(level + GOG_LEVEL_UNITS);
    void *best = 0;
    float best_d = 1e9f, tan30 = 0.57735f;
    int n = 0;
    for (; node && n < 100000; node = (void **)node[1], n++) {
        unsigned char *u = (unsigned char *)node[0];
        float p[3], dx, dz, lx, lz, d;
        if (!u || u == hero) continue;
        if (!((bool_fn)GOG_ALIVE)(u, 0)) continue;
        if (!((bool_fn)(*(void ***)u)[0x20 / 4])(u, 0)) continue;
        if (!u[0x6d] || !u[0x161]) continue;
        if (!((align_fn)GOG_ALIGNMENT_MATCHES)(alignment, u)) continue;
        /* The 360 leaves out the hero's first pet (its list at +0x5ac); GOG's getter of the hero's
         * pets is 0x40e720(index), as PC's clickLeft uses it (0x4146c2). */
        if (u == ((get_pet_fn)GOG_GET_PET)(hero, 0, 0)) continue;
        if (!((bool_fn)GOG_UNIT_OK)(u, 0)) continue;
        ((get_position_fn)GOG_GET_POSITION)(u, 0, p, 1);
        dx = p[0] - m[3];
        dz = p[2] - m[11];
        d = root(dx * dx + dz * dz);
        if (d >= best_d) continue;
        /* The hero's facing, in the world, is the third column of its rotation (m[2], m[10]):
         * the same direction the old cone used. lz: along it; lx: across it. */
        lz = m[2] * dx + m[10] * dz;
        lx = m[10] * dx - m[2] * dz;
        if (d > TARGET_MIN) {
            float from[3], to[3];
            if (!(lz > 0 && (lx < 0 ? -lx : lx) <= lz * tan30 && d <= range)) continue;
            from[0] = m[3]; from[1] = m[7] + 5.0f; from[2] = m[11];
            to[0] = p[0]; to[1] = p[1] + 5.0f; to[2] = p[2];
            if (!((sight_fn)GOG_SIGHT_UNOBSTRUCTED)(level, 0, from, to, 0)) continue;
        }
        best = u;
        best_d = d;
    }
    return best;
}

static const char *class_of(void *object) {
    unsigned char *col = object ? *(unsigned char **)(*(unsigned char **)object - 4) : 0;
    unsigned char *td = col ? *(unsigned char **)(col + 12) : 0;
    return td ? (const char *)(td + 8) : "?";
}

/* True if the object's class is NAME or derives from it (MSVC RTTI: the class hierarchy). */
static int derives_from(void *object, const char *name) {
    unsigned char *col = object ? *(unsigned char **)(*(unsigned char **)object - 4) : 0;
    unsigned char *chd = col ? *(unsigned char **)(col + 16) : 0;
    unsigned n, k;
    unsigned char **bases;
    if (!chd) return 0;
    n = *(unsigned *)(chd + 8);
    bases = *(unsigned char ***)(chd + 12);
    for (k = 0; k < n && k < 64; k++) {
        const char *b = (const char *)(*(unsigned char **)bases[k] + 8), *q = name;
        while (*b && *b == *q) b++, q++;
        if (!*b && !*q) return 1;
    }
    return 0;
}

/* The 360's item candidate (sub_821A4470): items of the level not of kind 0x1d, shown (+0x19c),
 * the virtual +0x20, not +0x19d, +0x161; first pass: within 60 degrees and 3 units, or within
 * 0.5; gold (kind 0x7a) counts 0.8 nearer; out of sight counts 10 farther; the nearest. Second
 * pass, if none: any such item within 2.0. */
static void *item_in_view(unsigned char *hero, unsigned char *level, const float *m, float *out_d) {
    int pass;
    float tan60 = 1.7320508f;
    for (pass = 0; pass < 2; pass++) {
        void **node = *(void ***)*(void **)(level + GOG_LEVEL_ITEMS);
        void *best = 0;
        float best_d = 1e9f;
        int n = 0;
        for (; node && n < 100000; node = (void **)node[1], n++) {
            unsigned char *it = (unsigned char *)node[0];
            float p[3], dx, dz, lx, lz, d;
            int ok;
            if (!it || !it[0x19c]) continue;
            if (!((bool_fn)(*(void ***)it)[0x20 / 4])(it, 0)) continue;
            /* The 360 leaves out its unit kind 0x1d here; the kind numbers differ between the builds
             * (GOG's 0x7a is not gold), and GOG's list also holds trigger units: items only. */
            if (!derives_from(it, ".?AVCItem@@")) continue;
            /* Barrels (CBreakable) stay in: the 360 highlights them and X breaks them; A leaves them
             * (buttons_pressed). Trigger units (doors, levers, chests) are worked by A. */
            if (it[0x19d] || !it[0x161]) continue;
            ((get_position_fn)GOG_GET_POSITION)(it, 0, p, 1);
            dx = p[0] - m[3];
            dz = p[2] - m[11];
            d = root(dx * dx + dz * dz);
            lz = m[2] * dx + m[10] * dz;
            lx = m[10] * dx - m[2] * dz;
            if (pass == 0) {
                int in_cone = lz > 0 && (lx < 0 ? -lx : lx) <= lz * tan60;
                ok = (in_cone && d <= ITEM_RANGE) || d <= 0.5f;
                if (derives_from(it, ".?AVCItemGold@@")) {
                    d -= 0.8f;
                    if (in_cone && d <= ITEM_RANGE) ok = 1;
                }
            } else {
                ok = d <= 2.0f;
            }
            if (!ok) continue;
            {
                float from[3], to[3];
                from[0] = m[3]; from[1] = m[7] + 5.0f; from[2] = m[11];
                to[0] = p[0]; to[1] = p[1] + 5.0f; to[2] = p[2];
                if (!((sight_fn)GOG_SIGHT_UNOBSTRUCTED)(level, 0, from, to, 0)) d += 10.0f;
            }
            if (d < best_d) { best = it; best_d = d; }
        }
        if (best) { if (out_d) *out_d = best_d; return best; }
    }
    return 0;
}

/* On a press of X or A: what is near the hero, units and items, with their class (a diagnosis). */
static void list_near(unsigned char *hero, unsigned char *level, const float *m) {
    int list;
    for (list = 0; list < 2; list++) {
        void **node = *(void ***)*(void **)(level + (list ? GOG_LEVEL_ITEMS : GOG_LEVEL_UNITS));
        int n = 0;
        for (; node && n < 100000; node = (void **)node[1], n++) {
            unsigned char *u = (unsigned char *)node[0];
            float p[3], dx, dz, d, lz, lx;
            if (!u || u == hero) continue;
            ((get_position_fn)GOG_GET_POSITION)(u, 0, p, 1);
            dx = p[0] - m[3]; dz = p[2] - m[11];
            d = root(dx * dx + dz * dz);
            if (d > 4.0f && !(list && d <= 20.0f &&
                              (derives_from(u, ".?AVCBreakable@@") || derives_from(u, ".?AVCTriggerUnit@@"))))
                continue;
            lz = m[2] * dx + m[10] * dz;
            lx = m[10] * dx - m[2] * dz;
            x360_log(list ? "pad:   item " : "pad:   unit ");
            x360_log(class_of(u));
            x360_log(" d x100 "); x360_log_num((long)(d * 100));
            x360_log(" x "); x360_log_num((long)(lx * 100));
            x360_log(" z "); x360_log_num((long)(lz * 100));
            if (list) {
                x360_log(" shown19c "); x360_log_num(u[0x19c]);
                x360_log(" v20 "); x360_log_num(((bool_fn)(*(void ***)u)[0x20 / 4])(u, 0));
                x360_log(" citem "); x360_log_num(derives_from(u, ".?AVCItem@@"));
                x360_log(" b19d "); x360_log_num(u[0x19d]);
                x360_log(" b161 "); x360_log_num(u[0x161]);
            }
            x360_log("\r\n");
        }
    }
}

static long long t_units, t_items, w_units, w_items;
static int want_items;                      /* unused: the item search runs every 0.1 s for the highlight */
static int verbose;                         /* pad_log=1: the per-search and near-list diagnosis */
static void choose_targets(unsigned char *hero, unsigned char *level) {
    float m[16];
    void *c, *a, *it;
    hero_matrix(hero, m);
    /* The 360 (sub_821EB598): an enemy in the cone within 7; else any unit in the cone within 3.5;
     * both become the candidate X acts on and A talks to. */
    {
        long long t0, t1, t2;
        QueryPerformanceCounter(&t0);
        c = unit_in_view(hero, level, m, ALIGN_ENEMY, TARGET_RANGE);
        if (!c) {
            /* The second pass searches from 1.5 behind the hero (the 360 moves the matrix's origin
             * by -1.5 times the hero's facing, its virtual +0x94, GOG's +0xa0: the third column),
             * so a unit is a candidate only within about 2 ahead: NPCs light up almost touching. */
            float b[16];
            int k;
            for (k = 0; k < 16; k++) b[k] = m[k];
            b[3] -= BEHIND * m[2];
            b[7] -= BEHIND * m[6];
            b[11] -= BEHIND * m[10];
            c = unit_in_view(hero, level, b, ALIGN_ANY, TARGET_RANGE_ANY);
        }
        QueryPerformanceCounter(&t1);
        it = item_in_view(hero, level, m, 0);
        QueryPerformanceCounter(&t2);
        t_units += t1 - t0; t_items += t2 - t1;
        if (t1 - t0 > w_units) w_units = t1 - t0;
        if (t2 - t1 > w_items) w_items = t2 - t1;
    }
    a = c;
    static unsigned last_t_log;
    if (verbose && GetTickCount() < log_until && GetTickCount() - last_t_log >= 100) {
        last_t_log = GetTickCount();
        x360_log("pad: t "); x360_log_num((long)GetTickCount());
        x360_log(" facing "); x360_log_num((long)(m[2] * 1000)); x360_log(" "); x360_log_num((long)(m[10] * 1000));
        x360_log(" enemy "); x360_log_num((long)c);
        x360_log(" hero target "); x360_log_num((long)*(void **)(hero + 0x290));
        x360_log(((bool_fn)GOG_PERFORMING_ATTACK)(hero, 0) ? " attacking\r\n" : "\r\n");
    }
    candidate = c;
    any_candidate = a;
    item_candidate = it;
}

/* X on the candidate: what PC's CGameClient::clickLeft (GOG 0x4142a0) does on an enemy. */
static unsigned char *attack_level;
static void swing_toward(unsigned char *hero, float x, float z);

/* X on the candidate, as the 360 does (sub_8221BC80 and the button's action sub_82293548 never
 * call setDestination: the hero faces the target and attacks where it stands). In range: PC's
 * attack (stopPathing, setTarget, attack); out of range: an attack in place toward the target,
 * with no target set (attack() with a target walks to it). */
static void attack_candidate(unsigned char *hero) {
    if (((bool_fn)GOG_PERFORMING_ATTACK)(hero, 0)) return;
    ((set_target_fn)GOG_SET_TARGET)(hero, 0, candidate);
    if (((bool_int_fn)GOG_IN_ATTACK_RANGE)(hero, 0, 0)) {
        if (*(int *)(hero + 0x284) == 3 || ((bool_fn)GOG_PERFORMING_SKILL)(hero, 0)) return;
        ((void_fn)GOG_STOP_PATHING)(hero, 0);
        ((void_fn)GOG_ATTACK)(hero, 0);
        x360_log("pad: X: in range, attack\r\n");
    } else {
        float t[3];
        ((get_position_fn)GOG_GET_POSITION)(candidate, 0, t, 1);
        swing_toward(hero, t[0], t[2]);
        x360_log("pad: X: out of range, attack in place toward it\r\n");
    }
}

/* X with no candidate: the 360 swings anyway, toward the stick or the hero's facing
 * (sub_8221BC80: a point ahead, setTargetDirection, the button's action). */
/* An attack in place toward (x, z): what PC's hold-position branch of setDestination (GOG
 * 0x49511b) does, called directly: setDestination drops the hold during a skill's chain (+0x2d0)
 * and then walks to the point. No target (attack() chases one); the hold byte lets attack() swing
 * with none (it checks +0x290, +0x298 and +0x20a). */
static void swing_toward(unsigned char *hero, float x, float z) {
    unsigned char hold = hero[GOG_HOLD_POSITION];
    ((set_target_fn)GOG_SET_TARGET)(hero, 0, 0);
    ((set_target_fn)GOG_SET_TARGET_ITEM)(hero, 0, 0);
    ((void_fn)GOG_STOP_PATHING)(hero, 0);
    ((set_dir_fn)GOG_SET_TARGET_DIRECTION)(hero, 0, x, z);
    hero[GOG_HOLD_POSITION] = 1;
    ((void_fn)GOG_ATTACK)(hero, 0);
    hero[GOG_HOLD_POSITION] = hold;
}

/* X on a barrel: PC's click on a breakable (clickLeft, its kind 0x1d branch): in range, setTargetItem
 * and attack(); out of range the 360's X does not walk: an attack in place toward it. */
static void break_item(unsigned char *hero) {
    if (((bool_fn)GOG_PERFORMING_ATTACK)(hero, 0)) return;
    ((set_target_fn)GOG_SET_TARGET_ITEM)(hero, 0, item_candidate);
    if (((bool_int_fn)GOG_IN_ATTACK_RANGE)(hero, 0, 0)) {
        ((void_fn)GOG_STOP_PATHING)(hero, 0);
        ((void_fn)GOG_ATTACK)(hero, 0);
        x360_log("pad: X: break in range\r\n");
    } else {
        float t[3];
        ((get_position_fn)GOG_GET_POSITION)(item_candidate, 0, t, 1);
        swing_toward(hero, t[0], t[2]);
        x360_log("pad: X: toward the barrel\r\n");
    }
}

static void swing(unsigned char *hero) {
    float m[16], dx, dz, len;
    if (!attack_level || ((bool_fn)GOG_PERFORMING_ATTACK)(hero, 0)) return;
    hero_matrix(hero, m);
    if (stick_active()) { dx = face_x; dz = face_z; }
    else { dx = m[2]; dz = m[10]; }
    len = root(dx * dx + dz * dz);
    if (len < 1e-3f) return;
    swing_toward(hero, m[3] + 2.0f * dx / len, m[11] + 2.0f * dz / len);
    x360_log("pad: X: swing\r\n");
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

static HWND game_window;                     /* the game's window, cached the first time it is active */
static void post_key(unsigned vk) {
    HWND w = GetActiveWindow();
    if (w) game_window = w;
    w = game_window;
    if (!w) { x360_log("pad: no game window for the key\r\n"); return; }
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
    if (pressed & 0x4000) log_until = GetTickCount() + 3000;
    if (pressed & 0x4000) {
        if (candidate) attack_candidate(hero);
        else if (item_candidate && derives_from(item_candidate, ".?AVCBreakable@@")) break_item(hero);
        else swing(hero);
    }
    if (pressed & 0x1000) {
        if (item_candidate && !derives_from(item_candidate, ".?AVCBreakable@@")) {
            /* PC's click on an item (clickLeft, GOG 0x4146f8): stopPathing, setTargetItem, mode 7,
             * stopPathing, the virtual +0x1b4 with (0, level): the hero walks to it and picks it up. */
            void **vt = *(void ***)hero;
            ((void_fn)GOG_STOP_PATHING)(hero, 0);
            ((set_target_fn)GOG_SET_TARGET_ITEM)(hero, 0, item_candidate);
            ((int_fn)vt[GOG_SET_MODE_SLOT])(hero, 0, 7);
            ((void_fn)GOG_STOP_PATHING)(hero, 0);
            ((float_ptr_fn)vt[GOG_PICKUP_SLOT])(hero, 0, 0.0f, attack_level);
            x360_log("pad: A: pick up ");
            x360_log(class_of(item_candidate));
            x360_log("\r\n");
        } else if (any_candidate) {
            /* The 360 talks from where the hero stands: setTarget and the mode virtual with 9; the
             * engine's approachAI (OT) then opens the NPC's dialog if it is in reach, by its kind
             * (merchant, quest, ...), or drops the target if not. PC's click also sends a
             * destination, which walks the hero: left out. */
            void **vt = *(void ***)hero;
            if (!((bool_int_fn)GOG_ISA)(any_candidate, 0, 0x83)) {
                /* PC's click talks only to units of kind 0x83 (GOG 0x414608). */
                x360_log("pad: A: unit is not one to talk to\r\n");
            } else {
                x360_log("pad: A: talk to ");
                x360_log(class_of(any_candidate));
                x360_log("\r\n");
                /* approachAI measures to the target item first if one is set: clear it. */
                ((void_fn)GOG_STOP_PATHING)(hero, 0);
                ((set_target_fn)GOG_SET_TARGET_ITEM)(hero, 0, 0);
                ((set_target_fn)GOG_SET_TARGET)(hero, 0, any_candidate);
                ((int_fn)vt[GOG_SET_MODE_SLOT])(hero, 0, 9);
            }
        } else {
            float m[16];
            void *wide;
            hero_matrix(hero, m);
            wide = ((find_item_fn)GOG_FIND_ITEM)(attack_level, 0, m, 180.0f, 20.0f);
            x360_log("pad: A: nothing; any item within 20: ");
            x360_log_num((long)wide);
            x360_log("\r\n");
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

/* The 360's movement (section 9a): the hero faces the stick's point every frame
 * (setTargetDirection) and walks toward it on a 0.1 s timer (setDestination); releasing the stick
 * stops it. A change of the stick's direction probes the world at once, so the facing follows
 * without waiting for the timer. pad_face=0 keeps the old behaviour (only setDestination), to compare. */
static int face_each_frame = 1;
static float world_x, world_z;              /* the stick's direction in the world, from the last probe */
static int probed_lx, probed_ly, have_world;
static unsigned turn_t0;                    /* a measured turn: since a direction change */
static unsigned change_t0;                  /* when the stick's direction changed, before any probe */
static float turn_x, turn_z;
static float release_pos[3];
static unsigned release_t0;

/* The 360's lock on movement (sub_8219A258, OT's performingSkillLoose shape): while a skill is
 * under way the stick sends no destination. GOG's fields are the 360's + 0xc: the skill at +0x2d0
 * (360 +0x2c4), a flag at +0x20b (+0x1ff), its timer at +0x2c0 (+0x2b4), a flag at +0x2bd (+0x2b1). */
static int skill_locks_movement(unsigned char *hero) {
    if (!*(void **)(hero + 0x2d0)) return 0;
    if (hero[0x20b]) return 1;
    return *(float *)(hero + 0x2c0) > 0.0f && !hero[0x2bd];
}

static int direction_changed(void) {
    /* More than about 5 degrees between the stick now and at the last probe. */
    long dot = (long)lx * probed_lx + (long)ly * probed_ly;
    long n1 = (long)lx * lx + (long)ly * ly, n2 = (long)probed_lx * probed_lx + (long)probed_ly * probed_ly;
    float c;
    if (!have_world || !n1 || !n2) return 1;
    c = (float)dot / (root((float)n1) * root((float)n2));
    return c < 0.996f;
}

static int probe(unsigned char *client, unsigned char *hero, const float *pos) {
    float hit[3], dx, dz, len;
    in_probe = 1;
    if (!((find_world_fn)GOG_FIND_WORLD_LOCATION)(client, 0, hit, hero[GOG_HOLD_POSITION])) {
        in_probe = 0;
        note(2, "pad: findWorldLocation found nothing\r\n");
        return 0;
    }
    in_probe = 0;
    dx = hit[0] - pos[0];
    dz = hit[2] - pos[2];
    len = root(dx * dx + dz * dz);
    if (len < 1e-3f) return 0;
    world_x = dx / len;
    world_z = dz / len;
    face_x = world_x;
    face_z = world_z;
    probed_lx = lx;
    probed_ly = ly;
    have_world = 1;
    return 1;
}

static void measure_turn(unsigned char *hero, unsigned now) {
    float m[16], dot;
    if (!turn_t0) return;
    hero_matrix(hero, m);
    dot = m[2] * turn_x + m[10] * turn_z;
    if (dot > 0.94f) {
        x360_log("pad: turn ms ");
        x360_log_num((long)(now - turn_t0));
        x360_log(face_each_frame ? " (facing every frame)\r\n" : " (setDestination only)\r\n");
        turn_t0 = 0;
    } else if (now - turn_t0 > 2000) {
        x360_log("pad: turn not done in 2 s\r\n");
        turn_t0 = 0;
    }
}

static void order(void) {
    unsigned now = GetTickCount();
    if (!game_window) game_window = GetActiveWindow();
    unsigned char *app, *client, *hero, *level;
    float pos[3];
    static int was_active;
    unsigned pressed;
    if (in_probe) return;
    app = *(unsigned char **)GOG_APP_GLOBAL;
    client = app ? *(unsigned char **)(app + 0x64) : 0;
    hero = client ? *(unsigned char **)(client + 0x2c) : 0;
    level = client ? *(unsigned char **)(client + 0x38) : 0;
    if (!hero || !level) { note(1, "pad: no client, hero or level\r\n"); return; }
    diag_level = level;
    attack_level = level;
    {
        static int said;
        if (!said) {
            said = 1;
            x360_log("pad: hero turn rate (+0x224) x100 ");
            x360_log_num((long)(*(float *)(hero + 0x224) * 100));
            x360_log("\r\n");
        }
    }
    pressed = buttons & ~prev_buttons;
    prev_buttons = buttons;
    /* Back opens and closes the 360's inventory (proxy/x360_menu.c); while it is open the pad
     * does not move or act in the world. */
    if (pressed & BUTTON_BACK) {
        x360_inventory_toggle();
        if (x360_inventory_open()) ((void_fn)GOG_STOP_PATHING)(hero, 0);
    }
    if (x360_inventory_open()) { x360_inventory_pad(pressed, buttons, ly); return; }
    ((get_position_fn)GOG_GET_POSITION)(hero, 0, pos, 1);
    measure_turn(hero, now);
    if (release_t0 && now - release_t0 >= 1000) {
        /* How far the hero went after the stick was let go. */
        float dx = pos[0] - release_pos[0], dz = pos[2] - release_pos[2];
        x360_log("pad: after release x100 ");
        x360_log_num((long)(root(dx * dx + dz * dz) * 100));
        x360_log("\r\n");
        release_t0 = 0;
    }

    if (stick_active()) {
        if (!was_active) {
            /* Moving the stick drops the target, as on the 360. */
            ((set_target_fn)GOG_SET_TARGET)(hero, 0, 0);
            ((set_target_fn)GOG_SET_TARGET_ITEM)(hero, 0, 0);
            drop_calls++;
        }
        if (!(buttons & 0x4000)) {
            int changed = direction_changed();
            if (changed && !change_t0) change_t0 = now;
            if ((changed && face_each_frame) || now - last_order >= 100) {
                float old_x = world_x, old_z = world_z;
                int had = have_world;
                int ok = probe(client, hero, pos);
                if (ok && changed &&
                    (!had || old_x * world_x + old_z * world_z < 0.866f)) {
                    turn_t0 = change_t0 ? change_t0 : now; /* a turn of more than 30 degrees: time it from the stick */
                    turn_x = world_x;
                    turn_z = world_z;
                }
                if (ok) change_t0 = 0;
            }
            if (have_world && face_each_frame)
                ((set_dir_fn)GOG_SET_TARGET_DIRECTION)(hero, 0, pos[0] + 2.0f * world_x, pos[2] + 2.0f * world_z);
            if (have_world && now - last_order >= 100 && !skill_locks_movement(hero)) {
                last_order = now;
                aim_lx = lx;
                aim_ly = ly;
                ((set_destination_fn)GOG_SET_DESTINATION)(hero, 0, level, pos[0] + 2.0f * world_x,
                                                          pos[2] + 2.0f * world_z);
                dest_calls++;
                note(3, "pad: setDestination called\r\n");
            }
        }
    } else if (was_active) {
        /* Let go: stop where the hero is. The 360 sends a last destination on release; a
         * destination at the hero's own position left a few steps of the old path in GOG, so the
         * path is cut with stopPathing, as PC's clickLeft does before an attack. */
        if (face_each_frame) ((void_fn)GOG_STOP_PATHING)(hero, 0);
        release_t0 = now;
        change_t0 = 0;
        release_pos[0] = pos[0]; release_pos[1] = pos[1]; release_pos[2] = pos[2];
        have_world = 0;
    }
    was_active = stick_active();

    /* The 360 chooses its candidate every frame and highlights it; here every SEARCH_MS, and
     * again at once on a press of X or A. */
    {
        static unsigned last_search;
        if (pressed & (0x4000 | 0x1000)) log_until = now + 3000;
        if (verbose && (pressed & (0x4000 | 0x1000))) {
            float m[16];
            hero_matrix(hero, m);
            x360_log(pressed & 0x4000 ? "pad: X pressed; near:\r\n" : "pad: A pressed; near:\r\n");
            list_near(hero, level, m);
        }
        if ((!candidate && (pressed & 0x4000)) || (pressed & 0x1000) || now - last_search >= SEARCH_MS) {
            last_search = now;
            want_items = (pressed & 0x1000) != 0;
            {
                /* The searches' cost, every 5 s in the log: they run on the game's thread. */
                static long long sum, worst, qpf;
                static unsigned count, since;
                long long t0, t1;
                QueryPerformanceCounter(&t0);
                choose_targets(hero, level);
                QueryPerformanceCounter(&t1);
                sum += t1 - t0;
                if (t1 - t0 > worst) worst = t1 - t0;
                count++;
                if (verbose && now - since >= 5000) {
                    if (!qpf) QueryPerformanceFrequency(&qpf);
                    if (since && count && qpf) {
                        /* In floating point: 64-bit division needs the CRT's __alldiv. */
                        x360_log("pad: search us avg ");
                        x360_log_num((long)((double)sum * 1e6 / (double)qpf / (double)count));
                        x360_log(" max ");
                        x360_log_num((long)((double)worst * 1e6 / (double)qpf));
                        x360_log(" n ");
                        x360_log_num((long)count);
                        x360_log(" | units max ");
                        x360_log_num((long)((double)w_units * 1e6 / (double)qpf));
                        x360_log(" items max ");
                        x360_log_num((long)((double)w_items * 1e6 / (double)qpf));
                        x360_log(" units avg ");
                        x360_log_num((long)((double)t_units * 1e6 / (double)qpf / (double)count));
                        x360_log(" items avg ");
                        x360_log_num((long)((double)t_items * 1e6 / (double)qpf / (double)count));
                        x360_log("\r\n");
                    }
                    since = now;
                    t_units = t_items = w_units = w_items = 0;
                    sum = worst = 0;
                    count = 0;
                }
            }
            want_items = 0;
            highlight(level, candidate);
            highlight_item(level, item_candidate);
        }
    }
    if (pressed) buttons_pressed(hero, pressed);
    if ((buttons & 0x4000) && !(pressed & 0x4000) && now - last_order >= 100) {
        /* X held: search again and keep attacking, as holding PC's button repeats the click. */
        last_order = now;
        if (candidate) attack_candidate(hero);
        else if (item_candidate && derives_from(item_candidate, ".?AVCBreakable@@")) break_item(hero);
        else swing(hero);
    }
}

void x360_pad_start(const char *file, int by_message, int direct_mode, int face, int log) {
    face_each_frame = face;
    verbose = log;
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
