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

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <fftw3.h>
#include <string.h>
#include <assert.h>

#include "p3m-common.h"
#include "charge-assign.h"

#include "common.h"
#include "interpol.h"

#include "p3m-ad-self-forces.h"

int P3M_BRILLOUIN = 1;
int P3M_BRILLOUIN_TUNING = 1;

#define FREE_TRACE(A) 

#define DUMMY_G_STEP 10
#define DUMMY_G_MIN_SIZE 32
#define DUMMY_G_MAX_SIZE 300

static FLOAT_TYPE *dummy_g = NULL;
static int dummy_g_size = 0;
static interpolation_t *dummy_inter = NULL;

static void dummy_g_realloc(int mesh) {
  size_t new_size;
  if((dummy_g != NULL) && (mesh <= dummy_g_size))
    return;

  new_size = (dummy_g_size + DUMMY_G_STEP);
  new_size = (new_size < DUMMY_G_MIN_SIZE) ? DUMMY_G_MIN_SIZE : new_size;
  new_size = (new_size > DUMMY_G_MAX_SIZE) ? DUMMY_G_MAX_SIZE : new_size;

  new_size = new_size*new_size*new_size*sizeof(double);

  if(dummy_g != NULL)
    FFTW_FREE(dummy_g);

  dummy_g = FFTW_MALLOC(new_size);

  assert(dummy_g != NULL);
  
  for(int i = 0; i < mesh*mesh*mesh; i++)
    dummy_g[i] = 1.0;

}


FLOAT_TYPE sinc(FLOAT_TYPE d)
{
  FLOAT_TYPE PId = PI*d;
  return (d == 0.0) ? 1.0 : SIN(PId)/PId;
}

FLOAT_TYPE analytic_cotangent_sum(int n, FLOAT_TYPE mesh_i, int cao)
{
    FLOAT_TYPE c, res=0.0;
    c = SQR(COS(PI*mesh_i*(FLOAT_TYPE)n));

    switch (cao) {
    case 1 :
        res = 1;
        break;
    case 2 :
        res = (1.0+c*2.0)/3.0;
        break;
    case 3 :
        res = (2.0+c*(11.0+c*2.0))/15.0;
        break;
    case 4 :
        res = (17.0+c*(180.0+c*(114.0+c*4.0)))/315.0;
        break;
    case 5 :
        res = (62.0+c*(1072.0+c*(1452.0+c*(247.0+c*2.0))))/2835.0;
        break;
    case 6 :
        res = (1382.0+c*(35396.0+c*(83021.0+c*(34096.0+c*(2026.0+c*4.0)))))/155925.0;
        break;
    case 7 :
        res = (21844.0+c*(776661.0+c*(2801040.0+c*(2123860.0+c*(349500.0+c*(8166.0+c*4.0))))))/6081075.0;
        break;
    }

    return res;
}


void Init_differential_operator(data_t *d)
{
    /*
       Die Routine berechnet den fourieretransformierten
       Differentialoperator auf er Ebene der n, nicht der k,
       d.h. der Faktor  i*2*PI/L fehlt hier!
    */

    int    i;
    FLOAT_TYPE dMesh=(FLOAT_TYPE)d->mesh;
    FLOAT_TYPE dn;

    for (i=0; i<d->mesh; i++)
    {
        dn    = (FLOAT_TYPE)i;
        dn   -= ROUND(dn/dMesh)*dMesh;
        d->Dn[i] = dn;
    }

    d->Dn[d->mesh/2] = 0.0;

}

void Init_nshift(data_t *d)
{
    /* Verschiebt die Meshpunkte um Mesh/2 */

    int    i;
    FLOAT_TYPE dMesh=(FLOAT_TYPE)d->mesh;

    for (i=0; i<d->mesh; i++)
        d->nshift[i] = i - ROUND(i/dMesh)*dMesh;

}

