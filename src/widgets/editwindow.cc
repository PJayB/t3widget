/* Copyright (C) 2010 G.P. Halkes
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
#include <cstring>

#include "dialogs/gotodialog.h"
#include "dialogs/finddialog.h"
#include "editwindow.h"
#include "util.h"
#include "colorscheme.h"
#include "internal.h"
#include "findcontext.h"
#include "wrapinfo.h"

using namespace std;
namespace t3_widget {

goto_dialog_t *edit_window_t::goto_dialog;
sigc::connection edit_window_t::goto_connection;
find_dialog_t *edit_window_t::global_find_dialog;
sigc::connection edit_window_t::global_find_dialog_connection;
finder_t edit_window_t::global_finder;
replace_buttons_dialog_t *edit_window_t::replace_buttons;
sigc::connection edit_window_t::replace_buttons_connection;
sigc::connection edit_window_t::init_connected = connect_on_init(sigc::ptr_fun(edit_window_t::init));

const char *edit_window_t::ins_string[] = {"INS", "OVR"};
bool (text_buffer_t::*edit_window_t::proces_char[])(key_t) = { &text_buffer_t::insert_char, &text_buffer_t::overwrite_char};

void edit_window_t::init(void) {
	/* Construct these from t3_widget::init, such that the locale is set correctly and
	   gettext therefore returns the correctly localized strings. */
	goto_dialog = new goto_dialog_t();
	global_find_dialog = new find_dialog_t();
	replace_buttons = new replace_buttons_dialog_t();
}

edit_window_t::edit_window_t(text_buffer_t *_text, view_parameters_t *params) : edit_window(NULL),
		bottom_line_window(NULL), scrollbar(true), text(NULL), find_dialog(NULL), finder(NULL),
		wrap_type(wrap_type_t::NONE), wrap_info(NULL)
{
	init_unbacked_window(11, 11);
	if ((edit_window = t3_win_new(window, 10, 10, 0, 0, 0)) == NULL)
		throw bad_alloc();
	t3_win_show(edit_window);

	if ((bottom_line_window = t3_win_new(window, 1, 11, 0, 0, 0)) == NULL) {
		t3_win_del(edit_window);
		throw bad_alloc();
	}
	t3_win_set_anchor(bottom_line_window, window, T3_PARENT(T3_ANCHOR_BOTTOMLEFT) | T3_CHILD(T3_ANCHOR_BOTTOMLEFT));
	t3_win_show(bottom_line_window);

	set_widget_parent(&scrollbar);
	scrollbar.set_anchor(this, T3_PARENT(T3_ANCHOR_TOPRIGHT) | T3_CHILD(T3_ANCHOR_TOPRIGHT));
	scrollbar.set_size(10, None);

	set_text(_text == NULL ? new text_buffer_t() : _text, params);

	screen_pos = 0;
	focus = false;
}

edit_window_t::~edit_window_t(void) {
	t3_win_del(edit_window);
	t3_win_del(bottom_line_window);
	delete wrap_info;
}

void edit_window_t::set_text(text_buffer_t *_text, view_parameters_t *params) {
	if (text == _text)
		return;

	text = _text;
	if (params != NULL) {
		params->apply_parameters(this);
	} else if (wrap_info != NULL) {
		wrap_info->set_text_buffer(text);
		wrap_info->set_wrap_width(t3_win_get_width(edit_window) - 1);
		top_left.line = 0;
		top_left.pos = 0;
	}

	ensure_cursor_on_screen();
	redraw = true;
}

bool edit_window_t::set_size(optint height, optint width) {
	bool result = true;
	//FIXME: these int's are optional!!! Take that into account below!

	if (width != t3_win_get_width(window) || height > t3_win_get_height(window))
		redraw = true;

	result &= t3_win_resize(window, height, width);
	result &= t3_win_resize(edit_window, height - 1, width - 1);
	result &= t3_win_resize(bottom_line_window, 1, width);
	result &= scrollbar.set_size(height - 1, None);

	if (wrap_type != wrap_type_t::NONE) {
		top_left.pos = wrap_info->calculate_line_pos(top_left.line, 0, top_left.pos);
		wrap_info->set_wrap_width(width - 1);
		top_left.pos = wrap_info->find_line(top_left);
		text->last_set_pos = wrap_info->calculate_screen_pos();
	}
	ensure_cursor_on_screen();
	return result;
}

