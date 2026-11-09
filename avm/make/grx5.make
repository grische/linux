KERNEL_IMG_NAME	:= .vmlinux

# k/gw >= v1.133 compatibility
#
# Arch mips' install target in v4.9 does not support installing a stripped
# kernel image, thus we enforce building arch/mips/boot/vmlinux.bin and request
# k/gw to install it into $(INSTALL_PATH)/boot/.
KGW_LINUX_INSTALL = install&vmlinux.bin
KGW_LINUX_IMAGE_OVERRIDE = arch/mips/boot/vmlinux.bin

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

# vim: set ft=make noexpandtab ts=8:
