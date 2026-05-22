# mini-supervisor-trap: User-to-Supervisor Trap Demo

Demonstrates the full RISC-V trap flow from U-mode ecall to M-mode handler to mret.

## Flow
1. User mode program executes ECALL
2. Exception raised (cause=8, ECALL from U-mode)
3. Privilege elevates to Machine mode
4. Handler reads mcause/mtval, services request
5. mret returns to user mode

## Run
```sh
cd mini-riscv-arch
make && ./examples/example_riscv.exe
```
