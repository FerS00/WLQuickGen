#pragma once

#include <windows.h>

#include <string>

namespace wlqg {

// Keep in sync with project(VERSION) in CMakeLists.txt.
inline constexpr wchar_t kVersionLabel[] = L"v1.1.0";

// Dark palette shared with GET_HWID.
COLORREF BackgroundColor();
COLORREF SurfaceColor();
COLORREF BorderColor();
COLORREF TextColor();
COLORREF MutedTextColor();
COLORREF AccentGreen();
COLORREF AccentBlue();
COLORREF AccentAmber();
COLORREF AccentRed();
COLORREF Blend(COLORREF foreground, COLORREF background, int foreground_percent);

HFONT CreateUiFont(int height = -15, bool bold = false);
HFONT CreateMonoFont(int height = -15);
void ApplyUiFont(HWND window, HFONT font);
HBRUSH BackgroundBrush();
HBRUSH FieldBrush();

// Dark title bar on Windows 10 20H1+ / 11; no-op on older systems.
void ApplyDarkTitleBar(HWND window);

enum class ButtonKind { Primary, Secondary };
HWND CreateThemedButton(HWND parent, int id, const std::wstring& text, ButtonKind kind,
    int x, int y, int width, int height, HFONT font);
void DrawThemedButton(const DRAWITEMSTRUCT& item);

// Borderless single-line edit that sits inside a field drawn with DrawField.
HWND CreateFieldEdit(HWND parent, int id, DWORD style, const RECT& field, HFONT font);
// Colors field edits (enabled edits arrive via WM_CTLCOLOREDIT, disabled via WM_CTLCOLORSTATIC).
LRESULT ColorFieldEdit(HDC dc, bool enabled);

void FillRoundRect(HDC dc, const RECT& rect, int radius, COLORREF fill, COLORREF border);
void DrawCard(HDC dc, const RECT& rect);
void DrawField(HDC dc, const RECT& rect, bool focused, bool enabled);
void DrawCheckbox(HDC dc, const RECT& rect, const std::wstring& text, bool checked, bool hot, bool focused,
    COLORREF background, HFONT font);
void DrawStatusLine(HDC dc, const RECT& rect, const std::wstring& text, COLORREF color, HFONT font);
// Draws a pill badge whose right edge is at `right`.
void DrawBadge(HDC dc, int right, int top, const std::wstring& text, COLORREF color, HFONT font);

} // namespace wlqg
