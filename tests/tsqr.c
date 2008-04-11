/* tsqr -- test file for mpc_sqr.

Copyright (C) 2002, 2005, 2008 Andreas Enge, Paul Zimmermann

This file is part of the MPC Library.

The MPC Library is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or (at your
option) any later version.

The MPC Library is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
License for more details.

You should have received a copy of the GNU Lesser General Public License
along with the MPC Library; see the file COPYING.LIB.  If not, write to
the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
MA 02111-1307, USA. */

#include <stdio.h>
#include <stdlib.h>
#include "gmp.h"
#include "mpfr.h"
#include "mpc.h"
#include "mpc-impl.h"

void cmpsqr (mpc_srcptr, mpc_rnd_t);
void testsqr (long, long, mp_prec_t, mpc_rnd_t);
void special (void);


void cmpsqr (mpc_srcptr x, mpc_rnd_t rnd)
   /* computes the square of x with the specific function or by simple     */
   /* multiplication using the rounding mode rnd and compares the results  */
   /* and return values.                                                   */
   /* In our current test suite, the real and imaginary parts of x have    */
   /* the same precision, and we use this precision also for the result.   */
   /* Furthermore, we check whether computing the square in the same       */
   /* place yields the same result.                                        */
   /* We also compute the result with four times the precision and check   */
   /* whether the rounding is correct. Error reports in this part of the   */
   /* algorithm might still be wrong, though, since there are two          */
   /* consecutive roundings.                                               */
{
  mpc_t z, t, u;
  int   inexact_z, inexact_t;

  mpc_init2 (z, MPC_MAX_PREC (x));
  mpc_init2 (t, MPC_MAX_PREC (x));
  mpc_init2 (u, 4 * MPC_MAX_PREC (x));

  inexact_z = mpc_sqr (z, x, rnd);
  inexact_t = mpc_mul (t, x, x, rnd);

  if (mpc_cmp (z, t))
    {
      fprintf (stderr, "sqr and mul differ for rnd=(%s,%s) \nx=",
               mpfr_print_rnd_mode(MPC_RND_RE(rnd)),
               mpfr_print_rnd_mode(MPC_RND_IM(rnd)));
      mpc_out_str (stderr, 2, 0, x, MPC_RNDNN);
      fprintf (stderr, "\nmpc_sqr gives ");
      mpc_out_str (stderr, 2, 0, z, MPC_RNDNN);
      fprintf (stderr, "\nmpc_mul gives ");
      mpc_out_str (stderr, 2, 0, t, MPC_RNDNN);
      fprintf (stderr, "\n");
      exit (1);
    }
  if (inexact_z != inexact_t)
    {
      fprintf (stderr, "The return values of sqr and mul differ for rnd=(%s,%s) \nx=  ",
               mpfr_print_rnd_mode(MPC_RND_RE(rnd)),
               mpfr_print_rnd_mode(MPC_RND_IM(rnd)));
      mpc_out_str (stderr, 2, 0, x, MPC_RNDNN);
      fprintf (stderr, "\nx^2=");
      mpc_out_str (stderr, 2, 0, z, MPC_RNDNN);
      fprintf (stderr, "\nmpc_sqr gives %i", inexact_z);
      fprintf (stderr, "\nmpc_mul gives %i", inexact_t);
      fprintf (stderr, "\n");
      exit (1);
    }

  mpc_set (t, x, MPC_RNDNN);
  inexact_t = mpc_sqr (t, t, rnd);
  if (mpc_cmp (z, t))
    {
      fprintf (stderr, "sqr and sqr in place differ for rnd=(%s,%s) \nx=",
               mpfr_print_rnd_mode(MPC_RND_RE(rnd)),
               mpfr_print_rnd_mode(MPC_RND_IM(rnd)));
      mpc_out_str (stderr, 2, 0, x, MPC_RNDNN);
      fprintf (stderr, "\nmpc_sqr          gives ");
      mpc_out_str (stderr, 2, 0, z, MPC_RNDNN);
      fprintf (stderr, "\nmpc_sqr in place gives ");
      mpc_out_str (stderr, 2, 0, t, MPC_RNDNN);
      fprintf (stderr, "\n");
      exit (1);
    }
  if (inexact_z != inexact_t)
    {
      fprintf (stderr, "The return values of sqr and sqr in place differ for rnd=(%s,%s) \nx=  ",
               mpfr_print_rnd_mode(MPC_RND_RE(rnd)),
               mpfr_print_rnd_mode(MPC_RND_IM(rnd)));
      mpc_out_str (stderr, 2, 0, x, MPC_RNDNN);
      fprintf (stderr, "\nx^2=");
      mpc_out_str (stderr, 2, 0, z, MPC_RNDNN);
      fprintf (stderr, "\nmpc_sqr          gives %i", inexact_z);
      fprintf (stderr, "\nmpc_sqr in place gives %i", inexact_t);
      fprintf (stderr, "\n");
      exit (1);
    }

  mpc_sqr (u, x, rnd);
  mpc_set (t, u, rnd);
  if (mpc_cmp (z, t))
    {
      fprintf (stderr, "rounding in sqr might be incorrect for rnd=(%s,%s) \nx=",
               mpfr_print_rnd_mode(MPC_RND_RE(rnd)),
               mpfr_print_rnd_mode(MPC_RND_IM(rnd)));
      mpc_out_str (stderr, 2, 0, x, MPC_RNDNN);
      fprintf (stderr, "\nmpc_sqr                     gives ");
      mpc_out_str (stderr, 2, 0, z, MPC_RNDNN);
      fprintf (stderr, "\nmpc_sqr quadruple precision gives ");
      mpc_out_str (stderr, 2, 0, u, MPC_RNDNN);
      fprintf (stderr, "\nand is rounded to                 ");
      mpc_out_str (stderr, 2, 0, t, MPC_RNDNN);
      fprintf (stderr, "\n");
      exit (1);
    }

  mpc_clear (z);
  mpc_clear (t);
}


