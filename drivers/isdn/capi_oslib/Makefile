
TARGET		 := "isdn_oslib"

EXTRA_CFLAGS += -D__CAPI_OSLIB__ -Wall -Wextra -Wno-unused-parameter

ifneq ($(CONFIG_CAPI_OSLIB_DEBUG),y)
EXTRA_CFLAGS		+= -DNDEBUG
else
EXTRA_CFLAGS 		+= -g -ggdb
EXTRA_CFLAGS		+= -DDEBUG
endif

capi_oslib-objs		+= \
						appl.o \
						ca.o \
						ca_mem.o \
						ca_sched.o \
						ca_timer.o \
						local_capi.o \
						capi.o \
						capi_events.o \
						capi_pipe.o \
						host.o \
						main.o \
						transparent.o \
						file.o \
						memmap.o \
						debug.o \
						socket_if.o

obj-$(CONFIG_CAPI_OSLIB) += capi_oslib.o

ifeq ($(CONFIG_MACH_BCM963138),y)
   # -Werror causes a lot of critical errors resulting from warnings when using standard macros, so switch it off
   override BCM_KBUILD_CMDLINE_FLAGS := $(subst -Werror,,$(BCM_KBUILD_CMDLINE_FLAGS))
endif
