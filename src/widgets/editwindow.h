/* Copyright (C) 2011 G.P. Halkes
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 3, as
   published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef T3_WIDGET_EDITWINDOW_H
#define T3_WIDGET_EDITWINDOW_H

namespace t3_widget {
	class edit_window_t;
}; // namespace

#include <t3widget/widgets/widget.h>
#include <t3widget/widgets/scrollbar.h>
#include <t3widget/textbuffer.h>
#include <t3widget/key.h>
#include <t3widget/interfaces.h>

namespace t3_widget {

class finder_t;
class wrap_info_t;
class goto_dialog_t;
class find_dialog_t;
class replace_buttons_dialog_t;

/** Class implementing an edit widget. */
class T3_WIDGET_API edit_window_t : public widget_t, public center_component_t, public container_t, public bad_draw_recheck_t {
	protected:
		static goto_dialog_t *goto_dialog;
		static sigc::connection goto_connection;
		static find_dialog_t *global_find_dialog;
		static sigc::connection global_find_dialog_connection;
		static finder_t global_finder;
		static replace_buttons_dialog_t *replace_buttons;
		static sigc::connection replace_buttons_connection;
		static sigc::connection init_connected;

		t3_window_t *edit_window, /**< Window containing the text. */
			*bottom_line_window; /**< Window holding the information line at the bottom. */
		scrollbar_t scrollbar; /**< Scrollbar on the right of the text. */
		text_buffer_t *text; /**< Buffer holding the text currently displayed. */
		int screen_pos; /**< Cached position of cursor in screen coordinates. */
		int tabsize; /**< Width of a tab, in cells. */
		bool focus; /**< Boolean indicating whether this edit_window_t has the input focus. */
		bool tab_spaces; /**< Boolean indicating whether to use spaces for tab. */
		/** Associated find dialog.
		    By default this is the shared dialog, but can be set to a different one.
		*/
		find_dialog_t *find_dialog;
		finder_t *finder; /**< Object used for find actions in the text. */
		wrap_type_t wrap_type; /**< The wrap_type_t used for display. */
		wrap_info_t *wrap_info; /**< Required information for wrapped display, or @c NULL if not in use. */
		/** The top-left coordinate in the text.
		    This is either a proper text_coordinate_t when wrapping is disabled, or
		    a line and sub-line (pos @c member) coordinate when wrapping is enabled.
		*/
		text_coordinate_t top_left;
		int ins_mode, /**< Current insert/overwrite mode. */
			last_set_pos; /**< Last horiziontal position set by user action. */
		bool auto_indent; /**< Boolean indicating whether automatic indentation should be enabled. */
		bool indent_aware_home; /**< Boolean indicating whether home key should handle indentation specially. */


		/** Function to initialize the shared dialogs and data. */
		static void init(void);
		/** Strings displayed in the bottom line when inserting/overwriting. */
		static const char *ins_string[];
		/** Function pointer for calling insert/replace depending on insert/overwrite status. */
		static bool (text_buffer_t::*proces_char[])(key_t);

		/** Ensure that the cursor is visible. */
		void ensure_cursor_on_screen(void);
		/** Redraw the contents of the edit_window_t. */
		void repaint_screen(void);
		/** Handle cursor right key. */
		void inc_x(void);
		/** Handle control-cursor right key. */
		void next_word(void);
		/** Handle cursor left key. */
		void dec_x(void);
		/** Handle control-cursor left key. */
		void previous_word(void);
		/** Handle cursor down key. */
		void inc_y(void);
		/** Handle cursor up key. */
		void dec_y(void);
		/** Handle page-down key. */
		void pgdn(void);
		/** Handle page-up key. */
		void pgup(void);
		/** Handle home key. */
		void home(void);
		/** Reset the selection. */
		void reset_selection(void);
		/** Set the selection mode based on the current key pressed by the user. */
		void set_selection_mode(key_t key);
		/** Delete the selection. */
		void delete_selection(void);

		/** The find or replace action has been activated in the find or replace buttons dialog. */
		void find_activated(find_action_t action, finder_t *finder);
		/** Move the cursor to the specified line. */
		void goto_line(int line);
		/** Handle setting of the wrap mode. */
		void set_wrap_internal(wrap_type_t wrap);

