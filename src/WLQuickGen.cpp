// WLQuickGen - portable WinLicense license generator (FileKey)
// ------------------------------------------------------------
// Drop this executable inside the "Specific Generators\<Product>\" folder
// created by WinLicense. It locates the generator DLL that already lives
// there (CustomWinlicenseSDK.dll, or WinLicenseSDK.dll for the standard SDK)
// and produces a license file next to the executable.
//
// Minimal workflow: paste the HWID -> press Generate.
//
// Everything else (license file name, registration name, organization,
// custom data, license hash, language) is pre-configured once per folder in
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
//   Generate button  control_id 1011  (class Button)
//   Status text      control_id 1012  (class Static) -> starts with "OK:" or "ERROR:"
//   Show file        control_id 1013  (class Button)
//   Language         control_id 1015  (class Button)
// The "OK:" / "ERROR:" prefixes never change with the interface language.
// No modal dialogs are shown; results are reported in the status text only.

#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>

#include "../resource.h"
#include "i18n.h"
#include "theme.h"

#include <array>
#include <cstdint>
#include <cwchar>
#include <fstream>
#include <initializer_list>
#include <string>
#include <vector>

namespace
{
using wlqg::Localize;
using wlqg::Text;

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
constexpr int IDC_SHOWFILE = 1013;
constexpr int IDC_LANGUAGE = 1015;

// ---------------------------------------------------------------------------
// Layout (client area, pixels).
// ---------------------------------------------------------------------------
constexpr int  kClientWidth  = 640;
constexpr int  kClientHeight = 476;
constexpr int  kMargin       = 28;
constexpr int  kRight        = kClientWidth - kMargin;
constexpr int  kLanguageWidth = 56;
constexpr RECT kHwidCard{kMargin, 100, kRight, 190};
constexpr RECT kHwidField{kMargin + 20, 138, kRight - 20, 174};
constexpr RECT kExpiryCard{kMargin, 204, kRight, 294};
constexpr RECT kCheckRect{kMargin + 20, 242, kMargin + 220, 278};
constexpr RECT kDayField{368, 242, 424, 278};
constexpr RECT kMonthField{440, 242, 496, 278};
constexpr RECT kYearField{512, 242, kRight - 20, 278};
constexpr RECT kOutputInfo{kMargin + 2, 306, kRight, 324};
constexpr RECT kSdkInfo{kMargin + 2, 326, kRight, 344};
constexpr RECT kStatusRect{kMargin + 2, 348, kRight, 388};
constexpr int  kButtonTop    = 396;
constexpr int  kButtonHeight = 40;

enum class StatusKind { Info, Warning, Busy, Ok, Error };

// ---------------------------------------------------------------------------
// State.
// ---------------------------------------------------------------------------
HWND g_window{};
HWND g_hwid{};
HWND g_noExpire{};
HWND g_day{};
HWND g_month{};
HWND g_year{};
HWND g_status{};
HWND g_generate{};
HWND g_showFile{};
HWND g_language{};

HFONT g_font{};
HFONT g_titleFont{};
HFONT g_smallFont{};
HFONT g_smallBoldFont{};
HFONT g_monoFont{};

// Status is kept as a key so it can be re-rendered when the language changes.
StatusKind   g_statusKind = StatusKind::Info;
Text         g_statusText = Text::Ready;
std::wstring g_statusArg0;
std::wstring g_statusArg1;

std::wstring g_dllPath;       // detected generator DLL
std::wstring g_dataDirectory; // folder holding GeneratorSeed.gns / GeneratorDatabase.abs
bool         g_customMode = true;
std::uint16_t g_dllMachine = 0;
std::wstring g_lastOutput;    // last license written in this session

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
    std::ifstream input(path.c_str(), std::ios::binary);
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
#if defined(_M_X64) || defined(__x86_64__)
    return 0x8664;
#else
    return 0x014c;
#endif
}

bool ArchitectureMismatch()
{
    return g_dllMachine != 0 && g_dllMachine != OwnMachine();
}

