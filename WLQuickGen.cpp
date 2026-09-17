// WLQuickGen - portable WinLicense license generator (FileKey)
// ------------------------------------------------------------
// Drop this executable inside the "Specific Generators\<Product>\" folder
// created by WinLicense. It locates the generator DLL that already lives
// there (CustomWinlicenseSDK.dll, or WinLicenseSDK.dll for the standard SDK)
// and produces a license file next to the executable.
//
// Minimal workflow: paste the HWID -> press GENERATE.
//
// Everything else (license file name, registration name, organization,
// custom data, license hash) is pre-configured once per folder in
// WLQuickGen.ini, so the window only exposes what changes per license.
//
// The protected product and the SDK DLL are never modified: this tool only
// calls the official WinLicense key generation exports.
//
// Automation notes (pywinauto):
//   window class "WLQuickGenWindow", title "WLQuickGen"
//   HWID edit        control_id 1002  (class Edit)
//   Day/Month/Year   control_id 1007 / 1008 / 1009
//   "No expiry"      control_id 1006  (class Button, checkbox)
//   GENERATE button  control_id 1011  (class Button)
//   Status text      control_id 1012  (class Static) -> starts with "OK:" or "ERROR:"
//   Signature        control_id 1014  (class Static)
// No modal dialogs are shown; results are reported in the status text only.

#include <windows.h>
#include <uxtheme.h>

#include "resource.h"

#pragma comment(lib, "uxtheme.lib")

#include <array>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace
{
// ---------------------------------------------------------------------------
// License features structure (identical to the WinLicense SDK header).
// ---------------------------------------------------------------------------
struct sLicenseFeatures
{
    unsigned   cb;                    // size of struct
    unsigned   NumDays;               // expiration days
    unsigned   NumExec;               // expiration executions
    SYSTEMTIME ExpDate;               // expiration date
    unsigned   CountryId;             // country ID
    unsigned   Runtime;               // expiration runtime
    unsigned   GlobalMinutes;         // global time expiration
    SYSTEMTIME InstallDate;           // date to install the license since created
    unsigned   NetInstances;          // network instances via shared file
    unsigned   EmbedLicenseInfoInKey; // embed Name+Company+Custom (Dynamic SmartKeys)
    unsigned   EmbedCreationDate;     // embed the date the key was created
};

// ANSI variants are used on purpose: they are the ones the official generator
// (WLGen_<Product>.exe) and the Oreans sample use. The Unicode variants
// (...ExW) embed the name as UTF-16 and the protected product rejects the
// resulting file as a corrupt license.
using CustomFileKeyEx = int(__stdcall*)(const char*, const char*, const char*,
                                        const char*, sLicenseFeatures*, char*);
using StandardFileKeyEx = int(__stdcall*)(const char*, const char*, const char*,
                                          const char*, const char*, sLicenseFeatures*, char*);

// ---------------------------------------------------------------------------
// Control identifiers (kept stable for UI automation).
// ---------------------------------------------------------------------------
constexpr int IDC_HWID     = 1002;
constexpr int IDC_NOEXPIRE = 1006;
constexpr int IDC_DAY      = 1007;
constexpr int IDC_MONTH    = 1008;
constexpr int IDC_YEAR     = 1009;
constexpr int IDC_GENERATE = 1011;
constexpr int IDC_STATUS   = 1012;
constexpr int IDC_SIGNATURE = 1014;

// ---------------------------------------------------------------------------
// Theme.
// ---------------------------------------------------------------------------
constexpr COLORREF kBackground = RGB(13, 13, 13);
constexpr COLORREF kField      = RGB(26, 26, 26);
constexpr COLORREF kText       = RGB(235, 235, 235);
constexpr COLORREF kAccent     = RGB(220, 30, 30);
constexpr COLORREF kAccentDark = RGB(150, 18, 18);
constexpr COLORREF kMuted      = RGB(150, 150, 150);
constexpr COLORREF kOk         = RGB(80, 220, 120);
constexpr COLORREF kSignature  = RGB(120, 120, 120);

HBRUSH g_backgroundBrush{};
HBRUSH g_fieldBrush{};
HFONT  g_font{};
HFONT  g_buttonFont{};
HFONT  g_smallFont{};

HWND g_hwid{};
HWND g_noExpire{};
HWND g_day{};
HWND g_month{};
HWND g_year{};
HWND g_status{};
HWND g_signature{};

std::wstring g_dllPath;       // detected generator DLL
std::wstring g_dataDirectory; // folder holding GeneratorSeed.gns / GeneratorDatabase.abs
bool         g_customMode = true;

// Pre-configured through WLQuickGen.ini.
std::wstring g_licenseFileName = L"License.dat";
std::wstring g_configuredName;
std::wstring g_organization;
std::wstring g_customData;
std::wstring g_licenseHash;
bool         g_nameIsHwid = true;

// ---------------------------------------------------------------------------
// Basic helpers.
// ---------------------------------------------------------------------------
std::wstring ModuleDirectory()
{
    std::vector<wchar_t> buffer(MAX_PATH, L'\0');
    for (;;)
    {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0)
            return {};
        if (length < buffer.size() - 1)
        {
            std::wstring path(buffer.data(), length);
            const auto slash = path.find_last_of(L"\\/");
            return slash == std::wstring::npos ? path : path.substr(0, slash);
        }
        buffer.resize(buffer.size() * 2);
    }
}