data_t *Init_data(const method_t *m, system_t *s, parameters_t *p) {
    int mesh3 = p->mesh*p->mesh*p->mesh;
    data_t *d = Init_array(1, sizeof(data_t));

    d->mesh = p->mesh;
    
    if ( m->flags & METHOD_FLAG_Qmesh)
        d->Qmesh = Init_array(2*mesh3, sizeof(FLOAT_TYPE));
    else
        d->Qmesh = NULL;

    if ( m->flags & METHOD_FLAG_ik ) {
        d->Fmesh = Init_vector_array(2*mesh3);
        d->Dn = Init_array(d->mesh, sizeof(FLOAT_TYPE));
        Init_differential_operator(d);
    }
    else {
        d->Fmesh = NULL;
        d->Dn = NULL;
    }

    d->nshift = NULL;

    if ( m->flags & METHOD_FLAG_nshift ) {
      d->nshift = Init_array(d->mesh, sizeof(FLOAT_TYPE));
      Init_nshift(d);
    }

    d->dQdx[0] = d->dQdy[0] = d->dQdz[0] = NULL;
    d->dQdx[1] = d->dQdy[1] = d->dQdz[1] = NULL;

    if( m->flags & METHOD_FLAG_self_force_correction)
      d->self_force_corrections = Init_array(my_power(1+2*P3M_SELF_BRILLOUIN, 3), 3*sizeof(FLOAT_TYPE));

    if ( m->flags & METHOD_FLAG_ad ) {
      int i;
      int max = ( m->flags & METHOD_FLAG_interlaced) ? 2 : 1;

        for (i = 0; i < max; i++) {
            d->dQdx[i] = Init_array( s->nparticles*p->cao3, sizeof(FLOAT_TYPE) );
            d->dQdy[i] = Init_array( s->nparticles*p->cao3, sizeof(FLOAT_TYPE) );
            d->dQdz[i] = Init_array( s->nparticles*p->cao3, sizeof(FLOAT_TYPE) );
        }
    }

    if ( m->flags & METHOD_FLAG_ca ) {
      int i;
      int max = ( m->flags & METHOD_FLAG_interlaced ) ? 2 : 1;
      d->cf[1] = NULL;
      d->ca_ind[1] = NULL;
      
      for (i = 0; i < max; i++) {
	d->cf[i] = Init_array( p->cao3 * s->nparticles, sizeof(FLOAT_TYPE));
	d->ca_ind[i] = Init_array( 3*s->nparticles, sizeof(int));
      }
	
      if( !p->tuning )
	d->inter = Init_interpolation( p->ip, m->flags & METHOD_FLAG_ad );
      else {
	if(dummy_inter == NULL)
	  dummy_inter = Init_interpolation( 6, 1 );
	d->inter = dummy_inter;
      }
    }
    else {
      d->cf[0] = NULL;
      d->ca_ind[0] = NULL;
      d->cf[1] = NULL;
      d->ca_ind[1] = NULL;
      d->inter = NULL;
    }

    if ( m->flags & METHOD_FLAG_G_hat) {
      d->G_hat = Init_array(mesh3, sizeof(FLOAT_TYPE));

      if( !p->tuning) {
        m->Influence_function( s, p, d );   
      } else {
	dummy_g_realloc(d->mesh);
	d->G_hat = dummy_g;
      }
    }
    else
      d->G_hat = NULL;    


    d->forward_plans = 0;
    d->backward_plans = 0;

    return d;
}

