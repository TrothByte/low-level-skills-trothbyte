// Models common agent mistakes in a Rust-for-Linux module:
//   1. std types (Vec, String) — the kernel links only core, so std does not
//      exist; the kernel crate re-exports allocator types as kernel::alloc.
//   2. std::sync::Mutex — user-space futex code; kernel modules use
//      kernel::sync::Mutex / SpinLock.
//   3. module parameters declared as GLOBAL STATICS instead of inside the
//      kernel::module! { params: ... } block.
//   4. unwrap()/expect() panic paths in module code — a panic in the kernel
//      is an oops, not a recoverable error.
//
// This file COMPILES AND RUNS on the host (it is a plain std program). That
// is exactly the trap: host-runnable Rust is not kernel-runnable Rust. A
// real kernel build (make LLVM=1, CONFIG_RUST) rejects it, and the static
// checker (examples/tools/module_contract_check.py) flags each mistake.
//
// Real kernel compile: target-only (kernel tree + CONFIG_RUST required).
#![allow(dead_code)]

use std::sync::Mutex;
use std::string::String;
use std::vec::Vec;

// WRONG: module parameters must live inside `kernel::module! { params: ... }`
// where the macro can emit the param descriptors; globals like this are not
// visible to the module metadata system and cannot be set from modprobe.
static BUF_SIZE: usize = 4096;
static READ_ONLY: bool = false;

struct RingBuffer {
    data: Vec<u8>,
    head: usize,
}

impl RingBuffer {
    fn new(capacity: usize) -> Self {
        RingBuffer {
            data: vec![0u8; capacity],
            head: 0,
        }
    }

    fn push(&mut self, byte: u8) {
        self.data[self.head] = byte;
        self.head = (self.head + 1) % self.data.len();
    }
}

fn main() {
    // WRONG: std::sync::Mutex is a user-space futex-based lock; the kernel
    // has no std. Even the right-looking kernel path would need context
    // awareness (sleepable Mutex vs SpinLock).
    let lock = Mutex::new(RingBuffer::new(BUF_SIZE));
    let mut buf = lock.lock().unwrap();
    buf.push(0xAA);
    let name = String::from("my_mod");
    println!("host run OK: pushed byte 0xAA into {name} ring buffer");
}
