/**    Copyright (C) 2011,2012,2013,2014 Florian Weik <fweik@icp.uni-stuttgart.de>

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

#include "charge-assign.h"

#include <stdio.h>
#include <math.h>
#include <assert.h>

// #define CA_DEBUG

/* Helper functions */

#ifdef WRAP1
inline static int wrap_mesh_index(int ind, int mesh) {
  if((ind > 0) && (ind < mesh))
    return ind;
  else if(ind < 0)
    ind += mesh;
  else if(ind >= mesh)
    ind -= mesh;
  return ind;
}
#else
inline static int wrap_mesh_index(int ind, int mesh) {
  return ind += (ind < 0)*mesh - (ind >= mesh)*mesh;
}
#endif

  // mesh > cao/2 !
  /* if((ret < 0) || (ret >= mesh)) */
  /*   return wrap_mesh_index(ret, mesh); */

inline static int int_floor(const FLOAT_TYPE x)
{
  const int i = (int)x; /* truncate */
  const int n = ( x != (FLOAT_TYPE)i );
  const int g = ( x < 0 );
  return i - ( n & g ); /* i-1 if x<0 and x!=i */
}

/* Charge assignment functions */

/* Assignment on complex grid */
void assign_charge_dynamic(system_t *s, parameters_t *p, data_t *d, int ii)
{
    int dim, i0, i1, i2, id;
    FLOAT_TYPE tmp0, tmp1;
    /* position of a particle in local mesh units */
    FLOAT_TYPE pos;
    /* 1d-index of nearest mesh point */
    int nmp;
    /* index for caf interpolation grid */
    int arg[3];
    /* index, index jumps for rs_mesh array */
    FLOAT_TYPE cur_ca_frac_val;
    FLOAT_TYPE *cf_cnt;
    // Mesh coordinates of the closest mesh point
    int base[3];
    int i,j,k;
    FLOAT_TYPE MI2 = 2.0*(FLOAT_TYPE)MaxInterpol;

    FLOAT_TYPE Hi = (double)d->mesh/(double)s->length;

    FLOAT_TYPE *cf = d->cf[ii];
    FLOAT_TYPE **interpol = d->inter->interpol;
    FLOAT_TYPE *Qmesh = d->Qmesh;
    FLOAT_TYPE q;
    const int cao = p->cao;
    const int mesh = d->mesh;

    // Make sure parameter-set and data-set are compatible

    FLOAT_TYPE pos_shift;

    /* Shift for odd charge assignment order */
    pos_shift = (FLOAT_TYPE)((p->cao-1)/2);

    for (id=0;id<s->nparticles;id++) {
        /* particle position in mesh coordinates */
        for (dim=0;dim<3;dim++) {
            pos    = s->p->fields[dim][id]*Hi - pos_shift + 0.5*ii;
            nmp = int_floor(pos + 0.5);
	    base[dim]  = wrap_mesh_index( nmp, d->mesh);
            arg[dim] = int_floor((pos - nmp + 0.5)*MI2);
            d->ca_ind[ii][3*id + dim] = base[dim];
        }
	q = s->q[id];
        cf_cnt = cf + id*p->cao3;
	for (i0=0; i0<cao; i0++) {
	  i = wrap_mesh_index(base[0] + i0, mesh);
	  tmp0 = q * interpol[arg[0]][i0];
	  for (i1=0; i1<cao; i1++) {
	    tmp1 = tmp0 * interpol[arg[1]][i1];
	    j = wrap_mesh_index(base[1] + i1, mesh);
	    for (i2=0; i2<cao; i2++) {
	      cur_ca_frac_val = tmp1 * interpol[arg[2]][i2];
	      k = wrap_mesh_index(base[2] + i2, mesh);
	      *cf_cnt++ = cur_ca_frac_val;
	      Qmesh[c_ind(i,j,k)+ii] += cur_ca_frac_val;
	    }
	  }
	} 
    }
}

#define assign_charge_template(cao) void assign_charge_##cao(system_t *s, parameters_t *p, data_t *d, int ii) \
{ \
    int dim, i0, i1, i2, id; \
    FLOAT_TYPE tmp0, tmp1; \
    /* position of a particle in local mesh units */ \
    FLOAT_TYPE pos; \
    /* 1d-index of nearest mesh point */ \
    int nmp; \
    /* index for caf interpolation grid */ \
    int arg[3]; \
    /* index, index jumps for rs_mesh array */ \
    FLOAT_TYPE cur_ca_frac_val; \
    FLOAT_TYPE * restrict cf_cnt; \
    int base[3]; \
    int i,j,k; \
    FLOAT_TYPE MI2 = 2.0*(FLOAT_TYPE)MaxInterpol; \
 \
    FLOAT_TYPE Hi = (double)d->mesh/(double)s->length; \
 \
    FLOAT_TYPE * restrict cf = d->cf[ii]; \
    FLOAT_TYPE ** restrict interpol = d->inter->interpol; \
    FLOAT_TYPE * restrict Qmesh = d->Qmesh; \
    FLOAT_TYPE q; \
    const int mesh = d->mesh; \
 \
 \
    FLOAT_TYPE pos_shift; \
 \
    /* Shift for odd charge assignment order */ \
    pos_shift = (FLOAT_TYPE)((cao-1)/2); \
 \
    for (id=0;id<s->nparticles;id++) { \
        /* particle position in mesh coordinates */ \
        for (dim=0;dim<3;dim++) { \
            pos    = s->p->fields[dim][id]*Hi - pos_shift + 0.5*ii; \
            nmp = int_floor(pos + 0.5); \
	    base[dim]  = wrap_mesh_index( nmp, d->mesh); \
            arg[dim] = int_floor((pos - nmp + 0.5)*MI2); \
            d->ca_ind[ii][3*id + dim] = base[dim]; \
        } \
	q = s->q[id]; \
        cf_cnt = cf + id*cao*cao*cao; \
	for (i0=0; i0<cao; i0++) { \
	  i = wrap_mesh_index(base[0] + i0, mesh); \
	  tmp0 = q * interpol[arg[0]][i0]; \
	  for (i1=0; i1<cao; i1++) { \
	    tmp1 = tmp0 * interpol[arg[1]][i1]; \
	    j = wrap_mesh_index(base[1] + i1, mesh); \
	    for (i2=0; i2<cao; i2++) { \
	      cur_ca_frac_val = tmp1 * interpol[arg[2]][i2]; \
	      k = wrap_mesh_index(base[2] + i2, mesh); \
	      *cf_cnt++ = cur_ca_frac_val; \
	      Qmesh[c_ind(i,j,k)+ii] += cur_ca_frac_val; \
	    } \
	  } \
	}  \
    } \
} \

assign_charge_template(1)
assign_charge_template(2)
assign_charge_template(3)
assign_charge_template(4)
assign_charge_template(5)
assign_charge_template(6)
assign_charge_template(7)

void assign_charge(system_t *s, parameters_t *p, data_t *d, int ii) {
  switch(p->cao) {
  case 1:
    assign_charge_1(s, p, d, ii);
    break;
  case 2:
    assign_charge_2(s, p, d, ii);
    break;
  case 3:
    assign_charge_3(s, p, d, ii);
    break;
  case 4:
    assign_charge_4(s, p, d, ii);
    break;
  case 5:
    assign_charge_5(s, p, d, ii);
    break;
  case 6:
    assign_charge_6(s, p, d, ii);
    break;
  case 7:
    assign_charge_7(s, p, d, ii);
    break;
  default:
    fprintf(stderr, "Charge assinment order %d not known.", p->cao);
    break;
  }
}