std::wstring IniPath()
{
    return ModuleDirectory() + L"\\WLQuickGen.ini";
}

bool FileExists(const std::wstring& path)
{
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

std::wstring GetText(HWND control)
{
    const int length = GetWindowTextLengthW(control);
    if (length <= 0)
        return {};
    std::wstring value(static_cast<std::size_t>(length) + 1, L'\0');
    const int copied = GetWindowTextW(control, value.data(), length + 1);
    value.resize(static_cast<std::size_t>(copied));
    return value;
}

std::wstring Trim(std::wstring value)
{
    while (!value.empty() && iswspace(value.front()))
        value.erase(value.begin());
    while (!value.empty() && iswspace(value.back()))
        value.pop_back();
    return value;
}

// The SDK expects ANSI strings (system code page).
std::string ToAnsi(const std::wstring& value)
{
    if (value.empty())
        return {};
    const int length = WideCharToMultiByte(CP_ACP, 0, value.data(), static_cast<int>(value.size()),
                                           nullptr, 0, nullptr, nullptr);
    if (length <= 0)
        return {};
    std::string result(static_cast<std::size_t>(length), '\0');
    WideCharToMultiByte(CP_ACP, 0, value.data(), static_cast<int>(value.size()),
                        result.data(), length, nullptr, nullptr);
    return result;
}

// Results are reported here only: no modal dialogs, so UI automation never blocks.
void SetStatus(const std::wstring& text)
{
    SetWindowTextW(g_status, text.c_str());
    InvalidateRect(g_status, nullptr, TRUE);
    UpdateWindow(g_status);
}

void Fail(const std::wstring& message)
{
    SetStatus(L"ERROR: " + message);
}

std::wstring ParentOf(const std::wstring& path)
{
    const auto slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? std::wstring{} : path.substr(0, slash);
}

// Immediate subdirectories of "base", so the product folder can sit one level
// below the executable (e.g. exe in "Specific Generators\", product in "...\DPP\").
std::vector<std::wstring> SubDirectories(const std::wstring& base)
{
    std::vector<std::wstring> result;
    WIN32_FIND_DATAW data{};
    HANDLE find = FindFirstFileW((base + L"\\*").c_str(), &data);
    if (find == INVALID_HANDLE_VALUE)
        return result;
    do
    {
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
            wcscmp(data.cFileName, L".") != 0 && wcscmp(data.cFileName, L"..") != 0)
            result.push_back(base + L"\\" + data.cFileName);
    } while (FindNextFileW(find, &data));
    FindClose(find);
    return result;
}

// SDK files always live in <Product>\DLL or <Product>\EXE, so probe the
// executable folder, those two subfolders, the parent, and one level of
// product subfolders.
std::wstring FindRelative(const std::wstring& base, std::initializer_list<const wchar_t*> names)
{
    std::vector<std::wstring> roots;
    roots.push_back(base);
    roots.push_back(base + L"\\DLL");
    roots.push_back(base + L"\\EXE");
    const std::wstring parent = ParentOf(base);
    if (!parent.empty())
    {
        roots.push_back(parent);
        roots.push_back(parent + L"\\DLL");
        roots.push_back(parent + L"\\EXE");
    }
    for (const auto& sub : SubDirectories(base))
    {
        roots.push_back(sub);
        roots.push_back(sub + L"\\DLL");
        roots.push_back(sub + L"\\EXE");
    }
    for (const auto& root : roots)
        for (const wchar_t* name : names)
        {
            const std::wstring full = root + L"\\" + name;
            if (FileExists(full))
                return full;
        }
    return {};
}

