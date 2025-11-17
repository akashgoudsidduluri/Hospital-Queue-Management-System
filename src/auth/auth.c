/* Simple salted SHA-256 based auth (no OpenSSL).
   Suitable for demos; better than plaintext but not PBKDF2/Argon2.
*/
#include "auth.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <wincrypt.h>
#include <conio.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#endif

#define SALT_LEN 16
#define HASH_LEN 32
#define MAX_LINE 1024
#define MAX_USER 128
#define MAX_PASS 256

typedef struct {
    uint32_t state[8];
    uint64_t bitlen;
    uint8_t data[64];
    size_t datalen;
} SHA256_CTX;

static uint32_t K[64] = {
  0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
  0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
  0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
  0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
  0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
  0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
  0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
  0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static uint32_t rotr(uint32_t x, uint32_t n){ return (x >> n) | (x << (32-n)); }
static uint32_t ch(uint32_t x, uint32_t y, uint32_t z){ return (x & y) ^ (~x & z); }
static uint32_t maj(uint32_t x, uint32_t y, uint32_t z){ return (x & y) ^ (x & z) ^ (y & z); }
static uint32_t bsig0(uint32_t x){ return rotr(x,2) ^ rotr(x,13) ^ rotr(x,22); }
static uint32_t bsig1(uint32_t x){ return rotr(x,6) ^ rotr(x,11) ^ rotr(x,25); }
static uint32_t ssig0(uint32_t x){ return rotr(x,7) ^ rotr(x,18) ^ (x >> 3); }
static uint32_t ssig1(uint32_t x){ return rotr(x,17) ^ rotr(x,19) ^ (x >> 10); }

static void sha256_transform(SHA256_CTX *ctx, const uint8_t data[]) {
    uint32_t m[64], a,b,c,d,e,f,g,h,t1,t2;
    for (int i=0;i<16;++i) {
        m[i] = (data[i*4] <<24) | (data[i*4+1]<<16) | (data[i*4+2]<<8) | (data[i*4+3]);
    }
    for (int i=16;i<64;++i) m[i] = ssig1(m[i-2]) + m[i-7] + ssig0(m[i-15]) + m[i-16];
    a=ctx->state[0]; b=ctx->state[1]; c=ctx->state[2]; d=ctx->state[3];
    e=ctx->state[4]; f=ctx->state[5]; g=ctx->state[6]; h=ctx->state[7];
    for (int i=0;i<64;++i) {
        t1 = h + bsig1(e) + ch(e,f,g) + K[i] + m[i];
        t2 = bsig0(a) + maj(a,b,c);
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    ctx->state[0]+=a; ctx->state[1]+=b; ctx->state[2]+=c; ctx->state[3]+=d;
    ctx->state[4]+=e; ctx->state[5]+=f; ctx->state[6]+=g; ctx->state[7]+=h;
}

static void sha256_init(SHA256_CTX *ctx){
    ctx->datalen = 0; ctx->bitlen = 0;
    ctx->state[0]=0x6a09e667; ctx->state[1]=0xbb67ae85; ctx->state[2]=0x3c6ef372; ctx->state[3]=0xa54ff53a;
    ctx->state[4]=0x510e527f; ctx->state[5]=0x9b05688c; ctx->state[6]=0x1f83d9ab; ctx->state[7]=0x5be0cd19;
}

static void sha256_update(SHA256_CTX *ctx, const uint8_t data[], size_t len){
    for (size_t i=0;i<len;++i){
        ctx->data[ctx->datalen++] = data[i];
        if (ctx->datalen == 64) {
            sha256_transform(ctx, ctx->data);
            ctx->bitlen += 512;
            ctx->datalen = 0;
        }
    }
}

static void sha256_final(SHA256_CTX *ctx, uint8_t hash[]){
    size_t i = ctx->datalen;
    if (ctx->datalen < 56) {
        ctx->data[i++] = 0x80;
        while (i < 56) ctx->data[i++] = 0x00;
    } else {
        ctx->data[i++] = 0x80;
        while (i < 64) ctx->data[i++] = 0x00;
        sha256_transform(ctx, ctx->data);
        memset(ctx->data, 0, 56);
    }
    ctx->bitlen += ctx->datalen * 8;
    ctx->data[63] = ctx->bitlen;
    ctx->data[62] = ctx->bitlen >> 8;
    ctx->data[61] = ctx->bitlen >> 16;
    ctx->data[60] = ctx->bitlen >> 24;
    ctx->data[59] = ctx->bitlen >> 32;
    ctx->data[58] = ctx->bitlen >> 40;
    ctx->data[57] = ctx->bitlen >> 48;
    ctx->data[56] = ctx->bitlen >> 56;
    sha256_transform(ctx, ctx->data);
    for (i=0;i<4;++i){
        hash[i]      = (ctx->state[0] >> (24 - i*8)) & 0x000000ff;
        hash[i+4]    = (ctx->state[1] >> (24 - i*8)) & 0x000000ff;
        hash[i+8]    = (ctx->state[2] >> (24 - i*8)) & 0x000000ff;
        hash[i+12]   = (ctx->state[3] >> (24 - i*8)) & 0x000000ff;
        hash[i+16]   = (ctx->state[4] >> (24 - i*8)) & 0x000000ff;
        hash[i+20]   = (ctx->state[5] >> (24 - i*8)) & 0x000000ff;
        hash[i+24]   = (ctx->state[6] >> (24 - i*8)) & 0x000000ff;
        hash[i+28]   = (ctx->state[7] >> (24 - i*8)) & 0x000000ff;
    }
}

static void sha256(const uint8_t *data, size_t len, uint8_t out[HASH_LEN]) {
    SHA256_CTX ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, out);
}

static void hex_encode(const unsigned char* in, size_t inlen, char* out, size_t outlen) {
    const char hex[] = "0123456789abcdef";
    if (outlen < (inlen * 2 + 1)) { if (outlen) out[0]='\0'; return;}
    for (size_t i=0;i<inlen;++i){
        out[i*2] = hex[(in[i] >> 4) & 0xF];
        out[i*2+1] = hex[in[i] & 0xF];
    }
    out[inlen*2] = '\0';
}

static int hex_decode(const char* hex, unsigned char* out, size_t outlen) {
    size_t hlen = strlen(hex);
    if (hlen % 2 != 0) return -1;
    size_t need = hlen/2;
    if (outlen < need) return -1;
    for (size_t i=0;i<need;++i){
        unsigned int v;
        if (sscanf(hex + i*2, "%2x", &v) != 1) return -1;
        out[i] = (unsigned char)v;
    }
    return (int)need;
}

static int ct_cmp(const unsigned char* a, const unsigned char* b, size_t n){
    unsigned char diff = 0;
    for (size_t i=0;i<n;++i) diff |= a[i] ^ b[i];
    return diff == 0;
}

/* ✅ read_password - silent input (no asterisks, no echo) */
void read_password(char *password, size_t max_len) {
    if (!password || max_len == 0) return;

#if defined(_WIN32) || defined(_WIN64)
    int i = 0;
    int ch;
    while (i < (int)max_len - 1) {
        ch = _getch();
        if (ch == '\r' || ch == '\n') {
            putchar('\n');
            break;
        }
        if (ch == '\b') {
            if (i > 0) i--;
        } else {
            password[i++] = (char)ch;
        }
    }
    password[i] = '\0';
#else
    struct termios old_term, new_term;
    tcgetattr(STDIN_FILENO, &old_term);
    new_term = old_term;
    new_term.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_term);

    fgets(password, (int)max_len, stdin);
    password[strcspn(password, "\r\n")] = '\0';

    tcsetattr(STDIN_FILENO, TCSANOW, &old_term);
    putchar('\n');
#endif
}

