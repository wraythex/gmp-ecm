/* Elliptic Curve Method implementation: stage 2 routines.

  Copyright 2002, 2003 Paul Zimmermann and Alexander Kruppa.

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the
  Free Software Foundation; either version 2 of the License, or (at your
  option) any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along
  with this program; see the file COPYING.  If not, write to the Free
  Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
  02111-1307, USA.
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "gmp.h"
#include "ecm.h"

#if WANT_ASSERT
#include <assert.h>
#define ASSERT(expr)   assert (expr)
#else
#define ASSERT(expr)   do {} while (0)
#endif


#ifdef __GNUC__
#define UNUSED __attribute__ ((unused))
#else
#define UNUSED
#endif

#define PTR(x) ((x)->_mp_d)
#define getbit(x,i) (PTR(x)[i/mp_bits_per_limb] & ((mp_limb_t)1<<(i%mp_bits_per_limb)))
#define min(a,b) (((a)<(b))?(a):(b))

int multiplyW2n (mpz_t, point *, curve *, mpz_t *, unsigned int, mpmod_t, 
                 mpres_t, mpres_t, mpres_t *, unsigned long *, unsigned long *);

#if 0

/* The original point addition and doubling functions in Weierstrass
   coordinates. Not needed now as everything is done via
   addWnm() and multiplyW2n */

/*
  (x1:y1) <- 2*(x:y) where (x:y) can be identical to (x1:y1).
  a is the Weierstrass curve parameter, u and v are auxiliary variables.
  Uses 4 mul and 1 extgcd.
*/
static int
duplicateW (mpz_t p, mpres_t x1, mpres_t y1, mpres_t x, mpres_t y, mpmod_t n, 
            mpres_t a, mpres_t u, mpres_t v)
{
  mpres_add (u, y, y, n);
  if (!mpres_invert (v, u, n))
    {
      mpres_gcd (p, u, n);
      return 1;
    }
  mpres_mul (u, x, x, n);
  mpres_mul_ui (u, u, 3, n);
  mpres_add (u, u, a, n);
  mpres_mul (p, u, v, n);
  mpres_mul (u, p, p, n);
  mpres_add (v, x, x, n);
  mpres_sub (u, u, v, n);
  mpres_sub (v, x, u, n);
  mpres_mul (v, v, p, n);
  mpres_sub (y1, v, y, n);
  mpres_set (x1, u, n); /* can we avoid this mpz_set (using mpz_swap perhaps)? */

  return 0;
}

/* Computes (x:y) <- (x1:y1) + (x2:y2).
   Returns non-zero iff a factor is found (then it is stored in p).
   n is the number to factor.
   u, v are auxiliary variables.
   Uses 3 mul and 1 extgcd.
*/
static int
addW (mpz_t p, mpres_t x, mpres_t y, mpres_t x1, mpres_t y1, mpres_t x2, 
      mpres_t y2, mpmod_t n, mpres_t u, mpres_t v)
{
  mpres_sub (u, x2, x1, n);
  if (!mpres_invert (v, u, n))
    {
      mpres_gcd (p, u, n);
      return 1;
    }
  mpres_sub (p, y2, y1, n);
  mpres_mul (p, v, p, n);
  mpres_mul (u, p, p, n);
  mpres_sub (u, u, x1, n);
  mpres_sub (v, u, x2, n);
  mpres_sub (u, x1, v, n);
  mpres_mul (u, u, p, n);
  mpres_sub (y, u, y1, n);
  mpres_set (x, v, n); /* Fixme: avoid this copy! (Use commutativity) */

  return 0;
}

#endif

/* R_i <- q_i * S, 0 <= i < n, where q_i are large positive integers, S 
   is a point on an elliptic curve. Uses max(bits in q_i) modular 
   inversions (one less if max(q_i) is a power of 2).
   Needs up to n+2 cells in T. */

