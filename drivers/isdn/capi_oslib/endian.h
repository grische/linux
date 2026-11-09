/* File: endian.h
 * Author: André Raupach
 * Beschreibung: Diese Datei beherbergt Funktionen um ein Datum von Little-Endian in den
 *               machine-Endian Typ zu konvertieren und andersherum.
 *
 * History:
 *
 *  Name                Date            Beschreibung
 *  André Raupach       13.10.2008      Anlegen der Datei und schreiben der ersten konvertierungsfunktionen
 *                                      sowie deren Makros.
 *  */
#ifdef __BE_ENDIAN
#define li_set(x, y)    __machine_to_lien((unsigned long int*)(x), sizeof(x), &(y))
#define li_get(x)       (typeof(x))__lien_to_machine_2(&(x), sizeof(x))
#else
#define li_set(x, y)    x
#define li_get(x)       x
#endif

/* Name: __lien_to_machine_2
 *
 * Beschreibung: Die Funktion konvertiert Little-Endian abgelegte Daten in den Endian-Typ der aktuellen machine
 *
 * Parameter:   ptr - Pointer auf Datum welches Little-Endian ist und konvertiert werden soll
 *              byte_count - Ist die Anzahl der bytes des entsprechenden Typs des Datum das konvertiert werden soll
 *
 * Rückgabe:    ist ein long int in dem das konvertierte Datum enthalten ist.
 */
static inline unsigned long __lien_to_machine_2(void *ptr, unsigned int byte_count) 
{
	unsigned long value = 0;
	unsigned long tmp;
	unsigned char *cptr = ptr;  
	unsigned int counter;  
 
	for (counter = 0; counter < byte_count; counter++)  
	{  
		tmp = cptr[counter];
		value |= (tmp << ((counter & 0x07) << 3));  
	}  
	return value;  
}

/* Name: __machine_to_lien
 *
 * Beschreibung: Die Funktion konvertiert ein Datum des Endian-Typs der aktuellen machine in ein Little-Endian
 *
 * Parameter:   ptr - Pointer auf Speicher der das konvertierte Datum enthalten soll (Ziel der Konvertierung)
 *              value - Datum welches in Little-Endian konvertiert werden soll (Quelle der Konvertierung)
 *              byte_count - Ist die Anzahl der bytes des entsprechenden Typs des Datum das konvertiert werden soll (value)
 *
 * Rückgabe:    void*.
 */
static inline void* __machine_to_lien(unsigned long value, unsigned int byte_len, void *ptr) 
{
	unsigned int counter;  
	unsigned char tmp;
	unsigned char *cptr = ptr;
	
	for (counter = 0; counter < byte_len; counter++)  
	{  
		tmp = (value >> ((counter & 0x07) << 3)) & 0xFF;  
		cptr[counter] = tmp;  
	}
	return ptr;
}


/*static inline unsigned int __lien_to_machine_1(unsigned char *ptr, unsigned int byte_len) 
{
	unsigned int value = 0;

	switch(byte_len) 
	{
		case 1:
			value |= (*ptr >> 0);
			break;
		case 2:
			value |= (*ptr >> 0);
			value |= (*ptr >> 8);
			break;
		case 4:
			value |= (*ptr >> 0);
			value |= (*ptr >> 8);
			value |= (*ptr >> 16);
			value |= (*ptr >> 24);
			break;
		default:
			break;
	}
	return value;
}
*/