void edit_window_t::ensure_cursor_on_screen(void) {
	int width;

	if (text->cursor.pos == text->get_line_max(text->cursor.line))
		width = 1;
	else
		width = text->width_at_cursor();


	if (wrap_type == wrap_type_t::NONE) {
		screen_pos = text->calculate_screen_pos(NULL, tabsize);

		if (text->cursor.line < top_left.line) {
			top_left.line = text->cursor.line;
			redraw = true;
		}

		if (text->cursor.line >= top_left.line + t3_win_get_height(edit_window)) {
			top_left.line = text->cursor.line - t3_win_get_height(edit_window) + 1;
			redraw = true;
		}

		if (screen_pos < top_left.pos) {
			top_left.pos = screen_pos;
			redraw = true;
		}

		if (screen_pos + width > top_left.pos + t3_win_get_width(edit_window)) {
			top_left.pos = screen_pos + width - t3_win_get_width(edit_window);
			redraw = true;
		}
	} else {
		text_coordinate_t bottom;
		int sub_line = wrap_info->find_line(text->cursor);
		screen_pos = wrap_info->calculate_screen_pos();

		if (text->cursor.line < top_left.line || (text->cursor.line == top_left.line && sub_line < top_left.pos)) {
			top_left.line = text->cursor.line;
			top_left.pos = sub_line;
			redraw = true;
		} else {
			bottom = top_left;
			wrap_info->add_lines(bottom, t3_win_get_height(edit_window) - 1);

			while (text->cursor.line > bottom.line) {
				wrap_info->add_lines(top_left, wrap_info->get_line_count(bottom.line) - bottom.pos);
				bottom.line++;
				bottom.pos = 0;
				redraw = true;
			}

			if (text->cursor.line == bottom.line && sub_line > bottom.pos) {
				wrap_info->add_lines(top_left, sub_line - bottom.pos);
				redraw = true;
			}
		}
	}
}

void edit_window_t::repaint_screen(void) {
	text_coordinate_t current_start, current_end;
	text_line_t::paint_info_t info;
	int i;

	t3_win_set_default_attrs(edit_window, attributes.text);

	current_start = text->get_selection_start();
	current_end = text->get_selection_end();

	if (current_end.line < current_start.line || (current_end.line == current_start.line &&
			current_end.pos < current_start.pos)) {
		current_start = current_end;
		current_end = text->get_selection_start();
	}

	info.size = t3_win_get_width(edit_window);
	info.tabsize = tabsize;
	info.normal_attr = 0;
	info.selected_attr = attributes.text_selected;

	if (wrap_type == wrap_type_t::NONE) {
		info.leftcol = top_left.pos;
		for (i = 0; i < t3_win_get_height(edit_window) && (i + top_left.line) < text->size(); i++) {
			info.selection_start = top_left.line + i == current_start.line ? current_start.pos : -1;
			if (top_left.line + i >= current_start.line) {
				if (top_left.line + i < current_end.line)
					info.selection_end = INT_MAX;
				else if (top_left.line + i == current_end.line)
					info.selection_end = current_end.pos;
				else
					info.selection_end = -1;
			} else {
				info.selection_end = -1;
			}

			info.cursor = focus && top_left.line + i == text->cursor.line ? text->cursor.pos : -1;
			t3_win_set_paint(edit_window, i, 0);
			t3_win_clrtoeol(edit_window);
			text->paint_line(edit_window, top_left.line + i, &info);
		}
	} else {
		text_coordinate_t end = wrap_info->get_end();
		text_coordinate_t draw_line = top_left;
		info.leftcol = 0;
		for (i = 0; i < t3_win_get_height(edit_window); i++, wrap_info->add_lines(draw_line, 1)) {
			info.selection_start = draw_line.line == current_start.line ? current_start.pos : -1;
			if (draw_line.line >= current_start.line) {
				if (draw_line.line < current_end.line)
					info.selection_end = INT_MAX;
				else if (draw_line.line == current_end.line)
					info.selection_end = current_end.pos;
				else
					info.selection_end = -1;
			} else {
				info.selection_end = -1;
			}

			info.cursor = focus && draw_line.line == text->cursor.line ? text->cursor.pos : -1;
			t3_win_set_paint(edit_window, i, 0);
			t3_win_clrtoeol(edit_window);
			wrap_info->paint_line(edit_window, draw_line, &info);

			if (draw_line.line == end.line && draw_line.pos == end.pos)
				break;
		}
	}
	t3_win_clrtobot(edit_window);
}

