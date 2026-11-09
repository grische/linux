
#pragma GCC push_options
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/semaphore.h>
#include <avm/capi_oslib/new.h>
#include <avm/capi_oslib/oslib.h>
#include <linux/interrupt.h>
#include <linux/env.h>
#pragma GCC pop_options

#include "debug.h"
#include "host.h"
#include "file.h"
#include "capi_oslib.h"
#include "ca.h"
#include "capi_pipe.h"
#include "socket_if.h"

#define ETH_P_RCAPI		0x8888
#define ETH_DEV_RCAPI	"lan"

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
int		io_addr		= 0;
int		irq_num		= 0;
static int		trace_mode	= 0;
int		capi_oslib_trace_mode	= 0;

EXPORT_SYMBOL(capi_oslib_trace_mode);

module_param (io_addr, int, 0 );
module_param (irq_num, int, 0 );
module_param (trace_mode, int, 0 );

MODULE_PARM_DESC (io_addr, "io_addr: I/O address");
MODULE_PARM_DESC (irq_num, "irq_num: IRQ number");
MODULE_PARM_DESC (trace_mode, "trace mode");

struct _stack_init_params  *capi_oslib_init_params;
struct _stack_interrupt_library *capi_oslib_interrupt_library;
lib_callback_t *capi_oslib_stack;

static long int lavm_stack_attach_oncpu(lib_callback_t *stack_library);
static long int lavm_stack_detach_oncpu(lib_callback_t *stack_library);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int __init capi_oslib_init(void) {
	return capi_oslib_file_init();
}
module_init(capi_oslib_init)

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if defined(CONFIG_CAPI_OSLIB_MODULE)
int __exit capi_oslib_exit(void) {
	capi_oslib_file_cleanup();
	return 0;
}
module_exit(capi_oslib_exit);
#endif /*--- #if defined(CONFIG_CAPI_OSLIB_MODULE) ---*/


/*--------------------------------------------------------------------------------*\
 * auf definierter CPU wegen PCMLINK_TASKLET_CONTROL_CPU
\*--------------------------------------------------------------------------------*/
static long int lavm_stack_detach_oncpu(lib_callback_t *stack_library){
	DEB_INFO("[avm_stack_detach] stack_library->cm_exit\n");
	if(stack_library && stack_library->cm_exit)
		stack_library->cm_exit();
	os_disable_scheduler();
	capi_oslib_remove_card(capi_oslib_init_params);
	capi_oslib_socket_release();
	CA_MEM_EXIT( );
	capi_oslib_stack = NULL;
	return 0;
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_stack_detach(lib_callback_t *stack_library) {
	if(stack_library && (capi_oslib_stack == stack_library)) {
#if defined(CONFIG_SMP)
		return work_on_cpu(PCMLINK_TASKLET_CONTROL_CPU, (long int (*)(void *))lavm_stack_detach_oncpu, stack_library);
#endif/*--- #if defined(CONFIG_SMP) ---*/
		return lavm_stack_detach_oncpu(stack_library);
	}
	return 1;
}
EXPORT_SYMBOL(avm_stack_detach);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_stack_attach(lib_callback_t *stack_library, struct _stack_interrupt_library *irq_library, struct _stack_init_params *p_params) {

#if defined(CONFIG_SMP)
	printk(KERN_INFO"[capi_oslib]%s: cpu%d -> cpu%d\n", __func__, raw_smp_processor_id(), PCMLINK_TASKLET_CONTROL_CPU);
#endif/*--- #if defined(CONFIG_SMP) ---*/

	capi_oslib_init_params        = p_params;
	capi_oslib_interrupt_library  = irq_library;
	capi_oslib_stack              = stack_library;

	if(p_params) {
		/*--- printk(KERN_INFO "driver params overwritten io_addr=0x%x irq_num=%u\n", p_params->io_addr, p_params->irq_num); ---*/
		io_addr = p_params->io_addr;
		irq_num = p_params->irq_num;
		trace_mode = p_params->trace_mode;

		capi_oslib_trace_mode = trace_mode;
	}
#if defined(CONFIG_SMP)
	return work_on_cpu(PCMLINK_TASKLET_CONTROL_CPU, (long int (*)(void *))lavm_stack_attach_oncpu, stack_library);
#else
	return lavm_stack_attach_oncpu(stack_library);
#endif/*--- #if defined(CONFIG_SMP) ---*/
}
/*--------------------------------------------------------------------------------*\
 * auf definierter CPU wegen PCMLINK_TASKLET_CONTROL_CPU
\*--------------------------------------------------------------------------------*/
static long int lavm_stack_attach_oncpu(lib_callback_t *stack_library){
	char *Version;

	Capi_Pipe_Init();

	HOST_INIT(SOURCE_UNKNOWN, 25 /* max APPLs */, 100 /* max NCCIs */, 0 /* CAPI_INDEX */);

	DEB_INFO("[avm_stack_attach] capi_oslib_install_card \n");
	capi_oslib_install_card (capi_oslib_init_params);

	DEB_INFO("[avm_stack_attach] capi_oslib_file_activate\n");
	capi_oslib_file_activate();
#if defined(CONFIG_MACH_PUMA6)
	printk(KERN_INFO"[capi_oslib] remote-capi dev=%s eth_proto=0x%x\n", ETH_DEV_RCAPI, ETH_P_RCAPI);
	capi_oslib_socket_init(ETH_DEV_RCAPI, ETH_P_RCAPI);
#endif/*--- #if defined(CONFIG_MACH_PUMA6) ---*/

	DEB_INFO("[avm_stack_attach] CAPI_INIT\n");
	CAPI_INIT(); /*--- kernel capi init ---*/

	DEB_INFO("[avm_stack_attach] stack_library->cm_start\n");
	stack_library->cm_start();

	DEB_INFO("[avm_stack_attach] CA_MALLOC(256 * %u)\n", stack_library->controllers);
	CAPI_Version = (char *)CA_MALLOC(256 * stack_library->controllers);

	DEB_INFO("[avm_stack_attach] stack_library->cm_init(0x%08X, %u)\n", io_addr, irq_num);
	Version = stack_library->cm_init(io_addr, irq_num);

	if(Version) {
		DEB_INFO("[avm_stack_attach] memcpy(Version ...)\n");
		memcpy(CAPI_Version, Version, 256 * stack_library->controllers);
	} else {
		DEB_ERR("ERROR no version information\n");
		avm_stack_detach(stack_library);
		return 1;
	}

	if(capi_oslib_init_params && capi_oslib_init_params->irq_num)
		disable_irq (capi_oslib_init_params->irq_num);
	DEB_INFO("[avm_stack_attach] stack_library->cm_activate\n");
	stack_library->cm_activate();
	stack_library->cm_handle_events();
	if(capi_oslib_init_params && capi_oslib_init_params->irq_num)
		enable_irq (capi_oslib_init_params->irq_num);

	os_trigger_scheduler();

	DEB_INFO("%s: cpu%d done\n", __func__, raw_smp_processor_id());
	return 0;
} /* fritz_init */
EXPORT_SYMBOL(avm_stack_attach);

