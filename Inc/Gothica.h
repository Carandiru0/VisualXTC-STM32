#ifndef GOTHICA_H
#define GOTHICA_H
#include "PreprocessorCore.h"
#include "Font.h"

extern const unsigned char Gothica64x128_8x18[];			//bugfix -- must be extern array not a pointer

static const struct sFont_Gothica : public FontType
{
private:
	static uint8_t const Gothica_CHARACTER_WIDTH[59];

public:
	sFont_Gothica()
	: FontType( /*.FONT_WIDTH*/(64),
							/*.FONT_HEIGHT*/(128),
							/*.FONT_ASCII_START*/(33),
							/*.FONT_ASCII_END*/('X'),
							/*.FONT_GLYPHWIDTH*/(8),
							/*.FONT_GLYPHHEIGHT*/(18),
							/*.FONT_GLYPHSWIDE*/(8),
							/*.FONT_GLYPHSHIGH*/(7),
							/*.FONT_SPACING*/(1),
							/*.CHARACTER_WIDTH*/(Gothica_CHARACTER_WIDTH),
							/*.FontMap*/(Gothica64x128_8x18) )
	{}
} oFont_Gothica;

#endif