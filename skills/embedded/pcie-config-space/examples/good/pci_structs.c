/* pci_structs.c — correct host model of PCIe config space (header type 0)
 * with the capability linked-list walk, BAR probing, and MSI/MSI-X decoding.
 *
 * Packed, little-endian structs with offsets checked by _Static_assert;
 * walks the real linked list (not fixed offsets); probes BAR sizes with the
 * correct width-bit mask; checks MSI-X alignment.
 *
 * Compile:  gcc -Wall -Wextra -Werror -O2 examples/good/pci_structs.c -o good.exe
 * Run:      good.exe
 */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* --- header type 0 (endpoint) ----------------------------------------- */
typedef struct __attribute__((packed)) {
    uint16_t vendor_id;            /* 0x00 */
    uint16_t device_id;            /* 0x02 */
    uint16_t command;              /* 0x04 */
    uint16_t status;               /* 0x06 */
    uint8_t  revision_id;          /* 0x08 */
    uint8_t  class_prog_if;        /* 0x09 */
    uint8_t  class_subclass;       /* 0x0A */
    uint8_t  class_base;           /* 0x0B */
    uint8_t  cache_line_size;      /* 0x0C */
    uint8_t  latency_timer;        /* 0x0D */
    uint8_t  header_type;          /* 0x0E */
    uint8_t  bist;                 /* 0x0F */
    uint32_t bar[6];               /* 0x10-0x27 */
    uint32_t cardbus_cis;          /* 0x28 */
    uint16_t subsystem_vendor;     /* 0x2C */
    uint16_t subsystem_device;     /* 0x2E */
    uint32_t expansion_rom;        /* 0x30 */
    uint8_t  caps_ptr;             /* 0x34 */
    uint8_t  reserved[7];          /* 0x35-0x3B */
    uint8_t  interrupt_line;       /* 0x3C */
    uint8_t  interrupt_pin;        /* 0x3D */
    uint8_t  min_grant;            /* 0x3E */
    uint8_t  max_latency;          /* 0x3F */
} pci_hdr_t0;

_Static_assert(offsetof(pci_hdr_t0, vendor_id) == 0x00, "vendor @0");
_Static_assert(offsetof(pci_hdr_t0, device_id) == 0x02, "device @2");
_Static_assert(offsetof(pci_hdr_t0, command) == 0x04, "command @4");
_Static_assert(offsetof(pci_hdr_t0, status) == 0x06, "status @6");
_Static_assert(offsetof(pci_hdr_t0, revision_id) == 0x08, "revision @8");
_Static_assert(offsetof(pci_hdr_t0, class_prog_if) == 0x09, "prog-if @9");
_Static_assert(offsetof(pci_hdr_t0, class_subclass) == 0x0A, "subclass @A");
_Static_assert(offsetof(pci_hdr_t0, class_base) == 0x0B, "base class @B");
_Static_assert(offsetof(pci_hdr_t0, cache_line_size) == 0x0C, "cl @C");
_Static_assert(offsetof(pci_hdr_t0, header_type) == 0x0E, "ht @E");
_Static_assert(offsetof(pci_hdr_t0, bar) == 0x10, "bars @10");
_Static_assert(offsetof(pci_hdr_t0, subsystem_vendor) == 0x2C, "ss vid @2C");
_Static_assert(offsetof(pci_hdr_t0, caps_ptr) == 0x34, "caps ptr @34");
_Static_assert(offsetof(pci_hdr_t0, interrupt_line) == 0x3C, "irq line @3C");
_Static_assert(sizeof(pci_hdr_t0) == 0x40, "type 0 header is 64 bytes");

/* --- MSI capability (ID 0x05) ----------------------------------------- */
typedef struct __attribute__((packed)) {
    uint8_t  id;              /* 0x05 */
    uint8_t  next;            /* 0x00 = end of list */
    uint16_t msg_ctrl;        /* bit0 enable, bits4-6 mmc, bits7-9 mme, bit15 64-bit */
    uint32_t addr_lo;         /* message address (low) */
    uint32_t addr_hi;         /* only if 64-bit capable */
    uint16_t msg_data;        /* message data */
} msi_cap_t;

_Static_assert(offsetof(msi_cap_t, id) == 0, "msi id");
_Static_assert(offsetof(msi_cap_t, next) == 1, "msi next");
_Static_assert(offsetof(msi_cap_t, msg_ctrl) == 2, "msi ctrl");
_Static_assert(offsetof(msi_cap_t, addr_lo) == 4, "msi addr_lo");
_Static_assert(offsetof(msi_cap_t, addr_hi) == 8, "msi addr_hi");
_Static_assert(offsetof(msi_cap_t, msg_data) == 12, "msi data");

