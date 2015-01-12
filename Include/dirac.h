/***************************************************************************\
* Copyright (c) 2008, Claudio Pica                                          *   
* All rights reserved.                                                      * 
\***************************************************************************/

#ifndef DIRAC_H
#define DIRAC_H

#include "suN_types.h"
#include "utils.h"


void Dphi_(spinor_field *out, spinor_field *in);
void Dphi(double m0, spinor_field *out, spinor_field *in);
void g5Dphi(double m0, spinor_field *out, spinor_field *in);
void g5Dphi_sq(double m0, spinor_field *out, spinor_field *in);

void Dphi_flt_(spinor_field_flt *out, spinor_field_flt *in);
void Dphi_flt(double m0, spinor_field_flt *out, spinor_field_flt *in);
void g5Dphi_flt(double m0, spinor_field_flt *out, spinor_field_flt *in);
void g5Dphi_sq_flt(double m0, spinor_field_flt *out, spinor_field_flt *in);

unsigned long int getMVM();
unsigned long int getMVM_flt();

/* Even/Odd preconditioned matrix */
void Dphi_eopre(double m0, spinor_field *out, spinor_field *in);
void Dphi_oepre(double m0, spinor_field *out, spinor_field *in);
void g5Dphi_eopre(double m0, spinor_field *out, spinor_field *in);
void g5Dphi_eopre_sq(double m0, spinor_field *out, spinor_field *in);

void Dphi_eopre_flt(double m0, spinor_field_flt *out, spinor_field_flt *in);
void Dphi_oepre_flt(double m0, spinor_field_flt *out, spinor_field_flt *in);
void g5Dphi_eopre_flt(double m0, spinor_field_flt *out, spinor_field_flt *in);
void g5Dphi_eopre_sq_flt(double m0, spinor_field_flt *out, spinor_field_flt *in);

/* Dirac operators used in the Update */

void set_dirac_mass(double mass); //this is the mass used in the following operators
void H(spinor_field *out, spinor_field *in);
void H_flt(spinor_field_flt *out, spinor_field_flt *in);
void D(spinor_field *out, spinor_field *in);
void D_flt(spinor_field_flt *out, spinor_field_flt *in);

#endif
	
