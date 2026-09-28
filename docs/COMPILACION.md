# Compilación

## Estructura

| Archivo | Contenido |
|---|---|
| `src/WLQuickGen.cpp` | Detección del SDK, `WLQuickGen.ini`, generación y ventana. |
| `src/theme.h/.cpp` | Paleta oscura, tarjetas, botones con hover, casilla, insignia, línea de estado. Mismo diseño que GET_HWID. |
| `src/i18n.h/.cpp` | Textos en inglés, español y portugués. |
| `WLQuickGen.rc` | Icono, manifiesto (Common Controls 6, DPI) y `VERSIONINFO`. |
| `CMakeLists.txt` | Build con CMake (MSVC o MinGW). |
| `build.bat` | Build directo con `cl.exe` para x86 y x64. |

Los fuentes están en UTF-8: MSVC necesita `/utf-8` (ya incluido en `build.bat` y
`CMakeLists.txt`). El runtime se enlaza de forma estática (`/MT`), así que el exe
no depende del redistribuible de Visual C++.

## Visual Studio con `build.bat`

Requiere el toolset C++ x86/x64 y el Windows SDK (`rc.exe`).

```bat
build.bat
```

Genera `WLQuickGen-x86.exe` y `WLQuickGen-x64.exe` en la raíz. Si Visual Studio no
está en la ruta por defecto, edita `VCVARS` al principio del script.

## CMake

```powershell
cmake -S . -B build-x64 -A x64
cmake --build build-x64 --config Release
cmake -S . -B build-x86 -A Win32
cmake --build build-x86 --config Release
```

Los ejecutables quedan en `build-*/bin/Release/WLQuickGen-<arch>.exe`.

## MinGW (compilación cruzada desde Linux)

Útil para comprobar que compila sin Windows:

```sh
x86_64-w64-mingw32-windres -I. WLQuickGen.rc -O coff -o res64.o
x86_64-w64-mingw32-g++ -std=c++17 -O2 -Wall -Wextra -municode -mwindows \
    -DUNICODE -D_UNICODE src/*.cpp res64.o -o WLQuickGen-x64.exe \
    -lcomctl32 -lgdi32 -lshell32 -static
```

Sustituye `x86_64` por `i686` para la versión x86.

## Integración continua y releases

`.github/workflows/release.yml`:

1. Compila x86 y x64 con Visual Studio 2022 en `windows-2022`.
2. Comprueba que el campo `Machine` del PE corresponde a cada arquitectura.
3. Empaqueta `WLQuickGen-<versión>-windows-<arch>.zip` (exe, README y
   `automate.py`) con su `.sha256`.
4. Si la ejecución viene de un tag `v*` (o de un lanzamiento manual con
   *release* marcado), publica la release con la sección correspondiente de
   `CHANGELOG.md` como notas.

Para publicar una versión nueva:

1. Sube la versión en `CMakeLists.txt`, `src/theme.h` (`kVersionLabel`) y
   `WLQuickGen.rc` (`FILEVERSION`, `PRODUCTVERSION` y cadenas).
2. Añade `## [x.y.z] — fecha` en `CHANGELOG.md`.
3. Crea y sube el tag: `git tag vX.Y.Z && git push origin vX.Y.Z`.
