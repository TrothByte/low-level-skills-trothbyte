// Host-side contract model for a Rust-for-Linux module (kernel crate).
//
// This file compiles on the host WITHOUT the kernel tree: the `kernel` crate
// is modeled by the local `kernel` module below, which keeps the same API
// shape — errno-backed kernel::error::Error with `?`, a kernel::module!
// declaration with params in the block, kernel::sync::Mutex (sleepable) and
// GFP-flag-aware allocation, and an unsafe wrapper around a C mock whose
// SAFETY comment states the real contract.
//
// A REAL module is built with `make LLVM=1` against the kernel tree
// (CONFIG_RUST, no_std). This file is only the host-checkable model: the
// exact macro keyword set and type names drift between kernel releases, so
// verify them against rust/kernel/*.rs in the target tree.
//
// Build: rustc --edition 2021 --crate-type lib --crate-name standalone_contract examples/good/standalone_contract.rs -o build/standalone_contract.rlib

#![no_std]
#![allow(dead_code, unused_unsafe, unused_variables)]

// ---------------------------------------------------------------------------
// Mock of the `kernel` crate API surface used by modules.
// ---------------------------------------------------------------------------
mod kernel {
    /// Models kernel::error::Error: wraps a valid negative errno. The real
    /// type guarantees `>= -MAX_ERRNO && < 0` (invariant documented in
    /// rust/kernel/error.rs); `from_errno` rejects out-of-range values.
    pub mod error {
        #[derive(Debug, Clone, Copy, PartialEq, Eq)]
        pub struct Error(core::ffi::c_long);

        impl Error {
            pub const fn code(self) -> i32 {
                self.0 as i32
            }
        }

        pub const EINVAL: Error = Error(-22);
        pub const ENOMEM: Error = Error(-12);

        /// Models kernel::error::Result (aliases Result with Error).
        pub type Result<T> = core::result::Result<T, Error>;
    }

    /// Models kernel::alloc: fallible, flag-aware allocation. The real crate
    /// re-exports Box/KBox, Vec/KVec (modules kbox/kvec) and a Flags type;
    /// older kernels named the flag type GfpFlags (drift — check the tree).
    pub mod alloc {
        #[derive(Debug, Clone, Copy, PartialEq, Eq)]
        pub struct Flags(u32);

        pub const GFP_KERNEL: Flags = Flags(0x10); // sleepable, process ctx
        pub const GFP_ATOMIC: Flags = Flags(0x02); // atomic/IRQ ctx
    }

    /// Models kernel::sync::Mutex: wraps the C `struct mutex` (sleepable,
    /// process context only). For atomic context use SpinLock instead. This
    /// mock is a plain UnsafeCell; the real type is a wrapper over the C
    /// struct with a Guard that holds the lock for its whole lifetime.
    pub mod sync {
        pub struct Mutex<T: ?Sized> {
            value: core::cell::UnsafeCell<T>,
        }

        impl<T> Mutex<T> {
            pub const fn new(value: T) -> Self {
                Mutex {
                    value: core::cell::UnsafeCell::new(value),
                }
            }

            pub fn lock(&self) -> MutexGuard<'_, T> {
                MutexGuard { lock: self }
            }
        }

        pub struct MutexGuard<'a, T: ?Sized> {
            lock: &'a Mutex<T>,
        }

        impl<'a, T> core::ops::Deref for MutexGuard<'a, T> {
            type Target = T;

            fn deref(&self) -> &T {
                // SAFETY: the guard is created only by Mutex::lock, so the
                // C mutex is held for the entire guard lifetime and the
                // value cannot be aliased while this reference exists.
                unsafe { &*self.lock.value.get() }
            }
        }

        impl<'a, T> core::ops::DerefMut for MutexGuard<'a, T> {
            fn deref_mut(&mut self) -> &mut T {
                // SAFETY: same guard invariant as Deref: exclusive access is
                // guaranteed for the lifetime of the guard.
                unsafe { &mut *self.lock.value.get() }
            }
        }
    }
}

/// Mock of the C side, standing in for generated bindings
/// (rust/bindings/bindings_helper.h -> bindgen).
mod c_bindings_mock {
    /// Mock of a C allocation routine that returns a raw buffer.
    /// Contract: on success the pointer is valid and `size` bytes writable;
    /// ownership transfers to the caller, who must pass it to `c_free`.
    pub unsafe fn c_alloc(size: usize) -> *mut u8 {
        // SAFETY: this mock never dereferences or reads the pointer; it only
        // models the binding. The real binding's safety requirements are
        // documented in the generated bindings of the target kernel.
        unsafe { core::ptr::null_mut() }
    }

