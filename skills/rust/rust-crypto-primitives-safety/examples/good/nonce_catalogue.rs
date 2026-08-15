use std::collections::HashSet;

struct NonceCatalogue {
    used: HashSet<u64>,
}

impl NonceCatalogue {
    fn new() -> Self {
        NonceCatalogue { used: HashSet::new() }
    }
    fn issue(&mut self, nonce: u64) -> Result<u64, String> {
        if self.used.contains(&nonce) {
            return Err(format!("nonce {nonce:#018x} already used"));
        }
        self.used.insert(nonce);
        Ok(nonce)
    }
}

fn main() {
    let mut cat = NonceCatalogue::new();
    let n1 = cat.issue(0x1122_3344_5566_7788).unwrap();
    let n2 = cat.issue(0x1122_3344_5566_7789).unwrap();
    println!("issued: {n1:#018x} {n2:#018x}");
    match cat.issue(n1) {
        Err(e) => println!("reuse rejected: {e}"),
        Ok(_) => println!("reuse not detected"),
    }
}
