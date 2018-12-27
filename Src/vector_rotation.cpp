/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

#include "globals.h"
#include "vector_rotation.h"

sRotationConstants const RotationConstants
	__attribute__((section (".dtcm_const")));

sRotationConstants::sRotationConstants()
: v180(180.0f), v90(90.0f), v45(45.0f), v1(1.0f)
{}