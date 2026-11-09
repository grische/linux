#ifndef _LINUX_ZUGRIFF_H
#define _LINUX_ZUGRIFF_H


#define SET_APPL_ID(msg,a) (*(uint16_t far *)((msg)+2) = (a))

#if defined(__KERNEL__)
#include <linux/types.h>

#if defined(__BIG_ENDIAN)
#define ZUGRIFF_HOST_BE
#else
#define ZUGRIFF_HOST_LE
#endif

#else /*--- #if defined(__KERNEL__) ---*/
#include <stdint.h>

#if defined(__BYTE_ORDER)

#if __BYTE_ORDER == __BIG_ENDIAN
#define ZUGRIFF_HOST_BE
#else 
#define ZUGRIFF_HOST_LE
#endif

#else /*--- #if defined(__BYTE_ORDER) ---*/

#error __BYTE_ORDER undefined (missing include for endian.h?)

#endif /*--- #else  ---*//*--- #if defined(__BYTE_ORDER) ---*/

#endif /*--- #else  ---*//*--- #if defined(__KERNEL__) ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#define ENABLE_OBSOLETE_ADD_FUNCTIONS
#define ENABLE_OBSOLETE_CSTRUCT_FUNCTIONS

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#if defined(ENABLE_OBSOLETE_ADD_FUNCTIONS)
static inline uint8_t *add_byte(uint8_t *p, uint8_t s) {
	*p++ = (uint8_t)(s);
	return p;
}
#endif /*--- #if defined(ENABLE_OBSOLETE_ADD_FUNCTIONS) ---*/

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#if defined(ENABLE_OBSOLETE_ADD_FUNCTIONS)
static inline uint8_t *add_word(uint8_t *p, uint16_t s) {
	*p++ = (uint8_t)(s);
	*p++ = (uint8_t)(s >> 8);
	return p;
}
#endif /*--- #if defined(ENABLE_OBSOLETE_ADD_FUNCTIONS) ---*/

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#if defined(ENABLE_OBSOLETE_ADD_FUNCTIONS)
static inline void SET_WORD (void *Msg, uint16_t w) {
	add_word((uint8_t *)Msg, w);
}
#endif /*--- #if defined(ENABLE_OBSOLETE_ADD_FUNCTIONS) ---*/

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#if defined(ENABLE_OBSOLETE_ADD_FUNCTIONS)
static inline uint8_t *add_dword(uint8_t *p, uint32_t s) {
	*p++ = (uint8_t)(s);
	*p++ = (uint8_t)(s >> 8);
	*p++ = (uint8_t)(s >> 16);
	*p++ = (uint8_t)(s >> 24);
	return p;
}
#endif /*--- #if defined(ENABLE_OBSOLETE_ADD_FUNCTIONS) ---*/

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#if defined(ENABLE_OBSOLETE_ADD_FUNCTIONS)
static inline void SET_DWORD (void *Msg, uint32_t w) {
	add_dword( (uint8_t *)Msg, w);
}
#endif /*--- #if defined(ENABLE_OBSOLETE_ADD_FUNCTIONS) ---*/

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#if defined(ENABLE_OBSOLETE_CSTRUCT_FUNCTIONS)
static inline uint8_t *add_length(uint8_t *p, uint32_t len) {
	if(len >= 255) {
		*p++ = 0xFF;
		*p++ = (uint8_t)len;
		*p++ = (uint8_t)(len >> 8);
		return p;
	}
	*p++ = (uint8_t)len;
	return p;
}
#endif /*--- #if defined(ENABLE_OBSOLETE_CSTRUCT_FUNCTIONS) ---*/

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#if defined(ENABLE_OBSOLETE_CSTRUCT_FUNCTIONS)
static inline uint8_t *add_cstruct(uint8_t *p, uint8_t *Buffer, uint32_t len) {
	p = add_length(p, len);
	while(len) 
		*p++ = *Buffer++;
	return p;
}
#endif /*--- #if defined(ENABLE_OBSOLETE_CSTRUCT_FUNCTIONS) ---*/

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#if defined(ENABLE_OBSOLETE_CSTRUCT_FUNCTIONS)
static inline uint32_t cstruct_length(uint32_t len) {
	if(len >= 255)
		return len + 3;
	return len + 1;
}
#endif /*--- #if defined(ENABLE_OBSOLETE_CSTRUCT_FUNCTIONS) ---*/

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#if defined(ENABLE_OBSOLETE_CSTRUCT_FUNCTIONS)
static inline uint8_t *extract_length(uint8_t *p, uint32_t *len) {
	if(*p == 0xFF) {
		p++;
		*len  = *p++;
		*len |= ((uint32_t)(*p++)) << 8;
		return p;
	}
	*len  = *p++;
	return p;
}
#endif /*--- #if defined(ENABLE_OBSOLETE_CSTRUCT_FUNCTIONS) ---*/

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#if defined(ENABLE_OBSOLETE_EXTRACT_FUNCTIONS)
static inline uint8_t *extract_dword(uint8_t *p, uint32_t *value) {
	*value  = *p++;
	*value |= ((uint32_t)(*p++)) << 8;
	*value |= ((uint32_t)(*p++)) << 16;
	*value |= ((uint32_t)(*p++)) << 24;
	return p;
}
#endif /*--- #if defined(ENABLE_OBSOLETE_EXTRACT_FUNCTIONS) ---*/

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#if defined(ENABLE_OBSOLETE_EXTRACT_FUNCTIONS)
static inline uint8_t *extract_word(uint8_t *p, uint16_t *value) {
	*value  = *p++;
	*value |= ((uint16_t)(*p++)) << 8;
	return p;
}
#endif /*--- #if defined(ENABLE_OBSOLETE_EXTRACT_FUNCTIONS) ---*/

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#if defined(ENABLE_OBSOLETE_EXTRACT_FUNCTIONS)
static inline uint8_t *extract_byte(uint8_t *p, uint8_t *value) {
	*value  = *p++;
	return p;
}
#endif /*--- #if defined(ENABLE_OBSOLETE_EXTRACT_FUNCTIONS) ---*/

