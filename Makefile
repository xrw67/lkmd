# LKMD (Linux Kernel Module Debugger)

obj-m:=lkmd.o

lkmd-objs:=lkmd_main.o \
	lkmd_bp.o \
	lkmd_id.o \
	lkmd_io.o \
	lkmd_support.o \
	arch/lkmda_bp.o \
	arch/lkmda_id.o \
	arch/lkmda_io.o \
	arch/lkmda_support.o \
	arch/x86-dis.o

KDIR:=/lib/modules/$(shell uname -r)/build
PWD:=$(shell pwd)

ifneq (,$(findstring -fno-optimize-sibling-calls,$(KBUILD_CFLAGS)))
  CFLAGS_lkmda_bt.o += -DNO_SIBLINGS
endif

REGPARM := $(subst -mregparm=,,$(filter -mregparm=%,$(KBUILD_CFLAGS)))
ifeq (,$(REGPARM))
ifeq ($(CONFIG_X86_32),y)
  REGPARM := 3
else
  REGPARM := 6
endif
endif

CFLAGS_lkmda_bt.o += -DREGPARM=$(REGPARM) -DCCVERSION="$(CCVERSION)"

all:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules
#	mv lkmd.ko lkmd-$(shell uname -r)-$(shell uname -m).ko

install:
	insmod lkmd.ko

exit:
	rmmod lkmd

clean:
	rm -rf *.o *.ko *.mod.c .*.cmd .depend .*.o.d .tmp_versions Module.markers *.ko.unsigned Module.symvers Module.symvers modules.order
	rm -rf arch/*.o arch/*.mod.c arch/.*.cmd arch/.depend arch/.*.o.d
	
