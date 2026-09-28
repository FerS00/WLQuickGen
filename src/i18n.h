#pragma once

#include <string>

namespace wlqg {

enum class Language { English, Spanish, Portuguese };

enum class Text {
    Subtitle,
    BadgeCustomSdk,
    BadgeStandardSdk,
    BadgeNoSdk,
    BadgeWrongArch,
    HwidCard,
    HwidCue,
    HwidLength,
    ExpirationCard,
    NoExpiry,
    NeverExpires,
    ExpiresToday,
    ExpiresInDays,
    DateInPast,
    DateIncomplete,
    DateInvalidShort,
    OutputInfo,
    SdkInfo,
    SdkMissingInfo,
    Generate,
    ShowFile,
    // Status line.
    Ready,
    ReadyNoSdk,
    Working,
    Written,
    // Errors.
    ErrNoDll,
    ErrBadFileName,
    ErrEmptyHwid,
    ErrNeedHash,
    ErrDateMissing,
    ErrDateRange,
    ErrDateNotExist,
    ErrWrongArch,
    ErrLoadFailed,
    ErrMissingExport,
    ErrSdkFailed,
    ErrWriteFailed,
    ErrWriteIncomplete,
    ErrPlaceFailed,
};

// Language picked from WLQuickGen.ini ("auto", "en", "es", "pt"); "auto"
// follows the Windows display language and falls back to English.
Language ResolveLanguage(const std::wstring& setting);
Language CurrentLanguage();
void SetCurrentLanguage(Language language);
Language NextLanguage(Language language);
// Short code shown on the language button and stored in the ini ("EN", "ES", "PT").
const wchar_t* LanguageCode(Language language);

const wchar_t* Localize(Text text);
// Replaces "{0}" and "{1}" in the localized text.
std::wstring Localize(Text text, const std::wstring& first, const std::wstring& second = {});

} // namespace wlqg
