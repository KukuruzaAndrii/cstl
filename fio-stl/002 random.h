/* ************************************************************************* */
#if !defined(FIO_INCLUDE_FILE) /* Dev test - ignore line */
#define FIO___DEV___           /* Development inclusion - ignore line */
#define FIO_RAND               /* Development inclusion - ignore line */
#include "./include.h"         /* Development inclusion - ignore line */
#endif                         /* Development inclusion - ignore line */
/* *****************************************************************************




                      Psedo-Random Generator Functions
                    and friends - risky hash / stable hash



Copyright and License: see header file (000 copyright.h) / top of file
***************************************************************************** */
#if defined(FIO_RAND) && !defined(H___FIO_RAND_H)
#define H___FIO_RAND_H

/* *****************************************************************************
Random - API
***************************************************************************** */

/** Returns 64 psedo-random bits. Probably not cryptographically safe. */
SFUNC uint64_t fio_rand64(void);

/** Writes `len` bytes of psedo-random bits to the target buffer. */
SFUNC void fio_rand_bytes(void *target, size_t len);

/** Feeds up to 1023 bytes of entropy to the random state. */
IFUNC void fio_rand_feed2seed(void *buf_, size_t len);

/** Reseeds the random engine using system state (rusage / jitter). */
SFUNC void fio_rand_reseed(void);

/* *****************************************************************************
Risky / Stable Hash - API
***************************************************************************** */

/** Computes a facil.io Risky Hash (Risky v.3). */
SFUNC uint64_t fio_risky_hash(const void *buf, size_t len, uint64_t seed);

/** Adds a bit of entropy to pointer values. Designed to be unsafe. */
FIO_IFUNC uint64_t fio_risky_ptr(void *ptr);

/** Adds a bit of entropy to numeral values. Designed to be unsafe. */
FIO_IFUNC uint64_t fio_risky_num(uint64_t number, uint64_t seed);

/** Computes a facil.io Stable Hash (will not be updated, even if broken). */
SFUNC uint64_t fio_stable_hash(const void *data, size_t len, uint64_t seed);

/** Computes a facil.io Stable Hash (will not be updated, even if broken). */
SFUNC void fio_stable_hash128(void *restrict dest,
                              const void *restrict data,
                              size_t len,
                              uint64_t seed);

#define FIO_USE_STABLE_HASH_WHEN_CALLING_RISKY_HASH 0
/* *****************************************************************************
Risky Hash - Implementation

Note: I don't remember what information I used when designing this, but Risky
Hash is probably NOT cryptographically safe (though I wish it was).

Here's a few resources about hashes that might explain more:
- https://komodoplatform.com/cryptographic-hash-function/
- https://en.wikipedia.org/wiki/Avalanche_effect
- http://ticki.github.io/blog/designing-a-good-non-cryptographic-hash-function/

***************************************************************************** */

/* Primes with with 16 bits, half of them set. */
#define FIO_U16_HASH_PRIME0 0xDA23U
#define FIO_U16_HASH_PRIME1 0xB48BU
#define FIO_U16_HASH_PRIME2 0xC917U
#define FIO_U16_HASH_PRIME3 0xD855U
#define FIO_U16_HASH_PRIME4 0xE0B9U
#define FIO_U16_HASH_PRIME5 0xE471U
#define FIO_U16_HASH_PRIME6 0x85CDU
#define FIO_U16_HASH_PRIME7 0xD433U
#define FIO_U16_HASH_PRIME8 0xE951U
#define FIO_U16_HASH_PRIME9 0xA8E5U

/* Primes with with 32 bits, half of them set. */
#define FIO_U32_HASH_PRIME0 0xC19F5985UL
#define FIO_U32_HASH_PRIME1 0x8D567931UL
#define FIO_U32_HASH_PRIME2 0x9C178B17UL
#define FIO_U32_HASH_PRIME3 0xA4B842DFUL
#define FIO_U32_HASH_PRIME4 0xB0B94EC9UL
#define FIO_U32_HASH_PRIME5 0xFA9E7084UL
#define FIO_U32_HASH_PRIME6 0xCA63037BUL
#define FIO_U32_HASH_PRIME7 0xD728C15DUL
#define FIO_U32_HASH_PRIME8 0xA872A277UL
#define FIO_U32_HASH_PRIME9 0xF5781551UL

