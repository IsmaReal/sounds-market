# Flow — Rhythmic Shaping Tool

Plugin de efecto (VST3 / AU / Standalone) inspirado en **Gross Beat**, armado como un
**rack de módulos**. Cada módulo tiene su propia envolvente que dibujás con nodos sobre
una grilla de beats sincronizada al tempo del DAW.

Está hecho en C++ con [JUCE 8](https://juce.com) y CMake.

## Módulos

| Módulo | Qué dibuja la envolvente | Controles |
|--------|--------------------------|-----------|
| **Volume** (viene por defecto) | La ganancia de la señal en cada momento del patrón. | Mix |
| **Reverb** | Cuánta reverb se escucha en cada momento del patrón. | Mix, Size, Decay, Tone |

- Los módulos se procesan en serie, **de arriba hacia abajo**.
- **+ ADD MODULE** agrega un módulo (hasta 8).
- La **×** de cada tarjeta lo quita. Click derecho en la tarjeta: subir, bajar o quitar.
- Arrastrando los **puntitos** de la izquierda de la tarjeta se reordena el rack.
- El botón de **encendido** apaga el módulo sin perder su configuración.
- Al hacer click en una tarjeta, la grilla y el panel **Module Controls** muestran ese módulo.

## La grilla

- **Click** en un espacio vacío: agrega un nodo (y lo podés arrastrar sin soltar).
- **Arrastrar** un nodo: lo mueve. Con **Shift** ignora el snap.
- **Doble click** o **click derecho** sobre un nodo: lo borra.
- **Punto chico** en el medio de cada segmento: arrastralo para curvar el segmento;
  doble click (o click derecho) lo vuelve recto.
- Dos nodos en la misma posición generan un corte vertical (gate duro).

Arriba de la grilla:

- **Grid**: largo del patrón (1, 2, 4 u 8 beats; por defecto 4).
- **Snap**: 1/4, 1/8, 1/16, 1/32 y tresillos.
- **Curve**: suavizado de los saltos de la envolvente (*Hard* 0,5 ms, *Smooth* 3 ms, *Soft*
  15 ms). Solo se nota en los cortes verticales (gates): evita clicks.

## Presets

Hay dos niveles:

**Presets generales** (arriba, en el encabezado): guardan el rack completo (módulos, orden,
envolventes, parámetros y ajustes de la grilla).

- `<` / `>` pasan al anterior / siguiente; el desplegable muestra *Factory* y *User*.
- El ícono de **guardar** pide un nombre y lo guarda en `Documentos/Flow/Presets`.
- El **menú** (☰) permite exportar / importar archivos `.flowpreset`, abrir la carpeta de
  presets, limpiar el patrón del módulo seleccionado o volver al rack inicial.
- Los de fábrica se agregan en `Source/RackPresets.h` (por ahora solo *Init*).

**Presets de módulo** (en el panel *Module Controls*, al lado del nombre del módulo): cambian
la envolvente del módulo seleccionado y, si el preset lo define, sus parámetros.

- **Básicos**, iguales para todos los módulos:
  - *Init* (plano).
  - *Gate*: 1/1, 1/2, 1/4, 1/8, 1/16, 1/32, tresillos (1/2T, 1/4T, 1/8T, 1/16T) y con
    puntillo (1/4D, 1/8D).
  - *Sidechain*: 1/1, 1/2, 1/4, 1/8, 1/16, 1/4T, 1/8T, 1/4D, 1/8D.
  - *Ramp Up* y *Ramp Down*: 1/1, 1/2, 1/4, 1/8, 1/16.
- **Creativos**: Swells, Trance Gate, fades, etc. Los propios de cada módulo se agregan en
  `presets::forType()` dentro de `Source/Presets.h`.

Los básicos siguen la figura musical: si cambiás el **Grid**, se regeneran para que "Gate 1/8"
siga siendo de corcheas.

Todo se guarda dentro del proyecto del DAW.

## Dry/Wet y pausa

- **DRY/WET** (abajo del rack) mezcla la señal original con la salida de todo el rack. Se puede
  automatizar desde el DAW.
- Cuando el DAW está en **pausa** (o el plugin está puenteado), los efectos dejan de actuar: se
  apagan con un fundido de ~10 ms y se borran sus colas, así no queda sonando la reverb. Al
  volver a play arrancan limpios. En la versión Standalone (sin transporte) el patrón corre
  solo al tempo interno.

## Estructura

```
Source/
  Envelope.h          Modelo de la envolvente (sin JUCE): nodos, curvas, evaluación.
  EnvelopeState.h     Envolvente compartida UI ↔ audio (tabla sin bloqueos).
  ModuleType.h        Tipos de módulo.
  Presets.h           Presets de módulo (básicos y creativos).
  RackPresets.h       Presets generales y carpeta de presets del usuario.
  PresetSelector.*    Selector [<] [preset ▾] [>].
  Modules.*           Módulos del rack: base, Volume y Reverb.
  PluginProcessor.*   Rack, sincronía con el host, guardado del estado.
  FlowLookAndFeel.*   Colores, sliders, combos, íconos y logo.
  RackPanel.*         Tarjetas de los módulos, botón "Add module" y panel para elegir módulo.
  EnvelopeEditor.*    Grilla con los nodos.
  ModuleControls.*    Panel "Module Controls".
  PluginEditor.*      Ventana principal.
Tests/
  EnvelopeTests.cpp   Tests del modelo de la envolvente.
  ProcessorTests.cpp  Simula un host a 120 BPM: rack, volumen, reverb y estado.
```

### Cómo funciona por dentro

- El hilo de audio nunca toca los nodos: cada vez que editás una envolvente se renderiza
  una tabla de 4096 puntos que el módulo copia (con un *try-lock*) al empezar el bloque.
- El rack se le entrega al hilo de audio como una lista nueva; la anterior se libera en el
  hilo de la UI, así agregar o quitar módulos no genera cortes ni reserva memoria en el audio.

### Agregar un módulo nuevo

1. Crear una clase que herede de `flow::Module` en `Modules.h/.cpp` e implementar
   `processEffect` (recibe el valor de la envolvente de cada muestra).
2. Declarar sus parámetros en `getParameterSpecs()`; el panel Module Controls los muestra solo.
3. Sumarlo a `ModuleType` (en `ModuleType.h`), `createModule`, `moduleTypeToId`,
   `getAvailableModuleTypes`, `getModuleTypeName` y `getModuleTypeDescription`.
4. Dibujar su ícono en `drawModuleIcon` (`FlowLookAndFeel.cpp`).

## Compilar

Necesitás CMake 3.22+ y un compilador C++17:

- **Windows**: Visual Studio 2022 (con "Desarrollo para el escritorio con C++").
- **macOS**: Xcode.
- **Linux**: `g++`/`clang` y las dependencias de JUCE
  (`libasound2-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libfreetype-dev libfontconfig1-dev`).

```bash
cd plugins/flow
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

JUCE se descarga solo la primera vez. Si ya lo tenés clonado podés usar
`-DJUCE_DIR=/ruta/a/JUCE`.

Al terminar de compilar, el plugin **se instala solo** en la carpeta de plugins del sistema:

- Windows: `C:\Program Files\Common Files\VST3\Flow.vst3`
- macOS: `~/Library/Audio/Plug-Ins/VST3/` (y el AU en `~/Library/Audio/Plug-Ins/Components/`)
- Linux: `~/.vst3/`

En Windows esa carpeta necesita permisos de administrador. Para no tener que abrir la
terminal como administrador cada vez, dale permiso de escritura a tu usuario **una sola vez**
desde una PowerShell abierta como administrador:

```powershell
icacls "C:\Program Files\Common Files\VST3" /grant "${env:USERNAME}:(OI)(CI)M"
```

Si no querés que se copie, configurá con `-DFLOW_COPY_AFTER_BUILD=OFF`; el VST3 queda en
`build/Flow_artefacts/<Debug|Release>/VST3/Flow.vst3`.

Antes de compilar, cerrá el plugin en el DAW (o el proyecto): si está cargado, Windows bloquea
el archivo y la copia falla.

En FL Studio: *Options → Manage plugins → Find plugins*, y después cargalo como efecto en
un canal del mixer.

### Tests

```bash
cd build && ctest -C Release --output-on-failure
```

### Builds automáticos

El workflow `.github/workflows/flow.yml` compila el plugin en Windows, macOS y Linux en
cada push que toque `plugins/flow/`, corre los tests y deja el VST3 (y el AU en macOS)
como artefacto descargable en la pestaña *Actions* de GitHub.

## Ideas para seguir

- Módulos **Pan** y **Delay** (ya contemplados en el diseño).
- Envolvente de **tiempo** (stutter / reverse / slow-down) como en Gross Beat.
- Parámetros automatizables desde el DAW.
- Deshacer / rehacer y copiar/pegar patrones entre módulos.