/*------------------------------------------------------------------------------------------*\
 * QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD
 * QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD
\*------------------------------------------------------------------------------------------*/
static inline void *set_unaligned_qword(void *_dest, uint64_t _source) {
	uint8_t *source = (uint8_t *)&_source;
	uint8_t *dest = (uint8_t *)_dest;
	dest[0] = source[0];
	dest[1] = source[1];
	dest[2] = source[2];
	dest[3] = source[3];
	dest[4] = source[4];
	dest[5] = source[5];
	dest[6] = source[6];
	dest[7] = source[7];
	return dest + sizeof(uint64_t);
}
#define copy_qword_to_unaligned set_unaligned_qword

/*------------------------------------------------------------------------------------------*\
 * QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD
\*------------------------------------------------------------------------------------------*/
static inline void *set_le_unaligned_qword(void *_dest, uint64_t source) {
	uint8_t *dest = (uint8_t *)_dest;
	dest[0] = (uint8_t)(source >> 0);
	dest[1] = (uint8_t)(source >> 8);
	dest[2] = (uint8_t)(source >> 16);
	dest[3] = (uint8_t)(source >> 24);
	dest[4] = (uint8_t)(source >> 32);
	dest[5] = (uint8_t)(source >> 40);
	dest[6] = (uint8_t)(source >> 48);
	dest[7] = (uint8_t)(source >> 56);
	return dest + sizeof(uint64_t);
}
#define copy_qword_to_le_unaligned set_le_unaligned_qword
#define SET_QWORD                  set_le_unaligned_qword

