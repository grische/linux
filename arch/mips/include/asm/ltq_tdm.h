#ifndef __ltq_tdm_h__
#define __ltq_tdm_h__

/**
 * enhanced avm-tdm-features needed by pcmlink
 */

int tdm_clk_enable(unsigned int enable);

/**
 * idx: 0  frame-irq
 *      1  pcm-irq
 */
int tdm_get_irqdata(unsigned int idx,
		    unsigned int *irq_nr, unsigned int *irq_type);
#endif
