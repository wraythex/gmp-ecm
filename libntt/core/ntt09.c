#include "ntt/ntt-impl.h"

#define NC 11

static const uint8_t fixed_const[NC] = {1};

static const uint8_t *
ntt9_get_fixed_ntt_const(void)
{
  return fixed_const;
}

void
X(ntt9_init)(spv_t out, sp_t p, sp_t d, 
	  sp_t primroot, sp_t order, sp_t perm)
{
  uint32_t i;
  sp_t t1, t2, t3, t4, t5, t6, t7, t8;
  sp_t h1, h2, h3, h4, h5, h6, h7, h8;
  sp_t w1, w2, w3, w4, w5, w6;
  sp_t w[9];
  sp_t inv6 = sp_inv(6, p, d);
  sp_t inv2 = sp_inv(2, p, d);
    
  w[1] = sp_pow(primroot, order / 9, p, d);
  for (i = 2; i < 9; i++)
    w[i] = sp_mul(w[i-1], w[1], p, d);

  w1 = w[2];
  w2 = w[4];
  w3 = w[8];
  w4 = w[7];
  w5 = w[5];
  w6 = w[1];

  t3 = sp_add(w1, w1, p);
  t3 = sp_add(t3, w2, p);
  t3 = sp_sub(t3, w3, p);
  t3 = sp_sub(t3, w4, p);
  t3 = sp_sub(t3, w4, p);
  t3 = sp_sub(t3, w5, p);
  t3 = sp_add(t3, w6, p);

  t4 = sp_add(w1, w2, p);
  t4 = sp_add(t4, w2, p);
  t4 = sp_add(t4, w3, p);
  t4 = sp_sub(t4, w4, p);
  t4 = sp_sub(t4, w5, p);
  t4 = sp_sub(t4, w5, p);
  t4 = sp_sub(t4, w6, p);

  t5 = sp_add(w1, w1, p);
  t5 = sp_sub(t5, w2, p);
  t5 = sp_sub(t5, w3, p);
  t5 = sp_add(t5, w4, p);
  t5 = sp_add(t5, w4, p);
  t5 = sp_sub(t5, w5, p);
  t5 = sp_sub(t5, w6, p);

  t6 = sp_neg(w1, p);
  t6 = sp_add(t6, w2, p);
  t6 = sp_add(t6, w2, p);
  t6 = sp_sub(t6, w3, p);
  t6 = sp_sub(t6, w4, p);
  t6 = sp_add(t6, w5, p);
  t6 = sp_add(t6, w5, p);
  t6 = sp_sub(t6, w6, p);

  h1 = sp_add(w[6], w[3], p);
  h2 = sp_sub(w[6], w[3], p);

  h3 = sp_sub(t3, t4, p);
  h4 = sp_neg(t3, p); 
  h5 = t4;
  h6 = sp_sub(t5, t6, p);
  h7 = sp_neg(sp_add(t5, t6, p), p);
  h7 = sp_sub(h7, t6, p);
  h8 = t6;

  out[0] = 1;

  out[1] = sp_mul(h1, inv2, p, d);
  out[2] = sp_mul(h2, inv2, p, d);

  out[3] = sp_sub(out[1], 1, p);
  out[4] = out[2];

  out[5] = sp_mul(h3, inv6, p, d);
  out[6] = sp_mul(h4, inv6, p, d);
  out[7] = sp_mul(h5, inv6, p, d);
  out[8] = sp_mul(h6, inv6, p, d);
  out[9] = sp_mul(h7, inv6, p, d);
  out[10] = sp_mul(h8, inv6, p, d);
}

