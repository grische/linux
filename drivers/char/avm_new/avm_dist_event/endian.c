#include <linux/kernel.h>
#include <linux/string.h>

#include "avm_event_gen_enum_range.h"
#include "avm_event_gen_types.h"
#include "avm_event_gen_endian_external_defines.h"
#include "avm_event_gen_endian.h"

#include "endian.h"

//#define ENDIAN_DEBUG

#ifdef UNIT_TEST
#define STATIC
#else /*--- #ifdef UNIT_TEST ---*/
#define STATIC static
#endif /*--- #else ---*/ /*--- #ifdef UNIT_TEST ---*/

/*------------------------------------------------------------------------------------------*\
 * Defines
\*------------------------------------------------------------------------------------------*/
#define DO_CHECK_AHA_MESSAGE
#if defined(__LITTLE_ENDIAN)
#define MACHINE_IS_LITTLE_ENDIAN

#define IS_LITTLE_ENDIAN 1
#else /*--- #if __BYTE_ORDER == __LITTLE_ENDIAN ---*/
#define IS_LITTLE_ENDIAN 0
#endif /*--- #else ---*/ /*--- #if __BYTE_ORDER == __LITTLE_ENDIAN ---*/

/*------------------------------------------------------------------------------------------*\
 * Prototypes
\*------------------------------------------------------------------------------------------*/
STATIC uint64_t one_entry_swap_from_machine(const unsigned char *in,
					    unsigned char *out,
					    uint32_t size) __attribute__ ((unused));
STATIC uint64_t one_entry_swap_to_machine(const unsigned char *in,
					  unsigned char *out,
					  uint32_t size) __attribute__ ((unused));
#ifdef DO_CHECK_AHA_MESSAGE
STATIC uint64_t one_entry_no_swap_just_check(const unsigned char *in,
					     unsigned char *out,
					     uint32_t size) __attribute__ ((unused));

/*--------------------------------------------------------------------------------*\
 * just resolve unaligned access from compiler
\*--------------------------------------------------------------------------------*/
union __p_access {
	uint8_t  bval;
	uint16_t sval __attribute__((packed));
	uint32_t dval __attribute__((packed));
	uint64_t qval __attribute__((packed));
};
/*------------------------------------------------------------------------------------------*\
 * Private methods
\*------------------------------------------------------------------------------------------*/
STATIC uint64_t one_entry_no_swap_just_check(const unsigned char *in,
					     unsigned char *out, uint32_t size)
{
	union __p_access *pin, *pout;
	uint64_t value = 0;

	if (!size)
		return 0;

	pin  = (union __p_access *)in;
	pout = (union __p_access *)out;

	// normale Wandlung
	switch (size) {
	case 1:
		value = pout->bval = pin->bval;
		break;
	case 2:
		value = pout->sval = pin->sval;
		break;
	case 4:
		value = pout->dval = pin->dval;
		break;
	case 8:
		value = pout->qval = pin->qval;
		break;

	default:
		// maybe a char array -> just copy
		memcpy(out, in, size);
		break;
	}

	return value;
}
#endif /*--- #ifdef DO_CHECK_AHA_MESSAGE ---*/

/*
 * Convert one single value.
 */
STATIC uint64_t one_entry_swap_from_machine(const unsigned char *in,
					    unsigned char *out, uint32_t size)
{
	union __p_access *pin, *pout;
	uint64_t value = 0;

	if (!size)
		return 0;

	pin  = (union __p_access *)in;
	pout = (union __p_access *)out;

	// normale Wandlung
	switch (size) {
	case 1:
		value = pout->bval = pin->bval;
		break;
	case 2:
		pout->sval = __bswap_16(pin->sval);
		value = pin->sval;
		break;
	case 4:
		pout->dval = __bswap_32(pin->dval);
		value = pin->dval;
		break;
	case 8:
		pout->qval = __bswap_64(pin->qval);
		value = pin->qval;
		break;

	default:
		// maybe a char array -> just copy
		memcpy(out, in, size);
		break;
	}

	return value;
}

/*
 * Convert one single value.
 *
 * @size: Groesse des Feldes (wenn hoechstes Bit gesetzt ist -> Bitfeld)
 */