int
multiplyW2n (mpz_t p, point *R, curve *S, mpz_t *q, unsigned int n, 
              mpmod_t modulus, mpres_t u, mpres_t v, mpres_t *T,
              unsigned long *tot_muls, unsigned long *tot_gcds)
{
  unsigned int i, maxbit, k, /* k is the number of values to batch invert */
               l, t, muls = 0, gcds = 0;
#ifdef WANT_EXPCOST
  unsigned int hamweight = 0;
#endif

  int youpi = 0;
  mpz_t flag; /* Used as bit field, keeps track of which R[i] contain partial results */
  point s;    /* 2^t * S */

  if (n == 0)
    return 0;

  mpz_init (flag);
  mpres_init (s.x, modulus);
  mpres_init (s.y, modulus);
  mpres_set (s.x, S->x, modulus);
  mpres_set (s.y, S->y, modulus);

  /* Set maxbit to index of highest set bit among all the q[i] */
  /* Index of highest bit of q is sizeinbase(q, 2) - 1 */
  maxbit = 0;
  for (i = 0; i < n; i++)
    {
      if (mpz_sgn (q[i]) == 0)
        {
          fprintf (stderr, "multiplyW2n: multiplicand q[%d] == 0, neutral element not supported\n", i);
          exit (EXIT_FAILURE);
        }
      if (mpz_sgn (q[i]) < 0)
        {
          fprintf (stderr, "multiplyW2n: multiplicand q[%d] < 0, negatives not supported\n", i);
          exit (EXIT_FAILURE);
        }
      if ((t = mpz_sizeinbase (q[i], 2) - 1) > maxbit)
          maxbit = t;
#ifdef WANT_EXPCOST
      hamweight += mpz_popcount (q[i]) - 1;
#endif
    }

#ifdef WANT_EXPCOST
  printf ("Expecting %d multiplications and %d extgcds\n", 
          4 * (maxbit) + 6 * hamweight - 3, maxbit + 1);      /* maxbit is floor(log_2(max(q_i))) */
#endif

  for (t = 0; t <= maxbit && !youpi; t++)    /* Examine t-th bit of the q[i] */
    {
      /* See which values need inverting and put them into T[]. Keep number
         of those values in k */
      k = 0;
      
      /* Will we have to double s at the end of this pass? If yes,  
         schedule 2*s.y for inverting */
      if (t < maxbit)
        mpres_add (T[k++], s.y, s.y, modulus);
      
      for (i = 0; i < n && !youpi; i++) 
        if (mpz_tstbit (q[i], t))       /* If q[i] & (1<<t), we'll add s to R[i] */
          if (mpz_tstbit (flag, i))     /* Does R[i] contain a partial result yet ? */
            {                           /* If Yes: need actual point addition so */
              mpres_sub (T[k], s.x, R[i].x, modulus); /* schedule (s.x-R[i].x) for inverting */
              if (k > 0)
                mpres_mul (T[k], T[k], T[k - 1], modulus);
              k++;
            }                           /* If No: we'll simply set R[i] to s later on, nothing tbd here */
      
      /* So there are k values in need of inverting, call them v[m], 0 <= m < k. */      
      /* Here T[m], 0 <= m < k, contains v[0]*...*v[m] */
      
      /* Put inverse of the product of all scheduled values in T[k]*/
      if (k > 0)
        {
          muls += 3 * (k - 1);
          gcds++;
          if (!mpres_invert (T[k], T[k - 1], modulus))
            {
              /* If a factor was found, put factor in p, 
                 flag success and bail out of loop */
              mpres_gcd (p, T[k - 1], modulus);
              youpi = 1;
              break;
            }
        }
      
      /* T[k] now contains 1/(v[0]*...*v[k - 1]), 
         T[m], 0 <= m < k, still contain v[0]*...*v[m] */
      
      l = k - 1;

      for (i = n; i-- > 0; ) /* Go through the R[i] again, backwards */
        if (mpz_tstbit (q[i], t))
          {
            if (mpz_tstbit (flag, i))
              {
                /* T[k] contains 1/(v[0]*...*v[l]) */
                if (l > 0) /* need to separate the values */
                  {
                    /* T[l - 1] has v[0]*...*v[l-1] */
                    mpres_mul (T[l], T[l - 1], T[k], modulus); /* So T[l] now has 1/v[l] == 1/(s.x - R[i].x) */
                    mpres_sub (u, s.x, R[i].x, modulus);
                    mpres_mul (T[k], T[k], u, modulus);        /* T[k] now has 1/(v[0]*...*v[l - 1]) */
                  }
                else
                  {
                    /* T[k] contains 1/v[0] */
                    mpres_set (T[0], T[k], modulus); 
                  }
                
                /* 1/(s.x - R[i].x) is in T[l] */
#ifdef WANT_ASSERT
                mpres_sub (u, s.x, R[i].x, modulus);
                mpres_mul (u, u, T[l], modulus);
                mpres_get_z (p, u, modulus);
                mpz_mod (p, p, modulus->orig_modulus);
                if (mpz_cmp_ui (p, 1) != 0) 
                  gmp_printf ("ERROR: (s.x - R[%d].x) * T[%d] == %Zd\n", i, l, T[l - 1]);
#endif
                
                mpres_sub (u, s.y, R[i].y, modulus);   /* U    = y2 - y1 */
                mpres_mul (T[l], T[l], u, modulus);    /* T[l] = (y2-y1)/(x2-x1) = lambda */
                mpres_mul (u, T[l], T[l], modulus);    /* U    = lambda^2 */
                mpres_sub (u, u, R[i].x, modulus);     /* U    = lambda^2 - x1 */
                mpres_sub (R[i].x, u, s.x, modulus);   /* x3   = lambda^2 - x1 - x2 */
                mpres_sub (u, s.x, R[i].x, modulus);   /* U    = x2 - x3 */
                mpres_mul (u, u, T[l], modulus);       /* U    = lambda*(x2 - x3) */
                mpres_sub (R[i].y, u, s.y, modulus);   /* y3   = lambda*(x2 - x3) - y2 */
                muls += 3;
                l--;
              }
            else /* R[i] does not contain a partial result. */
              {
                mpres_set (R[i].x, s.x, modulus);   /* Just set R[i] to s */
                mpres_set (R[i].y, s.y, modulus);
                mpz_setbit (flag, i);               /* and flag it as used */
              }
          }
      
      if (t < maxbit) /* Double s */
        { 
          ASSERT(l==0);
#ifdef WANT_ASSERT
          mpres_add (u, s.y, s.y, modulus);
          mpres_mul (u, u, T[k], modulus);
          mpres_get_z (p, u, modulus);
          mpz_mod (p, p, modulus->orig_modulus);
          if (mpz_cmp_ui (p, 1) != 0)
            gmp_printf ("ERROR: At t==%d, 2*s.y / (2*s.y) == %Zd\n", t, p);
#endif          

                                               /* 1/(2*s.y) is in T[k] */
          mpres_mul (u, s.x, s.x, modulus);    /* U = X^2 */
          mpres_mul_ui (u, u, 3, modulus);     /* U = 3*X^2 */
          mpres_add (u, u, S->A, modulus);     /* U = 3*X^2 + A */
          mpres_mul (T[k], T[k], u, modulus);  /* T = (3*X^2 + A) / (2*Y) = lambda */
          mpres_mul (u, T[k], T[k], modulus);  /* U = lambda^2 */
          mpres_sub (u, u, s.x, modulus);      /* U = lambda^2 - X */
          mpres_sub (u, u, s.x, modulus);      /* U = lambda^2 - 2*X = s.x' */
          mpres_sub (v, s.x, u, modulus);      /* V = s.x - s.x' */
          mpres_mul (v, v, T[k], modulus);     /* V = lambda*(s.x - s.x') */
          mpres_sub (s.y, v, s.y, modulus);    /* s.y' = lambda*(s.x - s.x') - s.y */
          mpres_set (s.x, u, modulus);
          muls += 4;
        }
    }
  
  mpres_clear (s.y, modulus);
  mpres_clear (s.x, modulus);
  mpz_clear (flag);

  if (tot_muls != NULL)
    *tot_muls += muls;
  if (tot_gcds != NULL)
    *tot_gcds += gcds;
  
  return youpi;
}


