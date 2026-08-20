#!/usr/bin/env python3
"""Host-side USB descriptor set validator.

Reads a JSON descriptor set ({"speed": "low|full|high", "descriptors": [[int,...], ...]})
or a raw binary descriptor blob (.bin) and checks the descriptor-layout rules that
firmware authors get wrong when hand-authoring a USB device stack:

  device   : bLength 18 / type 1; EP0 wMaxPacketSize valid for the speed;
             bNumConfigurations matches the number of config descriptors found.
  config   : bLength 9 / type 2; wTotalLength equals the serialized config
             bytes (endpoint/class descriptors must be counted); bNumInterfaces
             matches the interface count; bmAttributes bit 7 set.
  order    : config -> interface -> (class-specific) -> endpoint. An endpoint or
             class descriptor before its interface is a FAIL.
  interface: bLength 9 / type 4; bNumEndpoints matches the endpoint count.
  endpoint : bLength 7 / type 5; endpoint number 1..15; wMaxPacketSize and
             bInterval valid for the speed and transfer type.
  class    : a HID descriptor (type 0x21, bLength 9, valid bcdHID) is required
             between an interface with bInterfaceClass 0x03 and its endpoints.
  strings  : string index 0 must be the language-ID array (bLength 4, type 3).

Exit code 0 = all rules PASS, 1 = at least one FAIL.

Usage:
  python usb_descriptor_validate.py examples/good/usb_msc_descriptors.json
  python usb_descriptor_validate.py --speed full examples/good/usb_msc_descriptors.bin
  python usb_descriptor_validate.py --dump examples/good/usb_msc_descriptors.json
  python usb_descriptor_validate.py --dump-binary out.bin examples/good/usb_msc_descriptors.json
"""
import argparse
import json
import sys

DT_DEVICE = 0x01
DT_CONFIG = 0x02
DT_STRING = 0x03
DT_INTERFACE = 0x04
DT_ENDPOINT = 0x05
DT_HID = 0x21

EP0_SIZES = {"low": (8,), "full": (8, 16, 32, 64), "high": (8, 16, 32, 64)}
BULK_SIZES = {"full": (8, 16, 32, 64), "high": (512,)}
MAX_ISO = {"full": 1023, "high": 1024}
MAX_INT = {"low": 8, "full": 64, "high": 1024}
MAX_INT_INTERVAL = {"low": 255, "full": 255, "high": 16}
ISO_INTERVAL = (1, 16)

CLASS_HID = 0x03

TRANSFER_NAMES = {0: "control", 1: "isochronous", 2: "bulk", 3: "interrupt"}

FAILURES = []


def fail(rule, message):
    FAILURES.append((rule, message))
    print("[FAIL] %-22s %s" % (rule, message))


def le16(b, off):
    return b[off] | (b[off + 1] << 8)


def parse_bin(data, speed):
    blobs = []
    i = 0
    while i < len(data):
        length = data[i]
        if length < 2 or i + length > len(data):
            fail("parse", "malformed descriptor at offset %d: bLength 0x%02X" % (i, length))
            break
        blobs.append(bytes(data[i:i + length]))
        i += length
    return blobs, speed


def load(path):
    if path.endswith(".bin"):
        raw = open(path, "rb").read()
        return parse_bin(raw, "full")
    if path.endswith(".json"):
        with open(path, "r", encoding="utf-8") as fh:
            doc = json.load(fh)
        speed = doc.get("speed", "full")
        if speed not in ("low", "full", "high"):
            fail("parse", "unknown speed %r (want low/full/high)" % speed)
            speed = "full"
        return [bytes(b) for b in doc["descriptors"]], speed
    fail("parse", "unsupported file type (want .json or .bin)")
    return [], "full"


class Checker:
    def __init__(self, blobs, speed):
        self.blobs = blobs
        self.speed = speed
        self.device = blobs[0] if blobs else None
        self.configs = []   # list of {"blob": bytes, "items": [bytes, ...], "index": int}
        self.strings = []
        self._group()

    def _group(self):
        i = 0
        while i < len(self.blobs):
            b = self.blobs[i]
            btype = b[1] if len(b) >= 2 else None
            if btype == DT_CONFIG:
                j = i + 1
                items = [b]
                while j < len(self.blobs):
                    nxt = self.blobs[j]
                    nt = nxt[1] if len(nxt) >= 2 else None
                    if nt in (DT_CONFIG, DT_STRING):
                        break
                    items.append(nxt)
                    j += 1
                self.configs.append({"blob": b, "items": items, "index": len(self.configs) + 1})
                i = j
            elif btype == DT_STRING:
                self.strings.append(b)
                i += 1
            else:
                i += 1