static void 
ntt9_run_core(spv_t in, spv_size_t istride,
		spv_t out, spv_size_t ostride,
		sp_t p, spv_t ntt_const)
{
  sp_t x0, x1, x2, x3, x4, x5, x6;
  sp_t     t1, t2, t3, t4, t5, t6, t7, t8;
  sp_t p0, p1, p2, p3, p4, p5, p6, p7, p8;

  sp_t x0e, x1e, t0e, t1e, p0e, p1e, p2e;

  x0 = in[0 * istride];
  x1 = in[1 * istride];
  x2 = in[2 * istride];
  x0e = in[3 * istride];
  x3 = in[4 * istride];
  x6 = in[5 * istride];
  x1e = in[6 * istride];
  x5 = in[7 * istride];
  x4 = in[8 * istride];

  t0e = sp_ntt_add(x0e, x1e, p);
  t1e = sp_ntt_sub_partial(x0e, x1e, p);

  p1 = sp_ntt_add(x1, x3, p);
  p1 = sp_ntt_add(p1, x5, p);
  p2 = sp_ntt_add(x2, x4, p);
  p2 = sp_ntt_add(p2, x6, p);
  p3 = sp_ntt_sub(x1, x5, p);
  p4 = sp_ntt_sub(x2, x6, p);
  p5 = sp_ntt_sub(x3, x5, p);
  p6 = sp_ntt_sub(x4, x6, p);

  t1 = sp_ntt_add(p1, p2, p);
  t2 = sp_ntt_sub_partial(p1, p2, p);
  t3 = sp_ntt_sub(p3, p5, p);
  t5 = sp_ntt_add(t3, p6, p);
  t3 = sp_ntt_sub(t3, p6, p);
  t4 = sp_ntt_add(p4, p5, p);
  t6 = sp_ntt_sub(p4, p5, p);

  p0e = sp_ntt_add(x0, t0e, p);
  p0 = t1;
  p1 = t1;
  p2 = t2;
  p3 = t3;
  p4 = t4;
  p5 = sp_ntt_add_partial(t3, t4, p);
  p6 = t5;
  p7 = t6;
  p8 = sp_ntt_add_partial(t5, t6, p);

  p1 = sp_ntt_mul(p1, ntt_const[1], ntt_const[NC+1], p);
  p2 = sp_ntt_mul(p2, ntt_const[2], ntt_const[NC+2], p);
  t0e = sp_ntt_mul(t0e, ntt_const[3], ntt_const[NC+3], p);
  t1e = sp_ntt_mul(t1e, ntt_const[4], ntt_const[NC+4], p);
  p3 = sp_ntt_mul(p3, ntt_const[5], ntt_const[NC+5], p);
  p4 = sp_ntt_mul(p4, ntt_const[6], ntt_const[NC+6], p);
  p5 = sp_ntt_mul(p5, ntt_const[7], ntt_const[NC+7], p);
  p6 = sp_ntt_mul(p6, ntt_const[8], ntt_const[NC+8], p);
  p7 = sp_ntt_mul(p7, ntt_const[9], ntt_const[NC+9], p);
  p8 = sp_ntt_mul(p8, ntt_const[10], ntt_const[NC+10], p);

  t0e = sp_ntt_add(t0e, p0e, p);
  t1 = sp_ntt_add(p1, p2, p);
  t2 = sp_ntt_sub(p1, p2, p);
  t3 = sp_ntt_add(p3, p5, p);
  t4 = sp_ntt_add(p4, p5, p);
  t5 = sp_ntt_add(p6, p8, p);
  t6 = sp_ntt_add(p7, p8, p);

  p1e = sp_ntt_add(t0e, t1e, p);
  p2e = sp_ntt_sub(t0e, t1e, p);
  p3 = sp_ntt_add(t3, t5, p);
  p4 = sp_ntt_add(t4, t6, p);
  p5 = sp_ntt_sub(t4, t6, p);
  p5 = sp_ntt_sub(p5, p3, p);
  p6 = sp_ntt_sub(t5, t3, p);

  p0 = sp_ntt_add(p0, p0e, p);
  t1 = sp_ntt_add(t1, p0e, p);
  t2 = sp_ntt_add(t2, p0e, p);
  t3 = sp_ntt_add(p3, p1e, p);
  t4 = sp_ntt_add(p4, p2e, p);
  t5 = sp_ntt_add(p5, p1e, p);
  t6 = sp_ntt_add(p6, p2e, p);
  t7 = sp_ntt_add(p3, p5, p);
  t7 = sp_ntt_sub(p1e, t7, p);
  t8 = sp_ntt_add(p4, p6, p);
  t8 = sp_ntt_sub(p2e, t8, p);

  out[0 * ostride] = p0;
  out[1 * ostride] = t8;
  out[2 * ostride] = t3;
  out[3 * ostride] = t2;
  out[4 * ostride] = t4;
  out[5 * ostride] = t7;
  out[6 * ostride] = t1;
  out[7 * ostride] = t6;
  out[8 * ostride] = t5;
}

