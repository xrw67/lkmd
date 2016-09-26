/*
 * Kernel Debugger Architecture Independent Console I/O handler
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 1999-2006 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/ctype.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/console.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/nmi.h>
#include <linux/delay.h>
#include <linux/kallsyms.h>
#include <linux/input.h>
#include <linux/tty.h>
#include "lkmd.h"
#include "lkmd_private.h"


#define CMD_BUFLEN 256
char kdb_prompt_str[CMD_BUFLEN];
static struct console *kdbcons;

char *lkmd_read(char *buffer, size_t bufsize)
{
	char *cp = buffer;
	char *bufend = buffer + bufsize - 2;	/* Reserve space for newline and null byte */
	char *lastchar;
	char tmp;
	static char tmpbuffer[CMD_BUFLEN];
	int len = strlen(buffer);
	get_char_func *f;

	if (len > 0 ) {
		cp += len;
		if (*(buffer+len-1) == '\n')
			cp--;
	}

	lastchar = cp;
	*cp = '\0';
	lkmd_printf("%s", buffer);

	for (;;) {
		int key;
		for (f = &poll_funcs[0]; ; ++f) {
			if (*f == NULL) {
				/* Reset NMI watchdog once per poll loop */
				touch_nmi_watchdog();
				f = &poll_funcs[0];
			}
			key = (*f)();
			if (key == -1) {
				continue;
			}
			if (bufsize <= 2) {
				if (key == '\r')
					key = '\n';
				lkmd_printf("%c", key);
				*buffer++ = key;
				*buffer = '\0';
				return buffer;
			}
			break;	/* A key to process */
		}

		switch (key) {
		case 8: /* backspace */
			if (cp > buffer) {
				if (cp < lastchar) {
					memcpy(tmpbuffer, cp, lastchar - cp);
					memcpy(cp-1, tmpbuffer, lastchar - cp);
				}
				*(--lastchar) = '\0';
				--cp;
				lkmd_printf("\b%s \r", cp);
				tmp = *cp;
				*cp = '\0';
				lkmd_printf(kdb_prompt_str);
				lkmd_printf("%s", buffer);
				*cp = tmp;
			}
			break;
		case 13: /* enter \r */
		case 10: /* enter \n */
			*lastchar++ = '\n';
			*lastchar++ = '\0';
			lkmd_printf("\n");
			return buffer;
		case 4: /* Del */
			if(cp < lastchar) {
				memcpy(tmpbuffer, cp+1, lastchar - cp -1);
				memcpy(cp, tmpbuffer, lastchar - cp -1);
				*(--lastchar) = '\0';
				lkmd_printf("%s \r", cp);
				tmp = *cp;
				*cp = '\0';
				lkmd_printf(kdb_prompt_str);
				lkmd_printf("%s", buffer);
				*cp = tmp;
			}
			break;
		case 1: /* Home */
			if(cp > buffer) {
				lkmd_printf("\r");
				lkmd_printf(kdb_prompt_str);
				cp = buffer;
			}
			break;
		case 5: /* End */
			if(cp < lastchar) {
				lkmd_printf("%s", cp);
				cp = lastchar;
			}
			break;
		case 2: /* Left */
			if (cp > buffer) {
				lkmd_printf("\b");
				--cp;
			}
			break;
		case 14: /* Down */
			memset(tmpbuffer, ' ', strlen(kdb_prompt_str)+(lastchar-buffer));
			*(tmpbuffer+strlen(kdb_prompt_str)+(lastchar-buffer)) = '\0';
			lkmd_printf("\r%s\r", tmpbuffer);
			*lastchar = (char)key;
			*(lastchar+1) = '\0';
			return lastchar;
		case 6: /* Right */
			if (cp < lastchar) {
				lkmd_printf("%c", *cp);
				++cp;
			}
			break;
		case 16: /* Up */
			memset(tmpbuffer, ' ', strlen(kdb_prompt_str)+(lastchar-buffer));
			*(tmpbuffer+strlen(kdb_prompt_str)+(lastchar-buffer)) = '\0';
			lkmd_printf("\r%s\r", tmpbuffer);
			*lastchar = (char)key;
			*(lastchar+1) = '\0';
			return lastchar;
		default:
			if (key >= 32 &&lastchar < bufend) {
				if (cp < lastchar) {
					memcpy(tmpbuffer, cp, lastchar - cp);
					memcpy(cp+1, tmpbuffer, lastchar - cp);
					*++lastchar = '\0';
					*cp = key;
					lkmd_printf("%s\r", cp);
					++cp;
					tmp = *cp;
					*cp = '\0';
					lkmd_printf(kdb_prompt_str);
					lkmd_printf("%s", buffer);
					*cp = tmp;
				} else {
					*++lastchar = '\0';
					*cp++ = key;
					lkmd_printf("%c", key);
				}
			}
			break;
		}
	}
}

