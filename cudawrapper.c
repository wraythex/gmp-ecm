#include "ecm-gpu.h"

#ifdef WITH_GPU
#include "cuda.h"

#define TWO32 4294967296 /* 2^32 */ 

extern int select_and_init_GPU (int, int, FILE*);
extern float cuda_Main (biguint_t, biguint_t, biguint_t, digit_t, biguint_t*, 
                        biguint_t*, biguint_t*, biguint_t*, mpz_t, unsigned int, 
                        unsigned int, FILE*, FILE*);

void print_factor_cofactor (mpz_t N, mpz_t factor)
{
    gmp_fprintf(stdout,"********** Factor found in step 1: %Zd\n", factor);
    if (mpz_cmp (factor, N) ==0 )
      fprintf(stdout, "Found input number N\n");
    else
    {
      mpz_t cofactor;
      mpz_init (cofactor);
    
      mpz_divexact(cofactor, N, factor);
      
      gmp_fprintf(stdout,"Found %s factor of %u digits: %Zd\n",
              mpz_probab_prime_p (factor, 5) ? "probable prime" : "composite", 
              mpz_sizeinbase (factor, 10), factor);
     
      gmp_fprintf(stdout,"%s cofactor %Zd has %u digits\n",
            mpz_probab_prime_p (cofactor, 5) ? "Probable prime" : "Composite", 
            cofactor, mpz_sizeinbase (cofactor, 10));

      mpz_clear(cofactor);
    }
}

unsigned int findfactor(mpz_t N, mpz_t xfin, mpz_t zfin)
{
  mpz_t gcd;
  mpz_init (gcd);

  mpz_gcd(gcd, zfin, N);
  
  if (mpz_cmp_ui (gcd, 1)==0)
  {
    mpz_invert (zfin, zfin, N);
    mpz_mul (xfin, xfin, zfin);
    mpz_mod (xfin, xfin, N);
      
    mpz_clear(gcd);
    return ECM_NO_FACTOR_FOUND;
  }
  else //gcd !=1 (and gcd>0 because N>0) so we found a factor
  {
    print_factor_cofactor (N, gcd);
    mpz_clear(gcd);
    return ECM_FACTOR_FOUND_STEP1;
  }
}

void to_mont_repr (mpz_t x, mpz_t n)
{
  mpz_mul_2exp (x, x, MAX_BITS);
  mpz_mod(x, x, n);
}

void from_mont_repr (mpz_t x, mpz_t n, mpz_t invB)
{
  mpz_mul(x, x, invB);
  mpz_mod(x, x, n);
}

void mpz_to_biguint (biguint_t a, mpz_t b)
{
  int i;

  for (i=0;i<NB_DIGITS;i++)
  {
#if GMP_NUMB_BITS == 32
    a[i]=mpz_getlimbn(b, i);  
#else // GMP_NUMB_BITS == 64
    if (i%2 == 0)
      a[i]=(mpz_getlimbn(b, i/2) & 0x00000000ffffffff);
    else
      a[i]=(mpz_getlimbn(b, i/2) >> 32);  
#endif
  }
}

void biguint_to_mpz (mpz_t a, biguint_t b)
{
  int i;
  
  mpz_set_ui(a, 0);

  for (i=NB_DIGITS-1;i>=0;i--)
  {
    mpz_mul_2exp(a, a, 32);
	  mpz_add_ui(a , a, b[i]);
  }
}