/* Input: Points X[0]..X[(n+1)*m-1]
   T is used for temporary values and needs to have (n-1)*m+1 entries.

   Performs the following loop with only one gcdext, using Montgomery's trick:
   for (i=0;i<m;i++)
     for (j=0;j<n;j++) {
         res=addW(p,x[j+n*i],y[j+n*i],x[j+n*i],y[j+n*i],x[j+1+n*i],y[j+1+n*i],
                  n,u[0],v[0]);
         if (res) return(1); }
   return(0);

   Uses one inversion and 6*n*m-3 multiplications for n*m > 0
*/

static int
addWnm (mpz_t p, point *X, curve *S, mpmod_t modulus, unsigned int m, unsigned int n, 
        mpres_t *T, unsigned long *tot_muls, unsigned long *tot_gcds)
{
  unsigned int k, l;
  int i, j;

  if (n == 0 || m == 0)
    return 0;

  k = 0;
  for (i = m - 1; i >= 0; i--)    /* Go through the m different lists */
    for (j = n - 1; j >= 0; j--)  /* Go through each list backwards */
      {                           /* And prepare the values to be inverted */
        mpres_sub (T[k], X[i * (n + 1) + j + 1].x, X[i * (n + 1) + j].x, modulus);
        /* Schedule X2.x - X1.x */

        if (mpres_is_zero (T[k]))  /* If both points are identical (or the x-coordinates at least) */
          mpres_add (T[k], X[i * (n + 1) + j + 1].y, X[i * (n + 1) + j + 1].y, modulus); 
          /* Schedule 2*X[...].y instead*/

        if (k > 0)
          mpres_mul (T[k], T[k], T[k - 1], modulus);
        k++;
      }

  /* v_m = X[i * (n + 1) + j] - X[i * (n + 1) + j + 1], 0 <= j < n,
     and m = i * n + j */
  /* Here T[m] = v_0 * ... * v_m, 0 <= m < k */

  if (k > 0 && !mpres_invert (T[k], T[k - 1], modulus))
    {
      mpres_gcd (p, T[k - 1], modulus);
      (*tot_muls) += m * n - 1;
      (*tot_gcds) ++;
      return 1;
    }

  /* T[k] = 1/(v_0 * ... * v_m), 0 <= m < k */

  l = k - 1;

  for (i = 0; (unsigned) i < m; i++)
    for (j = 0; (unsigned) j < n; j++)
      {
        point *X1, *X2;
        X1 = X + i * (n + 1) + j;
        X2 = X + i * (n + 1) + j + 1;

        if (l == 0)
          mpz_set (T[0], T[k]);
        else
          mpres_mul (T[l], T[k], T[l - 1], modulus); 
          /* T_l = 1/(v_0 * ... * v_l) * (v_0 * ... * v_{l-1}) = 1/v_l */

        mpres_sub (T[k + 1], X2->x, X1->x, modulus); /* T[k+1] = v_{l} */

        if (mpres_is_zero(T[k + 1])) /* Identical points, so double X1 */
          {
            if (l > 0)
              {
                mpres_add (T[k + 1], X1->y, X1->y, modulus); /* T[k+1] = v_{l} */
                mpres_mul (T[k], T[k], T[k + 1], modulus);
                /* T_k = 1/(v_0 * ... * v_l) * v_l = 1/(v_0 * ... * v_{l-1}) */
              }
            
            mpres_mul (T[k + 1], X1->x, X1->x, modulus);
            mpres_mul_ui (T[k + 1], T[k + 1], 3, modulus);
            mpres_add (T[k + 1], T[k + 1], S->A, modulus);
            mpres_mul (T[l], T[k + 1], T[l], modulus); /* T[l] = lambda */
            mpres_mul (T[k + 1], T[l], T[l], modulus);       /* T1   = lambda^2 */
            mpres_sub (T[k + 1], T[k + 1], X1->x, modulus);  /* T1   = lambda^2 - x1 */
            mpres_sub (X1->x, T[k + 1], X2->x, modulus);     /* X1.x = lambda^2 - x1 - x2 = x3 */
            mpres_sub (T[k + 1], X2->x, X1->x, modulus);     /* T1   = x2 - x3 */
            mpres_mul (T[k + 1], T[k + 1], T[l], modulus);   /* T1   = lambda*(x2 - x3) */
            mpres_sub (X1->y, T[k + 1], X2->y, modulus);     /* Y1   = lambda*(x2 - x3) - y2 = y3 */
          }
        else
          {
            if (l > 0)
              {
                mpres_mul (T[k], T[k], T[k + 1], modulus);
                /* T_k = 1/(v_0 * ... * v_l) * v_l = 1/(v_0 * ... * v_{l-1}) */
              }

            mpres_sub (T[k + 1], X2->y, X1->y, modulus);     /* T1   = y2 - y1 */
            mpres_mul (T[l], T[l], T[k + 1], modulus);       /* Tl   = (y2 - y1) / (x2 - x1) = lambda */
            mpres_mul (T[k + 1], T[l], T[l], modulus);       /* T1   = lambda^2 */
            mpres_sub (T[k + 1], T[k + 1], X1->x, modulus);  /* T1   = lambda^2 - x1 */
            mpres_sub (X1->x, T[k + 1], X2->x, modulus);     /* X1.x = lambda^2 - x1 - x2 = x3 */
            mpres_sub (T[k + 1], X2->x, X1->x, modulus);     /* T1   = x2 - x3 */
            mpres_mul (T[k + 1], T[k + 1], T[l], modulus);   /* T1   = lambda*(x2 - x3) */
            mpres_sub (X1->y, T[k + 1], X2->y, modulus);     /* Y1   = lambda*(x2 - x3) - y2 = y3 */
          }
        
        l--;
      }

  if (tot_muls != NULL)
    (*tot_muls) += 6 * m * n - 3;
  if (tot_gcds != NULL)
    (*tot_gcds) ++;

  return 0;
}