std::uint16_t ReadPeMachine(const std::wstring& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
        return 0;
    std::array<std::uint8_t, 4096> header{};
    input.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size()));
    const std::streamsize count = input.gcount();
    if (count < 0x40 || header[0] != 'M' || header[1] != 'Z')
        return 0;
    const std::uint32_t peOffset = *reinterpret_cast<const std::uint32_t*>(&header[0x3c]);
    if (peOffset > static_cast<std::uint32_t>(count) - 6u ||
        *reinterpret_cast<const std::uint32_t*>(&header[peOffset]) != 0x00004550u)
        return 0;
    return *reinterpret_cast<const std::uint16_t*>(&header[peOffset + 4]);
}

std::uint16_t OwnMachine()
{
#if defined(_M_X64)
    return 0x8664;
#else
    return 0x014c;
#endif
}

// ---------------------------------------------------------------------------
// SDK detection.
// ---------------------------------------------------------------------------
void DetectSdk()
{
    const std::wstring base = ModuleDirectory();
    g_dllPath = FindRelative(base, {L"CustomWinlicenseSDK.dll"});
    if (!g_dllPath.empty())
    {
        g_customMode = true;
    }
    else
    {
        g_dllPath = FindRelative(base, {L"WinLicenseSDK.dll"});
        g_customMode = false;
    }
    // The custom DLL is self-contained; the standard SDK needs these data files.
    const std::wstring seed = FindRelative(base, {L"GeneratorSeed.gns", L"GeneratorDatabase.abs"});
    if (!seed.empty())
        g_dataDirectory = ParentOf(seed);
    else if (!g_dllPath.empty())
        g_dataDirectory = ParentOf(g_dllPath);
    else
        g_dataDirectory = base;
}

// ---------------------------------------------------------------------------
// Per-folder configuration (WLQuickGen.ini).
// ---------------------------------------------------------------------------
std::wstring IniGet(const wchar_t* key, const wchar_t* fallback)
{
    std::array<wchar_t, 1024> buffer{};
    GetPrivateProfileStringW(L"WLQuickGen", key, fallback,
                             buffer.data(), static_cast<DWORD>(buffer.size()), IniPath().c_str());
    return std::wstring(buffer.data());
}

void LoadSettings()
{
    const std::wstring path = IniPath();
    g_licenseFileName = Trim(IniGet(L"LicenseFileName", L"License.dat"));
    if (g_licenseFileName.empty())
        g_licenseFileName = L"License.dat";
    g_configuredName = IniGet(L"Name", L"");
    g_organization = IniGet(L"Organization", L"");
    g_customData = IniGet(L"CustomData", L"");
    g_licenseHash = Trim(IniGet(L"LicenseHash", L""));
    g_nameIsHwid = GetPrivateProfileIntW(L"WLQuickGen", L"NameIsHwid", 1, path.c_str()) != 0;

    // Write a template on first run so the settings are discoverable.
    if (!FileExists(path))
    {
        WritePrivateProfileStringW(L"WLQuickGen", L"LicenseFileName", g_licenseFileName.c_str(), path.c_str());
        WritePrivateProfileStringW(L"WLQuickGen", L"NameIsHwid", L"1", path.c_str());
        WritePrivateProfileStringW(L"WLQuickGen", L"Name", L"", path.c_str());
        WritePrivateProfileStringW(L"WLQuickGen", L"Organization", L"", path.c_str());
        WritePrivateProfileStringW(L"WLQuickGen", L"CustomData", L"", path.c_str());
        WritePrivateProfileStringW(L"WLQuickGen", L"LicenseHash", L"", path.c_str());
    }
}

// ---------------------------------------------------------------------------
// Expiration date.
// ---------------------------------------------------------------------------
bool BuildExpiration(SYSTEMTIME& expiration, std::wstring& error)
{
    const std::wstring dayText = Trim(GetText(g_day));
    const std::wstring monthText = Trim(GetText(g_month));
    const std::wstring yearText = Trim(GetText(g_year));
    if (dayText.empty() || monthText.empty() || yearText.empty())
    {
        error = L"enter day, month and year, or tick \"No expiry\".";
        return false;
    }
    const int day = _wtoi(dayText.c_str());
    const int month = _wtoi(monthText.c_str());
    const int year = _wtoi(yearText.c_str());
    if (year < 1601 || year > 9999 || month < 1 || month > 12 || day < 1 || day > 31)
    {
        error = L"invalid date (day 1-31, month 1-12, year 1601-9999).";
        return false;
    }
    expiration = {};
    expiration.wYear = static_cast<WORD>(year);
    expiration.wMonth = static_cast<WORD>(month);
    expiration.wDay = static_cast<WORD>(day);
    expiration.wHour = 23;
    expiration.wMinute = 59;
    expiration.wSecond = 59;
    FILETIME check{};
    if (!SystemTimeToFileTime(&expiration, &check))
    {
        error = L"that date does not exist in the calendar.";
        return false;
    }
    return true;
}