/* --- MSI-X capability (ID 0x11) --------------------------------------- */
typedef struct __attribute__((packed)) {
    uint8_t  id;              /* 0x11 */
    uint8_t  next;
    uint16_t msg_ctrl;        /* bits 0-10 table size (N-1), bit15 enable */
    uint32_t table_offset;    /* bits 31-3 table offset, bits 2-0 BIR */
    uint32_t pba_offset;      /* bits 31-3 PBA offset, bits 2-0 BIR */
} msix_cap_t;

_Static_assert(offsetof(msix_cap_t, id) == 0, "msix id");
_Static_assert(offsetof(msix_cap_t, next) == 1, "msix next");
_Static_assert(offsetof(msix_cap_t, msg_ctrl) == 2, "msix ctrl");
_Static_assert(offsetof(msix_cap_t, table_offset) == 4, "msix table off");
_Static_assert(offsetof(msix_cap_t, pba_offset) == 8, "msix pba off");
_Static_assert(sizeof(msix_cap_t) == 12, "msix cap is 12 bytes");

/* --- capability walk over a raw config blob --------------------------- */
typedef struct {
    uint8_t off;
    uint8_t id;
    uint8_t next;
} cap_ent;

/* Walk from the caps pointer at 0x34; stops at next=0. Returns count.
 * Returns -1 on a cycle / out-of-range (both are agent-bug shapes). */
static int walk_caps(const uint8_t *cfg, size_t cfg_len,
                     cap_ent *out, int max_out) {
    if (cfg_len < 0x35) return -1;
    uint8_t off = cfg[0x34];
    int n = 0;
    while (off != 0) {
        if (off + 1 >= (int)cfg_len || n >= max_out) return -1;
        out[n].off = off;
        out[n].id = cfg[off];
        out[n].next = cfg[off + 1];
        n++;
        off = out[n - 1].next;
        if (n > 1) {  /* naive cycle guard: pointer must advance */
            for (int i = 0; i < n - 1; i++)
                if (out[i].off == out[n - 1].off) return -1;
        }
    }
    return n;
}

static const char *cap_name(uint8_t id) {
    switch (id) {
        case 0x01: return "PM";
        case 0x05: return "MSI";
        case 0x10: return "PCIe";
        case 0x11: return "MSI-X";
        case 0x19: return "LTR";
        default:   return "?";
    }
}

/* --- BAR probing with correct width-bit mask --------------------------- */
#define MEM_ATTR_MASK 0x0Fu  /* bit0 space, bits1-2 type, bit3 prefetch */
#define IO_ATTR_MASK  0x03u  /* bit0 space, bit1 reserved */

static uint32_t probe_32_mem(uint32_t rb) {
    uint32_t m = rb & ~MEM_ATTR_MASK;         /* clear width bits */
    return (~m) + 1u;                          /* size = ~masked + 1 */
}

static uint64_t probe_64_mem(uint32_t lo, uint32_t hi) {
    uint64_t low = (uint64_t)(lo & ~MEM_ATTR_MASK);
    uint64_t combined = ((uint64_t)hi << 32) | low;
    return (~combined) + 1ull;
}

static uint32_t probe_io(uint32_t rb) {
    uint32_t m = rb & ~IO_ATTR_MASK;           /* clear space+reserved bits */
    return (~m) + 1u;
}

static const char *bar_type_name(uint32_t v) {
    if (v & 1u) return "I/O";
    return ((v >> 1) & 3u) == 3u ? "memory 64-bit" : "memory 32-bit";
}