    /// Mock of the matching C deallocator.
    pub unsafe fn c_free(ptr: *mut u8) {
        // SAFETY: the mock keeps no allocator state; in the real kernel the
        // deallocator must be called with the same GFP context used at
        // allocation time, once, and never again.
        unsafe {
            core::hint::black_box(ptr);
        }
    }
}

// ---------------------------------------------------------------------------
// The module declaration.
// ---------------------------------------------------------------------------

/// Host stand-in for `kernel::module!`. The real macro (rust/kernel/module.rs)
/// emits MODULE_INFO metadata, the parameter descriptors (param.rs), and the
/// init/exit glue. On the host it expands to nothing so the file still
/// compiles; the declaration below documents the real shape.
#[macro_export]
macro_rules! module {
    ($($item:tt)*) => {};
}

// Real kernel syntax (6.x): parameters live INSIDE the macro, never as
// globals. Verify the exact keyword set against rust/kernel/module.rs in the
// target tree — it has been redesigned since the 6.1 merge.
//
// kernel::module! {
//     type: RustForLinuxModule,
//     name: "rust_contract_model",
//     author: "Low-level skills TrothByte",
//     description: "host-side model of the kernel crate module contract",
//     license: "GPL",
//     params: {
//         buf_size: usize { default = 4096 },
//         read_only: bool { default = false },
//     },
// }
module! {
    type: RustForLinuxModule,
    name: "rust_contract_model",
    author: "Low-level skills TrothByte",
    description: "host-side model of the kernel crate module contract",
    license: "GPL",
    params: {
        buf_size: usize { default = 4096 },
        read_only: bool { default = false },
    },
}

pub struct RustForLinuxModule {
    buf_size: usize,
    read_only: bool,
}

// ---------------------------------------------------------------------------
// Init path.
// ---------------------------------------------------------------------------

/// Models `#[unsafe(no_mangle)] pub extern "C" fn init_module() ->
/// kernel::error::Result<...>`. Errors propagate with `?`; a panic here is
/// a kernel oops, so module paths never unwrap or expect.
fn init_module() -> kernel::error::Result<()> {
    // In the real module the params are read from the metadata the macro
    // emitted; the model reads them from the declared struct.
    let params = RustForLinuxModule {
        buf_size: 4096,
        read_only: false,
    };

    // kernel::sync::Mutex (sleepable) is fine in process context. In atomic
    // context this would have to be kernel::sync::SpinLock. std::sync::Mutex
    // does not exist in the kernel.
    let count = kernel::sync::Mutex::new(0u32);
    {
        let mut guard = count.lock();
        *guard += 1;
    }

    // Allocation is fallible and flag-aware: GFP_KERNEL sleeps, so it is only
    // legal in process context; atomic contexts need GFP_ATOMIC.
    let flags = kernel::alloc::GFP_KERNEL;
    let buf = allocate(params.buf_size, flags)?;

    // SAFETY: `allocate` returned a non-null, `buf_size`-byte buffer and
    // transferred ownership to us; the buffer stays valid and exclusively
    // ours until `free_buffer` is called with the same pointer.
    let slice: &mut [u8] = unsafe { core::slice::from_raw_parts_mut(buf, params.buf_size) };
    slice[0] = 42;

    // SAFETY: `buf` is the un-freed pointer from `allocate` above; ownership
    // is returned here, once, at the same GFP context.
    let _ = unsafe { free_buffer_impl(buf) };
    Ok(())
}

fn allocate(size: usize, _flags: kernel::alloc::Flags) -> kernel::error::Result<*mut u8> {
    // SAFETY: c_alloc's contract (valid writable buffer or null) is
    // documented above; we check the null case before any use.
    let ptr = unsafe { c_bindings_mock::c_alloc(size) };
    if ptr.is_null() {
        return Err(kernel::error::ENOMEM);
    }
    Ok(ptr)
}

unsafe fn free_buffer_impl(ptr: *mut u8) {
    // SAFETY: free_buffer_impl is unsafe because the caller must hand over a
    // pointer previously returned by `allocate`; this mock just returns it to
    // the C side, which is the contract the SAFETY line above documents.
    unsafe { c_bindings_mock::c_free(ptr) }
}