void Free_data(data_t *d) {
    int i;

    if( d == NULL )
      return;

    FREE_TRACE(puts("Free_data(); Free ghat.");)
      // Free G_hat only if it's not the dummy influence function.
      if ((d->G_hat != NULL) && (d->G_hat != dummy_g))
        FFTW_FREE(d->G_hat);

    FREE_TRACE(puts("Free qmesh.");)
    if (d->Qmesh != NULL)
        FFTW_FREE(d->Qmesh);

    FREE_TRACE(puts("Free Fmesh.");)
    if(d->Fmesh != NULL)
      Free_vector_array(d->Fmesh);

    FREE_TRACE(puts("Free dshift.");)
    if (d->nshift != NULL)
        FFTW_FREE(d->nshift);

    if (d->Dn != NULL)
        FFTW_FREE(d->Dn);

    for (i=0;i<2;i++) {
        if (d->dQdx[i] != NULL)
            FFTW_FREE(d->dQdx[i]);
        if (d->dQdy[i] != NULL)
            FFTW_FREE(d->dQdy[i]);
        if (d->dQdz[i] != NULL)
            FFTW_FREE(d->dQdz[i]);
    }

    if((d->inter != NULL ) && (d->inter != dummy_inter)) {
      Free_interpolation(d->inter);
      d->inter = NULL;
    }

    for (i=0;i<2;i++) {
        if (d->cf[i] != NULL)
            FFTW_FREE(d->cf[i]);
        if (d->ca_ind[i] != NULL)
            FFTW_FREE(d->ca_ind[i]);
    }

    for(i=0; i<d->forward_plans; i++) {
      FFTW_DESTROY_PLAN(d->forward_plan[i]);
    }

    for(i=0; i<d->backward_plans; i++) {
      FFTW_DESTROY_PLAN(d->backward_plan[i]);
    }

    FFTW_FREE(d);

}

FLOAT_TYPE C_ewald(int nx, int ny, int nz, system_t *s, parameters_t *p) {
  int mx, my, mz;
  int nmx, nmy, nmz;
  FLOAT_TYPE km2;
  FLOAT_TYPE ret = 0.0;

  for (mx = -P3M_BRILLOUIN; mx <= P3M_BRILLOUIN; mx++) {
    nmx = nx + p->mesh*mx;
    for (my = -P3M_BRILLOUIN; my <= P3M_BRILLOUIN; my++) {
      nmy = ny + p->mesh*my;
      for (mz = -P3M_BRILLOUIN; mz <= P3M_BRILLOUIN; mz++) {
	nmz = nz + p->mesh*mz;

	km2 = SQR(2.0*PI/s->length) * ( SQR ( nmx ) + SQR ( nmy ) + SQR ( nmz ) );
	ret += EXP(- 2.0 * km2 / ( 4.0 * SQR(p->alpha)) ) / km2;
      }
    }
  }
  return 16.0 * SQR(PI) * ret;
}

FLOAT_TYPE K2(int nx, int ny, int nz, FLOAT_TYPE l) {
  return SQR(2.0*PI/l) * ( SQR ( nx ) + SQR ( ny ) + SQR ( nz ) ); 
}

FLOAT_TYPE G(int nx, int ny, int nz, FLOAT_TYPE l, FLOAT_TYPE alpha) {
  FLOAT_TYPE k2 = K2(nx, ny, nz, l);
  return (4*PI/k2) * EXP(-k2/(4*alpha*alpha));
}

FLOAT_TYPE Gm(int nx, int ny, int nz, FLOAT_TYPE l, FLOAT_TYPE alpha, int m, int mc) {
  FLOAT_TYPE k2 = 0.0;

  FLOAT_TYPE ret=0.0;
  int nmx, nmy, nmz;
  for(int mx = -mc; mx <=mc; mx++){
    nmx = nx + mx*m;
    for(int my = -mc; my <=mc; my++){
      nmy = ny + mx*my;
      for(int mz = -mc; mz <=mc; mz++){
	nmz = nz + mz*m;
        k2 = K2(nmx, nmy, nmz, l);
	ret += (4*PI/k2) * EXP(-k2/(4*alpha*alpha));
      }
    }
  }
  return ret;
}

