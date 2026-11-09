/* SPDX-License-Identifier: GPL-2.0+ */
#ifndef _RESOURCE_H_
#define _RESOURCE_H_

/**
 */
void Recource_Init(void);
unsigned int Resource_Alloc(unsigned int MaxPromille, unsigned int MinPromille);
void Resource_Free(unsigned int UsedPromille);


#endif /*--- #ifndef _RESOURCE_H_ ---*/
