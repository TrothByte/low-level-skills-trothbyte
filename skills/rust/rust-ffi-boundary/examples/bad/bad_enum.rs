// BAD: plain Rust enum crossed the FFI boundary.
// A plain enum has unspecified layout and is not FFI-safe; rustc only warns
// (improper_ctypes_definitions: "enum has no representation hint"). The C
// side reads an int-sized enum; the Rust side may lay out differently.
pub enum Status {
    Ok,
    Busy,
    Error,
}

#[no_mangle]
pub extern "C" fn classify(s: Status) -> i32 {
    match s {
        Status::Ok => 0,
        Status::Busy => 1,
        Status::Error => 2,
    }
}
