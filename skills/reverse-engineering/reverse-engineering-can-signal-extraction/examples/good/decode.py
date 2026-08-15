"""Minimal DBC decoder — self-contained, no third-party imports.

Implements the two DBC bit-numbering conventions exactly:
  - little-endian (Intel, "@1"): bit n of the frame = byte (n//8), bit (n%8).
    start_bit is the position of the signal's LSB; bits extend upward.
  - big-endian (Motorola, "@0"): bits are numbered in a SAWTOOTH pattern —
    within each byte the numbers run MSB-first. sawtooth number s maps to
    physical (byte s//8, bit 7-(s%8)). start_bit is the position of the
    signal's MSB; bits extend to higher sawtooth numbers.
"""
import re
import sys


def parse_dbc(path):
    msgs = {}
    cur = None
    with open(path) as f:
        for line in f:
            line = line.rstrip("\n")
            if line.startswith("BO_ "):
                parts = line.split()
                mid = int(parts[1])
                cur = {"name": parts[2], "dlc": int(parts[3].split("(")[0]),
                       "signals": []}
                msgs[mid] = cur
            elif line.startswith(" SG_ ") and cur is not None:
                name, spec = line[5:].split(" : ", 1)
                fields = spec.split()
                start_len = fields[0]          # e.g. "16|16@1+"
                sb_s, rest = start_len.split("|")
                len_s, ord_s = rest.split("@")
                m = re.match(r"\(([^,]+),([^)]+)\)", fields[1])
                cur["signals"].append({
                    "name": name.strip(),
                    "start_bit": int(sb_s),
                    "length": int(len_s),
                    "byte_order": ord_s[0],    # '1' = Intel LE, '0' = Motorola BE
                    "value_type": ord_s[1],    # '+' unsigned, '-' signed
                    "factor": float(m.group(1)),
                    "offset": float(m.group(2)),
                })
    return msgs


def extract_linear(frame, lsb, length):
    raw = 0
    for i in range(length):
        idx = lsb + i
        raw |= ((frame[idx // 8] >> (idx % 8)) & 1) << i
    return raw


def extract_sawtooth(frame, msb, length):
    raw = 0
    for i in range(length):
        saw = msb + i
        raw = (raw << 1) | ((frame[saw // 8] >> (7 - (saw % 8))) & 1)
    return raw


def sign_extend(raw, bits):
    sign = 1 << (bits - 1)
    return (raw & (sign - 1)) - sign if raw & sign else raw


def decode_message(msgs, mid, frame):
    msg = msgs[mid]
    out = {}
    for s in msg["signals"]:
        if s["byte_order"] == "1":
            raw = extract_linear(frame, s["start_bit"], s["length"])
        else:
            raw = extract_sawtooth(frame, s["start_bit"], s["length"])
        if s["value_type"] == "-":
            raw = sign_extend(raw, s["length"])
        out[s["name"]] = raw * s["factor"] + s["offset"]
    return out


def main():
    dbc = sys.argv[1] if len(sys.argv) > 1 else "vehicle.dbc"
    msgs = parse_dbc(dbc)

    engine = bytearray(8)
    engine[1] = 0x40             # CoolantTemp raw 0x40 = 64
    engine[2] = 0x11             # EngineSpeed LE u16 low byte
    engine[3] = 0x22             # EngineSpeed LE u16 high byte (bits 0..2 = Gear)
    print("EngineData:", decode_message(msgs, 256, engine))

    state = bytearray(8)
    state[0] = 0x02              # WheelSpeedFL Motorola u16, bytes 0-1
    state[1] = 0x58
    state[4] = 0x02              # WheelSpeedFR Motorola u16, bytes 4-5
    state[5] = 0x26
    state[6] = 0x46              # BrakePedal Motorola u8, byte 6
    print("VehicleState:", decode_message(msgs, 512, state))


if __name__ == "__main__":
    main()
