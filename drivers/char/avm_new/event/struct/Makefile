#####################################################################################
#
#   vim: noexpandtab
#
#####################################################################################
#
#   Copyright (C) 2013 AVM GmbH <fritzbox_info@avm.de>
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
#####################################################################################


AVM_EVENT_GEN_H_FILES     := \
						avm_event_gen_endian_external_defines.h \
						avm_event_gen_endian.h \
						avm_event_gen_enum_range.h \
						avm_event_gen_flow.dot \
						avm_event_gen_types.h

AVM_EVENT_GEN_C_FILES     := \
						avm_event_gen_debug.c \
						avm_event_gen_enum_range.c
AVM_EVENT_GEN_O_FILES     := \
						avm_event_gen_enum_range.o \
						avm_event_gen_debug.o

AVM_EVENT_GEN_PERL_FILES  := avm_event_generate.pl \
						avm_event_generate_enum.pm \
						avm_event_generate_struct.pm \
						avm_event_generate_union.pm \
						input_enum.pm \
						input_params.pm \
						input_struct.pm \
						input_union.pm

$(AVM_EVENT_GEN_H_FILES) $(AVM_EVENT_GEN_C_FILES): $(AVM_EVENT_GEN_PERL_FILES)
	perl avm_event_generate.pl

.NOTPARALLEL: $(AVM_EVENT_GEN_H_FILES) $(AVM_EVENT_GEN_C_FILES)

endian.c:       $(AVM_EVENT_GEN_H_FILES)
avm_dist_event_types.h: $(AVM_EVENT_GEN_H_FILES)

obj-$(CONFIG_AVM_EVENT_20) += endian.o $(AVM_EVENT_GEN_O_FILES)

# EXTRA_CFLAGS	+= -g -ggdb -save-temps
# EXTRA_CFLAGS	+= -DAVM_NEW_HWREV_LIST="\" 156 191 171 137 168 197 139 145 167 141 144 170 175 177 181 183 196 202 203 185 193 192 198 189 179 188 180 96 184 195 190 201 194 200 205 206 172 173 \""

ifeq ($(CONFIG_MACH_BCM963138),y)
   # -Werror causes a lot of critical errors resulting from warnings when using standard macros, so switch it off
   override BCM_KBUILD_CMDLINE_FLAGS := $(subst -Werror,,$(BCM_KBUILD_CMDLINE_FLAGS))
endif