#ifdef HAVE_SIMD
static void
ntt9_run_core_simd(spv_t in, spv_size_t istride, spv_size_t idist,
		spv_t out, spv_size_t ostride, spv_size_t odist,
		sp_t p, spv_t ntt_const, spv_size_t vsize)
{
  sp_simd_t x0, x1, x2, x3, x4, x5, x6;
  sp_simd_t     t1, t2, t3, t4, t5, t6, t7, t8;
  sp_simd_t p0, p1, p2, p3, p4, p5, p6, p7, p8;

  sp_simd_t x0e, x1e, t0e, t1e, p0e, p1e, p2e;

  x0 = sp_simd_gather(in + 0 * istride, idist, vsize);
  x1 = sp_simd_gather(in + 1 * istride, idist, vsize);
  x2 = sp_simd_gather(in + 2 * istride, idist, vsize);
  x0e = sp_simd_gather(in + 3 * istride, idist, vsize);
  x3 = sp_simd_gather(in + 4 * istride, idist, vsize);
  x6 = sp_simd_gather(in + 5 * istride, idist, vsize);
  x1e = sp_simd_gather(in + 6 * istride, idist, vsize);
  x5 = sp_simd_gather(in + 7 * istride, idist, vsize);
  x4 = sp_simd_gather(in + 8 * istride, idist, vsize);

  t0e = sp_ntt_add_simd(x0e, x1e, p);
  t1e = sp_ntt_sub_partial_simd(x0e, x1e, p);

  p1 = sp_ntt_add_simd(x1, x3, p);
  p1 = sp_ntt_add_simd(p1, x5, p);
  p2 = sp_ntt_add_simd(x2, x4, p);
  p2 = sp_ntt_add_simd(p2, x6, p);
  p3 = sp_ntt_sub_simd(x1, x5, p);
  p4 = sp_ntt_sub_simd(x2, x6, p);
  p5 = sp_ntt_sub_simd(x3, x5, p);
  p6 = sp_ntt_sub_simd(x4, x6, p);

  t1 = sp_ntt_add_simd(p1, p2, p);
  t2 = sp_ntt_sub_partial_simd(p1, p2, p);
  t3 = sp_ntt_sub_simd(p3, p5, p);
  t5 = sp_ntt_add_simd(t3, p6, p);
  t3 = sp_ntt_sub_simd(t3, p6, p);
  t4 = sp_ntt_add_simd(p4, p5, p);
  t6 = sp_ntt_sub_simd(p4, p5, p);

  p0e = sp_ntt_add_simd(x0, t0e, p);
  p0 = t1;
  p1 = t1;
  p2 = t2;
  p3 = t3;
  p4 = t4;
  p5 = sp_ntt_add_partial_simd(t3, t4, p);
  p6 = t5;
  p7 = t6;
  p8 = sp_ntt_add_partial_simd(t5, t6, p);

  p1 = sp_ntt_mul_simd(p1, ntt_const[1], ntt_const[NC+1], p);
  p2 = sp_ntt_mul_simd(p2, ntt_const[2], ntt_const[NC+2], p);
  t0e = sp_ntt_mul_simd(t0e, ntt_const[3], ntt_const[NC+3], p);
  t1e = sp_ntt_mul_simd(t1e, ntt_const[4], ntt_const[NC+4], p);
  p3 = sp_ntt_mul_simd(p3, ntt_const[5], ntt_const[NC+5], p);
  p4 = sp_ntt_mul_simd(p4, ntt_const[6], ntt_const[NC+6], p);
  p5 = sp_ntt_mul_simd(p5, ntt_const[7], ntt_const[NC+7], p);
  p6 = sp_ntt_mul_simd(p6, ntt_const[8], ntt_const[NC+8], p);
  p7 = sp_ntt_mul_simd(p7, ntt_const[9], ntt_const[NC+9], p);
  p8 = sp_ntt_mul_simd(p8, ntt_const[10], ntt_const[NC+10], p);

  t0e = sp_ntt_add_simd(t0e, p0e, p);
  t1 = sp_ntt_add_simd(p1, p2, p);
  t2 = sp_ntt_sub_simd(p1, p2, p);
  t3 = sp_ntt_add_simd(p3, p5, p);
  t4 = sp_ntt_add_simd(p4, p5, p);
  t5 = sp_ntt_add_simd(p6, p8, p);
  t6 = sp_ntt_add_simd(p7, p8, p);

  p1e = sp_ntt_add_simd(t0e, t1e, p);
  p2e = sp_ntt_sub_simd(t0e, t1e, p);
  p3 = sp_ntt_add_simd(t3, t5, p);
  p4 = sp_ntt_add_simd(t4, t6, p);
  p5 = sp_ntt_sub_simd(t4, t6, p);
  p5 = sp_ntt_sub_simd(p5, p3, p);
  p6 = sp_ntt_sub_simd(t5, t3, p);

  p0 = sp_ntt_add_simd(p0, p0e, p);
  t1 = sp_ntt_add_simd(t1, p0e, p);
  t2 = sp_ntt_add_simd(t2, p0e, p);
  t3 = sp_ntt_add_simd(p3, p1e, p);
  t4 = sp_ntt_add_simd(p4, p2e, p);
  t5 = sp_ntt_add_simd(p5, p1e, p);
  t6 = sp_ntt_add_simd(p6, p2e, p);
  t7 = sp_ntt_add_simd(p3, p5, p);
  t7 = sp_ntt_sub_simd(p1e, t7, p);
  t8 = sp_ntt_add_simd(p4, p6, p);
  t8 = sp_ntt_sub_simd(p2e, t8, p);

  sp_simd_scatter(p0, out + 0 * ostride, odist, vsize);
  sp_simd_scatter(t8, out + 1 * ostride, odist, vsize);
  sp_simd_scatter(t3, out + 2 * ostride, odist, vsize);
  sp_simd_scatter(t2, out + 3 * ostride, odist, vsize);
  sp_simd_scatter(t4, out + 4 * ostride, odist, vsize);
  sp_simd_scatter(t7, out + 5 * ostride, odist, vsize);
  sp_simd_scatter(t1, out + 6 * ostride, odist, vsize);
  sp_simd_scatter(t6, out + 7 * ostride, odist, vsize);
  sp_simd_scatter(t5, out + 8 * ostride, odist, vsize);
}
#endif

static void
ntt9_run(spv_t in, spv_size_t istride, spv_size_t idist,
    		spv_t out, spv_size_t ostride, spv_size_t odist,
    		spv_size_t num_transforms, sp_t p, spv_t ntt_const)
{
  spv_size_t i = 0;

  for (; i < num_transforms; i++)
    ntt9_run_core(in + i * idist, istride, 
                out + i * odist, ostride, p, ntt_const);
}


#ifdef HAVE_SIMD
static void
ntt9_run_simd(spv_t in, spv_size_t istride, spv_size_t idist,
    		spv_t out, spv_size_t ostride, spv_size_t odist,
    		spv_size_t num_transforms, sp_t p, spv_t ntt_const)
{
  spv_size_t i = 0;
  spv_size_t num_simd = SP_SIMD_VSIZE * (num_transforms / SP_SIMD_VSIZE);

  for (; i < num_simd; i += SP_SIMD_VSIZE)
    ntt9_run_core_simd(in + i * idist, istride, idist, 
                        out + i * odist, ostride, odist, p, 
			ntt_const, SP_SIMD_VSIZE);

  if (i < num_transforms)
    ntt9_run_core_simd(in + i * idist, istride, idist, 
                        out + i * odist, ostride, odist, p, 
			ntt_const, num_transforms - i);
}
#endif


