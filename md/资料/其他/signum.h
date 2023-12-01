/* Signal number definitions.  Linux version.
   Copyright (C) 1995,1996,1997,1998,1999,2003 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <http://www.gnu.org/licenses/>.  */

#ifdef	_SIGNAL_H

/* Fake signal functions.  */
#define SIG_ERR	((__sighandler_t) -1)		/* Error return.  */
#define SIG_DFL	((__sighandler_t) 0)		/* Default action.  */
#define SIG_IGN	((__sighandler_t) 1)		/* Ignore signal.  */

#ifdef __USE_UNIX98
# define SIG_HOLD	((__sighandler_t) 2)	/* Add signal to hold mask.  */
#endif


/* Signals.  */
#define	SIGHUP		1	/*  挂起信号  */
#define	SIGINT		2	/* 中断信号  */
#define	SIGQUIT		3	/* 退出信号 */
#define	SIGILL		4	/* 非法指令  */
#define	SIGTRAP		5	/* 跟踪/断点中断  */
#define	SIGABRT		6	/* 放弃 */
#define	SIGIOT		6	/* IOT trap (4.2 BSD).  */
#define	SIGBUS		7	/* 意味着指针所对应的地址是有效地址，但总线不能正常使用该指针。通常是未对齐的数据访问所致  */
#define	SIGFPE		8	/* 浮点异常  */
#define	SIGKILL		9	/* 删除（不能捕获或者忽略）*/
#define	SIGUSR1		10	/* 用户信号1  */
#define	SIGSEGV		11	/* 意味着指针所对应的地址是无效地址，没有物理内存对应该地址。 */
#define	SIGUSR2		12	/*用户信号2  */
#define	SIGPIPE		13	/* 管道错误  */
#define	SIGALRM		14	/* 闹钟  */
#define	SIGTERM		15	/* 软件终止  */
#define	SIGSTKFLT	16	/* Stack fault.  */
#define	SIGCLD		SIGCHLD	/* Same as SIGCHLD (System V).  */
#define	SIGCHLD		17	/* 子状态改变  */
#define	SIGCONT		18	/*停止的进程继续进行*/
#define	SIGSTOP		19	/* 停止（不能捕获或忽略）  */
#define	SIGTSTP		20	/* 用户停止请求  */
#define	SIGTTIN		21	/* Background read from tty (POSIX).  */
#define	SIGTTOU		22	/* Background write to tty (POSIX).  */
#define	SIGURG		23	/* Urgent condition on socket (4.2 BSD).  */
#define	SIGXCPU		24	/* CPU limit exceeded (4.2 BSD).  */
#define	SIGXFSZ		25	/* File size limit exceeded (4.2 BSD).  */
#define	SIGVTALRM	26	/* Virtual alarm clock (4.2 BSD).  */
#define	SIGPROF		27	/* Profiling alarm clock (4.2 BSD).  */
#define	SIGWINCH	28	/* Window size change (4.3 BSD, Sun).  */
#define	SIGPOLL		SIGIO	/* Pollable event occurred (System V).  */
#define	SIGIO		29	/* I/O now possible (4.2 BSD).  */
#define	SIGPWR		30	/* Power failure restart (System V).  */
#define SIGSYS		31	/* Bad system call.  */
#define SIGUNUSED	31

#define	_NSIG		65	/* Biggest signal number + 1
				   (including real-time signals).  */

#define SIGRTMIN        (__libc_current_sigrtmin ())
#define SIGRTMAX        (__libc_current_sigrtmax ())

/* These are the hard limits of the kernel.  These values should not be
   used directly at user level.  */
#define __SIGRTMIN	32
#define __SIGRTMAX	(_NSIG - 1)

#endif	/* <signal.h> included.  */
