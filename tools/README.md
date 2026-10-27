# Scratch → C++ converter

These tools produced `Generated/` and `assets/` from `Glue3D.sb3`. They work on other Scratch projects too.

```bash
# 1. unzip the project
mkdir sb3 && cd sb3 && unzip ../MyProject.sb3 && cd ..

# 2. costumes/sounds -> assets/ (renders SVGs in Chromium with Scratch's fonts)
cd tools && npm install && npx playwright install chromium && cd ..
node tools/rasterize.js sb3 assets

# 3. blocks -> C++
python3 tools/sb3_to_cpp.py sb3 Generated assets --namespace Glue3D --title "My Project" --rename "OldSpriteName=NiceName"
```

Then copy `scratch-render-fonts/src/NotoSans-Medium.ttf` (from `tools/node_modules`) to `assets/fonts/`, and build
with CMake (see the main README).

- `--rename` gives a sprite a friendlier class name. Scratch sprite names can be any characters,
  e.g. Glue3D's sprite is just a backtick.
- `--members Class=X.members.h` puts hand-written members into a generated class (like a C# partial class):
  the file is `#include`d inside the class body, and `X.includes.h` at the top of the header.
- The generator prints any blocks it doesn't support. They're left in the code as comments.
- Needs Python 3.8+ (and Node.js 18+ for the asset step).

## Glue3D's own edits

GluePP changes a few places in the generated Glue3D code (start menu, File > build.., Unity-style camera,
"export" removed, editor-only parts). After re-running `sb3_to_cpp.py` for Glue3D, re-apply them with:

```bash
python3 tools/sb3_to_cpp.py sb3 Generated assets --namespace Glue3D --title Glue3D \
    --rename "`=Glue3D" --members "Glue3DSprite=../Custom/Glue3DSprite.members.h"
python3 tools/glue3d_edits.py Generated
```

`glue3d_edits.py` also finds the generated methods that a built game can never reach and wraps them in
`#ifndef GLUE3D_PLAYER`, so the game player is compiled without the editor.