void edit_window_t::inc_x(void) {
	if (text->cursor.pos == text->get_line_max(text->cursor.line)) {
		if (text->cursor.line >= text->size() - 1)
			return;

		text->cursor.line++;
		text->cursor.pos = 0;
	} else {
		text->adjust_position(1);
	}
	ensure_cursor_on_screen();
	text->last_set_pos = screen_pos;
}

void edit_window_t::next_word(void) {
	text->get_next_word();
	ensure_cursor_on_screen();
	text->last_set_pos = screen_pos;
}

void edit_window_t::dec_x(void) {
	if (screen_pos == 0) {
		if (text->cursor.line == 0)
			return;

		text->cursor.line--;
		text->cursor.pos = text->get_line_max(text->cursor.line);
	} else {
		text->adjust_position(-1);
	}
	ensure_cursor_on_screen();
	text->last_set_pos = screen_pos;
}

void edit_window_t::previous_word(void) {
	text->get_previous_word();
	ensure_cursor_on_screen();
	text->last_set_pos = screen_pos;
}

void edit_window_t::inc_y(void) {
	if (wrap_type == wrap_type_t::NONE) {
		if (text->cursor.line + 1 < text->size()) {
			text->cursor.line++;
			text->cursor.pos = text->calculate_line_pos(text->cursor.line, text->last_set_pos, tabsize);
			ensure_cursor_on_screen();
		} else {
			text->cursor.pos = text->get_line_max(text->cursor.line);
			ensure_cursor_on_screen();
			text->last_set_pos = screen_pos;
		}
	} else {
		int new_sub_line = wrap_info->find_line(text->cursor) + 1;
		if (wrap_info->get_line_count(text->cursor.line) == new_sub_line) {
			if (text->cursor.line + 1 < text->size()) {
				text->cursor.line++;
				text->cursor.pos = text->calculate_line_pos(text->cursor.line, text->last_set_pos, tabsize);
				ensure_cursor_on_screen();
			} else {
				text->cursor.pos = text->get_line_max(text->cursor.line);
				ensure_cursor_on_screen();
				text->last_set_pos = screen_pos;
			}
		} else {
			text->cursor.pos = wrap_info->calculate_line_pos(text->cursor.line, text->last_set_pos, new_sub_line);
			ensure_cursor_on_screen();
		}
	}
}

void edit_window_t::dec_y(void) {
	if (wrap_type == wrap_type_t::NONE) {
		if (text->cursor.line > 0) {
			text->cursor.line--;
			text->cursor.pos = text->calculate_line_pos(text->cursor.line, text->last_set_pos, tabsize);
			ensure_cursor_on_screen();
		} else {
			text->last_set_pos = text->cursor.pos = 0;
			ensure_cursor_on_screen();
		}
	} else {
		int sub_line = wrap_info->find_line(text->cursor);
		if (sub_line > 0) {
			text->cursor.pos = wrap_info->calculate_line_pos(text->cursor.line, text->last_set_pos, sub_line - 1);
			ensure_cursor_on_screen();
		} else if (text->cursor.line > 0) {
			text->cursor.line--;
			text->cursor.pos = wrap_info->calculate_line_pos(text->cursor.line, text->last_set_pos,
				wrap_info->get_line_count(text->cursor.line) - 1);
			ensure_cursor_on_screen();
		} else {
			text->cursor.pos = 0;
			ensure_cursor_on_screen();
			text->last_set_pos = screen_pos;
		}
	}
}

