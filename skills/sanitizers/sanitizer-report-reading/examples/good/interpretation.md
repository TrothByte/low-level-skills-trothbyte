# Interpretation of the sample reports (ground truth)

The five fixtures live in `examples/reports/`. They are hand-written, internally-consistent
texts modeled on real sanitizer output; this environment cannot run the sanitizer runtimes.
"Verified here" means checked for self-consistency against this document (addresses, region
bounds, shadow-byte counts, stack cross-references). Live-run verification is UNVERIFIED in
this environment; see `evals/README.md` for the target verification commands.

## 1. asan-heap-oob.txt — heap-buffer-overflow (READ)

- **Category**: `heap-buffer-overflow`, READ of size 2.
- **Access site**: `tools/pktparse.c:42:18` in `parse_record`.
- **Region**: 16 bytes `[0x602000000020,0x602000000030)`, allocated at
  `tools/pktparse.c:87:15` (`__interceptor_malloc` frame above it).
- **Shadow bytes**: `00 00` = the 16 addressable bytes, flanked by `fa` (heap redzones);
  the READ landed on the right redzone — the report says `0 bytes to the right`, so the
  read starts exactly at the region end.
- **Root cause**: the loop bound `i <= hdr->count` (off-by-one) in `parse_record`, so the
  last iteration computes index `count * 4` where the payload holds only `4 * count` bytes
  (indices 0..15, access at index 16).
- **Fix**: change the bound to `i < hdr->count`, or add an explicit index guard
  (`i * 4 + 2 <= payload_len`). Do NOT grow the malloc — the allocation size matches the
  documented contract.

## 2. asan-uaf.txt — heap-use-after-free (READ)

- **Category**: `heap-use-after-free`, READ of size 8.
- **Access site**: `tools/connserver.c:95:11` in `conn_poll` — reads `conn->socket`.
- **Freed by**: `tools/connserver.c:210:5` in `conn_close`, reached from `handle_timeout`
  at `tools/connserver.c:96:9`.
- **Allocated by**: `tools/connserver.c:120:9` in `conn_new`, reached from `accept_conn`
  at `tools/connserver.c:70:9`.
- **Root cause**: the timer callback (`timer_fire`, `tools/connserver.c:44:9`) still holds a
  raw pointer to a connection that `handle_timeout` closed on the same tick; there is no
  reference count or handle validation keeping the object alive while `conn_poll` runs.
- **Fix**: give `conn` a reference count (addref when the timer is armed, release after
  `conn_poll`), or cancel the pending timer inside `conn_close`, or make the callback carry
  a validated handle instead of a raw pointer.

## 3. tsan-race.txt — data race (READ vs WRITE)

- **Category**: `data race`, 4-byte read vs write.
- **Thread T1 read**: `tools/jobsched.c:33:17` in `worker_report`.
- **Thread T0 write**: `tools/jobsched.c:58:9` in `schedule_tick`.
- **Shared object**: heap block of size 24 at `0x7b100000a180` from `stats_new`
  (`tools/jobsched.c:12:9`), allocated by the main thread.
- **Root cause**: `stats->processed` is written by the scheduler thread and read by worker
  threads with no mutex, no atomic, and no happens-before edge between them.
- **Fix**: guard both sides with the same mutex, or make `processed` an `atomic_uint`
  (TSan then sees only the intentional atomic accesses). Synchronize BOTH threads; fixing
  one side only keeps the race.

## 4. ubsan-shift.txt — left shift of negative value

- **Category**: UBSan `left shift of negative value` (signed left shift is UB).
- **Site**: `tools/bitpack.c:19:12` in `pack_flags`, called from `encode_header`
  (`tools/bitpack.c:64:5`).
- **Root cause**: `flags` is a signed `int`; when a flag bit above the sign bit is set the
  value is negative and `flags << n` is UB (C11 6.5.7p3-4).
- **Fix**: shift an unsigned type: `(uint32_t)flags << n`, or declare `flags` unsigned.
  Because UBSan ran in recover mode (default), the process continued after the error — the
  exit code alone is not a clean signal; gate with `-fno-sanitize-recover=undefined`.

## 5. msan-uninit.txt — use-of-uninitialized-value

- **Category**: `use-of-uninitialized-value`.
- **Use site**: `tools/pktcheck.c:25:16` in `checksum` — reads `pkt->hdr.seq`.
- **Origin**: the heap allocation in `alloc_packet` (`tools/pktcheck.c:41:16`) — a
  `malloc(sizeof(pkt_t))` that never wrote `hdr.seq` before the read.
- **Root cause**: the packet header field `seq` is left uninitialized; the allocation never
  received the value the protocol expects.
- **Fix**: initialize at the origin (`pkt->hdr.seq = 0;`) or use `calloc`. The read in
  `checksum` is correct — do not mask or skip it.