// The SDK distinguishes "absent field" (NULL) from "empty string", exactly as
// the official sample does: Name[0] == 0 ? NULL : Name.
const char* Ptr(const std::string& value)
{
    return value.empty() ? nullptr : value.c_str();
}

// ---------------------------------------------------------------------------
// Generation.
// ---------------------------------------------------------------------------
void Generate()
{
    SetStatus(L"Working...");

    if (g_dllPath.empty())
    {
        Fail(L"generator DLL not found. Place this executable inside the "
             L"Specific Generators folder of the product.");
        return;
    }
    if (g_licenseFileName.find_first_of(L"\\/:*?\"<>|") != std::wstring::npos ||
        g_licenseFileName == L"." || g_licenseFileName == L".." ||
        g_licenseFileName.find(L"..") != std::wstring::npos)
    {
        Fail(L"LicenseFileName in WLQuickGen.ini must be a plain file name.");
        return;
    }

    const std::wstring hwidWide = Trim(GetText(g_hwid));
    if (hwidWide.empty())
    {
        Fail(L"HWID is empty.");
        return;
    }

    const std::string hwid = ToAnsi(hwidWide);
    const std::string name = ToAnsi(g_nameIsHwid ? hwidWide : g_configuredName);
    const std::string org = ToAnsi(g_organization);
    const std::string custom = ToAnsi(g_customData);
    const std::string hash = ToAnsi(g_licenseHash);

    if (!g_customMode && hash.empty())
    {
        Fail(L"the standard SDK (WinLicenseSDK.dll) needs LicenseHash in WLQuickGen.ini.");
        return;
    }

    sLicenseFeatures features{};
    features.cb = sizeof(features);
    if (SendMessageW(g_noExpire, BM_GETCHECK, 0, 0) != BST_CHECKED)
    {
        std::wstring error;
        if (!BuildExpiration(features.ExpDate, error))
        {
            Fail(error);
            return;
        }
    }

    // The generator DLL reads its data while the call runs, so the working
    // directory must stay set until generation finishes.
    std::vector<wchar_t> previousDir(32768, L'\0');
    const DWORD previousLen = GetCurrentDirectoryW(static_cast<DWORD>(previousDir.size()), previousDir.data());
    if (!g_dataDirectory.empty())
    {
        SetCurrentDirectoryW(g_dataDirectory.c_str());
        SetDllDirectoryW(g_dataDirectory.c_str());
    }
    auto restoreDirectories = [&]() {
        if (previousLen > 0 && previousLen < previousDir.size())
            SetCurrentDirectoryW(previousDir.data());
        SetDllDirectoryW(nullptr);
    };

    HMODULE library = LoadLibraryW(g_dllPath.c_str());
    if (!library)
    {
        const DWORD loadError = GetLastError();
        restoreDirectories();
        Fail(loadError == ERROR_BAD_EXE_FORMAT
                 ? L"the generator DLL has a different architecture. Use the x86 or x64 build to match."
                 : L"could not load the generator DLL.");
        return;
    }

    std::vector<char> buffer(64 * 1024, 0);
    int size = 0;
    if (g_customMode)
    {
        auto function = reinterpret_cast<CustomFileKeyEx>(
            GetProcAddress(library, "WLCustomGenLicenseFileKeyEx"));
        if (!function)
        {
            FreeLibrary(library);
            restoreDirectories();
            Fail(L"the DLL does not export WLCustomGenLicenseFileKeyEx.");
            return;
        }
        size = function(Ptr(name), Ptr(org), Ptr(custom), Ptr(hwid), &features, buffer.data());
    }
    else
    {
        auto function = reinterpret_cast<StandardFileKeyEx>(
            GetProcAddress(library, "WLGenLicenseFileKeyEx"));
        if (!function)
        {
            FreeLibrary(library);
            restoreDirectories();
            Fail(L"the DLL does not export WLGenLicenseFileKeyEx.");
            return;
        }
        size = function(Ptr(hash), Ptr(name), Ptr(org), Ptr(custom), Ptr(hwid), &features, buffer.data());
    }
    FreeLibrary(library);
    restoreDirectories();

    if (size <= 0 || size > static_cast<int>(buffer.size()))
    {
        Fail(L"the SDK did not generate the license (returned " + std::to_wstring(size) + L").");
        return;
    }

    const std::wstring outputPath = ModuleDirectory() + L"\\" + g_licenseFileName;
    // Overwrite without prompting (a .previous copy is kept) so automation never blocks.
    if (FileExists(outputPath))
        CopyFileW(outputPath.c_str(), (outputPath + L".previous").c_str(), FALSE);

    const std::wstring temporary = outputPath + L".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            Fail(L"could not write the license file.");
            return;
        }
        output.write(buffer.data(), size);
        if (!output)
        {
            output.close();
            DeleteFileW(temporary.c_str());
            Fail(L"could not complete the license file.");
            return;
        }
    }
    if (!MoveFileExW(temporary.c_str(), outputPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        DeleteFileW(temporary.c_str());
        Fail(L"could not place the license file next to the executable.");
        return;
    }

    SetStatus(L"OK: " + g_licenseFileName + L" written (" + std::to_wstring(size) + L" bytes)");
}

