#include "theme.h"

#include <commctrl.h>

#include <algorithm>

namespace wlqg {
namespace {

constexpr wchar_t kButtonKindProp[] = L"WLQuickGenButtonKind";
constexpr wchar_t kButtonHoverProp[] = L"WLQuickGenButtonHover";
constexpr UINT_PTR kButtonSubclassId = 1;

LRESULT CALLBACK ButtonSubclassProc(HWND button, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR, DWORD_PTR) {
    switch (message) {
    case WM_MOUSEMOVE:
        if (!GetPropW(button, kButtonHoverProp)) {
            SetPropW(button, kButtonHoverProp, reinterpret_cast<HANDLE>(1));
            TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, button, 0};
            TrackMouseEvent(&track);
            InvalidateRect(button, nullptr, FALSE);
        }
        break;
    case WM_MOUSELEAVE:
        RemovePropW(button, kButtonHoverProp);
        InvalidateRect(button, nullptr, FALSE);
        break;
    case WM_LBUTTONDBLCLK:
        // Owner-drawn buttons swallow fast second clicks; treat them as clicks.
        message = WM_LBUTTONDOWN;
        break;
    case WM_SETCURSOR:
        if (IsWindowEnabled(button)) {
            SetCursor(LoadCursorW(nullptr, IDC_HAND));
            return TRUE;
        }
        break;
    case WM_NCDESTROY:
        RemovePropW(button, kButtonHoverProp);
        RemovePropW(button, kButtonKindProp);
        RemoveWindowSubclass(button, ButtonSubclassProc, kButtonSubclassId);
        break;
    default: break;
    }
    return DefSubclassProc(button, message, wparam, lparam);
}

} // namespace

COLORREF BackgroundColor() { return RGB(13, 17, 23); }
COLORREF SurfaceColor() { return RGB(22, 27, 34); }
COLORREF BorderColor() { return RGB(48, 54, 61); }
COLORREF TextColor() { return RGB(230, 237, 243); }
COLORREF MutedTextColor() { return RGB(139, 148, 158); }
COLORREF AccentGreen() { return RGB(63, 185, 80); }
COLORREF AccentBlue() { return RGB(56, 139, 253); }
COLORREF AccentAmber() { return RGB(210, 153, 34); }
COLORREF AccentRed() { return RGB(248, 81, 73); }

COLORREF Blend(COLORREF foreground, COLORREF background, int foreground_percent) {
    const auto mix = [foreground_percent](int a, int b) { return (a * foreground_percent + b * (100 - foreground_percent)) / 100; };
    return RGB(mix(GetRValue(foreground), GetRValue(background)),
        mix(GetGValue(foreground), GetGValue(background)),
        mix(GetBValue(foreground), GetBValue(background)));
}

