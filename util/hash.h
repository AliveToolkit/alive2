#include <cstring>

// halfsip(2,4) hash
// Inspired from https://github.com/veorq/SipHash/blob/master/halfsiphash.c
// In public domain

#define ROTL(x, b) (uint32_t)(((x) << (b)) | ((x) >> (32 - (b))))

#define U32TO8_LE(p, v)          \
  (p)[0] = (uint8_t)((v));       \
  (p)[1] = (uint8_t)((v) >> 8);  \
  (p)[2] = (uint8_t)((v) >> 16); \
  (p)[3] = (uint8_t)((v) >> 24);

#define U8TO32_LE(p)                                        \
  (((uint32_t)((p)[0])) | ((uint32_t)((p)[1]) << 8) |       \
   ((uint32_t)((p)[2]) << 16) | ((uint32_t)((p)[3]) << 24))

class GenHash {
  uint32_t v0 = 0;
  uint32_t v1 = 0;
  uint32_t v2 = UINT32_C(0x6c796765);
  uint32_t v3 = UINT32_C(0x74656462);
  uint32_t total_length = 0;

  void sipround() {
    v0 += v1;
    v1 = ROTL(v1, 5);
    v1 ^= v0;
    v0 = ROTL(v0, 16);
    v2 += v3;
    v3 = ROTL(v3, 8);
    v3 ^= v2;
    v0 += v3;
    v3 = ROTL(v3, 7);
    v3 ^= v0;
    v2 += v1;
    v1 = ROTL(v1, 13);
    v1 ^= v2;
    v2 = ROTL(v2, 16);
  }

  void c_rounds() {
    sipround();
    sipround();
  }

  void d_rounds() {
    sipround();
    sipround();
    sipround();
    sipround();
  }

public:
  void add(const void *ptr, size_t inlen) {
    unsigned char *ni = (unsigned char *)ptr;
    const unsigned char *end = ni + inlen - (inlen % sizeof(uint32_t));

    total_length += (uint32_t)inlen;

    for (; ni != end; ni += 4) {
      uint32_t m = U8TO32_LE(ni);
      v3 ^= m;
      c_rounds();
      v0 ^= m;
    }

    if (int left = inlen & 3) {
      uint32_t b = ((uint32_t)inlen) << 24;
      switch (left) {
      case 3:
        b |= ((uint32_t)ni[2]) << 16;
        [[fallthrough]];
      case 2:
        b |= ((uint32_t)ni[1]) << 8;
        [[fallthrough]];
      case 1:
        b |= ((uint32_t)ni[0]);
        break;
      }
      v3 ^= b;
      c_rounds();
      v0 ^= b;
    }
  }

  void add(uint8_t v) {
    add(&v, sizeof(v));
  }

  unsigned operator()() {
    uint32_t b = total_length << 24;
    v3 ^= b;
    c_rounds();
    v0 ^= b;

    v2 ^= 0xff;
    d_rounds();
    b = v1 ^ v3;

    uint8_t array[4];
    U32TO8_LE(array, b);
    unsigned out;
    std::memcpy(&out, array, sizeof(out));
    return out;
  }
};