STATIC uint64_t one_entry_swap_to_machine(const unsigned char *in,
					  unsigned char *out, uint32_t size)
{
	union __p_access *pin, *pout;
	uint64_t value = 0;

	if (!size)
		return 0;

	pin  = (union __p_access *)in;
	pout = (union __p_access *)out;

	// normale Wandlung
	switch (size) {
	case 1:
		value = pout->bval = pin->bval;
		break;
	case 2:
		pout->sval = __bswap_16(pin->sval);
		value = pout->sval;
		break;
	case 4:
		pout->dval = __bswap_32(pin->dval);
		value = pout->dval;
		break;
	case 8:
		pout->qval = __bswap_64(pin->qval);
		value = pout->qval;
		break;

	default:
		// maybe a char array -> just copy
		memcpy(out, in, size);
		break;
	}

	return value;
}

/*
 * Convert one single value.
 *
 * @size: Groesse des Feldes (wenn hoechstes Bit gesetzt ist -> Bitfeld)
 */
STATIC uint64_t one_entry_do_nothing(const unsigned char *in,
				     unsigned char *out,
				     uint32_t size)
{
	union __p_access *pin, *pout;
	uint64_t value = 0;

	if (!size)
		return 0;

	pin  = (union __p_access *)in;
	pout = (union __p_access *)out;

	// normale Wandlung
	switch (size) {
	case 1:
		value = pin->bval;
		break;
	case 2:
		value = pin->sval;
		break;
	case 4:
		value = pin->dval;
		break;
	case 8:
		value = pin->qval;
		break;

	default:
		// maybe a char array -> just copy
		memcpy(out, in, size);
		break;
	}

	return value;
}

struct convert_version_info {
	uint32_t in_version;
	uint32_t out_version;
};

typedef struct sublen {
	int32_t err;
	uint32_t converted_bytes;
	int32_t inCorr;
	int32_t outCorr;
} sublength_t;
sublength_t errReturn = { -1, 0, 0, 0 };

/**
 * Diese Funktion schreibt eine korrigierte Laenge der Nachricht (falls API-Unterschiede ausgeschlossen oder Defaults eingefuegt werden)
 */
STATIC void write_subLen(sublenFieldDescr_t *descr,
			 uint64_t (*func)(const unsigned char *in,
					  unsigned char *out, uint32_t size),
			 int8_t isLittleEndian)
{
	if (!descr->C)
		return;
	if (descr->length == 0xffffffff)
		return;

	//    ALOG_SIMPLE_TRACE("%d: {%s} length=%d, out=0x%x\n", __LINE__, descr->C->name, descr->length, *(uint32_t*)descr->out);

	if (descr->C->bits) {
		// out is machine format?
		if (func == one_entry_swap_from_machine) {
			if (isLittleEndian) {
				// TODO: diese Code ist eigentlich nicht richtig und funktioniert nur zufällig, da das Längenfeld (in Verbindung mit der gedrehten struct-Beschreibung) 16Bit aligned ist
				*(uint32_t *)descr->out =
					(*(uint32_t *)descr->out &
					 ~(((1 << descr->C->bits) - 1)
					   << descr->C->bit_offset)) // Löschen des alten Wertes
					|
					(__bswap_32(
						descr->length &
						((1 << descr->C->bits) -
						 1))); // Setzen des neuen Wertes
			} else {
				*(uint32_t *)descr->out =
					(*(uint32_t *)descr->out &
					 ~(((1 << descr->C->bits) - 1)
					   << (descr->C->bit_offset -
					       descr->C->bits))) // Löschen des alten Wertes
					|
					((descr->length &
					  ((1 << descr->C->bits) - 1))
					 << (descr->C->bit_offset -
					     descr->C->bits)); // Setzen des neuen Wertes
			}
		} else if (func == one_entry_swap_to_machine) {
			if (isLittleEndian) {
				*(uint32_t *)descr->out =
					(*(uint32_t *)descr->out &
					 ~(((1 << descr->C->bits) - 1)
					   << (descr->C->bit_offset -
					       descr->C->bits))) // Löschen des alten Wertes
					|
					((descr->length &
					  ((1 << descr->C->bits) - 1))
					 << (descr->C->bit_offset -
					     descr->C->bits)); // Setzen des neuen Wertes
			} else {
				*(uint32_t *)descr->out =
					(*(uint32_t *)descr->out &
					 ~(((1 << descr->C->bits) - 1)
					   << (descr->C->bit_offset -
					       descr->C->bits))) // Löschen des alten Wertes
					|
					((descr->length &
					  ((1 << descr->C->bits) - 1))
					 << (descr->C->bit_offset -
					     descr->C->bits)); // Setzen des neuen Wertes
			}
		} else {
			if (isLittleEndian) {
				*(uint32_t *)descr->out =
					(*(uint32_t *)descr->out &
					 ~(((1 << descr->C->bits) - 1)
					   << (descr->C->bit_offset -
					       descr->C->bits))) // Löschen des alten Wertes
					|
					((descr->length &
					  ((1 << descr->C->bits) - 1))
					 << (descr->C->bit_offset -
					     descr->C->bits)); // Setzen des neuen Wertes
			} else {
				*(uint32_t *)descr->out =
					(*(uint32_t *)descr->out &
					 ~(((1 << descr->C->bits) - 1)
					   << (descr->C->bit_offset -
					       descr->C->bits))) // Löschen des alten Wertes
					|
					((descr->length &
					  ((1 << descr->C->bits) - 1))
					 << (descr->C->bit_offset -
					     descr->C->bits)); // Setzen des neuen Wertes
			}
		}
	} else {
		// out is machine format?
		if (func == one_entry_swap_from_machine) {
			if (isLittleEndian) {
				func((((unsigned char *)&descr->length)),
				     descr->out, descr->C->size);
			} else {
				func((((unsigned char *)&descr->length) +
				      (4 - descr->C->size)),
				     descr->out, descr->C->size);
			}
		} else if (func == one_entry_swap_to_machine) {
			if (isLittleEndian) {
				one_entry_no_swap_just_check(
					(((unsigned char *)&descr->length)),
					descr->out, descr->C->size);
			} else {
				one_entry_no_swap_just_check(
					(((unsigned char *)&descr->length) +
					 (4 - descr->C->size)),
					descr->out, descr->C->size);
			}
		} else {
			if (isLittleEndian) {
				func((unsigned char *)&descr->length,
				     descr->out, descr->C->size);
			} else {
				func((((unsigned char *)&descr->length) +
				      (4 - descr->C->size)),
				     descr->out, descr->C->size);
			}
		}
	}

	//    ALOG_SIMPLE_TRACE("%d: {%s} length=%d, out=0x%x\n", __LINE__, descr->C->name, descr->length, *(uint32_t*)descr->out);
}