int gpu_ecm_stage1 (mpz_t N, mpz_t s, unsigned int number_of_curves, 
                    unsigned int firstinvd, float *gputime)
{
  int fct_ret = ECM_NO_FACTOR_FOUND;
  int ret;

  unsigned int invd;
  unsigned int i;

  mpz_t N3; /* N3 = 3*N */
  mpz_t w; /* w = 2^(SIZE_DIGIT) */
  mpz_t invN; /* invN = -N^-1 mod w */
  mpz_t invB; /* invB = 2^(-MAX_BITS) mod N ; B is w^NB_DIGITS */
  mpz_t invw; /* w^(-1) mod N */
  mpz_t M; /* (invN*N+1)/w */
  mpz_t xp, zp, x2p, z2p;

  /* The same variables but for the GPU */
  biguint_t *h_xarray, *h_zarray, *h_x2array, *h_z2array;
  digit_t h_invN;
  biguint_t h_N, h_3N, h_M;

  /*****************************/
  /* Initialize some variables */
  /*****************************/
  mpz_init (N3);
  mpz_init (invw);
  mpz_init (w);
  mpz_init (M);
  mpz_init (xp);
  mpz_init (zp);
  mpz_init (x2p);
  mpz_init (z2p);
  mpz_init (invN);
  mpz_init (invB);

  h_xarray= (biguint_t *) malloc (number_of_curves * sizeof (biguint_t));
  h_zarray= (biguint_t *) malloc (number_of_curves * sizeof (biguint_t));
  h_x2array= (biguint_t *) malloc (number_of_curves * sizeof (biguint_t));
  h_z2array= (biguint_t *) malloc (number_of_curves * sizeof (biguint_t));

  /*Some computation depending on N */
  mpz_mul_ui (N3, N, 3); /* Compute N3 = 3*N */
  mpz_ui_pow_ui (w, 2, SIZE_DIGIT); /* Compute w = 2^SIZE_DIGIT */
    
  mpz_invert (invN, N, w);
  mpz_sub (invN, w, invN); /* Compute invN = -N^-1 mod w */
   
  mpz_mul (M, invN, N);
  mpz_add_ui (M, M, 1);
  mpz_divexact (M, M, w); /* Compute M = (invN*N+1)/w */

  mpz_to_biguint (h_N, N); 
  mpz_to_biguint (h_3N, N3); 
  mpz_to_biguint (h_M, M); 
  h_invN = mpz_get_ui (invN); 
   
  mpz_ui_pow_ui (invB, 2, MAX_BITS); 
  mpz_invert (invB, invB, N); /* Compute invB = 2^(-MAX_BITS) mod N */

  mpz_invert (invw, w, N); /* Compute inw = 2^-SIZE_DIGIT % N */

  /* xp zp x2p are independent of N and the curve */
  mpz_set_ui (xp, 2);
  mpz_set_ui (zp, 1);
  mpz_set_ui (x2p, 9);

  /* Compute their Montgomery representation */
  to_mont_repr (xp, N);
  to_mont_repr (zp, N);
  to_mont_repr (x2p, N);
  
  /* for each curve, compute z2p and put xp, zp, x2p, z2p in the h_*array  */
  for (invd = firstinvd; invd < firstinvd+number_of_curves; invd++)
  {
    i = invd - firstinvd;

    mpz_mul_ui (z2p, invw, invd);
    mpz_mod (z2p, z2p, N);
    mpz_mul_2exp (z2p, z2p, 6);
    mpz_add_ui (z2p, z2p, 8);
    mpz_mod (z2p, z2p, N); /* z2p = 8+64*d */

    to_mont_repr (z2p, N);

    mpz_to_biguint(h_xarray[i], xp); 
    mpz_to_biguint(h_zarray[i], zp); 
    mpz_to_biguint(h_x2array[i], x2p); 
    mpz_to_biguint(h_z2array[i], z2p); 
  } 
 
  /* Call the wrapper function that call the GPU */
  *gputime=cuda_Main (h_N, h_3N, h_M, h_invN, h_xarray, h_zarray, h_x2array, 
                     h_z2array, s, firstinvd, number_of_curves,
                     stdout, stdout);

  /* Analyse results */
  for (invd = firstinvd; invd < firstinvd+number_of_curves; invd++)
  {
    i = invd - firstinvd;

    biguint_to_mpz(xp, h_xarray[i]); 
    biguint_to_mpz(zp, h_zarray[i]); 
    
    from_mont_repr (xp, N, invB);
    from_mont_repr (zp, N, invB);
  
    ret = findfactor (N, xp, zp);

    if (ret != ECM_NO_FACTOR_FOUND)
      fct_ret = ret;
    //if (ret==ECM_NO_FACTOR_FOUND && savefilename != NULL)
    //  write_resumefile_wrapper (savefilename, &n, B1, xp, invd, invw);
    //else if (ret==ECM_FACTOR_FOUND)
      /* Maybe print A (or NU now) for GMP-ECM */
    //  fprintf(OUTPUT_STD_VERBOSE, "Factor found with (d*2^32) mod N = %u\n", invd);
          
    }
  
  mpz_clear (N3);
  mpz_clear (invN);
  mpz_clear (invw);
  mpz_clear (w);
  mpz_clear (M);
  mpz_clear (xp);
  mpz_clear (zp);
  mpz_clear (x2p);
  mpz_clear (z2p);
  mpz_clear (invB);
  
  free ((void *) h_xarray);
  free ((void *) h_zarray);
  free ((void *) h_x2array);
  free ((void *) h_z2array);

  return fct_ret;
}
#endif

