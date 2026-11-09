/*-------------------------------------------------------------------------------------*\

	$Id: transparent.h 1.1 2004/04/21 12:39:43Z strevisany Exp $

	$Log: transparent.h $
	Revision 1.1  2004/04/21 12:39:43Z  strevisany
	Initial revision

\*-------------------------------------------------------------------------------------*/
extern unsigned int E1Tx_OpenTransparent(void);
extern unsigned int E1Tx_CloseTransparent(void);
extern unsigned int E1Tx_SendTransparent(unsigned char *Buffer, unsigned int BufferLength);
extern int Transparent_Init(void (*DataInd)(const unsigned char *, int));
extern void Transparent_Deinit(void);