#define assign_charge_real_template(cao) void assign_charge_real_##cao(system_t *s, parameters_t *p, data_t *d) \
{ \
    int dim, i0, i1, i2, id; \
    FLOAT_TYPE tmp0, tmp1; \
    FLOAT_TYPE pos; \
    int nmp; \
    int arg[3]; \
    FLOAT_TYPE cur_ca_frac_val; \
    FLOAT_TYPE *cf_cnt; \
    int base[3]; \
    int i,j,k; \
    const FLOAT_TYPE MI2 = 2.0*(FLOAT_TYPE)MaxInterpol; \
\
    const FLOAT_TYPE Hi = (double)d->mesh/(double)s->length; \
\
    FLOAT_TYPE *cf = d->cf[0]; \
    FLOAT_TYPE **interpol = d->inter->interpol; \
    FLOAT_TYPE *Qmesh = d->Qmesh; \
    FLOAT_TYPE q; \
    const int mesh = d->mesh; \
    int indx, indy; \
 \
    FLOAT_TYPE pos_shift; \
\
    pos_shift = (FLOAT_TYPE)((cao-1)/2); \
\
    for (id=0;id<s->nparticles;id++) { \
        for (dim=0;dim<3;dim++) { \
            pos    = s->p->fields[dim][id]*Hi - pos_shift; \
            nmp = int_floor(pos + 0.5); \
	    base[dim]  = wrap_mesh_index( nmp, d->mesh); \
            arg[dim] = int_floor((pos - nmp + 0.5)*MI2); \
            d->ca_ind[0][3*id + dim] = base[dim]; \
        }	q = s->q[id]; \
        cf_cnt = cf + id*cao*cao*cao; \
	for (i0=0; i0<cao; i0++) { \
	  i = wrap_mesh_index(base[0] + i0, mesh); \
	  indx = mesh*(mesh+2) * i; \
	  tmp0 = q * interpol[arg[0]][i0]; \
	  for (i1=0; i1<cao; i1++) { \
	    tmp1 = tmp0 * interpol[arg[1]][i1]; \
	    j = wrap_mesh_index(base[1] + i1, mesh); \
	    indy = indx + (mesh+2) * j; \
	    for (i2=0; i2<cao; i2++) { \
	      cur_ca_frac_val = tmp1 * interpol[arg[2]][i2]; \
	      k = wrap_mesh_index(base[2] + i2, mesh); \
	      *cf_cnt++ = cur_ca_frac_val; \
	      Qmesh[indy + k] += cur_ca_frac_val; \
	    } \
	  } \
	}  \
    } \
} \


assign_charge_real_template(1)
assign_charge_real_template(2)
assign_charge_real_template(3)
assign_charge_real_template(4)
assign_charge_real_template(5)
assign_charge_real_template(6)
assign_charge_real_template(7)

void assign_charge_real(system_t *s, parameters_t *p, data_t *d) {
  switch(p->cao) {
  case 1:
    assign_charge_real_1(s, p, d);
    break;
  case 2:
    assign_charge_real_2(s, p, d);
    break;
  case 3:
    assign_charge_real_3(s, p, d);
    break;
  case 4:
    assign_charge_real_4(s, p, d);
    break;
  case 5:
    assign_charge_real_5(s, p, d);
    break;
  case 6:
    assign_charge_real_6(s, p, d);
    break;
  case 7:
    assign_charge_real_7(s, p, d);
    break;
  default:
    fprintf(stderr, "Charge assinment order %d not known.", p->cao);
    break;
  }
}
 
#define assign_charge_real_res_template(cao) void assign_charge_real_res_##cao(system_t *s, parameters_t *p, data_t *d) \
{ \
    int dim, i0, i1, i2, id; \
    FLOAT_TYPE tmp0, tmp1; \
    FLOAT_TYPE pos; \
    int nmp; \
    int arg[3]; \
    FLOAT_TYPE cur_ca_frac_val; \
    FLOAT_TYPE *cf_cnt; \
    int base[3]; \
    int i,j,k; \
    const FLOAT_TYPE MI2 = 2.0*(FLOAT_TYPE)MaxInterpol; \
 \
    const FLOAT_TYPE Hi = (double)d->mesh/(double)s->length; \
 \
    FLOAT_TYPE * restrict cf = d->cf[0]; \
    FLOAT_TYPE ** restrict interpol = d->inter->interpol; \
    FLOAT_TYPE * restrict Qmesh = d->Qmesh; \
    FLOAT_TYPE q; \
    const int mesh = d->mesh; \
    int indx, indy; \
 \
    FLOAT_TYPE pos_shift; \
 \
    pos_shift = (FLOAT_TYPE)((cao-1)/2); \
 \
    for (id=0;id<s->nparticles;id++) { \
        for (dim=0;dim<3;dim++) { \
            pos    = s->p->fields[dim][id]*Hi - pos_shift; \
            nmp = int_floor(pos + 0.5); \
	    base[dim]  = wrap_mesh_index( nmp, d->mesh); \
            arg[dim] = int_floor((pos - nmp + 0.5)*MI2); \
            d->ca_ind[0][3*id + dim] = base[dim]; \
        }	q = s->q[id]; \
        cf_cnt = cf + id*cao*cao*cao; \
	for (i0=0; i0<cao; i0++) { \
	  i = wrap_mesh_index(base[0] + i0, mesh); \
	  indx = mesh*(mesh+2) * i; \
	  tmp0 = q * interpol[arg[0]][i0]; \
	  for (i1=0; i1<cao; i1++) { \
	    tmp1 = tmp0 * interpol[arg[1]][i1]; \
	    j = wrap_mesh_index(base[1] + i1, mesh); \
	    indy = indx + (mesh+2) * j; \
	    for (i2=0; i2<cao; i2++) { \
	      cur_ca_frac_val = tmp1 * interpol[arg[2]][i2]; \
	      k = wrap_mesh_index(base[2] + i2, mesh); \
	      *cf_cnt++ = cur_ca_frac_val;	       \
	      Qmesh[indy + k] += cur_ca_frac_val; \
	    } \
	  } \
	}  \
    } \
} \

assign_charge_real_res_template(1)
assign_charge_real_res_template(2)
assign_charge_real_res_template(3)
assign_charge_real_res_template(4)
assign_charge_real_res_template(5)
assign_charge_real_res_template(6)
assign_charge_real_res_template(7)

void assign_charge_real_res(system_t *s, parameters_t *p, data_t *d) {
  switch(p->cao) {
  case 1:
    assign_charge_real_res_1(s, p, d);
    break;
  case 2:
    assign_charge_real_res_2(s, p, d);
    break;
  case 3:
    assign_charge_real_res_3(s, p, d);
    break;
  case 4:
    assign_charge_real_res_4(s, p, d);
    break;
  case 5:
    assign_charge_real_res_5(s, p, d);
    break;
  case 6:
    assign_charge_real_res_6(s, p, d);
    break;
  case 7:
    assign_charge_real_res_7(s, p, d);
    break;
  default:
    fprintf(stderr, "Charge assinment order %d not known.", p->cao);
    break;
  }
}

void assign_charge_real_nostor_dynamic(system_t *s, parameters_t *p, data_t *d)
{
    int dim, i0, i1, i2, id;
    FLOAT_TYPE tmp0, tmp1;
    /* position of a particle in local mesh units */
    FLOAT_TYPE pos;
    /* 1d-index of nearest mesh point */
    int nmp;
    /* index for caf interpolation grid */
    int arg[3];
    /* index, index jumps for rs_mesh array */
    // Mesh coordinates of the closest mesh point
    int base[3];
    int i,j,k;
    FLOAT_TYPE MI2 = 2.0*(FLOAT_TYPE)MaxInterpol;

    FLOAT_TYPE Hi = (double)d->mesh/(double)s->length;

    FLOAT_TYPE **interpol = d->inter->interpol;
    FLOAT_TYPE *Qmesh = d->Qmesh;
    FLOAT_TYPE q;
    const int cao = p->cao;
    const int mesh = d->mesh;

    // Make sure parameter-set and data-set are compatible

    FLOAT_TYPE pos_shift;

    /* Shift for odd charge assignment order */
    pos_shift = (FLOAT_TYPE)((p->cao-1)/2);

    for (id=0;id<s->nparticles;id++) {
        /* particle position in mesh coordinates */
        for (dim=0;dim<3;dim++) {
            pos    = s->p->fields[dim][id]*Hi - pos_shift;
            nmp = int_floor(pos + 0.5);
	    base[dim]  = wrap_mesh_index( nmp, d->mesh);
            arg[dim] = int_floor((pos - nmp + 0.5)*MI2);
        }	
	q = s->q[id];
	for (i0=0; i0<cao; i0++) {
	  i = wrap_mesh_index(base[0] + i0, mesh);
	  tmp0 = q * interpol[arg[0]][i0];
	  for (i1=0; i1<cao; i1++) {
	    tmp1 = tmp0 * interpol[arg[1]][i1];
	    j = wrap_mesh_index(base[1] + i1, mesh);
	    for (i2=0; i2<cao; i2++) {
	      k = wrap_mesh_index(base[2] + i2, mesh);
	      Qmesh[mesh*(mesh+2) * i + (mesh+2) * j + k] += tmp1 * interpol[arg[2]][i2];
	    }
	  }
	} 
    }
}