/* Primes with with 64 bits, half of them set. */
#define FIO_U64_HASH_PRIME0 0x39664DEECA23D825
#define FIO_U64_HASH_PRIME1 0x48644F7B3959621F
#define FIO_U64_HASH_PRIME2 0x613A19F5CB0D98D5
#define FIO_U64_HASH_PRIME3 0x84B56B93C869EA0F
#define FIO_U64_HASH_PRIME4 0x8EE38D13E0D95A8D
#define FIO_U64_HASH_PRIME5 0x92E99EC981F0E279
#define FIO_U64_HASH_PRIME6 0xDDC3100BEF158BB1
#define FIO_U64_HASH_PRIME7 0x918F4D38049F78BD
#define FIO_U64_HASH_PRIME8 0xB6C9F8032A35E2D9
#define FIO_U64_HASH_PRIME9 0xFA2A5F16D2A128D5

/** Adds bit entropy to a pointer values. Designed to be unsafe. */
FIO_IFUNC uint64_t fio_risky_num(uint64_t n, uint64_t seed) {
  seed ^= fio_lrot64(seed, 47);
  seed += FIO_U64_HASH_PRIME0;
  seed = seed | 1;
  uint64_t h = n + seed;
  h += fio_lrot64(seed, 5);
  h += fio_bswap64(seed);
  h += fio_lrot64(h, 27);
  h += fio_lrot64(h, 49);
  return h;
}

/** Adds bit entropy to a pointer values. Designed to be unsafe. */
FIO_IFUNC uint64_t fio_risky_ptr(void *ptr) {
  return fio_risky_num((uint64_t)(uintptr_t)ptr, FIO_U64_HASH_PRIME9);
}

/* *****************************************************************************
Possibly `extern` Implementation
***************************************************************************** */
#if defined(FIO_EXTERN_COMPLETE) || !defined(FIO_EXTERN)

/* Risky Hash initialization constants */
#define FIO_RISKY3_IV0 0x0000001000000001ULL
#define FIO_RISKY3_IV1 0x0000010000000010ULL
#define FIO_RISKY3_IV2 0x0000100000000100ULL
#define FIO_RISKY3_IV3 0x0001000000001000ULL
/* read u64 in local endian */
#define FIO_RISKY_BUF2U64 fio_buf2u64u

