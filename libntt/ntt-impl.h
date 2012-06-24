#ifndef _NTT_IMPL_H
#define _NTT_IMPL_H

#include "sp.h"

typedef uint32_t (*get_num_ntt_const_t)(void);

typedef void (*nttdata_init_t)(spv_t out, 
				sp_t p, sp_t d,
				sp_t primroot, sp_t order);

typedef void (*ntt_run_t)(spv_t x, spv_size_t stride,
			  sp_t p, spv_t ntt_const);

typedef void (*ntt_pfa_run_t)(spv_t x, spv_size_t cofactor, 
			  sp_t p, spv_t ntt_const);

typedef void (*ntt_twiddle_run_t)(spv_t x, spv_size_t stride,
			  spv_size_t num_transforms, 
			  sp_t p, spv_t ntt_const);


/* a copy of sp_add, but operating on array offsets. These
   are always size_t types, and may have a size different 
   from an sp_t */

static inline spv_size_t sp_array_inc(spv_size_t a, spv_size_t b, spv_size_t m) 
{
#if (defined(__GNUC__) || defined(__ICL)) && \
    (defined(__x86_64__) || defined(__i386__))

  spv_size_t t = a - m, tr = a + b;

  __asm__ (
    "add %2, %1    # sp_array_inc: t += b\n\t"
    "cmovc %1, %0  # sp_array_inc: if (cy) tr = t \n\t"
    : "+r" (tr), "+&r" (t)
    : "g" (b)
    : "cc"
  );

  return tr;

#elif defined(_MSC_VER) && !defined(_WIN64)

  __asm
    {
        mov     eax, a
        add     eax, b
        mov     edx, eax
        sub     edx, m
        cmovnc  eax, edx
    }

#else

  spv_size_t t = a + b;
  if (t >= m)
    t -= m;
  return t;

#endif
}

sp_t sp_ntt_reciprocal (sp_t w, sp_t p);

/* if the modulus is 2 bits or more smaller than the machine
   word size, the core NTT routines use a redundant representation 
   of the transform elements. Modular multiplies do not do their
   final modular reduction, and inputs to the multiplies are not 
   reduced mod p either. The transform results are in [0, 2p) */

#if SP_NUMB_BITS < SP_TYPE_BITS - 1
#define HAVE_PARTIAL_MOD
#endif

static inline sp_t sp_add_partial(sp_t a, sp_t b, sp_t p)
{
#ifdef HAVE_PARTIAL_MOD
  return a + b;
#else
  return sp_add(a, b, p);
#endif
}

static inline sp_t sp_sub_partial(sp_t a, sp_t b, sp_t p)
{
#ifdef HAVE_PARTIAL_MOD
  return a - b + p;
#else
  return sp_sub(a, b, p);
#endif
}

/* perform modular multiplication when the multiplier
   and its generalized inverse are both known and precomputed */

static inline sp_t sp_ntt_mul(sp_t x, sp_t w, sp_t w_inv, sp_t p)
{
  sp_t r;

#if SP_TYPE_BITS == GMP_LIMB_BITS  /* use GMP functions */

  mp_limb_t q;
  ATTRIBUTE_UNUSED mp_limb_t tmp;
  umul_ppmm(q, tmp, x, w_inv);

#elif SP_TYPE_BITS < GMP_LIMB_BITS  /* ordinary multiply */

  mp_limb_t prod = (mp_limb_t)(x) * (mp_limb_t)(w_inv);
  sp_t q = (sp_t)(prod >> SP_TYPE_BITS);

#else  /* worst case: build product from smaller multiplies */

  sp_t q;
  mp_limb_t a1 = (mp_limb_t)((x) >> GMP_LIMB_BITS);
  mp_limb_t a0 = (mp_limb_t)(x);
  mp_limb_t b1 = (mp_limb_t)((w_inv) >> GMP_LIMB_BITS);
  mp_limb_t b0 = (mp_limb_t)(w_inv);
  mp_limb_t a0b0_hi, a0b0_lo;
  mp_limb_t a0b1_hi, a0b1_lo;
  mp_limb_t a1b0_hi, a1b0_lo;
  mp_limb_t a1b1_hi, a1b1_lo;
  mp_limb_t cy1, cy2;

  umul_ppmm(a0b0_hi, a0b0_lo, a0, b0);
  umul_ppmm(a0b1_hi, a0b1_lo, a0, b1);
  umul_ppmm(a1b0_hi, a1b0_lo, a1, b0);
  umul_ppmm(a1b1_hi, a1b1_lo, a1, b1);

  /* both inputs will usually have all their bits 
     significant, so carries can propagate the entire 
     length of the 128-bit product */

  add_ssaaaa(cy1, a0b0_hi,
      	     0, a0b0_hi,
	     0, a0b1_lo);
  add_ssaaaa(cy1, a0b0_hi,
      	     cy1, a0b0_hi,
	     0, a1b0_lo);
  add_ssaaaa(cy2, a1b1_lo,
      	     0, a1b1_lo,
	     0, cy1);
  add_ssaaaa(cy2, a1b1_lo,
      	     cy2, a1b1_lo,
	     0, a0b1_hi);
  add_ssaaaa(cy2, a1b1_lo,
      	     cy2, a1b1_lo,
	     0, a1b0_hi);

  q = (sp_t)(a1b1_hi + cy2) << GMP_LIMB_BITS | a1b1_lo;

#endif

#ifdef HAVE_PARTIAL_MOD
  r = x * w - q * (p >> 1);
  return r;
#else
  r = x * w - q * p;
  return sp_sub(r, p, p);
#endif
}

