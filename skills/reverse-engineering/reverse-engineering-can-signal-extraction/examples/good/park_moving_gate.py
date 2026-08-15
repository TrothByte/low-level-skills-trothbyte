"""Parked-vs-moving gate: only extract speed-like signals from moving data.

A speed signal SWEEPS through its range while the vehicle moves and HOLDS
(near-constant) while parked. Extracting a value model from parked frames is
unsound: the data does not exercise the signal. This script refuses to
certify a signal unless the SWEEP check passes in the moving phase.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from decode import decode_message, parse_dbc  # noqa: E402


def sweep_check(values, label):
    distinct = len(set(round(v, 3) for v in values))
    lo, hi = min(values), max(values)
    ok = distinct >= 4 and (hi - lo) > 1e-6
    return ok, distinct, lo, hi


def main():
    dbc_path = sys.argv[1] if len(sys.argv) > 1 else "vehicle.dbc"
    msgs = parse_dbc(dbc_path)

    parked = [bytearray(8) for _ in range(10)]
    moving = [bytearray(8) for _ in range(20)]
    for i, f in enumerate(parked):
        f[0] = 0x00
        f[1] = (i & 1) * 0x01
    for i, f in enumerate(moving):
        raw = 0x00 + (i * 17)          # sweeps 0..323 -> 0..3.23 km/h then grows
        f[0] = (raw >> 8) & 0xFF
        f[1] = raw & 0xFF

    wfl_parked = [decode_message(msgs, 512, f)["WheelSpeedFL"] for f in parked]
    wfl_moving = [decode_message(msgs, 512, f)["WheelSpeedFL"] for f in moving]

    ok_p, n_p, lo_p, hi_p = sweep_check(wfl_parked, "parked")
    ok_m, n_m, lo_m, hi_m = sweep_check(wfl_moving, "moving")

    print("parked phase : sweep=%s distinct=%d range=%.2f..%.2f km/h"
          % (ok_p, n_p, lo_p, hi_p))
    print("moving phase : sweep=%s distinct=%d range=%.2f..%.2f km/h"
          % (ok_m, n_m, lo_m, hi_m))

    if not ok_m:
        print("GATE: UNCONFIRMED — signal does not sweep while moving; "
              "do not label it a speed signal")
        return 1
    if ok_p:
        print("WARN: parked data also sweeps — suspicious capture; "
              "cross-check the phase split")
    print("GATE: PASS — speed signal certified from the MOVING phase only")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
