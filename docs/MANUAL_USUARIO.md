# Manual de usuario

![WLQuickGen en español](screenshot-es.png)

## Preparación (una vez por producto)

1. Descarga de [Releases](https://github.com/FerS00/WLQuickGen/releases) el zip de la
   arquitectura de la DLL del generador (normalmente `x86`).
2. Copia `WLQuickGen-x86.exe` (o `-x64`) dentro de `Specific Generators\<Producto>\`.
   También sirve dejarlo en `Specific Generators\` si solo hay un producto debajo.
3. Ejecútalo una vez: crea `WLQuickGen.ini` a su lado.
4. Abre el ini y pon en `LicenseFileName` el nombre que espera tu producto
   (por ejemplo `licence.dat`). Si usas el SDK estándar (`WinLicenseSDK.dll`),
   rellena también `LicenseHash`.

La insignia de la esquina superior indica lo que se detectó:

| Insignia | Significado |
|---|---|
| Verde · SDK personalizado | `CustomWinlicenseSDK.dll` encontrado. No necesita `LicenseHash`. |
| Azul · SDK estándar | `WinLicenseSDK.dll` encontrado. Necesita `LicenseHash`. |
| Ámbar · Arquitectura distinta | La DLL es de 32 bits y el exe de 64 (o al revés). Usa la otra versión. |
| Roja · SDK no encontrado | El exe no está dentro de la carpeta del generador. |

Debajo de las tarjetas se muestran la ruta de salida y la DLL usada.

## Generar una licencia

1. Pega el HWID del equipo destino en la tarjeta **HWID**. A la derecha se ve el
   número de caracteres, útil para detectar un pegado incompleto.
2. En **CADUCIDAD**:
   - deja marcada **Sin caducidad** para una licencia permanente, o
   - desmárcala y escribe día / mes / año. El resumen de la derecha confirma
     la fecha (`Caduca en 459 días`) o avisa si es inválida o ya pasó.
     La licencia vence a las 23:59:59 de ese día.
3. Pulsa **Generar** (o Enter).
4. La línea de estado se pone en verde con el nombre y tamaño del archivo.
   **Ver archivo** abre el Explorador con la licencia seleccionada.

Si ya existía una licencia con ese nombre, se conserva como `<nombre>.previous`.

## Idioma

La interfaz sigue el idioma de Windows (español, inglés o portugués; inglés
para el resto). El botón `EN` / `ES` / `PT` junto a la insignia cambia el idioma
al instante y lo guarda en `WLQuickGen.ini` (`Language=`). Para volver al modo
automático escribe `Language=auto`.

## Mensajes de error frecuentes

| Mensaje | Qué hacer |
|---|---|
| No se encontró la DLL del generador | Mueve el exe a `Specific Generators\<Producto>\`. |
| La DLL del generador tiene otra arquitectura | Usa la versión indicada (`x86` o `x64`). |
| El SDK estándar necesita LicenseHash | Copia el hash del proyecto WinLicense al ini. |
| LicenseFileName debe ser un nombre de archivo simple | Sin carpetas ni `..`; solo `nombre.ext`. |
| Esa fecha no existe en el calendario | Por ejemplo 31/02. Corrige el día. |
| El SDK no generó la licencia (devolvió N) | HWID con formato no aceptado por el producto o DLL de otro producto. |

## Automatización

Consulta la sección *Automation with pywinauto* del [README](../README.md) y
[`examples/automate.py`](../examples/automate.py). El texto del control de estado
empieza siempre con `OK:` o `ERROR:` sin importar el idioma.
