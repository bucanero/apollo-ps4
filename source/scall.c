#include "scall.h"

int sys_open(const char *path, int flags, int mode) {
    int result;
    int err;
    
    asm volatile(
        ".intel_syntax;"
        "mov rax, 5;"       // System call number for open: 5
        "syscall;"          // Invoke syscall
        : "=a" (result),    // Output operand: result
          "=@ccc" (err)     // Output operand: err (clobbers condition codes)
    );

    return result;
}

int sys_mknod(const char *path, mode_t mode, dev_t dev) {
    int result;
    int err;

    asm volatile(
    ".intel_syntax;"    // Switches the assembly syntax to Intel syntax
    "mov rax, 14;"      // Moves the value 14 into the RAX register (which typically holds the system call number)
    "mov r10, rcx;"     // Moves the value of the RCX register into the R10 register
    "syscall;"          // Executes a system call using the values in the registers
    : "=a"(result),     // Output constraint: Tells the compiler that the result of the operation will be stored in the RAX register
    "=@ccc"(err)        // Output constraint: Indicates that error information will be stored in the specified location
    );

    return result;
}