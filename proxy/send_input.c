/* send_input.exe (x86, Win32, no CRT): mouse and keyboard input for a window of the same Wine
 * prefix, through SendInput, as the game reads real input. Under Xwayland, X11's XTest does not
 * reach the game; this runs inside Wine instead (tools/send_input.py runs it with Proton's wine).
 *
 *   send_input.exe TITLE STEP...
 *     move X Y      the cursor to (X, Y) in the window's client pixels
 *     click         left button down and up (rclick: the right one)
 *     key VK        a key down and up, by virtual-key code in decimal (13 Enter, 27 Escape, ...)
 *     wait MS       sleep MS milliseconds
 *     where         print the cursor's client position and whether the window is in the foreground
 * The window is the largest visible top-level one titled exactly TITLE; if Wine has it minimized
 * (the game minimizes itself on losing the focus, though X keeps showing it), it is restored; it
 * is brought to the foreground first. --list prints every top-level window instead.
 */

typedef unsigned long DWORD;
typedef unsigned short WORD;
typedef int BOOL;
typedef void *HWND;
typedef struct { long x, y; } POINT;

#define IMPORT __declspec(dllimport)
IMPORT char *__stdcall GetCommandLineA(void);
IMPORT void __stdcall Sleep(DWORD ms);
IMPORT void __stdcall ExitProcess(unsigned code);
typedef struct { long left, top, right, bottom; } RECT;
typedef BOOL(__stdcall *ENUMPROC)(HWND w, long param);
IMPORT BOOL __stdcall EnumWindows(ENUMPROC fn, long param);
IMPORT int __stdcall GetWindowTextA(HWND w, char *out, int size);
IMPORT BOOL __stdcall IsWindowVisible(HWND w);
IMPORT BOOL __stdcall IsIconic(HWND w);
IMPORT BOOL __stdcall GetClientRect(HWND w, RECT *r);
IMPORT BOOL __stdcall ShowWindow(HWND w, int how);
IMPORT BOOL __stdcall SetForegroundWindow(HWND w);
IMPORT BOOL __stdcall ClientToScreen(HWND w, POINT *p);
IMPORT BOOL __stdcall SetCursorPos(int x, int y);
IMPORT unsigned __stdcall SendInput(unsigned n, void *inputs, int size);
IMPORT unsigned __stdcall MapVirtualKeyA(unsigned code, unsigned type);
IMPORT BOOL __stdcall GetCursorPos(POINT *p);
IMPORT BOOL __stdcall ScreenToClient(HWND w, POINT *p);
IMPORT HWND __stdcall GetForegroundWindow(void);
IMPORT void *__stdcall GetStdHandle(DWORD which);
IMPORT BOOL __stdcall WriteFile(void *f, const void *data, DWORD size, DWORD *done, void *overlapped);

/* INPUT for x86: type, then the union (MOUSEINPUT is the largest: 24 bytes). */
typedef struct {
    DWORD type;
    union {
        struct { long dx, dy; DWORD data, flags, time; void *extra; } mi;
        struct { WORD vk, scan; DWORD flags, time; void *extra; } ki;
    } u;
} INPUT;

#define INPUT_MOUSE 0
#define INPUT_KEYBOARD 1
#define MOUSEEVENTF_MOVE 0x0001
#define MOUSEEVENTF_LEFTDOWN 0x0002
#define MOUSEEVENTF_LEFTUP 0x0004
#define MOUSEEVENTF_RIGHTDOWN 0x0008
#define MOUSEEVENTF_RIGHTUP 0x0010
#define KEYEVENTF_KEYUP 0x0002

void *memset(void *d, int c, unsigned n) { unsigned char *p = d; while (n--) *p++ = (unsigned char)c; return d; }

static char *args[64];
static int nargs;

static void split(char *s) {
    while (*s && nargs < 64) {
        while (*s == ' ' || *s == '\t') s++;
        if (!*s) break;
        if (*s == '"') {
            args[nargs++] = ++s;
            while (*s && *s != '"') s++;
        } else {
            args[nargs++] = s;
            while (*s && *s != ' ' && *s != '\t') s++;
        }
        if (*s) *s++ = 0;
    }
}

static int same(const char *a, const char *b) {
    while (*a && *a == *b) a++, b++;
    return *a == *b;
}

