/* mpzspv.c - "mpz small prime polynomial" functions for arithmetic on mpzv's
   reduced modulo a mpzspm

Copyright 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Dave Newman,
Jason Papadopoulos, Alexander Kruppa, Paul Zimmermann.

The SP Library is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 3 of the License, or (at your
option) any later version.

The SP Library is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
License for more details.

You should have received a copy of the GNU Lesser General Public License
along with the SP Library; see the file COPYING.LIB.  If not, write to
the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
MA 02110-1301, USA. */

#define _GNU_SOURCE
#include "config.h"
#include <stdio.h> /* for stderr */
#include <stdlib.h>
#include <errno.h>
#include <string.h> /* for memset */
#ifdef USE_VALGRIND
#include <valgrind/memcheck.h>
#include "ecm-gmp.h"
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_AIO_H
#include <aio.h>
#endif
#include "ecm-impl.h"

#define TRACE_ntt_sqr_reciprocal 0
#define TRACE_ntt_mul 0
#define WANT_PROFILE 0

#define IN_MEMORY(x) ((x) != NULL && (x)->storage == 0)
#define ON_DISK(x) ((x) != NULL && (x)->storage != 0)

static void  mpzspv_seek_and_read (mpzspv_t, spv_size_t, FILE **, size_t, 
    size_t, mpzspm_t);
static void mpzspv_seek_and_write (mpzspv_t, spv_size_t, FILE **, size_t, 
    size_t, mpzspm_t);
static int mpzspv_open_fileset (mpzspv_handle_t, const char *, spv_size_t);
static void mpzspv_close_fileset (mpzspv_handle_t);
static void spv_add_or_mul_file (spv_t, const spv_t, FILE *, const spv_size_t, 
    const spv_size_t, const spv_size_t, const spv_size_t, const int, 
    const spm_t);
#ifdef HAVE_AIO_READ
static int mpzspv_lio_rw (struct aiocb *[], mpzspv_t, spv_size_t, FILE **,  
                     spv_size_t, spv_size_t, const mpzspm_t, int);
static int mpzspv_lio_suspend (const struct aiocb *[], const mpzspm_t);
#endif


static inline void
valgrind_check_mpzinp(ATTRIBUTE_UNUSED const mpz_t m)
{
#ifdef USE_VALGRIND
  VALGRIND_CHECK_MEM_IS_DEFINED(m, sizeof(mpz_t));
  VALGRIND_CHECK_MEM_IS_ADDRESSABLE(PTR(m), ALLOC(m) * sizeof(mp_limb_t));
  VALGRIND_CHECK_MEM_IS_DEFINED(PTR(m), ABSIZ(m) * sizeof(mp_limb_t));
#endif
}

static mpzspv_t
mpzspv_init (spv_size_t len, const mpzspm_t mpzspm)
{
  unsigned int i;
  mpzspv_t x = (mpzspv_t) malloc (mpzspm->sp_num * sizeof (spv_t));
  
  if (x == NULL)
    return NULL;
  
  for (i = 0; i < mpzspm->sp_num; i++)
    {
      x[i] = (spv_t) sp_aligned_malloc (len * sizeof (sp_t));
      
      if (x[i] == NULL)
	{
	  while (i--)
	    sp_aligned_free (x[i]);
	  
	  free (x);
	  return NULL;
	}
    }
  
  return x;
}

static void
mpzspv_clear (mpzspv_t x, const mpzspm_t mpzspm)
{
  unsigned int i;
  
  for (i = 0; i < mpzspm->sp_num; i++)
    sp_aligned_free (x[i]);
  
  free (x);
}


/* If filename == NULL, allocates memory for storage.
   If filename != NULL, opens a file set */

mpzspv_handle_t
mpzspv_init_handle (const char *filename, const spv_size_t len, 
                    const mpzspm_t mpzspm)
{
  mpzspv_handle_t handle;
  
  handle = (mpzspv_handle_t) malloc (sizeof (_mpzspv_handle_t));
  if (handle == NULL)
    return NULL;
  
  handle->mpzspm = mpzspm;

  if (filename == NULL)
    {
      handle->storage = 0;
      handle->mem = mpzspv_init (len, mpzspm);
      handle->files = NULL;
    }
  else
    {
      handle->storage = 1;
      handle->mem = NULL;
      mpzspv_open_fileset (handle, filename, len);
    }

  if (handle->mem == NULL && handle->files == NULL)
    {
      free (handle);
      handle = NULL;
    }

  return handle;
}


void
mpzspv_clear_handle (mpzspv_handle_t handle)
{
  if (handle == NULL)
    return;
  
  if (IN_MEMORY(handle))
    {
      mpzspv_clear (handle->mem, handle->mpzspm);
      handle->mem = NULL;
      handle->mpzspm = NULL;
    }
  else 
    {
      mpzspv_close_fileset (handle);
      handle->files = NULL;
      handle->mpzspm = NULL;
    }

  free (handle);
}


/* check that:
 *  - each of the spv's is at least offset + len long
 *  - the data specified by (offset, len) is correctly normalised in the
 *    range [0, sp)
 *
 * return 1 for success, 0 for failure */

int
mpzspv_verify_in (const mpzspv_handle_t x, const spv_size_t offset, 
                  const spv_size_t len)
{
  unsigned int i;
  
  if (x->storage == 0)
    {
      for (i = 0; i < x->mpzspm->sp_num; i++)
        {
          if (spv_verify_in (x->mem[i] + offset, len, x->mpzspm->spm[i]->sp) == 0)
            return 0;
        }
    }

  return 1;
}


/* check that:
 *  - each of the spv's is at least offset + len long
 *
 * return 1 for success, 0 for failure */

int
mpzspv_verify_out (const mpzspv_handle_t x, const spv_size_t offset, 
                   const spv_size_t len)
{
  unsigned int i;
  
  if (x->storage == 0)
    {
      for (i = 0; i < x->mpzspm->sp_num; i++)
        {
          if (spv_verify_out (x->mem[i] + offset, len, x->mpzspm->spm[i]->sp) == 0)
            return 0;
        }
    }

  return 1;
}

void
mpzspv_set (mpzspv_handle_t r, const spv_size_t r_offset, 
    const mpzspv_handle_t x, const spv_size_t x_offset, const spv_size_t len)
{
  
  ASSERT_ALWAYS (r->mpzspm == x->mpzspm);

  if (r->storage == 0 && x->storage == 0)
    {
      unsigned int i;
      ASSERT (mpzspv_verify_out (r, r_offset, len));
      ASSERT (mpzspv_verify_in (x, x_offset, len));
      
      for (i = 0; i < r->mpzspm->sp_num; i++)
        spv_set (r->mem[i] + r_offset, x->mem[i] + x_offset, len);
    }
  else if (r->storage == 0 && x->storage == 1)
    {
      ASSERT (mpzspv_verify_out (r, r_offset, len));
      mpzspv_seek_and_read (r->mem, r_offset, x->files, x_offset, len, x->mpzspm);
    }
  else if (r->storage == 1 && x->storage == 0)
    {
      ASSERT (mpzspv_verify_in (x, x_offset, len));
      mpzspv_seek_and_write (x->mem, x_offset, r->files, r_offset, len, x->mpzspm);
    }
  else
    {
      /* File to file transfer not implemented - probably not needed */
      abort();
    }  
}

void
mpzspv_reverse (mpzspv_handle_t r, const spv_size_t r_offset, 
    const mpzspv_handle_t x, const spv_size_t x_offset, const spv_size_t len)
{
  unsigned int i;
  
  if (r->storage == 1 || x->storage == 1)
    {
      /* Not implemented yet */
      abort();
    }
  
  ASSERT (mpzspv_verify_out (r, r_offset, len));
  ASSERT (mpzspv_verify_in (x, x_offset, len));
  ASSERT_ALWAYS (r->mpzspm == x->mpzspm);
  
  for (i = 0; i < x->mpzspm->sp_num; i++)
    spv_rev (r->mem[i] + r_offset, x->mem[i] + x_offset, len);
}

void
mpzspv_set_sp (mpzspv_handle_t r, const spv_size_t offset, 
    const sp_t c, const spv_size_t len)
{
  unsigned int i;
  
  if (r->storage == 1)
    {
      /* Not implemented yet */
      abort();
    }
  else
    {
      ASSERT (mpzspv_verify_out (r, offset, len));
      ASSERT (c < SP_MIN); /* not strictly necessary but avoids mod functions */
      
      for (i = 0; i < r->mpzspm->sp_num; i++)
        spv_set_sp (r->mem[i] + offset, c, len);
    }
}

void
mpzspv_neg (mpzspv_handle_t r, const spv_size_t r_offset, 
    const mpzspv_handle_t x, const spv_size_t x_offset, const spv_size_t len)
{
  unsigned int i;
  
  if (r->storage == 1 || x->storage == 1)
    {
      /* Not implemented yet */
      abort();
    }

  ASSERT (mpzspv_verify_out (r, r_offset, len));
  ASSERT (mpzspv_verify_in (x, x_offset, len));
  ASSERT_ALWAYS (r->mpzspm == x->mpzspm);
  
  for (i = 0; i < x->mpzspm->sp_num; i++)
    spv_neg (r->mem[i] + r_offset, x->mem[i] + x_offset, len, x->mpzspm->spm[i]->sp);
}

void
mpzspv_add (mpzspv_handle_t r, const spv_size_t r_offset, 
            const mpzspv_handle_t x, const spv_size_t x_offset, 
            const mpzspv_handle_t y, const spv_size_t y_offset, 
            const spv_size_t len)
{
  const size_t block_size = 65536;
  
  ASSERT_ALWAYS (r->mpzspm == x->mpzspm);
  ASSERT_ALWAYS (r->mpzspm == y->mpzspm);
  
  if (r->storage == 0 && x->storage == 0 && y->storage == 0)
    {
      unsigned int i;

      ASSERT (mpzspv_verify_out (r, r_offset, len));
      ASSERT (mpzspv_verify_in (x, x_offset, len));
      ASSERT (mpzspv_verify_in (y, y_offset, len));

      for (i = 0; i < r->mpzspm->sp_num; i++)
        spv_add (r->mem[i] + r_offset, x->mem[i] + x_offset, y->mem[i] + y_offset, len, 
                 r->mpzspm->spm[i]->sp);
    }
  else if (r->storage == 0 && x->storage == 0 && y->storage == 1)
    {
      unsigned int i;

      ASSERT (mpzspv_verify_out (r, r_offset, len));
      ASSERT (mpzspv_verify_in (x, x_offset, len));

      for (i = 0; i < r->mpzspm->sp_num; i++)
        {
          spv_add_or_mul_file (r->mem[i] + r_offset, x->mem[i] + x_offset, 
              y->files[i], y_offset, len, len, block_size, 0, 
              r->mpzspm->spm[i]);
        }
    }
  else if (r->storage == 0 && x->storage == 1 && y->storage == 0)
    {
      unsigned int i;

      ASSERT (mpzspv_verify_out (r, r_offset, len));
      ASSERT (mpzspv_verify_in (y, y_offset, len));

      for (i = 0; i < r->mpzspm->sp_num; i++)
        {
          spv_add_or_mul_file (r->mem[i] + r_offset, y->mem[i] + y_offset, 
              x->files[i], x_offset, len, len, block_size, 0, 
              r->mpzspm->spm[i]);
        }
    }
  else
    {
      /* FIXME: call add_file */
      abort();
    }
}


/* convert mpz to CRT representation, naive version */
static void
mpzspv_from_mpzv_slow (mpzspv_t x, const spv_size_t offset, const mpz_t mpz, 
                       const mpzspm_t mpzspm, ATTRIBUTE_UNUSED mpz_t rem,
		       const unsigned int sp_num)
{
  unsigned int j;

  /* GMP's comments on mpn_preinv_mod_1:
   *
   * "This function used to be documented, but is now considered obsolete.  It
   * continues to exist for binary compatibility, even when not required
   * internally."
   *
   * It doesn't accept 0 as the dividend so we have to treat this case
   * separately */
  
  valgrind_check_mpzinp (mpz);
  if (mpz_sgn (mpz) == 0)
    {
      for (j = 0; j < sp_num; j++)
        x[j][offset] = 0;
    }
  else for (j = 0; j < sp_num; j++)
    { 
#if SP_TYPE_BITS > GMP_LIMB_BITS
      mpz_tdiv_r(rem, mpz, mpzspm->spm[j]->mp_sp);
      x[j][offset] = mpz_get_sp(rem);
#else
      x[j][offset] = mpn_mod_1 (PTR(mpz), SIZ(mpz),
                              (mp_limb_t) mpzspm->spm[j]->sp);
#endif
    }
}


/* convert mpzvi to CRT representation, fast version, assumes
   mpzspm->T has been precomputed (see mpzspm.c) */
static void
mpzspv_from_mpzv_fast (mpzspv_t x, const spv_size_t offset, mpz_t mpzvi,
                       const mpzspm_t mpzspm, 
		       ATTRIBUTE_UNUSED mpz_t rem,
		       unsigned int sp_num)
{
  unsigned int i, j, k, i0 = I0_THRESHOLD, I0;
  mpzv_t *T = mpzspm->T;
  unsigned int d = mpzspm->d, ni;

  ASSERT (d > i0);
  valgrind_check_mpzinp (mpzvi);

  /* T[0] serves as vector of temporary mpz_t's, since it contains the small
     primes, which are also in mpzspm->spm[j]->sp */
  /* initially we split mpzvi in two */
  ni = 1 << (d - 1);
  mpz_mod (T[0][0], mpzvi, T[d-1][0]);
  mpz_mod (T[0][ni], mpzvi, T[d-1][1]);
  for (i = d-1; i-- > i0;)
    { /* goes down from depth i+1 to i */
      ni = 1 << i;
      for (j = k = 0; j + ni < sp_num; j += 2*ni, k += 2)
        {
          mpz_mod (T[0][j+ni], T[0][j], T[i][k+1]);
          mpz_mod (T[0][j], T[0][j], T[i][k]);
        }
      /* for the last entry T[0][j] if j < sp_num, there is nothing to do */
    }
  /* last steps */
  I0 = 1 << i0;
  for (j = 0; j < sp_num; j += I0)
    for (k = j; k < j + I0 && k < sp_num; k++)
      {
#if SP_TYPE_BITS > GMP_LIMB_BITS
	mpz_tdiv_r(rem, T[0][j], mpzspm->spm[k]->mp_sp);
	x[k][offset] = mpz_get_sp(rem);
#else
	x[k][offset] = mpn_mod_1 (PTR(T[0][j]), SIZ(T[0][j]),
                                (mp_limb_t) mpzspm->spm[k]->sp);
#endif
      }
}

/* See: Daniel J. Bernstein and Jonathan P. Sorenson,
 * Modular Exponentiation via the explicit Chinese Remainder Theorem
 *
 * memory: mpzspm->sp_num floats */

static inline void
mpzspv_to_mpz(mpz_t res, const mpzspv_t x, const spv_size_t offset, 
              const mpzspm_t mpzspm, mpz_t mt ATTRIBUTE_UNUSED)
{
  unsigned int i;
  float f = 0.5;
  mpz_set_ui (res, 0);

  for (i = 0; i < mpzspm->sp_num; i++)
    {
      const sp_t t = sp_mul (x[i][offset], mpzspm->crt3[i], 
          mpzspm->spm[i]->sp, mpzspm->spm[i]->mul_c);

      if (sizeof (unsigned long) < sizeof (sp_t))
        {
          mpz_set_sp (mt, t);
          mpz_addmul (res, mpzspm->crt1[i], mt);
        } else {
          ASSERT (t <= ULONG_MAX);
          mpz_addmul_ui (res, mpzspm->crt1[i], (unsigned long) t);
        }

      f += (float) t * mpzspm->prime_recip[i];
    }

  mpz_add (res, res, mpzspm->crt2[(unsigned int) f]);
}


void
mpzspv_fromto_mpzv (mpzspv_handle_t x, const spv_size_t offset, 
    const spv_size_t len, 
    mpz_producerfunc_t producer, void * producer_state, 
    mpz_consumerfunc_t consumer, void * consumer_state)
{
  const unsigned int sp_num = x->mpzspm->sp_num;
  const int have_consumer = consumer != NULL || consumer_state != NULL;
  const int have_producer = producer != NULL || producer_state != NULL;
  spv_size_t block_len = 1<<16, len_done = 0, read_done = 0, buffer_offset;
  mpz_t mpz1, mpz2, mt;
#if defined(HAVE_AIO_READ)
  mpzspv_t buffer[2];
  struct aiocb **aiocb_list = NULL;
#else
  mpzspv_t buffer[1];
#endif
  unsigned int nr_buffers;
  unsigned int work_buffer = 0; /* Which of the two buffers (in case of 
                                   HAVE_AIO_READ) is used for NTT conversion,
                                   the other one is used for disk I/O */

  ASSERT (sizeof (mp_limb_t) >= sizeof (sp_t));

  mpz_init(mpz1);
  mpz_init(mpz2);
  mpz_init(mt);

  if (IN_MEMORY(x))
    {
      block_len = len; /* Do whole thing at once */
      buffer[0] = x->mem;
      buffer_offset = offset;
      nr_buffers = 1;
    }
  else
    {
      /* Do piecewise, using a temp buffer */
      unsigned int i;
      char *env = getenv ("MPZSPV_FROMTO_MPZV_BLOCKLEN");
      
      if (env != NULL)
        {
          spv_size_t b = strtoul (env, NULL, 10);
          if (b > 0)
            block_len = b;
        }
      
#if defined(HAVE_AIO_READ)
      nr_buffers = 2;
#else
      nr_buffers = 1;
#endif
      for (i = 0; i < nr_buffers; i++)
        {
          buffer[i] = mpzspv_init (block_len, x->mpzspm);
          ASSERT_ALWAYS (buffer[i] != NULL);
        }
      buffer_offset = 0;
    }

#if defined(HAVE_AIO_READ)
  if (ON_DISK(x)) 
    {
      /* Allocate aiocb array and an array of pointers to it */
      unsigned int i;
      aiocb_list = (struct aiocb **) malloc (sp_num * sizeof (struct aiocb *));
      ASSERT_ALWAYS (aiocb_list != NULL);
      /* First of the pointers points to malloc-ed memory */
      aiocb_list[0] = (struct aiocb *) malloc (sp_num * sizeof(struct aiocb));
      ASSERT_ALWAYS (aiocb_list[0] != NULL);
      for (i = 0; i < sp_num; i++)
        aiocb_list[i] = &aiocb_list[0][i];
    }
#endif

#if defined(HAVE_AIO_READ)
  if (have_consumer && ON_DISK(x)) 
    {
      /* Read first buffer's worth of data from disk files */
      const spv_size_t read_now = MIN(len, block_len);
      if (read_now > 0)
        {
#if WANT_PROFILE
          unsigned long realstart = realtime();
#endif
          int r;
          r = mpzspv_lio_rw (aiocb_list, buffer[0], 0, x->files, offset, 
                             read_now, x->mpzspm, 0);
          ASSERT_ALWAYS (r == 0);
          r = mpzspv_lio_suspend ((const struct aiocb **) aiocb_list, 
                                  x->mpzspm);
          ASSERT_ALWAYS (r == 0);
          read_done += read_now;
#if WANT_PROFILE
          printf("%s(): read files from position %" PRISPVSIZE 
                 " started at %lu took %lu ms\n", 
                 __func__, offset, realstart, realtime() - realstart);
#endif
        }
    }
#endif

  while (len_done < len)
    {
      const spv_size_t len_now = MIN(len - len_done, block_len);
      const spv_size_t read_now = MIN(len - read_done, block_len);
      spv_size_t i;
#if WANT_PROFILE
      unsigned long realstart;
#endif

      /* Read x from disk files */
      if (have_consumer && ON_DISK(x) && read_now > 0) 
        {
#if WANT_PROFILE
          unsigned long realstart = realtime();
#endif
#if defined(HAVE_AIO_READ)
          int r;
          r = mpzspv_lio_rw (aiocb_list, buffer[work_buffer ^ 1], 0, x->files, 
                             offset + read_done, read_now, x->mpzspm, 0);
          ASSERT_ALWAYS (r == 0);
#else
          mpzspv_seek_and_read (buffer[0], 0, x->files, offset + read_done, 
                                read_now, x->mpzspm);
#endif
#if WANT_PROFILE
          printf("%s(): scheduling read files from position %" PRISPVSIZE 
                 " started at %lu took %lu ms\n", 
                 __func__, offset + read_done, realstart, 
                 realtime() - realstart);
#endif
        }

      /* Do the conversion */
#if WANT_PROFILE
      realstart = realtime();
#endif
      for (i = 0; i < len_now; i++)
        {
          if (have_producer)
            {
              if (producer != NULL)
                {
                  /* Get new mpz1 from producer */
                  (*producer)(producer_state, mpz1);
                  valgrind_check_mpzinp (mpz1);
                } else {
                  /* Get new mpz1 from listz_t */
                  valgrind_check_mpzinp (((mpz_t *)producer_state)[len_done + i]);
                  mpz_set (mpz1, ((listz_t)producer_state)[len_done + i]);
                }
            }
          
          if (have_consumer)
            {
              /* Convert NTT entry to mpz2 */
              mpzspv_to_mpz (mpz2, buffer[work_buffer], buffer_offset + i, 
                             x->mpzspm, mt);
              if (consumer != NULL)
                {
                  /* Give mpz2 to consumer function */
                  mpz_mod (mpz2, mpz2, x->mpzspm->modulus);
                  (*consumer)(consumer_state, mpz2);
                } else {
                  mpz_mod (((listz_t)consumer_state)[len_done + i], mpz2, 
                           x->mpzspm->modulus);
                }
            }
          
          if (have_producer)
            {
              /* Convert the mpz1 we got from producer to NTT */
              mpzspv_from_mpzv_slow (buffer[work_buffer], buffer_offset + i, 
                                     mpz1, x->mpzspm, mt, sp_num);
            }
        }
#if WANT_PROFILE
    printf("%s(): processing buffer started at %lu took %lu ms\n", 
           __func__, realstart, realtime() - realstart);
#endif

      if (have_consumer && ON_DISK(x) && read_now > 0) 
        {
          /* Wait for read to complete */
#if WANT_PROFILE
          unsigned long realstart = realtime();
#endif
#if defined(HAVE_AIO_READ)
          int r;
          r = mpzspv_lio_suspend ((const struct aiocb **) aiocb_list, 
                                  x->mpzspm);
          ASSERT_ALWAYS (r == 0);
#endif
#if WANT_PROFILE
          printf("%s(): suspend of read files from position %" PRISPVSIZE 
                 " started at %lu took %lu ms\n", 
                 __func__, offset + read_done, realstart, realtime() - realstart);
#endif
          read_done += read_now;
        }

    /* Write current buffer to disk files */
    if (have_producer && ON_DISK(x)) {
#if WANT_PROFILE
      unsigned long realstart = realtime();
#endif
#if defined(HAVE_AIO_READ)
      int r;
      r = mpzspv_lio_rw (aiocb_list, buffer[work_buffer], 0, x->files, 
                         offset + len_done, len_now, x->mpzspm, 1);
      ASSERT_ALWAYS (r == 0);
#else
      mpzspv_seek_and_write (buffer[work_buffer], 0, x->files, offset + len_done, 
                             len_now, x->mpzspm);
#endif
#if WANT_PROFILE
      printf("%s(): write files at position %" PRISPVSIZE 
             " started at %lu took %lu ms\n", 
             __func__, offset + len_done, realstart, realtime() - realstart);
#endif
    }
    len_done += len_now;

    /* Toggle between the two buffers */
    if (nr_buffers == 2) 
      work_buffer ^= 1;

    /* If we write NTT data to memory, we need to advance the offset to fill 
       the entire array. If we use a temp buffer, we reuse the same buffer 
       each time */
    if (IN_MEMORY(x))
      buffer_offset += len_now;
  }
  mpz_clear(mpz1);
  mpz_clear(mpz2);
  mpz_clear(mt);

  if (!IN_MEMORY(x))
    {
      unsigned int i;
      for (i = 0; i < nr_buffers; i++)
        {
          mpzspv_clear (buffer[i], x->mpzspm);
          buffer[i] = NULL;
        }
    }

#if defined(HAVE_AIO_READ)
  if (ON_DISK(x)) 
    {
      free (aiocb_list[0]);
      free(aiocb_list);
    }
#endif
}


/* B&S: ecrt mod m mod p_j.
 *
 * memory: MPZSPV_NORMALISE_STRIDE mpzspv coeffs
 *         6 * MPZSPV_NORMALISE_STRIDE sp's
 *         MPZSPV_NORMALISE_STRIDE floats */
void
mpzspv_normalise (mpzspv_handle_t x, const spv_size_t offset, 
                  const spv_size_t len)
{
  unsigned int i, j, sp_num = x->mpzspm->sp_num;
  spv_size_t k, l;
  sp_t v;
  spv_t s, d, w;
  spm_t *spm = x->mpzspm->spm;
  
  float prime_recip;
  float *f;
  mpzspv_handle_t t;
  
  if (x->storage == 1)
    {
      /* Not implemented yet */
      abort();
    }
  
  ASSERT (mpzspv_verify_in (x, offset, len)); 

  f = (float *) malloc (MPZSPV_NORMALISE_STRIDE * sizeof (float));
  s = (spv_t) malloc (3 * MPZSPV_NORMALISE_STRIDE * sizeof (sp_t));
  d = (spv_t) malloc (3 * MPZSPV_NORMALISE_STRIDE * sizeof (sp_t));
  if (f == NULL || s == NULL || d == NULL)
    {
      fprintf (stderr, "%s(): Cannot allocate memory\n", __func__);
      exit (1);
    }
  t = mpzspv_init_handle (NULL, MPZSPV_NORMALISE_STRIDE, x->mpzspm);
  
  memset (s, 0, 3 * MPZSPV_NORMALISE_STRIDE * sizeof (sp_t));

  for (l = 0; l < len; l += MPZSPV_NORMALISE_STRIDE)
    {
      spv_size_t stride = MIN (MPZSPV_NORMALISE_STRIDE, len - l);
      
      /* FIXME: use B&S Theorem 2.2 */
      for (k = 0; k < stride; k++)
	f[k] = 0.5;
      
      for (i = 0; i < sp_num; i++)
        {
          prime_recip = 1.0f / (float) spm[i]->sp;
      
          for (k = 0; k < stride; k++)
	    {
	      x->mem[i][l + k + offset] = sp_mul (x->mem[i][l + k + offset],
	          x->mpzspm->crt3[i], spm[i]->sp, spm[i]->mul_c);
	      f[k] += (float) x->mem[i][l + k + offset] * prime_recip;
	    }
        }
      
      for (i = 0; i < sp_num; i++)
        {
	  for (k = 0; k < stride; k++)
	    {
	      sp_wide_mul (d[3 * k + 1], d[3 * k], x->mpzspm->crt5[i],
		  (sp_t) f[k]);
              d[3 * k + 2] = 0;
	    }
	
          for (j = 0; j < sp_num; j++)
            {
	      w = x->mem[j] + offset;
	      v = x->mpzspm->crt4[i][j];
	    
	      for (k = 0; k < stride; k++)
	        sp_wide_mul (s[3 * k + 1], s[3 * k], w[k + l], v);
 	      
	      /* this mpn_add_n accounts for about a third of the function's
	       * runtime */
	      mpn_add_n (d, d, s, 3 * stride);
            }      

          for (k = 0; k < stride; k++)
	    t->mem[i][k] = mpn_mod_1 (d + 3 * k, 3, spm[i]->sp);
        }	  
      mpzspv_set (x, l + offset, t, 0, stride);
    }
  
  mpzspv_clear_handle (t);
  
  free (s);
  free (d);
  free (f);
}


void
mpzspv_random (mpzspv_handle_t x, const spv_size_t offset, 
               const spv_size_t len)
{
  unsigned int i;

  if (x->storage == 1)
    {
      /* Not implemented yet */
      abort();
    }
  
  ASSERT (mpzspv_verify_out (x, offset, len));

  for (i = 0; i < x->mpzspm->sp_num; i++)
    spv_random (x->mem[i] + offset, len, x->mpzspm->spm[i]->sp);
}


/* Adds or multiplies sp_t's from x and a file and stores result in r. 
   r[i] = x[i] + f[i] (+ x[i + wrap_size] + f[i + wrap_size] ...)
   Adds if add_or_mul == 0 */
static void
spv_add_or_mul_file (spv_t r, const spv_t x, FILE *f, const spv_size_t f_offset, 
    const spv_size_t len, const spv_size_t wrap_size, 
    const spv_size_t block_len, const int add_or_mul, const spm_t spm)
{
  spv_t tmp_block;
  spv_size_t nr_read = 0;

  if (len == 0)
    return; 

  ASSERT(block_len > 0);
  ASSERT(wrap_size > 0);
  ASSERT(block_len <= wrap_size); /* We assume at most 1 wrap per block */

  tmp_block = (spv_t) sp_aligned_malloc (block_len * sizeof (sp_t));
  if (tmp_block == NULL) 
    {
      fprintf (stderr, "%s(): could not allocate memory\n", __func__);
      abort();
    }

  while (nr_read < len)
    {
      const spv_size_t nr_now = MIN(len - nr_read, block_len);
      const spv_size_t offset_within_wrap = nr_read % wrap_size;
      const spv_size_t len_before_wrap = 
        MIN(nr_now, wrap_size - offset_within_wrap);
      const spv_size_t len_after_wrap = nr_now - len_before_wrap;

      spv_seek_and_read (tmp_block, nr_now, f_offset + nr_read, f);

      if (add_or_mul == 0)
        spv_add (r + offset_within_wrap, x + nr_read, tmp_block, 
                 len_before_wrap, spm->sp);
      else
        spv_pwmul (r + offset_within_wrap, x + nr_read, tmp_block, 
                   len_before_wrap, spm->sp, spm->mul_c);

      if (len_after_wrap != 0)
        {
          if (add_or_mul == 0)
            spv_add (r, x + nr_read + len_before_wrap, 
                     tmp_block + len_before_wrap, len_after_wrap, 
                     spm->sp);
          else
            spv_pwmul (r, x + nr_read + len_before_wrap, 
                       tmp_block + len_before_wrap, len_after_wrap, 
                       spm->sp, spm->mul_c);
        }
      nr_read += nr_now;
    }
  sp_aligned_free (tmp_block);
}


/* Adds or multiplies sp_t's from two file and stores result in r. 
   Adds if add_or_mul == 0 */
static void ATTRIBUTE_UNUSED
spv_add_or_mul_2file (spv_t r, FILE *f1, FILE *f2, const spv_size_t len, 
    const spv_size_t wrap_size, const spv_size_t block_len, 
    const int add_or_mul, const spm_t spm)
{
  spv_t tmp_block1, tmp_block2;
  spv_size_t nr_read = 0;

  if (len == 0)
    return; 

  ASSERT(block_len > 0);
  ASSERT(wrap_size > 0);
  ASSERT(block_len <= wrap_size); /* We assume at most 1 wrap per block */

  tmp_block1 = (spv_t) sp_aligned_malloc (block_len * sizeof (sp_t));
  tmp_block2 = (spv_t) sp_aligned_malloc (block_len * sizeof (sp_t));
  if (tmp_block1 == NULL || tmp_block2 == NULL) 
    {
      fprintf (stderr, "%s(): could not allocate memory\n", __func__);
      abort();
    }

  while (nr_read < len)
    {
      const spv_size_t nr_now = MIN(len - nr_read, block_len);
      const spv_size_t offset_within_wrap = nr_read % wrap_size;
      const spv_size_t len_before_wrap = 
        MIN(nr_now, wrap_size - offset_within_wrap);
      const spv_size_t len_after_wrap = nr_now - len_before_wrap;

      fread (tmp_block1, sizeof(sp_t), nr_now, f1);
      fread (tmp_block2, sizeof(sp_t), nr_now, f2);

      if (add_or_mul == 0)
        spv_add (r + offset_within_wrap, tmp_block1, tmp_block2, 
                 len_before_wrap, spm->sp);
      else
        spv_pwmul (r + offset_within_wrap, tmp_block1, tmp_block2, 
                   len_before_wrap, spm->sp, spm->mul_c);

      if (len_after_wrap != 0)
        {
          if (add_or_mul == 0)
            spv_add (r, tmp_block1 + len_before_wrap, 
                     tmp_block2 + len_before_wrap, len_after_wrap, spm->sp);
          else
            spv_pwmul (r, tmp_block1 + len_before_wrap, 
                       tmp_block2 + len_before_wrap, len_after_wrap, spm->sp, 
                       spm->mul_c);
        }
      nr_read += nr_now;
    }
  
  sp_aligned_free (tmp_block1);
  sp_aligned_free (tmp_block2);
}


static void 
mpzspv_seek_and_read (mpzspv_t dst, spv_size_t offset, FILE **sp_files, 
                      const size_t fileoffset, size_t nread, mpzspm_t mpzspm)
{
  unsigned int j;
  for (j = 0; j < mpzspm->sp_num; j++)
  {
    spv_seek_and_read (dst[j] + offset, nread, fileoffset, sp_files[j]);
  }
}


static void ATTRIBUTE_UNUSED 
mpzspv_seek_and_write (mpzspv_t src, const spv_size_t offset, FILE **sp_files, 
                       const size_t fileoffset, const size_t nwrite, 
                       const mpzspm_t mpzspm)
{
  unsigned int j;
  for (j = 0; j < mpzspm->sp_num; j++)
  {
    spv_seek_and_write (src[j] + offset, nwrite, fileoffset, sp_files[j]);
  }
}

