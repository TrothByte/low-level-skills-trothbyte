/*
 * GOOD: rollback boot-failure counter (host-runnable model).
 * The new slot boots in TRIAL state; every reboot before commit increments a
 * counter. If the app commits (mark valid) within the limit, the new image is
 * kept. If the counter exceeds the limit, the bootloader reverts to the
 * previous slot. This is the MCUboot / ESP-IDF
 * esp_ota_mark_app_valid_cancel_rollback model.
 *
 * Build: gcc -Wall -Wextra -Werror -O2 rollback_counter.c -o rbc
 */
#include <stdio.h>
#include <assert.h>

#define MAX_FAILED_BOOTS 3

struct boot_state {
    int booted_new;        /* 1 = new slot active (trial), 0 = old slot */
    int failed_boots;      /* reboots before commit */
    int committed;         /* app marked the new image valid */
};

static void reset_to_new_slot(struct boot_state *s) {
    s->booted_new = 1;
    s->failed_boots = 0;
    s->committed = 0;
}

static void on_reboot(struct boot_state *s) {
    if (!s->booted_new || s->committed)
        return;                          /* nothing to watch */
    s->failed_boots++;
    if (s->failed_boots > MAX_FAILED_BOOTS) {
        s->booted_new = 0;               /* rollback to previous slot */
        s->failed_boots = 0;
    }
}

static void app_commits(struct boot_state *s) {
    if (s->booted_new)
        s->committed = 1;                /* mark valid, cancel rollback */
}

int main(void) {
    struct boot_state s;
    reset_to_new_slot(&s);

    /* image crashes on boot: reboots 1,2,3 don't commit */
    on_reboot(&s); on_reboot(&s); on_reboot(&s);
    printf("after 3 failed boots: booted_new=%d (still trial)\n", s.booted_new);
    assert(s.booted_new == 1);

    on_reboot(&s);                       /* 4th reboot exceeds limit */
    printf("after 4th reboot: booted_new=%d (rolled back)\n", s.booted_new);
    assert(s.booted_new == 0);

    /* stable image case: commits within limit, no rollback */
    struct boot_state ok;
    reset_to_new_slot(&ok);
    app_commits(&ok);
    on_reboot(&ok);
    printf("stable image committed: booted_new=%d, no rollback\n", ok.booted_new);
    assert(ok.booted_new == 1 && ok.committed == 1);

    printf("PASS: rollback counter behaves per spec\n");
    return 0;
}
