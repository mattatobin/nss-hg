/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Multiple Precision Integer optimization code for 
 * the Compaq Alpha processor.
 *
 * The Initial Developer of the Original Code is
 * Richard C. Swift.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):	Richard C. Swift	(swift@netscape.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "mpi-priv.h"
#include <c_asm.h>


#define MP_MUL_DxD(a, b, Phi, Plo)		\
 { Plo = asm ("mulq %a0, %a1, %v0", a, b);	\
   Phi = asm ("umulh %a0, %a1, %v0", a, b); }	\

/* This is empty for the loop in s_mpv_mul_d	*/
#define CARRY_ADD

#define ONE_MUL				\
    a_i = *a++;				\
    MP_MUL_DxD(a_i, b, a1b1, a0b0);	\
    a0b0 += carry;			\
    if (a0b0 < carry)			\
      ++a1b1;				\
    CARRY_ADD				\
    *c++ = a0b0;			\
    carry = a1b1;			\

#define FOUR_MUL			\
	ONE_MUL				\
	ONE_MUL				\
	ONE_MUL				\
	ONE_MUL				\

#define SIXTEEN_MUL			\
	FOUR_MUL			\
	FOUR_MUL			\
	FOUR_MUL			\
	FOUR_MUL			\

#define THIRTYTWO_MUL			\
	SIXTEEN_MUL			\
	SIXTEEN_MUL			\

#define ONETWENTYEIGHT_MUL		\
	THIRTYTWO_MUL			\
	THIRTYTWO_MUL			\
	THIRTYTWO_MUL			\
	THIRTYTWO_MUL			\


#define EXPAND_256(CALL)		\
 mp_digit carry = 0;			\
 mp_digit a_i;				\
 mp_digit a0b0, a1b1;			\
 if (a_len &255) {			\
	if (a_len &1) {			\
	  ONE_MUL			\
	}				\
	if (a_len &2) {			\
	  ONE_MUL			\
	  ONE_MUL			\
	}				\
	if (a_len &4) {			\
	  FOUR_MUL			\
	}				\
	if (a_len &8) {			\
	  FOUR_MUL			\
	  FOUR_MUL			\
	}				\
	if (a_len & 16 ) {		\
	  SIXTEEN_MUL			\
	}				\
	if (a_len & 32 ) {		\
	  THIRTYTWO_MUL			\
	}				\
	if (a_len & 64 ) {		\
	  THIRTYTWO_MUL			\
	  THIRTYTWO_MUL			\
	}				\
	if (a_len & 128) {		\
	  ONETWENTYEIGHT_MUL		\
	}				\
	a_len = a_len & (-256);		\
  }					\
  if (a_len>=256 ) {			\
	carry = CALL(a, a_len, b, c, carry);	\
	c += a_len;			\
  }					\

#define FUNC_NAME(NAME)			\
mp_digit NAME(const mp_digit *a, 	\
	mp_size a_len,			\
	mp_digit b, mp_digit *c, 	\
	mp_digit carry)			\

#define DECLARE_MUL_256(FNAME)		\
FUNC_NAME(FNAME)			\
{					\
  mp_digit a_i;				\
  mp_digit a0b0, a1b1;			\
  while (a_len) {			\
	ONETWENTYEIGHT_MUL		\
	ONETWENTYEIGHT_MUL		\
	a_len-= 256;			\
  }					\
  return carry;				\
}					\

/* Expanding the loop in s_mpv_mul_d appeared to slow down the
   (admittedly) small number of tests (i.e., timetest) used to
   measure performance, so this define disables that optimization. */
#define DO_NOT_EXPAND 1

/* Need forward declaration so it can be instantiated after
	the routine that uses it; this helps locality somewhat	*/
#if !defined(DO_NOT_EXPAND)
FUNC_NAME(s_mpv_mul_d_MUL256);
#endif

/* c = a * b */
void s_mpv_mul_d(const mp_digit *a, mp_size a_len, 
			mp_digit b, mp_digit *c)
{
#if defined(DO_NOT_EXPAND)
  mp_digit carry = 0;
  while (a_len--) {
    mp_digit a_i = *a++;
    mp_digit a0b0, a1b1;

    MP_MUL_DxD(a_i, b, a1b1, a0b0);

    a0b0 += carry;
    if (a0b0 < carry)
      ++a1b1;
    *c++ = a0b0;
    carry = a1b1;
  }
#else
  EXPAND_256(s_mpv_mul_d_MUL256)
#endif
  *c = carry;
}

#if !defined(DO_NOT_EXPAND)
DECLARE_MUL_256(s_mpv_mul_d_MUL256)
#endif

#undef CARRY_ADD
/* This is redefined for the loop in s_mpv_mul_d_add */
#define CARRY_ADD			\
    a0b0 += a_i = *c;			\
    if (a0b0 < a_i)			\
      ++a1b1;				\

/* Need forward declaration so it can be instantiated between the
	two routines that use it; this helps locality somewhat	*/
FUNC_NAME(s_mpv_mul_d_add_MUL256);

/* c += a * b */
void s_mpv_mul_d_add(const mp_digit *a, mp_size a_len, 
			mp_digit b, mp_digit *c)
{
  EXPAND_256(s_mpv_mul_d_add_MUL256)
  *c = carry;
}

/* Instantiate multiply 256 routine here */
DECLARE_MUL_256(s_mpv_mul_d_add_MUL256)

/* Presently, this is only used by the Montgomery arithmetic code. */
/* c += a * b */
void s_mpv_mul_d_add_prop(const mp_digit *a, mp_size a_len, 
			mp_digit b, mp_digit *c)
{
  EXPAND_256(s_mpv_mul_d_add_MUL256)
  while (carry) {
    mp_digit c_i = *c;
    carry += c_i;
    *c++ = carry;
    carry = carry < c_i;
  }
}

