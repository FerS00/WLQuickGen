#include "i18n.h"

#include <windows.h>

#include <cwctype>

namespace wlqg {
namespace {

struct Entry {
    Text key;
    const wchar_t* en;
    const wchar_t* es;
    const wchar_t* pt;
};

constexpr Entry kTexts[] = {
    {Text::Subtitle, L"WinLicense license generator · FileKey",
        L"Generador de licencias WinLicense · FileKey", L"Gerador de licenças WinLicense · FileKey"},
    {Text::BadgeCustomSdk, L"Custom SDK", L"SDK personalizado", L"SDK personalizado"},
    {Text::BadgeStandardSdk, L"Standard SDK", L"SDK estándar", L"SDK padrão"},
    {Text::BadgeNoSdk, L"SDK not found", L"SDK no encontrado", L"SDK não encontrado"},
    {Text::BadgeWrongArch, L"Architecture mismatch", L"Arquitectura distinta", L"Arquitetura diferente"},
    {Text::HwidCard, L"HWID", L"HWID", L"HWID"},
    {Text::HwidCue, L"Paste the HWID of the target machine", L"Pega el HWID del equipo destino",
        L"Cole o HWID do computador de destino"},
    {Text::HwidLength, L"{0} characters", L"{0} caracteres", L"{0} caracteres"},
    {Text::ExpirationCard, L"EXPIRATION", L"CADUCIDAD", L"EXPIRAÇÃO"},
    {Text::NoExpiry, L"No expiry", L"Sin caducidad", L"Sem expiração"},
    {Text::NeverExpires, L"The license never expires", L"La licencia no caduca", L"A licença não expira"},
    {Text::ExpiresToday, L"Expires today at 23:59", L"Caduca hoy a las 23:59", L"Expira hoje às 23:59"},
    {Text::ExpiresInDays, L"Expires in {0} days", L"Caduca en {0} días", L"Expira em {0} dias"},
    {Text::DateInPast, L"That date is already in the past", L"Esa fecha ya pasó", L"Essa data já passou"},
    {Text::DateIncomplete, L"Enter the date as DD / MM / YYYY", L"Introduce la fecha como DD / MM / AAAA",
        L"Informe a data como DD / MM / AAAA"},
    {Text::DateInvalidShort, L"Invalid date", L"Fecha no válida", L"Data inválida"},
    {Text::OutputInfo, L"Output  {0}", L"Salida  {0}", L"Saída  {0}"},
    {Text::SdkInfo, L"SDK  {0}", L"SDK  {0}", L"SDK  {0}"},
    {Text::SdkMissingInfo, L"SDK  place this executable inside Specific Generators\\<Product>\\",
        L"SDK  coloca este ejecutable dentro de Specific Generators\\<Producto>\\",
        L"SDK  coloque este executável dentro de Specific Generators\\<Produto>\\"},
    {Text::Generate, L"Generate", L"Generar", L"Gerar"},
    {Text::ShowFile, L"Show file", L"Ver archivo", L"Ver arquivo"},

    {Text::Ready, L"Ready. Paste an HWID and press Generate.", L"Listo. Pega un HWID y pulsa Generar.",
        L"Pronto. Cole um HWID e clique em Gerar."},
    {Text::ReadyNoSdk, L"Generator DLL not found next to this executable.",
        L"No se encontró la DLL del generador junto a este ejecutable.",
        L"A DLL do gerador não foi encontrada junto a este executável."},
    {Text::Working, L"Working…", L"Generando…", L"Gerando…"},
    {Text::Written, L"{0} written ({1} bytes)", L"{0} generado ({1} bytes)", L"{0} gerado ({1} bytes)"},

    {Text::ErrNoDll, L"generator DLL not found. Place this executable inside the Specific Generators folder of the product.",
        L"no se encontró la DLL del generador. Coloca este ejecutable dentro de la carpeta Specific Generators del producto.",
        L"a DLL do gerador não foi encontrada. Coloque este executável dentro da pasta Specific Generators do produto."},
    {Text::ErrBadFileName, L"LicenseFileName in WLQuickGen.ini must be a plain file name.",
        L"LicenseFileName en WLQuickGen.ini debe ser un nombre de archivo simple.",
        L"LicenseFileName em WLQuickGen.ini deve ser um nome de arquivo simples."},
    {Text::ErrEmptyHwid, L"HWID is empty.", L"el HWID está vacío.", L"o HWID está vazio."},
    {Text::ErrNeedHash, L"the standard SDK (WinLicenseSDK.dll) needs LicenseHash in WLQuickGen.ini.",
        L"el SDK estándar (WinLicenseSDK.dll) necesita LicenseHash en WLQuickGen.ini.",
        L"o SDK padrão (WinLicenseSDK.dll) precisa de LicenseHash em WLQuickGen.ini."},
    {Text::ErrDateMissing, L"enter day, month and year, or tick \"No expiry\".",
        L"introduce día, mes y año, o marca \"Sin caducidad\".",
        L"informe dia, mês e ano, ou marque \"Sem expiração\"."},
    {Text::ErrDateRange, L"invalid date (day 1-31, month 1-12, year 1601-9999).",
        L"fecha no válida (día 1-31, mes 1-12, año 1601-9999).",
        L"data inválida (dia 1-31, mês 1-12, ano 1601-9999)."},
    {Text::ErrDateNotExist, L"that date does not exist in the calendar.", L"esa fecha no existe en el calendario.",
        L"essa data não existe no calendário."},
    {Text::ErrWrongArch, L"the generator DLL has a different architecture. Use the {0} build.",
        L"la DLL del generador tiene otra arquitectura. Usa la versión {0}.",
        L"a DLL do gerador tem outra arquitetura. Use a versão {0}."},
    {Text::ErrLoadFailed, L"could not load the generator DLL (error {0}).",
        L"no se pudo cargar la DLL del generador (error {0}).",
        L"não foi possível carregar a DLL do gerador (erro {0})."},
    {Text::ErrMissingExport, L"the DLL does not export {0}.", L"la DLL no exporta {0}.", L"a DLL não exporta {0}."},
    {Text::ErrSdkFailed, L"the SDK did not generate the license (returned {0}).",
        L"el SDK no generó la licencia (devolvió {0}).", L"o SDK não gerou a licença (retornou {0})."},
    {Text::ErrWriteFailed, L"could not write the license file.", L"no se pudo escribir el archivo de licencia.",
        L"não foi possível gravar o arquivo de licença."},
    {Text::ErrWriteIncomplete, L"could not complete the license file.", L"no se pudo completar el archivo de licencia.",
        L"não foi possível concluir o arquivo de licença."},
    {Text::ErrPlaceFailed, L"could not place the license file next to the executable.",
        L"no se pudo colocar el archivo de licencia junto al ejecutable.",
        L"não foi possível colocar o arquivo de licença junto ao executável."},
};

Language g_language = Language::English;

Language SystemLanguage() {
    switch (PRIMARYLANGID(GetUserDefaultUILanguage())) {
    case LANG_SPANISH: return Language::Spanish;
    case LANG_PORTUGUESE: return Language::Portuguese;
    default: return Language::English;
    }
}

} // namespace

Language ResolveLanguage(const std::wstring& setting) {
    std::wstring value;
    for (wchar_t c : setting) {
        if (!std::iswspace(c)) value.push_back(static_cast<wchar_t>(std::towlower(c)));
    }
    if (value == L"en") return Language::English;
    if (value == L"es") return Language::Spanish;
    if (value == L"pt") return Language::Portuguese;
    return SystemLanguage();
}

Language CurrentLanguage() { return g_language; }

void SetCurrentLanguage(Language language) { g_language = language; }

Language NextLanguage(Language language) {
    switch (language) {
    case Language::English: return Language::Spanish;
    case Language::Spanish: return Language::Portuguese;
    default: return Language::English;
    }
}

const wchar_t* LanguageCode(Language language) {
    switch (language) {
    case Language::Spanish: return L"ES";
    case Language::Portuguese: return L"PT";
    default: return L"EN";
    }
}

const wchar_t* Localize(Text text) {
    for (const Entry& entry : kTexts) {
        if (entry.key != text) continue;
        switch (g_language) {
        case Language::Spanish: return entry.es;
        case Language::Portuguese: return entry.pt;
        default: return entry.en;
        }
    }
    return L"";
}

std::wstring Localize(Text text, const std::wstring& first, const std::wstring& second) {
    std::wstring result = Localize(text);
    std::size_t position = result.find(L"{0}");
    if (position != std::wstring::npos) result.replace(position, 3, first);
    position = result.find(L"{1}");
    if (position != std::wstring::npos) result.replace(position, 3, second);
    return result;
}

} // namespace wlqg
