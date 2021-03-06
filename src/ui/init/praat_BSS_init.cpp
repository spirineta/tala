/* praat_BSS_init.c
 *
 * Copyright (C) 2010-2011 David Weenink
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
	djmw 20101003
	djmw 20110101 Latest modification
*/

#include "ui/praat.h"

#include "dwtools/ICA.h"
#include "dwtools/PCA.h"
#include "fon/Sound.h"
#include "ui/Interpreter.h"

extern "C" void praat_SSCP_as_TableOfReal_init (void *klas);
extern "C" void praat_TableOfReal_init (void *klas);
void praat_TableOfReal_init3  (void *klas);

/********************** CrossCorrelationTable(s) ******************/

FORM (CrossCorrelationTables_createTestSet, L"CrossCorrelationTables: Create test set", L"CrossCorrelationTables: Create test set...")
	WORD (L"Name", L"5x5")
	NATURAL (L"Matrix dimension", L"5")
	NATURAL (L"Number of matrices", L"20")
	BOOLEAN (L"First is positive definite", 1)
	REAL (L"Sigma", L"0.02")
	OK
DO
	if (! praat_new1 (CrossCorrelationTables_createTestSet (GET_INTEGER (L"Matrix dimension"),
		GET_INTEGER (L"Number of matrices"), GET_INTEGER (L"First is positive definite"), GET_REAL (L"Sigma")), GET_STRING (L"Name"))) return 0;
END

CrossCorrelationTable CrossCorrelationTable_createSimple (wchar_t *covars, wchar_t *centroid, long numberOfSamples)
{
	long dimension = Melder_countTokens (centroid);
	long ncovars = Melder_countTokens (covars);
	long ncovars_wanted = dimension * (dimension + 1) / 2;
	if (ncovars != ncovars_wanted) return (structCrossCorrelationTable *)Melder_errorp1 (L"The number of matrix elements and the number of "
		"centroid elements are not in concordance. There should be \"d(d+1)/2\" matrix values and \"d\" centroid values.");

	CrossCorrelationTable me = CrossCorrelationTable_create (dimension); cherror

	// Construct the full matrix from the upper-diagonal elements

	{
		long inum = 1, irow = 1, icol, nmissing, inumc;
		for (wchar_t *token = Melder_firstToken (covars); token != NULL && inum <= ncovars_wanted; token = Melder_nextToken (), inum++)
		{
			double number;
			nmissing = (irow - 1) * irow / 2;
			inumc = inum + nmissing;
			irow = (inumc - 1) / dimension + 1;
			icol = ((inumc - 1) % dimension) + 1;
			if (! Interpreter_numericExpression_FIXME (token, &number))
			{
				Melder_error5 (L"CrossCorrelationTable: item ", Melder_integer (inum), L" \"", token, L"\"is not a number.");
				goto end;
			}
			my data[irow][icol] = my data[icol][irow] = number;
			if (icol == dimension) irow++;
		}


		inum = 1;
		for (wchar_t *token = Melder_firstToken (centroid); token != NULL && inum <= dimension; token = Melder_nextToken (), inum++)
		{
			double number;
			if (! Interpreter_numericExpression_FIXME (token, &number))
			{
				Melder_error5 (L"Centroid: item ", Melder_integer (inum), L" \"", token, L"\"is not a number.");
				goto end;
			}
			my centroid[inum] = number;
		}
		my numberOfObservations = numberOfSamples;
	}
end:
	if (Melder_hasError ()) forget (me);
	return me;
}

FORM (CrossCorrelationTable_createSimple, L"Create simple CrossCorrelationTable", 0)
	WORD (L"Name", L"ct")
	SENTENCE (L"Cross correlations", L"1.0 0.0 1.0")
	SENTENCE (L"Centroid", L"0.0 0.0")
	POSITIVE (L"Number of samples", L"100.0")
	OK
DO
	if (! praat_new1 (CrossCorrelationTable_createSimple (GET_STRING (L"Cross correlations"), GET_STRING (L"Centroid"),
		GET_REAL (L"Number of samples")), GET_STRING (L"Name"))) return 0;