/*------------------------------------------------------------------------------------------*\
 * QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD
\*------------------------------------------------------------------------------------------*/
static inline void *set_be_unaligned_qword(void *_dest, uint64_t source) {
	uint8_t *dest = (uint8_t *)_dest;
	dest[0] = (uint8_t)(source >> 56);
	dest[1] = (uint8_t)(source >> 48);
	dest[2] = (uint8_t)(source >> 40);
	dest[3] = (uint8_t)(source >> 32);
	dest[4] = (uint8_t)(source >> 24);
	dest[5] = (uint8_t)(source >> 16);
	dest[6] = (uint8_t)(source >>  8);
	dest[7] = (uint8_t)(source >>  0);
	return dest + sizeof(uint64_t);
}
#define copy_qword_to_be_unaligned set_be_unaligned_qword

/*------------------------------------------------------------------------------------------*\
 * QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD
\*------------------------------------------------------------------------------------------*/
static inline void *set_le_aligned_qword(void *_dest, uint64_t source) {
	uint8_t *dest = (uint8_t *)_dest;
#if defined(ZUGRIFF_HOST_LE)
	*(uint64_t *)_dest = source;
#endif /*--- #if defined(ZUGRIFF_HOST_LE) ---*/
#if defined(ZUGRIFF_HOST_BE)
	dest[0] = (uint8_t)(source >> 0);
	dest[1] = (uint8_t)(source >> 8);
	dest[2] = (uint8_t)(source >> 16);
	dest[3] = (uint8_t)(source >> 24);
	dest[4] = (uint8_t)(source >> 32);
	dest[5] = (uint8_t)(source >> 40);
	dest[6] = (uint8_t)(source >> 48);
	dest[7] = (uint8_t)(source >> 56);
#endif /*--- #if defined(ZUGRIFF_HOST_BE) ---*/
	return dest + sizeof(uint64_t);
}
#define copy_qword_to_le_aligned set_le_aligned_qword

/*------------------------------------------------------------------------------------------*\
 * QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD
\*------------------------------------------------------------------------------------------*/
static inline uint64_t extract_le_unaligned_qword(void *_source) {
	uint8_t *source = (uint8_t*)_source;
	return 
		((uint64_t)source[0] <<  0) | 
		((uint64_t)source[1] <<  8) | 
		((uint64_t)source[2] << 16) | 
		((uint64_t)source[3] << 24) |
		((uint64_t)source[4] << 32) | 
		((uint64_t)source[5] << 40) | 
		((uint64_t)source[6] << 48) | 
		((uint64_t)source[7] << 56) |
		0;
}
#define copy_qword_from_le_unaligned        extract_le_unaligned_qword
#define EXTRACT_QWORD                       extract_le_unaligned_qword

/*------------------------------------------------------------------------------------------*\
 * QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD
\*------------------------------------------------------------------------------------------*/
static inline uint64_t extract_be_unaligned_qword(void *_source) {
	uint8_t *source = (uint8_t*)_source;
	return 
		((uint64_t)source[0] << 56) | 
		((uint64_t)source[1] << 48) | 
		((uint64_t)source[2] << 40) | 
		((uint64_t)source[3] << 32) |
		((uint64_t)source[4] << 24) | 
		((uint64_t)source[5] << 16) | 
		((uint64_t)source[6] <<  8) | 
		((uint64_t)source[7] <<  0) |
		0;
}
#define copy_qword_from_be_unaligned        extract_be_unaligned_qword

/*------------------------------------------------------------------------------------------*\
 * QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD
\*------------------------------------------------------------------------------------------*/
static inline uint64_t extract_le_aligned_qword(void *_source) {
	uint8_t *source = (uint8_t*)_source;
	return 
		((uint64_t)source[0] <<  0) | 
		((uint64_t)source[1] <<  8) | 
		((uint64_t)source[2] << 16) | 
		((uint64_t)source[3] << 24) |
		((uint64_t)source[4] << 32) | 
		((uint64_t)source[5] << 40) | 
		((uint64_t)source[6] << 48) | 
		((uint64_t)source[7] << 56) |
		0;
}
#define copy_qword_from_le_aligned        extract_le_aligned_qword

