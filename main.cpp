// ============================================================
//  Emergency Response & Ambulance Dispatch Simulator
//  main.cpp  -  Windows GUI (native Win32 API, no extra libraries)
// ============================================================
#ifdef UNICODE
#undef UNICODE
#endif
#ifdef _UNICODE
#undef _UNICODE
#endif

#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstring>
#include "dispatch_system.h"

#ifdef _MSC_VER
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

#ifndef IDC_HAND
#define IDC_HAND MAKEINTRESOURCE(32649)
#endif

// ---------------- control IDs ----------------
enum {
    ID_TYPE = 100, ID_LOCATION, ID_SEVERITY, ID_BTN_ADDCASE,
    ID_AMB_LOCATION, ID_BTN_ADDAMB,
    ID_BTN_NEXT, ID_BTN_ALL, ID_BTN_COMPLETE,
    ID_LV_PENDING, ID_LV_AMB, ID_LV_ACTIVE, ID_LV_HISTORY,
    ID_LOG, ID_MAP
};

// ---------------- colours ----------------
const COLORREF C_BG        = RGB(232, 239, 248);   // window background
const COLORREF C_HEADING   = RGB(20, 50, 110);     // section headings
const COLORREF C_SKY       = C_BG;                 // the three top boxes use the same light blue as the window
const COLORREF C_SKY_EDGE  = RGB(70, 130, 180);
const COLORREF C_MAP_BG    = RGB(245, 248, 255);
const COLORREF C_ROAD      = RGB(176, 186, 202);
const COLORREF C_ROUTE     = RGB(37, 99, 235);
const COLORREF C_NODE      = RGB(31, 97, 177);
const COLORREF C_EMERGENCY = RGB(214, 48, 49);
const COLORREF C_SELECTED  = RGB(255, 140, 0);
const COLORREF C_AMB_FREE  = RGB(34, 170, 84);
const COLORREF C_AMB_BUSY  = RGB(214, 48, 49);

// ---------------- globals ----------------
DispatchSystem sys;

HWND hType, hLocation, hSeverity, hAmbLocation;
HWND hPending, hAmbList, hActive, hHistory, hLog, hMap;
HFONT hFontUI, hFontBold, hFontMono, hFontMap, hFontMapBold;
HBRUSH hBgBrush, hLogBrush, hSkyBrush;
std::vector<HWND> headings;

// map state
int              selectedNode = -1;     // location clicked on the map
std::vector<int> mapRoute;              // route highlighted on the map

// screen position of every location on the map (same order as the graph)
const POINT nodePos[10] = {
    { 200, 190 },  // 0 Saddar
    {  90, 240 },  // 1 Clifton
    { 250, 270 },  // 2 DHA
    { 440, 250 },  // 3 Korangi
    { 555, 200 },  // 4 Landhi
    { 550,  55 },  // 5 Malir
    { 470, 120 },  // 6 Airport
    { 330,  85 },  // 7 Gulshan-e-Iqbal
    { 170, 100 },  // 8 North Nazimabad
    {  60,  50 }   // 9 Orangi Town
};

void refreshAll();