/*
 * Convert the given 'in' structure into big/little endian.
 *
 * @func: converter function
 */
STATIC sublength_t convert_endian(
	uint64_t (*func)(const unsigned char *in, unsigned char *out,
			 uint32_t size),
	uint32_t select_value, int32_t bytes_to_convert,
	struct _endian_convert *C, const unsigned char *in, unsigned char *out,
	struct convert_version_info version_info, uint32_t level)
{
	//-----
	sublength_t new_length = {
		.err = 0, .converted_bytes = 0, .inCorr = 0, .outCorr = 0
	};
	unsigned char bitfield_count = 0;
	uint32_t current_value = 0;

	uint32_t array_element_size = 0;
	uint32_t array_element_anzahl = 0;
	uint32_t tmp;
	//char *entry_name = C->struct_name;
	//uint32_t entry_len = bytes_to_convert;
	struct _endian_convert *C_fromENDIAN_CONVERT_SELECT = NULL;

	sublenFieldDescr_t this_layer_totallength_C = {
		.C = NULL, .out = NULL, .length = 0
	}; // Feld auf dieser Ebene in das eine Sub-Länge eingetragen werden soll
	sublenFieldDescr_t this_layer_sublength_C = {
		.C = NULL, .out = NULL, .length = 0
	}; // Feld auf dieser Ebene in das eine Sub-Länge eingetragen werden soll
	sublenFieldDescr_t array_element_anzahl_field_C __attribute__ ((unused)) = {
		.C = NULL, .out = NULL, .length = 0
	}; // Feld auf dieser Ebene in das eine korrigiert Array-Elemente-Anzahl eingetragen werden soll