void assign_charge_real_nostor_res_dynamic(system_t *s, parameters_t *p, data_t *d)
{
    int dim, i0, i1, i2, id;
    FLOAT_TYPE tmp0, tmp1;
    /* position of a particle in local mesh units */
    FLOAT_TYPE pos;
    /* 1d-index of nearest mesh point */
    int nmp;
    /* index for caf interpolation grid */
    int arg[3];
    /* index, index jumps for rs_mesh array */
    // Mesh coordinates of the closest mesh point
    int base[3];
    int i,j,k;
    FLOAT_TYPE MI2 = 2.0*(FLOAT_TYPE)MaxInterpol;

    FLOAT_TYPE Hi = (double)d->mesh/(double)s->length;

    FLOAT_TYPE ** restrict interpol = d->inter->interpol;
    FLOAT_TYPE * restrict Qmesh = d->Qmesh;
    FLOAT_TYPE q;
    const int cao = p->cao;
    const int mesh = d->mesh;

    // Make sure parameter-set and data-set are compatible

    FLOAT_TYPE pos_shift;

    /* Shift for odd charge assignment order */
    pos_shift = (FLOAT_TYPE)((p->cao-1)/2);

    for (id=0;id<s->nparticles;id++) {
        /* particle position in mesh coordinates */
        for (dim=0;dim<3;dim++) {
            pos    = s->p->fields[dim][id]*Hi - pos_shift;
            nmp = int_floor(pos + 0.5);
	    base[dim]  = wrap_mesh_index( nmp, d->mesh);
            arg[dim] = int_floor((pos - nmp + 0.5)*MI2);
        }	
	q = s->q[id];
	for (i0=0; i0<cao; i0++) {
	  i = wrap_mesh_index(base[0] + i0, mesh);
	  tmp0 = q * interpol[arg[0]][i0];
	  for (i1=0; i1<cao; i1++) {
	    tmp1 = tmp0 * interpol[arg[1]][i1];
	    j = wrap_mesh_index(base[1] + i1, mesh);
	    for (i2=0; i2<cao; i2++) {
	      k = wrap_mesh_index(base[2] + i2, mesh);
	      Qmesh[mesh*(mesh+2) * i + (mesh+2) * j + k] += tmp1 * interpol[arg[2]][i2];
	    }
	  }
	} 
    }
}

#define assign_charge_real_nostor_template(cao) void assign_charge_real_nostor_##cao(system_t *s, parameters_t *p, data_t *d) \
{ \
    int dim, i0, i1, i2, id; \
    FLOAT_TYPE tmp0, tmp1; \
    /* position of a particle in local mesh units */ \
    FLOAT_TYPE pos; \
    /* 1d-index of nearest mesh point */ \
    int nmp; \
    /* index for caf interpolation grid */ \
    int arg[3]; \
    /* index, index jumps for rs_mesh array */ \
    int base[3]; \
    int i,j,k; \
    FLOAT_TYPE MI2 = 2.0*(FLOAT_TYPE)MaxInterpol; \
 \
    FLOAT_TYPE Hi = (double)d->mesh/(double)s->length; \
 \
    FLOAT_TYPE ** restrict interpol = d->inter->interpol; \
    FLOAT_TYPE * restrict Qmesh = d->Qmesh; \
    FLOAT_TYPE q; \
    const int mesh = d->mesh; \
 \
 \
    FLOAT_TYPE pos_shift; \
 \
    /* Shift for odd charge assignment order */ \
    pos_shift = (FLOAT_TYPE)((cao-1)/2); \
 \
    for (id=0;id<s->nparticles;id++) { \
        /* particle position in mesh coordinates */ \
        for (dim=0;dim<3;dim++) { \
            pos    = s->p->fields[dim][id]*Hi - pos_shift; \
            nmp = int_floor(pos + 0.5); \
	    base[dim]  = wrap_mesh_index( nmp, d->mesh); \
            arg[dim] = int_floor((pos - nmp + 0.5)*MI2); \
        }	 \
	q = s->q[id]; \
	for (i0=0; i0<cao; i0++) { \
	  i = wrap_mesh_index(base[0] + i0, mesh); \
	  tmp0 = q * interpol[arg[0]][i0]; \
	  for (i1=0; i1<cao; i1++) { \
	    tmp1 = tmp0 * interpol[arg[1]][i1]; \
	    j = wrap_mesh_index(base[1] + i1, mesh); \
	    for (i2=0; i2<cao; i2++) { \
	      k = wrap_mesh_index(base[2] + i2, mesh); \
	      Qmesh[mesh*(mesh+2) * i + (mesh+2) * j + k] += tmp1 * interpol[arg[2]][i2]; \
	    } \
	  } \
	}  \
    } \
}


assign_charge_real_nostor_template(1)
assign_charge_real_nostor_template(2)
assign_charge_real_nostor_template(3)
assign_charge_real_nostor_template(4)
assign_charge_real_nostor_template(5)
assign_charge_real_nostor_template(6)
assign_charge_real_nostor_template(7)

void assign_charge_real_nostor(system_t *s, parameters_t *p, data_t *d) {
  switch(p->cao) {
  case 1:
    assign_charge_real_nostor_1(s, p, d);
    break;
  case 2:
    assign_charge_real_nostor_2(s, p, d);
    break;
  case 3:
    assign_charge_real_nostor_3(s, p, d);
    break;
  case 4:
    assign_charge_real_nostor_4(s, p, d);
    break;
  case 5:
    assign_charge_real_nostor_5(s, p, d);
    break;
  case 6:
    assign_charge_real_nostor_6(s, p, d);
    break;
  case 7:
    assign_charge_real_nostor_7(s, p, d);
    break;
  default:
    fprintf(stderr, "Charge assinment order %d not known.", p->cao);
    break;
  }
}



// assign the forces obtained from k-space
void assign_forces(FLOAT_TYPE force_prefac, system_t *s, parameters_t *p, data_t *d, forces_t *f, int ii) {
  int i,i0,i1,i2;
  FLOAT_TYPE *cf_cnt;
  int *base;
  int j,k,l;
  FLOAT_TYPE B;
  FLOAT_TYPE field_x, field_y, field_z;
  int l_ind;
  FLOAT_TYPE *fmesh_x = d->Fmesh->fields[0], *fmesh_y = d->Fmesh->fields[1], *fmesh_z = d->Fmesh->fields[2];
  int mesh = d->mesh;

  const int cao = p->cao;

  cf_cnt = d->cf[ii];

  for (i=0; i<s->nparticles; i++) {
    field_x = field_y = field_z = 0;
    base = d->ca_ind[ii] + 3*i;
    for (i0=0; i0<cao; i0++) {
      j = wrap_mesh_index(base[0] + i0, mesh);
      for (i1=0; i1<cao; i1++) {
	k = wrap_mesh_index(base[1] + i1, mesh);
	for (i2=0; i2<cao; i2++) {
	  l = wrap_mesh_index(base[2] + i2, mesh);
	  B = force_prefac*(*cf_cnt++);
	  l_ind = c_ind(j,k,l)+ii;
	  field_x -= fmesh_x[l_ind]*B;
	  field_y -= fmesh_y[l_ind]*B;
	  field_z -= fmesh_z[l_ind]*B;
	}
      }
    }
    f->f_k->fields[0][i] += field_x;
    f->f_k->fields[1][i] += field_y;
    f->f_k->fields[2][i] += field_z;

    if (ii==1) {
      f->f_k->fields[0][i] *= 0.5;
      f->f_k->fields[1][i] *= 0.5;
      f->f_k->fields[2][i] *= 0.5;
    }
  }
}

// assign the forces obtained from k-space
#define assign_forces_interlacing_template(cao) void assign_forces_interlacing_##cao(FLOAT_TYPE force_prefac, system_t *s, parameters_t *p, data_t *d, forces_t *f) { \
  int i,i0,i1,i2; \
  FLOAT_TYPE * restrict cf_cnt1, * restrict cf_cnt2; \
  int * restrict base1, * restrict base2; \
  int j1,k1,l1,j2,k2,l2; \
  FLOAT_TYPE B1, B2; \
  FLOAT_TYPE field_x, field_y, field_z; \
  int l_ind1, l_ind2; \
  const FLOAT_TYPE * restrict fmesh_x = d->Fmesh->fields[0], * restrict fmesh_y = d->Fmesh->fields[1], * restrict fmesh_z = d->Fmesh->fields[2]; \
 \
  const int mesh = d->mesh; \
 \
  cf_cnt1 = d->cf[0]; \
  cf_cnt2 = d->cf[1]; \
 \
  for (i=0; i<s->nparticles; i++) { \
    field_x = field_y = field_z = 0; \
    base1 = d->ca_ind[0] + 3*i; \
    base2 = d->ca_ind[1] + 3*i; \
    for (i0=0; i0<cao; i0++) { \
      j1 = wrap_mesh_index(base1[0] + i0, mesh); \
      j2 = wrap_mesh_index(base2[0] + i0, mesh); \
      for (i1=0; i1<cao; i1++) { \
	k1 = wrap_mesh_index(base1[1] + i1, mesh); \
	k2 = wrap_mesh_index(base2[1] + i1, mesh); \
	for (i2=0; i2<cao; i2++) { \
	  l1 = wrap_mesh_index(base1[2] + i2, mesh); \
	  l2 = wrap_mesh_index(base2[2] + i2, mesh); \
	  B1 = 0.5*force_prefac*(*cf_cnt1++); \
	  B2 = 0.5*force_prefac*(*cf_cnt2++); \
	  l_ind1 = c_ind(j1,k1,l1); \
	  l_ind2 = c_ind(j2,k2,l2); \
	  field_x -= fmesh_x[l_ind1]*B1 + fmesh_x[l_ind2+1]*B2; \
	  field_y -= fmesh_y[l_ind1]*B1 + fmesh_y[l_ind2+1]*B2; \
	  field_z -= fmesh_z[l_ind1]*B1 + fmesh_z[l_ind2+1]*B2; \
	} \
      } \
    } \
    f->f_k->fields[0][i] += field_x; \
    f->f_k->fields[1][i] += field_y; \
    f->f_k->fields[2][i] += field_z; \
  } \
} \

