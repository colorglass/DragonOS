#include "trap.h"
#include "gate.h"
#include <common/kprint.h>
#include <debug/traceback/traceback.h>
#include <process/process.h>
#include <process/ptrace.h>
#include <sched/sched.h>
#include <arch/arch.h>

extern void ignore_int();

// 0 #DE 除法错误
void do_divide_error(struct pt_regs *regs, unsigned long error_code)
{
    // kerror("do_divide_error(0)");
    kerror("do_divide_error(0),\tError Code:%#18lx,\tRSP:%#18lx,\tRIP:%#18lx\t CPU:%d\t pid=%d\n", error_code,
           regs->rsp, regs->rip, rs_current_cpu_id(), rs_current_pcb_pid());
    traceback(regs);

    rs_process_do_exit(-1);
}

// 1 #DB 调试异常
void do_debug(struct pt_regs *regs, unsigned long error_code)
{
    printk("[ ");
    printk_color(RED, BLACK, "ERROR / TRAP");
    printk(" ] do_debug(1),\tError Code:%#18lx,\tRSP:%#18lx,\tRIP:%#18lx\t CPU:%d, pid:%d\n", error_code, regs->rsp, regs->rip,
           rs_current_pcb_cpuid(), rs_current_pcb_pid());

    while (1)
        hlt();
}

// 2 不可屏蔽中断
void do_nmi(struct pt_regs *regs, unsigned long error_code)
{

    printk("[ ");
    printk_color(BLUE, BLACK, "INT");
    printk(" ] do_nmi(2),\tError Code:%#18lx,\tRSP:%#18lx,\tRIP:%#18lx\t CPU:%d\n", error_code, regs->rsp, regs->rip,
           rs_current_pcb_cpuid());

    while (1)
        hlt();
}

// 3 #BP 断点异常
void do_int3(struct pt_regs *regs, unsigned long error_code)
{

    printk("[ ");
    printk_color(YELLOW, BLACK, "TRAP");
    printk(" ] do_int3(3),\tError Code:%#18lx,\tRSP:%#18lx,\tRIP:%#18lx\t CPU:%d\n", error_code, regs->rsp, regs->rip,
           rs_current_pcb_cpuid());

    while (1)
        hlt();
}

// 4 #OF 溢出异常
void do_overflow(struct pt_regs *regs, unsigned long error_code)
{

    printk("[ ");
    printk_color(YELLOW, BLACK, "TRAP");
    printk(" ] do_overflow(4),\tError Code:%#18lx,\tRSP:%#18lx,\tRIP:%#18lx\t CPU:%d\n", error_code, regs->rsp,
           regs->rip, rs_current_pcb_cpuid());
    rs_process_do_exit(-1);
}

// 5 #BR 越界异常
void do_bounds(struct pt_regs *regs, unsigned long error_code)
{

    kerror("do_bounds(5),\tError Code:%#18lx,\tRSP:%#18lx,\tRIP:%#18lx\t CPU:%d\n", error_code, regs->rsp, regs->rip,
           rs_current_pcb_cpuid());

    while (1)
        hlt();
}

// 6 #UD 无效/未定义的机器码
void do_undefined_opcode(struct pt_regs *regs, unsigned long error_code)
{

    kerror("do_undefined_opcode(6),\tError Code:%#18lx,\tRSP:%#18lx,\tRIP:%#18lx\t CPU:%d, pid:%ld", error_code,
           regs->rsp, regs->rip, rs_current_pcb_cpuid(), rs_current_pcb_pid());
    traceback(regs);
    rs_process_do_exit(-1);
}

// 7 #NM 设备异常（FPU不存在）
void do_dev_not_avaliable(struct pt_regs *regs, unsigned long error_code)
{

    kerror("do_dev_not_avaliable(7),\tError Code:%#18lx,\tRSP:%#18lx,\tRIP:%#18lx\t CPU:%d, pid=%d\n", error_code, regs->rsp,
           regs->rip, rs_current_pcb_cpuid(), rs_current_pcb_pid());

    rs_process_do_exit(-1);
}

// 8 #DF 双重错误
void do_double_fault(struct pt_regs *regs, unsigned long error_code)
{

    printk("[ ");
    printk_color(RED, BLACK, "Terminate");
    printk(" ] do_double_fault(8),\tError Code:%#18lx,\tRSP:%#18lx,\tRIP:%#18lx\t CPU:%d\n", error_code, regs->rsp,
           regs->rip, rs_current_pcb_cpuid());
    traceback(regs);
    rs_process_do_exit(-1);
}

