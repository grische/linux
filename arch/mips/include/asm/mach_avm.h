#ifndef _mach_avm_h_
#define _mach_avm_h_

#include <linux/err.h>
#include <linux/kconfig.h>
#include <asm/ptrace.h>
#include <spaces.h>
#if !defined(CONFIG_SOC_TYPE_GRX500_TEP)
#include <avm/enh/reboot_status.h>
#endif
#include <clocksource/intel-gptc-timer.h>

#if defined(CONFIG_LANTIQ)

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
enum _avm_clock_id {
	avm_clock_id_non = 0x00,
	avm_clock_id_cpu = 0x01,
	avm_clock_id_fpi = 0x02, /*--- On-Chip Bus ---*/
	avm_clock_id_usb = 0x04,
	avm_clock_id_pci = 0x08,
	avm_clock_id_ephy = 0x10, /*--- Ethernet CLKO ---*/
	avm_clock_id_ddr = 0x20, /*--- DDR ---*/
	avm_clock_id_pp32 = 0x40, /*--- Paketprocessor ---*/
};
/*--------------------------------------------------------------------------------*\
 * based on gptc (independent from cpu-clock)
\*--------------------------------------------------------------------------------*/
extern unsigned long grx_get_cyclefreq(void);
extern unsigned long grx_get_cycles(void);

extern void show_registers(const struct pt_regs *regs);

#ifdef CONFIG_GRX5_AVM_FRITZ_BOX
# define OPTIMIZE_CYCLE
#else
# undef OPTIMIZE_CYCLE
#endif

#if defined(OPTIMIZE_CYCLE)
/*--------------------------------------------------------------------------------*\
 * dirty - aber wir brauchen den Cycle-Timer ohne Umweg ueber Funktion
\*--------------------------------------------------------------------------------*/
#define GPTC_CLKSRC_TIMER3_A 4
#define GPTC0_ADDR ((void __iomem *)(IO_BASE | 0x16b00000))
#define GPTC_CNT(X) (0x28 + (X/2)*0x20 + (X%2)*0x04)
static inline unsigned long avm_get_cycles(void)
{
	return *((volatile unsigned long *)(GPTC0_ADDR + GPTC_CNT(GPTC_CLKSRC_TIMER3_A)));
}
#else /*--- #if defined(OPTIMIZE_CYCLE) ---*/
#define avm_get_cycles() grx_get_cycles()
#endif /*--- #else ---*/ /*--- #if defined(OPTIMIZE_CYCLE) ---*/

#define avm_get_cyclefreq() grx_get_cyclefreq()
#define avm_cycles_cpuclock_depend() (0)

extern unsigned int lantiq_get_clock(enum _avm_clock_id clock_id);

#define avm_get_clock lantiq_get_clock

#if defined(CONFIG_SOC_GRX500) || defined(CONFIG_SOC_TYPE_GRX500_TEP)
#include <linux/avm_hw_config_def.h>

enum _hw_gpio_direction { GPIO_INPUT_PIN = 0, GPIO_OUTPUT_PIN = 1 };

enum _hw_gpio_function {
	GPIO_PIN = AVM_DEF_HW_FUNCTION_GPIO_PIN,
	FUNCTION_PINMUX1 = AVM_DEF_HW_FUNCTION_PINMUX1,
	FUNCTION_PINMUX2 = AVM_DEF_HW_FUNCTION_PINMUX2,
	FUNCTION_PINMUX3 = AVM_DEF_HW_FUNCTION_PINMUX3,
	FUNCTION_PIN_NOCHANGE = AVM_DEF_HW_FUNCTION_PIN_NOCHANGE,
};

enum _hw_gpio_config {
	PINCONF_PARAM_PULLDOWN = 0,
	PINCONF_PARAM_PULLUP = 1,
	PINCONF_PARAM_OPEN_DRAIN = 2,
	PINCONF_PARAM_SLEW_RATE = 3,
	PINCONF_PARAM_DRIVE_CURRENT = 4,
	PINCONF_PARAM_NOCHANGE = 0xFF
};