	this_layer_totallength_C.length = bytes_to_convert;
	while (bytes_to_convert > 0) {
		sublength_t sub_length = { .err = 0,
					   .converted_bytes = 0,
					   .inCorr = 0,
					   .outCorr = 0 };

		// falls die out-Version kleiner ist, kann das Feld ignoriert werden -> Wert wird verworfen
		if (C->ab_version &&
		    (C->ab_version > version_info.out_version)) {
#ifdef ENDIAN_DEBUG
			printk(KERN_ERR
			       "%02u:C(%p len:% 4d ofs:% 4d v:%u.%u %28s) ptr(!! IGNORE !! i:%p o:%p) corr->(i:% 4d o:% 4d) v(i:%u.%u o:%u.%u) toGo:%4u\n",
			       level, C, C->size, C->offset,
			       AVM_DIST_EVENT_VERSION__GET_MAJOR(C->ab_version),
			       AVM_DIST_EVENT_VERSION__GET_MINOR(C->ab_version),
			       C->name, in, out, new_length.inCorr,
			       new_length.outCorr,
			       AVM_DIST_EVENT_VERSION__GET_MAJOR(
				       version_info.in_version),
			       AVM_DIST_EVENT_VERSION__GET_MINOR(
				       version_info.in_version),
			       AVM_DIST_EVENT_VERSION__GET_MAJOR(
				       version_info.out_version),
			       AVM_DIST_EVENT_VERSION__GET_MINOR(
				       version_info.out_version),
			       bytes_to_convert);
#endif /*--- #ifdef ENDIAN_DEBUG ---*/

			// check if there are unconverted bitfields
			if (bitfield_count) {
				uint32_t last_length =
					C->offset - (C - 1)->offset;

				if (bitfield_count) {
					func(in + (C - 1)->offset +
						     new_length.inCorr,
					     out + (C - 1)->offset +
						     new_length.outCorr,
					     bitfield_count / 8);
					bitfield_count = 0;
				}

				bytes_to_convert -= last_length;
				new_length.converted_bytes += last_length;
			}

			if (C->size == 0 && C->substruct) {
				// kann passieren, da in unions keine Groeße angegeben ist -> dann m[ssen wir sie halt herausfinden
				sub_length = convert_endian(
					one_entry_do_nothing, select_value,
					bytes_to_convert, C->substruct,
					in + C->offset + new_length.inCorr,
					out + C->offset + new_length.outCorr,
					version_info, level + 1);

				if (sub_length.err == -1) {
					return errReturn;
				}

				bytes_to_convert -= sub_length.converted_bytes;
				new_length.converted_bytes +=
					sub_length.converted_bytes;
				new_length.outCorr -=
					sub_length.converted_bytes;
			} else {
				bytes_to_convert -= C->size;
				new_length.converted_bytes += C->size;
				new_length.outCorr -= C->size;
			}

			if ((C->flags & ENDIAN_CONVERT_LAST)) {
				break;
			}
			C++;
			continue;
		}

		// Feld konvertieren
		if (C->flags & ENDIAN_CONVERT_IGNORE) {
			// Feld ignorieren
		} else if (!(C->substruct) &&
			   C->flags & ENDIAN_CONVERT_ARRAY_USE) {
			// Bei Arrays ueberspringen wir die Konvertierung des aktuellen Feldes,
			// denn das passiert ja spaeter bei der Arraybehandlung.
			// Fuer Substrukturen hingegen ist das zulaessig, weil bei denen keine
			// .size hinterlegt ist (=0) sodass bytes_to_convert nicht veraendert wird
		} else {
			if (C->bits) {
				bitfield_count += C->bits;

				// do not convert, but get value if its a select type
				current_value =
					func(in + C->offset + new_length.inCorr,
					     (unsigned char *)&tmp, 4);
				current_value =
					(current_value >>
					 (32 - C->bits - C->bit_offset)) &
					((1 << C->bits) - 1);

				if (bitfield_count >= 32) {
					func(in + C->offset + new_length.inCorr,
					     out + C->offset +
						     new_length.outCorr,
					     4);
					bitfield_count -= 32;

					new_length.converted_bytes += 4;
					bytes_to_convert -= 4;
				}
			} else {
				// zuerst ein noch ausstehendes Bitfeld konvertieren?
				if (bitfield_count) {
					uint32_t last_length =
						C->offset - (C - 1)->offset;

					if (bitfield_count) {
						func(in + (C - 1)->offset +
							     new_length.inCorr,
						     out + (C - 1)->offset +
							     new_length.outCorr,
						     bitfield_count / 8);
						bitfield_count = 0;
					}

					bytes_to_convert -= last_length;
					new_length.converted_bytes +=
						last_length;
				}

				// Feld existiert nicht im in? -> Default-Wert kopieren
				if (C->ab_version &&
				    (C->ab_version > version_info.in_version)) {
					current_value = C->default_value;
					new_length.inCorr -= C->size;

					memcpy(out + C->offset +
						       new_length.outCorr,
					       &current_value, C->size);
					bytes_to_convert += C->size;
				} else {
					current_value =
						func(in + C->offset +
							     new_length.inCorr,
						     out + C->offset +
							     new_length.outCorr,
						     C->size);
				}

				bytes_to_convert -= C->size;
				new_length.converted_bytes += C->size;
			}

			if (C->flags & ENDIAN_CONVERT_SUB_LENGTH) {
				this_layer_sublength_C.C = C;
				this_layer_sublength_C.in =
					in + C->offset + new_length.inCorr;
				this_layer_sublength_C.out =
					out + C->offset + new_length.outCorr;
			}
			if (C->flags & ENDIAN_CONVERT_TOTAL_LENGTH) {
				this_layer_totallength_C.C = C;
				this_layer_totallength_C.in =
					in + C->offset + new_length.inCorr;
				this_layer_totallength_C.out =
					out + C->offset + new_length.outCorr;
			}
			if (C->flags & ENDIAN_CONVERT_ARRAY_ELEMENT_ANZAHL) {
				array_element_anzahl_field_C.C = C;
				array_element_anzahl_field_C.in =
					in + C->offset + new_length.inCorr;
				array_element_anzahl_field_C.out =
					out + C->offset + new_length.outCorr;
				array_element_anzahl_field_C.length =
					0xffffffff;
			}
		}

		if (bytes_to_convert < 0) {
			//fprintf(stderr, "[%s] fatal error: bytes_to_convert (%d) < 0\n", __FUNCTION__, bytes_to_convert);
			return errReturn;
		}

#ifdef ENDIAN_DEBUG
		printk(KERN_ERR
		       "%02u:C(%p len:% 4d ofs:% 4d v:%u.%u %28s) ptr(v:0x%08x i:%p o:%p) corr->(i:% 4d o:% 4d) v(i:%u.%u o:%u.%u) toGo:%4u\n",
		       level, C, C->size, C->offset,
		       AVM_DIST_EVENT_VERSION__GET_MAJOR(C->ab_version),
		       AVM_DIST_EVENT_VERSION__GET_MINOR(C->ab_version),
		       C->name, current_value, in, out, new_length.inCorr,
		       new_length.outCorr,
		       AVM_DIST_EVENT_VERSION__GET_MAJOR(
			       version_info.in_version),
		       AVM_DIST_EVENT_VERSION__GET_MINOR(
			       version_info.in_version),
		       AVM_DIST_EVENT_VERSION__GET_MAJOR(
			       version_info.out_version),
		       AVM_DIST_EVENT_VERSION__GET_MINOR(
			       version_info.out_version),
		       bytes_to_convert);
#endif /*--- #ifdef ENDIAN_DEBUG ---*/

		// Select-Wert?
		if (C->flags & ENDIAN_CONVERT_SELECT) {
			select_value = current_value;
			C_fromENDIAN_CONVERT_SELECT = C;
		}

		// ist es ein Enum? -> range check
		// Anmerkungen:
		// 1. unions können auch eine enum-check-Fkt besitzen, aber der zu prüfende Wert ist dann der select_value
		// 2. IN + OUT-Version testen
		if (C->enum_check_function &&
		    C->enum_check_function((C->flags &
					    ENDIAN_CONVERT_SELECT_ENTRY) ?
						   select_value :
						   current_value,
					   version_info.in_version)) {
			//            fprintf(stderr, "[%s] fatal error: select value out of range (%d), thread=%s\n", __FUNCTION__, current_value, netLibMisc_getThreadName());
			return errReturn;
		}
		if (C->enum_check_function &&
		    C->enum_check_function((C->flags &
					    ENDIAN_CONVERT_SELECT_ENTRY) ?
						   select_value :
						   current_value,
					   version_info.out_version)) {
			//            fprintf(stderr, "[%s] fatal error: select value out of range (%d), thread=%s\n", __FUNCTION__, current_value, netLibMisc_getThreadName());
			return errReturn;
		}

		// Array-Behandlung
		if (C->flags & ENDIAN_CONVERT_ARRAY_ELEMENT_SIZE) {
			array_element_size = current_value;
		}
		if (C->flags & ENDIAN_CONVERT_ARRAY_ELEMENT_ANZAHL) {
			array_element_anzahl = current_value;
		}

		// Ab in tiefere Ebenen -> substrukturen auswerten
		{
			struct _endian_convert *sub = NULL;

			if (C->flags & ENDIAN_CONVERT_SELECT_ENTRY) {
				sub = &C->substruct[select_value];
			} else if (C->substruct &&
				   !(C->flags & ENDIAN_CONVERT_ARRAY_USE)) {
				sub = C->substruct;
			}

			if (sub) {
				sub_length = convert_endian(
					func, select_value, bytes_to_convert,
					sub, in + C->offset + new_length.inCorr,
					out + C->offset + new_length.outCorr,
					version_info, level + 1);

				if (sub_length.err <
				    0) { /*--- fataler message fehler ---*/
					//fprintf(stderr, "[%s] fatal error: process substruct failed.\n\tat field: '%s'\n\tin struct: '%s'\n\tglobal_len: %d (before process)\n",
					//                __FUNCTION__, C->name, C->struct_name, bytes_to_convert);

					return sub_length;
				}
				if ((int32_t)sub_length.converted_bytes >
				    (bytes_to_convert + (-sub_length.inCorr))) {
					//fprintf(stderr, "[%s] fatal error: sub_length (bytes=%d, inCorr=%d, outCorr=%d) > bytes_to_convert (%d), name=%s\n", __FUNCTION__,
					//        sub_length.converted_bytes, sub_length.inCorr, sub_length.outCorr, bytes_to_convert, C->name);
					return errReturn;
				}

				bytes_to_convert -= sub_length.converted_bytes;
				new_length.converted_bytes +=
					sub_length.converted_bytes;
				new_length.inCorr += sub_length.inCorr;
				new_length.outCorr += sub_length.outCorr;
				this_layer_sublength_C.length +=
					sub_length.converted_bytes +
					sub_length.outCorr;
				this_layer_totallength_C.length +=
					sub_length.outCorr;

				if (sub->ab_version >
					    version_info.out_version &&
				    C_fromENDIAN_CONVERT_SELECT) {
					// dieser Eintrag wurde ignoriert, und wenn es ein union ist, muss der Type-Eintrag (der der diese union aussucht) auch ignoriert werden und auch alle anderen Felder dieser Ebene.
					new_length.outCorr -=
						(new_length.converted_bytes +
						 new_length.outCorr);
					C_fromENDIAN_CONVERT_SELECT = NULL;
				}
			}

			// Array-Spielereien
			if (C->flags & ENDIAN_CONVERT_ARRAY_USE) {
				uint32_t corrected_array_element_anzahl;
				uint32_t correct_array_element_anzahl;

				/*--- fprintf(stderr, "[%s] ENDIAN_CONVERT_ARRAY_USE: element size %d element_anzahl %d\n", __FUNCTION__, array_element_size, array_element_anzahl); ---*/

				if (array_element_size == 0) {
					array_element_size =
						C->size; // ist wohl null...
				}
				/*--- fprintf(stderr, "[%s] array_element_size=%d\n", __func__, array_element_size); ---*/

				corrected_array_element_anzahl =
					array_element_anzahl;
				correct_array_element_anzahl =
					array_element_anzahl;
				while (array_element_anzahl) {
					/*--- fprintf(stderr, "[%s:%d] array_element_anzahl=%d\n", __func__, __LINE__, array_element_anzahl); ---*/
					if (C->substruct) {
						sub_length = convert_endian(
							func, select_value,
							bytes_to_convert,
							C->substruct,
							in + C->offset +
								new_length
									.inCorr,
							out + C->offset +
								new_length
									.outCorr,
							version_info,
							level + 1);
						if (sub_length.err <
						    0) { /*--- fataler message fehler ---*/
							return sub_length;
						}

						array_element_size =
							sub_length
								.converted_bytes;

						if (sub_length.outCorr < 0) {
							//                            ALOG_SIMPLE_TRACE("%d: sub_length.outCorr=%d\n", __LINE__, sub_length.outCorr);
							corrected_array_element_anzahl--;
						}
					} else {
						func(in + C->offset +
							     new_length.inCorr,
						     out + C->offset +
							     new_length.outCorr,
						     array_element_size);
					}
					/*--- fprintf(stderr, "[%s:%d] array_element_size=%d, bytes_to_convert=%d, length=%d\n", __func__, __LINE__, array_element_size, bytes_to_convert, length); ---*/

					in += array_element_size;
					out += array_element_size;

					if (bytes_to_convert <
					    (int32_t)array_element_size) {
						//ACHTUNG: beim temp_master konvertieren von "alter" API auf "neuer" API (mit AIN-string) läuft man in bestimmten Fällen(z.B. JanL Testconfig) in diesen Fall
						//fprintf(stderr, "[%s] fatal error: array length (rest: %d) > bytes_to_convert (%d)\n", __FUNCTION__, array_element_anzahl, bytes_to_convert);
						return errReturn;
					}
					array_element_anzahl--;
					bytes_to_convert -= array_element_size;
					new_length.converted_bytes +=
						array_element_size;
					new_length.inCorr += sub_length.inCorr;
					new_length.outCorr +=
						sub_length.outCorr;
					this_layer_sublength_C.length +=
						array_element_size;
				}

				// korrigierte anzahl der Array-Elemente in das entsprechende Feld eintragen
				if (corrected_array_element_anzahl !=
				    correct_array_element_anzahl) {
					array_element_anzahl_field_C.length =
						corrected_array_element_anzahl;
				} else {
					array_element_anzahl_field_C.C = NULL;
				}

				if ((C->flags & ENDIAN_CONVERT_LAST)) {
					break;
				}
				C++;
			} else if (C->flags & ENDIAN_CONVERT_REPEAT) {
				in += (sub_length.converted_bytes + C->size);
				out += (sub_length.converted_bytes + C->size);
				this_layer_sublength_C.length +=
					array_element_size;

				if (array_element_anzahl) {
					/*--- fprintf(stderr, "[%s] ENDIAN_CONVERT_REPEAT: element_anzahl %d\n", __FUNCTION__, array_element_anzahl); ---*/
					/*--- array element anzahl kann genutzt werden um die Array anzahl zu begrenzen */
					array_element_anzahl--;

					if (array_element_anzahl == 0) {
						if ((C->flags &
						     ENDIAN_CONVERT_LAST)) {
							break;
						}
						C++;
					}
				}
			} else {
				if ((C->flags & ENDIAN_CONVERT_LAST)) {
					break;
				}
				C++;
			}
		}
	}

