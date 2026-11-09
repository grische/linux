avm-autogen-headers-y += include/generated/uapi/avm/event/gen-types.h

define filechk_event_gen_types
	$(srctree)/$(src)/event/generate-event-types "$(srctree)/$(src)"
endef

include/generated/uapi/avm/event/gen-types.h: /dev/null FORCE
	$(call filechk,event_gen_types)