def check_device(chk):
    dev = chk.device
    if dev is None:
        fail("device-descriptor", "no descriptors at all")
        return
    if len(dev) != 18 or dev[0] != 18:
        fail("device-descriptor", "bLength %d / len %d: device descriptor must be 18 bytes, type 1"
             % (dev[0] if dev else 0, len(dev)))
        return
    if dev[1] != DT_DEVICE:
        fail("device-descriptor", "bDescriptorType 0x%02X: expected 0x01" % dev[1])
    else:
        print("[PASS] device-descriptor      bLength 18, bDescriptorType 0x01, bcdUSB 0x%04X"
              % le16(dev, 2))
    mps0 = dev[7]
    if mps0 not in EP0_SIZES[chk.speed]:
        fail("device-ep0-maxpacket", "bMaxPacketSize0 0x%02X (%d): %s-speed EP0 must be %s bytes"
             % (mps0, mps0, chk.speed,
                {8: "8", 16: "16", 32: "32", 64: "64"}.get(mps0) or "8/16/32/64"))
    else:
        print("[PASS] device-ep0-maxpacket    bMaxPacketSize0 0x%02X (%d) valid for %s speed"
              % (mps0, mps0, chk.speed))
    ncfg = dev[17]
    if ncfg != len(chk.configs):
        fail("device-num-configs", "bNumConfigurations %d but %d configuration descriptor(s) present"
             % (ncfg, len(chk.configs)))
    else:
        print("[PASS] device-num-configs     bNumConfigurations %d == %d config descriptor(s)"
              % (ncfg, len(chk.configs)))


def check_configs(chk):
    for cfg in chk.configs:
        idx = cfg["index"]
        blob = cfg["blob"]
        items = cfg["items"]
        if len(blob) != 9 or blob[0] != 9:
            fail("config-descriptor", "config %d: bLength %d / len %d: must be 9 bytes, type 2"
                 % (idx, blob[0], len(blob)))
            continue
        if blob[1] != DT_CONFIG:
            fail("config-descriptor", "config %d: bDescriptorType 0x%02X, expected 0x02" % (idx, blob[1]))
        else:
            print("[PASS] config-descriptor      config %d: bLength 9, type 0x02" % idx)
        declared = le16(blob, 2)
        serialized = sum(len(it) for it in items)
        if declared != serialized:
            fail("config-total-length", "config %d wTotalLength %d != serialized %d (endpoint/class "
                 "descriptors missing from wTotalLength)" % (idx, declared, serialized))
        else:
            print("[PASS] config-total-length    config %d wTotalLength %d == serialized %d bytes"
                  % (idx, declared, serialized))
        bm = blob[7]
        if (bm & 0x80) == 0 or (bm & ~0xE0) != 0:
            fail("config-bmattributes", "config %d bmAttributes 0x%02X: bit 7 must be set (0x80), "
                 "only bits 5-6 (self-powered / remote wakeup) may be added" % (idx, bm))
        else:
            print("[PASS] config-bmattributes    config %d bmAttributes 0x%02X (bit 7 set)" % (idx, bm))
        nifc = blob[4]
        ifc_count = sum(1 for it in items if len(it) >= 2 and it[1] == DT_INTERFACE)
        if nifc != ifc_count:
            fail("config-num-interfaces", "config %d bNumInterfaces %d but %d interface descriptor(s) found"
                 % (idx, nifc, ifc_count))
        else:
            print("[PASS] config-num-interfaces  config %d bNumInterfaces %d == %d interface(s)"
                  % (idx, nifc, ifc_count))


def check_order(chk):
    for cfg in chk.configs:
        idx = cfg["index"]
        saw_interface = False
        ep_seen = 0
        hid_seen = 0
        for it in cfg["items"]:
            if len(it) < 2:
                continue
            t = it[1]
            if t == DT_INTERFACE:
                saw_interface = True
                ep_seen = 0
                hid_seen = 0
            elif t == DT_ENDPOINT:
                if not saw_interface:
                    fail("order", "config %d: endpoint 0x%02X before any interface descriptor"
                         % (idx, it[2] if len(it) > 2 else 0))
                ep_seen += 1
            elif DT_HID <= t <= 0x2F:
                if not saw_interface:
                    fail("order", "config %d: class-specific descriptor (type 0x%02X) before its interface"
                         % (idx, t))
                if ep_seen > 0:
                    fail("order", "config %d: class-specific descriptor (type 0x%02X) after an endpoint "
                         "(must come between interface and endpoints)" % (idx, t))
                if t == DT_HID:
                    hid_seen += 1