static void
ntt9_twiddle_run_core(spv_t in, spv_size_t istride,
		spv_t out, spv_size_t ostride,
		spv_t w, sp_t p, spv_t ntt_const)
{
  sp_t x0, x1, x2, x3, x4, x5, x6;
  sp_t     t1, t2, t3, t4, t5, t6, t7, t8;
  sp_t p0, p1, p2, p3, p4, p5, p6, p7, p8;

  sp_t x0e, x1e, t0e, t1e, p0e, p1e, p2e;

  x0 = in[0 * istride];
  x1 = in[1 * istride];
  x2 = in[2 * istride];
  x0e = in[3 * istride];
  x3 = in[4 * istride];
  x6 = in[5 * istride];
  x1e = in[6 * istride];
  x5 = in[7 * istride];
  x4 = in[8 * istride];

  t0e = sp_ntt_add(x0e, x1e, p);
  t1e = sp_ntt_sub_partial(x0e, x1e, p);

  p1 = sp_ntt_add(x1, x3, p);
  p1 = sp_ntt_add(p1, x5, p);
  p2 = sp_ntt_add(x2, x4, p);
  p2 = sp_ntt_add(p2, x6, p);
  p3 = sp_ntt_sub(x1, x5, p);
  p4 = sp_ntt_sub(x2, x6, p);
  p5 = sp_ntt_sub(x3, x5, p);
  p6 = sp_ntt_sub(x4, x6, p);

  t1 = sp_ntt_add(p1, p2, p);
  t2 = sp_ntt_sub_partial(p1, p2, p);
  t3 = sp_ntt_sub(p3, p5, p);
  t5 = sp_ntt_add(t3, p6, p);
  t3 = sp_ntt_sub(t3, p6, p);
  t4 = sp_ntt_add(p4, p5, p);
  t6 = sp_ntt_sub(p4, p5, p);

  p0e = sp_ntt_add(x0, t0e, p);
  p0 = t1;
  p1 = t1;
  p2 = t2;
  p3 = t3;
  p4 = t4;
  p5 = sp_ntt_add_partial(t3, t4, p);
  p6 = t5;
  p7 = t6;
  p8 = sp_ntt_add_partial(t5, t6, p);

  p1 = sp_ntt_mul(p1, ntt_const[1], ntt_const[NC+1], p);
  p2 = sp_ntt_mul(p2, ntt_const[2], ntt_const[NC+2], p);
  t0e = sp_ntt_mul(t0e, ntt_const[3], ntt_const[NC+3], p);
  t1e = sp_ntt_mul(t1e, ntt_const[4], ntt_const[NC+4], p);
  p3 = sp_ntt_mul(p3, ntt_const[5], ntt_const[NC+5], p);
  p4 = sp_ntt_mul(p4, ntt_const[6], ntt_const[NC+6], p);
  p5 = sp_ntt_mul(p5, ntt_const[7], ntt_const[NC+7], p);
  p6 = sp_ntt_mul(p6, ntt_const[8], ntt_const[NC+8], p);
  p7 = sp_ntt_mul(p7, ntt_const[9], ntt_const[NC+9], p);
  p8 = sp_ntt_mul(p8, ntt_const[10], ntt_const[NC+10], p);

  t0e = sp_ntt_add(t0e, p0e, p);
  t1 = sp_ntt_add(p1, p2, p);
  t2 = sp_ntt_sub(p1, p2, p);
  t3 = sp_ntt_add(p3, p5, p);
  t4 = sp_ntt_add(p4, p5, p);
  t5 = sp_ntt_add(p6, p8, p);
  t6 = sp_ntt_add(p7, p8, p);

  p1e = sp_ntt_add(t0e, t1e, p);
  p2e = sp_ntt_sub(t0e, t1e, p);
  p3 = sp_ntt_add(t3, t5, p);
  p4 = sp_ntt_add(t4, t6, p);
  p5 = sp_ntt_sub(t4, t6, p);
  p5 = sp_ntt_sub(p5, p3, p);
  p6 = sp_ntt_sub(t5, t3, p);

  p0 = sp_ntt_add(p0, p0e, p);
  t1 = sp_ntt_add_partial(t1, p0e, p);
  t2 = sp_ntt_add_partial(t2, p0e, p);
  t3 = sp_ntt_add_partial(p3, p1e, p);
  t4 = sp_ntt_add_partial(p4, p2e, p);
  t5 = sp_ntt_add_partial(p5, p1e, p);
  t6 = sp_ntt_add_partial(p6, p2e, p);
  t7 = sp_ntt_add(p3, p5, p);
  t7 = sp_ntt_sub_partial(p1e, t7, p);
  t8 = sp_ntt_add(p4, p6, p);
  t8 = sp_ntt_sub_partial(p2e, t8, p);

  t8 = sp_ntt_mul(t8, w[0], w[1], p);
  t3 = sp_ntt_mul(t3, w[2], w[3], p);
  t2 = sp_ntt_mul(t2, w[4], w[5], p);
  t4 = sp_ntt_mul(t4, w[6], w[7], p);
  t7 = sp_ntt_mul(t7, w[8], w[9], p);
  t1 = sp_ntt_mul(t1, w[10], w[11], p);
  t6 = sp_ntt_mul(t6, w[12], w[13], p);
  t5 = sp_ntt_mul(t5, w[14], w[15], p);

  out[0 * ostride] = p0;
  out[1 * ostride] = t8;
  out[2 * ostride] = t3;
  out[3 * ostride] = t2;
  out[4 * ostride] = t4;
  out[5 * ostride] = t7;
  out[6 * ostride] = t1;
  out[7 * ostride] = t6;
  out[8 * ostride] = t5;
}