// Build name that matches the detected DLL ("x86" or "x64").
const wchar_t* MatchingBuild()
{
    return g_dllMachine == 0x8664 ? L"x64" : L"x86";
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
    g_dllMachine = g_dllPath.empty() ? 0 : ReadPeMachine(g_dllPath);
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
    wlqg::SetCurrentLanguage(wlqg::ResolveLanguage(IniGet(L"Language", L"auto")));

    // Write a template on first run so the settings are discoverable.
    if (!FileExists(path))
    {
        WritePrivateProfileStringW(L"WLQuickGen", L"LicenseFileName", g_licenseFileName.c_str(), path.c_str());
        WritePrivateProfileStringW(L"WLQuickGen", L"NameIsHwid", L"1", path.c_str());
        WritePrivateProfileStringW(L"WLQuickGen", L"Name", L"", path.c_str());
        WritePrivateProfileStringW(L"WLQuickGen", L"Organization", L"", path.c_str());
        WritePrivateProfileStringW(L"WLQuickGen", L"CustomData", L"", path.c_str());
        WritePrivateProfileStringW(L"WLQuickGen", L"LicenseHash", L"", path.c_str());
        WritePrivateProfileStringW(L"WLQuickGen", L"Language", L"auto", path.c_str());
    }
}

// ---------------------------------------------------------------------------
// Status line. The window text carries a language-independent "OK:" /
// "ERROR:" prefix for automation; the painted line shows a colored dot instead.
// ---------------------------------------------------------------------------
std::wstring StatusMessage()
{
    return Localize(g_statusText, g_statusArg0, g_statusArg1);
}

void RefreshStatus()
{
    std::wstring text = StatusMessage();
    if (g_statusKind == StatusKind::Ok)
        text = L"OK: " + text;
    else if (g_statusKind == StatusKind::Error)
        text = L"ERROR: " + text;
    SetWindowTextW(g_status, text.c_str());
    InvalidateRect(g_status, nullptr, FALSE);
    UpdateWindow(g_status);
}

void SetStatus(StatusKind kind, Text text, const std::wstring& arg0 = {}, const std::wstring& arg1 = {})
{
    g_statusKind = kind;
    g_statusText = text;
    g_statusArg0 = arg0;
    g_statusArg1 = arg1;
    RefreshStatus();
}

void Fail(Text text, const std::wstring& arg0 = {})
{
    SetStatus(StatusKind::Error, text, arg0);
}

COLORREF StatusColor()
{
    switch (g_statusKind)
    {
    case StatusKind::Ok: return wlqg::AccentGreen();
    case StatusKind::Error: return wlqg::AccentRed();
    case StatusKind::Warning: return wlqg::AccentAmber();
    case StatusKind::Busy: return wlqg::AccentBlue();
    default: return wlqg::MutedTextColor();
    }
}

void SetReadyStatus()
{
    if (g_dllPath.empty())
        SetStatus(StatusKind::Warning, Text::ReadyNoSdk);
    else if (ArchitectureMismatch())
        SetStatus(StatusKind::Warning, Text::ErrWrongArch, MatchingBuild());
    else
        SetStatus(StatusKind::Info, Text::Ready);
}