	if (!(C->flags & ENDIAN_CONVERT_LAST)) {
		//fprintf(stderr, "[%s] fatal error: sudden end of message,\n"
		//                "\tat field: '%s'\n"
		//                "\tin struct: '%s'\n"
		//                "\tin entry struct: '%s'\n"
		//                "\tglobal_len: %d\n"
		//                "\tentry_len: %d\n"
		//                "\tlevel: %u\n"
		//                "\tthread: %s\n",
		//        __FUNCTION__, C->name, C->struct_name, entry_name, bytes_to_convert, entry_len, level, netLibMisc_getThreadName());
		return errReturn;
	}

	// write corrected size fields
	write_subLen(&this_layer_sublength_C, func, IS_LITTLE_ENDIAN);
	write_subLen(&this_layer_totallength_C, func, IS_LITTLE_ENDIAN);
	write_subLen(&array_element_anzahl_field_C, func, IS_LITTLE_ENDIAN);

	return new_length;
}

/*------------------------------------------------------------------------------------------*\
 * Public
\*------------------------------------------------------------------------------------------*/
int32_t convert_fromBigEndian_toMachine(uint32_t bytes_to_convert,
					struct _endian_convert *C,
					const unsigned char *in,
					unsigned char *out,
					uint32_t version_from)
{
	sublength_t ret = {
		.err = 0, .converted_bytes = 0, .inCorr = 0, .outCorr = 0
	};
	struct convert_version_info version_info = {
		.in_version = version_from,
		.out_version = AVM_DIST_EVENT_VERSION
	};

#ifdef MACHINE_IS_LITTLE_ENDIAN
	ret = convert_endian(one_entry_swap_to_machine, 0, bytes_to_convert, C,
			     in, out, version_info, 0);
#else /*--- #ifdef MACHINE_IS_LITTLE_ENDIAN ---*/
#ifdef DO_CHECK_AHA_MESSAGE
	ret = convert_endian(one_entry_no_swap_just_check, 0, bytes_to_convert,
			     C, in, out, version_info, 0);
#else /*--- #ifdef DO_CHECK_AHA_MESSAGE ---*/
	if (in != out)
		memcpy(out, in, bytes_to_convert);
	ret.converted_bytes = bytes_to_convert;
#endif /*--- #else ---*/ /*--- #ifdef DO_CHECK_AHA_MESSAGE ---*/
#endif /*--- #else ---*/ /*--- #ifdef MACHINE_IS_LITTLE_ENDIAN ---*/

	return ret.converted_bytes + ret.outCorr;
}

