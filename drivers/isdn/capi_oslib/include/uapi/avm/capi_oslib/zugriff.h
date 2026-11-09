#ifndef _LINUX_ZUGRIFF_H
#define _LINUX_ZUGRIFF_H

#define SET_APPL_ID(msg,a) (*(unsigned short far *)((msg)+2) = (a))

#if defined(__KERNEL__)

#if defined(__BIG_ENDIAN)
#define ZUGRIFF_HOST_BE
#else
#define ZUGRIFF_HOST_LE
#endif

#else /*--- #if defined(__KERNEL__) ---*/

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
static inline unsigned char *add_byte(unsigned char *p, unsigned char s) {
	*p++ = (unsigned char)(s);
	return p;
}
#endif /*--- #if defined(ENABLE_OBSOLETE_ADD_FUNCTIONS) ---*/

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#if defined(ENABLE_OBSOLETE_ADD_FUNCTIONS)
static inline unsigned char *add_word(unsigned char *p, unsigned short s) {
	*p++ = (unsigned char)(s);
	*p++ = (unsigned char)(s >> 8);
	return p;
}
#endif /*--- #if defined(ENABLE_OBSOLETE_ADD_FUNCTIONS) ---*/

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#if defined(ENABLE_OBSOLETE_ADD_FUNCTIONS)
static inline void SET_WORD (void *Msg, unsigned short w) {
	add_word((unsigned char *)Msg, w);
}
#endif /*--- #if defined(ENABLE_OBSOLETE_ADD_FUNCTIONS) ---*/

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#if defined(ENABLE_OBSOLETE_ADD_FUNCTIONS)
static inline unsigned char *add_dword(unsigned char *p, unsigned long s) {
	*p++ = (unsigned char)(s);
	*p++ = (unsigned char)(s >> 8);
	*p++ = (unsigned char)(s >> 16);
	*p++ = (unsigned char)(s >> 24);
	return p;
}
#endif /*--- #if defined(ENABLE_OBSOLETE_ADD_FUNCTIONS) ---*/

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#if defined(ENABLE_OBSOLETE_ADD_FUNCTIONS)
static inline void SET_DWORD (void *Msg, unsigned long  w) {
	add_dword( (unsigned char *)Msg, w);
}
#endif /*--- #if defined(ENABLE_OBSOLETE_ADD_FUNCTIONS) ---*/

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#if defined(ENABLE_OBSOLETE_CSTRUCT_FUNCTIONS)
static inline unsigned char *add_length(unsigned char *p, unsigned int len) {
	if(len >= 255) {
		*p++ = 0xFF;
		*p++ = (unsigned char)len;
		*p++ = (unsigned char)(len >> 8);
		return p;
	}
	*p++ = (unsigned char)len;
	return p;
}
#endif /*--- #if defined(ENABLE_OBSOLETE_CSTRUCT_FUNCTIONS) ---*/

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#if defined(ENABLE_OBSOLETE_CSTRUCT_FUNCTIONS)
static inline unsigned char *add_cstruct(unsigned char *p, unsigned char *Buffer, unsigned int len) {
	p = add_length(p, len);
	while(len) 
		*p++ = *Buffer++;
	return p;
}
#endif /*--- #if defined(ENABLE_OBSOLETE_CSTRUCT_FUNCTIONS) ---*/

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#if defined(ENABLE_OBSOLETE_CSTRUCT_FUNCTIONS)
static inline unsigned int cstruct_length(unsigned int len) {
	if(len >= 255)
		return len + 3;
	return len + 1;
}
#endif /*--- #if defined(ENABLE_OBSOLETE_CSTRUCT_FUNCTIONS) ---*/

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#if defined(ENABLE_OBSOLETE_CSTRUCT_FUNCTIONS)
static inline unsigned char *extract_length(unsigned char *p, unsigned int *len) {
	if(*p == 0xFF) {
		p++;
		*len  = *p++;
		*len |= ((unsigned int)(*p++)) << 8;
		return p;
	}
	*len  = *p++;
	return p;
}
#endif /*--- #if defined(ENABLE_OBSOLETE_CSTRUCT_FUNCTIONS) ---*/

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#if defined(ENABLE_OBSOLETE_EXTRACT_FUNCTIONS)
static inline unsigned char *extract_dword(unsigned char *p, unsigned long *value) {
	*value  = *p++;
	*value |= ((unsigned long)(*p++)) << 8;
	*value |= ((unsigned long)(*p++)) << 16;
	*value |= ((unsigned long)(*p++)) << 24;
	return p;
}
#endif /*--- #if defined(ENABLE_OBSOLETE_EXTRACT_FUNCTIONS) ---*/

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#if defined(ENABLE_OBSOLETE_EXTRACT_FUNCTIONS)
static inline unsigned char *extract_word(unsigned char *p, unsigned short *value) {
	*value  = *p++;
	*value |= ((unsigned short)(*p++)) << 8;
	return p;
}
#endif /*--- #if defined(ENABLE_OBSOLETE_EXTRACT_FUNCTIONS) ---*/

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#if defined(ENABLE_OBSOLETE_EXTRACT_FUNCTIONS)
static inline unsigned char *extract_byte(unsigned char *p, unsigned char *value) {
	*value  = *p++;
	return p;
}
#endif /*--- #if defined(ENABLE_OBSOLETE_EXTRACT_FUNCTIONS) ---*/

