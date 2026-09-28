# Third-party notices

WLQuickGen's own source code is licensed under the
[PolyForm Noncommercial License 1.0.0](LICENSE). That license does not apply to
the third-party material listed below, which is not owned by the author of
WLQuickGen.

| Component | Location in this repository | Source / origin | License | Copyright | Project link |
|---|---|---|---|---|---|
| WinLicense SDK — function signatures and the `sLicenseFeatures` structure layout | Declarations in `src/WLQuickGen.cpp` | Oreans WinLicense SDK documentation/headers, reproduced only as the interface needed to call the SDK at runtime | Proprietary (Oreans Technologies). The SDK itself is **not** included or redistributed. | Oreans Technologies | <https://www.oreans.com/WinLicense.php> |

Notes:

- The generator DLLs (`CustomWinlicenseSDK.dll`, `WinLicenseSDK.dll`,
  `ECCfunctions.dll`), generator seeds/databases and license files are excluded
  by `.gitignore` and must never be committed.
- "WinLicense" and "Oreans" are names of their respective owner and are used here
  only to describe compatibility.
