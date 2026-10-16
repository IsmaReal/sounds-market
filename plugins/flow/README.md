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
- **Curve**: qué tan rápido sigue el sonido a la envolvente (*Hard* para cortes secos,
  *Smooth* por defecto, *Soft* para transiciones suaves sin clicks).

## Presets

- El selector **Preset** aplica patrones de fábrica (Sidechain Pulse, Swells, Gates,
  Trance Gate, fades, etc.) a la envolvente del **módulo seleccionado**. La flecha `<`
  pasa al patrón anterior.
- El ícono de **guardar** exporta el rack completo a un archivo `.flowpreset`.
- El **menú** (☰) permite cargar un `.flowpreset`, limpiar el patrón del módulo
  seleccionado o resetear el rack.
- Todo el rack (módulos, orden, envolventes y ajustes) se guarda dentro del proyecto del DAW.

Cuando el DAW está detenido el patrón sigue corriendo con un reloj interno al tempo del
proyecto, así podés escucharlo en vivo con una entrada de audio.

## Estructura

```
Source/
  Envelope.h          Modelo de la envolvente (sin JUCE): nodos, curvas, evaluación.
  EnvelopeState.h     Envolvente compartida UI ↔ audio (tabla sin bloqueos).
  Presets.h           Patrones de fábrica.
  Modules.*           Módulos del rack: base, Volume y Reverb.
  PluginProcessor.*   Rack, sincronía con el host, guardado del estado.
  FlowLookAndFeel.*   Colores, sliders, combos, íconos y logo.
  RackPanel.*         Tarjetas de los módulos y botón "Add module".
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
3. Sumarlo a `ModuleType`, `createModule`, `moduleTypeToId`, `getAvailableModuleTypes` y
   `getModuleTypeName`.

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