/*------------------------------------------------------------------------------------------*\
 * QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD
 * QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD
\*------------------------------------------------------------------------------------------*/
static inline void *set_unaligned_qword(void *_dest, unsigned long long _source) {
	unsigned char *source = (unsigned char *)&_source;
	unsigned char *dest = (unsigned char *)_dest;
	dest[0] = source[0];
	dest[1] = source[1];
	dest[2] = source[2];
	dest[3] = source[3];
	dest[4] = source[4];
	dest[5] = source[5];
	dest[6] = source[6];
	dest[7] = source[7];
	return dest + sizeof(unsigned long long);
}
#define copy_qword_to_unaligned set_unaligned_qword

/*------------------------------------------------------------------------------------------*\
 * QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD
\*------------------------------------------------------------------------------------------*/
static inline void *set_le_unaligned_qword(void *_dest, unsigned long long source) {
	unsigned char *dest = (unsigned char *)_dest;
	dest[0] = (unsigned char)(source >> 0);
	dest[1] = (unsigned char)(source >> 8);
	dest[2] = (unsigned char)(source >> 16);
	dest[3] = (unsigned char)(source >> 24);
	dest[4] = (unsigned char)(source >> 32);
	dest[5] = (unsigned char)(source >> 40);
	dest[6] = (unsigned char)(source >> 48);
	dest[7] = (unsigned char)(source >> 56);
	return dest + sizeof(unsigned long long);
}
#define copy_qword_to_le_unaligned set_le_unaligned_qword
#define SET_QWORD                  set_le_unaligned_qword

/*------------------------------------------------------------------------------------------*\
 * QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD
\*------------------------------------------------------------------------------------------*/
static inline void *set_be_unaligned_qword(void *_dest, unsigned long long source) {
	unsigned char *dest = (unsigned char *)_dest;
	dest[0] = (unsigned char)(source >> 56);
	dest[1] = (unsigned char)(source >> 48);
	dest[2] = (unsigned char)(source >> 40);
	dest[3] = (unsigned char)(source >> 32);
	dest[4] = (unsigned char)(source >> 24);
	dest[5] = (unsigned char)(source >> 16);
	dest[6] = (unsigned char)(source >>  8);
	dest[7] = (unsigned char)(source >>  0);
	return dest + sizeof(unsigned long long);
}
#define copy_qword_to_be_unaligned set_be_unaligned_qword

/*------------------------------------------------------------------------------------------*\
 * QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD
\*------------------------------------------------------------------------------------------*/
static inline void *set_le_aligned_qword(void *_dest, unsigned long long source) {
	unsigned char *dest = (unsigned char *)_dest;
#if defined(ZUGRIFF_HOST_LE)
	*(unsigned long long *)_dest = source;
#endif /*--- #if defined(ZUGRIFF_HOST_LE) ---*/
#if defined(ZUGRIFF_HOST_BE)
	dest[0] = (unsigned char)(source >> 0);
	dest[1] = (unsigned char)(source >> 8);
	dest[2] = (unsigned char)(source >> 16);
	dest[3] = (unsigned char)(source >> 24);
	dest[4] = (unsigned char)(source >> 32);
	dest[5] = (unsigned char)(source >> 40);
	dest[6] = (unsigned char)(source >> 48);
	dest[7] = (unsigned char)(source >> 56);
#endif /*--- #if defined(ZUGRIFF_HOST_BE) ---*/
	return dest + sizeof(unsigned long long);
}
#define copy_qword_to_le_aligned set_le_aligned_qword

