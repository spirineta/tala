/* Editor.c
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
 * pb 2002/03/07 GPL
 * pb 2002/03/22 Editor_setPublish2Callback
 * pb 2005/09/01 do not add a "(cannot) undo" button if there is no data to save
 * pb 2007/06/10 wchar_t
 * pb 2007/09/02 form/ok/do_pictureWindow
 * pb 2007/09/08 createMenuItems_file and createMenuItems_edit
 * pb 2007/09/19 info
 * pb 2007/12/05 enums
 * pb 2008/03/20 split off Help menu
 * sdk 2008/03/24 GTK
 * pb 2009/01/18 arguments to UiForm *callbacks
 * pb 2009/08/19 allow editor windows without menu bar (requested by DemoEditor)
 * fb 2010/02/23 GTK
 * pb 2010/07/29 removed GuiWindow_show
 */

#include "Editor.h"

#include "ui/machine.h"
#include "ui/praat.h"
#include "ui/praat_script.h"
#include "ui/Preferences.h"
#include "ScriptEditor.h"
#include "ui/UiHistory.h"

#include "sys/enums_getText.h"
#include "Editor_enums.h"
#include "sys/enums_getValue.h"
#include "Editor_enums.h"

#include "EditorM.h"

#include <time.h>

static void commonCallback (GUI_ARGS) {
	EditorCommand *cmd = (EditorCommand *)void_me;
	if (G_OBJECT_TYPE (w) == GTK_TYPE_RADIO_MENU_ITEM && ! gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (w))) {
		return;
	}
	if (cmd->_editor && cmd->_editor->isScriptable() && ! wcsstr (cmd->_itemTitle, L"...")) {
		UiForm::history.write (L"\n");
		UiForm::history.write (cmd->_itemTitle);
	}
	if (! cmd->_commandCallback (cmd->_editor, cmd, NULL, NULL, NULL)) Melder_flushError (NULL);
}

/********** PREFERENCES **********/

static struct {
	struct {
		bool eraseFirst;
		enum kEditor_writeNameAtTop writeNameAtTop;
	} picture;
} preferences;

void Editor::prefs (void) {
	Preferences_addBool (L"Editor.picture.eraseFirst", & preferences.picture.eraseFirst, true);
	Preferences_addEnum (L"Editor.picture.writeNameAtTop", & preferences.picture.writeNameAtTop, kEditor_writeNameAtTop, DEFAULT);
}

/********** EditorCommand **********/

EditorCommand::~EditorCommand () {
	Melder_free (_itemTitle);
	Melder_free (_script);
	forget (_dialog);
}

/********** EditorMenu ***********/

EditorMenu::~EditorMenu () {
	Melder_free (_menuTitle);
	forget (_commands);
}

EditorCommand * EditorMenu::addCommand (const wchar_t *itemTitle, long flags,
	int (*commandCallback) (Editor *editor, EditorCommand *cmd, UiForm *sendingForm, const wchar_t *sendingString, Interpreter *interpreter))
{
	EditorCommand *cmd = new EditorCommand();
	cmd->_editor = _editor;
	cmd->_menu = this;
	if ((cmd->_itemTitle = Melder_wcsdup_e (itemTitle)) == NULL) { delete cmd; return NULL; }
	cmd->_itemWidget =
		commandCallback == NULL ? GuiMenu_addSeparator (_menuWidget) :
		flags & Editor_HIDDEN ? NULL :
		GuiMenu_addItem (_menuWidget, itemTitle, flags, commonCallback, cmd);
	Collection_addItem (_commands, cmd);
	cmd->_commandCallback = commandCallback;
	return cmd;
}

/********** Editor **********/

void Editor::gui_window_cb_goAway (void *editor) {
	((Editor *)editor)->goAway ();
}

int Editor::menu_cb_newScript (EDITOR_ARGS) {
	ScriptEditor::createFromText (editor_me->_parent, editor_me, NULL);
end:
	iferror return 0;
	return 1;
}