/* puts in F[0..dF-1] the successive values of 

   Dickson_{S, a} (j) * s  where s is a point on the elliptic curve

   for j == 1 mod 6, j and d coprime.
   Returns non-zero iff a factor was found (then stored in f).
*/

int
ecm_rootsF (mpz_t f, listz_t F, unsigned int d, unsigned int dF, curve *s,
        int S, mpmod_t modulus, int verbose, unsigned long *tot_muls)
{
  unsigned int i, j, k, stepj;
  unsigned long muls = 0, gcds = 0;
  unsigned int size_fd;
  int st, st1;
  int youpi = 0, dickson_a;
  listz_t coeffs;
  point *fd;
  mpres_t *T;
  
  if (dF == 0)
    return 0;

  st = cputime ();

  mpres_get_z (F[0], s->x, modulus); /* (1*P)=P for ECM */

  if (dF > 1)
    {
      /* If S < 0, use degree |S| Dickson poly, otherwise use x^S */
      dickson_a = (S < 0) ? -1 : 0;
      S = abs (S);
      size_fd = (unsigned int) S + 1;
      
      /* We only calculate j*P where gcd (j, stepj) == 1 and j == 1 (mod 6)
         by doing eulerphi(stepj/6) separate progressions. */
      /* Now choose a value for stepj */
      stepj = 6;

      if (d / stepj > 50 && d % 5 == 0)
        {
          stepj *= 5;
          size_fd *= 4;
        }

      if (d / stepj > 100 && d % 7 == 0)
        {
          stepj *= 7;
          size_fd *= 6;
        }
      
      if (d / stepj > 500 && d % 11 == 0)
        {
          stepj *= 11;
          size_fd *= 10;
        }
      
      if (verbose >= 3)
        printf ("Computing roots where == 1 (mod 6) and coprime to %d\n", stepj);


      /* Allocate memory for fd[] and T[] */

      fd = (point *) xmalloc (size_fd * sizeof (point));
      for (j = 0 ; j < size_fd; j++)
        {
          mpres_init (fd[j].x, modulus);
          mpres_init (fd[j].y, modulus);
        }

      T = (mpres_t *) xmalloc ((size_fd + 4) * sizeof (mpres_t));
      for (j = 0 ; j < size_fd + 4; j++)
        mpres_init (T[j], modulus);
      

      /* Init finite differences tables */

      st = cputime ();
      
      coeffs = init_list (size_fd);
      j = 0;
      for (k = 1; k < stepj; k += 6)
        if (gcd (k, stepj) == 1)
          {
            fin_diff_coeff (coeffs + j, k, stepj, S, dickson_a);
            if (j > 0)
              mpz_set_ui (coeffs[j + S], 1);
            j += S + 1;
          }
#ifdef DEBUG
      if (j != size_fd)
        {
           fprintf (stderr, "ecm_rootsF: Wanted %d fd[] entries but got %d\n", size_fd, j);
           exit (EXIT_FAILURE);
        }
#endif
      if (verbose >= 4)
        for (j = 0; j < size_fd; j++)
          gmp_printf ("coeffs[%d] = %Zd\n", j, coeffs[j]);
      
      youpi = multiplyW2n (f, fd, s, coeffs, size_fd, modulus, 
                           T[0], T[1], T + 2, &muls, &gcds);
      if (youpi && verbose >= 2)
        printf ("Found factor while computing fd[] * X\n");
      
      j = S + 1;
      for (k = 7; k < stepj && !youpi; k += 6)
        if (gcd (k, stepj) == 1)
          {
            mpres_set (fd[j + S].x, fd[S].x, modulus);
            mpres_set (fd[j + S].y, fd[S].y, modulus);
            j += S + 1;
          }
      if (verbose >= 2)
        {
          st1 = cputime ();
          printf ("Initializing tables of differences for F took %dms, %lu muls and %lu extgcds\n", 
                  st1 - st, muls, gcds);
          st = st1;
          if (tot_muls != NULL)
            *tot_muls += muls;
          muls = 0;
          gcds = 0;
        }

      clear_list (coeffs, size_fd);


      /* Now for the actual calculation of the roots. k keeps track of which fd[] 
         entry to get the next root from. */

      i = 0;
      j = 1;
      k = 0;
      while (i < dF && !youpi)
        {
          if (gcd (j, stepj) == 1) /* Is this a j value where we computed f(j)*X? */
            {
              if (gcd (j, d) == 1) /* Is this a j value where we want f(j)*X? as a root of F? */
                mpres_get_z (F[i++], fd[k].x, modulus);

              k += S + 1;
              if (k == size_fd && i < dF) /* Is it time to update fd[] ? */
                {
                  unsigned int howmany = min(size_fd / (S + 1), dF - i);
                  youpi = addWnm (f, fd, s, modulus, howmany, S, T, &muls, &gcds);
                  k = 0;
                  if (youpi && verbose >= 2)
                    printf ("Found factor while computing roots of F\n");
                }
            }
          j += 6;
        }

      for (j = 0 ; j < size_fd + 4; j++)
        mpres_clear (T[j], modulus);
      free (T);
      
      for (j = 0; j < size_fd; j++)
        {
          mpres_clear (fd[j].x, modulus);
          mpres_clear (fd[j].y, modulus);
        }
      free (fd);
    }

  if (youpi)
    return 1;
  
  if (verbose >= 2)
    printf ("Computing roots of F took %dms, %ld muls and %ld extgcds\n", 
            cputime () - st, muls, gcds);

  if (tot_muls != NULL)
    *tot_muls += muls;

  return 0;
}