// ---------------------------------------------------------------------------
// Expiration date.
// ---------------------------------------------------------------------------
bool NoExpiry()
{
    return SendMessageW(g_noExpire, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

bool BuildExpiration(SYSTEMTIME& expiration, Text& error)
{
    const std::wstring dayText = Trim(GetText(g_day));
    const std::wstring monthText = Trim(GetText(g_month));
    const std::wstring yearText = Trim(GetText(g_year));
    if (dayText.empty() || monthText.empty() || yearText.empty())
    {
        error = Text::ErrDateMissing;
        return false;
    }
    const int day = _wtoi(dayText.c_str());
    const int month = _wtoi(monthText.c_str());
    const int year = _wtoi(yearText.c_str());
    if (year < 1601 || year > 9999 || month < 1 || month > 12 || day < 1 || day > 31)
    {
        error = Text::ErrDateRange;
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
        error = Text::ErrDateNotExist;
        return false;
    }
    return true;
}

// Whole days from today (local time) until the given date.
long long DaysUntil(const SYSTEMTIME& date)
{
    SYSTEMTIME today{};
    GetLocalTime(&today);
    SYSTEMTIME a{};
    a.wYear = today.wYear;
    a.wMonth = today.wMonth;
    a.wDay = today.wDay;
    SYSTEMTIME b{};
    b.wYear = date.wYear;
    b.wMonth = date.wMonth;
    b.wDay = date.wDay;
    FILETIME fa{}, fb{};
    SystemTimeToFileTime(&a, &fa);
    SystemTimeToFileTime(&b, &fb);
    const auto value = [](const FILETIME& f) {
        return static_cast<long long>((static_cast<unsigned long long>(f.dwHighDateTime) << 32) | f.dwLowDateTime);
    };
    return (value(fb) - value(fa)) / 864000000000LL;
}

// One-line summary shown in the expiration card header.
std::wstring ExpirySummary(COLORREF& color)
{
    color = wlqg::MutedTextColor();
    if (NoExpiry())
        return Localize(Text::NeverExpires);
    SYSTEMTIME expiration{};
    Text error{};
    if (!BuildExpiration(expiration, error))
    {
        if (error == Text::ErrDateMissing)
            return Localize(Text::DateIncomplete);
        color = wlqg::AccentRed();
        return Localize(Text::DateInvalidShort);
    }
    const long long days = DaysUntil(expiration);
    if (days < 0)
    {
        color = wlqg::AccentAmber();
        return Localize(Text::DateInPast);
    }
    color = wlqg::AccentGreen();
    if (days == 0)
        return Localize(Text::ExpiresToday);
    return Localize(Text::ExpiresInDays, std::to_wstring(days));
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
    SetStatus(StatusKind::Busy, Text::Working);

    if (g_dllPath.empty())
    {
        Fail(Text::ErrNoDll);
        return;
    }
    if (ArchitectureMismatch())
    {
        Fail(Text::ErrWrongArch, MatchingBuild());
        return;
    }
    if (g_licenseFileName.find_first_of(L"\\/:*?\"<>|") != std::wstring::npos ||
        g_licenseFileName == L"." || g_licenseFileName == L".." ||
        g_licenseFileName.find(L"..") != std::wstring::npos)
    {
        Fail(Text::ErrBadFileName);
        return;
    }

    const std::wstring hwidWide = Trim(GetText(g_hwid));
    if (hwidWide.empty())
    {
        Fail(Text::ErrEmptyHwid);
        return;
    }

    const std::string hwid = ToAnsi(hwidWide);
    const std::string name = ToAnsi(g_nameIsHwid ? hwidWide : g_configuredName);
    const std::string org = ToAnsi(g_organization);
    const std::string custom = ToAnsi(g_customData);
    const std::string hash = ToAnsi(g_licenseHash);

    if (!g_customMode && hash.empty())
    {
        Fail(Text::ErrNeedHash);
        return;
    }

    sLicenseFeatures features{};
    features.cb = sizeof(features);
    if (!NoExpiry())
    {
        Text error{};
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
        if (loadError == ERROR_BAD_EXE_FORMAT)
            Fail(Text::ErrWrongArch, OwnMachine() == 0x8664 ? L"x86" : L"x64");
        else
            Fail(Text::ErrLoadFailed, std::to_wstring(loadError));
        return;
    }

    std::vector<char> buffer(64 * 1024, 0);
    int size = 0;
    if (g_customMode)
    {
        auto function = reinterpret_cast<CustomFileKeyEx>(
            reinterpret_cast<void*>(GetProcAddress(library, "WLCustomGenLicenseFileKeyEx")));
        if (!function)
        {
            FreeLibrary(library);
            restoreDirectories();
            Fail(Text::ErrMissingExport, L"WLCustomGenLicenseFileKeyEx");
            return;
        }
        size = function(Ptr(name), Ptr(org), Ptr(custom), Ptr(hwid), &features, buffer.data());
    }
    else
    {
        auto function = reinterpret_cast<StandardFileKeyEx>(
            reinterpret_cast<void*>(GetProcAddress(library, "WLGenLicenseFileKeyEx")));
        if (!function)
        {
            FreeLibrary(library);
            restoreDirectories();
            Fail(Text::ErrMissingExport, L"WLGenLicenseFileKeyEx");
            return;
        }
        size = function(Ptr(hash), Ptr(name), Ptr(org), Ptr(custom), Ptr(hwid), &features, buffer.data());
    }
    FreeLibrary(library);
    restoreDirectories();

    if (size <= 0 || size > static_cast<int>(buffer.size()))
    {
        Fail(Text::ErrSdkFailed, std::to_wstring(size));
        return;
    }

    const std::wstring outputPath = ModuleDirectory() + L"\\" + g_licenseFileName;
    // Overwrite without prompting (a .previous copy is kept) so automation never blocks.
    if (FileExists(outputPath))
        CopyFileW(outputPath.c_str(), (outputPath + L".previous").c_str(), FALSE);

    const std::wstring temporary = outputPath + L".tmp";
    {
        std::ofstream output(temporary.c_str(), std::ios::binary | std::ios::trunc);
        if (!output)
        {
            Fail(Text::ErrWriteFailed);
            return;
        }
        output.write(buffer.data(), size);
        if (!output)
        {
            output.close();
            DeleteFileW(temporary.c_str());
            Fail(Text::ErrWriteIncomplete);
            return;
        }
    }
    if (!MoveFileExW(temporary.c_str(), outputPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        DeleteFileW(temporary.c_str());
        Fail(Text::ErrPlaceFailed);
        return;
    }

    g_lastOutput = outputPath;
    EnableWindow(g_showFile, TRUE);
    SetStatus(StatusKind::Ok, Text::Written, g_licenseFileName, std::to_wstring(size));
}

void ShowOutputFile()
{
    if (g_lastOutput.empty() || !FileExists(g_lastOutput))
        return;
    const std::wstring arguments = L"/select,\"" + g_lastOutput + L"\"";
    ShellExecuteW(g_window, L"open", L"explorer.exe", arguments.c_str(), nullptr, SW_SHOWNORMAL);
}

// ---------------------------------------------------------------------------
// User interface.
// ---------------------------------------------------------------------------
void UpdateDateFields()
{
    const bool enabled = !NoExpiry();
    EnableWindow(g_day, enabled);
    EnableWindow(g_month, enabled);
    EnableWindow(g_year, enabled);
    RECT card = kExpiryCard;
    InvalidateRect(g_window, &card, FALSE);
}

void ApplyLanguage()
{
    SetWindowTextW(g_generate, Localize(Text::Generate));
    SetWindowTextW(g_showFile, Localize(Text::ShowFile));
    SetWindowTextW(g_noExpire, Localize(Text::NoExpiry));
    SetWindowTextW(g_language, wlqg::LanguageCode(wlqg::CurrentLanguage()));
    SendMessageW(g_hwid, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(Localize(Text::HwidCue)));
    RefreshStatus();
    RedrawWindow(g_window, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
}

void CycleLanguage()
{
    const wlqg::Language next = wlqg::NextLanguage(wlqg::CurrentLanguage());
    wlqg::SetCurrentLanguage(next);
    std::wstring code = wlqg::LanguageCode(next);
    for (wchar_t& c : code)
        c = static_cast<wchar_t>(towlower(c));
    WritePrivateProfileStringW(L"WLQuickGen", L"Language", code.c_str(), IniPath().c_str());
    ApplyLanguage();
}

HWND AddEdit(int id, DWORD style, const RECT& field, HFONT font)
{
    return wlqg::CreateFieldEdit(g_window, id, style, field, font);
}

void CreateControls()
{
    g_font = wlqg::CreateUiFont(-15);
    g_titleFont = wlqg::CreateUiFont(-26, true);
    g_smallFont = wlqg::CreateUiFont(-13);
    g_smallBoldFont = wlqg::CreateUiFont(-12, true);
    g_monoFont = wlqg::CreateMonoFont(-15);

    g_language = wlqg::CreateThemedButton(g_window, IDC_LANGUAGE, L"EN", wlqg::ButtonKind::Secondary,
        kRight - kLanguageWidth, 26, kLanguageWidth, 30, g_smallBoldFont);

    g_hwid = AddEdit(IDC_HWID, ES_AUTOHSCROLL, kHwidField, g_monoFont);
    SendMessageW(g_hwid, EM_LIMITTEXT, 4096, 0);

    // A real BS_AUTOCHECKBOX keeps BM_GETCHECK / click semantics for automation;
    // it is painted through NM_CUSTOMDRAW.
    g_noExpire = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
        kCheckRect.left, kCheckRect.top, kCheckRect.right - kCheckRect.left, kCheckRect.bottom - kCheckRect.top,
        g_window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_NOEXPIRE)), nullptr, nullptr);
    wlqg::ApplyUiFont(g_noExpire, g_font);
    SendMessageW(g_noExpire, BM_SETCHECK, BST_CHECKED, 0);

    g_day = AddEdit(IDC_DAY, ES_NUMBER | ES_CENTER, kDayField, g_font);
    g_month = AddEdit(IDC_MONTH, ES_NUMBER | ES_CENTER, kMonthField, g_font);
    g_year = AddEdit(IDC_YEAR, ES_NUMBER | ES_CENTER, kYearField, g_font);
    SendMessageW(g_day, EM_LIMITTEXT, 2, 0);
    SendMessageW(g_month, EM_LIMITTEXT, 2, 0);
    SendMessageW(g_year, EM_LIMITTEXT, 4, 0);

    g_status = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
        kStatusRect.left, kStatusRect.top, kStatusRect.right - kStatusRect.left, kStatusRect.bottom - kStatusRect.top,
        g_window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_STATUS)), nullptr, nullptr);

    const int generateWidth = 160;
    const int showWidth = 150;
    g_generate = wlqg::CreateThemedButton(g_window, IDC_GENERATE, L"", wlqg::ButtonKind::Primary,
        kRight - generateWidth, kButtonTop, generateWidth, kButtonHeight, g_font);
    g_showFile = wlqg::CreateThemedButton(g_window, IDC_SHOWFILE, L"", wlqg::ButtonKind::Secondary,
        kRight - generateWidth - 12 - showWidth, kButtonTop, showWidth, kButtonHeight, g_font);
    EnableWindow(g_showFile, FALSE);

    ApplyLanguage();
    SetReadyStatus();
    UpdateDateFields();
    SetFocus(g_hwid);
}

