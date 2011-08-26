#ifndef P3M_IK_H
#define P3M_IK_H

#include "p3m.h"

void Influence_function_berechnen_ik(system_t*, parameters_t*, data_t*);
void P3M_ik(system_t *, parameters_t *, data_t *);
data_t *Init_ik(system_t*, parameters_t*);

extern const method_t method_p3m_ik;

#endif
