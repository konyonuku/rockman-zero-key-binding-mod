// The binding window.
//
// The key list is drawn by hand rather than built out of controls: every row
// shows the same key from two games at once, the key itself is a button whose
// look carries meaning, and a row has to be able to switch into "waiting for a
// key" while the rest of the window stays put. Standard controls fight all
// three. The language box is drawn by hand for a different reason, set out
// where it is built. The buttons and the checkbox are ordinary controls.

#include "gui.h"

#include "config.h"
#include "keys.h"
#include "strings.h"

#include <windows.h>
#include <windowsx.h>
#include <stdio.h>
#include <string.h>

namespace {

// ---------------------------------------------------------------- appearance
const COLORREF kBg = RGB(243, 243, 243);
const COLORREF kCard = RGB(255, 255, 255);
const COLORREF kLine = RGB(229, 229, 229);
const COLORREF kLineSoft = RGB(240, 240, 240);
const COLORREF kText = RGB(26, 26, 26);
const COLORREF kDim = RGB(138, 138, 138);
const COLORREF kMid = RGB(95, 95, 95);
const COLORREF kFaint = RGB(205, 205, 205);
const COLORREF kGroupBg = RGB(250, 250, 250);
const COLORREF kWarnBg = RGB(255, 244, 206);
const COLORREF kWarnBorder = RGB(240, 195, 109);
const COLORREF kWarnText = RGB(98, 72, 0);
const COLORREF kAccent = RGB(0, 103, 192);
const COLORREF kAccentFill = RGB(229, 241, 251);
const COLORREF kRowChanged = RGB(247, 251, 255);
const COLORREF kKeyBg = RGB(252, 252, 252);
const COLORREF kKeyBorder = RGB(200, 200, 200);

// Metrics at 96 dpi; everything is scaled through S().
const int kPad = 16;
const int kWarnH = 66;
const int kSide = 190;          // language box and key list button
const int kColKey = 200;
const int kColMin = 240;        // narrowest a description column may get
const int kRowH = 28;
const int kGroupH = 30;
const int kHeadH = 30;
const int kFootH = 96;
const int kWinW = 832;
const int kWinH = 920;

// ---------------------------------------------------------------- the rows
enum RowKind { kGroupRow, kKeyRow };

struct RowDef {
    RowKind kind;
    unsigned char dik;
    int group;                  // 0..3, indexes the group names in UiText
};

const RowDef kRows[] = {
    { kGroupRow, 0, 0 },
    { kKeyRow, 0xC8, 0 }, { kKeyRow, 0xD0, 0 },
    { kKeyRow, 0xCB, 0 }, { kKeyRow, 0xCD, 0 },
    { kGroupRow, 0, 1 },
    { kKeyRow, 0x39, 1 }, { kKeyRow, 0x1E, 1 }, { kKeyRow, 0x20, 1 },
    { kKeyRow, 0x2E, 1 }, { kKeyRow, 0x2D, 1 }, { kKeyRow, 0x1F, 1 },
    { kKeyRow, 0x21, 1 },
    { kGroupRow, 0, 2 },
    { kKeyRow, 0x2C, 2 }, { kKeyRow, 0x1C, 2 }, { kKeyRow, 0x25, 2 },
    { kKeyRow, 0x32, 2 }, { kKeyRow, 0x33, 2 }, { kKeyRow, 0x34, 2 },
    { kGroupRow, 0, 3 },
    { kKeyRow, 0x0F, 3 }, { kKeyRow, 0x01, 3 },
};
const int kRowCount = int(sizeof(kRows) / sizeof(kRows[0]));

const wchar_t* GroupName(const UiText& t, int g) {
    switch (g) {
        case 0: return t.groupMove;
        case 1: return t.groupAction;
        case 2: return t.groupSubDisplay;
        default: return t.groupSystem;
    }
}

// ---------------------------------------------------------------- state
enum {
    kIdLang = 1001,
    kIdKeyList = 1002,
    kIdShowGui = 1003,
    kIdReset = 1004,
    kIdApply = 1005,
};

struct Gui {
    Config* cfg;
    Config work;                // edited copy; only applied on the way out
    bool applied;

    HWND main;
    HWND list;
    HWND langBox;
    HWND langMenu;
    HWND keyList;
    HWND showGui;
    HWND reset;
    HWND apply;

    int dpi;
    int scroll;                 // pixels the list is scrolled by
    int capture;                // row being rebound, -1 when idle
    int langHot;                // language under the pointer in the open menu

    HFONT fRegular, fBold, fKey, fKeyBold, fSmall, fTiny, fCtrl;
};

Gui g;

int S(int v) { return MulDiv(v, g.dpi, 96); }

const UiText& T() {
    int l = g.work.lang;
    return kUi[l >= 0 && l < kLangCount ? l : 0];
}

HWND g_keyListWnd;

void HideLangMenu();
void ApplyLanguageToControls();

// ---------------------------------------------------------------- drawing
void FillRound(HDC dc, RECT r, int radius, COLORREF fill, COLORREF border, int width) {
    HPEN pen = CreatePen(PS_SOLID, width, border);
    HBRUSH brush = CreateSolidBrush(fill);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, radius, radius);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);
}

void FillSolid(HDC dc, RECT r, COLORREF c) {
    HBRUSH b = CreateSolidBrush(c);
    ::FillRect(dc, &r, b);
    DeleteObject(b);
}

void HLine(HDC dc, int x1, int x2, int y, COLORREF c) {
    RECT r = { x1, y, x2, y + 1 };
    FillSolid(dc, r, c);
}

// Text anchored by one of its edges rather than by a rectangle, which is how
// the three columns line up: right edge, centre, left edge.
enum Anchor { kLeft, kCentre, kRight };

void DrawLabel(HDC dc, int x, int cy, const wchar_t* s, HFONT font,
              COLORREF colour, Anchor anchor) {
    if (!s)
        return;
    HGDIOBJ old = SelectObject(dc, font);
    SetTextColor(dc, colour);
    SetBkMode(dc, TRANSPARENT);
    SIZE sz;
    GetTextExtentPoint32W(dc, s, (int)wcslen(s), &sz);
    int tx = anchor == kLeft ? x : anchor == kRight ? x - sz.cx : x - sz.cx / 2;
    TextOutW(dc, tx, cy - sz.cy / 2, s, (int)wcslen(s));
    SelectObject(dc, old);
}