static void
mul_dct_file (const spv_t r, const spv_t spv, FILE *dct_file, 
              const spv_size_t dftlen, const spv_size_t blocklen, const spm_t spm)
{
  const spv_size_t dctlen = dftlen / 2 + 1;
  spv_size_t nr_read = 0, i;
  unsigned long m = 5UL;
  spv_t tmp;
  
  ASSERT(dftlen % 2 == 0);
  if (dftlen == 0)
    return;
  
  tmp = (spv_t) sp_aligned_malloc (MIN(blocklen, dctlen) * sizeof (sp_t));
  if (tmp == NULL) 
    {
      fprintf (stderr, "%s(): could not allocate memory\n", __func__);
      abort();
    }

  while (nr_read < dctlen)
    {
      const spv_size_t read_now = MIN(dctlen - nr_read, blocklen);
      const spv_size_t mul_now = MIN(dctlen - nr_read - 1, blocklen);
      
      spv_seek_and_read (tmp, read_now, nr_read, dct_file);
      
      i = 0;
      if (nr_read == 0)
        {
          r[0] = sp_mul (spv[0], tmp[0], spm->sp, spm->mul_c);
          i = 1;
        }
      
      for ( ; i < mul_now; i++)
        {
          const spv_size_t j = nr_read + i;
          /* This works, but why? */
          if (3*j > m)
            m = 2UL * m + 1;
          
          r[2*j] = sp_mul (spv[2*j], tmp[i], spm->sp, spm->mul_c);
          r[m - 2*j] = sp_mul (spv[m - 2*j], tmp[i], spm->sp, spm->mul_c);
        }
      nr_read += read_now;
      if (nr_read == dctlen)
        {
#ifdef USE_VALGRIND
          VALGRIND_CHECK_VALUE_IS_DEFINED(tmp[i]);
#endif
          r[1] = sp_mul (spv[1], tmp[i], spm->sp, spm->mul_c);
        }
    }
  sp_aligned_free(tmp);
}


/* Multiply the DFT of a polynomial by the DCT-I of a reciprocal Laurent
   polynomial. */
static void
mul_dct(spv_t r, const spv_t spv, const spv_t dct, const spv_size_t len, 
        const spm_t spm)
{
  unsigned long m = 5UL, i;
  
  if (len > 0)
    r[0] = sp_mul (spv[0], dct[0], spm->sp, spm->mul_c);
  if (len > 1)
    r[1] = sp_mul (spv[1], dct[len / 2UL], spm->sp, spm->mul_c);
  
  ASSERT(len % 2 == 0);
  for (i = 2UL; i < len; i += 2UL)
    {
      /* This works, but why? */
      if (i + i / 2UL > m)
        m = 2UL * m + 1;
      
      r[i] = sp_mul (spv[i], dct[i / 2UL], spm->sp, spm->mul_c);
      r[m - i] = sp_mul (spv[m - i], dct[i / 2UL], spm->sp, 
                         spm->mul_c);
    }
}


/* Do multiplication via NTT. Depending on the value of "steps", does 
   forward transform of, pair-wise multiplication, inverse transform. 
   Input and output spv_t's can be stored in files. 
   It is permissible to let any combination of x, y, and r point at the same 
   memory or files. */

void
mpzspv_mul_ntt (mpzspv_handle_t r, const spv_size_t offsetr, 
    mpzspv_handle_t x, const spv_size_t offsetx, const spv_size_t lenx, 
    mpzspv_handle_t y, const spv_size_t offsety, const spv_size_t leny, 
    const spv_size_t ntt_size, const int monic, const spv_size_t monic_pos, 
    const int steps)
{
  const spv_size_t block_len = 16384;
  const spv_size_t log2_ntt_size = ceil_log_2 (ntt_size);
  const int do_fft1 = (steps & NTT_MUL_STEP_FFT1) != 0;
  const int do_pwmul = (steps & NTT_MUL_STEP_MUL) != 0;
  const int do_pwmul_dct = (steps & NTT_MUL_STEP_MULDCT) != 0;
  const int do_ifft = (steps & NTT_MUL_STEP_IFFT) != 0;
  int i;
  mpzspm_t mpzspm = NULL;

  /* Check that the inputs/outputs all use the same NTT definition */
  if (x != NULL) 
    mpzspm = x->mpzspm;
  if (y != NULL) 
    {
      ASSERT_ALWAYS (mpzspm == NULL || y->mpzspm == mpzspm);
      mpzspm = y->mpzspm;
    }
  if (r != NULL)
    {
      ASSERT_ALWAYS (mpzspm == NULL || r->mpzspm == mpzspm);
      mpzspm = r->mpzspm;
    }

  if (do_pwmul && do_pwmul_dct)
    {
      fprintf (stderr, "%s(): Error, both PWMUL and PWMULDCT requested\n",
               __func__);
      abort();
    }
  
  if (IN_MEMORY(x))
    {
      ASSERT (mpzspv_verify_in (x, offsetx, lenx));
    }
  if (IN_MEMORY(y)) 
    {
      ASSERT (mpzspv_verify_in (y, offsety, leny));
    }
  if (IN_MEMORY(r))
    {
      ASSERT (mpzspv_verify_out (r, offsetr, ntt_size));
    }

#if TRACE_ntt_mul
  printf ("%s (r = {%d, %p, %p, %p}, offsetr = %lu, "
          "x = {%d, %p, %p, %p}, offsetx = %lu, lenx = %lu, "
          "y = {%d, %p, %p, %p}, offsety = %lu, leny = %lu, "
          "ntt_size = %lu, monic = %d, monic_pos = %lu, steps = %d)\n", __func__, 
          r ? r->storage : 0, r ? r->mpzspm : NULL, r ? r->mem : NULL, r ? r->files : NULL, (unsigned long) offsetr, 
          x ? x->storage : 0, x ? x->mpzspm : NULL, x ? x->mem : NULL, x ? x->files : NULL, (unsigned long) offsetx, (unsigned long) lenx,
          y ? y->storage : 0, y ? y->mpzspm : NULL, y ? y->mem : NULL, y ? y->files : NULL, (unsigned long) offsety, (unsigned long) leny, 
          (unsigned long) ntt_size, monic, (unsigned long) monic_pos, steps);
  if (x != NULL)
    mpzspv_print (x, offsetx, lenx, "x");
  if (y != NULL)
    mpzspv_print (y, offsetx, lenx, "y");
#endif
  
  /* Need parallelization at higher level (e.g., handling a branch of the 
     product tree in one thread) to make this worthwhile for ECM */

#if defined(_OPENMP)
#pragma omp parallel if (ntt_size > 32768)
#endif
  {

#if defined(_OPENMP)
#pragma omp for
#endif
  for (i = 0; i < (int) mpzspm->sp_num; i++)
    {
      const spm_t spm = mpzspm->spm[i];
      const spv_t spvx = IN_MEMORY(x) ? x->mem[i] + offsetx : NULL;
      const spv_t spvy = IN_MEMORY(y) ? y->mem[i] + offsety : NULL;
      const spv_t spvr = IN_MEMORY(r) ? r->mem[i] + offsetr : NULL;
      spv_t tmp = NULL;
#if WANT_PROFILE
      unsigned long realstart;
#endif

      /* If we do any arithmetic, we need some memory to do it in. 
         If r is in memory, we can use that as temp storage, so long as we 
         don't overwrite input data we still need. */

      /* This test does not check whether r+offsetr and y+offsety point to
         non-overlapping memory; it simply takes r==y as not allowing r for
         temp space */
      if (IN_MEMORY(r) && !(IN_MEMORY(y) && r->mem == y->mem))
        tmp = spvr;
      else
        {
          tmp = (spv_t) sp_aligned_malloc (ntt_size * sizeof (sp_t));
          if (tmp == NULL)
            {
              fprintf (stderr, "%s(): Cannot allocate tmp memory\n",
                       __func__);
              abort();
            }
        }

      /* If we do any arithmetic, read the data of x into tmp and do any 
         wrap-around */
      if (do_fft1 || do_pwmul || do_pwmul_dct || do_ifft)
        {
          ASSERT_ALWAYS(x != NULL && r != NULL);
          if (IN_MEMORY(x))
            {
              spv_size_t j;
              if (tmp != spvx)
                spv_set (tmp, spvx, MIN(ntt_size, lenx));
              for (j = ntt_size; j < lenx; j += ntt_size)
                {
                  spv_size_t len_now = MIN(lenx - j, ntt_size);
                  spv_add (tmp, tmp, spvx + j, len_now, spm->sp);
                }
            }
          else 
            {
#if WANT_PROFILE
              realstart = realtime();
#endif
              spv_seek_and_read (tmp, MIN(ntt_size, lenx), offsetx, 
                                 x->files[i]);
              if (ntt_size < lenx)
                spv_add_or_mul_file (tmp, tmp, x->files[i], offsetx + lenx, 
                                 lenx - ntt_size, ntt_size, block_len, 0, spm);
#if WANT_PROFILE
              printf("%s(): read vector %d started at %lu took %lu ms\n", 
                     __func__, i, realstart, realtime() - realstart);
#endif
            } 

          if (ntt_size > lenx)
            spv_set_zero (tmp + lenx, ntt_size - lenx);
        }
      
      if (do_fft1) 
        {
#if WANT_PROFILE
          realstart = realtime();
#endif
          if (monic)
            tmp[lenx % ntt_size] = sp_add (tmp[lenx % ntt_size], 1, spm->sp);

          spv_ntt_gfp_dif (tmp, log2_ntt_size, spm);
#if WANT_PROFILE
          printf("%s(): fft on vector %d started at %lu took %lu ms\n", 
                 __func__, i, realstart, realtime() - realstart);
#endif
        }

      if (do_pwmul) 
        {
#if WANT_PROFILE
          realstart = realtime();
#endif
          ASSERT_ALWAYS(y != NULL);
          ASSERT_ALWAYS(leny == ntt_size);
          if (IN_MEMORY(y))
            spv_pwmul (tmp, tmp, spvy, ntt_size, spm->sp, spm->mul_c);
          else 
            spv_add_or_mul_file (tmp, tmp, y->files[i], offsety, ntt_size, ntt_size, 
                             block_len, 1, spm);
#if WANT_PROFILE
          printf("%s(): pwmul on vector %d started at %lu took %lu ms\n", 
                 __func__, i, realstart, realtime() - realstart);
#endif
        }
      else if (do_pwmul_dct)
        {
          ASSERT_ALWAYS(y != NULL);
          ASSERT_ALWAYS(leny == ntt_size / 2 + 1);
#if WANT_PROFILE
          realstart = realtime();
#endif
          if (IN_MEMORY(y))
            mul_dct (tmp, tmp, spvy, ntt_size, spm);
          else
            mul_dct_file (tmp, tmp, y->files[i], ntt_size, block_len, spm);
#if WANT_PROFILE
          printf("%s(): pwmuldct on vector %d started at %lu took %lu ms\n", 
                 __func__, i, realstart, realtime() - realstart);
#endif
        }

      if (do_ifft) 
        {
          ASSERT (sizeof (mp_limb_t) >= sizeof (sp_t));

#if WANT_PROFILE
          realstart = realtime();
#endif
          spv_ntt_gfp_dit (tmp, log2_ntt_size, spm);

          /* spm->sp - (spm->sp - 1) / ntt_size is the inverse of ntt_size */
          spv_mul_sp (tmp, tmp, spm->sp - (spm->sp - 1) / ntt_size,
                      ntt_size, spm->sp, spm->mul_c);

          if (monic)
            tmp[monic_pos % ntt_size] = sp_sub (tmp[monic_pos % ntt_size],
                1, spm->sp);
#if WANT_PROFILE
          printf("%s(): ifft on vector %d started at %lu took %lu ms\n", 
                 __func__, i, realstart, realtime() - realstart);
#endif
        }

      if (do_fft1 || do_pwmul || do_pwmul_dct || do_ifft)
        {
          if (IN_MEMORY(r))
            {
              if (tmp != spvr)
                spv_set (spvr, tmp, ntt_size);
            }
          else
            {
#if WANT_PROFILE
              realstart = realtime();
#endif
              spv_seek_and_write (tmp, ntt_size, offsetr, r->files[i]);
#if WANT_PROFILE
          printf("%s(): write of vector %d started at %lu took %lu ms\n", 
                 __func__, i, realstart, realtime() - realstart);
#endif
            }

          if (tmp != spvr)
            sp_aligned_free (tmp);
        }
    }
  }

#if TRACE_ntt_mul
  if (r != NULL)
    mpzspv_print (r, offsetx, lenx, "r");
#endif
}


