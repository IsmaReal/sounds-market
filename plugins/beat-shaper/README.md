# Beat Shaper

Plugin de efecto (VST3 / AU / Standalone) inspirado en el modo volumen de **Gross Beat**:
una grilla de **4 beats** donde dibujás la envolvente de volumen con nodos, sincronizada
al tempo del DAW.

Está hecho en C++ con [JUCE 8](https://juce.com) y CMake.

## Qué hace

- Grilla de 4 beats (un compás de 4/4) que se repite en loop siguiendo la posición del host.
- Envolvente de volumen editable con nodos:
  - **Click** en un espacio vacío: agrega un nodo (y lo podés arrastrar sin soltar).
  - **Arrastrar** un nodo: lo mueve. Con **Shift** ignora el snap.
  - **Doble click** o **click derecho** sobre un nodo: lo borra.
  - **Punto azul** en el medio de cada segmento: arrastralo para curvar el segmento;
    doble click (o click derecho) lo vuelve recto.
  - Dos nodos en la misma posición generan un corte vertical (gate duro).
- **Snap** a negras, corcheas, semicorcheas, fusas y tresillos.
- **Presets** de fábrica: sidechain, gates 1/4 – 1/8 – 1/16, trance gate, fades, etc.
- **Mix**: mezcla entre la señal original y la procesada.
- **Smooth**: suavizado del volumen (en ms) para evitar clicks en los cortes.
- Cuando el DAW está detenido el patrón sigue corriendo con un reloj interno al tempo
  del proyecto, así podés escucharlo en vivo con una entrada de audio.
- El patrón y el snap se guardan dentro del proyecto del DAW.

## Estructura

```
Source/
  Envelope.h          Modelo de la envolvente (sin JUCE): nodos, curvas, evaluación.
  Presets.h           Patrones de fábrica.
  PluginProcessor.*   Audio: sincronía con el host, tabla de ganancia, suavizado, estado.
  EnvelopeEditor.*    Componente de la grilla con los nodos.
  PluginEditor.*      Ventana del plugin (presets, snap, perillas).
Tests/
  EnvelopeTests.cpp   Tests del modelo de la envolvente.
  ProcessorTests.cpp  Simula un host a 120 BPM y verifica el volumen aplicado.
```

El hilo de audio nunca toca los nodos directamente: cada vez que editás la envolvente se
renderiza una tabla de 4096 puntos que el procesador copia (sin bloquearse) al comienzo
del siguiente bloque.

## Compilar

Necesitás CMake 3.22+ y un compilador C++17:

- **Windows**: Visual Studio 2022 (con "Desarrollo para el escritorio con C++").
- **macOS**: Xcode.
- **Linux**: `g++`/`clang` y las dependencias de JUCE
  (`libasound2-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libfreetype-dev libfontconfig1-dev`).

```bash
cd plugins/beat-shaper
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

JUCE se descarga solo la primera vez. Si ya lo tenés clonado podés usar
`-DJUCE_DIR=/ruta/a/JUCE`.

El VST3 queda en `build/BeatShaper_artefacts/Release/VST3/Beat Shaper.vst3`. Copialo a:

- Windows: `C:\Program Files\Common Files\VST3\`
- macOS: `~/Library/Audio/Plug-Ins/VST3/` (y el `.component` en `~/Library/Audio/Plug-Ins/Components/`)
- Linux: `~/.vst3/`

En FL Studio: *Options → Manage plugins → Find plugins*, y después cargalo como efecto en
un canal del mixer.

También se genera una versión **Standalone** para probarlo sin DAW.

### Tests

```bash
cd build && ctest -C Release --output-on-failure
```

### Builds automáticos

El workflow `.github/workflows/beat-shaper.yml` compila el plugin en Windows, macOS y
Linux en cada push que toque `plugins/beat-shaper/`, corre los tests y deja el VST3 (y el
AU en macOS) como artefacto descargable en la pestaña *Actions* de GitHub.

## Ideas para seguir

- Más páginas de patrón (Gross Beat tiene 36) y cambio de patrón por MIDI.
- Envolvente de **tiempo** (stutter / reverse / slow-down) además de la de volumen.
- Largo de patrón configurable (1, 2, 8 beats).
- Deshacer / rehacer y copiar/pegar patrones.
- Dibujo libre con el mouse (modo lápiz).
