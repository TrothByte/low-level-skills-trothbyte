/*
 * BAD: // intentionally incorrect — naive single-channel encoder counting.
 * Counts every rising edge of channel A and guesses direction from the level
 * of B. This "looks correct but wrong" class (mcuoneclipse 2025) loses counts
 * near rest, misreads direction, and is wrong for the gray-code sequence.
 *
 * Host runnable: feed it the same step sequence as good/gray_code_encoder.c
 * and compare final position.
 */
#include <stdio.h>
#include <assert.h>

static int edge_count_pos = 0;
static int last_a = 0;

static void feed_edge(int a, int b) {
    if (a && !last_a)          /* rising edge of A only */
        edge_count_pos += (b ? 1 : -1);
    last_a = a;
}

int main(void) {
    /* gray-code forward sequence 00 01 11 10 00 ... (4 steps = +1 full rev) */
    const int seq[][2] = {{0,0},{0,1},{1,1},{1,0},{0,0},{0,1},{1,1},{1,0}};
    for (int i = 0; i < 8; i++)
        feed_edge(seq[i][0], seq[i][1]);
    printf("edge-count position after 8 transitions: %d\n", edge_count_pos);
    printf("(expected +2 for 8 gray steps; edge counting undercounts)\n");
    return 0;
}
