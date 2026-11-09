ccflags-$(CONFIG_AVM_EVENT_20) += -I$(objtree)/$(obj)/struct/

obj-$(CONFIG_AVM_EVENT) += avm_event.o

avm_event-y += main.o
avm_event-y += file.o
avm_event-y += alloc.o

avm_event-$(CONFIG_AVM_EVENTNODE_PUMA7) += node_pumaX.o
avm_event-$(CONFIG_AVM_EVENTNODE_PUMA7) += remote.o

### AVM Event 2.0
obj-$(CONFIG_AVM_EVENT_20) += struct/