// ---------------------------------------------------------------------------
// User interface.
// ---------------------------------------------------------------------------
HWND AddEdit(HWND parent, DWORD style, int id, int x, int y, int width, int height)
{
    HWND control = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | style,
                                   x, y, width, height, parent,
                                   reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                   GetModuleHandleW(nullptr), nullptr);
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
    return control;
}

HWND AddLabel(HWND parent, const wchar_t* text, int x, int y, int width)
{
    HWND label = CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE, x, y, width, 18,
                                 parent, nullptr, GetModuleHandleW(nullptr), nullptr);
    SendMessageW(label, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
    return label;
}

void UpdateDateFields()
{
    const bool noExpire = SendMessageW(g_noExpire, BM_GETCHECK, 0, 0) == BST_CHECKED;
    EnableWindow(g_day, !noExpire);
    EnableWindow(g_month, !noExpire);
    EnableWindow(g_year, !noExpire);
}

void DrawGenerateButton(LPDRAWITEMSTRUCT item)
{
    const bool pressed = (item->itemState & ODS_SELECTED) != 0;
    HBRUSH face = CreateSolidBrush(pressed ? kAccentDark : kAccent);
    FillRect(item->hDC, &item->rcItem, face);
    DeleteObject(face);

    SetBkMode(item->hDC, TRANSPARENT);
    SetTextColor(item->hDC, RGB(255, 255, 255));
    HGDIOBJ previous = SelectObject(item->hDC, g_buttonFont);
    DrawTextW(item->hDC, L"GENERATE", -1, &item->rcItem,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(item->hDC, previous);

    if (item->itemState & ODS_FOCUS)
    {
        RECT focus = item->rcItem;
        InflateRect(&focus, -3, -3);
        SetTextColor(item->hDC, RGB(255, 255, 255));
        DrawFocusRect(item->hDC, &focus);
    }
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        AddLabel(window, L"HWID", 20, 18, 200);
        g_hwid = AddEdit(window, ES_AUTOHSCROLL, IDC_HWID, 20, 40, 500, 26);

        AddLabel(window, L"EXPIRATION", 20, 84, 200);
        g_noExpire = CreateWindowExW(0, L"BUTTON", L"No expiry",
                                     WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                     20, 108, 124, 24, window,
                                     reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_NOEXPIRE)),
                                     GetModuleHandleW(nullptr), nullptr);
        SendMessageW(g_noExpire, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
        SendMessageW(g_noExpire, BM_SETCHECK, BST_CHECKED, 0);
        // Disable visual styles on the checkbox so WM_CTLCOLORBTN colours apply
        // (the themed renderer would draw dark text on the dark background).
        SetWindowTheme(g_noExpire, L"", L"");

        AddLabel(window, L"DD", 152, 88, 40);
        g_day = AddEdit(window, ES_NUMBER | ES_CENTER, IDC_DAY, 152, 108, 52, 26);
        AddLabel(window, L"MM", 212, 88, 40);
        g_month = AddEdit(window, ES_NUMBER | ES_CENTER, IDC_MONTH, 212, 108, 52, 26);
        AddLabel(window, L"YYYY", 272, 88, 50);
        g_year = AddEdit(window, ES_NUMBER | ES_CENTER, IDC_YEAR, 272, 108, 72, 26);

        CreateWindowExW(0, L"BUTTON", L"GENERATE",
                        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | BS_DEFPUSHBUTTON,
                        368, 104, 152, 34, window,
                        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_GENERATE)),
                        GetModuleHandleW(nullptr), nullptr);

        g_status = CreateWindowExW(0, L"STATIC", L"Ready.", WS_CHILD | WS_VISIBLE,
                                   20, 156, 500, 36, window,
                                   reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_STATUS)),
                                   GetModuleHandleW(nullptr), nullptr);
        SendMessageW(g_status, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);

        g_signature = CreateWindowExW(0, L"STATIC", L"by FerS0",
                                      WS_CHILD | WS_VISIBLE | SS_RIGHT,
                                      340, 198, 180, 16, window,
                                      reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SIGNATURE)),
                                      GetModuleHandleW(nullptr), nullptr);
        SendMessageW(g_signature, WM_SETFONT, reinterpret_cast<WPARAM>(g_smallFont), TRUE);

        DetectSdk();
        LoadSettings();
        UpdateDateFields();
        SetFocus(g_hwid);
        return 0;
    }
    case WM_CTLCOLORSTATIC:
    {
        HDC dc = reinterpret_cast<HDC>(wParam);
        HWND control = reinterpret_cast<HWND>(lParam);
        SetBkColor(dc, kBackground);
        if (control == g_status)
        {
            const std::wstring text = GetText(g_status);
            if (text.rfind(L"OK:", 0) == 0)
                SetTextColor(dc, kOk);
            else if (text.rfind(L"ERROR:", 0) == 0)
                SetTextColor(dc, kAccent);
            else
                SetTextColor(dc, kMuted);
        }
        else if (control == g_signature)
            SetTextColor(dc, kSignature);
        else if (control == g_noExpire)
            SetTextColor(dc, kText);
        else
            SetTextColor(dc, kAccent);
        return reinterpret_cast<LRESULT>(g_backgroundBrush);
    }
    case WM_CTLCOLORBTN:
    {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetBkColor(dc, kBackground);
        SetTextColor(dc, kText);
        return reinterpret_cast<LRESULT>(g_backgroundBrush);
    }
    case WM_CTLCOLOREDIT:
    {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetBkColor(dc, kField);
        SetTextColor(dc, kText);
        return reinterpret_cast<LRESULT>(g_fieldBrush);
    }
    case WM_DRAWITEM:
        if (static_cast<int>(wParam) == IDC_GENERATE)
        {
            DrawGenerateButton(reinterpret_cast<LPDRAWITEMSTRUCT>(lParam));
            return TRUE;
        }
        break;
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_NOEXPIRE:
            UpdateDateFields();
            break;
        case IDC_GENERATE:
            Generate();
            break;
        default:
            break;
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}
}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show)
{
    g_backgroundBrush = CreateSolidBrush(kBackground);
    g_fieldBrush = CreateSolidBrush(kField);
    g_font = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                         OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                         DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    g_buttonFont = CreateFontW(-15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                               OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                               DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    g_smallFont = CreateFontW(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    HICON icon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON,
                                               0, 0, LR_DEFAULTSIZE | LR_SHARED));
    HICON iconSmall = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON,
                                                    GetSystemMetrics(SM_CXSMICON),
                                                    GetSystemMetrics(SM_CYSMICON), LR_SHARED));

    const wchar_t className[] = L"WLQuickGenWindow";
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.hInstance = instance;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.lpszClassName = className;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = g_backgroundBrush;
    windowClass.hIcon = icon;
    windowClass.hIconSm = iconSmall;
    RegisterClassExW(&windowClass);

    RECT desired{0, 0, 540, 220};
    AdjustWindowRect(&desired, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
    HWND window = CreateWindowExW(0, className, L"WLQuickGen",
                                  WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                  CW_USEDEFAULT, CW_USEDEFAULT,
                                  desired.right - desired.left, desired.bottom - desired.top,
                                  nullptr, nullptr, instance, nullptr);
    if (!window)
        return 1;
    ShowWindow(window, show);
    UpdateWindow(window);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        if (!IsDialogMessageW(window, &message))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }

    DeleteObject(g_backgroundBrush);
    DeleteObject(g_fieldBrush);
    DeleteObject(g_font);
    DeleteObject(g_buttonFont);
    DeleteObject(g_smallFont);
    return static_cast<int>(message.wParam);
}