/*
 * kdb_getstr
 *
 *	Print the prompt string and read a command from the
 *	input device.
 *
 * Parameters:
 *	buffer	Address of buffer to receive command
 *	bufsize Size of buffer in bytes
 *	prompt	Pointer to string to use as prompt string
 * Returns:
 *	Pointer to command buffer.
 * Locking:
 *	None.
 * Remarks:
 *	For SMP kernels, the processor number will be
 *	substituted for %d, %x or %o in the prompt.
 */

char *kdb_getstr(char *buffer, size_t bufsize, char *prompt)
{
	if(prompt && kdb_prompt_str!=prompt)
		strncpy(kdb_prompt_str, prompt, CMD_BUFLEN);
	lkmd_printf(kdb_prompt_str);
	kdb_nextline = 1;	/* Prompt and input resets line number */
	return lkmd_read(buffer, bufsize);
}


/*
 * kdb_input_flush
 *
 *	Get rid of any buffered console input.
 *
 * Parameters:
 *	none
 * Returns:
 *	nothing
 * Locking:
 *	none
 * Remarks:
 *	Call this function whenever you want to flush input.  If there is any
 *	outstanding input, it ignores all characters until there has been no
 *	data for approximately 1ms.
 */

static void kdb_input_flush(void)
{
	get_char_func *f;
	int res;
	int flush_delay = 1;
	while (flush_delay) {
		flush_delay--;
empty:
		touch_nmi_watchdog();
		for (f = &poll_funcs[0]; *f; ++f) {
			res = (*f)();
			if (res != -1) {
				flush_delay = 1;
				goto empty;
			}
		}
		if (flush_delay)
			mdelay(1);
	}
}

/*
 * lkmd_printf
 *
 *	Print a string to the output device(s).
 *
 * Parameters:
 *	printf-like format and optional args.
 * Returns:
 *	0
 * Locking:
 *	None.
 * Remarks:
 *	use 'kdbcons->write()' to avoid polluting 'log_buf' with kdb output.
 */

static char kdb_buffer[256];	/* A bit too big to go on stack */

static void lkmd_console_write(const char *s, unsigned len)
{
	struct console *c = console_drivers;

	/* Write to all consoles. */
	while (c) {
		if ((c->flags & CON_ENABLED) && c->write) {
			c->write(c, s, len);
		}
		touch_nmi_watchdog();
		c = c->next;
	}

	printk(s);
}