assign_forces_interlacing_template(1)
assign_forces_interlacing_template(2)
assign_forces_interlacing_template(3)
assign_forces_interlacing_template(4)
assign_forces_interlacing_template(5)
assign_forces_interlacing_template(6)
assign_forces_interlacing_template(7)

void assign_forces_interlacing(FLOAT_TYPE prefactor, system_t *s, parameters_t *p, data_t *d, forces_t *f) {
  switch(p->cao) {
  case 1:
    assign_forces_interlacing_1(prefactor, s, p, d, f);
    break;
  case 2:
    assign_forces_interlacing_2(prefactor, s, p, d, f);
    break;
  case 3:
    assign_forces_interlacing_3(prefactor, s, p, d, f);
    break;
  case 4:
    assign_forces_interlacing_4(prefactor, s, p, d, f);
    break;
  case 5:
    assign_forces_interlacing_5(prefactor, s, p, d, f);
    break;
  case 6:
    assign_forces_interlacing_6(prefactor, s, p, d, f);
    break;
  case 7:
    assign_forces_interlacing_7(prefactor, s, p, d, f);
    break;
  default:
    fprintf(stderr, "Charge assinment order %d not known.", p->cao);
    break;
  }
}

#define assign_forces_interlacing_ad_template(cao) void assign_forces_interlacing_ad_##cao(FLOAT_TYPE force_prefac, system_t *s, parameters_t *p, data_t *d, forces_t *f) { \
  int i,i0,i1,i2; \
  int cf_cnt; \
  int *base1, *base2; \
  int j1,k1,l1,j2,k2,l2; \
  FLOAT_TYPE B1, B2; \
  FLOAT_TYPE field_x, field_y, field_z; \
  int mesh = d->mesh; \
 \
  FLOAT_TYPE *dQ0 = d->dQ[0]; \
 \
  FLOAT_TYPE *dQ1 = d->dQ[1]; \
 \
  cf_cnt = 0; \
 \
  for (i=0; i<s->nparticles; i++) { \
    field_x = field_y = field_z = 0; \
    base1 = d->ca_ind[0] + 3*i; \
    base2 = d->ca_ind[1] + 3*i; \
    cf_cnt = 3*i*cao*cao*cao; \
    for (i0=0; i0<cao; i0++) { \
      j1 = wrap_mesh_index(base1[0] + i0, mesh); \
      j2 = wrap_mesh_index(base2[0] + i0, mesh); \
      for (i1=0; i1<cao; i1++) { \
	k1 = wrap_mesh_index(base1[1] + i1, mesh); \
	k2 = wrap_mesh_index(base2[1] + i1, mesh); \
	for (i2=0; i2<cao; i2++) { \
	  l1 = wrap_mesh_index(base1[2] + i2, mesh); \
	  l2 = wrap_mesh_index(base2[2] + i2, mesh); \
	  B1 = d->Qmesh[c_ind(j1,k1,l1)+0]; \
	  B2 = d->Qmesh[c_ind(j2,k2,l2)+1]; \
	  field_x -= 0.5*force_prefac*(B1*dQ0[cf_cnt+0] + B2*dQ1[cf_cnt+0]); \
	  field_y -= 0.5*force_prefac*(B1*dQ0[cf_cnt+1] + B2*dQ1[cf_cnt+1]); \
	  field_z -= 0.5*force_prefac*(B1*dQ0[cf_cnt+2] + B2*dQ1[cf_cnt+2]); \
	  cf_cnt+=3; \
	} \
      } \
    } \
    f->f_k->fields[0][i] += field_x; \
    f->f_k->fields[1][i] += field_y; \
    f->f_k->fields[2][i] += field_z; \
  } \
} \

assign_forces_interlacing_ad_template(1)
assign_forces_interlacing_ad_template(2)
assign_forces_interlacing_ad_template(3)
assign_forces_interlacing_ad_template(4)
assign_forces_interlacing_ad_template(5)
assign_forces_interlacing_ad_template(6)
assign_forces_interlacing_ad_template(7)

void assign_forces_interlacing_ad(FLOAT_TYPE prefactor, system_t *s, parameters_t *p, data_t *d, forces_t *f) {
  switch(p->cao) {
  case 1:
    assign_forces_interlacing_ad_1(prefactor, s, p, d, f);
    break;
  case 2:
    assign_forces_interlacing_ad_2(prefactor, s, p, d, f);
    break;
  case 3:
    assign_forces_interlacing_ad_3(prefactor, s, p, d, f);
    break;
  case 4:
    assign_forces_interlacing_ad_4(prefactor, s, p, d, f);
    break;
  case 5:
    assign_forces_interlacing_ad_5(prefactor, s, p, d, f);
    break;
  case 6:
    assign_forces_interlacing_ad_6(prefactor, s, p, d, f);
    break;
  case 7:
    assign_forces_interlacing_ad_7(prefactor, s, p, d, f);
    break;
  default:
    fprintf(stderr, "Charge assinment order %d not known.", p->cao);
    break;
  }
}


void assign_forces_real_dynamic(FLOAT_TYPE force_prefac, system_t *s, parameters_t *p, data_t *d, forces_t *f) {
  int i,i0,i1,i2;
  FLOAT_TYPE *cf_cnt;
  int *base;
  int j,k,l;
  FLOAT_TYPE B;
  FLOAT_TYPE field_x, field_y, field_z;
  int l_ind;
  FLOAT_TYPE * restrict fmesh_x = d->Fmesh->fields[0], * restrict fmesh_y = d->Fmesh->fields[1], * restrict fmesh_z = d->Fmesh->fields[2];
  int mesh = d->mesh;

  const int cao = p->cao;

  cf_cnt = d->cf[0];

  for (i=0; i<s->nparticles; i++) {
    field_x = field_y = field_z = 0;
    base = d->ca_ind[0] + 3*i;
    for (i0=0; i0<cao; i0++) {
      j = wrap_mesh_index(base[0] + i0, mesh);
      for (i1=0; i1<cao; i1++) {
	k = wrap_mesh_index(base[1] + i1, mesh);
	for (i2=0; i2<cao; i2++) {
	  l = wrap_mesh_index(base[2] + i2, mesh);
	  B = force_prefac*(*cf_cnt++);
	  l_ind = mesh*(mesh+2) * j + (mesh+2) * k + l;
	  field_x -= fmesh_x[l_ind]*B;
	  field_y -= fmesh_y[l_ind]*B;
	  field_z -= fmesh_z[l_ind]*B;
	}
      }
    }
    f->f_k->fields[0][i] += field_x;
    f->f_k->fields[1][i] += field_y;
    f->f_k->fields[2][i] += field_z;

  }

}

#define assign_forces_real_template(cao) void assign_forces_real_##cao(FLOAT_TYPE force_prefac, system_t *s, parameters_t *p, data_t *d, forces_t *f) { \
  int i,i0,i1,i2; \
  FLOAT_TYPE * restrict cf_cnt; \
  int * restrict base; \
  int j,k,l; \
  FLOAT_TYPE B; \
  FLOAT_TYPE field_x, field_y, field_z; \
  int l_ind; \
  FLOAT_TYPE * restrict fmesh_x = d->Fmesh->fields[0], * restrict fmesh_y = d->Fmesh->fields[1], * restrict fmesh_z = d->Fmesh->fields[2]; \
  int mesh = d->mesh; \
 \
  cf_cnt = d->cf[0]; \
 \
  for (i=0; i<s->nparticles; i++) { \
    field_x = field_y = field_z = 0; \
    base = d->ca_ind[0] + 3*i; \
    for (i0=0; i0<cao; i0++) { \
      j = wrap_mesh_index(base[0] + i0, mesh); \
      for (i1=0; i1<cao; i1++) { \
	k = wrap_mesh_index(base[1] + i1, mesh); \
	for (i2=0; i2<cao; i2++) { \
	  l = wrap_mesh_index(base[2] + i2, mesh); \
	  B = force_prefac*(*cf_cnt++); \
	  l_ind = mesh*(mesh+2) * j + (mesh+2) * k + l; \
	  field_x -= fmesh_x[l_ind]*B; \
	  field_y -= fmesh_y[l_ind]*B; \
	  field_z -= fmesh_z[l_ind]*B; \
	} \
      } \
    } \
    f->f_k->fields[0][i] += field_x; \
    f->f_k->fields[1][i] += field_y; \
    f->f_k->fields[2][i] += field_z; \
 \
  } \
 \
} \