int Editor::menu_cb_openScript (EDITOR_ARGS) {
	ScriptEditor *scriptEditor = ScriptEditor::createFromText (editor_me->_parent, editor_me, NULL);
	scriptEditor->showOpen ();
end:
	iferror return 0;
	return 1;
}

int Editor::menu_cb_close (EDITOR_ARGS) {
	editor_me->goAway ();
	return 1;
}

/*
	This creates _shell and _dialog,
	calls the createMenus and createChildren methods,
	and manages _shell and _dialog.
	'width' and 'height' determine the dimensions of the editor:
	if 'width' < 0, the width of the screen is added to it;
	if 'height' < 0, the height of the screeen is added to it;
	if 'width' is 0, the width is based on the children;
	if 'height' is 0, the height is base on the children.
	'x' and 'y' determine the position of the editor:
	if 'x' > 0, 'x' is the distance to the left edge of the screen;
	if 'x' < 0, |'x'| is the diatnce to the right edge of the screen;
	if 'x' is 0, the editor is horizontally centred on the screen;
	if 'y' > 0, 'y' is the distance to the top of the screen;
	if 'y' < 0, |'y'| is the diatnce to the bottom of the screen;
	if 'y' is 0, the editor is vertically centred on the screen;
	This routine does not transfer ownership of 'data' to the Editor,
	and the Editor will not destroy 'data' when the Editor itself is destroyed;
	however, some Editors may change 'data' (and not only '*data'),
	in which case the original 'data' IS destroyed,
	so the creator will have to install the dataChangedCallback in order to be notified,
	and replace its now dangling pointers with the new one.
	To prevent synchronicity problems, the Editor should destroy the old 'data'
	immediately AFTER calling its dataChangedCallback.
	Most Editors, by the way, will not need to change 'data'; they only change '*data',
	but the dataChangedCallback may still be useful if there is more than one editor
	with the same data; in this case, the owner of all those editors will
	(in the dataChangedCallback it installed) broadcast the change to the other editors
	by sending them an Editor_dataChanged () message.
*/
Editor::Editor (GuiObject parent, int x, int y, int width, int height, const wchar_t *title, Any data)
	: _name(Melder_wcsdup_f (title)),
	  _parent(parent),
	  _menuBar(NULL),
	  _undoButton(NULL),
	  _searchButton(NULL),
	  _menus(NULL),
	  _data(data),
	  _previousData(NULL),
	  _pictureGraphics(NULL),
	  _destroyCallback(NULL),
	  _destroyClosure(NULL),
	  _dataChangedCallback(NULL),
	  _dataChangedClosure(NULL),
	  _publishCallback(NULL),
	  _publishClosure(NULL),
	  _publish2Callback(NULL),
	  _publish2Closure(NULL) {
	GdkScreen *screen = gdk_screen_get_default ();
	if (parent != NULL) {
		GuiObject parent_win = gtk_widget_get_ancestor (parent, GTK_TYPE_WINDOW);
		if (parent_win != NULL) {
			screen = gtk_window_get_screen (GTK_WINDOW (parent_win));
		}
	}
	int screenWidth = gdk_screen_get_width (screen);
	int screenHeight = gdk_screen_get_height (screen);
	int left, right, top, bottom;
	screenHeight -= Machine_getTitleBarHeight ();
	if (width < 0) width += screenWidth;
	if (height < 0) height += screenHeight;
	if (width > screenWidth - 10) width = screenWidth - 10;
	if (height > screenHeight - 10) height = screenHeight - 10;
	if (x > 0) {
		right = (left = x) + width;
	} else if (x < 0) {
		left = (right = screenWidth + x) - width;
	} else { /* Randomize. */
		right = (left = NUMrandomInteger (4, screenWidth - width - 4)) + width;
	}
	if (y > 0) {
		bottom = (top = y) + height;
	} else if (y < 0) {
		top = (bottom = screenHeight + y) - height;
	} else { /* Randomize. */
		bottom = (top = NUMrandomInteger (4, screenHeight - height - 4)) + height;
	}
	#ifndef _WIN32
		top += Machine_getTitleBarHeight ();
		bottom += Machine_getTitleBarHeight ();
	#endif
	_dialog = GuiWindow_create (parent, left, top, right - left, bottom - top, title, gui_window_cb_goAway, this, canFullScreen() ? GuiWindow_FULLSCREEN : 0);
	_shell = GuiObject_parent (_dialog);

	/* Create menus. */

	Melder_clearError ();   /* FIXME: to protect against CategoriesEditor */
	_menus = Ordered_create (); // FIXME create menubar'd subclass
	_menuBar = Gui_addMenuBar (_dialog);
	EditorMenu *helpMenu = addMenu (L"Help", 0);
	createMenus ();
	GuiObject_show (_menuBar); // FIXME
	/*if (isScriptable()) {
		addCommand (L"File", L"New editor script", 0, menu_cb_newScript);
		addCommand (L"File", L"Open editor script...", 0, menu_cb_openScript);
		addCommand (L"File", L"-- after script --", 0, 0);
	}*/
	//if (hasMenuBar()) {
		/*
		 * Add the scripted commands.
		 */
		/*praat_addCommandsToEditor (this); FIXME
		addCommand (L"File", L"Close", 'W', menu_cb_close);*/
	//}

	GuiObject_show (_dialog);
}