/*------------------------------------------------------------------------------------------*\
 * QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD
\*------------------------------------------------------------------------------------------*/
static inline uint64_t extract_unaligned_qword(void *_source) {
	uint8_t *source = (uint8_t*)_source;
	uint32_t _dest;
	uint8_t *dest = (uint8_t *)&_dest;
	dest[0] = source[0];
	dest[1] = source[1];
	dest[2] = source[2];
	dest[3] = source[3];
	dest[4] = source[4];
	dest[5] = source[5];
	dest[6] = source[6];
	dest[7] = source[7];
	return _dest;
}
#define copy_qword_from_unaligned        extract_unaligned_qword

/*------------------------------------------------------------------------------------------*\
 * DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD
 * DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD
\*------------------------------------------------------------------------------------------*/
static inline void *set_unaligned_dword(void *_dest, uint32_t _source) {
	uint8_t *source = (uint8_t *)&_source;
	uint8_t *dest = (uint8_t *)_dest;
	dest[0] = source[0];
	dest[1] = source[1];
	dest[2] = source[2];
	dest[3] = source[3];
	return dest + sizeof(uint32_t);
}
#define copy_dword_to_unaligned set_unaligned_dword

/*------------------------------------------------------------------------------------------*\
 * DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD
\*------------------------------------------------------------------------------------------*/
static inline void *set_le_unaligned_dword(void *_dest, uint32_t source) {
	uint8_t *dest = (uint8_t *)_dest;
	dest[0] = (uint8_t)(source >> 0);
	dest[1] = (uint8_t)(source >> 8);
	dest[2] = (uint8_t)(source >> 16);
	dest[3] = (uint8_t)(source >> 24);
	return dest + sizeof(uint32_t);
}
#define copy_dword_to_le_unaligned set_le_unaligned_dword
#define SET_DWORD                  set_le_unaligned_dword

/*------------------------------------------------------------------------------------------*\
 * DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD
\*------------------------------------------------------------------------------------------*/
static inline void *set_be_unaligned_dword(void *_dest, uint32_t source) {
	uint8_t *dest = (uint8_t *)_dest;
	dest[0] = (uint8_t)(source >> 24);
	dest[1] = (uint8_t)(source >> 16);
	dest[2] = (uint8_t)(source >>  8);
	dest[3] = (uint8_t)(source >>  0);
	return dest + sizeof(uint32_t);
}
#define copy_dword_to_be_unaligned set_be_unaligned_dword

/*------------------------------------------------------------------------------------------*\
 * DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD
\*------------------------------------------------------------------------------------------*/
static inline void *set_le_aligned_dword(void *_dest, uint32_t source) {
	uint8_t *dest = (uint8_t *)_dest;
#if defined(ZUGRIFF_HOST_LE)
	*(uint32_t *)_dest = source;
#endif /*--- #if defined(ZUGRIFF_HOST_LE) ---*/
#if defined(ZUGRIFF_HOST_BE)
	dest[0] = (uint8_t)(source >> 0);
	dest[1] = (uint8_t)(source >> 8);
	dest[2] = (uint8_t)(source >> 16);
	dest[3] = (uint8_t)(source >> 24);
#endif /*--- #if defined(ZUGRIFF_HOST_BE) ---*/
	return dest + sizeof(uint32_t);
}
#define copy_dword_to_le_aligned set_le_aligned_dword

/*------------------------------------------------------------------------------------------*\
 * DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD
\*------------------------------------------------------------------------------------------*/
static inline uint32_t extract_le_unaligned_dword(void *_source) {
	uint8_t *source = (uint8_t*)_source;
	return 
		((uint32_t)source[0] <<  0) | 
		((uint32_t)source[1] <<  8) | 
		((uint32_t)source[2] << 16) | 
		((uint32_t)source[3] << 24) |
		0;
}
#define copy_dword_from_le_unaligned        extract_le_unaligned_dword
#define EXTRACT_DWORD                       extract_le_unaligned_dword

