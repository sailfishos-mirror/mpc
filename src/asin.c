/* mpc_asin -- arcsine of a complex number.

Copyright (C) 2009, 2010, 2011, 2012, 2013, 2014, 2020, 2022, 2024 INRIA

This file is part of GNU MPC.

GNU MPC is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 3 of the License, or (at your
option) any later version.

GNU MPC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this program. If not, see http://www.gnu.org/licenses/ .
*/

/* References:
   [1] MPC: Algorithms and Error Analysis, Andreas Enge, Philippe Théveny,
       Paul Zimmermann, see mpc/doc/algorithms.pdf.
*/

#include <stdio.h>
#include <limits.h> /* for ULONG_MAX */
#include "mpc-impl.h"

/* Special case op = 1 + i*y for tiny y (see algorithms.tex).
   Return 0 if special formula fails, otherwise put in z1 the approximate
   value which needs to be converted to rop.
   z1 is a temporary variable with enough precision.
 */
static int
mpc_asin_special (mpc_ptr rop, mpc_srcptr op, mpc_rnd_t rnd, mpc_ptr z1)
{
  mpfr_exp_t ey = mpfr_get_exp (mpc_imagref (op));
  mpfr_t abs_y;
  mpfr_prec_t p;
  int inex;

  /* |Re(asin(1+i*y)) - pi/2| <= y^(1/2) */
  if (ey >= 0 || ((-ey) / 2 < mpfr_get_prec (mpc_realref (z1))))
    return 0;

  mpfr_const_pi (mpc_realref (z1), MPFR_RNDN);
  mpfr_div_2exp (mpc_realref (z1), mpc_realref (z1), 1, MPFR_RNDN); /* exact */
  p = mpfr_get_prec (mpc_realref (z1));
  /* if z1 has precision p, the error on z1 is 1/2*ulp(z1) = 2^(-p) so far,
     and since ey <= -2p, then y^(1/2) <= 1/2*ulp(z1) too, thus the total
     error is bounded by ulp(z1) */
  if (!mpfr_can_round (mpc_realref(z1), p, MPFR_RNDN, MPFR_RNDZ,
                       mpfr_get_prec (mpc_realref(rop)) +
                       (MPC_RND_RE(rnd) == MPFR_RNDN)))
    return 0;

  /* |Im(asin(1+i*y)) - y^(1/2)| <= (1/12) * y^(3/2) for y >= 0 (err >= 0)
     |Im(asin(1-i*y)) + y^(1/2)| <= (1/12) * y^(3/2) for y >= 0 (err <= 0) */
  abs_y[0] = mpc_imagref (op)[0];
  if (mpfr_signbit (mpc_imagref (op)))
    MPFR_CHANGE_SIGN (abs_y);
  inex = mpfr_sqrt (mpc_imagref (z1), abs_y, MPFR_RNDN); /* error <= 1/2 ulp */
  if (mpfr_signbit (mpc_imagref (op)))
    MPFR_CHANGE_SIGN (mpc_imagref (z1));
  /* If z1 has precision p, the error on z1 is 1/2*ulp(z1) = 2^(-p) so far,
     and (1/12) * y^(3/2) <= (1/8) * y * y^(1/2) <= 2^(ey-3)*2^p*ulp(y^(1/2))
     thus for p+ey-3 <= -1 we have (1/12) * y^(3/2) <= (1/2) * ulp(y^(1/2)),
     and the total error is bounded by ulp(z1).
     Note: if y^(1/2) is exactly representable, and ends with many zeroes,
     then mpfr_can_round below will fail; however in that case we know that
     Im(asin(1+i*y)) is away from +/-y^(1/2) wrt zero. */
  if (inex == 0) /* enlarge im(z1) so that the inexact flag is correct */
    {
      if (mpfr_signbit (mpc_imagref (op)))
        mpfr_nextbelow (mpc_imagref (z1));
      else
        mpfr_nextabove (mpc_imagref (z1));
      return 1;
    }
  p = mpfr_get_prec (mpc_imagref (z1));
  if (!mpfr_can_round (mpc_imagref(z1), p - 1, MPFR_RNDA, MPFR_RNDZ,
                      mpfr_get_prec (mpc_imagref(rop)) +
                      (MPC_RND_IM(rnd) == MPFR_RNDN)))
    return 0;
  return 1;
}

/* Put in s an approximation of asin(z) using:
   asin z = z + 1/2*z^3/3 + (1*3)/(2*4)*z^5/5 + ...
   Assume |Re(z)|, |Im(z)| < 1/2.
   Return non-zero if we can get the correct result by rounding s:
   mpc_set (rop, s, ...) */
static int
mpc_asin_series (mpc_srcptr rop, mpc_ptr s, mpc_srcptr z, mpc_rnd_t rnd)
{
  mpc_t w, t;
  unsigned long k, err, kx, ky;
  mpfr_prec_t p;
  mpfr_exp_t ex, ey, e;
  int tiny = 0; // does asin(z) round to z?

  /* assume z = (x,y) with |x|,|y| < 2^(-e) with e >= 1, see the error
     analysis in algorithms.tex */
  ex = mpfr_get_exp (mpc_realref (z));
  MPC_ASSERT(ex <= -1);
  ey = mpfr_get_exp (mpc_imagref (z));
  MPC_ASSERT(ey <= -1);

  p = mpfr_get_prec (mpc_realref (s)); /* working precision */
  MPC_ASSERT(mpfr_get_prec (mpc_imagref (s)) == p);

  mpc_init2 (w, p);
  mpc_init2 (t, p);
  mpc_set (s, z, MPC_RNDNN);
  mpc_sqr (w, z, MPC_RNDNN);
  mpc_set (t, z, MPC_RNDNN);
  for (k = 1; ; k++)
    {
      mpfr_exp_t exp_x, exp_y;
      mpc_mul (t, t, w, MPC_RNDNN);
      mpc_mul_ui (t, t, (2 * k - 1) * (2 * k - 1), MPC_RNDNN);
      mpc_div_ui (t, t, (2 * k) * (2 * k + 1), MPC_RNDNN);
      exp_x = mpfr_get_exp (mpc_realref (s));
      exp_y = mpfr_get_exp (mpc_imagref (s));
      if (mpfr_get_exp (mpc_realref (t)) < exp_x - p &&
          mpfr_get_exp (mpc_imagref (t)) < exp_y - p)
        /* Re(t) < 1/2 ulp(Re(s)) and Im(t) < 1/2 ulp(Im(s)),
           thus adding t to s will not change s */
        break;
      mpc_add (s, s, t, MPC_RNDNN);
    }
  mpc_clear (w);
  mpc_clear (t);
  if (mpc_cmp (s, z) == 0) {
    /* If s=z, we used only the first term of the Taylor expansion,
       thus asin(z) rounds to z, with error term of the sign of t.
       We sligthly modify the real/imaginary parts of s so that
       mpfr_can_round does not fail, and to get the correct ternary value. */
    if (mpfr_signbit (mpc_realref (t)) == 0)
      mpfr_nextabove (mpc_realref (s));
    else
      mpfr_nextbelow (mpc_realref (s));
    if (mpfr_signbit (mpc_imagref (t)) == 0)
      mpfr_nextabove (mpc_imagref (s));
    else
      mpfr_nextbelow (mpc_imagref (s));
    tiny = 1;
  }
  /* check (2k-1)^2 is exactly representable */
  MPC_ASSERT(2 * k - 1 <= ULONG_MAX / (2 * k - 1));
  /* maximal absolute error on Re(s) is:
     (5k-3)k/2*2^(-1-p) for ex=-1
     5k/2*2^(ex-p) for ex <= -2 */
  if (ex == 1)
    {
      MPC_ASSERT(5 * k - 3 <= ULONG_MAX / k);
      kx = (5 * k - 3) * k;
    }
  else
    kx = 5 * k;
  kx = (kx + 1) / 2; /* takes into account the 1/2 factor in both cases */
  /* now (5k-3)k/2 <= kx for ex=-1, and 5k/2 <= kx for ex <= -2, thus
     the maximal absolute error on Re(s) is bounded by kx*2^(ex-p) */

  /* for the real part, convert the maximal absolute error kx*2^(ex-p) into
     relative error */
  e = mpfr_get_exp (mpc_realref (s));
  /* ulp(Re(s)) = 2^(e-p) */
  /* the relative error is kx*2^(ex-e) */
  err = ex - e;
  /* now the rounding error is bounded by kx*2^err*ulp(Re(s)), add the
     mathematical error which is bounded by ulp(Re(s)): the first neglected
     term is less than 1/2*ulp(Re(s)), and each term decreases by at least
     a factor 2, since |z^2| <= 1/2. */
  kx++;
  for (; kx > 1; err++, kx = (kx + 1) / 2);
  err = (err < 0) ? 0 : err;
  /* can we round Re(s) with error less than 2^(EXP(Re(s))-err) ? */
  if (!tiny && !mpfr_can_round (mpc_realref (s), p - err, MPFR_RNDN, MPFR_RNDZ,
                                mpfr_get_prec (mpc_realref (rop)) +
                                (MPC_RND_RE(rnd) == MPFR_RNDN)))
    return 0;

  /* same for the imaginary part */
  if (ey == 1)
    {
      MPC_ASSERT(5 * k - 3 <= ULONG_MAX / k);
      ky = (5 * k - 3) * k;
    }
  else
    ky = 5 * k;
  ky = (ky + 1) / 2; /* takes into account the 1/2 factor in both cases */
  /* now (5k-3)k/2 <= ky for ey=-1, and 5k/2 <= ky for ey <= -2, thus
     the maximal absolute error on Im(s) is bounded by ky*2^(ey-p) */

  /* convert the maximal absolute error ky*2^(ey-p) into relative error */
  e = mpfr_get_exp (mpc_imagref (s));
  /* ulp(Im(s)) = 2^(e-p) */
  /* the relative error is ky*2^(ey-e) */
  err = ey - e;
  /* now the rounding error is bounded by ky*ulp(Im(s)), add the
     mathematical error which is bounded by ulp(Im(s)): the first neglected
     term is less than 1/2*ulp(Im(s)), and each term decreases by at least
     a factor 2, since |z^2| <= 1/2. */
  ky++;
  for (err = 0; ky > 1; err++, ky = (ky + 1) / 2);
  err = (err < 0) ? 0 : err;
  /* can we round Im(s) with error less than 2^(EXP(Im(s))-err) ? */
  if (!tiny && !mpfr_can_round (mpc_imagref (s), p - err, MPFR_RNDN, MPFR_RNDZ,
                                mpfr_get_prec (mpc_imagref (rop)) +
                                (MPC_RND_IM(rnd) == MPFR_RNDN)))
    return 0;
  return 1;
}

