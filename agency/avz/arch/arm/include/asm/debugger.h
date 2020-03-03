/*
 * Copyright (C) 2016,2017 Daniel Rossier <daniel.rossier@soo.tech>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef DEBUGGER_H
#define DEBUGGER_H

#include <soo/uapi/avz.h>


extern void show_registers(struct cpu_user_regs *regs);
extern void show_backtrace_regs(struct cpu_user_regs *);
extern void show_backtrace(ulong sp, ulong lr, ulong pc);

static inline void show_execution_state(struct cpu_user_regs *regs)
{
    show_registers(regs);
}

extern void dump_execution_state(void);

static inline void dump_all_execution_state(void)
{
    ulong sp;
    ulong lr;

    dump_execution_state();
    sp = (ulong)__builtin_frame_address(0);
    lr = (ulong)__builtin_return_address(0);

    show_backtrace(sp, lr, lr);
}

static inline void __force_crash(void)
{
    dump_all_execution_state();
    __builtin_trap();
}

static inline void debugger_trap_immediate(void)
{
    dump_all_execution_state();
#ifdef CRASH_DEBUG
    __builtin_trap();
#endif
}

static inline void unimplemented(void)
{
#ifdef VERBOSE
    dump_all_execution_state();
#endif
    panic(__FUNCTION__);
}

extern void __attn(void);
#define ATTN() __attn();

#define FORCE_CRASH() __force_crash()

#ifdef CRASH_DEBUG

#include <avz/gdbstub.h>

static inline int debugger_trap_fatal(
    unsigned int vector, struct cpu_user_regs *regs)
{
    (void)__trap_to_gdb(regs, vector);
    return vector;
}

#else /* CRASH_DEBUG */

static inline int debugger_trap_fatal(
    unsigned int vector, struct cpu_user_regs *regs)
{
    show_backtrace(regs->r13, regs->r14, regs->r15);
    return vector;
}

#endif /* CRASH_DEBUG */


#endif /* DEBUGGER_H */