def check_interfaces(chk):
    for cfg in chk.configs:
        idx = cfg["index"]
        items = cfg["items"]
        for pos, ifc in enumerate(items):
            if len(ifc) < 2 or ifc[1] != DT_INTERFACE:
                continue
            if len(ifc) != 9 or ifc[0] != 9:
                fail("interface-descriptor", "config %d interface #%d: bLength %d / len %d: must be 9 bytes, type 4"
                     % (idx, pos, ifc[0], len(ifc)))
                continue
            ifnum = ifc[2]
            nendpoints = ifc[4]
            count = 0
            for after in items[pos + 1:]:
                if len(after) >= 2 and after[1] == DT_INTERFACE:
                    break
                if len(after) >= 2 and after[1] == DT_ENDPOINT:
                    count += 1
            if nendpoints != count:
                fail("interface-num-endpoints", "config %d interface %d bNumEndpoints %d but %d endpoint "
                     "descriptor(s) follow it" % (idx, ifnum, nendpoints, count))
            else:
                print("[PASS] interface-num-endpoints config %d interface %d bNumEndpoints %d == %d"
                      % (idx, ifnum, nendpoints, count))


def check_endpoints(chk):
    for cfg in chk.configs:
        idx = cfg["index"]
        for it in cfg["items"]:
            if len(it) < 2 or it[1] != DT_ENDPOINT:
                continue
            if len(it) != 7 or it[0] != 7:
                fail("endpoint-descriptor", "config %d: endpoint 0x%02X bLength %d / len %d: must be 7 bytes, type 5"
                     % (idx, it[2] if len(it) > 2 else 0, it[0], len(it)))
                continue
            addr = it[2]
            num = addr & 0x0F
            if num == 0 or num > 15:
                fail("endpoint-address", "config %d: endpoint address 0x%02X: endpoint number %d out of range 1..15"
                     % (idx, addr, num))
            xfer = it[3] & 0x03
            mps = le16(it, 4)
            expect = None
            if xfer == 0:      # control
                expect = EP0_SIZES[chk.speed]
            elif xfer == 2:    # bulk
                expect = BULK_SIZES.get(chk.speed)
            elif xfer == 3:    # interrupt
                if mps > MAX_INT[chk.speed] or mps < 1:
                    fail("endpoint-maxpacket", "config %d: interrupt endpoint 0x%02X wMaxPacketSize %d "
                         "invalid for %s speed (max %d)" % (idx, addr, mps, chk.speed, MAX_INT[chk.speed]))
                if it[6] < 1 or it[6] > MAX_INT_INTERVAL[chk.speed]:
                    fail("endpoint-interval", "config %d: interrupt endpoint 0x%02X bInterval %d invalid "
                         "(%s speed: 1..%d)" % (idx, addr, it[6], chk.speed, MAX_INT_INTERVAL[chk.speed]))
            elif xfer == 1:    # isochronous
                if mps < 1 or mps > MAX_ISO[chk.speed]:
                    fail("endpoint-maxpacket", "config %d: isochronous endpoint 0x%02X wMaxPacketSize %d "
                         "invalid for %s speed (max %d)" % (idx, addr, mps, chk.speed, MAX_ISO[chk.speed]))
                if it[6] < ISO_INTERVAL[0] or it[6] > ISO_INTERVAL[1]:
                    fail("endpoint-interval", "config %d: isochronous endpoint 0x%02X bInterval %d invalid "
                         "(must be 1..%d)" % (idx, addr, it[6], ISO_INTERVAL[1]))
            if xfer == 2 and it[6] != 0:
                fail("endpoint-interval", "config %d: bulk endpoint 0x%02X bInterval must be 0 (got %d)"
                     % (idx, addr, it[6]))
            if expect is not None and mps not in expect:
                fail("endpoint-maxpacket", "config %d: %s endpoint 0x%02X wMaxPacketSize %d invalid for %s "
                     "speed (must be %s)" % (idx, TRANSFER_NAMES[xfer], addr, mps, chk.speed,
                     ", ".join(str(x) for x in expect)))
            elif expect is not None:
                print("[PASS] endpoint-maxpacket    config %d: %s endpoint 0x%02X wMaxPacketSize %d valid"
                      % (idx, TRANSFER_NAMES[xfer], addr, mps))
            print("[PASS] endpoint-descriptor   config %d: endpoint 0x%02X bLength 7, type 5, %s"
                  % (idx, addr, TRANSFER_NAMES[xfer]))