/*------------------- definitions for SIMD transforms ----------------*/

#ifdef HAVE_SSE2

typedef __m128i sp_simd_t;

#define SP_SIMD_VSIZE (128 / SP_TYPE_BITS)

static inline sp_simd_t sp_simd_gather(spv_t x)
{
#if SP_TYPE_BITS == 32

  return ploadu(x);

#else

  sp_simd_t t = pload_lo64(x);
  return pload_hi64(t, x + 1);

#endif
}

static inline sp_simd_t sp_simd_pfa_gather(spv_t x, spv_size_t start_off, 
					spv_size_t inc, spv_size_t n)
{
#if SP_TYPE_BITS == 32

  spv_size_t j0 = start_off;
  spv_size_t j1 = sp_array_inc(j0, inc, n);
  spv_size_t j2 = sp_array_inc(j0, 2 * inc, n);
  spv_size_t j3 = sp_array_inc(j0, 3 * inc, n);
  sp_simd_t t0 = pload_lo32(x + j0);
  sp_simd_t t1 = pload_lo32(x + j1);
  sp_simd_t t2 = pload_lo32(x + j2);
  sp_simd_t t3 = pload_lo32(x + j3);
  sp_simd_t r0 = punpcklo32(t0, t1);
  sp_simd_t r1 = punpcklo32(t2, t3);
  return punpcklo64(r0, r1);

#else

  spv_size_t j0 = start_off;
  spv_size_t j1 = sp_array_inc(j0, inc, n);
  sp_simd_t t = pload_lo64(x + j0);
  return pload_hi64(t, x + j1);

#endif
}

static inline void sp_simd_scatter(sp_simd_t t, spv_t x)
{
#if SP_TYPE_BITS == 32

  pstoreu(t, x);

#else

  pstore_lo64(t, x);
  pstore_hi64(t, x + 1);

#endif
}

static inline void sp_simd_pfa_scatter(sp_simd_t t, spv_t x, 
    				spv_size_t start_off, 
				spv_size_t inc, spv_size_t n)
{
#if SP_TYPE_BITS == 32

  spv_size_t j0 = start_off;
  spv_size_t j1 = sp_array_inc(j0, inc, n);
  spv_size_t j2 = sp_array_inc(j0, 2 * inc, n);
  spv_size_t j3 = sp_array_inc(j0, 3 * inc, n);
  pstore_lo32(t, x + j0);
  t = _mm_srli_si128(t, 4);
  pstore_lo32(t, x + j1);
  t = _mm_srli_si128(t, 4);
  pstore_lo32(t, x + j2);
  t = _mm_srli_si128(t, 4);
  pstore_lo32(t, x + j3);

#else

  spv_size_t j0 = start_off;
  spv_size_t j1 = sp_array_inc(j0, inc, n);
  pstore_lo64(t, x + j0);
  pstore_hi64(t, x + j1);

#endif
}