// ---------------- small helpers ----------------
HWND makeLabel(HWND parent, const char* text, int x, int y, int w, int h) {
    return CreateWindowA("STATIC", text, WS_CHILD | WS_VISIBLE, x, y, w, h, parent, 0, 0, 0);
}
HWND makeHeading(HWND parent, const char* text, int x, int y, int w, int h) {
    HWND l = makeLabel(parent, text, x, y, w, h);
    headings.push_back(l);
    return l;
}
struct GroupInfo { RECT r; const char* title; };
std::vector<GroupInfo> groups;               // sky-blue boxes, painted by the panel
void addGroup(const char* title, int x, int y, int w, int h) {
    GroupInfo g;
    g.r.left = x; g.r.top = y; g.r.right = x + w; g.r.bottom = y + h;
    g.title = title;
    groups.push_back(g);
}
HWND makeButton(HWND parent, const char* text, int id, int x, int y, int w, int h) {
    return CreateWindowA("BUTTON", text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                         x, y, w, h, parent, (HMENU)(INT_PTR)id, 0, 0);
}
HWND makeCombo(HWND parent, int id, int x, int y, int w) {
    return CreateWindowA("COMBOBOX", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST,
                         x, y, w, 200, parent, (HMENU)(INT_PTR)id, 0, 0);
}
void comboAdd(HWND combo, const std::string& s) {
    SendMessageA(combo, CB_ADDSTRING, 0, (LPARAM)s.c_str());
}
HWND makeListView(HWND parent, int id, int x, int y, int w, int h,
                  const char* cols[], const int widths[], int n) {
    HWND lv = CreateWindowA(WC_LISTVIEWA, "",
                            WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP |
                            LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                            x, y, w, h, parent, (HMENU)(INT_PTR)id, 0, 0);
    SendMessageA(lv, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    for (int i = 0; i < n; i++) {
        LVCOLUMNA c;
        ZeroMemory(&c, sizeof(c));
        c.mask = LVCF_TEXT | LVCF_WIDTH;
        c.pszText = (LPSTR)cols[i];
        c.cx = widths[i];
        SendMessageA(lv, LVM_INSERTCOLUMNA, i, (LPARAM)&c);
    }
    return lv;
}
void lvAddRow(HWND lv, const std::vector<std::string>& cells) {
    int row = (int)SendMessageA(lv, LVM_GETITEMCOUNT, 0, 0);
    LVITEMA it;
    ZeroMemory(&it, sizeof(it));
    it.mask = LVIF_TEXT;
    it.iItem = row;
    it.pszText = (LPSTR)cells[0].c_str();
    SendMessageA(lv, LVM_INSERTITEMA, 0, (LPARAM)&it);
    for (size_t c = 1; c < cells.size(); c++) {
        LVITEMA sub;
        ZeroMemory(&sub, sizeof(sub));
        sub.iSubItem = (int)c;
        sub.pszText = (LPSTR)cells[c].c_str();
        SendMessageA(lv, LVM_SETITEMTEXTA, row, (LPARAM)&sub);
    }
}
void clearList(HWND lv) { SendMessageA(lv, LVM_DELETEALLITEMS, 0, 0); }

// show text in the "Latest Route" box (converts \n to \r\n)
void showText(const std::string& text) {
    std::string t;
    for (size_t i = 0; i < text.size(); i++) {
        if (text[i] == '\n') t += "\r\n"; else t += text[i];
    }
    SetWindowTextA(hLog, t.c_str());
}

BOOL CALLBACK setFontProc(HWND h, LPARAM f) {
    SendMessageA(h, WM_SETFONT, (WPARAM)f, TRUE);
    return TRUE;
}

// ============================================================
//  Road network map (custom-drawn window)
// ============================================================
bool onRoute(int u, int v) {
    for (size_t i = 0; i + 1 < mapRoute.size(); i++)
        if ((mapRoute[i] == u && mapRoute[i + 1] == v) ||
            (mapRoute[i] == v && mapRoute[i + 1] == u)) return true;
    return false;
}

void fillRectColor(HDC dc, const RECT& r, COLORREF c) {
    HBRUSH b = CreateSolidBrush(c);
    FillRect(dc, &r, b);
    DeleteObject(b);
}

void drawLegend(HDC dc, const RECT& rc) {
    struct Item { int kind; COLORREF color; const char* text; };
    const Item items[] = {
        { 0, C_AMB_FREE,  "Available ambulance" },
        { 0, C_AMB_BUSY,  "Busy ambulance" },
        { 1, C_NODE,      "Location" },
        { 1, C_EMERGENCY, "Emergency" },
        { 2, C_SELECTED,  "Selected" },
        { 3, C_ROUTE,     "Route" }
    };
    int y = rc.bottom - 18;
    int x = 10;
    SelectObject(dc, hFontMap);
    SetTextColor(dc, RGB(50, 60, 80));
    for (int i = 0; i < 6; i++) {
        const Item& it = items[i];
        HBRUSH b = CreateSolidBrush(it.color);
        HPEN p = CreatePen(PS_SOLID, it.kind == 3 ? 4 : 2, it.color);
        HGDIOBJ ob = SelectObject(dc, b);
        HGDIOBJ op = SelectObject(dc, p);
        if (it.kind == 0) {                       // square
            RECT r = { x, y + 2, x + 10, y + 12 };
            FillRect(dc, &r, b);
        } else if (it.kind == 1) {                // filled circle
            Ellipse(dc, x, y + 1, x + 12, y + 13);
        } else if (it.kind == 2) {                // ring
            SelectObject(dc, GetStockObject(NULL_BRUSH));
            Ellipse(dc, x, y + 1, x + 12, y + 13);
        } else {                                  // line
            MoveToEx(dc, x, y + 7, 0);
            LineTo(dc, x + 14, y + 7);
        }
        SelectObject(dc, ob);
        SelectObject(dc, op);
        DeleteObject(b);
        DeleteObject(p);

        x += 18;
        SIZE sz;
        GetTextExtentPoint32A(dc, it.text, (int)strlen(it.text), &sz);
        TextOutA(dc, x, y, it.text, (int)strlen(it.text));
        x += sz.cx + 14;
    }
}

void drawMap(HDC dc, const RECT& rc) {
    fillRectColor(dc, rc, C_MAP_BG);
    HBRUSH frame = CreateSolidBrush(RGB(150, 160, 180));
    FrameRect(dc, &rc, frame);
    DeleteObject(frame);
    SetBkMode(dc, TRANSPARENT);

    int n = sys.city.size();

    // ---- roads (normal first, route on top) ----
    for (int pass = 0; pass < 2; pass++) {
        for (int u = 0; u < n; u++) {
            const std::vector<Edge>& nb = sys.city.neighbors(u);
            for (size_t k = 0; k < nb.size(); k++) {
                int v = nb[k].to;
                if (v < u) continue;                       // draw each road once
                bool hot = onRoute(u, v);
                if ((pass == 1) != hot) continue;
                HPEN p = CreatePen(PS_SOLID, hot ? 5 : 2, hot ? C_ROUTE : C_ROAD);
                HGDIOBJ old = SelectObject(dc, p);
                MoveToEx(dc, nodePos[u].x, nodePos[u].y, 0);
                LineTo(dc, nodePos[v].x, nodePos[v].y);
                SelectObject(dc, old);
                DeleteObject(p);
            }
        }
    }

    // ---- distance labels ----
    SelectObject(dc, hFontMapBold);
    for (int u = 0; u < n; u++) {
        const std::vector<Edge>& nb = sys.city.neighbors(u);
        for (size_t k = 0; k < nb.size(); k++) {
            int v = nb[k].to;
            if (v < u) continue;
            std::string t = DispatchSystem::num(nb[k].weight) + " km";
            SIZE sz;
            GetTextExtentPoint32A(dc, t.c_str(), (int)t.size(), &sz);
            int mx = (nodePos[u].x + nodePos[v].x) / 2;
            int my = (nodePos[u].y + nodePos[v].y) / 2;
            RECT r = { mx - sz.cx / 2 - 3, my - sz.cy / 2, mx + sz.cx / 2 + 3, my + sz.cy / 2 };
            bool hot = onRoute(u, v);
            fillRectColor(dc, r, hot ? RGB(219, 234, 254) : C_MAP_BG);
            SetTextColor(dc, hot ? RGB(20, 60, 160) : RGB(70, 80, 100));
            DrawTextA(dc, t.c_str(), (int)t.size(), &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
    }

    // ---- locations that have an unfinished emergency ----
    bool emergency[10] = { false };
    for (size_t i = 0; i < sys.cases.size(); i++)
        if (sys.cases[i].status != COMPLETED) emergency[sys.cases[i].location] = true;

    // ---- location circles and names ----
    for (int i = 0; i < n; i++) {
        int x = nodePos[i].x, y = nodePos[i].y;
        if (i == selectedNode) {
            HPEN ring = CreatePen(PS_SOLID, 3, C_SELECTED);
            HGDIOBJ op = SelectObject(dc, ring);
            HGDIOBJ ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
            Ellipse(dc, x - 16, y - 16, x + 16, y + 16);
            SelectObject(dc, op);
            SelectObject(dc, ob);
            DeleteObject(ring);
        }
        HBRUSH b = CreateSolidBrush(emergency[i] ? C_EMERGENCY : C_NODE);
        HPEN p = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
        HGDIOBJ ob = SelectObject(dc, b);
        HGDIOBJ op = SelectObject(dc, p);
        Ellipse(dc, x - 11, y - 11, x + 11, y + 11);
        SelectObject(dc, ob);
        SelectObject(dc, op);
        DeleteObject(b);
        DeleteObject(p);

        const std::string& nm = sys.city.name(i);
        SIZE sz;
        GetTextExtentPoint32A(dc, nm.c_str(), (int)nm.size(), &sz);
        RECT r = { x - sz.cx / 2 - 2, y + 14, x + sz.cx / 2 + 2, y + 14 + sz.cy };
        fillRectColor(dc, r, C_MAP_BG);
        SetTextColor(dc, RGB(30, 40, 70));
        DrawTextA(dc, nm.c_str(), (int)nm.size(), &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // ---- ambulances (squares with labels) ----
    int perNode[10] = { 0 };
    for (size_t i = 0; i < sys.ambulances.size(); i++) {
        const Ambulance& a = sys.ambulances[i];
        int k = perNode[a.location]++;
        int x = nodePos[a.location].x, y = nodePos[a.location].y;
        int sx = (x > 450) ? x - 26 - k * 22 : x + 12 + k * 22;
        int sy = y - 24;
        COLORREF c = a.available ? C_AMB_FREE : C_AMB_BUSY;
        RECT sq = { sx, sy, sx + 14, sy + 14 };
        fillRectColor(dc, sq, c);
        std::string t = "A" + DispatchSystem::num(a.id);
        RECT tr = { sx - 10, sy - 16, sx + 24, sy - 1 };
        SetTextColor(dc, c);
        DrawTextA(dc, t.c_str(), (int)t.size(), &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    drawLegend(dc, rc);
}

// clicking a location previews the route of the nearest available ambulance
void onMapClick(int mx, int my) {
    int hit = -1;
    for (int i = 0; i < sys.city.size(); i++) {
        int dx = mx - nodePos[i].x, dy = my - nodePos[i].y;
        if (dx * dx + dy * dy <= 16 * 16) { hit = i; break; }
    }
    if (hit < 0) {
        selectedNode = -1;
        mapRoute.clear();
        showText("Click a location on the map to preview the nearest available ambulance route.");
    } else {
        selectedNode = hit;
        int amb, km;
        std::vector<int> route;
        if (sys.previewRoute(hit, amb, km, route)) {
            mapRoute = route;
            showText("MAP SELECTION: " + sys.city.name(hit) + "\n" +
                     "Nearest available ambulance: #" + DispatchSystem::num(amb) +
                     " (at " + sys.city.name(route[0]) + ")\n" +
                     "Distance : " + DispatchSystem::num(km) + " km\n" +
                     "Route    : " + sys.routeText(route));
        } else {
            mapRoute.clear();
            showText("MAP SELECTION: " + sys.city.name(hit) + "\nNo ambulance is available right now.");
        }
    }
    InvalidateRect(hMap, 0, FALSE);
}

LRESULT CALLBACK MapProc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC wdc = BeginPaint(w, &ps);
        RECT rc;
        GetClientRect(w, &rc);
        HDC dc = CreateCompatibleDC(wdc);                   // draw off-screen: no flicker
        HBITMAP bmp = CreateCompatibleBitmap(wdc, rc.right, rc.bottom);
        HGDIOBJ old = SelectObject(dc, bmp);
        drawMap(dc, rc);
        BitBlt(wdc, 0, 0, rc.right, rc.bottom, dc, 0, 0, SRCCOPY);
        SelectObject(dc, old);
        DeleteObject(bmp);
        DeleteDC(dc);
        EndPaint(w, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_LBUTTONDOWN:
        onMapClick((short)LOWORD(lp), (short)HIWORD(lp));
        return 0;
    }
    return DefWindowProcA(w, msg, wp, lp);
}

// ============================================================
//  Coloured buttons and table rows
// ============================================================
COLORREF buttonColor(int id) {
    switch (id) {
        case ID_BTN_ADDCASE: return RGB(41, 98, 185);
        case ID_BTN_ADDAMB:  return RGB(0, 128, 128);
        case ID_BTN_NEXT:    return RGB(200, 50, 50);
        case ID_BTN_ALL:     return RGB(230, 126, 34);
        default:             return RGB(39, 150, 80);     // complete
    }
}

void drawButton(const DRAWITEMSTRUCT* d) {
    COLORREF c = buttonColor((int)d->CtlID);
    if (d->itemState & ODS_SELECTED)
        c = RGB(GetRValue(c) * 3 / 4, GetGValue(c) * 3 / 4, GetBValue(c) * 3 / 4);
    FillRect(d->hDC, &d->rcItem, hSkyBrush);
    HBRUSH b = CreateSolidBrush(c);
    HPEN p = CreatePen(PS_SOLID, 1, c);
    HGDIOBJ ob = SelectObject(d->hDC, b);
    HGDIOBJ op = SelectObject(d->hDC, p);
    RoundRect(d->hDC, d->rcItem.left, d->rcItem.top, d->rcItem.right, d->rcItem.bottom, 10, 10);
    SelectObject(d->hDC, ob);
    SelectObject(d->hDC, op);
    DeleteObject(b);
    DeleteObject(p);

    char text[64];
    GetWindowTextA(d->hwndItem, text, sizeof(text));
    SetBkMode(d->hDC, TRANSPARENT);
    SetTextColor(d->hDC, RGB(255, 255, 255));
    HGDIOBJ of = SelectObject(d->hDC, hFontBold);
    RECT r = d->rcItem;
    DrawTextA(d->hDC, text, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(d->hDC, of);
    if (d->itemState & ODS_FOCUS) {
        RECT f = d->rcItem;
        InflateRect(&f, -4, -4);
        DrawFocusRect(d->hDC, &f);
    }
}

// colours for a table row, chosen from one key column
bool rowColors(int lvId, HWND lv, int row, COLORREF& bk, COLORREF& fg) {
    int col = (lvId == ID_LV_AMB) ? 2 : (lvId == ID_LV_HISTORY) ? 1 : 3;
    char buf[32];
    buf[0] = 0;
    LVITEMA it;
    ZeroMemory(&it, sizeof(it));
    it.iSubItem = col;
    it.pszText = buf;
    it.cchTextMax = sizeof(buf);
    SendMessageA(lv, LVM_GETITEMTEXTA, row, (LPARAM)&it);
    std::string s = buf;
    if (s == "Critical")   { bk = RGB(255, 205, 205); fg = RGB(150, 0, 0);   return true; }
    if (s == "High")       { bk = RGB(255, 226, 190); fg = RGB(150, 70, 0);  return true; }
    if (s == "Medium")     { bk = RGB(255, 246, 190); fg = RGB(120, 100, 0); return true; }
    if (s == "Low")        { bk = RGB(209, 240, 214); fg = RGB(20, 100, 40); return true; }
    if (s == "Available")  { bk = RGB(209, 240, 214); fg = RGB(20, 100, 40); return true; }
    if (s == "Busy")       { bk = RGB(255, 205, 205); fg = RGB(150, 0, 0);   return true; }
    if (s == "Dispatched") { bk = RGB(214, 232, 255); fg = RGB(20, 70, 160); return true; }
    if (s == "Completed")  { bk = RGB(209, 240, 214); fg = RGB(20, 100, 40); return true; }
    return false;
}

// ---------------- refresh the tables and the map ----------------
void caseRow(HWND lv, const EmergencyCase& c, bool withAmbulance) {
    std::vector<std::string> row;
    row.push_back(DispatchSystem::num(c.id));
    row.push_back(c.type);
    row.push_back(sys.city.name(c.location));
    row.push_back(DispatchSystem::severityName(c.severity));
    if (withAmbulance) {
        row.push_back("#" + DispatchSystem::num(c.ambulanceId));
        row.push_back(DispatchSystem::num(c.distance) + " km");
    } else {
        row.push_back(DispatchSystem::statusName(c.status));
    }
    lvAddRow(lv, row);
}

void refreshAll() {
    // pending cases: prioritized (heap order) first, then the incoming queue
    clearList(hPending);
    std::vector<int> ids = sys.pending.sortedIds();
    for (size_t i = 0; i < ids.size(); i++) caseRow(hPending, sys.cases[ids[i] - 1], false);
    std::vector<int> q = sys.incoming.toVector();
    for (size_t i = 0; i < q.size(); i++) caseRow(hPending, sys.cases[q[i] - 1], false);

    // ambulances
    clearList(hAmbList);
    for (size_t i = 0; i < sys.ambulances.size(); i++) {
        const Ambulance& a = sys.ambulances[i];
        std::vector<std::string> row;
        row.push_back(DispatchSystem::num(a.id));
        row.push_back(sys.city.name(a.location));
        row.push_back(a.available ? "Available" : "Busy");
        row.push_back(a.caseId ? "#" + DispatchSystem::num(a.caseId) : "-");
        lvAddRow(hAmbList, row);
    }

    // active cases
    clearList(hActive);
    for (size_t i = 0; i < sys.cases.size(); i++)
        if (sys.cases[i].status == DISPATCHED) caseRow(hActive, sys.cases[i], true);

    // history (linked list)
    clearList(hHistory);
    std::vector<HistoryRecord> h = sys.history.toVector();
    for (size_t i = 0; i < h.size(); i++) {
        std::vector<std::string> row;
        row.push_back(DispatchSystem::num(h[i].no));
        row.push_back(h[i].event);
        row.push_back("#" + DispatchSystem::num(h[i].caseId));
        row.push_back("#" + DispatchSystem::num(h[i].ambulanceId));
        row.push_back(h[i].event == "Dispatched" ? DispatchSystem::num(h[i].distance) + " km" : "-");
        lvAddRow(hHistory, row);
    }

    InvalidateRect(hMap, 0, FALSE);
}

// ---------------- button actions ----------------
void onAddCase() {
    int t = (int)SendMessageA(hType, CB_GETCURSEL, 0, 0);
    int l = (int)SendMessageA(hLocation, CB_GETCURSEL, 0, 0);
    int s = (int)SendMessageA(hSeverity, CB_GETCURSEL, 0, 0);
    if (t < 0 || l < 0 || s < 0) {
        MessageBoxA(0, "Please choose the emergency type, location and severity.", "Add Case", MB_OK | MB_ICONWARNING);
        return;
    }
    char type[64];
    SendMessageA(hType, CB_GETLBTEXT, t, (LPARAM)type);
    int id = sys.addCase(type, l, s + 1);
    showText("Case #" + DispatchSystem::num(id) + " added to the incoming queue:\n" + type + " at " +
             sys.city.name(l) + " (" + DispatchSystem::severityName(s + 1) + ").");
    refreshAll();
}

void onAddAmbulance() {
    int l = (int)SendMessageA(hAmbLocation, CB_GETCURSEL, 0, 0);
    if (l < 0) {
        MessageBoxA(0, "Please choose a location for the ambulance.", "Add Ambulance", MB_OK | MB_ICONWARNING);
        return;
    }
    int id = sys.addAmbulance(l);
    showText("Ambulance #" + DispatchSystem::num(id) + " added at " + sys.city.name(l) + ".");
    refreshAll();
}

void onDispatchNext() {
    std::string msg;
    bool ok = sys.dispatchNext(msg);
    showText("LATEST ROUTE\n" + msg);
    if (ok) { mapRoute = sys.lastRoute; selectedNode = -1; }
    refreshAll();
}

void onDispatchAll() {
    std::string msg;
    int count = sys.dispatchAll(msg);
    showText("LATEST ROUTE\n" + msg);
    if (count > 0) { mapRoute = sys.lastRoute; selectedNode = -1; }
    refreshAll();
}

void onComplete() {
    int sel = (int)SendMessageA(hActive, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED);
    if (sel < 0) {
        MessageBoxA(0, "Select a case in the Active Cases table first.", "Complete Case", MB_OK | MB_ICONINFORMATION);
        return;
    }
    char buf[32];
    LVITEMA it;
    ZeroMemory(&it, sizeof(it));
    it.iSubItem = 0;
    it.pszText = buf;
    it.cchTextMax = sizeof(buf);
    SendMessageA(hActive, LVM_GETITEMTEXTA, sel, (LPARAM)&it);
    std::string msg;
    sys.completeCase(atoi(buf), msg);
    showText(msg);
    mapRoute.clear();
    refreshAll();
}

// ---------------- build the window ----------------
void createControls(HWND win) {
    const char* types[]    = { "Road Accident", "Heart Attack", "Fire Injury",
                               "Breathing Problem", "Fall / Fracture", "Other" };
    const char* severity[] = { "Critical", "High", "Medium", "Low" };

    // ===== left column =====
    addGroup("New Emergency Case", 10, 8, 400, 122);
    makeLabel(win, "Emergency type", 20, 30, 120, 20);
    makeLabel(win, "Location",       150, 30, 120, 20);
    makeLabel(win, "Severity",       290, 30, 100, 20);
    hType     = makeCombo(win, ID_TYPE,     20,  54, 120);
    hLocation = makeCombo(win, ID_LOCATION, 150, 54, 130);
    hSeverity = makeCombo(win, ID_SEVERITY, 290, 54, 110);
    makeButton(win, "Add Case", ID_BTN_ADDCASE, 20, 90, 130, 28);

    addGroup("Add Ambulance", 420, 8, 220, 122);
    makeLabel(win, "Starting location", 430, 30, 190, 20);
    hAmbLocation = makeCombo(win, ID_AMB_LOCATION, 430, 54, 200);
    makeButton(win, "Add Ambulance", ID_BTN_ADDAMB, 430, 90, 130, 28);

    addGroup("Dispatch Operations", 10, 138, 630, 60);
    makeButton(win, "Dispatch Next Case",     ID_BTN_NEXT,     20,  162, 170, 28);
    makeButton(win, "Dispatch All",           ID_BTN_ALL,      200, 162, 150, 28);
    makeButton(win, "Complete Selected Case", ID_BTN_COMPLETE, 360, 162, 270, 28);

    for (int i = 0; i < 6; i++) comboAdd(hType, types[i]);
    for (int i = 0; i < 4; i++) comboAdd(hSeverity, severity[i]);
    for (int i = 0; i < sys.city.size(); i++) {
        comboAdd(hLocation, sys.city.name(i));
        comboAdd(hAmbLocation, sys.city.name(i));
    }
    SendMessageA(hType, CB_SETCURSEL, 0, 0);
    SendMessageA(hLocation, CB_SETCURSEL, 0, 0);
    SendMessageA(hSeverity, CB_SETCURSEL, 2, 0);
    SendMessageA(hAmbLocation, CB_SETCURSEL, 0, 0);

    makeHeading(win, "Pending Emergency Cases (most urgent first)", 10, 202, 500, 20);
    const char* c1[] = { "Case", "Type", "Location", "Severity", "Status" };
    const int   w1[] = { 50, 150, 160, 100, 150 };
    hPending = makeListView(win, ID_LV_PENDING, 10, 226, 630, 140, c1, w1, 5);

    makeHeading(win, "Dispatch & Completion History", 10, 374, 500, 20);
    const char* c4[] = { "No.", "Event", "Case", "Ambulance", "Distance" };
    const int   w4[] = { 50, 140, 120, 150, 150 };
    hHistory = makeListView(win, ID_LV_HISTORY, 10, 398, 630, 100, c4, w4, 5);

    makeHeading(win, "Active Cases (select one, then click Complete)", 10, 506, 500, 20);
    const char* c3[] = { "Case", "Type", "Location", "Severity", "Ambulance", "Distance" };
    const int   w3[] = { 50, 140, 150, 90, 90, 90 };
    hActive = makeListView(win, ID_LV_ACTIVE, 10, 530, 630, 140, c3, w3, 6);

    // ===== right column =====
    makeHeading(win, "Visual Road Network   |   Click a location to preview a route", 650, 8, 600, 20);
    hMap = CreateWindowA("RoadMapView", "", WS_CHILD | WS_VISIBLE,
                         650, 32, 600, 340, win, (HMENU)ID_MAP, GetModuleHandleA(0), 0);

    makeHeading(win, "Latest Route / Map Selection", 650, 380, 500, 20);
    hLog = CreateWindowA("EDIT", "",
                         WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                         650, 404, 600, 96, win, (HMENU)ID_LOG, 0, 0);

    makeHeading(win, "Ambulances", 650, 508, 500, 20);
    const char* c2[] = { "ID", "Current Location", "Status", "Case" };
    const int   w2[] = { 60, 220, 150, 140 };
    hAmbList = makeListView(win, ID_LV_AMB, 650, 532, 600, 138, c2, w2, 4);

    // fonts
    hFontUI      = CreateFontA(-15, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, "Segoe UI");
    hFontBold    = CreateFontA(-15, 0, 0, 0, FW_BOLD,   0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, "Segoe UI");
    hFontMono    = CreateFontA(-15, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, FIXED_PITCH, "Consolas");
    hFontMap     = CreateFontA(-12, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, "Segoe UI");
    hFontMapBold = CreateFontA(-12, 0, 0, 0, FW_BOLD,   0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, "Segoe UI");
    EnumChildWindows(win, setFontProc, (LPARAM)hFontUI);
    for (size_t i = 0; i < headings.size(); i++)
        SendMessageA(headings[i], WM_SETFONT, (WPARAM)hFontBold, TRUE);
    SendMessageA(hLog, WM_SETFONT, (WPARAM)hFontMono, TRUE);

    showText("Welcome! 3 ambulances are ready.\nAdd emergency cases, then click Dispatch.\nYou can also click a location on the map to preview a route.");
    refreshAll();
}

// The panel holds every control. It has a fixed size and is centred in the main window.
const int PANEL_W = 1260;
const int PANEL_H = 680;
HWND hPanel;
int  minWinW, minWinH;

LRESULT CALLBACK PanelProc(HWND win, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        createControls(win);
        return 0;

    case WM_COMMAND:
        if (HIWORD(wp) == BN_CLICKED) {
            switch (LOWORD(wp)) {
                case ID_BTN_ADDCASE:  onAddCase();      break;
                case ID_BTN_ADDAMB:   onAddAmbulance(); break;
                case ID_BTN_NEXT:     onDispatchNext(); break;
                case ID_BTN_ALL:      onDispatchAll();  break;
                case ID_BTN_COMPLETE: onComplete();     break;
            }
        }
        return 0;

    case WM_PAINT: {                          // sky-blue boxes with titles
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(win, &ps);
        HBRUSH fill = CreateSolidBrush(C_SKY);
        HPEN edge = CreatePen(PS_SOLID, 1, C_SKY_EDGE);
        HGDIOBJ oldBrush = SelectObject(dc, fill);
        HGDIOBJ oldPen = SelectObject(dc, edge);
        HGDIOBJ oldFont = SelectObject(dc, hFontBold);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, C_HEADING);
        for (size_t i = 0; i < groups.size(); i++) {
            const RECT& g = groups[i].r;
            RoundRect(dc, g.left, g.top, g.right, g.bottom, 12, 12);
            TextOutA(dc, g.left + 12, g.top + 2, groups[i].title, (int)strlen(groups[i].title));
        }
        SelectObject(dc, oldFont);
        SelectObject(dc, oldBrush);
        SelectObject(dc, oldPen);
        DeleteObject(fill);
        DeleteObject(edge);
        EndPaint(win, &ps);
        return 0;
    }

    case WM_DRAWITEM:                         // coloured buttons
        drawButton((const DRAWITEMSTRUCT*)lp);
        return TRUE;

    case WM_CTLCOLORSTATIC: {                 // coloured labels and result box
        HDC dc = (HDC)wp;
        HWND c = (HWND)lp;
        if (c == hLog) {
            SetBkColor(dc, RGB(255, 255, 250));
            SetTextColor(dc, RGB(20, 30, 70));
            return (LRESULT)hLogBrush;
        }
        RECT cr;
        GetWindowRect(c, &cr);
        MapWindowPoints(0, win, (POINT*)&cr, 2);
        POINT mid = { (cr.left + cr.right) / 2, (cr.top + cr.bottom) / 2 };
        bool inGroup = false;
        for (size_t i = 0; i < groups.size(); i++)
            if (PtInRect(&groups[i].r, mid)) inGroup = true;
        bool isHeading = false;
        for (size_t i = 0; i < headings.size(); i++)
            if (headings[i] == c) isHeading = true;
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, isHeading ? C_HEADING : RGB(20, 40, 80));
        return (LRESULT)(inGroup ? hSkyBrush : hBgBrush);
    }

    case WM_NOTIFY: {                         // coloured table rows
        NMHDR* h = (NMHDR*)lp;
        if (h->code == NM_CUSTOMDRAW &&
            (h->idFrom == ID_LV_PENDING || h->idFrom == ID_LV_AMB ||
             h->idFrom == ID_LV_ACTIVE  || h->idFrom == ID_LV_HISTORY)) {
            NMLVCUSTOMDRAW* cd = (NMLVCUSTOMDRAW*)lp;
            switch (cd->nmcd.dwDrawStage) {
                case CDDS_PREPAINT:
                    return CDRF_NOTIFYITEMDRAW;
                case CDDS_ITEMPREPAINT:
                    return CDRF_NOTIFYSUBITEMDRAW;
                case (CDDS_ITEMPREPAINT | CDDS_SUBITEM): {
                    if (cd->nmcd.uItemState & CDIS_SELECTED) return CDRF_DODEFAULT;
                    COLORREF bk, fg;
                    if (rowColors((int)h->idFrom, h->hwndFrom, (int)cd->nmcd.dwItemSpec, bk, fg)) {
                        cd->clrTextBk = bk;
                        cd->clrText = fg;
                        return CDRF_NEWFONT;
                    }
                    return CDRF_DODEFAULT;
                }
            }
            return CDRF_DODEFAULT;
        }
        break;
    }

    }
    return DefWindowProcA(win, msg, wp, lp);
}

// ---------------- main window: keeps the panel centred ----------------
void centerPanel(HWND win) {
    RECT rc;
    GetClientRect(win, &rc);
    int x = (rc.right - PANEL_W) / 2;
    int y = (rc.bottom - PANEL_H) / 2;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    MoveWindow(hPanel, x, y, PANEL_W, PANEL_H, TRUE);
}

LRESULT CALLBACK WndProc(HWND win, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        hPanel = CreateWindowExA(WS_EX_CONTROLPARENT, "DispatchPanel", "",
                                 WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
                                 0, 0, PANEL_W, PANEL_H, win, 0, GetModuleHandleA(0), 0);
        return 0;
    case WM_SIZE:
        if (hPanel) centerPanel(win);
        return 0;
    case WM_GETMINMAXINFO: {                  // do not allow a window smaller than the panel
        MINMAXINFO* mm = (MINMAXINFO*)lp;
        mm->ptMinTrackSize.x = minWinW;
        mm->ptMinTrackSize.y = minWinH;
        return 0;
    }
    case WM_DESTROY:
        DeleteObject(hFontUI);
        DeleteObject(hFontBold);
        DeleteObject(hFontMono);
        DeleteObject(hFontMap);
        DeleteObject(hFontMapBold);
        DeleteObject(hBgBrush);
        DeleteObject(hLogBrush);
        DeleteObject(hSkyBrush);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(win, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE, LPSTR, int show) {
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icc);

    hBgBrush  = CreateSolidBrush(C_BG);
    hLogBrush = CreateSolidBrush(RGB(255, 255, 250));
    hSkyBrush = CreateSolidBrush(C_SKY);

    // map window class
    WNDCLASSA mc;
    ZeroMemory(&mc, sizeof(mc));
    mc.lpfnWndProc = MapProc;
    mc.hInstance = inst;
    mc.hCursor = LoadCursor(0, IDC_HAND);
    mc.lpszClassName = "RoadMapView";
    RegisterClassA(&mc);

    // panel class (holds all controls)
    WNDCLASSA pc;
    ZeroMemory(&pc, sizeof(pc));
    pc.lpfnWndProc = PanelProc;
    pc.hInstance = inst;
    pc.hCursor = LoadCursor(0, IDC_ARROW);
    pc.hbrBackground = hBgBrush;
    pc.lpszClassName = "DispatchPanel";
    RegisterClassA(&pc);

    // main window class
    WNDCLASSA wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(0, IDC_ARROW);
    wc.hbrBackground = hBgBrush;
    wc.lpszClassName = "DispatchWindow";
    RegisterClassA(&wc);

    DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;   // can be resized and maximized
    RECT r = { 0, 0, PANEL_W, PANEL_H };                   // client area = panel size
    AdjustWindowRect(&r, style, FALSE);
    minWinW = r.right - r.left;
    minWinH = r.bottom - r.top;

    HWND win = CreateWindowA("DispatchWindow", "Emergency Response & Ambulance Dispatch Simulator",
                             style | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
                             r.right - r.left, r.bottom - r.top, 0, 0, inst, 0);
    if (!win) return 0;
    ShowWindow(win, show);

    MSG m;
    while (GetMessageA(&m, 0, 0, 0) > 0) {
        if (!IsDialogMessageA(win, &m)) {
            TranslateMessage(&m);
            DispatchMessageA(&m);
        }
    }
    return (int)m.wParam;
}
