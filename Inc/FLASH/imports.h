#ifndef IMPORTS_FLASH
#define IMPORTS_FLASH

// Ground AO Textures/Masks
static constexpr uint32_t const top_ao_stride = 23;
static constexpr uint32_t const top_ao_numoflines = 12;
extern const unsigned char _flash_top_ao_bothsides[]; extern unsigned int const top_ao_bothsides_size;
extern const unsigned char _flash_top_ao_oneside[]; extern unsigned int const top_ao_oneside_size;

static constexpr uint32_t const side_ao_stride = 12;
static constexpr uint32_t const side_ao_numoflines = 33;
extern const unsigned char _flash_side_7_both_ao[]; extern unsigned int const side_7_both_ao_size;
extern const unsigned char _flash_side_7_botom_ao[]; extern unsigned int const side_7_botom_ao_size;


// VOX (MagickaVoxel) Models
extern const unsigned char _vox_sr71[];
extern const unsigned char _vox_f2a[];
extern const unsigned char _vox_sidewinder[];



#endif /*IMPORTS_FLASH*/
