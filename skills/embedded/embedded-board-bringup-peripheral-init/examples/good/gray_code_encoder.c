/*
 * GOOD: quadrature encoder as a 2-bit gray-code state machine.
 * Decodes every (A,B) transition against the gray-code cycle
 * 00 -> 01 -> 11 -> 10 (forward) and its reverse. One position unit per
 * transition; direction from transition order. Host-runnable and the core of
 * correct encoder handling on any MCU.
 *
 * Build: gcc -Wall -Wextra -Werror -O2 gray_code_encoder.c -o enc
 */
#include <stdio.h>
#include <assert.h>

static int position = 0;
static int prev_state = -1;   /* unknown until first sample */

static void decode(int a, int b) {
    int cur = (a << 1) | b;              /* 2-bit state */
    if (prev_state == -1) {              /* first sample: no transition yet */
        prev_state = cur;
        return;
    }
    if (cur == prev_state) {
        return;                          /* no transition */
    }
    /* gray-code cycle: 00->01->11->10->00 is forward */
    if ((prev_state == 0 && cur == 1) ||
        (prev_state == 1 && cur == 3) ||
        (prev_state == 3 && cur == 2) ||
        (prev_state == 2 && cur == 0))
        position += 1;
    else if ((prev_state == 1 && cur == 0) ||
             (prev_state == 3 && cur == 1) ||
             (prev_state == 2 && cur == 3) ||
             (prev_state == 0 && cur == 2))
        position -= 1;
    /* else: illegal skip (lost transition) — position stays */
    prev_state = cur;
}

int main(void) {
    /* 8 samples forward: 00 01 11 10 00 01 11 10 -> 7 transitions, +7
       then 7 samples reverse: 11 01 00 10 11 01 00 -> 7 transitions, -7 */
    const int seq[][2] = {
        {0,0},{0,1},{1,1},{1,0},{0,0},{0,1},{1,1},{1,0},
        {1,1},{0,1},{0,0},{1,0},{1,1},{0,1},{0,0}
    };
    for (int i = 0; i < 15; i++) {
        int before = position;
        decode(seq[i][0], seq[i][1]);
        int step = position - before;
        if (i == 0)
            assert(step == 0);                  /* first sample primes */
        else if (i < 8)
            assert(step == 1);                  /* forward */
        else
            assert(step == -1);                 /* reverse */
    }
    printf("gray-code encoder position after full cycle: %d\n", position);
    assert(position == 0);       /* +7 forward, -7 reverse */
    printf("PASS: position tracked exactly\n");
    return 0;
}