void edit_window_t::pgdn(void) {
	bool need_adjust = true;

	if (wrap_type == wrap_type_t::NONE) {
		if (text->cursor.line + t3_win_get_height(edit_window) - 1 < text->size()) {
			text->cursor.line += t3_win_get_height(edit_window) - 1;
		} else {
			text->cursor.line = text->size() - 1;
			text->cursor.pos = text->get_line_max(text->cursor.line);
			need_adjust = false;
		}

		/* If the end of the text is already on the screen, don't change the top line. */
		if (top_left.line + t3_win_get_height(edit_window) < text->size()) {
			top_left.line += t3_win_get_height(edit_window) - 1;
			if (top_left.line + t3_win_get_height(edit_window) > text->size())
				top_left.line = text->size() - t3_win_get_height(edit_window);
			redraw = true;
		}

		if (need_adjust)
			text->cursor.pos = text->calculate_line_pos(text->cursor.line, text->last_set_pos, tabsize);

	} else {
		text_coordinate_t new_top_left = top_left;
		text_coordinate_t new_cursor(text->cursor.line, wrap_info->find_line(text->cursor));

		if (wrap_info->add_lines(new_cursor, t3_win_get_height(edit_window) - 1)) {
			text->cursor.line = new_cursor.line;
			text->cursor.pos = text->get_line_max(text->cursor.line);
			need_adjust = false;
		} else {
			text->cursor.line = new_cursor.line;
		}

		/* If the end of the text is already on the screen, don't change the top line. */
		if (!wrap_info->add_lines(new_top_left, t3_win_get_height(edit_window))) {
			top_left = new_top_left;
			wrap_info->sub_lines(top_left, 1);
		}

		if (need_adjust)
			text->cursor.pos = wrap_info->calculate_line_pos(text->cursor.line, text->last_set_pos, new_cursor.pos);
	}
	ensure_cursor_on_screen();

	if (!need_adjust)
		text->last_set_pos = screen_pos;
}

void edit_window_t::pgup(void) {
	bool need_adjust = true;

	if (wrap_type == wrap_type_t::NONE) {
		if (top_left.line < t3_win_get_height(edit_window) - 1) {
			if (top_left.line != 0) {
				redraw = true;
				top_left.line = 0;
			}

			if (text->cursor.line < t3_win_get_height(edit_window) - 1) {
				text->cursor.line = 0;
				text->last_set_pos = text->cursor.pos = 0;
				need_adjust = false;
			} else {
				text->cursor.line -= t3_win_get_height(edit_window) - 1;
			}
		} else {
			text->cursor.line -= t3_win_get_height(edit_window) - 1;
			top_left.line -= t3_win_get_height(edit_window) - 1;
			redraw = true;
		}

		if (need_adjust)
			text->cursor.pos = text->calculate_line_pos(text->cursor.line, text->last_set_pos, tabsize);
	} else {
		text_coordinate_t new_cursor(text->cursor.line, wrap_info->find_line(text->cursor));

		if (wrap_info->sub_lines(new_cursor, t3_win_get_height(edit_window) - 1)) {
			text->cursor.line = 0;
			text->cursor.pos = 0;
			text->last_set_pos = 0;
			need_adjust = false;
		} else {
			text->cursor.line = new_cursor.line;
		}

		wrap_info->sub_lines(top_left, t3_win_get_height(edit_window) - 1);

		if (need_adjust)
			text->cursor.pos = wrap_info->calculate_line_pos(text->cursor.line, text->last_set_pos, new_cursor.pos);
	}
	ensure_cursor_on_screen();
}

void edit_window_t::reset_selection(void) {
	text->set_selection_mode(selection_mode_t::NONE);
	redraw = true;
}

void edit_window_t::set_selection_mode(key_t key) {
	selection_mode_t selection_mode = text->get_selection_mode();
	switch (key & ~(EKEY_CTRL | EKEY_META | EKEY_SHIFT)) {
		case EKEY_END:
		case EKEY_HOME:
		case EKEY_PGUP:
		case EKEY_PGDN:
		case EKEY_LEFT:
		case EKEY_RIGHT:
		case EKEY_UP:
		case EKEY_DOWN:
			if ((selection_mode == selection_mode_t::SHIFT || selection_mode == selection_mode_t::ALL) && !(key & EKEY_SHIFT)) {
				reset_selection();
			} else if ((key & EKEY_SHIFT) && selection_mode != selection_mode_t::MARK) {
				text->set_selection_mode(selection_mode_t::SHIFT);
			}
			break;
		default:
			break;
	}
}

void edit_window_t::delete_selection(void) {
	text_coordinate_t current_start, current_end;

	text->delete_selection();

	current_start = text->get_selection_start();
	current_end = text->get_selection_end();

	if ((current_end.line < current_start.line) || (current_end.line == current_start.line && current_end.pos < current_start.pos))
		text->cursor = current_end;
	else
		text->cursor = current_start;

	redraw = true;
	ensure_cursor_on_screen();
	text->last_set_pos = screen_pos;
	reset_selection();
}

