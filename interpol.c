/**    Copyright (C) 2011,2012,2013 Florian Weik <fweik@icp.uni-stuttgart.de>

       This program is free software: you can redistribute it and/or modify
       it under the terms of the GNU General Public License as published by
       the Free Software Foundation, either version 3 of the License, or
       (at your option) any later version.

       This program is distributed in the hope that it will be useful,
       but WITHOUT ANY WARRANTY; without even the implied warranty of
       MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
       GNU General Public License for more details.

       You should have received a copy of the GNU General Public License
       along with this program.  If not, see <http://www.gnu.org/licenses/>. **/

#include <stdio.h>
#include <stdlib.h>

#include "types.h"
#include "interpol.h"
#include "common.h"

const interpolation_function_t ip_bspline = { INTERPOL_BSPLINE, caf_bspline, caf_bspline_d, caf_bspline_k };
const interpolation_function_t ip_kaiserbessel = { INTERPOL_KAISER, caf_kaiserbessel, NULL, caf_kaiserbessel_k };

interpolation_t *Init_interpolation(int ip, int derivatives)
{
  FLOAT_TYPE dInterpol=(FLOAT_TYPE)MaxInterpol;
  FLOAT_TYPE x;
  long   i,j;

  interpolation_function_t i_fct = ip_bspline;
  

  interpolation_t *ret;
  ret = (interpolation_t *)Init_array( 1, sizeof(interpolation_t));

  ret->interpol = (FLOAT_TYPE **)Init_array( (2* MaxInterpol + 1), sizeof(FLOAT_TYPE *));

  ret->interpol_d = NULL;

  for( i = 0; i < (2* MaxInterpol + 1); i++) {
    ret->interpol[i] = (FLOAT_TYPE *)Init_array( ip+2 , sizeof(FLOAT_TYPE));
  }

  if(derivatives) {
    ret->interpol_d = (FLOAT_TYPE **)Init_array(  (2* MaxInterpol + 1), sizeof(FLOAT_TYPE *));
    for( i = 0; i < (2* MaxInterpol + 1); i++) {
      ret->interpol_d[i] = (FLOAT_TYPE *)Init_array( ip + 2, sizeof(FLOAT_TYPE) );
    }
  }

  for(j=0;j<=ip;j++)
    for (i=-MaxInterpol; i<=MaxInterpol; i++)
      {
	x=i/(2.0*dInterpol);
	ret->interpol[i+MaxInterpol][j] = i_fct.U(j, x, ip+1);
	if(derivatives)
	  ret->interpol_d[i+MaxInterpol][j] = i_fct.U_d(j, x, ip+1);
      }
  ret->U_hat = i_fct.U_hat;
  return ret;
}

//@TODO: remove memleak
void Free_interpolation(interpolation_t *i) {
  for( int j = 0; j < (2*MaxInterpol+1); j++)
    FFTW_FREE(i->interpol[j]);
  FFTW_FREE(i->interpol);
  if(i->interpol_d != NULL) {
    for( int j = 0; j < (2*MaxInterpol+1); j++)
      FFTW_FREE(i->interpol_d[j]);
    FFTW_FREE(i->interpol_d);
  }

  FFTW_FREE(i);
}