assign_forces_real_template(1)
assign_forces_real_template(2)
assign_forces_real_template(3)
assign_forces_real_template(4)
assign_forces_real_template(5)
assign_forces_real_template(6)
assign_forces_real_template(7)

void assign_forces_real(FLOAT_TYPE prefactor, system_t *s, parameters_t *p, data_t *d, forces_t *f) {
  switch(p->cao) {
  case 1:
    assign_forces_real_1(prefactor, s, p, d, f);
    break;
  case 2:
    assign_forces_real_2(prefactor, s, p, d, f);
    break;
  case 3:
    assign_forces_real_3(prefactor, s, p, d, f);
    break;
  case 4:
    assign_forces_real_4(prefactor, s, p, d, f);
    break;
  case 5:
    assign_forces_real_5(prefactor, s, p, d, f);
    break;
  case 6:
    assign_forces_real_6(prefactor, s, p, d, f);
    break;
  case 7:
    assign_forces_real_7(prefactor, s, p, d, f);
    break;
  default:
    fprintf(stderr, "Charge assinment order %d not known.", p->cao);
    break;
  }
}


void assign_forces_real_nostor_dynamic(FLOAT_TYPE force_prefac, system_t *s, parameters_t *p, data_t *d, forces_t *f) {
  int i,i0,i1,i2;
  FLOAT_TYPE pos;
  int base[3];
  int arg[3];
  int nmp, dim;
  int j,k,l;
  FLOAT_TYPE B;
  FLOAT_TYPE field_x, field_y, field_z;
  int l_ind;
  FLOAT_TYPE * restrict fmesh_x = d->Fmesh->fields[0], * restrict fmesh_y = d->Fmesh->fields[1], * restrict fmesh_z = d->Fmesh->fields[2];
  const int mesh = d->mesh;
  FLOAT_TYPE MI2 = 2.0*(FLOAT_TYPE)MaxInterpol;
  FLOAT_TYPE Hi = (double)d->mesh/(double)s->length;
  FLOAT_TYPE ** restrict interpol = d->inter->interpol;
  const int cao = p->cao;
  FLOAT_TYPE tmp0, tmp1;
  const FLOAT_TYPE pos_shift = (FLOAT_TYPE)((p->cao-1)/2);
  FLOAT_TYPE q;

  for (i=0; i<s->nparticles; i++) {
    field_x = field_y = field_z = 0;
    for (dim=0;dim<3;dim++) {
      pos    = s->p->fields[dim][i]*Hi - pos_shift;
      nmp = int_floor(pos + 0.5);
      base[dim]  = wrap_mesh_index( nmp, d->mesh);
      arg[dim] = int_floor((pos - nmp + 0.5)*MI2);
    }
    q = s->q[i];
    for (i0=0; i0<cao; i0++) {
      j = wrap_mesh_index(base[0] + i0, mesh);
      tmp0 = q * interpol[arg[0]][i0];
      for (i1=0; i1<cao; i1++) {
	tmp1 = tmp0 * interpol[arg[1]][i1];
	k = wrap_mesh_index(base[1] + i1, mesh);
	for (i2=0; i2<cao; i2++) {
	  l = wrap_mesh_index(base[2] + i2, mesh);
	  B = force_prefac* tmp1 * interpol[arg[2]][i2];
	  l_ind = mesh*(mesh+2) * j + (mesh+2) * k + l;
	  field_x -= fmesh_x[l_ind]*B;
	  field_y -= fmesh_y[l_ind]*B;
	  field_z -= fmesh_z[l_ind]*B;
	}
      }
    }
    f->f_k->fields[0][i] += field_x;
    f->f_k->fields[1][i] += field_y;
    f->f_k->fields[2][i] += field_z;

  }

}

#define assign_forces_real_nostor_template(cao) void assign_forces_real_nostor_##cao(FLOAT_TYPE force_prefac, system_t *s, parameters_t *p, data_t *d, forces_t *f) { \
  int i,i0,i1,i2; \
  FLOAT_TYPE pos; \
  int base[3]; \
  int arg[3]; \
  int nmp, dim; \
  int j,k,l; \
  FLOAT_TYPE B; \
  FLOAT_TYPE field_x, field_y, field_z; \
  int l_ind; \
  FLOAT_TYPE * restrict fmesh_x = d->Fmesh->fields[0], * restrict fmesh_y = d->Fmesh->fields[1], * restrict fmesh_z = d->Fmesh->fields[2]; \
  const int mesh = d->mesh; \
  FLOAT_TYPE MI2 = 2.0*(FLOAT_TYPE)MaxInterpol; \
  FLOAT_TYPE Hi = (double)d->mesh/(double)s->length; \
  FLOAT_TYPE ** restrict interpol = d->inter->interpol; \
  FLOAT_TYPE tmp0, tmp1; \
  const FLOAT_TYPE pos_shift = (FLOAT_TYPE)((cao-1)/2); \
  FLOAT_TYPE q; \
 \
  for (i=0; i<s->nparticles; i++) { \
    field_x = field_y = field_z = 0; \
    for (dim=0;dim<3;dim++) { \
      pos    = s->p->fields[dim][i]*Hi - pos_shift; \
      nmp = int_floor(pos + 0.5); \
      base[dim]  = wrap_mesh_index( nmp, d->mesh); \
      arg[dim] = int_floor((pos - nmp + 0.5)*MI2); \
    } \
    q = s->q[i]; \
    for (i0=0; i0<cao; i0++) { \
      j = wrap_mesh_index(base[0] + i0, mesh); \
      tmp0 = q * interpol[arg[0]][i0]; \
      for (i1=0; i1<cao; i1++) { \
	tmp1 = tmp0 * interpol[arg[1]][i1]; \
	k = wrap_mesh_index(base[1] + i1, mesh); \
	for (i2=0; i2<cao; i2++) { \
	  l = wrap_mesh_index(base[2] + i2, mesh); \
	  B = force_prefac* tmp1 * interpol[arg[2]][i2]; \
	  l_ind = mesh*(mesh+2) * j + (mesh+2) * k + l; \
	  field_x -= fmesh_x[l_ind]*B; \
	  field_y -= fmesh_y[l_ind]*B; \
	  field_z -= fmesh_z[l_ind]*B; \
	} \
      } \
    } \
    f->f_k->fields[0][i] += field_x; \
    f->f_k->fields[1][i] += field_y; \
    f->f_k->fields[2][i] += field_z; \
 \
  } \
 \
} \

assign_forces_real_nostor_template(1)
assign_forces_real_nostor_template(2)
assign_forces_real_nostor_template(3)
assign_forces_real_nostor_template(4)
assign_forces_real_nostor_template(5)
assign_forces_real_nostor_template(6)
assign_forces_real_nostor_template(7)

void assign_forces_real_nostor(FLOAT_TYPE prefactor, system_t *s, parameters_t *p, data_t *d, forces_t *f) {
  switch(p->cao) {
  case 1:
    assign_forces_real_nostor_1(prefactor, s, p, d, f);
    break;
  case 2:
    assign_forces_real_nostor_2(prefactor, s, p, d, f);
    break;
  case 3:
    assign_forces_real_nostor_3(prefactor, s, p, d, f);
    break;
  case 4:
    assign_forces_real_nostor_4(prefactor, s, p, d, f);
    break;
  case 5:
    assign_forces_real_nostor_5(prefactor, s, p, d, f);
    break;
  case 6:
    assign_forces_real_nostor_6(prefactor, s, p, d, f);
    break;
  case 7:
    assign_forces_real_nostor_7(prefactor, s, p, d, f);
    break;
  default:
    fprintf(stderr, "Charge assinment order %d not known.", p->cao);
    break;
  }
}

