/* bb.c - PZ71 searcher, Android/JNI build. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdatomic.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <jni.h>
#include <android/log.h>

#if defined(__x86_64__)||defined(_M_X64)
#  include <immintrin.h>
#endif

#define LOG_TAG "PZ71"
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#define PZ_VERSION "PZ71 v2 (android)"
#define MAX_THREADS 256

typedef uint8_t  u8;
typedef uint32_t u32;
typedef uint64_t u64;
typedef unsigned __int128 u128;

static const char *DEFAULT_LABEL = "1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU";
static const unsigned char DEFAULT_HASH160[20] = {
    0xf6,0xf5,0x43,0x1d,0x25,0xbb,0xf7,0xb1,0x2e,0x8a,
    0xdd,0x9a,0xf5,0xe3,0x47,0x5c,0x44,0xa0,0xa5,0xb8};

static const u32 SHA256_K[64] = {
0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u};

#define ROTR32(x,n) (((x)>>(n))|((x)<<(32-(n))))
#define ROTL32(x,n) (((x)<<(n))|((x)>>(32-(n))))

static void sha256_block(const u8 block[64], u8 out[32])
{
    u32 w[64];
    for (int i = 0; i < 16; i++)
        w[i] = ((u32)block[i*4] << 24) | ((u32)block[i*4+1] << 16) |
               ((u32)block[i*4+2] << 8) | (u32)block[i*4+3];
    for (int i = 16; i < 64; i++) {
        u32 s0 = ROTR32(w[i-15],7) ^ ROTR32(w[i-15],18) ^ (w[i-15] >> 3);
        u32 s1 = ROTR32(w[i-2],17) ^ ROTR32(w[i-2],19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    u32 a=0x6a09e667u,b=0xbb67ae85u,c=0x3c6ef372u,d=0xa54ff53au;
    u32 e=0x510e527fu,f=0x9b05688cu,g=0x1f83d9abu,h=0x5be0cd19u;
    for (int i = 0; i < 64; i++) {
        u32 S1 = ROTR32(e,6) ^ ROTR32(e,11) ^ ROTR32(e,25);
        u32 ch = (e & f) ^ (~e & g);
        u32 T1 = h + S1 + ch + SHA256_K[i] + w[i];
        u32 S0 = ROTR32(a,2) ^ ROTR32(a,13) ^ ROTR32(a,22);
        u32 mj = (a & b) ^ (a & c) ^ (b & c);
        h=g; g=f; f=e; e=d+T1; d=c; c=b; b=a; a=T1+S0+mj;
    }
    u32 st[8] = {0x6a09e667u+a,0xbb67ae85u+b,0x3c6ef372u+c,0xa54ff53au+d,
                 0x510e527fu+e,0x9b05688cu+f,0x1f83d9abu+g,0x5be0cd19u+h};
    for (int i = 0; i < 8; i++) {
        out[i*4]   = (u8)(st[i] >> 24);
        out[i*4+1] = (u8)(st[i] >> 16);
        out[i*4+2] = (u8)(st[i] >> 8);
        out[i*4+3] = (u8)st[i];
    }
}

static void sha256_short(const u8 *data, size_t len, u8 out[32])
{
    u8 block[64];
    memset(block, 0, 64);
    memcpy(block, data, len);
    block[len] = 0x80;
    u64 bits = (u64)len * 8;
    for (int i = 0; i < 8; i++) block[56 + i] = (u8)(bits >> (56 - 8*i));
    sha256_block(block, out);
}

static void sha256d_short(const u8 *data, size_t len, u8 out[32])
{
    u8 first[32];
    sha256_short(data, len, first);
    sha256_short(first, 32, out);
}

static const int RMD_RL[80]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,7,4,13,1,10,6,15,3,12,0,9,5,2,14,11,8,3,10,14,4,9,15,8,1,2,7,0,6,13,11,5,12,1,9,11,10,0,8,12,4,13,3,7,15,14,5,6,2,4,0,5,9,7,12,2,10,14,1,3,8,11,6,15,13};
static const int RMD_SL[80]={11,14,15,12,5,8,7,9,11,13,14,15,6,7,9,8,7,6,8,13,11,9,7,15,7,12,15,9,11,7,13,12,11,13,6,7,14,9,13,15,14,8,13,6,5,12,7,5,11,12,14,15,14,15,9,8,9,14,5,6,8,6,5,12,9,15,5,11,6,8,13,12,5,12,13,14,11,8,5,6};
static const int RMD_RR[80]={5,14,7,0,9,2,11,4,13,6,15,8,1,10,3,12,6,11,3,7,0,13,5,10,14,15,8,12,4,9,1,2,15,5,1,3,7,14,6,9,11,8,12,2,10,0,4,13,8,6,4,1,3,11,15,0,5,12,2,13,9,7,10,14,12,15,10,4,1,5,8,7,6,2,13,14,0,3,9,11};
static const int RMD_SR[80]={8,9,9,11,13,15,15,5,7,7,8,11,14,14,12,6,9,13,15,7,12,8,9,11,7,7,12,7,6,15,13,11,9,7,15,11,8,6,6,14,12,13,5,14,13,13,7,5,15,5,8,11,14,14,6,14,6,9,12,9,12,5,15,8,8,5,12,9,12,5,14,6,8,13,6,5,15,13,11,11};

static void ripemd160_compress(const u8 block[64], u8 out[20])
{
    u32 X[16];
    for (int i = 0; i < 16; i++)
        X[i] = (u32)block[i*4] | ((u32)block[i*4+1] << 8) |
               ((u32)block[i*4+2] << 16) | ((u32)block[i*4+3] << 24);
    u32 a = 0x67452301u, b = 0xEFCDAB89u, c = 0x98BADCFEu,
        d = 0x10325476u, e = 0xC3D2E1F0u;
    u32 aa = a, bb = b, cc = c, dd = d, ee = e;
    for (int j = 0; j < 80; j++) {
        int rnd = j >> 4;
        u32 t;
        switch (rnd) {
            case 0:  t = b ^ c ^ d;              break;
            case 1:  t = (b & c) | (~b & d);     break;
            case 2:  t = (b | ~c) ^ d;           break;
            case 3:  t = (b & d) | (c & ~d);     break;
            default: t = b ^ (c | ~d);           break;
        }
        u32 KL = rnd==0?0x00000000u:rnd==1?0x5A827999u:rnd==2?0x6ED9EBA1u:
                 rnd==3?0x8F1BBCDCu:0xA953FD4Eu;
        t += a + X[RMD_RL[j]] + KL;
        t = ROTL32(t, RMD_SL[j]) + e;
        a = e; e = d; d = ROTL32(c, 10); c = b; b = t;
        switch (rnd) {
            case 0:  t = bb ^ (cc | ~dd);        break;
            case 1:  t = (bb & dd) | (cc & ~dd); break;
            case 2:  t = (bb | ~cc) ^ dd;        break;
            case 3:  t = (bb & cc) | (~bb & dd); break;
            default: t = bb ^ cc ^ dd;           break;
        }
        u32 KR = rnd==0?0x50A28BE6u:rnd==1?0x5C4DD124u:rnd==2?0x6D703EF3u:
                 rnd==3?0x7A6D76E9u:0x00000000u;
        t += aa + X[RMD_RR[j]] + KR;
        t = ROTL32(t, RMD_SR[j]) + ee;
        aa = ee; ee = dd; dd = ROTL32(cc, 10); cc = bb; bb = t;
    }
    u32 st[5];
    st[0] = 0xEFCDAB89u + c + dd;
    st[1] = 0x98BADCFEu + d + ee;
    st[2] = 0x10325476u + e + aa;
    st[3] = 0xC3D2E1F0u + a + bb;
    st[4] = 0x67452301u + b + cc;
    for (int i = 0; i < 5; i++) {
        out[i*4]   = (u8)st[i];
        out[i*4+1] = (u8)(st[i] >> 8);
        out[i*4+2] = (u8)(st[i] >> 16);
        out[i*4+3] = (u8)(st[i] >> 24);
    }
}

static void ripemd160_short(const u8 *in, size_t len, u8 out[20])
{
    u8 block[64];
    memset(block, 0, 64);
    memcpy(block, in, len);
    block[len] = 0x80;
    u64 bits = (u64)len * 8;
    for (int i = 0; i < 8; i++) block[56 + i] = (u8)(bits >> (8*i));
    ripemd160_compress(block, out);
}

static void hash160_pub(const u8 pub33[33], u8 out[20])
{
    u8 sha[32];
    sha256_short(pub33, 33, sha);
    ripemd160_short(sha, 32, out);
}

#if defined(__AVX2__)
#  define LANES 8
#else
#  define LANES 4
#endif
typedef u32 vu __attribute__((vector_size(4*LANES), aligned(16)));
#define VC(x) ((vu){0} + (u32)(x))
#define VROTR(x,n) (((x)>>(n))|((x)<<(32-(n))))
#define VROTL(x,n) (((x)<<(n))|((x)>>(32-(n))))

static inline void sha256_x(const vu win[16], vu out[8])
{
    vu w[16];
    for (int i = 0; i < 16; i++) w[i] = win[i];
    vu a=VC(0x6a09e667u),b=VC(0xbb67ae85u),c=VC(0x3c6ef372u),d=VC(0xa54ff53au);
    vu e=VC(0x510e527fu),f=VC(0x9b05688cu),g=VC(0x1f83d9abu),h=VC(0x5be0cd19u);
#pragma GCC unroll 64
    for (int i = 0; i < 64; i++) {
        vu wi;
        if (i < 16) {
            wi = w[i];
        } else {
            vu w15 = w[(i-15)&15], w2 = w[(i-2)&15];
            vu s0 = VROTR(w15,7) ^ VROTR(w15,18) ^ (w15 >> 3);
            vu s1 = VROTR(w2,17) ^ VROTR(w2,19) ^ (w2 >> 10);
            w[i&15] = w[i&15] + s0 + w[(i-7)&15] + s1;
            wi = w[i&15];
        }
        vu S1 = VROTR(e,6) ^ VROTR(e,11) ^ VROTR(e,25);
        vu ch = g ^ (e & (f ^ g));
        vu T1 = h + S1 + ch + SHA256_K[i] + wi;
        vu S0 = VROTR(a,2) ^ VROTR(a,13) ^ VROTR(a,22);
        vu mj = (a & b) | (c & (a | b));
        h=g; g=f; f=e; e=d+T1; d=c; c=b; b=a; a=T1+S0+mj;
    }
    out[0]=a+0x6a09e667u; out[1]=b+0xbb67ae85u; out[2]=c+0x3c6ef372u; out[3]=d+0xa54ff53au;
    out[4]=e+0x510e527fu; out[5]=f+0x9b05688cu; out[6]=g+0x1f83d9abu; out[7]=h+0x5be0cd19u;
}

#define RF1(x,y,z) ((x)^(y)^(z))
#define RF2(x,y,z) (((x)&(y))|(~(x)&(z)))
#define RF3(x,y,z) (((x)|~(y))^(z))
#define RF4(x,y,z) (((x)&(z))|((y)&~(z)))
#define RF5(x,y,z) ((x)^((y)|~(z)))
#define RSTEP(a,b,c,d,e,f,x,k,s) { a = VROTL(a + f(b,c,d) + X[x] + (u32)(k), s) + e; c = VROTL(c,10); }

static inline void ripemd160_x(const vu X[16], vu st[5])
{
    vu a=VC(0x67452301u),b=VC(0xEFCDAB89u),c=VC(0x98BADCFEu),d=VC(0x10325476u),e=VC(0xC3D2E1F0u);
    vu aa=a,bb=b,cc=c,dd=d,ee=e;
    RSTEP(a,b,c,d,e,RF1,0,0x00000000u,11);  RSTEP(aa,bb,cc,dd,ee,RF5,5,0x50A28BE6u,8);
    RSTEP(e,a,b,c,d,RF1,1,0x00000000u,14);  RSTEP(ee,aa,bb,cc,dd,RF5,14,0x50A28BE6u,9);
    RSTEP(d,e,a,b,c,RF1,2,0x00000000u,15);  RSTEP(dd,ee,aa,bb,cc,RF5,7,0x50A28BE6u,9);
    RSTEP(c,d,e,a,b,RF1,3,0x00000000u,12);  RSTEP(cc,dd,ee,aa,bb,RF5,0,0x50A28BE6u,11);
    RSTEP(b,c,d,e,a,RF1,4,0x00000000u,5);   RSTEP(bb,cc,dd,ee,aa,RF5,9,0x50A28BE6u,13);
    RSTEP(a,b,c,d,e,RF1,5,0x00000000u,8);   RSTEP(aa,bb,cc,dd,ee,RF5,2,0x50A28BE6u,15);
    RSTEP(e,a,b,c,d,RF1,6,0x00000000u,7);   RSTEP(ee,aa,bb,cc,dd,RF5,11,0x50A28BE6u,15);
    RSTEP(d,e,a,b,c,RF1,7,0x00000000u,9);   RSTEP(dd,ee,aa,bb,cc,RF5,4,0x50A28BE6u,5);
    RSTEP(c,d,e,a,b,RF1,8,0x00000000u,11);  RSTEP(cc,dd,ee,aa,bb,RF5,13,0x50A28BE6u,7);
    RSTEP(b,c,d,e,a,RF1,9,0x00000000u,13);  RSTEP(bb,cc,dd,ee,aa,RF5,6,0x50A28BE6u,7);
    RSTEP(a,b,c,d,e,RF1,10,0x00000000u,14); RSTEP(aa,bb,cc,dd,ee,RF5,15,0x50A28BE6u,8);
    RSTEP(e,a,b,c,d,RF1,11,0x00000000u,15); RSTEP(ee,aa,bb,cc,dd,RF5,8,0x50A28BE6u,11);
    RSTEP(d,e,a,b,c,RF1,12,0x00000000u,6);  RSTEP(dd,ee,aa,bb,cc,RF5,1,0x50A28BE6u,14);
    RSTEP(c,d,e,a,b,RF1,13,0x00000000u,7);  RSTEP(cc,dd,ee,aa,bb,RF5,10,0x50A28BE6u,14);
    RSTEP(b,c,d,e,a,RF1,14,0x00000000u,9);  RSTEP(bb,cc,dd,ee,aa,RF5,3,0x50A28BE6u,12);
    RSTEP(a,b,c,d,e,RF1,15,0x00000000u,8);  RSTEP(aa,bb,cc,dd,ee,RF5,12,0x50A28BE6u,6);

    RSTEP(e,a,b,c,d,RF2,7,0x5A827999u,7);   RSTEP(ee,aa,bb,cc,dd,RF4,6,0x5C4DD124u,9);
    RSTEP(d,e,a,b,c,RF2,4,0x5A827999u,6);   RSTEP(dd,ee,aa,bb,cc,RF4,11,0x5C4DD124u,13);
    RSTEP(c,d,e,a,b,RF2,13,0x5A827999u,8);  RSTEP(cc,dd,ee,aa,bb,RF4,3,0x5C4DD124u,15);
    RSTEP(b,c,d,e,a,RF2,1,0x5A827999u,13);  RSTEP(bb,cc,dd,ee,aa,RF4,7,0x5C4DD124u,7);
    RSTEP(a,b,c,d,e,RF2,10,0x5A827999u,11); RSTEP(aa,bb,cc,dd,ee,RF4,0,0x5C4DD124u,12);
    RSTEP(e,a,b,c,d,RF2,6,0x5A827999u,9);   RSTEP(ee,aa,bb,cc,dd,RF4,13,0x5C4DD124u,8);
    RSTEP(d,e,a,b,c,RF2,15,0x5A827999u,7);  RSTEP(dd,ee,aa,bb,cc,RF4,5,0x5C4DD124u,9);
    RSTEP(c,d,e,a,b,RF2,3,0x5A827999u,15);  RSTEP(cc,dd,ee,aa,bb,RF4,10,0x5C4DD124u,11);
    RSTEP(b,c,d,e,a,RF2,12,0x5A827999u,7);  RSTEP(bb,cc,dd,ee,aa,RF4,14,0x5C4DD124u,7);
    RSTEP(a,b,c,d,e,RF2,0,0x5A827999u,12);  RSTEP(aa,bb,cc,dd,ee,RF4,15,0x5C4DD124u,7);
    RSTEP(e,a,b,c,d,RF2,9,0x5A827999u,15);  RSTEP(ee,aa,bb,cc,dd,RF4,8,0x5C4DD124u,12);
    RSTEP(d,e,a,b,c,RF2,5,0x5A827999u,9);   RSTEP(dd,ee,aa,bb,cc,RF4,12,0x5C4DD124u,7);
    RSTEP(c,d,e,a,b,RF2,2,0x5A827999u,11);  RSTEP(cc,dd,ee,aa,bb,RF4,4,0x5C4DD124u,6);
    RSTEP(b,c,d,e,a,RF2,14,0x5A827999u,7);  RSTEP(bb,cc,dd,ee,aa,RF4,9,0x5C4DD124u,15);
    RSTEP(a,b,c,d,e,RF2,11,0x5A827999u,13); RSTEP(aa,bb,cc,dd,ee,RF4,1,0x5C4DD124u,13);
    RSTEP(e,a,b,c,d,RF2,8,0x5A827999u,12);  RSTEP(ee,aa,bb,cc,dd,RF4,2,0x5C4DD124u,11);

    RSTEP(d,e,a,b,c,RF3,3,0x6ED9EBA1u,11);  RSTEP(dd,ee,aa,bb,cc,RF3,15,0x6D703EF3u,9);
    RSTEP(c,d,e,a,b,RF3,10,0x6ED9EBA1u,13); RSTEP(cc,dd,ee,aa,bb,RF3,5,0x6D703EF3u,7);
    RSTEP(b,c,d,e,a,RF3,14,0x6ED9EBA1u,6);  RSTEP(bb,cc,dd,ee,aa,RF3,1,0x6D703EF3u,15);
    RSTEP(a,b,c,d,e,RF3,4,0x6ED9EBA1u,7);   RSTEP(aa,bb,cc,dd,ee,RF3,3,0x6D703EF3u,11);
    RSTEP(e,a,b,c,d,RF3,9,0x6ED9EBA1u,14);  RSTEP(ee,aa,bb,cc,dd,RF3,7,0x6D703EF3u,8);
    RSTEP(d,e,a,b,c,RF3,15,0x6ED9EBA1u,9);  RSTEP(dd,ee,aa,bb,cc,RF3,14,0x6D703EF3u,6);
    RSTEP(c,d,e,a,b,RF3,8,0x6ED9EBA1u,13);  RSTEP(cc,dd,ee,aa,bb,RF3,6,0x6D703EF3u,6);
    RSTEP(b,c,d,e,a,RF3,1,0x6ED9EBA1u,15);  RSTEP(bb,cc,dd,ee,aa,RF3,9,0x6D703EF3u,14);
    RSTEP(a,b,c,d,e,RF3,2,0x6ED9EBA1u,14);  RSTEP(aa,bb,cc,dd,ee,RF3,11,0x6D703EF3u,12);
    RSTEP(e,a,b,c,d,RF3,7,0x6ED9EBA1u,8);   RSTEP(ee,aa,bb,cc,dd,RF3,8,0x6D703EF3u,13);
    RSTEP(d,e,a,b,c,RF3,0,0x6ED9EBA1u,13);  RSTEP(dd,ee,aa,bb,cc,RF3,12,0x6D703EF3u,5);
    RSTEP(c,d,e,a,b,RF3,6,0x6ED9EBA1u,6);   RSTEP(cc,dd,ee,aa,bb,RF3,2,0x6D703EF3u,14);
    RSTEP(b,c,d,e,a,RF3,13,0x6ED9EBA1u,5);  RSTEP(bb,cc,dd,ee,aa,RF3,10,0x6D703EF3u,13);
    RSTEP(a,b,c,d,e,RF3,11,0x6ED9EBA1u,12); RSTEP(aa,bb,cc,dd,ee,RF3,0,0x6D703EF3u,13);
    RSTEP(e,a,b,c,d,RF3,5,0x6ED9EBA1u,7);   RSTEP(ee,aa,bb,cc,dd,RF3,4,0x6D703EF3u,7);
    RSTEP(d,e,a,b,c,RF3,12,0x6ED9EBA1u,5);  RSTEP(dd,ee,aa,bb,cc,RF3,13,0x6D703EF3u,5);

    RSTEP(c,d,e,a,b,RF4,1,0x8F1BBCDCu,11);  RSTEP(cc,dd,ee,aa,bb,RF2,8,0x7A6D76E9u,15);
    RSTEP(b,c,d,e,a,RF4,9,0x8F1BBCDCu,12);  RSTEP(bb,cc,dd,ee,aa,RF2,6,0x7A6D76E9u,5);
    RSTEP(a,b,c,d,e,RF4,11,0x8F1BBCDCu,14); RSTEP(aa,bb,cc,dd,ee,RF2,4,0x7A6D76E9u,8);
    RSTEP(e,a,b,c,d,RF4,10,0x8F1BBCDCu,15); RSTEP(ee,aa,bb,cc,dd,RF2,1,0x7A6D76E9u,11);
    RSTEP(d,e,a,b,c,RF4,0,0x8F1BBCDCu,14);  RSTEP(dd,ee,aa,bb,cc,RF2,3,0x7A6D76E9u,14);
    RSTEP(c,d,e,a,b,RF4,8,0x8F1BBCDCu,15);  RSTEP(cc,dd,ee,aa,bb,RF2,11,0x7A6D76E9u,14);
    RSTEP(b,c,d,e,a,RF4,12,0x8F1BBCDCu,9);  RSTEP(bb,cc,dd,ee,aa,RF2,15,0x7A6D76E9u,6);
    RSTEP(a,b,c,d,e,RF4,4,0x8F1BBCDCu,8);   RSTEP(aa,bb,cc,dd,ee,RF2,0,0x7A6D76E9u,14);
    RSTEP(e,a,b,c,d,RF4,13,0x8F1BBCDCu,9);  RSTEP(ee,aa,bb,cc,dd,RF2,5,0x7A6D76E9u,6);
    RSTEP(d,e,a,b,c,RF4,3,0x8F1BBCDCu,14);  RSTEP(dd,ee,aa,bb,cc,RF2,12,0x7A6D76E9u,9);
    RSTEP(c,d,e,a,b,RF4,7,0x8F1BBCDCu,5);   RSTEP(cc,dd,ee,aa,bb,RF2,2,0x7A6D76E9u,12);
    RSTEP(b,c,d,e,a,RF4,15,0x8F1BBCDCu,6);  RSTEP(bb,cc,dd,ee,aa,RF2,13,0x7A6D76E9u,9);
    RSTEP(a,b,c,d,e,RF4,14,0x8F1BBCDCu,8);  RSTEP(aa,bb,cc,dd,ee,RF2,9,0x7A6D76E9u,12);
    RSTEP(e,a,b,c,d,RF4,5,0x8F1BBCDCu,6);   RSTEP(ee,aa,bb,cc,dd,RF2,7,0x7A6D76E9u,5);
    RSTEP(d,e,a,b,c,RF4,6,0x8F1BBCDCu,5);   RSTEP(dd,ee,aa,bb,cc,RF2,10,0x7A6D76E9u,15);
    RSTEP(c,d,e,a,b,RF4,2,0x8F1BBCDCu,12);  RSTEP(cc,dd,ee,aa,bb,RF2,14,0x7A6D76E9u,8);

    RSTEP(b,c,d,e,a,RF5,4,0xA953FD4Eu,9);   RSTEP(bb,cc,dd,ee,aa,RF1,12,0x00000000u,8);
    RSTEP(a,b,c,d,e,RF5,0,0xA953FD4Eu,15);  RSTEP(aa,bb,cc,dd,ee,RF1,15,0x00000000u,5);
    RSTEP(e,a,b,c,d,RF5,5,0xA953FD4Eu,5);   RSTEP(ee,aa,bb,cc,dd,RF1,10,0x00000000u,12);
    RSTEP(d,e,a,b,c,RF5,9,0xA953FD4Eu,11);  RSTEP(dd,ee,aa,bb,cc,RF1,4,0x00000000u,9);
    RSTEP(c,d,e,a,b,RF5,7,0xA953FD4Eu,6);   RSTEP(cc,dd,ee,aa,bb,RF1,1,0x00000000u,12);
    RSTEP(b,c,d,e,a,RF5,12,0xA953FD4Eu,8);  RSTEP(bb,cc,dd,ee,aa,RF1,5,0x00000000u,5);
    RSTEP(a,b,c,d,e,RF5,2,0xA953FD4Eu,13);  RSTEP(aa,bb,cc,dd,ee,RF1,8,0x00000000u,14);
    RSTEP(e,a,b,c,d,RF5,10,0xA953FD4Eu,12); RSTEP(ee,aa,bb,cc,dd,RF1,7,0x00000000u,6);
    RSTEP(d,e,a,b,c,RF5,14,0xA953FD4Eu,5);  RSTEP(dd,ee,aa,bb,cc,RF1,6,0x00000000u,8);
    RSTEP(c,d,e,a,b,RF5,1,0xA953FD4Eu,12);  RSTEP(cc,dd,ee,aa,bb,RF1,2,0x00000000u,13);
    RSTEP(b,c,d,e,a,RF5,3,0xA953FD4Eu,13);  RSTEP(bb,cc,dd,ee,aa,RF1,13,0x00000000u,6);
    RSTEP(a,b,c,d,e,RF5,8,0xA953FD4Eu,14);  RSTEP(aa,bb,cc,dd,ee,RF1,14,0x00000000u,5);
    RSTEP(e,a,b,c,d,RF5,11,0xA953FD4Eu,11); RSTEP(ee,aa,bb,cc,dd,RF1,0,0x00000000u,15);
    RSTEP(d,e,a,b,c,RF5,6,0xA953FD4Eu,8);   RSTEP(dd,ee,aa,bb,cc,RF1,3,0x00000000u,13);
    RSTEP(c,d,e,a,b,RF5,15,0xA953FD4Eu,5);  RSTEP(cc,dd,ee,aa,bb,RF1,9,0x00000000u,11);
    RSTEP(b,c,d,e,a,RF5,13,0xA953FD4Eu,6);  RSTEP(bb,cc,dd,ee,aa,RF1,11,0x00000000u,11);

    st[0] = VC(0xEFCDAB89u) + c + dd;
    st[1] = VC(0x98BADCFEu) + d + ee;
    st[2] = VC(0x10325476u) + e + aa;
    st[3] = VC(0xC3D2E1F0u) + a + bb;
    st[4] = VC(0x67452301u) + b + cc;
}

static inline vu vbswap(vu x)
{
    return (x >> 24) | ((x >> 8) & 0xFF00u) | ((x << 8) & 0xFF0000u) | (x << 24);
}

static const char B58[]="123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
static void b58_encode(const unsigned char*in,size_t inlen,char*out){
    size_t zeros=0;while(zeros<inlen&&in[zeros]==0)zeros++;
    unsigned char buf[128];size_t blen=0;
    for(size_t i=zeros;i<inlen;i++){
        int carry=in[i];
        for(size_t j=0;j<blen;j++){carry+=256*buf[j];buf[j]=(unsigned char)(carry%58);carry/=58;}
        while(carry){buf[blen++]=(unsigned char)(carry%58);carry/=58;}
    }
    size_t oi=0;
    for(size_t i=0;i<zeros;i++)out[oi++]='1';
    for(size_t i=blen;i>0;i--)out[oi++]=B58[buf[i-1]];
    out[oi]='\0';
}
static int b58check_decode(const char*s,unsigned char*out21){
    if(!s||!out21)return 0;
    size_t slen=strlen(s);
    if(slen==0||slen>100)return 0;
    size_t zeros=0;while(zeros<slen&&s[zeros]=='1')zeros++;
    unsigned char be[64];size_t blen=0;memset(be,0,sizeof be);
    for(size_t i=zeros;i<slen;i++){
        const char*p=strchr(B58,(unsigned char)s[i]);if(!p||!s[i])return 0;
        unsigned carry=(unsigned)(p-B58);
        for(size_t j=0;j<blen;j++){unsigned v=(unsigned)be[j]*58u+carry;be[j]=(unsigned char)(v&0xffu);carry=v>>8;}
        while(carry){if(blen>=sizeof be)return 0;be[blen++]=(unsigned char)(carry&0xffu);carry>>=8;}
    }
    if(zeros+blen>25)return 0;
    unsigned char full[25];memset(full,0,sizeof full);
    for(size_t i=0;i<blen;i++)full[24-i]=be[i];
    if(full[0]!=0x00&&full[0]!=0x05)return 0;
    unsigned char chk[32];sha256d_short(full,21,chk);
    if(memcmp(chk,full+21,4)!=0)return 0;
    memcpy(out21,full,21);return 1;
}

#define FC 0x1000003D1ULL
typedef struct { u64 v[4]; } fe;

static inline void fe_set_u64(fe*r,u64 x){r->v[0]=x;r->v[1]=0;r->v[2]=0;r->v[3]=0;}
static inline int  fe_is_one(const fe*a){return a->v[0]==1&&(a->v[1]|a->v[2]|a->v[3])==0;}
static inline int  fe_eq(const fe*a,const fe*b){
    return ((a->v[0]^b->v[0])|(a->v[1]^b->v[1])|(a->v[2]^b->v[2])|(a->v[3]^b->v[3]))==0;
}
static inline void fe_final(fe*r){
    u128 c=(u128)r->v[0]+FC; u64 s0=(u64)c; c>>=64;
    c+=r->v[1]; u64 s1=(u64)c; c>>=64;
    c+=r->v[2]; u64 s2=(u64)c; c>>=64;
    c+=r->v[3]; u64 s3=(u64)c; c>>=64;
    u64 m=(u64)0-(u64)c;
    r->v[0]=(s0&m)|(r->v[0]&~m); r->v[1]=(s1&m)|(r->v[1]&~m);
    r->v[2]=(s2&m)|(r->v[2]&~m); r->v[3]=(s3&m)|(r->v[3]&~m);
}

#if defined(__x86_64__)||defined(_M_X64)
#define ULL_T unsigned long long
static inline void fe_add(fe*r,const fe*a,const fe*b){
    ULL_T s0,s1,s2,s3,t0,t1,t2,t3; unsigned char c;
    c=_addcarry_u64(0,a->v[0],b->v[0],&s0); c=_addcarry_u64(c,a->v[1],b->v[1],&s1);
    c=_addcarry_u64(c,a->v[2],b->v[2],&s2); c=_addcarry_u64(c,a->v[3],b->v[3],&s3);
    u64 over=c;
    unsigned char d=_addcarry_u64(0,s0,FC,&t0); d=_addcarry_u64(d,s1,0,&t1);
    d=_addcarry_u64(d,s2,0,&t2); d=_addcarry_u64(d,s3,0,&t3);
    u64 m=(u64)0-(over|(u64)d);
    r->v[0]=(t0&m)|(s0&~m); r->v[1]=(t1&m)|(s1&~m);
    r->v[2]=(t2&m)|(s2&~m); r->v[3]=(t3&m)|(s3&~m);
}
static inline void fe_sub(fe*r,const fe*a,const fe*b){
    ULL_T d0,d1,d2,d3,e0,e1,e2,e3; unsigned char c;
    c=_subborrow_u64(0,a->v[0],b->v[0],&d0); c=_subborrow_u64(c,a->v[1],b->v[1],&d1);
    c=_subborrow_u64(c,a->v[2],b->v[2],&d2); c=_subborrow_u64(c,a->v[3],b->v[3],&d3);
    u64 m=(u64)0-(u64)c;
    unsigned char k=_subborrow_u64(0,d0,FC&m,&e0); k=_subborrow_u64(k,d1,0,&e1);
    k=_subborrow_u64(k,d2,0,&e2); (void)_subborrow_u64(k,d3,0,&e3);
    r->v[0]=e0;r->v[1]=e1;r->v[2]=e2;r->v[3]=e3;
}
#else
static inline void fe_add(fe*r,const fe*a,const fe*b){
    u128 c=0; u64 s[4],t[4];
    for(int i=0;i<4;i++){c+=(u128)a->v[i]+b->v[i];s[i]=(u64)c;c>>=64;}
    u64 c1=(u64)c;
    c=FC;
    for(int i=0;i<4;i++){c+=s[i];t[i]=(u64)c;c>>=64;}
    u64 m=(u64)0-(c1|(u64)c);
    for(int i=0;i<4;i++) r->v[i]=(t[i]&m)|(s[i]&~m);
}
static inline void fe_sub(fe*r,const fe*a,const fe*b){
    u64 d[4]; u64 bw=0;
    for(int i=0;i<4;i++){
        u64 x=a->v[i],y=b->v[i];
        u64 t=x-y; u64 b1=(x<y);
        u64 u=t-bw; u64 b2=(t<bw);
        d[i]=u; bw=b1|b2;
    }
    u64 m=(u64)0-bw, sub=FC&m, bb=0;
    for(int i=0;i<4;i++){
        u64 x=d[i],y=(i==0)?sub:0;
        u64 t=x-y; u64 b1=(x<y);
        u64 u=t-bb; u64 b2=(t<bb);
        r->v[i]=u; bb=b1|b2;
    }
}
#endif

static inline void fe_reduce512(fe*r,const u64 t[8]){
    u64 n[5]; u128 c=0;
    for(int i=0;i<4;i++){c+=(u128)t[4+i]*FC+t[i];n[i]=(u64)c;c>>=64;}
    n[4]=(u64)c;
    c=(u128)n[4]*FC+n[0]; u64 r0=(u64)c; c>>=64;
    c+=n[1]; u64 r1=(u64)c; c>>=64;
    c+=n[2]; u64 r2=(u64)c; c>>=64;
    c+=n[3]; u64 r3=(u64)c; c>>=64;
    if(c){
        u128 d=(u128)r0+FC; r0=(u64)d; d>>=64;
        d+=r1; r1=(u64)d; d>>=64;
        d+=r2; r2=(u64)d; d>>=64;
        d+=r3; r3=(u64)d;
    }
    r->v[0]=r0;r->v[1]=r1;r->v[2]=r2;r->v[3]=r3;
    fe_final(r);
}

#define ADDP(pl,ph) { c0+=(pl); u64 _k=(c0<(pl)); u64 _h=(ph)+_k; c1+=_h; c2+=(c1<_h); }
#define MACC(i,j) { u128 _p=(u128)a->v[i]*b->v[j]; u64 _pl=(u64)_p,_ph=(u64)(_p>>64); ADDP(_pl,_ph) }
#define SACC(i)   { u128 _p=(u128)a->v[i]*a->v[i]; u64 _pl=(u64)_p,_ph=(u64)(_p>>64); ADDP(_pl,_ph) }
#define SDBL(i,j) { u128 _p=(u128)a->v[i]*a->v[j]; u64 _pl=(u64)_p,_ph=(u64)(_p>>64); ADDP(_pl,_ph) ADDP(_pl,_ph) }
#define NEXTCOL(k) { t[k]=c0; c0=c1; c1=c2; c2=0; }

static inline void fe_mul(fe*r,const fe*a,const fe*b){
    u64 t[8]; u64 c0=0,c1=0,c2=0;
    MACC(0,0) NEXTCOL(0)
    MACC(0,1) MACC(1,0) NEXTCOL(1)
    MACC(0,2) MACC(1,1) MACC(2,0) NEXTCOL(2)
    MACC(0,3) MACC(1,2) MACC(2,1) MACC(3,0) NEXTCOL(3)
    MACC(1,3) MACC(2,2) MACC(3,1) NEXTCOL(4)
    MACC(2,3) MACC(3,2) NEXTCOL(5)
    MACC(3,3) NEXTCOL(6)
    t[7]=c0;
    fe_reduce512(r,t);
}

static inline void fe_sqr(fe*r,const fe*a){
    u64 t[8]; u64 c0=0,c1=0,c2=0;
    SACC(0) NEXTCOL(0)
    SDBL(0,1) NEXTCOL(1)
    SDBL(0,2) SACC(1) NEXTCOL(2)
    SDBL(0,3) SDBL(1,2) NEXTCOL(3)
    SDBL(1,3) SACC(2) NEXTCOL(4)
    SDBL(2,3) NEXTCOL(5)
    SACC(3) NEXTCOL(6)
    t[7]=c0;
    fe_reduce512(r,t);
}

static inline void fe_sqrn(fe*r,const fe*a,int n){
    fe_sqr(r,a);
    for(int i=1;i<n;i++) fe_sqr(r,r);
}

static void fe_inv(fe*r,const fe*a){
    fe x2,x3,x6,x9,x11,x22,x44,x88,x176,x220,x223,t;
    fe_sqr(&x2,a);   fe_mul(&x2,&x2,a);
    fe_sqr(&x3,&x2); fe_mul(&x3,&x3,a);
    fe_sqrn(&x6,&x3,3);   fe_mul(&x6,&x6,&x3);
    fe_sqrn(&x9,&x6,3);   fe_mul(&x9,&x9,&x3);
    fe_sqrn(&x11,&x9,2);  fe_mul(&x11,&x11,&x2);
    fe_sqrn(&x22,&x11,11);fe_mul(&x22,&x22,&x11);
    fe_sqrn(&x44,&x22,22);fe_mul(&x44,&x44,&x22);
    fe_sqrn(&x88,&x44,44);fe_mul(&x88,&x88,&x44);
    fe_sqrn(&x176,&x88,88);fe_mul(&x176,&x176,&x88);
    fe_sqrn(&x220,&x176,44);fe_mul(&x220,&x220,&x44);
    fe_sqrn(&x223,&x220,3);fe_mul(&x223,&x223,&x3);
    fe_sqrn(&t,&x223,23); fe_mul(&t,&t,&x22);
    fe_sqrn(&t,&t,5);     fe_mul(&t,&t,a);
    fe_sqrn(&t,&t,3);     fe_mul(&t,&t,&x2);
    fe_sqrn(&t,&t,2);     fe_mul(r,&t,a);
}

static void fe_to_be32(unsigned char out[32],const fe*a){
    for(int i=0;i<4;i++){
        u64 limb=a->v[3-i];
        for(int j=0;j<8;j++)out[i*8+j]=(unsigned char)(limb>>(56-8*j));
    }
}

#define GRP_HALF 512
#define GRP      (2*GRP_HALF+1)
#define CHUNK_GROUPS 1024
#define CHUNK    ((u128)GRP*CHUNK_GROUPS)

typedef struct { fe x,y; } ap;
typedef struct { fe x,y,z; } gej;

static ap g_tab[GRP_HALF+1];
static ap g_step;
static ap g_pow[128];
static const u64 GX[4]={0x59F2815B16F81798ULL,0x029BFCDB2DCE28D9ULL,0x55A06295CE870B07ULL,0x79BE667EF9DCBBACULL};
static const u64 GY[4]={0x9C47D08FFB10D4B8ULL,0xFD17B448A6855419ULL,0x5DA4FBFC0E1108A8ULL,0x483ADA7726A3C465ULL};

static void ap_dbl(ap*R,const ap*P){
    fe t,t3,d,inv,lam,x3,y3;
    fe_sqr(&t,&P->x); fe_add(&t3,&t,&t); fe_add(&t3,&t3,&t);
    fe_add(&d,&P->y,&P->y); fe_inv(&inv,&d); fe_mul(&lam,&t3,&inv);
    fe_sqr(&x3,&lam); fe_sub(&x3,&x3,&P->x); fe_sub(&x3,&x3,&P->x);
    fe_sub(&t,&P->x,&x3); fe_mul(&y3,&lam,&t); fe_sub(&y3,&y3,&P->y);
    R->x=x3; R->y=y3;
}
static void ap_add(ap*R,const ap*P,const ap*Q){
    fe dx,dy,inv,lam,x3,y3,t;
    fe_sub(&dx,&Q->x,&P->x); fe_sub(&dy,&Q->y,&P->y);
    fe_inv(&inv,&dx); fe_mul(&lam,&dy,&inv);
    fe_sqr(&x3,&lam); fe_sub(&x3,&x3,&P->x); fe_sub(&x3,&x3,&Q->x);
    fe_sub(&t,&P->x,&x3); fe_mul(&y3,&lam,&t); fe_sub(&y3,&y3,&P->y);
    R->x=x3; R->y=y3;
}

static void gej_madd(gej*R,const gej*P,const ap*Q){
    fe Z1Z1,U2,S2,H,HH,I,J,r,V,t,X3,Y3,Z3;
    fe_sqr(&Z1Z1,&P->z);
    fe_mul(&U2,&Q->x,&Z1Z1);
    fe_mul(&t,&Z1Z1,&P->z); fe_mul(&S2,&Q->y,&t);
    fe_sub(&H,&U2,&P->x);
    fe_sqr(&HH,&H);
    fe_add(&I,&HH,&HH); fe_add(&I,&I,&I);
    fe_mul(&J,&H,&I);
    fe_sub(&t,&S2,&P->y); fe_add(&r,&t,&t);
    fe_mul(&V,&P->x,&I);
    fe_sqr(&X3,&r); fe_sub(&X3,&X3,&J);
    fe_add(&t,&V,&V); fe_sub(&X3,&X3,&t);
    fe_sub(&t,&V,&X3); fe_mul(&Y3,&r,&t);
    fe_mul(&t,&P->y,&J); fe_add(&t,&t,&t); fe_sub(&Y3,&Y3,&t);
    fe_add(&t,&P->z,&H); fe_sqr(&Z3,&t); fe_sub(&Z3,&Z3,&Z1Z1); fe_sub(&Z3,&Z3,&HH);
    R->x=X3; R->y=Y3; R->z=Z3;
}

static int scalar_mul(u128 k, ap*out){
    if(k==0) return 0;
    gej acc; int have=0;
    for(int j=0;j<128;j++){
        if(!((k>>j)&1)) continue;
        if(!have){ acc.x=g_pow[j].x; acc.y=g_pow[j].y; fe_set_u64(&acc.z,1); have=1; }
        else gej_madd(&acc,&acc,&g_pow[j]);
    }
    fe zi,z2,z3;
    fe_inv(&zi,&acc.z); fe_sqr(&z2,&zi); fe_mul(&z3,&z2,&zi);
    fe_mul(&out->x,&acc.x,&z2); fe_mul(&out->y,&acc.y,&z3);
    return 1;
}

static void setup_tables(void){
    for(int i=0;i<4;i++){g_pow[0].x.v[i]=GX[i];g_pow[0].y.v[i]=GY[i];}
    for(int j=1;j<128;j++) ap_dbl(&g_pow[j],&g_pow[j-1]);
    g_tab[1]=g_pow[0];
    ap_dbl(&g_tab[2],&g_pow[0]);
    for(int i=3;i<=GRP_HALF;i++) ap_add(&g_tab[i],&g_tab[i-1],&g_pow[0]);
    scalar_mul((u128)GRP,&g_step);
}

static void u128_to_be32(u128 v,unsigned char out[32]){
    memset(out,0,16);
    for(int i=15;i>=0;i--){out[16+i]=(unsigned char)(v&0xff);v>>=8;}
}
static void u128_to_hex(u128 v,char out[33]){
    static const char*H="0123456789abcdef";
    for(int i=31;i>=0;i--){out[i]=H[v&0xf];v>>=4;}
    out[32]='\0';
}
static const char *hex_trim(const char*h){ while(*h=='0'&&h[1])h++; return h; }
static int parse_u128(const char*s,u128*out){
    int base=10;
    if(s[0]=='0'&&(s[1]=='x'||s[1]=='X')){base=16;s+=2;}
    if(!*s)return 0;
    u128 v=0;
    for(;*s;s++){
        int d;
        if(*s=='_'||*s==',')continue;
        if(*s>='0'&&*s<='9')d=*s-'0';
        else if(base==16&&*s>='a'&&*s<='f')d=*s-'a'+10;
        else if(base==16&&*s>='A'&&*s<='F')d=*s-'A'+10;
        else return 0;
        u128 nv=v*(unsigned)base+(unsigned)d;
        if(nv/(unsigned)base!=v&&v!=0)return 0;
        v=nv;
    }
    *out=v;return 1;
}
static int hexval(int ch){
    if(ch>='0'&&ch<='9')return ch-'0';
    if(ch>='a'&&ch<='f')return ch-'a'+10;
    if(ch>='A'&&ch<='F')return ch-'A'+10;
    return -1;
}
static int hex_to_bytes(const char*s,unsigned char*out,size_t n){
    if(strlen(s)!=n*2)return 0;
    for(size_t i=0;i<n;i++){
        int hi=hexval((unsigned char)s[2*i]),lo=hexval((unsigned char)s[2*i+1]);
        if(hi<0||lo<0)return 0;
        out[i]=(unsigned char)((hi<<4)|lo);
    }
    return 1;
}

typedef struct { u64 s[4]; } rng_t;
static u64 rotl64(u64 x,int k){return (x<<k)|(x>>(64-k));}
static u64 rng_next(rng_t*r){
    u64*s=r->s;
    u64 res=rotl64(s[1]*5,7)*9;
    u64 t=s[1]<<17;
    s[2]^=s[0];s[3]^=s[1];s[1]^=s[2];s[0]^=s[3];s[2]^=t;
    s[3]=rotl64(s[3],45);
    return res;
}
static void rng_seed(rng_t*r,u64 seed){
    for(int i=0;i<4;i++){
        seed+=0x9E3779B97F4A7C15ULL;
        u64 z=seed;
        z=(z^(z>>30))*0xBF58476D1CE4E5B9ULL;
        z=(z^(z>>27))*0x94D049BB133111EBULL;
        r->s[i]=z^(z>>31);
    }
}
static u128 rng_u128(rng_t*r){ return ((u128)rng_next(r)<<64)|(u128)rng_next(r); }

static unsigned char   g_target[20];
static u32             g_tw[5];
static atomic_int      g_stop=0;
static atomic_int      g_active=0;
static atomic_ullong   g_keys=0;
static pthread_mutex_t g_blk_mtx=PTHREAD_MUTEX_INITIALIZER;
static u128            g_blk_next;
static u128            g_range_start;
static u128            g_range_end;
static atomic_ullong   g_cur_lo[MAX_THREADS], g_cur_hi[MAX_THREADS];
static int             g_quiet=0;
static int             g_found=0;
static u128            g_found_key=0;

static atomic_int      g_ui_state=0;
static atomic_int      g_ready=0;
static double          g_t_start=0.0;
static char            g_files_dir[512] = "";
static char            g_found_wif[64]  = "";
static char            g_found_addr[64] = "";
static char            g_found_hex[33]  = "";
static char            g_last_msg[256]  = "";
static u128 g_chunk = (u128)GRP*CHUNK_GROUPS;

static void set_target(const unsigned char h[20]){
    memcpy(g_target,h,20);
    for(int i=0;i<5;i++)
        g_tw[i]=(u32)h[i*4]|((u32)h[i*4+1]<<8)|((u32)h[i*4+2]<<16)|((u32)h[i*4+3]<<24);
}

static double now_sec(void){
    struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);
    return (double)ts.tv_sec+(double)ts.tv_nsec/1e9;
}

static void pub33_of(const ap*P,u8 pub33[33]){
    pub33[0]=(u8)(0x02|(P->y.v[0]&1));
    fe_to_be32(pub33+1,&P->x);
}
static int verify_key(u128 k,u8 pub33[33]){
    ap P;
    if(!scalar_mul(k,&P))return 0;
    pub33_of(&P,pub33);
    u8 h[20];
    hash160_pub(pub33,h);
    return memcmp(h,g_target,20)==0;
}

static void report_found(u128 k,const u8 pub33[33]){
    char hex[33];u128_to_hex(k,hex);
    const char*hp=hex_trim(hex);
    unsigned char key32[32];u128_to_be32(k,key32);
    unsigned char payload[34];
    payload[0]=0x80; memcpy(payload+1,key32,32); payload[33]=0x01;
    unsigned char chk[32];
    sha256d_short(payload,34,chk);
    unsigned char full[38];
    memcpy(full,payload,34); memcpy(full+34,chk,4);
    char wif[64]; b58_encode(full,38,wif);
    unsigned char h20[20]; hash160_pub(pub33,h20);
    unsigned char ap25[25]; ap25[0]=0x00; memcpy(ap25+1,h20,20);
    sha256d_short(ap25,21,chk); memcpy(ap25+21,chk,4);
    char addr[64]; b58_encode(ap25,25,addr);

    snprintf(g_found_hex, sizeof g_found_hex, "%s", hp);
    snprintf(g_found_wif, sizeof g_found_wif, "%s", wif);
    snprintf(g_found_addr,sizeof g_found_addr,"%s", addr);
    snprintf(g_last_msg,  sizeof g_last_msg,  "MATCH FOUND");

    if(!g_quiet){
        ALOGI("=== MATCH ===");
        ALOGI("hex: %s", hp);
        ALOGI("WIF: %s", wif);
        ALOGI("addr: %s", addr);
    }
    if(g_files_dir[0]){
        char path[768];
        snprintf(path, sizeof path, "%s/FOUND_KEY.txt", g_files_dir);
        FILE*f=fopen(path,"a");
        if(f){
            fprintf(f,"key_hex=%s\nWIF=%s\naddress=%s\n\n",hp,wif,addr);
            fclose(f);
            ALOGI("saved to %s", path);
        } else {
            ALOGE("failed to open %s", path);
        }
    }
}

typedef struct {
    int id, mode;
    fe *d, *p;
    u64 lx[LANES][4];
    u8  lpar[LANES];
    u128 lkey[LANES];
    int ln;
} worker_t;

static int flush_lanes(worker_t*w){
    int n=w->ln;
    if(n==0)return 0;
    for(int l=n;l<LANES;l++){
        memcpy(w->lx[l],w->lx[0],32); w->lpar[l]=w->lpar[0]; w->lkey[l]=w->lkey[0];
    }
    w->ln=0;

    u32 tw[16][LANES];
    for(int l=0;l<LANES;l++){
        const u64*x=w->lx[l];
        u32 xw[8];
        xw[0]=(u32)(x[3]>>32); xw[1]=(u32)x[3];
        xw[2]=(u32)(x[2]>>32); xw[3]=(u32)x[2];
        xw[4]=(u32)(x[1]>>32); xw[5]=(u32)x[1];
        xw[6]=(u32)(x[0]>>32); xw[7]=(u32)x[0];
        u32 pre=0x02u|(u32)w->lpar[l];
        tw[0][l]=(pre<<24)|(xw[0]>>8);
        for(int j=1;j<8;j++) tw[j][l]=(xw[j-1]<<24)|(xw[j]>>8);
        tw[8][l]=(xw[7]<<24)|0x00800000u;
        for(int j=9;j<15;j++) tw[j][l]=0;
        tw[15][l]=264u;
    }
    vu W[16],H[8],X[16],R[5];
    for(int j=0;j<16;j++) memcpy(&W[j],tw[j],sizeof(vu));
    sha256_x(W,H);
    for(int j=0;j<8;j++) X[j]=vbswap(H[j]);
    X[8]=VC(0x80u);
    for(int j=9;j<14;j++) X[j]=VC(0);
    X[14]=VC(256u); X[15]=VC(0);
    ripemd160_x(X,R);

    vu eq=(vu)(R[0]==VC(g_tw[0]));
    u32 m[LANES]; memcpy(m,&eq,sizeof m);
    u32 any=0; for(int l=0;l<LANES;l++) any|=m[l];
    if(!any) return 0;

    u32 r[5][LANES];
    for(int i=0;i<5;i++) memcpy(r[i],&R[i],sizeof(vu));
    for(int l=0;l<n;l++){
        if(r[0][l]==g_tw[0]&&r[1][l]==g_tw[1]&&r[2][l]==g_tw[2]&&r[3][l]==g_tw[3]&&r[4][l]==g_tw[4]){
            u8 pub[33];
            if(verify_key(w->lkey[l],pub)){
                if(atomic_exchange(&g_stop,1)==0){
                    g_found=1; g_found_key=w->lkey[l];
                    report_found(w->lkey[l],pub);
                    atomic_store(&g_ui_state, 2);
                }
                return 1;
            }
            ALOGE("[warn] SIMD candidate failed scalar verification (ignored)");
        }
    }
    return 0;
}

static inline int lane_push(worker_t*w,const fe*x,u64 par,u128 key){
    int l=w->ln;
    memcpy(w->lx[l],x->v,32); w->lpar[l]=(u8)(par&1); w->lkey[l]=key;
    if(++w->ln==LANES) return flush_lanes(w);
    return 0;
}

static int scan_chunk(worker_t*w,u128 k_lo,u128 n_keys){
    const int H=GRP_HALF;
    fe *d=w->d,*p=w->p;
    u128 ngroups=(n_keys+GRP-1)/GRP;
    u128 c=k_lo+H;
    ap P;
    if(!scalar_mul(c,&P)) return 0;
    u64 remaining=(u64)n_keys, pending=0;
    w->ln=0;

    for(u128 g=0;g<ngroups;g++){
        if(atomic_load_explicit(&g_stop,memory_order_relaxed)) break;

        for(int i=1;i<=H;i++) fe_sub(&d[i-1],&g_tab[i].x,&P.x);
        fe_sub(&d[H],&g_step.x,&P.x);
        p[0]=d[0];
        for(int k=1;k<=H;k++) fe_mul(&p[k],&p[k-1],&d[k]);
        fe iv; fe_inv(&iv,&p[H]);
        for(int k=H;k>=1;k--){
            fe t=d[k];
            fe_mul(&d[k],&iv,&p[k-1]);
            fe_mul(&iv,&iv,&t);
        }
        d[0]=iv;

        if(lane_push(w,&P.x,P.y.v[0],c)) return 1;
        for(int i=1;i<=H;i++){
            const ap*Q=&g_tab[i];
            fe dy,lam,x3,y3,t;
            fe_sub(&dy,&Q->y,&P.y);
            fe_mul(&lam,&dy,&d[i-1]);
            fe_sqr(&x3,&lam); fe_sub(&x3,&x3,&P.x); fe_sub(&x3,&x3,&Q->x);
            fe_sub(&t,&P.x,&x3); fe_mul(&y3,&lam,&t); fe_sub(&y3,&y3,&P.y);
            if(lane_push(w,&x3,y3.v[0],c+i)) return 1;
            fe_add(&dy,&Q->y,&P.y);
            fe_mul(&lam,&dy,&d[i-1]);
            fe_sqr(&x3,&lam); fe_sub(&x3,&x3,&P.x); fe_sub(&x3,&x3,&Q->x);
            fe_sub(&t,&x3,&P.x); fe_mul(&y3,&lam,&t); fe_sub(&y3,&y3,&P.y);
            if(lane_push(w,&x3,y3.v[0],c-i)) return 1;
        }
        {
            fe dy,lam,x3,y3,t;
            fe_sub(&dy,&g_step.y,&P.y);
            fe_mul(&lam,&dy,&d[H]);
            fe_sqr(&x3,&lam); fe_sub(&x3,&x3,&P.x); fe_sub(&x3,&x3,&g_step.x);
            fe_sub(&t,&P.x,&x3); fe_mul(&y3,&lam,&t); fe_sub(&y3,&y3,&P.y);
            P.x=x3; P.y=y3; c+=GRP;
        }
        u64 done=remaining<(u64)GRP?remaining:(u64)GRP;
        remaining-=done; pending+=done;
        if((g&63)==63){
            atomic_fetch_add_explicit(&g_keys,pending,memory_order_relaxed); pending=0;
            atomic_store_explicit(&g_cur_lo[w->id],(u64)c,memory_order_relaxed);
            atomic_store_explicit(&g_cur_hi[w->id],(u64)(c>>64),memory_order_relaxed);
        }
    }
    if(flush_lanes(w)) return 1;
    atomic_fetch_add_explicit(&g_keys,pending,memory_order_relaxed);
    return 0;
}

typedef struct { int id, mode; } warg_t;

static void*worker_main(void*arg){
    warg_t*wa=(warg_t*)arg;
    worker_t*w=(worker_t*)calloc(1,sizeof(worker_t));
    w->id=wa->id; w->mode=wa->mode;
    w->d=(fe*)malloc((GRP_HALF+1)*sizeof(fe));
    w->p=(fe*)malloc((GRP_HALF+1)*sizeof(fe));
    if(!w->d||!w->p){ALOGE("out of memory");atomic_store(&g_stop,1);goto done;}

    if(wa->mode==0){
        for(;;){
            if(atomic_load_explicit(&g_stop,memory_order_relaxed))break;
            pthread_mutex_lock(&g_blk_mtx);
            u128 bstart=g_blk_next;
            if(bstart>g_range_end||bstart<g_range_start){pthread_mutex_unlock(&g_blk_mtx);break;}
            u128 len=g_range_end-bstart+1;
            if(len>g_chunk)len=g_chunk;
            g_blk_next=bstart+len;
            pthread_mutex_unlock(&g_blk_mtx);
            if(scan_chunk(w,bstart,len))break;
        }
    }else if(wa->mode==1){
        rng_t rng;
        rng_seed(&rng,(u64)time(NULL)^((u64)(wa->id+1)*0xD1B54A32D192ED03ULL)^(u64)(now_sec()*1e6));
        u128 span=g_range_end-g_range_start+1;
        u128 chunk=span<g_chunk?span:g_chunk;
        u128 slots=span-chunk+1;
        while(!atomic_load_explicit(&g_stop,memory_order_relaxed)){
            u128 k0=g_range_start+(rng_u128(&rng)%slots);
            if(scan_chunk(w,k0,chunk))break;
        }
    }else if(wa->mode==2){
        rng_t rng;
        rng_seed(&rng,(u64)time(NULL)^((u64)(wa->id+1)*0xD1B54A32D192ED03ULL)^(u64)(now_sec()*1e6));
        u128 span=g_range_end-g_range_start+1;
        while(!atomic_load_explicit(&g_stop,memory_order_relaxed)){
            u128 k=g_range_start+(rng_u128(&rng)%span);
            atomic_store_explicit(&g_cur_lo[w->id],(u64)k,memory_order_relaxed);
            atomic_store_explicit(&g_cur_hi[w->id],(u64)(k>>64),memory_order_relaxed);
            ap P;
            if(scalar_mul(k,&P)){
                u8 pub[33]; pub33_of(&P,pub);
                u8 h[20]; hash160_pub(pub,h);
                if(memcmp(h,g_target,20)==0){
                    if(atomic_exchange(&g_stop,1)==0){
                        g_found=1; g_found_key=k;
                        report_found(k,pub);
                        atomic_store(&g_ui_state, 2);
                    }
                    break;
                }
            }
            atomic_fetch_add_explicit(&g_keys,1,memory_order_relaxed);
        }
    }
done:
    free(w->d);free(w->p);free(w);
    atomic_fetch_sub(&g_active,1);
    return NULL;
}

static int hexeq(const fe*a,const char*hex64){
    u8 b[32],e[32]; fe_to_be32(b,a);
    return hex_to_bytes(hex64,e,32)&&memcmp(b,e,32)==0;
}
static int selftest(void){
    u8 out[32];
    static const u8 sha_expect[32]={0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,
        0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad};
    sha256_short((const u8*)"abc",3,out);
    if(memcmp(out,sha_expect,32)){ALOGE("SELFTEST: SHA-256 failed");return 0;}
    static const u8 rmd_expect[20]={0x8e,0xb2,0x08,0xf7,0xe0,0x5d,0x98,0x7a,0x9b,0x04,
        0x4a,0x8e,0x98,0xc6,0xb0,0x87,0xf1,0x5a,0x0b,0xfc};
    u8 ro[20]; ripemd160_short((const u8*)"abc",3,ro);
    if(memcmp(ro,rmd_expect,20)){ALOGE("SELFTEST: RIPEMD-160 failed");return 0;}

    u8 pay[21];
    if(!b58check_decode(DEFAULT_LABEL,pay)||memcmp(pay+1,DEFAULT_HASH160,20)){
        ALOGE("SELFTEST: default label/hash160 mismatch");return 0;}

    rng_t r; rng_seed(&r,12345);
    for(int it=0;it<200;it++){
        fe a,b,c,s; for(int i=0;i<4;i++)a.v[i]=rng_next(&r); a.v[3]&=0x7FFFFFFFFFFFFFFFULL;
        fe_sqr(&s,&a); fe_mul(&c,&a,&a); if(!fe_eq(&s,&c)){ALOGE("SELFTEST: fe_sqr");return 0;}
        fe_inv(&b,&a); fe_mul(&c,&a,&b); if(!fe_is_one(&c)){ALOGE("SELFTEST: fe_inv");return 0;}
        fe t1,t2; fe_add(&t1,&a,&b); fe_sub(&t2,&t1,&b); if(!fe_eq(&t2,&a)){ALOGE("SELFTEST: add/sub");return 0;}
    }
    if(!hexeq(&g_pow[1].x,"C6047F9441ED7D6D3045406E95C07CD85C778E4B8CEF3CA7ABAC09B95C709EE5")){
        ALOGE("SELFTEST: 2G wrong");return 0;}
    ap P3; if(!scalar_mul(3,&P3)||!hexeq(&P3.x,"F9308A019258C31049344F85F89D5229B531C845836F99B08601F113BCE036F9")){
        ALOGE("SELFTEST: 3G wrong");return 0;}
    if(!fe_eq(&g_tab[3].x,&P3.x)||!fe_eq(&g_tab[3].y,&P3.y)){ALOGE("SELFTEST: table");return 0;}

    worker_t*w=(worker_t*)calloc(1,sizeof(worker_t));
    for(int round=0;round<4;round++){
        u8 pubs[LANES][33];
        for(int l=0;l<LANES;l++){
            pubs[l][0]=(u8)(2+(rng_next(&r)&1));
            for(int i=0;i<32;i++)pubs[l][1+i]=(u8)rng_next(&r);
        }
        u32 tw[16][LANES];
        for(int l=0;l<LANES;l++){
            u8 blk[64]; memset(blk,0,64); memcpy(blk,pubs[l],33); blk[33]=0x80; blk[62]=0x01; blk[63]=0x08;
            for(int j=0;j<16;j++) tw[j][l]=((u32)blk[4*j]<<24)|((u32)blk[4*j+1]<<16)|((u32)blk[4*j+2]<<8)|blk[4*j+3];
        }
        vu W[16],H[8],X[16],R[5];
        for(int j=0;j<16;j++) memcpy(&W[j],tw[j],sizeof(vu));
        sha256_x(W,H);
        for(int j=0;j<8;j++) X[j]=vbswap(H[j]);
        X[8]=VC(0x80u); for(int j=9;j<14;j++)X[j]=VC(0); X[14]=VC(256u); X[15]=VC(0);
        ripemd160_x(X,R);
        for(int l=0;l<LANES;l++){
            u8 h[20]; hash160_pub(pubs[l],h);
            for(int i=0;i<5;i++){
                u32 got; memcpy(&got,(u8*)&R[i]+4*l,4);
                u32 exp=(u32)h[4*i]|((u32)h[4*i+1]<<8)|((u32)h[4*i+2]<<16)|((u32)h[4*i+3]<<24);
                if(got!=exp){ALOGE("SELFTEST: SIMD hash mismatch");free(w);return 0;}
            }
        }
    }
    free(w);
    return 1;
}

static int parse_part(const char*s0,u32*N,u32*K){
    char s[64];snprintf(s,sizeof s,"%s",s0);
    for(char*p=s;*p;p++) if(*p>='A'&&*p<='Z')*p=(char)(*p+32);
    if(!strcmp(s,"full")||!strcmp(s,"all")||!strcmp(s,"1")||!strcmp(s,"1:1")){*N=1;*K=1;return 1;}
    if(!strcmp(s,"half1")||!strcmp(s,"h1")||!strcmp(s,"first")||!strcmp(s,"firsthalf")||!strcmp(s,"1st")){*N=2;*K=1;return 1;}
    if(!strcmp(s,"half2")||!strcmp(s,"h2")||!strcmp(s,"second")||!strcmp(s,"secondhalf")||!strcmp(s,"2nd")){*N=2;*K=2;return 1;}
    if(s[0]=='q'&&s[1]>='1'&&s[1]<='4'&&!s[2]){*N=4;*K=(u32)(s[1]-'0');return 1;}
    unsigned n,k;char junk;
    if(sscanf(s,"%u:%u%c",&n,&k,&junk)==2&&n>=1&&n<=1000000&&k>=1&&k<=n){*N=n;*K=k;return 1;}
    return 0;
}
static void apply_part(u128 s,u128 e,u32 N,u32 K,u128*ns,u128*ne){
    u128 span=e-s+1,size=span/N,rem=span%N;
    u128 idx=K-1;
    u128 off=idx*size+(idx<rem?idx:rem);
    u128 len=size+(idx<rem?1:0);
    *ns=s+off; *ne=*ns+len-1;
}

static void*controller_main(void*arg){
    int mode=*(int*)arg;
    free(arg);

    u128 start = g_range_start;
    u128 end   = g_range_end;
    int threads = (int)g_blk_next;
    g_blk_next = start;

    atomic_store(&g_stop,0);
    atomic_store(&g_keys,0);
    atomic_store(&g_ui_state,1);
    g_found=0; g_found_key=0;
    g_found_hex[0]=g_found_wif[0]=g_found_addr[0]=0;
    g_last_msg[0]=0;
    for(int i=0;i<MAX_THREADS;i++){atomic_store(&g_cur_lo[i],0);atomic_store(&g_cur_hi[i],0);}

    g_t_start = now_sec();

    pthread_t*tids=(pthread_t*)calloc((size_t)threads,sizeof(pthread_t));
    warg_t*was=(warg_t*)calloc((size_t)threads,sizeof(warg_t));
    atomic_store(&g_active,threads);
    for(int i=0;i<threads;i++){was[i].id=i;was[i].mode=mode;pthread_create(&tids[i],NULL,worker_main,&was[i]);}

    while(!atomic_load(&g_stop) && atomic_load(&g_active)>0) {
        struct timespec ts={0,250000000L};
        nanosleep(&ts,NULL);
    }
    atomic_store(&g_stop,1);
    for(int i=0;i<threads;i++)pthread_join(tids[i],NULL);

    if(atomic_load(&g_ui_state) != 2) {
        atomic_store(&g_ui_state, mode==0 ? 3 : 4);
        snprintf(g_last_msg,sizeof g_last_msg, mode==0?"sequential range finished":"stopped");
    }
    free(tids); free(was);
    return NULL;
}

JNIEXPORT jint JNICALL
Java_com_pz71_searcher_Native_init(JNIEnv*env, jclass clazz){
    (void)env;(void)clazz;
    if(atomic_load(&g_ready)) return 1;
    setup_tables();
    if(!selftest()) return 0;
    atomic_store(&g_ready,1);
    return 1;
}

JNIEXPORT jint JNICALL
Java_com_pz71_searcher_Native_start(JNIEnv*env, jclass clazz,
        jint mode, jstring jstart, jstring jend, jstring jpart,
        jstring jtarget, jint threads, jint chunkSize, jstring jfiles)
{
    (void)clazz;
    if(!atomic_load(&g_ready)) {
        setup_tables();
        if(!selftest()) return -1;
        atomic_store(&g_ready,1);
    }
    if(atomic_load(&g_ui_state)==1) return -2;

    const char* cstart  = (*env)->GetStringUTFChars(env,jstart,NULL);
    const char* cend    = (*env)->GetStringUTFChars(env,jend,NULL);
    const char* cpart   = (*env)->GetStringUTFChars(env,jpart,NULL);
    const char* ctarget = (*env)->GetStringUTFChars(env,jtarget,NULL);
    const char* cfiles  = (*env)->GetStringUTFChars(env,jfiles,NULL);

    u128 rs,re;
    int ok = parse_u128(cstart,&rs) & parse_u128(cend,&re);
    if(!ok || rs<1 || rs>re || re>((u128)1<<126)) {
        (*env)->ReleaseStringUTFChars(env,jstart,cstart);
        (*env)->ReleaseStringUTFChars(env,jend,cend);
        (*env)->ReleaseStringUTFChars(env,jpart,cpart);
        (*env)->ReleaseStringUTFChars(env,jtarget,ctarget);
        (*env)->ReleaseStringUTFChars(env,jfiles,cfiles);
        return -3;
    }
    u32 N,K;
    if(!parse_part(cpart,&N,&K)) {
        (*env)->ReleaseStringUTFChars(env,jstart,cstart);
        (*env)->ReleaseStringUTFChars(env,jend,cend);
        (*env)->ReleaseStringUTFChars(env,jpart,cpart);
        (*env)->ReleaseStringUTFChars(env,jtarget,ctarget);
        (*env)->ReleaseStringUTFChars(env,jfiles,cfiles);
        return -4;
    }
    if((u128)N>re-rs+1) {
        (*env)->ReleaseStringUTFChars(env,jstart,cstart);
        (*env)->ReleaseStringUTFChars(env,jend,cend);
        (*env)->ReleaseStringUTFChars(env,jpart,cpart);
        (*env)->ReleaseStringUTFChars(env,jtarget,ctarget);
        (*env)->ReleaseStringUTFChars(env,jfiles,cfiles);
        return -5;
    }

    unsigned char h[20];
    int tok = 0;
    if(strlen(ctarget)==40) tok = hex_to_bytes(ctarget,h,20);
    if(!tok){
        unsigned char pay[21];
        if(b58check_decode(ctarget,pay)){ memcpy(h,pay+1,20); tok=1; }
    }
    if(!tok){
        (*env)->ReleaseStringUTFChars(env,jstart,cstart);
        (*env)->ReleaseStringUTFChars(env,jend,cend);
        (*env)->ReleaseStringUTFChars(env,jpart,cpart);
        (*env)->ReleaseStringUTFChars(env,jtarget,ctarget);
        (*env)->ReleaseStringUTFChars(env,jfiles,cfiles);
        return -6;
    }
    set_target(h);

    snprintf(g_files_dir, sizeof g_files_dir, "%s", cfiles);

    u128 ns,ne;
    apply_part(rs,re,N,K,&ns,&ne);

    g_range_start=ns; g_range_end=ne;
    if(threads<1)threads=1; if(threads>MAX_THREADS)threads=MAX_THREADS;

    int* arg = (int*)malloc(sizeof(int));
    *arg = mode;
    g_blk_next = (u128)threads;
    g_chunk = (chunkSize < 1) ? 1 : (u128)chunkSize;

    pthread_t tid;
    pthread_create(&tid,NULL,controller_main,arg);
    pthread_detach(tid);

    (*env)->ReleaseStringUTFChars(env,jstart,cstart);
    (*env)->ReleaseStringUTFChars(env,jend,cend);
    (*env)->ReleaseStringUTFChars(env,jpart,cpart);
    (*env)->ReleaseStringUTFChars(env,jtarget,ctarget);
    (*env)->ReleaseStringUTFChars(env,jfiles,cfiles);
    return 0;
}

JNIEXPORT void JNICALL
Java_com_pz71_searcher_Native_stop(JNIEnv*env, jclass clazz){
    (void)env;(void)clazz;
    atomic_store(&g_stop,1);
}

JNIEXPORT jstring JNICALL
Java_com_pz71_searcher_Native_status(JNIEnv*env, jclass clazz){
    (void)clazz;
    int state = atomic_load(&g_ui_state);
    const char* state_s =
        state==0 ? "idle" :
        state==1 ? "running" :
        state==2 ? "found" :
        state==3 ? "done" : "stopped";

    double elapsed = (state==0)?0.0:(now_sec()-g_t_start);
    if(elapsed<0.001) elapsed=0.001;

    unsigned long long keys = atomic_load(&g_keys);
    double spd = (double)keys/elapsed;

    u128 span = (g_range_end>=g_range_start)?(g_range_end-g_range_start+1):1;
    double pct = 100.0*(double)keys/(double)span;

    u128 cur;
    if(state==1){
        cur=((u128)atomic_load(&g_cur_hi[0])<<64)|atomic_load(&g_cur_lo[0]);
        if(cur==0) cur=g_range_start;
    } else cur=g_range_start;
    char curhex[33]; u128_to_hex(cur,curhex);

    char spdstr[32];
    if(spd>=1e9) snprintf(spdstr,sizeof spdstr,"%.2fG",spd/1e9);
    else if(spd>=1e6) snprintf(spdstr,sizeof spdstr,"%.2fM",spd/1e6);
    else if(spd>=1e3) snprintf(spdstr,sizeof spdstr,"%.2fk",spd/1e3);
    else snprintf(spdstr,sizeof spdstr,"%.2f",spd);

    char out[1024];
    snprintf(out,sizeof out,"%s|%.1f|%llu|%s|%.4f|%s|%s|%s|%s|%s",
        state_s, elapsed, keys, spdstr, pct, hex_trim(curhex),
        g_found_hex, g_found_wif, g_found_addr, g_last_msg);
    return (*env)->NewStringUTF(env,out);
}
JNIEXPORT jstring JNICALL
Java_com_pz71_searcher_Native_getCurrentKey(JNIEnv*env, jclass clazz){
    (void)clazz;
    u128 k = g_blk_next;
    char h[33]; u128_to_hex(k, h);
    return (*env)->NewStringUTF(env, h);
}