END

MixingMatrix MixingMatrix_createSimple (long numberOfChannels, long numberOfComponents, wchar_t *elements)
{
	long inum = 1, irow, icol, ntokens = Melder_countTokens (elements);
	if (ntokens == 0) return (structMixingMatrix *)Melder_errorp1 (L"No matrix elements.");
	long nwanted = numberOfChannels * numberOfComponents;

	MixingMatrix me = MixingMatrix_create (numberOfChannels, numberOfComponents);
	if (me == NULL) return NULL;

	// Construct the full matrix from the elements
	double number;
	for (wchar_t *token = Melder_firstToken (elements); token != NULL && inum <= ntokens; token = Melder_nextToken (), inum++)
	{
		irow = (inum - 1) / numberOfComponents + 1;
		icol = (inum - 1) % numberOfComponents + 1;
		if (! Interpreter_numericExpression_FIXME (token, &number))
		{
			Melder_error5 (L"MixingMatrix: item ", Melder_integer (inum), L" \"", token, L"\"is not a number.");
			goto end;
		}
		my data[irow][icol] = number;
	}
	if (ntokens < nwanted)
	{
		for (long i = inum; i <= nwanted; i++)
		{
			irow = (inum - 1) / numberOfComponents + 1;
			icol = (inum - 1) % numberOfComponents + 1;
			my data[irow][icol] = number; // repeat the last number given!
		}
	}
end:
	if (Melder_hasError ()) forget (me);
	return me;
}

FORM (MixingMatrix_createSimple, L"Create simple MixingMatrix", 0)
	WORD (L"Name", L"mm")
	NATURAL (L"Number of channels", L"2")
	NATURAL (L"Number of components", L"2")
	SENTENCE (L"Mixing coefficients", L"1.0 1.0 1.0 1.0")
	OK
DO
	if (! praat_new1 (MixingMatrix_createSimple (GET_INTEGER (L"Number of channels"), GET_INTEGER (L"Number of components"),
		GET_STRING (L"Mixing coefficients")), GET_STRING (L"Name"))) return 0;
END

DIRECT (CrossCorrelationTable_help)
	Melder_help (L"CrossCorrelationTable");
END

DIRECT (CrossCorrelationTable_to_PCA)
	EVERY_TO (SSCP_to_PCA (OBJECT))
END

FORM (Sound_and_PCA_to_Sound_pc, L"ui/editors/AmplitudeTierEditor.h", 0)
	NATURAL (L"Number of components", L"15")
	BOOLEAN (L"Whiten", 1)
	OK
DO
	Sound me = (structSound *)ONLY (classSound);
	PCA thee = (structPCA *)ONLY (classPCA);
	if (! praat_new2 (Sound_and_PCA_to_Sound_pc (me, thee, GET_INTEGER (L"Number of components"),
		GET_INTEGER (L"Whiten")), Thing_getName(me), L"_pc")) return 0;
END

DIRECT (CrossCorrelationTable_to_CrossCorrelationTables)
	CrossCorrelationTables thee = CrossCorrelationTables_create ();
	long id = 0, nrows, ncols;
	int status = 1;
	if (thee == NULL) return 0;
	WHERE (SELECTED)
	{
		CrossCorrelationTable ct = (structCrossCorrelationTable *)OBJECT, ctc;
		id ++;
		if (id == 1) { nrows = ct -> numberOfRows; ncols = ct -> numberOfColumns; }
		if (ct -> numberOfRows != nrows || ct -> numberOfColumns != ncols)
		{ status = 0; Melder_error3 (L"All tables must have the same dimensions (", Melder_integer (id), L")."); break; }
		ctc = (structCrossCorrelationTable *)Data_copy (ct);
		if (ctc == NULL || ! Collection_addItem (thee, ctc)) break;
	}
	if (Melder_hasError () || status != 1) { forget (thee); return 0; }
	if (! praat_new2 (thee, L"ct_", Melder_integer (id))) return 0;