int32_t convert_fromLittleEndian_toMachine(uint32_t bytes_to_convert,
					   struct _endian_convert *C,
					   const unsigned char *in,
					   unsigned char *out,
					   uint32_t version_from)
{
	sublength_t ret = {
		.err = 0, .converted_bytes = 0, .inCorr = 0, .outCorr = 0
	};
	struct convert_version_info version_info = {
		.in_version = version_from,
		.out_version = AVM_DIST_EVENT_VERSION
	};

#ifdef MACHINE_IS_LITTLE_ENDIAN
#ifdef DO_CHECK_AHA_MESSAGE
	ret = convert_endian(one_entry_no_swap_just_check, 0, bytes_to_convert,
			     C, in, out, version_info, 0);
#else /*--- #ifdef DO_CHECK_AHA_MESSAGE ---*/
	if (in != out)
		memcpy(out, in, bytes_to_convert);
	ret.converted_bytes = bytes_to_convert;
#endif /*--- #else ---*/ /*--- #ifdef DO_CHECK_AHA_MESSAGE ---*/
#else /*--- #ifdef MACHINE_IS_LITTLE_ENDIAN ---*/
	ret = convert_endian(one_entry_swap_to_machine, 0, bytes_to_convert, C,
			     in, out, version_info, 0);
#endif /*--- #else ---*/ /*--- #ifdef MACHINE_IS_LITTLE_ENDIAN ---*/

	return ret.converted_bytes + ret.outCorr;
}

