#pragma once

/**
 * ngi_surrender_eip123() - Transfer eip123 control to interaptiv.
 *
 * Interaptiv needs permanent acces to the eip123 in order to use if for
 * cipher offloading. From a security perspective bootcore cannot use the
 * eip123 at the same time as the interaptiv because eip123 needs dma access
 * to host cpu buffers. The idea is that the bootcore uses the eip123
 * only for a short time to extract the keying material it needs from the
 * OTP and enough entropy and then stop using eip123 before even booting
 * the interaptiv.
 *
 * Calling this function accomplishes the following:
 *    * the eip123 can no longer access bootcore memory
 *    * interaptiv gains access to eip123 registers
 *    * the ngi config is fixed (sealed) and cannot be changed again
 *
 * These steps are irreversible because this function also prevents writing
 * to the protection registers for good (until next reset).
 *
 * Only has an effect when executed on the bootcore. A stub implementation
 * is provied for the interaptiv (see below).
 *
 * Context: Any Context
 */
#ifdef CONFIG_SOC_GRX500_BOOTCORE
void ngi_surrender_eip123(void);
#endif