/* Computes a DCT-I of length dctlen. Input is the spvlen coefficients
   in spv. FIXME: handle wrap-around in input data */

void
mpzspv_to_dct1 (mpzspv_handle_t dct, const mpzspv_handle_t spv,  
                const spv_size_t spvlen, const spv_size_t dctlen)
{
  const spv_size_t ntt_size = 2 * (dctlen - 1); /* Length for the DFT */
  const spv_size_t log2_l = ceil_log_2 (ntt_size);
  int j;

  ASSERT_ALWAYS (dct->mpzspm == spv->mpzspm);
  ASSERT (mpzspv_verify_out (dct, 0, dctlen));
  ASSERT (mpzspv_verify_in (spv, 0, spvlen));

#ifdef _OPENMP
#pragma omp parallel
#endif
  {

#ifdef _OPENMP
#pragma omp for
#endif
  for (j = 0; j < (int) spv->mpzspm->sp_num; j++)
    {
      const spm_t spm = spv->mpzspm->spm[j];
      spv_size_t i;
      
      spv_t tmp = (spv_t) sp_aligned_malloc (ntt_size * sizeof (sp_t));
      if (tmp == NULL)
        {
          fprintf (stderr, "%s(): Cannot allocate tmp memory in\n", __func__);
          abort();
        }

      if (ON_DISK(spv))
        {
          spv_seek_and_read (tmp, spvlen, 0, spv->files[j]);
        } else {
          /* Copy spv to tmp */
          spv_set (tmp, spv->mem[j], spvlen);
        }
      /* Make a symmetric copy of input coefficients in tmp. E.g., 
         with spv = [3, 2, 1], spvlen = 3, dctlen = 5 (hence ntt_size = 8), 
         we want tmp = [3, 2, 1, 0, 0, 0, 1, 2] */
      spv_rev (tmp + ntt_size - spvlen + 1, tmp + 1, spvlen - 1);
      /* Now we have [3, 2, 1, ?, ?, ?, 1, 2]. Fill the ?'s with zeros. */
      spv_set_sp (tmp + spvlen, (sp_t) 0, ntt_size - 2 * spvlen + 1);

#if 0
      printf ("%s: tmp[%d] = [", __func__, j);
      for (i = 0; i < ntt_size; i++)
          printf ("%lu, ", tmp[i]);
      printf ("]\n");
#endif
      
      spv_ntt_gfp_dif (tmp, log2_l, spm);

#if 0
      printf ("%s: tmp[%d] = [", __func__, j);
      for (i = 0; i < ntt_size; i++)
          printf ("%lu, ", tmp[i]);
      printf ("]\n");
#endif

      /* The forward transform is scrambled. We want elements [0 ... ntt_size/2]
         of the unscrabled data, that is all the coefficients with the most 
         significant bit in the index (in log2(ntt_size) word size) unset, plus the 
         element at index ntt_size/2. By scrambling, these map to the elements with 
         even index, plus the element at index 1. 
         The elements with scrambled index 2*i are stored in h[i], the
         element with scrambled index 1 is stored in h[params->ntt_size] */
  
#ifdef WANT_ASSERT
      /* Test that the coefficients are symmetric (if they were unscrambled)
         and that our algorithm for finding identical coefficients in the 
         scrambled data works */
      {
        spv_size_t m = 5;
        for (i = 2; i < ntt_size; i += 2L)
          {
            /* This works, but why? */
            if (i + i / 2L > m)
                m = 2L * m + 1L;

            ASSERT (tmp[i] == tmp[m - i]);
#if 0
            printf ("%s: DFT[%lu] == DFT[%lu]\n", __func__, i, m - i);
#endif
          }
      }
#endif

      /* Copy coefficients to dct buffer */
      {
        spv_t out_buf = IN_MEMORY(dct) ? dct->mem[j] : tmp;
        const sp_t coeff_1 = tmp[1];
        for (i = 0; i < dctlen - 1; i++)
          out_buf[i] = tmp[i * 2];
        out_buf[dctlen - 1] = coeff_1;
        if (ON_DISK(dct))
          {
            /* Write data back to file */
            spv_seek_and_write (tmp, dctlen, 0, dct->files[j]);
          }
      }

      sp_aligned_free(tmp);
    }
  }
}


ATTRIBUTE_UNUSED
static void
spv_print_vec (const char *msg, const spv_t spv, const spv_size_t l)
{
  spv_size_t i;
  printf ("%s [%lu", msg, spv[0]);
  for (i = 1; i < l; i++)
    printf (", %lu", spv[i]);
  printf ("]\n");
}


