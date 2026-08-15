// Good harness: the target entry is mapped first (pkt_parse), Size is the only
// trusted length, and the call is a pure bytes-to-code bridge (rule 1, 2, 3).
#include <stddef.h>
#include <stdint.h>

int pkt_parse(const uint8_t *buf, size_t len);

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
  if (Size < 8) {
    return 0;
  }
  return pkt_parse(Data, Size);
}
