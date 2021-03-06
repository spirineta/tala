/* CC.c
 * 
 * Copyright (C) 1993-2008 David Weenink
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 djmw 20011016 removed some causes for compiler warnings
 djmw 20020315 GPL header
 djmw 20061212 Changed info to Melder_writeLine<x> format.
 djmw 20071012 Added: o_CAN_WRITE_AS_ENCODING.h
 djmw 20080122 float -> double
 djmw 20080513 CC_getValue
 */

#include "CC.h"
#include "num/NUM2.h"

#include "sys/oo/oo_DESTROY.h"
#include "CC_def.h"
#include "sys/oo/oo_COPY.h"
#include "CC_def.h"
#include "sys/oo/oo_EQUAL.h"
#include "CC_def.h"
#include "sys/oo/oo_CAN_WRITE_AS_ENCODING.h"
#include "CC_def.h"
#include "sys/oo/oo_WRITE_TEXT.h"
#include "CC_def.h"
#include "sys/oo/oo_WRITE_BINARY.h"
#include "CC_def.h"
#include "sys/oo/oo_READ_TEXT.h"
#include "CC_def.h"
#include "sys/oo/oo_READ_BINARY.h"
#include "CC_def.h"
#include "sys/oo/oo_DESCRIPTION.h"
#include "CC_def.h"

static void info (I)
{
	iam (CC);
	classData -> info (me);
	MelderInfo_writeLine5 (L"Time domain:", Melder_double (my xmin), L" to ", Melder_double (my xmax), L" seconds");
	MelderInfo_writeLine2 (L"Number of frames: ", Melder_integer (my nx));
	MelderInfo_writeLine3 (L"Time step: ", Melder_double (my dx), L" seconds");
	MelderInfo_writeLine3 (L"First frame at: ", Melder_double (my x1), L" seconds");
	MelderInfo_writeLine2 (L"Number of coefficients: ", Melder_integer (my maximumNumberOfCoefficients));
	MelderInfo_writeLine3 (L"Minimum frequency: ", Melder_double (my fmin), L" Hz");
	MelderInfo_writeLine3 (L"Maximum frequency: ", Melder_double (my fmax), L" Hz");
}

class_methods (CC, Sampled)
	us -> version = 1;
	class_method_local (CC, destroy)
	class_method_local (CC, equal)
	class_method_local (CC, canWriteAsEncoding)
	class_method_local (CC, copy)
	class_method (info)
	class_method_local (CC, readText)
	class_method_local (CC, readBinary)
	class_method_local (CC, writeText)
	class_method_local (CC, writeBinary)
	class_method_local (CC, description)
class_methods_end

int CC_Frame_init (CC_Frame me, long numberOfCoefficients)
{
	if ((my c = NUMdvector (1, numberOfCoefficients)) == NULL) return 0;
	my numberOfCoefficients = numberOfCoefficients;
	return 1;
}

int CC_init (I, double tmin, double tmax, long nt, double dt, double t1,
	long maximumNumberOfCoefficients, double fmin, double fmax)
{
	iam (CC);
	my fmin = fmin;
	my fmax = fmax;
	my maximumNumberOfCoefficients = maximumNumberOfCoefficients;
	return Sampled_init (me, tmin, tmax, nt, dt, t1) &&
		(my frame = NUMstructvector (CC_Frame, 1, nt));
}

Matrix CC_to_Matrix (I)
{
	iam (CC);
	Matrix thee;
	long i, j;
	
	if ((thee = Matrix_create (my xmin, my xmax, my nx, my dx, my x1,
		1, my maximumNumberOfCoefficients, my maximumNumberOfCoefficients,
			1, 1)) == NULL) return thee;

	for (i = 1; i <= my nx; i++)
	{
		CC_Frame cf = & my frame[i];
		for (j = 1; j <= cf -> numberOfCoefficients; j++)
		{
			thy z[j][i] = cf -> c[j];
		}
	}
	return thee;
}

void CC_getNumberOfCoefficients_extrema (I, long startframe, 
	long endframe, long *min, long *max)
{
	iam (CC);
	long i;
	
	Melder_assert (startframe <= endframe);
	
	if (startframe == 0 && endframe == 0)
	{
		startframe = 1; endframe = my nx;
	}
	if (startframe < 1) startframe = 1;
	if (endframe > my nx) endframe = my nx;
	
	*min = my maximumNumberOfCoefficients;
	*max = 0;
	
	for (i = startframe; i <= endframe; i++)
	{
		CC_Frame f = & my frame[i];
		long nc = f -> numberOfCoefficients;
		
		if (nc < *min) *min = nc;
		else if (nc > *max) *max = nc;
	}
}

long CC_getMinimumNumberOfCoefficients (I, long startframe, long endframe)
{
	iam (CC);
	long min, max;
	
	CC_getNumberOfCoefficients_extrema (me, startframe, endframe, &min, &max);

	return min;
}

long CC_getMaximumNumberOfCoefficients (I, long startframe, long endframe)
{
	iam (CC);
	long min, max;
	
	CC_getNumberOfCoefficients_extrema (me, startframe, endframe, &min, &max);

	return max;
}

double CC_getValue (I, double t, long index)
{
	iam (CC);
	long iframe = Sampled_xToNearestIndex (me, t);
	if (iframe < 0 || iframe > my nx) return NUMundefined;
	CC_Frame cf = & me -> frame[iframe];
	return index > cf -> numberOfCoefficients ? NUMundefined : cf -> c[index];
}

/* End of file CC.c */
