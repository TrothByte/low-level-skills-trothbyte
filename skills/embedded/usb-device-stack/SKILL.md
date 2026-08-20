---
name: usb-device-stack
description: Use when writing or reviewing USB device firmware — descriptors, endpoints, configuration/interface hierarchies, enumeration, or class descriptors. Teaches the descriptor layout and enumeration state machine agents get wrong when hand-authoring USB stacks.
---

# USB Device Stack: Descriptors and Enumeration

## When to use

- Writing or reviewing firmware that implements a USB device stack:
  descriptor tables, EP0 request handling, endpoint setup.
- Hand-authoring descriptor arrays in C (bare metal, STM32 USB-FS/HS, Zephyr,
  TinyUSB, or a vendor HAL).
- Debugging enumeration that fails on a real host: "device not recognized",
  "Unknown Device", no VID/PID shown in the OS.
- Adding a class (HID keyboard/mouse, MSC, CDC-ACM) on top of an existing
  device stack.
- Reviewing LLM-generated USB code where descriptor bytes "look plausible"
  but do not survive a host-side check.

## When not to use

- Writing a USB host stack — this skill is firmware-side, answering host
  enumeration.
- Debugging the USB physical layer (D+/D- routing, signal integrity, crystal
  clocking) — use `embedded-board-bringup-peripheral-init`.
- Choosing USB vs SPI/I2C at the system level, or OS/kernel USB drivers.
- Enumeration failures that turn out to be MPU/caching or register-ordering
  problems — use `embedded-volatile-and-memory-ordering`.

## What the agent often gets wrong

1. bLength/bDescriptorType wrong per descriptor — the classic is a config
   descriptor whose wTotalLength forgets the endpoint descriptors, or a HID
   descriptor with the wrong length.
2. Descriptor ORDER: config -> interface -> (class-specific) -> endpoint.
   Agents place endpoints before the interface descriptor, or omit the
   interface entirely.
3. wMaxPacketSize wrong: full-speed EP0 must be 8/16/32/64, high-speed bulk
   must be 512; wrong bInterval on interrupt endpoints.
4. Endpoint address bits: bEndpointAddress bit 7 = direction (IN), bits 3:0 =
   endpoint number. Agents write "EP1" as 0x01 instead of 0x81.
5. bNumInterfaces/bNumEndpoints inconsistent with the descriptors actually
   serialized.
6. STALL behavior: the device must STALL unknown standard requests; agents NAK
   forever or crash the stack on unexpected setups.
7. SET_ADDRESS sequencing: the new address applies only after the STATUS stage
   of the control transfer; agents switch too early or too late.
8. bmAttributes bit 7 must be set (0x80) even for bus-powered devices; agents
   clear it.
9. String descriptors: index 0 is the language-ID array; iManufacturer/
   iProduct = 0 means "no string".

## How to reason correctly

1. Write descriptors bottom-up but verify top-down: build endpoint -> interface
   -> class -> config -> device, then validate wTotalLength against the actual
   serialized bytes.
2. Keep a single source of truth (structs/arrays) and derive wTotalLength,
   bNumInterfaces and bNumEndpoints programmatically — never hand-count.
3. Model enumeration as a state machine (Default -> Address -> Configured) and
   for every standard request decide whether it is legal in the current state;
   STALL everything else.
4. Verify on a real host or host-side tool: usbview, `lsusb -v`, Wireshark
   USBPcap — a real enumeration is the gate.
5. Make class descriptors match the interface class code: a HID descriptor
   (type 0x21) only under bInterfaceClass 0x03, CDC functional descriptors only
   under 0x02.

## What to verify

- Every descriptor parses with the correct bLength/bType/order; wTotalLength
  equals the actual serialized config length.
- Enumeration completes: the host shows the right VID/PID and interface count.
- Endpoint addresses/directions match the transfer types the application
  actually uses.
- Standard requests are handled and unknown requests STALL, never NAK/crash.
- No ABI/packing issues in descriptor structs (see
  `embedded-volatile-and-memory-ordering` for #pragma pack rules).

## How to verify

```
python examples/tools/usb_descriptor_validate.py examples/good/usb_msc_descriptors.json   # exit 0
python examples/tools/usb_descriptor_validate.py --speed full examples/good/usb_msc_descriptors.bin   # exit 0
python examples/tools/usb_descriptor_validate.py examples/good/usb_hid_descriptors.json  # exit 0
python examples/tools/usb_descriptor_validate.py examples/bad/bad_descriptors.json       # exit 1, FAIL lines name each bug
gcc -std=c11 -Wall -Wextra -Werror examples/good/descriptor_layout.c -o out && ./out     # exit 0
gcc -std=c11 -Wall -Wextra -Werror examples/bad/descriptor_layout.c -o out && ./out      # exit 1, prints the bugs
```

Target (needs USB hardware): `lsusb -v` on the enumerated device, `usbview`,
Wireshark with USBPcap, Zephyr `west build` of a USB sample plus a host
enumeration check, STM32Cube USB device examples.

## Where the knowledge comes from

- USB 2.0 specification — Chapter 9 (https://www.usb.org/document-library/usb-20-specification)
- USB Device Class Specification for HID (https://www.usb.org/sites/default/files/hid1_11.pdf)
- USB in a NutShell (https://www.beyondlogic.org/usbnutshell/usb1.shtml)
- stm32 USB device library / Zephyr USB device stack docs (https://docs.zephyrproject.org/latest/hardware/peripherals/usb.html)

## Related skills

- `embedded-hw-register-datasheet-verification` — encode the spec as
  compilable C with static asserts; same discipline for descriptor structs.
- `embedded-board-bringup-peripheral-init` — clocking and init order a USB
  peripheral needs before it can enumerate.
- `pcie-config-space` — sibling link-level configuration space: capability
  ordering and length bugs in config headers.
- `embedded-volatile-and-memory-ordering` — #pragma pack / ABI and volatile
  rules for descriptor structs and shared endpoint FIFOs.
- `embedded-interrupt-and-nested` — endpoint IRQ handlers and ISR/main
  concurrency around completion flags.

## Evaluation

- Synthetic: the good MSC and HID descriptor sets pass the validator with exit
  0; the bad set fails with one named rule per bug (EP0 max packet, missing
  endpoint in wTotalLength, endpoint-before-interface ordering, HID length,
  interface/endpoint counts); the bad C prints its wrong lengths and exits 1.
- False-positive: a correct MSC set with two bulk endpoints, a correct HID
  1.11 descriptor (bLength 9), and a low-speed EP0 of 8 must NOT be flagged.
- Adversarial: a plausible descriptor set with the HID descriptor silently
  shortened to 7 bytes, or wTotalLength omitting one endpoint while the bytes
  are still serialized, must be caught by name.
- Historical: the HID-descriptor-length bug class (hosts drop devices with
  malformed class descriptors) and the config wTotalLength bug class (Windows
  fails configuration / reports "Unknown Device") are both represented by the
  fixtures.
- Verified facts: host runs recorded in `evals/README.md` (validator exit
  codes, gcc exit codes, printed lengths).
