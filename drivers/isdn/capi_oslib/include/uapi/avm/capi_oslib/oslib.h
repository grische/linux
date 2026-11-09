#ifndef _linux_capi_oslib_h_
#define _linux_capi_oslib_h_

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _stack_init_params {
	int	io_addr;
	int	irq_num;
	int	dect_hw;
	int	dect_on;
	int	local_ec;
	int	debug_mode;
	int	trace_mode;
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
typedef struct __f {
	unsigned	nfuncs;
	void	     (* sched_ctrl) (unsigned);
	void         (* wakeup_ctrl) (unsigned);
} functions_t, * functions_p;

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
typedef struct __cb {
	unsigned int controllers;

	unsigned (* cm_start) (void);
	char * (* cm_init) (unsigned, unsigned);
	int (* cm_activate) (void);
	int (* cm_exit) (void);
	unsigned (* cm_handle_events) (void);
	int (* cm_schedule) (void);
	void (* cm_timer_irq_control) (unsigned);
	void (* cm_register_ca_functions) (functions_p);
	unsigned (* check_controller) (unsigned, unsigned *);
	unsigned (*cm_bufsize)(void);
	unsigned (*cm_ctrl_tasklet)(unsigned int);
	unsigned char *(*cm_getprofile)(unsigned int);

	void * (* lib_heap_init) (void *, unsigned);
	void (* lib_heap_exit) (void *);
	void * (* lib_heap_alloc) (void *, unsigned);
	void (* lib_heap_free) (void *, void *);
} lib_callback_t;

#ifdef __CAPI_OSLIB__
extern struct _stack_init_params  *capi_oslib_init_params;
extern lib_callback_t *capi_oslib_stack;
#endif /*--- #ifdef __CAPI_OSLIB__ ---*/

int avm_stack_attach(lib_callback_t *stack_library, struct _stack_init_params *p_params);
int avm_stack_detach(lib_callback_t *stack_library);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
enum _tasklet_control {
	tasklet_control_enter_critical = 0,
	tasklet_control_leave_critical = 1
};
void capi_oslib_init_tasklet_control(void (* tasklet_control)(enum _tasklet_control));

void os_trigger_scheduler(void);
void os_disable_scheduler (void);
void os_enable_scheduler (void);

#if 0
unsigned long EnterCritical (void);
unsigned long _EnterCritical (char *, int);
void LeaveCritical (unsigned long);
void _LeaveCritical (unsigned long, char *, int);
#else
void EnterCritical (void);
void _EnterCritical (char *, int);
void LeaveCritical (void);
void _LeaveCritical (char *, int);
#endif
/*--------------------------------------------------------------------------------*\
 * only for test
\*--------------------------------------------------------------------------------*/
/*--- #define CAPIOSLIB_CHECK_LATENCY ---*/
#if defined(CAPIOSLIB_CHECK_LATENCY)
extern void capi_parse_timestamp(unsigned int handle, char *name, unsigned char *data, unsigned int datalen);
extern void capi_generate_timestamp(unsigned int handle, unsigned char *data, unsigned int datalen);
void capi_capicodec_timestamp(unsigned char *datain, unsigned char *dataout, unsigned int datalenin, unsigned int datalenout);
void capi_check_latency(unsigned int handle, char *name, unsigned int start);
#endif/*--- #if defined(CAPIOSLIB_CHECK_LATENCY) ---*/
#endif /*--- #ifndef _linux_capi_oslib_h_ ---*/