/*  Computes a facil.io Risky Hash. */
SFUNC uint64_t fio_risky_hash(const void *data_, size_t len, uint64_t seed) {
#if (FIO_USE_STABLE_HASH_WHEN_CALLING_RISKY_HASH - 1 + 1)
  return fio_stable_hash(data_, len, seed);
#else
  uint64_t v[4] FIO_ALIGN(
      32) = {FIO_RISKY3_IV0, FIO_RISKY3_IV1, FIO_RISKY3_IV2, FIO_RISKY3_IV3};
  uint64_t w[4] FIO_ALIGN(32);
  const uint8_t *data = (const uint8_t *)data_;

#define FIO_RISKY3_ROUND64(vi, w_)                                             \
  w[vi] = w_;                                                                  \
  v[vi] += w[vi];                                                              \
  v[vi] = fio_lrot64(v[vi], 29);                                               \
  v[vi] += w[vi];                                                              \
  v[vi] *= FIO_U64_HASH_PRIME##vi;

#define FIO_RISKY3_ROUND256(w0, w1, w2, w3)                                    \
  FIO_RISKY3_ROUND64(0, w0);                                                   \
  FIO_RISKY3_ROUND64(1, w1);                                                   \
  FIO_RISKY3_ROUND64(2, w2);                                                   \
  FIO_RISKY3_ROUND64(3, w3);

  if (seed) {
    /* process the seed as if it was a prepended 8 Byte string. */
    v[0] *= seed;
    v[1] *= seed;
    v[2] *= seed;
    v[3] *= seed;
    v[1] ^= seed;
    v[2] ^= seed;
    v[3] ^= seed;
  }

  for (size_t i = 31; i < len; i += 32) {
    /* 32 bytes / 256 bit access */
    FIO_RISKY3_ROUND256(FIO_RISKY_BUF2U64(data),
                        FIO_RISKY_BUF2U64(data + 8),
                        FIO_RISKY_BUF2U64(data + 16),
                        FIO_RISKY_BUF2U64(data + 24));
    data += 32;
  }
  switch (len & 24) {
  case 24: FIO_RISKY3_ROUND64(2, FIO_RISKY_BUF2U64(data + 16)); /*fall through*/
  case 16: FIO_RISKY3_ROUND64(1, FIO_RISKY_BUF2U64(data + 8));  /*fall through*/
  case 8: FIO_RISKY3_ROUND64(0, FIO_RISKY_BUF2U64(data + 0)); data += len & 24;
  }

  /* add offset information to padding */
  uint64_t tmp = ((uint64_t)len & 0xFF) << 56;
  /* leftover bytes */
  switch ((len & 7)) {
  case 7: tmp |= ((uint64_t)data[6]) << 48; /* fall through */
  case 6: tmp |= ((uint64_t)data[5]) << 40; /* fall through */
  case 5: tmp |= ((uint64_t)data[4]) << 32; /* fall through */
  case 4: tmp |= ((uint64_t)data[3]) << 24; /* fall through */
  case 3: tmp |= ((uint64_t)data[2]) << 16; /* fall through */
  case 2: tmp |= ((uint64_t)data[1]) << 8;  /* fall through */
  case 1:
    tmp |= ((uint64_t)data[0]);
    switch ((len & 24)) { /* the last (now padded) byte's position */
    case 24: FIO_RISKY3_ROUND64(3, tmp); break; /*offset 24 in 32 byte segment*/
    case 16: FIO_RISKY3_ROUND64(2, tmp); break; /*offset 16 in 32 byte segment*/
    case 8: FIO_RISKY3_ROUND64(1, tmp); break;  /*offset  8 in 32 byte segment*/
    case 0: FIO_RISKY3_ROUND64(0, tmp); break;  /*offset  0 in 32 byte segment*/
    }
  }

  /* irreversible avalanche... I think */
  uint64_t r = (len) ^ ((uint64_t)len << 36);
  r += fio_lrot64(v[0], 17) + fio_lrot64(v[1], 13) + fio_lrot64(v[2], 47) +
       fio_lrot64(v[3], 57);
  r += v[0] ^ v[1];
  r ^= fio_lrot64(r, 13);
  r += v[1] ^ v[2];
  r ^= fio_lrot64(r, 29);
  r += v[2] ^ v[3];
  r += fio_lrot64(r, 33);
  r += v[3] ^ v[0];
  r ^= (r >> 29) * FIO_U64_HASH_PRIME4;
  r ^= fio_lrot64(r, 29);
  return r;
#endif /* FIO_USE_STABLE_HASH_WHEN_CALLING_RISKY_HASH */
}

#undef FIO_RISKY3_IV0
#undef FIO_RISKY3_IV1
#undef FIO_RISKY3_IV2
#undef FIO_RISKY3_IV3
#undef FIO_RISKY_BUF2U64
#undef FIO_RISKY3_ROUND64
#undef FIO_RISKY3_ROUND256

/* *****************************************************************************
Stable Hash (unlike Risky Hash, this can be used for non-ephemeral hashing)
***************************************************************************** */
#define FIO_STABLE_HASH_ROUND_WORD(i)                                          \
  v[i] += w[i];                                                                \
  v[i] += prime[i];                                                            \
  v[i] *= prime[i];                                                            \
  w[i] = fio_lrot64(w[i], 19);                                                 \
  v[i] += w[i] + seed;

FIO_IFUNC void fio_stable_hash___inner(uint64_t dest[4],
                                       const void *restrict data_,
                                       const size_t len,
                                       uint64_t seed) {
  const uint8_t *data = (const uint8_t *)data_;
  /* seed selection is constant time to avoid leaking seed data */
  seed += len;
  seed ^= fio_lrot64(seed, 47);
  seed = (seed << 1) + 1;
  uint64_t v[4] = {seed, seed, seed, seed};
  uint64_t const prime[4] = {FIO_U32_HASH_PRIME0,
                             FIO_U32_HASH_PRIME1,
                             FIO_U32_HASH_PRIME2,
                             FIO_U32_HASH_PRIME3};

  for (size_t j = 31; j < len; j += 32) {
    /* consumes 32 bytes (256 bits) each loop */
    uint64_t w[4];
    for (size_t i = 0; i < 4; ++i) {
      w[i] = fio_ltole64(fio_buf2u64u(data));
      data += 8;
      FIO_STABLE_HASH_ROUND_WORD(i);
    }
  }
  { /* pad with zeros (even if %32 == 0) and add len to last word */
    uint64_t w[4] = {0};
    fio_memcpy31x(w, data, len); /* copies `len & 31` bytes */
    for (size_t i = 0; i < 4; ++i)
      w[i] = fio_ltole64(w[i]);
    w[3] += len;
    for (size_t i = 0; i < 4; ++i) {
      FIO_STABLE_HASH_ROUND_WORD(i);
    }
  }
  for (size_t i = 0; i < 4; ++i) {
    dest[i] = v[i];
  }
}