// 9 协处理器越界（保留）
void do_coprocessor_segment_overrun(struct pt_regs *regs, unsigned long error_code)
{

    kerror("do_coprocessor_segment_overrun(9),\tError Code:%#18lx,\tRSP:%#18lx,\tRIP:%#18lx\t CPU:%d\n", error_code,
           regs->rsp, regs->rip, rs_current_pcb_cpuid());

    rs_process_do_exit(-1);
}

// 10 #TS 无效的TSS段
void do_invalid_TSS(struct pt_regs *regs, unsigned long error_code)
{

    printk("[");
    printk_color(RED, BLACK, "ERROR");
    printk("] do_invalid_TSS(10),\tError Code:%#18lx,\tRSP:%#18lx,\tRIP:%#18lx\t CPU:%d\n", error_code, regs->rsp,
           regs->rip, rs_current_pcb_cpuid());

    printk_color(YELLOW, BLACK, "Information:\n");
    // 解析错误码
    if (error_code & 0x01)
        printk("The exception occurred during delivery of an event external to the program.\n");

    if (error_code & 0x02)
        printk("Refers to a descriptor in the IDT.\n");
    else
    {
        if (error_code & 0x04)
            printk("Refers to a descriptor in the current LDT.\n");
        else
            printk("Refers to a descriptor in the GDT.\n");
    }

    printk("Segment Selector Index:%10x\n", error_code & 0xfff8);

    printk("\n");

    rs_process_do_exit(-1);
}

// 11 #NP 段不存在
void do_segment_not_exists(struct pt_regs *regs, unsigned long error_code)
{

    kerror("do_segment_not_exists(11),\tError Code:%#18lx,\tRSP:%#18lx,\tRIP:%#18lx\t CPU:%d\n", error_code, regs->rsp,
           regs->rip, rs_current_pcb_cpuid());
    rs_process_do_exit(-1);
}

// 12 #SS SS段错误
void do_stack_segment_fault(struct pt_regs *regs, unsigned long error_code)
{

    kerror("do_stack_segment_fault(12),\tError Code:%#18lx,\tRSP:%#18lx,\tRIP:%#18lx\t CPU:%d\n", error_code, regs->rsp,
           regs->rip, rs_current_pcb_cpuid());
    // kinfo("cs=%#04x, ds=%#04x, ss=%#04x", regs->cs, regs->ds, regs->ss);
    traceback(regs);
    rs_process_do_exit(-1);
}

// 13 #GP 通用保护性异常
void do_general_protection(struct pt_regs *regs, unsigned long error_code)
{

    kerror("do_general_protection(13),\tError Code:%#18lx,\tRSP:%#18lx,\tRIP:%#18lx\t CPU:%d\tpid=%ld\n", error_code,
           regs->rsp, regs->rip, rs_current_pcb_cpuid(), rs_current_pcb_pid());
    if (error_code & 0x01)
        printk_color(RED, BLACK,
                     "The exception occurred during delivery of an event external to the program,such as an interrupt "
                     "or an earlier exception.\n");

    if (error_code & 0x02)
        printk_color(RED, BLACK, "Refers to a gate descriptor in the IDT;\n");
    else
        printk_color(RED, BLACK, "Refers to a descriptor in the GDT or the current LDT;\n");

    if ((error_code & 0x02) == 0)
        if (error_code & 0x04)
            printk_color(RED, BLACK, "Refers to a segment or gate descriptor in the LDT;\n");
        else
            printk_color(RED, BLACK, "Refers to a descriptor in the current GDT;\n");

    printk_color(RED, BLACK, "Segment Selector Index:%#010x\n", error_code & 0xfff8);
    traceback(regs);
    rs_process_do_exit(-1);
}

// 14 #PF 页故障
void do_page_fault(struct pt_regs *regs, unsigned long error_code)
{
    cli();
    unsigned long cr2 = 0;
#if ARCH(I386) || ARCH(X86_64)
    __asm__ __volatile__("movq	%%cr2,	%0" : "=r"(cr2)::"memory");
#endif
    kerror("do_page_fault(14),Error code :%#018lx,RSP:%#018lx, RBP=%#018lx, RIP:%#018lx CPU:%d, pid=%d\n", error_code,
           regs->rsp, regs->rbp, regs->rip, rs_current_pcb_cpuid(), rs_current_pcb_pid());
    kerror("regs->rax = %#018lx\n", regs->rax);
    if (!(error_code & 0x01))
        printk_color(RED, BLACK, "Page Not-Present,\t");

    if (error_code & 0x02)
        printk_color(RED, BLACK, "Write Cause Fault,\t");
    else
        printk_color(RED, BLACK, "Read Cause Fault,\t");

    if (error_code & 0x04)
        printk_color(RED, BLACK, "Fault in user(3)\t");
    else
        printk_color(RED, BLACK, "Fault in supervisor(0,1,2)\t");

    if (error_code & 0x08)
        printk_color(RED, BLACK, ",Reserved Bit Cause Fault\t");

    if (error_code & 0x10)
        printk_color(RED, BLACK, ",Instruction fetch Cause Fault");

    printk_color(RED, BLACK, "\n");

    printk_color(RED, BLACK, "CR2:%#018lx\n", cr2);

    traceback(regs);
    sti();
    rs_process_do_exit(-1);
    // current_pcb->state = PROC_STOPPED;
    // sched();
}