void PaintText(HDC dc, const std::wstring& text, RECT rect, UINT format)
{
    DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &rect, format | DT_SINGLELINE | DT_NOPREFIX);
}

void DrawFieldFor(HDC dc, HWND edit, const RECT& field)
{
    wlqg::DrawField(dc, field, GetFocus() == edit, IsWindowEnabled(edit) != FALSE);
}

void Paint(HDC dc)
{
    RECT client{0, 0, kClientWidth, kClientHeight};
    FillRect(dc, &client, wlqg::BackgroundBrush());
    SetBkMode(dc, TRANSPARENT);

    // Header.
    HGDIOBJ old = SelectObject(dc, g_titleFont);
    SetTextColor(dc, wlqg::TextColor());
    TextOutW(dc, kMargin, 22, L"WLQuickGen", 10);
    SelectObject(dc, g_font);
    SetTextColor(dc, wlqg::MutedTextColor());
    PaintText(dc, Localize(Text::Subtitle), {kMargin, 60, kRight, 80}, DT_LEFT | DT_END_ELLIPSIS);

    Text badge = Text::BadgeCustomSdk;
    COLORREF badgeColor = wlqg::AccentGreen();
    if (g_dllPath.empty())
    {
        badge = Text::BadgeNoSdk;
        badgeColor = wlqg::AccentRed();
    }
    else if (ArchitectureMismatch())
    {
        badge = Text::BadgeWrongArch;
        badgeColor = wlqg::AccentAmber();
    }
    else if (!g_customMode)
    {
        badge = Text::BadgeStandardSdk;
        badgeColor = wlqg::AccentBlue();
    }
    wlqg::DrawBadge(dc, kRight - kLanguageWidth - 10, 28, Localize(badge), badgeColor, g_smallFont);

    // HWID card.
    wlqg::DrawCard(dc, kHwidCard);
    SelectObject(dc, g_smallBoldFont);
    SetTextColor(dc, wlqg::MutedTextColor());
    const RECT hwidLabel{kHwidCard.left + 20, kHwidCard.top + 12, kHwidCard.right - 20, kHwidCard.top + 32};
    PaintText(dc, Localize(Text::HwidCard), hwidLabel, DT_LEFT | DT_VCENTER);
    const std::wstring hwid = Trim(GetText(g_hwid));
    if (!hwid.empty())
    {
        SelectObject(dc, g_smallFont);
        PaintText(dc, Localize(Text::HwidLength, std::to_wstring(hwid.size())), hwidLabel, DT_RIGHT | DT_VCENTER);
    }
    DrawFieldFor(dc, g_hwid, kHwidField);

    // Expiration card.
    wlqg::DrawCard(dc, kExpiryCard);
    SelectObject(dc, g_smallBoldFont);
    SetTextColor(dc, wlqg::MutedTextColor());
    const RECT expiryLabel{kExpiryCard.left + 20, kExpiryCard.top + 12, kExpiryCard.right - 20, kExpiryCard.top + 32};
    PaintText(dc, Localize(Text::ExpirationCard), expiryLabel, DT_LEFT | DT_VCENTER);
    COLORREF summaryColor{};
    const std::wstring summary = ExpirySummary(summaryColor);
    SelectObject(dc, g_smallFont);
    SetTextColor(dc, summaryColor);
    PaintText(dc, summary, expiryLabel, DT_RIGHT | DT_VCENTER | DT_END_ELLIPSIS);
    DrawFieldFor(dc, g_day, kDayField);
    DrawFieldFor(dc, g_month, kMonthField);
    DrawFieldFor(dc, g_year, kYearField);
    SelectObject(dc, g_font);
    SetTextColor(dc, NoExpiry() ? wlqg::BorderColor() : wlqg::MutedTextColor());
    PaintText(dc, L"/", {kDayField.right, kDayField.top, kMonthField.left, kDayField.bottom}, DT_CENTER | DT_VCENTER);
    PaintText(dc, L"/", {kMonthField.right, kMonthField.top, kYearField.left, kMonthField.bottom}, DT_CENTER | DT_VCENTER);

    // Where the license goes and which SDK produces it.
    SelectObject(dc, g_smallFont);
    SetTextColor(dc, wlqg::MutedTextColor());
    const std::wstring output = ModuleDirectory() + L"\\" + g_licenseFileName;
    PaintText(dc, Localize(Text::OutputInfo, output), kOutputInfo, DT_LEFT | DT_VCENTER | DT_PATH_ELLIPSIS);
    PaintText(dc, g_dllPath.empty() ? std::wstring(Localize(Text::SdkMissingInfo)) : Localize(Text::SdkInfo, g_dllPath),
        kSdkInfo, DT_LEFT | DT_VCENTER | DT_PATH_ELLIPSIS);

    // Footer.
    const COLORREF footer = wlqg::Blend(wlqg::MutedTextColor(), wlqg::BackgroundColor(), 70);
    SetTextColor(dc, footer);
    const std::wstring version = std::wstring(wlqg::kVersionLabel) + (OwnMachine() == 0x8664 ? L" · x64" : L" · x86");
    PaintText(dc, version, {kMargin, kClientHeight - 30, kRight, kClientHeight - 10}, DT_LEFT | DT_VCENTER);
    PaintText(dc, L"@FerS0", {kMargin, kClientHeight - 30, kRight, kClientHeight - 10}, DT_RIGHT | DT_VCENTER);
    SelectObject(dc, old);
}

