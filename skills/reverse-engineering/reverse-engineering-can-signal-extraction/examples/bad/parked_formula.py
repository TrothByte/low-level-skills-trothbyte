# intentionally incorrect — fits and certifies a "speed" formula from PARKED
# data. The parked capture never exercises the signal (it HOLDS near a
# constant), so the fitted model is an artifact, not the speed behavior.
# The script still reports success — this is the FP the parked-vs-moving gate
# exists to stop.
"""BAD: extracts a "speed" formula from PARKED data and certifies it.

The parked capture never exercises the signal (it HOLDS near a constant), so
any formula fit is an artifact of the parked value, not the speed behavior.
The script still reports success — this is the FP failure mode the gate exists
to stop.
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "good"))
from decode import decode_message, parse_dbc  # noqa: E402


def main():
    dbc_path = sys.argv[1] if len(sys.argv) > 1 else "vehicle.dbc"
    msgs = parse_dbc(dbc_path)

    parked = [bytearray(8) for _ in range(20)]
    for f in parked:
        f[0] = 0x00
        f[1] = 0x01

    wfl = [decode_message(msgs, 512, f)["WheelSpeedFL"] for f in parked]
    mean = sum(wfl) / len(wfl)
    print("speed model fitted from parked data: WheelSpeedFL ~ %.4f km/h"
          % mean)
    print("RESULT: speed signal extracted and certified")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
