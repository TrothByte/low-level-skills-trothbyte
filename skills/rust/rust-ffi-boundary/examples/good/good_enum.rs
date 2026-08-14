// GOOD: C-compatible enum with explicit discriminants.
// #[repr(C)] fieldless enums are int-sized (4 bytes) while values fit an int,
// matching a C `enum`. Values are written out so edits cannot renumber them.
#[repr(C)]
pub enum Status {
    Ok = 0,
    Busy = 1,
    Error = 2,
}

#[no_mangle]
pub extern "C" fn status_is_ok(s: Status) -> bool {
    matches!(s, Status::Ok)
}
