#ifndef _Sound_PointProcess_h_
#define _Sound_PointProcess_h_
/* Sound_PointProcess.h
 *
 * Copyright (C) 2010-2011 Paul Boersma
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * pb 2011/03/03
 */

#include "Sound.h"
#include "PointProcess.h"

#ifdef __cplusplus
	extern "C" {
#endif

Sound Sound_PointProcess_to_SoundEnsemble_correlate (Sound me, PointProcess thee, double tmin, double tmax);

#ifdef __cplusplus
	}
#endif

/* End of file Sound_PointProcess.h */
#endif
