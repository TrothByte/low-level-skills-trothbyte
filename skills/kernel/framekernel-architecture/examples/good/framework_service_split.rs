// GOOD: models the framework/service split in one address space.
// The framework exposes typed, privileged functions; a service may call
// them only when it holds a framework-issued token. Access control is a
// capability-like token, not IPC — the call is a direct function call, as
// in the framekernel model.
// Compile+run: rustc -O framework_service_split.rs -o /tmp/f1.exe && /tmp/f1.exe
#![allow(dead_code)]

#[derive(Clone, Copy, PartialEq)]
struct ServiceToken {
    id: u32,
    rights: u32,
}

const FRAME_WRITE: u32 = 1;
const FRAME_READ: u32 = 2;

struct Framework {
    frames: Vec<Vec<u8>>,
}

impl Framework {
    fn write_frame(&self, token: ServiceToken, idx: usize, data: &[u8]) -> Result<(), &'static str> {
        let _ = data;                  // direct in-address-space copy target
        if token.rights & FRAME_WRITE == 0 {
            return Err("service lacks FRAME_WRITE right");
        }
        if idx >= self.frames.len() {
            return Err("frame index out of range");
        }
        // direct in-address-space access (no IPC copy)
        Ok(())
    }
}

struct Service {
    token: ServiceToken,
}

fn main() {
    let fw = Framework { frames: vec![vec![0u8; 64]; 4] };
    let net_svc = Service { token: ServiceToken { id: 1, rights: FRAME_READ } };
    let block_svc = Service { token: ServiceToken { id: 2, rights: FRAME_READ | FRAME_WRITE } };

    assert!(fw.write_frame(block_svc.token, 2, b"x").is_ok());
    assert!(fw.write_frame(net_svc.token, 2, b"x").is_err(),
            "read-only service must not write a frame");
    println!("GOOD: privilege token gates the direct framework call; read-only service rejected");
}
