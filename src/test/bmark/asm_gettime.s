.text

.globl asm_gettime

.intel_syntax noprefix

asm_gettime:
  # PROLOGUE
  push rbp
  mov rbp, rsp # Setup stack frame

  push rdi
  push rsi

  # BODY
  mov rax, 228 # sys_clock_gettime
  mov rsi, rdi # rsi points to struct timespec
  mov rdi, 1 # CLOCK_MONOTONIC
  syscall

  # EPILOGUE
  pop rsi
  pop rdi
  pop rbp # Restore stack pointer
  ret
