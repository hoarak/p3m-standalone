#include "charge-assign.h"

#include <stdio.h>
#include <math.h>
#include <assert.h>

// #define CA_DEBUG

inline int wrap_mesh_index(int ind, int mesh) {
  int ret;
  if(ind < 0)
    ret = ind + mesh;
  else if(ind >= mesh)
    ret = ind - mesh;
  else 
    ret = ind;
  if((ret < 0) || (ret >= mesh))
    return wrap_mesh_index(ret, mesh);
  return ret;
}

void assign_charge(system_t *s, parameters_t *p, data_t *d, int ii)
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
    int cf_cnt;
    // Mesh coordinates of the closest mesh point
    int base[3];
    int i,j,k;
    FLOAT_TYPE MI2 = 2.0*(FLOAT_TYPE)MaxInterpol;

    FLOAT_TYPE Hi = (double)d->mesh/(double)s->length;

    // Make sure parameter-set and data-set are compatible

    double pos_shift;

    /* Shift for odd charge assignment order */
    pos_shift = (double)((p->cao-1)/2);

    for (id=0;id<s->nparticles;id++) {
        cf_cnt = id*p->cao3;
        /* particle position in mesh coordinates */
        for (dim=0;dim<3;dim++) {
            pos    = s->p->fields[dim][id]*Hi - pos_shift + 0.5*ii;
            nmp = (int) FLOOR(pos + 0.5);
	    base[dim]  = wrap_mesh_index( nmp, d->mesh);
            arg[dim] = (int) FLOOR((pos - nmp + 0.5)*MI2);
            d->ca_ind[ii][3*id + dim] = base[dim];
        }

        for (i0=0; i0<p->cao; i0++) {
	  i = wrap_mesh_index(base[0] + i0, d->mesh);
            tmp0 = s->q[id] * d->inter->interpol[i0][arg[0]];
            for (i1=0; i1<p->cao; i1++) {
	      j = wrap_mesh_index(base[1] + i1, d->mesh);
                tmp1 = tmp0 * d->inter->interpol[i1][arg[1]];
                for (i2=0; i2<p->cao; i2++) {
		  k = wrap_mesh_index(base[2] + i2, d->mesh);
		  cur_ca_frac_val = tmp1 * d->inter->interpol[i2][arg[2]];
		  d->cf[ii][cf_cnt++] = cur_ca_frac_val ;
		  d->Qmesh[c_ind(i,j,k)+ii] += cur_ca_frac_val;
                }
            }
        }
    }
}

// assign the forces obtained from k-space
void assign_forces(FLOAT_TYPE force_prefac, system_t *s, parameters_t *p, data_t *d, forces_t *f, int ii) {
    int i,i0,i1,i2;
    int cf_cnt=0;
    int *base;
    int j,k,l;
    FLOAT_TYPE B;
    
    cf_cnt=0;
    
    for (i=0; i<s->nparticles; i++) {
      base = d->ca_ind[ii] + 3*i;
      for (i0=0; i0<p->cao; i0++) {
	j = wrap_mesh_index(base[0] + i0, d->mesh);
	for (i1=0; i1<p->cao; i1++) {
	  k = wrap_mesh_index(base[1] + i1, d->mesh);
	  for (i2=0; i2<p->cao; i2++) {
	    l = wrap_mesh_index(base[2] + i2, d->mesh);
	    B = force_prefac*d->cf[ii][cf_cnt];
	    f->f_k->fields[0][i] -= d->Fmesh->fields[0][c_ind(j,k,l)+ii]*B;
	    f->f_k->fields[1][i] -= d->Fmesh->fields[1][c_ind(j,k,l)+ii]*B;
	    f->f_k->fields[2][i] -= d->Fmesh->fields[2][c_ind(j,k,l)+ii]*B;

	    cf_cnt++;
	  }
	}
      }
      if (ii==1) {
	f->f_k->fields[0][i] *= 0.5;
	f->f_k->fields[1][i] *= 0.5;
	f->f_k->fields[2][i] *= 0.5;
      }
    }
}


