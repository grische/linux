/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
#ifndef __dynpll_h__
#define __dynpll_h__

int dynpll_init(void);
int dynpll_setfrequency(unsigned int freq);
int dynpll_showregister(void);

#define XTAL_FREQUENCY      25000000
#define STD_FREQUENCY       25000000
#define DYNPLL_GPIO_S0      10
#define DYNPLL_GPIO_SDATA   12
#define DYNPLL_GPIO_SCLK     8
#endif/*--- #ifndef __dynpll_h__ ---*/
