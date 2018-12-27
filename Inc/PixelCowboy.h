#ifndef PIXELCOWBOY_H
#define PIXELCOWBOY_H
#include "PreprocessorCore.h"
#include "font.h"

extern const unsigned char PixelCowboy_128x128_14x12[];

static const struct sFont_PixelCowboy : public FontType
{
private:
	static uint8_t const PixelCowboy_CHARACTER_WIDTH[90];

public:
	sFont_PixelCowboy()
	: FontType( /*.FONT_WIDTH*/(128),
							/*.FONT_HEIGHT*/(128),
							/*.FONT_ASCII_START*/(32),
							/*.FONT_ASCII_END*/('y'),
							/*.FONT_GLYPHWIDTH*/(14),
							/*.FONT_GLYPHHEIGHT*/(12),
							/*.FONT_GLYPHSWIDE*/(9),
							/*.FONT_GLYPHSHIGH*/(10),
							/*.FONT_SPACING*/(1),
							/*.CHARACTER_WIDTH*/(PixelCowboy_CHARACTER_WIDTH),
							/*.FontMap*/(PixelCowboy_128x128_14x12) )
	{}
} oFont_PixelCowboy;

#endif