END


FORM (Sound_to_CrossCorrelationTable, L"Sound: To CrossCorrelationTable", L"Sound: To CrossCorrelationTable...")
	REAL (L"left Time range (s)", L"0.0")
	REAL (L"right Time range (s)", L"10.0")
	REAL (L"Lag time (s)", L"0.0")
	OK
DO
	EVERY_TO (Sound_to_CrossCorrelationTable ((structSound *)OBJECT, GET_REAL (L"left Time range"), GET_REAL (L"right Time range"),
		GET_REAL (L"Lag time")))
END

DIRECT (CrossCorrelationTables_help)
	Melder_help (L"CrossCorrelationTables");
END

FORM (CrossCorrelationTables_getDiagonalityMeasure, L"CrossCorrelationTables: Get diagonality measure", L"CrossCorrelationTables: Get diagonality measure...")
	NATURAL (L"First table", L"1")
	NATURAL (L"Last table", L"100")
	OK
DO
	double dm = CrossCorrelationTables_getDiagonalityMeasure ((structCrossCorrelationTables *)ONLY_OBJECT,
		NULL, GET_INTEGER (L"First table"), GET_INTEGER (L"Last table"));
	Melder_information2 (Melder_double (dm), L" (= average sum of squared off-diagonal elements)");
END

FORM (CrossCorrelationTables_extractCrossCorrelationTable, L"CrossCorrelationTables: Extract one CrossCorrelationTable", 0)
	NATURAL (L"Index", L"1")
	OK
DO
	CrossCorrelationTables me = (structCrossCorrelationTables *)ONLY_OBJECT;
	long index = GET_INTEGER (L"Index");
	if (index > my size) return Melder_error1 (L"Index too large.");
	if (! praat_new3 (Data_copy (my item[index]), Thing_getName (me), L"_", Melder_integer (index))) return 0;
END


FORM (CrossCorrelationTables_to_Diagonalizer, L"CrossCorrelationTables: To Diagonalizer", 0)
	NATURAL (L"Maximum number of iterations", L"100")
	POSITIVE (L"Tolerance", L"0.001")
	OPTIONMENU (L"Diagonalization method", 2)
	OPTION (L"qdiag")
	OPTION (L"ffdiag")
	OK
DO
	EVERY_TO (CrossCorrelationTables_to_Diagonalizer ((structCrossCorrelationTables *)OBJECT, GET_INTEGER (L"Maximum number of iterations"),
		GET_REAL (L"Tolerance"), GET_INTEGER (L"Diagonalization method")))
END

FORM (Diagonalizer_and_CrossCorrelationTables_improveDiagonality, L"Diagonalizer & CrossCorrelationTables: Improve diagonality", 0)
	NATURAL (L"Maximum number of iterations", L"100")
	POSITIVE (L"Tolerance", L"0.001")
	OPTIONMENU (L"Diagonalization method", 2)
	OPTION (L"qdiag")
	OPTION (L"ffdiag")
	OK
DO
	if (! Diagonalizer_and_CrossCorrelationTables_improveDiagonality ((structDiagonalizer *)ONLY (classDiagonalizer),
		(structCrossCorrelationTables *)ONLY (classCrossCorrelationTables), GET_INTEGER (L"Maximum number of iterations"),
		GET_REAL (L"Tolerance"), GET_INTEGER (L"Diagonalization method"))) return 0;
END

FORM (CrossCorrelationTables_and_Diagonalizer_getDiagonalityMeasure, L"CrossCorrelationTables & Diagonalizer: Get diagonality measure", 0)
	NATURAL (L"First table", L"1")
	NATURAL (L"Last table", L"100")
	OK
DO
	double dm = CrossCorrelationTables_and_Diagonalizer_getDiagonalityMeasure ((structCrossCorrelationTables *)ONLY (classCrossCorrelationTables),
		(structDiagonalizer *)ONLY (classDiagonalizer), NULL, GET_INTEGER (L"First table"), GET_INTEGER (L"Last table"));
	Melder_information2 (Melder_double (dm), L" (= average sum of squared off-diagonal elements)");
END

DIRECT (CrossCorrelationTable_and_Diagonalizer_diagonalize)
	CrossCorrelationTable me = (structCrossCorrelationTable *)ONLY (classCrossCorrelationTable);
	Diagonalizer thee = (structDiagonalizer *)ONLY (classDiagonalizer);
	if (! praat_new3 (CrossCorrelationTable_and_Diagonalizer_diagonalize (me, thee), Thing_getName (me), L"_", Thing_getName (thee))) return 0;
END

DIRECT (CrossCorrelationTables_and_Diagonalizer_diagonalize)
	CrossCorrelationTables me = (structCrossCorrelationTables *)ONLY (classCrossCorrelationTables);
	Diagonalizer thee = (structDiagonalizer *)ONLY (classDiagonalizer);
	if (! praat_new3 (CrossCorrelationTables_and_Diagonalizer_diagonalize (me, thee), Thing_getName (me), L"_", Thing_getName (thee))) return 0;
END

FORM (CrossCorrelationTables_and_MixingMatrix_improveUnmixing, L"ui/editors/AmplitudeTierEditor.h", 0)
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"Iteration parameters")
	NATURAL (L"Maximum number of iterations", L"100")
	POSITIVE (L"Tolerance", L"0.001")
	OPTIONMENU (L"Diagonalization method", 2)
	OPTION (L"qdiag")
	OPTION (L"ffdiag")
	OK
DO
	if (! MixingMatrix_and_CrossCorrelationTables_improveUnmixing ((structMixingMatrix *)ONLY (classMixingMatrix),
		(structCrossCorrelationTables *)ONLY (classCrossCorrelationTables), GET_INTEGER (L"Maximum number of iterations"), GET_REAL (L"Tolerance"), GET_INTEGER (L"Diagonalization method"))) return 0;
END

DIRECT (Diagonalizer_to_MixingMatrix)
	EVERY_TO (Diagonalizer_to_MixingMatrix ((structDiagonalizer *)OBJECT))
END

FORM (Sound_to_MixingMatrix, L"ui/editors/AmplitudeTierEditor.h", 0)
	REAL (L"left Time range (s)", L"0.0")
	REAL (L"right Time range (s)", L"10.0")
	NATURAL (L"Number of cross-correlations", L"20")
	POSITIVE (L"Lag times (s)", L"0.002")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"Iteration parameters")
	NATURAL (L"Maximum number of iterations", L"100")
	POSITIVE (L"Tolerance", L"0.001")
	OPTIONMENU (L"Diagonalization method", 2)
	OPTION (L"qdiag")
	OPTION (L"ffdiag")
	OK
DO
	EVERY_TO (Sound_to_MixingMatrix ((structSound *)OBJECT,
		GET_REAL (L"left Time range"), GET_REAL (L"right Time range"), GET_INTEGER (L"Number of cross-correlations"),
		GET_REAL (L"Lag times"), GET_INTEGER (L"Maximum number of iterations"), GET_REAL (L"Tolerance"),
		GET_INTEGER (L"Diagonalization method")))
END

FORM (Sound_to_CrossCorrelationTables, L"Sound: To CrossCorrelationTables", 0)
	REAL (L"left Time range (s)", L"0.0")
	REAL (L"right Time range (s)", L"10.0")
	NATURAL (L"Number of cross-correlations", L"20")
	POSITIVE (L"Lag times (s)", L"0.002")
	OK
DO
	EVERY_TO (Sound_to_CrossCorrelationTables ((structSound *)OBJECT, GET_REAL (L"left Time range"), GET_REAL (L"right Time range"), GET_REAL (L"Lag times"), GET_INTEGER (L"Number of cross-correlations")))
END

FORM (Sound_to_Sound_bss, L"Sound: To Sound (blind source separation)", L"Sound: To Sound (blind source separation)...")
	REAL (L"left Time range (s)", L"0.0")
	REAL (L"right Time range (s)", L"10.0")
	NATURAL (L"Number of cross-correlations", L"20")
	POSITIVE (L"Lag times (s)", L"0.002")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"Iteration parameters")
	NATURAL (L"Maximum number of iterations", L"100")
	POSITIVE (L"Tolerance", L"0.001")
	OPTIONMENU (L"Diagonalization method", 2)
	OPTION (L"qdiag")
	OPTION (L"ffdiag")
	OK
DO
	EVERY_TO (Sound_to_Sound_BSS ((structSound *)OBJECT, GET_REAL (L"left Time range"), GET_REAL (L"right Time range"),
		GET_INTEGER (L"Number of cross-correlations"), GET_REAL (L"Lag times"),
		GET_INTEGER (L"Maximum number of iterations"), GET_REAL (L"Tolerance"),
		GET_INTEGER (L"Diagonalization method")))
END

DIRECT (Sound_and_MixingMatrix_mix)
	Sound s = (structSound *)ONLY (classSound);
	MixingMatrix mm = (structMixingMatrix *)ONLY (classMixingMatrix);
	if (! praat_new2 (Sound_and_MixingMatrix_mix (s, mm), Thing_getName (s), L"_mixed")) return 0;
END

DIRECT (Sound_and_MixingMatrix_unmix)
	Sound s = (structSound *)ONLY (classSound);
	MixingMatrix mm = (structMixingMatrix *)ONLY (classMixingMatrix);
	if (! praat_new2 (Sound_and_MixingMatrix_unmix (s, mm), Thing_getName (s), L"_unmixed")) return 0;
END

DIRECT (TableOfReal_to_MixingMatrix)
	EVERY_TO (TableOfReal_to_MixingMatrix ((structTableOfReal *)OBJECT))
END

FORM (TableOfReal_and_TableOfReal_crossCorrelations, L"TableOfReal & TableOfReal: Cross-correlations", 0)
	OPTIONMENU (L"Correlations between", 1)
	OPTION (L"Rows")
	OPTION (L"Columns")
	BOOLEAN (L"Center", 0)
	BOOLEAN (L"Normalize", 0)
	OK
DO
	TableOfReal t1 = NULL, t2 = NULL;
	int by_columns = GET_INTEGER (L"Correlations between") - 1;
	WHERE (SELECTED && Thing_member (OBJECT, classTableOfReal)) { if (t1) t2 = (structTableOfReal *)OBJECT; else t1 = (structTableOfReal *)OBJECT; }
	if (! praat_new1 (TableOfReal_and_TableOfReal_crossCorrelations (t1, t2, by_columns,
		GET_INTEGER (L"Center"), GET_INTEGER (L"Normalize")),
		(by_columns ? L"by_columns" : L"by_rows"))) return 0;
END

void praat_TableOfReal_init3  (void *klas)
{
	praat_TableOfReal_init (klas);
	praat_addAction1 (klas, 2, L"To TableOfReal (cross-correlations)...", 0, 0, DO_TableOfReal_and_TableOfReal_crossCorrelations);
}

extern "C" void praat_BSS_init (void);
void praat_BSS_init (void)
{
	Thing_recognizeClassesByName (classDiagonalizer, classMixingMatrix, classCrossCorrelationTable, classCrossCorrelationTables, NULL);

	praat_addMenuCommand (L"Objects", L"New", L"Create simple CrossCorrelationTable...", L"Create simple Covariance...", praat_HIDDEN + praat_DEPTH_1, DO_CrossCorrelationTable_createSimple);
	praat_addMenuCommand (L"Objects", L"New", L"Create test CrossCorrelationTables...", L"Create simple CrossCorrelationTable...", praat_HIDDEN + praat_DEPTH_1, DO_CrossCorrelationTables_createTestSet);
	praat_addMenuCommand (L"Objects", L"New", L"Create simple MixingMatrix...", L"Create test CrossCorrelationTables...", praat_HIDDEN + praat_DEPTH_1, DO_MixingMatrix_createSimple);

	praat_addAction1 (classCrossCorrelationTable, 0, L"CrossCorrelationTable help", 0, 0, DO_CrossCorrelationTable_help);
	praat_SSCP_as_TableOfReal_init (classCrossCorrelationTable);
	praat_addAction1 (classCrossCorrelationTable, 0, L"To PCA", 0, 0, DO_CrossCorrelationTable_to_PCA);
	praat_addAction1 (classCrossCorrelationTable, 0, L"To CrossCorrelationTables", 0, 0, DO_CrossCorrelationTable_to_CrossCorrelationTables);

	praat_addAction1 (classCrossCorrelationTables, 0, L"CrossCorrelationTables help", 0, 0, DO_CrossCorrelationTables_help);
	praat_addAction1 (classCrossCorrelationTables, 1, L"Extract CrossCorrelationTable...", 0, 0, DO_CrossCorrelationTables_extractCrossCorrelationTable);
	praat_addAction1 (classCrossCorrelationTables, 1, L"Get diagonality measure...", 0, 0, DO_CrossCorrelationTables_getDiagonalityMeasure);
	praat_addAction1 (classCrossCorrelationTables, 0, L"To Diagonalizer...", 0, 0, DO_CrossCorrelationTables_to_Diagonalizer);

	praat_TableOfReal_init3 (classDiagonalizer);
	praat_addAction1 (classDiagonalizer, 0, L"To MixingMatrix", 0, 0, DO_Diagonalizer_to_MixingMatrix);

	praat_TableOfReal_init3 (classMixingMatrix);

	praat_addAction1 (classSound, 0, L"To MixingMatrix...",  L"Resample...", praat_HIDDEN + praat_DEPTH_1, DO_Sound_to_MixingMatrix);
	praat_addAction1 (classSound, 0, L"To CrossCorrelationTable...",  L"Resample...", 1, DO_Sound_to_CrossCorrelationTable);
	praat_addAction1 (classSound, 0, L"To CrossCorrelationTables...",  L"Resample...", praat_HIDDEN + praat_DEPTH_1, DO_Sound_to_CrossCorrelationTables);

	praat_addAction1 (classSound, 0, L"To Sound (blind source separation)...", L"Resample...", 1, DO_Sound_to_Sound_bss);

	praat_addAction1 (classTableOfReal, 0, L"To MixingMatrix", L"To Configuration", praat_HIDDEN, DO_TableOfReal_to_MixingMatrix);

	praat_addAction2 (classSound, 1, classMixingMatrix, 1, L"Mix", 0, 0, DO_Sound_and_MixingMatrix_mix);
	praat_addAction2 (classSound, 1, classMixingMatrix, 1, L"Unmix", 0, 0, DO_Sound_and_MixingMatrix_unmix);

	praat_addAction2 (classSound, 1, classPCA, 1, L"To Sound (pc)...", 0 , 0, DO_Sound_and_PCA_to_Sound_pc);

	praat_addAction2 (classCrossCorrelationTable, 1, classDiagonalizer, 1, L"Diagonalize", 0 , 0, DO_CrossCorrelationTable_and_Diagonalizer_diagonalize);

	praat_addAction2 (classCrossCorrelationTables, 1, classDiagonalizer, 1, L"Get diagonality measure...", 0 , 0, DO_CrossCorrelationTables_and_Diagonalizer_getDiagonalityMeasure);
	praat_addAction2 (classCrossCorrelationTables, 1, classDiagonalizer, 1, L"Diagonalize", 0 , 0, DO_CrossCorrelationTables_and_Diagonalizer_diagonalize);
	praat_addAction2 (classCrossCorrelationTables, 1, classDiagonalizer, 1, L"Improve diagonality...", 0 , 0, DO_Diagonalizer_and_CrossCorrelationTables_improveDiagonality);

	praat_addAction2 (classCrossCorrelationTables, 1, classMixingMatrix, 1, L"Improve unmixing...", 0 , 0, DO_CrossCorrelationTables_and_MixingMatrix_improveUnmixing);
}

/* End of file praat_BSS_init.c */