int TextWidth(HDC dc, const wchar_t* s, HFONT font) {
    HGDIOBJ old = SelectObject(dc, font);
    SIZE sz;
    GetTextExtentPoint32W(dc, s, (int)wcslen(s), &sz);
    SelectObject(dc, old);
    return sz.cx;
}

void WarningTriangle(HDC dc, int x, int y, int size, COLORREF fill, COLORREF hole) {
    POINT pts[3] = { { x + size / 2, y }, { x + size, y + size }, { x, y + size } };
    HBRUSH b = CreateSolidBrush(fill);
    HPEN p = CreatePen(PS_SOLID, 1, fill);
    HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, p);
    Polygon(dc, pts, 3);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(b);
    DeleteObject(p);
    RECT bar = { x + size / 2 - S(1), y + size / 3, x + size / 2 + S(1),
                 y + size - size / 4 };
    FillSolid(dc, bar, hole);
    RECT dot = { x + size / 2 - S(1), y + size - size / 6, x + size / 2 + S(1),
                 y + size - size / 6 + S(2) };
    FillSolid(dc, dot, hole);
}

// ---------------------------------------------------------------- dialogs
// A message box of our own. The system one is perfectly good, except that its
// buttons are labelled in whatever language Windows itself was installed in,
// while this window lets the player pick a different one. An English window
// that answers with Korean buttons reads as a fault, so the box is ours.
struct Dialog {
    const wchar_t* title;
    const wchar_t* body;        // newlines split it into lines
    const wchar_t* button[2];
    int buttonCount;
    int chosen;                 // which button was pressed, -1 if dismissed
    HWND wnd;
};

Dialog g_dlg;

enum { kIdDlgFirst = 2001 };

// Walks the body a line at a time. Measures when there is nowhere to draw,
// which is how the box learns the size it needs before it exists.
int DialogBody(HDC dc, const wchar_t* body, int x, int y, int step, bool draw) {
    int widest = 0;
    for (const wchar_t* p = body; p; ) {
        const wchar_t* nl = wcschr(p, L'\n');
        int len = nl ? int(nl - p) : int(wcslen(p));
        if (draw) {
            HGDIOBJ old = SelectObject(dc, g.fRegular);
            SetTextColor(dc, kText);
            SetBkMode(dc, TRANSPARENT);
            TextOutW(dc, x, y, p, len);
            SelectObject(dc, old);
        } else {
            HGDIOBJ old = SelectObject(dc, g.fRegular);
            SIZE sz;
            GetTextExtentPoint32W(dc, p, len, &sz);
            SelectObject(dc, old);
            if (sz.cx > widest)
                widest = sz.cx;
        }
        y += step;
        p = nl ? nl + 1 : nullptr;
    }
    return draw ? y : widest;
}