/*------------------------------------------------------------------------------------------*\
 * QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD
\*------------------------------------------------------------------------------------------*/
static inline unsigned long long extract_le_unaligned_qword(void *_source) {
	unsigned char *source = (unsigned char*)_source;
	return 
		((unsigned long long)source[0] <<  0) | 
		((unsigned long long)source[1] <<  8) | 
		((unsigned long long)source[2] << 16) | 
		((unsigned long long)source[3] << 24) |
		((unsigned long long)source[4] << 32) | 
		((unsigned long long)source[5] << 40) | 
		((unsigned long long)source[6] << 48) | 
		((unsigned long long)source[7] << 56) |
		0;
}
#define copy_qword_from_le_unaligned        extract_le_unaligned_qword
#define EXTRACT_QWORD                       extract_le_unaligned_qword

/*------------------------------------------------------------------------------------------*\
 * QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD
\*------------------------------------------------------------------------------------------*/
static inline unsigned long long extract_be_unaligned_qword(void *_source) {
	unsigned char *source = (unsigned char*)_source;
	return 
		((unsigned long long)source[0] << 56) | 
		((unsigned long long)source[1] << 48) | 
		((unsigned long long)source[2] << 40) | 
		((unsigned long long)source[3] << 32) |
		((unsigned long long)source[4] << 24) | 
		((unsigned long long)source[5] << 16) | 
		((unsigned long long)source[6] <<  8) | 
		((unsigned long long)source[7] <<  0) |
		0;
}
#define copy_qword_from_be_unaligned        extract_be_unaligned_qword

/*------------------------------------------------------------------------------------------*\
 * QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD
\*------------------------------------------------------------------------------------------*/
static inline unsigned long long extract_le_aligned_qword(void *_source) {
	unsigned char *source = (unsigned char*)_source;
	return 
		((unsigned long long)source[0] <<  0) | 
		((unsigned long long)source[1] <<  8) | 
		((unsigned long long)source[2] << 16) | 
		((unsigned long long)source[3] << 24) |
		((unsigned long long)source[4] << 32) | 
		((unsigned long long)source[5] << 40) | 
		((unsigned long long)source[6] << 48) | 
		((unsigned long long)source[7] << 56) |
		0;
}
#define copy_qword_from_le_aligned        extract_le_aligned_qword

/*------------------------------------------------------------------------------------------*\
 * QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD QWORD
\*------------------------------------------------------------------------------------------*/
static inline unsigned long extract_unaligned_qword(void *_source) {
	unsigned char *source = (unsigned char*)_source;
	unsigned long _dest;
	unsigned char *dest = (unsigned char *)&_dest;
	dest[0] = source[0];
	dest[1] = source[1];
	dest[2] = source[2];
	dest[3] = source[3];
	return _dest;
}
#define copy_qword_from_unaligned        extract_unaligned_qword

/*------------------------------------------------------------------------------------------*\
 * DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD
 * DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD
\*------------------------------------------------------------------------------------------*/
static inline void *set_unaligned_dword(void *_dest, unsigned long _source) {
	unsigned char *source = (unsigned char *)&_source;
	unsigned char *dest = (unsigned char *)_dest;
	dest[0] = source[0];
	dest[1] = source[1];
	dest[2] = source[2];
	dest[3] = source[3];
	return dest + sizeof(unsigned long);
}
#define copy_dword_to_unaligned set_unaligned_dword

/*------------------------------------------------------------------------------------------*\
 * DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD
\*------------------------------------------------------------------------------------------*/
static inline void *set_le_unaligned_dword(void *_dest, unsigned long source) {
	unsigned char *dest = (unsigned char *)_dest;
	dest[0] = (unsigned char)(source >> 0);
	dest[1] = (unsigned char)(source >> 8);
	dest[2] = (unsigned char)(source >> 16);
	dest[3] = (unsigned char)(source >> 24);
	return dest + sizeof(unsigned long);
}
#define copy_dword_to_le_unaligned set_le_unaligned_dword
#define SET_DWORD                  set_le_unaligned_dword

/*------------------------------------------------------------------------------------------*\
 * DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD
\*------------------------------------------------------------------------------------------*/
static inline void *set_be_unaligned_dword(void *_dest, unsigned long source) {
	unsigned char *dest = (unsigned char *)_dest;
	dest[0] = (unsigned char)(source >> 24);
	dest[1] = (unsigned char)(source >> 16);
	dest[2] = (unsigned char)(source >>  8);
	dest[3] = (unsigned char)(source >>  0);
	return dest + sizeof(unsigned long);
}
#define copy_dword_to_be_unaligned set_be_unaligned_dword