void edit_window_t::find_activated(find_action_t action, finder_t *_finder) {
	finder_t *local_finder;

	local_finder = finder == NULL ? &global_finder : finder;
	if (_finder != NULL)
		*local_finder = *_finder;

	switch (action) {
		case find_action_t::FIND:
			if (!text->find(local_finder))
				goto not_found;

			ensure_cursor_on_screen();
			redraw = true;
			if (local_finder->get_flags() & find_flags_t::REPLACEMENT_VALID) {
				replace_buttons_connection.disconnect();
				#warning FIXME: connection should be removed asap to prevent calls to deleted windows
				replace_buttons_connection = replace_buttons->connect_activate(
					sigc::bind(sigc::mem_fun(this, &edit_window_t::find_activated), (finder_t *) NULL));
				replace_buttons->center_over(center_window);
				replace_buttons->show();
			}
			break;
		case find_action_t::REPLACE:
			text->replace(local_finder);
			redraw = true;
		case find_action_t::SKIP:
			if (!text->find(local_finder)) {
				ensure_cursor_on_screen();
				goto not_found;
			}
			redraw = true;
			ensure_cursor_on_screen();
			replace_buttons->reshow(action);
			break;
		case find_action_t::REPLACE_ALL: {
			int replacements;

			for (replacements = 0; text->find(local_finder); replacements++)
				text->replace(local_finder);

			if (replacements == 0)
				goto not_found;
			redraw = true;
			break;
		}
		case find_action_t::REPLACE_IN_SELECTION:
			//FIXME: do the replacement
			break;
		default:
			break;
	}
	return;

not_found:
	//FIXME: show search string
	message_dialog->set_message("Search string not found");
	message_dialog->center_over(center_window);
	message_dialog->show();
}

