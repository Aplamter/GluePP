# Bundled libraries

Everything GluePP needs is in this folder, so building it downloads nothing.

| Folder | Library | Version | What it does here | License |
| --- | --- | --- | --- | --- |
| `glfw/` | [GLFW](https://www.glfw.org) | 3.4 | Window, OpenGL context, keyboard and mouse | zlib (`glfw/LICENSE.md`) |
| `glad/` | [glad](https://github.com/Dav1dde/glad) | 2.0.8 (generated) | Loads the OpenGL 3.3 core functions | MIT / public domain (generated code) |
| `stb/` | [stb_image, stb_image_write, stb_truetype](https://github.com/nothings/stb) | master | PNG loading, screenshots, UI font | MIT or public domain (`stb/LICENSE`) |
| `miniaudio/` | [miniaudio](https://miniaud.io) | 0.11.22 | Sound playback (WAV and MP3 decoding built in) | MIT-0 or public domain (`miniaudio/LICENSE`) |
| `nlohmann/` | [nlohmann/json](https://github.com/nlohmann/json) | 3.11.3 | Project files, asset manifest, build settings | MIT (`nlohmann/LICENSE.MIT`) |

OpenGL itself isn't a library you install: it comes with your graphics driver (on Windows, `opengl32.dll` is part
of the system). GluePP needs a GPU and driver with OpenGL 3.3, which any GPU from the last ~12 years has.

The glad loader was generated with: `python -m glad --api gl:core=3.3 --extensions "" --out-path glad c`