/*------------------------------------------------------------------------------------------*\
 * DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD
\*------------------------------------------------------------------------------------------*/
static inline void *set_le_aligned_dword(void *_dest, unsigned long source) {
	unsigned char *dest = (unsigned char *)_dest;
#if defined(ZUGRIFF_HOST_LE)
	*(unsigned long *)_dest = source;
#endif /*--- #if defined(ZUGRIFF_HOST_LE) ---*/
#if defined(ZUGRIFF_HOST_BE)
	dest[0] = (unsigned char)(source >> 0);
	dest[1] = (unsigned char)(source >> 8);
	dest[2] = (unsigned char)(source >> 16);
	dest[3] = (unsigned char)(source >> 24);
#endif /*--- #if defined(ZUGRIFF_HOST_BE) ---*/
	return dest + sizeof(unsigned long);
}
#define copy_dword_to_le_aligned set_le_aligned_dword

/*------------------------------------------------------------------------------------------*\
 * DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD
\*------------------------------------------------------------------------------------------*/
static inline unsigned long extract_le_unaligned_dword(void *_source) {
	unsigned char *source = (unsigned char*)_source;
	return 
		((unsigned long)source[0] <<  0) | 
		((unsigned long)source[1] <<  8) | 
		((unsigned long)source[2] << 16) | 
		((unsigned long)source[3] << 24) |
		0;
}
#define copy_dword_from_le_unaligned        extract_le_unaligned_dword
#define EXTRACT_DWORD                       extract_le_unaligned_dword

/*------------------------------------------------------------------------------------------*\
 * DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD
\*------------------------------------------------------------------------------------------*/
static inline unsigned long extract_be_unaligned_dword(void *_source) {
	unsigned char *source = (unsigned char*)_source;
	return 
		((unsigned long)source[0] << 24) | 
		((unsigned long)source[1] << 16) | 
		((unsigned long)source[2] <<  8) | 
		((unsigned long)source[3] <<  0) |
		0;
}
#define copy_dword_from_be_unaligned        extract_be_unaligned_dword

/*------------------------------------------------------------------------------------------*\
 * DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD
\*------------------------------------------------------------------------------------------*/
static inline unsigned long extract_le_aligned_dword(void *_source) {
	unsigned char *source = (unsigned char*)_source;
	return 
		((unsigned long)source[0] <<  0) | 
		((unsigned long)source[1] <<  8) | 
		((unsigned long)source[2] << 16) | 
		((unsigned long)source[3] << 24) |
		0;
}
#define copy_dword_from_le_aligned        extract_le_aligned_dword

