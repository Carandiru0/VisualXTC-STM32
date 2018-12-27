#ifndef SDF_13_HEADER_JPEG_H 
#define SDF_13_HEADER_JPEG_H 


namespace SDF_13_Header  
{

static constexpr uint16_t const ORIGWIDTH =		900;
static constexpr uint16_t const ORIGHEIGHT =	1350;

static constexpr uint8_t const  NUMSHADES =		32;

static constexpr uint32_t const TOTALSIZE =		238019;

static constexpr uint32_t const SIZES[NUMSHADES] = { 	
								1010, 1723, 3654, 5833, 8826, 10238, 11316, 12066,
								12321, 12421, 12257, 12021, 11494, 11194, 10688, 10388,
								10085, 9376, 8649, 7851, 7162, 6528, 6044, 5389,
								4919, 4824, 4579, 4303, 3605, 3172, 2398, 1685,
								};

static constexpr uint8_t const  SHADES[NUMSHADES] = { 	
								0, 10, 20, 28, 37, 42, 48, 60,
								66, 73, 80, 87, 96, 104, 112, 121,
								128, 137, 145, 153, 161, 169, 179, 186,
								194, 202, 210, 219, 232, 235, 243, 255,
								};

}  // end namespace 

#endif 