//FIXME: make every action into a separate function for readability
bool edit_window_t::process_key(key_t key) {
	set_selection_mode(key);

	switch (key) {
		case EKEY_RIGHT | EKEY_SHIFT:
		case EKEY_RIGHT:
			inc_x();
			break;
		case EKEY_RIGHT | EKEY_CTRL:
		case EKEY_RIGHT | EKEY_CTRL | EKEY_SHIFT:
			next_word();
			break;
		case EKEY_LEFT | EKEY_SHIFT:
		case EKEY_LEFT:
			dec_x();
			break;
		case EKEY_LEFT | EKEY_CTRL | EKEY_SHIFT:
		case EKEY_LEFT | EKEY_CTRL:
			previous_word();
			break;
		case EKEY_DOWN | EKEY_SHIFT:
		case EKEY_DOWN:
			inc_y();
			break;
		case EKEY_UP | EKEY_SHIFT:
		case EKEY_UP:
			dec_y();
			break;
		case EKEY_PGUP | EKEY_SHIFT:
		case EKEY_PGUP:
			pgup();
			break;
		case EKEY_PGDN | EKEY_SHIFT:
		case EKEY_PGDN:
			pgdn();
			break;
		case EKEY_HOME | EKEY_SHIFT:
		case EKEY_HOME:
			screen_pos = text->last_set_pos = 0;
			text->cursor.pos = text->calculate_line_pos(text->cursor.line, 0, tabsize);
			if (wrap_type == wrap_type_t::NONE && top_left.pos != 0)
				ensure_cursor_on_screen();
			break;
		case EKEY_HOME | EKEY_CTRL | EKEY_SHIFT:
		case EKEY_HOME | EKEY_CTRL:
			screen_pos = text->last_set_pos = text->cursor.pos = 0;
			text->cursor.line = 0;
			if ((wrap_type == wrap_type_t::NONE && top_left.pos != 0) || top_left.line != 0)
				ensure_cursor_on_screen();
			break;
		case EKEY_END | EKEY_SHIFT:
		case EKEY_END:
			text->cursor.pos = text->get_line_max(text->cursor.line);
			ensure_cursor_on_screen();
			text->last_set_pos = screen_pos;
			break;
		case EKEY_END | EKEY_CTRL | EKEY_SHIFT:
		case EKEY_END | EKEY_CTRL: {
			text->cursor.line = text->size() - 1;
			text->cursor.pos = text->get_line_max(text->cursor.line);
			ensure_cursor_on_screen();
			text->last_set_pos = screen_pos;
			break;
		}
		case EKEY_INS:
			text->ins_mode ^= 1;
			break;

		/* Below this line all the keys modify the text. */
		case EKEY_DEL:
			if (text->get_selection_mode() == selection_mode_t::NONE) {
				if (text->cursor.pos != text->get_line_max(text->cursor.line)) {
					text->delete_char();

					if (wrap_type != wrap_type_t::NONE)
						ensure_cursor_on_screen();

					redraw = true;
				} else if (text->cursor.line + 1 < text->size()) {
					text->merge(false);

					if (wrap_type != wrap_type_t::NONE)
						ensure_cursor_on_screen();

					redraw = true;
				}
			} else {
				delete_selection();
			}
			break;

		case EKEY_NL:
			if (text->get_selection_mode() != selection_mode_t::NONE)
				delete_selection();

			text->break_line();
			ensure_cursor_on_screen();
			text->last_set_pos = screen_pos;
			redraw = true;
			break;

		case EKEY_BS:
			if (text->get_selection_mode() == selection_mode_t::NONE) {
				if (text->cursor.pos <= text->get_line_max(text->cursor.line)) {
					if (text->cursor.pos != 0) {
						text->backspace_char();
						redraw = true;
					} else if (text->cursor.line != 0) {
						text->merge(true);
						redraw = true;
					}
				} else {
					ASSERT(0);
				}
				ensure_cursor_on_screen();
				text->last_set_pos = screen_pos;
			} else {
				delete_selection();
			}
			break;

		case EKEY_CTRL | 'c':
		case EKEY_INS | EKEY_CTRL:
			cut_copy(false);
			break;
		case EKEY_CTRL | 'x':
		case EKEY_DEL | EKEY_SHIFT:
			cut_copy(true);
			break;
		case EKEY_CTRL | 'v':
		case EKEY_INS | EKEY_SHIFT:
			paste();
			break;

		case EKEY_CTRL | 'y':
			redo();
			break;
		case EKEY_CTRL | 'z':
			undo();
			break;

		case EKEY_CTRL | 'a':
			select_all();
			break;

		case EKEY_CTRL | 'g':
			goto_line();
			break;

		case 0: //CTRL-SPACE (and others)
			switch (text->get_selection_mode()) {
				case selection_mode_t::MARK:
					reset_selection();
					break;
				case selection_mode_t::NONE:
				case selection_mode_t::ALL:
				case selection_mode_t::SHIFT:
					text->set_selection_mode(selection_mode_t::MARK);
					break;
				default:
					/* Should not happen, but just try to get back to a sane state. */
					reset_selection();
					break;
			}
			break;
		case EKEY_ESC:
			if (text->get_selection_mode() == selection_mode_t::MARK)
				reset_selection();
			break;

		case EKEY_CTRL | 'f':
		case EKEY_CTRL | 'r':
			find_replace(key == (EKEY_CTRL | 'r'));
			break;

		case EKEY_F3:
			find_next(false);
			break;
		case EKEY_F3 | EKEY_SHIFT:
			find_next(true);
			break;

		case EKEY_F9:
			insert_special();
			break;

		default:
			if (key < 32 && key != '\t')
				return false;

			key &= ~EKEY_PROTECT;
			if (key == 10)
				return false;

			if (key < 0x110000) {
				int local_insmode = text->ins_mode;
				if (text->get_selection_mode() != selection_mode_t::NONE) {
					delete_selection();
					local_insmode = 0;
				}

				(text->*proces_char[local_insmode])(key);
				ensure_cursor_on_screen();
				redraw = true;
				text->last_set_pos = screen_pos;
			}
			return false;
	}
	return true;
}

