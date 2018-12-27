/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

#ifndef ATOMIC_OPS_H
#define ATOMIC_OPS_H

#ifdef __cplusplus
#include "PreprocessorCore.h"

// ####### Template Argyment for atomic_strict must be a 32bit interger type (uint32_t, int32_t), bool is actually a byte on ARM (do not use bool)
// bool requires ldrexb / strexb implementation
// use : atomic_strict_uint32_t or atomic_strict_int32_t

// ####### Atomic SET ######## //
// This is a write to memory that is atomic. ldrex signals lock acquired on memory location. strex signals attempt to update acquired memory location
// strex is in a loop based on its ability to update the memory exclusively, and guarrenteees the atomic store 
// Due to the inherent nature, this atomic set operation implicity defines reads as lock-free. Reads from same memory location
// can be done as normal because of this fact. Use atomic_strict for the variable to guarentee this.
template <typename T>
__attribute__((always_inline)) STATIC_INLINE void atomic_set(volatile T* const __restrict ptr, int const set_value) 
{
  unsigned int old_value_tmp,
							 strex_result_tmp;

	__asm volatile (R"(   
										.strex_loop%=:												// loop must be here to avoid possible infinite loop )https://electronics.stackexchange.com/questions/25690/critical-sections-on-cortex-m3)
											ldrex %[old_value_tmp], %[ptr]
											strex %[strex_result_tmp], %[set_value], %[ptr]
											cmp %[strex_result_tmp], #0
											bne .strex_loop%=
										)"
											 : [old_value_tmp]"=&r"(old_value_tmp), 
												 [strex_result_tmp]"=&r"(strex_result_tmp), 
												 [ptr]"+m"(*ptr)
											 : [ptr]"r"(ptr), 
												 [set_value]"r"(set_value)
											 : "cc", "memory");
}
template <typename T>
__attribute__((always_inline)) STATIC_INLINE void atomic_increment(volatile T* const __restrict ptr) 
{
  unsigned int value_tmp,
							 strex_result_tmp;

	__asm volatile (R"( 
										.strex_loop%=:								// loop must be here to avoid possible infinite loop )https://electronics.stackexchange.com/questions/25690/critical-sections-on-cortex-m3)
											ldrex %[value_tmp], %[ptr]
											add %[value_tmp], #1
											strex %[strex_result_tmp], %[value_tmp], %[ptr]
											cmp %[strex_result_tmp], #0
											bne .strex_loop%=
										)"
											 : [value_tmp]"=&r"(value_tmp), 
												 [strex_result_tmp]"=&r"(strex_result_tmp), 
												 [ptr]"+m"(*ptr)
											 : [ptr]"r"(ptr)
											 : "cc", "memory");
}
template <typename T>
__attribute__((always_inline)) STATIC_INLINE void atomic_decrement(volatile T* const __restrict ptr) 
{
  unsigned int value_tmp,
							 strex_result_tmp;

	__asm volatile (R"(   
										.strex_loop%=:											// loop must be here to avoid possible infinite loop )https://electronics.stackexchange.com/questions/25690/critical-sections-on-cortex-m3)
											ldrex %[value_tmp], %[ptr]
											sub %[value_tmp], #1
											strex %[strex_result_tmp], %[value_tmp], %[ptr]
											cmp %[strex_result_tmp], #0
											bne .strex_loop%=
										)"
											 : [value_tmp]"=&r"(value_tmp), 
												 [strex_result_tmp]"=&r"(strex_result_tmp), 
												 [ptr]"+m"(*ptr)
											 : [ptr]"r"(ptr)
											 : "cc", "memory");
}

template <class T>
struct atomic_strict {
	
protected:
	volatile T exclusive;
	
public:
	// enforce initialization value
	__attribute__((always_inline)) inline atomic_strict(int const set_init_value) : exclusive(set_init_value) {}
	__attribute__((always_inline)) inline atomic_strict(atomic_strict&& rhs) = default;
		
	__attribute__((always_inline)) __attribute__((pure)) inline operator T const() const
	{
		return(exclusive);
	}
	
	__attribute__((always_inline)) inline void operator=(T const& set_value)
	{
		atomic_set<T>(&exclusive, set_value);
	}
	__attribute__((always_inline)) inline T const operator++() // pre-increment
	{
		atomic_increment<T>(&exclusive);
		return(*this);
	}
	__attribute__((always_inline)) inline T const operator++(int const) // post-increment
	{
		T const old_value(exclusive);
		(void)operator++(); // pre-increment leverage
		return(old_value); // returns value before increment, the way post increment is supposed to work
	}
	__attribute__((always_inline)) inline T const operator--() // pre-decrement
	{
		atomic_decrement<T>(&exclusive);
		return(*this);
	}
	__attribute__((always_inline)) inline T const operator--(int const) // post-decrement
	{
		T const old_value(exclusive);
		(void)operator--(); // pre-decrement leverage
		return(old_value); // returns value before decrement, the way post decrement is supposed to work
	}
private:
	// prevent any normal ops that would allow setting of this type
	atomic_strict() = delete;
	void operator=(atomic_strict const&) = delete;
	atomic_strict(atomic_strict const&) = delete;
};
	
#define atomic_strict_uint32_t atomic_strict<uint32_t>
#define atomic_strict_int32_t atomic_strict<int32_t>
/*
void layouttemplate(int *src, int *dst, int len) {
	assert((len % 4) == 0);
	int tmp;
	// This uses the "%=" template string to create a label which can be used
	// elsewhere inside the assembly block, but which will not conflict with
	// inlined copies of it.
	// R is a C++11 raw string literal. See the example in 10.2 File-scope inline assembly.
	__asm(R"(
		.Lloop%=:
			ldr %[tmp], %[src], #4
			str %[tmp], %[dst], #4
			subs %[len], #4
			bne .Lloop%=)"
		: [dst] "=&m" (*dst),
			[tmp] "=&r" (tmp),
			[len] "+r" (len)
		: [src] "m" (*src));
}
*/


	

#endif  // __cplusplus
#endif