#define assign_charge_and_derivatives_template(cao) void assign_charge_and_derivatives_##cao(system_t *s, parameters_t *p, data_t *d, int ii) \
{ \
    int dim, i0, i1, i2; \
    int id; \
    FLOAT_TYPE tmp0, tmp1, tmp2; \
    FLOAT_TYPE tmp0_x, tmp1_y, tmp2_z; \
    /* position of a particle in local mesh units */ \
    FLOAT_TYPE pos; \
    /* 1d-index of nearest mesh point */ \
    int nmp; \
    /* index for caf interpolation grid */ \
    int arg[3]; \
    /* index, index jumps for rs_mesh array */ \
    int cf_cnt; \
 \
    int base[3]; \
    int i,j,k; \
    int Mesh = p->mesh; \
    FLOAT_TYPE MI2 = 2.0*(FLOAT_TYPE)MaxInterpol; \
 \
    FLOAT_TYPE Hi = (double)Mesh/s->length; \
    FLOAT_TYPE Leni = 1.0/s->length; \
    FLOAT_TYPE q, qLeni; \
 \
    double pos_shift; \
 \
    FLOAT_TYPE * restrict Qmesh = d->Qmesh; \
    FLOAT_TYPE * restrict dQ = d->dQ[ii]; \
 \
    pos_shift = (double)((cao-1)/2); \
    /* particle position in mesh coordinates */ \
    for (id=0;id<s->nparticles;id++) { \
        cf_cnt = 3*id*cao*cao*cao; \
	q = s->q[id] ; \
	qLeni = q * Leni; \
        for (dim=0;dim<3;dim++) { \
	  pos    = s->p->fields[dim][id]*Hi - pos_shift + 0.5*ii; \
	  nmp = int_floor(pos + 0.5); \
	  base[dim]  = wrap_mesh_index( nmp, d->mesh); \
	  arg[dim] = int_floor((pos - nmp + 0.5)*MI2); \
	  d->ca_ind[ii][3*id + dim] = base[dim]; \
        } \
 \
        for (i0=0; i0<cao; i0++) { \
	  i = wrap_mesh_index(base[0] + i0, d->mesh); \
	  tmp0 = d->inter->interpol[arg[0]][i0]; \
	  tmp0_x = d->inter->interpol_d[arg[0]][i0]; \
	  for (i1=0; i1<cao; i1++) { \
	    j = wrap_mesh_index(base[1] + i1, d->mesh); \
	    tmp1 = d->inter->interpol[arg[1]][i1]; \
	    tmp1_y = d->inter->interpol_d[arg[1]][i1]; \
	    for (i2=0; i2<cao; i2++) { \
	      k = wrap_mesh_index(base[2] + i2, d->mesh); \
	      tmp2 = d->inter->interpol[arg[2]][i2]; \
	      tmp2_z = d->inter->interpol_d[arg[2]][i2]; \
 \
	      dQ[cf_cnt+0] = tmp0_x * tmp1 * tmp2 * qLeni; \
	      dQ[cf_cnt+1] = tmp0 * tmp1_y * tmp2 * qLeni; \
	      dQ[cf_cnt+2] = tmp0 * tmp1 * tmp2_z * qLeni; \
 \
	      Qmesh[c_ind(i,j,k)+ii] += q * tmp0 * tmp1 * tmp2; \
	      cf_cnt+=3; \
	    } \
	  } \
        } \
    } \
} \

assign_charge_and_derivatives_template(1)
assign_charge_and_derivatives_template(2)
assign_charge_and_derivatives_template(3)
assign_charge_and_derivatives_template(4)
assign_charge_and_derivatives_template(5)
assign_charge_and_derivatives_template(6)
assign_charge_and_derivatives_template(7)

void assign_charge_and_derivatives(system_t *s, parameters_t *p, data_t *d, int ii) {
  switch(p->cao) {
  case 1:
    assign_charge_and_derivatives_1(s, p, d, ii);
    break;
  case 2:
    assign_charge_and_derivatives_2(s, p, d, ii);
    break;
  case 3:
    assign_charge_and_derivatives_3(s, p, d, ii);
    break;
  case 4:
    assign_charge_and_derivatives_4(s, p, d, ii);
    break;
  case 5:
    assign_charge_and_derivatives_5(s, p, d, ii);
    break;
  case 6:
    assign_charge_and_derivatives_6(s, p, d, ii);
    break;
  case 7:
    assign_charge_and_derivatives_7(s, p, d, ii);
    break;
  default:
    fprintf(stderr, "Charge assinment order %d not known.", p->cao);
    break;
  }
}


void assign_charge_and_derivatives_real_dynamic(system_t *s, parameters_t *p, data_t *d)
{
    int dim, i0, i1, i2;
    int id;
    FLOAT_TYPE tmp0, tmp1, tmp2;
    FLOAT_TYPE tmp0_x, tmp1_y, tmp2_z;
    /* position of a particle in local mesh units */
    FLOAT_TYPE pos;
    /* 1d-index of nearest mesh point */
    int nmp;
    /* index for caf interpolation grid */
    int arg[3];
    /* index, index jumps for rs_mesh array */
    int cf_cnt;

    int base[3];
    int i,j,k;
    const int Mesh = p->mesh;
    FLOAT_TYPE MI2 = 2.0*(FLOAT_TYPE)MaxInterpol;

    FLOAT_TYPE Hi = (double)Mesh/s->length;
    FLOAT_TYPE Leni = 1.0/s->length;
    FLOAT_TYPE q, qLeni;

    FLOAT_TYPE * restrict Qmesh = d->Qmesh;
    FLOAT_TYPE * restrict dQ = d->dQ[0];
    FLOAT_TYPE ** restrict interpol = d->inter->interpol;
    FLOAT_TYPE ** restrict interpol_d = d->inter->interpol_d;

    double pos_shift;

    pos_shift = (double)((p->cao-1)/2);
    /* particle position in mesh coordinates */
    for (id=0;id<s->nparticles;id++) {
        cf_cnt = 3*id*p->cao3;
	q = s->q[id] ;
	qLeni = q * Leni;
        for (dim=0;dim<3;dim++) {
	  pos    = s->p->fields[dim][id]*Hi - pos_shift;
	  nmp = int_floor(pos + 0.5);
	  base[dim]  = wrap_mesh_index( nmp, Mesh);
	  arg[dim] = int_floor((pos - nmp + 0.5)*MI2);
	  d->ca_ind[0][3*id + dim] = base[dim];
        }

        for (i0=0; i0<p->cao; i0++) {
	  i = wrap_mesh_index(base[0] + i0, Mesh);
	  tmp0 = interpol[arg[0]][i0];
	  tmp0_x = interpol_d[arg[0]][i0];
	  for (i1=0; i1<p->cao; i1++) {
	    j = wrap_mesh_index(base[1] + i1, Mesh);
	    tmp1 = interpol[arg[1]][i1];
	    tmp1_y = interpol_d[arg[1]][i1];
	    for (i2=0; i2<p->cao; i2++) {
	      k = wrap_mesh_index(base[2] + i2, Mesh);
	      tmp2 = interpol[arg[2]][i2];
	      tmp2_z = interpol_d[arg[2]][i2];

	      dQ[cf_cnt+0] = tmp0_x * tmp1 * tmp2 * qLeni;
	      dQ[cf_cnt+1] = tmp0 * tmp1_y * tmp2 * qLeni;
	      dQ[cf_cnt+2] = tmp0 * tmp1 * tmp2_z * qLeni;

	      Qmesh[Mesh*(Mesh+2) * i + (Mesh+2) * j + k] += q * tmp0 * tmp1 * tmp2;
	      cf_cnt+=3;
	    }
	  }
        }
    }
}

#define assign_charge_and_derivatives_real_template(cao) void assign_charge_and_derivatives_real_##cao(system_t *s, parameters_t *p, data_t *d) \
{ \
    int dim, i0, i1, i2; \
    int id; \
    FLOAT_TYPE tmp0, tmp1, tmp2; \
    FLOAT_TYPE tmp0_x, tmp1_y, tmp2_z; \
    /* position of a particle in local mesh units */ \
    FLOAT_TYPE pos; \
    /* 1d-index of nearest mesh point */ \
    int nmp; \
    /* index for caf interpolation grid */ \
    int arg[3]; \
    /* index, index jumps for rs_mesh array */ \
    int cf_cnt; \
 \
    int base[3]; \
    int i,j,k; \
    const int Mesh = p->mesh; \
    FLOAT_TYPE MI2 = 2.0*(FLOAT_TYPE)MaxInterpol; \
 \
    FLOAT_TYPE Hi = (double)Mesh/s->length; \
    FLOAT_TYPE Leni = 1.0/s->length; \
    FLOAT_TYPE q, qLeni; \
 \
    FLOAT_TYPE * restrict Qmesh = d->Qmesh; \
    FLOAT_TYPE * restrict dQ = d->dQ[0]; \
    FLOAT_TYPE ** restrict interpol = d->inter->interpol; \
    FLOAT_TYPE ** restrict interpol_d = d->inter->interpol_d; \
 \
    double pos_shift; \
 \
    pos_shift = (double)((cao-1)/2); \
    /* particle position in mesh coordinates */ \
    for (id=0;id<s->nparticles;id++) { \
        cf_cnt = 3*id*cao*cao*cao; \
	q = s->q[id] ; \
	qLeni = q * Leni; \
        for (dim=0;dim<3;dim++) { \
	  pos    = s->p->fields[dim][id]*Hi - pos_shift; \
	  nmp = int_floor(pos + 0.5); \
	  base[dim]  = wrap_mesh_index( nmp, Mesh); \
	  arg[dim] = int_floor((pos - nmp + 0.5)*MI2); \
	  d->ca_ind[0][3*id + dim] = base[dim]; \
        } \
 \
        for (i0=0; i0<cao; i0++) { \
	  i = wrap_mesh_index(base[0] + i0, Mesh); \
	  tmp0 = interpol[arg[0]][i0]; \
	  tmp0_x = interpol_d[arg[0]][i0]; \
	  for (i1=0; i1<cao; i1++) { \
	    j = wrap_mesh_index(base[1] + i1, Mesh); \
	    tmp1 = interpol[arg[1]][i1]; \
	    tmp1_y = interpol_d[arg[1]][i1]; \
	    for (i2=0; i2<cao; i2++) { \
	      k = wrap_mesh_index(base[2] + i2, Mesh); \
	      tmp2 = interpol[arg[2]][i2]; \
	      tmp2_z = interpol_d[arg[2]][i2]; \
 \
	      dQ[cf_cnt+0] = tmp0_x * tmp1 * tmp2 * qLeni; \
	      dQ[cf_cnt+1] = tmp0 * tmp1_y * tmp2 * qLeni; \
	      dQ[cf_cnt+2] = tmp0 * tmp1 * tmp2_z * qLeni; \
 \
	      Qmesh[Mesh*(Mesh+2) * i + (Mesh+2) * j + k] += q * tmp0 * tmp1 * tmp2; \
	      cf_cnt+=3; \
	    } \
	  } \
        } \
    } \
} \