/* Computes a facil.io Stable Hash. */
SFUNC uint64_t fio_stable_hash(const void *data_, size_t len, uint64_t seed) {
  uint64_t r;
  uint64_t v[4];
  fio_stable_hash___inner(v, data_, len, seed);
  /* summing & avalanche */
  r = v[0] + v[1] + v[2] + v[3];
  for (size_t i = 0; i < 4; ++i)
    v[i] = fio_bswap64(v[i]);
  r ^= fio_lrot64(r, 5);
  r += v[0] ^ v[1];
  r ^= fio_lrot64(r, 27);
  r += v[1] ^ v[2];
  r ^= fio_lrot64(r, 49);
  r += v[2] ^ v[3];
  r ^= (r >> 29) * FIO_U64_HASH_PRIME0;
  r ^= fio_lrot64(r, 29);
  return r;
}

SFUNC void fio_stable_hash128(void *restrict dest,
                              const void *restrict data_,
                              size_t len,
                              uint64_t seed) {

  uint64_t v[4];
  fio_stable_hash___inner(v, data_, len, seed);
  uint64_t r[2];
  uint64_t prime[2] = {FIO_U64_HASH_PRIME0, FIO_U64_HASH_PRIME1};
  r[0] = v[0] + v[1] + v[2] + v[3];
  r[1] = v[0] ^ v[1] ^ v[2] ^ v[3];
  for (size_t i = 0; i < 4; ++i)
    v[i] = fio_bswap64(v[i]);
  for (size_t i = 0; i < 2; ++i) {
    r[i] ^= fio_lrot64(r[i], 5);
    r[i] += v[0] ^ v[1];
    r[i] ^= fio_lrot64(r[i], 27);
    r[i] += v[1] ^ v[2];
    r[i] ^= fio_lrot64(r[i], 49);
    r[i] += v[2] ^ v[3];
    r[i] ^= (r[i] >> 29) * prime[i];
    r[i] ^= fio_lrot64(r[i], 29);
  }
  fio_memcpy16(dest, r);
}

#undef FIO_STABLE_HASH_ROUND_WORD
/* *****************************************************************************
Random - Implementation
***************************************************************************** */

#if FIO_OS_POSIX ||                                                            \
    (__has_include("sys/resource.h") && __has_include("sys/time.h"))
#include <sys/resource.h>
#include <sys/time.h>
#endif

static volatile uint64_t fio___rand_state[4]; /* random state */
static volatile size_t fio___rand_counter;    /* seed counter */
/* feeds random data to the algorithm through this 256 bit feed. */
static volatile uint64_t fio___rand_buffer[4] = {0x9c65875be1fce7b9ULL,
                                                 0x7cc568e838f6a40d,
                                                 0x4bb8d885a0fe47d5,
                                                 0x95561f0927ad7ecd};

IFUNC void fio_rand_feed2seed(void *buf_, size_t len) {
  len &= 1023;
  uint8_t *buf = (uint8_t *)buf_;
  uint8_t offset = (fio___rand_counter & 3);
  uint64_t tmp = 0;
  for (size_t i = 0; i < (len >> 3); ++i) {
    tmp = fio_buf2u64u(buf);
    fio___rand_buffer[(offset++ & 3)] ^= tmp;
    buf += 8;
  }
  switch (len & 7) {
  case 7: tmp <<= 8; tmp |= buf[6]; /* fall through */
  case 6: tmp <<= 8; tmp |= buf[5]; /* fall through */
  case 5: tmp <<= 8; tmp |= buf[4]; /* fall through */
  case 4: tmp <<= 8; tmp |= buf[3]; /* fall through */
  case 3: tmp <<= 8; tmp |= buf[2]; /* fall through */
  case 2: tmp <<= 8; tmp |= buf[1]; /* fall through */
  case 1:
    tmp <<= 8;
    tmp |= buf[1];
    fio___rand_buffer[(offset & 3)] ^= tmp;
  }
}

