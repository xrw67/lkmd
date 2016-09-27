# LKMD
Linux Kernel Module Debugger, base on KDB

tested on Virtualbox + Centos 7.2 x64


## Install

	git clone http://github.com/elemeta/lkmd.git ~/lkmd
	cd ~/lkmd
	make
	insmod lkmd.ko

## Uninstall

	rmmod lkmd

## Enter debugger

Insert int3 instruction into you source code

	asm("int3\n");

## Architecture

- lkmd_main.c : Debug Core
- lkmd_io.c : I/O Driver(etc. Keyboard)
- lkmd_id.c : Disassembly engine
- lkmd_bp.c : Breakpoint and Single Step engine
- x86 : Intel x86 arch implement

## Contact me

elemeta, <elemeta47@gmail.com>

