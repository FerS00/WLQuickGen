# WLQuickGen

A small, portable license generator for products protected with
[WinLicense](https://www.oreans.com/WinLicense.php).

Drop a single `.exe` into the `Specific Generators\<Product>\` folder that
WinLicense creates, paste the target machine's HWID, press **GENERATE**, and the
license file is written next to the executable.

It is a leaner replacement for the `WLGen_<Product>.exe` generator that
WinLicense ships: same official SDK exports, one screen, and built for UI
automation.

![WLQuickGen](docs/screenshot.png)

## Why

When you already have many protected products, re-protecting each one just to
change how licenses are issued is expensive. WLQuickGen needs nothing from the
product: it only uses the generator DLL that WinLicense already produced for it.

## Features

- **Portable** — one executable per generator folder, no installation.
- **Auto-detects the SDK** — finds `CustomWinlicenseSDK.dll` (custom generator,
  no license hash needed) or `WinLicenseSDK.dll` (standard generator).
- **Minimal UI** — HWID, expiration date or "No expiry", GENERATE.
- **Automation friendly** — stable control IDs, no modal dialogs, results
  reported as text. Works out of the box with `pywinauto`.
- **x86 and x64 builds** — match the architecture of the generator DLL.
- **Safe writes** — atomic replace, previous license kept as `.previous`.

## Usage

1. Copy the build matching your generator DLL's architecture (usually x86) into
   the product folder, e.g. `...\Specific Generators\DPP\`.
2. Run it once — it creates `WLQuickGen.ini` next to itself.
3. Set the license file name in that ini (see below).
4. Paste the HWID, choose the expiration, press **GENERATE**.

The tool searches for the SDK in its own folder, in `DLL\` and `EXE\`, in the
parent folder, and one level of product subfolders — so it works whether you put
it in the product root or in `Specific Generators\` next to several products.

## Configuration (`WLQuickGen.ini`)

Everything that does not change per license lives here, which keeps the window
minimal:

```ini
[WLQuickGen]
LicenseFileName=License.dat
NameIsHwid=1
Name=
Organization=
CustomData=
LicenseHash=
```

| Key | Meaning |
|---|---|
| `LicenseFileName` | Name of the generated file. Products differ — set it once per folder. |
| `NameIsHwid` | `1` uses the HWID as the registration name (what the stock generator does). `0` uses `Name`. |
| `Name` / `Organization` / `CustomData` | Optional license fields. Empty means "not set" (`NULL`), which is not the same as an empty string to the SDK. |
| `LicenseHash` | Required **only** for the standard SDK (`WinLicenseSDK.dll`). The custom DLL embeds it. |

## Automation with pywinauto

No modal dialogs are ever shown; the status line reports the outcome and starts
with `OK:` or `ERROR:`.

| Control | `control_id` | Class |
|---|---|---|
| HWID | 1002 | Edit |
| No expiry | 1006 | Button (checkbox) |
| Day / Month / Year | 1007 / 1008 / 1009 | Edit |
| GENERATE | 1011 | Button |
| Status | 1012 | Static |

Window class `WLQuickGenWindow`, title `WLQuickGen`.

```python
from pywinauto.application import Application
import time

app = Application(backend="win32").start(r"...\DPP\WLQuickGen-x86.exe")
dlg = app.window(class_name="WLQuickGenWindow")
dlg.wait("ready", timeout=10)

dlg.child_window(control_id=1002, class_name="Edit").set_edit_text(hwid)
dlg.child_window(control_id=1011, class_name="Button").click()
time.sleep(1)

status = dlg.child_window(control_id=1012, class_name="Static").window_text()
assert status.startswith("OK:"), status
dlg.close()
```

A ready-to-run version is in [`examples/automate.py`](examples/automate.py).

> Use 32-bit Python to drive the x86 build; pywinauto warns otherwise (it still
> works for these controls, but matching bitness is more reliable).

## Building

Requires Visual Studio with the C++ toolset (x86 and x64) and the Windows SDK
(`rc.exe`).

```
build.bat
```

Produces `WLQuickGen-x86.exe` and `WLQuickGen-x64.exe`. If Visual Studio is not
in the default location, edit `VCVARS` at the top of `build.bat`.

## ⚠️ Do not commit WinLicense material

This repository contains **only** the generator's own source. The `.gitignore`
blocks the files that carry your product's generation secrets:

- `CustomWinlicenseSDK.dll`, `WinLicenseSDK.dll`, `ECCfunctions.dll`
- `GeneratorSeed.gns`, `GeneratorDatabase.abs`
- `WLQuickGen.ini` (may contain `LicenseHash`)
- generated `.dat` / `.txt` / `.reg` license files

Publishing any of those would let anyone generate licenses for your product.
Check `git status` before your first push.

## Notes and limits

- The executable's architecture **must** match the generator DLL's. A 32-bit
  process cannot load a 64-bit DLL. The tool reports this clearly.
- Supported family: **FileKey** (license file). TextKey, Registry, SmartKey and
  Dynamic SmartKey are out of scope.
- The tool calls the **ANSI** SDK exports (`WLCustomGenLicenseFileKeyEx` /
  `WLGenLicenseFileKeyEx`). The Unicode variants embed the name as UTF-16 and
  produce a file the protected product rejects as a corrupt license.
- Licenses are not deterministic: generating twice with identical input yields
  different bytes. Compare sizes, not bytes.

## License

Not chosen yet — add one before publishing.