#ifdef HAVE_SIMD
static void
ntt9_twiddle_run_core_simd(
        spv_t in, spv_size_t istride, spv_size_t idist,
		spv_t out, spv_size_t ostride, spv_size_t odist,
		sp_simd_t *w, sp_t p, spv_t ntt_const, spv_size_t vsize)
{
  sp_simd_t x0, x1, x2, x3, x4, x5, x6;
  sp_simd_t     t1, t2, t3, t4, t5, t6, t7, t8;
  sp_simd_t p0, p1, p2, p3, p4, p5, p6, p7, p8;

  sp_simd_t x0e, x1e, t0e, t1e, p0e, p1e, p2e;

  x0 = sp_simd_gather(in + 0 * istride, idist, vsize);
  x1 = sp_simd_gather(in + 1 * istride, idist, vsize);
  x2 = sp_simd_gather(in + 2 * istride, idist, vsize);
  x0e = sp_simd_gather(in + 3 * istride, idist, vsize);
  x3 = sp_simd_gather(in + 4 * istride, idist, vsize);
  x6 = sp_simd_gather(in + 5 * istride, idist, vsize);
  x1e = sp_simd_gather(in + 6 * istride, idist, vsize);
  x5 = sp_simd_gather(in + 7 * istride, idist, vsize);
  x4 = sp_simd_gather(in + 8 * istride, idist, vsize);

  t0e = sp_ntt_add_simd(x0e, x1e, p);
  t1e = sp_ntt_sub_partial_simd(x0e, x1e, p);

  p1 = sp_ntt_add_simd(x1, x3, p);
  p1 = sp_ntt_add_simd(p1, x5, p);
  p2 = sp_ntt_add_simd(x2, x4, p);
  p2 = sp_ntt_add_simd(p2, x6, p);
  p3 = sp_ntt_sub_simd(x1, x5, p);
  p4 = sp_ntt_sub_simd(x2, x6, p);
  p5 = sp_ntt_sub_simd(x3, x5, p);
  p6 = sp_ntt_sub_simd(x4, x6, p);

  t1 = sp_ntt_add_simd(p1, p2, p);
  t2 = sp_ntt_sub_partial_simd(p1, p2, p);
  t3 = sp_ntt_sub_simd(p3, p5, p);
  t5 = sp_ntt_add_simd(t3, p6, p);
  t3 = sp_ntt_sub_simd(t3, p6, p);
  t4 = sp_ntt_add_simd(p4, p5, p);
  t6 = sp_ntt_sub_simd(p4, p5, p);

  p0e = sp_ntt_add_simd(x0, t0e, p);
  p0 = t1;
  p1 = t1;
  p2 = t2;
  p3 = t3;
  p4 = t4;
  p5 = sp_ntt_add_partial_simd(t3, t4, p);
  p6 = t5;
  p7 = t6;
  p8 = sp_ntt_add_partial_simd(t5, t6, p);

  p1 = sp_ntt_mul_simd(p1, ntt_const[1], ntt_const[NC+1], p);
  p2 = sp_ntt_mul_simd(p2, ntt_const[2], ntt_const[NC+2], p);
  t0e = sp_ntt_mul_simd(t0e, ntt_const[3], ntt_const[NC+3], p);
  t1e = sp_ntt_mul_simd(t1e, ntt_const[4], ntt_const[NC+4], p);
  p3 = sp_ntt_mul_simd(p3, ntt_const[5], ntt_const[NC+5], p);
  p4 = sp_ntt_mul_simd(p4, ntt_const[6], ntt_const[NC+6], p);
  p5 = sp_ntt_mul_simd(p5, ntt_const[7], ntt_const[NC+7], p);
  p6 = sp_ntt_mul_simd(p6, ntt_const[8], ntt_const[NC+8], p);
  p7 = sp_ntt_mul_simd(p7, ntt_const[9], ntt_const[NC+9], p);
  p8 = sp_ntt_mul_simd(p8, ntt_const[10], ntt_const[NC+10], p);

  t0e = sp_ntt_add_simd(t0e, p0e, p);
  t1 = sp_ntt_add_simd(p1, p2, p);
  t2 = sp_ntt_sub_simd(p1, p2, p);
  t3 = sp_ntt_add_simd(p3, p5, p);
  t4 = sp_ntt_add_simd(p4, p5, p);
  t5 = sp_ntt_add_simd(p6, p8, p);
  t6 = sp_ntt_add_simd(p7, p8, p);

  p1e = sp_ntt_add_simd(t0e, t1e, p);
  p2e = sp_ntt_sub_simd(t0e, t1e, p);
  p3 = sp_ntt_add_simd(t3, t5, p);
  p4 = sp_ntt_add_simd(t4, t6, p);
  p5 = sp_ntt_sub_simd(t4, t6, p);
  p5 = sp_ntt_sub_simd(p5, p3, p);
  p6 = sp_ntt_sub_simd(t5, t3, p);

  p0 = sp_ntt_add_simd(p0, p0e, p);
  t1 = sp_ntt_add_partial_simd(t1, p0e, p);
  t2 = sp_ntt_add_partial_simd(t2, p0e, p);
  t3 = sp_ntt_add_partial_simd(p3, p1e, p);
  t4 = sp_ntt_add_partial_simd(p4, p2e, p);
  t5 = sp_ntt_add_partial_simd(p5, p1e, p);
  t6 = sp_ntt_add_partial_simd(p6, p2e, p);
  t7 = sp_ntt_add_simd(p3, p5, p);
  t7 = sp_ntt_sub_partial_simd(p1e, t7, p);
  t8 = sp_ntt_add_simd(p4, p6, p);
  t8 = sp_ntt_sub_partial_simd(p2e, t8, p);

  t8 = sp_ntt_twiddle_mul_simd(t8, w + 0, p);
  t3 = sp_ntt_twiddle_mul_simd(t3, w + 2, p);
  t2 = sp_ntt_twiddle_mul_simd(t2, w + 4, p);
  t4 = sp_ntt_twiddle_mul_simd(t4, w + 6, p);
  t7 = sp_ntt_twiddle_mul_simd(t7, w + 8, p);
  t1 = sp_ntt_twiddle_mul_simd(t1, w + 10, p);
  t6 = sp_ntt_twiddle_mul_simd(t6, w + 12, p);
  t5 = sp_ntt_twiddle_mul_simd(t5, w + 14, p);

  sp_simd_scatter(p0, out + 0 * ostride, odist, vsize);
  sp_simd_scatter(t8, out + 1 * ostride, odist, vsize);
  sp_simd_scatter(t3, out + 2 * ostride, odist, vsize);
  sp_simd_scatter(t2, out + 3 * ostride, odist, vsize);
  sp_simd_scatter(t4, out + 4 * ostride, odist, vsize);
  sp_simd_scatter(t7, out + 5 * ostride, odist, vsize);
  sp_simd_scatter(t1, out + 6 * ostride, odist, vsize);
  sp_simd_scatter(t6, out + 7 * ostride, odist, vsize);
  sp_simd_scatter(t5, out + 8 * ostride, odist, vsize);
}
#endif