static inline sp_simd_t sp_simd_add(sp_simd_t a, sp_simd_t b, sp_t p)
{
#if SP_TYPE_BITS == 32

  sp_simd_t vp = pshufd(pcvt_i32(p), 0x00);
  sp_simd_t t0, t1;

  t0 = paddd(a, b);
  t0 = psubd(t0, vp);
  t1 = pcmpgtd(psetzero(), t0);
  t1 = pand(t1, vp);
  return paddd(t0, t1);

#else

  sp_simd_t vp = pshufd(pcvt_i64(p), 0x44);
  sp_simd_t t0, t1;

  t0 = paddq(a, b);
  t0 = psubq(t0, vp);
  t1 = pcmpgtd(psetzero(), t0);
  t1 = pshufd(t1, 0xf5);
  t1 = pand(t1, vp);
  return paddq(t0, t1);

#endif
}

static inline sp_simd_t sp_simd_add_partial(sp_simd_t a, sp_simd_t b, sp_t p)
{
#ifdef HAVE_PARTIAL_MOD

  #if SP_TYPE_BITS == 32
  return paddd(a, b);
  #else
  return paddq(a, b);
  #endif

#else

  return sp_simd_add(a, b, p);

#endif
}

static inline sp_simd_t sp_simd_sub(sp_simd_t a, sp_simd_t b, sp_t p)
{
#if SP_TYPE_BITS == 32

  sp_simd_t vp = pshufd(pcvt_i32(p), 0x00);
  sp_simd_t t0, t1;

  t0 = psubd(a, b);
  t1 = pcmpgtd(psetzero(), t0);
  t1 = pand(t1, vp);
  return paddd(t0, t1);

#else

  sp_simd_t vp = pshufd(pcvt_i64(p), 0x44);
  sp_simd_t t0, t1;

  t0 = psubq(a, b);
  t1 = pcmpgtd(psetzero(), t0);
  t1 = pshufd(t1, 0xf5);
  t1 = pand(t1, vp);
  return paddq(t0, t1);

#endif
}

static inline sp_simd_t sp_simd_sub_partial(sp_simd_t a, sp_simd_t b, sp_t p)
{
#ifdef HAVE_PARTIAL_MOD

  #if SP_TYPE_BITS == 32

  sp_simd_t vp = pshufd(pcvt_i32(p), 0x00);
  return paddd(psubd(a, b), vp);

  #else

  sp_simd_t vp = pshufd(pcvt_i64(p), 0x44);
  return paddq(psubq(a, b), vp);

  #endif

#else

  return sp_simd_sub(a, b, p);

#endif
}