extern void __iomem *avm_gpio_shim_get_inputreg_and_bit(unsigned int gpio_pin,
						       int *bit);
extern int avm_gpio_shim_ctrl(unsigned int gpio_pin,
			     enum _hw_gpio_function pin_mode,
			     enum _hw_gpio_direction pin_dir);
extern int avm_gpio_shim_out_bit(unsigned int gpio_pin, int value);
extern int avm_gpio_shim_in_bit(unsigned int gpio_pin);

#define avm_gpio_ctrl avm_gpio_shim_ctrl
#define avm_gpio_out_bit avm_gpio_shim_out_bit
#define avm_gpio_in_bit avm_gpio_shim_in_bit
static inline void __iomem *avm_gpio_get_membase_and_bit(unsigned int gpio_pin,
		int *bit)
{
	void __iomem *ret = avm_gpio_shim_get_inputreg_and_bit(gpio_pin, bit);

	return IS_ERR(ret) ? NULL : ret;
}

/*--------------------------------------------------------------------------------*\
 * collect info about which cpuid use core_x, tc_x and linux-mode
 * (needed for backtrace)
\*--------------------------------------------------------------------------------*/
void avm_register_cpuid(char *name, unsigned int cpuid, unsigned int core,
			unsigned int tc);

/*--------------------------------------------------------------------------------*\
 * Request which (linux-)cpu_id  behind tc/core
 * ret:  cpu_id - if cpu_id == -1: this is no linux-os
 * name: name of OS on this CPU
\*--------------------------------------------------------------------------------*/
int get_cpuid_by_mt(unsigned int core, unsigned int tc, char **name);

/*--------------------------------------------------------------------------------*\
 * Request which tc/core behind linux-cpu
 *
 * ret:   0      Linux-OS
 *      < 0      invalid CPU_ID
 *      > 0      other
\*--------------------------------------------------------------------------------*/
int get_mt_by_cpuid(unsigned int cpu_id, unsigned int *core, unsigned int *tc);

#else // CONFIG_SOC_GRX500

#include <asm/hw_gpio.h>
/*--- #define avm_gpio_init        arx188_gpio_init ---*/
/*--- #define avm_gpio_ctrl        arx188_gpio_ctrl ---*/
/*--- #define avm_gpio_out_bit     arx188_gpio_out_bit ---*/
/*--- #define avm_gpio_in_bit      arx188_gpio_in_bit ---*/

/*------------------------------------------------------------------------------------------*\
 * 240-254 char	LOCAL/EXPERIMENTAL USE
 * 240-254 block	LOCAL/EXPERIMENTAL USE
 *         Allocated for local/experimental use.  For devices not
 *         assigned official numbers, these ranges should be
 *         used in order to avoid conflicting with future assignments.
\*------------------------------------------------------------------------------------------*/
#define AVM_DECT_IO_MAJOR 227
#define AVM_USERMAN_MAJOR 228
#define KDSLD_USERMAN_MAJOR 229
#define AVM_TIATM_MAJOR 230
#define AVM_EVENT_MAJOR 241
#define WATCHDOG_MAJOR 242
#define KDSLD_MAJOR 243
#define KDSLDPTRACE_MAJOR 244
#define UBIK_MAJOR 245
#define DEBUG_TRACE_MAJOR 246
#define AVM_LED_MAJOR 247
#define AVM_I2C_MAJOR 248
#define YAFFS 249
#define AVM_AUDIO_MAJOR 250
#define AVM_NEW_LED_MAJOR 251
#define AVM_POWER_MAJOR 252
#define AVM_NET_TRACE_MAJOR 255
#define AVM_VINAX_MAJOR 253
#define AVM_HSK_MAJOR 254
#define AVM_ATH_EEPROM 239

#endif // CONFIG_SOC_GRX500 else
#endif /*--- #if defined(CONFIG_LANTIQ) ---*/

