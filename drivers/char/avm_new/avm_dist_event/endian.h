#ifndef _AVM_EVENT_ENDIAN_H_
#define _AVM_EVENT_ENDIAN_H_
#ifdef __cplusplus
extern "C" {
#endif

#include <linux/swab.h>
#include "avm_event_gen_endian_external_defines.h"

#ifdef __KERNEL__
#define __bswap_16(x) __swab16(x)
#define __bswap_32(x) __swab32(x)
#define __bswap_64(x) __swab64(x)
#endif

/*------------------------------------------------------------------------------------------*\
     * Defines
    \*------------------------------------------------------------------------------------------*/
#if defined(__LITTLE_ENDIAN)
#define MACHINE_IS_LITTLE_ENDIAN
#else /*--- #if __BYTE_ORDER == __LITTLE_ENDIAN ---*/
#endif /*--- #else ---*/ /*--- #if __BYTE_ORDER == __LITTLE_ENDIAN ---*/

typedef struct sublenFieldDescr {
	struct _endian_convert *C;

	const unsigned char *in;
	unsigned char *out;
	uint32_t length;
} sublenFieldDescr_t;

int32_t convert_fromBigEndian_toMachine(uint32_t, struct _endian_convert *,
					const unsigned char *, unsigned char *,
					uint32_t);
int32_t convert_fromLittleEndian_toMachine(uint32_t, struct _endian_convert *,
					   const unsigned char *,
					   unsigned char *, uint32_t);
int32_t convert_fromMachine_toBigEndian(uint32_t, struct _endian_convert *,
					const unsigned char *, unsigned char *,
					uint32_t);
int32_t convert_fromMachine_toLittleEndian(uint32_t, struct _endian_convert *,
					   const unsigned char *,
					   unsigned char *, uint32_t);
int32_t convert_fromMachine_toMachine(uint32_t, struct _endian_convert *,
				      const unsigned char *, unsigned char *,
				      uint32_t);

#ifdef __cplusplus
}
#endif
#endif /*--- #ifndef _AVM_EVENT_ENDIAN_H_ ---*/
