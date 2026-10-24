#!/usr/bin/env bash
# Erzeugt alle Assets der FPV-Drohne neu. Voraussetzung: python3 mit numpy, pillow und bpy
# (pip install bpy numpy pillow) oder ein installiertes Blender 4.2+.
set -euo pipefail
cd "$(dirname "$0")/.."
python3 Assets/Source/Scripts/gen_textures.py
if python3 -c "import bpy" 2>/dev/null; then
  python3 Assets/Source/Blender/build_fpv_assets.py "$@"
else
  blender -b -P Assets/Source/Blender/build_fpv_assets.py -- "$@"
fi
python3 Assets/Source/Scripts/gen_ui.py
python3 Assets/Source/Scripts/gen_audio.py
