#ifndef _LINUX_AVM_PA_ARBITER_H
#define _LINUX_AVM_PA_ARBITER_H

#include <linux/types.h>
#include <linux/list.h>
#include <linux/kernel.h>
#include <avm/pa/avm_pa.h>
#include <avm/pa/avm_pa_hw.h>

#define MAX_HW_PA_INSTANCES 2

#define HW_PA_CAPABILITY_BRIDGING (1 << 0)
#define HW_PA_CAPABILITY_ROUTING  (1 << 1)
#define HW_PA_CAPABILITY_LAN2WAN  (1 << 2)
#define HW_PA_CAPABILITY_LAN2LAN  (1 << 3)

struct avm_hardware_pa_instance {
    unsigned long features;
    volatile unsigned char enabled;
    char *name;
    int (*add_session)(struct avm_pa_session *avm_session);
    int (*change_session)(struct avm_pa_session *avm_session);
    int (*remove_session)(struct avm_pa_session *avm_session);
    int (*session_stats)(struct avm_pa_session *avm_session,
                                struct avm_pa_session_stats *stats);
};

void avm_pa_multiplexer_register_instance( struct avm_hardware_pa_instance *hw_pa_instance );
void avm_pa_multiplexer_unregister_instance( struct avm_hardware_pa_instance *hw_pa_instance );
void avm_pa_multiplexer_init(void);


/* --- used by virtual_tx channel -> hw_session_id to avm_session_handle   ---*/
#define INVALID_SESSION 0xFFFF
extern avm_session_handle (*ifx_lookup_routing_entry)(unsigned int routing_entry, unsigned int is_lan, unsigned int is_mc);


#endif
