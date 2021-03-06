#ifndef _TextGrid_Sound_h_
#define _TextGrid_Sound_h_
/* TextGrid_Sound.h
 *
 * Copyright (C) 1992-2011 Paul Boersma
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

#include "TextGrid.h"
#include "Sound.h"
#include "Pitch.h"

#ifdef __cplusplus
	extern "C" {
#endif

Collection TextGrid_Sound_extractAllIntervals (TextGrid me, Sound sound, long itier, int preserveTimes);
Collection TextGrid_Sound_extractNonemptyIntervals (TextGrid me, Sound sound, long itier, int preserveTimes);
Collection TextGrid_Sound_extractIntervalsWhere (TextGrid me, Sound sound,
	long itier, int which_Melder_STRING, const wchar_t *text, int preserveTimes);

int TextGrid_Sound_readFromBdfFile (MelderFile file, TextGrid *textGrid, Sound *sound);

#ifdef __cplusplus
	}
#endif

/* End of file TextGrid_Sound.h */
#endif