int32_t convert_fromMachine_toBigEndian(uint32_t bytes_to_convert,
					struct _endian_convert *C,
					const unsigned char *in,
					unsigned char *out,
					uint32_t version_to)
{
	sublength_t ret = {
		.err = 0, .converted_bytes = 0, .inCorr = 0, .outCorr = 0
	};
	struct convert_version_info version_info = {
		.in_version = AVM_DIST_EVENT_VERSION, .out_version = version_to
	};

#ifdef MACHINE_IS_LITTLE_ENDIAN
	ret = convert_endian(one_entry_swap_from_machine, 0, bytes_to_convert,
			     C, in, out, version_info, 0);
#else /*--- #ifdef MACHINE_IS_LITTLE_ENDIAN ---*/
#ifdef DO_CHECK_AHA_MESSAGE
	ret = convert_endian(one_entry_no_swap_just_check, 0, bytes_to_convert,
			     C, in, out, version_info, 0);
#else /*--- #ifdef DO_CHECK_AHA_MESSAGE ---*/
	if (in != out)
		memcpy(out, in, bytes_to_convert);
	ret.converted_bytes = bytes_to_convert;
#endif /*--- #else ---*/ /*--- #ifdef DO_CHECK_AHA_MESSAGE ---*/
#endif /*--- #else ---*/ /*--- #ifdef MACHINE_IS_LITTLE_ENDIAN ---*/

	return ret.converted_bytes + ret.outCorr;
}

