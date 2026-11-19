// Single stb_image implementation for the whole project (zigcore is linked by game, launcher, zigimport).
// PNG + JPEG; also provides the zlib decoder used by core/Zip.cpp.
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG  // iTunesArtwork (Steam icon, L6)
#include "stb_image.h"