void edit_window_t::update_contents(void) {
	text_coordinate_t logical_cursor_pos;
	char info[30];
	int info_width, name_width;
	text_line_t::paint_info_t paint_info;
	selection_mode_t selection_mode;

	if (!focus && !redraw)
		return;

	selection_mode = text->get_selection_mode();
	if (selection_mode != selection_mode_t::NONE && selection_mode != selection_mode_t::ALL) {
		text->set_selection_end();

		if (selection_mode == selection_mode_t::SHIFT) {
			if (text->selection_empty())
				reset_selection();
		}
	}

	//FIXME: don't want to fully repaint on every key when selecting!!
	redraw = false;
	repaint_screen();

	t3_win_set_default_attrs(bottom_line_window, attributes.menubar);
	t3_win_set_paint(bottom_line_window, 0, 0);
	t3_win_addchrep(bottom_line_window, ' ', 0, t3_win_get_width(bottom_line_window));

	if (wrap_type == wrap_type_t::NONE) {
		scrollbar.set_parameters(max(text->size(), top_left.line + t3_win_get_height(edit_window)),
			top_left.line, t3_win_get_height(edit_window));
	} else {
		int i, count = 0;
		for (i = 0; i < top_left.line; i++)
			count += wrap_info->get_line_count(i);
		count += top_left.pos;

		scrollbar.set_parameters(max(wrap_info->get_size(), count + t3_win_get_height(edit_window)),
			count, t3_win_get_height(edit_window));
	}
	scrollbar.update_contents();

	logical_cursor_pos = text->cursor;
	logical_cursor_pos.pos = text->calculate_screen_pos(NULL, tabsize);

	snprintf(info, 29, "L: %-4d C: %-4d %c %s", logical_cursor_pos.line + 1, logical_cursor_pos.pos + 1,
		text->is_modified() ? '*' : ' ', ins_string[text->ins_mode]);
	info_width = t3_term_strwidth(info);
	name_width = t3_win_get_width(bottom_line_window) - info_width - 3;

	if (name_width > 3) {
		const text_line_t *name_line = text->get_name_line();
		/* FIXME: is it really necessary to do this on each key stroke??? */
		t3_win_set_paint(bottom_line_window, 0, 0);
		if (name_line->calculate_screen_width(0, name_line->get_length(), 1) > name_width) {
			t3_win_addstr(bottom_line_window, "..", 0);
			paint_info.start = name_line->adjust_position(name_line->get_length(), -(name_width - 2));
			paint_info.size = name_width - 2;
		} else {
			paint_info.start = 0;
			paint_info.size = name_width;
		}
		paint_info.leftcol = 0;
		paint_info.max = INT_MAX;
		paint_info.tabsize = 1;
		paint_info.flags = text_line_t::TAB_AS_CONTROL | text_line_t::SPACECLEAR;
		paint_info.selection_start = -1;
		paint_info.selection_end = -1;
		paint_info.cursor = -1;
		paint_info.normal_attr = 0;
		paint_info.selected_attr = 0;

		name_line->paint_line(bottom_line_window, &paint_info);
	}

	t3_win_set_paint(bottom_line_window, 0, t3_win_get_width(bottom_line_window) - strlen(info) - 1);
	t3_win_addstr(bottom_line_window, info, 0);
}

void edit_window_t::set_focus(bool _focus) {
	focus = _focus;
	redraw = true; //FXIME: Only for painting/removing cursor
}

int edit_window_t::get_text_width(void) {
	return t3_win_get_width(edit_window);
}

void edit_window_t::undo(void) {
	if (text->apply_undo() == 0) {
		reset_selection();
		redraw = true;
		ensure_cursor_on_screen();
		text->last_set_pos = screen_pos;
	}
}

void edit_window_t::redo(void) {
	if (text->apply_redo() == 0) {
		reset_selection();
		redraw = true;
		ensure_cursor_on_screen();
		text->last_set_pos = screen_pos;
	}
}

void edit_window_t::cut_copy(bool cut) {
	if (text->get_selection_mode() != selection_mode_t::NONE) {
		if (text->selection_empty()) {
			reset_selection();
			return;
		}

		if (copy_buffer != NULL)
			delete copy_buffer;

		copy_buffer = text->convert_selection();

		if (cut)
			delete_selection();
		else if (text->get_selection_mode() == selection_mode_t::MARK)
			text->set_selection_mode(selection_mode_t::SHIFT);
	}
}

void edit_window_t::paste(void) {
	if (copy_buffer != NULL) {
		if (text->get_selection_mode() == selection_mode_t::NONE) {
			text->insert_block(copy_buffer);
		} else {
			text->replace_selection(copy_buffer);
			reset_selection();
		}
		ensure_cursor_on_screen();
		text->last_set_pos = screen_pos;
		redraw = true;
	}
}

void edit_window_t::select_all(void) {
	text->set_selection_mode(selection_mode_t::ALL);
	redraw = true;
}

void edit_window_t::insert_special(void) {
	insert_char_dialog->center_over(center_window);
	insert_char_dialog->reset();
	insert_char_dialog->show();
}

