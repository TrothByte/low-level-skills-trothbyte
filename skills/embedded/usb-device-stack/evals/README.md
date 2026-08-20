# Evaluation — usb-device-stack

Skill: `skills/embedded/usb-device-stack`.
Stability target: `researched` (needs USB hardware for the final gate) with
host-verified Python validator + gcc fixtures.

## Verified facts (host, recorded 2026-08-20)

Environment: Windows 10/11 PowerShell, Python 3.11, GCC 16.1.0 (MSYS2/MinGW
x86-64), repo `Low-level skills TrothByte`.

1. Good MSC set (`examples/good/usb_msc_descriptors.json`, 9 descriptors)
   validates clean — 13 PASS lines, `RESULT: ALL PASS`, exit 0. The raw
   binary blob (`usb_msc_descriptors.bin`, 116 bytes) round-trips and also
   validates with `--speed full`, exit 0.
2. Good HID set (`examples/good/usb_hid_descriptors.json`, low speed, HID boot
   keyboard) validates clean — 10 PASS lines, exit 0. The HID descriptor
   (bLength 9, type 0x21, bcdHID 0x0111, report len 38) passes the class rule.
3. Bad set (`examples/bad/bad_descriptors.json`) fails with 7 named rules,
   exit 1: `device-ep0-maxpacket` (EP0 0x33), `config-total-length`
   (wTotalLength 29 != 32, endpoint not counted), `config-num-interfaces`
   (2 vs 1), `order` (endpoint 0x81 before interface), `interface-num-endpoints`
   (2 vs 1), and two `hid-descriptor` failures (bLength 7, bNumDescriptors 0).
4. Good C fixture: `gcc -std=c11 -Wall -Wextra -Werror examples/good/descriptor_layout.c`
   compiles and runs, exit 0. Prints: device 18, config 9, interface 9,
   endpoint 7, wTotalLength 32, EP1 IN 0x81, EP1 OUT 0x02, bmAttributes 0x80,
   `DESCRIPTOR LAYOUT OK`.
5. Bad C fixture: the same compile flags build cleanly and the program runs
   (exit 1) printing the bugs: `sizeof endpoint descriptor: 8 (should be 7)`,
   `wTotalLength 26 (should be 32)`, `serialized config bytes 34 (should be
   32)`, then `ORDERING BUG: endpoint 0x81 serialized before the interface
   descriptor`.
6. `python tools/lint/skill_lint.py skills/embedded/usb-device-stack/SKILL.md`
   reports `OK`, exit 0 (0 warnings, 0 errors).

## Synthetic evals

- easy/positive: full-speed MSC set with one interface and two bulk endpoints
  — all validator rules PASS, exit 0.
- easy/positive: low-speed HID boot keyboard — EP0 8, interrupt endpoint 8,
  HID descriptor bLength 9 — all PASS.
- medium/negative: config wTotalLength 29 while 32 bytes serialize (one
  endpoint omitted from the count) — must fail `config-total-length`.
- medium/negative: endpoint 0x81 serialized before the interface — must fail
  `order`.
- hard/negative: HID interface whose HID descriptor is 7 bytes instead of 9 —
  must fail `hid-descriptor` with the length named.
- hard/negative: bad C struct with `uint16_t bEndpointAddress` — must print
  sizeof 8 and the wrong wTotalLength, exit 1.

## False-positive evals

- A correct MSC set with 2 bulk endpoints and correct bNumEndpoints — NOT
  flagged.
- A correct HID 1.11 descriptor (bLength 9, wDescriptorLength 38) — NOT
  flagged; `hid-descriptor` PASS line emitted.
- A low-speed EP0 max packet of 8 — NOT flagged by `device-ep0-maxpacket`.
- bmAttributes 0xA0 (bus-powered + remote wakeup, bit 7 set) — NOT flagged.
- Correct string descriptor 0 (language-ID array, bLength 4) — NOT flagged.

## Historical evals

- HID descriptor length bug class: HID descriptors authored at 7 or 8 bytes
  caused devices to be dropped by Windows and mis-parsed by host stacks;
  represented by `examples/bad/bad_descriptors.json` (HID bLength 7) and the
  `hid-descriptor` rule.
- Config wTotalLength bug class: descriptors whose wTotalLength omits
  endpoint descriptors made Windows report the device as unconfigured /
  "Unknown Device"; represented by the `config-total-length` rule and the bad
  C fixture (wTotalLength 26 vs 32).
- EP0 max-packet bug class: wrong bMaxPacketSize0 (e.g. 51) fails the first
  descriptor fetch at enumeration; represented by `device-ep0-maxpacket`.

## Adversarial evals

- A "correct-looking" descriptor set that serializes the OUT endpoint bytes
  but declares wTotalLength without them — must be caught by
  `config-total-length` even though every individual descriptor has valid
  bLength/bType.
- A plausible HID interface with the HID descriptor silently shortened to 7
  bytes and a wrong bNumDescriptors — both named by `hid-descriptor`.
- A C descriptor struct that looks right but packs to 8 bytes (endpoint
  address as uint16_t), shifting wMaxPacketSize/bInterval offsets — caught by
  the printed sizeof/wTotalLength mismatch in the bad C fixture.
- An endpoint-before-interface byte stream that a naive parser would accept —
  caught by the `order` rule.

## Verification commands (target — hardware + host tooling)

```
# host, reproducible (run from the skill directory)
python examples/tools/usb_descriptor_validate.py examples/good/usb_msc_descriptors.json
python examples/tools/usb_descriptor_validate.py --speed full examples/good/usb_msc_descriptors.bin
python examples/tools/usb_descriptor_validate.py examples/good/usb_hid_descriptors.json
python examples/tools/usb_descriptor_validate.py examples/bad/bad_descriptors.json   # exit 1
gcc -std=c11 -Wall -Wextra -Werror examples/good/descriptor_layout.c -o out && ./out
gcc -std=c11 -Wall -Wextra -Werror examples/bad/descriptor_layout.c -o out && ./out  # exit 1

# target — requires USB hardware (Zephyr / STM32)
west build -b <board> samples/subsys/usb/<hid|mass|cdc>   # then plug in
lsusb -v -d 1234:5678        # must show the interface/endpoint tree
usbview                      # Windows: correct VID/PID and interface count
Wireshark + USBPcap          # capture the enumeration SETUP/ACK handshake
STM32Cube USB device examples + a host enumeration check
```

## Scoring

- detection: names the violated rule (wTotalLength, ordering, EP0 max packet,
  HID length, counts) from the validator FAIL lines or the C fixture output.
- reasoning: explains why wTotalLength must equal the serialized config bytes,
  why an endpoint before an interface is unreachable, and why the HID
  descriptor is 9 bytes.
- fix: corrects the offending bytes/structs so the good fixtures' rules all
  PASS and the C fixture prints the canonical sizes (18/9/9/7, wTotalLength
  32).
- verification: runs the host validator on the fixed set (exit 0) and, on
  hardware, confirms the device enumerates with the expected VID/PID and
  interfaces (`lsusb -v`/usbview).
