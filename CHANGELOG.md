# Changelog

## [1.1.0] — 2026-09-28

### Changed

- New interface with the same design as GET_HWID: dark palette, rounded cards for
  **HWID** and **Expiration**, custom buttons with hover/pressed/disabled states,
  custom-drawn checkbox, colored status line, dark title bar on Windows 10/11,
  double-buffered painting and a monospaced HWID field.
- SDK badge in the header: custom SDK (green), standard SDK (blue), architecture
  mismatch (amber) or SDK not found (red). The output path and the DLL in use are
  shown under the cards.
- Live expiration summary ("Expires in 459 days", date in the past, invalid date)
  and HWID character count.
- Long status messages wrap onto a second line instead of being cut off.
- Sources moved to `src/` (`WLQuickGen.cpp`, `theme.*`, `i18n.*`); version
  resource added to the executable.

### Added

- **Dynamic language**: English, Spanish and Portuguese. Follows the Windows
  display language by default; the `EN`/`ES`/`PT` button switches it live and
  stores it in `WLQuickGen.ini` (`Language=auto|en|es|pt`).
- **Show file** button (control `1013`): opens Explorer with the generated
  license selected.
- Enter in any field generates the license.
- Architecture check at startup: a 32/64-bit mismatch between the executable and
  the generator DLL is reported before generating, naming the build to use.
- `CMakeLists.txt` and a GitHub Actions workflow that builds x86/x64, verifies
  the PE machine type and publishes releases with SHA-256 checksums.
- Spanish documentation: user manual, build guide and logic graphs
  (`docs/MANUAL_USUARIO.md`, `docs/COMPILACION.md`, `docs/GRAFO_LOGICA.md`).

### Kept (automation contract)

- Window class `WLQuickGenWindow`, title `WLQuickGen`, control IDs `1002`,
  `1006`–`1009`, `1011`, `1012`.
- The status text still starts with `OK:` or `ERROR:` in every language; only
  the message after the prefix is translated. `examples/automate.py` works
  unchanged.
- No modal dialogs.

### Removed

- The `by FerS0` static control (`1014`); the signature is now painted in the
  footer as `@FerS0` next to the version.

## [1.0.0] — 2026-09-17

- First release: portable FileKey license generator using the ANSI
  `WLCustomGenLicenseFileKeyEx` / `WLGenLicenseFileKeyEx` exports, per-folder
  `WLQuickGen.ini`, atomic writes with `.previous` backup, x86 and x64 builds.