static inline sp_simd_t sp_simd_ntt_mul(sp_simd_t a, sp_t w, 
					sp_t w_inv, sp_t p)
{
#if SP_TYPE_BITS == 32

  sp_simd_t t0, t1, t2, t3, vp, vw, vwi;

  #ifdef HAVE_PARTIAL_MOD
  vp = pshufd(pcvt_i32(p >> 1), 0x00);
  #else
  vp = pshufd(pcvt_i32(p), 0x00);
  #endif
  vw = pshufd(pcvt_i32(w), 0x00);
  vwi= pshufd(pcvt_i32(w_inv), 0x00);

  t0 = pmuludq(a, vwi);
  t1 = pshufd(a, 0x31);
  t2 = pmuludq(t1, vwi);

  t3 = pmuludq(a, vw);
  t1 = pmuludq(t1, vw);

  t0 = psrlq(t0, 32);
  t2 = psrlq(t2, 32);
  t0 = pmuludq(t0, vp);
  t2 = pmuludq(t2, vp);

  t3 = psubq(t3, t0);
  t1 = psubq(t1, t2);

  t3 = pshufd(t3, 0x08);
  t1 = pshufd(t1, 0x08);
  t3 = punpcklo32(t3, t1);

  #ifdef HAVE_PARTIAL_MOD
  return t3;
  #else
  return sp_simd_sub(t3, vp, p);
  #endif

#elif GMP_LIMB_BITS == 32   /* 64-bit sp_t on a 32-bit machine */

  sp_simd_t vp, vw, vwi, vmask;
  sp_simd_t t0, t1, t2, t3, t4, t5, t6;

  #ifdef HAVE_PARTIAL_MOD
  vp = pshufd(pcvt_i64(p), 0x44);
  vp = psrlq(vp, 1);
  #else
  vp = pshufd(pcvt_i64(p), 0x44);
  #endif
  vw = pshufd(pcvt_i64(w), 0x44);
  vwi= pshufd(pcvt_i64(w_inv), 0x44);
  vmask = pshufd(pcvt_i32(0xffffffff), 0x44);

  t0 = pmuludq(a, vwi);
  t4 = pshufd(a, 0xf5);
  t1 = pmuludq(t4, vwi);
  t5 = pshufd(vwi, 0xf5);
  t2 = pmuludq(t5, a);
  t3 = pmuludq(t4, t5);

  t4 = psrlq(t1, 32);
  t5 = psrlq(t2, 32);
  t3 = paddq(t3, t4);
  t3 = paddq(t3, t5);

  t4 = psrlq(t0, 32);
  t1 = pand(t1, vmask);
  t2 = pand(t2, vmask);
  t4 = paddq(t4, t1);
  t4 = paddq(t4, t2);
  t4 = psrlq(t4, 32);
  t3 = paddq(t3, t4);  /* t3 = hi64(a * winv) */

  t0 = pmuludq(a, vw);
  t4 = pshufd(a, 0xf5);
  t1 = pmuludq(t4, vw);
  t5 = pshufd(vw, 0xf5);
  t2 = pmuludq(t5, a);

  t1 = psllq(t1, 32);
  t2 = psllq(t2, 32);
  t6 = paddq(t0, t1);
  t6 = paddq(t6, t2); /* t6 = lo64(a * w) */

  t0 = pmuludq(t3, vp);
  t4 = pshufd(t3, 0xf5);
  t1 = pmuludq(t4, vp);
  t5 = pshufd(vp, 0xf5);
  t2 = pmuludq(t5, t3);

  t1 = psllq(t1, 32);
  t2 = psllq(t2, 32);
  t0 = paddq(t0, t1);
  t0 = paddq(t0, t2); /* t0 = lo64(t3 * p) */

  t6 = psubq(t6, t0);
  #ifdef HAVE_PARTIAL_MOD
  return t6;
  #else
  return sp_simd_sub(t6, vp, p);
  #endif

#else

  /* there's no way the SSE2 unit can keep up with a
     64-bit multiplier in the ALU */

  sp_simd_t t0, t1;
  sp_t a0, a1;

  pstore_i64(a0, a);
  pstore_i64(a1, pshufd(a, 0x0e));

  a0 = sp_ntt_mul(a0, w, w_inv, p);
  a1 = sp_ntt_mul(a1, w, w_inv, p);

  t0 = pcvt_i64(a0);
  t1 = pcvt_i64(a1);
  return punpcklo64(t0, t1);

#endif
}


#endif  /*-----------------------------------------------------------*/

typedef struct
{
  uint32_t size;
  get_num_ntt_const_t get_num_ntt_const;
  nttdata_init_t nttdata_init;
  ntt_run_t ntt_run;
  ntt_pfa_run_t ntt_pfa_run;
  ntt_twiddle_run_t ntt_twiddle_run;
} nttconfig_t;

/* functions pointers and precomputed data needed by
   all versions of a codelet */

typedef struct
{
  const nttconfig_t *config;
  spv_t ntt_const;
} codelet_data_t;


/* an NTT is built up of one or more passes through
   the input data */

typedef enum
{
  PASS_TYPE_DIRECT,
  PASS_TYPE_PFA,
  PASS_TYPE_TWIDDLE
} pass_type_t;

#define MAX_PFA_CODELETS 6
#define MAX_PASSES 10

typedef struct
{
  pass_type_t type;
  spv_size_t stride;

  union
  {
    struct
    {
      codelet_data_t *codelet;
    } direct;

    struct
    {
      uint32_t num_codelets;
      codelet_data_t *codelets[MAX_PFA_CODELETS];
    } pfa;

  } d;

} nttpass_t;

/* central repository for all NTT data that shares a
   modulus and primitive root */
typedef struct
{
  uint32_t num_codelets;
  codelet_data_t *codelets;
  spv_t codelet_const;

  nttpass_t *passes;
} nttdata_t;

/* external interface */

void * ntt_init(sp_t size, sp_t primroot, sp_t p, sp_t d);
void ntt_free(void *data);

uint32_t planner_init(spm_t spm, sp_t size, spm_t existing);
void planner_free(nttpass_t *passes, uint32_t num_passes);

#endif /* _NTT_IMPL_H */