int gpu_ecm (ATTRIBUTE_UNUSED mpz_t f, ATTRIBUTE_UNUSED mpz_t N, 
             ATTRIBUTE_UNUSED mpz_t s, ATTRIBUTE_UNUSED double B1,
             ATTRIBUTE_UNUSED int device, ATTRIBUTE_UNUSED int *device_init, 
             ATTRIBUTE_UNUSED unsigned int *nb_curves, 
             ATTRIBUTE_UNUSED unsigned int firstinvd)
#ifndef WITH_GPU
{
  fprintf(stderr, "This version of libecm does not contain the GPU code.\n"
                  "You should recompile it with ./configure --enable-gpu or\n"
                  "link a version of libecm which contain the GPU code.\n");
  exit(EXIT_FAILURE);
}
#else
{
  int main_ret = ECM_NO_FACTOR_FOUND;
  long st;
  float gputime = 0.0;

  /* I think it can't fail but i'm not sure. */
  if (GMP_NUMB_BITS != 32 && GMP_NUMB_BITS != 64) 
  {
    fprintf (stderr, "GPU: Error, GMP_NUMB_BITS should be either 32 or 64.\n");
    exit (EXIT_FAILURE);
  }

  /* Check that N is not too big */
  if (mpz_sizeinbase (N, 2) > MAX_BITS-6)
  {
    fprintf (stderr, "GPU: Error, input number should be stricly lower"
                     " than 2^%d\n", MAX_BITS-6);
    exit (EXIT_FAILURE);
  }

  /* Check that N is positive and handle special cases (N=1 and N is even) */
  if (mpz_cmp_ui (N, 0) <= 0)
  {
    fprintf (stderr, "GPU: Error, input number should be positive\n");
    exit (EXIT_FAILURE);
  }
  else if (mpz_cmp_ui (N, 1) == 0)
  {
    mpz_set_ui (f, 1);
    main_ret=ECM_FACTOR_FOUND_STEP1;
    goto end_gpu_ecm;
  }
  else if (mpz_divisible_ui_p (N, 2))
  {
    mpz_set_ui (f, 2);
    main_ret=ECM_FACTOR_FOUND_STEP1;
    goto end_gpu_ecm;
  }
  
  
  if (!*device_init)
    {
      st = cputime ();
      *nb_curves = select_and_init_GPU (device, *nb_curves, stdout);
      fprintf(stdout, "Selection and initialization of the device took %ldms\n", 
                      elltime (st, cputime ()));
      /* TRICKS: If initialization of the device is too long (few seconds), */
      /* try running 'nvidia-smi -q -l' on the background .                 */
      *device_init = 1;
    }
  fprintf (stdout,"TMP for debug: number_of_curves=%d\n", *nb_curves);

  if (firstinvd < 2 || firstinvd > TWO32-*nb_curves)
  {
    /* firstinvd is in fact firstnu (almost because the GPU is a 32-bit machine
     * so on 64-bit machine this is not exactly the case). TODO changefirstinv
     * in firstinvnu everywhere. */
    fprintf(stderr,"GPU: Error, firstinvd should be bigger than 2 and " 
                   "smaller than %lu.\n", TWO32-*nb_curves);
    exit(EXIT_FAILURE);
  }
  
  /* TODO: before beginning stage1: 
            print B1, firstinv, nb_curves (modify a little 
              print_B1_B2_poly) 
  print_B1_B2_poly (int verbosity, int method, double B1, double B1done, 
		  mpz_t B2min_param, mpz_t B2min, mpz_t B2, int S, mpz_t parameter,
		  int parameter_is_A, mpz_t go, int batch)
  In the meantime, we use this temporary printf:
  */
  gmp_fprintf (stdout, "Using B1=%1.0f, firstinvd=%u, with %u curves\n", 
                                          B1, firstinvd, *nb_curves);
  

  gpu_ecm_stage1 (N, s, *nb_curves, firstinvd, &gputime);

  fprintf (stdout, "time GPU: %.3fs\n", (gputime/1000));
  fprintf (stdout, "Throughput: %.3f\n", 1000 * (*nb_curves)/gputime);

  /* TODO: print CPU time for stage1 */
  /* TODO: Do stage2 (compute the parameters first (like in ecm.c) */
  /* TODO: Save in a file if requested */


  end_gpu_ecm:

  return main_ret;
}
#endif