assign_charge_and_derivatives_real_template(1)
assign_charge_and_derivatives_real_template(2)
assign_charge_and_derivatives_real_template(3)
assign_charge_and_derivatives_real_template(4)
assign_charge_and_derivatives_real_template(5)
assign_charge_and_derivatives_real_template(6)
assign_charge_and_derivatives_real_template(7)

void assign_charge_and_derivatives_real(system_t *s, parameters_t *p, data_t *d) {
  switch(p->cao) {
  case 1:
    assign_charge_and_derivatives_real_1(s, p, d);
    break;
  case 2:
    assign_charge_and_derivatives_real_2(s, p, d);
    break;
  case 3:
    assign_charge_and_derivatives_real_3(s, p, d);
    break;
  case 4:
    assign_charge_and_derivatives_real_4(s, p, d);
    break;
  case 5:
    assign_charge_and_derivatives_real_5(s, p, d);
    break;
  case 6:
    assign_charge_and_derivatives_real_6(s, p, d);
    break;
  case 7:
    assign_charge_and_derivatives_real_7(s, p, d);
    break;
  default:
    fprintf(stderr, "Charge assinment order %d not known.", p->cao);
    break;
  }
}


// assign the forces obtained from k-space
#define assign_forces_ad_template(cao) void assign_forces_ad_##cao(double force_prefac, system_t *s, parameters_t *p, data_t *d, forces_t *f, int ii) \
{ \
  int i,i0,i1,i2; \
  int cf_cnt=0; \
  int *base; \
  int j,k,l; \
  FLOAT_TYPE B; \
  FLOAT_TYPE force_x, force_y, force_z; \
  const int mesh = d->mesh; \
 \
  cf_cnt=0; \
  for (i=0; i<s->nparticles; i++) { \
    force_x = force_y = force_z = 0.0; \
    base = d->ca_ind[ii] + 3*i; \
    cf_cnt = 3*i*cao*cao*cao; \
    for (i0=0; i0<cao; i0++) { \
      j = wrap_mesh_index(base[0] + i0, mesh); \
      for (i1=0; i1<cao; i1++) { \
	k = wrap_mesh_index(base[1] + i1, mesh); \
	for (i2=0; i2<cao; i2++) { \
	  l = wrap_mesh_index(base[2] + i2, mesh); \
 \
	  B = force_prefac*d->Qmesh[c_ind(j,k,l)+ii]; \
	  force_x -= B*d->dQ[ii][cf_cnt+0]; \
	  force_y -= B*d->dQ[ii][cf_cnt+1]; \
	  force_z -= B*d->dQ[ii][cf_cnt+2]; \
	  cf_cnt+=3; \
	} \
      } \
    } \
    f->f_k->fields[0][i] += force_x; \
    f->f_k->fields[1][i] += force_y; \
    f->f_k->fields[2][i] += force_z; \
    if (ii==1) { \
      f->f_k->fields[0][i] *= 0.5; \
      f->f_k->fields[1][i] *= 0.5; \
      f->f_k->fields[2][i] *= 0.5; \
    } \
  } \
} \

assign_forces_ad_template(1)
assign_forces_ad_template(2)
assign_forces_ad_template(3)
assign_forces_ad_template(4)
assign_forces_ad_template(5)
assign_forces_ad_template(6)
assign_forces_ad_template(7)

void assign_forces_ad(FLOAT_TYPE prefactor, system_t *s, parameters_t *p, data_t *d, forces_t *f, int ii) {
  switch(p->cao) {
  case 1:
    assign_forces_ad_1(prefactor, s, p, d, f, ii);
    break;
  case 2:
    assign_forces_ad_2(prefactor, s, p, d, f, ii);
    break;
  case 3:
    assign_forces_ad_3(prefactor, s, p, d, f, ii);
    break;
  case 4:
    assign_forces_ad_4(prefactor, s, p, d, f, ii);
    break;
  case 5:
    assign_forces_ad_5(prefactor, s, p, d, f, ii);
    break;
  case 6:
    assign_forces_ad_6(prefactor, s, p, d, f, ii);
    break;
  case 7:
    assign_forces_ad_7(prefactor, s, p, d, f, ii);
    break;
  default:
    fprintf(stderr, "Charge assinment order %d not known.", p->cao);
    break;
  }
}


void assign_forces_ad_real_dynamic(double force_prefac, system_t *s, parameters_t *p, data_t *d, forces_t *f)
{
  int i,i0,i1,i2;
  int cf_cnt=0;
  int *base;
  int j,k,l;
  FLOAT_TYPE B;
  FLOAT_TYPE force_x, force_y, force_z;
  const int cao = p->cao;
  const int mesh = p->mesh;
  FLOAT_TYPE * restrict dQ = d->dQ[0];

  cf_cnt=0;
  for (i=0; i<s->nparticles; i++) {
    force_x = force_y = force_z = 0.0;
    base = d->ca_ind[0] + 3*i;
    cf_cnt = 3*i*p->cao3;
    for (i0=0; i0<cao; i0++) {
      j = wrap_mesh_index(base[0] + i0, mesh);
      for (i1=0; i1<cao; i1++) {
	k = wrap_mesh_index(base[1] + i1, mesh);
	for (i2=0; i2<cao; i2++) {
	  l = wrap_mesh_index(base[2] + i2, mesh);
	  B = force_prefac*d->Qmesh[mesh*(mesh+2) * j + (mesh+2) * k + l];
	  force_x -= B*dQ[cf_cnt+0];
	  force_y -= B*dQ[cf_cnt+1];
	  force_z -= B*dQ[cf_cnt+2];
	  cf_cnt+=3;
	}
      }
    }
    f->f_k->fields[0][i] += force_x;
    f->f_k->fields[1][i] += force_y;
    f->f_k->fields[2][i] += force_z;
  }
}

#define assign_forces_ad_real_template(cao) void assign_forces_ad_real_##cao(double force_prefac, system_t *s, parameters_t *p, data_t *d, forces_t *f) \
{ \
  int i,i0,i1,i2; \
  int cf_cnt=0; \
  int * restrict base; \
  int j,k,l; \
  FLOAT_TYPE B; \
  FLOAT_TYPE force_x, force_y, force_z; \
  const int mesh = p->mesh; \
  FLOAT_TYPE * restrict dQ = d->dQ[0]; \
 \
  cf_cnt=0; \
  for (i=0; i<s->nparticles; i++) { \
    force_x = force_y = force_z = 0.0; \
    base = d->ca_ind[0] + 3*i; \
    cf_cnt = 3*i*cao * cao *cao; \
    for (i0=0; i0<cao; i0++) { \
      j = wrap_mesh_index(base[0] + i0, mesh); \
      for (i1=0; i1<cao; i1++) { \
	k = wrap_mesh_index(base[1] + i1, mesh); \
	for (i2=0; i2<cao; i2++) { \
	  l = wrap_mesh_index(base[2] + i2, mesh); \
	  B = force_prefac*d->Qmesh[mesh*(mesh+2) * j + (mesh+2) * k + l]; \
	  force_x -= B*dQ[cf_cnt+0]; \
	  force_y -= B*dQ[cf_cnt+1]; \
	  force_z -= B*dQ[cf_cnt+2]; \
	  cf_cnt+=3; \
	} \
      } \
    } \
    f->f_k->fields[0][i] += force_x; \
    f->f_k->fields[1][i] += force_y; \
    f->f_k->fields[2][i] += force_z; \
  } \
} \