/*------------------------------------------------------------------------------------------*\
 * DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD
\*------------------------------------------------------------------------------------------*/
static inline uint32_t extract_be_unaligned_dword(void *_source) {
	uint8_t *source = (uint8_t*)_source;
	return 
		((uint32_t)source[0] << 24) | 
		((uint32_t)source[1] << 16) | 
		((uint32_t)source[2] <<  8) | 
		((uint32_t)source[3] <<  0) |
		0;
}
#define copy_dword_from_be_unaligned        extract_be_unaligned_dword

/*------------------------------------------------------------------------------------------*\
 * DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD
\*------------------------------------------------------------------------------------------*/
static inline uint32_t extract_le_aligned_dword(void *_source) {
	uint8_t *source = (uint8_t*)_source;
	return 
		((uint32_t)source[0] <<  0) | 
		((uint32_t)source[1] <<  8) | 
		((uint32_t)source[2] << 16) | 
		((uint32_t)source[3] << 24) |
		0;
}
#define copy_dword_from_le_aligned        extract_le_aligned_dword

/*------------------------------------------------------------------------------------------*\
 * DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD
\*------------------------------------------------------------------------------------------*/
static inline uint32_t extract_unaligned_dword(void *_source) {
	uint8_t *source = (uint8_t*)_source;
	uint32_t _dest;
	uint8_t *dest = (uint8_t *)&_dest;
	dest[0] = source[0];
	dest[1] = source[1];
	dest[2] = source[2];
	dest[3] = source[3];
	return _dest;
}
#define copy_dword_from_unaligned        extract_unaligned_dword

/*------------------------------------------------------------------------------------------*\
 * WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD
 * WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD
\*------------------------------------------------------------------------------------------*/
static inline void *set_unaligned_word(void *_dest, uint16_t _source) {
	uint8_t *source = (uint8_t *)&_source;
	uint8_t *dest = (uint8_t *)_dest;
	dest[0] = source[0];
	dest[1] = source[1];
	return dest + sizeof(uint16_t);
}
#define copy_word_to_unaligned set_unaligned_word

/*------------------------------------------------------------------------------------------*\
 * WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD
\*------------------------------------------------------------------------------------------*/
static inline void *set_le_unaligned_word(void *_dest, uint16_t source) {
	uint8_t *dest = (uint8_t *)_dest;
	dest[0] = (uint8_t)(source >> 0);
	dest[1] = (uint8_t)(source >> 8);
	return dest + sizeof(uint16_t);
}
#define copy_word_to_le_unaligned set_le_unaligned_word
#define SET_WORD                  set_le_unaligned_word

/*------------------------------------------------------------------------------------------*\
 * WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD
\*------------------------------------------------------------------------------------------*/
static inline void *set_be_unaligned_word(void *_dest, uint16_t source) {
	uint8_t *dest = (uint8_t *)_dest;
	dest[0] = (uint8_t)(source >> 8);
	dest[1] = (uint8_t)(source >> 0);
	return dest + sizeof(uint16_t);
}
#define copy_word_to_be_unaligned set_be_unaligned_word

/*------------------------------------------------------------------------------------------*\
 * WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD
\*------------------------------------------------------------------------------------------*/
static inline void *set_le_aligned_word(void *_dest, uint16_t source) {
	uint8_t *dest = (uint8_t *)_dest;
#if defined(ZUGRIFF_HOST_LE)
	*(uint16_t *)_dest = source;
#endif /*--- #if defined(ZUGRIFF_HOST_LE) ---*/
#if defined(ZUGRIFF_HOST_BE)
	dest[0] = (uint8_t)(source >> 0);
	dest[1] = (uint8_t)(source >> 8);
#endif /*--- #if defined(ZUGRIFF_HOST_BE) ---*/
	return dest + sizeof(uint16_t);
}
#define copy_word_to_le_aligned set_le_aligned_word

