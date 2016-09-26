#ifndef _LKMD_H
#define _LKMD_H

/*
 * Kernel Debugger Architecture Independent Global Headers
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 2000-2007 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (C) 2000 Stephane Eranian <eranian@hpl.hp.com>
 */

#include <linux/init.h>
#include <linux/sched.h>
#include <asm/atomic.h>

/* These are really private, but they must be defined before including
 * asm-$(ARCH)/kdb.h, so make them public and put them here.
 */
extern int kdb_getuserarea_size(void *, unsigned long, size_t);
extern int kdb_putuserarea_size(unsigned long, void *, size_t);

#include "arch/lkmda.h"

#define KDB_MAJOR_VERSION	0
#define KDB_MINOR_VERSION	1
#define KDB_TEST_VERSION	""

/*
 * kdb_initial_cpu is initialized to -1, and is set to the cpu
 * number whenever the kernel debugger is entered.
 */
extern volatile int kdb_initial_cpu;
#define KDB_IS_RUNNING() (kdb_initial_cpu != -1)
extern int kdb_on;

/*
 * kdb_diemsg
 *
 *	Contains a pointer to the last string supplied to the
 *	kernel 'die' panic function.
 */
extern const char *kdb_diemsg;

#define KDB_FLAG_CMD_INTERRUPT	(1 << 1)	/* Previous command was interrupted */
#define KDB_FLAG_NOIPI		    (1 << 2)	/* Do not send IPIs */

extern volatile int kdb_flags;			/* Global flags, see kdb_state for per cpu state */

extern void kdb_save_flags(void);
extern void kdb_restore_flags(void);

#define KDB_FLAG(flag)		(kdb_flags & KDB_FLAG_##flag)
#define KDB_FLAG_SET(flag)	((void)(kdb_flags |= KDB_FLAG_##flag))
#define KDB_FLAG_CLEAR(flag)	((void)(kdb_flags &= ~KDB_FLAG_##flag))

/*
 * External entry point for the kernel debugger.  The pt_regs
 * at the time of entry are supplied along with the reason for
 * entry to the kernel debugger.
 */

typedef enum {
	KDB_REASON_ENTER=1,		/* 1  KDB_ENTER() trap/fault - regs valid */
	KDB_REASON_ENTER_SLAVE,	/* 2  KDB_ENTER_SLAVE() trap/fault - regs valid */
	KDB_REASON_BREAK,		/* 3  Breakpoint inst. - regs valid */
	KDB_REASON_DEBUG,		/* 4  Debug Fault - regs valid */
	KDB_REASON_OOPS,		/* 5  Kernel Oops - regs valid */
	KDB_REASON_SWITCH,		/* 6  CPU switch - regs valid*/
	KDB_REASON_KEYBOARD,	/* 7  Keyboard entry - regs valid */
	KDB_REASON_NMI,			/* 8  Non-maskable interrupt; regs valid */
	KDB_REASON_RECURSE,		/* 9  Recursive entry to kdb; regs probably valid */
	KDB_REASON_CPU_UP,		/* 10 Add one cpu to kdb; regs invalid */
	KDB_REASON_SILENT,		/* 11 Silent entry/exit to kdb; regs invalid - internal only */
} kdb_reason_t;

extern int kdb(kdb_reason_t, int, struct pt_regs *);

/* Mainly used by kdb code, but this function is sometimes used
 * by hacked debug code so make it generally available, not private.
 */
extern void lkmd_printf(const char *,...)
	    __attribute__ ((format (printf, 1, 2)));
typedef void (*kdb_printf_t)(const char *, ...)
	     __attribute__ ((format (printf, 1, 2)));
extern void kdb_init(void);

#if defined(CONFIG_SMP)
/*
 * Kernel debugger non-maskable IPI handler.
 */
extern int kdb_ipi(struct pt_regs *, void (*ack_interrupt)(void));
extern void smp_kdb_stop(void);
#else	/* CONFIG_SMP */
#define	smp_kdb_stop()
#endif	/* CONFIG_SMP */

static inline int kdb_process_cpu(const struct task_struct *p)
{
	unsigned int cpu = task_thread_info(p)->cpu;
	if (cpu > NR_CPUS)
		cpu = 0;
	return cpu;
}

#endif	/* !_LKMD_H */
