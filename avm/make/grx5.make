KERNEL_IMG_NAME	:= .vmlinux

START_SCRIPT_CAPI := $(FILESYSTEM)/etc/init.d/E41-capi
START_SCRIPT_PIGLET := $(FILESYSTEM)/etc/init.d/S11-piglet

START_SCRIPTS_ARCH := $(LINUX_KERNEL_SOURCE)/scripts/E36-init_net_grx

.lha.options: $(LINUX_KERNEL_BUILDDIR)/System.map
	rm -f $@
	echo -e "-l \c" >> $@
	grep 'T _text' $< | sed -e's/..\(.*\) T .*/0x80\1/g' | tr '\n' ' ' >> $@

	echo -e "-e \c" >> $@
	grep 'T kernel_entry' $< | sed -e's/..\(.*\) T .*/0x80\1/g' | tr '\n' ' ' >> $@

$(FILESYSTEM)/$(KERNEL_IMG_NAME): $(LINUX_KERNEL_BUILDDIR)/vmlinux
	@echo "  OBJCOPY $(notdir $<) -> $@"
	$(CROSS_COMPILE)objcopy --remove-section .note.gnu.build-id -S -O binary $< $@
	# fuer dissassemble-helper in avmterm:
	echo `nm $< | grep "kallsyms_\(names\|token\|num_syms\|addresses\|relative_base\|offsets\)"` >> $@
	echo `file -b -e elf $<` >>$@

linux_all: linux_dtbs

.PHONY: install_arch_scripts
install: install_arch_scripts
install_arch_scripts:
	$(foreach script,$(START_SCRIPTS_ARCH),$(call cmd_install,$(script),$(FILESYSTEM)/etc/init.d/))

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