/*------------------------------------------------------------------------------------------*\
 * VIRIAN VIRIAN VIRIAN VIRIAN VIRIAN VIRIAN VIRIAN VIRIAN VIRIAN VIRIAN VIRIAN VIRIAN VIRIAN
\*------------------------------------------------------------------------------------------*/
#if defined(CONFIG_SOC_AR724X) || defined(CONFIG_SOC_AR934X) ||                \
	defined(CONFIG_SOC_QCA955X) || defined(CONFIG_SOC_QCA953X) ||          \
	defined(CONFIG_SOC_QCA956X)

#include <atheros.h>
#include <atheros_gpio.h>
#define avm_gpio_init ath_avm_gpio_init /*--- init ath_avm_gpio_init(void) ---*/
#define avm_gpio_ctrl                                                          \
	ath_avm_gpio_ctrl /*--- int ath_avm_gpio_ctrl(unsigned int gpio_pin, enum _hw_gpio_function pin_mode, enum _hw_gpio_direction pin_dir) ---*/
#define avm_gpio_out_bit                                                       \
	ath_avm_gpio_out_bit /*--- int ath_avm_gpio_out_bit(unsigned int gpio_pin, int value)  ---*/
#define avm_gpio_in_bit                                                        \
	ath_avm_gpio_in_bit /*--- int ath_avm_gpio_in_bit(unsigned int gpio_pin)  ---*/
#define avm_gpio_in_value                                                      \
	ath_avm_gpio_in_value /*--- unsigned int ath_avm_gpio_in_value(void)  ---*/
#define avm_gpio_set_bitmask                                                   \
	ath_avm_gpio_set_bitmask /*--- void ath_avm_gpio_set_bitmask(unsigned int mask, unsigned int value)  ---*/
#define avm_gpio_request_irq ath_avm_gpio_request_irq
#define avm_gpio_enable_irq ath_avm_gpio_enable_irq
#define avm_gpio_disable_irq ath_avm_gpio_disable_irq

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
enum _avm_clock_id {
	avm_clock_id_non = 0x00,
	avm_clock_id_cpu = 0x01,
	avm_clock_id_ddr = 0x02,
	avm_clock_id_ahb = 0x04,
	avm_clock_id_ref = 0x08,
	avm_clock_id_peripheral = 0x10 /*--- Uart ---*/
};
extern unsigned int ath_get_clock(enum _avm_clock_id id);
#define avm_get_clock ath_get_clock

extern void ath_avm_gpio_dump_registers(const char *prefix);
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#define MAX_ENV_ENTRY 256
#define FLASH_ENV_ENTRY_SIZE 64
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
char *avm_urlader_env_get_variable(int idx);
char *avm_urlader_env_get_value_by_id(unsigned int id);
char *avm_urlader_env_get_value(char *var);
int avm_urlader_env_set_variable(char *var, char *val);
int avm_urlader_env_unset_variable(char *var);
int avm_urlader_env_defrag(void);

/*------------------------------------------------------------------------------------------*\
 * 240-254 char	LOCAL/EXPERIMENTAL USE
 * 240-254 block	LOCAL/EXPERIMENTAL USE
 *         Allocated for local/experimental use.  For devices not
 *         assigned official numbers, these ranges should be
 *         used in order to avoid conflicting with future assignments.
\*------------------------------------------------------------------------------------------*/
#define AVM_DECT_IO_MAJOR 227
#define AVM_USERMAN_MAJOR 228
#define KDSLD_USERMAN_MAJOR 229
#define AVM_TIATM_MAJOR 230
#define AVM_EVENT_MAJOR 241
#define WATCHDOG_MAJOR 242
#define KDSLD_MAJOR 243
#define KDSLDPTRACE_MAJOR 244
#define UBIK_MAJOR 245
#define DEBUG_TRACE_MAJOR 246
#define AVM_LED_MAJOR 247
#define AVM_I2C_MAJOR 248
#define YAFFS 249
#define AVM_AUDIO_MAJOR 250
#define AVM_NEW_LED_MAJOR 251
#define AVM_POWER_MAJOR 252
#define AVM_NET_TRACE_MAJOR 255
#define AVM_VINAX_MAJOR 253
#define AVM_HSK_MAJOR 254
#define AVM_ATH_EEPROM 239