void
testsqr (long a, long b, mp_prec_t prec, mpc_rnd_t rnd)
{
  mpc_t x;

  mpc_init2 (x, prec);

  mpc_set_si_si (x, a, b, rnd);

  cmpsqr (x, rnd);

  mpc_clear (x);
}


void
special ()
{
  mpc_t x, z;
  int inexact;

  mpc_init (x);
  mpc_init (z);

  mpc_set_prec (x, 8);
  mpc_set_prec (z, 8);
  mpc_set_si_si (x, 4, 3, MPC_RNDNN);
  inexact = mpc_sqr (z, x, MPC_RNDNN);
  if (MPC_INEX_RE(inexact) || MPC_INEX_IM(inexact))
    {
      fprintf (stderr, "Error: (4+3*I)^2 should be exact with prec=8\n");
      exit (1);
    }

  mpc_set_prec (x, 27);
  mpfr_set_str (MPC_RE(x), "1.11111011011000010101000000e-2", 2, GMP_RNDN);
  mpfr_set_str (MPC_IM(x), "1.11010001010110111001110001e-3", 2, GMP_RNDN);

  cmpsqr (x, 0);

  mpc_clear (x);
  mpc_clear (z);
}


int
main()
{
  mpc_t x;
  mpc_rnd_t rnd_re, rnd_im;
  mp_prec_t prec;
  int i;

  special ();

  testsqr (247, -65, 8, 24);
  testsqr (5, -896, 3, 2);
  testsqr (-3, -512, 2, 16);
  testsqr (266013312, 121990769, 27, 0);
  testsqr (170, 9, 8, 0);
  testsqr (768, 85, 8, 16);
  testsqr (145, 1816, 8, 24);
  testsqr (0, 1816, 8, 24);
  testsqr (145, 0, 8, 24);

  mpc_init (x);

  for (prec = 2; prec < 1000; prec++)
    {
      mpc_set_prec (x, prec);

      for (i = 0; i < (int) (1000/prec); i++)
        {
          mpc_random (x);

          for (rnd_re = 0; rnd_re < 4; rnd_re ++)
            for (rnd_im = 0; rnd_im < 4; rnd_im ++)
              cmpsqr (x, RNDC(rnd_re, rnd_im));
        }
    }

  mpc_clear (x);

  return 0;
}