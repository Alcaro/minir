#pragma once
#include "global.h"

struct image {
	unsigned int width;
	unsigned int height;
	void * pixels;
	
	//Small, or even large, amounts of padding between each scanline is fine. However, each scanline is packed.
	//The pitch is in bytes.
	unsigned int pitch;
	
	videoformat format;
};

static inline uint8_t videofmt_byte_per_pixel(videoformat fmt)
{
	static const uint8_t table[]={2, 4, 2, 0, 3, 2, 4};
	return table[fmt];
}

#ifdef __cplusplus
extern "C" {
#endif

//Four-digit values have alpha channels. Values with a slash discard some of the bits.
//c=convert, t=table, C=convert but not resize
//Supported bit depths:
//                      src
//        1/555 8/888 565 888 1555 8888
//  1/555   ct
//  8/888   ct    c    ct
//dst 565              c
//    888   Ct         Ct  C
//   1555
//   8888                           c
//More will be added as needed.
//For image_convert, src and dst must have the same width and height; however, pitch and bpp may vary. Overlap is not allowed.
//Converting a format to itself just uses memcpy on each scanline.
//The conversion tables have the size [fixme]
void image_create_convert_table(videoformat srcfmt, videoformat dstfmt, void* dst);
const void * image_get_convert_table(videoformat srcfmt, videoformat dstfmt);
void image_convert(const struct image * src, struct image * dst);
void image_convert_resize(const struct image * src, struct image * dst);


//Valid formats: All
//pngcomments are { "key", "value", "key", "value", NULL }, or a toplevel NULL
bool png_encode(const struct image * img, const char * * pngcomments,  void* * pngdata, size_t * pnglen);

//Valid formats: 888, (8)888, 8888
//If there is transparency, 8888 is mandatory.
bool png_decode(const void * pngdata, size_t pnglen, struct image * img, videoformat format);

//Calls png_decode or similar.
bool image_decode(const void * imagedata, size_t imglen, struct image * out, videoformat format);

#ifdef __cplusplus
}
#endif
