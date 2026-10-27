// The single-header libraries' implementations live in this one file.
#define STB_IMAGE_IMPLEMENTATION
#define STBI_WINDOWS_UTF8
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBIW_WINDOWS_UTF8
#include "stb_image_write.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

// Playback only; decoders for WAV, MP3 and FLAC are built in.
#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_ENCODING
#include "miniaudio.h"