// 15 Intel保留，请勿使用

// 16 #MF x87FPU错误
void do_x87_FPU_error(struct pt_regs *regs, unsigned long error_code)
{

    kerror("do_x87_FPU_error(16),\tError Code:%#18lx,\tRSP:%#18lx,\tRIP:%#18lx\t CPU:%d\n", error_code, regs->rsp,
           regs->rip, rs_current_pcb_cpuid());

    while (1)
        hlt();
}

// 17 #AC 对齐检测
void do_alignment_check(struct pt_regs *regs, unsigned long error_code)
{

    kerror("do_alignment_check(17),\tError Code:%#18lx,\tRSP:%#18lx,\tRIP:%#18lx\t CPU:%d\n", error_code, regs->rsp,
           regs->rip, rs_current_pcb_cpuid());

    rs_process_do_exit(-1);
}

// 18 #MC 机器检测
void do_machine_check(struct pt_regs *regs, unsigned long error_code)
{

    kerror("do_machine_check(18),\tError Code:%#18lx,\tRSP:%#18lx,\tRIP:%#18lx\t CPU:%d\n", error_code, regs->rsp,
           regs->rip, rs_current_pcb_cpuid());

    rs_process_do_exit(-1);
}

// 19 #XM SIMD浮点异常
void do_SIMD_exception(struct pt_regs *regs, unsigned long error_code)
{

    kerror("do_SIMD_exception(19),\tError Code:%#18lx,\tRSP:%#18lx,\tRIP:%#18lx\t CPU:%d\n", error_code, regs->rsp,
           regs->rip, rs_current_pcb_cpuid());

    rs_process_do_exit(-1);
}

// 20 #VE 虚拟化异常
void do_virtualization_exception(struct pt_regs *regs, unsigned long error_code)
{

    kerror("do_virtualization_exception(20),\tError Code:%#18lx,\tRSP:%#18lx,\tRIP:%#18lx\t CPU:%d\n", error_code,
           regs->rsp, regs->rip, rs_current_pcb_cpuid());

    rs_process_do_exit(-1);
}

// 21-21 Intel保留，请勿使用

/**
 * @brief 当系统收到未知的中断时，执行此处理函数
 *
 * @param regs
 * @param error_code
 */
void ignore_int_handler(struct pt_regs *regs, unsigned long error_code)
{
    kwarn("Unknown interrupt or fault at RIP.\n");
}

void sys_vector_init()
{

#if ARCH(I386) || ARCH(X86_64)
    // 将idt重置为新的ignore_int入点（此前在head.S中有设置，
    // 但是那个不完整，某些版本的编译器的输出，在真机运行时会破坏进程执行环境，从而导致#GP
    for (int i = 0; i < 256; ++i)
        set_intr_gate(i, 0, ignore_int);

    set_intr_gate(0, 0, divide_error);
    set_intr_gate(1, 0, debug);
    set_intr_gate(2, 0, nmi);
    set_system_trap_gate(3, 0, int3);
    set_system_trap_gate(4, 0, overflow);
    set_system_trap_gate(5, 0, bounds);
    set_intr_gate(6, 0, undefined_opcode);
    set_intr_gate(7, 0, dev_not_avaliable);
    set_intr_gate(8, 0, double_fault);
    set_intr_gate(9, 0, coprocessor_segment_overrun);
    set_intr_gate(10, 0, invalid_TSS);
    set_intr_gate(11, 0, segment_not_exists);
    set_intr_gate(12, 0, stack_segment_fault);
    set_intr_gate(13, 0, general_protection);
    set_intr_gate(14, 0, page_fault);
    // 中断号15由Intel保留，不能使用
    set_intr_gate(16, 0, x87_FPU_error);
    set_intr_gate(17, 0, alignment_check);
    set_intr_gate(18, 0, machine_check);
    set_intr_gate(19, 0, SIMD_exception);
    set_intr_gate(20, 0, virtualization_exception);
    // 中断号21-31由Intel保留，不能使用

    // 32-255为用户自定义中断内部
#endif
}