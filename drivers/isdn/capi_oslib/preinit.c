/*-------------------------------------------------##########################*\
 * Modul : preinit.c  created at  Fri  05-May-1995  13:23:52 by jochen
 * $Id: preinit.c 1.2 2002/11/18 14:43:18Z mpommerenke Exp $
 * $Log: preinit.c $
 * Revision 1.2  2002/11/18 14:43:18Z  mpommerenke
 * Revision 1.1  2001/07/04 15:33:44Z  MPommerenke
 * Initial revision
 * Revision 1.1  2001/07/04 11:04:47Z  MPommerenke
 * Initial revision
 * Revision 1.1  2001/06/11 12:47:14Z  MPommerenke
 * Initial revision
 * Revision 1.2  1999/11/23 11:48:23Z  MPommerenke
 * - MaPom: neue oslib
 * Revision 1.1  1999/07/23 08:37:25Z  MPommerenke
 * Initial revision
 * Revision 1.1  1998/11/10 13:43:44  DFriedel
 * Initial revision
 * Revision 1.1  1997/04/08 17:00:42  JSchieberlein.sw.avm
 * Initial revision
 * Revision 1.2  1995/12/13 11:07:31  JSchieberlein.sw.avm
 * CAPI20, neuer Release Mechanismus
 * Revision 1.1  1995/05/10 13:36:02  JOSH
 * Initial revision
 * Revision 1.1  95/05/05 15:20:00  jochen
 * Initial revision
\*---------------------------------------------------------------------------*/
#include "string.h"
#include "preinit.h"
#include <memory.h>
#include "monitor.h"

char *PREINIT_Params = "\0\0\0\0"; /*----- cmd == 1 --###*/

typedef enum {
	wait_for_cmd,
	wait_for_len,
	wait_for_params
} _state;

/*-----------------------------##########################*/
void PREINIT(unsigned long data) {

	static _state state = wait_for_cmd;
	static char **store = 0;
	static unsigned long len = 0;
	static int anz = 0;

	switch (state) {
		/*-----------------------------##########################*/
		case wait_for_cmd:
			if (data == 1)
				store = &PREINIT_Params;
			else
				store = 0;
			state = wait_for_len;
			break;
		/*-----------------------------##########################*/
		case wait_for_len:
			anz = 0;
			len = (int)data;
			if (store != 0) {
				*store = (char *)malloc((unsigned)len+4);
				if(*store) {
					memset(*store, 0, (unsigned)len+4);
				}
			}
			state = wait_for_params;
			break;
		/*-----------------------------##########################*/
		case wait_for_params:
			if (store != 0)
				memcpy ((*store)+anz, &data, 4);
			anz += 4;
			if (anz >= len)
				state = wait_for_cmd;
			break;
	}
}