/*------------------------------------------------------------------------------------------*\
 * WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD
\*------------------------------------------------------------------------------------------*/
static inline uint16_t extract_le_unaligned_word(void *_source) {
	uint8_t *source = (uint8_t*)_source;
	return 
		((uint16_t)source[0] <<  0) | 
		((uint16_t)source[1] <<  8) | 
		0;
}
#define copy_word_from_le_unaligned        extract_le_unaligned_word
#define EXTRACT_WORD                       extract_le_unaligned_word

/*------------------------------------------------------------------------------------------*\
 * WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD
\*------------------------------------------------------------------------------------------*/
static inline uint16_t extract_be_unaligned_word(void *_source) {
	uint8_t *source = (uint8_t*)_source;
	return 
		((uint16_t)source[0] <<  8) | 
		((uint16_t)source[1] <<  0) | 
		0;
}
#define copy_word_from_be_unaligned        extract_be_unaligned_word

/*------------------------------------------------------------------------------------------*\
 * WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD
\*------------------------------------------------------------------------------------------*/
static inline uint16_t extract_le_aligned_word(void *_source) {
	uint8_t *source = (uint8_t*)_source;
	return 
		((uint16_t)source[0] <<  0) | 
		((uint16_t)source[1] <<  8) | 
		0;
}
#define copy_word_from_le_aligned        extract_le_aligned_word

/*------------------------------------------------------------------------------------------*\
 * WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD
\*------------------------------------------------------------------------------------------*/
static inline uint16_t extract_unaligned_word(void *_source) {
	uint16_t _dest;
	uint8_t *source = (uint8_t*)_source;
	uint8_t *dest = (uint8_t *)&_dest;
	dest[0] = source[0];
	dest[1] = source[1];
	return _dest;
}
#define copy_word_from_unaligned        extract_unaligned_word

/*------------------------------------------------------------------------------------------*\
 * BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE
 * BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE
\*------------------------------------------------------------------------------------------*/
static inline void *set_unaligned_byte(void *_dest, uint8_t source) {
	uint8_t *dest = (uint8_t *)_dest;
	*dest = source;
	return dest + sizeof(uint8_t);
}
#define copy_byte_to_unaligned set_unaligned_byte

/*------------------------------------------------------------------------------------------*\
 * BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE
\*------------------------------------------------------------------------------------------*/
static inline void *set_le_unaligned_byte(void *_dest, uint8_t source) {
	uint8_t *dest = (uint8_t *)_dest;
	*dest = source;
	return dest + sizeof(uint8_t);
}
#define copy_byte_to_le_unaligned set_le_unaligned_byte
#define SET_BYTE                  set_le_unaligned_byte

/*------------------------------------------------------------------------------------------*\
 * BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE
\*------------------------------------------------------------------------------------------*/
static inline void *set_le_aligned_byte(void *_dest, uint8_t source) {
	uint8_t *dest = (uint8_t *)_dest;
	*dest = source;
	return dest + sizeof(uint8_t);
}
#define copy_byte_to_le_aligned set_le_aligned_byte

/*------------------------------------------------------------------------------------------*\
 * BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE
\*------------------------------------------------------------------------------------------*/
static inline uint8_t extract_le_unaligned_byte(void *_source) {
	uint8_t *source = (uint8_t*)_source;
	return *source;
}
#define copy_byte_from_le_unaligned        extract_le_unaligned_byte
#define EXTRACT_BYTE                       extract_le_unaligned_byte

/*------------------------------------------------------------------------------------------*\
 * BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE
\*------------------------------------------------------------------------------------------*/
static inline uint8_t extract_le_aligned_byte(void *_source) {
	uint8_t *source = (uint8_t*)_source;
	return *source;
}
#define copy_byte_from_le_aligned        extract_le_aligned_byte

/*------------------------------------------------------------------------------------------*\
 * BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE
\*------------------------------------------------------------------------------------------*/
static inline uint8_t extract_unaligned_byte(void *_source) {
	uint8_t *source = (uint8_t*)_source;
	return *source;
}
#define copy_byte_from_unaligned        extract_unaligned_byte

#endif /*--- #ifndef _LINUX_ZUGRIFF_H ---*/
