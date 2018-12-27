#ifndef FRAM_SDF_MemoryLayout_h
#define FRAM_SDF_MemoryLayout_h

#include "globals.h"
#include "quadspi.h"

static constexpr uint32_t const BlueNoise_x256_MemoryStart = 0;

extern unsigned int const BlueNoise_x256_size;
uint8_t const* const BlueNoise_x256 = (uint8_t const* const)(QUADSPI_ADDRESS + BlueNoise_x256_MemoryStart);

// ######### //

/*
static constexpr uint32_t const SDF_4_MemoryStart = (SDF_2_Header::TOTALSIZE - SDF_2_Header::SIZES[0]);

//static constexpr uint32_t const SDF_Succubus_4_0_Size = SDF_4_Header::SIZES[0];
static constexpr uint32_t const SDF_Succubus_4_1_Size = SDF_4_Header::SIZES[1];
static constexpr uint32_t const SDF_Succubus_4_2_Size = SDF_4_Header::SIZES[2];
static constexpr uint32_t const SDF_Succubus_4_3_Size = SDF_4_Header::SIZES[3];
static constexpr uint32_t const SDF_Succubus_4_4_Size = SDF_4_Header::SIZES[4];
static constexpr uint32_t const SDF_Succubus_4_5_Size = SDF_4_Header::SIZES[5];
static constexpr uint32_t const SDF_Succubus_4_6_Size = SDF_4_Header::SIZES[6];
static constexpr uint32_t const SDF_Succubus_4_7_Size = SDF_4_Header::SIZES[7];
static constexpr uint32_t const SDF_Succubus_4_8_Size = SDF_4_Header::SIZES[8];
static constexpr uint32_t const SDF_Succubus_4_9_Size = SDF_4_Header::SIZES[9];
static constexpr uint32_t const SDF_Succubus_4_10_Size = SDF_4_Header::SIZES[10];
static constexpr uint32_t const SDF_Succubus_4_11_Size = SDF_4_Header::SIZES[11];
static constexpr uint32_t const SDF_Succubus_4_12_Size = SDF_4_Header::SIZES[12];
static constexpr uint32_t const SDF_Succubus_4_13_Size = SDF_4_Header::SIZES[13];
static constexpr uint32_t const SDF_Succubus_4_14_Size = SDF_4_Header::SIZES[14];
static constexpr uint32_t const SDF_Succubus_4_15_Size = SDF_4_Header::SIZES[15];

uint8_t const* const SDF_Succubus_4_1 = (uint8_t const* const)(QUADSPI_ADDRESS + SDF_4_MemoryStart);
uint8_t const* const SDF_Succubus_4_2 = SDF_Succubus_4_1 + SDF_Succubus_4_1_Size;
uint8_t const* const SDF_Succubus_4_3 = SDF_Succubus_4_2 + SDF_Succubus_4_2_Size;
uint8_t const* const SDF_Succubus_4_4 = SDF_Succubus_4_3 + SDF_Succubus_4_3_Size;
uint8_t const* const SDF_Succubus_4_5 = SDF_Succubus_4_4 + SDF_Succubus_4_4_Size;
uint8_t const* const SDF_Succubus_4_6 = SDF_Succubus_4_5 + SDF_Succubus_4_5_Size;
uint8_t const* const SDF_Succubus_4_7 = SDF_Succubus_4_6 + SDF_Succubus_4_6_Size;
uint8_t const* const SDF_Succubus_4_8 = SDF_Succubus_4_7 + SDF_Succubus_4_7_Size;
uint8_t const* const SDF_Succubus_4_9 = SDF_Succubus_4_8 + SDF_Succubus_4_8_Size;
uint8_t const* const SDF_Succubus_4_10 = SDF_Succubus_4_9 + SDF_Succubus_4_9_Size;
uint8_t const* const SDF_Succubus_4_11 = SDF_Succubus_4_10 + SDF_Succubus_4_10_Size;
uint8_t const* const SDF_Succubus_4_12 = SDF_Succubus_4_11 + SDF_Succubus_4_11_Size;
uint8_t const* const SDF_Succubus_4_13 = SDF_Succubus_4_12 + SDF_Succubus_4_12_Size;
uint8_t const* const SDF_Succubus_4_14 = SDF_Succubus_4_13 + SDF_Succubus_4_13_Size;
uint8_t const* const SDF_Succubus_4_15 = SDF_Succubus_4_14 + SDF_Succubus_4_14_Size;


// ######### //


static constexpr uint32_t const SDF_6_MemoryStart = (SDF_2_Header::TOTALSIZE - SDF_2_Header::SIZES[0]) + (SDF_4_Header::TOTALSIZE - SDF_4_Header::SIZES[0]);

//static constexpr uint32_t const SDF_Succubus_6_0_Size = SDF_6_Header::SIZES[0];
static constexpr uint32_t const SDF_Succubus_6_1_Size = SDF_6_Header::SIZES[1];
static constexpr uint32_t const SDF_Succubus_6_2_Size = SDF_6_Header::SIZES[2];
static constexpr uint32_t const SDF_Succubus_6_3_Size = SDF_6_Header::SIZES[3];
static constexpr uint32_t const SDF_Succubus_6_4_Size = SDF_6_Header::SIZES[4];
static constexpr uint32_t const SDF_Succubus_6_5_Size = SDF_6_Header::SIZES[5];
static constexpr uint32_t const SDF_Succubus_6_6_Size = SDF_6_Header::SIZES[6];
static constexpr uint32_t const SDF_Succubus_6_7_Size = SDF_6_Header::SIZES[7];
static constexpr uint32_t const SDF_Succubus_6_8_Size = SDF_6_Header::SIZES[8];
static constexpr uint32_t const SDF_Succubus_6_9_Size = SDF_6_Header::SIZES[9];
static constexpr uint32_t const SDF_Succubus_6_10_Size = SDF_6_Header::SIZES[10];
static constexpr uint32_t const SDF_Succubus_6_11_Size = SDF_6_Header::SIZES[11];
static constexpr uint32_t const SDF_Succubus_6_12_Size = SDF_6_Header::SIZES[12];
static constexpr uint32_t const SDF_Succubus_6_13_Size = SDF_6_Header::SIZES[13];
static constexpr uint32_t const SDF_Succubus_6_14_Size = SDF_6_Header::SIZES[14];
static constexpr uint32_t const SDF_Succubus_6_15_Size = SDF_6_Header::SIZES[15];

uint8_t const* const SDF_Succubus_6_1 = (uint8_t const* const)(QUADSPI_ADDRESS + SDF_6_MemoryStart);
uint8_t const* const SDF_Succubus_6_2 = SDF_Succubus_6_1 + SDF_Succubus_6_1_Size;
uint8_t const* const SDF_Succubus_6_3 = SDF_Succubus_6_2 + SDF_Succubus_6_2_Size;
uint8_t const* const SDF_Succubus_6_4 = SDF_Succubus_6_3 + SDF_Succubus_6_3_Size;
uint8_t const* const SDF_Succubus_6_5 = SDF_Succubus_6_4 + SDF_Succubus_6_4_Size;
uint8_t const* const SDF_Succubus_6_6 = SDF_Succubus_6_5 + SDF_Succubus_6_5_Size;
uint8_t const* const SDF_Succubus_6_7 = SDF_Succubus_6_6 + SDF_Succubus_6_6_Size;
uint8_t const* const SDF_Succubus_6_8 = SDF_Succubus_6_7 + SDF_Succubus_6_7_Size;
uint8_t const* const SDF_Succubus_6_9 = SDF_Succubus_6_8 + SDF_Succubus_6_8_Size;
uint8_t const* const SDF_Succubus_6_10 = SDF_Succubus_6_9 + SDF_Succubus_6_9_Size;
uint8_t const* const SDF_Succubus_6_11 = SDF_Succubus_6_10 + SDF_Succubus_6_10_Size;
uint8_t const* const SDF_Succubus_6_12 = SDF_Succubus_6_11 + SDF_Succubus_6_11_Size;
uint8_t const* const SDF_Succubus_6_13 = SDF_Succubus_6_12 + SDF_Succubus_6_12_Size;
uint8_t const* const SDF_Succubus_6_14 = SDF_Succubus_6_13 + SDF_Succubus_6_13_Size;
uint8_t const* const SDF_Succubus_6_15 = SDF_Succubus_6_14 + SDF_Succubus_6_14_Size;


// ######### //


static constexpr uint32_t const SDF_7_MemoryStart = (SDF_2_Header::TOTALSIZE - SDF_2_Header::SIZES[0]) + (SDF_4_Header::TOTALSIZE - SDF_4_Header::SIZES[0]) + (SDF_6_Header::TOTALSIZE - SDF_6_Header::SIZES[0]);

//static constexpr uint32_t const SDF_Succubus_7_0_Size = SDF_7_Header::SIZES[0];
static constexpr uint32_t const SDF_Succubus_7_1_Size = SDF_7_Header::SIZES[1];
static constexpr uint32_t const SDF_Succubus_7_2_Size = SDF_7_Header::SIZES[2];
static constexpr uint32_t const SDF_Succubus_7_3_Size = SDF_7_Header::SIZES[3];
static constexpr uint32_t const SDF_Succubus_7_4_Size = SDF_7_Header::SIZES[4];
static constexpr uint32_t const SDF_Succubus_7_5_Size = SDF_7_Header::SIZES[5];
static constexpr uint32_t const SDF_Succubus_7_6_Size = SDF_7_Header::SIZES[6];
static constexpr uint32_t const SDF_Succubus_7_7_Size = SDF_7_Header::SIZES[7];
static constexpr uint32_t const SDF_Succubus_7_8_Size = SDF_7_Header::SIZES[8];
static constexpr uint32_t const SDF_Succubus_7_9_Size = SDF_7_Header::SIZES[9];
static constexpr uint32_t const SDF_Succubus_7_10_Size = SDF_7_Header::SIZES[10];
static constexpr uint32_t const SDF_Succubus_7_11_Size = SDF_7_Header::SIZES[11];
static constexpr uint32_t const SDF_Succubus_7_12_Size = SDF_7_Header::SIZES[12];
static constexpr uint32_t const SDF_Succubus_7_13_Size = SDF_7_Header::SIZES[13];
static constexpr uint32_t const SDF_Succubus_7_14_Size = SDF_7_Header::SIZES[14];
static constexpr uint32_t const SDF_Succubus_7_15_Size = SDF_7_Header::SIZES[15];

uint8_t const* const SDF_Succubus_7_1 = (uint8_t const* const)(QUADSPI_ADDRESS + SDF_7_MemoryStart);
uint8_t const* const SDF_Succubus_7_2 = SDF_Succubus_7_1 + SDF_Succubus_7_1_Size;
uint8_t const* const SDF_Succubus_7_3 = SDF_Succubus_7_2 + SDF_Succubus_7_2_Size;
uint8_t const* const SDF_Succubus_7_4 = SDF_Succubus_7_3 + SDF_Succubus_7_3_Size;
uint8_t const* const SDF_Succubus_7_5 = SDF_Succubus_7_4 + SDF_Succubus_7_4_Size;
uint8_t const* const SDF_Succubus_7_6 = SDF_Succubus_7_5 + SDF_Succubus_7_5_Size;
uint8_t const* const SDF_Succubus_7_7 = SDF_Succubus_7_6 + SDF_Succubus_7_6_Size;
uint8_t const* const SDF_Succubus_7_8 = SDF_Succubus_7_7 + SDF_Succubus_7_7_Size;
uint8_t const* const SDF_Succubus_7_9 = SDF_Succubus_7_8 + SDF_Succubus_7_8_Size;
uint8_t const* const SDF_Succubus_7_10 = SDF_Succubus_7_9 + SDF_Succubus_7_9_Size;
uint8_t const* const SDF_Succubus_7_11 = SDF_Succubus_7_10 + SDF_Succubus_7_10_Size;
uint8_t const* const SDF_Succubus_7_12 = SDF_Succubus_7_11 + SDF_Succubus_7_11_Size;
uint8_t const* const SDF_Succubus_7_13 = SDF_Succubus_7_12 + SDF_Succubus_7_12_Size;
uint8_t const* const SDF_Succubus_7_14 = SDF_Succubus_7_13 + SDF_Succubus_7_13_Size;
uint8_t const* const SDF_Succubus_7_15 = SDF_Succubus_7_14 + SDF_Succubus_7_14_Size;
*/

#endif