/* Perform the neccessary initialisation to allow computation of
   
     Dickson_{S, a}(s+n*d) * P , where P is a point on the elliptic curve
   
   for successive n, where Dickson_{S, a} is the degree S Dickson
   polynomial with parameter a. For a == 0, Dickson_{S, a} (x) = x^S.
   
   If a factor is found during the initialisation, NULL is returned and the
     factor in f.
*/


ecm_rootsG_state *
ecm_rootsG_init (mpz_t f, curve *X, double s, unsigned int d, unsigned int dF,
                 unsigned int blocks, int S, mpmod_t modulus, int verbose)
{
  unsigned int k, nr, lenT;
  unsigned long muls = 0, gcds = 0;
  listz_t coeffs;
  ecm_rootsG_state *state;
  int youpi = 0;
  int dickson_a;
  int T_inv;
  double bestnr;
  int st = 0;
  
  if (verbose >= 2)
    st = cputime ();
  
  /* If S < 0, use degree |S| Dickson poly, otherwise use x^S */
  dickson_a = (S < 0) ? -1 : 0;
  S = abs (S);

  if (modulus->repr == MOD_BASE2)
    T_inv = 18;
  else
    T_inv = 6;
  
  bestnr = -(4 + T_inv) + sqrt(12. * dF * blocks * (T_inv - 3) * 
           log (2 * d) / log (2) - (4 + T_inv) * (4 + T_inv));
  bestnr /= 6. * S * log (2 * d) / log (2);
  
  if (bestnr < 1.)
    nr = 1;
  else
    nr = (unsigned int) (bestnr + .5);

  while ((blocks * dF) % nr != 0)
    nr--;

  if (verbose >= 3)
    printf ("ecm_rootsG_init: s=%f, d=%u, S=%u, T_inv = %d, nr=%d\n", 
            s, d, S, T_inv, nr);
  
  state = (ecm_rootsG_state *) xmalloc (sizeof (ecm_rootsG_state));

  state->S = S;
  state->nr = nr;
  state->next = 0;

  state->fd = (point *) xmalloc (nr * (S + 1) * sizeof (point));
  for (k = 0; k < nr * ((unsigned) S + 1); k++)
    {
      mpres_init (state->fd[k].x, modulus);
      mpres_init (state->fd[k].y, modulus);
    }
  
  lenT = nr * (S + 1) + 4;
  state->T = (mpres_t *) xmalloc (lenT * sizeof (mpres_t));
  for (k = 0; k < (unsigned) lenT; k++)
    {
      mpres_init (state->T[k], modulus);
      mpres_init (state->T[k], modulus);
    }  

  coeffs = init_list (nr * (S + 1));
  
  for (k = 0; k < nr; k++)
    {
      fin_diff_coeff (coeffs + k * (S + 1), s + (double) k * (double) d, nr * d, S, dickson_a);
      if (verbose >= 4)
        {
          gmp_printf ("coeffs[%d][0] == %Zd\n", k, coeffs[k * (S + 1)]);
          gmp_printf ("coeffs[%d][1] == %Zd\n", k, coeffs[k * (S + 1) + 1]);
        }
      if (k > 0) /* The highest coefficient is the same for all progressions */
        mpz_set_ui (coeffs[k * (S + 1) + S], 1);
    }

  youpi = multiplyW2n (f, state->fd, X, coeffs, nr * (S + 1), modulus, 
                       state->T[0], state->T[1], state->T + 2, &muls, &gcds);

  for (k = 1; k < nr; k++)
    {
      mpres_set (state->fd[k * (S + 1) + S].x, state->fd[S].x, modulus);
      mpres_set (state->fd[k * (S + 1) + S].y, state->fd[S].y, modulus);
    }
  
  if (verbose >= 4)
    {
      for (k = 0; k < (unsigned) nr * (S + 1) && youpi == 0; k++)
        {
          printf ("ecm_rootsG_init: coeffs[%i] = ", k);
          mpz_out_str (stdout, 10, coeffs[k]);
          printf ("\n");
          printf ("fd[%u] = (", k);
          mpz_out_str (stdout, 10, state->fd[k].x);
          printf (":");
          mpz_out_str (stdout, 10, state->fd[k].y);
          printf (")\n");
        }
    }
  
  clear_list (coeffs, nr * (S + 1));
  
  if (youpi) 
    {
      if (verbose >= 2)
        printf ("Found factor while computing fd[]\n");
      
      ecm_rootsG_clear (state, S, modulus);
      
      /* Signal that a factor was found */
      state = NULL;
    } else {
      if (verbose >= 2)
        {
          st = cputime () - st;
          printf ("Initializing table of differences for G took %dms, %lu muls and %lu extgcds\n",
            st, muls, gcds);
        }
    }
  
  return state;
}