static void
spv_sqr_reciprocal(const spv_size_t n, const spm_t spm, const spv_t spv, 
                   const sp_t max_ntt_size)
{
  const spv_size_t log2_n = ceil_log_2 (n);
  const spv_size_t len = ((spv_size_t) 2) << log2_n;
  const spv_size_t log2_len = 1 + log2_n;
  sp_t w1, w2, invlen;
  const sp_t sp = spm->sp, mul_c = spm->mul_c;
  spv_size_t i;

  /* Zero out NTT elements [n .. len-n] */
  spv_set_sp (spv + n, (sp_t) 0, len - 2*n + 1);

#if TRACE_ntt_sqr_reciprocal
  printf ("%s: NTT vector mod %lu\n", __func__, sp);
  spv_print_vec ("%s: before weighting:", __func__, spv, n);
#endif

  /* Compute the root for the weight signal, a 3rd primitive root 
     of unity */
  w1 = sp_pow (spm->prim_root, max_ntt_size / 3UL, sp, mul_c);
  /* Compute iw= 1/w */
  w2 = sp_pow (spm->inv_prim_root, max_ntt_size / 3UL, sp, mul_c);
#if TRACE_ntt_sqr_reciprocal
  printf ("w1 = %lu ,w2 = %lu\n", w1, w2);
#endif
  ASSERT(sp_mul(w1, w2, sp, mul_c) == (sp_t) 1);
  ASSERT(w1 != (sp_t) 1);
  ASSERT(sp_pow (w1, 3UL, sp, mul_c) == (sp_t) 1);
  ASSERT(w2 != (sp_t) 1);
  ASSERT(sp_pow (w2, 3UL, sp, mul_c) == (sp_t) 1);

  /* Fill NTT elements spv[len-n+1 .. len-1] with coefficients and
     apply weight signal to spv[i] and spv[l-i] for 0 <= i < n
     Use the fact that w^i + w^{-i} = -1 if i != 0 (mod 3). */
  for (i = 0; i + 2 < n; i += 3)
    {
      sp_t t, u;
      
      if (i > 0)
        spv[len - i] = spv[i];
      
      t = spv[i + 1];
      u = sp_mul (t, w1, sp, mul_c);
      spv[i + 1] = u;
      spv[len - i - 1] = sp_neg (sp_add (t, u, sp), sp);

      t = spv[i + 2];
      u = sp_mul (t, w2, sp, mul_c);
      spv[i + 2] = u;
      spv[len - i - 2] = sp_neg (sp_add (t, u, sp), sp);
    }
  if (i < n && i > 0)
    {
      spv[len - i] = spv[i];
    }
  if (i + 1 < n)
    {
      sp_t t, u;
      t = spv[i + 1];
      u = sp_mul (t, w1, sp, mul_c);
      spv[i + 1] = u;
      spv[len - i - 1] = sp_neg (sp_add (t, u, sp), sp);
    }

#if TRACE_ntt_sqr_reciprocal
  spv_print_vec ("%s: after weighting:", __func__, spv, len);
#endif

  /* Forward DFT of dft[j] */
  spv_ntt_gfp_dif (spv, log2_len, spm);

#if TRACE_ntt_sqr_reciprocal
  spv_print_vec ("%s: after forward transform:", __func__, spv, len);
#endif

  /* Square the transformed vector point-wise */
  spv_pwmul (spv, spv, spv, len, sp, mul_c);

#if TRACE_ntt_sqr_reciprocal
  spv_print_vec ("%s: after point-wise squaring:", __func__, spv, len);
#endif

  /* Inverse transform of dft[j] */
  spv_ntt_gfp_dit (spv, log2_len, spm);

#if TRACE_ntt_sqr_reciprocal
  spv_print_vec ("%s: after inverse transform:", __func__, spv, len);
#endif

  /* Un-weight and divide by transform length */
  invlen = sp - (sp - (sp_t) 1) / len; /* invlen = 1/len (mod sp) */
  w1 = sp_mul (invlen, w1, sp, mul_c);
  w2 = sp_mul (invlen, w2, sp, mul_c);
  for (i = 0; i < 2 * n - 3; i += 3)
    {
      spv[i] = sp_mul (spv[i], invlen, sp, mul_c);
      spv[i + 1] = sp_mul (spv[i + 1], w2, sp, mul_c);
      spv[i + 2] = sp_mul (spv[i + 2], w1, sp, mul_c);
    }
  if (i < 2 * n - 1)
    spv[i] = sp_mul (spv[i], invlen, sp, mul_c);
  if (i < 2 * n - 2)
    spv[i + 1] = sp_mul (spv[i + 1], w2, sp, mul_c);
  
#if TRACE_ntt_sqr_reciprocal
  spv_print_vec ("%s: after un-weighting:", __func__, spv, len);
#endif

  /* Separate the coefficients of R in the wrapped-around product. */

  /* Set w1 = cuberoot(1)^l where cuberoot(1) is the same primitive
     3rd root of unity we used for the weight signal */
  w1 = sp_pow (spm->prim_root, max_ntt_size / 3UL, sp, mul_c);
  w1 = sp_pow (w1, len % 3UL, sp, mul_c);
  
  /* Set w2 = 1/(w1 - 1/w1). Incidentally, w2 = 1/sqrt(-3) */
  w2 = sp_inv (w1, sp, mul_c);
  w2 = sp_sub (w1, w2, sp);
  w2 = sp_inv (w2, sp, mul_c);
#if TRACE_ntt_sqr_reciprocal
  printf ("For separating: w1 = %lu, w2 = %lu\n", w1, w2);
#endif
  
  for (i = len - (2*n - 2); i <= len / 2; i++)
    {
      sp_t t, u;
      /* spv[i] = s_i + w^{-l} s_{l-i}. 
         spv[l-i] = s_{l-i} + w^{-l} s_i */
      t = sp_mul (spv[i], w1, sp, mul_c); /* t = w^l s_i + s_{l-i} */
      t = sp_sub (t, spv[len - i], sp);   /* t = w^l s_i + w^{-l} s_i */
      t = sp_mul (t, w2, sp, mul_c);      /* t = s_1 */

      u = sp_sub (spv[i], t, sp);         /* u = w^{-l} s_{l-i} */
      u = sp_mul (u, w1, sp, mul_c);      /* u = s_{l-i} */
      spv[i] = t;
      spv[len - i] = u;
      ASSERT(i < len / 2 || t == u);
    }

#if TRACE_ntt_sqr_reciprocal
  spv_print_vec ("%s: after un-wrapping:", __func__, spv, len);
#endif
}


/* Square an RLP */

void 
mpzspv_sqr_reciprocal (mpzspv_handle_t x, const spv_size_t n)
{
  const spv_size_t log2_n = ceil_log_2 (n);
  const spv_size_t len = ((spv_size_t) 2) << log2_n;

  ASSERT(x->mpzspm->max_ntt_size % 3UL == 0UL);
  ASSERT(len % 3UL != 0UL);
  ASSERT(x->mpzspm->max_ntt_size % len == 0UL);

#ifdef _OPENMP
#pragma omp parallel
#endif
  {
    int j;

#ifdef _OPENMP
#pragma omp for
#endif
    for (j = 0; j < (int) (x->mpzspm->sp_num); j++)
      {
        spv_t tmp;
        
        if (ON_DISK(x))
          {
            tmp = (spv_t) sp_aligned_malloc (len * sizeof (sp_t));

            if (tmp == NULL)
              {
                fprintf (stderr, "%s(): Cannot allocate tmp memory\n", __func__);
                abort();
              }
            spv_seek_and_read (tmp, n, 0, x->files[j]);
          }
        else
          {
            tmp = x->mem[j];
          }
        
        spv_sqr_reciprocal (n, x->mpzspm->spm[j], tmp, x->mpzspm->max_ntt_size);
        
        if (ON_DISK(x))
          {
            spv_seek_and_write (tmp, 2 * n - 1, 0, x->files[j]);
            sp_aligned_free (tmp);
          }
      }
    }
}

