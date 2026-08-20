# USB Device Stack — Reference

Sources: USB 2.0 specification Chapter 9 (descriptors, standard requests,
enumeration); USB Device Class Definition for HID 1.11; USB in a NutShell
(beyondlogic.org); stm32 USB device library / Zephyr USB device stack docs.

## 1. Descriptor hierarchy and byte layouts

- **RULE**: descriptors form a strict tree. Device (one) -> Configuration(s) ->
  Interface(s) -> (class-specific descriptors) -> Endpoint(s). Every descriptor
  starts with bLength (byte 0) then bDescriptorType (byte 1); the host walks
  the tree using bLength, so a wrong bLength corrupts everything after it.
- **WHY AI GETS IT WRONG**: treats descriptors as "a blob of bytes" and writes
  each struct by memory, drifting field order and lengths (e.g. endpoint
  bEndpointAddress written as `uint16_t`).
- **CORRECT REASONING**: each struct size is fixed by the spec; encode them as
  `#pragma pack(1)` C structs and `_Static_assert` the sizes so the compiler
  enforces the layout.
- **EXAMPLE (bad)**: endpoint descriptor with `uint16_t bEndpointAddress`
  packs to 8 bytes, not 7 — every following field shifts and the host stops
  parsing (reproduced in `examples/bad/descriptor_layout.c`).
- **COUNTEREXAMPLE (good)**: `examples/good/descriptor_layout.c` asserts
  sizeof 18/9/9/7 and wTotalLength 32 derived from the structs.
- **VERIFICATION**: `python examples/tools/usb_descriptor_validate.py` on the
  JSON or `.bin` set; `gcc -Wall -Wextra -Werror` on the C fixtures.
- **SOURCE**: `usb20-spec` Table 9-5 (descriptor types), 9-6..9-13.

## 2. Device descriptor (18 bytes, type 0x01)

- **RULE**: layout — bLength 0x12, bDescriptorType 0x01, bcdUSB (LE), class/
  subclass/protocol, bMaxPacketSize0 (8 for low speed; 8/16/32/64 for full and
  high), idVendor, idProduct, bcdDevice, iManufacturer, iProduct,
  iSerialNumber, bNumConfigurations. Device-level class is 0 for most devices
  (class lives in the interfaces).
- **WHY AI GETS IT WRONG**: writes `bMaxPacketSize0` as 64 for a low-speed
  device (host rejects), or sets device class to the interface class (HID/MSC
  belong on the interface).
- **CORRECT REASONING**: EP0 max packet is a per-speed constant; check it
  against the bus the device actually runs. Device class 0 means "class is in
  each interface descriptor".
- **EXAMPLE (bad)**: `bMaxPacketSize0 = 0x33` (51) — not a legal EP0 size;
  host fails enumeration. Caught by the validator's `device-ep0-maxpacket`.
- **VERIFICATION**: validator checks bLength==18/type==0x01, EP0 size, and
  bNumConfigurations against the config descriptors actually present.
- **SOURCE**: `usb20-spec` Table 9-8.

## 3. Configuration descriptor and wTotalLength

- **RULE**: config descriptor is 9 bytes (type 0x02): bLength, type,
  wTotalLength, bNumInterfaces, bConfigurationValue, iConfiguration,
  bmAttributes, bMaxPower. wTotalLength is the total of the configuration
  descriptor PLUS every interface, class-specific, and endpoint descriptor in
  that configuration.
- **WHY AI GETS IT WRONG**: writes wTotalLength from `sizeof(config_desc)`
  alone, forgetting the interfaces and endpoints, or hand-counts wrong.
- **CORRECT REASONING**: derive wTotalLength from the actual serialized bytes
  (`sum(sizeof(...))` over the structs), never type a constant.
- **EXAMPLE (bad)**: wTotalLength 29 while 32 bytes are serialized (one 7-byte
  endpoint not counted) — Windows reports the device cannot be configured.
  Caught by `config-total-length`.
- **VERIFICATION**: validator compares wTotalLength to the serialized group;
  the good MSC fixture has 32 = 9+9+7+7.
- **SOURCE**: `usb20-spec` Table 9-10; `beyondlogic-usb-in-a-nutshell`.

## 4. bmAttributes bit 7 and bMaxPower

- **RULE**: bmAttributes bit 7 (0x80) is REQUIRED for every device (it was
  "reserved" in USB 1.0, must be set in USB 2.0). Bit 6 = self-powered,
  bit 5 = remote wakeup. bMaxPower is in 2 mA units (0x32 = 100 mA).
- **WHY AI GETS IT WRONG**: writes 0x00 or only 0x60, or thinks bit 7 is
  optional.
- **CORRECT REASONING**: the bit is set by construction: `0x80` plus optional
  `0x40` (self-powered) or `0x20` (remote wakeup).
- **VERIFICATION**: validator `config-bmattributes` requires bit 7 set and no
  undefined bits.
