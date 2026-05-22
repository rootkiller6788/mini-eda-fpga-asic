# mini-riscv-arch — RISC-V Privileged Architecture (C99)

RISC-V privileged architecture implementation: CSR registers, privilege modes,
exception/interrupt handling, MMU with Sv32 paging, and Debug Module.

## Features

- **CSR File**: Full control/status register set (mstatus/mcause/mepc/satp etc.)
- **Privilege Modes**: M/S/U mode switching, trap delegation
- **Exceptions**: All 12 standard exception types + handler dispatch
- **Interrupts**: Software/timer/external IRQs with priority encoding
- **MMU**: Sv32 page table, TLB with LRU eviction, PMP checks
- **Debug**: Debug Module with halt/resume/step/breakpoints/abstract commands

## Build

```sh
make          # build static library and example
make test     # build and run tests
make clean    # remove build artifacts
```

## Course Alignment

- UC Berkeley CS152: RISC-V Computer Architecture
- MIT 6.191: Complex Digital Systems
- CMU 18-447: Computer Architecture

## License

MIT