assign_forces_ad_real_template(1)
assign_forces_ad_real_template(2)
assign_forces_ad_real_template(3)
assign_forces_ad_real_template(4)
assign_forces_ad_real_template(5)
assign_forces_ad_real_template(6)
assign_forces_ad_real_template(7)

void assign_forces_ad_real(FLOAT_TYPE prefactor, system_t *s, parameters_t *p, data_t *d, forces_t *f) {
  switch(p->cao) {
  case 1:
    assign_forces_ad_real_1(prefactor, s, p, d, f);
    break;
  case 2:
    assign_forces_ad_real_2(prefactor, s, p, d, f);
    break;
  case 3:
    assign_forces_ad_real_3(prefactor, s, p, d, f);
    break;
  case 4:
    assign_forces_ad_real_4(prefactor, s, p, d, f);
    break;
  case 5:
    assign_forces_ad_real_5(prefactor, s, p, d, f);
    break;
  case 6:
    assign_forces_ad_real_6(prefactor, s, p, d, f);
    break;
  case 7:
    assign_forces_ad_real_7(prefactor, s, p, d, f);
    break;
  default:
    fprintf(stderr, "Charge assinment order %d not known.", p->cao);
    break;
  }
}


/* Interpolation functions for the calculation of inhomogenouse error estimates */

void assign_charge_q2(system_t *s, parameters_t *p, FLOAT_TYPE *Qmesh, int mesh, interpolation_t *inter)
{
    int dim, i0, i1, i2, id;
    FLOAT_TYPE tmp0, tmp1;
    /* position of a particle in local mesh units */
    FLOAT_TYPE pos;
    /* 1d-index of nearest mesh point */
    int nmp;
    /* index for caf interpolation grid */
    int arg[3];
    /* index, index jumps for rs_mesh array */
    FLOAT_TYPE cur_ca_frac_val;
    // Mesh coordinates of the closest mesh point
    int base[3];
    int i,j,k;
    FLOAT_TYPE MI2 = 2.0*(FLOAT_TYPE)MaxInterpol;

    FLOAT_TYPE Hi = (double)p->mesh/(double)s->length;

    FLOAT_TYPE **interpol = inter->interpol;
    FLOAT_TYPE q2;
    const int cao = p->cao;

    // Make sure parameter-set and data-set are compatible

    FLOAT_TYPE pos_shift;

    /* Shift for odd charge assignment order */
    pos_shift = (FLOAT_TYPE)((p->cao-1)/2);

    for (id=0;id<s->nparticles;id++) {
        /* particle position in mesh coordinates */
        for (dim=0;dim<3;dim++) {
            pos    = s->p->fields[dim][id]*Hi - pos_shift;
            nmp = int_floor(pos + 0.5);
	    base[dim]  = wrap_mesh_index( nmp, mesh);
            arg[dim] = int_floor((pos - nmp + 0.5)*MI2);
        }
	q2 = SQR(s->q[id]);
	for (i0=0; i0<cao; i0++) {
	  i = wrap_mesh_index(base[0] + i0, mesh);
	  tmp0 = q2 * interpol[arg[0]][i0];
	  for (i1=0; i1<cao; i1++) {
	    tmp1 = tmp0 * interpol[arg[1]][i1];
	    j = wrap_mesh_index(base[1] + i1, mesh);
	    for (i2=0; i2<cao; i2++) {
	      cur_ca_frac_val = tmp1 * interpol[arg[2]][i2];
	      k = wrap_mesh_index(base[2] + i2, mesh);
	      Qmesh[2*(mesh*mesh*i+mesh*j+k)] += cur_ca_frac_val;
	    }
	  }
	} 
    }
}

void assign_charge_nocf(system_t *s, parameters_t *p, FLOAT_TYPE *Qmesh, int mesh, interpolation_t *inter)
{
  int dim, i0, i1, i2, id;
  FLOAT_TYPE tmp0, tmp1;
  /* position of a particle in local mesh units */
  FLOAT_TYPE pos;
  /* 1d-index of nearest mesh point */
  int nmp;
  /* index for caf interpolation grid */
  int arg[3];
  /* index, index jumps for rs_mesh array */
  FLOAT_TYPE cur_ca_frac_val;
  // Mesh coordinates of the closest mesh point
  int base[3];
  int i,j,k;
  FLOAT_TYPE MI2 = 2.0*(FLOAT_TYPE)MaxInterpol;

  FLOAT_TYPE Hi = (double)p->mesh/(double)s->length;

  FLOAT_TYPE **interpol = inter->interpol;
  FLOAT_TYPE q;
  const int cao = p->cao;

  // Make sure parameter-set and data-set are compatible

  FLOAT_TYPE pos_shift;

  /* Shift for odd charge assignment order */
  pos_shift = (FLOAT_TYPE)((p->cao-1)/2);

  for (id=0;id<s->nparticles;id++) {
    /* particle position in mesh coordinates */
    for (dim=0;dim<3;dim++) {
      pos    = s->p->fields[dim][id]*Hi - pos_shift;
      nmp = int_floor(pos + 0.5);
      base[dim]  = wrap_mesh_index( nmp, mesh);
      arg[dim] = int_floor((pos - nmp + 0.5)*MI2);
    }
    q = s->q[id];
    for (i0=0; i0<cao; i0++) {
      i = wrap_mesh_index(base[0] + i0, mesh);
      tmp0 = q * interpol[arg[0]][i0];
      for (i1=0; i1<cao; i1++) {
	tmp1 = tmp0 * interpol[arg[1]][i1];
	j = wrap_mesh_index(base[1] + i1, mesh);
	for (i2=0; i2<cao; i2++) {
	  cur_ca_frac_val = tmp1 * interpol[arg[2]][i2];
	  k = wrap_mesh_index(base[2] + i2, mesh);
	  /* printf("assignment %d %d %d cf %e\n", i, j, k, cur_ca_frac_val); */
	  Qmesh[2*(mesh*mesh*i+mesh*j+k)] += cur_ca_frac_val;
	}
      }
    } 
  }
}

void collect_rms_nocf(system_t *s, parameters_t *p, FLOAT_TYPE *Qmesh, FLOAT_TYPE *rms, int mesh, interpolation_t *inter)
{
  int dim, i0, i1, i2, id;
  FLOAT_TYPE tmp0, tmp1;
  /* position of a particle in local mesh units */
  FLOAT_TYPE pos;
  /* 1d-index of nearest mesh point */
  int nmp;
  /* index for caf interpolation grid */
  int arg[3];
  /* index, index jumps for rs_mesh array */
  FLOAT_TYPE cur_ca_frac_val;
  // Mesh coordinates of the closest mesh point
  int base[3];
  int i,j,k;
  FLOAT_TYPE MI2 = 2.0*(FLOAT_TYPE)MaxInterpol;

  FLOAT_TYPE Hi = (double)p->mesh/(double)s->length;

  FLOAT_TYPE **interpol = inter->interpol;
  FLOAT_TYPE q2;
  const int cao = p->cao;

  // Make sure parameter-set and data-set are compatible

  FLOAT_TYPE pos_shift;

  /* Shift for odd charge assignment order */
  pos_shift = (FLOAT_TYPE)((p->cao-1)/2);

  for (id=0;id<s->nparticles;id++) {
    /* particle position in mesh coordinates */
    for (dim=0;dim<3;dim++) {
      pos    = s->p->fields[dim][id]*Hi - pos_shift;
      nmp = int_floor(pos + 0.5);
      base[dim]  = wrap_mesh_index( nmp, mesh);
      arg[dim] = int_floor((pos - nmp + 0.5)*MI2);
    }
    q2 = SQR(s->q[id]);
    rms[id] = 0.0;
    for (i0=0; i0<cao; i0++) {
      i = wrap_mesh_index(base[0] + i0, mesh);
      tmp0 = q2 * interpol[arg[0]][i0];
      for (i1=0; i1<cao; i1++) {
	tmp1 = tmp0 * interpol[arg[1]][i1];
	j = wrap_mesh_index(base[1] + i1, mesh);
	for (i2=0; i2<cao; i2++) {
	  cur_ca_frac_val = tmp1 * interpol[arg[2]][i2];
	  k = wrap_mesh_index(base[2] + i2, mesh);
	  /* printf("assignment %d %d %d cf %e\n", i, j, k, cur_ca_frac_val); */
	  rms[id] += Qmesh[2*(mesh*mesh*i+mesh*j+k)] * cur_ca_frac_val;
	}
      }
    } 
  }
}
