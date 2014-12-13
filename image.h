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

#ifdef __cplusplus
extern "C" {
#endif

	//fmt_0rgb1555,
	//fmt_xrgb8888,
	//fmt_rgb565,
	//
	//fmt_none,//this should be 0, but libretro says I can't do that
	//
	//fmt_rgb888,
	//fmt_argb1555,
	//fmt_argb8888,

//Supported bit depths:
//c=convert, t=table, C=convert but not resize
//                          src
//        1/555 8/888 565 - 888 1555 8888
//  1/555
//  8/888
//    565
//dst   -
//    888
//   1555
//   8888
//    
//    
//       15 16 24 32 33
//    15 c
//    16    c
//    24 Ct Ct C
//    32 ct ct    c
//    33             c
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
bool png_encode(const struct image * img, const char * * pngcomments,  void* * pngdata, unsigned int * pnglen);

//Valid formats: 888, (8)888, 8888
//If there is transparency, 8888 is mandatory.
bool png_decode(const void* pngdata, unsigned int pnglen, struct image * img, videoformat format);

#ifdef __cplusplus
}
#endif
