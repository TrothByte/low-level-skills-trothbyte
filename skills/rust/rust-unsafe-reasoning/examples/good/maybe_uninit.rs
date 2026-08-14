use std::mem::MaybeUninit;

fn make_vec(n: usize) -> Vec<u8> {
    let mut slots: Vec<MaybeUninit<u8>> = Vec::with_capacity(n);
    slots.resize_with(n, MaybeUninit::<u8>::uninit);
    for (i, slot) in slots.iter_mut().enumerate() {
        slot.write(i as u8);
    }
    // SAFETY: every slot was written by the loop above before assume_init.
    unsafe { slots.into_iter().map(|s| s.assume_init()).collect() }
}

fn main() {
    let v = make_vec(4);
    println!("{:?}", v);
}