Editor::~Editor () {
	goAway();
}

EditorMenu *Editor::addMenu (const wchar_t *menuTitle, long flags) {
	EditorMenu *menu = new EditorMenu ();
	menu->_editor = this;
	if (! (menu->_menuTitle = Melder_wcsdup_e (menuTitle))) { delete menu; return NULL; }
	menu->_menuWidget = GuiMenuBar_addMenu (_menuBar, menuTitle, flags);
	Collection_addItem (_menus, menu);
	menu->_commands = Ordered_create ();
	return menu;
}

EditorMenu * Editor::getMenu (const wchar_t *menuTitle) {
	int numberOfMenus = _menus -> size, imenu;
	for (imenu = 1; imenu <= numberOfMenus; imenu ++) {
		EditorMenu *menu = (EditorMenu*)_menus -> item [imenu];
		if (wcsequ (menuTitle, menu -> _menuTitle))
			return menu;
	}
	Melder_error3 (L"(Editor::getMenu:) No menu \"", menuTitle, L"\".");
	return NULL;
}

void Editor::setMenuSensitive (const wchar_t *menuTitle, int sensitive) {
	int numberOfMenus = _menus -> size, imenu;
	for (imenu = 1; imenu <= numberOfMenus; imenu ++) {
		EditorMenu *menu = (EditorMenu *)_menus -> item [imenu];
		if (wcsequ (menuTitle, menu -> _menuTitle)) {
			GuiObject_setSensitive (menu -> _menuWidget, sensitive);
			return;
		}
	}
}

EditorCommand *Editor::getMenuCommand (const wchar_t *menuTitle, const wchar_t *itemTitle) {
	int numberOfMenus = _menus -> size, imenu;
	for (imenu = 1; imenu <= numberOfMenus; imenu ++) {
		EditorMenu *menu = (EditorMenu *)_menus -> item [imenu];
		if (wcsequ (menuTitle, menu -> _menuTitle)) {
			int numberOfCommands = menu -> _commands -> size, icommand;
			for (icommand = 1; icommand <= numberOfCommands; icommand ++) {
				EditorCommand *command = (EditorCommand *)menu -> _commands -> item [icommand];
				if (wcsequ (itemTitle, command -> _itemTitle))
					return command;
			}
		}
	}
	Melder_error5 (L"(Editor::getMenuCommand:) No menu \"", menuTitle, L"\" with item \"", itemTitle, L"\".");
	return NULL;
}

int Editor::doMenuCommand (const wchar_t *commandTitle, const wchar_t *arguments, Interpreter *interpreter) {
	int numberOfMenus = _menus -> size, imenu;
	for (imenu = 1; imenu <= numberOfMenus; imenu ++) {
		EditorMenu *menu = (EditorMenu *)_menus -> item [imenu];
		int numberOfCommands = menu -> _commands -> size, icommand;
		for (icommand = 1; icommand <= numberOfCommands; icommand ++) {
			EditorCommand *command = (EditorCommand *)menu -> _commands -> item [icommand];
			if (wcsequ (commandTitle, command -> _itemTitle)) {
				if (! command -> _commandCallback (this, command, NULL, arguments, interpreter))
					return 0;
				return 1;
			}
		}
	}
	return Melder_error1 (L"Command not available in editor");
}

