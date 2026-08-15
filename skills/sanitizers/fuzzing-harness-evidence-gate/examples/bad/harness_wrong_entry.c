// intentionally incorrect
// The target is pkt_parse(const uint8_t *buf, size_t len). This harness never
// calls it: it converts Data with atoi and returns. The parser is unreachable,
// so any "clean" result says nothing about the parser (reference rule 1, 7).
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

int pkt_parse(const uint8_t *buf, size_t len);

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
  int port = atoi((const char *)Data);
  (void)port;
  return 0;
}