int32_t convert_fromMachine_toLittleEndian(uint32_t bytes_to_convert,
					   struct _endian_convert *C,
					   const unsigned char *in,
					   unsigned char *out,
					   uint32_t version_to)
{
	sublength_t ret = {
		.err = 0, .converted_bytes = 0, .inCorr = 0, .outCorr = 0
	};
	struct convert_version_info version_info = {
		.in_version = AVM_DIST_EVENT_VERSION, .out_version = version_to
	};

#ifdef MACHINE_IS_LITTLE_ENDIAN
#ifdef DO_CHECK_AHA_MESSAGE
	ret = convert_endian(one_entry_no_swap_just_check, 0, bytes_to_convert,
			     C, in, out, version_info, 0);
#else /*--- #ifdef DO_CHECK_AHA_MESSAGE ---*/
	if (in != out)
		memcpy(out, in, bytes_to_convert);
	ret.converted_bytes = bytes_to_convert;
#endif /*--- #else ---*/ /*--- #ifdef DO_CHECK_AHA_MESSAGE ---*/
#else /*--- #ifdef MACHINE_IS_LITTLE_ENDIAN ---*/
	ret = convert_endian(one_entry_swap_from_machine, 0, bytes_to_convert,
			     C, in, out, version_info, 0);
#endif /*--- #else ---*/ /*--- #ifdef MACHINE_IS_LITTLE_ENDIAN ---*/

	return ret.converted_bytes + ret.outCorr;
}

int32_t convert_fromMachine_toMachine(uint32_t bytes_to_convert,
				      struct _endian_convert *C,
				      const unsigned char *in,
				      unsigned char *out,
				      uint32_t version_from)
{
	sublength_t ret = {
		.err = 0, .converted_bytes = 0, .inCorr = 0, .outCorr = 0
	};
	struct convert_version_info version_info = {
		.in_version = version_from,
		.out_version = AVM_DIST_EVENT_VERSION
	};

	ret = convert_endian(one_entry_no_swap_just_check, 0, bytes_to_convert,
			     C, in, out, version_info, 0);

	return ret.converted_bytes + ret.outCorr;
}

#ifdef UNIT_TEST
int32_t convert_fromMachine_toMachine_chooseAPI(
	uint32_t bytes_to_convert,
	struct _endian_convert *C,
	const unsigned char *in, unsigned char *out,
	uint32_t version_from, uint32_t version_to)
{
	sublength_t ret = {
		.err = 0, .converted_bytes = 0, .inCorr = 0, .outCorr = 0
	};
	struct convert_version_info version_info = { .in_version = version_from,
						     .out_version =
							     version_to };

	ret = convert_endian(one_entry_no_swap_just_check, 0, bytes_to_convert,
			     C, in, out, version_info, 0);

	return ret.converted_bytes + ret.outCorr;
}
#endif /*--- #ifdef UNIT_TEST ---*/