FLOAT_TYPE C(int nx, int ny, int nz, FLOAT_TYPE l, FLOAT_TYPE alpha, int mc, int m) {
  FLOAT_TYPE ret=0.0;
  int nmx, nmy, nmz;
  for(int mx = -mc; mx <=mc; mx++){
    nmx = nx + mx*m;
    for(int my = -mc; my <=mc; my++){
      nmy = ny + mx*my;
      for(int mz = -mc; mz <=mc; mz++){
	nmz = nz + mz*m;
	ret += K2(nmx, nmy, nmz,l) * G(nmx,nmy,nmz,l,alpha) * G(nmx,nmy,nmz,l,alpha);
      }
    }
  }
  return ret;
}



FLOAT_TYPE U2(int nx, int ny, int nz, int m, int p) {
  return pow(sinc((FLOAT_TYPE)(nx)/m)*sinc((FLOAT_TYPE)(ny)/m)*sinc((FLOAT_TYPE)(nz)/m), 2*p);
}

FLOAT_TYPE U(int nx, int ny, int nz, int m, int p) {
  return pow(sinc((FLOAT_TYPE)(nx)/m)*sinc((FLOAT_TYPE)(ny)/m)*sinc((FLOAT_TYPE)(nz)/m), p);
}

FLOAT_TYPE A(int nx, int ny, int nz, FLOAT_TYPE l, FLOAT_TYPE alpha, int m, int mc, int p) {
  FLOAT_TYPE Um = 0, Umk = 0, u2;
  int nmx,nmy,nmz;
  for(int mx = -mc; mx <=mc; mx++){
    nmx = nx + mx*m;
    for(int my = -mc; my <=mc; my++){
      nmy = ny + mx*my;
      for(int mz = -mc; mz <=mc; mz++){
	nmz = nz + mz*m;
	u2 = U2(nmx, nmy, nmz, m, p);
	Um += u2;
	Umk += K2(nmx, nmy, nmz,l) * u2;
      }
    }
  }
  return Um*Umk;
}

FLOAT_TYPE B(int nx, int ny, int nz, FLOAT_TYPE l, FLOAT_TYPE alpha, int m, int mc, int p) {
  FLOAT_TYPE u2gk2 = 0.0;
  int nmx,nmy,nmz;
  for(int mx = -mc; mx <=mc; mx++){
    nmx = nx + mx*m;
    for(int my = -mc; my <=mc; my++){
      nmy = ny + mx*my;
      for(int mz = -mc; mz <=mc; mz++){
	nmz = nz + mz*m;
	u2gk2 += U2(nmx, nmy, nmz, m, p) * G(nmx,nmy,nmz,l,alpha)  * K2(nmx, nmy, nmz,l);
      }
    }
  }
  return u2gk2;
}


#define NTRANS(N) (N<0) ? (N + d->mesh) : N
#define NT(N) (N<0) ? (N + mesh) : N

FLOAT_TYPE *Error_map(system_t *s, forces_t *f, forces_t *f_ref, int mesh, int cao) {
  system_t *s2 = Init_system(s->nparticles);
  s2->length = s->length;

  parameters_t param;
  param.mesh = mesh;
  param.cao = cao;

  interpolation_t *inter = Init_interpolation( cao - 1, 0 );

  FLOAT_TYPE dF, dF_total = 0.0, dF_total_mesh = 0.0;
  FLOAT_TYPE *error_mesh = Init_array( mesh*mesh*mesh, 2*sizeof(FLOAT_TYPE));
  memset( error_mesh, 0, 2*mesh*mesh*mesh*sizeof(FLOAT_TYPE));

  for(int i = 0; i<s->nparticles; i++) {
    dF = 0;
    for(int j = 0; j<3; j++) {
      dF += SQR(f_ref->f_k->fields[j][i] - f->f_k->fields[j][i]);
      s2->p->fields[j][i] = s->p->fields[j][i];
    }
    /* printf("part %d rms2 %e\n", i, dF); */
    s2->q[i] = dF;
    dF_total += dF;

  }
  assign_charge_nocf(s2, &param, error_mesh, mesh, inter);

  for(int i = 0; i < 2*mesh*mesh*mesh; i++)
    dF_total_mesh += error_mesh[i];

  printf("dF_total %e dF_total_mesh %e\n", FLOAT_CAST dF_total, FLOAT_CAST dF_total_mesh);

  return error_mesh;
}