static void
ntt9_twiddle_run(spv_t in, spv_size_t istride, spv_size_t idist,
    			spv_t out, spv_size_t ostride, spv_size_t odist,
    			spv_t w, spv_size_t num_transforms, sp_t p, spv_t ntt_const)
{
  spv_size_t i = 0, j = 0;

  for (; i < num_transforms; i++, j += 2*(9-1))
    ntt9_twiddle_run_core(in + i * idist, istride, 
			out + i * odist, ostride,
			w + j, p, ntt_const);
}


#ifdef HAVE_SIMD
static void
ntt9_twiddle_run_simd(spv_t in, spv_size_t istride, spv_size_t idist,
    			spv_t out, spv_size_t ostride, spv_size_t odist,
    			spv_t w, spv_size_t num_transforms, sp_t p, spv_t ntt_const)
{
  spv_size_t i = 0, j = 0;
  spv_size_t num_simd = SP_SIMD_VSIZE * (num_transforms / SP_SIMD_VSIZE);

  for (; i < num_simd; i += SP_SIMD_VSIZE,
		  	j += 2*(9-1)*SP_SIMD_VSIZE)
    ntt9_twiddle_run_core_simd(
		in + i * idist, istride, idist,
		out + i * odist, ostride, odist,
		(sp_simd_t *)(w + j), p, 
		ntt_const, SP_SIMD_VSIZE);

  if (i < num_transforms)
    ntt9_twiddle_run_core_simd(
		in + i * idist, istride, idist,
		out + i * odist, ostride, odist,
		(sp_simd_t *)(w + j), p, 
		ntt_const, num_transforms - i);
}
#endif

static void
ntt9_pfa_run_core(spv_t x, spv_size_t start,
	  spv_size_t inc, spv_size_t n,
	  sp_t p, spv_t ntt_const)
{
  spv_size_t j0, j1, j2, j3, j4, j5, j6, j7, j8;
  sp_t x0, x1, x2, x3, x4, x5, x6;
  sp_t     t1, t2, t3, t4, t5, t6, t7, t8;
  sp_t p0, p1, p2, p3, p4, p5, p6, p7, p8;

  sp_t x0e, x1e, t0e, t1e, p0e, p1e, p2e;

  j0 = start;
  j1 = sp_array_inc(j0, inc, n);
  j2 = sp_array_inc(j0, 2 * inc, n);
  j3 = sp_array_inc(j0, 3 * inc, n);
  j4 = sp_array_inc(j0, 4 * inc, n);
  j5 = sp_array_inc(j0, 5 * inc, n);
  j6 = sp_array_inc(j0, 6 * inc, n);
  j7 = sp_array_inc(j0, 7 * inc, n);
  j8 = sp_array_inc(j0, 8 * inc, n);

  x0 = x[j0];
  x1 = x[j1];
  x2 = x[j2];
  x0e = x[j3];
  x3 = x[j4];
  x6 = x[j5];
  x1e = x[j6];
  x5 = x[j7];
  x4 = x[j8];

  t0e = sp_ntt_add(x0e, x1e, p);
  t1e = sp_ntt_sub_partial(x0e, x1e, p);

  p1 = sp_ntt_add(x1, x3, p);
  p1 = sp_ntt_add(p1, x5, p);
  p2 = sp_ntt_add(x2, x4, p);
  p2 = sp_ntt_add(p2, x6, p);
  p3 = sp_ntt_sub(x1, x5, p);
  p4 = sp_ntt_sub(x2, x6, p);
  p5 = sp_ntt_sub(x3, x5, p);
  p6 = sp_ntt_sub(x4, x6, p);

  t1 = sp_ntt_add(p1, p2, p);
  t2 = sp_ntt_sub_partial(p1, p2, p);
  t3 = sp_ntt_sub(p3, p5, p);
  t5 = sp_ntt_add(t3, p6, p);
  t3 = sp_ntt_sub(t3, p6, p);
  t4 = sp_ntt_add(p4, p5, p);
  t6 = sp_ntt_sub(p4, p5, p);

  p0e = sp_ntt_add(x0, t0e, p);
  p0 = t1;
  p1 = t1;
  p2 = t2;
  p3 = t3;
  p4 = t4;
  p5 = sp_ntt_add_partial(t3, t4, p);
  p6 = t5;
  p7 = t6;
  p8 = sp_ntt_add_partial(t5, t6, p);

  p1 = sp_ntt_mul(p1, ntt_const[1], ntt_const[NC+1], p);
  p2 = sp_ntt_mul(p2, ntt_const[2], ntt_const[NC+2], p);
  t0e = sp_ntt_mul(t0e, ntt_const[3], ntt_const[NC+3], p);
  t1e = sp_ntt_mul(t1e, ntt_const[4], ntt_const[NC+4], p);
  p3 = sp_ntt_mul(p3, ntt_const[5], ntt_const[NC+5], p);
  p4 = sp_ntt_mul(p4, ntt_const[6], ntt_const[NC+6], p);
  p5 = sp_ntt_mul(p5, ntt_const[7], ntt_const[NC+7], p);
  p6 = sp_ntt_mul(p6, ntt_const[8], ntt_const[NC+8], p);
  p7 = sp_ntt_mul(p7, ntt_const[9], ntt_const[NC+9], p);
  p8 = sp_ntt_mul(p8, ntt_const[10], ntt_const[NC+10], p);

  t0e = sp_ntt_add(t0e, p0e, p);
  t1 = sp_ntt_add(p1, p2, p);
  t2 = sp_ntt_sub(p1, p2, p);
  t3 = sp_ntt_add(p3, p5, p);
  t4 = sp_ntt_add(p4, p5, p);
  t5 = sp_ntt_add(p6, p8, p);
  t6 = sp_ntt_add(p7, p8, p);

  p1e = sp_ntt_add(t0e, t1e, p);
  p2e = sp_ntt_sub(t0e, t1e, p);
  p3 = sp_ntt_add(t3, t5, p);
  p4 = sp_ntt_add(t4, t6, p);
  p5 = sp_ntt_sub(t4, t6, p);
  p5 = sp_ntt_sub(p5, p3, p);
  p6 = sp_ntt_sub(t5, t3, p);

  p0 = sp_ntt_add(p0, p0e, p);
  t1 = sp_ntt_add(t1, p0e, p);
  t2 = sp_ntt_add(t2, p0e, p);
  t3 = sp_ntt_add(p3, p1e, p);
  t4 = sp_ntt_add(p4, p2e, p);
  t5 = sp_ntt_add(p5, p1e, p);
  t6 = sp_ntt_add(p6, p2e, p);
  t7 = sp_ntt_add(p3, p5, p);
  t7 = sp_ntt_sub(p1e, t7, p);
  t8 = sp_ntt_add(p4, p6, p);
  t8 = sp_ntt_sub(p2e, t8, p);

  x[j0] = p0;
  x[j1] = t8;
  x[j2] = t3;
  x[j3] = t2;
  x[j4] = t4;
  x[j5] = t7;
  x[j6] = t1;
  x[j7] = t6;
  x[j8] = t5;
}

