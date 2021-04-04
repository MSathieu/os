#include <cpu/ioports.h>
#include <cpu/paging.h>
#include <stdio.h>
#include <struct.h>
#include <sys/ioapic.h>
#include <sys/scheduler.h>
#include <sys/syscall.h>

size_t ioports_pid[0x10000];
struct process* isa_irqs_process[16];
static bool isa_irqs_fired[16];

void syscall_grant_ioport(union syscall_args* args) {
  if (args->arg1 || args->arg2 || args->arg3 || args->arg4) {
    puts("Reserved argument is set");
    terminate_current_task(&args->registers);
    return;
  }
  if (!has_process_capability(current_task->process, CAP_IOPORT)) {
    puts("Not allowed to register port access");
    terminate_current_task(&args->registers);
    return;
  }
  if (!current_task->spawned_process) {
    puts("No process is currently being spawned");
    terminate_current_task(&args->registers);
    return;
  }
  if (args->arg0 >= 0x10000) {
    puts("Invalid port");
    terminate_current_task(&args->registers);
    return;
  }
  if (ioports_pid[args->arg0]) {
    puts("Requested port has already been granted");
    terminate_current_task(&args->registers);
    return;
  }
  ioports_pid[args->arg0] = current_task->spawned_process->pid;
  current_task->spawned_process->ioports_assigned = 1;
}
void syscall_access_ioport(union syscall_args* args) {
  if (args->arg4) {
    puts("Reserved argument is set");
    terminate_current_task(&args->registers);
    return;
  }
  if (args->arg0 >= 0x10000) {
    puts("Invalid port");
    terminate_current_task(&args->registers);
    return;
  }
  if (ioports_pid[args->arg0] != current_task->process->pid) {
    puts("No permission to access port");
    terminate_current_task(&args->registers);
    return;
  }
  switch (args->arg1) {
  case 0:
    switch (args->arg2) {
    case 0:
      args->return_value = inb(args->arg0);
      break;
    case 1:
      args->return_value = inw(args->arg0);
      break;
    case 2:
      args->return_value = inl(args->arg0);
      break;
    default:
      puts("Argument out of range");
      terminate_current_task(&args->registers);
    }
    break;
  case 1:
    switch (args->arg2) {
    case 0:
      outb(args->arg0, args->arg3);
      break;
    case 1:
      outw(args->arg0, args->arg3);
      break;
    case 2:
      outl(args->arg0, args->arg3);
      break;
    default:
      puts("Argument out of range");
      terminate_current_task(&args->registers);
    }
    break;
  default:
    puts("Argument out of range");
    terminate_current_task(&args->registers);
  }
}
static void usermode_irq_handler(struct isr_registers* registers) {
  size_t isr = registers->isr;
  struct task* handler = isa_irqs_process[isr - 48]->irq_handler;
  if (handler) {
    handler->blocked = 0;
    handler->registers.rax = isr - 48;
    schedule_task(handler, registers);
    isa_irqs_process[isr - 48]->irq_handler = 0;
  } else {
    isa_irqs_fired[isr - 48] = 1;
  }
}
void syscall_register_irq(union syscall_args* args) {
  if (args->arg2 || args->arg3 || args->arg4) {
    puts("Reserved argument is set");
    terminate_current_task(&args->registers);
    return;
  }
  if (!has_process_capability(current_task->process, CAP_IRQ)) {
    puts("Not allowed to register IRQ");
    terminate_current_task(&args->registers);
    return;
  }
  if (!current_task->spawned_process) {
    puts("No process is currently being spawned");
    terminate_current_task(&args->registers);
    return;
  }
  if (args->arg0) {
    puts("Argument out of range");
    terminate_current_task(&args->registers);
    return;
  }
  if (args->arg1 >= 16) {
    puts("Invalid IRQ");
    terminate_current_task(&args->registers);
    return;
  }
  if (isr_handlers[48 + args->arg1]) {
    puts("Requested IRQ has already been registered");
    terminate_current_task(&args->registers);
    return;
  }
  isa_irqs_process[args->arg1] = current_task->spawned_process;
  register_isa_irq(args->arg1, usermode_irq_handler);
  current_task->spawned_process->irqs_assigned = 1;
}
void syscall_clear_irqs(union syscall_args* args) {
  if (args->arg0 || args->arg1 || args->arg2 || args->arg3 || args->arg4) {
    puts("Reserved argument is set");
    terminate_current_task(&args->registers);
    return;
  }
  if (!current_task->process->irqs_assigned) {
    puts("No permission to handle IRQs");
    terminate_current_task(&args->registers);
    return;
  }
  for (size_t i = 0; i < 16; i++) {
    if (isa_irqs_process[i] == current_task->process) {
      isa_irqs_fired[i] = 0;
    }
  }
}
void syscall_wait_irq(union syscall_args* args) {
  if (args->arg0 || args->arg1 || args->arg2 || args->arg3 || args->arg4) {
    puts("Reserved argument is set");
    terminate_current_task(&args->registers);
    return;
  }
  if (!current_task->process->irqs_assigned) {
    puts("No permission to handle IRQs");
    terminate_current_task(&args->registers);
    return;
  }
  for (size_t i = 0; i < 16; i++) {
    if (isa_irqs_process[i] == current_task->process && isa_irqs_fired[i]) {
      isa_irqs_fired[i] = 0;
      args->return_value = i;
      return;
    }
  }
  current_task->process->irq_handler = current_task;
  block_current_task(&args->registers);
}
void syscall_map_phys_memory(union syscall_args* args) {
  if (args->arg3 || args->arg4) {
    puts("Reserved argument is set");
    terminate_current_task(&args->registers);
    return;
  }
  if (!has_process_capability(current_task->process, CAP_MAP_MEMORY)) {
    puts("No permission to map memory");
    terminate_current_task(&args->registers);
    return;
  }
  if (args->arg0 % 0x1000 || args->arg1 % 0x1000) {
    puts("Invalid address or size");
    terminate_current_task(&args->registers);
    return;
  }
  if (args->arg2 >= 2) {
    puts("Argument out of range");
    terminate_current_task(&args->registers);
    return;
  }
  if (args->arg2 && !current_task->spawned_process) {
    puts("No process is currently being spawned");
    terminate_current_task(&args->registers);
    return;
  }
  if (args->arg2) {
    switch_pml4(current_task->spawned_process->address_space);
    physical_mappings_process = current_task->spawned_process;
  } else {
    physical_mappings_process = current_task->process;
  }
  args->return_value = get_free_ipc_range(args->arg1);
  for (size_t i = 0; i < args->arg1; i += 0x1000) {
    create_mapping(args->return_value + i, args->arg0 + i, 1, 1, 0, 1);
  }
  physical_mappings_process = 0;
  if (args->arg2) {
    switch_pml4(current_task->process->address_space);
  }
}
void syscall_get_fb_info(union syscall_args* args) {
  if (args->arg1 || args->arg2 || args->arg3 || args->arg4) {
    puts("Reserved argument is set");
    terminate_current_task(&args->registers);
    return;
  }
  if (!has_process_capability(current_task->process, CAP_GET_FB_INFO)) {
    puts("No permission to get framebuffer info");
    terminate_current_task(&args->registers);
    return;
  }
  switch (args->arg0) {
  case 0:
    args->return_value = loader_struct.fb_address;
    break;
  case 1:
    args->return_value = loader_struct.fb_width;
    break;
  case 2:
    args->return_value = loader_struct.fb_height;
    break;
  case 3:
    args->return_value = loader_struct.fb_pitch;
    break;
  case 4:
    args->return_value = loader_struct.fb_bpp;
    break;
  case 5:
    args->return_value = loader_struct.fb_red_index;
    break;
  case 6:
    args->return_value = loader_struct.fb_green_index;
    break;
  case 7:
    args->return_value = loader_struct.fb_blue_index;
    break;
  default:
    puts("Argument out of range");
    terminate_current_task(&args->registers);
  }
}