- **SOURCE**: `usb20-spec` Table 9-10.

## 5. Descriptor ordering: config -> interface -> class -> endpoint

- **RULE**: within a configuration the byte stream must be: the configuration
  descriptor, then for each interface — the interface descriptor, any
  class-specific descriptors, then the interface's endpoint descriptors.
  Endpoints belong to the interface, never before it.
- **WHY AI GETS IT WRONG**: concatenates endpoint structs first ("the hardware
  registers come first" intuition) or drops the interface descriptor.
- **CORRECT REASONING**: the host parses interface to find its endpoints; an
  endpoint outside any interface is unreachable and often aborts parsing.
- **EXAMPLE (bad)**: endpoint 0x81 serialized before the interface descriptor
  — the host either misparses or the endpoint never appears in
  `lsusb -v`. Caught by `order` and reproduced in `examples/bad/descriptor_layout.c`.
- **VERIFICATION**: validator `order` rule tracks interface presence before
  any endpoint/class descriptor.
- **SOURCE**: `usb20-spec` 9.6.2/9.6.4, Table 9-5 ordering.

## 6. Endpoint descriptor and addressing

- **RULE**: endpoint descriptor is 7 bytes (type 0x05): bLength, type,
  bEndpointAddress, bmAttributes, wMaxPacketSize, bInterval.
  bEndpointAddress: bit 7 = direction (1 IN device->host, 0 OUT host->device),
  bits 3:0 = endpoint number. bmAttributes bits 0:1 = transfer type (0 control,
  1 isochronous, 2 bulk, 3 interrupt).
- **WHY AI GETS IT WRONG**: writes 0x01 for "EP1" forgetting the IN bit
  (should be 0x81), or confuses bits 0:1 with other bits.
- **CORRECT REASONING**: address = `0x80 | endpoint_number` for IN,
  `endpoint_number` for OUT. Transfer type bits are the low two bits of
  bmAttributes.
- **EXAMPLE (bad)**: OUT endpoint declared 0x81 (both IN bit and OUT usage) —
  application writes to an endpoint the host never enumerated. The validator
  and `descriptor_layout.c` print the address so this is visible.
- **VERIFICATION**: validator checks endpoint number 1..15, bLength 7; the
  C fixtures print 0x81 IN vs 0x02 OUT.
- **SOURCE**: `usb20-spec` Table 9-13; `beyondlogic-usb-in-a-nutshell`.

## 7. wMaxPacketSize and bInterval vs speed and transfer type

- **RULE**: EP0: 8 (low), 8/16/32/64 (full/high). Bulk: 8/16/32/64 (full),
  exactly 512 (high); low speed has NO bulk. Interrupt: max 8 (low),
  64 (full), 1024 (high); bInterval 1..255 (low/full), 1..16 (high).
  Isochronous: max 1023 (full), 1024 (high). Bulk bInterval must be 0.
- **WHY AI GETS IT WRONG**: copies a full-speed value into a high-speed device
  (bulk 64 instead of 512) or writes bInterval 0 on an interrupt endpoint.
- **CORRECT REASONING**: pick the speed first, then the transfer type, then the
  legal wMaxPacketSize set; the host enforces these and will drop the endpoint.
- **VERIFICATION**: validator `endpoint-maxpacket` / `endpoint-interval` rules
  take the fixture's `speed` field.
- **SOURCE**: `usb20-spec` Tables 9-7, 9-9, 9-10, 9-13.

## 8. Enumeration state machine and standard requests

- **RULE**: states are Default (powered, no address), Address (SET_ADDRESS
  completed), Configured (SET_CONFIGURATION received). Standard requests:
  GET_DESCRIPTOR, SET_ADDRESS, SET_CONFIGURATION, GET_CONFIGURATION,
  GET_STATUS, SET_FEATURE, CLEAR_FEATURE, SET_INTERFACE, GET_INTERFACE,
  SET_DESCRIPTOR (optional), SYNCH_FRAME (optional). SET_ADDRESS is only valid
  in the Default state; SET_CONFIGURATION only in Address/Configured.
- **WHY AI GETS IT WRONG**: treats requests as "just handle the ones I know"
  and either NAKs unknown requests forever (host gives up) or handles
  SET_ADDRESS as a write of a number that must take effect immediately.
- **CORRECT REASONING**: implement a state enum and a request dispatch:
  known request + correct state -> process; anything else -> STALL (STALL
  handshake on EP0), never NAK indefinitely.
- **EXAMPLE (bad)**: SET_CONFIGURATION answered before the device reaches
  Address state, or GET_DESCRIPTOR(device) only answering the 8-byte prefix
  form forever (see section 9).
- **VERIFICATION**: on hardware — Wireshark/USBPcap trace of enumeration;
  on host — the state machine logic is reviewed against the request table in
  `usb20-spec` Table 9-4.
- **SOURCE**: `usb20-spec` 9.4, Table 9-4; `beyondlogic-usb-in-a-nutshell`
  (chapters 5-6).

## 9. SET_ADDRESS timing and the two-phase GET_DESCRIPTOR

- **RULE**: SET_ADDRESS is a control transfer with the new address applied
  AFTER its STATUS stage — the device must still answer the STATUS stage at the
  old address. GET_DESCRIPTOR(device) is typically issued twice: once with
  wLength=8 to learn bMaxPacketSize0, then again with wLength=18 for the full
  descriptor; the device must answer both.
- **WHY AI GETS IT WRONG**: switches the address at the START of SET_ADDRESS
  (host sees the STATUS stage vanish), or only ever returns 18 bytes for the
  8-byte request.
- **CORRECT REASONING**: honor the control transfer stages (SETUP, DATA, or
  STATUS per the bmRequestType/request) and defer the address change until the
  ZLP status packet completes; return min(wLength, descriptor length) bytes.
- **VERIFICATION**: trace with Wireshark USBPcap: the host's SET_ADDRESS is
  followed by an ACKed STATUS at the old address, then a GET_DESCRIPTOR(device,
  wLength=8) answered with 8 bytes.
- **SOURCE**: `usb20-spec` 9.4.3, 9.4.6; `beyondlogic-usb-in-a-nutshell`.

## 10. Interface class descriptors: HID

- **RULE**: a HID interface (bInterfaceClass 0x03) must have exactly one HID
  descriptor between the interface and its endpoints. HID 1.11 layout:
  bLength 9, bDescriptorType 0x21, bcdHID (e.g. 0x0111), bCountryCode,
  bNumDescriptors (1), bDescriptorType 0x22 (report), wDescriptorLength.
- **WHY AI GETS IT WRONG**: writes the HID descriptor 7 or 8 bytes (the
  historic bug class), wrong bcdHID, or puts it after the endpoints.
- **CORRECT REASONING**: the HID descriptor is 9 bytes in every 1.0/1.1
  version; the report descriptor length must match the actual report blob.
- **EXAMPLE (bad)**: bLength 7 HID descriptor in `examples/bad/bad_descriptors.json`
  — the host walks 7 bytes, hits garbage, and Windows drops the device.
- **VERIFICATION**: validator `hid-descriptor` rule requires bLength 9,
  bcdHID >= 0x0100, one report descriptor with nonzero length; the good HID
  fixture passes.
- **SOURCE**: `hid11-spec` §6.2.1 (HID descriptor table).

## 11. String descriptors and language IDs

- **RULE**: string descriptor 0 is the language-ID array: bLength 4, type 0x03,
  then one or more LANGIDs (0x0409 = English US). Indexes iManufacturer/
  iProduct/iSerialNumber that are 0 mean "no string"; nonzero indexes must have
  a matching descriptor. UTF-16LE payload after the LANGID array.
- **WHY AI GETS IT WRONG**: starts strings at index 1 without the LANGID array
  (host requests index 0 and gets garbage), or declares iProduct=2 with no
  strings at all.
- **CORRECT REASONING**: reserve index 0 for the LANGID array, then use
  indexes 1..n; keep i* fields in sync with the descriptors actually emitted.
- **VERIFICATION**: validator `string-index-zero` rule checks index 0 is the
  4-byte LANGID array and that declared string indexes have descriptors.
- **SOURCE**: `usb20-spec` 9.6.7, Table 9-15.

## 12. Host-side verification loop

- **RULE**: a hand-written descriptor set is never trusted until parsed by a
  host tool and, ideally, enumerated on hardware.
- **WHY AI GETS IT WRONG**: compiles the firmware, sees no build error, and
  declares the USB stack done.
- **CORRECT REASONING**: three gates — (1) byte-level validator on the
  descriptor set (`usb_descriptor_validate.py`), (2) struct-level
  `_Static_assert` compile checks, (3) a real host enumeration
  (`lsusb -v`, usbview, or Wireshark/USBPcap showing the SETUP handshake).
- **VERIFICATION**: the fixture commands in `SKILL.md`; on hardware,
  `lsusb -v -d 1234:5678` must show the interface/endpoint tree that matches
  the intended design.
- **SOURCE**: `zephyr-usb-docs` (device stack architecture); `stm32-usb-lib`.

## Common failure modes

- D11 (bLength/bType wrong): device descriptor 18 vs 18-byte claims; endpoint
  struct packed to 8 bytes.
- D12 (wTotalLength bug): config length omits endpoints — Windows "Unknown
  Device" / configuration failure.
- D13 (ordering): endpoint before interface, HID descriptor after endpoint.
- D14 (speed mismatch): EP0 8 on low, 64/512 bulk wrong for the speed.
- D15 (address bits): EP1 IN written as 0x01.
- D16 (state machine): unknown request NAKed forever; SET_ADDRESS applied too
  early; full descriptor returned for the 8-byte probe.