def check_hid(chk):
    for cfg in chk.configs:
        idx = cfg["index"]
        items = cfg["items"]
        for pos, ifc in enumerate(items):
            if len(ifc) < 2 or ifc[1] != DT_INTERFACE or ifc[5] != CLASS_HID:
                continue
            ifnum = ifc[2]
            group = []
            for after in items[pos + 1:]:
                if len(after) >= 2 and after[1] == DT_INTERFACE:
                    break
                group.append(after)
            hid = [b for b in group if len(b) >= 2 and b[1] == DT_HID]
            problems = []
            if not hid:
                fail("hid-descriptor", "config %d interface %d is HID (class 0x03) but has no HID "
                     "descriptor (type 0x21) between interface and endpoints" % (idx, ifnum))
                continue
            if len(hid) > 1:
                problems.append("has %d HID descriptors (HID 1.11 allows one)" % len(hid))
            h = hid[0]
            if h[0] != 9:
                problems.append("bLength %d, expected 9 (HID 1.11 descriptor is 9 bytes)" % h[0])
            if len(h) >= 4 and le16(h, 2) < 0x0100:
                problems.append("bcdHID 0x%04X too old (want >= 0x0100)" % le16(h, 2))
            if len(h) >= 6 and h[5] < 1:
                problems.append("bNumDescriptors 0 (must be >= 1)")
            if len(h) >= 7 and h[6] != 0x22 and len(h) >= 6 and h[5] >= 1:
                problems.append("report descriptor type 0x%02X, expected 0x22" % h[6])
            if len(h) == 9 and le16(h, 7) == 0:
                problems.append("wDescriptorLength 0 (report descriptor must have a length)")
            if problems:
                for p in problems:
                    fail("hid-descriptor", "config %d interface %d: %s" % (idx, ifnum, p))
            else:
                print("[PASS] hid-descriptor         config %d interface %d: HID descriptor bLength 9, "
                      "bcdHID 0x%04X, report len %d"
                      % (idx, ifnum, le16(h, 2), le16(h, 7)))


def check_strings(chk):
    if not chk.strings:
        return
    s0 = chk.strings[0]
    if len(s0) != 4 or s0[0] != 4 or s0[1] != DT_STRING:
        fail("string-index-zero", "string descriptor 0 must be the language-ID array (bLength 4, type 3); "
             "got bLength %d, type 0x%02X" % (s0[0], s0[1] if len(s0) > 1 else 0))
    else:
        print("[PASS] string-index-zero      string 0 is the language-ID array (bLength 4, "
              "langid 0x%04X)" % le16(s0, 2))
    for i, s in enumerate(chk.strings):
        if len(s) < 2 or s[0] != len(s) or s[1] != DT_STRING:
            fail("string-descriptor", "string descriptor %d bLength %d != actual length %d or type != 0x03"
                 % (i, s[0], len(s)))
    dev = chk.device
    if dev is not None and len(dev) >= 18:
        for field, idx in (("iManufacturer", dev[14]), ("iProduct", dev[15]),
                           ("iSerialNumber", dev[16])):
            if idx != 0 and len(chk.strings) < idx + 1:
                fail("string-index", "%s=%d declared but only %d string descriptor(s) present"
                     % (field, idx, len(chk.strings)))


def main(argv):
    ap = argparse.ArgumentParser(description="USB descriptor set validator")
    ap.add_argument("path", help=".json descriptor set or .bin blob")
    ap.add_argument("--speed", choices=("low", "full", "high"), help="bus speed for .bin input")
    ap.add_argument("--dump", action="store_true", help="print the serialized byte blob (hex text) and exit")
    ap.add_argument("--dump-binary", metavar="OUT", help="write the serialized blob as raw bytes to OUT and exit")
    args = ap.parse_args(argv)

    if args.path.endswith(".bin"):
        raw = open(args.path, "rb").read()
        blobs, speed = parse_bin(raw, args.speed or "full")
    else:
        blobs, speed = load(args.path)
    if args.dump_binary:
        with open(args.dump_binary, "wb") as fh:
            fh.write(b"".join(blobs))
        print("wrote %d bytes to %s" % (sum(len(b) for b in blobs), args.dump_binary))
        return 0
    if args.dump:
        blob = b"".join(blobs)
        print(" ".join("%02X" % b for b in blob))
        print("total %d bytes" % len(blob))
        return 0

    print("speed: %s   descriptors: %d" % (speed, len(blobs)))
    chk = Checker(blobs, speed)
    check_device(chk)
    check_configs(chk)
    check_order(chk)
    check_interfaces(chk)
    check_endpoints(chk)
    check_hid(chk)
    check_strings(chk)

    if FAILURES:
        print("\nRESULT: %d FAIL rule(s)" % len(FAILURES))
        return 1
    print("\nRESULT: ALL PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
