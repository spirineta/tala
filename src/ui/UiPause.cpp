/* UiPause.c
 *
 * Copyright (C) 2009-2011 Paul Boersma
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
 * pb 2009/01/20 created
 * pb 2010/07/13 GTK
 * pb 2010/07/26 removed UiFile_hide
 * pb 2011/02/01 cancelContinueButton
 */

#include "melder_gui.h"
#include "UiPause.h"
#include "praatP.h"
#include "UiForm.h"

static UiForm *thePauseForm = NULL;
static UiForm::UiField *thePauseFormRadio = NULL;
static int thePauseForm_clicked = 0;
static int theCancelContinueButton = 0;
static int theEventLoopDepth = 0;

static int thePauseFormOkCallback (UiForm *sendingForm, const wchar_t *sendingString, Interpreter *interpreter, const wchar_t *invokingButtonTitle, bool modified, void *closure) {
	(void) sendingForm;
	(void) sendingString;
	(void) interpreter;
	(void) invokingButtonTitle;
	(void) modified;
	/* BUG: should also restore praatP. editor. */
	/*
	 * Get the data from the pause form.
	 */
	thePauseForm_clicked = thePauseForm->getClickedContinueButton ();
	if (thePauseForm_clicked != theCancelContinueButton)
		thePauseForm->Interpreter_addVariables ((Interpreter*)closure);   // 'closure', not 'interpreter' or 'theInterpreter'!
	return 1;
}
static int thePauseFormCancelCallback (Any dia, void *closure) {
	(void) dia;
	(void) closure;
	if (theCancelContinueButton != 0) {
		thePauseForm_clicked = theCancelContinueButton;
	} else {
		thePauseForm_clicked = -1;   // STOP button
	}
	return 1;
}
int UiPause::begin (GuiObject topShell, const wchar_t *title, Interpreter *interpreter) {
	if (theEventLoopDepth > 0)
		error1 (L"Praat cannot have more than one pause form at a time.")
	forget (thePauseForm);   // in case an earlier build-up of another pause window was interrupted
	thePauseForm = new UiForm (topShell, Melder_wcscat2 (L"Pause: ", title),
		thePauseFormOkCallback, interpreter,   // pass interpreter as closure!
		NULL, NULL); cherror
end:
	iferror return 0;
	return 1;
}
int UiPause::real (const wchar_t *label, const wchar_t *defaultValue) {
	if (thePauseForm == NULL)
		error1 (L"The function \"real\" has to be between a \"beginPause\" and an \"endPause\".")
	thePauseForm->addReal (label, defaultValue); cherror
end:
	iferror return 0;
	return 1;
}
int UiPause::positive (const wchar_t *label, const wchar_t *defaultValue) {
	if (thePauseForm == NULL)
		error1 (L"The function \"positive\" has to be between a \"beginPause\" and an \"endPause\".")
	thePauseForm->addPositive (label, defaultValue); cherror
end:
	iferror return 0;
	return 1;
}
int UiPause::integer (const wchar_t *label, const wchar_t *defaultValue) {
	if (thePauseForm == NULL)
		error1 (L"The function \"integer\" has to be between a \"beginPause\" and an \"endPause\".")
	thePauseForm->addInteger (label, defaultValue); cherror
end:
	iferror return 0;
	return 1;
}
int UiPause::natural (const wchar_t *label, const wchar_t *defaultValue) {
	if (thePauseForm == NULL)
		error1 (L"The function \"natural\" has to be between a \"beginPause\" and an \"endPause\".")
	thePauseForm->addNatural (label, defaultValue); cherror
end:
	iferror return 0;
	return 1;
}
int UiPause::word (const wchar_t *label, const wchar_t *defaultValue) {
	if (thePauseForm == NULL)
		error1 (L"The function \"word\" has to be between a \"beginPause\" and an \"endPause\".")
	thePauseForm->addWord (label, defaultValue); cherror
end:
	iferror return 0;
	return 1;
}
int UiPause::sentence (const wchar_t *label, const wchar_t *defaultValue) {
	if (thePauseForm == NULL)
		error1 (L"The function \"sentence\" has to be between a \"beginPause\" and an \"endPause\".")
	thePauseForm->addSentence (label, defaultValue); cherror
end:
	iferror return 0;
	return 1;
}
int UiPause::text (const wchar_t *label, const wchar_t *defaultValue) {
	if (thePauseForm == NULL)
		error1 (L"The function \"text\" has to be between a \"beginPause\" and an \"endPause\".")
	thePauseForm->addText (label, defaultValue); cherror
end:
	iferror return 0;
	return 1;
}
int UiPause::boolean (const wchar_t *label, int defaultValue) {
	if (thePauseForm == NULL)
		error1 (L"The function \"boolean\" has to be between a \"beginPause\" and an \"endPause\".")
	thePauseForm->addBoolean (label, defaultValue); cherror
end:
	iferror return 0;
	return 1;
}
int UiPause::choice (const wchar_t *label, int defaultValue) {
	if (thePauseForm == NULL)
		error1 (L"The function \"choice\" has to be between a \"beginPause\" and an \"endPause\".")
	thePauseFormRadio = thePauseForm->addRadio (label, defaultValue); cherror
end:
	iferror return 0;
	return 1;
}
int UiPause::optionMenu (const wchar_t *label, int defaultValue) {
	if (thePauseForm == NULL)
		error1 (L"The function \"optionMenu\" has to be between a \"beginPause\" and an \"endPause\".")
	thePauseFormRadio = thePauseForm->addOptionMenu (label, defaultValue); cherror
end:
	iferror return 0;
	return 1;
}
int UiPause::option (const wchar_t *label) {
	if (thePauseForm == NULL)
		error1 (L"The function \"option\" has to be between a \"beginPause\" and an \"endPause\".")
	if (thePauseFormRadio == NULL) {
		forget (thePauseForm);
		error1 (L"Found the function \"option\" without a preceding \"choice\" or \"optionMenu\".")
	}
	thePauseFormRadio->addOptionMenu (label); cherror
end:
	iferror return 0;
	return 1;
}
int UiPause::comment (const wchar_t *label) {
	if (thePauseForm == NULL)
		error1 (L"The function \"comment\" has to be between a \"beginPause\" and an \"endPause\".")
	thePauseForm->addLabel (L"", label); cherror
end:
	iferror return 0;
	return 1;
}
int UiPause::end (int numberOfContinueButtons, int defaultContinueButton, int cancelContinueButton,
	const wchar_t *continueText1, const wchar_t *continueText2, const wchar_t *continueText3,
	const wchar_t *continueText4, const wchar_t *continueText5, const wchar_t *continueText6,
	const wchar_t *continueText7, const wchar_t *continueText8, const wchar_t *continueText9,
	const wchar_t *continueText10, Interpreter *interpreter)
{
	if (thePauseForm == NULL)
		error1 (L"Found the function \"endPause\" without a preceding \"beginPause\".")
	{
		thePauseForm->setPauseForm (numberOfContinueButtons, defaultContinueButton, cancelContinueButton,
			continueText1, continueText2, continueText3, continueText4, continueText5,
			continueText6, continueText7, continueText8, continueText9, continueText10,
			thePauseFormCancelCallback);
		theCancelContinueButton = cancelContinueButton;
		thePauseForm->finish (); cherror
		int wasBackgrounding = Melder_backgrounding;
		structMelderDir dir = { { 0 } };
		Melder_getDefaultDir (& dir);
		//if (theCurrentPraatApplication -> batch) goto end;
		if (wasBackgrounding) praat_foreground ();
		/*
		 * Put the pause form on the screen.
		 */
		thePauseForm->do_ (false);
		/*
		 * Wait for the user to click Stop or Continue.
		 */
		#ifndef CONSOLE_APPLICATION
			thePauseForm_clicked = 0;
			Melder_assert (theEventLoopDepth == 0);
			theEventLoopDepth ++;
			do {
				gtk_main_iteration ();
			} while (! thePauseForm_clicked);
			theEventLoopDepth --;
			if (wasBackgrounding) praat_background ();
			Melder_setDefaultDir (& dir);
			/* BUG: should also restore praatP. editor. */
			thePauseForm = NULL;   // undangle
			thePauseFormRadio = NULL;   // undangle
			if (thePauseForm_clicked == -1) {
				interpreter->stop ();
				error1 (L"You interrupted the script.");
				//Melder_flushError (NULL);
				//Melder_clearError ();
			} else {
				//Melder_casual ("Clicked %d", thePauseForm_clicked);
			}
		#endif
	}
end:
	iferror return 0;
	return thePauseForm_clicked;
}

/* End of file UiPause.c */