/* Try to get correct rounding for large |z| with Im(z) < 0,
   where t is an auxiliary variable with the same precision for both parts.
   Assume |z| >= 2 and prec(t) >= 4. */
static int
mpc_asin_large (mpc_srcptr rop, mpc_ptr t, mpc_srcptr z, mpc_rnd_t rnd)
{
  mpfr_prec_t p = mpfr_get_prec (mpc_realref (t));
  /* mpc_asin_large() is called with a variable t having the
     same precision for its real and imaginary parts */
  MPC_ASSERT(p >= 4);

  /* See the error analysis in algorithms.tex. */
  mpfr_exp_t ex = mpfr_get_exp (mpc_realref (z));
  mpfr_exp_t ey = mpfr_get_exp (mpc_imagref (z));
  mpfr_exp_t ez = (ex >= ey) ? ex : ey;
  if (2 * ez < p + 1)
    return 0;
  // t=2*i*z
  mpc_mul_2ui (t, z, 1, MPC_RNDNN); // 2*z
  // multiply by i
  mpfr_swap (mpc_realref (t), mpc_imagref (t));
  MPFR_CHANGE_SIGN (mpc_realref (t));
  mpc_log (t, t, MPC_RNDNN);
  // multiply by -i
  mpfr_swap (mpc_realref (t), mpc_imagref (t));
  MPFR_CHANGE_SIGN (mpc_imagref (t));
  // the error bound on both parts is 3 ulps thus less than 2^2 ulps
  int ok = mpfr_can_round (mpc_realref (t), p - 2, MPFR_RNDN, MPFR_RNDZ,
                           mpfr_get_prec (mpc_realref (rop)) + (MPC_RND_RE(rnd) == MPFR_RNDN));
  ok = ok && mpfr_can_round (mpc_imagref (t), p - 2, MPFR_RNDN, MPFR_RNDZ,
                             mpfr_get_prec (mpc_imagref (rop)) + (MPC_RND_IM(rnd) == MPFR_RNDN));
  return ok;
}

