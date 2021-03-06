/* praat_Fon.c
 *
 * Copyright (C) 1992-2010 Paul Boersma
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
 * pb 2010/10/19
 */

#include "fon/AmplitudeTier.h"
#include "ui/editors/AmplitudeTierEditor.h"
#include "fon/Cochleagram_and_Excitation.h"
#include "fon/Distributions_and_Transition.h"
#include "ui/editors/DurationTierEditor.h"
#include "fon/Excitation_to_Formant.h"
#include "fon/FormantGrid.h"
#include "ui/editors/FormantGridEditor.h"
#include "fon/FormantTier.h"
#include "fon/Harmonicity.h"
#include "fon/IntensityTier.h"
#include "ui/editors/IntensityTierEditor.h"
#include "fon/LongSound.h"
#include "fon/Ltas_to_SpectrumTier.h"
#include "ui/editors/ManipulationEditor.h"
#include "fon/Matrix_and_Pitch.h"
#include "fon/Matrix_and_PointProcess.h"
#include "fon/Matrix_and_Polygon.h"
#include "fon/ParamCurve.h"
#include "fon/Pitch_to_PitchTier.h"
#include "fon/Pitch_to_PointProcess.h"
#include "fon/Pitch_to_Sound.h"
#include "ui/editors/PitchEditor.h"
#include "fon/PitchTier_to_PointProcess.h"
#include "fon/PitchTier_to_Sound.h"
#include "ui/editors/PitchTierEditor.h"
#include "ui/editors/PointEditor.h"
#include "fon/PointProcess_and_Sound.h"
#include "Praat_tests.h"
#include "fon/Sound_and_Spectrogram.h"
#include "fon/Sound_and_Spectrum.h"
#include "fon/Sound_PointProcess.h"
#include "ui/editors/SpectrogramEditor.h"
#include "fon/Spectrum_and_Spectrogram.h"
#include "fon/Spectrum_to_Excitation.h"
#include "fon/Spectrum_to_Formant.h"
#include "ui/editors/SpectrumEditor.h"
#include "fon/SpellingChecker.h"
#include "fon/TextGrid.h"
#include "fon/VocalTract.h"
#include "fon/VoiceAnalysis.h"
#include "fon/WordList.h"
#include "stat/Distributions_and_Strings.h"
#include "stat/Table.h"
#include "ui/editors/StringsEditor.h"
#include "ui/Formula.h"
#include "ui/Interpreter.h"
#include "ui/UiFile.h"

#include "ui/praat.h"

int Matrix_formula (Matrix me, const wchar_t *expression, Interpreter *interpreter, Matrix target);

static const wchar_t *STRING_FROM_TIME_SECONDS = L"left Time range (s)";
static const wchar_t *STRING_TO_TIME_SECONDS = L"right Time range (s)";
static const wchar_t *STRING_FROM_TIME = L"left Time range";
static const wchar_t *STRING_TO_TIME = L"right Time range";
static const wchar_t *STRING_FROM_FREQUENCY_HZ = L"left Frequency range (Hz)";
static const wchar_t *STRING_TO_FREQUENCY_HZ = L"right Frequency range (Hz)";
static const wchar_t *STRING_FROM_FREQUENCY = L"left Frequency range";
static const wchar_t *STRING_TO_FREQUENCY = L"right Frequency range";

/***** Common dialog contents. *****/

void praat_dia_timeRange (UiForm *dia);
void praat_dia_timeRange (UiForm *dia) {
	REAL (STRING_FROM_TIME_SECONDS, L"0.0")
	REAL (STRING_TO_TIME_SECONDS, L"0.0 (= all)")
}
void praat_get_timeRange (UiForm *dia, double *tmin, double *tmax);
void praat_get_timeRange (UiForm *dia, double *tmin, double *tmax) {
	*tmin = GET_REAL (STRING_FROM_TIME);
	*tmax = GET_REAL (STRING_TO_TIME);
}
int praat_get_frequencyRange (UiForm *dia, double *fmin, double *fmax);
int praat_get_frequencyRange (UiForm *dia, double *fmin, double *fmax) {
	*fmin = GET_REAL (STRING_FROM_FREQUENCY);
	*fmax = GET_REAL (STRING_TO_FREQUENCY);
	REQUIRE (*fmax > *fmin, L"Maximum frequency must be greater than minimum frequency.")
	return 1;
}
static void dia_Vector_getExtremum (UiForm *dia) {
	UiForm::UiField *radio;
	praat_dia_timeRange (dia);
	RADIO (L"Interpolation", 2)
	RADIOBUTTON (L"None")
	RADIOBUTTON (L"Parabolic")
	RADIOBUTTON (L"Cubic")
	RADIOBUTTON (L"Sinc70")
	RADIOBUTTON (L"Sinc700")
}
static void dia_Vector_getValue (UiForm *dia) {
	UiForm::UiField *radio;
	REAL (L"Time (s)", L"0.5")
	RADIO (L"Interpolation", 3)
	RADIOBUTTON (L"Nearest")
	RADIOBUTTON (L"Linear")
	RADIOBUTTON (L"Cubic")
	RADIOBUTTON (L"Sinc70")
	RADIOBUTTON (L"Sinc700")
}

static int getTminTmaxFminFmax (UiForm *dia, double *tmin, double *tmax, double *fmin, double *fmax) {
	*tmin = GET_REAL (STRING_FROM_TIME);
	*tmax = GET_REAL (STRING_TO_TIME);
	*fmin = GET_REAL (STRING_FROM_FREQUENCY);
	*fmax = GET_REAL (STRING_TO_FREQUENCY);
	REQUIRE (*fmax > *fmin, L"Maximum frequency must be greater than minimum frequency.")
	return 1;
}
#define GET_TMIN_TMAX_FMIN_FMAX \
	double tmin, tmax, fmin, fmax; \
	if (! getTminTmaxFminFmax (dia, & tmin, & tmax, & fmin, & fmax)) return 0;

/***** Two auxiliary routines, exported. *****/

extern "C" int praat_Fon_formula (UiForm *dia, Interpreter *interpreter);
int praat_Fon_formula (UiForm *dia, Interpreter *interpreter) {
	int IOBJECT;
	WHERE_DOWN (SELECTED) {
		Matrix_formula ((structMatrix *)OBJECT, GET_STRING (L"formula"), interpreter, NULL);
		praat_dataChanged (OBJECT);
		iferror return 0;
	}
	return 1;
}

Graphics Movie_create (const wchar_t *title, int width, int height);
Graphics Movie_create (const wchar_t *title, int width, int height) {
	static Graphics graphics;
	static GuiObject dialog, drawingArea;
	if (! graphics) {
		dialog = GuiDialog_create (theCurrentPraatApplication -> topShell, 100, 100, width + 2, height + 2, title, NULL, NULL, 0);
		drawingArea = GuiDrawingArea_createShown (dialog, 0, width, 0, height, NULL, NULL, NULL, NULL, NULL, 0);
		GuiObject_show (dialog);
		graphics = Graphics_create_xmdrawingarea (drawingArea);
	}
	GuiWindow_setTitle (GuiObject_parent (dialog), title);
	GuiObject_size (GuiObject_parent (dialog), width + 2, height + 2);
	GuiObject_size (drawingArea, width, height);
	GuiObject_show (dialog);
	return graphics;
}

/***** AMPLITUDETIER *****/

int RealTier_formula (I, const wchar_t *expression, Interpreter *interpreter, thou) {
	iam (RealTier);
	thouart (RealTier);
	Formula_compile (interpreter, me, expression, kFormula_EXPRESSION_TYPE_NUMERIC, TRUE); cherror
	if (thee == NULL) thee = me;
	for (long icol = 1; icol <= my points -> size; icol ++) {
		struct Formula_Result result;
		Formula_run (0, icol, & result); cherror
		if (result. result.numericResult == NUMundefined)
			error1 (L"Cannot put an undefined value into the tier.\nFormula not finished.")
		((RealPoint) thy points -> item [icol]) -> value = result. result.numericResult;
	}
end:
	iferror return 0;
	return 1;
}

FORM (AmplitudeTier_addPoint, L"Add one point", L"AmplitudeTier: Add point...")
	REAL (L"Time (s)", L"0.5")
	REAL (L"Sound pressure (Pa)", L"0.8")
	OK
DO
	WHERE (SELECTED) {
		if (! RealTier_addPoint (OBJECT, GET_REAL (L"Time"), GET_REAL (L"Sound pressure"))) return 0;
		praat_dataChanged (OBJECT);
	}
END

FORM (AmplitudeTier_create, L"Create empty AmplitudeTier", NULL)
	WORD (L"Name", L"empty")
	REAL (L"Start time (s)", L"0.0")
	REAL (L"End time (s)", L"1.0")
	OK
DO
	double startTime = GET_REAL (L"Start time"), endTime = GET_REAL (L"End time");
	REQUIRE (endTime > startTime, L"End time must be greater than start time.")
	if (! praat_new1 (AmplitudeTier_create (startTime, endTime), GET_STRING (L"Name"))) return 0;
END

DIRECT (AmplitudeTier_downto_PointProcess)
	EVERY_TO (AnyTier_downto_PointProcess (OBJECT))
END

DIRECT (AmplitudeTier_downto_TableOfReal)
	EVERY_TO (AmplitudeTier_downto_TableOfReal ((structAmplitudeTier *)OBJECT))
END

DIRECT (AmplitudeTier_edit)
	if (theCurrentPraatApplication -> batch) {
		return Melder_error1 (L"Cannot edit an AmplitudeTier from batch.");
	} else {
		Sound sound = NULL;
		WHERE (SELECTED)
			if (CLASS == classSound) sound = (structSound *)OBJECT;
		WHERE (SELECTED && CLASS == classAmplitudeTier)
			if (! praat_installEditor (new AmplitudeTierEditor (theCurrentPraatApplication -> topShell, ID_AND_FULL_NAME,
				(structAmplitudeTier *)OBJECT, sound, TRUE), IOBJECT)) return 0;
	}
END

FORM (AmplitudeTier_formula, L"AmplitudeTier: Formula", L"AmplitudeTier: Formula...")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"# ncol = the number of points")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"for col from 1 to ncol")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"   # x = the time of the colth point, in seconds")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"   # self = the value of the colth point, in Pascal")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"   self = `formula'")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"endfor")
	TEXTFIELD (L"formula", L"- self ; upside down")
	OK
DO
	WHERE_DOWN (SELECTED) {
		RealTier_formula ((structRealTier *)OBJECT, GET_STRING (L"formula"), interpreter, NULL);
		praat_dataChanged (OBJECT);
		iferror return 0;
	}
END

static void dia_AmplitudeTier_getRangeProperty (UiForm *dia) {
	REAL (L"Shortest period (s)", L"0.0001")
	REAL (L"Longest period (s)", L"0.02")
	POSITIVE (L"Maximum amplitude factor", L"1.6")
}

FORM (AmplitudeTier_getShimmer_local, L"AmplitudeTier: Get shimmer (local)", L"AmplitudeTier: Get shimmer (local)...")
	dia_AmplitudeTier_getRangeProperty (dia);
	OK
DO
	Melder_informationReal (AmplitudeTier_getShimmer_local ((structAmplitudeTier *)ONLY (classAmplitudeTier),
		GET_REAL (L"Shortest period"), GET_REAL (L"Longest period"), GET_REAL (L"Maximum amplitude factor")), NULL);
END

FORM (AmplitudeTier_getShimmer_local_dB, L"AmplitudeTier: Get shimmer (local, dB)", L"AmplitudeTier: Get shimmer (local, dB)...")
	dia_AmplitudeTier_getRangeProperty (dia);
	OK
DO
	Melder_informationReal (AmplitudeTier_getShimmer_local_dB ((structAmplitudeTier *)ONLY (classAmplitudeTier),
		GET_REAL (L"Shortest period"), GET_REAL (L"Longest period"), GET_REAL (L"Maximum amplitude factor")), NULL);
END

FORM (AmplitudeTier_getShimmer_apq3, L"AmplitudeTier: Get shimmer (apq3)", L"AmplitudeTier: Get shimmer (apq3)...")
	dia_AmplitudeTier_getRangeProperty (dia);
	OK
DO
	Melder_informationReal (AmplitudeTier_getShimmer_apq3 ((structAmplitudeTier *)ONLY (classAmplitudeTier),
		GET_REAL (L"Shortest period"), GET_REAL (L"Longest period"), GET_REAL (L"Maximum amplitude factor")), NULL);
END

FORM (AmplitudeTier_getShimmer_apq5, L"AmplitudeTier: Get shimmer (apq5)", L"AmplitudeTier: Get shimmer (apq5)...")
	dia_AmplitudeTier_getRangeProperty (dia);
	OK
DO
	Melder_informationReal (AmplitudeTier_getShimmer_apq5 ((structAmplitudeTier *)ONLY (classAmplitudeTier),
		GET_REAL (L"Shortest period"), GET_REAL (L"Longest period"), GET_REAL (L"Maximum amplitude factor")), NULL);
END

FORM (AmplitudeTier_getShimmer_apq11, L"AmplitudeTier: Get shimmer (apq11)", L"AmplitudeTier: Get shimmer (apq11)...")
	dia_AmplitudeTier_getRangeProperty (dia);
	OK
DO
	Melder_informationReal (AmplitudeTier_getShimmer_apq11 ((structAmplitudeTier *)ONLY (classAmplitudeTier),
		GET_REAL (L"Shortest period"), GET_REAL (L"Longest period"), GET_REAL (L"Maximum amplitude factor")), NULL);
END

FORM (AmplitudeTier_getShimmer_dda, L"AmplitudeTier: Get shimmer (dda)", L"AmplitudeTier: Get shimmer (dda)...")
	dia_AmplitudeTier_getRangeProperty (dia);
	OK
DO
	Melder_informationReal (AmplitudeTier_getShimmer_dda ((structAmplitudeTier *)ONLY (classAmplitudeTier),
		GET_REAL (L"Shortest period"), GET_REAL (L"Longest period"), GET_REAL (L"Maximum amplitude factor")), NULL);
END

/*FORM (AmplitudeTier_getValueAtTime, L"Get AmplitudeTier value", L"AmplitudeTier: Get value at time...")
	REAL (L"Time (s)", L"0.5")
	OK
DO
	Melder_informationReal (RealTier_getValueAtTime (ONLY_OBJECT, GET_REAL (L"Time")), L"Pa");
END
	
FORM (AmplitudeTier_getValueAtIndex, L"Get AmplitudeTier value", L"AmplitudeTier: Get value at index...")
	INTEGER (L"Point number", L"10")
	OK
DO
	Melder_informationReal (RealTier_getValueAtIndex (ONLY_OBJECT, GET_INTEGER (L"Point number")), L"Pa");
END*/

DIRECT (AmplitudeTier_help) Melder_help (L"AmplitudeTier"); END

FORM (AmplitudeTier_to_IntensityTier, L"AmplitudeTier: To IntensityTier", L"AmplitudeTier: To IntensityTier...")
	REAL (L"Threshold (dB)", L"-10000.0")
	OK
DO
	EVERY_TO (AmplitudeTier_to_IntensityTier ((structAmplitudeTier *)OBJECT, GET_REAL (L"Threshold")))
END

FORM (AmplitudeTier_to_Sound, L"AmplitudeTier: To Sound (pulse train)", L"AmplitudeTier: To Sound (pulse train)...")
	POSITIVE (L"Sampling frequency (Hz)", L"44100")
	NATURAL (L"Interpolation depth (samples)", L"2000")
	OK
DO
	EVERY_TO (AmplitudeTier_to_Sound ((structAmplitudeTier *)OBJECT, GET_REAL (L"Sampling frequency"), GET_INTEGER (L"Interpolation depth")))
END

DIRECT (info_AmplitudeTier_Sound_edit)
	Melder_information1 (L"To include a copy of a Sound in your AmplitudeTier editor:\n"
		"   select an AmplitudeTier and a Sound, and click \"View & Edit\".");
END

/***** AMPLITUDETIER & SOUND *****/

DIRECT (Sound_AmplitudeTier_multiply)
	Sound sound = (structSound *)ONLY (classSound);
	if (! praat_new2 (Sound_AmplitudeTier_multiply (sound, (structAmplitudeTier *)ONLY (classAmplitudeTier)), sound -> name, L"_amp")) return 0;
END

/***** COCHLEAGRAM *****/

void Matrix_movie (I, Graphics g);

void Cochleagram_paint (Cochleagram me, Graphics g, double tmin, double tmax, int garnish)
{
	static double border [1 + 12] =
		{ 0, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80 };
	Cochleagram copy = (structCochleagram *)Data_copy (me);
	long iy, ix, itmin, itmax;
	if (! copy) return;
	if (tmax <= tmin) { tmin = my xmin; tmax = my xmax; }
	Matrix_getWindowSamplesX (me, tmin, tmax, & itmin, & itmax);
	for (iy = 2; iy < my ny; iy ++)
		for (ix = itmin; ix <= itmax; ix ++)
			if (my z [iy] [ix] > my z [iy - 1] [ix] &&
				my z [iy] [ix] > my z [iy + 1] [ix])
			{
				copy -> z [iy - 1] [ix] += 10;
				copy -> z [iy] [ix] += 10;
				copy -> z [iy + 1] [ix] += 10;
			}
	Graphics_setInner (g);
	Graphics_setWindow (g, tmin, tmax, 0, my ny * my dy);
	Graphics_grey (g, copy -> z,
		itmin, itmax, Matrix_columnToX (me, itmin), Matrix_columnToX (me, itmax),
		1, my ny, 0.5 * my dy, (my ny - 0.5) * my dy,
		12, border);
	Graphics_unsetInner (g);
	forget (copy);
	if (garnish) {
		Graphics_drawInnerBox (g);
		Graphics_textBottom (g, 1, L"Time (s)");
		Graphics_marksBottom (g, 2, 1, 1, 0);
		Graphics_textLeft (g, 1, L"Place (Bark)");
		Graphics_marksLeftEvery (g, 1.0, 5.0, 1, 1, 0);
	}
}

FORM (Cochleagram_difference, L"Cochleagram difference", 0)
	praat_dia_timeRange (dia);
	OK
DO
	Data coch1 = NULL, coch2 = NULL;
	WHERE (SELECTED && CLASS == classCochleagram) { if (coch1) coch2 = (structData *)OBJECT; else coch1 = (structData *)OBJECT; }
	Melder_informationReal (Cochleagram_difference ((Cochleagram) coch1, (Cochleagram) coch2,
			GET_REAL (L"left Time range"), GET_REAL (L"right Time range")), L"Hertz (root-mean-square)");
END

FORM (Cochleagram_formula, L"Cochleagram Formula", L"Cochleagram: Formula...")
	LABEL (L"label", L"`x' is time in seconds, `y' is place in Bark")
	LABEL (L"label", L"y := y1; for row := 1 to nrow do { x := x1; "
		"for col := 1 to ncol do { self [row, col] := `formula' ; x := x + dx } y := y + dy }")
	TEXTFIELD (L"formula", L"self")
	OK
DO
	if (! praat_Fon_formula (dia, interpreter)) return 0;
END

DIRECT (Cochleagram_help) Melder_help (L"Cochleagram"); END

DIRECT (Cochleagram_movie)
	Graphics g = Movie_create (L"Cochleagram movie", 300, 300);
	WHERE (SELECTED) Matrix_movie ((structMatrix *)OBJECT, g);
END

FORM (Cochleagram_paint, L"Paint Cochleagram", 0)
	praat_dia_timeRange (dia);
	BOOLEAN (L"Garnish", 1)
	OK
DO
	EVERY_DRAW (Cochleagram_paint ((structCochleagram *)OBJECT, GRAPHICS,
		GET_REAL (L"left Time range"), GET_REAL (L"right Time range"), GET_INTEGER (L"Garnish")))
END

FORM (Cochleagram_to_Excitation, L"From Cochleagram to Excitation", 0)
	REAL (L"Time (s)", L"0.0")
	OK
DO
	EVERY_TO (Cochleagram_to_Excitation ((structCochleagram *)OBJECT, GET_REAL (L"Time")))
END

DIRECT (Cochleagram_to_Matrix)
	EVERY_TO (Cochleagram_to_Matrix ((structCochleagram *)OBJECT))
END

/***** DISTRIBUTIONS *****/

FORM (Distributions_to_Transition, L"To Transition", 0)
	NATURAL (L"Environment", L"1")
	BOOLEAN (L"Greedy", 1)
	OK
DO
	if (! praat_new1 (Distributions_to_Transition ((structDistributions *)ONLY_OBJECT, NULL, GET_INTEGER (L"Environment"),
		NULL, GET_INTEGER (L"Greedy")), NULL)) return 0;
END

FORM (Distributions_to_Transition_adj, L"To Transition", 0)
	NATURAL (L"Environment", L"1")
	BOOLEAN (L"Greedy", 1)
	OK
DO
	if (! praat_new1 (Distributions_to_Transition ((structDistributions *)ONLY (classDistributions), NULL,
		GET_INTEGER (L"Environment"), (structTransition *)ONLY (classTransition), GET_INTEGER (L"Greedy")), NULL)) return 0;
END

FORM (Distributions_to_Transition_noise, L"To Transition (noise)", 0)
	NATURAL (L"Environment", L"1")
	BOOLEAN (L"Greedy", 1)
	OK
DO
	Distributions underlying = NULL, surface = NULL;
	WHERE (SELECTED) { if (underlying) surface = (structDistributions *)OBJECT; else underlying = (structDistributions *)OBJECT; }
	if (! praat_new1 (Distributions_to_Transition (underlying, surface, GET_INTEGER (L"Environment"),
		NULL, GET_INTEGER (L"Greedy")), NULL)) return 0;
END

FORM (Distributions_to_Transition_noise_adj, L"To Transition (noise)", 0)
	NATURAL (L"Environment", L"1")
	BOOLEAN (L"Greedy", 1)
	OK
DO
	Distributions underlying = NULL, surface = NULL;
	WHERE (SELECTED && CLASS == classDistributions) { if (underlying) surface = (structDistributions *)OBJECT; else underlying = (structDistributions *)OBJECT; }
	if (! praat_new1 (Distributions_to_Transition (underlying, surface, GET_INTEGER (L"Environment"),
		(structTransition *)ONLY (classTransition), GET_INTEGER (L"Greedy")), NULL)) return 0;
END

/***** DISTRIBUTIONS & TRANSITION *****/

DIRECT (Distributions_Transition_map)
	if (! praat_new1 (Distributions_Transition_map ((structDistributions *)ONLY (classDistributions), (structTransition *)ONLY (classTransition)),
		L"surface")) return 0;
END

/***** DURATIONTIER *****/

FORM (DurationTier_addPoint, L"Add one point to DurationTier", L"DurationTier: Add point...")
	REAL (L"Time (s)", L"0.5")
	REAL (L"Relative duration", L"1.5")
	OK
DO
	WHERE (SELECTED) {
		if (! RealTier_addPoint ((structRealTier *)OBJECT, GET_REAL (L"Time"), GET_REAL (L"Relative duration"))) return 0;
		praat_dataChanged (OBJECT);
	}
END

FORM (DurationTier_create, L"Create empty DurationTier", L"Create DurationTier...")
	WORD (L"Name", L"empty")
	REAL (L"Start time (s)", L"0.0")
	REAL (L"End time (s)", L"1.0")
	OK
DO
	double startTime = GET_REAL (L"Start time"), endTime = GET_REAL (L"End time");
	REQUIRE (endTime > startTime, L"End time must be greater than start time.")
	if (! praat_new1 (DurationTier_create (startTime, endTime), GET_STRING (L"Name"))) return 0;
END

DIRECT (DurationTier_downto_PointProcess)
	EVERY_TO (AnyTier_downto_PointProcess (OBJECT))
END

DIRECT (DurationTier_edit)
	if (theCurrentPraatApplication -> batch) {
		return Melder_error1 (L"Cannot edit a DurationTier from batch.");
	} else {
		Sound sound = NULL;
		WHERE (SELECTED)
			if (CLASS == classSound) sound = (structSound *)OBJECT;
		WHERE (SELECTED && CLASS == classDurationTier)
			if (! praat_installEditor (new DurationTierEditor (theCurrentPraatApplication -> topShell, ID_AND_FULL_NAME,
				(structDurationTier *)OBJECT, sound, TRUE), IOBJECT)) return 0;
	}
END

FORM (DurationTier_formula, L"DurationTier: Formula", L"DurationTier: Formula...")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"# ncol = the number of points")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"for col from 1 to ncol")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"   # x = the time of the colth point, in seconds")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"   # self = the value of the colth point, in relative units")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"   self = `formula'")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"endfor")
	TEXTFIELD (L"formula", L"self * 1.5 ; slow down")
	OK
DO
	WHERE_DOWN (SELECTED) {
		RealTier_formula ((structRealTier *)OBJECT, GET_STRING (L"formula"), interpreter, NULL);
		praat_dataChanged (OBJECT);
		iferror return 0;
	}
END

FORM (DurationTier_getTargetDuration, L"Get target duration", 0)
	REAL (L"left Time range (s)", L"0.0")
	REAL (L"right Time range (s)", L"1.0")
	OK
DO
	Melder_informationReal (RealTier_getArea ((structRealTier *)ONLY_OBJECT,
		GET_REAL (L"left Time range"), GET_REAL (L"right Time range")), L"seconds");
END

FORM (DurationTier_getValueAtTime, L"Get DurationTier value", L"DurationTier: Get value at time...")
	REAL (L"Time (s)", L"0.5")
	OK
DO
	Melder_informationReal (RealTier_getValueAtTime ((structRealTier *)ONLY_OBJECT, GET_REAL (L"Time")), NULL);
END
	
FORM (DurationTier_getValueAtIndex, L"Get DurationTier value", L"Duration: Get value at index...")
	INTEGER (L"Point number", L"10")
	OK
DO
	Melder_informationReal (RealTier_getValueAtIndex ((structRealTier *)ONLY_OBJECT, GET_INTEGER (L"Point number")), NULL);
END

DIRECT (DurationTier_help) Melder_help (L"DurationTier"); END

DIRECT (info_DurationTier_Sound_edit)
	Melder_information1 (L"To include a copy of a Sound in your DurationTier editor:\n"
		"   select a DurationTier and a Sound, and click \"View & Edit\".");
END

/***** EXCITATION *****/

void Excitation_draw (Excitation me, Graphics g,
	double fmin, double fmax, double minimum, double maximum, int garnish)
{
	long ifmin, ifmax;
	if (fmax <= fmin) { fmin = my xmin; fmax = my xmax; }
	Matrix_getWindowSamplesX (me, fmin, fmax, & ifmin, & ifmax);
	if (maximum <= minimum)
		Matrix_getWindowExtrema (me, ifmin, ifmax, 1, 1, & minimum, & maximum);
	if (maximum <= minimum) { minimum -= 20; maximum += 20; }
	Graphics_setInner (g);
	Graphics_setWindow (g, fmin, fmax, minimum, maximum);
	Graphics_function (g, my z [1], ifmin, ifmax,
		Matrix_columnToX (me, ifmin), Matrix_columnToX (me, ifmax));
	Graphics_unsetInner (g);
	if (garnish) {
		Graphics_drawInnerBox (g);
		Graphics_textBottom (g, 1, L"Frequency (Bark)");
		Graphics_textLeft (g, 1, L"Excitation (phon)");
		Graphics_marksBottomEvery (g, 1, 5, 1, 1, 0);
		Graphics_marksLeftEvery (g, 1, 20, 1, 1, 0);
	}
}

FORM (Excitation_draw, L"Draw Excitation", 0)
	REAL (L"From frequency (Bark)", L"0")
	REAL (L"To frequency (Bark)", L"25.6")
	REAL (L"Minimum (phon)", L"0")
	REAL (L"Maximum (phon)", L"100")
	BOOLEAN (L"Garnish", 1)
	OK
DO
	EVERY_DRAW (Excitation_draw ((structExcitation *)OBJECT, GRAPHICS,
		GET_REAL (L"From frequency"), GET_REAL (L"To frequency"),
		GET_REAL (L"Minimum"), GET_REAL (L"Maximum"), GET_INTEGER (L"Garnish")))
END

FORM (Excitation_formula, L"Excitation Formula", L"Excitation: Formula...")
	LABEL (L"label", L"`x' is the place in Bark, `col' is the bin number")
	LABEL (L"label", L"x := 0;   for col := 1 to ncol do { self [1, col] := `formula' ; x := x + dx }")
	TEXTFIELD (L"formula", L"self")
	OK
DO
	if (! praat_Fon_formula (dia, interpreter)) return 0;
END

DIRECT (Excitation_getLoudness)
	Melder_informationReal (Excitation_getLoudness ((structExcitation *)ONLY (classExcitation)), L"sones");
END

DIRECT (Excitation_help) Melder_help (L"Excitation"); END

FORM (Excitation_to_Formant, L"From Excitation to Formant", 0)
	NATURAL (L"Maximum number of formants", L"20")
	OK
DO
	EVERY_TO (Excitation_to_Formant ((structExcitation *)OBJECT, GET_INTEGER (L"Maximum number of formants")))
END

DIRECT (Excitation_to_Matrix)
	EVERY_TO (Excitation_to_Matrix ((structExcitation *)OBJECT))
END

/***** FORMANT *****/

void Formant_drawTracks (Formant me, Graphics g, double tmin, double tmax, double fmax, int garnish) {
	long itmin, itmax, iframe, itrack, ntrack = Formant_getMinNumFormants (me);
	if (tmax <= tmin) { tmin = my xmin; tmax = my xmax; }
	if (! Sampled_getWindowSamples (me, tmin, tmax, & itmin, & itmax)) return;
	Graphics_setInner (g);
	Graphics_setWindow (g, tmin, tmax, 0.0, fmax);
	for (itrack = 1; itrack <= ntrack; itrack ++) {
		for (iframe = itmin; iframe < itmax; iframe ++) {
			Formant_Frame curFrame = & my frame [iframe], nextFrame = & my frame [iframe + 1];
			double x1 = Sampled_indexToX (me, iframe), x2 = Sampled_indexToX (me, iframe + 1);
			double f1 = curFrame -> formant [itrack]. frequency;
			double f2 = nextFrame -> formant [itrack]. frequency;
			if (NUMdefined (x1) && NUMdefined (f1) && NUMdefined (x2) && NUMdefined (f2))
				Graphics_line (g, x1, f1, x2, f2);
		}
	}
	Graphics_unsetInner (g);
	if (garnish) {
		Graphics_drawInnerBox (g);
		Graphics_textBottom (g, 1, L"Time (s)");
		Graphics_textLeft (g, 1, L"Formant frequency (Hz)");
		Graphics_marksBottom (g, 2, 1, 1, 0);
		Graphics_marksLeftEvery (g, 1.0, 1000.0, 1, 1, 1);
	}
}

void Formant_drawSpeckles_inside (Formant me, Graphics g, double tmin, double tmax, double fmin, double fmax,
	double suppress_dB, double dotSize)
{
	long itmin, itmax, iframe, iformant;
	double maximumIntensity = 0.0, minimumIntensity;
	if (tmax <= tmin) { tmin = my xmin; tmax = my xmax; }
	if (! Sampled_getWindowSamples (me, tmin, tmax, & itmin, & itmax)) return;
	Graphics_setWindow (g, tmin, tmax, fmin, fmax);

	for (iframe = itmin; iframe <= itmax; iframe ++) {
		Formant_Frame frame = & my frame [iframe];
		if (frame -> intensity > maximumIntensity)
			maximumIntensity = frame -> intensity;
	}
	if (maximumIntensity == 0.0 || suppress_dB <= 0.0)
		minimumIntensity = 0.0;   /* Ignore. */
	else
		minimumIntensity = maximumIntensity / pow (10, suppress_dB / 10);

	for (iframe = itmin; iframe <= itmax; iframe ++) {
		Formant_Frame frame = & my frame [iframe];
		double x = Sampled_indexToX (me, iframe);
		if (frame -> intensity < minimumIntensity) continue;
		for (iformant = 1; iformant <= frame -> nFormants; iformant ++) {
			double frequency = frame -> formant [iformant]. frequency;
			if (frequency >= fmin && frequency <= fmax)
				Graphics_fillCircle_mm (g, x, frequency, dotSize);
		}
	}
}

void Formant_drawSpeckles (Formant me, Graphics g, double tmin, double tmax, double fmax, double suppress_dB,
	int garnish)
{
	Graphics_setInner (g);
	Formant_drawSpeckles_inside (me, g, tmin, tmax, 0.0, fmax, suppress_dB, 1.0);
	Graphics_unsetInner (g);
	if (garnish) {
		Graphics_drawInnerBox (g);
		Graphics_textBottom (g, 1, L"Time (s)");
		Graphics_textLeft (g, 1, L"Formant frequency (Hz)");
		Graphics_marksBottom (g, 2, 1, 1, 0);
		Graphics_marksLeftEvery (g, 1.0, 1000.0, 1, 1, 1);
	}
}

void Formant_scatterPlot (Formant me, Graphics g, double tmin, double tmax,
	int iformant1, double fmin1, double fmax1, int iformant2, double fmin2, double fmax2,
	double size_mm, const wchar_t *mark, int garnish)
{
	long itmin, itmax, iframe;
	if (iformant1 < 1 || iformant2 < 1) return;
	if (tmax <= tmin) { tmin = my xmin; tmax = my xmax; }
	if (! Sampled_getWindowSamples (me, tmin, tmax, & itmin, & itmax)) return;
	if (fmax1 == fmin1)
		Formant_getExtrema (me, iformant1, tmin, tmax, & fmin1, & fmax1);
	if (fmax1 == fmin1) return;
	if (fmax2 == fmin2)
		Formant_getExtrema (me, iformant2, tmin, tmax, & fmin2, & fmax2);
	if (fmax2 == fmin2) return;
	Graphics_setInner (g);
	Graphics_setWindow (g, fmin1, fmax1, fmin2, fmax2);
	for (iframe = itmin; iframe <= itmax; iframe ++) {
		Formant_Frame frame = & my frame [iframe];
		double x, y;
		if (iformant1 > frame -> nFormants || iformant2 > frame -> nFormants) continue;
		x = frame -> formant [iformant1]. frequency;
		y = frame -> formant [iformant2]. frequency;
		if (x == 0.0 || y == 0.0) continue;
		Graphics_mark (g, x, y, size_mm, mark);
	}
	Graphics_unsetInner (g);
	if (garnish) {
		wchar_t text [100];
		Graphics_drawInnerBox (g);
		swprintf (text, 100, L"%%F_%d (Hz)", iformant1);
		Graphics_textBottom (g, 1, text);
		swprintf (text, 100, L"%%F_%d (Hz)", iformant2);
		Graphics_textLeft (g, 1, text);
		Graphics_marksBottom (g, 2, 1, 1, 0);
		Graphics_marksLeft (g, 2, 1, 1, 0);
	}
}

DIRECT (Formant_downto_FormantGrid)
	EVERY_TO (Formant_downto_FormantGrid ((structFormant *)OBJECT))
END

DIRECT (Formant_downto_FormantTier)
	EVERY_TO (Formant_downto_FormantTier ((structFormant *)OBJECT))
END

FORM (Formant_drawSpeckles, L"Draw Formant", L"Formant: Draw speckles...")
	praat_dia_timeRange (dia);
	POSITIVE (L"Maximum frequency (Hz)", L"5500.0")
	REAL (L"Dynamic range (dB)", L"30.0")
	BOOLEAN (L"Garnish", 1)
	OK
DO
	EVERY_DRAW (Formant_drawSpeckles ((structFormant *)OBJECT, GRAPHICS,
		GET_REAL (L"left Time range"), GET_REAL (L"right Time range"), GET_REAL (L"Maximum frequency"),
		GET_REAL (L"Dynamic range"), GET_INTEGER (L"Garnish")))
END

FORM (Formant_drawTracks, L"Draw formant tracks", L"Formant: Draw tracks...")
	praat_dia_timeRange (dia);
	POSITIVE (L"Maximum frequency (Hz)", L"5500.0")
	BOOLEAN (L"Garnish", 1)
	OK
DO
	EVERY_DRAW (Formant_drawTracks ((structFormant *)OBJECT, GRAPHICS,
		GET_REAL (L"left Time range"), GET_REAL (L"right Time range"), GET_REAL (L"Maximum frequency"),
		GET_INTEGER (L"Garnish")))
END

int Formant_formula_bandwidths (Formant me, const wchar_t *formula, Interpreter *interpreter) {
	long iframe, iformant, nrow = Formant_getMaxNumFormants (me);
	Matrix mat = NULL;
	if (nrow < 1) return Melder_error1 (L"(Formant_formula_bandwidths:) No formants available.");
	mat = Matrix_create (my xmin, my xmax, my nx, my dx, my x1, 0.5, nrow + 0.5, nrow, 1, 1); cherror
	for (iframe = 1; iframe <= my nx; iframe ++) {
		Formant_Frame frame = & my frame [iframe];
		for (iformant = 1; iformant <= frame -> nFormants; iformant ++)
			mat -> z [iformant] [iframe] = frame -> formant [iformant]. bandwidth;
	}
	Matrix_formula (mat, formula, interpreter, NULL); cherror
	for (iframe = 1; iframe <= my nx; iframe ++) {
		Formant_Frame frame = & my frame [iframe];
		for (iformant = 1; iformant <= frame -> nFormants; iformant ++)
			frame -> formant [iformant]. bandwidth = mat -> z [iformant] [iframe];
	}
end:
	forget (mat);
	iferror return 0;
	return 1;
}

FORM (Formant_formula_bandwidths, L"Formant: Formula (bandwidths)", L"Formant: Formula (bandwidths)...")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"row is formant number, col is frame number: for row from 1 to nrow do for col from 1 to ncol do B (row, col) :=")
	TEXTFIELD (L"formula", L"self / 2 ; sharpen all peaks")
	OK
DO
	WHERE (SELECTED) {
		if (! Formant_formula_bandwidths ((structFormant *)OBJECT, GET_STRING (L"formula"), interpreter)) return 0;
		praat_dataChanged (OBJECT);
	}
END

int Formant_formula_frequencies (Formant me, const wchar_t *formula, Interpreter *interpreter) {
	long iframe, iformant, nrow = Formant_getMaxNumFormants (me);
	Matrix mat = NULL;
	if (nrow < 1) return Melder_error1 (L"(Formant_formula_frequencies:) No formants available.");
	mat = Matrix_create (my xmin, my xmax, my nx, my dx, my x1, 0.5, nrow + 0.5, nrow, 1, 1); cherror
	for (iframe = 1; iframe <= my nx; iframe ++) {
		Formant_Frame frame = & my frame [iframe];
		for (iformant = 1; iformant <= frame -> nFormants; iformant ++)
			mat -> z [iformant] [iframe] = frame -> formant [iformant]. frequency;
	}
	Matrix_formula (mat, formula, interpreter, NULL); cherror
	for (iframe = 1; iframe <= my nx; iframe ++) {
		Formant_Frame frame = & my frame [iframe];
		for (iformant = 1; iformant <= frame -> nFormants; iformant ++)
			frame -> formant [iformant]. frequency = mat -> z [iformant] [iframe];
	}
end:
	forget (mat);
	iferror return 0;
	return 1;
}

FORM (Formant_formula_frequencies, L"Formant: Formula (frequencies)", L"Formant: Formula (frequencies)...")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"row is formant number, col is frame number: for row from 1 to nrow do for col from 1 to ncol do F (row, col) :=")
	TEXTFIELD (L"formula", L"if row = 2 then self + 200 else self fi")
	OK
DO
	WHERE (SELECTED) {
		if (! Formant_formula_frequencies ((structFormant *)OBJECT, GET_STRING (L"formula"), interpreter)) return 0;
		praat_dataChanged (OBJECT);
	}
END

FORM (Formant_getBandwidthAtTime, L"Formant: Get bandwidth", L"Formant: Get bandwidth at time...")
	NATURAL (L"Formant number", L"1")
	REAL (L"Time (s)", L"0.5")
	RADIO (L"Unit", 1)
	RADIOBUTTON (L"Hertz")
	RADIOBUTTON (L"Bark")
	RADIO (L"Interpolation", 1)
	RADIOBUTTON (L"Linear")
	OK
DO
	Melder_informationReal (Formant_getBandwidthAtTime ((structFormant *)ONLY (classFormant), GET_INTEGER (L"Formant number"),
		GET_REAL (L"Time"), GET_INTEGER (L"Unit") - 1), GET_STRING (L"Unit"));
END
	
FORM (Formant_getMaximum, L"Formant: Get maximum", L"Formant: Get maximum...")
	NATURAL (L"Formant number", L"1")
	praat_dia_timeRange (dia);
	RADIO (L"Unit", 1)
	RADIOBUTTON (L"Hertz")
	RADIOBUTTON (L"Bark")
	RADIO (L"Interpolation", 2)
	RADIOBUTTON (L"None")
	RADIOBUTTON (L"Parabolic")
	OK
DO
	Melder_informationReal (Formant_getMaximum ((structFormant *)ONLY (classFormant), GET_INTEGER (L"Formant number"),
		GET_REAL (L"left Time range"), GET_REAL (L"right Time range"), GET_INTEGER (L"Unit") - 1,
		GET_INTEGER (L"Interpolation") - 1), GET_STRING (L"Unit"));
END

DIRECT (Formant_getMaximumNumberOfFormants)
	Melder_information2 (Melder_integer (Formant_getMaxNumFormants ((structFormant *)ONLY_OBJECT)),
		L" (there are at most this many formants in every frame)");
END

FORM (Formant_getMean, L"Formant: Get mean", L"Formant: Get mean...")
	NATURAL (L"Formant number", L"1")
	praat_dia_timeRange (dia);
	RADIO (L"Unit", 1)
	RADIOBUTTON (L"Hertz")
	RADIOBUTTON (L"Bark")
	OK
DO
	Melder_informationReal (Formant_getMean ((structFormant *)ONLY (classFormant), GET_INTEGER (L"Formant number"),
		GET_REAL (L"left Time range"), GET_REAL (L"right Time range"), GET_INTEGER (L"Unit") - 1), GET_STRING (L"Unit"));
END

FORM (Formant_getMinimum, L"Formant: Get minimum", L"Formant: Get minimum...")
	NATURAL (L"Formant number", L"1")
	praat_dia_timeRange (dia);
	RADIO (L"Unit", 1)
	RADIOBUTTON (L"Hertz")
	RADIOBUTTON (L"Bark")
	RADIO (L"Interpolation", 2)
	RADIOBUTTON (L"None")
	RADIOBUTTON (L"Parabolic")
	OK
DO
	Melder_informationReal (Formant_getMinimum ((structFormant *)ONLY (classFormant), GET_INTEGER (L"Formant number"),
		GET_REAL (L"left Time range"), GET_REAL (L"right Time range"), GET_INTEGER (L"Unit") - 1,
		GET_INTEGER (L"Interpolation") - 1), GET_STRING (L"Unit"));
END

DIRECT (Formant_getMinimumNumberOfFormants)
	Melder_information2 (Melder_integer (Formant_getMinNumFormants ((structFormant *)ONLY_OBJECT)),
		L" (there are at least this many formants in every frame)");
END

FORM (Formant_getNumberOfFormants, L"Formant: Get number of formants", L"Formant: Get number of formants...")
	NATURAL (L"Frame number", L"1")
	OK
DO
	long frame = GET_INTEGER (L"Frame number");
	Formant me = (structFormant *)ONLY_OBJECT;
	if (frame > my nx) return Melder_error5 (L"There is no frame ", Melder_integer (frame),
		L" in a Formant with only ", Melder_integer (my nx), L" frames.");
	Melder_information2 (Melder_integer (my frame [frame]. nFormants), L" formants");
END

FORM (Formant_getQuantile, L"Formant: Get quantile", 0)
	NATURAL (L"Formant number", L"1")
	praat_dia_timeRange (dia);
	RADIO (L"Unit", 1)
	RADIOBUTTON (L"Hertz")
	RADIOBUTTON (L"Bark")
	REAL (L"Quantile", L"0.50 (= median)")
	OK
DO
	Melder_informationReal (Formant_getQuantile ((structFormant *)ONLY (classFormant), GET_INTEGER (L"Formant number"),
		GET_REAL (L"Quantile"), GET_REAL (L"left Time range"), GET_REAL (L"right Time range"), GET_INTEGER (L"Unit") - 1), GET_STRING (L"Unit"));
END

FORM (Formant_getQuantileOfBandwidth, L"Formant: Get quantile of bandwidth", 0)
	NATURAL (L"Formant number", L"1")
	praat_dia_timeRange (dia);
	RADIO (L"Unit", 1)
	RADIOBUTTON (L"Hertz")
	RADIOBUTTON (L"Bark")
	REAL (L"Quantile", L"0.50 (= median)")
	OK
DO
	Melder_informationReal (Formant_getQuantileOfBandwidth ((structFormant *)ONLY (classFormant), GET_INTEGER (L"Formant number"),
		GET_REAL (L"Quantile"), GET_REAL (L"left Time range"), GET_REAL (L"right Time range"), GET_INTEGER (L"Unit") - 1), GET_STRING (L"Unit"));
END

FORM (Formant_getStandardDeviation, L"Formant: Get standard deviation", 0)
	NATURAL (L"Formant number", L"1")
	praat_dia_timeRange (dia);
	RADIO (L"Unit", 1)
	RADIOBUTTON (L"Hertz")
	RADIOBUTTON (L"Bark")
	OK
DO
	Melder_informationReal (Formant_getStandardDeviation ((structFormant *)ONLY (classFormant), GET_INTEGER (L"Formant number"),
		GET_REAL (L"left Time range"), GET_REAL (L"right Time range"), GET_INTEGER (L"Unit") - 1), GET_STRING (L"Unit"));
END

FORM (Formant_getTimeOfMaximum, L"Formant: Get time of maximum", L"Formant: Get time of maximum...")
	NATURAL (L"Formant number", L"1")
	praat_dia_timeRange (dia);
	RADIO (L"Unit", 1)
	RADIOBUTTON (L"Hertz")
	RADIOBUTTON (L"Bark")
	RADIO (L"Interpolation", 2)
	RADIOBUTTON (L"None")
	RADIOBUTTON (L"Parabolic")
	OK
DO
	Melder_informationReal (Formant_getTimeOfMaximum ((structFormant *)ONLY (classFormant), GET_INTEGER (L"Formant number"),
		GET_REAL (L"left Time range"), GET_REAL (L"right Time range"),
		GET_INTEGER (L"Unit") - 1, GET_INTEGER (L"Interpolation") - 1), L"seconds");
END

FORM (Formant_getTimeOfMinimum, L"Formant: Get time of minimum", L"Formant: Get time of minimum...")
	NATURAL (L"Formant number", L"1")
	praat_dia_timeRange (dia);
	RADIO (L"Unit", 1)
	RADIOBUTTON (L"Hertz")
	RADIOBUTTON (L"Bark")
	RADIO (L"Interpolation", 2)
	RADIOBUTTON (L"None")
	RADIOBUTTON (L"Parabolic")
	OK
DO
	Melder_informationReal (Formant_getTimeOfMinimum ((structFormant *)ONLY (classFormant), GET_INTEGER (L"Formant number"),
		GET_REAL (L"left Time range"), GET_REAL (L"right Time range"),
		GET_INTEGER (L"Unit") - 1, GET_INTEGER (L"Interpolation") - 1), L"seconds");
END

FORM (Formant_getValueAtTime, L"Formant: Get value", L"Formant: Get value at time...")
	NATURAL (L"Formant number", L"1")
	REAL (L"Time (s)", L"0.5")
	RADIO (L"Unit", 1)
	RADIOBUTTON (L"Hertz")
	RADIOBUTTON (L"Bark")
	RADIO (L"Interpolation", 1)
	RADIOBUTTON (L"Linear")
	OK
DO
	Melder_informationReal (Formant_getValueAtTime ((structFormant *)ONLY (classFormant), GET_INTEGER (L"Formant number"),
		GET_REAL (L"Time"), GET_INTEGER (L"Unit") - 1), GET_STRING (L"Unit"));
END
	
DIRECT (Formant_help) Melder_help (L"Formant"); END

FORM (Formant_downto_Table, L"Formant: Down to Table", 0)
	BOOLEAN (L"Include frame number", false)
	BOOLEAN (L"Include time", true)
	NATURAL (L"Time decimals", L"6")
	BOOLEAN (L"Include intensity", false)
	NATURAL (L"Intensity decimals", L"3")
	BOOLEAN (L"Include number of formants", true)
	NATURAL (L"Frequency decimals", L"3")
	BOOLEAN (L"Include bandwidths", true)
	OK
DO
	EVERY_TO (Formant_downto_Table ((structFormant *)OBJECT, GET_INTEGER (L"Include frame number"),
		GET_INTEGER (L"Include time"), GET_INTEGER (L"Time decimals"),
		GET_INTEGER (L"Include intensity"), GET_INTEGER (L"Intensity decimals"),
		GET_INTEGER (L"Include number of formants"), GET_INTEGER (L"Frequency decimals"),
		GET_INTEGER (L"Include bandwidths")))
END

FORM (Formant_list, L"Formant: List", 0)
	BOOLEAN (L"Include frame number", false)
	BOOLEAN (L"Include time", true)
	NATURAL (L"Time decimals", L"6")
	BOOLEAN (L"Include intensity", false)
	NATURAL (L"Intensity decimals", L"3")
	BOOLEAN (L"Include number of formants", true)
	NATURAL (L"Frequency decimals", L"3")
	BOOLEAN (L"Include bandwidths", true)
	OK
DO
	EVERY (Formant_list ((structFormant *)OBJECT, GET_INTEGER (L"Include frame number"),
		GET_INTEGER (L"Include time"), GET_INTEGER (L"Time decimals"),
		GET_INTEGER (L"Include intensity"), GET_INTEGER (L"Intensity decimals"),
		GET_INTEGER (L"Include number of formants"), GET_INTEGER (L"Frequency decimals"),
		GET_INTEGER (L"Include bandwidths")))
END

FORM (Formant_scatterPlot, L"Formant: Scatter plot", 0)
	praat_dia_timeRange (dia);
	NATURAL (L"Horizontal formant number", L"2")
	REAL (L"left Horizontal range (Hz)", L"3000")
	REAL (L"right Horizontal range (Hz)", L"400")
	NATURAL (L"Vertical formant number", L"1")
	REAL (L"left Vertical range (Hz)", L"1500")
	REAL (L"right Vertical range (Hz)", L"100")
	POSITIVE (L"Mark size (mm)", L"1.0")
	BOOLEAN (L"Garnish", 1)
	SENTENCE (L"Mark string (+xo.)", L"+")
	OK
DO
	EVERY_DRAW (Formant_scatterPlot ((structFormant *)OBJECT, GRAPHICS,
		GET_REAL (L"left Time range"), GET_REAL (L"right Time range"),
		GET_INTEGER (L"Horizontal formant number"),
		GET_REAL (L"left Horizontal range"), GET_REAL (L"right Horizontal range"),
		GET_INTEGER (L"Vertical formant number"),
		GET_REAL (L"left Vertical range"), GET_REAL (L"right Vertical range"),
		GET_REAL (L"Mark size"), GET_STRING (L"Mark string"), GET_INTEGER (L"Garnish")))
END

DIRECT (Formant_sort)
	WHERE (SELECTED) {
		Formant_sort ((structFormant *)OBJECT);
		praat_dataChanged (OBJECT);
	}
END

FORM (Formant_to_Matrix, L"From Formant to Matrix", 0)
	INTEGER (L"Formant", L"1")
	OK
DO
	EVERY_TO (Formant_to_Matrix ((structFormant *)OBJECT, GET_INTEGER (L"Formant")))
END

FORM (Formant_tracker, L"Formant tracker", L"Formant: Track...")
	NATURAL (L"Number of tracks (1-5)", L"3")
	REAL (L"Reference F1 (Hz)", L"550")
	REAL (L"Reference F2 (Hz)", L"1650")
	REAL (L"Reference F3 (Hz)", L"2750")
	REAL (L"Reference F4 (Hz)", L"3850")
	REAL (L"Reference F5 (Hz)", L"4950")
	REAL (L"Frequency cost (/kHz)", L"1.0")
	REAL (L"Bandwidth cost", L"1.0")
	REAL (L"Transition cost (/octave)", L"1.0")
	OK
DO
	long numberOfTracks = GET_INTEGER (L"Number of tracks");
	REQUIRE (numberOfTracks <= 5, L"Number of tracks cannot be more than 5.")
	EVERY_TO (Formant_tracker ((structFormant *)OBJECT, GET_INTEGER (L"Number of tracks"),
		GET_REAL (L"Reference F1"), GET_REAL (L"Reference F2"),
		GET_REAL (L"Reference F3"), GET_REAL (L"Reference F4"),
		GET_REAL (L"Reference F5"), GET_REAL (L"Frequency cost"),
		GET_REAL (L"Bandwidth cost"), GET_REAL (L"Transition cost")))
END

/***** FORMANT & POINTPROCESS *****/

DIRECT (Formant_PointProcess_to_FormantTier)
	Formant formant = (structFormant *)ONLY (classFormant);
	PointProcess point = (structPointProcess *)ONLY (classPointProcess);
	if (! praat_new3 (Formant_PointProcess_to_FormantTier (formant, point),
		formant -> name, L"_", point -> name)) return 0;
END

/***** FORMANT & SOUND *****/

DIRECT (Sound_Formant_filter)
	Sound me = (structSound *)ONLY (classSound);
	if (! praat_new2 (Sound_Formant_filter (me, (structFormant *)ONLY (classFormant)), my name, L"_filt")) return 0;
END

DIRECT (Sound_Formant_filter_noscale)
	Sound me = (structSound *)ONLY (classSound);
	if (! praat_new2 (Sound_Formant_filter_noscale (me, (structFormant *)ONLY (classFormant)), my name, L"_filt")) return 0;
END

/***** FORMANTGRID *****/

FORM (FormantGrid_addBandwidthPoint, L"FormantGrid: Add bandwidth point", L"FormantGrid: Add bandwidth point...")
	NATURAL (L"Formant number", L"1")
	REAL (L"Time (s)", L"0.5")
	POSITIVE (L"Bandwidth (Hz)", L"100")
	OK
DO
	WHERE (SELECTED) {
		if (! FormantGrid_addBandwidthPoint ((structFormantGrid *)OBJECT, GET_INTEGER (L"Formant number"),
			GET_REAL (L"Time"), GET_REAL (L"Bandwidth"))) return 0;
		praat_dataChanged (OBJECT);
	}
END

FORM (FormantGrid_addFormantPoint, L"FormantGrid: Add formant point", L"FormantGrid: Add formant point...")
	NATURAL (L"Formant number", L"1")
	REAL (L"Time (s)", L"0.5")
	POSITIVE (L"Frequency (Hz)", L"550")
	OK
DO
	WHERE (SELECTED) {
		if (! FormantGrid_addFormantPoint ((structFormantGrid *)OBJECT, GET_INTEGER (L"Formant number"),
			GET_REAL (L"Time"), GET_REAL (L"Frequency"))) return 0;
		praat_dataChanged (OBJECT);
	}
END

FORM (FormantGrid_create, L"Create FormantGrid", NULL)
	WORD (L"Name", L"schwa")
	REAL (L"Start time (s)", L"0.0")
	REAL (L"End time (s)", L"1.0")
	NATURAL (L"Number of formants", L"10")
	POSITIVE (L"Initial first formant (Hz)", L"550")
	POSITIVE (L"Initial formant spacing (Hz)", L"1100")
	REAL (L"Initial first bandwidth (Hz)", L"60")
	REAL (L"Initial bandwidth spacing (Hz)", L"50")
	OK
DO
	double startTime = GET_REAL (L"Start time"), endTime = GET_REAL (L"End time");
	REQUIRE (endTime > startTime, L"End time must be greater than start time.")
	if (! praat_new1 (FormantGrid_create (startTime, endTime, GET_INTEGER (L"Number of formants"),
		GET_REAL (L"Initial first formant"), GET_REAL (L"Initial formant spacing"),
		GET_REAL (L"Initial first bandwidth"), GET_REAL (L"Initial bandwidth spacing")), GET_STRING (L"Name"))) return 0;
END

static void cb_FormantGridEditor_publish (Editor *editor, void *closure, Any publish) {
	(void) editor;
	(void) closure;
	if (! praat_new1 (publish, L"fromFormantGridEditor")) { Melder_flushError (NULL); return; }
	praat_updateSelection ();
}
DIRECT (FormantGrid_edit)
	if (theCurrentPraatApplication -> batch) {
		return Melder_error1 (L"Cannot edit a FormantGrid from batch.");
	} else {
		WHERE (SELECTED) {
			FormantGridEditor *editor = new FormantGridEditor (theCurrentPraatApplication -> topShell, ID_AND_FULL_NAME, (structFormantGrid *)OBJECT);
			if (! praat_installEditor (editor, IOBJECT)) return 0;
			editor->setPublishCallback (cb_FormantGridEditor_publish, NULL);
		}
	}
END

int FormantGrid_formula_bandwidths (I, const wchar_t *expression, Interpreter *interpreter, thou) {
	iam (FormantGrid);
	thouart (FormantGrid);
	Formula_compile (interpreter, me, expression, kFormula_EXPRESSION_TYPE_NUMERIC, TRUE); cherror
	if (thee == NULL) thee = me;
	for (long irow = 1; irow <= my formants -> size; irow ++) {
		RealTier bandwidth = (structRealTier *)thy bandwidths -> item [irow];
		for (long icol = 1; icol <= bandwidth -> points -> size; icol ++) {
			struct Formula_Result result;
			Formula_run (irow, icol, & result); cherror
			if (result. result.numericResult == NUMundefined)
				error1 (L"Cannot put an undefined value into the tier.\nFormula not finished.")
			((RealPoint) bandwidth -> points -> item [icol]) -> value = result. result.numericResult;
		}
	}
end:
	iferror return 0;
	return 1;
}

FORM (FormantGrid_formula_bandwidths, L"FormantGrid: Formula (bandwidths)", L"Formant: Formula (bandwidths)...")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"row is formant number, col is point number: for row from 1 to nrow do for col from 1 to ncol do B (row, col) :=")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"self [] is the FormantGrid itself, so it returns frequencies, not bandwidths!")
	TEXTFIELD (L"formula", L"self / 10 ; one tenth of the formant frequency")
	OK
DO
	WHERE (SELECTED) {
		if (! FormantGrid_formula_bandwidths ((structFormantGrid *)OBJECT, GET_STRING (L"formula"), interpreter, NULL)) return 0;
		praat_dataChanged (OBJECT);
	}
END

int FormantGrid_formula_frequencies (I, const wchar_t *expression, Interpreter *interpreter, thou) {
	iam (FormantGrid);
	thouart (FormantGrid);
	Formula_compile (interpreter, me, expression, kFormula_EXPRESSION_TYPE_NUMERIC, TRUE); cherror
	if (thee == NULL) thee = me;
	for (long irow = 1; irow <= my formants -> size; irow ++) {
		RealTier formant = (structRealTier *)thy formants -> item [irow];
		for (long icol = 1; icol <= formant -> points -> size; icol ++) {
			struct Formula_Result result;
			Formula_run (irow, icol, & result); cherror
			if (result. result.numericResult == NUMundefined)
				error1 (L"Cannot put an undefined value into the tier.\nFormula not finished.")
			((RealPoint) formant -> points -> item [icol]) -> value = result. result.numericResult;
		}
	}
end:
	iferror return 0;
	return 1;
}

FORM (FormantGrid_formula_frequencies, L"FormantGrid: Formula (frequencies)", L"Formant: Formula (frequencies)...")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"row is formant number, col is point number: for row from 1 to nrow do for col from 1 to ncol do F (row, col) :=")
	TEXTFIELD (L"formula", L"if row = 2 then self + 200 else self fi")
	OK
DO
	WHERE (SELECTED) {
		if (! FormantGrid_formula_frequencies ((structFormantGrid *)OBJECT, GET_STRING (L"formula"), interpreter, NULL)) return 0;
		praat_dataChanged (OBJECT);
	}
END

DIRECT (FormantGrid_help) Melder_help (L"FormantGrid"); END

FORM (FormantGrid_removeBandwidthPointsBetween, L"Remove bandwidth points between", L"FormantGrid: Remove bandwidth points between...")
	NATURAL (L"Formant number", L"1")
	REAL (L"From time (s)", L"0.3")
	REAL (L"To time (s)", L"0.7")
	OK
DO
	WHERE (SELECTED) {
		FormantGrid_removeBandwidthPointsBetween ((structFormantGrid *)OBJECT, GET_INTEGER (L"Formant number"),
			GET_REAL (L"From time"), GET_REAL (L"To time"));
		praat_dataChanged (OBJECT);
	}
END

FORM (FormantGrid_removeFormantPointsBetween, L"Remove formant points between", L"FormantGrid: Remove formant points between...")
	NATURAL (L"Formant number", L"1")
	REAL (L"From time (s)", L"0.3")
	REAL (L"To time (s)", L"0.7")
	OK
DO
	WHERE (SELECTED) {
		FormantGrid_removeFormantPointsBetween ((structFormantGrid *)OBJECT, GET_INTEGER (L"Formant number"),
			GET_REAL (L"From time"), GET_REAL (L"To time"));
		praat_dataChanged (OBJECT);
	}
END

FORM (FormantGrid_to_Formant, L"FormantGrid: To Formant", 0)
	POSITIVE (L"Time step (s)", L"0.01")
	REAL (L"Intensity (Pa\u00B2)", L"0.1")
	OK
DO
	double intensity = GET_REAL (L"Intensity");
	REQUIRE (intensity >= 0.0, L"Intensity cannot be negative.")
	EVERY_TO (FormantGrid_to_Formant ((structFormantGrid *)OBJECT, GET_REAL (L"Time step"), intensity))
END

/***** FORMANTGRID & SOUND *****/

DIRECT (Sound_FormantGrid_filter)
	Sound me = (structSound *)ONLY (classSound);
	if (! praat_new2 (Sound_FormantGrid_filter (me, (structFormantGrid *)ONLY (classFormantGrid)), my name, L"_filt")) return 0;
END

DIRECT (Sound_FormantGrid_filter_noscale)
	Sound me = (structSound *)ONLY (classSound);
	if (! praat_new2 (Sound_FormantGrid_filter_noscale (me, (structFormantGrid *)ONLY (classFormantGrid)), my name, L"_filt")) return 0;
END

/***** FORMANTTIER *****/

void FormantTier_speckle (FormantTier me, Graphics g, double tmin, double tmax, double fmax, int garnish) {
	long imin, imax, i, j;
	if (tmax <= tmin) { tmin = my xmin; tmax = my xmax; }
	Graphics_setWindow (g, tmin, tmax, 0.0, fmax);
	Graphics_setInner (g);
	imin = AnyTier_timeToHighIndex (me, tmin);
	imax = AnyTier_timeToLowIndex (me, tmax);
	if (imin > 0) for (i = imin; i <= imax; i ++) {
		FormantPoint point = (structFormantPoint *)my points -> item [i];
		double t = point -> time;
		for (j = 1; j <= point -> numberOfFormants; j ++) {
			double f = point -> formant [j-1];
			if (f <= fmax) Graphics_fillCircle_mm (g, t, f, 1.0);
		}
	}
	Graphics_unsetInner (g);
	if (garnish) {
		Graphics_drawInnerBox (g);
		Graphics_textBottom (g, TRUE, L"Time (s)");
		Graphics_marksBottom (g, 2, TRUE, TRUE, FALSE);
		Graphics_marksLeft (g, 2, TRUE, TRUE, FALSE);
		Graphics_textLeft (g, TRUE, L"Frequency (Hz)");
	}
}

FORM (FormantTier_addPoint, L"Add one point", L"FormantTier: Add point...")
	REAL (L"Time (s)", L"0.5")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"Frequencies and bandwidths (Hz):")
	TEXTFIELD (L"fb pairs", L"500 50 1500 100 2500 150 3500 200 4500 300")
	OK
DO
	FormantPoint point = FormantPoint_create (GET_REAL (L"Time"));
	double *f = point -> formant, *b = point -> bandwidth;
	char *fbpairs = Melder_peekWcsToUtf8 (GET_STRING (L"fb pairs"));
	int numberOfFormants = sscanf (fbpairs, "%lf%lf%lf%lf%lf%lf%lf%lf%lf%lf%lf%lf%lf%lf%lf%lf%lf%lf%lf%lf",
		f, b, f+1, b+1, f+2, b+2, f+3, b+3, f+4, b+4, f+5, b+5, f+6, b+6, f+7, b+7, f+8, b+8, f+9, b+9) / 2;
	if (numberOfFormants < 1) {
		forget (point);
		return Melder_error1 (L"Number of formant-bandwidth pairs must be at least 1.");
	}
	point -> numberOfFormants = numberOfFormants;
	WHERE (SELECTED) {
		if (! AnyTier_addPoint (OBJECT, Data_copy (point))) { forget (point); return 0; }
		praat_dataChanged (OBJECT);
	}
	forget (point);
END

FORM (FormantTier_create, L"Create empty FormantTier", NULL)
	WORD (L"Name", L"empty")
	REAL (L"Start time (s)", L"0.0")
	REAL (L"End time (s)", L"1.0")
	OK
DO
	double startTime = GET_REAL (L"Start time"), endTime = GET_REAL (L"End time");
	REQUIRE (endTime > startTime, L"End time must be greater than start time.")
	if (! praat_new1 (FormantTier_create (startTime, endTime), GET_STRING (L"Name"))) return 0;
END

FORM (FormantTier_downto_TableOfReal, L"Down to TableOfReal", 0)
	BOOLEAN (L"Include formants", 1)
	BOOLEAN (L"Include bandwidths", 0)
	OK
DO
	EVERY_TO (FormantTier_downto_TableOfReal ((structFormantTier *)OBJECT, GET_INTEGER (L"Include formants"), GET_INTEGER (L"Include bandwidths")))
END

FORM (FormantTier_getBandwidthAtTime, L"FormantTier: Get bandwidth", L"FormantTier: Get bandwidth at time...")
	NATURAL (L"Formant number", L"1")
	REAL (L"Time (s)", L"0.5")
	OK
DO
	Melder_informationReal (FormantTier_getBandwidthAtTime ((structFormantTier *)ONLY (classFormantTier), GET_INTEGER (L"Formant number"),
		GET_REAL (L"Time")), L"Hertz");
END
	
FORM (FormantTier_getValueAtTime, L"FormantTier: Get value", L"FormantTier: Get value at time...")
	NATURAL (L"Formant number", L"1")
	REAL (L"Time (s)", L"0.5")
	OK
DO
	Melder_informationReal (FormantTier_getValueAtTime ((structFormantTier *)ONLY (classFormantTier), GET_INTEGER (L"Formant number"),
		GET_REAL (L"Time")), L"Hertz");
END
	
DIRECT (FormantTier_help) Melder_help (L"FormantTier"); END

FORM (FormantTier_speckle, L"Draw FormantTier", 0)
	praat_dia_timeRange (dia);
	POSITIVE (L"Maximum frequency (Hz)", L"5500.0")
	BOOLEAN (L"Garnish", 1)
	OK
DO
	EVERY_DRAW (FormantTier_speckle ((structFormantTier *)OBJECT, GRAPHICS,
		GET_REAL (L"left Time range"), GET_REAL (L"right Time range"), GET_REAL (L"Maximum frequency"), GET_INTEGER (L"Garnish")))
END

/***** FORMANTTIER & SOUND *****/

DIRECT (Sound_FormantTier_filter)
	Sound me = (structSound *)ONLY (classSound);
	if (! praat_new2 (Sound_FormantTier_filter (me, (structFormantTier *)ONLY (classFormantTier)), my name, L"_filt")) return 0;
END

DIRECT (Sound_FormantTier_filter_noscale)
	Sound me = (structSound *)ONLY (classSound);
	if (! praat_new2 (Sound_FormantTier_filter_noscale (me, (structFormantTier *)ONLY (classFormantTier)), my name, L"_filt")) return 0;
END

/***** HARMONICITY *****/

void Matrix_drawRows (I, Graphics g, double xmin, double xmax, double ymin, double ymax,
	double minimum, double maximum);

FORM (Harmonicity_draw, L"Draw harmonicity", 0)
	praat_dia_timeRange (dia);
	REAL (L"Minimum", L"0.0")
	REAL (L"Maximum", L"0.0 (= auto)")
	OK
DO
	EVERY_DRAW (Matrix_drawRows ((structMatrix *)OBJECT, GRAPHICS,
		GET_REAL (L"left Time range"), GET_REAL (L"right Time range"), 0.0, 0.0,
		GET_REAL (L"Minimum"), GET_REAL (L"Maximum")))
END

FORM (Harmonicity_formula, L"Harmonicity Formula", L"Harmonicity: Formula...")
	LABEL (L"label", L"x is time")
	LABEL (L"label", L"for col := 1 to ncol do { self [col] := `formula' ; x := x + dx }")
	TEXTFIELD (L"formula", L"self")
	OK
DO
	if (! praat_Fon_formula (dia, interpreter)) return 0;
END

FORM (Harmonicity_getMaximum, L"Harmonicity: Get maximum", L"Harmonicity: Get maximum...")
	dia_Vector_getExtremum (dia);
	OK
DO
	Melder_informationReal (Vector_getMaximum ((structVector *)ONLY (classHarmonicity),
		GET_REAL (L"left Time range"), GET_REAL (L"right Time range"), GET_INTEGER (L"Interpolation") - 1), L"dB");
END

FORM (Harmonicity_getMean, L"Harmonicity: Get mean", L"Harmonicity: Get mean...")
	praat_dia_timeRange (dia);
	OK
DO
	Melder_informationReal (Harmonicity_getMean ((structHarmonicity *)ONLY (classHarmonicity),
		GET_REAL (L"left Time range"), GET_REAL (L"right Time range")), L"dB");
END

FORM (Harmonicity_getMinimum, L"Harmonicity: Get minimum", L"Harmonicity: Get minimum...")
	dia_Vector_getExtremum (dia);
	OK
DO
	Melder_informationReal (Vector_getMinimum ((structVector *)ONLY (classHarmonicity),
		GET_REAL (L"left Time range"), GET_REAL (L"right Time range"), GET_INTEGER (L"Interpolation") - 1), L"dB");
END

FORM (Harmonicity_getStandardDeviation, L"Harmonicity: Get standard deviation", L"Harmonicity: Get standard deviation...")
	praat_dia_timeRange (dia);
	OK
DO
	Melder_informationReal (Harmonicity_getStandardDeviation ((structHarmonicity *)ONLY (classHarmonicity),
		GET_REAL (L"left Time range"), GET_REAL (L"right Time range")), L"dB");
END

FORM (Harmonicity_getTimeOfMaximum, L"Harmonicity: Get time of maximum", L"Harmonicity: Get time of maximum...")
	dia_Vector_getExtremum (dia);
	OK
DO
	Melder_informationReal (Vector_getXOfMaximum (ONLY (classHarmonicity),
		GET_REAL (L"left Time range"), GET_REAL (L"right Time range"), GET_INTEGER (L"Interpolation") - 1), L"seconds");
END

FORM (Harmonicity_getTimeOfMinimum, L"Harmonicity: Get time of minimum", L"Harmonicity: Get time of minimum...")
	dia_Vector_getExtremum (dia);
	OK
DO
	Melder_informationReal (Vector_getXOfMinimum (ONLY (classHarmonicity),
		GET_REAL (L"left Time range"), GET_REAL (L"right Time range"), GET_INTEGER (L"Interpolation") - 1), L"seconds");
END

FORM (Harmonicity_getValueAtTime, L"Harmonicity: Get value", L"Harmonicity: Get value at time...")
	dia_Vector_getValue (dia);
	OK
DO
	Melder_informationReal (Vector_getValueAtX (ONLY (classHarmonicity), GET_REAL (L"Time"), 1, GET_INTEGER (L"Interpolation") - 1), L"dB");
END
	
FORM (Harmonicity_getValueInFrame, L"Get value in frame", L"Harmonicity: Get value in frame...")
	INTEGER (L"Frame number", L"10")
	OK
DO
	Harmonicity me = (structHarmonicity *)ONLY (classHarmonicity);
	long frameNumber = GET_INTEGER (L"Frame number");
	Melder_informationReal (frameNumber < 1 || frameNumber > my nx ? NUMundefined : my z [1] [frameNumber], L"dB");
END

DIRECT (Harmonicity_help) Melder_help (L"Harmonicity"); END

DIRECT (Harmonicity_to_Matrix)
	EVERY_TO (Harmonicity_to_Matrix ((structHarmonicity *)OBJECT))
END

/***** INTENSITY *****/

void Intensity_drawInside (Intensity me, Graphics g, double tmin, double tmax, double minimum, double maximum) {
	long itmin, itmax;
	if (tmax <= tmin) { tmin = my xmin; tmax = my xmax; }   /* Autowindow. */
	Matrix_getWindowSamplesX (me, tmin, tmax, & itmin, & itmax);
	if (maximum <= minimum)
		Matrix_getWindowExtrema (me, itmin, itmax, 1, 1, & minimum, & maximum);   /* Autoscale. */
	if (maximum <= minimum) { minimum -= 10; maximum += 10; }
	Graphics_setWindow (g, tmin, tmax, minimum, maximum);
	Graphics_function (g, my z [1], itmin, itmax, Matrix_columnToX (me, itmin), Matrix_columnToX (me, itmax));
}

void Intensity_draw (Intensity me, Graphics g, double tmin, double tmax,
	double minimum, double maximum, int garnish)
{
	Graphics_setInner (g);
	Intensity_drawInside (me, g, tmin, tmax, minimum, maximum);
	Graphics_unsetInner (g);
	if (garnish) {
		Graphics_drawInnerBox (g);
		Graphics_textBottom (g, 1, L"Time (s)");
		Graphics_marksBottom (g, 2, 1, 1, 0);
		Graphics_marksLeft (g, 2, 1, 1, 0);
		Graphics_textLeft (g, 1, L"Intensity (dB)");
	}
}

FORM (Intensity_draw, L"Draw Intensity", 0)
	praat_dia_timeRange (dia);
	REAL (L"Minimum (dB)", L"0.0")
	REAL (L"Maximum (dB)", L"0.0 (= auto)")
	BOOLEAN (L"Garnish", 1)
	OK
DO
	EVERY_DRAW (Intensity_draw ((structIntensity *)OBJECT, GRAPHICS, GET_REAL (L"left Time range"), GET_REAL (L"right Time range"),
		GET_REAL (L"Minimum"), GET_REAL (L"Maximum"), GET_INTEGER (L"Garnish")))
END

DIRECT (Intensity_downto_IntensityTier)
	EVERY_TO (Intensity_downto_IntensityTier ((structIntensity *)OBJECT))
END

DIRECT (Intensity_downto_Matrix)
	EVERY_TO (Intensity_to_Matrix ((structIntensity *)OBJECT))
END

FORM (Intensity_formula, L"Intensity Formula", 0)
	LABEL (L"label", L"`x' is the time in seconds, `col' is the frame number, `self' is in dB")
	LABEL (L"label", L"x := x1;   for col := 1 to ncol do { self [col] := `formula' ; x := x + dx }")
	TEXTFIELD (L"formula", L"0")
	OK
DO
	if (! praat_Fon_formula (dia, interpreter)) return 0;
END

FORM (Intensity_getMaximum, L"Intensity: Get maximum", L"Intensity: Get maximum...")
	dia_Vector_getExtremum (dia);
	OK
DO
	Melder_informationReal (Vector_getMaximum (ONLY (classIntensity),
		GET_REAL (L"left Time range"), GET_REAL (L"right Time range"), GET_INTEGER (L"Interpolation") - 1), L"dB");
END

FORM (old_Intensity_getMean, L"Intensity: Get mean", L"Intensity: Get mean...")
	praat_dia_timeRange (dia);
	OK
DO
	Melder_informationReal (Sampled_getMean_standardUnit (ONLY (classIntensity), GET_REAL (L"left Time range"), GET_REAL (L"right Time range"),
		0, 0, TRUE), L"dB");
END

FORM (Intensity_getMean, L"Intensity: Get mean", L"Intensity: Get mean...")
	praat_dia_timeRange (dia);
	RADIO (L"Averaging method", 1)
		RADIOBUTTON (L"energy")
		RADIOBUTTON (L"sones")
		RADIOBUTTON (L"dB")
	OK
DO_ALTERNATIVE (old_Intensity_getMean)
	Melder_informationReal (Sampled_getMean_standardUnit (ONLY (classIntensity), GET_REAL (L"left Time range"), GET_REAL (L"right Time range"),
		0, GET_INTEGER (L"Averaging method"), TRUE), L"dB");
END

FORM (Intensity_getMinimum, L"Intensity: Get minimum", L"Intensity: Get minimum...")
	dia_Vector_getExtremum (dia);
	OK
DO
	Melder_informationReal (Vector_getMinimum (ONLY (classIntensity),
		GET_REAL (L"left Time range"), GET_REAL (L"right Time range"), GET_INTEGER (L"Interpolation") - 1), L"dB");
END

FORM (Intensity_getQuantile, L"Intensity: Get quantile", 0)
	praat_dia_timeRange (dia);
	REAL (L"Quantile (0-1)", L"0.50")
	OK
DO
	Melder_informationReal (Intensity_getQuantile ((structIntensity *)ONLY (classIntensity), GET_REAL (L"left Time range"), GET_REAL (L"right Time range"),
		GET_REAL (L"Quantile")), L"dB");
END

FORM (Intensity_getStandardDeviation, L"Intensity: Get standard deviation", L"Intensity: Get standard deviation...")
	praat_dia_timeRange (dia);
	OK
DO
	Melder_informationReal (Vector_getStandardDeviation (ONLY (classIntensity), GET_REAL (L"left Time range"), GET_REAL (L"right Time range"), 1), L"dB");
END

FORM (Intensity_getTimeOfMaximum, L"Intensity: Get time of maximum", L"Intensity: Get time of maximum...")
	dia_Vector_getExtremum (dia);
	OK
DO
	Melder_informationReal (Vector_getXOfMaximum (ONLY (classIntensity),
		GET_REAL (L"left Time range"), GET_REAL (L"right Time range"), GET_INTEGER (L"Interpolation") - 1), L"seconds");
END

FORM (Intensity_getTimeOfMinimum, L"Intensity: Get time of minimum", L"Intensity: Get time of minimum...")
	dia_Vector_getExtremum (dia);
	OK
DO
	Melder_informationReal (Vector_getXOfMinimum (ONLY (classIntensity),
		GET_REAL (L"left Time range"), GET_REAL (L"right Time range"), GET_INTEGER (L"Interpolation") - 1), L"seconds");
END

FORM (Intensity_getValueAtTime, L"Intensity: Get value", L"Intensity: Get value at time...")
	dia_Vector_getValue (dia);
	OK
DO
	Melder_informationReal (Vector_getValueAtX (ONLY (classIntensity), GET_REAL (L"Time"), 1, GET_INTEGER (L"Interpolation") - 1), L"dB");
END
	
FORM (Intensity_getValueInFrame, L"Get value in frame", L"Intensity: Get value in frame...")
	INTEGER (L"Frame number", L"10")
	OK
DO
	Intensity me = (structIntensity *)ONLY (classIntensity);
	long frameNumber = GET_INTEGER (L"Frame number");
	Melder_informationReal (frameNumber < 1 || frameNumber > my nx ? NUMundefined : my z [1] [frameNumber], L"dB");
END

DIRECT (Intensity_help) Melder_help (L"Intensity"); END

DIRECT (Intensity_to_IntensityTier_peaks)
	EVERY_TO (Intensity_to_IntensityTier_peaks ((structIntensity *)OBJECT))
END

DIRECT (Intensity_to_IntensityTier_valleys)
	EVERY_TO (Intensity_to_IntensityTier_valleys ((structIntensity *)OBJECT))
END

/***** INTENSITY & PITCH *****/

static void Pitch_getExtrema (Pitch me, double *minimum, double *maximum) {
	long i;
	*minimum = 1e30, *maximum = -1e30;
	for (i = 1; i <= my nx; i ++) {
		double frequency = my frame [i]. candidate [1]. frequency;
		if (frequency == 0.0) continue;   /* Voiceless. */
		if (frequency < *minimum) *minimum = frequency;
		if (frequency > *maximum) *maximum = frequency;
	}
	if (*maximum == -1e30) *maximum = 0.0;
	if (*minimum == 1e30) *minimum = 0.0;
}

void Pitch_Intensity_draw (Pitch pitch, Intensity intensity, Graphics g,
	double f1, double f2, double s1, double s2, int garnish, int connect)
{
	long i, previousI = 0;
	double previousX = NUMundefined, previousY = NUMundefined;
	if (f2 <= f1) Pitch_getExtrema (pitch, & f1, & f2);
	if (f1 == 0.0) return;   /* All voiceless. */
	if (f1 == f2) { f1 -= 1.0; f2 += 1.0; }
	if (s2 <= s1) Matrix_getWindowExtrema (intensity, 0, 0, 1, 1, & s1, & s2);
	if (s1 == s2) { s1 -= 1.0; s2 += 1.0; }
	Graphics_setWindow (g, f1, f2, s1, s2);
	Graphics_setInner (g);
	for (i = 1; i <= pitch -> nx; i ++) {
		double t = Sampled_indexToX (pitch, i);
		double x = pitch -> frame [i]. candidate [1]. frequency;
		double y = Sampled_getValueAtX (intensity, t, Pitch_LEVEL_FREQUENCY, kPitch_unit_HERTZ, TRUE);
		if (x == 0) {
			continue;   /* Voiceless. */
		}
		if (connect & 1) Graphics_fillCircle_mm (g, x, y, 1.0);
		if ((connect & 2) && NUMdefined (previousX)) {
			if (previousI >= 1 && previousI < i - 1) {
				Graphics_setLineType (g, Graphics_DOTTED);
			}
			Graphics_line (g, previousX, previousY, x, y);
			Graphics_setLineType (g, Graphics_DRAWN);
		}
		previousX = x;
		previousY = y;
		previousI = i;
	}
	Graphics_unsetInner (g);
	if (garnish) {
		Graphics_drawInnerBox (g);
		Graphics_textBottom (g, 1, L"Fundamental frequency (Hz)");
		Graphics_marksBottom (g, 2, 1, 1, 0);
		Graphics_textLeft (g, 1, L"Intensity (dB)");
		Graphics_marksLeft (g, 2, 1, 1, 0);
	}
}

FORM (Pitch_Intensity_draw, L"Plot intensity by pitch", 0)
	REAL (L"From frequency (Hertz)", L"0.0")
	REAL (L"To frequency (Hertz)", L"0.0 (= auto)")
	REAL (L"From intensity (dB)", L"0.0")
	REAL (L"To intensity (dB)", L"100.0")
	BOOLEAN (L"Garnish", 1)
	RADIO (L"Drawing method", 1)
	RADIOBUTTON (L"Speckles")
	RADIOBUTTON (L"Curve")
	RADIOBUTTON (L"Speckles and curve")
	OK
DO
	EVERY_DRAW (Pitch_Intensity_draw ((structPitch *)ONLY (classPitch), (structIntensity *)ONLY (classIntensity), GRAPHICS,
		GET_REAL (L"From frequency"), GET_REAL (L"To frequency"),
		GET_REAL (L"From intensity"), GET_REAL (L"To intensity"), GET_INTEGER (L"Garnish"), GET_INTEGER (L"Drawing method")))
END

FORM (Pitch_Intensity_speckle, L"Plot intensity by pitch", 0)
	REAL (L"From frequency (Hertz)", L"0.0")
	REAL (L"To frequency (Hertz)", L"0.0 (= auto)")
	REAL (L"From intensity (dB)", L"0.0")
	REAL (L"To intensity (dB)", L"100.0")
	BOOLEAN (L"Garnish", 1)
	OK
DO
	EVERY_DRAW (Pitch_Intensity_draw ((structPitch *)ONLY (classPitch), (structIntensity *)ONLY (classIntensity), GRAPHICS,
		GET_REAL (L"From frequency"), GET_REAL (L"To frequency"),
		GET_REAL (L"From intensity"), GET_REAL (L"To intensity"), GET_INTEGER (L"Garnish"), 1))
END

/***** INTENSITY & POINTPROCESS *****/

DIRECT (Intensity_PointProcess_to_IntensityTier)
	Intensity intensity = (structIntensity *)ONLY (classIntensity);
	if (! praat_new1 (Intensity_PointProcess_to_IntensityTier (intensity, (structPointProcess *)ONLY (classPointProcess)),
		intensity -> name)) return 0;
END

/***** INTENSITYTIER *****/

FORM (IntensityTier_addPoint, L"Add one point", L"IntensityTier: Add point...")
	REAL (L"Time (s)", L"0.5")
	REAL (L"Intensity (dB)", L"75")
	OK
DO
	WHERE (SELECTED) {
		if (! RealTier_addPoint (OBJECT, GET_REAL (L"Time"), GET_REAL (L"Intensity"))) return 0;
		praat_dataChanged (OBJECT);
	}
END

FORM (IntensityTier_create, L"Create empty IntensityTier", NULL)
	WORD (L"Name", L"empty")
	REAL (L"Start time (s)", L"0.0")
	REAL (L"End time (s)", L"1.0")
	OK
DO
	double startTime = GET_REAL (L"Start time"), endTime = GET_REAL (L"End time");
	REQUIRE (endTime > startTime, L"End time must be greater than start time.")
	if (! praat_new1 (IntensityTier_create (startTime, endTime), GET_STRING (L"Name"))) return 0;
END

DIRECT (IntensityTier_downto_PointProcess)
	EVERY_TO (AnyTier_downto_PointProcess (OBJECT))
END

DIRECT (IntensityTier_downto_TableOfReal)
	EVERY_TO (IntensityTier_downto_TableOfReal ((structIntensityTier *)OBJECT))
END

DIRECT (IntensityTier_edit)
	if (theCurrentPraatApplication -> batch) {
		return Melder_error1 (L"Cannot edit an IntensityTier from batch.");
	} else {
		Sound sound = NULL;
		WHERE (SELECTED)
			if (CLASS == classSound) sound = (structSound *)OBJECT;
		WHERE (SELECTED && CLASS == classIntensityTier)
			if (! praat_installEditor (new IntensityTierEditor (theCurrentPraatApplication -> topShell, ID_AND_FULL_NAME,
				(structIntensityTier *)OBJECT, sound, TRUE), IOBJECT)) return 0;
	}
END

FORM (IntensityTier_formula, L"IntensityTier: Formula", L"IntensityTier: Formula...")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"# ncol = the number of points")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"for col from 1 to ncol")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"   # x = the time of the colth point, in seconds")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"   # self = the value of the colth point, in dB")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"   self = `formula'")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"endfor")
	TEXTFIELD (L"formula", L"self + 3.0")
	OK
DO
	WHERE_DOWN (SELECTED) {
		RealTier_formula (OBJECT, GET_STRING (L"formula"), interpreter, NULL);
		praat_dataChanged (OBJECT);
		iferror return 0;
	}
END

FORM (IntensityTier_getValueAtTime, L"Get IntensityTier value", L"IntensityTier: Get value at time...")
	REAL (L"Time (s)", L"0.5")
	OK
DO
	Melder_informationReal (RealTier_getValueAtTime (ONLY_OBJECT, GET_REAL (L"Time")), L"dB");
END
	
FORM (IntensityTier_getValueAtIndex, L"Get IntensityTier value", L"IntensityTier: Get value at index...")
	INTEGER (L"Point number", L"10")
	OK
DO
	Melder_informationReal (RealTier_getValueAtIndex (ONLY_OBJECT, GET_INTEGER (L"Point number")), L"dB");
END

DIRECT (IntensityTier_help) Melder_help (L"IntensityTier"); END

DIRECT (IntensityTier_to_AmplitudeTier)
	EVERY_TO (IntensityTier_to_AmplitudeTier ((structIntensityTier *)OBJECT))
END

DIRECT (info_IntensityTier_Sound_edit)
	Melder_information1 (L"To include a copy of a Sound in your IntensityTier editor:\n"
		"   select an IntensityTier and a Sound, and click \"View & Edit\".");
END

/***** INTENSITYTIER & POINTPROCESS *****/

DIRECT (IntensityTier_PointProcess_to_IntensityTier)
	IntensityTier intensity = (structIntensityTier *)ONLY (classIntensityTier);
	if (! praat_new1 (IntensityTier_PointProcess_to_IntensityTier (intensity, (structPointProcess *)ONLY (classPointProcess)), intensity -> name)) return 0;
END

/***** INTENSITYTIER & SOUND *****/

DIRECT (Sound_IntensityTier_multiply_old)
	Sound sound = (structSound *)ONLY (classSound);
	if (! praat_new2 (Sound_IntensityTier_multiply (sound, (structIntensityTier *)ONLY (classIntensityTier), TRUE), sound -> name, L"_int")) return 0;
END

FORM (Sound_IntensityTier_multiply, L"Sound & IntervalTier: Multiply", 0)
	BOOLEAN (L"Scale to 0.9", 1)
	OK
DO
	Sound sound = (structSound *)ONLY (classSound);
	if (! praat_new2 (Sound_IntensityTier_multiply (sound, (structIntensityTier *)ONLY (classIntensityTier), GET_INTEGER (L"Scale to 0.9")), sound -> name, L"_int")) return 0;
END

/***** INTERVALTIER, rest in praat_TextGrid_init.c *****/

FORM_READ (IntervalTier_readFromXwaves, L"Read IntervalTier from Xwaves", 0, true)
	if (! praat_new1 (IntervalTier_readFromXwaves (file), MelderFile_name (file))) return 0;
END

/***** LTAS *****/
/*
	If *pxmin equals *pxmax, then autowindowing from my xmin to my xmax.
	If *pymin equals *pymax, then autoscaling from minimum to maximum;
	if minimum then equals maximum, defaultDy will be subtracted from *pymin and added to *pymax;
	it must be a positive real number (e.g. 0.5 Pa for Sound, 1.0 dB for Ltas).
	method can be "curve", "bars", "poles", or "speckles"; it must not be NULL;
	if anything else is specified, a curve is drawn.
*/
void Vector_draw (I, Graphics g, double *pxmin, double *pxmax, double *pymin, double *pymax,
	double defaultDy, const wchar_t *method)
{
	iam (Vector);
	bool xreversed = *pxmin > *pxmax, yreversed = *pymin > *pymax;
	if (xreversed) { double temp = *pxmin; *pxmin = *pxmax; *pxmax = temp; }
	if (yreversed) { double temp = *pymin; *pymin = *pymax; *pymax = temp; }
	long ixmin, ixmax, ix;
	/*
	 * Automatic domain.
	 */
	if (*pxmin == *pxmax) {
		*pxmin = my xmin;
		*pxmax = my xmax;
	}
	/*
	 * Domain expressed in sample numbers.
	 */
	Matrix_getWindowSamplesX (me, *pxmin, *pxmax, & ixmin, & ixmax);
	/*
	 * Automatic vertical range.
	 */
	if (*pymin == *pymax) {
		Matrix_getWindowExtrema (me, ixmin, ixmax, 1, 1, pymin, pymax);
		if (*pymin == *pymax) {
			*pymin -= defaultDy;
			*pymax += defaultDy;
		}
	}
	/*
	 * Set coordinates for drawing.
	 */
	Graphics_setInner (g);
	Graphics_setWindow (g, xreversed ? *pxmax : *pxmin, xreversed ? *pxmin : *pxmax, yreversed ? *pymax : *pymin, yreversed ? *pymin : *pymax);
	if (wcsstr (method, L"bars") || wcsstr (method, L"Bars")) {
		for (ix = ixmin; ix <= ixmax; ix ++) {
			double x = Sampled_indexToX (me, ix);
			double y = my z [1] [ix];
			double left = x - 0.5 * my dx, right = x + 0.5 * my dx;
			if (y > *pymax) y = *pymax;
			if (left < *pxmin) left = *pxmin;
			if (right > *pxmax) right = *pxmax;
			Graphics_line (g, left, y, right, y);
			Graphics_line (g, left, y, left, *pymin);
			Graphics_line (g, right, y, right, *pymin);
		}
	} else if (wcsstr (method, L"poles") || wcsstr (method, L"Poles")) {
		for (ix = ixmin; ix <= ixmax; ix ++) {
			double x = Sampled_indexToX (me, ix);
			Graphics_line (g, x, 0, x, my z [1] [ix]);
		}
	} else if (wcsstr (method, L"speckles") || wcsstr (method, L"Speckles")) {
		for (ix = ixmin; ix <= ixmax; ix ++) {
			double x = Sampled_indexToX (me, ix);
			Graphics_fillCircle_mm (g, x, my z [1] [ix], 1.0);
		}
	} else {
		/*
		 * The default: draw as a curve.
		 */
		Graphics_function (g, my z [1], ixmin, ixmax,
			Matrix_columnToX (me, ixmin), Matrix_columnToX (me, ixmax));
	}
	Graphics_unsetInner (g);
}

void Ltas_draw (Ltas me, Graphics g, double fmin, double fmax, double minimum, double maximum, int garnish, const wchar_t *method) {
	Vector_draw (me, g, & fmin, & fmax, & minimum, & maximum, 1.0, method);
	if (garnish) {
		Graphics_drawInnerBox (g);
		Graphics_textBottom (g, 1, L"Frequency (Hz)");
		Graphics_marksBottom (g, 2, 1, 1, 0);
		Graphics_textLeft (g, 1, L"Sound pressure level (dB/Hz)");
		Graphics_marksLeft (g, 2, 1, 1, 0);
	}
}

DIRECT (Ltases_average)
	Collection ltases;
	int n = 0;
	WHERE (SELECTED) n ++;
	ltases = Collection_create (classLtas, n);
	WHERE (SELECTED) Collection_addItem (ltases, OBJECT);
	if (! praat_new1 (Ltases_average (ltases), L"averaged")) {
		ltases -> size = 0;   /* Undangle. */
		forget (ltases);
		return 0;
	}
	ltases -> size = 0;   /* Undangle. */
	forget (ltases);
END

FORM (Ltas_computeTrendLine, L"Ltas: Compute trend line", L"Ltas: Compute trend line...")
	REAL (L"left Frequency range (Hz)", L"600.0")
	POSITIVE (L"right Frequency range (Hz)", L"4000.0")
	OK
DO
	WHERE (SELECTED) {
		if (! praat_new2 (Ltas_computeTrendLine ((structLtas *)OBJECT, GET_REAL (L"left Frequency range"), GET_REAL (L"right Frequency range")),
			NAME, L"_trend")) return 0;
	}
END

FORM (old_Ltas_draw, L"Ltas: Draw", 0)
	REAL (L"left Frequency range (Hz)", L"0.0")
	REAL (L"right Frequency range (Hz)", L"0.0 (= all)")
	REAL (L"left Power range (dB/Hz)", L"-20.0")
	REAL (L"right Power range (dB/Hz)", L"80.0")
	BOOLEAN (L"Garnish", 1)
	OK
DO
	EVERY_DRAW (Ltas_draw ((structLtas *)OBJECT, GRAPHICS, GET_REAL (L"left Frequency range"), GET_REAL (L"right Frequency range"),
		GET_REAL (L"left Power range"), GET_REAL (L"right Power range"), GET_INTEGER (L"Garnish"), L"Bars"))
END

FORM (Ltas_draw, L"Ltas: Draw", 0)
	REAL (L"left Frequency range (Hz)", L"0.0")
	REAL (L"right Frequency range (Hz)", L"0.0 (= all)")
	REAL (L"left Power range (dB/Hz)", L"-20.0")
	REAL (L"right Power range (dB/Hz)", L"80.0")
	BOOLEAN (L"Garnish", 1)
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"ui/editors/AmplitudeTierEditor.h")
	OPTIONMENU (L"Drawing method", 2)
		OPTION (L"Curve")
		OPTION (L"Bars")
		OPTION (L"Poles")
		OPTION (L"Speckles")
	OK
DO_ALTERNATIVE (old_Ltas_draw)
	EVERY_DRAW (Ltas_draw ((structLtas *)OBJECT, GRAPHICS, GET_REAL (L"left Frequency range"), GET_REAL (L"right Frequency range"),
		GET_REAL (L"left Power range"), GET_REAL (L"right Power range"), GET_INTEGER (L"Garnish"), GET_STRING (L"Drawing method")))
END

FORM (Ltas_formula, L"Ltas Formula", 0)
	LABEL (L"label", L"`x' is the frequency in Hertz, `col' is the bin number")
	LABEL (L"label", L"x := x1;   for col := 1 to ncol do { self [1, col] := `formula' ; x := x + dx }")
	TEXTFIELD (L"formula", L"0")
	OK
DO
	if (! praat_Fon_formula (dia, interpreter)) return 0;
END

FORM (Ltas_getBinNumberFromFrequency, L"Ltas: Get band from frequency", L"Ltas: Get band from frequency...")
	REAL (L"Frequency (Hz)", L"2000")
	OK
DO
	Melder_informationReal (Sampled_xToIndex (ONLY (classLtas), GET_REAL (L"Frequency")), NULL);
END

DIRECT (Ltas_getBinWidth)
	Ltas me = (structLtas *)ONLY (classLtas);
	Melder_informationReal (my dx, L"Hertz");
END

FORM (Ltas_getFrequencyFromBinNumber, L"Ltas: Get frequency from bin number", L"Ltas: Get frequency from bin number...")
	NATURAL (L"Bin number", L"1")
	OK
DO
	Melder_informationReal (Sampled_indexToX (ONLY (classLtas), GET_INTEGER (L"Bin number")), L"Hertz");
END

FORM (Ltas_getFrequencyOfMaximum, L"Ltas: Get frequency of maximum", L"Ltas: Get frequency of maximum...")
	REAL (L"From frequency (Hz)", L"0.0")
	REAL (L"To frequency (Hz)", L"0.0 (= all)")
	RADIO (L"Interpolation", 1)
	RADIOBUTTON (L"None")
	RADIOBUTTON (L"Parabolic")
	RADIOBUTTON (L"Cubic")
	RADIOBUTTON (L"Sinc70")
	RADIOBUTTON (L"Sinc700")
	OK
DO
	Melder_informationReal (Vector_getXOfMaximum (ONLY (classLtas),
		GET_REAL (L"From frequency"), GET_REAL (L"To frequency"), GET_INTEGER (L"Interpolation") - 1), L"Hertz");
END

FORM (Ltas_getFrequencyOfMinimum, L"Ltas: Get frequency of minimum", L"Ltas: Get frequency of minimum...")
	REAL (L"From frequency (s)", L"0.0")
	REAL (L"To frequency (s)", L"0.0 (= all)")
	RADIO (L"Interpolation", 1)
	RADIOBUTTON (L"None")
	RADIOBUTTON (L"Parabolic")
	RADIOBUTTON (L"Cubic")
	RADIOBUTTON (L"Sinc70")
	RADIOBUTTON (L"Sinc700")
	OK
DO
	Melder_informationReal (Vector_getXOfMinimum (ONLY (classLtas),
		GET_REAL (L"From frequency"), GET_REAL (L"To frequency"), GET_INTEGER (L"Interpolation") - 1), L"Hertz");
END

DIRECT (Ltas_getHighestFrequency)
	Ltas me = (structLtas *)ONLY (classLtas);
	Melder_informationReal (my xmax, L"Hertz");
END

FORM (Ltas_getLocalPeakHeight, L"Ltas: Get local peak height", 0)
	REAL (L"left Environment (Hz)", L"1700.0")
	REAL (L"right Environment (Hz)", L"4200.0")
	REAL (L"left Peak (Hz)", L"2400.0")
	REAL (L"right Peak (Hz)", L"3200.0")
	RADIO (L"Averaging method", 1)
		RADIOBUTTON (L"energy")
		RADIOBUTTON (L"sones")
		RADIOBUTTON (L"dB")
	OK
DO
	double environmentMin = GET_REAL (L"left Environment"), environmentMax = GET_REAL (L"right Environment");
	double peakMin = GET_REAL (L"left Peak"), peakMax = GET_REAL (L"right Peak");
	REQUIRE (environmentMin < peakMin, L"The beginning of the environment must lie before the peak.")
	REQUIRE (peakMin < peakMax, L"The end of the peak must lie after its beginning.")
	REQUIRE (environmentMax > peakMax, L"The end of the environment must lie after the peak.")
	Melder_informationReal (Ltas_getLocalPeakHeight ((structLtas *)ONLY (classLtas), environmentMin, environmentMax,
		peakMin, peakMax, GET_INTEGER (L"Averaging method")), L"dB");
END

DIRECT (Ltas_getLowestFrequency)
	Ltas me = (structLtas *)ONLY (classLtas);
	Melder_informationReal (my xmin, L"Hertz");
END

FORM (Ltas_getMaximum, L"Ltas: Get maximum", L"Ltas: Get maximum...")
	REAL (L"From frequency (Hz)", L"0.0")
	REAL (L"To frequency (Hz)", L"0.0 (= all)")
	RADIO (L"Interpolation", 1)
	RADIOBUTTON (L"None")
	RADIOBUTTON (L"Parabolic")
	RADIOBUTTON (L"Cubic")
	RADIOBUTTON (L"Sinc70")
	RADIOBUTTON (L"Sinc700")
	OK
DO
	Melder_informationReal (Vector_getMaximum (ONLY (classLtas),
		GET_REAL (L"From frequency"), GET_REAL (L"To frequency"), GET_INTEGER (L"Interpolation") - 1), L"dB");
END

FORM (Ltas_getMean, L"Ltas: Get mean", L"Ltas: Get mean...")
	REAL (L"From frequency (Hz)", L"0.0")
	REAL (L"To frequency (Hz)", L"0.0 (= all)")
	RADIO (L"Averaging method", 1)
		RADIOBUTTON (L"energy")
		RADIOBUTTON (L"sones")
		RADIOBUTTON (L"dB")
	OK
DO
	Melder_informationReal (Sampled_getMean_standardUnit (ONLY (classLtas), GET_REAL (L"From frequency"), GET_REAL (L"To frequency"),
		0, GET_INTEGER (L"Averaging method"), FALSE), L"dB");
END

FORM (Ltas_getMinimum, L"Ltas: Get minimum", L"Ltas: Get minimum...")
	REAL (L"From frequency (Hz)", L"0.0")
	REAL (L"To frequency (Hz)", L"0.0 (= all)")
	RADIO (L"Interpolation", 1)
	RADIOBUTTON (L"None")
	RADIOBUTTON (L"Parabolic")
	RADIOBUTTON (L"Cubic")
	RADIOBUTTON (L"Sinc70")
	RADIOBUTTON (L"Sinc700")
	OK
DO
	Melder_informationReal (Vector_getMinimum (ONLY (classLtas),
		GET_REAL (L"From frequency"), GET_REAL (L"To frequency"), GET_INTEGER (L"Interpolation") - 1), L"dB");
END

DIRECT (Ltas_getNumberOfBins)
	Ltas me = (structLtas *)ONLY (classLtas);
	Melder_information2 (Melder_integer (my nx), L" bins");
END

FORM (Ltas_getSlope, L"Ltas: Get slope", 0)
	REAL (L"left Low band (Hz)", L"0.0")
	REAL (L"right Low band (Hz)", L"1000.0")
	REAL (L"left High band (Hz)", L"1000.0")
	REAL (L"right High band (Hz)", L"4000.0")
	RADIO (L"Averaging method", 1)
		RADIOBUTTON (L"energy")
		RADIOBUTTON (L"sones")
		RADIOBUTTON (L"dB")
	OK
DO
	Melder_informationReal (Ltas_getSlope ((structLtas *)ONLY (classLtas), GET_REAL (L"left Low band"), GET_REAL (L"right Low band"),
		GET_REAL (L"left High band"), GET_REAL (L"right High band"), GET_INTEGER (L"Averaging method")), L"dB");
END

FORM (Ltas_getStandardDeviation, L"Ltas: Get standard deviation", L"Ltas: Get standard deviation...")
	REAL (L"From frequency (Hz)", L"0.0")
	REAL (L"To frequency (Hz)", L"0.0 (= all)")
	RADIO (L"Averaging method", 1)
		RADIOBUTTON (L"energy")
		RADIOBUTTON (L"sones")
		RADIOBUTTON (L"dB")
	OK
DO
	Melder_informationReal (Sampled_getStandardDeviation_standardUnit (ONLY (classLtas), GET_REAL (L"From frequency"), GET_REAL (L"To frequency"),
		0, GET_INTEGER (L"Averaging method"), FALSE), L"dB");
END

FORM (Ltas_getValueAtFrequency, L"Ltas: Get value", L"Ltas: Get value at frequency...")
	REAL (L"Frequency (Hertz)", L"1500")
	RADIO (L"Interpolation", 1)
	RADIOBUTTON (L"Nearest")
	RADIOBUTTON (L"Linear")
	RADIOBUTTON (L"Cubic")
	RADIOBUTTON (L"Sinc70")
	RADIOBUTTON (L"Sinc700")
	OK
DO
	Melder_informationReal (Vector_getValueAtX (ONLY (classLtas), GET_REAL (L"Frequency"), 1, GET_INTEGER (L"Interpolation") - 1), L"dB");
END
	
FORM (Ltas_getValueInBin, L"Get value in bin", L"Ltas: Get value in bin...")
	INTEGER (L"Bin number", L"100")
	OK
DO
	Ltas me = (structLtas *)ONLY (classLtas);
	long binNumber = GET_INTEGER (L"Bin number");
	Melder_informationReal (binNumber < 1 || binNumber > my nx ? NUMundefined : my z [1] [binNumber], L"dB");
END

DIRECT (Ltas_help) Melder_help (L"Ltas"); END

DIRECT (Ltases_merge)
	Collection ltases;
	int n = 0;
	WHERE (SELECTED) n ++;
	ltases = Collection_create (classLtas, n);
	WHERE (SELECTED) Collection_addItem (ltases, OBJECT);
	if (! praat_new1 (Ltases_merge (ltases), L"merged")) {
		ltases -> size = 0;   /* Undangle. */
		forget (ltases);
		return 0;
	}
	ltases -> size = 0;   /* Undangle. */
	forget (ltases);
END

FORM (Ltas_subtractTrendLine, L"Ltas: Subtract trend line", L"Ltas: Subtract trend line...")
	REAL (L"left Frequency range (Hertz)", L"600.0")
	POSITIVE (L"right Frequency range (Hertz)", L"4000.0")
	OK
DO
	WHERE (SELECTED) {
		if (! praat_new2 (Ltas_subtractTrendLine ((structLtas *)OBJECT, GET_REAL (L"left Frequency range"), GET_REAL (L"right Frequency range")),
			NAME, L"_fit")) return 0;
	}
END

DIRECT (Ltas_to_Matrix)
	EVERY_TO (Ltas_to_Matrix ((structLtas *)OBJECT))
END

DIRECT (Ltas_to_SpectrumTier_peaks)
	EVERY_TO (Ltas_to_SpectrumTier_peaks ((structLtas *)OBJECT))
END

/***** MANIPULATION *****/

static void cb_ManipulationEditor_publish (Editor *editor, void *closure, Any publish) {
	(void) editor;
	(void) closure;
	if (! praat_new1 (publish, L"fromManipulationEditor")) { Melder_flushError (NULL); return; }
	praat_updateSelection ();
}
DIRECT (Manipulation_edit)
	if (theCurrentPraatApplication -> batch) {
		return Melder_error1 (L"Cannot edit a Manipulation from batch.");
	} else {
		WHERE (SELECTED) {
			ManipulationEditor *editor = new ManipulationEditor (theCurrentPraatApplication -> topShell, ID_AND_FULL_NAME, (structManipulation *)OBJECT);
			if (! praat_installEditor (editor, IOBJECT)) return 0;
			editor->setPublishCallback (cb_ManipulationEditor_publish, NULL);
		}
	}
END

DIRECT (Manipulation_extractDurationTier)
	WHERE (SELECTED) {
		Manipulation ana = (structManipulation *)OBJECT;
		if (ana -> duration) {
			if (! praat_new1 (Data_copy (ana -> duration), NULL)) return 0;
		} else {
			return Melder_error1 (L"Manipulation does not contain a DurationTier.");
		}
	}
END

DIRECT (Manipulation_extractOriginalSound)
	WHERE (SELECTED) {
		Manipulation ana = (structManipulation *)OBJECT;
		if (ana -> sound) {
			if (! praat_new1 (Data_copy (ana -> sound), NULL)) return 0;
		} else {
			return Melder_error1 (L"Manipulation does not contain a Sound.");
		}
	}
END

DIRECT (Manipulation_extractPitchTier)
	WHERE (SELECTED) {
		Manipulation ana = (structManipulation *)OBJECT;
		if (ana -> pitch) {
			if (! praat_new1 (Data_copy (ana -> pitch), NULL)) return 0;
		} else {
			return Melder_error1 (L"Manipulation does not contain a PitchTier.");
		}
	}
END

DIRECT (Manipulation_extractPulses)
	WHERE (SELECTED) {
		Manipulation ana = (structManipulation *)OBJECT;
		if (ana -> pulses) {
			if (! praat_new1 (Data_copy (ana -> pulses), NULL)) return 0;
		} else {
			return Melder_error1 (L"Manipulation does not contain a PointProcess.");
		}
	}
END

DIRECT (Manipulation_getResynthesis_lpc)
	EVERY_TO (Manipulation_to_Sound ((structManipulation *)OBJECT, Manipulation_PITCH_LPC))
END

DIRECT (Manipulation_getResynthesis_overlapAdd)
	EVERY_TO (Manipulation_to_Sound ((structManipulation *)OBJECT, Manipulation_OVERLAPADD))
END

DIRECT (Manipulation_help) Melder_help (L"Manipulation"); END

DIRECT (Manipulation_play_lpc)
	EVERY_CHECK (Manipulation_play ((structManipulation *)OBJECT, Manipulation_PITCH_LPC))
END

DIRECT (Manipulation_play_overlapAdd)
	EVERY_CHECK (Manipulation_play ((structManipulation *)OBJECT, Manipulation_OVERLAPADD))
END

DIRECT (Manipulation_removeDuration)
	WHERE (SELECTED) {
		Manipulation ana = (structManipulation *)OBJECT;
		forget (ana -> duration);
		praat_dataChanged (ana);
	}
END

DIRECT (Manipulation_removeOriginalSound)
	WHERE (SELECTED) {
		Manipulation ana = (structManipulation *)OBJECT;
		forget (ana -> sound);
		praat_dataChanged (ana);
	}
END

FORM_WRITE (Manipulation_writeToBinaryFileWithoutSound, L"Binary file without Sound", 0, 0)
	if (! Manipulation_writeToBinaryFileWithoutSound ((structManipulation *)ONLY_OBJECT, file)) return 0;
END

FORM_WRITE (Manipulation_writeToTextFileWithoutSound, L"Text file without Sound", 0, 0)
	if (! Manipulation_writeToTextFileWithoutSound ((structManipulation *)ONLY_OBJECT, file)) return 0;
END

DIRECT (info_DurationTier_Manipulation_replace)
	Melder_information1 (L"To replace the DurationTier in a Manipulation object,\n"
		"select a DurationTier object and a Manipulation object\nand choose \"Replace duration\".");
END

DIRECT (info_PitchTier_Manipulation_replace)
	Melder_information1 (L"To replace the PitchTier in a Manipulation object,\n"
		"select a PitchTier object and a Manipulation object\nand choose \"Replace pitch\".");
END

/***** MANIPULATION & DURATIONTIER *****/

DIRECT (Manipulation_replaceDurationTier)
	Manipulation ana = (structManipulation *)ONLY (classManipulation);
	if (! Manipulation_replaceDurationTier (ana, (structDurationTier *)ONLY (classDurationTier))) return 0;
	praat_dataChanged (ana);
END

DIRECT (Manipulation_replaceDurationTier_help) Melder_help (L"Manipulation: Replace duration tier"); END

/***** MANIPULATION & PITCHTIER *****/

DIRECT (Manipulation_replacePitchTier)
	Manipulation ana = (structManipulation *)ONLY (classManipulation);
	if (! Manipulation_replacePitchTier (ana, (structPitchTier *)ONLY (classPitchTier))) return 0;
	praat_dataChanged (ana);
END

DIRECT (Manipulation_replacePitchTier_help) Melder_help (L"Manipulation: Replace pitch tier"); END

/***** MANIPULATION & POINTPROCESS *****/

DIRECT (Manipulation_replacePulses)
	Manipulation ana = (structManipulation *)ONLY (classManipulation);
	if (! Manipulation_replacePulses (ana, (structPointProcess *)ONLY (classPointProcess))) return 0;
	praat_dataChanged (ana);
END

/***** MANIPULATION & SOUND *****/

DIRECT (Manipulation_replaceOriginalSound)
	Manipulation ana = (structManipulation *)ONLY (classManipulation);
	if (! Manipulation_replaceOriginalSound (ana, (structSound *)ONLY (classSound))) return 0;
	praat_dataChanged (ana);
END

/***** MANIPULATION & TEXTTIER *****/

DIRECT (Manipulation_TextTier_to_Manipulation)
	if (! praat_new1 (Manipulation_AnyTier_to_Manipulation ((structManipulation *)ONLY (classManipulation), (structAnyTier *)ONLY (classTextTier)),
		((Manipulation) (ONLY (classManipulation))) -> name)) return 0;	
END

/***** MATRIX *****/

/*
	All of these routines show the samples of a Matrix whose x and y values
	are inside the window [xmin, xmax] * [ymin, ymax].
	The scaling of the values of these samples is determined by "minimum" and "maximum".
	All of these routines can perform automatic windowing and scaling:
	if xmax <= xmin, the window in the x direction will be set to [my xmin, my xmax];
	if ymax <= ymin, the window in the y direction will be set to [my ymin, my ymax];
	if maximum <= minimum, the windowing (scaling) in the z direction will be determined
	by the minimum and maximum values of the samples inside the window.
*/

/*
	Every row is plotted as a function of x,
	with straight lines connecting the sample points.
	The rows are stacked from bottom to top.
*/
void Matrix_drawRows (I, Graphics g, double xmin, double xmax, double ymin, double ymax,
	double minimum, double maximum)
{
	iam (Matrix);
	long ixmin, ixmax, iymin, iymax, iy;
	if (xmax <= xmin) { xmin = my xmin; xmax = my xmax; }
	if (ymax <= ymin) { ymin = my ymin; ymax = my ymax; }
	(void) Matrix_getWindowSamplesX (me, xmin, xmax, & ixmin, & ixmax);
	(void) Matrix_getWindowSamplesY (me, ymin, ymax, & iymin, & iymax);
	if (maximum <= minimum)
		(void) Matrix_getWindowExtrema (me, ixmin, ixmax, iymin, iymax, & minimum, & maximum);
	if (maximum <= minimum) { minimum -= 1.0; maximum += 1.0; }
	if (xmin >= xmax) return;
	Graphics_setInner (g);
	for (iy = iymin; iy <= iymax; iy ++)
	{
		Graphics_setWindow (g, xmin, xmax,
			minimum - (iy - iymin) * (maximum - minimum),
			maximum + (iymax - iy) * (maximum - minimum));
		Graphics_function (g, my z [iy], ixmin, ixmax,
					Matrix_columnToX (me, ixmin), Matrix_columnToX (me, ixmax));
	}
	Graphics_unsetInner (g);
	if (iymin < iymax)
		Graphics_setWindow (g, xmin, xmax, my y1 + (iymin - 1.5) * my dy, my y1 + (iymax - 0.5) * my dy);
}

void Matrix_drawOneContour (I, Graphics g, double xmin, double xmax, double ymin, double ymax,
	double height)
{
	iam (Matrix);
	bool xreversed = xmin > xmax, yreversed = ymin > ymax;
	if (xmax == xmin) { xmin = my xmin; xmax = my xmax; }
	if (ymax == ymin) { ymin = my ymin; ymax = my ymax; }
	if (xreversed) { double temp = xmin; xmin = xmax; xmax = temp; }
	if (yreversed) { double temp = ymin; ymin = ymax; ymax = temp; }
	long ixmin, ixmax, iymin, iymax;
	(void) Matrix_getWindowSamplesX (me, xmin, xmax, & ixmin, & ixmax);
	(void) Matrix_getWindowSamplesY (me, ymin, ymax, & iymin, & iymax);
	if (xmin == xmax || ymin == ymax) return;
	Graphics_setInner (g);
	Graphics_setWindow (g, xreversed ? xmax : xmin, xreversed ? xmin : xmax, yreversed ? ymax : ymin, yreversed ? ymin : ymax);
	Graphics_contour (g, my z,
		ixmin, ixmax, Matrix_columnToX (me, ixmin), Matrix_columnToX (me, ixmax),
		iymin, iymax, Matrix_rowToY (me, iymin), Matrix_rowToY (me, iymax),
		height);
	Graphics_rectangle (g, xmin, xmax, ymin, ymax);
	Graphics_unsetInner (g);
}

/* A contour altitude plot with curves at multiple heights. */
void Matrix_drawContours (I, Graphics g, double xmin, double xmax, double ymin, double ymax,
	double minimum, double maximum)
{
	iam (Matrix);
	double border [1 + 8];
	long ixmin, ixmax, iymin, iymax, iborder;
	if (xmax == xmin) { xmin = my xmin; xmax = my xmax; }
	if (ymax == ymin) { ymin = my ymin; ymax = my ymax; }
	(void) Matrix_getWindowSamplesX (me, xmin, xmax, & ixmin, & ixmax);
	(void) Matrix_getWindowSamplesY (me, ymin, ymax, & iymin, & iymax);
	if (maximum <= minimum)
		(void) Matrix_getWindowExtrema (me, ixmin, ixmax, iymin, iymax, & minimum, & maximum);
	if (maximum <= minimum) { minimum -= 1.0; maximum += 1.0; }
	for (iborder = 1; iborder <= 8; iborder ++)
		border [iborder] = minimum + iborder * (maximum - minimum) / (8 + 1);
	if (xmin == xmax || ymin == ymax) return;
	Graphics_setInner (g);
	Graphics_setWindow (g, xmin, xmax, ymin, ymax);
	Graphics_altitude (g, my z,
		ixmin, ixmax, Matrix_columnToX (me, ixmin), Matrix_columnToX (me, ixmax),
		iymin, iymax, Matrix_rowToY (me, iymin), Matrix_rowToY (me, iymax),
		8, border);
	Graphics_rectangle (g, xmin, xmax, ymin, ymax);
	Graphics_unsetInner (g);
}

/* A contour plot with multiple shades of grey and white (low) and black (high) paint. */
void Matrix_paintContours (I, Graphics g, double xmin, double xmax, double ymin, double ymax,
	double minimum, double maximum)
{
	iam (Matrix);
	double border [1 + 30];
	long ixmin, ixmax, iymin, iymax, iborder;
	if (xmax <= xmin) { xmin = my xmin; xmax = my xmax; }
	if (ymax <= ymin) { ymin = my ymin; ymax = my ymax; }
	(void) Matrix_getWindowSamplesX (me, xmin, xmax, & ixmin, & ixmax);
	(void) Matrix_getWindowSamplesY (me, ymin, ymax, & iymin, & iymax);
	if (maximum <= minimum)
		(void) Matrix_getWindowExtrema (me, ixmin, ixmax, iymin, iymax, & minimum, & maximum);
	if (maximum <= minimum) { minimum -= 1.0; maximum += 1.0; }
	for (iborder = 1; iborder <= 30; iborder ++)
		border [iborder] = minimum + iborder * (maximum - minimum) / (30 + 1);
	if (xmin >= xmax || ymin >= ymax) return;
	Graphics_setInner (g);
	Graphics_setWindow (g, xmin, xmax, ymin, ymax);
	Graphics_grey (g, my z,
		ixmin, ixmax, Matrix_columnToX (me, ixmin), Matrix_columnToX (me, ixmax),
		iymin, iymax, Matrix_rowToY (me, iymin), Matrix_rowToY (me, iymax),
		30, border);
	Graphics_rectangle (g, xmin, xmax, ymin, ymax);
	Graphics_unsetInner (g);
}

static void cellArrayOrImage (I, Graphics g, double xmin, double xmax, double ymin, double ymax,
	double minimum, double maximum, int interpolate)
{
	iam (Matrix);
	long ixmin, ixmax, iymin, iymax;
	if (xmax <= xmin) { xmin = my xmin; xmax = my xmax; }
	if (ymax <= ymin) { ymin = my ymin; ymax = my ymax; }
	(void) Matrix_getWindowSamplesX (me, xmin - 0.49999 * my dx, xmax + 0.49999 * my dx,
		& ixmin, & ixmax);
	(void) Matrix_getWindowSamplesY (me, ymin - 0.49999 * my dy, ymax + 0.49999 * my dy,
		& iymin, & iymax);
	if (maximum <= minimum)
		(void) Matrix_getWindowExtrema (me, ixmin, ixmax, iymin, iymax, & minimum, & maximum);
	if (maximum <= minimum) { minimum -= 1.0; maximum += 1.0; }
	if (xmin >= xmax || ymin >= ymax) return;
	Graphics_setInner (g);
	Graphics_setWindow (g, xmin, xmax, ymin, ymax);
	if (interpolate)
		Graphics_image (g, my z,
			ixmin, ixmax, Matrix_columnToX (me, ixmin - 0.5), Matrix_columnToX (me, ixmax + 0.5),
			iymin, iymax, Matrix_rowToY (me, iymin - 0.5), Matrix_rowToY (me, iymax + 0.5),
			minimum, maximum);
	else
		Graphics_cellArray (g, my z,
			ixmin, ixmax, Matrix_columnToX (me, ixmin - 0.5), Matrix_columnToX (me, ixmax + 0.5),
			iymin, iymax, Matrix_rowToY (me, iymin - 0.5), Matrix_rowToY (me, iymax + 0.5),
			minimum, maximum);
	Graphics_rectangle (g, xmin, xmax, ymin, ymax);
	Graphics_unsetInner (g);
}

/*
	Two-dimensional interpolation of greys.
	The larger the value of the sample, the darker the greys.
*/
void Matrix_paintImage (I, Graphics g, double xmin, double xmax, double ymin, double ymax,
	double minimum, double maximum)
{
	cellArrayOrImage (void_me, g, xmin, xmax, ymin, ymax, minimum, maximum, TRUE);
}

/*
	Every sample is drawn as a grey rectangle.
	The larger the value of the sample, the darker the rectangle.
*/
void Matrix_paintCells (I, Graphics g, double xmin, double xmax, double ymin, double ymax,
	double minimum, double maximum)
{
	cellArrayOrImage (void_me, g, xmin, xmax, ymin, ymax, minimum, maximum, FALSE);
}

/*
	3D surface plot. Every space between adjacent four samples is drawn as a tetragon filled with a grey value.
	'elevation' may be 30 degrees, 'azimuth' may be 45 degrees.
*/
void Matrix_paintSurface (I, Graphics g, double xmin, double xmax, double ymin, double ymax,
	double minimum, double maximum, double elevation, double azimuth)
{
	iam (Matrix);
	long ixmin, ixmax, iymin, iymax;
	if (xmax <= xmin) { xmin = my xmin; xmax = my xmax; }
	if (ymax <= ymin) { ymin = my ymin; ymax = my ymax; }
	(void) Matrix_getWindowSamplesX (me, xmin, xmax, & ixmin, & ixmax);
	(void) Matrix_getWindowSamplesY (me, ymin, ymax, & iymin, & iymax);
	if (maximum <= minimum)
		(void) Matrix_getWindowExtrema (me, ixmin, ixmax, iymin, iymax, & minimum, & maximum);
	if (maximum <= minimum) { minimum -= 1.0; maximum += 1.0; }
	Graphics_setInner (g);
	Graphics_setWindow (g, -1, 1, minimum, maximum);
	Graphics_surface (g, my z,
		ixmin, ixmax, Matrix_columnToX (me, ixmin), Matrix_columnToX (me, ixmax),
		iymin, iymax, Matrix_rowToY (me, iymin), Matrix_rowToY (me, iymax),
		minimum, maximum, elevation, azimuth);
	Graphics_unsetInner (g);
}

extern int sginap (long);
void Matrix_movie (I, Graphics g) {
	iam (Matrix);
	double minimum = 0, maximum = 1;
	double *column = NUMdvector (1, my ny);
	Matrix_getWindowExtrema (me, 1, my nx, 1, my ny, & minimum, & maximum);
	Graphics_setViewport (g, 0, 1, 0, 1);
	Graphics_setWindow (g, my ymin, my ymax, minimum, maximum);
	for (long icol = 1; icol <= my nx; icol ++) {
		long irow;
		for (irow = 1; irow <= my ny; irow ++)
			column [irow] = my z [irow] [icol];
		Graphics_clearWs (g);
		Graphics_function (g, column, 1, my ny, my ymin, my ymax);
		Graphics_flushWs (g);
		#ifdef sgi
			sginap (2);
		#endif
	}
	NUMdvector_free (column, 1);
}

DIRECT (Matrix_appendRows)
	Matrix m1 = NULL, m2 = NULL;
	WHERE (SELECTED) { if (m1) m2 = (structMatrix *)OBJECT; else m1 = (structMatrix *)OBJECT; }
	if (! praat_new3 (Matrix_appendRows (m1, m2), m1 -> name, L"_", m2 -> name)) return 0;
END

FORM (Matrix_create, L"Create Matrix", L"Create Matrix...")
	WORD (L"Name", L"xy")
	REAL (L"xmin", L"1.0")
	REAL (L"xmax", L"1.0")
	NATURAL (L"Number of columns", L"1")
	POSITIVE (L"dx", L"1.0")
	REAL (L"x1", L"1.0")
	REAL (L"ymin", L"1.0")
	REAL (L"ymax", L"1.0")
	NATURAL (L"Number of rows", L"1")
	POSITIVE (L"dy", L"1.0")
	REAL (L"y1", L"1.0")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"Formula:")
	TEXTFIELD (L"formula", L"x*y")
	OK
DO
	double xmin = GET_REAL (L"xmin"), xmax = GET_REAL (L"xmax");
	double ymin = GET_REAL (L"ymin"), ymax = GET_REAL (L"ymax");
	if (xmax < xmin)
		return Melder_error5 (L"xmax (", Melder_single (xmax), L") should not be less than xmin (", Melder_single (xmin), L").");
	if (ymax < ymin)
		return Melder_error5 (L"ymax (", Melder_single (ymax), L") should not be less than ymin (", Melder_single (ymin), L").");
	if (! praat_new1 (Matrix_create (
		xmin, xmax, GET_INTEGER (L"Number of columns"), GET_REAL (L"dx"), GET_REAL (L"x1"),
		ymin, ymax, GET_INTEGER (L"Number of rows"), GET_REAL (L"dy"), GET_REAL (L"y1")),
		GET_STRING (L"Name"))) return 0;
	praat_updateSelection ();
	return praat_Fon_formula (dia, interpreter);
END

FORM (Matrix_createSimple, L"Create simple Matrix", L"Create simple Matrix...")
	WORD (L"Name", L"xy")
	NATURAL (L"Number of rows", L"10")
	NATURAL (L"Number of columns", L"10")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"Formula:")
	TEXTFIELD (L"formula", L"x*y")
	OK
DO
	if (! praat_new1 (Matrix_createSimple (
		GET_INTEGER (L"Number of rows"), GET_INTEGER (L"Number of columns")),
		GET_STRING (L"Name"))) return 0;
	praat_updateSelection ();
	return praat_Fon_formula (dia, interpreter);
END

FORM (Matrix_drawOneContour, L"Draw one altitude contour", 0)
	REAL (L"From x =", L"0.0")
	REAL (L"To x =", L"0.0")
	REAL (L"From y =", L"0.0")
	REAL (L"To y =", L"0.0")
	REAL (L"Height", L"0.5")
	OK
DO
	EVERY_DRAW (Matrix_drawOneContour (OBJECT, GRAPHICS,
		GET_REAL (L"From x ="), GET_REAL (L"To x ="), GET_REAL (L"From y ="), GET_REAL (L"To y ="),
		GET_REAL (L"Height")))
END

FORM (Matrix_drawContours, L"Draw altitude contours", 0)
	REAL (L"From x =", L"0.0")
	REAL (L"To x =", L"0.0")
	REAL (L"From y =", L"0.0")
	REAL (L"To y =", L"0.0")
	REAL (L"Minimum", L"0.0")
	REAL (L"Maximum", L"0.0")
	OK
DO
	EVERY_DRAW (Matrix_drawContours (OBJECT, GRAPHICS,
		GET_REAL (L"From x ="), GET_REAL (L"To x ="), GET_REAL (L"From y ="), GET_REAL (L"To y ="),
		GET_REAL (L"Minimum"), GET_REAL (L"Maximum")))
END

FORM (Matrix_drawRows, L"Draw rows", 0)
	REAL (L"From x =", L"0.0")
	REAL (L"To x =", L"0.0")
	REAL (L"From y =", L"0.0")
	REAL (L"To y =", L"0.0")
	REAL (L"Minimum", L"0.0")
	REAL (L"Maximum", L"0.0")
	OK
DO
	EVERY_DRAW (Matrix_drawRows (OBJECT, GRAPHICS,
		GET_REAL (L"From x ="), GET_REAL (L"To x ="),
		GET_REAL (L"From y ="), GET_REAL (L"To y ="),
		GET_REAL (L"Minimum"), GET_REAL (L"Maximum")))
END

DIRECT (Matrix_eigen)
	WHERE (SELECTED) {
		Matrix vec, val;
		if (! Matrix_eigen (OBJECT, & vec, & val)) return 0;
		if (! praat_new1 (vec, L"eigenvectors")) return 0;
		if (! praat_new1 (val, L"eigenvalues")) return 0;
	}
END

/*
	Arguments:
		"me" is the Matrix referred to as "self" or with "nx" etc. in the expression
		"target" is the Matrix whose elements will change according to:
			FOR row FROM 1 TO my ny
				FOR col FROM 1 TO my nx
					target -> z [row, col] = expression
				ENDFOR
			ENDFOR
		"expression" is the text to be compiled and interpreted.
		If "target" is NULL, the result will go to "me"; otherwise, to "target".
	Return value:
		0 in case of failure, otherwise 1.
*/
int Matrix_formula (Matrix me, const wchar_t *expression, Interpreter *interpreter, Matrix target) {
	struct Formula_Result result;
	Formula_compile (interpreter, me, expression, kFormula_EXPRESSION_TYPE_NUMERIC, TRUE); cherror
	if (target == NULL) target = me;
	for (long irow = 1; irow <= my ny; irow ++) {
		for (long icol = 1; icol <= my nx; icol ++) {
			Formula_run (irow, icol, & result); cherror
			target -> z [irow] [icol] = result. result.numericResult;
		}
	}
end:
	iferror return 0;
	return 1;
}

int Matrix_formula_part (Matrix me, double xmin, double xmax, double ymin, double ymax,
	const wchar_t *expression, Interpreter *interpreter, Matrix target)
{
	long ixmin, ixmax, iymin, iymax;
	if (xmax <= xmin) { xmin = my xmin; xmax = my xmax; }
	if (ymax <= ymin) { ymin = my ymin; ymax = my ymax; }
	(void) Matrix_getWindowSamplesX (me, xmin, xmax, & ixmin, & ixmax);
	(void) Matrix_getWindowSamplesY (me, ymin, ymax, & iymin, & iymax);
	struct Formula_Result result;
	Formula_compile (interpreter, me, expression, kFormula_EXPRESSION_TYPE_NUMERIC, TRUE); cherror
	if (target == NULL) target = me;
	for (long irow = iymin; irow <= iymax; irow ++) {
		for (long icol = ixmin; icol <= ixmax; icol ++) {
			Formula_run (irow, icol, & result); cherror
			target -> z [irow] [icol] = result. result.numericResult;
		}
	}
end:
	iferror return 0;
	return 1;
}

FORM (Matrix_formula, L"Matrix Formula", L"Formula...")
	LABEL (L"label", L"y := y1; for row := 1 to nrow do { x := x1; "
		"for col := 1 to ncol do { self [row, col] := `formula' ; x := x + dx } y := y + dy }")
	TEXTFIELD (L"formula", L"self")
	OK
DO
	if (! praat_Fon_formula (dia, interpreter)) return 0;
END

DIRECT (Matrix_getHighestX) Matrix me = (structMatrix *)ONLY_OBJECT; Melder_informationReal (my xmax, NULL); END
DIRECT (Matrix_getHighestY) Matrix me = (structMatrix *)ONLY_OBJECT; Melder_informationReal (my ymax, NULL); END
DIRECT (Matrix_getLowestX) Matrix me = (structMatrix *)ONLY_OBJECT; Melder_informationReal (my xmin, NULL); END
DIRECT (Matrix_getLowestY) Matrix me = (structMatrix *)ONLY_OBJECT; Melder_informationReal (my ymin, NULL); END
DIRECT (Matrix_getNumberOfColumns) Matrix me = (structMatrix *)ONLY_OBJECT; Melder_information1 (Melder_integer (my nx)); END
DIRECT (Matrix_getNumberOfRows) Matrix me = (structMatrix *)ONLY_OBJECT; Melder_information1 (Melder_integer (my ny)); END
DIRECT (Matrix_getColumnDistance) Matrix me = (structMatrix *)ONLY_OBJECT; Melder_informationReal (my dx, NULL); END
DIRECT (Matrix_getRowDistance) Matrix me = (structMatrix *)ONLY_OBJECT; Melder_informationReal (my dy, NULL); END
DIRECT (Matrix_getSum) Matrix me = (structMatrix *)ONLY_OBJECT; Melder_informationReal (Matrix_getSum (me), NULL); END

DIRECT (Matrix_getMaximum)
	Matrix me = (structMatrix *)ONLY_OBJECT;
	double minimum = NUMundefined, maximum = NUMundefined;
	Matrix_getWindowExtrema (me, 0, 0, 0, 0, & minimum, & maximum);
	Melder_informationReal (maximum, NULL);
END

DIRECT (Matrix_getMinimum)
	Matrix me = (structMatrix *)ONLY_OBJECT;
	double minimum = NUMundefined, maximum = NUMundefined;
	Matrix_getWindowExtrema (me, 0, 0, 0, 0, & minimum, & maximum);
	Melder_informationReal (minimum, NULL);
END

FORM (Matrix_getValueAtXY, L"Matrix: Get value at xy", 0)
	REAL (L"X", L"0")
	REAL (L"Y", L"0")
	OK
DO
	Matrix me = (structMatrix *)ONLY_OBJECT;
	double x = GET_REAL (L"X"), y = GET_REAL (L"Y");
	Melder_information6 (Melder_double (Matrix_getValueAtXY (me, x, y)),
		L" (at x = ", Melder_double (x), L" and y = ", Melder_double (y), L")");
END

FORM (Matrix_getValueInCell, L"Matrix: Get value in cell", 0)
	NATURAL (L"Row number", L"1") NATURAL (L"Column number", L"1") OK DO Matrix me = (structMatrix *)ONLY_OBJECT;
	long row = GET_INTEGER (L"Row number"), column = GET_INTEGER (L"Column number");
	REQUIRE (row <= my ny, L"Row number must not exceed number of rows.")
	REQUIRE (column <= my nx, L"Column number must not exceed number of columns.")
	Melder_informationReal (my z [row] [column], NULL); END
FORM (Matrix_getXofColumn, L"Matrix: Get x of column", 0)
	NATURAL (L"Column number", L"1") OK DO
	Melder_informationReal (Matrix_columnToX (ONLY_OBJECT, GET_INTEGER (L"Column number")), NULL); END
FORM (Matrix_getYofRow, L"Matrix: Get y of row", 0)
	NATURAL (L"Row number", L"1") OK DO
	Melder_informationReal (Matrix_rowToY (ONLY_OBJECT, GET_INTEGER (L"Row number")), NULL); END

DIRECT (Matrix_help) Melder_help (L"Matrix"); END

DIRECT (Matrix_movie)
	Graphics g = Movie_create (L"Matrix movie", 300, 300);
	WHERE (SELECTED) Matrix_movie (OBJECT, g);
END

FORM (Matrix_paintCells, L"Matrix: Paint cells with greys", L"Matrix: Paint cells...")
	REAL (L"From x =", L"0.0")
	REAL (L"To x =", L"0.0")
	REAL (L"From y =", L"0.0")
	REAL (L"To y =", L"0.0")
	REAL (L"Minimum", L"0.0")
	REAL (L"Maximum", L"0.0")
	OK
DO
	EVERY_DRAW (Matrix_paintCells (OBJECT, GRAPHICS,
		GET_REAL (L"From x ="), GET_REAL (L"To x ="), GET_REAL (L"From y ="), GET_REAL (L"To y ="),
		GET_REAL (L"Minimum"), GET_REAL (L"Maximum")))
END

FORM (Matrix_paintContours, L"Matrix: Paint altitude contours with greys", 0)
	REAL (L"From x =", L"0.0")
	REAL (L"To x =", L"0.0")
	REAL (L"From y =", L"0.0")
	REAL (L"To y =", L"0.0")
	REAL (L"Minimum", L"0.0")
	REAL (L"Maximum", L"0.0")
	OK
DO
	EVERY_DRAW (Matrix_paintContours (OBJECT, GRAPHICS,
		GET_REAL (L"From x ="), GET_REAL (L"To x ="), GET_REAL (L"From y ="), GET_REAL (L"To y ="),
		GET_REAL (L"Minimum"), GET_REAL (L"Maximum")))
END

FORM (Matrix_paintImage, L"Matrix: Paint grey image", 0)
	REAL (L"From x =", L"0.0")
	REAL (L"To x =", L"0.0")
	REAL (L"From y =", L"0.0")
	REAL (L"To y =", L"0.0")
	REAL (L"Minimum", L"0.0")
	REAL (L"Maximum", L"0.0")
	OK
DO
	EVERY_DRAW (Matrix_paintImage (OBJECT, GRAPHICS,
		GET_REAL (L"From x ="), GET_REAL (L"To x ="), GET_REAL (L"From y ="), GET_REAL (L"To y ="),
		GET_REAL (L"Minimum"), GET_REAL (L"Maximum")))
END

FORM (Matrix_paintSurface, L"Matrix: Paint 3-D surface plot", 0)
	REAL (L"From x =", L"0.0")
	REAL (L"To x =", L"0.0")
	REAL (L"From y =", L"0.0")
	REAL (L"To y =", L"0.0")
	REAL (L"Minimum", L"0.0")
	REAL (L"Maximum", L"0.0")
	OK
DO
	EVERY_DRAW (Matrix_paintSurface (OBJECT, GRAPHICS,
		GET_REAL (L"From x ="), GET_REAL (L"To x ="), GET_REAL (L"From y ="), GET_REAL (L"To y ="),
		GET_REAL (L"Minimum"), GET_REAL (L"Maximum"), 30, 45))
END

FORM (Matrix_power, L"Matrix: Power...", 0)
	NATURAL (L"Power", L"2")
	OK
DO
	EVERY_TO (Matrix_power (OBJECT, GET_INTEGER (L"Power")))
END

FORM_READ (Matrix_readFromRawTextFile, L"Read Matrix from raw text file", 0, true)
	if (! praat_new1 (Matrix_readFromRawTextFile (file), MelderFile_name (file))) return 0;
END

FORM_READ (Matrix_readAP, L"Read Matrix from LVS AP file", 0, true)
	if (! praat_new1 (Matrix_readAP (file), MelderFile_name (file))) return 0;
END

FORM (Matrix_setValue, L"Matrix: Set value", L"Matrix: Set value...")
	NATURAL (L"Row number", L"1")
	NATURAL (L"Column number", L"1")
	REAL (L"New value", L"0.0")
	OK
DO
	WHERE (SELECTED) {
		Matrix me = (structMatrix *)OBJECT;
		long row = GET_INTEGER (L"Row number"), column = GET_INTEGER (L"Column number");
		REQUIRE (row <= my ny, L"Row number must not be greater than number of rows.")
		REQUIRE (column <= my nx, L"Column number must not be greater than number of columns.")
		my z [row] [column] = GET_REAL (L"New value");
		praat_dataChanged (me);
	}
END

DIRECT (Matrix_to_Cochleagram)
	EVERY_TO (Matrix_to_Cochleagram ((structMatrix *)OBJECT))
END

DIRECT (Matrix_to_Excitation)
	EVERY_TO (Matrix_to_Excitation ((structMatrix *)OBJECT))
END

DIRECT (Matrix_to_Harmonicity)
	EVERY_TO (Matrix_to_Harmonicity ((structMatrix *)OBJECT))
END

DIRECT (Matrix_to_Intensity)
	EVERY_TO (Matrix_to_Intensity ((structMatrix *)OBJECT))
END

DIRECT (Matrix_to_Pitch)
	EVERY_TO (Matrix_to_Pitch ((structMatrix *)OBJECT))
END

DIRECT (Matrix_to_Spectrogram)
	EVERY_TO (Matrix_to_Spectrogram ((structMatrix *)OBJECT))
END

DIRECT (Matrix_to_Spectrum)
	EVERY_TO (Matrix_to_Spectrum ((structMatrix *)OBJECT))
END

DIRECT (Matrix_to_Ltas)
	EVERY_TO (Matrix_to_Ltas ((structMatrix *)OBJECT))
END

DIRECT (Matrix_to_ParamCurve)
	Matrix m1 = NULL, m2 = NULL;
	WHERE (SELECTED) { if (m1) m2 = (structMatrix *)OBJECT; else m1 = (structMatrix *)OBJECT; }
	if (! praat_new3 (ParamCurve_create (m1, m2), m1 -> name, L"_", m2 -> name)) return 0;
END

DIRECT (Matrix_to_PointProcess)
	EVERY_TO (Matrix_to_PointProcess ((structMatrix *)OBJECT))
END

DIRECT (Matrix_to_Polygon)
	EVERY_TO (Matrix_to_Polygon ((structMatrix *)OBJECT))
END

FORM (Matrix_to_Sound_mono, L"Matrix: To Sound (mono)", 0)
	INTEGER (L"Row", L"1")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"(negative values count from last row)")
	OK
DO
	EVERY_TO (Matrix_to_Sound_mono ((structMatrix *)OBJECT, GET_INTEGER (L"Row")))
END

DIRECT (Matrix_to_TableOfReal)
	EVERY_TO (Matrix_to_TableOfReal ((structMatrix *)OBJECT))
END

DIRECT (Matrix_to_Transition)
	EVERY_TO (Matrix_to_Transition ((structMatrix *)OBJECT))
END

DIRECT (Matrix_to_VocalTract)
	EVERY_TO (Matrix_to_VocalTract ((structMatrix *)OBJECT))
END

FORM_WRITE (Matrix_writeToMatrixTextFile, L"Save Matrix as matrix text file", 0, L"mat")
	if (! Matrix_writeToMatrixTextFile ((structMatrix *)ONLY_OBJECT, file)) return 0;
END

FORM_WRITE (Matrix_writeToHeaderlessSpreadsheetFile, L"Save Matrix as spreadsheet", 0, L"txt")
	if (! Matrix_writeToHeaderlessSpreadsheetFile ((structMatrix *)ONLY_OBJECT, file)) return 0;
END

/***** PARAMCURVE *****/

/*
	Function:
		draw the points of the curve between parameter values t1 and t2,
		in time steps dt starting at t1 and including t2,
		along the axes [x1, x2] x [y1, y2].
	Defaults:
		t2 <= t1: draw all (overlapping) points.
		dt <= 0.0: time step is the smaller of my x -> dx and my y -> dx.
		x2 <= x1: autoscaling along horizontal axis.
		y2 <= y1: autoscaling along vertical axis.
	Return value:
		1 if OK, 0 if out of memory.
*/
int ParamCurve_draw (I, Graphics g, double t1, double t2, double dt,
	double x1, double x2, double y1, double y2, int garnish)
{
	iam (ParamCurve);
	long numberOfPoints, i;
	double *x, *y;
	if (t2 <= t1) {
		double tx1 = my x -> x1;
		double ty1 = my y -> x1;
		double tx2 = my x -> x1 + (my x -> nx - 1) * my x -> dx;
		double ty2 = my y -> x1 + (my y -> nx - 1) * my y -> dx;
		t1 = tx1 > ty1 ? tx1 : ty1;
		t2 = tx2 < ty2 ? tx2 : ty2;
	}
	if (x2 <= x1) Matrix_getWindowExtrema (my x, 0, 0, 1, 1, & x1, & x2);
	if (x1 == x2) { x1 -= 1.0; x2 += 1.0; }
	if (y2 <= y1) Matrix_getWindowExtrema (my y, 0, 0, 1, 1, & y1, & y2);
	if (y1 == y2) { y1 -= 1.0; y2 += 1.0; }
	if (dt <= 0.0)
		dt = my x -> dx < my y -> dx ? my x -> dx : my y -> dx;
	numberOfPoints = (long) ceil ((t2 - t1) / dt) + 1;
	if (! (x = NUMdvector (1, numberOfPoints)) || ! (y = NUMdvector (1, numberOfPoints))) {
		NUMdvector_free (x, 1);
		return 0;
	}
	for (i = 1; i <= numberOfPoints; i ++) {
		double t = i == numberOfPoints ? t2 : t1 + (i - 1) * dt;
		double index = Sampled_xToIndex (my x, t);
		x [i] = NUM_interpolate_sinc (my x -> z [1], my x -> nx, index, 50);
		index = Sampled_xToIndex (my y, t);
		y [i] = NUM_interpolate_sinc (my y -> z [1], my y -> nx, index, 50);
	}
	Graphics_setWindow (g, x1, x2, y1, y2);
	Graphics_setInner (g);
	Graphics_polyline (g, numberOfPoints, & x [1], & y [1]);
	Graphics_unsetInner (g);
	NUMdvector_free (x, 1);
	NUMdvector_free (y, 1);
	if (garnish) {
		Graphics_drawInnerBox (g);
		Graphics_marksBottom (g, 2, 1, 1, 0);
		Graphics_marksLeft (g, 2, 1, 1, 0);
	}
	return 1;
}

FORM (ParamCurve_draw, L"Draw parametrized curve", 0)
	REAL (L"Tmin", L"0.0")
	REAL (L"Tmax", L"0.0")
	REAL (L"Step", L"0.0")
	REAL (L"Xmin", L"0.0")
	REAL (L"Xmax", L"0.0")
	REAL (L"Ymin", L"0.0")
	REAL (L"Ymax", L"0.0")
	BOOLEAN (L"Garnish", 1)
	OK
DO
	EVERY_DRAW (ParamCurve_draw ((structParamCurve *)OBJECT, GRAPHICS,
		GET_REAL (L"Tmin"), GET_REAL (L"Tmax"), GET_REAL (L"Step"),
		GET_REAL (L"Xmin"), GET_REAL (L"Xmax"), GET_REAL (L"Ymin"), GET_REAL (L"Ymax"),
		GET_INTEGER (L"Garnish")))
END

DIRECT (ParamCurve_help) Melder_help (L"ParamCurve"); END

/***** PITCH *****/

static void Sampled_speckleInside (I, Graphics g, double xmin, double xmax, double ymin, double ymax,
	double speckle_mm, long ilevel, int unit)
{
	iam (Sampled);
	long ixmin, ixmax, ix;
	Function_unidirectionalAutowindow (me, & xmin, & xmax);
	Sampled_getWindowSamples (me, xmin, xmax, & ixmin, & ixmax);
	if (ClassFunction_isUnitLogarithmic (my methods, ilevel, unit)) {
		ymin = ClassFunction_convertStandardToSpecialUnit (my methods, ymin, ilevel, unit);
		ymax = ClassFunction_convertStandardToSpecialUnit (my methods, ymax, ilevel, unit);
	}
	if (ymax <= ymin) return;
	Graphics_setWindow (g, xmin, xmax, ymin, ymax);
	for (ix = ixmin; ix <= ixmax; ix ++) {
		double value = Sampled_getValueAtSample (me, ix, ilevel, unit);
		if (NUMdefined (value)) {
			double x = Sampled_indexToX (me, ix);
			if (value >= ymin && value <= ymax) {
				Graphics_fillCircle_mm (g, x, value, speckle_mm);
			}
		}
	}
}

void Sampled_drawInside (I, Graphics g, double xmin, double xmax, double ymin, double ymax,
	double speckle_mm, long ilevel, int unit)
{
	iam (Sampled);
	long ixmin, ixmax, ix, startOfDefinedStretch = -1;
	double *xarray = NULL, *yarray = NULL;
	double previousValue = NUMundefined;
	if (speckle_mm != 0.0) {
		Sampled_speckleInside (me, g, xmin, xmax, ymin, ymax, speckle_mm, ilevel, unit);
		return;
	}
	Function_unidirectionalAutowindow (me, & xmin, & xmax);
	Sampled_getWindowSamples (me, xmin, xmax, & ixmin, & ixmax);
	if (ClassFunction_isUnitLogarithmic (my methods, ilevel, unit)) {
		ymin = ClassFunction_convertStandardToSpecialUnit (my methods, ymin, ilevel, unit);
		ymax = ClassFunction_convertStandardToSpecialUnit (my methods, ymax, ilevel, unit);
	}
	if (ymax <= ymin) return;
	Graphics_setWindow (g, xmin, xmax, ymin, ymax);
	xarray = NUMdvector (ixmin - 1, ixmax + 1); cherror
	yarray = NUMdvector (ixmin - 1, ixmax + 1); cherror
	previousValue = Sampled_getValueAtSample (me, ixmin - 1, ilevel, unit);
	if (NUMdefined (previousValue)) {
		startOfDefinedStretch = ixmin - 1;
		xarray [ixmin - 1] = Sampled_indexToX (me, ixmin - 1);
		yarray [ixmin - 1] = previousValue;
	}
	for (ix = ixmin; ix <= ixmax; ix ++) {
		double x = Sampled_indexToX (me, ix), value = Sampled_getValueAtSample (me, ix, ilevel, unit);
		if (NUMdefined (value)) {
			if (NUMdefined (previousValue)) {
				xarray [ix] = x;
				yarray [ix] = value;
			} else {
				startOfDefinedStretch = ix - 1;
				xarray [ix - 1] = x - 0.5 * my dx;
				yarray [ix - 1] = value;
				xarray [ix] = x;
				yarray [ix] = value;
			}
		} else if (NUMdefined (previousValue)) {
			Melder_assert (startOfDefinedStretch >= ixmin - 1);
			if (ix > ixmin) {
				xarray [ix] = x - 0.5 * my dx;
				yarray [ix] = previousValue;
				if (xarray [startOfDefinedStretch] < xmin) {
					double phase = (xmin - xarray [startOfDefinedStretch]) / my dx;
					xarray [startOfDefinedStretch] = xmin;
					yarray [startOfDefinedStretch] = phase * yarray [startOfDefinedStretch + 1] + (1.0 - phase) * yarray [startOfDefinedStretch];
				}
				Graphics_polyline (g, ix + 1 - startOfDefinedStretch, & xarray [startOfDefinedStretch], & yarray [startOfDefinedStretch]);
			}
			startOfDefinedStretch = -1;
		}
		previousValue = value;
	}
	if (startOfDefinedStretch > -1) {
		double x = Sampled_indexToX (me, ixmax + 1), value = Sampled_getValueAtSample (me, ixmax + 1, ilevel, unit);
		Melder_assert (NUMdefined (previousValue));
		if (NUMdefined (value)) {
			xarray [ixmax + 1] = x;
			yarray [ixmax + 1] = value;
		} else {
			xarray [ixmax + 1] = x - 0.5 * my dx;
			yarray [ixmax + 1] = previousValue;
		}
		if (xarray [startOfDefinedStretch] < xmin) {
			double phase = (xmin - xarray [startOfDefinedStretch]) / my dx;
			xarray [startOfDefinedStretch] = xmin;
			yarray [startOfDefinedStretch] = phase * yarray [startOfDefinedStretch + 1] + (1.0 - phase) * yarray [startOfDefinedStretch];
		}
		if (xarray [ixmax + 1] > xmax) {
			double phase = (xarray [ixmax + 1] - xmax) / my dx;
			xarray [ixmax + 1] = xmax;
			yarray [ixmax + 1] = phase * yarray [ixmax] + (1.0 - phase) * yarray [ixmax + 1];
		}
		Graphics_polyline (g, ixmax + 2 - startOfDefinedStretch, & xarray [startOfDefinedStretch], & yarray [startOfDefinedStretch]);
	}
end:
	NUMdvector_free (xarray, ixmin - 1);
	NUMdvector_free (yarray, ixmin - 1);
	Melder_clearError ();
}

void Pitch_drawInside (Pitch me, Graphics g, double xmin, double xmax, double fmin, double fmax, int speckle, int unit) {
	Sampled_drawInside (me, g, xmin, xmax, fmin, fmax, speckle, Pitch_LEVEL_FREQUENCY, unit);
}

/*
	If tmax <= tmin, draw whole time domain.
*/
void Pitch_draw (Pitch me, Graphics g, double tmin, double tmax, double fmin, double fmax, int garnish, int speckle, int unit) {
	Graphics_setInner (g);
	Pitch_drawInside (me, g, tmin, tmax, fmin, fmax, speckle, unit);
	Graphics_unsetInner (g);
	if (garnish) {
		Graphics_drawInnerBox (g);
		Graphics_textBottom (g, TRUE, L"Time (s)");
		Graphics_marksBottom (g, 2, TRUE, TRUE, FALSE);
		static MelderString buffer = { 0 };
		MelderString_empty (& buffer);
		MelderString_append3 (& buffer, L"Pitch (", ClassFunction_getUnitText (classPitch, Pitch_LEVEL_FREQUENCY, unit, Function_UNIT_TEXT_GRAPHICAL), L")");
		Graphics_textLeft (g, TRUE, buffer.string);
		if (ClassFunction_isUnitLogarithmic (classPitch, Pitch_LEVEL_FREQUENCY, unit)) {
			Graphics_marksLeftLogarithmic (g, 6, TRUE, TRUE, FALSE);
		} else {
			Graphics_marksLeft (g, 2, TRUE, TRUE, FALSE);
		}
	}
}

DIRECT (Pitch_getNumberOfVoicedFrames)
	Pitch me = (structPitch *)ONLY (classPitch);
	Melder_information2 (Melder_integer (Pitch_countVoicedFrames (me)), L" voiced frames");
END

DIRECT (Pitch_difference)
	Pitch pit1 = NULL, pit2 = NULL;
	WHERE (SELECTED && CLASS == classPitch) { if (pit1) pit2 = (structPitch *)OBJECT; else pit1 = (structPitch *)OBJECT; }
	Pitch_difference (pit1, pit2);
END

FORM (Pitch_draw, L"Pitch: Draw", L"Pitch: Draw...")
	praat_dia_timeRange (dia);
	REAL (STRING_FROM_FREQUENCY_HZ, L"0.0")
	POSITIVE (STRING_TO_FREQUENCY_HZ, L"500.0")
	BOOLEAN (L"Garnish", 1)
	OK
DO
	GET_TMIN_TMAX_FMIN_FMAX
	EVERY_DRAW (Pitch_draw ((structPitch *)OBJECT, GRAPHICS, tmin, tmax, fmin, fmax,
		GET_INTEGER (L"Garnish"), Pitch_speckle_NO, kPitch_unit_HERTZ))
END

FORM (Pitch_drawErb, L"Pitch: Draw erb", L"Pitch: Draw...")
	REAL (STRING_FROM_TIME_SECONDS, L"0.0")
	REAL (STRING_TO_TIME_SECONDS, L"0.0 (= all)")
	REAL (L"left Frequency range (ERB)", L"0")
	REAL (L"right Frequency range (ERB)", L"10.0")
	BOOLEAN (L"Garnish", 1)
	OK
DO
	GET_TMIN_TMAX_FMIN_FMAX
	EVERY_DRAW (Pitch_draw ((structPitch *)OBJECT, GRAPHICS, tmin, tmax, fmin, fmax,
		GET_INTEGER (L"Garnish"), Pitch_speckle_NO, kPitch_unit_ERB))
END

FORM (Pitch_drawLogarithmic, L"Pitch: Draw logarithmic", L"Pitch: Draw...")
	REAL (STRING_FROM_TIME_SECONDS, L"0.0")
	REAL (STRING_TO_TIME_SECONDS, L"0.0 (= all)")
	POSITIVE (STRING_FROM_FREQUENCY_HZ, L"50.0")
	POSITIVE (STRING_TO_FREQUENCY_HZ, L"500.0")
	BOOLEAN (L"Garnish", 1)
	OK
DO
	GET_TMIN_TMAX_FMIN_FMAX
	EVERY_DRAW (Pitch_draw ((structPitch *)OBJECT, GRAPHICS, tmin, tmax, fmin, fmax,
		GET_INTEGER (L"Garnish"), Pitch_speckle_NO, kPitch_unit_HERTZ_LOGARITHMIC))
END

FORM (Pitch_drawMel, L"Pitch: Draw mel", L"Pitch: Draw...")
	REAL (STRING_FROM_TIME_SECONDS, L"0.0")
	REAL (STRING_TO_TIME_SECONDS, L"0.0 (= all)")
	REAL (L"left Frequency range (mel)", L"0.0")
	REAL (L"right Frequency range (mel)", L"500.0")
	BOOLEAN (L"Garnish", 1)
	OK
DO
	GET_TMIN_TMAX_FMIN_FMAX
	EVERY_DRAW (Pitch_draw ((structPitch *)OBJECT, GRAPHICS, tmin, tmax, fmin, fmax,
		GET_INTEGER (L"Garnish"), Pitch_speckle_NO, kPitch_unit_MEL))
END

FORM (Pitch_drawSemitones, L"Pitch: Draw semitones", L"Pitch: Draw...")
	REAL (STRING_FROM_TIME_SECONDS, L"0.0")
	REAL (STRING_TO_TIME_SECONDS, L"0.0 (= all)")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"Range in semitones re 100 Hertz:")
	REAL (L"left Frequency range (st)", L"-12.0")
	REAL (L"right Frequency range (st)", L"30.0")
	BOOLEAN (L"Garnish", 1)
	OK
DO
	GET_TMIN_TMAX_FMIN_FMAX
	EVERY_DRAW (Pitch_draw ((structPitch *)OBJECT, GRAPHICS, tmin, tmax, fmin, fmax,
		GET_INTEGER (L"Garnish"), Pitch_speckle_NO, kPitch_unit_SEMITONES_100))
END

DIRECT (Pitch_edit)
	if (theCurrentPraatApplication -> batch)
		return Melder_error1 (L"Cannot edit a Pitch from batch.");
	else
		WHERE (SELECTED)
			if (! praat_installEditor (new PitchEditor (theCurrentPraatApplication -> topShell, ID_AND_FULL_NAME, (structPitch *)OBJECT), IOBJECT))
				return 0;
END

int Pitch_formula (Pitch me, const wchar_t *formula, Interpreter *interpreter) {
	Matrix m = Matrix_create (my xmin, my xmax, my nx, my dx, my x1, 1, my maxnCandidates, my maxnCandidates, 1, 1); cherror
	for (long iframe = 1; iframe <= my nx; iframe ++) {
		Pitch_Frame frame = & my frame [iframe];
		for (long icand = 1; icand <= frame -> nCandidates; icand ++)
			m -> z [icand] [iframe] = frame -> candidate [icand]. frequency;
	}
	Matrix_formula (m, formula, interpreter, NULL); cherror
	for (long iframe = 1; iframe <= my nx; iframe ++) {
		Pitch_Frame frame = & my frame [iframe];
		for (long icand = 1; icand <= frame -> nCandidates; icand ++)
			frame -> candidate [icand]. frequency = m -> z [icand] [iframe];
	}
end:
	forget (m);
	iferror return Melder_error1 (L"(Pitch_formula:) Not performed.");
	return 1;
}

FORM (Pitch_formula, L"Pitch: Formula", L"Formula...")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"x = time; col = frame; row = candidate (1 = current path); frequency (time, candidate) :=")
	TEXTFIELD (L"formula", L"self*2; Example: octave jump up")
	OK
DO
	WHERE (SELECTED) {
		if (! Pitch_formula ((structPitch *)OBJECT, GET_STRING (L"formula"), interpreter)) return 0;
		praat_dataChanged (OBJECT);
	}
END

FORM (Pitch_getMaximum, L"Pitch: Get maximum", 0)
	praat_dia_timeRange (dia);
	OPTIONMENU_ENUM (L"Unit", kPitch_unit, DEFAULT)
	RADIO (L"Interpolation", 2)
	RADIOBUTTON (L"None")
	RADIOBUTTON (L"Parabolic")
	OK
DO
	enum kPitch_unit unit = GET_ENUM (kPitch_unit, L"Unit");
	double value = Pitch_getMaximum ((structPitch *)ONLY (classPitch), GET_REAL (L"left Time range"), GET_REAL (L"right Time range"),
		unit, GET_INTEGER (L"Interpolation") - 1);
	value = ClassFunction_convertToNonlogarithmic (classPitch, value, Pitch_LEVEL_FREQUENCY, unit);
	Melder_informationReal (value, ClassFunction_getUnitText (classPitch, Pitch_LEVEL_FREQUENCY, unit, 0));
END

FORM (Pitch_getMean, L"Pitch: Get mean", 0)
	praat_dia_timeRange (dia);
	OPTIONMENU_ENUM (L"Unit", kPitch_unit, DEFAULT)
	OK
DO
	enum kPitch_unit unit = GET_ENUM (kPitch_unit, L"Unit");
	double value = Pitch_getMean ((structPitch *)ONLY (classPitch), GET_REAL (L"left Time range"), GET_REAL (L"right Time range"), unit);
	value = ClassFunction_convertToNonlogarithmic (classPitch, value, Pitch_LEVEL_FREQUENCY, unit);
	Melder_informationReal (value, ClassFunction_getUnitText (classPitch, Pitch_LEVEL_FREQUENCY, unit, 0));
END

FORM (Pitch_getMeanAbsoluteSlope, L"Pitch: Get mean absolute slope", 0)
	RADIO (L"Unit", 1)
		RADIOBUTTON (L"Hertz")
		RADIOBUTTON (L"Mel")
		RADIOBUTTON (L"Semitones")
		RADIOBUTTON (L"ERB")
	OK
DO
	int unit = GET_INTEGER (L"Unit");
	double slope;
	long nVoiced = (unit == 1 ? Pitch_getMeanAbsSlope_hertz : unit == 2 ? Pitch_getMeanAbsSlope_mel : unit == 3 ? Pitch_getMeanAbsSlope_semitones : Pitch_getMeanAbsSlope_erb)
		((structPitch *)ONLY (classPitch), & slope);
	if (nVoiced < 2) {
		Melder_information1 (L"--undefined--");
	} else {
		Melder_information4 (Melder_double (slope), L" ", GET_STRING (L"Unit"), L"/s");
	}
END

DIRECT (Pitch_getMeanAbsSlope_noOctave)
	double slope;
	(void) Pitch_getMeanAbsSlope_noOctave ((structPitch *)ONLY (classPitch), & slope);
	Melder_informationReal (slope, L"Semitones/s");
END

FORM (Pitch_getMinimum, L"Pitch: Get minimum", 0)
	praat_dia_timeRange (dia);
	OPTIONMENU_ENUM (L"Unit", kPitch_unit, DEFAULT)
	RADIO (L"Interpolation", 2)
	RADIOBUTTON (L"None")
	RADIOBUTTON (L"Parabolic")
	OK
DO
	enum kPitch_unit unit = GET_ENUM (kPitch_unit, L"Unit");
	double value = Sampled_getMinimum (ONLY (classPitch), GET_REAL (L"left Time range"), GET_REAL (L"right Time range"),
		Pitch_LEVEL_FREQUENCY, unit, GET_INTEGER (L"Interpolation") - 1);
	value = ClassFunction_convertToNonlogarithmic (classPitch, value, Pitch_LEVEL_FREQUENCY, unit);
	Melder_informationReal (value, ClassFunction_getUnitText (classPitch, Pitch_LEVEL_FREQUENCY, unit, 0));
END

FORM (Pitch_getQuantile, L"Pitch: Get quantile", 0)
	praat_dia_timeRange (dia);
	REAL (L"Quantile", L"0.50 (= median)")
	OPTIONMENU_ENUM (L"Unit", kPitch_unit, DEFAULT)
	OK
DO
	enum kPitch_unit unit = GET_ENUM (kPitch_unit, L"Unit");
	double value = Sampled_getQuantile (ONLY (classPitch), GET_REAL (L"left Time range"), GET_REAL (L"right Time range"),
		GET_REAL (L"Quantile"), Pitch_LEVEL_FREQUENCY, unit);
	value = ClassFunction_convertToNonlogarithmic (classPitch, value, Pitch_LEVEL_FREQUENCY, unit);
	Melder_informationReal (value, ClassFunction_getUnitText (classPitch, Pitch_LEVEL_FREQUENCY, unit, 0));
END

FORM (Pitch_getStandardDeviation, L"Pitch: Get standard deviation", 0)
	praat_dia_timeRange (dia);
	OPTIONMENU (L"Unit", 1)
		OPTION (L"Hertz")
		OPTION (L"mel")
		OPTION (L"logHertz")
		OPTION (L"semitones")
		OPTION (L"ERB")
	OK
DO
	int unit = GET_INTEGER (L"Unit");
	double value;
	const wchar_t *unitText;
	unit =
		unit == 1 ? kPitch_unit_HERTZ :
		unit == 2 ? kPitch_unit_MEL :
		unit == 3 ? kPitch_unit_LOG_HERTZ :
		unit == 4 ? kPitch_unit_SEMITONES_1 :
		kPitch_unit_ERB;
	value = Pitch_getStandardDeviation ((structPitch *)ONLY (classPitch), GET_REAL (L"left Time range"), GET_REAL (L"right Time range"), unit);
	unitText =
		unit == kPitch_unit_HERTZ ? L"Hz" :
		unit == kPitch_unit_MEL ? L"mel" :
		unit == kPitch_unit_LOG_HERTZ ? L"logHz" :
		unit == kPitch_unit_SEMITONES_1 ? L"semitones" :
		L"ERB";
	Melder_informationReal (value, unitText);
END

FORM (Pitch_getTimeOfMaximum, L"Pitch: Get time of maximum", 0)
	praat_dia_timeRange (dia);
	OPTIONMENU_ENUM (L"Unit", kPitch_unit, DEFAULT)
	RADIO (L"Interpolation", 2)
	RADIOBUTTON (L"None")
	RADIOBUTTON (L"Parabolic")
	OK
DO
	Melder_informationReal (Pitch_getTimeOfMaximum ((structPitch *)ONLY (classPitch),
		GET_REAL (L"left Time range"), GET_REAL (L"right Time range"),
		GET_ENUM (kPitch_unit, L"Unit"), GET_INTEGER (L"Interpolation") - 1), L"seconds");
END

FORM (Pitch_getTimeOfMinimum, L"Pitch: Get time of minimum", 0)
	praat_dia_timeRange (dia);
	OPTIONMENU_ENUM (L"Unit", kPitch_unit, DEFAULT)
	RADIO (L"Interpolation", 2)
	RADIOBUTTON (L"None")
	RADIOBUTTON (L"Parabolic")
	OK
DO
	Melder_informationReal (Pitch_getTimeOfMinimum ((structPitch *)ONLY (classPitch),
		GET_REAL (L"left Time range"), GET_REAL (L"right Time range"),
		GET_ENUM (kPitch_unit, L"Unit"), GET_INTEGER (L"Interpolation") - 1), L"seconds");
END

FORM (Pitch_getValueAtTime, L"Pitch: Get value at time", L"Pitch: Get value at time...")
	REAL (L"Time (s)", L"0.5")
	OPTIONMENU_ENUM (L"Unit", kPitch_unit, DEFAULT)
	RADIO (L"Interpolation", 2)
	RADIOBUTTON (L"Nearest")
	RADIOBUTTON (L"Linear")
	OK
DO
	enum kPitch_unit unit = GET_ENUM (kPitch_unit, L"Unit");
	double value = Sampled_getValueAtX (ONLY (classPitch), GET_REAL (L"Time"), Pitch_LEVEL_FREQUENCY, unit, GET_INTEGER (L"Interpolation") - 1);
	value = ClassFunction_convertToNonlogarithmic (classPitch, value, Pitch_LEVEL_FREQUENCY, unit);
	Melder_informationReal (value, ClassFunction_getUnitText (classPitch, Pitch_LEVEL_FREQUENCY, unit, 0));
END
	
FORM (Pitch_getValueInFrame, L"Pitch: Get value in frame", L"Pitch: Get value in frame...")
	INTEGER (L"Frame number", L"10")
	OPTIONMENU_ENUM (L"Unit", kPitch_unit, DEFAULT)
	OK
DO
	enum kPitch_unit unit = GET_ENUM (kPitch_unit, L"Unit");
	double value = Sampled_getValueAtSample (ONLY (classPitch), GET_INTEGER (L"Frame number"), Pitch_LEVEL_FREQUENCY, unit);
	value = ClassFunction_convertToNonlogarithmic (classPitch, value, Pitch_LEVEL_FREQUENCY, unit);
	Melder_informationReal (value, ClassFunction_getUnitText (classPitch, Pitch_LEVEL_FREQUENCY, unit, 0));
END

DIRECT (Pitch_help) Melder_help (L"Pitch"); END

DIRECT (Pitch_hum)
	EVERY_CHECK (Pitch_hum ((structPitch *)OBJECT, 0, 0))
END

DIRECT (Pitch_interpolate)
	EVERY_TO (Pitch_interpolate ((structPitch *)OBJECT))
END

DIRECT (Pitch_killOctaveJumps)
	EVERY_TO (Pitch_killOctaveJumps ((structPitch *)OBJECT))
END

DIRECT (Pitch_play)
	EVERY_CHECK (Pitch_play ((structPitch *)OBJECT, 0, 0))
END

FORM (Pitch_smooth, L"Pitch: Smooth", L"Pitch: Smooth...")
	REAL (L"Bandwidth (Hertz)", L"10.0")
	OK
DO
	EVERY_TO (Pitch_smooth ((structPitch *)OBJECT, GET_REAL (L"Bandwidth")))
END

FORM (Pitch_speckle, L"Pitch: Speckle", L"Pitch: Draw...")
	REAL (STRING_FROM_TIME_SECONDS, L"0.0")
	REAL (STRING_TO_TIME_SECONDS, L"0.0 (= all)")
	REAL (STRING_FROM_FREQUENCY_HZ, L"0.0")
	POSITIVE (STRING_TO_FREQUENCY_HZ, L"500.0")
	BOOLEAN (L"Garnish", 1)
	OK
DO
	GET_TMIN_TMAX_FMIN_FMAX
	EVERY_DRAW (Pitch_draw ((structPitch *)OBJECT, GRAPHICS, tmin, tmax, fmin, fmax,
		GET_INTEGER (L"Garnish"), Pitch_speckle_YES, kPitch_unit_HERTZ))
END

FORM (Pitch_speckleErb, L"Pitch: Speckle erb", L"Pitch: Draw...")
	REAL (STRING_FROM_TIME_SECONDS, L"0.0")
	REAL (STRING_TO_TIME_SECONDS, L"0.0 (= all)")
	REAL (L"left Frequency range (ERB)", L"0")
	REAL (L"right Frequency range (ERB)", L"10.0")
	BOOLEAN (L"Garnish", 1)
	OK
DO
	GET_TMIN_TMAX_FMIN_FMAX
	EVERY_DRAW (Pitch_draw ((structPitch *)OBJECT, GRAPHICS, tmin, tmax, fmin, fmax,
		GET_INTEGER (L"Garnish"), Pitch_speckle_YES, kPitch_unit_ERB))
END

FORM (Pitch_speckleLogarithmic, L"Pitch: Speckle logarithmic", L"Pitch: Draw...")
	REAL (STRING_FROM_TIME_SECONDS, L"0.0")
	REAL (STRING_TO_TIME_SECONDS, L"0.0 (= all)")
	POSITIVE (STRING_FROM_FREQUENCY_HZ, L"50.0")
	POSITIVE (STRING_TO_FREQUENCY_HZ, L"500.0")
	BOOLEAN (L"Garnish", 1)
	OK
DO
	GET_TMIN_TMAX_FMIN_FMAX
	EVERY_DRAW (Pitch_draw ((structPitch *)OBJECT, GRAPHICS, tmin, tmax, fmin, fmax,
		GET_INTEGER (L"Garnish"), Pitch_speckle_YES, kPitch_unit_HERTZ_LOGARITHMIC))
END

FORM (Pitch_speckleMel, L"Pitch: Speckle mel", L"Pitch: Draw...")
	REAL (STRING_FROM_TIME_SECONDS, L"0.0")
	REAL (STRING_TO_TIME_SECONDS, L"0.0 (= all)")
	REAL (L"left Frequency range (mel)", L"0")
	REAL (L"right Frequency range (mel)", L"500")
	BOOLEAN (L"Garnish", 1)
	OK
DO
	GET_TMIN_TMAX_FMIN_FMAX
	EVERY_DRAW (Pitch_draw ((structPitch *)OBJECT, GRAPHICS, tmin, tmax, fmin, fmax,
		GET_INTEGER (L"Garnish"), Pitch_speckle_YES, kPitch_unit_MEL))
END

FORM (Pitch_speckleSemitones, L"Pitch: Speckle semitones", L"Pitch: Draw...")
	REAL (STRING_FROM_TIME_SECONDS, L"0.0")
	REAL (STRING_TO_TIME_SECONDS, L"0.0 (= all)")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"Range in semitones re 100 Hertz:")
	REAL (L"left Frequency range (st)", L"-12.0")
	REAL (L"right Frequency range (st)", L"30.0")
	BOOLEAN (L"Garnish", 1)
	OK
DO
	GET_TMIN_TMAX_FMIN_FMAX
	EVERY_DRAW (Pitch_draw ((structPitch *)OBJECT, GRAPHICS, tmin, tmax, fmin, fmax,
		GET_INTEGER (L"Garnish"), Pitch_speckle_YES, kPitch_unit_SEMITONES_100))
END

FORM (Pitch_subtractLinearFit, L"Pitch: subtract linear fit", 0)
	RADIO (L"Unit", 1)
		RADIOBUTTON (L"Hertz")
		RADIOBUTTON (L"Hertz (logarithmic)")
		RADIOBUTTON (L"Mel")
		RADIOBUTTON (L"Semitones")
		RADIOBUTTON (L"ERB")
	OK
DO
	EVERY_TO (Pitch_subtractLinearFit ((structPitch *)OBJECT, GET_INTEGER (L"Unit") - 1))
END

DIRECT (Pitch_to_IntervalTier)
	EVERY_TO (IntervalTier_create (((Pitch) OBJECT) -> xmin, ((Pitch) OBJECT) -> xmax))
END

DIRECT (Pitch_to_Matrix)
	EVERY_TO (Pitch_to_Matrix ((structPitch *)OBJECT))
END

DIRECT (Pitch_to_PitchTier)
	EVERY_TO (Pitch_to_PitchTier ((structPitch *)OBJECT))
END

DIRECT (Pitch_to_PointProcess)
	EVERY_TO (Pitch_to_PointProcess ((structPitch *)OBJECT))
END

DIRECT (Pitch_to_Sound_pulses)
	EVERY_TO (Pitch_to_Sound ((structPitch *)OBJECT, 0, 0, FALSE))
END

DIRECT (Pitch_to_Sound_hum)
	EVERY_TO (Pitch_to_Sound ((structPitch *)OBJECT, 0, 0, TRUE))
END

FORM (Pitch_to_Sound_sine, L"Pitch: To Sound (sine)", 0)
	POSITIVE (L"Sampling frequency (Hz)", L"44100")
	RADIO (L"Cut voiceless stretches", 2)
		OPTION (L"exactly")
		OPTION (L"at nearest zero crossings")
	OK
DO
	EVERY_TO (Pitch_to_Sound_sine ((structPitch *)OBJECT, 0, 0, GET_REAL (L"Sampling frequency"), GET_INTEGER (L"Cut voiceless stretches") - 1))
END

FORM (Pitch_to_TextGrid, L"To TextGrid...", L"Pitch: To TextGrid...")
	SENTENCE (L"Tier names", L"Mary John bell")
	SENTENCE (L"Point tiers", L"bell")
	OK
DO
	EVERY_TO (TextGrid_create (((Pitch) OBJECT) -> xmin, ((Pitch) OBJECT) -> xmax,
		GET_STRING (L"Tier names"), GET_STRING (L"Point tiers")))
END

DIRECT (Pitch_to_TextTier)
	EVERY_TO (TextTier_create (((Pitch) OBJECT) -> xmin, ((Pitch) OBJECT) -> xmax))
END

/***** PITCH & PITCHTIER *****/

void RealTier_draw (I, Graphics g, double tmin, double tmax, double fmin, double fmax,
	int garnish, const wchar_t *method, const wchar_t *quantity)
{
	iam (RealTier);
	bool drawLines = wcsstr (method, L"lines") || wcsstr (method, L"Lines");
	bool drawSpeckles = wcsstr (method, L"speckles") || wcsstr (method, L"Speckles");
	long n = my points -> size, imin, imax, i;
	if (tmax <= tmin) { tmin = my xmin; tmax = my xmax; }
	Graphics_setWindow (g, tmin, tmax, fmin, fmax);
	Graphics_setInner (g);
	imin = AnyTier_timeToHighIndex (me, tmin);
	imax = AnyTier_timeToLowIndex (me, tmax);
	if (n == 0) {
	} else if (imax < imin) {
		double fleft = RealTier_getValueAtTime (me, tmin);
		double fright = RealTier_getValueAtTime (me, tmax);
		if (drawLines) Graphics_line (g, tmin, fleft, tmax, fright);
	} else for (i = imin; i <= imax; i ++) {
		RealPoint point = (structRealPoint *)my points -> item [i];
		double t = point -> time, f = point -> value;
		if (drawSpeckles) Graphics_fillCircle_mm (g, t, f, 1);
		if (drawLines) {
			if (i == 1)
				Graphics_line (g, tmin, f, t, f);
			else if (i == imin)
				Graphics_line (g, t, f, tmin, RealTier_getValueAtTime (me, tmin));
			if (i == n)
				Graphics_line (g, t, f, tmax, f);
			else if (i == imax)
				Graphics_line (g, t, f, tmax, RealTier_getValueAtTime (me, tmax));
			else {
				RealPoint pointRight = (structRealPoint *)my points -> item [i + 1];
				Graphics_line (g, t, f, pointRight -> time, pointRight -> value);
			}
		}
	}
	Graphics_unsetInner (g);
	if (garnish) {
		Graphics_drawInnerBox (g);
		Graphics_textBottom (g, TRUE, L"Time (s)");
		Graphics_marksBottom (g, 2, TRUE, TRUE, FALSE);
		Graphics_marksLeft (g, 2, TRUE, TRUE, FALSE);
		if (quantity) Graphics_textLeft (g, TRUE, quantity);
	}
}

/*void AmplitudeTier_draw (AmplitudeTier me, Graphics g, double tmin, double tmax,
	double ymin, double ymax, const wchar_t *method, int garnish)
{
	RealTier_draw (me, g, tmin, tmax, ymin, ymax, garnish, method, L"Sound pressure (Pa)");
}*/

/*void DurationTier_draw (DurationTier me, Graphics g, double tmin, double tmax,
	double ymin, double ymax, const wchar_t *method, int garnish)
{
	RealTier_draw (me, g, tmin, tmax, ymin, ymax, garnish, method, L"Relative duration");
}*/

/*void IntensityTier_draw (IntensityTier me, Graphics g, double tmin, double tmax,
	double ymin, double ymax, const wchar_t *method, int garnish)
{
	RealTier_draw (me, g, tmin, tmax, ymin, ymax, garnish, method, L"Intensity (dB)");
}*/

static void Pitch_line (Pitch me, Graphics g, double tmin, double fleft, double tmax, double fright,
	int nonPeriodicLineType)
{
	/*
	 * f = fleft + (t - tmin) * (fright - fleft) / (tmax - tmin);
	 */
	int lineType = Graphics_inqLineType (g);
	double lineWidth = Graphics_inqLineWidth (g);
	double slope = (fright - fleft) / (tmax - tmin);
	long imin = Sampled_xToNearestIndex (me, tmin);
	long imax = Sampled_xToNearestIndex (me, tmax), i;
	if (imin < 1) imin = 1;
	if (imax > my nx) imax = my nx;
	for (i = imin; i <= imax; i ++) {
		double tleft, tright;
		if (! Pitch_isVoiced_i (me, i)) {
			if (nonPeriodicLineType == 2) continue;
			Graphics_setLineType (g, Graphics_DOTTED);
			Graphics_setLineWidth (g, 0.67 * lineWidth);
		} else if (nonPeriodicLineType != 2) {
			Graphics_setLineWidth (g, 2 * lineWidth);
		}
		tleft = Sampled_indexToX (me, i) - 0.5 * my dx, tright = tleft + my dx;
		if (tleft < tmin) tleft = tmin;
		if (tright > tmax) tright = tmax;
		Graphics_line (g, tleft, fleft + (tleft - tmin) * slope,
			tright, fleft + (tright - tmin) * slope);
		Graphics_setLineType (g, lineType);
		Graphics_setLineWidth (g, lineWidth);
	}
}

void PitchTier_draw (PitchTier me, Graphics g, double tmin, double tmax,
	double fmin, double fmax, int garnish, const wchar_t *method)
{
	RealTier_draw (me, g, tmin, tmax, fmin, fmax, garnish, method, L"Frequency (Hz)");
}

void PitchTier_Pitch_draw (PitchTier me, Pitch uv, Graphics g,
	double tmin, double tmax, double fmin, double fmax, int nonPeriodicLineType, int garnish, const wchar_t *method)
{
	long n = my points -> size, imin, imax, i;
	if (nonPeriodicLineType == 0) {
		PitchTier_draw (me, g, tmin, tmax, fmin, fmax, garnish, method);
		return;
	}
	if (tmax <= tmin) { tmin = my xmin; tmax = my xmax; }
	Graphics_setWindow (g, tmin, tmax, fmin, fmax);
	Graphics_setInner (g);
	imin = AnyTier_timeToHighIndex (me, tmin);
	imax = AnyTier_timeToLowIndex (me, tmax);
	if (n == 0) {
	} else if (imax < imin) {
		double fleft = RealTier_getValueAtTime (me, tmin);
		double fright = RealTier_getValueAtTime (me, tmax);
		Pitch_line (uv, g, tmin, fleft, tmax, fright, nonPeriodicLineType);
	} else for (i = imin; i <= imax; i ++) {
		RealPoint point = (structRealPoint *)my points -> item [i];
		double t = point -> time, f = point -> value;
		Graphics_fillCircle_mm (g, t, f, 1);
		if (i == 1)
			Pitch_line (uv, g, tmin, f, t, f, nonPeriodicLineType);
		else if (i == imin)
			Pitch_line (uv, g, t, f, tmin, RealTier_getValueAtTime (me, tmin), nonPeriodicLineType);
		if (i == n)
			Pitch_line (uv, g, t, f, tmax, f, nonPeriodicLineType);
		else if (i == imax)
			Pitch_line (uv, g, t, f, tmax, RealTier_getValueAtTime (me, tmax), nonPeriodicLineType);
		else {
			RealPoint pointRight = (structRealPoint *)my points -> item [i + 1];
			Pitch_line (uv, g, t, f, pointRight -> time, pointRight -> value, nonPeriodicLineType);
		}
	}
	Graphics_unsetInner (g);
	if (garnish) {
		Graphics_drawInnerBox (g);
		Graphics_textBottom (g, 1, L"Time (s)");
		Graphics_marksBottom (g, 2, 1, 1, 0);
		Graphics_marksLeft (g, 2, 1, 1, 0);
		Graphics_textLeft (g, 1, L"Frequency (Hz)");
	}
}

FORM (old_PitchTier_Pitch_draw, L"PitchTier & Pitch: Draw", 0)
	praat_dia_timeRange (dia);
	REAL (L"From frequency (Hz)", L"0.0")
	REAL (L"To frequency (Hz)", L"500.0")
	RADIO (L"Line type for non-periodic intervals", 2)
		RADIOBUTTON (L"Normal")
		RADIOBUTTON (L"Dotted")
		RADIOBUTTON (L"Blank")
	BOOLEAN (L"Garnish", 1)
	OK
DO
	praat_picture_open ();
	PitchTier_Pitch_draw ((structPitchTier *)ONLY (classPitchTier), (structPitch *)ONLY (classPitch), GRAPHICS,
		GET_REAL (L"left Time range"), GET_REAL (L"right Time range"),
		GET_REAL (L"From frequency"), GET_REAL (L"To frequency"),
		GET_INTEGER (L"Line type for non-periodic intervals") - 1,
		GET_INTEGER (L"Garnish"), L"lines and speckles");
	praat_picture_close ();
END

FORM (PitchTier_Pitch_draw, L"PitchTier & Pitch: Draw", 0)
	praat_dia_timeRange (dia);
	REAL (L"From frequency (Hz)", L"0.0")
	REAL (L"To frequency (Hz)", L"500.0")
	RADIO (L"Line type for non-periodic intervals", 2)
		RADIOBUTTON (L"Normal")
		RADIOBUTTON (L"Dotted")
		RADIOBUTTON (L"Blank")
	BOOLEAN (L"Garnish", 1)
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"ui/editors/AmplitudeTierEditor.h")
	OPTIONMENU (L"Drawing method", 1)
		OPTION (L"lines")
		OPTION (L"speckles")
		OPTION (L"lines and speckles")
	OK
DO_ALTERNATIVE (old_PitchTier_Pitch_draw)
	praat_picture_open ();
	PitchTier_Pitch_draw ((structPitchTier *)ONLY (classPitchTier), (structPitch *)ONLY (classPitch), GRAPHICS,
		GET_REAL (L"left Time range"), GET_REAL (L"right Time range"),
		GET_REAL (L"From frequency"), GET_REAL (L"To frequency"),
		GET_INTEGER (L"Line type for non-periodic intervals") - 1,
		GET_INTEGER (L"Garnish"), GET_STRING (L"Drawing method"));
	praat_picture_close ();
END

DIRECT (Pitch_PitchTier_to_Pitch)
	Pitch pitch = (structPitch *)ONLY (classPitch);
	PitchTier tier = (structPitchTier *)ONLY (classPitchTier);
	if (! praat_new2 (Pitch_PitchTier_to_Pitch (pitch, tier), pitch -> name, L"_stylized")) return 0;
END

/***** PITCH & POINTPROCESS *****/

DIRECT (Pitch_PointProcess_to_PitchTier)
	Pitch pitch = (structPitch *)ONLY (classPitch);
	if (! praat_new1 (Pitch_PointProcess_to_PitchTier (pitch, (structPointProcess *)ONLY (classPointProcess)), pitch -> name)) return 0;
END

/***** PITCH & SOUND *****/

DIRECT (Sound_Pitch_to_Manipulation)
	Pitch pitch = (structPitch *)ONLY (classPitch);
	if (! praat_new1 (Sound_Pitch_to_Manipulation ((structSound *)ONLY (classSound), pitch), pitch -> name)) return 0;
END

DIRECT (Sound_Pitch_to_PointProcess_cc)
	wchar_t name [200];
	praat_name2 (name, classSound, classPitch);
	if (! praat_new1 (Sound_Pitch_to_PointProcess_cc ((structSound *)ONLY (classSound), (structPitch *)ONLY (classPitch)), name)) return 0;
END

FORM (Sound_Pitch_to_PointProcess_peaks, L"Sound & Pitch: To PointProcess (peaks)", 0)
	BOOLEAN (L"Include maxima", 1)
	BOOLEAN (L"Include minima", 0)
	OK
DO
	wchar_t name [200];
	praat_name2 (name, classSound, classPitch);
	if (! praat_new1 (Sound_Pitch_to_PointProcess_peaks ((structSound *)ONLY (classSound), (structPitch *)ONLY (classPitch),
		GET_INTEGER (L"Include maxima"), GET_INTEGER (L"Include minima")), name)) return 0;
END

/***** PITCHTIER *****/

FORM (PitchTier_addPoint, L"PitchTier: Add point", L"PitchTier: Add point...")
	REAL (L"Time (s)", L"0.5")
	REAL (L"Pitch (Hz)", L"200")
	OK
DO
	WHERE (SELECTED) {
		if (! RealTier_addPoint (OBJECT, GET_REAL (L"Time"), GET_REAL (L"Pitch"))) return 0;
		praat_dataChanged (OBJECT);
	}
END

FORM (PitchTier_create, L"Create empty PitchTier", NULL)
	WORD (L"Name", L"empty")
	REAL (L"Start time (s)", L"0.0")
	REAL (L"End time (s)", L"1.0")
	OK
DO
	double startTime = GET_REAL (L"Start time"), endTime = GET_REAL (L"End time");
	REQUIRE (endTime > startTime, L"End time must be greater than start time.")
	if (! praat_new1 (PitchTier_create (startTime, endTime), GET_STRING (L"Name"))) return 0;
END

DIRECT (PitchTier_downto_PointProcess)
	EVERY_TO (AnyTier_downto_PointProcess (OBJECT))
END

FORM (PitchTier_downto_TableOfReal, L"PitchTier: Down to TableOfReal", NULL)
	RADIO (L"Unit", 1)
	RADIOBUTTON (L"Hertz")
	RADIOBUTTON (L"Semitones")
	OK
DO
	EVERY_TO (PitchTier_downto_TableOfReal ((structPitchTier *)OBJECT, GET_INTEGER (L"Unit") - 1))
END

FORM (old_PitchTier_draw, L"PitchTier: Draw", 0)
	praat_dia_timeRange (dia);
	REAL (STRING_FROM_FREQUENCY_HZ, L"0.0")
	POSITIVE (STRING_TO_FREQUENCY_HZ, L"500.0")
	BOOLEAN (L"Garnish", 1)
	OK
DO
	double minimumFrequency = GET_REAL (STRING_FROM_FREQUENCY);
	double maximumFrequency = GET_REAL (STRING_TO_FREQUENCY);
	REQUIRE (maximumFrequency > minimumFrequency,
		L"Maximum frequency must be greater than minimum frequency.")
	EVERY_DRAW (PitchTier_draw ((structPitchTier *)OBJECT, GRAPHICS,
		GET_REAL (L"left Time range"), GET_REAL (L"right Time range"), minimumFrequency, maximumFrequency,
		GET_INTEGER (L"Garnish"), L"lines and speckles"))
END

FORM (PitchTier_draw, L"PitchTier: Draw", 0)
	praat_dia_timeRange (dia);
	REAL (STRING_FROM_FREQUENCY_HZ, L"0.0")
	POSITIVE (STRING_TO_FREQUENCY_HZ, L"500.0")
	BOOLEAN (L"Garnish", 1)
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"ui/editors/AmplitudeTierEditor.h")
	OPTIONMENU (L"Drawing method", 1)
		OPTION (L"lines")
		OPTION (L"speckles")
		OPTION (L"lines and speckles")
	OK
DO_ALTERNATIVE (old_PitchTier_draw)
	double minimumFrequency = GET_REAL (STRING_FROM_FREQUENCY);
	double maximumFrequency = GET_REAL (STRING_TO_FREQUENCY);
	REQUIRE (maximumFrequency > minimumFrequency,
		L"Maximum frequency must be greater than minimum frequency.")
	EVERY_DRAW (PitchTier_draw ((structPitchTier *)OBJECT, GRAPHICS,
		GET_REAL (L"left Time range"), GET_REAL (L"right Time range"), minimumFrequency, maximumFrequency,
		GET_INTEGER (L"Garnish"), GET_STRING (L"Drawing method")))
END

DIRECT (PitchTier_edit)
	if (theCurrentPraatApplication -> batch) {
		return Melder_error1 (L"Cannot edit a PitchTier from batch.");
	} else {
		Sound sound = NULL;
		WHERE (SELECTED)
			if (CLASS == classSound) sound = (structSound *)OBJECT;
		WHERE (SELECTED && CLASS == classPitchTier)
			if (! praat_installEditor (new PitchTierEditor (theCurrentPraatApplication -> topShell, ID_AND_FULL_NAME,
				(structPitchTier *)OBJECT, sound, TRUE), IOBJECT)) return 0;
	}
END

FORM (PitchTier_formula, L"PitchTier: Formula", L"PitchTier: Formula...")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"# ncol = the number of points")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"for col from 1 to ncol")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"   # x = the time of the colth point, in seconds")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"   # self = the value of the colth point, in Hertz")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"   self = `formula'")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"endfor")
	TEXTFIELD (L"formula", L"self * 2 ; one octave up")
	OK
DO
	WHERE_DOWN (SELECTED) {
		RealTier_formula (OBJECT, GET_STRING (L"formula"), interpreter, NULL);
		praat_dataChanged (OBJECT);
		iferror return 0;
	}
END

FORM (PitchTier_getMean_curve, L"PitchTier: Get mean (curve)", L"PitchTier: Get mean (curve)...")
	praat_dia_timeRange (dia);
	OK
DO
	Melder_informationReal (RealTier_getMean_curve (ONLY_OBJECT, GET_REAL (L"left Time range"), GET_REAL (L"right Time range")), L"Hz");
END
	
FORM (PitchTier_getMean_points, L"PitchTier: Get mean (points)", L"PitchTier: Get mean (points)...")
	praat_dia_timeRange (dia);
	OK
DO
	Melder_informationReal (RealTier_getMean_points (ONLY_OBJECT, GET_REAL (L"left Time range"), GET_REAL (L"right Time range")), L"Hz");
END
	
FORM (PitchTier_getStandardDeviation_curve, L"PitchTier: Get standard deviation (curve)", L"PitchTier: Get standard deviation (curve)...")
	praat_dia_timeRange (dia);
	OK
DO
	Melder_informationReal (RealTier_getStandardDeviation_curve (ONLY_OBJECT, GET_REAL (L"left Time range"), GET_REAL (L"right Time range")), L"Hz");
END
	
FORM (PitchTier_getStandardDeviation_points, L"PitchTier: Get standard deviation (points)", L"PitchTier: Get standard deviation (points)...")
	praat_dia_timeRange (dia);
	OK
DO
	Melder_informationReal (RealTier_getStandardDeviation_points (ONLY_OBJECT, GET_REAL (L"left Time range"), GET_REAL (L"right Time range")), L"Hz");
END
	
FORM (PitchTier_getValueAtTime, L"PitchTier: Get value at time", L"PitchTier: Get value at time...")
	REAL (L"Time (s)", L"0.5")
	OK
DO
	Melder_informationReal (RealTier_getValueAtTime (ONLY_OBJECT, GET_REAL (L"Time")), L"Hz");
END
	
FORM (PitchTier_getValueAtIndex, L"PitchTier: Get value at index", L"PitchTier: Get value at index...")
	INTEGER (L"Point number", L"10")
	OK
DO
	Melder_informationReal (RealTier_getValueAtIndex (ONLY_OBJECT, GET_INTEGER (L"Point number")), L"Hz");
END

DIRECT (PitchTier_help) Melder_help (L"PitchTier"); END

DIRECT (PitchTier_hum)
	EVERY_CHECK (PitchTier_hum ((structPitchTier *)OBJECT))
END

FORM (PitchTier_interpolateQuadratically, L"PitchTier: Interpolate quadratically", 0)
	NATURAL (L"Number of points per parabola", L"4")
	RADIO (L"Unit", 2)
	RADIOBUTTON (L"Hz")
	RADIOBUTTON (L"Semitones")
	OK
DO
	WHERE (SELECTED) {
		RealTier_interpolateQuadratically (OBJECT, GET_INTEGER (L"Number of points per parabola"), GET_INTEGER (L"Unit") - 1);
		praat_dataChanged (OBJECT);
	}
END

DIRECT (PitchTier_play)
	EVERY_CHECK (PitchTier_play ((structPitchTier *)OBJECT))
END

DIRECT (PitchTier_playSine)
	EVERY_CHECK (PitchTier_playPart_sine (OBJECT, 0.0, 0.0))
END

FORM (PitchTier_shiftFrequencies, L"PitchTier: Shift frequencies", 0)
	REAL (L"left Time range (s)", L"0.0")
	REAL (L"right Time range (s)", L"1000.0")
	REAL (L"Frequency shift", L"-20.0")
	OPTIONMENU (L"Unit", 1)
		OPTION (L"Hertz")
		OPTION (L"mel")
		OPTION (L"logHertz")
		OPTION (L"semitones")
		OPTION (L"ERB")
	OK
DO
	int unit = GET_INTEGER (L"Unit");
	unit =
		unit == 1 ? kPitch_unit_HERTZ :
		unit == 2 ? kPitch_unit_MEL :
		unit == 3 ? kPitch_unit_LOG_HERTZ :
		unit == 4 ? kPitch_unit_SEMITONES_1 :
		kPitch_unit_ERB;
	WHERE (SELECTED) {
		PitchTier_shiftFrequencies ((structPitchTier *)OBJECT, GET_REAL (L"left Time range"), GET_REAL (L"right Time range"), GET_REAL (L"Frequency shift"), unit);
		iferror return 0;
		praat_dataChanged (OBJECT);
	}
END

FORM (PitchTier_multiplyFrequencies, L"PitchTier: Multiply frequencies", 0)
	REAL (L"left Time range (s)", L"0.0")
	REAL (L"right Time range (s)", L"1000.0")
	POSITIVE (L"Factor", L"1.2")
	OK
DO
	WHERE (SELECTED) {
		PitchTier_multiplyFrequencies ((structPitchTier *)OBJECT,
			GET_REAL (L"left Time range"), GET_REAL (L"right Time range"), GET_REAL (L"Factor"));
		praat_dataChanged (OBJECT);
	}
END

FORM (PitchTier_stylize, L"PitchTier: Stylize", L"PitchTier: Stylize...")
	REAL (L"Frequency resolution", L"4.0")
	RADIO (L"Unit", 2)
	RADIOBUTTON (L"Hz")
	RADIOBUTTON (L"Semitones")
	OK
DO
	WHERE (SELECTED) {
		PitchTier_stylize ((structPitchTier *)OBJECT, GET_REAL (L"Frequency resolution"), GET_INTEGER (L"Unit") - 1);
		praat_dataChanged (OBJECT);
	}
END

DIRECT (PitchTier_to_PointProcess)
	EVERY_TO (PitchTier_to_PointProcess ((structPitchTier *)OBJECT))
END

FORM (PitchTier_to_Sound_phonation, L"PitchTier: To Sound (phonation)", 0)
	POSITIVE (L"Sampling frequency (Hz)", L"44100")
	POSITIVE (L"Adaptation factor", L"1.0")
	POSITIVE (L"Maximum period (s)", L"0.05")
	POSITIVE (L"Open phase", L"0.7")
	REAL (L"Collision phase", L"0.03")
	POSITIVE (L"Power 1", L"3.0")
	POSITIVE (L"Power 2", L"4.0")
	BOOLEAN (L"Hum", 0)
	OK
DO
	EVERY_TO (PitchTier_to_Sound_phonation ((structPitchTier *)OBJECT, GET_REAL (L"Sampling frequency"),
		GET_REAL (L"Adaptation factor"), GET_REAL (L"Maximum period"),
		GET_REAL (L"Open phase"), GET_REAL (L"Collision phase"), GET_REAL (L"Power 1"), GET_REAL (L"Power 2"), GET_INTEGER (L"Hum")))
END

FORM (PitchTier_to_Sound_pulseTrain, L"PitchTier: To Sound (pulse train)", 0)
	POSITIVE (L"Sampling frequency (Hz)", L"44100")
	POSITIVE (L"Adaptation factor", L"1.0")
	POSITIVE (L"Adaptation time", L"0.05")
	NATURAL (L"Interpolation depth (samples)", L"2000")
	BOOLEAN (L"Hum", 0)
	OK
DO
	EVERY_TO (PitchTier_to_Sound_pulseTrain ((structPitchTier *)OBJECT, GET_REAL (L"Sampling frequency"),
		GET_REAL (L"Adaptation factor"), GET_REAL (L"Adaptation time"),
		GET_INTEGER (L"Interpolation depth"), GET_INTEGER (L"Hum")))
END

FORM (PitchTier_to_Sound_sine, L"PitchTier: To Sound (sine)", 0)
	POSITIVE (L"Sampling frequency (Hz)", L"44100")
	OK
DO
	EVERY_TO (PitchTier_to_Sound_sine ((structPitchTier *)OBJECT, 0.0, 0.0, GET_REAL (L"Sampling frequency")))
END

DIRECT (info_PitchTier_Sound_edit)
	Melder_information1 (L"To include a copy of a Sound in your PitchTier editor:\n"
		"   select a PitchTier and a Sound, and click \"View & Edit\".");
END

FORM_WRITE (PitchTier_writeToPitchTierSpreadsheetFile, L"Save PitchTier as spreadsheet", 0, L"PitchTier")
	if (! PitchTier_writeToPitchTierSpreadsheetFile ((structPitchTier *)ONLY_OBJECT, file)) return 0;
END

FORM_WRITE (PitchTier_writeToHeaderlessSpreadsheetFile, L"Save PitchTier as spreadsheet", 0, L"txt")
	if (! PitchTier_writeToHeaderlessSpreadsheetFile ((structPitchTier *)ONLY_OBJECT, file)) return 0;
END

/***** PITCHTIER & POINTPROCESS *****/

DIRECT (PitchTier_PointProcess_to_PitchTier)
	PitchTier pitch = (structPitchTier *)ONLY (classPitchTier);
	if (! praat_new1 (PitchTier_PointProcess_to_PitchTier (pitch, (structPointProcess *)ONLY (classPointProcess)), pitch -> name)) return 0;
END

/***** POINTPROCESS *****/

void PointProcess_draw (PointProcess me, Graphics g, double tmin, double tmax, int garnish) {
	if (tmax <= tmin) { tmin = my xmin; tmax = my xmax; }
	Graphics_setWindow (g, tmin, tmax, -1, 1);
	if (my nt) {
		long imin = PointProcess_getHighIndex (me, tmin), imax = PointProcess_getLowIndex (me, tmax), i;
		int lineType = Graphics_inqLineType (g);
		Graphics_setLineType (g, Graphics_DOTTED);
		Graphics_setInner (g);
		for (i = imin; i <= imax; i ++) {
			Graphics_line (g, my t [i], -1, my t [i], 1);
		}
		Graphics_setLineType (g, lineType);
		Graphics_unsetInner (g);
	}
	if (garnish) {
		Graphics_drawInnerBox (g);
		Graphics_textBottom (g, 1, L"Time (s)");
		Graphics_marksBottom (g, 2, 1, 1, 0);
	}
}

FORM (PointProcess_addPoint, L"PointProcess: Add point", L"PointProcess: Add point...")
	REAL (L"Time (s)", L"0.5")
	OK
DO
	WHERE (SELECTED) {
		if (! PointProcess_addPoint ((structPointProcess *)OBJECT, GET_REAL (L"Time"))) return 0;
		praat_dataChanged (OBJECT);
	}
END

FORM (PointProcess_createEmpty, L"Create an empty PointProcess", L"Create empty PointProcess...")
	WORD (L"Name", L"empty")
	REAL (L"Start time (s)", L"0.0")
	REAL (L"End time (s)", L"1.0")
	OK
DO
	double tmin = GET_REAL (L"Start time"), tmax = GET_REAL (L"End time");
	if (tmax < tmin)
		return Melder_error5 (L"End time (", Melder_single (tmax), L") should not be less than start time (", Melder_single (tmin), L").");
	if (! praat_new1 (PointProcess_create (tmin, tmax, 0), GET_STRING (L"Name"))) return 0;
END

FORM (PointProcess_createPoissonProcess, L"Create Poisson process", L"Create Poisson process...")
	WORD (L"Name", L"poisson")
	REAL (L"Start time (s)", L"0.0")
	REAL (L"End time (s)", L"1.0")
	POSITIVE (L"Density (/s)", L"100.0")
	OK
DO
	double tmin = GET_REAL (L"Start time"), tmax = GET_REAL (L"End time");
	if (tmax < tmin)
		return Melder_error5 (L"End time (", Melder_single (tmax), L") should not be less than start time (", Melder_single (tmin), L").");
	if (! praat_new1 (PointProcess_createPoissonProcess (tmin, tmax, GET_REAL (L"Density")), GET_STRING (L"Name"))) return 0;
END

DIRECT (PointProcess_difference)
	PointProcess point1 = NULL, point2 = NULL;
	WHERE (SELECTED) { if (point1) point2 = (structPointProcess *)OBJECT; else point1 = (structPointProcess *)OBJECT; }
	if (! praat_new1 (PointProcesses_difference (point1, point2), L"difference")) return 0;
END

FORM (PointProcess_draw, L"PointProcess: Draw", 0)
	praat_dia_timeRange (dia);
	BOOLEAN (L"Garnish", 1)
	OK
DO
	EVERY_DRAW (PointProcess_draw ((structPointProcess *)OBJECT, GRAPHICS,
		GET_REAL (L"left Time range"), GET_REAL (L"right Time range"), GET_INTEGER (L"Garnish")))
END

DIRECT (PointProcess_edit)
	if (theCurrentPraatApplication -> batch) {
		return Melder_error1 (L"Cannot edit a PointProcess from batch.");
	} else {
		Sound sound = NULL;
		WHERE (SELECTED)
			if (CLASS == classSound) sound = (structSound *)OBJECT;
		WHERE (SELECTED && CLASS == classPointProcess)
			if (! praat_installEditor (new PointEditor (theCurrentPraatApplication -> topShell, ID_AND_FULL_NAME,
				(structPointProcess *)OBJECT, sound), IOBJECT)) return 0;
	}
END

FORM (PointProcess_fill, L"PointProcess: Fill", 0)
	praat_dia_timeRange (dia);
	POSITIVE (L"Period (s)", L"0.01")
	OK
DO
	WHERE (SELECTED) {
		int status = PointProcess_fill ((structPointProcess *)OBJECT, GET_REAL (L"left Time range"), GET_REAL (L"right Time range"), GET_REAL (L"Period"));
		praat_dataChanged (OBJECT);
		if (! status) return 0;
	}
END

FORM (PointProcess_getInterval, L"PointProcess: Get interval", L"PointProcess: Get interval...")
	REAL (L"Time (s)", L"0.5")
	OK
DO
	Melder_informationReal (PointProcess_getInterval ((structPointProcess *)ONLY (classPointProcess), GET_REAL (L"Time")), L"seconds");
END

static void dia_PointProcess_getRangeProperty (UiForm *dia) {
	praat_dia_timeRange (dia);
	REAL (L"Shortest period (s)", L"0.0001")
	REAL (L"Longest period (s)", L"0.02")
	POSITIVE (L"Maximum period factor", L"1.3")
}

FORM (PointProcess_getJitter_local, L"PointProcess: Get jitter (local)", L"PointProcess: Get jitter (local)...")
	dia_PointProcess_getRangeProperty (dia);
	OK
DO
	Melder_informationReal (PointProcess_getJitter_local ((structPointProcess *)ONLY (classPointProcess),
		GET_REAL (L"left Time range"), GET_REAL (L"right Time range"),
		GET_REAL (L"Shortest period"), GET_REAL (L"Longest period"), GET_REAL (L"Maximum period factor")), NULL);
END

FORM (PointProcess_getJitter_local_absolute, L"PointProcess: Get jitter (local, absolute)", L"PointProcess: Get jitter (local, absolute)...")
	dia_PointProcess_getRangeProperty (dia);
	OK
DO
	Melder_informationReal (PointProcess_getJitter_local_absolute ((structPointProcess *)ONLY (classPointProcess),
		GET_REAL (L"left Time range"), GET_REAL (L"right Time range"),
		GET_REAL (L"Shortest period"), GET_REAL (L"Longest period"), GET_REAL (L"Maximum period factor")), L"seconds");
END

FORM (PointProcess_getJitter_rap, L"PointProcess: Get jitter (rap)", L"PointProcess: Get jitter (rap)...")
	dia_PointProcess_getRangeProperty (dia);
	OK
DO
	Melder_informationReal (PointProcess_getJitter_rap ((structPointProcess *)ONLY (classPointProcess),
		GET_REAL (L"left Time range"), GET_REAL (L"right Time range"),
		GET_REAL (L"Shortest period"), GET_REAL (L"Longest period"), GET_REAL (L"Maximum period factor")), NULL);
END

FORM (PointProcess_getJitter_ppq5, L"PointProcess: Get jitter (ppq5)", L"PointProcess: Get jitter (ppq5)...")
	dia_PointProcess_getRangeProperty (dia);
	OK
DO
	Melder_informationReal (PointProcess_getJitter_ppq5 ((structPointProcess *)ONLY (classPointProcess),
		GET_REAL (L"left Time range"), GET_REAL (L"right Time range"),
		GET_REAL (L"Shortest period"), GET_REAL (L"Longest period"), GET_REAL (L"Maximum period factor")), NULL);
END

FORM (PointProcess_getJitter_ddp, L"PointProcess: Get jitter (ddp)", L"PointProcess: Get jitter (ddp)...")
	dia_PointProcess_getRangeProperty (dia);
	OK
DO
	Melder_informationReal (PointProcess_getJitter_ddp ((structPointProcess *)ONLY (classPointProcess),
		GET_REAL (L"left Time range"), GET_REAL (L"right Time range"),
		GET_REAL (L"Shortest period"), GET_REAL (L"Longest period"), GET_REAL (L"Maximum period factor")), NULL);
END

FORM (PointProcess_getMeanPeriod, L"PointProcess: Get mean period", L"PointProcess: Get mean period...")
	dia_PointProcess_getRangeProperty (dia);
	OK
DO
	Melder_informationReal (PointProcess_getMeanPeriod ((structPointProcess *)ONLY (classPointProcess),
		GET_REAL (L"left Time range"), GET_REAL (L"right Time range"),
		GET_REAL (L"Shortest period"), GET_REAL (L"Longest period"), GET_REAL (L"Maximum period factor")), L"seconds");
END

FORM (PointProcess_getStdevPeriod, L"PointProcess: Get stdev period", L"PointProcess: Get stdev period...")
	dia_PointProcess_getRangeProperty (dia);
	OK
DO
	Melder_informationReal (PointProcess_getStdevPeriod ((structPointProcess *)ONLY (classPointProcess),
		GET_REAL (L"left Time range"), GET_REAL (L"right Time range"),
		GET_REAL (L"Shortest period"), GET_REAL (L"Longest period"), GET_REAL (L"Maximum period factor")), L"seconds");
END

FORM (PointProcess_getLowIndex, L"PointProcess: Get low index", L"PointProcess: Get low index...")
	REAL (L"Time (s)", L"0.5")
	OK
DO
	Melder_information1 (Melder_integer (PointProcess_getLowIndex ((structPointProcess *)ONLY_OBJECT, GET_REAL (L"Time"))));
END

FORM (PointProcess_getHighIndex, L"PointProcess: Get high index", L"PointProcess: Get high index...")
	REAL (L"Time (s)", L"0.5")
	OK
DO
	Melder_information1 (Melder_integer (PointProcess_getHighIndex ((structPointProcess *)ONLY_OBJECT, GET_REAL (L"Time"))));
END

FORM (PointProcess_getNearestIndex, L"PointProcess: Get nearest index", L"PointProcess: Get nearest index...")
	REAL (L"Time (s)", L"0.5")
	OK
DO
	Melder_information1 (Melder_integer (PointProcess_getNearestIndex ((structPointProcess *)ONLY_OBJECT, GET_REAL (L"Time"))));
END

DIRECT (PointProcess_getNumberOfPoints)
	PointProcess me = (structPointProcess *)ONLY_OBJECT;
	Melder_information1 (Melder_integer (my nt));
END

FORM (PointProcess_getNumberOfPeriods, L"PointProcess: Get number of periods", L"PointProcess: Get number of periods...")
	dia_PointProcess_getRangeProperty (dia);
	OK
DO
	Melder_information1 (Melder_integer (PointProcess_getNumberOfPeriods ((structPointProcess *)ONLY (classPointProcess),
		GET_REAL (L"left Time range"), GET_REAL (L"right Time range"),
		GET_REAL (L"Shortest period"), GET_REAL (L"Longest period"), GET_REAL (L"Maximum period factor"))));
END

FORM (PointProcess_getTimeFromIndex, L"Get time", 0 /*"PointProcess: Get time from index..."*/)
	NATURAL (L"Point number", L"10")
	OK
DO
	PointProcess me = (structPointProcess *)ONLY_OBJECT;
	long i = GET_INTEGER (L"Point number");
	if (i > my nt) Melder_information1 (L"--undefined--");
	else Melder_informationReal (my t [i], L"seconds");
END

DIRECT (PointProcess_help) Melder_help (L"PointProcess"); END

DIRECT (PointProcess_hum)
	EVERY_CHECK (PointProcess_hum ((structPointProcess *)OBJECT,
		((PointProcess) OBJECT) -> xmin, ((PointProcess) OBJECT) -> xmax))
END

DIRECT (PointProcess_intersection)
	PointProcess point1 = NULL, point2 = NULL;
	WHERE (SELECTED) { if (point1) point2 = (structPointProcess *)OBJECT; else point1 = (structPointProcess *)OBJECT; }
	if (! praat_new1 (PointProcesses_intersection (point1, point2), L"intersection")) return 0;
END

DIRECT (PointProcess_play)
	EVERY_CHECK (PointProcess_play ((structPointProcess *)OBJECT))
END

FORM (PointProcess_removePoint, L"PointProcess: Remove point", L"PointProcess: Remove point...")
	NATURAL (L"Index", L"1")
	OK
DO
	WHERE (SELECTED) {
		PointProcess_removePoint ((structPointProcess *)OBJECT, GET_INTEGER (L"Index"));
		praat_dataChanged (OBJECT);
	}
END

FORM (PointProcess_removePointNear, L"PointProcess: Remove point near", L"PointProcess: Remove point near...")
	REAL (L"Time (s)", L"0.5")
	OK
DO
	WHERE (SELECTED) {
		PointProcess_removePointNear ((structPointProcess *)OBJECT, GET_REAL (L"Time"));
		praat_dataChanged (OBJECT);
	}
END

FORM (PointProcess_removePoints, L"PointProcess: Remove points", L"PointProcess: Remove points...")
	NATURAL (L"From index", L"1")
	NATURAL (L"To index", L"10")
	OK
DO
	WHERE (SELECTED) {
		PointProcess_removePoints ((structPointProcess *)OBJECT, GET_INTEGER (L"From index"), GET_INTEGER (L"To index"));
		praat_dataChanged (OBJECT);
	}
END

FORM (PointProcess_removePointsBetween, L"PointProcess: Remove points between", L"PointProcess: Remove points between...")
	REAL (L"left Time range (s)", L"0.3")
	REAL (L"right Time range (s)", L"0.7")
	OK
DO
	WHERE (SELECTED) {
		PointProcess_removePointsBetween ((structPointProcess *)OBJECT, GET_REAL (L"left Time range"), GET_REAL (L"right Time range"));
		praat_dataChanged (OBJECT);
	}
END

DIRECT (PointProcess_to_IntervalTier)
	EVERY_TO (IntervalTier_create (((PointProcess) OBJECT) -> xmin, ((PointProcess) OBJECT) -> xmax))
END

DIRECT (PointProcess_to_Matrix)
	EVERY_TO (PointProcess_to_Matrix ((structPointProcess *)OBJECT))
END

FORM (PointProcess_to_PitchTier, L"PointProcess: To PitchTier", L"PointProcess: To PitchTier...")
	POSITIVE (L"Maximum interval (s)", L"0.02")
	OK
DO
	EVERY_TO (PointProcess_to_PitchTier ((structPointProcess *)OBJECT, GET_REAL (L"Maximum interval")))
END

FORM (PointProcess_to_TextGrid, L"PointProcess: To TextGrid...", L"PointProcess: To TextGrid...")
	SENTENCE (L"Tier names", L"Mary John bell")
	SENTENCE (L"Point tiers", L"bell")
	OK
DO
	EVERY_TO (TextGrid_create (((PointProcess) OBJECT) -> xmin, ((PointProcess) OBJECT) -> xmax,
		GET_STRING (L"Tier names"), GET_STRING (L"Point tiers")))
END

FORM (PointProcess_to_TextGrid_vuv, L"PointProcess: To TextGrid (vuv)...", L"PointProcess: To TextGrid (vuv)...")
	POSITIVE (L"Maximum period (s)", L"0.02")
	REAL (L"Mean period (s)", L"0.01")
	OK
DO
	EVERY_TO (PointProcess_to_TextGrid_vuv ((structPointProcess *)OBJECT, GET_REAL (L"Maximum period"),
		GET_REAL (L"Mean period")))
END

DIRECT (PointProcess_to_TextTier)
	EVERY_TO (TextTier_create (((PointProcess) OBJECT) -> xmin, ((PointProcess) OBJECT) -> xmax))
END

FORM (PointProcess_to_Sound_phonation, L"PointProcess: To Sound (phonation)", L"PointProcess: To Sound (phonation)...")
	POSITIVE (L"Sampling frequency (Hz)", L"44100")
	POSITIVE (L"Adaptation factor", L"1.0")
	POSITIVE (L"Maximum period (s)", L"0.05")
	POSITIVE (L"Open phase", L"0.7")
	REAL (L"Collision phase", L"0.03")
	POSITIVE (L"Power 1", L"3.0")
	POSITIVE (L"Power 2", L"4.0")
	OK
DO
	EVERY_TO (PointProcess_to_Sound_phonation ((structPointProcess *)OBJECT, GET_REAL (L"Sampling frequency"),
		GET_REAL (L"Adaptation factor"), GET_REAL (L"Maximum period"),
		GET_REAL (L"Open phase"), GET_REAL (L"Collision phase"), GET_REAL (L"Power 1"), GET_REAL (L"Power 2")))
END

FORM (PointProcess_to_Sound_pulseTrain, L"PointProcess: To Sound (pulse train)", L"PointProcess: To Sound (pulse train)...")
	POSITIVE (L"Sampling frequency (Hz)", L"44100")
	POSITIVE (L"Adaptation factor", L"1.0")
	POSITIVE (L"Adaptation time (s)", L"0.05")
	NATURAL (L"Interpolation depth (samples)", L"2000")
	OK
DO
	EVERY_TO (PointProcess_to_Sound_pulseTrain ((structPointProcess *)OBJECT, GET_REAL (L"Sampling frequency"),
		GET_REAL (L"Adaptation factor"), GET_REAL (L"Adaptation time"),
		GET_INTEGER (L"Interpolation depth")))
END

DIRECT (PointProcess_to_Sound_hum)
	EVERY_TO (PointProcess_to_Sound_hum ((structPointProcess *)OBJECT))
END

DIRECT (PointProcess_union)
	PointProcess point1 = NULL, point2 = NULL;
	WHERE (SELECTED) { if (point1) point2 = (structPointProcess *)OBJECT; else point1 = (structPointProcess *)OBJECT; }
	if (! praat_new1 (PointProcesses_union (point1, point2), L"union")) return 0;
END

FORM (PointProcess_upto_IntensityTier, L"PointProcess: Up to IntensityTier", L"PointProcess: Up to IntensityTier...")
	POSITIVE (L"Intensity (dB)", L"70.0")
	OK
DO
	EVERY_TO (PointProcess_upto_IntensityTier ((structPointProcess *)OBJECT, GET_REAL (L"Intensity")))
END

FORM (PointProcess_upto_PitchTier, L"PointProcess: Up to PitchTier", L"PointProcess: Up to PitchTier...")
	POSITIVE (L"Frequency (Hz)", L"190.0")
	OK
DO
	EVERY_TO (PointProcess_upto_PitchTier ((structPointProcess *)OBJECT, GET_REAL (L"Frequency")))
END

FORM (PointProcess_upto_TextTier, L"PointProcess: Up to TextTier", L"PointProcess: Up to TextTier...")
	SENTENCE (L"Text", L"ui/editors/AmplitudeTierEditor.h")
	OK
DO
	EVERY_TO (PointProcess_upto_TextTier ((structPointProcess *)OBJECT, GET_STRING (L"Text")))
END

FORM (PointProcess_voice, L"PointProcess: Fill unvoiced parts", 0)
	POSITIVE (L"Period (s)", L"0.01")
	POSITIVE (L"Maximum voiced period (s)", L"0.02000000001")
	OK
DO
	WHERE (SELECTED) {
		int status = PointProcess_voice ((structPointProcess *)OBJECT, GET_REAL (L"Period"), GET_REAL (L"Maximum voiced period"));
		praat_dataChanged (OBJECT);
		if (! status) return 0;
	}
END

DIRECT (info_PointProcess_Sound_edit)
	Melder_information1 (L"To include a copy of a Sound in your PointProcess editor:\n"
		"   select a PointProcess and a Sound, and click \"View & Edit\".");
END

/***** POINTPROCESS & SOUND *****/

/*DIRECT (Sound_PointProcess_to_Manipulation)
	Sound sound = ONLY (classSound);
	PointProcess point = ONLY (classPointProcess);
	if (! praat_new1 (Sound_PointProcess_to_Manipulation (sound, point), point -> name)) return 0;
END*/

DIRECT (Point_Sound_transplantDomain)
	PointProcess point = (structPointProcess *)ONLY (classPointProcess);
	Sound sound = (structSound *)ONLY (classSound);
	point -> xmin = sound -> xmin;
	point -> xmax = sound -> xmax;
	praat_dataChanged (point);
END

FORM (Point_Sound_getShimmer_local, L"PointProcess & Sound: Get shimmer (local)", L"PointProcess & Sound: Get shimmer (local)...")
	dia_PointProcess_getRangeProperty (dia);
	POSITIVE (L"Maximum amplitude factor", L"1.6")
	OK
DO
	Melder_informationReal (PointProcess_Sound_getShimmer_local ((structPointProcess *)ONLY (classPointProcess), (structSound *)ONLY (classSound),
		GET_REAL (L"left Time range"), GET_REAL (L"right Time range"),
		GET_REAL (L"Shortest period"), GET_REAL (L"Longest period"),
		GET_REAL (L"Maximum period factor"), GET_REAL (L"Maximum amplitude factor")), NULL);
END

FORM (Point_Sound_getShimmer_local_dB, L"PointProcess & Sound: Get shimmer (local, dB)", L"PointProcess & Sound: Get shimmer (local, dB)...")
	dia_PointProcess_getRangeProperty (dia);
	POSITIVE (L"Maximum amplitude factor", L"1.6")
	OK
DO
	Melder_informationReal (PointProcess_Sound_getShimmer_local_dB ((structPointProcess *)ONLY (classPointProcess), (structSound *)ONLY (classSound),
		GET_REAL (L"left Time range"), GET_REAL (L"right Time range"),
		GET_REAL (L"Shortest period"), GET_REAL (L"Longest period"),
		GET_REAL (L"Maximum period factor"), GET_REAL (L"Maximum amplitude factor")), NULL);
END

FORM (Point_Sound_getShimmer_apq3, L"PointProcess & Sound: Get shimmer (apq3)", L"PointProcess & Sound: Get shimmer (apq3)...")
	dia_PointProcess_getRangeProperty (dia);
	POSITIVE (L"Maximum amplitude factor", L"1.6")
	OK
DO
	Melder_informationReal (PointProcess_Sound_getShimmer_apq3 ((structPointProcess *)ONLY (classPointProcess), (structSound *)ONLY (classSound),
		GET_REAL (L"left Time range"), GET_REAL (L"right Time range"),
		GET_REAL (L"Shortest period"), GET_REAL (L"Longest period"),
		GET_REAL (L"Maximum period factor"), GET_REAL (L"Maximum amplitude factor")), NULL);
END

FORM (Point_Sound_getShimmer_apq5, L"PointProcess & Sound: Get shimmer (apq)", L"PointProcess & Sound: Get shimmer (apq5)...")
	dia_PointProcess_getRangeProperty (dia);
	POSITIVE (L"Maximum amplitude factor", L"1.6")
	OK
DO
	Melder_informationReal (PointProcess_Sound_getShimmer_apq5 ((structPointProcess *)ONLY (classPointProcess), (structSound *)ONLY (classSound),
		GET_REAL (L"left Time range"), GET_REAL (L"right Time range"),
		GET_REAL (L"Shortest period"), GET_REAL (L"Longest period"),
		GET_REAL (L"Maximum period factor"), GET_REAL (L"Maximum amplitude factor")), NULL);
END

FORM (Point_Sound_getShimmer_apq11, L"PointProcess & Sound: Get shimmer (apq11)", L"PointProcess & Sound: Get shimmer (apq11)...")
	dia_PointProcess_getRangeProperty (dia);
	POSITIVE (L"Maximum amplitude factor", L"1.6")
	OK
DO
	Melder_informationReal (PointProcess_Sound_getShimmer_apq11 ((structPointProcess *)ONLY (classPointProcess), (structSound *)ONLY (classSound),
		GET_REAL (L"left Time range"), GET_REAL (L"right Time range"),
		GET_REAL (L"Shortest period"), GET_REAL (L"Longest period"),
		GET_REAL (L"Maximum period factor"), GET_REAL (L"Maximum amplitude factor")), NULL);
END

FORM (Point_Sound_getShimmer_dda, L"PointProcess & Sound: Get shimmer (dda)", L"PointProcess & Sound: Get shimmer (dda)...")
	dia_PointProcess_getRangeProperty (dia);
	POSITIVE (L"Maximum amplitude factor", L"1.6")
	OK
DO
	Melder_informationReal (PointProcess_Sound_getShimmer_dda ((structPointProcess *)ONLY (classPointProcess), (structSound *)ONLY (classSound),
		GET_REAL (L"left Time range"), GET_REAL (L"right Time range"),
		GET_REAL (L"Shortest period"), GET_REAL (L"Longest period"),
		GET_REAL (L"Maximum period factor"), GET_REAL (L"Maximum amplitude factor")), NULL);
END

FORM (PointProcess_Sound_to_AmplitudeTier_period, L"PointProcess & Sound: To AmplitudeTier (period)", 0)
	dia_PointProcess_getRangeProperty (dia);
	OK
DO
	PointProcess point = (structPointProcess *)ONLY (classPointProcess);
	Sound sound = (structSound *)ONLY (classSound);
	if (! praat_new3 (PointProcess_Sound_to_AmplitudeTier_period (point, sound,
		GET_REAL (L"left Time range"), GET_REAL (L"right Time range"),
		GET_REAL (L"Shortest period"), GET_REAL (L"Longest period"), GET_REAL (L"Maximum period factor")), sound -> name, L"_", point -> name)) return 0;
END

DIRECT (PointProcess_Sound_to_AmplitudeTier_point)
	PointProcess point = (structPointProcess *)ONLY (classPointProcess);
	Sound sound = (structSound *)ONLY (classSound);
	if (! praat_new3 (PointProcess_Sound_to_AmplitudeTier_point (point, sound), sound -> name, L"_", point -> name)) return 0;
END

FORM (PointProcess_Sound_to_Ltas, L"PointProcess & Sound: To Ltas", 0)
	POSITIVE (L"Maximum frequency (Hz)", L"5000")
	POSITIVE (L"Band width (Hz)", L"100")
	REAL (L"Shortest period (s)", L"0.0001")
	REAL (L"Longest period (s)", L"0.02")
	POSITIVE (L"Maximum period factor", L"1.3")
	OK
DO
	PointProcess point = (structPointProcess *)ONLY (classPointProcess);
	Sound sound = (structSound *)ONLY (classSound);
	if (! praat_new1 (PointProcess_Sound_to_Ltas (point, sound,
		GET_REAL (L"Maximum frequency"), GET_REAL (L"Band width"),
		GET_REAL (L"Shortest period"), GET_REAL (L"Longest period"), GET_REAL (L"Maximum period factor")), sound -> name)) return 0;
END

FORM (PointProcess_Sound_to_Ltas_harmonics, L"PointProcess & Sound: To Ltas (harmonics", 0)
	NATURAL (L"Maximum harmonic", L"20")
	REAL (L"Shortest period (s)", L"0.0001")
	REAL (L"Longest period (s)", L"0.02")
	POSITIVE (L"Maximum period factor", L"1.3")
	OK
DO
	PointProcess point = (structPointProcess *)ONLY (classPointProcess);
	Sound sound = (structSound *)ONLY (classSound);
	if (! praat_new1 (PointProcess_Sound_to_Ltas_harmonics (point, sound,
		GET_INTEGER (L"Maximum harmonic"),
		GET_REAL (L"Shortest period"), GET_REAL (L"Longest period"), GET_REAL (L"Maximum period factor")), sound -> name)) return 0;
END

FORM (Sound_PointProcess_to_SoundEnsemble_correlate, L"Sound & PointProcess: To SoundEnsemble (correlate)", 0)
	REAL (L"From time (s)", L"-0.1")
	REAL (L"To time (s)", L"1.0")
	OK
DO
	Sound sound = (structSound *)ONLY (classSound);
	PointProcess point = (structPointProcess *)ONLY (classPointProcess);
	if (! praat_new1 (Sound_PointProcess_to_SoundEnsemble_correlate (sound, point, GET_REAL (L"From time"), GET_REAL (L"To time")), point -> name)) return 0;
END

/***** POLYGON *****/

static void setWindow (Polygon me, Any graphics,
	double xmin, double xmax, double ymin, double ymax)
{
	Melder_assert (me);

	if (xmax <= xmin)   /* Autoscaling along x axis. */
	{
		xmax = xmin = my x [1];
		for (long i = 2; i <= my numberOfPoints; i ++)
		{
			if (my x [i] < xmin) xmin = my x [i];
			if (my x [i] > xmax) xmax = my x [i];
		}
		if (xmin == xmax)
		{
			xmin -= 1.0;
			xmax += 1.0;
		}
	}
	if (ymax <= ymin)   /* Autoscaling along y axis. */
	{
		ymax = ymin = my y [1];
		for (long i = 2; i <= my numberOfPoints; i ++)
		{
			if (my y [i] < ymin) ymin = my y [i];
			if (my y [i] > ymax) ymax = my y [i];
		}
		if (ymin == ymax)
		{
			ymin -= 1.0;
			ymax += 1.0;
		}
	}
	Graphics_setWindow (graphics, xmin, xmax, ymin, ymax);
}

void Polygon_drawMarks (I, Graphics g, double xmin, double xmax,
	double ymin, double ymax, double size_mm, const wchar_t *mark)
{
	iam (Polygon);

	Graphics_setInner (g);
	setWindow (me, g, xmin, xmax, ymin, ymax);
	for (long i = 1; i <= my numberOfPoints; i++)
	{
		Graphics_mark (g, my x[i], my y[i], size_mm, mark);
	}
	Graphics_unsetInner (g);
}

void Polygon_draw (I, Graphics g, double xmin, double xmax, double ymin, double ymax) {
	iam (Polygon);
	Graphics_setInner (g);
	setWindow (me, g, xmin, xmax, ymin, ymax);
	Graphics_polyline (g, my numberOfPoints, & my x [1], & my y [1]);
	Graphics_unsetInner (g);
}

void Polygon_paint (I, Graphics g, Graphics_Colour colour, double xmin, double xmax, double ymin, double ymax) {
	iam (Polygon);
	Graphics_setInner (g);
	setWindow (me, g, xmin, xmax, ymin, ymax);
	Graphics_setColour (g, colour);
	Graphics_fillArea (g, my numberOfPoints, & my x [1], & my y [1]);
	Graphics_unsetInner (g);
}

void Polygon_drawCircles (I, Graphics g,
	double xmin, double xmax, double ymin, double ymax, double diameter_mm)
{
	iam (Polygon);
	Graphics_setInner (g);
	setWindow (me, g, xmin, xmax, ymin, ymax);
	for (long i = 1; i <= my numberOfPoints; i ++)
		Graphics_circle_mm (g, my x [i], my y [i], diameter_mm);
	Graphics_unsetInner (g);
}

void Polygon_paintCircles (I, Graphics g,
	double xmin, double xmax, double ymin, double ymax, double diameter)
{
	iam (Polygon);
	Graphics_setInner (g);
	setWindow (me, g, xmin, xmax, ymin, ymax);
	for (long i = 1; i <= my numberOfPoints; i ++)
		Graphics_fillCircle_mm (g, my x [i], my y [i], diameter);
	Graphics_unsetInner (g);
}

void Polygons_drawConnection (I, thou, Graphics g,
	double xmin, double xmax, double ymin, double ymax, int hasArrow, double relativeLength)
{
	iam (Polygon); thouart (Polygon);
	double w2 = 0.5 * (1 - relativeLength), w1 = 1 - w2;
	long n = my numberOfPoints;
	if (thy numberOfPoints < n) n = thy numberOfPoints;
	Graphics_setInner (g);
	setWindow (me, g, xmin, xmax, ymin, ymax);
	for (long i = 1; i <= n; i ++) {
		double x1 = my x [i], x2 = thy x [i], y1 = my y [i], y2 = thy y [i];
		double dummy = w1 * x1 + w2 * x2;
		x2 = w1 * x2 + w2 * x1;
		x1 = dummy;
		dummy = w1 * y1 + w2 * y2;
		y2 = w1 * y2 + w2 * y1;
		y1 = dummy;
		if (hasArrow)
			Graphics_arrow (g, x1, y1, x2, y2);
		else
			Graphics_line (g, x1, y1, x2, y2);
	}
	Graphics_unsetInner (g);
}

FORM (Polygon_draw, L"Polygon: Draw", 0)
	REAL (L"Xmin", L"0.0")
	REAL (L"Xmax", L"0.0")
	REAL (L"Ymin", L"0.0")
	REAL (L"Ymax", L"0.0")
	OK
DO
	EVERY_DRAW (Polygon_draw ((structPolygon *)OBJECT, GRAPHICS, GET_REAL (L"Xmin"), GET_REAL (L"Xmax"),
		GET_REAL (L"Ymin"), GET_REAL (L"Ymax")))
END

FORM (Polygon_drawCircles, L"Polygon: Draw circles", 0)
	REAL (L"Xmin", L"0.0")
	REAL (L"Xmax", L"0.0 (= all)")
	REAL (L"Ymin", L"0.0")
	REAL (L"Ymax", L"0.0 (= all)")
	POSITIVE (L"Diameter (mm)", L"3")
	OK
DO
	EVERY_DRAW (Polygon_drawCircles ((structPolygon *)OBJECT, GRAPHICS,
		GET_REAL (L"Xmin"), GET_REAL (L"Xmax"), GET_REAL (L"Ymin"), GET_REAL (L"Ymax"),
		GET_REAL (L"Diameter")))
END

FORM (Polygons_drawConnection, L"Polygons: Draw connection", 0)
	REAL (L"Xmin", L"0.0")
	REAL (L"Xmax", L"0.0 (= all)")
	REAL (L"Ymin", L"0.0")
	REAL (L"Ymax", L"0.0 (= all)")
	BOOLEAN (L"Arrow", 0)
	POSITIVE (L"Relative length", L"0.9")
	OK
DO
	Polygon polygon1 = NULL, polygon2 = NULL;
	WHERE (SELECTED) { if (polygon1) polygon2 = (structPolygon *)OBJECT; else polygon1 = (structPolygon *)OBJECT; }
	EVERY_DRAW (Polygons_drawConnection (polygon1, polygon2, GRAPHICS,
		GET_REAL (L"Xmin"), GET_REAL (L"Xmax"), GET_REAL (L"Ymin"), GET_REAL (L"Ymax"),
		GET_INTEGER (L"Arrow"), GET_REAL (L"Relative length")))
END

DIRECT (Polygon_help) Melder_help (L"Polygon"); END

FORM (Polygon_paint, L"Polygon: Paint", 0)
	COLOUR (L"Colour (0-1, name, or {r,g,b})", L"0.5")
	REAL (L"Xmin", L"0.0")
	REAL (L"Xmax", L"0.0 (= all)")
	REAL (L"Ymin", L"0.0")
	REAL (L"Ymax", L"0.0 (= all)")
	OK
DO
	EVERY_DRAW (Polygon_paint ((structPolygon *)OBJECT, GRAPHICS, GET_COLOUR (L"Colour"),
		GET_REAL (L"Xmin"), GET_REAL (L"Xmax"), GET_REAL (L"Ymin"), GET_REAL (L"Ymax")))
END

FORM (Polygon_paintCircles, L"Polygon: Paint circles", 0)
	REAL (L"Xmin", L"0.0")
	REAL (L"Xmax", L"0.0 (= all)")
	REAL (L"Ymin", L"0.0")
	REAL (L"Ymax", L"0.0 (= all)")
	POSITIVE (L"Diameter (mm)", L"3")
	OK
DO
	EVERY_DRAW (Polygon_paintCircles ((structPolygon *)OBJECT, GRAPHICS,
		GET_REAL (L"Xmin"), GET_REAL (L"Xmax"), GET_REAL (L"Ymin"), GET_REAL (L"Ymax"),
		GET_REAL (L"Diameter")))
END

DIRECT (Polygon_randomize)
	EVERY (Polygon_randomize (OBJECT))
END

FORM (Polygon_salesperson, L"Polygon: Find shortest path", 0)
	NATURAL (L"Number of iterations", L"1")
	OK
DO
	EVERY (Polygon_salesperson (OBJECT, GET_INTEGER (L"Number of iterations")))
END

DIRECT (Polygon_to_Matrix)
	EVERY_TO (Polygon_to_Matrix (OBJECT))
END

/***** SOUND & PITCH & POINTPROCESS *****/

FORM (Sound_Pitch_PointProcess_voiceReport, L"Voice report", L"Voice")
	praat_dia_timeRange (dia);
	POSITIVE (L"left Pitch range (Hz)", L"75.0")
	POSITIVE (L"right Pitch range (Hz)", L"600.0")
	POSITIVE (L"Maximum period factor", L"1.3")
	POSITIVE (L"Maximum amplitude factor", L"1.6")
	REAL (L"Silence threshold", L"0.03")
	REAL (L"Voicing threshold", L"0.45")
	OK
DO
	MelderInfo_open ();
	Sound_Pitch_PointProcess_voiceReport ((structSound *)ONLY (classSound), (structPitch *)ONLY (classPitch), (structPointProcess *)ONLY (classPointProcess),
		GET_REAL (L"left Time range"), GET_REAL (L"right Time range"),
		GET_REAL (L"left Pitch range"), GET_REAL (L"right Pitch range"),
		GET_REAL (L"Maximum period factor"), GET_REAL (L"Maximum amplitude factor"),
		GET_REAL (L"Silence threshold"), GET_REAL (L"Voicing threshold"));
	MelderInfo_close ();
END

/***** SOUND & POINTPROCESS & PITCHTIER & DURATIONTIER *****/

FORM (Sound_Point_Pitch_Duration_to_Sound, L"To Sound", 0)
	POSITIVE (L"Longest period (s)", L"0.02")
	OK
DO
	if (! praat_new1 (Sound_Point_Pitch_Duration_to_Sound ((structSound *)ONLY (classSound), (structPointProcess *)ONLY (classPointProcess),
		(structPitchTier *)ONLY (classPitchTier), (structDurationTier *)ONLY (classDurationTier), GET_REAL (L"Longest period")), L"manip")) return 0;
END

/***** SPECTROGRAM *****/

void Spectrogram_paintInside (I, Graphics g, double tmin, double tmax, double fmin, double fmax,
	double maximum, int autoscaling, double dynamic, double preemphasis, double dynamicCompression)
{
	iam (Spectrogram);
	long itmin, itmax, ifmin, ifmax, ifreq, itime;
	double *preemphasisFactor, *dynamicFactor;
	if (tmax <= tmin) { tmin = my xmin; tmax = my xmax; }
	if (fmax <= fmin) { fmin = my ymin; fmax = my ymax; }
	if (! Matrix_getWindowSamplesX (me, tmin - 0.49999 * my dx, tmax + 0.49999 * my dx, & itmin, & itmax) ||
		 ! Matrix_getWindowSamplesY (me, fmin - 0.49999 * my dy, fmax + 0.49999 * my dy, & ifmin, & ifmax))
		return;
	Graphics_setWindow (g, tmin, tmax, fmin, fmax);
	if (! (preemphasisFactor = NUMdvector (ifmin, ifmax)) ||
		 ! (dynamicFactor = NUMdvector (itmin, itmax)))
		return;
	/* Pre-emphasis in place; also compute maximum after pre-emphasis. */
	for (ifreq = ifmin; ifreq <= ifmax; ifreq ++) {
		preemphasisFactor [ifreq] = (preemphasis / NUMln2) * log (ifreq * my dy / 1000.0);
		for (itime = itmin; itime <= itmax; itime ++) {
			double value = my z [ifreq] [itime];   /* Power. */
			value = (10.0/NUMln10) * log ((value + 1e-30) / 4.0e-10) + preemphasisFactor [ifreq];   /* dB */
			if (value > dynamicFactor [itime]) dynamicFactor [itime] = value;   /* Local maximum. */
			my z [ifreq] [itime] = value;
		}
	}
	/* Compute global maximum. */
	if (autoscaling) {
		maximum = 0.0;
		for (itime = itmin; itime <= itmax; itime ++)
			if (dynamicFactor [itime] > maximum) maximum = dynamicFactor [itime];
	}
	/* Dynamic compression in place. */
	for (itime = itmin; itime <= itmax; itime ++) {
		dynamicFactor [itime] = dynamicCompression * (maximum - dynamicFactor [itime]);
		for (ifreq = ifmin; ifreq <= ifmax; ifreq ++)
			my z [ifreq] [itime] += dynamicFactor [itime];
	}
	Graphics_image (g, my z,
		itmin, itmax,
		Matrix_columnToX (me, itmin - 0.5),
		Matrix_columnToX (me, itmax + 0.5),
		ifmin, ifmax,
		Matrix_rowToY (me, ifmin - 0.5),
		Matrix_rowToY (me, ifmax + 0.5),
		maximum - dynamic, maximum);
	for (ifreq = ifmin; ifreq <= ifmax; ifreq ++)
		for (itime = itmin; itime <= itmax; itime ++) {
			double value = 4.0e-10 * exp ((my z [ifreq] [itime] - dynamicFactor [itime]
				- preemphasisFactor [ifreq]) * (NUMln10 / 10.0)) - 1e-30;
			my z [ifreq] [itime] = value > 0.0 ? value : 0.0;
		}
	NUMdvector_free (preemphasisFactor, ifmin);
	NUMdvector_free (dynamicFactor, itmin);
}

/*
	Function:
		Draw me to a Graphics.
		If tmax <= tmin, draw all time samples.
		If fmax <= fmin, draw all frequency bands.
	Arguments:
		dynamicRange (dB): the difference between the maximum intensity and the lowest visible intensity.
		preemphasis (dB/octave): high-pass filtering.
		dynamicCompression (0-1):
			the amount by which weaker frames are enhanced in the direction of the strongest frame;
			0 = no compression, 1 = complete compression (all frames shown equally strong).
		garnish:
			a boolean that determines if a box, ticks, numbers, and text are written in the margins.
*/
void Spectrogram_paint (I, Graphics g,
	double tmin, double tmax, double fmin, double fmax, double maximum, int autoscaling,
	double dynamic, double preemphasis, double dynamicCompression,
	int garnish)
{
	iam (Spectrogram);
	Graphics_setInner (g);
	Spectrogram_paintInside (me, g, tmin, tmax, fmin, fmax, maximum, autoscaling,
		dynamic, preemphasis, dynamicCompression);
	Graphics_unsetInner (g);
	if (garnish) {
		Graphics_drawInnerBox (g);
		Graphics_textBottom (g, 1, L"Time (s)");
		Graphics_marksBottom (g, 2, 1, 1, 0);
		Graphics_marksLeft (g, 2, 1, 1, 0);
		Graphics_textLeft (g, 1, L"Frequency (Hz)");
	}
}

FORM (Spectrogram_paint, L"Spectrogram: Paint", L"Spectrogram: Paint...")
	praat_dia_timeRange (dia);
	REAL (L"left Frequency range (Hz)", L"0.0")
	REAL (L"right Frequency range (Hz)", L"0.0 (= all)")
	REAL (L"Maximum (dB/Hz)", L"100.0")
	BOOLEAN (L"Autoscaling", 1)
	POSITIVE (L"Dynamic range (dB)", L"50.0")
	REAL (L"Pre-emphasis (dB/oct)", L"6.0")
	REAL (L"Dynamic compression (0-1)", L"0.0")
	BOOLEAN (L"Garnish", 1)
	OK
DO
	EVERY_DRAW (Spectrogram_paint ((structSpectrogram *)OBJECT, GRAPHICS, GET_REAL (L"left Time range"),
		GET_REAL (L"right Time range"), GET_REAL (L"left Frequency range"), GET_REAL (L"right Frequency range"),
		GET_REAL (L"Maximum"), GET_INTEGER (L"Autoscaling"),
		GET_REAL (L"Dynamic range"), GET_REAL (L"Pre-emphasis"),
		GET_REAL (L"Dynamic compression"), GET_INTEGER (L"Garnish")))
END

FORM (Spectrogram_formula, L"Spectrogram: Formula", L"Spectrogram: Formula...")
	LABEL (L"label", L"Do for all times and frequencies:")
	LABEL (L"label", L"   `x' is the time in seconds")
	LABEL (L"label", L"   `y' is the frequency in Hertz")
	LABEL (L"label", L"   `self' is the current value in Pa\u00B2/Hz")
	LABEL (L"label", L"   Replace all values with:")
	TEXTFIELD (L"formula", L"self * exp (- x / 0.1)")
	OK
DO
	return praat_Fon_formula (dia, interpreter);
END

FORM (Spectrogram_getPowerAt, L"Spectrogram: Get power at (time, frequency)", 0)
	REAL (L"Time (s)", L"0.5")
	REAL (L"Frequency (Hz)", L"1000")
	OK
DO
	Spectrogram me = (structSpectrogram *)ONLY_OBJECT;
	double time = GET_REAL (L"Time"), frequency = GET_REAL (L"Frequency");
	MelderInfo_open ();
	MelderInfo_write1 (Melder_double (Matrix_getValueAtXY (me, time, frequency)));
	MelderInfo_write5 (L" Pa2/Hz (at time = ", Melder_double (time), L" seconds and frequency = ", Melder_double (frequency), L" Hz)");
	MelderInfo_close ();
END

DIRECT (Spectrogram_help) Melder_help (L"Spectrogram"); END

DIRECT (Spectrogram_movie)
	Graphics g = Movie_create (L"Spectrogram movie", 300, 300);
	WHERE (SELECTED) Matrix_movie (OBJECT, g);
END

DIRECT (Spectrogram_to_Matrix)
	EVERY_TO (Spectrogram_to_Matrix ((structSpectrogram *)OBJECT))
END

FORM (Spectrogram_to_Sound, L"Spectrogram: To Sound", 0)
	REAL (L"Sampling frequency (Hz)", L"44100")
	OK
DO
	EVERY_TO (Spectrogram_to_Sound ((structSpectrogram *)OBJECT, GET_REAL (L"Sampling frequency")))
END

FORM (Spectrogram_to_Spectrum, L"Spectrogram: To Spectrum (slice)", 0)
	REAL (L"Time (seconds)", L"0.0")
	OK
DO
	EVERY_TO (Spectrogram_to_Spectrum ((structSpectrogram *)OBJECT, GET_REAL (L"Time")))
END

DIRECT (Spectrogram_view)
	if (theCurrentPraatApplication -> batch)
		return Melder_error1 (L"Cannot view a Spectrogram from batch.");
	else
		WHERE (SELECTED)
			if (! praat_installEditor
				(new SpectrogramEditor (theCurrentPraatApplication -> topShell, ID_AND_FULL_NAME, OBJECT), IOBJECT))
					return 0;
END

/***** SPECTRUM *****/

void Spectrum_drawInside (Spectrum me, Graphics g, double fmin, double fmax, double minimum, double maximum) {
	double *yWC = NULL;
	long ifreq, ifmin, ifmax;
	int autoscaling = minimum >= maximum;

	if (fmax <= fmin) { fmin = my xmin; fmax = my xmax; }
	if (! Matrix_getWindowSamplesX (me, fmin, fmax, & ifmin, & ifmax)) return;

	yWC = NUMdvector (ifmin, ifmax); cherror

	/*
	 * First pass: compute power density.
	 */
	if (autoscaling) maximum = -1e30;
	for (ifreq = ifmin; ifreq <= ifmax; ifreq ++) {
		double y = our getValueAtSample (me, ifreq, 0, 2);
		if (autoscaling && y > maximum) maximum = y;
		yWC [ifreq] = y;
	}
	if (autoscaling) minimum = maximum - 60;   /* Default dynamic range is 60 dB. */

	/*
	 * Second pass: clip.
	 */
	for (ifreq = ifmin; ifreq <= ifmax; ifreq ++) {
		if (yWC [ifreq] < minimum) yWC [ifreq] = minimum;
		else if (yWC [ifreq] > maximum) yWC [ifreq] = maximum;
	}

	Graphics_setWindow (g, fmin, fmax, minimum, maximum);
	Graphics_function (g, yWC, ifmin, ifmax, Matrix_columnToX (me, ifmin), Matrix_columnToX (me, ifmax));
end:
	NUMdvector_free (yWC, ifmin);
	Melder_clearError ();
}

/*
	Function:
		draw a Spectrum into a Graphics.
	Preconditions:
		maximum > minimum;
	Arguments:
		[fmin, fmax]: frequencies in Hertz; x domain of drawing;
		Autowindowing: if fmax <= fmin, x domain of drawing is [my xmin, my xmax].
		[minimum, maximum]: power in dB/Hertz; y range of drawing.
*/
void Spectrum_draw (Spectrum me, Graphics g, double fmin, double fmax, double minimum, double maximum, int garnish) {
	Graphics_setInner (g);
	Spectrum_drawInside (me, g, fmin, fmax, minimum, maximum);
	Graphics_unsetInner (g);
	if (garnish) {
		Graphics_drawInnerBox (g);
		Graphics_textBottom (g, 1, L"Frequency (Hz)");
		Graphics_marksBottom (g, 2, TRUE, TRUE, FALSE);
		Graphics_textLeft (g, 1, L"Sound pressure level (dB/Hz)");
		Graphics_marksLeftEvery (g, 1.0, 20.0, TRUE, TRUE, FALSE);
	}
}

void Spectrum_drawLogFreq (Spectrum me, Graphics g, double fmin, double fmax, double minimum, double maximum, int garnish) {
	double *xWC = NULL, *yWC = NULL;
	long ifreq, ifmin, ifmax;
	int autoscaling = minimum >= maximum;
	if (fmax <= fmin) { fmin = my xmin; fmax = my xmax; }
	if (! Matrix_getWindowSamplesX (me, fmin, fmax, & ifmin, & ifmax)) return;
if(ifmin==1)ifmin=2;  /* BUG */
	xWC = NUMdvector (ifmin, ifmax); cherror
	yWC = NUMdvector (ifmin, ifmax); cherror

	/*
	 * First pass: compute power density.
	 */
	if (autoscaling) maximum = -1e6;
	for (ifreq = ifmin; ifreq <= ifmax; ifreq ++) {
		xWC [ifreq] = log10 (my x1 + (ifreq - 1) * my dx);
		yWC [ifreq] = our getValueAtSample (me, ifreq, 0, 2);
		if (autoscaling && yWC [ifreq] > maximum) maximum = yWC [ifreq];
	}
	if (autoscaling) minimum = maximum - 60;   /* Default dynamic range is 60 dB. */

	/*
	 * Second pass: clip.
	 */
	for (ifreq = ifmin; ifreq <= ifmax; ifreq ++) {
		if (yWC [ifreq] < minimum) yWC [ifreq] = minimum;
		else if (yWC [ifreq] > maximum) yWC [ifreq] = maximum;
	}

	Graphics_setInner (g);
	Graphics_setWindow (g, log10 (fmin), log10 (fmax), minimum, maximum);
	Graphics_polyline (g, ifmax - ifmin + 1, & xWC [ifmin], & yWC [ifmin]);
	Graphics_unsetInner (g);
	if (garnish) {
		Graphics_drawInnerBox (g);
		Graphics_textBottom (g, 1, L"Frequency (Hz)");
		Graphics_marksBottomLogarithmic (g, 3, TRUE, TRUE, FALSE);
		Graphics_textLeft (g, 1, L"Sound pressure level (dB/Hz)");
		Graphics_marksLeftEvery (g, 1.0, 20.0, TRUE, TRUE, FALSE);
	}
end:
	NUMdvector_free (xWC, ifmin);
	NUMdvector_free (yWC, ifmin);
	Melder_clearError ();
}

FORM (Spectrum_cepstralSmoothing, L"Spectrum: Cepstral smoothing", 0)
	POSITIVE (L"Bandwidth (Hz)", L"500.0")
	OK
DO
	EVERY_TO (Spectrum_cepstralSmoothing ((structSpectrum *)OBJECT, GET_REAL (L"Bandwidth")))
END

FORM (Spectrum_draw, L"Spectrum: Draw", 0)
	REAL (L"left Frequency range (Hz)", L"0.0")
	REAL (L"right Frequency range (Hz)", L"0.0 (= all)")
	REAL (L"Minimum power (dB/Hz)", L"0 (= auto)")
	REAL (L"Maximum power (dB/Hz)", L"0 (= auto)")
	BOOLEAN (L"Garnish", 1)
	OK
DO
	EVERY_DRAW (Spectrum_draw ((structSpectrum *)OBJECT, GRAPHICS, GET_REAL (L"left Frequency range"),
		GET_REAL (L"right Frequency range"), GET_REAL (L"Minimum power"), GET_REAL (L"Maximum power"),
		GET_INTEGER (L"Garnish")))
END

FORM (Spectrum_drawLogFreq, L"Spectrum: Draw (log freq)", 0)
	POSITIVE (L"left Frequency range (Hz)", L"10.0")
	POSITIVE (L"right Frequency range (Hz)", L"10000.0")
	REAL (L"Minimum power (dB/Hz)", L"0 (= auto)")
	REAL (L"Maximum power (dB/Hz)", L"0 (= auto)")
	BOOLEAN (L"Garnish", 1)
	OK
DO
	EVERY_DRAW (Spectrum_drawLogFreq ((structSpectrum *)OBJECT, GRAPHICS, GET_REAL (L"left Frequency range"),
		GET_REAL (L"right Frequency range"), GET_REAL (L"Minimum power"), GET_REAL (L"Maximum power"),
		GET_INTEGER (L"Garnish")))
END

DIRECT (Spectrum_edit)
	if (theCurrentPraatApplication -> batch) return Melder_error1 (L"Cannot edit a Spectrum from batch.");
	else WHERE (SELECTED)
		if (! praat_installEditor (new SpectrumEditor (theCurrentPraatApplication -> topShell, ID_AND_FULL_NAME, OBJECT), IOBJECT)) return 0;
END

FORM (Spectrum_formula, L"Spectrum: Formula", L"Spectrum: Formula...")
	LABEL (L"label", L"`x' is the frequency in Hertz, `col' is the bin number;   "
		"`y' = `row' is 1 (real part) or 2 (imaginary part)")
	LABEL (L"label", L"y := 1;   row := 1;   "
		"x := 0;   for col := 1 to ncol do { self [1, col] := `formula' ; x := x + dx }")
	LABEL (L"label", L"y := 2;   row := 2;   "
		"x := 0;   for col := 1 to ncol do { self [2, col] := `formula' ; x := x + dx }")
	TEXTFIELD (L"formula", L"0")
	OK
DO
	if (! praat_Fon_formula (dia, interpreter)) return 0;
END

FORM (Spectrum_getBandDensity, L"Spectrum: Get band density", 0)
	REAL (L"Band floor (Hz)", L"200.0") REAL (L"Band ceiling (Hz)", L"1000") OK DO
	Melder_informationReal (Spectrum_getBandDensity ((structSpectrum *)ONLY_OBJECT,
		GET_REAL (L"Band floor"), GET_REAL (L"Band ceiling")), L" Pa2 / Hz2"); END
FORM (Spectrum_getBandDensityDifference, L"Spectrum: Get band density difference", 0)
	REAL (L"Low band floor (Hz)", L"0") REAL (L"Low band ceiling (Hz)", L"500")
	REAL (L"High band floor (Hz)", L"500") REAL (L"High band ceiling (Hz)", L"4000") OK DO
	Melder_informationReal (Spectrum_getBandDensityDifference ((structSpectrum *)ONLY_OBJECT,
		GET_REAL (L"Low band floor"), GET_REAL (L"Low band ceiling"), GET_REAL (L"High band floor"), GET_REAL (L"High band ceiling")), L"dB"); END
FORM (Spectrum_getBandEnergy, L"Spectrum: Get band energy", 0)
	REAL (L"Band floor (Hz)", L"200.0") REAL (L"Band ceiling (Hz)", L"1000") OK DO
	Melder_informationReal (Spectrum_getBandEnergy ((structSpectrum *)ONLY_OBJECT, GET_REAL (L"Band floor"), GET_REAL (L"Band ceiling")), L"Pa2 sec"); END
FORM (Spectrum_getBandEnergyDifference, L"Spectrum: Get band energy difference", 0)
	REAL (L"Low band floor (Hz)", L"0") REAL (L"Low band ceiling (Hz)", L"500")
	REAL (L"High band floor (Hz)", L"500") REAL (L"High band ceiling (Hz)", L"4000") OK DO
	Melder_informationReal (Spectrum_getBandEnergyDifference ((structSpectrum *)ONLY_OBJECT,
		GET_REAL (L"Low band floor"), GET_REAL (L"Low band ceiling"), GET_REAL (L"High band floor"), GET_REAL (L"High band ceiling")), L"dB"); END	
FORM (Spectrum_getBinFromFrequency, L"Spectrum: Get bin from frequency", 0)
	REAL (L"Frequency (Hz)", L"2000") OK DO
	Melder_informationReal (Sampled_xToIndex (ONLY_OBJECT, GET_REAL (L"Frequency")), NULL); END
DIRECT (Spectrum_getBinWidth) Spectrum me = (structSpectrum *)ONLY_OBJECT; Melder_informationReal (my dx, L"Hertz"); END
FORM (Spectrum_getCentralMoment, L"Spectrum: Get central moment", L"Spectrum: Get central moment...")
	POSITIVE (L"Moment", L"3.0")
	POSITIVE (L"Power", L"2.0") OK DO
	Melder_informationReal (Spectrum_getCentralMoment ((structSpectrum *)ONLY_OBJECT,
	GET_REAL (L"Moment"), GET_REAL (L"Power")), L"Hertz to the power 'moment'"); END
FORM (Spectrum_getCentreOfGravity, L"Spectrum: Get centre of gravity", L"Spectrum: Get centre of gravity...")
	POSITIVE (L"Power", L"2.0") OK DO
	Melder_informationReal (Spectrum_getCentreOfGravity ((structSpectrum *)ONLY_OBJECT, GET_REAL (L"Power")), L"Hertz"); END
FORM (Spectrum_getFrequencyFromBin, L"Spectrum: Get frequency from bin", 0)
	NATURAL (L"Band number", L"1") OK DO
	Melder_informationReal (Sampled_indexToX (ONLY_OBJECT, GET_INTEGER (L"Band number")), L"Hertz"); END
DIRECT (Spectrum_getHighestFrequency) Spectrum me = (structSpectrum *)ONLY_OBJECT; Melder_informationReal (my xmax, L"Hertz"); END
FORM (Spectrum_getImaginaryValueInBin, L"Spectrum: Get imaginary value in bin", 0)
	NATURAL (L"Bin number", L"100") OK DO Spectrum me = (structSpectrum *)ONLY_OBJECT;
	long binNumber = GET_INTEGER (L"Bin number");
	REQUIRE (binNumber <= my nx, L"Bin number must not exceed number of bins.");
	Melder_informationReal (my z [2] [binNumber], NULL); END
FORM (Spectrum_getKurtosis, L"Spectrum: Get kurtosis", L"Spectrum: Get kurtosis...")
	POSITIVE (L"Power", L"2.0") OK DO
	Melder_informationReal (Spectrum_getKurtosis ((structSpectrum *)ONLY_OBJECT, GET_REAL (L"Power")), NULL); END
DIRECT (Spectrum_getLowestFrequency) Spectrum me = (structSpectrum *)ONLY_OBJECT; Melder_informationReal (my xmin, L"Hertz"); END
DIRECT (Spectrum_getNumberOfBins) Spectrum me = (structSpectrum *)ONLY_OBJECT; Melder_information2 (Melder_integer (my nx), L" bins"); END
FORM (Spectrum_getRealValueInBin, L"Spectrum: Get real value in bin", 0)
	NATURAL (L"Bin number", L"100") OK DO Spectrum me = (structSpectrum *)ONLY_OBJECT;
	long binNumber = GET_INTEGER (L"Bin number");
	REQUIRE (binNumber <= my nx, L"Bin number must not exceed number of bins.");
	Melder_informationReal (my z [1] [binNumber], NULL); END
FORM (Spectrum_getSkewness, L"Spectrum: Get skewness", L"Spectrum: Get skewness...")
	POSITIVE (L"Power", L"2.0") OK DO
	Melder_informationReal (Spectrum_getSkewness ((structSpectrum *)ONLY_OBJECT, GET_REAL (L"Power")), NULL); END
FORM (Spectrum_getStandardDeviation, L"Spectrum: Get standard deviation", L"Spectrum: Get standard deviation...")
	POSITIVE (L"Power", L"2.0") OK DO
	Melder_informationReal (Spectrum_getStandardDeviation ((structSpectrum *)ONLY_OBJECT, GET_REAL (L"Power")), L"Hertz"); END

DIRECT (Spectrum_help) Melder_help (L"Spectrum"); END

FORM (Spectrum_list, L"Spectrum: List", 0)
	BOOLEAN (L"Include bin number", false)
	BOOLEAN (L"Include frequency", true)
	BOOLEAN (L"Include real part", false)
	BOOLEAN (L"Include imaginary part", false)
	BOOLEAN (L"Include energy density", false)
	BOOLEAN (L"Include power density", true)
	OK
DO
	EVERY (Spectrum_list ((structSpectrum *)OBJECT, GET_INTEGER (L"Include bin number"), GET_INTEGER (L"Include frequency"),
		GET_INTEGER (L"Include real part"), GET_INTEGER (L"Include imaginary part"),
		GET_INTEGER (L"Include energy density"), GET_INTEGER (L"Include power density")))
END

FORM (Spectrum_lpcSmoothing, L"Spectrum: LPC smoothing", 0)
	NATURAL (L"Number of peaks", L"5")
	POSITIVE (L"Pre-emphasis from (Hz)", L"50.0")
	OK
DO
	EVERY_TO (Spectrum_lpcSmoothing ((structSpectrum *)OBJECT, GET_INTEGER (L"Number of peaks"), GET_REAL (L"Pre-emphasis from")))
END

FORM (Spectrum_passHannBand, L"Spectrum: Filter (pass Hann band)", L"Spectrum: Filter (pass Hann band)...")
	REAL (L"From frequency (Hz)", L"500")
	REAL (L"To frequency (Hz)", L"1000")
	POSITIVE (L"Smoothing (Hz)", L"100")
	OK
DO
	WHERE (SELECTED) {
		Spectrum_passHannBand ((structSpectrum *)OBJECT, GET_REAL (L"From frequency"), GET_REAL (L"To frequency"), GET_REAL (L"Smoothing"));
		praat_dataChanged (OBJECT);
	}
END

FORM (Spectrum_stopHannBand, L"Spectrum: Filter (stop Hann band)", L"Spectrum: Filter (stop Hann band)...")
	REAL (L"From frequency (Hz)", L"500")
	REAL (L"To frequency (Hz)", L"1000")
	POSITIVE (L"Smoothing (Hz)", L"100")
	OK
DO
	WHERE (SELECTED) {
		Spectrum_stopHannBand ((structSpectrum *)OBJECT, GET_REAL (L"From frequency"), GET_REAL (L"To frequency"), GET_REAL (L"Smoothing"));
		praat_dataChanged (OBJECT);
	}
END

FORM (Spectrum_to_Excitation, L"Spectrum: To Excitation", 0)
	POSITIVE (L"Frequency resolution (Bark)", L"0.1")
	OK
DO
	EVERY_TO (Spectrum_to_Excitation ((structSpectrum *)OBJECT, GET_REAL (L"Frequency resolution")))
END

FORM (Spectrum_to_Formant_peaks, L"Spectrum: To Formant (peaks)", 0)
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"Warning: this simply picks peaks from 0 Hz up!")
	NATURAL (L"Maximum number of formants", L"1000")
	OK
DO
	EVERY_TO (Spectrum_to_Formant ((structSpectrum *)OBJECT, GET_INTEGER (L"Maximum number of formants")))
END

FORM (Spectrum_to_Ltas, L"Spectrum: To Long-term average spectrum", 0)
	POSITIVE (L"Bandwidth (Hz)", L"1000")
	OK
DO
	EVERY_TO (Spectrum_to_Ltas ((structSpectrum *)OBJECT, GET_REAL (L"Bandwidth")))
END

DIRECT (Spectrum_to_Ltas_1to1)
	EVERY_TO (Spectrum_to_Ltas_1to1 ((structSpectrum *)OBJECT))
END

DIRECT (Spectrum_to_Matrix)
	EVERY_TO (Spectrum_to_Matrix ((structSpectrum *)OBJECT))
END

DIRECT (Spectrum_to_Sound)
	EVERY_TO (Spectrum_to_Sound ((structSpectrum *)OBJECT))
END

DIRECT (Spectrum_to_Spectrogram)
	EVERY_TO (Spectrum_to_Spectrogram ((structSpectrum *)OBJECT))
END

DIRECT (Spectrum_to_SpectrumTier_peaks)
	EVERY_TO (Spectrum_to_SpectrumTier_peaks ((structSpectrum *)OBJECT))
END

/***** SPECTRUMTIER *****/

void SpectrumTier_draw (SpectrumTier me, Graphics g, double fmin, double fmax,
	double pmin, double pmax, int garnish, const wchar_t *method)
{
	RealTier_draw (me, g, fmin, fmax, pmin, pmax, garnish, method, L"Power spectral density (dB)");
}

DIRECT (SpectrumTier_downto_Table)
	EVERY_TO (SpectrumTier_downto_Table ((structSpectrumTier *)OBJECT, true, true, true))
END

FORM (old_SpectrumTier_draw, L"SpectrumTier: Draw", 0)   // 2010/10/19
	REAL (L"left Frequency range (Hz)", L"0.0")
	REAL (L"right Frequency range (Hz)", L"10000.0")
	REAL (L"left Power range (dB)", L"20.0")
	REAL (L"right Power range (dB)", L"80.0")
	BOOLEAN (L"Garnish", 1)
	OK
DO
	EVERY_DRAW (SpectrumTier_draw ((structSpectrumTier *)OBJECT, GRAPHICS,
		GET_REAL (L"left Frequency range"), GET_REAL (L"right Frequency range"),
		GET_REAL (L"left Power range"), GET_REAL (L"right Power range"),
		GET_INTEGER (L"Garnish"), L"lines and speckles"))
END

FORM (SpectrumTier_draw, L"SpectrumTier: Draw", 0)
	REAL (L"left Frequency range (Hz)", L"0.0")
	REAL (L"right Frequency range (Hz)", L"10000.0")
	REAL (L"left Power range (dB)", L"20.0")
	REAL (L"right Power range (dB)", L"80.0")
	BOOLEAN (L"Garnish", 1)
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"ui/editors/AmplitudeTierEditor.h")
	OPTIONMENU (L"Drawing method", 1)
		OPTION (L"lines")
		OPTION (L"speckles")
		OPTION (L"lines and speckles")
	OK
DO_ALTERNATIVE (old_SpectrumTier_draw)
	EVERY_DRAW (SpectrumTier_draw ((structSpectrumTier *)OBJECT, GRAPHICS,
		GET_REAL (L"left Frequency range"), GET_REAL (L"right Frequency range"),
		GET_REAL (L"left Power range"), GET_REAL (L"right Power range"),
		GET_INTEGER (L"Garnish"), GET_STRING (L"Drawing method")))
END

FORM (SpectrumTier_list, L"SpectrumTier: List", 0)
	BOOLEAN (L"Include indexes", true)
	BOOLEAN (L"Include frequency", true)
	BOOLEAN (L"Include power density", true)
	OK
DO
	EVERY (SpectrumTier_list ((structSpectrumTier *)OBJECT, GET_INTEGER (L"Include indexes"), GET_INTEGER (L"Include frequency"),
		GET_INTEGER (L"Include power density")))
END

FORM (SpectrumTier_removePointsBelow, L"SpectrumTier: Remove points below", 0)
	REAL (L"Remove all points below (dB)", L"40.0")
	OK
DO
	WHERE (SELECTED) {
		RealTier_removePointsBelow ((structRealTier *)OBJECT, GET_REAL (L"Remove all points below"));
		praat_dataChanged (OBJECT);
	}
END

/***** STRINGS *****/

FORM (Strings_createAsFileList, L"Create Strings as file list", L"Create Strings as file list...")
	SENTENCE (L"Name", L"fileList")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"Path:")
	TEXTFIELD (L"path", L"/people/Miep/*.wav")
	OK
static int inited;
if (! inited) {
	structMelderDir defaultDir = { { 0 } };
	wchar_t *workingDirectory, path [300];
	Melder_getDefaultDir (& defaultDir);
	workingDirectory = Melder_dirToPath (& defaultDir);
	#if defined (UNIX)
		swprintf (path, 300, L"%ls/*.wav", workingDirectory);
	#elif defined (_WIN32)
	{
		int len = wcslen (workingDirectory);
		swprintf (path, 300, L"%ls%ls*.wav", workingDirectory, len == 0 || workingDirectory [len - 1] != '\\' ? L"\\" : L"ui/editors/AmplitudeTierEditor.h");
	}
	#else
		swprintf (path, 300, L"%ls*.wav", workingDirectory);
	#endif
	SET_STRING (L"path", path);
	inited = TRUE;
}
DO
	if (! praat_new1 (Strings_createAsFileList (GET_STRING (L"path")), GET_STRING (L"Name"))) return 0;
END

FORM (Strings_createAsDirectoryList, L"Create Strings as directory list", L"Create Strings as directory list...")
	SENTENCE (L"Name", L"directoryList")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"Path:")
	TEXTFIELD (L"path", L"/people/Miep/*")
	OK
static int inited;
if (! inited) {
	structMelderDir defaultDir = { { 0 } };
	wchar_t *workingDirectory, path [300];
	Melder_getDefaultDir (& defaultDir);
	workingDirectory = Melder_dirToPath (& defaultDir);
	#if defined (UNIX)
		swprintf (path, 300, L"%ls/*", workingDirectory);
	#elif defined (_WIN32)
	{
		int len = wcslen (workingDirectory);
		swprintf (path, 300, L"%ls%ls*", workingDirectory, len == 0 || workingDirectory [len - 1] != '\\' ? L"\\" : L"ui/editors/AmplitudeTierEditor.h");
	}
	#else
		swprintf (path, 300, L"%ls*", workingDirectory);
	#endif
	SET_STRING (L"path", path);
	inited = TRUE;
}
DO
	if (! praat_new1 (Strings_createAsDirectoryList (GET_STRING (L"path")), GET_STRING (L"Name"))) return 0;
END

DIRECT (Strings_edit)
	if (theCurrentPraatApplication -> batch) {
		return Melder_error1 (L"Cannot edit a Strings from batch.");
	} else {
		WHERE (SELECTED && CLASS == classStrings)
			if (! praat_installEditor (new StringsEditor (theCurrentPraatApplication -> topShell, ID_AND_FULL_NAME,
				OBJECT), IOBJECT)) return 0;
	}
END

DIRECT (Strings_equal)
	Strings s1 = NULL, s2 = NULL;
	WHERE (SELECTED) { if (s1) s2 = (structStrings *)OBJECT; else s1 = (structStrings *)OBJECT; }
	Melder_information1 (Melder_integer (Data_equal (s1, s2)));
END

DIRECT (Strings_genericize)
	WHERE (SELECTED) {
		int status = Strings_genericize ((structStrings *)OBJECT);
		praat_dataChanged (OBJECT);
		if (! status) return 0;
	}
END

DIRECT (Strings_getNumberOfStrings)
	Strings me = (structStrings *)ONLY_OBJECT;
	Melder_information1 (Melder_integer (my numberOfStrings));
END

FORM (Strings_getString, L"Get string", 0)
	NATURAL (L"Index", L"1")
	OK
DO
	Strings me = (structStrings *)ONLY_OBJECT;
	long index = GET_INTEGER (L"Index");
	Melder_information1 (index > my numberOfStrings ? L"ui/editors/AmplitudeTierEditor.h" : my strings [index]);
END

DIRECT (Strings_help) Melder_help (L"Strings"); END

DIRECT (Strings_nativize)
	WHERE (SELECTED) {
		int status = Strings_nativize ((structStrings *)OBJECT);
		praat_dataChanged (OBJECT);
		if (! status) return 0;
	}
END

DIRECT (Strings_randomize)
	WHERE (SELECTED) {
		Strings_randomize ((structStrings *)OBJECT);
		praat_dataChanged (OBJECT);
	}
END

FORM_READ (Strings_readFromRawTextFile, L"Read Strings from raw text file", 0, true)
	if (! praat_new1 (Strings_readFromRawTextFile (file), MelderFile_name (file))) return 0;
END

DIRECT (Strings_sort)
	WHERE (SELECTED) {
		Strings_sort ((structStrings *)OBJECT);
		praat_dataChanged (OBJECT);
	}
END

DIRECT (Strings_to_Distributions)
	EVERY_TO (Strings_to_Distributions ((structStrings *)OBJECT))
END

DIRECT (Strings_to_WordList)
	EVERY_TO (Strings_to_WordList ((structStrings *)OBJECT))
END

FORM_WRITE (Strings_writeToRawTextFile, L"Save Strings as text file", 0, L"txt")
	if (! Strings_writeToRawTextFile ((structStrings *)ONLY_OBJECT, file)) return 0;
END

/***** TABLE, rest in praat_Stat.c *****/

DIRECT (Table_to_Matrix)
	EVERY_TO (Table_to_Matrix ((structTable *)OBJECT))
END

/***** TEXTGRID, rest in praat_TextGrid_init.c *****/

FORM (TextGrid_create, L"Create TextGrid", L"Create TextGrid...")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"Hint: to label or segment an existing Sound,")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"select that Sound and choose \"To TextGrid...\".")
	REAL (L"Start time (s)", L"0.0")
	REAL (L"End time (s)", L"1.0")
	SENTENCE (L"All tier names", L"Mary John bell")
	SENTENCE (L"Which of these are point tiers?", L"bell")
	OK
DO
	double tmin = GET_REAL (L"Start time"), tmax = GET_REAL (L"End time");
	REQUIRE (tmax > tmin, L"End time should be greater than start time")
	if (! praat_new1 (TextGrid_create (tmin, tmax, GET_STRING (L"All tier names"), GET_STRING (L"Which of these are point tiers?")),
		GET_STRING (L"All tier names"))) return 0;
END

/***** TEXTTIER, rest in praat_TextGrid_init.c *****/

FORM_READ (TextTier_readFromXwaves, L"Read TextTier from Xwaves", 0, true)
	if (! praat_new1 (TextTier_readFromXwaves (file), MelderFile_name (file))) return 0;
END

/***** TIMEFRAMESAMPLED *****/

DIRECT (TimeFrameSampled_getNumberOfFrames)
	Sampled me = (structSampled *)ONLY_OBJECT;
	Melder_information2 (Melder_integer (my nx), L" frames");
END

FORM (TimeFrameSampled_getFrameFromTime, L"Get frame number from time", L"Get frame number from time...")
	REAL (L"Time (s)", L"0.5")
	OK
DO
	Melder_informationReal (Sampled_xToIndex ((structSampled *)ONLY_OBJECT, GET_REAL (L"Time")), NULL);
END

DIRECT (TimeFrameSampled_getFrameLength)
	Sampled me = (structSampled *)ONLY_OBJECT;
	Melder_informationReal (my dx, L"seconds");
END

FORM (TimeFrameSampled_getTimeFromFrame, L"Get time from frame number", L"Get time from frame number...")
	NATURAL (L"Frame number", L"1")
	OK
DO
	Melder_informationReal (Sampled_indexToX ((structSampled *)ONLY_OBJECT, GET_INTEGER (L"Frame number")), L"seconds");
END

/***** TIMEFUNCTION *****/

DIRECT (TimeFunction_getDuration)
	Function me = (structFunction *)ONLY_OBJECT;
	Melder_informationReal (my xmax - my xmin, L"seconds");
END

DIRECT (TimeFunction_getEndTime)
	Function me = (structFunction *)ONLY_OBJECT;
	Melder_informationReal (my xmax, L"seconds");
END

DIRECT (TimeFunction_getStartTime)
	Function me = (structFunction *)ONLY_OBJECT;
	Melder_informationReal (my xmin, L"seconds");
END

FORM (TimeFunction_scaleTimesBy, L"Scale times by", 0)
	POSITIVE (L"Factor", L"2.0")
	OK
DO
	WHERE (SELECTED) {
		Function_scaleXBy ((structFunction *)OBJECT, GET_REAL (L"Factor"));
		praat_dataChanged (OBJECT);
	}
END

FORM (TimeFunction_scaleTimesTo, L"Scale times to", 0)
	REAL (L"New start time (s)", L"0.0")
	REAL (L"New end time (s)", L"1.0")
	OK
DO
	double tminto = GET_REAL (L"New start time"), tmaxto = GET_REAL (L"New end time");
	REQUIRE (tminto < tmaxto, L"New end time should be greater than new start time.")
	WHERE (SELECTED) {
		Function me = (structFunction *)OBJECT;
		Function_scaleXTo (me, tminto, tmaxto);
		praat_dataChanged (me);
	}
END

FORM (TimeFunction_shiftTimesBy, L"Shift times by", 0)
	REAL (L"Shift (s)", L"0.5")
	OK
DO
	WHERE (SELECTED) {
		Function_shiftXBy ((structFunction *)OBJECT, GET_REAL (L"Shift"));
		praat_dataChanged (OBJECT);
	}
END

FORM (TimeFunction_shiftTimesTo, L"Shift times to", 0)
	RADIO (L"Shift", 1)
		OPTION (L"start time")
		OPTION (L"centre time")
		OPTION (L"end time")
	REAL (L"To time (s)", L"0.0")
	OK
DO
	int shift = GET_INTEGER (L"Shift");
	WHERE (SELECTED) {
		Function me = (structFunction *)OBJECT;
		Function_shiftXTo (me, shift == 1 ? my xmin : shift == 2 ? 0.5 * (my xmin + my xmax) : my xmax, GET_REAL (L"To time"));
		praat_dataChanged (me);
	}
END

DIRECT (TimeFunction_shiftToZero)
	WHERE (SELECTED) {
		Function me = (structFunction *)OBJECT;
		Function_shiftXTo (me, my xmin, 0.0);
		praat_dataChanged (me);
	}
END

/***** TIMETIER *****/

FORM (TimeTier_getHighIndexFromTime, L"Get high index", L"AnyTier: Get high index from time...")
	REAL (L"Time (s)", L"0.5")
	OK
DO
	AnyTier me = (structAnyTier *)ONLY_OBJECT;
	Melder_information1 (my points -> size == 0 ? L"--undefined--" : Melder_integer (AnyTier_timeToHighIndex (me, GET_REAL (L"Time"))));
END

FORM (TimeTier_getLowIndexFromTime, L"Get low index", L"AnyTier: Get low index from time...")
	REAL (L"Time (s)", L"0.5")
	OK
DO
	AnyTier me = (structAnyTier *)ONLY_OBJECT;
	Melder_information1 (my points -> size == 0 ? L"--undefined--" : Melder_integer (AnyTier_timeToLowIndex (me, GET_REAL (L"Time"))));
END

FORM (TimeTier_getNearestIndexFromTime, L"Get nearest index", L"AnyTier: Get nearest index from time...")
	REAL (L"Time (s)", L"0.5")
	OK
DO
	AnyTier me = (structAnyTier *)ONLY_OBJECT;
	Melder_information1 (my points -> size == 0 ? L"--undefined--" : Melder_integer (AnyTier_timeToNearestIndex (me, GET_REAL (L"Time"))));
END

DIRECT (TimeTier_getNumberOfPoints)
	AnyTier me = (structAnyTier *)ONLY_OBJECT;
	Melder_information2 (Melder_integer (my points -> size), L" points");
END

FORM (TimeTier_getTimeFromIndex, L"Get time", 0 /*"AnyTier: Get time from index..."*/)
	NATURAL (L"Point number", L"10")
	OK
DO
	AnyTier me = (structAnyTier *)ONLY_OBJECT;
	long i = GET_INTEGER (L"Point number");
	if (i > my points -> size) Melder_information1 (L"--undefined--");
	else Melder_informationReal (((AnyPoint) my points -> item [i]) -> time, L"seconds");
END

FORM (TimeTier_removePoint, L"Remove one point", L"AnyTier: Remove point...")
	NATURAL (L"Point number", L"1")
	OK
DO
	WHERE (SELECTED) {
		AnyTier_removePoint ((structAnyTier *)OBJECT, GET_INTEGER (L"Point number"));
		praat_dataChanged (OBJECT);
	}
END

FORM (TimeTier_removePointNear, L"Remove one point", L"AnyTier: Remove point near...")
	REAL (L"Time (s)", L"0.5")
	OK
DO
	WHERE (SELECTED) {
		AnyTier_removePointNear ((structAnyTier *)OBJECT, GET_REAL (L"Time"));
		praat_dataChanged (OBJECT);
	}
END

FORM (TimeTier_removePointsBetween, L"Remove points", L"AnyTier: Remove points between...")
	REAL (L"left Time range (s)", L"0.0")
	REAL (L"right Time range (s)", L"1.0")
	OK
DO
	WHERE (SELECTED) {
		AnyTier_removePointsBetween ((structAnyTier *)OBJECT, GET_REAL (L"left Time range"), GET_REAL (L"right Time range"));
		praat_dataChanged (OBJECT);
	}
END

/***** TRANSITION *****/

static void NUMrationalize (double x, long *numerator, long *denominator) {
	double epsilon = 1e-6;
	*numerator = 1;
	for (*denominator = 1; *denominator <= 100000; (*denominator) ++) {
		double numerator_d = x * *denominator, rounded = floor (numerator_d + 0.5);
		if (fabs (rounded - numerator_d) < epsilon) {
			*numerator = rounded;
			return;
		}
	}
	*denominator = 0;   /* Failure. */
}

static void print4 (wchar_t *buffer, double value, int iformat, int width, int precision) {
	wchar_t formatString [40];
	if (iformat == 4) {
		long numerator, denominator;
		NUMrationalize (value, & numerator, & denominator);
		if (numerator == 0)
			swprintf (buffer, 40, L"0");
		else if (denominator > 1)
			swprintf (buffer, 40, L"%ld/%ld", numerator, denominator);
		else
			swprintf (buffer, 40, L"%.7g", value);
	} else {
		swprintf (formatString, 40, L"%%%d.%d%c", width, precision, iformat == 1 ? 'f' : iformat == 2 ? 'e' : 'g');
		swprintf (buffer, 40, formatString, value);
	}
}

void Transition_drawAsNumbers (I, Graphics g, int iformat, int precision) {
	iam (Transition);
	double leftMargin, lineSpacing, maxTextWidth = 0, maxTextHeight = 0;
	long row, col;
	Graphics_setInner (g);
	Graphics_setWindow (g, 0.5, my numberOfStates + 0.5, 0, 1);
	leftMargin = Graphics_dxMMtoWC (g, 1);
	lineSpacing = Graphics_dyMMtoWC (g, 1.5 * Graphics_inqFontSize (g) * 25.4 / 72);
	Graphics_setTextAlignment (g, Graphics_CENTRE, Graphics_BOTTOM);
	for (col = 1; col <= my numberOfStates; col ++) {
		if (my stateLabels && my stateLabels [col] && my stateLabels [col] [0]) {
			Graphics_text (g, col, 1, my stateLabels [col]);
			if (! maxTextHeight) maxTextHeight = lineSpacing;
		}
	}
	for (row = 1; row <= my numberOfStates; row ++) {
		double y = 1 - lineSpacing * (row - 1 + 0.7);
		Graphics_setTextAlignment (g, Graphics_RIGHT, Graphics_HALF);
		if (my stateLabels && my stateLabels [row]) {
			double textWidth = Graphics_textWidth (g, my stateLabels [row]);
			if (textWidth > maxTextWidth) maxTextWidth = textWidth;
			Graphics_text (g, 0.5 - leftMargin, y, my stateLabels [row]);
		}
		Graphics_setTextAlignment (g, Graphics_CENTRE, Graphics_HALF);
		for (col = 1; col <= my numberOfStates; col ++) {
			wchar_t text [40];
			print4 (text, my data [row] [col], iformat, 0, precision);
			Graphics_text (g, col, y, text);
		}
	}
	if (maxTextWidth)
		Graphics_line (g, 0.5 - maxTextWidth - leftMargin, 1, my numberOfStates + 0.5, 1);
	if (maxTextHeight)
		Graphics_line (g, 0.5, 1 + maxTextHeight, 0.5, 1 - lineSpacing * (my numberOfStates + 0.2));
	Graphics_unsetInner (g);
}

DIRECT (Transition_conflate)
	EVERY_TO (Transition_to_Distributions_conflate ((structTransition *)OBJECT))
END

FORM (Transition_drawAsNumbers, L"Draw as numbers", 0)
	RADIO (L"Format", 1)
	RADIOBUTTON (L"decimal")
	RADIOBUTTON (L"exponential")
	RADIOBUTTON (L"free")
	RADIOBUTTON (L"rational")
	NATURAL (L"Precision", L"2")
	OK
DO
	EVERY_DRAW (Transition_drawAsNumbers ((structTransition *)OBJECT, GRAPHICS,
		GET_INTEGER (L"Format"), GET_INTEGER (L"Precision")))
END

DIRECT (Transition_eigen)
	WHERE (SELECTED) {
		Matrix vec, val;
		if (! Transition_eigen ((structTransition *)OBJECT, & vec, & val)) return 0;
		if (! praat_new1 (vec, L"eigenvectors")) return 0;
		if (! praat_new1 (val, L"eigenvalues")) return 0;
	}
END

DIRECT (Transition_help) Melder_help (L"Transition"); END

FORM (Transition_power, L"Transition: Power...", 0)
	NATURAL (L"Power", L"2")
	OK
DO
	EVERY_TO (Transition_power ((structTransition *)OBJECT, GET_INTEGER (L"Power")))
END

DIRECT (Transition_to_Matrix)
	EVERY_TO (Transition_to_Matrix ((structTransition *)OBJECT))
END

/***** Praat menu *****/

FORM (Praat_test, L"Praat test", 0)
	OPTIONMENU_ENUM (L"Test", kPraatTests, DEFAULT)
	SENTENCE (L"arg1", L"1000000")
	SENTENCE (L"arg2", L"ui/editors/AmplitudeTierEditor.h")
	SENTENCE (L"arg3", L"ui/editors/AmplitudeTierEditor.h")
	SENTENCE (L"arg4", L"ui/editors/AmplitudeTierEditor.h")
	OK
DO
	Praat_tests (GET_INTEGER (L"Test"), GET_STRING (L"arg1"),
		GET_STRING (L"arg2"), GET_STRING (L"arg3"), GET_STRING (L"arg4"));
END

/***** Help menu *****/

DIRECT (ObjectWindow) Melder_help (L"Object window"); END
DIRECT (Intro) Melder_help (L"Intro"); END
DIRECT (WhatsNew) Melder_help (L"What's new?"); END
DIRECT (TypesOfObjects) Melder_help (L"Types of objects"); END
DIRECT (Editors) Melder_help (L"Editors"); END
DIRECT (FrequentlyAskedQuestions) Melder_help (L"FAQ (Frequently Asked Questions)"); END
DIRECT (Acknowledgments) Melder_help (L"Acknowledgments"); END
DIRECT (FormulasTutorial) Melder_help (L"Formulas"); END
DIRECT (ScriptingTutorial) Melder_help (L"Scripting"); END
DIRECT (DemoWindow) Melder_help (L"Demo window"); END
DIRECT (Programming) Melder_help (L"Programming with Praat"); END
DIRECT (SearchManual) Melder_search (); END

/***** file recognizers *****/

static Any cgnSyntaxFileRecognizer (int nread, const char *header, MelderFile file) {
	if (nread < 57) return NULL;
	if (! strnequ (& header [0], "<?xml version=\"1.0\"?>", 21) ||
	    (! strnequ (& header [22], "<!DOCTYPE ttext SYSTEM \"ttext.dtd\">", 35) &&
	     ! strnequ (& header [23], "<!DOCTYPE ttext SYSTEM \"ttext.dtd\">", 35))) return NULL;
	return TextGrid_readFromCgnSyntaxFile (file);
}

static Any chronologicalTextGridTextFileRecognizer (int nread, const char *header, MelderFile file) {
	if (nread < 100) return NULL;
	if (strnequ (& header [0], "\"Praat chronological TextGrid text file\"ui/editors/AmplitudeTierEditor.h", 40))
		return TextGrid_readFromChronologicalTextFile (file);
	char headerCopy [101];
	memcpy (headerCopy, header, 100);
	headerCopy [100] = '\0';
	for (int i = 0; i < 100; i ++)
		if (headerCopy [i] == '\0') headerCopy [i] = '\001';
	//if (strstr (headerCopy, "\"\001P\001r\001a\001a\001t\001 \001c\001h\001r\001o\001n\001o\001l\001o\001g\001i\001c\001a\001l\001"
	//	" \001T\001e\001x\001t\001G\001r\001i\001d\001 t\001"))
	if (strstr (headerCopy, "\"\001P\001r\001a\001a\001t\001 \001c\001h\001r\001o\001n\001o\001l\001o\001g\001i\001c\001a\001l\001"
		" \001T\001e\001x\001t\001G\001r\001i\001d\001 \001t\001e\001x\001t\001 \001f\001i\001l\001e\001\"ui/editors/AmplitudeTierEditor.h"))
	{
		return TextGrid_readFromChronologicalTextFile (file);
	}
	return NULL;
}

/***** buttons *****/

void praat_TableOfReal_init (void *klas);   // Buttons for TableOfReal and for its subclasses.

void praat_TimeFunction_query_init (void *klas);   // Query buttons for time-based subclasses of Function.
void praat_TimeFunction_query_init (void *klas) {
	praat_addAction1 (klas, 1, L"Query time domain", 0, 1, 0);
	praat_addAction1 (klas, 1, L"Get start time", 0, 2, DO_TimeFunction_getStartTime);
						praat_addAction1 (klas, 1, L"Get starting time", 0, praat_HIDDEN + praat_DEPTH_2, DO_TimeFunction_getStartTime);
	praat_addAction1 (klas, 1, L"Get end time", 0, 2, DO_TimeFunction_getEndTime);
						praat_addAction1 (klas, 1, L"Get finishing time", 0, praat_HIDDEN + praat_DEPTH_2, DO_TimeFunction_getEndTime);
	praat_addAction1 (klas, 1, L"Get total duration", 0, 2, DO_TimeFunction_getDuration);
						praat_addAction1 (klas, 1, L"Get duration", 0, praat_HIDDEN + praat_DEPTH_2, DO_TimeFunction_getDuration);
}

void praat_TimeFunction_modify_init (void *klas);   // Modify buttons for time-based subclasses of Function.
void praat_TimeFunction_modify_init (void *klas) {
	praat_addAction1 (klas, 0, L"Modify times", 0, 1, 0);
	praat_addAction1 (klas, 0, L"Shift times by...", 0, 2, DO_TimeFunction_shiftTimesBy);
	praat_addAction1 (klas, 0, L"Shift times to...", 0, 2, DO_TimeFunction_shiftTimesTo);
						praat_addAction1 (klas, 0, L"Shift to zero", 0, praat_HIDDEN + praat_DEPTH_2, DO_TimeFunction_shiftToZero);   // hidden 2008
	praat_addAction1 (klas, 0, L"Scale times by...", 0, 2, DO_TimeFunction_scaleTimesBy);
	praat_addAction1 (klas, 0, L"Scale times to...", 0, 2, DO_TimeFunction_scaleTimesTo);
						praat_addAction1 (klas, 0, L"Scale times...", 0, praat_HIDDEN + praat_DEPTH_2, DO_TimeFunction_scaleTimesTo);   // hidden 2008
}

extern "C" void praat_TimeFrameSampled_query_init (void *klas);   // Query buttons for frame-based time-based subclasses of Sampled.
void praat_TimeFrameSampled_query_init (void *klas) {
	praat_TimeFunction_query_init (klas);
	praat_addAction1 (klas, 1, L"Query time sampling", 0, 1, 0);
	praat_addAction1 (klas, 1, L"Get number of frames", 0, 2, DO_TimeFrameSampled_getNumberOfFrames);
	praat_addAction1 (klas, 1, L"Get time step", 0, 2, DO_TimeFrameSampled_getFrameLength);
						praat_addAction1 (klas, 1, L"Get frame length", 0, praat_HIDDEN + praat_DEPTH_2, DO_TimeFrameSampled_getFrameLength);
						praat_addAction1 (klas, 1, L"Get frame duration", 0, praat_HIDDEN + praat_DEPTH_2, DO_TimeFrameSampled_getFrameLength);
	praat_addAction1 (klas, 1, L"Get time from frame number...", 0, 2, DO_TimeFrameSampled_getTimeFromFrame);
						praat_addAction1 (klas, 1, L"Get time from frame...", 0, praat_HIDDEN + praat_DEPTH_2, DO_TimeFrameSampled_getTimeFromFrame);
	praat_addAction1 (klas, 1, L"Get frame number from time...", 0, 2, DO_TimeFrameSampled_getFrameFromTime);
						praat_addAction1 (klas, 1, L"Get frame from time...", 0, praat_HIDDEN + praat_DEPTH_2, DO_TimeFrameSampled_getFrameFromTime);
}

void praat_TimeTier_query_init (void *klas);   // Query buttons for time-based subclasses of AnyTier.
void praat_TimeTier_query_init (void *klas) {
	praat_TimeFunction_query_init (klas);
	praat_addAction1 (klas, 1, L"Get number of points", 0, 1, DO_TimeTier_getNumberOfPoints);
	praat_addAction1 (klas, 1, L"Get low index from time...", 0, 1, DO_TimeTier_getLowIndexFromTime);
	praat_addAction1 (klas, 1, L"Get high index from time...", 0, 1, DO_TimeTier_getHighIndexFromTime);
	praat_addAction1 (klas, 1, L"Get nearest index from time...", 0, 1, DO_TimeTier_getNearestIndexFromTime);
	praat_addAction1 (klas, 1, L"Get time from index...", 0, 1, DO_TimeTier_getTimeFromIndex);
}

void praat_TimeTier_modify_init (void *klas);   // Modification buttons for time-based subclasses of AnyTier.
void praat_TimeTier_modify_init (void *klas) {
	praat_TimeFunction_modify_init (klas);
	praat_addAction1 (klas, 0, L"Remove point...", 0, 1, DO_TimeTier_removePoint);
	praat_addAction1 (klas, 0, L"Remove point near...", 0, 1, DO_TimeTier_removePointNear);
	praat_addAction1 (klas, 0, L"Remove points between...", 0, 1, DO_TimeTier_removePointsBetween);
}

INCLUDE_LIBRARY (praat_uvafon_Sound_init)
INCLUDE_LIBRARY (praat_uvafon_TextGrid_init)
INCLUDE_LIBRARY (praat_uvafon_Stat_init)
INCLUDE_LIBRARY (praat_uvafon_Artsynth_init)
INCLUDE_LIBRARY (praat_uvafon_David_init)
INCLUDE_LIBRARY (praat_uvafon_gram_init)
INCLUDE_LIBRARY (praat_uvafon_FFNet_init)
INCLUDE_LIBRARY (praat_uvafon_LPC_init)
INCLUDE_LIBRARY (praat_uvafon_Exp_init)

void praat_uvafon_init (void);
void praat_uvafon_init (void) {
	Thing_recognizeClassesByName (classSound, classMatrix, classPolygon, classPointProcess, classParamCurve,
		classSpectrum, classLtas, classSpectrogram, classFormant,
		classExcitation, classCochleagram, classVocalTract, classFormantPoint, classFormantTier, classFormantGrid,
		classLabel, classTier, classAutosegment,   /* Three obsolete classes. */
		classIntensity, classPitch, classHarmonicity,
		classTransition,
		classRealPoint, classRealTier, classPitchTier, classIntensityTier, classDurationTier, classAmplitudeTier, classSpectrumTier,
		classManipulation, classTextPoint, classTextInterval, classTextTier,
		classIntervalTier, classTextGrid, classLongSound, classWordList, classSpellingChecker,
		NULL);
	Thing_recognizeClassByOtherName (classManipulation, L"Psola");
	Thing_recognizeClassByOtherName (classManipulation, L"Analysis");
	Thing_recognizeClassByOtherName (classPitchTier, L"StylPitch");

	Data_recognizeFileType (cgnSyntaxFileRecognizer);
	Data_recognizeFileType (chronologicalTextGridTextFileRecognizer);

	ManipulationEditor::prefs ();
	SpectrumEditor::prefs ();
	FormantGridEditor::prefs ();

	praat_uvafon_Sound_init();
	praat_uvafon_TextGrid_init();

	praat_addMenuCommand (L"Objects", L"Praat", L"Praat test...", 0, praat_HIDDEN, DO_Praat_test);

	praat_addMenuCommand (L"Objects", L"New", L"-- new numerics --", 0, 0, 0);
	praat_addMenuCommand (L"Objects", L"New", L"Matrix", 0, 0, 0);
		praat_addMenuCommand (L"Objects", L"New", L"Create Matrix...", 0, 1, DO_Matrix_create);
		praat_addMenuCommand (L"Objects", L"New", L"Create simple Matrix...", 0, 1, DO_Matrix_createSimple);
	praat_addMenuCommand (L"Objects", L"Open", L"-- read raw --", 0, 0, 0);
	praat_addMenuCommand (L"Objects", L"Open", L"Read Matrix from raw text file...", 0, 0, DO_Matrix_readFromRawTextFile);
	praat_addMenuCommand (L"Objects", L"Open", L"Read Matrix from LVS AP file...", 0, praat_HIDDEN, DO_Matrix_readAP);
	praat_addMenuCommand (L"Objects", L"Open", L"Read Strings from raw text file...", 0, 0, DO_Strings_readFromRawTextFile);

	praat_uvafon_Stat_init();

	praat_addMenuCommand (L"Objects", L"New", L"Tiers", 0, 0, 0);
		praat_addMenuCommand (L"Objects", L"New", L"Create empty PointProcess...", 0, 1, DO_PointProcess_createEmpty);
		praat_addMenuCommand (L"Objects", L"New", L"Create Poisson process...", 0, 1, DO_PointProcess_createPoissonProcess);
		praat_addMenuCommand (L"Objects", L"New", L"-- new tiers ---", 0, 1, 0);
		praat_addMenuCommand (L"Objects", L"New", L"Create PitchTier...", 0, 1, DO_PitchTier_create);
		praat_addMenuCommand (L"Objects", L"New", L"Create FormantGrid...", 0, 1, DO_FormantGrid_create);
		praat_addMenuCommand (L"Objects", L"New", L"Create FormantTier...", 0, praat_HIDDEN | praat_DEPTH_1, DO_FormantTier_create);
		praat_addMenuCommand (L"Objects", L"New", L"Create IntensityTier...", 0, 1, DO_IntensityTier_create);
		praat_addMenuCommand (L"Objects", L"New", L"Create DurationTier...", 0, 1, DO_DurationTier_create);
		praat_addMenuCommand (L"Objects", L"New", L"Create AmplitudeTier...", 0, 1, DO_AmplitudeTier_create);
	praat_addMenuCommand (L"Objects", L"New", L"-- new textgrid --", 0, 0, 0);
	praat_addMenuCommand (L"Objects", L"New", L"Create TextGrid...", 0, 0, DO_TextGrid_create);
	praat_addMenuCommand (L"Objects", L"New", L"Create Strings as file list...", 0, 0, DO_Strings_createAsFileList);
	praat_addMenuCommand (L"Objects", L"New", L"Create Strings as directory list...", 0, 0, DO_Strings_createAsDirectoryList);

	praat_addMenuCommand (L"Objects", L"Open", L"-- read tier --", 0, 0, 0);
	praat_addMenuCommand (L"Objects", L"Open", L"Read from special tier file...", 0, 0, 0);
		praat_addMenuCommand (L"Objects", L"Open", L"Read TextTier from Xwaves...", 0, 1, DO_TextTier_readFromXwaves);
		praat_addMenuCommand (L"Objects", L"Open", L"Read IntervalTier from Xwaves...", 0, 1, DO_IntervalTier_readFromXwaves);

	praat_addMenuCommand (L"Objects", L"ApplicationHelp", L"Praat Intro", 0, '?', DO_Intro);
	#ifndef macintosh
		praat_addMenuCommand (L"Objects", L"Help", L"Object window", 0, 0, DO_ObjectWindow);
	#endif
	praat_addMenuCommand (L"Objects", L"ApplicationHelp", L"Frequently asked questions", 0, 0, DO_FrequentlyAskedQuestions);
	praat_addMenuCommand (L"Objects", L"ApplicationHelp", L"What's new?", 0, 0, DO_WhatsNew);
	praat_addMenuCommand (L"Objects", L"ApplicationHelp", L"Types of objects", 0, 0, DO_TypesOfObjects);
	praat_addMenuCommand (L"Objects", L"ApplicationHelp", L"Editors", 0, 0, DO_Editors);
	praat_addMenuCommand (L"Objects", L"ApplicationHelp", L"Acknowledgments", 0, 0, DO_Acknowledgments);
	praat_addMenuCommand (L"Objects", L"ApplicationHelp", L"-- shell help --", 0, 0, 0);
	praat_addMenuCommand (L"Objects", L"ApplicationHelp", L"Formulas tutorial", 0, 0, DO_FormulasTutorial);
	praat_addMenuCommand (L"Objects", L"ApplicationHelp", L"Scripting tutorial", 0, 0, DO_ScriptingTutorial);
	praat_addMenuCommand (L"Objects", L"ApplicationHelp", L"Demo window", 0, 0, DO_DemoWindow);
	praat_addMenuCommand (L"Objects", L"ApplicationHelp", L"Programming", 0, 0, DO_Programming);
	#ifdef macintosh
		praat_addMenuCommand (L"Objects", L"Help", L"Praat Intro", 0, '?', DO_Intro);
		praat_addMenuCommand (L"Objects", L"Help", L"Object window help", 0, 0, DO_ObjectWindow);
		praat_addMenuCommand (L"Objects", L"Help", L"-- manual --", 0, 0, 0);
		praat_addMenuCommand (L"Objects", L"Help", L"Search Praat manual...", 0, 'M', DO_SearchManual);
	#endif

	praat_addAction1 (classAmplitudeTier, 0, L"AmplitudeTier help", 0, 0, DO_AmplitudeTier_help);
	praat_addAction1 (classAmplitudeTier, 1, L"View & Edit", 0, praat_ATTRACTIVE, DO_AmplitudeTier_edit);
	praat_addAction1 (classAmplitudeTier, 1, L"Edit", 0, praat_HIDDEN, DO_AmplitudeTier_edit);
	praat_addAction1 (classAmplitudeTier, 0, L"View & Edit with Sound?", 0, 0, DO_info_AmplitudeTier_Sound_edit);
	praat_addAction1 (classAmplitudeTier, 0, L"Query -", 0, 0, 0);
		praat_TimeTier_query_init (classAmplitudeTier);
		praat_addAction1 (classAmplitudeTier, 1, L"Get shimmer (local)...", 0, 1, DO_AmplitudeTier_getShimmer_local);
		praat_addAction1 (classAmplitudeTier, 1, L"Get shimmer (local_dB)...", 0, 1, DO_AmplitudeTier_getShimmer_local_dB);
		praat_addAction1 (classAmplitudeTier, 1, L"Get shimmer (apq3)...", 0, 1, DO_AmplitudeTier_getShimmer_apq3);
		praat_addAction1 (classAmplitudeTier, 1, L"Get shimmer (apq5)...", 0, 1, DO_AmplitudeTier_getShimmer_apq5);
		praat_addAction1 (classAmplitudeTier, 1, L"Get shimmer (apq11)...", 0, 1, DO_AmplitudeTier_getShimmer_apq11);
		praat_addAction1 (classAmplitudeTier, 1, L"Get shimmer (dda)...", 0, 1, DO_AmplitudeTier_getShimmer_dda);
	praat_addAction1 (classAmplitudeTier, 0, L"Modify -", 0, 0, 0);
		praat_TimeTier_modify_init (classAmplitudeTier);
		praat_addAction1 (classAmplitudeTier, 0, L"Add point...", 0, 1, DO_AmplitudeTier_addPoint);
		praat_addAction1 (classAmplitudeTier, 0, L"Formula...", 0, 1, DO_AmplitudeTier_formula);
praat_addAction1 (classAmplitudeTier, 0, L"Synthesize", 0, 0, 0);
	praat_addAction1 (classAmplitudeTier, 0, L"To Sound (pulse train)...", 0, 0, DO_AmplitudeTier_to_Sound);
praat_addAction1 (classAmplitudeTier, 0, L"Convert", 0, 0, 0);
	praat_addAction1 (classAmplitudeTier, 0, L"To IntensityTier...", 0, 0, DO_AmplitudeTier_to_IntensityTier);
	praat_addAction1 (classAmplitudeTier, 0, L"Down to PointProcess", 0, 0, DO_AmplitudeTier_downto_PointProcess);
	praat_addAction1 (classAmplitudeTier, 0, L"Down to TableOfReal", 0, 0, DO_AmplitudeTier_downto_TableOfReal);

	praat_addAction1 (classCochleagram, 0, L"Cochleagram help", 0, 0, DO_Cochleagram_help);
	praat_addAction1 (classCochleagram, 1, L"Movie", 0, 0, DO_Cochleagram_movie);
praat_addAction1 (classCochleagram, 0, L"Info", 0, 0, 0);
	praat_addAction1 (classCochleagram, 2, L"Difference...", 0, 0, DO_Cochleagram_difference);
praat_addAction1 (classCochleagram, 0, L"Draw", 0, 0, 0);
	praat_addAction1 (classCochleagram, 0, L"Paint...", 0, 0, DO_Cochleagram_paint);
praat_addAction1 (classCochleagram, 0, L"Modify", 0, 0, 0);
	praat_addAction1 (classCochleagram, 0, L"Formula...", 0, 0, DO_Cochleagram_formula);
praat_addAction1 (classCochleagram, 0, L"Analyse", 0, 0, 0);
	praat_addAction1 (classCochleagram, 0, L"To Excitation (slice)...", 0, 0, DO_Cochleagram_to_Excitation);
praat_addAction1 (classCochleagram, 0, L"Hack", 0, 0, 0);
	praat_addAction1 (classCochleagram, 0, L"To Matrix", 0, 0, DO_Cochleagram_to_Matrix);

praat_addAction1 (classDistributions, 0, L"Learn", 0, 0, 0);
	praat_addAction1 (classDistributions, 1, L"To Transition...", 0, 0, DO_Distributions_to_Transition);
	praat_addAction1 (classDistributions, 2, L"To Transition (noise)...", 0, 0, DO_Distributions_to_Transition_noise);

	praat_addAction1 (classDurationTier, 0, L"DurationTier help", 0, 0, DO_DurationTier_help);
	praat_addAction1 (classDurationTier, 1, L"View & Edit", 0, praat_ATTRACTIVE, DO_DurationTier_edit);
	praat_addAction1 (classDurationTier, 1, L"Edit", 0, praat_HIDDEN, DO_DurationTier_edit);
	praat_addAction1 (classDurationTier, 0, L"View & Edit with Sound?", 0, 0, DO_info_DurationTier_Sound_edit);
	praat_addAction1 (classDurationTier, 0, L"& Manipulation: Replace?", 0, 0, DO_info_DurationTier_Manipulation_replace);
	praat_addAction1 (classDurationTier, 0, L"Query -", 0, 0, 0);
		praat_TimeTier_query_init (classDurationTier);
		praat_addAction1 (classDurationTier, 1, L"-- get content --", 0, 1, 0);
		praat_addAction1 (classDurationTier, 1, L"Get target duration...", 0, 1, DO_DurationTier_getTargetDuration);
	praat_addAction1 (classDurationTier, 0, L"Modify -", 0, 0, 0);
		praat_TimeTier_modify_init (classDurationTier);
		praat_addAction1 (classDurationTier, 0, L"Add point...", 0, 1, DO_DurationTier_addPoint);
		praat_addAction1 (classDurationTier, 0, L"Formula...", 0, 1, DO_DurationTier_formula);
praat_addAction1 (classDurationTier, 0, L"Convert", 0, 0, 0);
	praat_addAction1 (classDurationTier, 0, L"Down to PointProcess", 0, 0, DO_DurationTier_downto_PointProcess);

	praat_addAction1 (classExcitation, 0, L"Excitation help", 0, 0, DO_Excitation_help);
praat_addAction1 (classExcitation, 0, L"Draw", 0, 0, 0);
	praat_addAction1 (classExcitation, 0, L"Draw...", 0, 0, DO_Excitation_draw);
praat_addAction1 (classExcitation, 0, L"Analyse", 0, 0, 0);
	praat_addAction1 (classExcitation, 0, L"To Formant...", 0, 0, DO_Excitation_to_Formant);
praat_addAction1 (classExcitation, 1, L"Query -", 0, 0, 0);
	praat_addAction1 (classExcitation, 1, L"Get loudness", 0, 0, DO_Excitation_getLoudness);
praat_addAction1 (classExcitation, 0, L"Modify", 0, 0, 0);
	praat_addAction1 (classExcitation, 0, L"Formula...", 0, 0, DO_Excitation_formula);
praat_addAction1 (classExcitation, 0, L"Hack", 0, 0, 0);
	praat_addAction1 (classExcitation, 0, L"To Matrix", 0, 0, DO_Excitation_to_Matrix);

	praat_addAction1 (classFormant, 0, L"Formant help", 0, 0, DO_Formant_help);
	praat_addAction1 (classFormant, 0, L"Draw -", 0, 0, 0);
		praat_addAction1 (classFormant, 0, L"Speckle...", 0, 1, DO_Formant_drawSpeckles);
		praat_addAction1 (classFormant, 0, L"Draw tracks...", 0, 1, DO_Formant_drawTracks);
		praat_addAction1 (classFormant, 0, L"Scatter plot...", 0, 1, DO_Formant_scatterPlot);
	praat_addAction1 (classFormant, 0, L"List...", 0, 0, DO_Formant_list);
	praat_addAction1 (classFormant, 0, L"Down to Table...", 0, 0, DO_Formant_downto_Table);
	praat_addAction1 (classFormant, 0, L"Query -", 0, 0, 0);
		praat_TimeFrameSampled_query_init (classFormant);
		praat_addAction1 (classFormant, 1, L"Get number of formants...", 0, 1, DO_Formant_getNumberOfFormants);
		praat_addAction1 (classFormant, 1, L"Get minimum number of formants", 0, 1, DO_Formant_getMinimumNumberOfFormants);
		praat_addAction1 (classFormant, 1, L"Get maximum number of formants", 0, 1, DO_Formant_getMaximumNumberOfFormants);
		praat_addAction1 (classFormant, 1, L"-- get value --", 0, 1, 0);
		praat_addAction1 (classFormant, 1, L"Get value at time...", 0, 1, DO_Formant_getValueAtTime);
		praat_addAction1 (classFormant, 1, L"Get bandwidth at time...", 0, 1, DO_Formant_getBandwidthAtTime);
		praat_addAction1 (classFormant, 1, L"-- get extreme --", 0, 1, 0);
		praat_addAction1 (classFormant, 1, L"Get minimum...", 0, 1, DO_Formant_getMinimum);
		praat_addAction1 (classFormant, 1, L"Get time of minimum...", 0, 1, DO_Formant_getTimeOfMinimum);
		praat_addAction1 (classFormant, 1, L"Get maximum...", 0, 1, DO_Formant_getMaximum);
		praat_addAction1 (classFormant, 1, L"Get time of maximum...", 0, 1, DO_Formant_getTimeOfMaximum);
		praat_addAction1 (classFormant, 1, L"-- get distribution --", 0, 1, 0);
		praat_addAction1 (classFormant, 1, L"Get quantile...", 0, 1, DO_Formant_getQuantile);
		praat_addAction1 (classFormant, 1, L"Get quantile of bandwidth...", 0, 1, DO_Formant_getQuantileOfBandwidth);
		praat_addAction1 (classFormant, 1, L"Get mean...", 0, 1, DO_Formant_getMean);
		praat_addAction1 (classFormant, 1, L"Get standard deviation...", 0, 1, DO_Formant_getStandardDeviation);
	praat_addAction1 (classFormant, 0, L"Modify -", 0, 0, 0);
		praat_TimeFunction_modify_init (classFormant);
		praat_addAction1 (classFormant, 0, L"Sort", 0, 1, DO_Formant_sort);
		praat_addAction1 (classFormant, 0, L"Formula (frequencies)...", 0, 1, DO_Formant_formula_frequencies);
		praat_addAction1 (classFormant, 0, L"Formula (bandwidths)...", 0, 1, DO_Formant_formula_bandwidths);
praat_addAction1 (classFormant, 0, L"Convert", 0, 0, 0);
	praat_addAction1 (classFormant, 0, L"Track...", 0, 0, DO_Formant_tracker);
	praat_addAction1 (classFormant, 0, L"Down to FormantTier", 0, praat_HIDDEN, DO_Formant_downto_FormantTier);
	praat_addAction1 (classFormant, 0, L"Down to FormantGrid", 0, 0, DO_Formant_downto_FormantGrid);
praat_addAction1 (classFormant, 0, L"Hack", 0, 0, 0);
	praat_addAction1 (classFormant, 0, L"To Matrix...", 0, 0, DO_Formant_to_Matrix);

	praat_addAction1 (classFormantGrid, 0, L"FormantGrid help", 0, 0, DO_FormantGrid_help);
	praat_addAction1 (classFormantGrid, 1, L"View & Edit", 0, praat_ATTRACTIVE, DO_FormantGrid_edit);
	praat_addAction1 (classFormantGrid, 1, L"Edit", 0, praat_HIDDEN, DO_FormantGrid_edit);
	praat_addAction1 (classFormantGrid, 0, L"Modify -", 0, 0, 0);
		praat_TimeFunction_modify_init (classFormantGrid);
		praat_addAction1 (classFormantGrid, 0, L"Formula (frequencies)...", 0, 1, DO_FormantGrid_formula_frequencies);
		//praat_addAction1 (classFormantGrid, 0, L"Formula (bandwidths)...", 0, 1, DO_FormantGrid_formula_bandwidths);
		praat_addAction1 (classFormantGrid, 0, L"Add formant point...", 0, 1, DO_FormantGrid_addFormantPoint);
		praat_addAction1 (classFormantGrid, 0, L"Add bandwidth point...", 0, 1, DO_FormantGrid_addBandwidthPoint);
		praat_addAction1 (classFormantGrid, 0, L"Remove formant points between...", 0, 1, DO_FormantGrid_removeFormantPointsBetween);
		praat_addAction1 (classFormantGrid, 0, L"Remove bandwidth points between...", 0, 1, DO_FormantGrid_removeBandwidthPointsBetween);
	praat_addAction1 (classFormantGrid, 0, L"Convert -", 0, 0, 0);
		praat_addAction1 (classFormantGrid, 0, L"To Formant...", 0, 1, DO_FormantGrid_to_Formant);

	praat_addAction1 (classFormantTier, 0, L"FormantTier help", 0, 0, DO_FormantTier_help);
	praat_addAction1 (classFormantTier, 0, L"Draw -", 0, 0, 0);
		praat_addAction1 (classFormantTier, 0, L"Speckle...", 0, 1, DO_FormantTier_speckle);
	praat_addAction1 (classFormantTier, 0, L"Query -", 0, 0, 0);
		praat_TimeTier_query_init (classFormantTier);
		praat_addAction1 (classFormantTier, 1, L"-- get value --", 0, 1, 0);
		praat_addAction1 (classFormantTier, 1, L"Get value at time...", 0, 1, DO_FormantTier_getValueAtTime);
		praat_addAction1 (classFormantTier, 1, L"Get bandwidth at time...", 0, 1, DO_FormantTier_getBandwidthAtTime);
	praat_addAction1 (classFormantTier, 0, L"Modify -", 0, 0, 0);
		praat_TimeTier_modify_init (classFormantTier);
		praat_addAction1 (classFormantTier, 0, L"Add point...", 0, 1, DO_FormantTier_addPoint);
praat_addAction1 (classFormantTier, 0, L"Down", 0, 0, 0);
	praat_addAction1 (classFormantTier, 0, L"Down to TableOfReal...", 0, 0, DO_FormantTier_downto_TableOfReal);

	praat_addAction1 (classHarmonicity, 0, L"Harmonicity help", 0, 0, DO_Harmonicity_help);
	praat_addAction1 (classHarmonicity, 0, L"Draw", 0, 0, 0);
		praat_addAction1 (classHarmonicity, 0, L"Draw...", 0, 0, DO_Harmonicity_draw);
	praat_addAction1 (classHarmonicity, 1, L"Query -", 0, 0, 0);
		praat_TimeFrameSampled_query_init (classHarmonicity);
		praat_addAction1 (classHarmonicity, 1, L"-- get content --", 0, 1, 0);
		praat_addAction1 (classHarmonicity, 1, L"Get value at time...", 0, 1, DO_Harmonicity_getValueAtTime);
		praat_addAction1 (classHarmonicity, 1, L"Get value in frame...", 0, 1, DO_Harmonicity_getValueInFrame);
		praat_addAction1 (classHarmonicity, 1, L"-- get extreme --", 0, 1, 0);
		praat_addAction1 (classHarmonicity, 1, L"Get minimum...", 0, 1, DO_Harmonicity_getMinimum);
		praat_addAction1 (classHarmonicity, 1, L"Get time of minimum...", 0, 1, DO_Harmonicity_getTimeOfMinimum);
		praat_addAction1 (classHarmonicity, 1, L"Get maximum...", 0, 1, DO_Harmonicity_getMaximum);
		praat_addAction1 (classHarmonicity, 1, L"Get time of maximum...", 0, 1, DO_Harmonicity_getTimeOfMaximum);
		praat_addAction1 (classHarmonicity, 1, L"-- get statistics --", 0, 1, 0);
		praat_addAction1 (classHarmonicity, 1, L"Get mean...", 0, 1, DO_Harmonicity_getMean);
		praat_addAction1 (classHarmonicity, 1, L"Get standard deviation...", 0, 1, DO_Harmonicity_getStandardDeviation);
	praat_addAction1 (classHarmonicity, 0, L"Modify", 0, 0, 0);
		praat_TimeFunction_modify_init (classHarmonicity);
		praat_addAction1 (classHarmonicity, 0, L"Formula...", 0, 0, DO_Harmonicity_formula);
	praat_addAction1 (classHarmonicity, 0, L"Hack", 0, 0, 0);
		praat_addAction1 (classHarmonicity, 0, L"To Matrix", 0, 0, DO_Harmonicity_to_Matrix);

	praat_addAction1 (classIntensity, 0, L"Intensity help", 0, 0, DO_Intensity_help);
	praat_addAction1 (classIntensity, 0, L"Draw...", 0, 0, DO_Intensity_draw);
	praat_addAction1 (classIntensity, 1, L"Query -", 0, 0, 0);
		praat_TimeFrameSampled_query_init (classIntensity);
		praat_addAction1 (classIntensity, 1, L"-- get content --", 0, 1, 0);
		praat_addAction1 (classIntensity, 1, L"Get value at time...", 0, 1, DO_Intensity_getValueAtTime);
		praat_addAction1 (classIntensity, 1, L"Get value in frame...", 0, 1, DO_Intensity_getValueInFrame);
		praat_addAction1 (classIntensity, 1, L"-- get extreme --", 0, 1, 0);
		praat_addAction1 (classIntensity, 1, L"Get minimum...", 0, 1, DO_Intensity_getMinimum);
		praat_addAction1 (classIntensity, 1, L"Get time of minimum...", 0, 1, DO_Intensity_getTimeOfMinimum);
		praat_addAction1 (classIntensity, 1, L"Get maximum...", 0, 1, DO_Intensity_getMaximum);
		praat_addAction1 (classIntensity, 1, L"Get time of maximum...", 0, 1, DO_Intensity_getTimeOfMaximum);
		praat_addAction1 (classIntensity, 1, L"-- get statistics --", 0, 1, 0);
		praat_addAction1 (classIntensity, 1, L"Get quantile...", 0, 1, DO_Intensity_getQuantile);
		praat_addAction1 (classIntensity, 1, L"Get mean...", 0, 1, DO_Intensity_getMean);
		praat_addAction1 (classIntensity, 1, L"Get standard deviation...", 0, 1, DO_Intensity_getStandardDeviation);
	praat_addAction1 (classIntensity, 0, L"Modify", 0, 0, 0);
		praat_TimeFunction_modify_init (classIntensity);
		praat_addAction1 (classIntensity, 0, L"Formula...", 0, 0, DO_Intensity_formula);
	praat_addAction1 (classIntensity, 0, L"Analyse", 0, 0, 0);
		praat_addAction1 (classIntensity, 0, L"To IntensityTier (peaks)", 0, 0, DO_Intensity_to_IntensityTier_peaks);
		praat_addAction1 (classIntensity, 0, L"To IntensityTier (valleys)", 0, 0, DO_Intensity_to_IntensityTier_valleys);
	praat_addAction1 (classIntensity, 0, L"Convert", 0, 0, 0);
		praat_addAction1 (classIntensity, 0, L"Down to IntensityTier", 0, 0, DO_Intensity_downto_IntensityTier);
		praat_addAction1 (classIntensity, 0, L"Down to Matrix", 0, 0, DO_Intensity_downto_Matrix);

	praat_addAction1 (classIntensityTier, 0, L"IntensityTier help", 0, 0, DO_IntensityTier_help);
	praat_addAction1 (classIntensityTier, 1, L"View & Edit", 0, praat_ATTRACTIVE, DO_IntensityTier_edit);
	praat_addAction1 (classIntensityTier, 1, L"Edit", 0, praat_HIDDEN, DO_IntensityTier_edit);
	praat_addAction1 (classIntensityTier, 0, L"View & Edit with Sound?", 0, 0, DO_info_IntensityTier_Sound_edit);
	praat_addAction1 (classIntensityTier, 0, L"Query -", 0, 0, 0);
		praat_TimeTier_query_init (classIntensityTier);
		praat_addAction1 (classIntensityTier, 1, L"-- get content --", 0, 1, 0);
		praat_addAction1 (classIntensityTier, 1, L"Get value at time...", 0, 1, DO_IntensityTier_getValueAtTime);
		praat_addAction1 (classIntensityTier, 1, L"Get value at index...", 0, 1, DO_IntensityTier_getValueAtIndex);
	praat_addAction1 (classIntensityTier, 0, L"Modify -", 0, 0, 0);
		praat_TimeTier_modify_init (classIntensityTier);
		praat_addAction1 (classIntensityTier, 0, L"Add point...", 0, 1, DO_IntensityTier_addPoint);
		praat_addAction1 (classIntensityTier, 0, L"Formula...", 0, 1, DO_IntensityTier_formula);
praat_addAction1 (classIntensityTier, 0, L"Convert", 0, 0, 0);
	praat_addAction1 (classIntensityTier, 0, L"To AmplitudeTier", 0, 0, DO_IntensityTier_to_AmplitudeTier);
	praat_addAction1 (classIntensityTier, 0, L"Down to PointProcess", 0, 0, DO_IntensityTier_downto_PointProcess);
	praat_addAction1 (classIntensityTier, 0, L"Down to TableOfReal", 0, 0, DO_IntensityTier_downto_TableOfReal);

	praat_addAction1 (classLtas, 0, L"Ltas help", 0, 0, DO_Ltas_help);
	praat_addAction1 (classLtas, 0, L"Draw...", 0, 0, DO_Ltas_draw);
	praat_addAction1 (classLtas, 1, L"Query -", 0, 0, 0);
		praat_addAction1 (classLtas, 1, L"Frequency domain", 0, 1, 0);
		praat_addAction1 (classLtas, 1, L"Get lowest frequency", 0, 2, DO_Ltas_getLowestFrequency);
		praat_addAction1 (classLtas, 1, L"Get highest frequency", 0, 2, DO_Ltas_getHighestFrequency);
		praat_addAction1 (classLtas, 1, L"Frequency sampling", 0, 1, 0);
		praat_addAction1 (classLtas, 1, L"Get number of bins", 0, 2, DO_Ltas_getNumberOfBins);
			praat_addAction1 (classLtas, 1, L"Get number of bands", 0, praat_HIDDEN + praat_DEPTH_2, DO_Ltas_getNumberOfBins);
		praat_addAction1 (classLtas, 1, L"Get bin width", 0, 2, DO_Ltas_getBinWidth);
			praat_addAction1 (classLtas, 1, L"Get band width", 0, praat_HIDDEN + praat_DEPTH_2, DO_Ltas_getBinWidth);
		praat_addAction1 (classLtas, 1, L"Get frequency from bin number...", 0, 2, DO_Ltas_getFrequencyFromBinNumber);
			praat_addAction1 (classLtas, 1, L"Get frequency from band...", 0, praat_HIDDEN + praat_DEPTH_2, DO_Ltas_getFrequencyFromBinNumber);
		praat_addAction1 (classLtas, 1, L"Get bin number from frequency...", 0, 2, DO_Ltas_getBinNumberFromFrequency);
			praat_addAction1 (classLtas, 1, L"Get band from frequency...", 0, praat_HIDDEN + praat_DEPTH_2, DO_Ltas_getBinNumberFromFrequency);
		praat_addAction1 (classLtas, 1, L"-- get content --", 0, 1, 0);
		praat_addAction1 (classLtas, 1, L"Get value at frequency...", 0, 1, DO_Ltas_getValueAtFrequency);
		praat_addAction1 (classLtas, 1, L"Get value in bin...", 0, 1, DO_Ltas_getValueInBin);
			praat_addAction1 (classLtas, 1, L"Get value in band...", 0, praat_HIDDEN + praat_DEPTH_1, DO_Ltas_getValueInBin);
		praat_addAction1 (classLtas, 1, L"-- get extreme --", 0, 1, 0);
		praat_addAction1 (classLtas, 1, L"Get minimum...", 0, 1, DO_Ltas_getMinimum);
		praat_addAction1 (classLtas, 1, L"Get frequency of minimum...", 0, 1, DO_Ltas_getFrequencyOfMinimum);
		praat_addAction1 (classLtas, 1, L"Get maximum...", 0, 1, DO_Ltas_getMaximum);
		praat_addAction1 (classLtas, 1, L"Get frequency of maximum...", 0, 1, DO_Ltas_getFrequencyOfMaximum);
		praat_addAction1 (classLtas, 1, L"-- get statistics --", 0, 1, 0);
		praat_addAction1 (classLtas, 1, L"Get mean...", 0, 1, DO_Ltas_getMean);
		praat_addAction1 (classLtas, 1, L"Get slope...", 0, 1, DO_Ltas_getSlope);
		praat_addAction1 (classLtas, 1, L"Get local peak height...", 0, 1, DO_Ltas_getLocalPeakHeight);
		praat_addAction1 (classLtas, 1, L"Get standard deviation...", 0, 1, DO_Ltas_getStandardDeviation);
	praat_addAction1 (classLtas, 0, L"Modify", 0, 0, 0);
	praat_addAction1 (classLtas, 0, L"Formula...", 0, 0, DO_Ltas_formula);
	praat_addAction1 (classLtas, 0, L"Analyse", 0, 0, 0);
	praat_addAction1 (classLtas, 0, L"To SpectrumTier (peaks)", 0, 0, DO_Ltas_to_SpectrumTier_peaks);
	praat_addAction1 (classLtas, 0, L"Convert", 0, 0, 0);
	praat_addAction1 (classLtas, 0, L"Compute trend line...", 0, 0, DO_Ltas_computeTrendLine);
	praat_addAction1 (classLtas, 0, L"Subtract trend line...", 0, 0, DO_Ltas_subtractTrendLine);
	praat_addAction1 (classLtas, 0, L"Combine", 0, 0, 0);
	praat_addAction1 (classLtas, 0, L"Merge", 0, praat_HIDDEN, DO_Ltases_merge);
	praat_addAction1 (classLtas, 0, L"Average", 0, 0, DO_Ltases_average);
	praat_addAction1 (classLtas, 0, L"Hack", 0, 0, 0);
	praat_addAction1 (classLtas, 0, L"To Matrix", 0, 0, DO_Ltas_to_Matrix);

	praat_addAction1 (classManipulation, 0, L"Manipulation help", 0, 0, DO_Manipulation_help);
	praat_addAction1 (classManipulation, 1, L"View & Edit", 0, praat_ATTRACTIVE, DO_Manipulation_edit);
	praat_addAction1 (classManipulation, 1, L"Edit", 0, praat_HIDDEN, DO_Manipulation_edit);
	praat_addAction1 (classManipulation, 0, L"Play (overlap-add)", 0, 0, DO_Manipulation_play_overlapAdd);
	praat_addAction1 (classManipulation, 0, L"Play (PSOLA)", 0, praat_HIDDEN, DO_Manipulation_play_overlapAdd);
	praat_addAction1 (classManipulation, 0, L"Play (LPC)", 0, 0, DO_Manipulation_play_lpc);
	praat_addAction1 (classManipulation, 0, L"Get resynthesis (overlap-add)", 0, 0, DO_Manipulation_getResynthesis_overlapAdd);
	praat_addAction1 (classManipulation, 0, L"Get resynthesis (PSOLA)", 0, praat_HIDDEN, DO_Manipulation_getResynthesis_overlapAdd);
	praat_addAction1 (classManipulation, 0, L"Get resynthesis (LPC)", 0, 0, DO_Manipulation_getResynthesis_lpc);
	praat_addAction1 (classManipulation, 0, L"Extract original sound", 0, 0, DO_Manipulation_extractOriginalSound);
	praat_addAction1 (classManipulation, 0, L"Extract pulses", 0, 0, DO_Manipulation_extractPulses);
	praat_addAction1 (classManipulation, 0, L"Extract pitch tier", 0, 0, DO_Manipulation_extractPitchTier);
	praat_addAction1 (classManipulation, 0, L"Extract duration tier", 0, 0, DO_Manipulation_extractDurationTier);
	praat_addAction1 (classManipulation, 0, L"Modify -", 0, 0, 0);
		praat_TimeFunction_modify_init (classManipulation);
		praat_addAction1 (classManipulation, 0, L"Replace pitch tier?", 0, 1, DO_Manipulation_replacePitchTier_help);
		praat_addAction1 (classManipulation, 0, L"Replace duration tier?", 0, 1, DO_Manipulation_replaceDurationTier_help);
	praat_addAction1 (classManipulation, 1, L"Save as text file without Sound...", 0, 0, DO_Manipulation_writeToTextFileWithoutSound);
	praat_addAction1 (classManipulation, 1, L"Write to text file without Sound...", 0, praat_HIDDEN, DO_Manipulation_writeToTextFileWithoutSound);
	praat_addAction1 (classManipulation, 1, L"Save as binary file without Sound...", 0, 0, DO_Manipulation_writeToBinaryFileWithoutSound);
	praat_addAction1 (classManipulation, 1, L"Write to binary file without Sound...", 0, praat_HIDDEN, DO_Manipulation_writeToBinaryFileWithoutSound);

	praat_addAction1 (classMatrix, 0, L"Matrix help", 0, 0, DO_Matrix_help);
	praat_addAction1 (classMatrix, 1, L"Save as matrix text file...", 0, 0, DO_Matrix_writeToMatrixTextFile);
	praat_addAction1 (classMatrix, 1, L"Write to matrix text file...", 0, praat_HIDDEN, DO_Matrix_writeToMatrixTextFile);
	praat_addAction1 (classMatrix, 1, L"Save as headerless spreadsheet file...", 0, 0, DO_Matrix_writeToHeaderlessSpreadsheetFile);
	praat_addAction1 (classMatrix, 1, L"Write to headerless spreadsheet file...", 0, praat_HIDDEN, DO_Matrix_writeToHeaderlessSpreadsheetFile);
	praat_addAction1 (classMatrix, 1, L"Play movie", 0, 0, DO_Matrix_movie);
	praat_addAction1 (classMatrix, 0, L"Draw -", 0, 0, 0);
		praat_addAction1 (classMatrix, 0, L"Draw rows...", 0, 1, DO_Matrix_drawRows);
		praat_addAction1 (classMatrix, 0, L"Draw one contour...", 0, 1, DO_Matrix_drawOneContour);
		praat_addAction1 (classMatrix, 0, L"Draw contours...", 0, 1, DO_Matrix_drawContours);
		praat_addAction1 (classMatrix, 0, L"Paint image...", 0, 1, DO_Matrix_paintImage);
		praat_addAction1 (classMatrix, 0, L"Paint contours...", 0, 1, DO_Matrix_paintContours);
		praat_addAction1 (classMatrix, 0, L"Paint cells...", 0, 1, DO_Matrix_paintCells);
		praat_addAction1 (classMatrix, 0, L"Paint surface...", 0, 1, DO_Matrix_paintSurface);
	praat_addAction1 (classMatrix, 1, L"Query -", 0, 0, 0);
		praat_addAction1 (classMatrix, 1, L"Get lowest x", 0, 1, DO_Matrix_getLowestX);
		praat_addAction1 (classMatrix, 1, L"Get highest x", 0, 1, DO_Matrix_getHighestX);
		praat_addAction1 (classMatrix, 1, L"Get lowest y", 0, 1, DO_Matrix_getLowestY);
		praat_addAction1 (classMatrix, 1, L"Get highest y", 0, 1, DO_Matrix_getHighestY);
		praat_addAction1 (classMatrix, 1, L"-- get structure --", 0, 1, 0);
		praat_addAction1 (classMatrix, 1, L"Get number of rows", 0, 1, DO_Matrix_getNumberOfRows);
		praat_addAction1 (classMatrix, 1, L"Get number of columns", 0, 1, DO_Matrix_getNumberOfColumns);
		praat_addAction1 (classMatrix, 1, L"Get row distance", 0, 1, DO_Matrix_getRowDistance);
		praat_addAction1 (classMatrix, 1, L"Get column distance", 0, 1, DO_Matrix_getColumnDistance);
		praat_addAction1 (classMatrix, 1, L"Get y of row...", 0, 1, DO_Matrix_getYofRow);
		praat_addAction1 (classMatrix, 1, L"Get x of column...", 0, 1, DO_Matrix_getXofColumn);
		praat_addAction1 (classMatrix, 1, L"-- get value --", 0, 1, 0);
		praat_addAction1 (classMatrix, 1, L"Get value in cell...", 0, 1, DO_Matrix_getValueInCell);
		praat_addAction1 (classMatrix, 1, L"Get value at xy...", 0, 1, DO_Matrix_getValueAtXY);
		praat_addAction1 (classMatrix, 1, L"Get minimum", 0, 1, DO_Matrix_getMinimum);
		praat_addAction1 (classMatrix, 1, L"Get maximum", 0, 1, DO_Matrix_getMaximum);
		praat_addAction1 (classMatrix, 1, L"Get sum", 0, 1, DO_Matrix_getSum);
	praat_addAction1 (classMatrix, 0, L"Modify -", 0, 0, 0);
		praat_addAction1 (classMatrix, 0, L"Formula...", 0, 1, DO_Matrix_formula);
		praat_addAction1 (classMatrix, 0, L"Set value...", 0, 1, DO_Matrix_setValue);
praat_addAction1 (classMatrix, 0, L"Analyse", 0, 0, 0);
	praat_addAction1 (classMatrix, 0, L"Eigen", 0, 0, DO_Matrix_eigen);
	praat_addAction1 (classMatrix, 0, L"Synthesize", 0, 0, 0);
	praat_addAction1 (classMatrix, 0, L"Power...", 0, 0, DO_Matrix_power);
	praat_addAction1 (classMatrix, 0, L"Combine two Matrices -", 0, 0, 0);
		praat_addAction1 (classMatrix, 2, L"Merge (append rows)", 0, 1, DO_Matrix_appendRows);
		praat_addAction1 (classMatrix, 2, L"To ParamCurve", 0, 1, DO_Matrix_to_ParamCurve);
	praat_addAction1 (classMatrix, 0, L"Cast -", 0, 0, 0);
		praat_addAction1 (classMatrix, 0, L"To Cochleagram", 0, 1, DO_Matrix_to_Cochleagram);
		praat_addAction1 (classMatrix, 0, L"To Excitation", 0, 1, DO_Matrix_to_Excitation);
		praat_addAction1 (classMatrix, 0, L"To Harmonicity", 0, 1, DO_Matrix_to_Harmonicity);
		praat_addAction1 (classMatrix, 0, L"To Intensity", 0, 1, DO_Matrix_to_Intensity);
		praat_addAction1 (classMatrix, 0, L"To Ltas", 0, 1, DO_Matrix_to_Ltas);
		praat_addAction1 (classMatrix, 0, L"To Pitch", 0, 1, DO_Matrix_to_Pitch);
		praat_addAction1 (classMatrix, 0, L"To PointProcess", 0, 1, DO_Matrix_to_PointProcess);
		praat_addAction1 (classMatrix, 0, L"To Polygon", 0, 1, DO_Matrix_to_Polygon);
		praat_addAction1 (classMatrix, 0, L"To Sound (slice)...", 0, 1, DO_Matrix_to_Sound_mono);
		praat_addAction1 (classMatrix, 0, L"To Spectrogram", 0, 1, DO_Matrix_to_Spectrogram);
		praat_addAction1 (classMatrix, 0, L"To TableOfReal", 0, 1, DO_Matrix_to_TableOfReal);
		praat_addAction1 (classMatrix, 0, L"To Spectrum", 0, 1, DO_Matrix_to_Spectrum);
		praat_addAction1 (classMatrix, 0, L"To Transition", 0, 1, DO_Matrix_to_Transition);
		praat_addAction1 (classMatrix, 0, L"To VocalTract", 0, 1, DO_Matrix_to_VocalTract);

	praat_addAction1 (classParamCurve, 0, L"ParamCurve help", 0, 0, DO_ParamCurve_help);
	praat_addAction1 (classParamCurve, 0, L"Draw", 0, 0, 0);
	praat_addAction1 (classParamCurve, 0, L"Draw...", 0, 0, DO_ParamCurve_draw);

	praat_addAction1 (classPitch, 0, L"Pitch help", 0, 0, DO_Pitch_help);
	praat_addAction1 (classPitch, 1, L"View & Edit", 0, praat_ATTRACTIVE, DO_Pitch_edit);
	praat_addAction1 (classPitch, 1, L"Edit", 0, praat_HIDDEN, DO_Pitch_edit);
	praat_addAction1 (classPitch, 0, L"Play -", 0, 0, 0);
		praat_addAction1 (classPitch, 0, L"Play pulses", 0, 1, DO_Pitch_play);
		praat_addAction1 (classPitch, 0, L"Hum", 0, 1, DO_Pitch_hum);
	praat_addAction1 (classPitch, 0, L"Draw -", 0, 0, 0);
		praat_addAction1 (classPitch, 0, L"Draw...", 0, 1, DO_Pitch_draw);
		praat_addAction1 (classPitch, 0, L"Draw logarithmic...", 0, 1, DO_Pitch_drawLogarithmic);
		praat_addAction1 (classPitch, 0, L"Draw semitones...", 0, 1, DO_Pitch_drawSemitones);
		praat_addAction1 (classPitch, 0, L"Draw mel...", 0, 1, DO_Pitch_drawMel);
		praat_addAction1 (classPitch, 0, L"Draw erb...", 0, 1, DO_Pitch_drawErb);
		praat_addAction1 (classPitch, 0, L"Speckle...", 0, 1, DO_Pitch_speckle);
		praat_addAction1 (classPitch, 0, L"Speckle logarithmic...", 0, 1, DO_Pitch_speckleLogarithmic);
		praat_addAction1 (classPitch, 0, L"Speckle semitones...", 0, 1, DO_Pitch_speckleSemitones);
		praat_addAction1 (classPitch, 0, L"Speckle mel...", 0, 1, DO_Pitch_speckleMel);
		praat_addAction1 (classPitch, 0, L"Speckle erb...", 0, 1, DO_Pitch_speckleErb);
	praat_addAction1 (classPitch, 0, L"Query -", 0, 0, 0);
		praat_TimeFrameSampled_query_init (classPitch);
		praat_addAction1 (classPitch, 1, L"-- get content --", 0, 1, 0);
		praat_addAction1 (classPitch, 1, L"Count voiced frames", 0, 1, DO_Pitch_getNumberOfVoicedFrames);
		praat_addAction1 (classPitch, 1, L"Get value at time...", 0, 1, DO_Pitch_getValueAtTime);
		praat_addAction1 (classPitch, 1, L"Get value in frame...", 0, 1, DO_Pitch_getValueInFrame);
		praat_addAction1 (classPitch, 1, L"-- get extreme --", 0, 1, 0);
		praat_addAction1 (classPitch, 1, L"Get minimum...", 0, 1, DO_Pitch_getMinimum);
		praat_addAction1 (classPitch, 1, L"Get time of minimum...", 0, 1, DO_Pitch_getTimeOfMinimum);
		praat_addAction1 (classPitch, 1, L"Get maximum...", 0, 1, DO_Pitch_getMaximum);
		praat_addAction1 (classPitch, 1, L"Get time of maximum...", 0, 1, DO_Pitch_getTimeOfMaximum);
		praat_addAction1 (classPitch, 1, L"-- get statistics --", 0, 1, 0);
		praat_addAction1 (classPitch, 1, L"Get quantile...", 0, 1, DO_Pitch_getQuantile);
		/*praat_addAction1 (classPitch, 1, L"Get spreading...", 0, 1, DO_Pitch_getSpreading);*/
		praat_addAction1 (classPitch, 1, L"Get mean...", 0, 1, DO_Pitch_getMean);
		praat_addAction1 (classPitch, 1, L"Get standard deviation...", 0, 1, DO_Pitch_getStandardDeviation);
		praat_addAction1 (classPitch, 1, L"-- get slope --", 0, 1, 0);
		praat_addAction1 (classPitch, 1, L"Get mean absolute slope...", 0, 1, DO_Pitch_getMeanAbsoluteSlope);
		praat_addAction1 (classPitch, 1, L"Get slope without octave jumps", 0, 1, DO_Pitch_getMeanAbsSlope_noOctave);
		praat_addAction1 (classPitch, 2, L"-- query two --", 0, 1, 0);
		praat_addAction1 (classPitch, 2, L"Count differences", 0, 1, DO_Pitch_difference);
	praat_addAction1 (classPitch, 0, L"Modify -", 0, 0, 0);
		praat_TimeFunction_modify_init (classPitch);
		praat_addAction1 (classPitch, 0, L"Formula...", 0, 1, DO_Pitch_formula);
	praat_addAction1 (classPitch, 0, L"Annotate -", 0, 0, 0);
		praat_addAction1 (classPitch, 0, L"To TextGrid...", 0, 1, DO_Pitch_to_TextGrid);
		praat_addAction1 (classPitch, 0, L"-- to single tier --", 0, 1, 0);
		praat_addAction1 (classPitch, 0, L"To TextTier", 0, 1, DO_Pitch_to_TextTier);
		praat_addAction1 (classPitch, 0, L"To IntervalTier", 0, 1, DO_Pitch_to_IntervalTier);
praat_addAction1 (classPitch, 0, L"Analyse", 0, 0, 0);
	praat_addAction1 (classPitch, 0, L"To PointProcess", 0, 0, DO_Pitch_to_PointProcess);
praat_addAction1 (classPitch, 0, L"Synthesize", 0, 0, 0);
	praat_addAction1 (classPitch, 0, L"To Sound (pulses)", 0, 0, DO_Pitch_to_Sound_pulses);
	praat_addAction1 (classPitch, 0, L"To Sound (hum)", 0, 0, DO_Pitch_to_Sound_hum);
	praat_addAction1 (classPitch, 0, L"To Sound (sine)...", 0, 1, DO_Pitch_to_Sound_sine);
praat_addAction1 (classPitch, 0, L"Convert", 0, 0, 0);
	praat_addAction1 (classPitch, 0, L"Kill octave jumps", 0, 0, DO_Pitch_killOctaveJumps);
	praat_addAction1 (classPitch, 0, L"Interpolate", 0, 0, DO_Pitch_interpolate);
	praat_addAction1 (classPitch, 0, L"Smooth...", 0, 0, DO_Pitch_smooth);
	praat_addAction1 (classPitch, 0, L"Subtract linear fit...", 0, 0, DO_Pitch_subtractLinearFit);
	praat_addAction1 (classPitch, 0, L"Down to PitchTier", 0, 0, DO_Pitch_to_PitchTier);
	praat_addAction1 (classPitch, 0, L"To Matrix", 0, 0, DO_Pitch_to_Matrix);

	praat_addAction1 (classPitchTier, 1, L"Save as PitchTier spreadsheet file...", 0, 0, DO_PitchTier_writeToPitchTierSpreadsheetFile);
	praat_addAction1 (classPitchTier, 1, L"Write to PitchTier spreadsheet file...", 0, praat_HIDDEN, DO_PitchTier_writeToPitchTierSpreadsheetFile);
	praat_addAction1 (classPitchTier, 1, L"Save as headerless spreadsheet file...", 0, 0, DO_PitchTier_writeToHeaderlessSpreadsheetFile);
	praat_addAction1 (classPitchTier, 1, L"Write to headerless spreadsheet file...", 0, praat_HIDDEN, DO_PitchTier_writeToHeaderlessSpreadsheetFile);
	praat_addAction1 (classPitchTier, 0, L"PitchTier help", 0, 0, DO_PitchTier_help);
	praat_addAction1 (classPitchTier, 1, L"View & Edit", 0, praat_ATTRACTIVE, DO_PitchTier_edit);
	praat_addAction1 (classPitchTier, 1, L"Edit", 0, praat_HIDDEN, DO_PitchTier_edit);
	praat_addAction1 (classPitchTier, 0, L"View & Edit with Sound?", 0, 0, DO_info_PitchTier_Sound_edit);
	praat_addAction1 (classPitchTier, 0, L"Play pulses", 0, 0, DO_PitchTier_play);
	praat_addAction1 (classPitchTier, 0, L"Hum", 0, 0, DO_PitchTier_hum);
	praat_addAction1 (classPitchTier, 0, L"Play sine", 0, 0, DO_PitchTier_playSine);
	praat_addAction1 (classPitchTier, 0, L"Draw...", 0, 0, DO_PitchTier_draw);
	praat_addAction1 (classPitchTier, 0, L"& Manipulation: Replace?", 0, 0, DO_info_PitchTier_Manipulation_replace);
	praat_addAction1 (classPitchTier, 0, L"Query -", 0, 0, 0);
		praat_TimeTier_query_init (classPitchTier);
		praat_addAction1 (classPitchTier, 1, L"-- get content --", 0, 1, 0);
		praat_addAction1 (classPitchTier, 1, L"Get value at time...", 0, 1, DO_PitchTier_getValueAtTime);
		praat_addAction1 (classPitchTier, 1, L"Get value at index...", 0, 1, DO_PitchTier_getValueAtIndex);
		praat_addAction1 (classPitchTier, 1, L"-- get statistics --", 0, 1, 0);
		praat_addAction1 (classPitchTier, 1, L"Get mean (curve)...", 0, 1, DO_PitchTier_getMean_curve);
		praat_addAction1 (classPitchTier, 1, L"Get mean (points)...", 0, 1, DO_PitchTier_getMean_points);
		praat_addAction1 (classPitchTier, 1, L"Get standard deviation (curve)...", 0, 1, DO_PitchTier_getStandardDeviation_curve);
		praat_addAction1 (classPitchTier, 1, L"Get standard deviation (points)...", 0, 1, DO_PitchTier_getStandardDeviation_points);
	praat_addAction1 (classPitchTier, 0, L"Modify -", 0, 0, 0);
		praat_TimeTier_modify_init (classPitchTier);
		praat_addAction1 (classPitchTier, 0, L"Add point...", 0, 1, DO_PitchTier_addPoint);
		praat_addAction1 (classPitchTier, 0, L"Formula...", 0, 1, DO_PitchTier_formula);
		praat_addAction1 (classPitchTier, 0, L"-- stylize --", 0, 1, 0);
		praat_addAction1 (classPitchTier, 0, L"Stylize...", 0, 1, DO_PitchTier_stylize);
		praat_addAction1 (classPitchTier, 0, L"Interpolate quadratically...", 0, 1, DO_PitchTier_interpolateQuadratically);
		praat_addAction1 (classPitchTier, 0, L"-- modify frequencies --", 0, 1, 0);
		praat_addAction1 (classPitchTier, 0, L"Shift frequencies...", 0, 1, DO_PitchTier_shiftFrequencies);
		praat_addAction1 (classPitchTier, 0, L"Multiply frequencies...", 0, 1, DO_PitchTier_multiplyFrequencies);
	praat_addAction1 (classPitchTier, 0, L"Synthesize -", 0, 0, 0);
		praat_addAction1 (classPitchTier, 0, L"To PointProcess", 0, 1, DO_PitchTier_to_PointProcess);
		praat_addAction1 (classPitchTier, 0, L"To Sound (pulse train)...", 0, 1, DO_PitchTier_to_Sound_pulseTrain);
		praat_addAction1 (classPitchTier, 0, L"To Sound (phonation)...", 0, 1, DO_PitchTier_to_Sound_phonation);
		praat_addAction1 (classPitchTier, 0, L"To Sound (sine)...", 0, 1, DO_PitchTier_to_Sound_sine);
	praat_addAction1 (classPitchTier, 0, L"Convert -", 0, 0, 0);
		praat_addAction1 (classPitchTier, 0, L"Down to PointProcess", 0, 1, DO_PitchTier_downto_PointProcess);
		praat_addAction1 (classPitchTier, 0, L"Down to TableOfReal...", 0, 1, DO_PitchTier_downto_TableOfReal);

	praat_addAction1 (classPointProcess, 0, L"PointProcess help", 0, 0, DO_PointProcess_help);
	praat_addAction1 (classPointProcess, 1, L"View & Edit", 0, praat_ATTRACTIVE, DO_PointProcess_edit);
	praat_addAction1 (classPointProcess, 1, L"View & Edit alone", 0, praat_HIDDEN, DO_PointProcess_edit);
	praat_addAction1 (classPointProcess, 1, L"Edit alone", 0, praat_HIDDEN, DO_PointProcess_edit);
	praat_addAction1 (classPointProcess, 0, L"View & Edit with Sound?", 0, 0, DO_info_PointProcess_Sound_edit);
	praat_addAction1 (classPointProcess, 0, L"Play -", 0, 0, 0);
		praat_addAction1 (classPointProcess, 0, L"Play as pulse train", 0, 1, DO_PointProcess_play);
		praat_addAction1 (classPointProcess, 0, L"Hum", 0, 1, DO_PointProcess_hum);
	praat_addAction1 (classPointProcess, 0, L"Draw...", 0, 0, DO_PointProcess_draw);
	praat_addAction1 (classPointProcess, 0, L"Query -", 0, 0, 0);
		praat_TimeFunction_query_init (classPointProcess);
		praat_addAction1 (classPointProcess, 1, L"-- script get --", 0, 1, 0);
		praat_addAction1 (classPointProcess, 1, L"Get number of points", 0, 1, DO_PointProcess_getNumberOfPoints);
		praat_addAction1 (classPointProcess, 1, L"Get low index...", 0, 1, DO_PointProcess_getLowIndex);
		praat_addAction1 (classPointProcess, 1, L"Get high index...", 0, 1, DO_PointProcess_getHighIndex);
		praat_addAction1 (classPointProcess, 1, L"Get nearest index...", 0, 1, DO_PointProcess_getNearestIndex);
		praat_addAction1 (classPointProcess, 1, L"Get time from index...", 0, 1, DO_PointProcess_getTimeFromIndex);
		praat_addAction1 (classPointProcess, 1, L"Get interval...", 0, 1, DO_PointProcess_getInterval);
		praat_addAction1 (classPointProcess, 1, L"-- periods --", 0, 1, 0);
		praat_addAction1 (classPointProcess, 1, L"Get number of periods...", 0, 1, DO_PointProcess_getNumberOfPeriods);
		praat_addAction1 (classPointProcess, 1, L"Get mean period...", 0, 1, DO_PointProcess_getMeanPeriod);
		praat_addAction1 (classPointProcess, 1, L"Get stdev period...", 0, 1, DO_PointProcess_getStdevPeriod);
		praat_addAction1 (classPointProcess, 1, L"Get jitter (local)...", 0, 1, DO_PointProcess_getJitter_local);
		praat_addAction1 (classPointProcess, 1, L"Get jitter (local, absolute)...", 0, 1, DO_PointProcess_getJitter_local_absolute);
		praat_addAction1 (classPointProcess, 1, L"Get jitter (rap)...", 0, 1, DO_PointProcess_getJitter_rap);
		praat_addAction1 (classPointProcess, 1, L"Get jitter (ppq5)...", 0, 1, DO_PointProcess_getJitter_ppq5);
		praat_addAction1 (classPointProcess, 1, L"Get jitter (ddp)...", 0, 1, DO_PointProcess_getJitter_ddp);
	praat_addAction1 (classPointProcess, 0, L"Modify -", 0, 0, 0);
		praat_TimeFunction_modify_init (classPointProcess);
		praat_addAction1 (classPointProcess, 0, L"Add point...", 0, 1, DO_PointProcess_addPoint);
		praat_addAction1 (classPointProcess, 0, L"Remove point...", 0, 1, DO_PointProcess_removePoint);
		praat_addAction1 (classPointProcess, 0, L"Remove point near...", 0, 1, DO_PointProcess_removePointNear);
		praat_addAction1 (classPointProcess, 0, L"Remove points...", 0, 1, DO_PointProcess_removePoints);
		praat_addAction1 (classPointProcess, 0, L"Remove points between...", 0, 1, DO_PointProcess_removePointsBetween);
		praat_addAction1 (classPointProcess, 0, L"-- voice --", 0, 1, 0);
		praat_addAction1 (classPointProcess, 0, L"Fill...", 0, 1, DO_PointProcess_fill);
		praat_addAction1 (classPointProcess, 0, L"Voice...", 0, 1, DO_PointProcess_voice);
	praat_addAction1 (classPointProcess, 0, L"Annotate -", 0, 0, 0);
		praat_addAction1 (classPointProcess, 0, L"To TextGrid...", 0, 1, DO_PointProcess_to_TextGrid);
		praat_addAction1 (classPointProcess, 0, L"-- to single tier --", 0, 1, 0);
		praat_addAction1 (classPointProcess, 0, L"To TextTier", 0, 1, DO_PointProcess_to_TextTier);
		praat_addAction1 (classPointProcess, 0, L"To IntervalTier", 0, 1, DO_PointProcess_to_IntervalTier);
praat_addAction1 (classPointProcess, 0, L"Set calculus", 0, 0, 0);
	praat_addAction1 (classPointProcess, 2, L"Union", 0, 0, DO_PointProcess_union);
	praat_addAction1 (classPointProcess, 2, L"Intersection", 0, 0, DO_PointProcess_intersection);
	praat_addAction1 (classPointProcess, 2, L"Difference", 0, 0, DO_PointProcess_difference);
praat_addAction1 (classPointProcess, 0, L"Analyse", 0, 0, 0);
	praat_addAction1 (classPointProcess, 0, L"To PitchTier...", 0, 0, DO_PointProcess_to_PitchTier);
	praat_addAction1 (classPointProcess, 0, L"To TextGrid (vuv)...", 0, 0, DO_PointProcess_to_TextGrid_vuv);
praat_addAction1 (classPointProcess, 0, L"Synthesize", 0, 0, 0);
	praat_addAction1 (classPointProcess, 0, L"To Sound (pulse train)...", 0, 0, DO_PointProcess_to_Sound_pulseTrain);
	praat_addAction1 (classPointProcess, 0, L"To Sound (phonation)...", 0, 0, DO_PointProcess_to_Sound_phonation);
	praat_addAction1 (classPointProcess, 0, L"To Sound (hum)", 0, 0, DO_PointProcess_to_Sound_hum);
praat_addAction1 (classPointProcess, 0, L"Convert", 0, 0, 0);
	praat_addAction1 (classPointProcess, 0, L"To Matrix", 0, 0, DO_PointProcess_to_Matrix);
	praat_addAction1 (classPointProcess, 0, L"Up to TextTier...", 0, 0, DO_PointProcess_upto_TextTier);
	praat_addAction1 (classPointProcess, 0, L"Up to PitchTier...", 0, 0, DO_PointProcess_upto_PitchTier);
	praat_addAction1 (classPointProcess, 0, L"Up to IntensityTier...", 0, 0, DO_PointProcess_upto_IntensityTier);

	praat_addAction1 (classPolygon, 0, L"Polygon help", 0, 0, DO_Polygon_help);
praat_addAction1 (classPolygon, 0, L"Draw", 0, 0, 0);
	praat_addAction1 (classPolygon, 0, L"Draw...", 0, 0, DO_Polygon_draw);
	praat_addAction1 (classPolygon, 0, L"Paint...", 0, 0, DO_Polygon_paint);
	praat_addAction1 (classPolygon, 0, L"Draw circles...", 0, 0, DO_Polygon_drawCircles);
	praat_addAction1 (classPolygon, 0, L"Paint circles...", 0, 0, DO_Polygon_paintCircles);
	praat_addAction1 (classPolygon, 2, L"Draw connection...", 0, 0, DO_Polygons_drawConnection);
praat_addAction1 (classPolygon, 0, L"Modify", 0, 0, 0);
	praat_addAction1 (classPolygon, 0, L"Randomize", 0, 0, DO_Polygon_randomize);
	praat_addAction1 (classPolygon, 0, L"Salesperson...", 0, 0, DO_Polygon_salesperson);
praat_addAction1 (classPolygon, 0, L"Hack", 0, 0, 0);
	praat_addAction1 (classPolygon, 0, L"To Matrix", 0, 0, DO_Polygon_to_Matrix);

	praat_addAction1 (classSpectrogram, 0, L"Spectrogram help", 0, 0, DO_Spectrogram_help);
	praat_addAction1 (classSpectrogram, 1, L"View", 0, 0, DO_Spectrogram_view);
	praat_addAction1 (classSpectrogram, 1, L"Movie", 0, 0, DO_Spectrogram_movie);
	praat_addAction1 (classSpectrogram, 0, L"Query -", 0, 0, 0);
		praat_TimeFrameSampled_query_init (classSpectrogram);
		praat_addAction1 (classSpectrogram, 1, L"Get power at...", 0, 1, DO_Spectrogram_getPowerAt);
	praat_addAction1 (classSpectrogram, 0, L"Draw -", 0, 0, 0);
		praat_addAction1 (classSpectrogram, 0, L"Paint...", 0, 1, DO_Spectrogram_paint);
	praat_addAction1 (classSpectrogram, 0, L"Analyse -", 0, 0, 0);
		praat_addAction1 (classSpectrogram, 0, L"To Spectrum (slice)...", 0, 1, DO_Spectrogram_to_Spectrum);
	praat_addAction1 (classSpectrogram, 0, L"Synthesize -", 0, 0, 0);
		praat_addAction1 (classSpectrogram, 0, L"To Sound...", 0, 1, DO_Spectrogram_to_Sound);
	praat_addAction1 (classSpectrogram, 0, L"Modify -", 0, 0, 0);
		praat_TimeFunction_modify_init (classSpectrogram);
		praat_addAction1 (classSpectrogram, 0, L"Formula...", 0, 1, DO_Spectrogram_formula);
	praat_addAction1 (classSpectrogram, 0, L"Hack -", 0, 0, 0);
		praat_addAction1 (classSpectrogram, 0, L"To Matrix", 0, 1, DO_Spectrogram_to_Matrix);

	praat_addAction1 (classSpectrum, 0, L"Spectrum help", 0, 0, DO_Spectrum_help);
	praat_addAction1 (classSpectrum, 1, L"View & Edit", 0, praat_ATTRACTIVE, DO_Spectrum_edit);
	praat_addAction1 (classSpectrum, 1, L"Edit", 0, praat_HIDDEN, DO_Spectrum_edit);
	praat_addAction1 (classSpectrum, 0, L"Draw -", 0, 0, 0);
		praat_addAction1 (classSpectrum, 0, L"Draw...", 0, 1, DO_Spectrum_draw);
		praat_addAction1 (classSpectrum, 0, L"Draw (log freq)...", 0, 1, DO_Spectrum_drawLogFreq);
	praat_addAction1 (classSpectrum, 0, L"List...", 0, 0, DO_Spectrum_list);
	praat_addAction1 (classSpectrum, 1, L"Query -", 0, 0, 0);
		praat_addAction1 (classSpectrum, 1, L"Frequency domain", 0, 1, 0);
			praat_addAction1 (classSpectrum, 1, L"Get lowest frequency", 0, 2, DO_Spectrum_getLowestFrequency);
			praat_addAction1 (classSpectrum, 1, L"Get highest frequency", 0, 2, DO_Spectrum_getHighestFrequency);
		praat_addAction1 (classSpectrum, 1, L"Frequency sampling", 0, 1, 0);
			praat_addAction1 (classSpectrum, 1, L"Get number of bins", 0, 2, DO_Spectrum_getNumberOfBins);
			praat_addAction1 (classSpectrum, 1, L"Get bin width", 0, 2, DO_Spectrum_getBinWidth);
			praat_addAction1 (classSpectrum, 1, L"Get frequency from bin number...", 0, 2, DO_Spectrum_getFrequencyFromBin);
						praat_addAction1 (classSpectrum, 1, L"Get frequency from bin...", 0, praat_HIDDEN + praat_DEPTH_2, DO_Spectrum_getFrequencyFromBin);
			praat_addAction1 (classSpectrum, 1, L"Get bin number from frequency...", 0, 2, DO_Spectrum_getBinFromFrequency);
						praat_addAction1 (classSpectrum, 1, L"Get bin from frequency...", 0, praat_HIDDEN + praat_DEPTH_2, DO_Spectrum_getBinFromFrequency);
		praat_addAction1 (classSpectrum, 1, L"-- get content --", 0, 1, 0);
		praat_addAction1 (classSpectrum, 1, L"Get real value in bin...", 0, 1, DO_Spectrum_getRealValueInBin);
		praat_addAction1 (classSpectrum, 1, L"Get imaginary value in bin...", 0, 1, DO_Spectrum_getImaginaryValueInBin);
		praat_addAction1 (classSpectrum, 1, L"-- get energy --", 0, 1, 0);
		praat_addAction1 (classSpectrum, 1, L"Get band energy...", 0, 1, DO_Spectrum_getBandEnergy);
		praat_addAction1 (classSpectrum, 1, L"Get band density...", 0, 1, DO_Spectrum_getBandDensity);
		praat_addAction1 (classSpectrum, 1, L"Get band energy difference...", 0, 1, DO_Spectrum_getBandEnergyDifference);
		praat_addAction1 (classSpectrum, 1, L"Get band density difference...", 0, 1, DO_Spectrum_getBandDensityDifference);
		praat_addAction1 (classSpectrum, 1, L"-- get moments --", 0, 1, 0);
		praat_addAction1 (classSpectrum, 1, L"Get centre of gravity...", 0, 1, DO_Spectrum_getCentreOfGravity);
		praat_addAction1 (classSpectrum, 1, L"Get standard deviation...", 0, 1, DO_Spectrum_getStandardDeviation);
		praat_addAction1 (classSpectrum, 1, L"Get skewness...", 0, 1, DO_Spectrum_getSkewness);
		praat_addAction1 (classSpectrum, 1, L"Get kurtosis...", 0, 1, DO_Spectrum_getKurtosis);
		praat_addAction1 (classSpectrum, 1, L"Get central moment...", 0, 1, DO_Spectrum_getCentralMoment);
	praat_addAction1 (classSpectrum, 0, L"Modify -", 0, 0, 0);
		praat_addAction1 (classSpectrum, 0, L"Formula...", 0, 1, DO_Spectrum_formula);
		praat_addAction1 (classSpectrum, 0, L"Filter (pass Hann band)...", 0, 1, DO_Spectrum_passHannBand);
		praat_addAction1 (classSpectrum, 0, L"Filter (stop Hann band)...", 0, 1, DO_Spectrum_stopHannBand);
	praat_addAction1 (classSpectrum, 0, L"Analyse", 0, 0, 0);
		praat_addAction1 (classSpectrum, 0, L"To Excitation...", 0, 0, DO_Spectrum_to_Excitation);
		praat_addAction1 (classSpectrum, 0, L"To SpectrumTier (peaks)", 0, 0, DO_Spectrum_to_SpectrumTier_peaks);
		praat_addAction1 (classSpectrum, 0, L"To Formant (peaks)...", 0, 0, DO_Spectrum_to_Formant_peaks);
		praat_addAction1 (classSpectrum, 0, L"To Ltas...", 0, 0, DO_Spectrum_to_Ltas);
		praat_addAction1 (classSpectrum, 0, L"To Ltas (1-to-1)", 0, 0, DO_Spectrum_to_Ltas_1to1);
		praat_addAction1 (classSpectrum, 0, L"To Spectrogram", 0, 0, DO_Spectrum_to_Spectrogram);
	praat_addAction1 (classSpectrum, 0, L"Convert", 0, 0, 0);
		praat_addAction1 (classSpectrum, 0, L"Cepstral smoothing...", 0, 0, DO_Spectrum_cepstralSmoothing);
		praat_addAction1 (classSpectrum, 0, L"LPC smoothing...", 0, 0, DO_Spectrum_lpcSmoothing);
	praat_addAction1 (classSpectrum, 0, L"Synthesize", 0, 0, 0);
		praat_addAction1 (classSpectrum, 0, L"To Sound", 0, 0, DO_Spectrum_to_Sound);
						praat_addAction1 (classSpectrum, 0, L"To Sound (fft)", 0, praat_HIDDEN, DO_Spectrum_to_Sound);
	praat_addAction1 (classSpectrum, 0, L"Hack", 0, 0, 0);
		praat_addAction1 (classSpectrum, 0, L"To Matrix", 0, 0, DO_Spectrum_to_Matrix);

	praat_addAction1 (classSpectrumTier, 0, L"Draw...", 0, 0, DO_SpectrumTier_draw);
	praat_addAction1 (classSpectrumTier, 0, L"List...", 0, 0, DO_SpectrumTier_list);
	praat_addAction1 (classSpectrumTier, 0, L"Down to Table", 0, 0, DO_SpectrumTier_downto_Table);
	praat_addAction1 (classSpectrumTier, 0, L"Remove points below...", 0, 0, DO_SpectrumTier_removePointsBelow);

	praat_addAction1 (classStrings, 0, L"Strings help", 0, 0, DO_Strings_help);
	praat_addAction1 (classStrings, 1, L"Save as raw text file...", 0, 0, DO_Strings_writeToRawTextFile);
	praat_addAction1 (classStrings, 1, L"Write to raw text file...", 0, praat_HIDDEN, DO_Strings_writeToRawTextFile);
	praat_addAction1 (classStrings, 1, L"View & Edit", 0, praat_ATTRACTIVE, DO_Strings_edit);
	praat_addAction1 (classStrings, 1, L"Edit", 0, praat_HIDDEN, DO_Strings_edit);
	praat_addAction1 (classStrings, 0, L"Query", 0, 0, 0);
		praat_addAction1 (classStrings, 2, L"Equal?", 0, 0, DO_Strings_equal);
		praat_addAction1 (classStrings, 1, L"Get number of strings", 0, 0, DO_Strings_getNumberOfStrings);
		praat_addAction1 (classStrings, 1, L"Get string...", 0, 0, DO_Strings_getString);
	praat_addAction1 (classStrings, 0, L"Modify", 0, 0, 0);
		praat_addAction1 (classStrings, 0, L"Randomize", 0, 0, DO_Strings_randomize);
		praat_addAction1 (classStrings, 0, L"Sort", 0, 0, DO_Strings_sort);
		praat_addAction1 (classStrings, 0, L"Genericize", 0, 0, DO_Strings_genericize);
		praat_addAction1 (classStrings, 0, L"Nativize", 0, 0, DO_Strings_nativize);
	praat_addAction1 (classStrings, 0, L"Analyze", 0, 0, 0);
		praat_addAction1 (classStrings, 0, L"To Distributions", 0, 0, DO_Strings_to_Distributions);
	praat_addAction1 (classStrings, 0, L"Synthesize", 0, 0, 0);
		praat_addAction1 (classStrings, 0, L"To WordList", 0, 0, DO_Strings_to_WordList);

	praat_addAction1 (classTable, 0, L"Down to Matrix", 0, 0, DO_Table_to_Matrix);

	praat_addAction1 (classTransition, 0, L"Transition help", 0, 0, DO_Transition_help);
praat_addAction1 (classTransition, 0, L"Draw", 0, 0, 0);
	praat_addAction1 (classTransition, 0, L"Draw as numbers...", 0, 0, DO_Transition_drawAsNumbers);
praat_addAction1 (classTransition, 0, L"Analyse", 0, 0, 0);
	praat_addAction1 (classTransition, 0, L"Eigen", 0, 0, DO_Transition_eigen);
	praat_addAction1 (classTransition, 0, L"Conflate", 0, 0, DO_Transition_conflate);
praat_addAction1 (classTransition, 0, L"Synthesize", 0, 0, 0);
	praat_addAction1 (classTransition, 0, L"Power...", 0, 0, DO_Transition_power);
praat_addAction1 (classTransition, 0, L"Cast", 0, 0, 0);
	praat_addAction1 (classTransition, 0, L"To Matrix", 0, 0, DO_Transition_to_Matrix);

	praat_addAction2 (classAmplitudeTier, 1, classSound, 1, L"View & Edit", 0, praat_ATTRACTIVE, DO_AmplitudeTier_edit);
	praat_addAction2 (classAmplitudeTier, 1, classSound, 1, L"Edit", 0, praat_HIDDEN, DO_AmplitudeTier_edit);   // hidden 2011
	praat_addAction2 (classAmplitudeTier, 1, classSound, 1, L"Multiply", 0, 0, DO_Sound_AmplitudeTier_multiply);
	praat_addAction2 (classDistributions, 1, classTransition, 1, L"Map", 0, 0, DO_Distributions_Transition_map);
	praat_addAction2 (classDistributions, 1, classTransition, 1, L"To Transition...", 0, 0, DO_Distributions_to_Transition_adj);
	praat_addAction2 (classDistributions, 2, classTransition, 1, L"To Transition (noise)...", 0, 0, DO_Distributions_to_Transition_noise_adj);
	praat_addAction2 (classDurationTier, 1, classSound, 1, L"View & Edit", 0, praat_ATTRACTIVE, DO_DurationTier_edit);
	praat_addAction2 (classDurationTier, 1, classSound, 1, L"Edit", 0, praat_HIDDEN, DO_DurationTier_edit);
	praat_addAction2 (classFormant, 1, classPointProcess, 1, L"To FormantTier", 0, 0, DO_Formant_PointProcess_to_FormantTier);
	praat_addAction2 (classFormant, 1, classSound, 1, L"Filter", 0, 0, DO_Sound_Formant_filter);
	praat_addAction2 (classFormant, 1, classSound, 1, L"Filter (no scale)", 0, 0, DO_Sound_Formant_filter_noscale);
	praat_addAction2 (classFormantGrid, 1, classSound, 1, L"Filter", 0, 0, DO_Sound_FormantGrid_filter);
	praat_addAction2 (classFormantGrid, 1, classSound, 1, L"Filter (no scale)", 0, 0, DO_Sound_FormantGrid_filter_noscale);
	praat_addAction2 (classFormantTier, 1, classSound, 1, L"Filter", 0, 0, DO_Sound_FormantTier_filter);
	praat_addAction2 (classFormantTier, 1, classSound, 1, L"Filter (no scale)", 0, 0, DO_Sound_FormantTier_filter_noscale);
	praat_addAction2 (classIntensity, 1, classPitch, 1, L"Draw", 0, 0, 0);
	praat_addAction2 (classIntensity, 1, classPitch, 1, L"Draw (phonetogram)...", 0, 0, DO_Pitch_Intensity_draw);
	praat_addAction2 (classIntensity, 1, classPitch, 1, L"Speckle (phonetogram)...", 0, praat_HIDDEN, DO_Pitch_Intensity_speckle);   /* grandfathered 2005 */
	praat_addAction2 (classIntensity, 1, classPointProcess, 1, L"To IntensityTier", 0, 0, DO_Intensity_PointProcess_to_IntensityTier);
	praat_addAction2 (classIntensityTier, 1, classPointProcess, 1, L"To IntensityTier", 0, 0, DO_IntensityTier_PointProcess_to_IntensityTier);
	praat_addAction2 (classIntensityTier, 1, classSound, 1, L"View & Edit", 0, praat_ATTRACTIVE, DO_IntensityTier_edit);
	praat_addAction2 (classIntensityTier, 1, classSound, 1, L"Edit", 0, praat_HIDDEN, DO_IntensityTier_edit);
	praat_addAction2 (classIntensityTier, 1, classSound, 1, L"Multiply", 0, praat_HIDDEN, DO_Sound_IntensityTier_multiply_old);
	praat_addAction2 (classIntensityTier, 1, classSound, 1, L"Multiply...", 0, 0, DO_Sound_IntensityTier_multiply);
	praat_addAction2 (classManipulation, 1, classSound, 1, L"Replace original sound", 0, 0, DO_Manipulation_replaceOriginalSound);
	praat_addAction2 (classManipulation, 1, classPointProcess, 1, L"Replace pulses", 0, 0, DO_Manipulation_replacePulses);
	praat_addAction2 (classManipulation, 1, classPitchTier, 1, L"Replace pitch tier", 0, 0, DO_Manipulation_replacePitchTier);
	praat_addAction2 (classManipulation, 1, classDurationTier, 1, L"Replace duration tier", 0, 0, DO_Manipulation_replaceDurationTier);
	praat_addAction2 (classManipulation, 1, classTextTier, 1, L"To Manipulation", 0, 0, DO_Manipulation_TextTier_to_Manipulation);
	praat_addAction2 (classMatrix, 1, classSound, 1, L"To ParamCurve", 0, 0, DO_Matrix_to_ParamCurve);
	praat_addAction2 (classPitch, 1, classPitchTier, 1, L"Draw...", 0, 0, DO_PitchTier_Pitch_draw);
	praat_addAction2 (classPitch, 1, classPitchTier, 1, L"To Pitch", 0, 0, DO_Pitch_PitchTier_to_Pitch);
	praat_addAction2 (classPitch, 1, classPointProcess, 1, L"To PitchTier", 0, 0, DO_Pitch_PointProcess_to_PitchTier);
	praat_addAction3 (classPitch, 1, classPointProcess, 1, classSound, 1, L"Voice report...", 0, 0, DO_Sound_Pitch_PointProcess_voiceReport);
	praat_addAction2 (classPitch, 1, classSound, 1, L"To PointProcess (cc)", 0, 0, DO_Sound_Pitch_to_PointProcess_cc);
	praat_addAction2 (classPitch, 1, classSound, 1, L"To PointProcess (peaks)...", 0, 0, DO_Sound_Pitch_to_PointProcess_peaks);
	praat_addAction2 (classPitch, 1, classSound, 1, L"To Manipulation", 0, 0, DO_Sound_Pitch_to_Manipulation);
	praat_addAction2 (classPitchTier, 1, classPointProcess, 1, L"To PitchTier", 0, 0, DO_PitchTier_PointProcess_to_PitchTier);
	praat_addAction2 (classPitchTier, 1, classSound, 1, L"View & Edit", 0, praat_ATTRACTIVE, DO_PitchTier_edit);
	praat_addAction2 (classPitchTier, 1, classSound, 1, L"Edit", 0, praat_HIDDEN, DO_PitchTier_edit);
	praat_addAction2 (classPointProcess, 1, classSound, 1, L"View & Edit", 0, praat_ATTRACTIVE, DO_PointProcess_edit);
	praat_addAction2 (classPointProcess, 1, classSound, 1, L"Edit", 0, praat_HIDDEN, DO_PointProcess_edit);
praat_addAction2 (classPointProcess, 1, classSound, 1, L"Query", 0, 0, 0);
	praat_addAction2 (classPointProcess, 1, classSound, 1, L"Get shimmer (local)...", 0, 0, DO_Point_Sound_getShimmer_local);
	praat_addAction2 (classPointProcess, 1, classSound, 1, L"Get shimmer (local_dB)...", 0, 0, DO_Point_Sound_getShimmer_local_dB);
	praat_addAction2 (classPointProcess, 1, classSound, 1, L"Get shimmer (apq3)...", 0, 0, DO_Point_Sound_getShimmer_apq3);
	praat_addAction2 (classPointProcess, 1, classSound, 1, L"Get shimmer (apq5)...", 0, 0, DO_Point_Sound_getShimmer_apq5);
	praat_addAction2 (classPointProcess, 1, classSound, 1, L"Get shimmer (apq11)...", 0, 0, DO_Point_Sound_getShimmer_apq11);
	praat_addAction2 (classPointProcess, 1, classSound, 1, L"Get shimmer (dda)...", 0, 0, DO_Point_Sound_getShimmer_dda);
praat_addAction2 (classPointProcess, 1, classSound, 1, L"Modify", 0, 0, 0);
	praat_addAction2 (classPointProcess, 1, classSound, 1, L"Transplant time domain", 0, 0, DO_Point_Sound_transplantDomain);
praat_addAction2 (classPointProcess, 1, classSound, 1, L"Analyse", 0, 0, 0);
	/*praat_addAction2 (classPointProcess, 1, classSound, 1, L"To Manipulation", 0, 0, DO_Sound_PointProcess_to_Manipulation);*/
	praat_addAction2 (classPointProcess, 1, classSound, 1, L"To AmplitudeTier (point)", 0, 0, DO_PointProcess_Sound_to_AmplitudeTier_point);
	praat_addAction2 (classPointProcess, 1, classSound, 1, L"To AmplitudeTier (period)...", 0, 0, DO_PointProcess_Sound_to_AmplitudeTier_period);
	praat_addAction2 (classPointProcess, 1, classSound, 1, L"To Ltas...", 0, 0, DO_PointProcess_Sound_to_Ltas);
	praat_addAction2 (classPointProcess, 1, classSound, 1, L"To Ltas (only harmonics)...", 0, 0, DO_PointProcess_Sound_to_Ltas_harmonics);
praat_addAction2 (classPointProcess, 1, classSound, 1, L"Synthesize", 0, 0, 0);
	praat_addAction2 (classPointProcess, 1, classSound, 1, L"To Sound ensemble...", 0, 0, DO_Sound_PointProcess_to_SoundEnsemble_correlate);

	praat_addAction4 (classDurationTier, 1, classPitchTier, 1, classPointProcess, 1, classSound, 1, L"To Sound...", 0, 0, DO_Sound_Point_Pitch_Duration_to_Sound);

	praat_addMenuCommand (L"Objects", L"New", L"-- new synthesis --", 0, 0, 0);
	praat_uvafon_Artsynth_init();
	praat_uvafon_David_init();
	praat_addMenuCommand (L"Objects", L"New", L"-- new grammars --", 0, 0, 0);
	praat_uvafon_gram_init();
	praat_uvafon_FFNet_init();
	praat_uvafon_LPC_init();
	praat_uvafon_Exp_init();
}

/* End of file praat_Fon.c */