void 
ecm_rootsG_clear (ecm_rootsG_state *state, UNUSED int S, UNUSED mpmod_t modulus)
{
  unsigned int k;
  
  for (k = 0; k < state->nr * (state->S + 1); k++)
    {
      mpres_clear (state->fd[k].x, modulus);
      mpres_clear (state->fd[k].y, modulus);
    }
  free (state->fd);
  
  for (k = 0; k < state->nr * (state->S + 1) + 4; k++)
    mpres_clear (state->T[k], modulus);
  free (state->T);
  
  free (state);
}

/* Puts in G the successive values of

     Dickson_{S, a}(s+j*k) P
    
     where P is a point on the elliptic curve,
     0<= j <= d-1, k is the 'd' value from ecm_rootsG_init()
     and s is the 's' value of ecm_rootsG_init() or where a previous
     call to ecm_rootsG has left off.

   Returns non-zero iff a factor was found (then stored in f).
*/

int 
ecm_rootsG (mpz_t f, listz_t G, unsigned int d, ecm_rootsG_state *state, curve *X,
            UNUSED int Sparam, mpmod_t modulus, int verbose, unsigned long *tot_muls)
{
  unsigned int i, S;
  unsigned long muls = 0, gcds = 0;
  int youpi = 0, st;
  point *nextptr;
  
  st = cputime ();
  
  S = state->S;
  nextptr = state->fd + state->next * (S + 1);

  for (i = 0; i < d && youpi == 0; i++)
    {
      if (state->next == state->nr)
       {
          youpi = addWnm (f, state->fd, X, modulus, state->nr, S, state->T, 
                          &muls, &gcds);
          state->next = 0;
          nextptr = state->fd;
          
          if (youpi)
            {
              printf ("Found factor while computing G[]\n");
              break;
            }
        }
      
      mpres_get_z (G[i], nextptr->x, modulus);
      state->next ++;
      nextptr += S + 1;
    }
  
  if (verbose >= 2)
    printf ("Computing roots of G took %dms, %lu muls and %lu extgcd\n",
             cputime () - st, muls, gcds);
  
  if (tot_muls != NULL)
    *tot_muls += muls;
  
  return youpi;
}
