#ifndef LADYRADICAL_H
#define LADYRADICAL_H
#include "PreprocessorCore.h"
#include "Font.h"

extern const unsigned char LadyRadical64x128_9x14[];			//bugfix -- must be extern array not a pointer

static const struct sFont_LadyRadical : public FontType
{
private:
	static uint8_t const LadyRadical_CHARACTER_WIDTH[63];

public:
	sFont_LadyRadical()
	: FontType( /*.FONT_WIDTH*/(64),
							/*.FONT_HEIGHT*/(128),
							/*.FONT_ASCII_START*/(33),
							/*.FONT_ASCII_END*/('_'),
							/*.FONT_GLYPHWIDTH*/(9),
							/*.FONT_GLYPHHEIGHT*/(14),
							/*.FONT_GLYPHSWIDE*/(7),
							/*.FONT_GLYPHSHIGH*/(9),
							/*.FONT_SPACING*/(1),
							/*.CHARACTER_WIDTH*/(LadyRadical_CHARACTER_WIDTH),
							/*.FontMap*/(LadyRadical64x128_9x14) )
	{}
} oFont_LadyRadical;

#endif