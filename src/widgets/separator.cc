/* Copyright (C) 2011-2012,2018 G.P. Halkes
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
#include "widgets/separator.h"
#include "colorscheme.h"

namespace t3_widget {

separator_t::separator_t(bool _horizontal) : widget_t(1, 1, false), horizontal(_horizontal) {}

bool separator_t::set_size(optint height, optint width) {
  bool result = true;
  if (horizontal) {
    if (width.is_valid()) {
      result = window.resize(1, width.value());
    }
  } else {
    if (height.is_valid()) {
      result = window.resize(height.value(), 1);
    }
  }

  return result;
}

bool separator_t::process_key(key_t key) {
  (void)key;
  return false;
}

void separator_t::update_contents() {
  window.set_default_attrs(attributes.dialog);
  if (horizontal) {
    window.set_paint(0, 0);
    window.addchrep(T3_ACS_HLINE, T3_ATTR_ACS, window.get_width());
  } else {
    int i, height = window.get_height();
    for (i = 0; i < height; i++) {
      window.set_paint(i, 0);
      window.addch(T3_ACS_VLINE, T3_ATTR_ACS);
    }
  }
}

void separator_t::set_focus(focus_t focus) { (void)focus; }
bool separator_t::accepts_focus() { return false; }

}  // namespace
