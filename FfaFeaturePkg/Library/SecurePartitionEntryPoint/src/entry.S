/*
 * Copyright 2025 The aarch64-rt Authors.
 *
 * This project is dual-licensed under Apache 2.0 and MIT terms.
 * See LICENSE-APACHE and LICENSE-MIT for details.
 */

//
//  Copyright (c) 2021, ARM Limited. All rights reserved.
//
//  SPDX-License-Identifier: BSD-2-Clause-Patent
//
//

.macro adr_l, reg:req, sym:req
    adrp \reg, \sym
    add \reg, \reg, :lo12:\sym
.endm


.macro FfaMemPermSet start:req end:req perm:req
  adrp x29, \start
  add x29, x29, :lo12: \start

  adrp x30, \end
  add x30, x30, :lo12:\end

  /* x30 = end - begin */
  sub x30, x30, x29
  /* x28 = x30 >> 12 (number of pages) */
  mov x28, #12
  lsrv x28, x30, x28

  mov w0, #0x89
  movk w0, #0x8400, lsl #16
  mov x1, x29
  mov x2, x28
  mov w3, #\perm

  svc #0

  mov w1, #0x61
  movk w1, #0x8400, lsl #16
  cmp w1, w0
  b.ne .
.endm

/**
 * This is a generic entry point for an image. It carries out the operations required to prepare the
 * loaded image to be run. Specifically, it zeroes the bss section using registers x25 and above,
 * prepares the stack, enables floating point, and sets up the exception vector. It preserves x0-x3
 * for the Rust entry point, as these may contain boot parameters.
 */
.section .init.entry, "ax"
.global entry
entry:
    // Stash boot information registers from the SPMC
    mov x8, x0
    mov x9, x1
    mov x10, x2
    mov x11, x3

    // Set the correct permissions on stack memory
    FfaMemPermSet boot_stack_begin boot_stack_end 0x5

    /* Prepare the stack. */
    adr_l x30, boot_stack_end
    mov sp, x30

    // Restore boot information registers from the SPMC
    mov x0, x8
    mov x1, x9
    mov x2, x10
    mov x3, x11

    /* Call into Rust code. */
    b rust_entry

// /**
//  * Entry point for secondary cores.
//  *
//  * This requires that an initial stack pointer value be passed in x0, and the stack must contain
//  * the address of a Rust entry point to jump to and a parameter value to pass to it.
//  */
// .global secondary_entry
// secondary_entry:
//     bl enable_mmu

//     /* Disable trapping floating point access in EL1. */
//     mrs x30, cpacr_el1
//     orr x30, x30, #(0x3 << 20)
//     msr cpacr_el1, x30
//     isb

//     /* Set the stack pointer which was passed. */
//     mov sp, x0

//     /*
//      * Load Rust entry point address and argument from the bottom of the stack into callee-saved
//      * registers.
//      */
//     ldp x19, x20, [sp, #-16]

//     /* Set the exception vector. */
//     bl set_exception_vector

//     /* Pass argument to Rust entry point. */
//     mov x0, x19

//     /* Call into Rust code. */
//     br x20