/* ✅ auth_login - validate username and password */
bool auth_login(const char* users_file) {
    char username[MAX_USER];
    char pass[MAX_PASS];
    printf("Username: ");
    if (!fgets(username, sizeof(username), stdin)) return false;
    username[strcspn(username, "\n")] = '\0';

    FILE* f = fopen(users_file, "r");
    if (!f) { printf("No users file\n"); return false; }

    char line[MAX_LINE];
    int found = 0;
    char salt_hex[SALT_LEN*2+1] = {0};
    char hash_hex[HASH_LEN*2+1] = {0};
    
    while (fgets(line, sizeof(line), f)) {
        char u[MAX_USER];
        if (sscanf(line, "%127[^,],%32[^,],%64s", u, salt_hex, hash_hex) >= 3) {
            if (strcmp(u, username) == 0) { found = 1; break; }
        }
    }
    fclose(f);
    
    if (!found) { printf("Unknown user\n"); return false; }

    printf("Password: ");
    read_password(pass, sizeof(pass));

    /* Decode stored salt and hash */
    unsigned char salt[SALT_LEN], stored_hash[HASH_LEN];
    if (hex_decode(salt_hex, salt, sizeof(salt)) != SALT_LEN) {
        printf("Bad salt\n");
        return false;
    }
    if (hex_decode(hash_hex, stored_hash, sizeof(stored_hash)) != HASH_LEN) {
        printf("Bad hash\n");
        return false;
    }

    /* Hash entered password with stored salt: sha256(salt || password) */
    size_t plen = strlen(pass);
    unsigned char *buf = malloc(SALT_LEN + plen);
    if (!buf) return false;
    memcpy(buf, salt, SALT_LEN);
    memcpy(buf + SALT_LEN, pass, plen);
    unsigned char computed_hash[HASH_LEN];
    sha256(buf, SALT_LEN + plen, computed_hash);
    free(buf);

    /* Constant-time comparison */
    if (!ct_cmp(computed_hash, stored_hash, HASH_LEN)) {
        printf("Invalid credentials\n");
        return false;
    }
    return true;
}

/* ✅ auth_login_attempts - retry authentication */
bool auth_login_attempts(const char* users_file, int attempts) {
    for (int i = 0; i < attempts; ++i) {
        if (auth_login(users_file)) return true;
        if (i < attempts - 1) printf("Attempts left: %d\n", attempts - i - 1);
    }
    return false;
}