/* Implements algorithm POLYEVAL.

  Copyright 2003 Paul Zimmermann and Alexander Kruppa.

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

#include "gmp.h"
#include "ecm.h"

/* algorithm POLYEVAL from section 3.7 of Peter Montgomery's dissertation.
Input: 
   G - an array of k elements of R, G[i], 0 <= i < k
       representing the coefficients of a polynomial G(x) of degree < k
   Tree - the product tree produced by PolyFromRoots
   Tree[0][0..k-1] (degree k/2)
   Tree[1][0..k-1] (degree k/4), ...,
   Tree[lgk-1][0..k-1] (degree 1)
Output: the sequence of values of G(a[i]) are stored in G[i] for 0 <= i < k
Return value: number of multiplications used.
Remark: we need an auxiliary (k+1)-th cell G[k] in G.
The memory used is M(k) = max(3*floor(k/2)+list_mul_mem(floor(k/2)),
                              k+list_mul_mem(ceil(k/2)),
                              floor(k/2) + M(ceil(k/2))).
Since list_mul_mem(k) >= 2*k, the maximum is the 1st.
*/
unsigned int
polyeval (listz_t G, unsigned int k, listz_t *Tree, listz_t T, mpz_t n,
          int verbose, unsigned int sh)
{
  unsigned int l, m, muls;
  listz_t T0;

  if (k == 1)
    return 0;

  T0 = Tree[0] + sh;
  
  m = k / 2;
  l = k - m;

  /* divide G[0]+G[1]*x+...+G[k-1]*x^(k-1) by
            T0[l]+...+T0[k-1]*x^(m-1)+x^m,
            quotient in {T+m,l-1}, remainder in {T,m} */

  if (k == 2 * m)
    {
      /* FIXME: avoid the copy here by giving different 2nd and 3rd arguments
         to RecursiveDivision */
      list_set (T, G, k);
      /* the following needs k+m+list_mul_mem(m) in T */
      muls = RecursiveDivision (T + k, T, T0 + l, m, T + k + m, n, 1);
    }
  else /* k = 2m+1: subtract G[k-1]*x^(l-1) * T0 from G */
    {
      /* G - G[k-1] * (x^m + {T0+l,m}) * x^m */
      list_set (T, G, m);
      list_mul_z (T + m, T0 + l, G[k - 1], m, n);
      muls = m;
      list_sub (T + m, G + m, T + m, m);
      /* the following needs 3m+list_mul_mem(m) in T */
      muls += RecursiveDivision (T + 2 * m, T, T0 + l, m, T + 3 * m, n, 1);
    }
  /* in both cases we need 3*(k/2)+list_mul_mem(k/2) */

  /* right remainder is in {T,m} */

  /* k = 2l or k = 2l-1 */
  
  /* divide G[0]+G[1]*x+...+G[k-1]*x^(k-1) by
            T0[0]+...+T0[l-1]*x^(l-1)+x^l:
            quotient in {T+m,m-1}, remainder in {G,l} */

  if (k < 2 * l)
    mpz_set_ui (G[k], 0);
  /* the following needs k+list_mul_mem(l) in T */
  muls += RecursiveDivision (T + m, G, T0, l, T + k, n, 1);

  /* left remainder is in {G,l} */
  
  muls += polyeval (G, l, Tree + 1, T + m, n, verbose, sh);

  /* copy right remainder in {G+l,m} */
  list_set (G + l, T, m);
  muls += polyeval (G + l, m, Tree + 1, T, n, verbose, sh + l);

  return muls;
}
