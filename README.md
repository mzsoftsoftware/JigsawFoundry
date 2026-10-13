# PuzzleSolver 0.1

Qt6/OpenCV prototype for detecting blue jigsaw puzzle pieces on a wooden table.

## Requirements (Ubuntu/Debian)

```bash
sudo apt install cmake g++ qt6-base-dev libopencv-dev
```

## Build

```bash
cmake -S . -B build
cmake --build build -j$(nproc)
./build/PuzzleSolver
```

On EndeavourOS/Arch Linux, install `base-devel cmake qt6-base opencv` using pacman.

## Usage

1. Open a JPG or PNG image.
2. Click **Teile erkennen**.
3. Zoom with the mouse wheel and pan by dragging.

The initial LAB threshold and area filter are calibrated to the sample photograph (1536 x 2048). The detector is not yet universal; some pieces may be missed or background regions may be detected. The current prototype performs no edge matching.
