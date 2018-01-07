/* Copyright (C) 2011-2013 G.P. Halkes
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
#ifndef T3_WIDGET_TEXTFIELD_H
#define T3_WIDGET_TEXTFIELD_H

#include <string>
#include <map>
#include <vector>

#include <t3widget/widgets/widget.h>
#include <t3widget/textline.h>
#include <t3widget/contentlist.h>
#include <t3widget/widgets/smartlabel.h>
#include <t3widget/widgets/listpane.h>

namespace t3_widget {

class T3_WIDGET_API text_field_t : public widget_t, public center_component_t, public focus_widget_t, public bad_draw_recheck_t
{
	public:
		/** Actions which can be bound to keys. */
		enum Action {
	#define _T3_ACTION(action, name) ACTION_##action,
	#include <t3widget/widgets/textfield.actions.h>
	#undef _T3_ACTION
		};

	private:
		class T3_WIDGET_LOCAL drop_down_list_t;

		static signals::connection init_connected;
		static std::map<key_t, Action> key_bindings;

		struct T3_WIDGET_LOCAL implementation_t {
			int pos, /**< Cursor position in bytes. */
				screen_pos, /**< Cursor position in screen cells. */
				leftcol, /**< The first (left-most) shown column in screen cells. */
				selection_start_pos, /**< Selection start postion, or -1 if not applicable. */
				selection_end_pos; /**< Selection end postion, or -1 if not applicable. */

			selection_mode_t selection_mode; /**< Selection mode. */
			bool focus, /**< Boolean indicating whether this text_field_t has the input focus. */
				in_drop_down_list, /**< Boolean indicating whether the "cursor" is in the drop-down list. */
				/** Boolean indicating whether not to select the whole text on regaining focus.
					This boolean exists mostly to facilitate the "Insert Character" dialog. When
					the text_field_t regains focus after the dialog closes, it should not select
					the whole contents like it normally does on regaining focus.
				*/
				dont_select_on_focus,
				/** Boolean indicating whether the contents has changed since the last redraw.
					Used only for optimization purposes.
				*/
				edited;

			cleanup_ptr<text_line_t>::t line; /**< Variable containing the current text. */
			const key_t *filter_keys; /**< List of keys to accept or reject. */
			size_t filter_keys_size; /**< Size of #filter_keys. */
			bool filter_keys_accept; /**< Boolean indicating whether the keys in #filter_keys should be accepted or rejected. */

			smart_label_t *label; /**< Label associated with this text_field_t. */

			cleanup_ptr<drop_down_list_t>::t drop_down_list;

			implementation_t(void) : pos(0),
				screen_pos(0),
				leftcol(0),
				focus(false),
				in_drop_down_list(false),
				dont_select_on_focus(false),
				edited(false),
				line(new text_line_t()),
				filter_keys(NULL),
				label(NULL),
				drop_down_list(NULL)
			{}
		};
		pimpl_ptr<implementation_t>::t impl;

		/** Function to initialize the shared dialogs and data. */
		static void init(bool _init);

		/** Reset the selection. */
		void reset_selection(void);
		/** Change the current selection mode, based on @p key. */
		void set_selection(key_t key);
		/** Delete the current selection. */
		void delete_selection(bool save_to_copy_buffer);
		/** Make sure the text in the text_field_t is aligned such that the cursor is visible. */
		void ensure_cursor_on_screen(void);

		/** Set the end of the selection to the current position, updating the primary selection if so requested. */
		void set_selection_end(bool update_primary = true);

	protected:
		bool has_focus(void) const;

	public:
		text_field_t(void);
		virtual bool process_key(key_t key);
		virtual bool set_size(optint height, optint width);
		virtual void update_contents(void);
		virtual void set_focus(focus_t _focus);
		virtual void show(void);
		virtual void hide(void);
		/** Set the text of the text_field_t. */
		void set_text(const std::string *text);
		/** Set the text of the text_field_t. */
		void set_text(const char *text);
		/** Set the text of the text_field_t. */
		void set_text(const char *text, size_t size);
		/** Set the autocompletion list. */
		void set_autocomplete(string_list_base_t *_completions);
		/** Set the list of keys to accept or reject.
		    @param keys The list of keys to accept or reject.
		    @param nr_of_keys The size of @p keys.
		    @param accept Boolean indicating whether the keys in @p keys should be accepted or rejected.
		*/
		void set_key_filter(key_t *keys, size_t nr_of_keys, bool accept);
		/** Retrieve the text shown by the text_field_t. */
		const std::string *get_text(void) const;
		/** Associate a label with this text_field_t.
		    The reason to associate a smart_label_t with a text_field_t is that it
		    allows the use of a hotkey to jump to the text_field_t. The #is_hotkey
		    function will report the result of smart_label_t::is_hotkey.
		*/
		void set_label(smart_label_t *_label);
		virtual bool is_hotkey(key_t key);

		virtual void bad_draw_recheck(void);
		virtual bool process_mouse_event(mouse_event_t event);

		/** Returns the @c Action value for the given name, or @c nullopt if it could not be found. */
		static optional<Action> map_action_name(const std::string &name);
		/** Binds the key to the action.
		    Pass @c nullopt to unbind the key. */
		static void bind_key(key_t key, optional<Action> action);
		/** Returns a vector with the names of all the available actions. */
		static const std::vector<std::string> &get_action_names();

	T3_WIDGET_SIGNAL(activate, void);
};

/** Drop-down list implementation for text_field_t. */
class T3_WIDGET_LOCAL text_field_t::drop_down_list_t : public popup_t {
	private:
		text_field_t *field; /**< text_field_t this drop-down list is created for. */

		cleanup_ptr<filtered_list_base_t>::t completions; /**< List of possible selections. */
		list_pane_t *list_pane;

		void update_list_pane(void);
		void item_activated(void);
		void selection_changed(void);
	public:
		drop_down_list_t(text_field_t *_field);
		virtual bool process_key(key_t key);
		virtual void set_position(optint top, optint left);
		virtual bool set_size(optint height, optint width);
		virtual void update_contents(void);
		virtual void set_focus(focus_t focus) ;
		virtual void show(void);
		virtual void hide(void);
		virtual bool process_mouse_event(mouse_event_t key);

		/** Request that the drop-down is filtered based on the contents of the text_field_t it is asscociated with. */
		void update_view(void);
		/** Set the list of autocompletion options. */
		void set_autocomplete(string_list_base_t *completions);
		/** Return whether the autocompletion list is empty. */
		bool empty(void);
};

}; // namespace
#endif