FLOAT_TYPE Generic_error_estimate_inhomo(system_t *s, parameters_t *p, int mesh, int cao, int mc, char *out_file) {
  //  printf("Generic_error_estimate_inhomo(mesh %d cao %d, mc %d):\n", mesh, cao, mc);
  int ind = 0;
  int nx, ny, nz;
  // puts("Init Qmesh.");
  FLOAT_TYPE *Qmesh = Init_array( mesh*mesh*mesh*2, sizeof(FLOAT_TYPE));
  FLOAT_TYPE *Kmesh = Init_array( mesh*mesh*mesh*2, sizeof(FLOAT_TYPE));
  FLOAT_TYPE *Kernel[4];
  // puts("Plan FFT.");
  // printf("Mesh size %d, Mesh %p\n", mesh, Qmesh);
  FFTW_PLAN forward_plan = FFTW_PLAN_DFT_3D(mesh, mesh, mesh, (FFTW_COMPLEX*) Qmesh, (FFTW_COMPLEX*) Kmesh, FFTW_FORWARD, FFTW_MEASURE);
  FFTW_PLAN backward_plan = FFTW_PLAN_DFT_3D(mesh, mesh, mesh, (FFTW_COMPLEX*) Kmesh, (FFTW_COMPLEX*)Kmesh, FFTW_BACKWARD, FFTW_MEASURE);
  FFTW_PLAN kernel_backward_plan[3];
  for(int i = 0; i < 4; i++) {
    Kernel[i] = Init_array( 2*mesh*mesh*mesh, sizeof(FLOAT_TYPE));
    // printf("Kernel[%d] at %p\n", i, Kernel[i]);
    if(i < 3)
      kernel_backward_plan[i] = FFTW_PLAN_DFT_3D(mesh, mesh, mesh, (FFTW_COMPLEX *) Kernel[i], (FFTW_COMPLEX *) Kernel[i],FFTW_BACKWARD, FFTW_MEASURE);
  }

  FFTW_PLAN kernel_forward_plan = FFTW_PLAN_DFT_3D(mesh, mesh, mesh,(FFTW_COMPLEX *) Kernel[3], (FFTW_COMPLEX *) Kernel[3],FFTW_FORWARD, FFTW_MEASURE);
  int mesh3 = mesh*mesh*mesh;
  FLOAT_TYPE k2 = 0.0;
  parameters_t param;
  param.mesh = mesh;
  param.alpha = p->alpha;
  param.cao = cao;
  param.cao3 = cao*cao*cao;

  interpolation_t *inter = Init_interpolation( cao - 1, 0 );

  memset( Qmesh, 0, mesh*mesh*mesh*2 * sizeof(FLOAT_TYPE));
  memset( Kmesh, 0, mesh*mesh*mesh*2 * sizeof(FLOAT_TYPE));

  /* Calculate \rho^2 */
  assign_charge_q2(s, &param, Qmesh, mesh, inter);

  int tn[3];

  /* Homogenous part */

  FLOAT_TYPE u,um,b,a, u2, gm;

  /* Calculate K_homo(k) */

  for (nx=0; nx<mesh; nx++) {
    tn[0] = (nx >= mesh/2) ? (nx - mesh) : nx;
    for (ny=0; ny<mesh; ny++) {
      tn[1] = (ny >= mesh/2) ? (ny - mesh) : ny;
      for (nz=0; nz<mesh; nz++) {
	tn[2] = (nz >= mesh/2) ? (nz - mesh) : nz;
	ind = 2*(mesh*mesh*nx + mesh*ny + nz);

	if( (tn[0] == 0) &&  (tn[1] == 0) && (tn[2] == 0) ) 
	  k2 = 0.0;
	else {
	  u2 = U2(tn[0],tn[1],tn[2], mesh, param.cao);
	  b = B(tn[0],tn[1],tn[2], s->length, param.alpha, mesh, 1, param.cao);
	  a = A(tn[0],tn[1],tn[2], s->length, param.alpha, mesh, 1, param.cao);
	  gm = Gm(tn[0],tn[1],tn[2],s->length, param.alpha, mesh, 1);

	  k2 = (u2*b/a) - gm;

	  /* printf("(%d, %d, %d): tn = (%d, %d, %d), K2 = %.10e * %.10e / %.10e - %.10e = %.10e, %e\n", nx, ny, nz, tn[0], tn[1], tn[2], FLOAT_CAST u2, FLOAT_CAST b, FLOAT_CAST a, FLOAT_CAST gm, FLOAT_CAST k2, FLOAT_CAST (u2*b/a - gm)); */
	  /* printf("\t u2 = %e\n", FLOAT_CAST u2); */
	  /* printf("\t b = %e\n", FLOAT_CAST b); */
	  /* printf("\t a = %e\n", FLOAT_CAST a); */
	  /* printf("\t gm = %e\n", FLOAT_CAST gm); */
	  /* printf("\t -2*PI*tn/s->length = %e %e %e\n", FLOAT_CAST -2*PI*tn[0]/s->length, FLOAT_CAST -2*PI*tn[1]/s->length, FLOAT_CAST -2*PI*tn[2]/s->length); */
	}

	Kernel[0][ind + 0] = 0;
	Kernel[0][ind + 1] = -2*PI*tn[0]/s->length*k2;
	Kernel[1][ind + 0] = 0;
	Kernel[1][ind + 1] = -2*PI*tn[1]/s->length*k2;
	Kernel[2][ind + 0] = 0;
	Kernel[2][ind + 1] = -2*PI*tn[2]/s->length*k2;	  

	/* for(int i = 0; i < 3; i++) { */
	/*   printf("kernel[%d](%d, %d, %d) = %lf + I * %lf\n", i, nx, ny, nz, FLOAT_CAST Kernel[i][ind + 0], FLOAT_CAST Kernel[i][ind + 1]); */
	/* } */


      }
    }
  }

  /* Transform back */

  for(int i = 0; i < 3; i++)
    FFTW_EXECUTE(kernel_backward_plan[i]);

  FLOAT_TYPE kr;  

  /* Calculate K^2_homo(r) */

  for (nx=0; nx<mesh; nx++) {
    for (ny=0; ny<mesh; ny++) {
      for (nz=0; nz<mesh; nz++) {
	ind = 2*((mesh*mesh*nx) + mesh*(ny) + (nz));
	kr = 0;
	for(int i = 0; i < 3; i++) {
	  kr += SQR(Kernel[i][ind + 0]);
	  /* printf("kernel[%d](%d, %d, %d) = %lf + I * %lf\n", i, nx, ny, nz, FLOAT_CAST Kernel[i][ind + 0], FLOAT_CAST Kernel[i][ind + 1]); */
	}
	Kernel[3][ind + 0] = kr;
	Kernel[3][ind + 1] = 0;
      }
    }
  }

  /* Inhomogemous paprt */

  /* Calculate K_inhomo(k) */

  for(int mx = -mc; mx <=mc; mx++)
    for(int my = -mc; my <=mc; my++)
      for(int mz = -mc; mz <=mc; mz++) {
	if((mx == 0) && (my==0) && (mz==0))
	  continue;
	for (nx=0; nx<mesh; nx++) {
	  tn[0] = (nx >= mesh/2) ? (nx - mesh) : nx;
	  for (ny=0; ny<mesh; ny++) {
	    tn[1] = (ny >= mesh/2) ? (ny - mesh) : ny;
	    for (nz=0; nz<mesh; nz++) {
	      tn[2] = (nz >= mesh/2) ? (nz - mesh) : nz;
	      ind = 2*(mesh*mesh*nx + mesh*ny + nz);

	      if((tn[0] == 0) &&  (tn[1] == 0) && (tn[2] == 0)) 
		k2 = 0.0;
	      else {
		u = U(tn[0],tn[1],tn[2], mesh, param.cao);
		um = U(-tn[0]-mx*mesh,-tn[1]-my*mesh,-tn[2]-mz*mesh, mesh, param.cao);
		b = B(tn[0],tn[1],tn[2], s->length, param.alpha, mesh, 0, param.cao);
		a = A(tn[0],tn[1],tn[2], s->length, param.alpha, mesh, 0, param.cao);
		k2 = 2*PI*u*um*b/a;
		
	      }

	      Kernel[0][ind + 0] = 0;
	      Kernel[0][ind + 1] = -(tn[0]/s->length)*k2;
	      Kernel[1][ind + 0] = 0;
	      Kernel[1][ind + 1] = -(tn[1]/s->length)*k2;
	      Kernel[2][ind + 0] = 0;
	      Kernel[2][ind + 1] = -(tn[2]/s->length)*k2;	  

	      /* printf("(%d, %d, %d): tn = (%d, %d, %d), k2 = %e\n", nx, ny, nz, tn[0], tn[1], tn[2], FLOAT_CAST k2); */
	      /* printf("\t u = %e\n", FLOAT_CAST u); */
	      /* printf("\t um = %e\n", FLOAT_CAST um); */
	      /* printf("\t b = %e\n", FLOAT_CAST b); */
	      /* printf("\t a = %e\n", FLOAT_CAST a); */
	      /* for(int i = 0; i < 3; i++) { */
	      /* 	printf("\tkernel[%d](%d %d %d) = %e + I * %e\n", i, nx, ny, nz, FLOAT_CAST Kernel[i][ind + 0], FLOAT_CAST Kernel[i][ind + 1]); */
	      /* } */

	    }
	  }
	}

	/* Transform back */

	for(int i = 0; i < 3; i++)
	  FFTW_EXECUTE(kernel_backward_plan[i]);


	/* Calculate K^2_inhomo(r) */
	FLOAT_TYPE kr;  

	for (nx=0; nx<mesh; nx++) {
	  for (ny=0; ny<mesh; ny++) {
	    for (nz=0; nz<mesh; nz++) {
	      ind = 2*((mesh*mesh*nx) + mesh*(ny) + (nz));
	      kr = 0;
	      for(int i = 0; i < 3; i++) {
		kr += SQR(Kernel[i][ind + 0]);
		/* printf("kernel[%d](%d, %d, %d) = %lf + I * %lf\n", i, nx, ny, nz, FLOAT_CAST Kernel[i][ind + 0], FLOAT_CAST Kernel[i][ind + 1]); */
	      }
	      Kernel[3][ind + 0] += kr;
	    }
	  }
	}
      }

  /* Calculate K^2(k) = FFT[K^2_homo(r) + K^2_inhomo(r) */

  FFTW_EXECUTE(kernel_forward_plan);

  /* Transform \rho^2 to k-space */
  
  FFTW_EXECUTE(forward_plan);



  /* Calculate convolution [\rho^2 * K] */

  for (nx=0; nx<mesh; nx++) {
    for (ny=0; ny<mesh; ny++) {
      for (nz=0; nz<mesh; nz++) {
  	ind = 2*(mesh*mesh*nx + mesh*ny + nz);
	Kmesh[ind + 0] *= Kernel[3][ind]/mesh3;
	Kmesh[ind + 1] *= Kernel[3][ind]/mesh3;
      }
    }
  }

  /* Transform back to get real space error density. */

  FFTW_EXECUTE(backward_plan);

  
  FLOAT_TYPE *rms = Init_array(s->nparticles, sizeof(FLOAT_TYPE));
  memset(rms, 0, s->nparticles * sizeof(FLOAT_TYPE));

  /* Interpolate on particles */

  collect_rms_nocf(s, p, Kmesh, rms, mesh, inter);


  FLOAT_TYPE sum = 0.0;
  FILE *inhomo_out = NULL;
  if(out_file != NULL) {
    inhomo_out = fopen(out_file, "w");
  }
  for (nx=0; nx<mesh; nx++) {
    for (ny=0; ny<mesh; ny++) {
      for (nz=0; nz<mesh; nz++) {
	ind = 2*((mesh*mesh*nx) + mesh*(ny) + (nz));
	if(inhomo_out != NULL)
	  fprintf( inhomo_out, "%d %d %d %e %e %e %e %e\n", nx, ny, nz, 
		   FLOAT_CAST Kmesh[ind + 0], FLOAT_CAST Kmesh[ind + 1], FLOAT_CAST Kernel[0][ind + 0], FLOAT_CAST Kernel[0][ind + 1], FLOAT_CAST Qmesh[ind]);
	sum += Kmesh[ind]*Qmesh[ind];
      }
    }
  }

  if(inhomo_out != NULL)
    fclose(inhomo_out);

  FLOAT_TYPE sum_part = 0.0;

  for(int i = 0; i < s->nparticles; i++)
    sum_part += rms[i];

  FFTW_FREE(rms);
  FFTW_FREE(Qmesh);
  FFTW_FREE(Kmesh);
  FFTW_FREE(Kernel[0]);
  FFTW_FREE(Kernel[1]);
  FFTW_FREE(Kernel[2]);
  FFTW_FREE(Kernel[3]);
  Free_interpolation(inter);

  return  SQRT(sum_part)/SQR(s->nparticles);
 
}