HFONT CreateUiFont(int height, bool bold) {
    return CreateFontW(height, 0, 0, 0, bold ? FW_SEMIBOLD : FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

HFONT CreateMonoFont(int height) {
    return CreateFontW(height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FIXED_PITCH | FF_MODERN, L"Consolas");
}

void ApplyUiFont(HWND window, HFONT font) {
    SendMessageW(window, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
}

HBRUSH BackgroundBrush() {
    static HBRUSH brush = CreateSolidBrush(BackgroundColor());
    return brush;
}

HBRUSH FieldBrush() {
    // Fields are sunk into the cards, so they use the page background.
    return BackgroundBrush();
}

void ApplyDarkTitleBar(HWND window) {
    HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
    if (!dwm) return;
    using SetAttribute = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
    const auto set_attribute = reinterpret_cast<SetAttribute>(
        reinterpret_cast<void*>(GetProcAddress(dwm, "DwmSetWindowAttribute")));
    if (set_attribute) {
        const BOOL enabled = TRUE;
        // DWMWA_USE_IMMERSIVE_DARK_MODE: 20 on current builds, 19 on early Windows 10 builds.
        if (FAILED(set_attribute(window, 20, &enabled, sizeof(enabled)))) {
            set_attribute(window, 19, &enabled, sizeof(enabled));
        }
    }
    FreeLibrary(dwm);
}

HWND CreateThemedButton(HWND parent, int id, const std::wstring& text, ButtonKind kind,
    int x, int y, int width, int height, HFONT font) {
    HWND button = CreateWindowExW(0, L"BUTTON", text.c_str(), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
        x, y, width, height, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
    SetPropW(button, kButtonKindProp, reinterpret_cast<HANDLE>(static_cast<INT_PTR>(kind == ButtonKind::Primary ? 1 : 2)));
    SetWindowSubclass(button, ButtonSubclassProc, kButtonSubclassId, 0);
    ApplyUiFont(button, font);
    return button;
}

void DrawThemedButton(const DRAWITEMSTRUCT& item) {
    HDC dc = item.hDC;
    const RECT rect = item.rcItem;
    const bool primary = reinterpret_cast<INT_PTR>(GetPropW(item.hwndItem, kButtonKindProp)) == 1;
    const bool disabled = (item.itemState & ODS_DISABLED) != 0;
    const bool pressed = (item.itemState & ODS_SELECTED) != 0;
    const bool hover = GetPropW(item.hwndItem, kButtonHoverProp) != nullptr;
    const bool focus = (item.itemState & ODS_FOCUS) != 0 && (item.itemState & ODS_NOFOCUSRECT) == 0;

    COLORREF fill{};
    COLORREF border{};
    COLORREF text{};
    if (primary) {
        fill = disabled ? Blend(AccentBlue(), SurfaceColor(), 25)
            : pressed ? Blend(AccentBlue(), BackgroundColor(), 75)
            : hover ? Blend(AccentBlue(), RGB(255, 255, 255), 85)
            : AccentBlue();
        border = fill;
        text = disabled ? MutedTextColor() : RGB(255, 255, 255);
    } else {
        fill = pressed ? BackgroundColor() : hover ? Blend(TextColor(), SurfaceColor(), 8) : SurfaceColor();
        border = hover && !disabled ? Blend(TextColor(), BorderColor(), 25) : BorderColor();
        text = disabled ? Blend(MutedTextColor(), SurfaceColor(), 60) : TextColor();
    }
    if (focus) border = Blend(AccentBlue(), RGB(255, 255, 255), 60);

    FillRect(dc, &rect, BackgroundBrush());
    FillRoundRect(dc, rect, 10, fill, border);

    wchar_t caption[128]{};
    GetWindowTextW(item.hwndItem, caption, static_cast<int>(sizeof(caption) / sizeof(caption[0])));
    RECT text_rect = rect;
    if (pressed) OffsetRect(&text_rect, 0, 1);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, text);
    HFONT font = reinterpret_cast<HFONT>(SendMessageW(item.hwndItem, WM_GETFONT, 0, 0));
    HGDIOBJ old = font ? SelectObject(dc, font) : nullptr;
    DrawTextW(dc, caption, -1, &text_rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    if (old) SelectObject(dc, old);
}

HWND CreateFieldEdit(HWND parent, int id, DWORD style, const RECT& field, HFONT font) {
    // Center one text line vertically inside the field.
    HDC dc = GetDC(parent);
    HGDIOBJ old = SelectObject(dc, font);
    TEXTMETRICW metrics{};
    GetTextMetricsW(dc, &metrics);
    SelectObject(dc, old);
    ReleaseDC(parent, dc);
    const int height = metrics.tmHeight + 2;
    const int top = (field.top + field.bottom - height) / 2;
    HWND edit = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | style,
        field.left + 10, top, field.right - field.left - 20, height, parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
    ApplyUiFont(edit, font);
    return edit;
}

LRESULT ColorFieldEdit(HDC dc, bool enabled) {
    SetBkMode(dc, OPAQUE);
    SetBkColor(dc, BackgroundColor());
    SetTextColor(dc, enabled ? TextColor() : Blend(MutedTextColor(), BackgroundColor(), 60));
    return reinterpret_cast<LRESULT>(FieldBrush());
}

void FillRoundRect(HDC dc, const RECT& rect, int radius, COLORREF fill, COLORREF border) {
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ old_brush = SelectObject(dc, brush);
    HGDIOBJ old_pen = SelectObject(dc, pen);
    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
    DeleteObject(pen);
    DeleteObject(brush);
}

void DrawCard(HDC dc, const RECT& rect) {
    FillRoundRect(dc, rect, 14, SurfaceColor(), BorderColor());
}

void DrawField(HDC dc, const RECT& rect, bool focused, bool enabled) {
    const COLORREF border = focused ? AccentBlue()
        : enabled ? BorderColor()
        : Blend(BorderColor(), SurfaceColor(), 50);
    FillRoundRect(dc, rect, 10, BackgroundColor(), border);
}

void DrawCheckbox(HDC dc, const RECT& rect, const std::wstring& text, bool checked, bool hot, bool focused,
    COLORREF background, HFONT font) {
    HBRUSH fill = CreateSolidBrush(background);
    FillRect(dc, &rect, fill);
    DeleteObject(fill);

    const int middle = (rect.top + rect.bottom) / 2;
    const RECT box{rect.left + 1, middle - 9, rect.left + 19, middle + 9};
    if (checked) {
        const COLORREF accent = hot ? Blend(AccentBlue(), RGB(255, 255, 255), 85) : AccentBlue();
        FillRoundRect(dc, box, 6, accent, focused ? Blend(AccentBlue(), RGB(255, 255, 255), 60) : accent);
        HPEN pen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
        HGDIOBJ old_pen = SelectObject(dc, pen);
        const POINT check[] = {{box.left + 4, middle}, {box.left + 7, middle + 3}, {box.left + 13, middle - 4}};
        Polyline(dc, check, 3);
        SelectObject(dc, old_pen);
        DeleteObject(pen);
    } else {
        const COLORREF border = focused ? AccentBlue() : hot ? Blend(TextColor(), BorderColor(), 35) : BorderColor();
        FillRoundRect(dc, box, 6, BackgroundColor(), border);
    }

    RECT text_rect = rect;
    text_rect.left += 28;
    HGDIOBJ old = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, TextColor());
    DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &text_rect,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    SelectObject(dc, old);
}

void DrawStatusLine(HDC dc, const RECT& rect, const std::wstring& text, COLORREF color, HFONT font) {
    if (text.empty()) return;
    HGDIOBJ old = SelectObject(dc, font);
    // Long messages wrap onto a second line; the block stays vertically centered.
    RECT text_rect = rect;
    text_rect.left += 18;
    RECT measure = text_rect;
    DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &measure, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX | DT_CALCRECT);
    const int text_height = (std::min)(static_cast<int>(measure.bottom - measure.top), static_cast<int>(rect.bottom - rect.top));
    text_rect.top = rect.top + (rect.bottom - rect.top - text_height) / 2;
    text_rect.bottom = text_rect.top + text_height;
    TEXTMETRICW metrics{};
    GetTextMetricsW(dc, &metrics);
    const int middle = text_rect.top + metrics.tmHeight / 2;
    const RECT dot{rect.left, middle - 4, rect.left + 9, middle + 5};
    FillRoundRect(dc, dot, 9, color, color);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &text_rect,
        DT_LEFT | DT_WORDBREAK | DT_END_ELLIPSIS | DT_NOPREFIX);
    SelectObject(dc, old);
}

void DrawBadge(HDC dc, int right, int top, const std::wstring& text, COLORREF color, HFONT font) {
    HGDIOBJ old = SelectObject(dc, font);
    SIZE size{};
    GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &size);
    const int height = size.cy + 10;
    const RECT pill{right - size.cx - 34, top, right, top + height};
    FillRoundRect(dc, pill, height, Blend(color, BackgroundColor(), 16), Blend(color, BackgroundColor(), 45));
    const int middle = top + height / 2;
    const RECT dot{pill.left + 12, middle - 3, pill.left + 19, middle + 4};
    FillRoundRect(dc, dot, 7, color, color);
    RECT text_rect{pill.left + 25, pill.top, pill.right - 9, pill.bottom};
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &text_rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(dc, old);
}

} // namespace wlqg