void Editor::info () {
	Melder_clearInfo ();
	MelderInfo_open (); // FIXME object id like in Thing.c
	MelderInfo_writeLine2 (L"Editor type: ", type ());
	MelderInfo_writeLine2 (L"Editor name: ", _name ? _name : L"<no name>");
	time_t today = time (NULL);
	MelderInfo_writeLine2 (L"Date: ", Melder_peekUtf8ToWcs (ctime (& today)));   /* Includes a newline. */
	if (_data) {
		MelderInfo_writeLine2 (L"Data type: ", ((Thing) _data) -> methods -> _className);
		MelderInfo_writeLine2 (L"Data name: ", ((Thing) _data) -> name);
	}
	MelderInfo_close ();
}

void Editor::nameChanged () {
	if (_name)
		GuiWindow_setTitle (_shell, _name);
}

void Editor::goAway () {
	MelderAudio_stopPlaying (MelderAudio_IMPLICIT);
	/*
	 * The following command must be performed before the shell is destroyed.
	 * Otherwise, we would be forgetting dangling command dialogs here.
	 */
	forget (_menus);
	if (_shell) {
		Melder_assert (GTK_IS_WIDGET (_shell));
		gtk_widget_destroy (_shell);
	}
	if (_destroyCallback) _destroyCallback (this, _destroyClosure);
	forget (_previousData);
}

void Editor::dataChanged () {}

void Editor::clipboardChanged (Any clipboard) {}

void Editor::save () {
	if (_data) {
		forget (_previousData);
		_previousData = Data_copy (_data);
	}
}

void Editor::restore () {
	if (_data && _previousData)   /* Swap contents of _data and _previousData. */
		Thing_swap (_data, _previousData);
}

int Editor::menu_cb_undo (EDITOR_ARGS) {
	editor_me->restore ();
	if (wcsnequ (editor_me->_undoText, L"Undo", 4)) editor_me->_undoText [0] = 'R', editor_me->_undoText [1] = 'e';
	else if (wcsnequ (editor_me->_undoText, L"Redo", 4)) editor_me->_undoText [0] = 'U', editor_me->_undoText [1] = 'n';
	else wcscpy (editor_me->_undoText, L"Undo?");
	gtk_label_set_label (GTK_LABEL(gtk_bin_get_child(GTK_BIN(editor_me->_undoButton))), Melder_peekWcsToUtf8 (editor_me->_undoText));
	/*
	 * Send a message to myself (e.g., I will redraw myself).
	 */
	editor_me->dataChanged ();
	/*
	 * Send a message to _boss (e.g., she will notify the others that depend on me).
	 */
	editor_me->broadcastChange ();
	return 1;
}

int Editor::menu_cb_settingsReport (EDITOR_ARGS) {
	Melder_clearInfo ();
	MelderInfo_open (); // FIXME object id like in Thing.c
	editor_me->info ();
	MelderInfo_close ();
	return 1;
}

int Editor::menu_cb_info (EDITOR_ARGS) {
	if (editor_me->_data) Thing_info (editor_me->_data);
	return 1;
}

void Editor::createMenus () {
	EditorMenu *menu = addMenu (L"File", 0);
	if (isEditable()) { // FIXME subclass
		menu = addMenu (L"Edit", 0);
		if (_data)
			_undoButton = menu->addCommand (L"Cannot undo", GuiMenu_INSENSITIVE + 'Z', menu_cb_undo) -> _itemWidget;
	}
	menu = addMenu (L"Query", 0); // TODO check that this should always be executed
	menu->addCommand (L"Editor info", 0, menu_cb_settingsReport);
	menu->addCommand (L"Settings report", Editor_HIDDEN, menu_cb_settingsReport);
	if (_data) {
		static MelderString title = { 0 };
		MelderString_empty (& title);
		MelderString_append2 (& title, Thing_className (_data), L" info");
		menu->addCommand (title.string, 0, menu_cb_info);
	}
}