FLOAT_TYPE Generic_error_estimate(R3_to_R A, R3_to_R B, R3_to_R C, system_t *s, parameters_t *p, data_t *d) {
  // The Hockney-Eastwood pair-error functional.
  FLOAT_TYPE Q_HE = 0.0;
  // Linear index for G, this breaks notation, but G is calculated anyway, so this is convinient.
  int ind = 0;
  // Convinience variable to hold the current value of the influence function.
  FLOAT_TYPE G_hat = 0.0;
  FLOAT_TYPE a,b,c;

  int nx, ny, nz;

  for (nx=-d->mesh/2; nx<d->mesh/2; nx++) {
    for (ny=-d->mesh/2; ny<d->mesh/2; ny++) {
      for (nz=-d->mesh/2; nz<d->mesh/2; nz++) {
	if((nx!=0) || (ny!=0) || (nz!=0)) {
	  ind = r_ind(NTRANS(nx), NTRANS(ny), NTRANS(nz));
	  G_hat = d->G_hat[ind];

	  a = A(nx,ny,nz,s,p);
	  b = B(nx,ny,nz,s,p);
	  c = C(nx,ny,nz,s,p);

	  Q_HE += a * SQR(G_hat) - 2.0 * b * G_hat + c;

	  /* printf("A\t%e\tB\t%e\tC\t%e\n", FLOAT_CAST a , FLOAT_CAST b, FLOAT_CAST c); */
	  /* printf("B/A\t%e\tG_hat\t%e\n", FLOAT_CAST (b / a), FLOAT_CAST G_hat); */
	  /* printf("Q_HE\t%lf\tQ_opt\t%lf\n", FLOAT_CAST Q_HE, FLOAT_CAST Q_opt); */
	}
      }
    }
  }
  /* printf("Final Q_HE\t%lf\tQ_opt\t%lf\n", Q_HE, Q_opt); */
  /* printf("dF_opt = %e\n", s->q2* SQRT( FLOAT_ABS(Q_opt) / (FLOAT_TYPE)s->nparticles) / V); */

  return  Q_HE;
 
}

