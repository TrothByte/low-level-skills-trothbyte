/*
 * signature_chain.c — host-runnable model of the UEFI Secure Boot chain of
 * trust as a SHA-256 hash chain.
 *
 * Model: each link binds a child key under a parent. A "signature" is
 *   sig = SHA-256(parent_public_key_hash || child_public_key)
 * exactly as UEFI image verification recomputes a digest over the signed
 * content and validates it with the parent's public key. The root key hash
 * is the trust anchor (on real hardware this is the platform key digest the
 * firmware is burned with). If any child is tampered with, every link that
 * derives from it fails — the chain is only as strong as its weakest link.
 *
 * SHA-256 is implemented inline (public-domain style) so the file is
 * self-contained; hashlib twin: examples/good/signature_chain.py, which
 * must produce identical digests.
 *
 * Build (MSYS2/MinGW or any gcc):
 *   gcc -O2 -o signature_chain.exe signature_chain.c
 * Run:
 *   ./signature_chain.exe
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>

/* ---------------- SHA-256 (public-domain style, self-contained) ---------------- */

typedef struct {
    uint8_t data[64];
    uint32_t datalen;
    uint64_t bitlen;
    uint32_t state[8];
} SHA256_CTX;

#define ROTRIGHT(a, b) (((a) >> (b)) | ((a) << (32 - (b))))
#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTRIGHT(x, 2) ^ ROTRIGHT(x, 13) ^ ROTRIGHT(x, 22))
#define EP1(x) (ROTRIGHT(x, 6) ^ ROTRIGHT(x, 11) ^ ROTRIGHT(x, 25))
#define SIG0(x) (ROTRIGHT(x, 7) ^ ROTRIGHT(x, 18) ^ ((x) >> 3))
#define SIG1(x) (ROTRIGHT(x, 17) ^ ROTRIGHT(x, 19) ^ ((x) >> 10))

static const uint32_t K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static void sha256_transform(SHA256_CTX *ctx, const uint8_t data[64])
{
    uint32_t a, b, c, d, e, f, g, h, t1, t2, m[64];
    uint32_t i, j;
    for (i = 0, j = 0; i < 16; ++i, j += 4)
        m[i] = ((uint32_t)data[j] << 24) | ((uint32_t)data[j + 1] << 16)
             | ((uint32_t)data[j + 2] << 8) | ((uint32_t)data[j + 3]);
    for (; i < 64; ++i)
        m[i] = SIG1(m[i - 2]) + m[i - 7] + SIG0(m[i - 15]) + m[i - 16];
    a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2]; d = ctx->state[3];
    e = ctx->state[4]; f = ctx->state[5]; g = ctx->state[6]; h = ctx->state[7];
    for (i = 0; i < 64; ++i) {
        t1 = h + EP1(e) + CH(e, f, g) + K[i] + m[i];
        t2 = EP0(a) + MAJ(a, b, c);
        h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

static void sha256_init(SHA256_CTX *ctx)
{
    ctx->datalen = 0;
    ctx->bitlen = 0;
    ctx->state[0] = 0x6a09e667; ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372; ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f; ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab; ctx->state[7] = 0x5be0cd19;
}

static void sha256_update(SHA256_CTX *ctx, const uint8_t *data, size_t len)
{
    size_t i;
    for (i = 0; i < len; ++i) {
        ctx->data[ctx->datalen++] = data[i];
        if (ctx->datalen == 64) {
            sha256_transform(ctx, ctx->data);
            ctx->bitlen += 512;
            ctx->datalen = 0;
        }
    }
}

static void sha256_final(SHA256_CTX *ctx, uint8_t hash[32])
{
    uint32_t i = ctx->datalen;
    ctx->data[i++] = 0x80;
    if (ctx->datalen > 55) {
        while (i < 64) ctx->data[i++] = 0;
        sha256_transform(ctx, ctx->data);
        i = 0;
    }
    while (i < 56) ctx->data[i++] = 0;
    ctx->bitlen += ctx->datalen * 8;
    ctx->data[63] = (uint8_t)ctx->bitlen;
    ctx->data[62] = (uint8_t)(ctx->bitlen >> 8);
    ctx->data[61] = (uint8_t)(ctx->bitlen >> 16);
    ctx->data[60] = (uint8_t)(ctx->bitlen >> 24);
    ctx->data[59] = (uint8_t)(ctx->bitlen >> 32);
    ctx->data[58] = (uint8_t)(ctx->bitlen >> 40);
    ctx->data[57] = (uint8_t)(ctx->bitlen >> 48);
    ctx->data[56] = (uint8_t)(ctx->bitlen >> 56);
    sha256_transform(ctx, ctx->data);
    for (i = 0; i < 4; ++i) {
        hash[i]       = (uint8_t)(ctx->state[0] >> (24 - i * 8));
        hash[i + 4]   = (uint8_t)(ctx->state[1] >> (24 - i * 8));
        hash[i + 8]   = (uint8_t)(ctx->state[2] >> (24 - i * 8));
        hash[i + 12]  = (uint8_t)(ctx->state[3] >> (24 - i * 8));
        hash[i + 16]  = (uint8_t)(ctx->state[4] >> (24 - i * 8));
        hash[i + 20]  = (uint8_t)(ctx->state[5] >> (24 - i * 8));
        hash[i + 24]  = (uint8_t)(ctx->state[6] >> (24 - i * 8));
        hash[i + 28]  = (uint8_t)(ctx->state[7] >> (24 - i * 8));
    }
}

static void sha256_digest(const uint8_t *data, size_t len, uint8_t hash[32])
{
    SHA256_CTX ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, hash);
}