void Editor::form_pictureWindow (EditorCommand *cmd) {
	LABEL (L"", L"Picture window:")
	BOOLEAN (L"Erase first", 1);
}
void Editor::ok_pictureWindow (EditorCommand *cmd) {
	SET_INTEGER (L"Erase first", preferences.picture.eraseFirst);
}
void Editor::do_pictureWindow (EditorCommand *cmd) {
	preferences.picture.eraseFirst = GET_INTEGER (L"Erase first");
}

void Editor::form_pictureMargins (EditorCommand *cmd) {
	UiForm::UiField *radio = NULL;
	LABEL (L"", L"Margins:")
	OPTIONMENU_ENUM (L"Write name at top", kEditor_writeNameAtTop, DEFAULT);
}
void Editor::ok_pictureMargins (EditorCommand *cmd) {
	SET_ENUM (L"Write name at top", kEditor_writeNameAtTop, preferences.picture.writeNameAtTop);
}
void Editor::do_pictureMargins (EditorCommand *cmd) {
	preferences.picture.writeNameAtTop = GET_ENUM (kEditor_writeNameAtTop, L"Write name at top");
}

void Editor::raise () {
	GuiObject_show (_dialog);
}

void Editor::changeData (Any data) {
	/*if (data) _data = data; BUG */
	(void) data;
	dataChanged();
}

void Editor::changeClipboard (Any data) {
	clipboardChanged(data);
}

void Editor::setDestroyCallback (void (*cb) (Editor *editor, void *closure), void *closure) {
	_destroyCallback = cb;
	_destroyClosure = closure;
}

void Editor::broadcastChange () {
	if (_dataChangedCallback)
		_dataChangedCallback (this, _dataChangedClosure, NULL);
}

void Editor::setDataChangedCallback (void (*cb) (Editor *editor, void *closure, Any data), void *closure) {
	_dataChangedCallback = cb;
	_dataChangedClosure = closure;
}

void Editor::setPublishCallback (void (*cb) (Editor *editor, void *closure, Any publish), void *closure) {
	_publishCallback = cb;
	_publishClosure = closure;
}

void Editor::setPublish2Callback (void (*cb) (Editor *editor, void *closure, Any publish1, Any publish2), void *closure) {
	_publish2Callback = cb;
	_publish2Closure = closure;
}

void Editor::save (const wchar_t *text) {
	if (! _undoButton) return;
	GuiObject_setSensitive (_undoButton, True);
	swprintf (_undoText, 100, L"Undo %ls", text);
	gtk_label_set_label (GTK_LABEL(gtk_bin_get_child(GTK_BIN(_undoButton))), Melder_peekWcsToUtf8 (_undoText));
}

void Editor::openPraatPicture () {
	_pictureGraphics = praat_picture_editor_open (preferences.picture.eraseFirst);
}
void Editor::closePraatPicture () {
	if (_data != NULL && preferences.picture.writeNameAtTop != kEditor_writeNameAtTop_NO) {
		Graphics_setNumberSignIsBold (_pictureGraphics, false);
		Graphics_setPercentSignIsItalic (_pictureGraphics, false);
		Graphics_setCircumflexIsSuperscript (_pictureGraphics, false);
		Graphics_setUnderscoreIsSubscript (_pictureGraphics, false);
		Graphics_textTop (_pictureGraphics,
			preferences.picture.writeNameAtTop == kEditor_writeNameAtTop_FAR,
			((Data) _data) -> name);
		Graphics_setNumberSignIsBold (_pictureGraphics, true);
		Graphics_setPercentSignIsItalic (_pictureGraphics, true);
		Graphics_setCircumflexIsSuperscript (_pictureGraphics, true);
		Graphics_setUnderscoreIsSubscript (_pictureGraphics, true);
	}
	praat_picture_editor_close ();
}

/* End of file Editor.c */
