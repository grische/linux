/* SPDX-License-Identifier: GPL-2.0+ */

extern unsigned int E1Tx_OpenTransparent(void);
extern unsigned int E1Tx_CloseTransparent(void);
extern unsigned int E1Tx_SendTransparent(unsigned char *Buffer, unsigned int BufferLength);
extern int Transparent_Init(void (*DataInd)(const unsigned char *buf, int len));
extern void Transparent_Deinit(void);
