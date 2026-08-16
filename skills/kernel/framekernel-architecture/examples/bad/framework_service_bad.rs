// BAD: a service bypasses the framework's typed API and reaches
// framework-internal structures through unsafe pointer arithmetic in the
// shared address space. In the framekernel model this is exactly the
// violation that page tables and Rust typing exist to prevent: the service
// has no legitimate path to framework internals. The fixture compiles (with
// an explicit unsafe block) and "works", which is the trap.
// Compile+run: rustc -O framework_service_bad.rs -o /tmp/f2.exe && /tmp/f2.exe
// Marker: intentionally incorrect
#![allow(dead_code)]

struct FrameworkInternals {
    cred_table: Vec<u64>,   // privileged data the service must not reach
    frame_owner: Vec<u32>,
}

fn main() {
    let fw = Box::new(FrameworkInternals {
        cred_table: vec![0x1111_2222, 0x3333_4444],
        frame_owner: vec![1, 2, 3, 4],
    });

    // intentionally incorrect: the service computes a pointer into the
    // framework's internals from the shared address space and reads
    // privileged data without any token or page-table check.
    let fw_ptr: *const FrameworkInternals = &*fw;
    let mut raw: *mut u64 = unsafe { std::mem::transmute(fw_ptr) };
    unsafe {
        raw = raw.add(2);            // walk past cred_table into frame_owner
        let leak = *raw;
        println!("BAD: service read privileged framework data: {:#x}", leak);
    }
    println!("BAD: unsafe bypass of the framework API - in the framekernel this is a whole-system compromise, not a service-local bug");
}
