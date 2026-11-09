#ifndef _ARCH_ASM_PREALLOC_MEMORY_H_
#define _ARCH_ASM_PREALLOC_MEMORY_H_

/*
 * ==============================
 * Hinweis in eigener Sache:
 * =========================
 * Der unten folgende Block existiert auch in der Datei 'include/linux/env.h'.
 * Das ist gewollt, um Abhaengigkeiten zu den WLAN-Tools zu erfuellen.
 */

#if defined(CONFIG_MIPS_UR8_C55_MEMORY) && (CONFIG_MIPS_UR8_C55_MEMORY > 0)
#define HAVE_prom_c55_get_base_memory
#endif

#if defined(CONFIG_AR9VR9_C55_MEMORY_SIZE) &&                                  \
	(CONFIG_AR9VR9_C55_MEMORY_SIZE != 0) &&                                \
	defined(CONFIG_AR9VR9_C55_MEMORY_START)
#define HAVE_prom_c55_get_base_memory
#endif

#ifdef HAVE_prom_c55_get_base_memory
int prom_c55_get_base_memory(unsigned int *base, unsigned int *len);
#else
static inline int prom_c55_get_base_memory(unsigned int *base
					   __attribute__((unused)),
					   unsigned int *len
					   __attribute__((unused)))
{
	return -1;
}
#endif

#if (defined(CONFIG_MIPS_UR8_WLAN_MEMORY) &&                                   \
     (CONFIG_MIPS_UR8_WLAN_MEMORY > 0)) ||                                     \
	(defined(CONFIG_AVM_ARCH_STATIC_WLAN_MEMORY) &&                        \
	 (CONFIG_AVM_ARCH_STATIC_WLAN_MEMORY_SIZE > 0))
int prom_wlan_get_base_memory(unsigned int *base, unsigned int *len);
#else
static inline int prom_wlan_get_base_memory(unsigned int *base
					    __attribute__((unused)),
					    unsigned int *len
					    __attribute__((unused)))
{
	return -1;
}
#endif
/* ============================== */
#endif