static int 
mpzspv_open_fileset (mpzspv_handle_t handle, const char *file_stem, 
                     const spv_size_t len)
{
  const mpzspm_t mpzspm = handle->mpzspm;
  const unsigned int sp_num = mpzspm->sp_num;
  unsigned int i;
  
#if defined(HAVE_SETVBUF) && !defined(HAVE_AIO_READ)
  handle->files = (FILE **) malloc (2 * sp_num * sizeof(FILE *));
#else
  handle->files = (FILE **) malloc (sp_num * sizeof(FILE *));
#endif
  handle->filenames = (char **) malloc (sp_num * sizeof(char *));
  if (handle->files == NULL || handle->filenames == NULL)
    {
      fprintf (stderr, "%s(): could not allocate memory\n", __func__);
      free (handle->files);
      free (handle->filenames);
      return -1;
    }
  
  for (i = 0; i < sp_num; i++)
    {
#if defined(HAVE_SETVBUF) && !defined(HAVE_AIO_READ)
      void **buffers = (void**)handle->files + sp_num;
      const size_t bufsize = 1<<22;
#endif
      
      handle->filenames[i] = (char *) malloc ((strlen(file_stem) + 10) * sizeof(char));
      if (handle->filenames[i] == NULL)
        {
          fprintf (stderr, "%s(): could not allocate memory\n", __func__);
          break;
        }

      sprintf (handle->filenames[i], "%s.%u", file_stem, i);
      handle->files[i] = fopen(handle->filenames[i], "rb+");
      if (handle->files[i] == NULL)
        handle->files[i] = fopen(handle->filenames[i], "wb+");
      if (handle->files[i] == NULL)
        {
          fprintf (stderr, "%s(): error opening %s for writing\n", 
                   __func__, handle->filenames[i]);
          free (handle->filenames[i]);
          handle->filenames[i] = NULL;
          break;
        }
#ifdef HAVE_FALLOCATE
      /* Tell the file system to allocate space for the file, avoiding 
         fragementation */
      fallocate (fileno(handle->files[i]), 0, (off_t) 0, len * sizeof(sp_t));
#endif
#ifdef HAVE_SETVBUF
#ifdef HAVE_AIO_READ
      /* Set to unbuffered mode as we use aio_*() functions for reading
         in the background */
      setvbuf (handle->files[i], NULL, _IONBF, 0);
#else
      /* Allocate a bigger buffer so accesses during conversion from/to
         mpz_t's are more sequential. We need to remember the pointers
         to the buffers so we can free them later. For now we allocate
         twice as much memory for the FILE * array and put the pointers
         to the buffers in the second half. This is ugly - FIXME */
      buffers[i] = malloc (bufsize);
      setvbuf (handle->files[i], (char *)buffers[i], _IOFBF, bufsize);
#endif
#endif
    }
  
  if (i < sp_num)
    {
      /* Some error occurred. Deallocate everything */
      while (i-- > 0)
        {
          fclose (handle->files[i]);
          handle->files[i] = NULL;
          free (handle->filenames[i]);
          handle->filenames[i] = NULL;
        }
      free (handle->filenames);
      handle->filenames = NULL;
      free (handle->files);
      handle->files = NULL;
      return -1;
    }

    return 0;
}

static void
mpzspv_close_fileset (mpzspv_handle_t handle)
{
  unsigned int i;
  const mpzspm_t mpzspm = handle->mpzspm;
#if defined(HAVE_SETVBUF) && !defined(HAVE_AIO_READ)
  void **buffers = (void**)handle->files + mpzspm->sp_num;
#endif
  
  for (i = 0; i < mpzspm->sp_num; i++)
    {
      if (fclose(handle->files[i]) != 0)
        {
          fprintf (stderr, 
                   "%s(): fclose() set error code %d\n", 
                   __func__, errno);
          abort();
        }
      
      if (remove (handle->filenames[i]) != 0)
        {
          fprintf (stderr, 
                   "%s(): remove() set error code %d\n", 
                   __func__, errno);
          abort();
        }
      free (handle->filenames[i]);
      handle->filenames[i] = NULL;
      handle->files[i] = NULL;
#if defined(HAVE_SETVBUF) && !defined(HAVE_AIO_READ)
      free (buffers[i]);
      buffers[i] = NULL;
#endif
    }
  
  free (handle->filenames);
  handle->filenames = NULL;
  free (handle->files);
  handle->files = NULL;
}


#if defined(HAVE_AIO_READ)
/* If write=0, read data from a set of files and return as soon as the 
   reads are scheduled 
   - OR - 
   if write=1, write data to a set of files and wait until writes are 
   completed (which usually returns as soon as the writes are in the 
   system's disk write cache).
   In both cases, reads/writes "len" entries from/to file position 
   "file_offset" to/from position "mpzspv_offset" in "mpzspv". All 
   lengths/positions use one sp_t as the unit. */
static int 
mpzspv_lio_rw (struct aiocb *aiocb_list[], mpzspv_t mpzspv, 
               const spv_size_t mpzspv_offset, FILE **files,  
               const spv_size_t file_offset, const spv_size_t len, 
               const mpzspm_t mpzspm, const int write)
{
  unsigned int i;
  struct sigevent sev;
  int r;
  
  if (0)
    printf("%s(, , %lu, , %lu, %lu, , %d)\n", 
           __func__, (unsigned long) mpzspv_offset, (unsigned long) file_offset,
           (unsigned long) len, write);
  
  memset (&sev, 0, sizeof(struct sigevent));
  sev.sigev_notify = SIGEV_NONE;
  for (i = 0; i < mpzspm->sp_num; i++)
    {
      memset (aiocb_list[i], 0, sizeof (struct aiocb));
      aiocb_list[i]->aio_fildes = fileno(files[i]);
      aiocb_list[i]->aio_offset = file_offset * sizeof(sp_t);
      aiocb_list[i]->aio_buf = mpzspv[i] + mpzspv_offset;
      aiocb_list[i]->aio_nbytes = len * sizeof(sp_t);
      aiocb_list[i]->aio_reqprio = 0;
      aiocb_list[i]->aio_sigevent = sev;
      aiocb_list[i]->aio_lio_opcode = write ? LIO_WRITE : LIO_READ;
    }
  r = lio_listio (write ? LIO_WAIT : LIO_NOWAIT, aiocb_list, mpzspm->sp_num, 
                  NULL);
  return r;
}

/* Wait until all operations in aiocb_list[] have completed */
static int 
mpzspv_lio_suspend (const struct aiocb *aiocb_list[], const mpzspm_t mpzspm)
{
  unsigned int i;
  for (i = 0; i < mpzspm->sp_num; i++)
    {
      int r;

      do {
        r = aio_suspend (&aiocb_list[i], 1, NULL);
      } while (r == EAGAIN);

      if (r == EINTR)
        {
          fprintf (stderr, "%s(): Got EINTR, unhandled case. FIXME\n", 
                   __func__);
          return -1;
        }

      if (r == -1)
        {
          fprintf (stderr, "%s(): Got -1, errno = %d\n", __func__, errno);
          return -1;
        }

      ASSERT_ALWAYS (r == 0);
    }
  return 0;
}
#endif

static void
mpzspv_print_mem (const mpzspv_t mpzspv, const spv_size_t offset, 
              const spv_size_t len, const char *prefix, 
              const mpzspm_t mpzspm)
{
  unsigned int i;

  if (len == 0)
    {
      printf("%s: Zero length vector\n", prefix);
      return;
    }
  
  for (i = 0; i < mpzspm->sp_num; i++)
    {
      spv_size_t j;
      printf ("%s (%lu", prefix, mpzspv[i][offset]);
      for (j = 1; j < len; j++)
        {
          printf(", %lu", mpzspv[i][offset + j]);
        }
      printf (") (mod %lu) (in memory)\n", mpzspm->spm[i]->sp);
    }
}

static void
mpzspv_print_file (FILE **files, const spv_size_t offset, 
              const spv_size_t len, const char *prefix, 
              const mpzspm_t mpzspm)
{
  unsigned int i;
  spv_t tmp;

  if (len == 0)
    {
      printf("%s: Zero length vector\n", prefix);
      return;
    }
  
  tmp = (spv_t) sp_aligned_malloc (len * sizeof (sp_t));

  for (i = 0; i < mpzspm->sp_num; i++)
    {
      spv_size_t j;
      spv_seek_and_read (tmp, len, offset, files[i]);
      printf ("%s (%lu", prefix, tmp[0]);
      for (j = 1; j < len; j++)
        {
          printf(", %lu", tmp[j]);
        }
      printf (") (mod %lu) (on disk)\n", mpzspm->spm[i]->sp);
    }

    sp_aligned_free (tmp);
}

void
mpzspv_print (mpzspv_handle_t handle, const spv_size_t offset, 
              const spv_size_t len, const char *prefix)
{
  if (IN_MEMORY(handle))
    {
      mpzspv_print_mem (handle->mem, offset, len, prefix, handle->mpzspm);
    }
  else
    {
      mpzspv_print_file (handle->files, offset, len, prefix, handle->mpzspm);
    }
}
