#ifndef P3M_COMMON_H
#define P3M_COMMON_H

#include "types.h"

extern int P3M_BRILLOUIN_TUNING;
extern int P3M_BRILLOUIN;

#define r_ind(A,B,C) ((A)*d->mesh*d->mesh + (B)*d->mesh + (C))
#define c_ind(A,B,C) (2*d->mesh*d->mesh*(A)+2*d->mesh*(B)+2*(C))

FLOAT_TYPE sinc(FLOAT_TYPE);
FLOAT_TYPE analytic_cotangent_sum(int n, FLOAT_TYPE mesh_i, int cao);

void Init_differential_operator( data_t * );
void Init_nshift(data_t *);
data_t *Init_data(const method_t *, system_t *s, parameters_t *); 
void Free_data(data_t *);
#endif