int
mpc_asin (mpc_ptr rop, mpc_srcptr op, mpc_rnd_t rnd)
{
  mpfr_prec_t p, p_re, p_im;
  mpfr_rnd_t rnd_re, rnd_im;
  mpc_t z1;
  int inex, inex_re, inex_im, loop = 0;
  mpfr_exp_t saved_emin, saved_emax, err, olderr, ey0;

  /* case Re(op)=NaN or Im(op)=NaN */
  if (mpfr_nan_p (mpc_realref (op)) || mpfr_nan_p (mpc_imagref (op)))
    {
      if (mpfr_inf_p (mpc_realref (op)) || mpfr_inf_p (mpc_imagref (op)))
        {
          mpfr_set_nan (mpc_realref (rop));
          mpfr_set_inf (mpc_imagref (rop), mpfr_signbit (mpc_imagref (op)) ? -1 : +1);
        }
      else if (mpfr_zero_p (mpc_realref (op)))
        {
          mpfr_set (mpc_realref (rop), mpc_realref (op), MPFR_RNDN);
          mpfr_set_nan (mpc_imagref (rop));
        }
      else
        {
          mpfr_set_nan (mpc_realref (rop));
          mpfr_set_nan (mpc_imagref (rop));
        }

      return 0;
    }

  /* case Re(op)=Inf or Im(op)=Inf */
  if (mpfr_inf_p (mpc_realref (op)) || mpfr_inf_p (mpc_imagref (op)))
    {
      if (mpfr_inf_p (mpc_realref (op)))
        {
          int inf_im = mpfr_inf_p (mpc_imagref (op));

          inex_re = set_pi_over_2 (mpc_realref (rop),
             (mpfr_signbit (mpc_realref (op)) ? -1 : 1), MPC_RND_RE (rnd));
          mpfr_set_inf (mpc_imagref (rop), (mpfr_signbit (mpc_imagref (op)) ? -1 : 1));

          if (inf_im)
            mpfr_div_2ui (mpc_realref (rop), mpc_realref (rop), 1, MPFR_RNDN);
        }
      else
        {
          mpfr_set_zero (mpc_realref (rop), (mpfr_signbit (mpc_realref (op)) ? -1 : 1));
          inex_re = 0;
          mpfr_set_inf (mpc_imagref (rop), (mpfr_signbit (mpc_imagref (op)) ? -1 : 1));
        }

      return MPC_INEX (inex_re, 0);
    }

  /* pure real argument */
  if (mpfr_zero_p (mpc_imagref (op)))
    {
      int s_im;
      s_im = mpfr_signbit (mpc_imagref (op));

      if (mpfr_cmp_ui (mpc_realref (op), 1) > 0)
        {
          if (s_im)
            inex_im = -mpfr_acosh (mpc_imagref (rop), mpc_realref (op),
                                   INV_RND (MPC_RND_IM (rnd)));
          else
            inex_im = mpfr_acosh (mpc_imagref (rop), mpc_realref (op),
                                  MPC_RND_IM (rnd));
          inex_re = set_pi_over_2 (mpc_realref (rop),
             (mpfr_signbit (mpc_realref (op)) ? -1 : 1), MPC_RND_RE (rnd));
          if (s_im)
            mpc_conj (rop, rop, MPC_RNDNN);
        }
      else if (mpfr_cmp_si (mpc_realref (op), -1) < 0)
        {
          mpfr_t minus_op_re;
          minus_op_re[0] = mpc_realref (op)[0];
          MPFR_CHANGE_SIGN (minus_op_re);

          if (s_im)
            inex_im = -mpfr_acosh (mpc_imagref (rop), minus_op_re,
                                   INV_RND (MPC_RND_IM (rnd)));
          else
            inex_im = mpfr_acosh (mpc_imagref (rop), minus_op_re,
                                  MPC_RND_IM (rnd));
          inex_re = set_pi_over_2 (mpc_realref (rop),
             (mpfr_signbit (mpc_realref (op)) ? -1 : 1), MPC_RND_RE (rnd));
          if (s_im)
            mpc_conj (rop, rop, MPC_RNDNN);
        }
      else
        {
          inex_im = mpfr_set_ui (mpc_imagref (rop), 0, MPC_RND_IM (rnd));
          if (s_im)
            mpfr_neg (mpc_imagref (rop), mpc_imagref (rop), MPFR_RNDN);
          inex_re = mpfr_asin (mpc_realref (rop), mpc_realref (op), MPC_RND_RE (rnd));
        }

      return MPC_INEX (inex_re, inex_im);
    }

  /* pure imaginary argument */
  if (mpfr_zero_p (mpc_realref (op)))
    {
      int s;
      s = mpfr_signbit (mpc_realref (op));
      mpfr_set_ui (mpc_realref (rop), 0, MPFR_RNDN);
      if (s)
        mpfr_neg (mpc_realref (rop), mpc_realref (rop), MPFR_RNDN);
      inex_im = mpfr_asinh (mpc_imagref (rop), mpc_imagref (op), MPC_RND_IM (rnd));

      return MPC_INEX (0, inex_im);
    }

  saved_emin = mpfr_get_emin ();
  saved_emax = mpfr_get_emax ();
  mpfr_set_emin (mpfr_get_emin_min ());
  mpfr_set_emax (mpfr_get_emax_max ());

  /* regular complex: asin(z) = -i*log(i*z+sqrt(1-z^2)) (formula 4.4.26 in Abramowitz & Stegun) */
  rnd_re = MPC_RND_RE(rnd);
  rnd_im = MPC_RND_IM(rnd);

  p_re = mpfr_get_prec (mpc_realref(rop));
  p_im = mpfr_get_prec (mpc_imagref(rop));
  p = p_re >= p_im ? p_re : p_im;

  /* if Re(z)=1 and Im(z) is tiny, we have a cancellation */
  if (mpfr_cmp_ui (mpc_realref(op), 1) == 0 && ey0 < 0)
    p += -2 * ey0;

  mpc_t z;
  // "copy" the input into a local copy z
  mpc_realref (z)[0] = mpc_realref (op)[0];
  mpc_imagref (z)[0] = mpc_imagref (op)[0];

  /* if z is near the imaginary axis, we have a cancellation in
     i*z + sqrt(1-z^2) when Im(z) > 0, thus we impose Im(z) < 0,
     and use the symmetry asin(-z) = -asin(z). */
  int neg = mpfr_sgn (mpc_imagref (op)) > 0;
  if (neg) {
    MPFR_CHANGE_SIGN (mpc_realref (z));
    MPFR_CHANGE_SIGN (mpc_imagref (z));
  }

  mpc_init2 (z1, p);
  while (1)
  {
    mpfr_exp_t ex, ey, err_x, err_y, abs_ex, abs_ey, abs_e;
    int large_z, dif, lem_sqrt, lem_log;

    MPC_LOOP_NEXT(loop, op, rop);
    p += (loop <= 2) ? mpc_ceil_log2 (p) + 3 : p / 2; // ensures p>=4 in mpc_asin_large()
    mpc_set_prec (z1, p);

    /* try special code for 1+i*y with tiny y */
    if (loop == 1 && mpfr_cmp_ui (mpc_realref(z), 1) == 0 &&
        mpc_asin_special (rop, z, rnd, z1))
      break;

    /* try special code for small z */
    if (mpfr_get_exp (mpc_realref (z)) <= -1 &&
        mpfr_get_exp (mpc_imagref (z)) <= -1 &&
        mpc_asin_series (rop, z1, z, rnd))
      break;

    /* try special code for large z, requires |z| >= 2 */
    large_z = mpfr_get_exp (mpc_realref (z)) >= 2 || mpfr_get_exp (mpc_imagref (z)) >= 2;
    if (large_z && mpc_asin_large (rop, z1, z, rnd))
      break;

    /* z1 <- z^2 */
    mpc_sqr (z1, z, MPC_RNDNN);
    /* err(x) <= 1/2 ulp(x), err(y) <= 1/2 ulp(y) */
    /* z1 <- 1-z1 */
    ex = mpfr_get_exp (mpc_realref(z1));
    mpfr_ui_sub (mpc_realref(z1), 1, mpc_realref(z1), MPFR_RNDN);
    mpfr_neg (mpc_imagref(z1), mpc_imagref(z1), MPFR_RNDN);
    /* if Re(z1) = 0, we can't determine the relative error */
    if (mpfr_zero_p (mpc_realref(z1)))
      continue;
    /* the error on 1-Re(z1_old) is bounded by 1/2 ulp(z1), and that on
       Re(z1_old) is bounded by 1/2ulp(Re(z1_old)) = 1/2ulp*2^(ex-EXP(Re(z1))),
       thus the total error is bounded:
       * if ex >= EXP(Re(z1)), by 2^(ex - EXP(Re(z1))) ulps
       * otherwise by 1 ulp */
    ex = ex - mpfr_get_exp (mpc_realref(z1));
    ex = (ex <= 0) ? 0 : ex;
    /* err(x) <= 2^ex * ulp(x), convert into absolute error */
    abs_ex = ex + mpfr_get_exp (mpc_realref(z1)) - p;
    /* err(x) <= 2^abs_ex */
    /* the error on Im(z1) is bounded by 1/2 ulp, thus absolutely by
       2^EXP(Im(z1))-p-1 */
    abs_ey = mpfr_get_exp (mpc_imagref(z1)) - p - 1;
    /* err(y) <= 2^abs_ey */
    abs_e = (abs_ex >= abs_ey) ? abs_ex : abs_ey;
    /* err(x), err(y) <= 2^abs_e, i.e., the norm of the error is bounded by
       |h|<=2^(abs_ex+1/2) */
    /* z1 <- sqrt(z1): if z1 = z + h, then sqrt(z1) = sqrt(z) + h/2/sqrt(z1) */
    abs_ey = mpfr_get_exp (mpc_realref(z1)) >= mpfr_get_exp (mpc_imagref(z1))
      ? mpfr_get_exp (mpc_realref(z1)) : mpfr_get_exp (mpc_imagref(z1));
    /* we have |z1| >= 2^(abs_ey-1) thus 1/|z1| <= 2^(1-abs_ey) */

    /* Check if the conditions of Lemma lem:sqrt from algorithms.tex
       are fulfilled: x > 0, |y/x| < 1/2, and p >= 1, where the
       relative error on the real/imaginary parts of z1 is bounded by 2^-p.
       The condition |y/x| < 1/2 is fulfilled as soon as the difference of
       exponents is at least 2. Since the error on the real/imaginary parts
       of z1 is bounded by 2^ex ulps, the condition "p >= 1" translates into
       ex <= p-2, where p is the working precision. */
    dif = mpfr_get_exp (mpc_realref (z1)) - mpfr_get_exp (mpc_imagref (z1));
    lem_sqrt = mpfr_sgn (mpc_realref (z1)) > 0 && ex <= p - 2 && dif >= 2;

    mpc_sqrt (z1, z1, MPC_RNDNN);

    if (lem_sqrt) {
      /* Lemma lem:sqrt says that the induced relative error on Re(sqrt(z1))
         due to the initial error of z1 is bounded by 2^-q, if the initial
         error on z1 was bounded relative by 2^-q on both the real and
         imaginary parts.
         And the induced absolute error on Im(sqrt(z1)) is bounded by
         2^(-q-k+1)*|Re(sqrt(z1))|, where |Im(z1)/Re(z1)| < 2^-k before the
         square root.
         Here we have that the initial on z1 was bounded
         by 2^ex ulps on both the real and imaginary parts, thus the induced
         error on sqrt(z1) is bounded by 2^(ex+1) ulps. */
      err_x = ex + 1;
      err_y = mpfr_get_exp (mpc_realref (z1)) - mpfr_get_exp (mpc_imagref (z1))
        + ex + 2 - (dif - 1); // relative error on Im(z1)
    }
    else {
      abs_ex = 2 * abs_ex - abs_ey; /* |h^2/4/z1| <= 2^abs_ex */
      abs_ex = (abs_ex + 1) / 2; /* ceil(ex/2) */
      /* express ex in terms of ulp(z1) */
      abs_ey = mpfr_get_exp (mpc_realref(z1)) <= mpfr_get_exp (mpc_imagref(z1))
        ? mpfr_get_exp (mpc_realref(z1)) : mpfr_get_exp (mpc_imagref(z1));
      /* ulp(Re(z1)), ulp(Im(z1)) >= 2^(abs_ey-p) */
      ex = abs_ex - (abs_ey - p);
      /* take into account the rounding error in the mpc_sqrt call */
      err = (ex <= 0) ? 1 : ex + 1;
      err_x = err_y = err;
    }
    /* err(Re(z1)) <= 2^err_x * ulp(Re(z1))
       err(Im(z1)) <= 2^err_y * ulp(Im(z1)) */

    /* z1 <- i*z + z1 */
    ex = mpfr_get_exp (mpc_realref(z1));
    ey = mpfr_get_exp (mpc_imagref(z1));
    mpfr_sub (mpc_realref(z1), mpc_realref(z1), mpc_imagref(z), MPFR_RNDN);
    mpfr_add (mpc_imagref(z1), mpc_imagref(z1), mpc_realref(z), MPFR_RNDN);
    if (mpfr_zero_p (mpc_realref(z1)) || mpfr_zero_p (mpc_imagref(z1)))
      continue;
    ex -= mpfr_get_exp (mpc_realref(z1)); /* cancellation in x */
    ey -= mpfr_get_exp (mpc_imagref(z1)); /* cancellation in y */
    err_x += ex; // add cancellation in x
    err_y += ey; // add cancellation in y
    err_x = (err_x <= 0) ? 1 : err_x + 1; /* rounding error in sub/add */
    err_y = (err_y <= 0) ? 1 : err_y + 1; /* rounding error in sub/add */
    /* z1 <- log(z1): if z1 = z + h, then log(z1) = log(z) + h/t with
       |t| >= min(|z1|,|z|) */
    ex = mpfr_get_exp (mpc_realref(z1));
    ey = mpfr_get_exp (mpc_imagref(z1));
    /* Do the conditions of lem:log apply? The condition |y/x| < 2^-k
       with k >= 1 is satisfied as soon as the exponent difference is
       at least 2. */
    dif = ex - ey;
    lem_log = dif >= 2 && mpfr_cmp_ui (mpc_realref (z1), 2) >= 0;
    mpc_log (z1, z1, MPFR_RNDN);
    if (lem_log) {
      err = (err_x >= err_y) ? err_x : err_y;
      /* Lemma lem:log says that if the relative error on Re(z1) and Im(z1)
         before the mpc_log call is bounded relatively by 2^-q, then after
         the mpc_log call:
         * the relative error on the real part of z1 is bounded by 2^(-q+1)
         * the error on the imaginary part of z1 is bounded by
           2^(-q-k+3) * |Re(z1)|
         Since error on Re(z1) (resp. Im(z1)) before the mpc_log was bounded by
         2^err*ulp(Re(z1)) (resp. 2^err*ulp(Im(z1))), it was bounded
         relatively by 2^(err-p+1), thus we can take q = p - (err+1),
         and a relative error of 2^-q converts to an error of 2^(-q+p) ulps.
      */
      err_x = err + 2; // rel. error of 2^(-q+1) -> 2^(-q+1+p) ulps
      err_y = err + 4 - (dif - 1); // k >= dif-1
      ex = mpfr_get_exp (mpc_realref(z1));
      ey = mpfr_get_exp (mpc_imagref(z1));
      // express err_y in terms of ulp(Im(z1))
      err_y += ex - ey;
    }
    else {
      err_x += ex - p; /* revert to absolute error <= 2^err_x */
      err_y += ey - p; /* revert to absolute error <= 2^err_y */
      /* the error h before the log contributes to h/t */
      err = (err_x >= err_y) ? err_x : err_y;
      ex = (ex >= ey) ? ex : ey;
      err -= ex - 1; /* 1/|t| <= 1/|z| <= 2^(1-ex) */
      /* express err in terms of ulp(Re(z1)) */
      ex = mpfr_get_exp (mpc_realref(z1));
      ey = mpfr_get_exp (mpc_imagref(z1));
      err_x = err - ex + p;
      err_y = err - ey + p;
    }
    /* take into account the rounding error in the mpc_log call */
    err_x = (err_x <= 0) ? 1 : err_x + 1;
    err_y = (err_y <= 0) ? 1 : err_y + 1;
    /* z1 <- -i*z1 */
    mpfr_swap (mpc_realref(z1), mpc_imagref(z1));
    mpfr_neg (mpc_imagref(z1), mpc_imagref(z1), MPFR_RNDN);
    if (mpfr_can_round (mpc_realref(z1), p - err_y, MPFR_RNDN, MPFR_RNDZ,
                        p_re + (rnd_re == MPFR_RNDN)) &&
        mpfr_can_round (mpc_imagref(z1), p - err_x, MPFR_RNDN, MPFR_RNDZ,
                        p_im + (rnd_im == MPFR_RNDN)))
      break;
  }

  if (neg) {
    MPFR_CHANGE_SIGN (mpc_realref (z1));
    MPFR_CHANGE_SIGN (mpc_imagref (z1));
  }
  inex = mpc_set (rop, z1, rnd);
  mpc_clear (z1);

  /* restore the exponent range, and check the range of results */
  mpfr_set_emin (saved_emin);
  mpfr_set_emax (saved_emax);
  inex_re = mpfr_check_range (mpc_realref (rop), MPC_INEX_RE (inex),
                              MPC_RND_RE (rnd));
  inex_im = mpfr_check_range (mpc_imagref (rop), MPC_INEX_IM (inex),
                              MPC_RND_IM (rnd));

  return MPC_INEX (inex_re, inex_im);
}
