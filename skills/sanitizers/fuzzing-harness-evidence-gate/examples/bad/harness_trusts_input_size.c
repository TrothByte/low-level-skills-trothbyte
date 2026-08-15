// intentionally incorrect
// Size is the only trusted length. This harness re-derives a length from the
// fuzzed bytes and copies n attacker-controlled bytes into a stack buffer, so
// ASan fires inside the harness, never in the target (reference rule 3).
#include <stddef.h>
#include <stdint.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
  uint32_t n;
  memcpy(&n, Data, 4);             /* reads 4 bytes even when Size < 4 */
  char buf[16];
  memcpy(buf, Data + 4, n);        /* n is attacker-controlled, not bound by Size */
  return 0;
}
