# Grafo lógico de WLQuickGen

Mapa corto de la implementación (`src/WLQuickGen.cpp`, `src/theme.cpp`, `src/i18n.cpp`).

## Flujo principal

```mermaid
flowchart TD
    EXE[WLQuickGen-x86/x64.exe] --> DETECT[DetectSdk]
    DETECT --> CUSTOM{CustomWinlicenseSDK.dll?}
    CUSTOM -->|sí| MODEC[Modo personalizado<br/>sin LicenseHash]
    CUSTOM -->|no| STD{WinLicenseSDK.dll?}
    STD -->|sí| MODES[Modo estándar<br/>requiere LicenseHash]
    STD -->|no| NOSDK[Insignia roja<br/>SDK no encontrado]
    MODEC --> PE[ReadPeMachine]
    MODES --> PE
    PE -->|0x14c / 0x8664 distinto al exe| MISMATCH[Insignia ámbar<br/>arquitectura distinta]
    PE -->|coincide| DATA[Buscar GeneratorSeed.gns /<br/>GeneratorDatabase.abs]
    MISMATCH --> DATA
    NOSDK --> INI
    DATA --> INI[LoadSettings<br/>WLQuickGen.ini]
    INI --> LANG[ResolveLanguage]
    LANG --> UI[CreateControls + ApplyLanguage]

    UI -->|Generar / Enter| GEN[Generate]
    GEN --> V1{DLL y arquitectura OK?}
    V1 -->|no| ERR[ERROR: ...]
    V1 --> V2{LicenseFileName simple?}
    V2 -->|no| ERR
    V2 --> V3{HWID no vacío?}
    V3 -->|no| ERR
    V3 --> V4{Modo estándar sin hash?}
    V4 -->|sí| ERR
    V4 -->|no| V5{Sin caducidad?}
    V5 -->|no| DATE[BuildExpiration<br/>23:59:59 del día]
    DATE -->|inválida| ERR
    DATE --> LOAD
    V5 -->|sí| LOAD[SetCurrentDirectory +<br/>SetDllDirectory + LoadLibrary]
    LOAD -->|falla| ERR
    LOAD --> CALL[WLCustomGenLicenseFileKeyEx /<br/>WLGenLicenseFileKeyEx ANSI]
    CALL -->|tamaño ≤ 0| ERR
    CALL --> BACKUP[Copia previa → .previous]
    BACKUP --> TMP[Escribir .tmp]
    TMP --> MOVE[MoveFileEx REPLACE_EXISTING<br/>WRITE_THROUGH]
    MOVE --> OK[OK: archivo escrito<br/>habilita Ver archivo]
```

## Orden de búsqueda del SDK

`FindRelative` prueba cada raíz en este orden y devuelve la primera coincidencia.
`CustomWinlicenseSDK.dll` se busca en todas las raíces antes que `WinLicenseSDK.dll`.

```mermaid
flowchart TD
    B1["1. carpeta del exe"] --> B2["2. exe/DLL"] --> B3["3. exe/EXE"]
    B3 --> P1["4. carpeta padre"] --> P2["5. padre/DLL"] --> P3["6. padre/EXE"]
    P3 --> S1["7. cada subcarpeta"] --> S2["8. subcarpeta/DLL"] --> S3["9. subcarpeta/EXE"]
```

## Línea de estado

El texto del control `1012` es el contrato para la automatización. Los prefijos
`OK:` y `ERROR:` nunca se traducen; en pantalla se sustituyen por un punto de color.

```mermaid
stateDiagram-v2
    [*] --> Info: SDK correcto
    [*] --> Aviso: sin SDK / arquitectura distinta
    Info --> Ocupado: Generar
    Aviso --> Ocupado: Generar
    Ocupado --> OK: licencia escrita
    Ocupado --> Error: validación o SDK
    OK --> Ocupado: Generar
    Error --> Ocupado: Generar

    note right of Info: gris, sin prefijo
    note right of Aviso: ámbar, sin prefijo
    note right of Ocupado: azul, sin prefijo
    note right of OK: verde, prefijo OK
    note right of Error: rojo, prefijo ERROR
```

## Idioma

```mermaid
flowchart LR
    INI[Language= en/es/pt/auto] -->|en, es, pt| FIXED[Idioma fijo]
    INI -->|auto u otro valor| SYS[GetUserDefaultUILanguage]
    SYS -->|LANG_SPANISH| ES[Español]
    SYS -->|LANG_PORTUGUESE| PT[Portugués]
    SYS -->|otro| EN[Inglés]
    BTN[Botón EN/ES/PT] -->|EN → ES → PT → EN| APPLY[ApplyLanguage:<br/>textos, estado, repintado]
    BTN -->|WritePrivateProfileString| INI
```

El estado se guarda como clave (`Text`) más argumentos, no como cadena ya
traducida; por eso al cambiar de idioma el mensaje actual se vuelve a mostrar en
el idioma nuevo sin perder su resultado.

## Módulos

```mermaid
flowchart TD
    MAIN[src/WLQuickGen.cpp<br/>detección, ini, generación, ventana] --> I18N[src/i18n.cpp<br/>tabla EN/ES/PT]
    MAIN --> THEME[src/theme.cpp<br/>paleta, tarjetas, botones,<br/>casilla, insignia, estado]
    RC[WLQuickGen.rc] --> ICON[WLQuickGen.ico]
    RC --> MANIFEST[WLQuickGen.manifest<br/>Common Controls 6, DPI]
    RC --> VERSION[VERSIONINFO]
```