void edit_window_t::goto_line(void) {
	goto_connection.disconnect();
	#warning FIXME: connection should be removed asap to prevent calls to deleted windows
	goto_connection = goto_dialog->connect_activate(sigc::mem_fun1(this, &edit_window_t::goto_line));
	goto_dialog->center_over(center_window);
	goto_dialog->reset();
	goto_dialog->show();
}

void edit_window_t::goto_line(int line) {
	if (line < 1)
		return;

	reset_selection();
	text->cursor.line = (line > text->size() ? text->size() : line) - 1;
	if (text->cursor.pos > text->get_line_max(text->cursor.line))
		text->cursor.pos = text->get_line_max(text->cursor.line);
	ensure_cursor_on_screen();
	text->last_set_pos = screen_pos;
}

void edit_window_t::find_replace(bool replace) {
	find_dialog_t *dialog;
	if (find_dialog == NULL) {
		global_find_dialog_connection.disconnect();
		#warning FIXME: connection should be removed asap to prevent calls to deleted windows
		global_find_dialog_connection = global_find_dialog->connect_activate(
			sigc::mem_fun(this, &edit_window_t::find_activated));
		dialog = global_find_dialog;
	} else {
		dialog = find_dialog;
	}
	dialog->center_over(center_window);
	dialog->set_replace(replace);
	//FIXME: set selected text in dialog
	//dialog->set_text(text->get_selected_text());
	dialog->show();
}

void edit_window_t::find_next(bool backward) {
	if (!text->find(finder != NULL ? finder : &global_finder, backward)) {
		//FIXME: show search string
		message_dialog->set_message("Search string not found");
		message_dialog->center_over(center_window);
		message_dialog->show();
	}
	ensure_cursor_on_screen();
}

text_buffer_t *edit_window_t::get_text(void) {
	return text;
}

void edit_window_t::set_find_dialog(find_dialog_t *_find_dialog) {
	find_dialog = _find_dialog;
}

void edit_window_t::set_finder(finder_t *_finder) {
	finder = _finder;
}

void edit_window_t::force_redraw(void) {
	widget_t::force_redraw();
	ensure_cursor_on_screen();
}

void edit_window_t::set_tabsize(int _tabsize) {
	if (_tabsize == tabsize)
		return;
	tabsize = _tabsize;
	force_redraw();
}

void edit_window_t::bad_draw_recheck(void) {
	widget_t::force_redraw();
}

void edit_window_t::set_wrap(wrap_type_t wrap) {
	if (wrap == wrap_type)
		return;

	if (wrap == wrap_type_t::NONE) {
		if (wrap_info != NULL)
			delete wrap_info;
	} else {
		//FIXME: differentiate between wrap types
		if (wrap_info == NULL)
			wrap_info = new wrap_info_t(t3_win_get_width(edit_window) - 1, tabsize);
		wrap_info->set_text_buffer(text);
	}
	wrap_info->set_wrap_width(t3_win_get_width(edit_window) - 1);
	wrap_type = wrap;
	ensure_cursor_on_screen();
}

//====================== view_parameters_t ========================

edit_window_t::view_parameters_t *edit_window_t::save_view_parameters(void) {
	return new view_parameters_t(this);
}

edit_window_t::view_parameters_t::view_parameters_t(edit_window_t *view) {
	top_left = view->top_left;
	if (wrap_type != wrap_type_t::NONE)
		top_left.pos = view->wrap_info->calculate_line_pos(top_left.line, 0, top_left.pos);
	wrap_type = view->wrap_type;
	tabsize = view->tabsize;
}

edit_window_t::view_parameters_t::view_parameters_t(int _tabsize, wrap_type_t _wrap_type) :
	top_left(0, 0), wrap_type(_wrap_type), tabsize(_tabsize)
{}

void edit_window_t::view_parameters_t::apply_parameters(edit_window_t *view) {
	view->top_left = top_left;
	view->tabsize = tabsize;
	view->set_wrap(wrap_type);
	/* view->set_wrap will make sure that view->wrap_info is NULL if
	   wrap_type != NONE. */
	if (view->wrap_info != NULL) {
		view->wrap_info->set_text_buffer(view->text);
		view->top_left.pos = view->wrap_info->find_line(top_left);
	}
	// the calling function will call ensure_cursor_on_screen
}

}; // namespace
