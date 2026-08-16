# BAD: "fixing" KCSAN reports by silencing every access with a
# data_race()-style annotation, with no rationale for why any race is
# benign. The detector is turned into a rubber stamp.
# Marker: intentionally incorrect
# Run: python examples/bad/kcsan_masking.py

def silence(loc):
    # intentionally incorrect: annotation with no justification; masking,
    # not fixing. Real data_race() requires a documented benign-race reason.
    return f"data_race({loc})  # (no reason recorded)"

def main():
    reports = [
        "data-race in net_rx_path / net_tx_path on g_skb_table",
        "data-race in sysctl_read / sysctl_write on g_timeouts",
        "data-race in irq_handler / worker on g_stats",
    ]
    # intentionally incorrect: every report gets silenced; "KCSAN clean"
    # is printed even though each race is still a real concurrency bug.
    for r in reports:
        loc = r.split(" on ")[1]
        silence(loc)
    print("KCSAN clean: all reports annotated data_race()")
    print("BAD: every race above is still a real race; no benign-race "
          "justification was recorded for any of them")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