SFUNC void fio_rand_reseed(void) {
  const size_t jitter_samples = 16 | (fio___rand_state[0] & 15);
#if defined(RUSAGE_SELF)
  {
    struct rusage rusage;
    getrusage(RUSAGE_SELF, &rusage);
    fio___rand_state[0] ^=
        fio_risky_hash(&rusage, sizeof(rusage), fio___rand_state[0]);
  }
#endif
  for (size_t i = 0; i < jitter_samples; ++i) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    uint64_t clk =
        (uint64_t)((t.tv_sec << 30) + (int64_t)t.tv_nsec) + fio___rand_counter;
    fio___rand_state[0] ^= fio_risky_num(clk, fio___rand_state[0] + i);
    fio___rand_state[1] ^= fio_risky_num(clk, fio___rand_state[1] + i);
  }
  {
    uint64_t tmp[2];
    tmp[0] = fio_risky_num(fio___rand_buffer[0], fio___rand_state[0]) +
             fio_risky_num(fio___rand_buffer[1], fio___rand_state[1]);
    tmp[1] = fio_risky_num(fio___rand_buffer[2], fio___rand_state[0]) +
             fio_risky_num(fio___rand_buffer[3], fio___rand_state[1]);
    fio___rand_state[2] ^= tmp[0];
    fio___rand_state[3] ^= tmp[1];
  }
  fio___rand_buffer[0] = fio_lrot64(fio___rand_buffer[0], 31);
  fio___rand_buffer[1] = fio_lrot64(fio___rand_buffer[1], 29);
  fio___rand_buffer[2] ^= fio___rand_buffer[0];
  fio___rand_buffer[3] ^= fio___rand_buffer[1];
  fio___rand_counter += jitter_samples;
}

/* tested for randomness using code from: http://xoshiro.di.unimi.it/hwd.php */
SFUNC uint64_t fio_rand64(void) {
  /* modeled after xoroshiro128+, by David Blackman and Sebastiano Vigna */
  uint64_t r = 0;
  if (!((fio___rand_counter++) & (((size_t)1 << 12) - 1))) {
    /* re-seed state every 524,288 requests / 2^19-1 attempts  */
    fio_rand_reseed();
  }
  const uint64_t s0[] = {fio___rand_state[0],
                         fio___rand_state[1],
                         fio___rand_state[2],
                         fio___rand_state[3]}; /* load to registers */
  uint64_t s1[4];
  {
    const uint64_t mulp[] = {0x37701261ED6C16C7ULL,
                             0x764DBBB75F3B3E0DULL,
                             ~(0x37701261ED6C16C7ULL),
                             ~(0x764DBBB75F3B3E0DULL)}; /* load to registers */
    const uint64_t addc[] = {fio___rand_counter, 0, fio___rand_counter, 0};
    for (size_t i = 0; i < 4; ++i) {
      s1[i] = fio_lrot64(s0[i], 33);
      s1[i] += addc[i];
      s1[i] *= mulp[i];
      s1[i] += s0[i];
    }
  }
  for (size_t i = 0; i < 4; ++i) { /* store to memory */
    fio___rand_state[i] = s1[i];
  }
  {
    uint8_t rotc[] = {31, 29, 27, 30};
    for (size_t i = 0; i < 4; ++i) {
      s1[i] = fio_lrot64(s1[i], rotc[i]);
      r += s1[i];
    }
  }
  return r;
}

/* copies 64 bits of randomness (8 bytes) repeatedly. */
SFUNC void fio_rand_bytes(void *data_, size_t len) {
  if (!data_ || !len)
    return;
  uint8_t *data = (uint8_t *)data_;
  for (unsigned i = 31; i < len; i += 32) {
    uint64_t rv[4] = {fio_rand64(), fio_rand64(), fio_rand64(), fio_rand64()};
    fio_memcpy32(data, rv);
    data += 32;
  }
  if (len & 31) {
    uint64_t rv[4] = {fio_rand64(), fio_rand64(), fio_rand64(), fio_rand64()};
    fio_memcpy31x(data, rv, len);
  }
}
/* *****************************************************************************
Random - Cleanup
***************************************************************************** */
#endif /* FIO_EXTERN_COMPLETE */
#endif /* FIO_RAND */
#undef FIO_RAND