/*------------------------------------------------------------------------------------------*\
 * DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD DWORD
\*------------------------------------------------------------------------------------------*/
static inline unsigned long extract_unaligned_dword(void *_source) {
	unsigned char *source = (unsigned char*)_source;
	unsigned long _dest;
	unsigned char *dest = (unsigned char *)&_dest;
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
static inline void *set_unaligned_word(void *_dest, unsigned short _source) {
	unsigned char *source = (unsigned char *)&_source;
	unsigned char *dest = (unsigned char *)_dest;
	dest[0] = source[0];
	dest[1] = source[1];
	return dest + sizeof(unsigned short);
}
#define copy_word_to_unaligned set_unaligned_word

/*------------------------------------------------------------------------------------------*\
 * WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD
\*------------------------------------------------------------------------------------------*/
static inline void *set_le_unaligned_word(void *_dest, unsigned short source) {
	unsigned char *dest = (unsigned char *)_dest;
	dest[0] = (unsigned char)(source >> 0);
	dest[1] = (unsigned char)(source >> 8);
	return dest + sizeof(unsigned short);
}
#define copy_word_to_le_unaligned set_le_unaligned_word
#define SET_WORD                  set_le_unaligned_word

/*------------------------------------------------------------------------------------------*\
 * WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD
\*------------------------------------------------------------------------------------------*/
static inline void *set_be_unaligned_word(void *_dest, unsigned short source) {
	unsigned char *dest = (unsigned char *)_dest;
	dest[0] = (unsigned char)(source >> 8);
	dest[1] = (unsigned char)(source >> 0);
	return dest + sizeof(unsigned short);
}
#define copy_word_to_be_unaligned set_be_unaligned_word

/*------------------------------------------------------------------------------------------*\
 * WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD
\*------------------------------------------------------------------------------------------*/
static inline void *set_le_aligned_word(void *_dest, unsigned short source) {
	unsigned char *dest = (unsigned char *)_dest;
#if defined(ZUGRIFF_HOST_LE)
	*(unsigned short *)_dest = source;
#endif /*--- #if defined(ZUGRIFF_HOST_LE) ---*/
#if defined(ZUGRIFF_HOST_BE)
	dest[0] = (unsigned char)(source >> 0);
	dest[1] = (unsigned char)(source >> 8);
#endif /*--- #if defined(ZUGRIFF_HOST_BE) ---*/
	return dest + sizeof(unsigned short);
}
#define copy_word_to_le_aligned set_le_aligned_word

/*------------------------------------------------------------------------------------------*\
 * WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD
\*------------------------------------------------------------------------------------------*/
static inline unsigned short extract_le_unaligned_word(void *_source) {
	unsigned char *source = (unsigned char*)_source;
	return 
		((unsigned short)source[0] <<  0) | 
		((unsigned short)source[1] <<  8) | 
		0;
}
#define copy_word_from_le_unaligned        extract_le_unaligned_word
#define EXTRACT_WORD                       extract_le_unaligned_word

/*------------------------------------------------------------------------------------------*\
 * WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD
\*------------------------------------------------------------------------------------------*/
static inline unsigned short extract_be_unaligned_word(void *_source) {
	unsigned char *source = (unsigned char*)_source;
	return 
		((unsigned short)source[0] <<  8) | 
		((unsigned short)source[1] <<  0) | 
		0;
}
#define copy_word_from_be_unaligned        extract_be_unaligned_word

/*------------------------------------------------------------------------------------------*\
 * WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD
\*------------------------------------------------------------------------------------------*/
static inline unsigned short extract_le_aligned_word(void *_source) {
	unsigned char *source = (unsigned char*)_source;
	return 
		((unsigned short)source[0] <<  0) | 
		((unsigned short)source[1] <<  8) | 
		0;
}
#define copy_word_from_le_aligned        extract_le_aligned_word

/*------------------------------------------------------------------------------------------*\
 * WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD WORD
\*------------------------------------------------------------------------------------------*/
static inline unsigned short extract_unaligned_word(void *_source) {
	unsigned short _dest;
	unsigned char *source = (unsigned char*)_source;
	unsigned char *dest = (unsigned char *)&_dest;
	dest[0] = source[0];
	dest[1] = source[1];
	return _dest;
}
#define copy_word_from_unaligned        extract_unaligned_word

/*------------------------------------------------------------------------------------------*\
 * BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE
 * BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE
\*------------------------------------------------------------------------------------------*/
static inline void *set_unaligned_byte(void *_dest, unsigned char source) {
	unsigned char *dest = (unsigned char *)_dest;
	*dest = source;
	return dest + sizeof(unsigned char);
}
#define copy_byte_to_unaligned set_unaligned_byte

/*------------------------------------------------------------------------------------------*\
 * BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE
\*------------------------------------------------------------------------------------------*/
static inline void *set_le_unaligned_byte(void *_dest, unsigned char source) {
	unsigned char *dest = (unsigned char *)_dest;
	*dest = source;
	return dest + sizeof(unsigned char);
}
#define copy_byte_to_le_unaligned set_le_unaligned_byte
#define SET_BYTE                  set_le_unaligned_byte

/*------------------------------------------------------------------------------------------*\
 * BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE
\*------------------------------------------------------------------------------------------*/
static inline void *set_le_aligned_byte(void *_dest, unsigned char source) {
	unsigned char *dest = (unsigned char *)_dest;
	*dest = source;
	return dest + sizeof(unsigned char);
}
#define copy_byte_to_le_aligned set_le_aligned_byte

/*------------------------------------------------------------------------------------------*\
 * BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE
\*------------------------------------------------------------------------------------------*/
static inline unsigned char extract_le_unaligned_byte(void *_source) {
	unsigned char *source = (unsigned char*)_source;
	return *source;
}
#define copy_byte_from_le_unaligned        extract_le_unaligned_byte
#define EXTRACT_BYTE                       extract_le_unaligned_byte

/*------------------------------------------------------------------------------------------*\
 * BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE
\*------------------------------------------------------------------------------------------*/
static inline unsigned char extract_le_aligned_byte(void *_source) {
	unsigned char *source = (unsigned char*)_source;
	return *source;
}
#define copy_byte_from_le_aligned        extract_le_aligned_byte

/*------------------------------------------------------------------------------------------*\
 * BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE BYTE
\*------------------------------------------------------------------------------------------*/
static inline unsigned char extract_unaligned_byte(void *_source) {
	unsigned char *source = (unsigned char*)_source;
	return *source;
}
#define copy_byte_from_unaligned        extract_unaligned_byte

#endif /*--- #ifndef _LINUX_ZUGRIFF_H ---*/