#ifdef HAVE_SIMD
static void
ntt9_pfa_run_core_simd(spv_t x, spv_size_t start,
	  spv_size_t inc, spv_size_t inc2, spv_size_t n,
	  sp_t p, spv_t ntt_const, spv_size_t vsize)
{
  spv_size_t j0, j1, j2, j3, j4, j5, j6, j7, j8;
  sp_simd_t x0, x1, x2, x3, x4, x5, x6;
  sp_simd_t     t1, t2, t3, t4, t5, t6, t7, t8;
  sp_simd_t p0, p1, p2, p3, p4, p5, p6, p7, p8;

  sp_simd_t x0e, x1e, t0e, t1e, p0e, p1e, p2e;

  j0 = start;
  j1 = sp_array_inc(j0, inc, n);
  j2 = sp_array_inc(j0, 2 * inc, n);
  j3 = sp_array_inc(j0, 3 * inc, n);
  j4 = sp_array_inc(j0, 4 * inc, n);
  j5 = sp_array_inc(j0, 5 * inc, n);
  j6 = sp_array_inc(j0, 6 * inc, n);
  j7 = sp_array_inc(j0, 7 * inc, n);
  j8 = sp_array_inc(j0, 8 * inc, n);

  x0 = sp_simd_pfa_gather(x, j0, inc2, n, vsize);
  x1 = sp_simd_pfa_gather(x, j1, inc2, n, vsize);
  x2 = sp_simd_pfa_gather(x, j2, inc2, n, vsize);
  x0e = sp_simd_pfa_gather(x, j3, inc2, n, vsize);
  x3 = sp_simd_pfa_gather(x, j4, inc2, n, vsize);
  x6 = sp_simd_pfa_gather(x, j5, inc2, n, vsize);
  x1e = sp_simd_pfa_gather(x, j6, inc2, n, vsize);
  x5 = sp_simd_pfa_gather(x, j7, inc2, n, vsize);
  x4 = sp_simd_pfa_gather(x, j8, inc2, n, vsize);

  t0e = sp_ntt_add_simd(x0e, x1e, p);
  t1e = sp_ntt_sub_partial_simd(x0e, x1e, p);

  p1 = sp_ntt_add_simd(x1, x3, p);
  p1 = sp_ntt_add_simd(p1, x5, p);
  p2 = sp_ntt_add_simd(x2, x4, p);
  p2 = sp_ntt_add_simd(p2, x6, p);
  p3 = sp_ntt_sub_simd(x1, x5, p);
  p4 = sp_ntt_sub_simd(x2, x6, p);
  p5 = sp_ntt_sub_simd(x3, x5, p);
  p6 = sp_ntt_sub_simd(x4, x6, p);

  t1 = sp_ntt_add_simd(p1, p2, p);
  t2 = sp_ntt_sub_partial_simd(p1, p2, p);
  t3 = sp_ntt_sub_simd(p3, p5, p);
  t5 = sp_ntt_add_simd(t3, p6, p);
  t3 = sp_ntt_sub_simd(t3, p6, p);
  t4 = sp_ntt_add_simd(p4, p5, p);
  t6 = sp_ntt_sub_simd(p4, p5, p);

  p0e = sp_ntt_add_simd(x0, t0e, p);
  p0 = t1;
  p1 = t1;
  p2 = t2;
  p3 = t3;
  p4 = t4;
  p5 = sp_ntt_add_partial_simd(t3, t4, p);
  p6 = t5;
  p7 = t6;
  p8 = sp_ntt_add_partial_simd(t5, t6, p);

  p1 = sp_ntt_mul_simd(p1, ntt_const[1], ntt_const[NC+1], p);
  p2 = sp_ntt_mul_simd(p2, ntt_const[2], ntt_const[NC+2], p);
  t0e = sp_ntt_mul_simd(t0e, ntt_const[3], ntt_const[NC+3], p);
  t1e = sp_ntt_mul_simd(t1e, ntt_const[4], ntt_const[NC+4], p);
  p3 = sp_ntt_mul_simd(p3, ntt_const[5], ntt_const[NC+5], p);
  p4 = sp_ntt_mul_simd(p4, ntt_const[6], ntt_const[NC+6], p);
  p5 = sp_ntt_mul_simd(p5, ntt_const[7], ntt_const[NC+7], p);
  p6 = sp_ntt_mul_simd(p6, ntt_const[8], ntt_const[NC+8], p);
  p7 = sp_ntt_mul_simd(p7, ntt_const[9], ntt_const[NC+9], p);
  p8 = sp_ntt_mul_simd(p8, ntt_const[10], ntt_const[NC+10], p);

  t0e = sp_ntt_add_simd(t0e, p0e, p);
  t1 = sp_ntt_add_simd(p1, p2, p);
  t2 = sp_ntt_sub_simd(p1, p2, p);
  t3 = sp_ntt_add_simd(p3, p5, p);
  t4 = sp_ntt_add_simd(p4, p5, p);
  t5 = sp_ntt_add_simd(p6, p8, p);
  t6 = sp_ntt_add_simd(p7, p8, p);

  p1e = sp_ntt_add_simd(t0e, t1e, p);
  p2e = sp_ntt_sub_simd(t0e, t1e, p);
  p3 = sp_ntt_add_simd(t3, t5, p);
  p4 = sp_ntt_add_simd(t4, t6, p);
  p5 = sp_ntt_sub_simd(t4, t6, p);
  p5 = sp_ntt_sub_simd(p5, p3, p);
  p6 = sp_ntt_sub_simd(t5, t3, p);

  p0 = sp_ntt_add_simd(p0, p0e, p);
  t1 = sp_ntt_add_simd(t1, p0e, p);
  t2 = sp_ntt_add_simd(t2, p0e, p);
  t3 = sp_ntt_add_simd(p3, p1e, p);
  t4 = sp_ntt_add_simd(p4, p2e, p);
  t5 = sp_ntt_add_simd(p5, p1e, p);
  t6 = sp_ntt_add_simd(p6, p2e, p);
  t7 = sp_ntt_add_simd(p3, p5, p);
  t7 = sp_ntt_sub_simd(p1e, t7, p);
  t8 = sp_ntt_add_simd(p4, p6, p);
  t8 = sp_ntt_sub_simd(p2e, t8, p);

  sp_simd_pfa_scatter(p0, x, j0, inc2, n, vsize);
  sp_simd_pfa_scatter(t8, x, j1, inc2, n, vsize);
  sp_simd_pfa_scatter(t3, x, j2, inc2, n, vsize);
  sp_simd_pfa_scatter(t2, x, j3, inc2, n, vsize);
  sp_simd_pfa_scatter(t4, x, j4, inc2, n, vsize);
  sp_simd_pfa_scatter(t7, x, j5, inc2, n, vsize);
  sp_simd_pfa_scatter(t1, x, j6, inc2, n, vsize);
  sp_simd_pfa_scatter(t6, x, j7, inc2, n, vsize);
  sp_simd_pfa_scatter(t5, x, j8, inc2, n, vsize);
}
#endif