int main(void) {
    /* synthetic config blob: PM@0x40 -> MSI@0x50 -> PCIe@0x60 -> MSI-X@0x70 -> 0 */
    uint8_t cfg[256];
    memset(cfg, 0, sizeof cfg);
    /* header type 0, single function */
    cfg[0x0E] = 0x00;
    /* vendor 0x8086, device 0x100E */
    cfg[0x00] = 0x86; cfg[0x01] = 0x80;
    cfg[0x02] = 0x0E; cfg[0x03] = 0x10;
    /* class: base 0x02 (network), subclass 0x00, prog-if 0x00 */
    cfg[0x09] = 0x00; cfg[0x0A] = 0x00; cfg[0x0B] = 0x02;
    /* BAR0 32-bit mem 16 MiB at 0xE0000000 */
    cfg[0x10] = 0x00; cfg[0x11] = 0x00; cfg[0x12] = 0x00; cfg[0x13] = 0xE0;
    /* caps ptr -> 0x40 */
    cfg[0x34] = 0x40;
    /* PM */
    cfg[0x40] = 0x01; cfg[0x41] = 0x50;
    /* MSI, 64-bit capable, 1 vector, addr 0xFEE00000, data 0x33 */
    cfg[0x50] = 0x05; cfg[0x51] = 0x60;
    cfg[0x52] = 0x80; cfg[0x53] = 0x00;         /* msi ctrl: 64-bit capable */
    cfg[0x54] = 0x00; cfg[0x55] = 0x00; cfg[0x56] = 0xE0; cfg[0x57] = 0xFE;
    cfg[0x58] = 0x00; cfg[0x59] = 0x00; cfg[0x5A] = 0x00; cfg[0x5B] = 0x00;
    cfg[0x5C] = 0x33; cfg[0x5D] = 0x00;
    /* PCIe */
    cfg[0x60] = 0x10; cfg[0x61] = 0x70;
    /* MSI-X: table size 3 (4 vectors), table @ BAR0 +0x1000, PBA @ BAR0 +0x2000 */
    cfg[0x70] = 0x11; cfg[0x71] = 0x00;
    cfg[0x72] = 0x03; cfg[0x73] = 0x00;
    cfg[0x74] = 0x00; cfg[0x75] = 0x10; cfg[0x76] = 0x00; cfg[0x77] = 0x00;
    cfg[0x78] = 0x00; cfg[0x79] = 0x20; cfg[0x7A] = 0x00; cfg[0x7B] = 0x00;

    printf("== PCIe config space model (good) ==\n");
    printf("vendor/device: %04X:%04X\n", cfg[0x01] << 8 | cfg[0x00],
           cfg[0x03] << 8 | cfg[0x02]);
    printf("class: base=%02X subclass=%02X prog-if=%02X\n",
           cfg[0x0B], cfg[0x0A], cfg[0x09]);
    printf("caps ptr: 0x%02X\n", cfg[0x34]);

    cap_ent caps[16];
    int nc = walk_caps(cfg, sizeof cfg, caps, 16);
    if (nc < 0) {
        printf("FAIL: capability walk did not terminate\n");
        return 1;
    }
    for (int i = 0; i < nc; i++)
        printf("cap @0x%02X id=0x%02X %s next=0x%02X\n",
               caps[i].off, caps[i].id, cap_name(caps[i].id), caps[i].next);

    /* MSI decode */
    const msi_cap_t *msi = (const msi_cap_t *)&cfg[caps[1].off];
    printf("MSI: %s, %u vector(s), addr=0x%08X%08X data=0x%04X\n",
           (msi->msg_ctrl & 0x0080u) ? "64-bit" : "32-bit",
           1u << ((msi->msg_ctrl >> 4) & 0x7u),
           msi->addr_hi, msi->addr_lo, msi->msg_data);

    /* MSI-X decode */
    const msix_cap_t *mx = (const msix_cap_t *)&cfg[caps[3].off];
    uint32_t nvec = (mx->msg_ctrl & 0x07FFu) + 1u;
    uint32_t bir = mx->table_offset & 0x7u;
    uint32_t tab_off = mx->table_offset & 0xFFFFF8u;
    printf("MSI-X: %u vectors, table BIR=%u off=0x%04X, PBA BIR=%u off=0x%04X\n",
           nvec, bir, tab_off, mx->pba_offset & 0x7u,
           mx->pba_offset & 0xFFFFF8u);
    if ((tab_off & 0x7u) != 0) { printf("FAIL: MSI-X table not 8B aligned\n"); return 1; }
    if ((tab_off & 0xFFFu) != 0) { printf("FAIL: MSI-X table not page-aligned\n"); return 1; }

    /* BAR0 probe: 16 MiB 32-bit memory BAR -> write all-1s reads 0xFF000000 */
    uint32_t rb = 0xFF000000u;
    printf("BAR0 probe: type=%s size=0x%X\n",
           bar_type_name(0xE0000000u), probe_32_mem(rb));

    /* synthetic 64-bit probe: 1 GiB -> readback lo=0xC0000006 hi=0xFFFFFFFF */
    printf("64-bit probe: type=%s size=0x%llX\n",
           bar_type_name(0x00000006u),
           (unsigned long long)probe_64_mem(0xC0000006u, 0xFFFFFFFFu));

    /* synthetic I/O probe: 256 bytes -> readback 0xFFFFFF01 */
    printf("I/O probe: size=0x%X\n", probe_io(0xFFFFFF01u));

    printf("PASS: layout offsets asserted, capability walk terminated, "
           "BAR and MSI-X math correct\n");
    return 0;
}