void PaintDialog(HWND hwnd, HDC target) {
    RECT client;
    GetClientRect(hwnd, &client);
    HDC dc = CreateCompatibleDC(target);
    HBITMAP bmp = CreateCompatibleBitmap(target, client.right, client.bottom);
    HGDIOBJ oldBmp = SelectObject(dc, bmp);

    FillSolid(dc, client, kBg);
    // The title is already on the caption bar; repeating it here would only
    // say the same thing twice.
    DialogBody(dc, g_dlg.body, S(20), S(24), S(19), true);

    BitBlt(target, 0, 0, client.right, client.bottom, dc, 0, 0, SRCCOPY);
    SelectObject(dc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(dc);
}

LRESULT CALLBACK DialogProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            PaintDialog(hwnd, dc);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_CTLCOLORBTN: {
            static HBRUSH bg = CreateSolidBrush(kBg);
            SetBkMode((HDC)wp, TRANSPARENT);
            return (LRESULT)bg;
        }

        case WM_COMMAND: {
            int id = LOWORD(wp);
            if (id == IDCANCEL) {
                g_dlg.chosen = -1;
                DestroyWindow(hwnd);
                return 0;
            }
            if (id >= kIdDlgFirst && id < kIdDlgFirst + g_dlg.buttonCount) {
                g_dlg.chosen = id - kIdDlgFirst;
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        }

        case WM_CLOSE:
            g_dlg.chosen = -1;
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            g_dlg.wnd = nullptr;
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// Puts the box up and does not come back until it is answered. Returns the
// index of the button pressed, or -1 if the box was simply closed. Pass null
// for the second button to get a box with only one.
int Ask(const wchar_t* title, const wchar_t* body, const wchar_t* first,
        const wchar_t* second, int defaultButton) {
    HINSTANCE inst = GetModuleHandleW(nullptr);
    g_dlg.title = title;
    g_dlg.body = body;
    g_dlg.button[0] = first;
    g_dlg.button[1] = second;
    g_dlg.buttonCount = second ? 2 : 1;
    g_dlg.chosen = -1;

    HDC dc = GetDC(g.main);
    int textW = TextWidth(dc, title, g.fBold);
    int bodyW = DialogBody(dc, body, 0, 0, 0, false);
    if (bodyW > textW)
        textW = bodyW;
    int lines = 1;
    for (const wchar_t* p = body; (p = wcschr(p, L'\n')) != nullptr; ++p)
        ++lines;
    int btnW[2] = { 0, 0 };
    for (int i = 0; i < g_dlg.buttonCount; ++i) {
        btnW[i] = TextWidth(dc, g_dlg.button[i], g.fCtrl) + S(28);
        if (btnW[i] < S(88))
            btnW[i] = S(88);
    }
    ReleaseDC(g.main, dc);

    int pad = S(20), btnH = S(32);
    int clientW = textW + pad * 2;
    int barW = btnW[0] + btnW[1] + (g_dlg.buttonCount > 1 ? S(10) : 0) + pad * 2;
    if (clientW < barW)
        clientW = barW;
    if (clientW < S(330))
        clientW = S(330);
    int bodyTop = S(24);
    int clientH = bodyTop + lines * S(19) + S(20) + btnH + pad;

    RECT want = { 0, 0, clientW, clientH };
    DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU;
    AdjustWindowRectEx(&want, style, FALSE, WS_EX_DLGMODALFRAME);
    int w = want.right - want.left, h = want.bottom - want.top;

    RECT owner;
    GetWindowRect(g.main, &owner);
    int x = owner.left + ((owner.right - owner.left) - w) / 2;
    int y = owner.top + ((owner.bottom - owner.top) - h) / 3;

    g_dlg.wnd = CreateWindowExW(WS_EX_DLGMODALFRAME, L"KeyRebindDialog", title,
                                style, x, y, w, h, g.main, nullptr, inst, nullptr);
    if (!g_dlg.wnd)
        return -1;

    int bx = clientW - pad;
    HWND focusOn = nullptr;
    for (int i = g_dlg.buttonCount - 1; i >= 0; --i) {
        bx -= btnW[i];
        HWND b = CreateWindowExW(0, L"BUTTON", g_dlg.button[i],
                                 WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                     (i == defaultButton ? BS_DEFPUSHBUTTON
                                                         : BS_PUSHBUTTON),
                                 bx, clientH - pad - btnH, btnW[i], btnH,
                                 g_dlg.wnd, (HMENU)(INT_PTR)(kIdDlgFirst + i),
                                 inst, nullptr);
        SendMessageW(b, WM_SETFONT, (WPARAM)g.fCtrl, TRUE);
        if (i == defaultButton)
            focusOn = b;
        bx -= S(10);
    }

    EnableWindow(g.main, FALSE);
    ShowWindow(g_dlg.wnd, SW_SHOW);
    UpdateWindow(g_dlg.wnd);
    if (focusOn)
        SetFocus(focusOn);

    MSG msg;
    while (g_dlg.wnd) {
        BOOL got = GetMessageW(&msg, nullptr, 0, 0);
        if (got <= 0) {
            // The application is shutting down under us; put the quit back
            // where the outer loop will find it.
            PostQuitMessage(0);
            if (g_dlg.wnd)
                DestroyWindow(g_dlg.wnd);
            break;
        }
        if (!IsDialogMessageW(g_dlg.wnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    EnableWindow(g.main, TRUE);
    SetActiveWindow(g.main);
    return g_dlg.chosen;
}

// ---------------------------------------------------------------- layout
struct Columns {
    int zeroRight;              // right edge of the Zero description
    int keyCentre;
    int zxLeft;
};

Columns LayoutColumns(int width) {
    int inner = width - S(kPad) * 2;
    int desc = (inner - S(kColKey)) / 2;
    if (desc < S(kColMin))
        desc = S(kColMin);
    Columns c;
    c.zeroRight = S(kPad) + desc - S(14);
    c.keyCentre = S(kPad) + desc + S(kColKey) / 2;
    c.zxLeft = S(kPad) + desc + S(kColKey) + S(14);
    return c;
}

int ContentHeight() {
    int h = S(kHeadH);
    for (int i = 0; i < kRowCount; ++i)
        h += kRows[i].kind == kGroupRow ? S(kGroupH) : S(kRowH);
    return h;
}

// ---------------------------------------------------------------- list child
RECT KeyButtonRect(HDC dc, int centre, int cy, const wchar_t* label, bool capturing) {
    int w;
    if (capturing) {
        w = TextWidth(dc, T().pressAKey, g.fKey) + S(34);
        if (w < S(118))
            w = S(118);
    } else {
        w = TextWidth(dc, label, g.fKeyBold) + S(30);
        if (w < S(74))
            w = S(74);
    }
    RECT r = { centre - w / 2, cy - S(12), centre + w / 2, cy + S(12) };
    return r;
}

void PaintList(HWND hwnd, HDC target) {
    RECT client;
    GetClientRect(hwnd, &client);
    int w = client.right, h = client.bottom;

    HDC dc = CreateCompatibleDC(target);
    HBITMAP bmp = CreateCompatibleBitmap(target, w, h);
    HGDIOBJ oldBmp = SelectObject(dc, bmp);

    RECT all = { 0, 0, w, h };
    FillSolid(dc, all, kCard);

    const UiText& t = T();
    Columns col = LayoutColumns(w + S(kPad) * 2);
    // The child spans the card, so shift the columns back into its own space.
    col.zeroRight -= S(kPad);
    col.keyCentre -= S(kPad);
    col.zxLeft -= S(kPad);

    int y = -g.scroll;

    DrawLabel(dc, col.zeroRight, y + S(kHeadH) / 2, t.gameZero, g.fBold, kMid, kRight);
    DrawLabel(dc, col.keyCentre, y + S(kHeadH) / 2, t.colKey, g.fBold, kMid, kCentre);
    DrawLabel(dc, col.zxLeft, y + S(kHeadH) / 2, t.gameZx, g.fBold, kMid, kLeft);
    HLine(dc, 0, w, y + S(kHeadH), kLine);
    y += S(kHeadH);

    for (int i = 0; i < kRowCount; ++i) {
        const RowDef& row = kRows[i];
        int rh = row.kind == kGroupRow ? S(kGroupH) : S(kRowH);
        if (y + rh < 0) {
            y += rh;
            continue;
        }
        if (y > h)
            break;

        if (row.kind == kGroupRow) {
            RECT r = { 0, y, w, y + rh };
            FillSolid(dc, r, kGroupBg);
            DrawLabel(dc, S(14), y + rh / 2, GroupName(t, row.group), g.fSmall,
                     kDim, kLeft);
            HLine(dc, 0, w, y + rh, kLineSoft);
            y += rh;
            continue;
        }

        bool capturing = g.capture == i;
        unsigned char phys = g.work.bind[row.dik];
        bool changed = phys != row.dik;
        if (changed && !capturing) {
            RECT r = { 0, y, w, y + rh };
            FillSolid(dc, r, kRowChanged);
        }

        int cy = y + rh / 2;
        const wchar_t* zero = ActionText(g.work.lang, row.dik, false);
        const wchar_t* zx = ActionText(g.work.lang, row.dik, true);
        DrawLabel(dc, col.zeroRight, cy, zero ? zero : L"—", g.fRegular,
                 zero ? kText : kFaint, kRight);
        if (capturing) {
            // The prompt replaces the ZX text of this row only, so the table
            // keeps its shape while a key is being picked.
            DrawLabel(dc, col.zxLeft, cy, t.rightClickCancels, g.fSmall, kDim, kLeft);
        } else {
            DrawLabel(dc, col.zxLeft, cy, zx ? zx : L"—", g.fRegular,
                     zx ? kText : kFaint, kLeft);
        }

        const wchar_t* label = KeyLabel(phys);
        RECT kb = KeyButtonRect(dc, col.keyCentre, cy, label, capturing);
        if (capturing) {
            FillRound(dc, kb, S(4), kCard, kAccent, S(2));
            DrawLabel(dc, col.keyCentre, cy, t.pressAKey, g.fRegular, kAccent, kCentre);
        } else if (changed) {
            FillRound(dc, kb, S(4), kAccentFill, kAccent, S(2));
            DrawLabel(dc, col.keyCentre, cy, label, g.fKeyBold, kAccent, kCentre);
            wchar_t hint[64];
            swprintf_s(hint, t.defaultIs, KeyLabel(row.dik));
            DrawLabel(dc, kb.left - S(9), cy, hint, g.fTiny, kDim, kRight);
        } else {
            FillRound(dc, kb, S(4), kKeyBg, kKeyBorder, 1);
            DrawLabel(dc, col.keyCentre, cy, label, g.fKey, kText, kCentre);
        }
        y += rh;
    }

    BitBlt(target, 0, 0, w, h, dc, 0, 0, SRCCOPY);
    SelectObject(dc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(dc);
}

void UpdateScrollBar(HWND hwnd) {
    RECT c;
    GetClientRect(hwnd, &c);
    SCROLLINFO si = {};
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0;
    si.nMax = ContentHeight() - 1;
    si.nPage = c.bottom;
    si.nPos = g.scroll;
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);

    int maxScroll = ContentHeight() - c.bottom;
    if (maxScroll < 0)
        maxScroll = 0;
    if (g.scroll > maxScroll)
        g.scroll = maxScroll;
}

int RowAt(int py) {
    int y = S(kHeadH) - g.scroll;
    for (int i = 0; i < kRowCount; ++i) {
        int rh = kRows[i].kind == kGroupRow ? S(kGroupH) : S(kRowH);
        if (py >= y && py < y + rh)
            return kRows[i].kind == kKeyRow ? i : -1;
        y += rh;
    }
    return -1;
}

void StopCapture() {
    if (g.capture >= 0) {
        g.capture = -1;
        InvalidateRect(g.list, nullptr, FALSE);
    }
}

// A key may only stand for one action, so a clash is refused and the player is
// told which action already holds it.
bool TryBind(int rowIndex, unsigned char phys) {
    unsigned char game = kRows[rowIndex].dik;
    for (int i = 0; i < kGameKeyCount; ++i) {
        unsigned char other = kGameKeys[i];
        if (other != game && g.work.bind[other] == phys) {
            const UiText& t = T();
            wchar_t head[160], body[400];
            swprintf_s(head, t.clashHead, KeyLabel(phys));
            const wchar_t* zero = ActionText(g.work.lang, other, false);
            const wchar_t* zx = ActionText(g.work.lang, other, true);
            swprintf_s(body, L"%s\n\n%s: %s\n%s: %s\n\n%s", head,
                       t.gameZero, zero ? zero : L"—",
                       t.gameZx, zx ? zx : L"—", t.clashHint);
            Ask(t.clashTitle, body, t.ok, nullptr, 0);
            return false;
        }
    }
    g.work.bind[game] = phys;
    return true;
}

// WM_KEYDOWN carries the PS/2 scan code, which is what DirectInput calls a DIK
// code once the extended flag is folded in as the high bit.
//
// Right shift is the one key where the two disagree. Windows marks it extended
// so that a window can tell it from the left one, but the keyboard never sends
// the E0 prefix that the flag stands for, and DirectInput accordingly calls it
// 0x36 like the raw hardware does. Folding the flag in would invent 0xB6, a
// code the game never sees, and the key would refuse to bind. Measured against
// every other bindable key: only this one needs the exception.
unsigned char DikFromMessage(WPARAM, LPARAM lp) {
    unsigned char scan = (unsigned char)((lp >> 16) & 0xFF);
    bool extended = (lp & (1 << 24)) != 0;
    if (scan == 0x36)
        extended = false;
    return extended ? (unsigned char)(scan | 0x80) : scan;
}

bool IsBindable(unsigned char dik) {
    for (int i = 0; i < kBindableKeyCount; ++i) {
        if (kBindableKeys[i].dik == dik)
            return true;
    }
    return false;
}

LRESULT CALLBACK ListProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            PaintList(hwnd, dc);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_SIZE:
            UpdateScrollBar(hwnd);
            return 0;

        case WM_GETDLGCODE:
            return g.capture >= 0 ? DLGC_WANTALLKEYS : 0;

        case WM_LBUTTONDOWN: {
            SetFocus(hwnd);
            // Losing the focus normally closes the language menu, but do not
            // rely on the focus alone: a click in here always dismisses it.
            HideLangMenu();
            int row = RowAt(GET_Y_LPARAM(lp));
            g.capture = row;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        case WM_RBUTTONDOWN:
            StopCapture();
            return 0;

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            if (g.capture < 0)
                break;
            unsigned char dik = DikFromMessage(wp, lp);
            if (IsBindable(dik)) {
                int row = g.capture;
                g.capture = -1;
                TryBind(row, dik);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }

        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wp);
            g.scroll -= delta / WHEEL_DELTA * S(kRowH) * 3;
            if (g.scroll < 0)
                g.scroll = 0;
            UpdateScrollBar(hwnd);
            SetScrollPos(hwnd, SB_VERT, g.scroll, TRUE);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        case WM_VSCROLL: {
            RECT c;
            GetClientRect(hwnd, &c);
            int maxScroll = ContentHeight() - c.bottom;
            if (maxScroll < 0)
                maxScroll = 0;
            int pos = g.scroll;
            switch (LOWORD(wp)) {
                case SB_LINEUP: pos -= S(kRowH); break;
                case SB_LINEDOWN: pos += S(kRowH); break;
                case SB_PAGEUP: pos -= c.bottom; break;
                case SB_PAGEDOWN: pos += c.bottom; break;
                case SB_THUMBTRACK:
                case SB_THUMBPOSITION: {
                    SCROLLINFO si = {};
                    si.cbSize = sizeof(si);
                    si.fMask = SIF_TRACKPOS;
                    GetScrollInfo(hwnd, SB_VERT, &si);
                    pos = si.nTrackPos;
                    break;
                }
            }
            if (pos < 0) pos = 0;
            if (pos > maxScroll) pos = maxScroll;
            if (pos != g.scroll) {
                g.scroll = pos;
                SetScrollPos(hwnd, SB_VERT, pos, TRUE);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ---------------------------------------------------------------- main window
void PaintMain(HWND hwnd, HDC target) {
    RECT client;
    GetClientRect(hwnd, &client);
    int w = client.right, h = client.bottom;

    HDC dc = CreateCompatibleDC(target);
    HBITMAP bmp = CreateCompatibleBitmap(target, w, h);
    HGDIOBJ oldBmp = SelectObject(dc, bmp);

    RECT all = { 0, 0, w, h };
    FillSolid(dc, all, kBg);

    const UiText& t = T();
    int y = S(14);
    RECT warn = { S(kPad), y, w - S(kPad) - S(kSide) - S(10), y + S(kWarnH) };
    FillRound(dc, warn, S(4), kWarnBg, kWarnBorder, 1);
    WarningTriangle(dc, S(kPad + 16), y + S(16), S(22), kWarnText, kWarnBg);
    int tx = S(kPad + 46);
    DrawLabel(dc, tx, y + S(18), t.warn1, g.fBold, kWarnText, kLeft);
    DrawLabel(dc, tx, y + S(37), t.warn2, g.fRegular, kWarnText, kLeft);
    DrawLabel(dc, tx, y + S(53), t.warn3, g.fRegular, kWarnText, kLeft);

    // frame around the list child
    RECT listFrame;
    GetWindowRect(g.list, &listFrame);
    MapWindowPoints(nullptr, hwnd, (POINT*)&listFrame, 2);
    InflateRect(&listFrame, 1, 1);
    HPEN pen = CreatePen(PS_SOLID, 1, kLine);
    HGDIOBJ op = SelectObject(dc, pen);
    HGDIOBJ ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Rectangle(dc, listFrame.left, listFrame.top, listFrame.right, listFrame.bottom);
    SelectObject(dc, op);
    SelectObject(dc, ob);
    DeleteObject(pen);

    // the note under the checkbox
    RECT cb;
    GetWindowRect(g.showGui, &cb);
    MapWindowPoints(nullptr, hwnd, (POINT*)&cb, 2);
    DrawLabel(dc, cb.left + S(24), cb.bottom + S(11), t.showNextTimeHint, g.fTiny,
             kDim, kLeft);

    BitBlt(target, 0, 0, w, h, dc, 0, 0, SRCCOPY);
    SelectObject(dc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(dc);
}

void Relayout(HWND hwnd) {
    RECT c;
    GetClientRect(hwnd, &c);
    int w = c.right, h = c.bottom;

    HideLangMenu();
    int lx = w - S(kPad) - S(kSide);
    MoveWindow(g.langBox, lx, S(14), S(kSide), S(30), TRUE);
    MoveWindow(g.keyList, lx, S(14) + S(36), S(kSide), S(30), TRUE);

    int listTop = S(14) + S(kWarnH) + S(14);
    int listBottom = h - S(kFootH) - S(14);
    if (listBottom < listTop + S(60))
        listBottom = listTop + S(60);
    MoveWindow(g.list, S(kPad) + 1, listTop + 1,
               w - S(kPad) * 2 - 2, listBottom - listTop - 2, TRUE);

    int footY = h - S(kFootH) + S(2);
    MoveWindow(g.showGui, S(kPad) + 2, footY, w - S(kPad) * 2 - 4, S(20), TRUE);

    int bw1 = S(96), bw2 = S(132), bh = S(32);
    int bx2 = w - S(kPad) - bw2;
    int bx1 = bx2 - S(10) - bw1;
    int by = h - S(kPad) - bh;
    MoveWindow(g.reset, bx1, by, bw1, bh, TRUE);
    MoveWindow(g.apply, bx2, by, bw2, bh, TRUE);

    UpdateScrollBar(g.list);
}

void ApplyLanguageToControls() {
    const UiText& t = T();
    SetWindowTextW(g.main, t.windowTitle);
    SetWindowTextW(g.keyList, t.keyListButton);
    SetWindowTextW(g.showGui, t.showNextTime);
    SetWindowTextW(g.reset, t.reset);
    SetWindowTextW(g.apply, t.applyAndClose);
    InvalidateRect(g.main, nullptr, TRUE);
    InvalidateRect(g.list, nullptr, FALSE);
    if (g_keyListWnd) {
        SetWindowTextW(g_keyListWnd, t.keyListTitle);
        InvalidateRect(g_keyListWnd, nullptr, FALSE);
    }
}

// ---------------------------------------------------------------- language box
// Drawn and driven by hand rather than being a COMBOBOX. A combo opens its
// list in a window of its own that floats above everything, and inside the
// game that window never appears - the list could still be walked with the
// arrow keys, which is how the fault gave itself away. What replaces it is an
// ordinary child window living inside our own client area, so there is nothing
// left for the game's process to hide.
const int kLangItemH = 30;

void Chevron(HDC dc, int cx, int cy, COLORREF colour) {
    POINT p[3] = { { cx - S(4), cy - S(2) },
                   { cx + S(4), cy - S(2) },
                   { cx, cy + S(3) } };
    HBRUSH b = CreateSolidBrush(colour);
    HPEN pen = CreatePen(PS_SOLID, 1, colour);
    HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, pen);
    Polygon(dc, p, 3);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(b);
    DeleteObject(pen);
}

bool LangMenuOpen() {
    return g.langMenu && IsWindowVisible(g.langMenu);
}

void HideLangMenu() {
    if (LangMenuOpen())
        ShowWindow(g.langMenu, SW_HIDE);
    if (g.langBox)
        InvalidateRect(g.langBox, nullptr, FALSE);
}

void OpenLangMenu() {
    if (!g.langMenu || !g.langBox)
        return;
    RECT b;
    GetWindowRect(g.langBox, &b);
    MapWindowPoints(nullptr, g.main, (POINT*)&b, 2);
    g.langHot = g.work.lang;
    SetWindowPos(g.langMenu, HWND_TOP, b.left, b.bottom + S(2),
                 b.right - b.left, S(kLangItemH) * kLangCount + S(8),
                 SWP_SHOWWINDOW);
    InvalidateRect(g.langMenu, nullptr, FALSE);
    InvalidateRect(g.langBox, nullptr, FALSE);
    SetFocus(g.langMenu);
}

void SetLanguage(int lang) {
    if (lang < 0 || lang >= kLangCount || lang == g.work.lang)
        return;
    g.work.lang = lang;
    ApplyLanguageToControls();
    if (g.langBox)
        InvalidateRect(g.langBox, nullptr, FALSE);
    if (g.langMenu)
        InvalidateRect(g.langMenu, nullptr, FALSE);
}

LRESULT CALLBACK LangBoxProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC target = BeginPaint(hwnd, &ps);
            RECT c;
            GetClientRect(hwnd, &c);
            HDC dc = CreateCompatibleDC(target);
            HBITMAP bmp = CreateCompatibleBitmap(target, c.right, c.bottom);
            HGDIOBJ oldBmp = SelectObject(dc, bmp);

            FillSolid(dc, c, kBg);
            bool lit = LangMenuOpen() || GetFocus() == hwnd;
            RECT box = { 0, 0, c.right, c.bottom };
            FillRound(dc, box, S(4), kCard, lit ? kAccent : kKeyBorder, 1);
            DrawLabel(dc, S(10), c.bottom / 2, T().langName, g.fCtrl, kText, kLeft);
            Chevron(dc, c.right - S(14), c.bottom / 2, kMid);

            BitBlt(target, 0, 0, c.right, c.bottom, dc, 0, 0, SRCCOPY);
            SelectObject(dc, oldBmp);
            DeleteObject(bmp);
            DeleteDC(dc);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_GETDLGCODE:
            // Up and down move through the languages the way the old combo
            // did; everything else still navigates the window normally.
            return DLGC_WANTARROWS;

        case WM_SETFOCUS:
        case WM_KILLFOCUS:
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;

        case WM_LBUTTONDOWN:
            SetFocus(hwnd);
            if (LangMenuOpen())
                HideLangMenu();
            else
                OpenLangMenu();
            return 0;

        case WM_KEYDOWN:
            if (wp == VK_DOWN && (GetKeyState(VK_MENU) & 0x8000))
                OpenLangMenu();
            else if (wp == VK_DOWN)
                SetLanguage(g.work.lang + 1);
            else if (wp == VK_UP)
                SetLanguage(g.work.lang - 1);
            else if (wp == VK_SPACE || wp == VK_F4)
                OpenLangMenu();
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT CALLBACK LangMenuProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC target = BeginPaint(hwnd, &ps);
            RECT c;
            GetClientRect(hwnd, &c);
            HDC dc = CreateCompatibleDC(target);
            HBITMAP bmp = CreateCompatibleBitmap(target, c.right, c.bottom);
            HGDIOBJ oldBmp = SelectObject(dc, bmp);

            FillSolid(dc, c, kCard);
            RECT frame = { 0, 0, c.right, c.bottom };
            FillRound(dc, frame, S(4), kCard, kKeyBorder, 1);
            for (int i = 0; i < kLangCount; ++i) {
                int top = S(4) + i * S(kLangItemH);
                RECT item = { S(3), top, c.right - S(3), top + S(kLangItemH) };
                if (i == g.langHot)
                    FillSolid(dc, item, kAccentFill);
                DrawLabel(dc, S(12), top + S(kLangItemH) / 2, kUi[i].langName,
                          g.fCtrl, kText, kLeft);
                if (i == g.work.lang)
                    DrawLabel(dc, c.right - S(14), top + S(kLangItemH) / 2,
                              L"✓", g.fCtrl, kAccent, kRight);
            }

            BitBlt(target, 0, 0, c.right, c.bottom, dc, 0, 0, SRCCOPY);
            SelectObject(dc, oldBmp);
            DeleteObject(bmp);
            DeleteDC(dc);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_GETDLGCODE:
            return DLGC_WANTALLKEYS;

        case WM_MOUSEMOVE: {
            int i = (GET_Y_LPARAM(lp) - S(4)) / S(kLangItemH);
            if (i >= 0 && i < kLangCount && i != g.langHot) {
                g.langHot = i;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            int i = (GET_Y_LPARAM(lp) - S(4)) / S(kLangItemH);
            if (i >= 0 && i < kLangCount)
                SetLanguage(i);
            HideLangMenu();
            SetFocus(g.langBox);
            return 0;
        }

        case WM_KEYDOWN:
            if (wp == VK_DOWN && g.langHot + 1 < kLangCount) {
                ++g.langHot;
                InvalidateRect(hwnd, nullptr, FALSE);
            } else if (wp == VK_UP && g.langHot > 0) {
                --g.langHot;
                InvalidateRect(hwnd, nullptr, FALSE);
            } else if (wp == VK_RETURN || wp == VK_SPACE) {
                SetLanguage(g.langHot);
                HideLangMenu();
                SetFocus(g.langBox);
            } else if (wp == VK_ESCAPE) {
                HideLangMenu();
                SetFocus(g.langBox);
            }
            return 0;

        case WM_KILLFOCUS:
            // Anything that takes the focus away closes the menu, which is how
            // a click somewhere else in the window dismisses it.
            HideLangMenu();
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void DoReset() {
    const UiText& t = T();
    wchar_t body[400];
    swprintf_s(body, L"%s\n%s\n\n%s", t.resetHead, t.resetBody, t.resetAsk);
    // No is the button in focus, so a stray Enter cannot wipe the settings.
    if (Ask(t.resetTitle, body, t.yes, t.no, 1) != 0)
        return;
    for (int i = 0; i < kGameKeyCount; ++i)
        g.work.bind[kGameKeys[i]] = kGameKeys[i];
    StopCapture();
    InvalidateRect(g.list, nullptr, FALSE);
}

void ShowKeyList();
void MakeFonts();
void FreeFonts();
void ApplyFontsToControls();

LRESULT CALLBACK MainProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            PaintMain(hwnd, dc);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN: {
            SetBkMode((HDC)wp, TRANSPARENT);
            SetTextColor((HDC)wp, kText);
            static HBRUSH bg = CreateSolidBrush(kBg);
            return (LRESULT)bg;
        }

        case WM_SIZE:
            Relayout(hwnd);
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;

        case WM_GETMINMAXINFO: {
            MINMAXINFO* mmi = (MINMAXINFO*)lp;
            mmi->ptMinTrackSize.x = S(680);
            mmi->ptMinTrackSize.y = S(420);
            return 0;
        }

        case 0x02E0: {          // WM_DPICHANGED, dragged to another display
            g.dpi = HIWORD(wp);
            FreeFonts();
            MakeFonts();
            ApplyFontsToControls();
            const RECT* want = (const RECT*)lp;
            SetWindowPos(hwnd, nullptr, want->left, want->top,
                         want->right - want->left, want->bottom - want->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            Relayout(hwnd);
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        }

        case WM_COMMAND: {
            int id = LOWORD(wp);
            if (id == kIdKeyList) {
                ShowKeyList();
                return 0;
            }
            if (id == kIdShowGui) {
                g.work.showGui = SendMessageW(g.showGui, BM_GETCHECK, 0, 0) == BST_CHECKED;
                return 0;
            }
            if (id == kIdReset) {
                DoReset();
                return 0;
            }
            if (id == kIdApply) {
                g.applied = true;
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        }

        case WM_LBUTTONDOWN:
            HideLangMenu();
            return 0;

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ---------------------------------------------------------------- key list popup
struct KeyGroup {
    const wchar_t* UiText::* name;
    const unsigned char* keys;
    int count;
};

const unsigned char kArrowKeys[] = { 0xC8, 0xD0, 0xCB, 0xCD };
const unsigned char kLetterKeys[] = {
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,
    0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26,
    0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32,
};
const unsigned char kDigitKeys[] = {
    0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
};
const unsigned char kSymbolKeys[] = {
    0x29, 0x0C, 0x0D, 0x1A, 0x1B, 0x2B, 0x27, 0x28, 0x33, 0x34, 0x35,
};
const unsigned char kSpecialKeys[] = { 0x39, 0x1C, 0x0F, 0x3A, 0x01 };
const unsigned char kModifierKeys[] = { 0x2A, 0x36, 0x1D, 0x9D, 0x38, 0xB8 };

const KeyGroup kKeyGroups[] = {
    { &UiText::catArrows, kArrowKeys, 4 },
    { &UiText::catLetters, kLetterKeys, 26 },
    { &UiText::catDigits, kDigitKeys, 10 },
    { &UiText::catSymbols, kSymbolKeys, 11 },
    { &UiText::catSpecial, kSpecialKeys, 5 },
    { &UiText::catModifiers, kModifierKeys, 6 },
};
const int kKeyGroupCount = int(sizeof(kKeyGroups) / sizeof(kKeyGroups[0]));

// Lays the chips out, and draws them when asked to. Returning the height the
// content came to lets the window be made exactly as tall as it needs to be
// instead of guessing and leaving a band of empty space at the bottom.
int LayoutKeyList(HDC dc, int w, bool draw) {
    const UiText& t = T();
    wchar_t intro[160];
    swprintf_s(intro, t.keyListIntro, kBindableKeyCount);
    if (draw)
        DrawLabel(dc, S(kPad), S(14) + S(8), intro, g.fRegular, kMid, kLeft);

    int y = S(14) + S(24);
    int chipH = S(26), gap = S(6);
    for (int gi = 0; gi < kKeyGroupCount; ++gi) {
        const KeyGroup& grp = kKeyGroups[gi];
        if (draw)
            DrawLabel(dc, S(kPad), y + S(9), t.*(grp.name), g.fSmall, kDim, kLeft);
        y += S(18);
        int x = S(kPad);
        for (int i = 0; i < grp.count; ++i) {
            const wchar_t* label = KeyLabel(grp.keys[i]);
            int cw = TextWidth(dc, label, g.fRegular) + S(18);
            if (cw < S(34))
                cw = S(34);
            if (x + cw > w - S(kPad)) {
                x = S(kPad);
                y += chipH + gap;
            }
            if (draw) {
                RECT r = { x, y, x + cw, y + chipH };
                FillRound(dc, r, S(4), kCard, kKeyBorder, 1);
                DrawLabel(dc, x + cw / 2, y + chipH / 2, label, g.fRegular,
                          kText, kCentre);
            }
            x += cw + gap;
        }
        y += chipH + gap + S(8);
    }
    return y;
}

void PaintKeyList(HWND hwnd, HDC target) {
    RECT client;
    GetClientRect(hwnd, &client);
    int w = client.right, h = client.bottom;

    HDC dc = CreateCompatibleDC(target);
    HBITMAP bmp = CreateCompatibleBitmap(target, w, h);
    HGDIOBJ oldBmp = SelectObject(dc, bmp);
    RECT all = { 0, 0, w, h };
    FillSolid(dc, all, kBg);

    LayoutKeyList(dc, w, true);

    BitBlt(target, 0, 0, w, h, dc, 0, 0, SRCCOPY);
    SelectObject(dc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(dc);
}

LRESULT CALLBACK KeyListProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            PaintKeyList(hwnd, dc);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_CTLCOLORBTN: {
            static HBRUSH bg = CreateSolidBrush(kBg);
            SetBkMode((HDC)wp, TRANSPARENT);
            return (LRESULT)bg;
        }
        case WM_COMMAND:
            if (LOWORD(wp) == IDOK) {
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            g_keyListWnd = nullptr;
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void ShowKeyList() {
    if (g_keyListWnd) {
        SetForegroundWindow(g_keyListWnd);
        return;
    }
    const UiText& t = T();
    int w = S(640);

    // Measure the chips first: the window is then only as tall as they are,
    // plus room for the button along the bottom.
    HDC measure = GetDC(g.main);
    int h = LayoutKeyList(measure, w, false) + S(46) + S(8);
    ReleaseDC(g.main, measure);

    RECT parent;
    GetWindowRect(g.main, &parent);
    int x = parent.left + ((parent.right - parent.left) - w) / 2;
    int y = parent.top + S(60);

    RECT want = { 0, 0, w, h };
    AdjustWindowRectEx(&want, WS_CAPTION | WS_SYSMENU, FALSE, 0);
    g_keyListWnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME, L"KeyRebindKeyList", t.keyListTitle,
        WS_CAPTION | WS_SYSMENU | WS_VISIBLE, x, y,
        want.right - want.left, want.bottom - want.top,
        g.main, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!g_keyListWnd)
        return;

    HWND close = CreateWindowExW(0, L"BUTTON", t.close,
                                 WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                                 w - S(kPad) - S(92), h - S(46), S(92), S(32),
                                 g_keyListWnd, (HMENU)IDOK,
                                 GetModuleHandleW(nullptr), nullptr);
    SendMessageW(close, WM_SETFONT, (WPARAM)g.fCtrl, TRUE);
}

// ---------------------------------------------------------------- setup
// A desktop DC reports 96 whatever the display is really set to once the
// process is per-monitor aware, so ask about the window instead. The system
// font follows the real dpi either way, and a mismatch shows up as text too big
// for the boxes around it.
int QueryDpi(HWND hwnd) {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        typedef UINT(WINAPI * PfnForWindow)(HWND);
        typedef UINT(WINAPI * PfnForSystem)(void);
        if (hwnd) {
            PfnForWindow forWindow =
                (PfnForWindow)GetProcAddress(user32, "GetDpiForWindow");
            if (forWindow) {
                UINT d = forWindow(hwnd);
                if (d)
                    return (int)d;
            }
        }
        PfnForSystem forSystem =
            (PfnForSystem)GetProcAddress(user32, "GetDpiForSystem");
        if (forSystem) {
            UINT d = forSystem();
            if (d)
                return (int)d;
        }
    }
    HDC dc = GetDC(nullptr);
    int d = GetDeviceCaps(dc, LOGPIXELSY);
    ReleaseDC(nullptr, dc);
    return d ? d : 96;
}

HFONT MakeFont(int px, bool bold) {
    LOGFONTW lf = {};
    NONCLIENTMETRICSW ncm = {};
    ncm.cbSize = sizeof(ncm);
    if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0))
        lf = ncm.lfMessageFont;
    else
        wcscpy_s(lf.lfFaceName, L"Segoe UI");
    lf.lfHeight = -S(px);
    lf.lfWeight = bold ? FW_SEMIBOLD : FW_NORMAL;
    lf.lfQuality = CLEARTYPE_QUALITY;
    return CreateFontIndirectW(&lf);
}

void MakeFonts() {
    g.fRegular = MakeFont(12, false);
    g.fBold = MakeFont(12, true);
    g.fKey = MakeFont(13, false);
    g.fKeyBold = MakeFont(13, true);
    g.fSmall = MakeFont(11, false);
    g.fTiny = MakeFont(10, false);
    g.fCtrl = MakeFont(13, false);
}

void ApplyFontsToControls() {
    HWND controls[] = { g.keyList, g.showGui, g.reset, g.apply };
    for (int i = 0; i < 4; ++i) {
        if (controls[i])
            SendMessageW(controls[i], WM_SETFONT, (WPARAM)g.fCtrl, TRUE);
    }
}

// The window wants to be tall. Where the display cannot take that, it stops at
// the work area and the list scrolls instead.
void SizeToFit(HWND hwnd, bool centre) {
    RECT work = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    int workW = work.right - work.left, workH = work.bottom - work.top;

    RECT want = { 0, 0, S(kWinW), S(kWinH) };
    AdjustWindowRectEx(&want, WS_OVERLAPPEDWINDOW, FALSE, 0);
    int w = want.right - want.left, h = want.bottom - want.top;
    if (w > workW)
        w = workW;
    if (h > workH)
        h = workH;

    UINT flags = SWP_NOZORDER | SWP_NOACTIVATE;
    int x = work.left + (workW - w) / 2;
    int y = work.top + (workH - h) / 2;
    if (!centre)
        flags |= SWP_NOMOVE;
    SetWindowPos(hwnd, nullptr, x, y, w, h, flags);
}

void FreeFonts() {
    DeleteObject(g.fRegular);
    DeleteObject(g.fBold);
    DeleteObject(g.fKey);
    DeleteObject(g.fKeyBold);
    DeleteObject(g.fSmall);
    DeleteObject(g.fTiny);
    DeleteObject(g.fCtrl);
}

void RegisterClasses(HINSTANCE inst) {
    static bool done = false;
    if (done)
        return;
    done = true;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;

    wc.lpfnWndProc = MainProc;
    wc.lpszClassName = L"KeyRebindWindow";
    RegisterClassExW(&wc);

    wc.lpfnWndProc = ListProc;
    wc.lpszClassName = L"KeyRebindList";
    RegisterClassExW(&wc);

    wc.lpfnWndProc = KeyListProc;
    wc.lpszClassName = L"KeyRebindKeyList";
    RegisterClassExW(&wc);

    wc.lpfnWndProc = LangBoxProc;
    wc.lpszClassName = L"KeyRebindLangBox";
    RegisterClassExW(&wc);

    wc.lpfnWndProc = LangMenuProc;
    wc.lpszClassName = L"KeyRebindLangMenu";
    RegisterClassExW(&wc);

    wc.lpfnWndProc = DialogProc;
    wc.lpszClassName = L"KeyRebindDialog";
    RegisterClassExW(&wc);
}

}  // namespace

bool GuiRun(Config& cfg) {
    HINSTANCE inst = GetModuleHandleW(nullptr);
    memset(&g, 0, sizeof(g));
    g.cfg = &cfg;
    g.work = cfg;
    g.capture = -1;
    g.dpi = 96;

    RegisterClasses(inst);

    g.dpi = QueryDpi(nullptr);
    MakeFonts();

    g.main = CreateWindowExW(0, L"KeyRebindWindow", T().windowTitle,
                             WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT,
                             S(kWinW), S(kWinH), nullptr, nullptr, inst, nullptr);
    if (!g.main) {
        FreeFonts();
        return false;
    }

    // Now that there is a window, ask it what the display is really set to.
    int real = QueryDpi(g.main);
    if (real != g.dpi) {
        g.dpi = real;
        FreeFonts();
        MakeFonts();
    }

    g.langBox = CreateWindowExW(0, L"KeyRebindLangBox", nullptr,
                                WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS,
                                0, 0, 10, 10, g.main, (HMENU)kIdLang, inst, nullptr);

    g.keyList = CreateWindowExW(0, L"BUTTON", T().keyListButton,
                                WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS |
                                    BS_PUSHBUTTON,
                                0, 0, 10, 10, g.main, (HMENU)kIdKeyList, inst, nullptr);
    g.list = CreateWindowExW(0, L"KeyRebindList", nullptr,
                             WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_VSCROLL,
                             0, 0, 10, 10, g.main, nullptr, inst, nullptr);
    g.showGui = CreateWindowExW(0, L"BUTTON", T().showNextTime,
                                WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS |
                                    BS_AUTOCHECKBOX,
                                0, 0, 10, 10, g.main, (HMENU)kIdShowGui, inst, nullptr);
    g.reset = CreateWindowExW(0, L"BUTTON", T().reset,
                              WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                              0, 0, 10, 10, g.main, (HMENU)kIdReset, inst, nullptr);
    g.apply = CreateWindowExW(0, L"BUTTON", T().applyAndClose,
                              WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                              0, 0, 10, 10, g.main, (HMENU)kIdApply, inst, nullptr);

    g.langMenu = CreateWindowExW(0, L"KeyRebindLangMenu", nullptr,
                                 WS_CHILD | WS_CLIPSIBLINGS,
                                 0, 0, 10, 10, g.main, nullptr, inst, nullptr);

    ApplyFontsToControls();
    SendMessageW(g.showGui, BM_SETCHECK,
                 g.work.showGui ? BST_CHECKED : BST_UNCHECKED, 0);

    SizeToFit(g.main, true);
    Relayout(g.main);
    ShowWindow(g.main, SW_SHOW);
    UpdateWindow(g.main);
    SetFocus(g.list);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(g.main, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (g.applied)
        cfg = g.work;
    FreeFonts();
    return g.applied;
}