void PaintBuffered(HWND window)
{
    PAINTSTRUCT paint{};
    HDC dc = BeginPaint(window, &paint);
    HDC memory = CreateCompatibleDC(dc);
    HBITMAP bitmap = CreateCompatibleBitmap(dc, kClientWidth, kClientHeight);
    HGDIOBJ oldBitmap = SelectObject(memory, bitmap);
    Paint(memory);
    BitBlt(dc, paint.rcPaint.left, paint.rcPaint.top, paint.rcPaint.right - paint.rcPaint.left,
        paint.rcPaint.bottom - paint.rcPaint.top, memory, paint.rcPaint.left, paint.rcPaint.top, SRCCOPY);
    SelectObject(memory, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(memory);
    EndPaint(window, &paint);
}

void PaintStatus(const DRAWITEMSTRUCT& item)
{
    FillRect(item.hDC, &item.rcItem, wlqg::BackgroundBrush());
    // Error texts continue the "ERROR: " prefix, so they start in lower case.
    std::wstring text = StatusMessage();
    if (!text.empty())
        CharUpperBuffW(&text[0], 1);
    wlqg::DrawStatusLine(item.hDC, item.rcItem, text, StatusColor(), g_font);
}

LRESULT PaintCheckbox(NMCUSTOMDRAW& draw)
{
    if (draw.dwDrawStage != CDDS_PREPAINT)
        return CDRF_DODEFAULT;
    wlqg::DrawCheckbox(draw.hdc, draw.rc, GetText(g_noExpire), NoExpiry(),
        (draw.uItemState & CDIS_HOT) != 0, (draw.uItemState & CDIS_FOCUS) != 0,
        wlqg::SurfaceColor(), g_font);
    return CDRF_SKIPDEFAULT;
}

// Repaints the field border (focus ring) around an edit.
void InvalidateField(HWND edit)
{
    const RECT* field = edit == g_hwid ? &kHwidField
        : edit == g_day ? &kDayField
        : edit == g_month ? &kMonthField
        : edit == g_year ? &kYearField
        : nullptr;
    if (field)
        InvalidateRect(g_window, field, FALSE);
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        g_window = window;
        wlqg::ApplyDarkTitleBar(window);
        DetectSdk();
        LoadSettings();
        CreateControls();
        return 0;
    case WM_CTLCOLOREDIT:
        return wlqg::ColorFieldEdit(reinterpret_cast<HDC>(wParam), true);
    case WM_CTLCOLORSTATIC:
    {
        // Disabled edits report here.
        HWND control = reinterpret_cast<HWND>(lParam);
        if (control == g_day || control == g_month || control == g_year || control == g_hwid)
            return wlqg::ColorFieldEdit(reinterpret_cast<HDC>(wParam), false);
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetBkColor(dc, wlqg::BackgroundColor());
        SetTextColor(dc, wlqg::TextColor());
        return reinterpret_cast<LRESULT>(wlqg::BackgroundBrush());
    }
    case WM_DRAWITEM:
    {
        const auto* item = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
        if (item->CtlType == ODT_STATIC)
            PaintStatus(*item);
        else
            wlqg::DrawThemedButton(*item);
        return TRUE;
    }
    case WM_NOTIFY:
    {
        auto* header = reinterpret_cast<NMHDR*>(lParam);
        if (header->idFrom == IDC_NOEXPIRE && header->code == NM_CUSTOMDRAW)
            return PaintCheckbox(*reinterpret_cast<NMCUSTOMDRAW*>(lParam));
        break;
    }
    case WM_COMMAND:
    {
        const int id = LOWORD(wParam);
        const int code = HIWORD(wParam);
        switch (id)
        {
        case IDC_NOEXPIRE:
            UpdateDateFields();
            break;
        case IDOK: // Enter inside any field.
        case IDC_GENERATE:
            Generate();
            break;
        case IDC_SHOWFILE:
            ShowOutputFile();
            break;
        case IDC_LANGUAGE:
            CycleLanguage();
            break;
        case IDC_HWID:
        case IDC_DAY:
        case IDC_MONTH:
        case IDC_YEAR:
            if (code == EN_SETFOCUS || code == EN_KILLFOCUS)
                InvalidateField(reinterpret_cast<HWND>(lParam));
            else if (code == EN_CHANGE)
            {
                RECT card = id == IDC_HWID ? kHwidCard : kExpiryCard;
                card.bottom = card.top + 36; // header row only
                InvalidateRect(window, &card, FALSE);
            }
            break;
        default:
            break;
        }
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        PaintBuffered(window);
        return 0;
    case WM_DESTROY:
        for (HFONT font : {g_font, g_titleFont, g_smallFont, g_smallBoldFont, g_monoFont})
            if (font)
                DeleteObject(font);
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
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&controls);

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
    windowClass.hbrBackground = wlqg::BackgroundBrush();
    windowClass.hIcon = icon;
    windowClass.hIconSm = iconSmall;
    RegisterClassExW(&windowClass);

    constexpr DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
    RECT desired{0, 0, kClientWidth, kClientHeight};
    AdjustWindowRectEx(&desired, style, FALSE, 0);
    HWND window = CreateWindowExW(0, className, L"WLQuickGen", style,
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
    return static_cast<int>(message.wParam);
}
