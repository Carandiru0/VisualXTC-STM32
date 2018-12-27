/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

#ifndef FONT_H
#define FONT_H

typedef struct sFontType
{
	uint32_t const FONT_WIDTH,
								 FONT_HEIGHT,
								 FONT_ASCII_START,
								 FONT_ASCII_END,
								 FONT_GLYPHWIDTH,
								 FONT_GLYPHHEIGHT,
								 FONT_GLYPHSWIDE,
								 FONT_GLYPHSHIGH,
								 FONT_SPACING,
								 FONT_SPACEWIDTH;

	uint8_t const* const CHARACTER_WIDTH;
	
	uint8_t const* const FontMap;
	
	explicit sFontType( /*.FONT_WIDTH*/uint32_t const WIDTH,
										  /*.FONT_HEIGHT*/uint32_t const HEIGHT,
										  /*.FONT_ASCII_START*/uint32_t const ASCII_START,
										  /*.FONT_ASCII_END*/uint32_t const ASCII_END,
										  /*.FONT_GLYPHWIDTH*/uint32_t const GLYPHWIDTH,
										  /*.FONT_GLYPHHEIGHT*/uint32_t const GLYPHHEIGHT,
										  /*.FONT_GLYPHSWIDE*/uint32_t const GLYPHSWIDE,
										  /*.FONT_GLYPHSHIGH*/uint32_t const GLYPHSHIGH,
										  /*.FONT_SPACING*/uint32_t const SPACING,
										  /*.CHARACTER_WIDTH*/uint8_t const* const CHARWIDTHS,
										  /*.FontMap*/uint8_t const* const ExternFontMap )
	: FONT_WIDTH(WIDTH), FONT_HEIGHT(HEIGHT), FONT_ASCII_START(ASCII_START), FONT_ASCII_END(ASCII_END),
		FONT_GLYPHWIDTH(GLYPHWIDTH), FONT_GLYPHHEIGHT(GLYPHHEIGHT), FONT_GLYPHSWIDE(GLYPHSWIDE), FONT_GLYPHSHIGH(GLYPHSHIGH),
		FONT_SPACING(SPACING), FONT_SPACEWIDTH(FONT_GLYPHWIDTH >> 1),
		CHARACTER_WIDTH(CHARWIDTHS), FontMap(ExternFontMap)
	{}
} FontType;

#endif