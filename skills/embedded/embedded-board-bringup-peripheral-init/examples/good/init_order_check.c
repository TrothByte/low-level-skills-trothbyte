/*
 * GOOD: host-verifiable init-order checker.
 * Models the clock-tree-first rule as an explicit sequence of gates: clock
 * enable must precede the first peripheral register write. The checker walks
 * the init actions and reports a violation if a peripheral write happens
 * before its clock gate. This mirrors the rule applied to target init code.
 *
 * Build: gcc -Wall -Wextra -Werror -O2 init_order_check.c -o ioc
 */
#include <stdio.h>
#include <string.h>

enum action_kind { A_CLOCK_ENABLE, A_PERIPH_CONFIG };

struct action {
    enum action_kind kind;
    const char *name;
};

static int init_ok(const struct action *acts, int n) {
    int clock_on = 0;
    for (int i = 0; i < n; i++) {
        if (acts[i].kind == A_CLOCK_ENABLE)
            clock_on = 1;
        else if (acts[i].kind == A_PERIPH_CONFIG && !clock_on)
            return 0;
    }
    return 1;
}

int main(void) {
    const struct action bad_order[] = {
        {A_PERIPH_CONFIG, "TIM2->PSC"},
        {A_CLOCK_ENABLE, "RCC->APB1ENR |= TIM2EN"},
    };
    const struct action good_order[] = {
        {A_CLOCK_ENABLE, "RCC->APB1ENR |= TIM2EN"},
        {A_PERIPH_CONFIG, "TIM2->PSC"},
        {A_PERIPH_CONFIG, "TIM2->ARR"},
    };
    printf("bad_order (config before clock): %s\n",
           init_ok(bad_order, 2) ? "accepted (WRONG)" : "rejected (correct)");
    printf("good_order (clock before config): %s\n",
           init_ok(good_order, 3) ? "accepted (correct)" : "rejected (WRONG)");
    return 0;
}
