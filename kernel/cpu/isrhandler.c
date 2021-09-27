#include <cpu/isr.h>
#include <panic.h>
#include <stdio.h>
#include <sys/lapic.h>
#include <sys/lock.h>
#include <sys/scheduler.h>

static const char* exceptions[] = {
  "Divide error",
  "Debug",
  "Non maskable interrupt",
  "Breakpoint",
  "Overflow",
  "Bound range exceeded",
  "Invalid opcode",
  "Device not available",
  "Double fault",
  "Coprocessor segment overrun",
  "Invalid TSS",
  "Segment not present",
  "Stack fault",
  "General protection fault",
  "Page fault",
  "Reserved",
  "x87 floating-point error",
  "Alignment check",
  "Machine check",
  "SIMD floating-point error",
  "Virtualization exception",
  "Control protection exception",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved"};

isr_handler isr_handlers[256];

void isr_handler_common(struct isr_registers* registers) {
  if (broadcasted_nmi) {
    while (1) {
      asm volatile("hlt");
    }
  }
  if (32 <= registers->isr && registers->isr <= 47) {
    return; // PIC interrupt
  }
  if (registers->isr == 255) {
    return; // Spurious interrupt
  }
  acquire_lock();
  if (48 <= registers->isr && registers->isr <= 254) {
    lapic_eoi();
  }
  if (isr_handlers[registers->isr]) {
    isr_handlers[registers->isr](registers);
    return release_lock();
  }
  if (registers->isr < 32) {
    if (registers->cs == 0x23 && registers->isr != 2 && registers->isr != 8) {
      printf("Exception (%s) occurred at 0x%lx in user mode, terminating task\n", exceptions[registers->isr], registers->rip);
      terminate_current_task(registers);
      return release_lock();
    }
    printf("RIP: 0x%lx\n", registers->rip);
    panic(exceptions[registers->isr]);
  }
  printf("Unhandled interrupt %d\n", (int) registers->isr);
  release_lock();
}