void lkmd_printf(const char *fmt, ...)
{
	va_list ap;
	int diag;
	int linecount;
	int do_longjmp = 0;
	int got_printf_lock = 0;
	char *moreprompt = "more> ";
	static DEFINE_SPINLOCK(kdb_printf_lock);
	unsigned long uninitialized_var(flags);

	preempt_disable();
	/* Serialize kdb_printf if multiple cpus try to write at once.
	 * But if any cpu goes recursive in kdb, just print the output,
	 * even if it is interleaved with any other text.
	 */
	if (!KDB_STATE(PRINTF_LOCK)) {
		KDB_STATE_SET(PRINTF_LOCK);
		spin_lock_irqsave(&kdb_printf_lock, flags);
		got_printf_lock = 1;
	} else {
		__acquire(kdb_printf_lock);
	}

	diag = kdbgetintenv("LINES", &linecount);
	if (diag || linecount <= 1)
		linecount = 22;

	va_start(ap, fmt);
	vsnprintf(kdb_buffer, sizeof(kdb_buffer), fmt, ap);
	va_end(ap);

	lkmd_console_write(kdb_buffer, strlen(kdb_buffer));

	if (KDB_STATE(LONGJMP) && strchr(kdb_buffer, '\n'))
		kdb_nextline++;

	/* check for having reached the LINES number of printed lines */
	if (kdb_nextline == linecount) {
		char buf1[16]="";
#if defined(CONFIG_SMP)
		char buf2[32];
#endif

		/* Watch out for recursion here.  Any routine that calls
		 * kdb_printf will come back through here.  And kdb_read
		 * uses kdb_printf to echo on serial consoles ...
		 */
		kdb_nextline = 1;	/* In case of recursion */

		/*
		 * Pause until cr.
		 */
		moreprompt = kdbgetenv("MOREPROMPT");
		if (moreprompt == NULL) {
			moreprompt = "more> ";
		}

#if defined(CONFIG_SMP)
		if (strchr(moreprompt, '%')) {
			sprintf(buf2, moreprompt, get_cpu());
			put_cpu();
			moreprompt = buf2;
		}
#endif

		kdb_input_flush();
		lkmd_console_write(moreprompt, strlen(moreprompt));

		lkmd_read(buf1, 2); /* '2' indicates to return immediately after getting one key. */
		kdb_nextline = 1;	/* Really set output line 1 */

		/* empty and reset the buffer: */
		kdb_buffer[0] = '\0';
		if ((buf1[0] == 'q') || (buf1[0] == 'Q')) {
			/* user hit q or Q */
			do_longjmp = 1;
			KDB_FLAG_SET(CMD_INTERRUPT);	/* command was interrupted */
			/* end of command output; back to normal mode */
			lkmd_printf("\n");
		} else if (buf1[0] && buf1[0] != '\n') {
			/* user hit something other than enter */
			lkmd_printf("\nOnly 'q' or 'Q' are processed at more prompt, input ignored\n");
		}
		kdb_input_flush();
	}

	if (KDB_STATE(PRINTF_LOCK) && got_printf_lock) {
		got_printf_lock = 0;
		spin_unlock_irqrestore(&kdb_printf_lock, flags);
		KDB_STATE_CLEAR(PRINTF_LOCK);
	} else {
		__release(kdb_printf_lock);
	}
	preempt_enable();
	if (do_longjmp)
#ifdef kdba_setjmp
		kdba_longjmp(&kdbjmpbuf[smp_processor_id()], 1)
#endif	/* kdba_setjmp */
		;
}

/*
 * kdb_io_init
 *
 *	Initialize kernel debugger output environment.
 *
 *	Select a console device.  Only use a VT console if the user specified
 *	or defaulted console= /^tty[0-9]*$/
 */

int __init kdb_io_init(void)
{
	/*
	 * Select a console.
	 */
	struct console *c = console_drivers;
	int vt_console = 0;

	while (c) {
		if ((c->flags & CON_CONSDEV) && !kdbcons)
			kdbcons = c;
		if ((c->flags & CON_ENABLED) && strncmp(c->name, "tty", 3) == 0) {
			char *p = c->name + 3;
			while (isdigit(*p))
				++p;
			if (*p == '\0')
				vt_console = 1;
		}
		c = c->next;
	}

	if (!kdbcons) {
		printk(KERN_ERR "kdb: Initialization failed - no console.  kdb is disabled.\n");
		kdb_on = 0;
		return -EFAULT;
	}
	if (!vt_console) {
		printk(KERN_ERR "kdb: Initialization failed - no vt console.  kdb is disabled.\n");
		kdb_on = 0;
		return -EFAULT;
	}

	kdb_input_flush();
	return 0;
}
