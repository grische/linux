ccflags-y += -D__CAPI_OSLIB__

ccflags-$(if $(CONFIG_CAPI_OSLIB_DEBUG),n,y)) += -DNDEBUG
ccflags-$(CONFIG_CAPI_OSLIB_DEBUG)+= -DDEBUG

capi_oslib-objs += appl.o
capi_oslib-objs += ca.o
capi_oslib-objs += ca_mem.o
capi_oslib-objs += ca_sched.o
capi_oslib-objs += ca_timer.o
capi_oslib-objs += local_capi.o
capi_oslib-objs += capi.o
capi_oslib-objs += capi_events.o
capi_oslib-objs += capi_pipe.o
capi_oslib-objs += host.o
capi_oslib-objs += main.o
capi_oslib-objs += transparent.o
capi_oslib-objs += file.o
capi_oslib-objs += memmap.o
capi_oslib-objs += debug.o
capi_oslib-objs += socket_if.o

obj-$(CONFIG_CAPI_OSLIB) += capi_oslib.o