static void
ntt9_pfa_run(spv_t x, spv_size_t cofactor,
	  sp_t p, spv_t ntt_const)
{
  spv_size_t i = 0;
  spv_size_t incstart = 0;
  spv_size_t n = 9 * cofactor;
  spv_size_t inc = cofactor;
  spv_size_t inc2 = 9;

  for (; i < cofactor; i++, incstart += inc2)
    ntt9_pfa_run_core(x, incstart, inc, n, p, ntt_const);

}

#ifdef HAVE_SIMD
static void
ntt9_pfa_run_simd(spv_t x, spv_size_t cofactor,
	  sp_t p, spv_t ntt_const)
{
  spv_size_t i = 0;
  spv_size_t incstart = 0;
  spv_size_t n = 9 * cofactor;
  spv_size_t inc = cofactor;
  spv_size_t inc2 = 9;
  spv_size_t num_simd = SP_SIMD_VSIZE * (cofactor / SP_SIMD_VSIZE);

  for (i = 0; i < num_simd; i += SP_SIMD_VSIZE)
    {
      ntt9_pfa_run_core_simd(x, incstart, inc, inc2, n, p, 
	  			ntt_const, SP_SIMD_VSIZE);
      incstart += SP_SIMD_VSIZE * inc2;
    }

  if (i < cofactor)
    ntt9_pfa_run_core_simd(x, incstart, inc, inc2, n, p, 
	  			ntt_const, cofactor - i);
}
#endif

const nttconfig_t X(ntt9_config) = 
{
  9,
  NC,
  ntt9_get_fixed_ntt_const,
  X(ntt9_init),
#ifdef HAVE_SIMD
  ntt9_run_simd,
  ntt9_pfa_run_simd,
  ntt9_twiddle_run_simd,
#endif
  ntt9_run,
  ntt9_pfa_run,
  ntt9_twiddle_run
};