#endif /*--- #if defined(CONFIG_SOC_AR724X) || defined(CONFIG_SOC_AR934X) || defined(CONFIG_SOC_QCA955X) ---*/

/*------------------------------------------------------------------------------------------*\
 * IKANOS IKANOS IKANOS IKANOS IKANOS IKANOS IKANOS IKANOS IKANOS IKANOS IKANOS IKANOS
\*------------------------------------------------------------------------------------------*/
#ifdef CONFIG_MACH_FUSIV

#define default_ikan_lan_xtal 25000000
#define default_ikan_vdsl_xtal 16328000

enum _avm_clock_id {
	avm_clock_id_non = 0x00,
	avm_clock_id_cpu = 0x01, /*--- default: 500.000 MHz ---*/
	avm_clock_id_system = 0x02, /*--- default: 166.000 MHz ---*/
	avm_clock_id_usb = 0x04, /*--- default:  12.000 MHz ---*/
	avm_clock_id_usb2 = 0x08, /*--- default: 480.000 MHz ---*/
	avm_clock_id_21xx = 0x10, /*--- default: 100.000 MHz ??? ---*/
	avm_clock_id_ddr = 0x20, /*--- default: 333.000 MHz ---*/
	avm_clock_id_ephy = 0x80, /*--- default: 125.000 MHz ---*/
	avm_clock_id_pci = 0x100, /*--- default:  33.000 MHz ---*/
	avm_clock_id_pci66 = 0x200, /*--- default:  63.000 MHz ---*/
	avm_clock_id_ap = 0x400, /*--- default: 400.000 MHz ??? ---*/
	avm_clock_id_bme = 0x800, /*--- default: 400.000 MHz ---*/
	avm_clock_id_fd = 0x1000,
};

#include <ikan_clk.h>

#define avm_get_clock ikan_get_clock

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#define MAX_ENV_ENTRY 256
#define FLASH_ENV_ENTRY_SIZE 64

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
char *avm_urlader_env_get_variable(int idx);
char *avm_urlader_env_get_value_by_id(unsigned int id);
char *avm_urlader_env_get_value(char *var);
int avm_urlader_env_set_variable(char *var, char *val);
int avm_urlader_env_unset_variable(char *var);
int avm_urlader_env_defrag(void);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#include <asm/mach-fusiv/hw_gpio.h>
#include <ikan_gpio.h>

#define avm_gpio_init ikan_gpio_init
#define avm_gpio_ctrl ikan_gpio_ctrl
#define avm_gpio_request_irq ikan_gpio_request_irq
#define avm_gpio_enable_irq ikan_gpio_enable_irq
#define avm_gpio_disable_irq ikan_gpio_disable_irq
#define avm_read_irq_gpio ikan_read_irq_gpio
#define avm_gpio_set_delayed_irq_mode ikan_gpio_set_delayed_irq_mode
#define avm_gpio_out_bit ikan_gpio_out_bit
#define avm_gpio_in_bit ikan_gpio_in_bit
#define avm_gpio_in_value ikan_gpio_in_value
#define avm_gpio_set_bitmask ikan_gpio_set_bitmask