static int num(const char *s) {
    int v = 0, neg = *s == '-';
    if (neg) s++;
    while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0');
    return neg ? -v : v;
}

static void out(const char *s) {
    DWORD n = 0, done;
    while (s[n]) n++;
    WriteFile(GetStdHandle((DWORD)-11), s, n, &done, 0);
}

static void out_num(long v) {
    char buf[13];
    int i = 12, neg = v < 0;
    unsigned long u = neg ? -v : v;
    buf[i] = 0;
    do { buf[--i] = (char)('0' + u % 10); u /= 10; } while (u);
    if (neg) buf[--i] = '-';
    out(buf + i);
}

static void mouse(DWORD flags) {
    INPUT in;
    memset(&in, 0, sizeof in);
    in.type = INPUT_MOUSE;
    in.u.mi.flags = flags;
    SendInput(1, &in, sizeof in);
}

static void key(unsigned vk, DWORD flags) {
    INPUT in;
    memset(&in, 0, sizeof in);
    in.type = INPUT_KEYBOARD;
    in.u.ki.vk = (WORD)vk;
    in.u.ki.scan = (WORD)MapVirtualKeyA(vk, 0);
    in.u.ki.flags = flags;
    SendInput(1, &in, sizeof in);
}

static const char *wanted;
static HWND best;
static long best_area;

static BOOL __stdcall consider(HWND w, long param) {
    char title[128];
    RECT r;
    long area;
    GetWindowTextA(w, title, sizeof title);
    if (param) {
        GetClientRect(w, &r);
        out("window \""); out(title); out("\""); out(IsWindowVisible(w) ? " visible" : " hidden");
        out(IsIconic(w) ? " minimized " : " "); out_num(r.right - r.left); out("x"); out_num(r.bottom - r.top);
        out("\n");
        return 1;
    }
    if (!same(title, wanted) || !IsWindowVisible(w)) return 1;
    if (IsIconic(w)) { if (!best) best = w; return 1; } /* a minimized window's client area is 0 */
    GetClientRect(w, &r);
    area = (r.right - r.left) * (r.bottom - r.top);
    if (area > best_area) best = w, best_area = area;
    return 1;
}

void __stdcall start(void) {
    HWND w;
    int i;
    split(GetCommandLineA());
    if (nargs < 2) ExitProcess(2);
    if (same(args[1], "--list")) { EnumWindows(consider, 1); ExitProcess(0); }
    wanted = args[1];
    EnumWindows(consider, 0);
    w = best;
    if (!w) ExitProcess(1);
    if (IsIconic(w)) {
        /* The game minimizes itself (for Wine) when it loses the focus, while X keeps showing it. */
        ShowWindow(w, 9 /* SW_RESTORE */);
        Sleep(1500);
    }
    SetForegroundWindow(w);
    Sleep(200);
    for (i = 2; i < nargs; i++) {
        if (same(args[i], "move") && i + 2 < nargs) {
            POINT p;
            p.x = num(args[i + 1]);
            p.y = num(args[i + 2]);
            ClientToScreen(w, &p);
            SetCursorPos(p.x, p.y);
            mouse(MOUSEEVENTF_MOVE); /* a zero move, so the game sees a mouse event at the new place */
            i += 2;
        } else if (same(args[i], "click")) {
            mouse(MOUSEEVENTF_LEFTDOWN); Sleep(80); mouse(MOUSEEVENTF_LEFTUP);
        } else if (same(args[i], "rclick")) {
            mouse(MOUSEEVENTF_RIGHTDOWN); Sleep(80); mouse(MOUSEEVENTF_RIGHTUP);
        } else if (same(args[i], "key") && i + 1 < nargs) {
            unsigned vk = (unsigned)num(args[++i]);
            key(vk, 0); Sleep(80); key(vk, KEYEVENTF_KEYUP);
        } else if (same(args[i], "where")) {
            POINT p;
            GetCursorPos(&p);
            ScreenToClient(w, &p);
            out("cursor "); out_num(p.x); out(" "); out_num(p.y);
            out(GetForegroundWindow() == w ? " foreground\n" : " NOT foreground\n");
        } else if (same(args[i], "wait") && i + 1 < nargs) {
            Sleep((DWORD)num(args[++i]));
        } else {
            ExitProcess(3);
        }
        Sleep(120);
    }
    ExitProcess(0);
}
