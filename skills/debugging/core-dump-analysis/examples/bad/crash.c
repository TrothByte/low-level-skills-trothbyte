/*
 * crash.c — deliberate crash fixture for core-dump-analysis.
 *
 * Scenario: a stack buffer overflow clobbers the saved frame pointer and the
 * saved return address of `crash_here`. Returning from `crash_here` jumps to
 * 0x4141414141414141, which faults with SIGSEGV (Windows: access violation
 * 0xC0000005). This is a WORST-CASE post-mortem scenario on purpose: the
 * backtrace is garbage, so the agent must fall back to registers (rip points
 * at a non-mapped address), memory maps, and stack inspection to prove that
 * corruption happened and to locate the overflow.
 *
 * Build (host):  gcc -g -O0 -fno-stack-protector examples/bad/crash.c -o crash.exe
 * The `-fno-stack-protector` keeps the layout deterministic for the fixture;
 * real targets may also have /GS (Windows) or -fstack-protector, which shifts
 * WHERE the corruption is caught, not the analysis workflow.
 */

#include <stdio.h>
#include <string.h>

#define PAYLOAD \
  "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"

static void crash_here(const char *user_input) {
  char buf[8];
  strcpy(buf, user_input); /* overflow: buf[8] then saved rbp then return addr */
  printf("copied %zu bytes into an 8-byte buffer\n", strlen(user_input));
}

int main(int argc, char **argv) {
  const char *payload = (argc > 1) ? argv[1] : PAYLOAD;
  crash_here(payload);
  printf("survived (should never print)\n");
  return 0;
}