	public:
		class view_parameters_t;

		/** Create a new edit_window_t.
		    @param _text The text to display in the edit_window_t.
		    @param params The view parameters to set.
		*/
		edit_window_t(text_buffer_t *_text = NULL, const view_parameters_t *params = NULL);
		virtual ~edit_window_t(void);
		virtual bool process_key(key_t key);
		virtual bool set_size(optint height, optint width);
		virtual void update_contents(void);
		virtual void set_focus(bool focus);
		virtual void force_redraw(void);
		virtual void bad_draw_recheck(void);

		/** Set the text to display.
		    The previously displayed text will be replaced, without deleting. Use
		    get_text to retrieve the current text_buffer_t before calling this
		    function to make sure you have a reference to the current buffer that
		    you can use to delete the old text_buffer_t.
		*/
		void set_text(text_buffer_t *_text, const view_parameters_t *params = NULL);
		/** Get the text currently displayed. */
		text_buffer_t *get_text(void) const;
		/** Apply the undo action. */
		void undo(void);
		/** Apply the redo action. */
		void redo(void);
		/** Apply the cut or copy action.
		    @param cut A boolean indicating whether a cut (@c true) should be performed, or a copy (@c false).
		    The selected text is stored in copy_buffer.
		*/
		void cut_copy(bool cut);
		/** Apply the paste action.
		    The text to paste is taken from copy_buffer.
		*/
		void paste(void);
		/** Select the whole text. */
		void select_all(void);
		/** Show the Insert Special Character dialog. */
		void insert_special(void);
		/** Indent the current selection. */
		void indent_selection(void);
		/** Unindent the current selection. */
		void unindent_selection(void);

		/** Show the Goto Line dialog. */
		void goto_line(void);
		/** Show the find or replace dialog. */
		void find_replace(bool replace);
		/** Apply the find-next action.
		    @param backward Boolean indicating whether to reverse the direction of the search relative to the original search direction.
		*/
		void find_next(bool backward);
		/** Set the find_dialog_t to use.
		    In some cases it can be useful @e not to use the global find_dialog_t.
		    This function allows one to set the find_dialog_t used.
		*/
		void set_find_dialog(find_dialog_t *_find_dialog);
		/** Set the finder_t to use.
		    Because the finder_t stores the search information, such as the text searched
		    for, it is sometimes useful to use different finder_t instances for different windows.
		    The finder_t is used for example for the find-next action.
		*/
		void set_finder(finder_t *_finder);

		/** Set the size of a tab. */
		void set_tabsize(int _tabsize);
		/** Set the wrap type. */
		void set_wrap(wrap_type_t wrap);
		/** Set tab indents with spaces. */
		void set_tab_spaces(bool _tab_spaces);
		/** Set automatic indent. */
		void set_auto_indent(bool _auto_indent);
		/** Set indent aware home. */
		void set_indent_aware_home(bool _indent_aware_home);

		/** Get the size of a tab. */
		int get_tabsize(void);
		/** Get the wrap type. */
		wrap_type_t get_wrap(void);
		/** Get tab indents with spaces. */
		bool get_tab_spaces(void);
		/** Get automatic indent. */
		bool get_auto_indent(void);
		/** Get indent aware home. */
		bool get_indent_aware_home(void);

		/** Save the current view parameters, to allow them to be restored later. */
		view_parameters_t *save_view_parameters(void);
		/** Save the current view parameters, to allow them to be restored later. */
		void save_view_parameters(view_parameters_t *params);
};

class edit_window_t::view_parameters_t {
	friend class edit_window_t;

	private:
		text_coordinate_t top_left;
		wrap_type_t wrap_type;
		int tabsize;
		bool tab_spaces;
		int ins_mode, last_set_pos;
		bool auto_indent;
		bool indent_aware_home;

		view_parameters_t(edit_window_t *view);
		void apply_parameters(edit_window_t *view) const;

	public:
		view_parameters_t(void);
		void set_tabsize(int _tabsize);
		void set_wrap(wrap_type_t _wrap_type);
		void set_tab_spaces(bool _tab_spaces);
		void set_auto_indent(bool _auto_indent);
		void set_indent_aware_home(bool _indent_aware_home);
};


}; // namespace
#endif