void assign_charge_and_derivatives(system_t *s, parameters_t *p, data_t *d, int ii, int derivatives)
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
    FLOAT_TYPE cur_ca_frac_val;
    int cf_cnt;

    int base[3];
    int i,j,k;
    int Mesh = p->mesh;
    FLOAT_TYPE MI2 = 2.0*(FLOAT_TYPE)MaxInterpol;

    FLOAT_TYPE Hi = (double)Mesh/s->length;
    FLOAT_TYPE Leni = 1.0/s->length;


    double pos_shift;

    pos_shift = (double)((p->cao-1)/2);
    /* particle position in mesh coordinates */
    for (id=0;id<s->nparticles;id++) {
        cf_cnt = id*p->cao3;
        for (dim=0;dim<3;dim++) {
	  pos    = s->p->fields[dim][id]*Hi - pos_shift + 0.5*ii;
	  nmp = (int) FLOOR(pos + 0.5);
	  base[dim]  = wrap_mesh_index( nmp, d->mesh);
	  arg[dim] = (int) FLOOR((pos - nmp + 0.5)*MI2);
	  d->ca_ind[ii][3*id + dim] = base[dim];
	  //	  if(id == 0)
	  // printf("id %d dim %d base %d\n", id, dim, base[dim]);

        }

        for (i0=0; i0<p->cao; i0++) {
	  i = wrap_mesh_index(base[0] + i0, d->mesh);
	  tmp0 = d->inter->interpol[i0][arg[0]];
	  tmp0_x = d->inter->interpol_d[i0][arg[0]];
	  for (i1=0; i1<p->cao; i1++) {
	    j = wrap_mesh_index(base[1] + i1, d->mesh);
	    tmp1 = d->inter->interpol[i1][arg[1]];
	    tmp1_y = d->inter->interpol_d[i1][arg[1]];
	    for (i2=0; i2<p->cao; i2++) {
	      k = wrap_mesh_index(base[2] + i2, d->mesh);
	      tmp2 = d->inter->interpol[i2][arg[2]];
	      tmp2_z = d->inter->interpol_d[i2][arg[2]];
	      cur_ca_frac_val = s->q[id] * tmp0 * tmp1 * tmp2;
	      d->cf[ii][cf_cnt] = cur_ca_frac_val ;
	      if (derivatives) {
		d->dQdx[ii][cf_cnt] = Leni * tmp0_x * tmp1 * tmp2 * s->q[id];
		d->dQdy[ii][cf_cnt] = Leni * tmp0 * tmp1_y * tmp2 * s->q[id];
		d->dQdz[ii][cf_cnt] = Leni * tmp0 * tmp1 * tmp2_z * s->q[id];
	      }
	      d->Qmesh[c_ind(i,j,k)+ii] += cur_ca_frac_val;
	      cf_cnt++;
	    }
	  }
        }
    }
}

// assign the forces obtained from k-space
void assign_forces_ad(double force_prefac, system_t *s, parameters_t *p, data_t *d, forces_t *f, int ii)
{
    int i,i0,i1,i2;
    int cf_cnt=0, c_index;
    int *base;
    int j,k,l;
    FLOAT_TYPE B;

    cf_cnt=0;
    for (i=0; i<s->nparticles; i++) {
        base = d->ca_ind[ii] + 3*i;
        cf_cnt = i*p->cao3;
        for (i0=0; i0<p->cao; i0++) {
	  j = wrap_mesh_index(base[0] + i0, d->mesh);
            for (i1=0; i1<p->cao; i1++) {
	      k = wrap_mesh_index(base[1] + i1, d->mesh);
                for (i2=0; i2<p->cao; i2++) {
		  l = wrap_mesh_index(base[2] + i2, d->mesh);

		  c_index = c_ind(j,k,l)+ii;
		  B = force_prefac*d->Qmesh[c_index];
		  f->f_k->x[i] -= B*d->dQdx[ii][cf_cnt];
		  f->f_k->y[i] -= B*d->dQdy[ii][cf_cnt];
		  f->f_k->z[i] -= B*d->dQdz[ii][cf_cnt];
		  cf_cnt++;
                }
            }
        }
        if (ii==1) {
	  f->f_k->fields[0][i] *= 0.5;
	  f->f_k->fields[1][i] *= 0.5;
	  f->f_k->fields[2][i] *= 0.5;
        }
    }
}