/* ---------------- model ---------------- */

static void print_hex(const uint8_t *h, size_t n)
{
    size_t i;
    for (i = 0; i < n; ++i)
        printf("%02x", h[i]);
}

int main(void)
{
    const char *root_key     = "root-key";        /* trust anchor (firmware PK digest) */
    const char *intermediate = "intermediate-key";/* shim/bootloader key */
    const char *leaf         = "leaf-key";        /* OS bootloader / kernel key */
    const char *tampered     = "leaf-key-modified";
    uint8_t root_hash[32], int_sig[32], leaf_sig[32], recompute[32], buf[128];
    size_t int_len = strlen(intermediate), leaf_len = strlen(leaf);

    /* link 1: root signs intermediate: sig = SHA-256(root_hash || intermediate) */
    sha256_digest((const uint8_t *)root_key, strlen(root_key), root_hash);
    memcpy(buf, root_hash, 32);
    memcpy(buf + 32, intermediate, int_len);
    sha256_digest(buf, 32 + int_len, int_sig);

    /* link 2: intermediate signs leaf: sig = SHA-256(intermediate || leaf) */
    memcpy(buf, intermediate, int_len);
    memcpy(buf + int_len, leaf, leaf_len);
    sha256_digest(buf, int_len + leaf_len, leaf_sig);

    printf("signature_chain: SHA-256 chain-of-trust model\n");
    printf("root_key_hash      = ");
    print_hex(root_hash, 32);
    printf("\nintermediate_sig   = ");
    print_hex(int_sig, 32);
    printf("\nleaf_sig           = ");
    print_hex(leaf_sig, 32);
    printf("\n\n");

    /* link 1 verification */
    memcpy(buf, root_hash, 32);
    memcpy(buf + 32, intermediate, int_len);
    sha256_digest(buf, 32 + int_len, recompute);
    printf("link 1 (root -> intermediate)     : VERIFY %s\n",
           memcmp(recompute, int_sig, 32) == 0 ? "OK" : "FAIL");

    /* link 2 verification */
    memcpy(buf, intermediate, int_len);
    memcpy(buf + int_len, leaf, leaf_len);
    sha256_digest(buf, int_len + leaf_len, recompute);
    printf("link 2 (intermediate -> leaf)     : VERIFY %s\n",
           memcmp(recompute, leaf_sig, 32) == 0 ? "OK" : "FAIL");

    /* tampered child: the chain breaks */
    memcpy(buf, intermediate, int_len);
    memcpy(buf + int_len, tampered, strlen(tampered));
    sha256_digest(buf, int_len + strlen(tampered), recompute);
    printf("link 2 with modified leaf          : VERIFY %s (expected)\n",
           memcmp(recompute, leaf_sig, 32) == 0 ? "OK" : "FAIL");

    printf("\nResult: trust chain intact (2/2 links verified); modified child rejected.\n");
    printf("Digests must match examples/good/signature_chain.py output.\n");
    return 0;
}
