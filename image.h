struct image {
	unsigned int width;
	unsigned int height;
	void * pixels;
	
	//Small, or even large, amounts of padding between each scanline is fine. However, each scanline is packed.
	//The pitch is in bytes.
	unsigned int pitch;
	
	//Valid values:
	//33 (ARGB8888), 32 (XRGB8888), 24 (RGB888), 16 (RGB565), 15 (RGB555)
	//Supported values for encoding: 32 (XRGB8888), 24 (RGB888), 16 (RGB565), 15 (RGB555)
	//Decoding is supported to 33 (ARGB8888), as well as 32 and 24. If there is transparency information and bpp!=33, it fails.
	//24bpp is three uint8_t per pixel, order RGB; others are one uint16_t or uint32_t per pixel, host native endian.
	unsigned int bpp;
};

#ifdef __cplusplus
extern "C" {
#endif

//Supported bit depths:
//c=convert, t=table, C=convert but not resize
//            src
//       15 16 24 32 33
//    15 c
//    16    c
//dst 24 Ct Ct C
//    32 ct ct    c
//    33             c
//More will be added as needed.
//For image_convert, src and dst must have the same width and height; however, pitch and bpp may vary. Overlap is not allowed.
//Converting a format to itself just uses memcpy on each scanline.
//The conversion tables have the size [fixme]
void image_create_convert_table(unsigned int srcbpp, unsigned int dstbpp, void* dst);
const void * image_get_convert_table(unsigned int srcbpp, unsigned int dstbpp);
void image_convert(const struct image * src, struct image * dst);
void image_convert_resize(const struct image * src, struct image * dst);


//Valid bpp values: 32, 24, 16, 15
//pngcomments are { "key", "value", "key", "value", NULL }, or a toplevel NULL
bool png_encode(const struct image * img, const char * * pngcomments,  void* * pngdata, unsigned int * pnglen);

//Valid bpp values: 24, 32, 33
//If there is transparency, 33 is mandatory.
bool png_decode(const void* pngdata, unsigned int pnglen, struct image * img, unsigned int bpp);

#ifdef __cplusplus
}
#endif
