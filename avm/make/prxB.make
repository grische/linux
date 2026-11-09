KERNEL_IMG_NAME	:= .vmlinux

START_SCRIPT_CAPI := $(FILESYSTEM)/etc/init.d/E41-capi
START_SCRIPT_PIGLET := $(FILESYSTEM)/etc/init.d/S11-piglet

.lha.options: $(LINUX_KERNEL_BUILDDIR)/System.map
	rm -f $@
	echo -e "-l \c" >> $@
	grep 'T _text' $< | sed -e's/.\(.*\) T .*/0xa\1/g' | tr '\n' ' ' >> $@

	echo -e "-e \c" >> $@
	grep 'T kernel_entry' $< | sed -e's/.\(.*\) T .*/0xa\1/g' | tr '\n' ' ' >> $@

$(FILESYSTEM)/$(KERNEL_IMG_NAME): $(LINUX_KERNEL_BUILDDIR)/vmlinux
	@echo "  OBJCOPY $(notdir $<) -> $@"
	$(CROSS_COMPILE)objcopy --remove-section .note.gnu.build-id -S -O binary $< $@
	# fuer dissassemble-helper in avmterm:
	echo `nm $< | grep "kallsyms_\(names\|token\|num_syms\|addresses\)"` >> $@
	echo `file -b -e elf $<` >>$@

linux_all: linux_dtbs

.PHONY: install_dtbs
dtbs = $(wildcard $(LINUX_KERNEL_BUILDDIR)/arch/$(ARCH)/boot/dts/lantiq/avm/Fritz_Box_*.dtb)
install: install_dtbs
install_dtbs: $(dtbs)
ifeq ($(dtbs),)
	$(error No device-tree blobs found!)
endif
	@echo "  INSTALL generated device tree blobs"
	$(foreach dtb,$(dtbs),$(call cmd_install,$(dtb),$(FILESYSTEM)/avm_dts/))

# vim: set ft=make noexpandtab ts=8:
