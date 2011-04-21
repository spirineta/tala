#ifndef _HyperPage_h_
#define _HyperPage_h_
/* HyperPage.h
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
 * pb 2011/03/02
 */

#ifndef _Editor_h_
	#include "Editor.h"
#endif
#ifndef _Collection_h_
	#include "Collection.h"
#endif
#ifndef _Graphics_h_
	#include "Graphics.h"
#endif

#define HyperLink_members Data_members \
	double x1DC, x2DC, y1DC, y2DC;
#define HyperLink_methods Data_methods
class_create (HyperLink, Data);

#define HyperPage_ADD_BORDER  1
#define HyperPage_USE_ENTRY_HINT  2

HyperLink HyperLink_create (const wchar_t *name, double x1, double x2, double y1, double y2);

class HyperPage : public Editor {
  public:
	static void prefs (void);

	static int _hasHistory, _isOrdered;

	HyperPage (GuiObject parent, const wchar_t *title, Any data);
	~HyperPage ();

	const wchar_t * type () { return L"HyperPage"; }
	bool isEditable () { return false; }
	void clear ();
	int any (const wchar_t *text, int font, int size, int style, double minFooterDistance,
		double x, double secondIndent, double topSpacing, double bottomSpacing, unsigned long method);
	int pageTitle (const wchar_t *title);
	int intro (const wchar_t *text);
	int entry (const wchar_t *title);
	int paragraph (const wchar_t *text);
	int listItem (const wchar_t *text);
	int listItem1 (const wchar_t *text);
	int listItem2 (const wchar_t *text);
	int listItem3 (const wchar_t *text);
	int listTag (const wchar_t *text);
	int listTag1 (const wchar_t *text);
	int listTag2 (const wchar_t *text);
	int listTag3 (const wchar_t *text);
	int definition (const wchar_t *text);
	int definition1 (const wchar_t *text);
	int definition2 (const wchar_t *text);
	int definition3 (const wchar_t *text);
	int code (const wchar_t *text);
	int code1 (const wchar_t *text);
	int code2 (const wchar_t *text);
	int code3 (const wchar_t *text);
	int code4 (const wchar_t *text);
	int code5 (const wchar_t *text);
	int prototype (const wchar_t *text);
	int formula (const wchar_t *formula);
	int picture (double width_inches, double height_inches, void (*draw) (Graphics g));
	int script (double width_inches, double height_inches, const wchar_t *script);
	void setEntryHint (const wchar_t *entry);
	void initSheetOfPaper ();
	void updateVerticalScrollBar ();
	void draw ();
	void initScreen ();
	void saveHistory (const wchar_t *title);
	int goToPage (const wchar_t *title);
	int goToPage_i (long ipage);
	void defaultHeaders (EditorCommand *cmd);
	void setFontSize (int fontSize);
	int do_forth ();
	void createMenus ();
	long getCurrentPageNumber ();
	long getNumberOfPages ();
	void dataChanged ();
	int do_back ();

	GuiObject _drawingArea, _verticalScrollBar;
	Graphics _g, _ps;
	double _x, _y, _rightMargin, _previousBottomSpacing;
	long _pageNumber;
	Collection _links;
	int _printing, _top, _mirror;
	wchar_t *_insideHeader, *_middleHeader, *_outsideHeader;
	wchar_t *_insideFooter, *_middleFooter, *_outsideFooter;
	int _font, _fontSize;
	wchar_t *_entryHint; double _entryPosition;
	struct { wchar_t *page; int top; } _history [20];
	int _historyPointer;
	wchar_t *_currentPageTitle;
	GuiObject _fontSizeButton_10, _fontSizeButton_12, _fontSizeButton_14, _fontSizeButton_18, _fontSizeButton_24;
	GuiObject _holder;
	void *_praatApplication, *_praatObjects, *_praatPicture;
	bool _scriptErrorHasBeenNotified;
	structMelderDir _rootDirectory;

  protected:
	void updateSizeMenu ();
	void createVerticalScrollBar (GuiObject parent);
	void createChildren ();
};

/* End of file HyperPage.h */
#endif
