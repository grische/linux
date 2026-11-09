# SPDX-License-Identifier: GPL-2.0

genhdr-y += $(strip \
	$(patsubst include/generated/uapi/linux/%,%,\
	  $(wildcard \
	    $(addprefix include/generated/uapi/linux/,\
	      $(avm_labile_headers)))))
