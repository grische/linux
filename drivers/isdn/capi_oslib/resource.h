/*--------------------------------------------------------------------------------------*\

	$Header: //avm-fs01/entwicklung/basis/generierung/arm-runtime/oslib/include/rcs/resource.h 1.1 2001/07/04 15:34:11Z MPommerenke Exp $

	$Id: resource.h 1.1 2001/07/04 15:34:11Z MPommerenke Exp $

	$Log: resource.h $
	Revision 1.1  2001/07/04 15:34:11Z  MPommerenke
	Initial revision
	Revision 1.1  2001/07/04 11:05:11Z  MPommerenke
	Initial revision
	Revision 1.1  2001/06/11 12:47:55Z  MPommerenke
	Initial revision
	Revision 1.1  2000/04/10 13:58:41Z  MPommerenke

\*-------------------------------------------------------------------------------------*/
#ifndef _RESOURCE_H_
#define _RESOURCE_H_

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
void Recource_Init(void);
unsigned int Resource_Alloc(unsigned int MaxPromille, unsigned int MinPromille);
void Resource_Free(unsigned int UsedPromille);


#endif /*--- #ifndef _RESOURCE_H_ ---*/