/*------------------------------------------------------------------------------------------*\
 * 240-254 char LOCAL/EXPERIMENTAL USE
 * 240-254 block    LOCAL/EXPERIMENTAL USE
 *         Allocated for local/experimental use.  For devices not
 *         assigned official numbers, these ranges should be
 *         used in order to avoid conflicting with future assignments.
\*------------------------------------------------------------------------------------------*/
#define AVM_DECT_IO_MAJOR 227
#define AVM_USERMAN_MAJOR 228
#define KDSLD_USERMAN_MAJOR 229
#define AVM_TIATM_MAJOR 230
#define TFFS_MAJOR 240
#define AVM_EVENT_MAJOR 241
#define WATCHDOG_MAJOR 242
#define KDSLD_MAJOR 243
#define KDSLDPTRACE_MAJOR 244
#define UBIK_MAJOR 245
#define DEBUG_TRACE_MAJOR 246
#define AVM_LED_MAJOR 247
#define AVM_I2C_MAJOR 248
#define YAFFS 249
#define AVM_AUDIO_MAJOR 250
#define AVM_NEW_LED_MAJOR 251
#define AVM_POWER_MAJOR 252
#define AVM_NET_TRACE_MAJOR 255
#define AVM_VINAX_MAJOR 253
#define AVM_HSK_MAJOR 254
#define AVM_ATH_EEPROM 239

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
/*--- #include <asm/mips-boards/ur8_power.h> ---*/

/*--- #define avm_power_down_init               ur8_power_down_init ---*/
/*--- #define avm_put_device_into_power_down    ur8_put_device_into_power_down ---*/
/*--- #define avm_take_device_out_of_power_down ur8_take_device_out_of_power_down ---*/
/*--- #define avm_register_power_down_gpio      ur8_register_power_down_gpio ---*/
/*--- #define avm_release_power_down_gpio       ur8_release_power_down_gpio ---*/
/*--- #define avm_power_down_status             ur8_power_down_status ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
/*--- #include <asm/mach-ur8/hw_reset.h> ---*/
/*--- #include <asm/mips-boards/ur8_reset.h> ---*/

/*--- #define avm_reset_init                      ur8_reset_init ---*/
/*--- #define avm_take_device_out_of_reset        ur8_take_device_out_of_reset ---*/
/*--- #define avm_put_device_into_reset           ur8_put_device_into_reset ---*/
/*--- #define avm_reset_device                    ur8_reset_device ---*/
/*--- #define avm_reset_status                    ur8_reset_status ---*/
/*--- #define avm_register_reset_gpio               ur8_register_reset_gpio ---*/
/*--- #define avm_release_reset_gpio                ur8_release_reset_gpio ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
/*--- #include <asm/mips-boards/ur8int.h> ---*/

/*--- #define avm_int_init                        ur8int_init ---*/
/*--- #define avm_int_set_type                    ur8int_set_type ---*/
/*--- #define avm_int_ctrl_irq_pacing_setup       ur8int_ctrl_irq_pacing_setup ---*/
/*--- #define avm_int_ctrl_irq_pacing_register    ur8int_ctrl_irq_pacing_register ---*/
/*--- #define avm_int_ctrl_irq_pacing_unregister  ur8int_ctrl_irq_pacing_unregister ---*/
/*--- #define avm_int_ctrl_irq_pacing_set         ur8int_ctrl_irq_pacing_set ---*/
/*--- #define avm_nsec_timer                      ur8_nsec_timer ---*/

#endif /*--- #ifdef CONFIG_MACH_FUSIV ---*/

/*------------------------------------------------------------------------------------------*\
 * zusaetzlich folgende:
 *
 * 207 char	Compaq ProLiant health feature indicate
 * 220 char	Myricom Myrinet "GM" board
 * 224 char	A2232 serial card
 * 225 char	A2232 serial card (alternate devices)
 * 227 char	IBM 3270 terminal Unix tty access
 * 228 char	IBM 3270 terminal block-mode access
 * 229 char	IBM iSeries virtual console
 * 230 char	IBM iSeries virtual tape
 *
\*------------------------------------------------------------------------------------------*/

//Only used as fallback, if location is not stored in device-tree under reserved-memory with compatible 'avm,reboot_string' or with name 'avm_reboot_string'
#define AVM_REBOOT_STRING_LOCATION (0xa0001000)
#define AVM_REBOOT_STRING_SIZE 512

#endif /*--- #ifndef _mach_avm_h_ ---*/
