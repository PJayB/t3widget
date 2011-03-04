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
#ifndef MULTIWIDGET_H
#define MULTIWIDGET_H

#include "widgets/widgets.h"

class multi_widget_t : public widget_t, public container_t {
	private:
		struct item_t {
			widget_t *widget;
			int width;
			bool takes_focus;
			bool send_keys;
		};
		std::list<item_t> widgets;
		int width, fixed_sum, proportion_sum;

	public:
		multi_widget_t(container_t *parent);
		virtual bool process_key(key_t key);
		virtual bool set_size(optint height, optint width);
		virtual void update_contents(void);
		virtual void set_focus(bool focus);
		virtual bool accepts_focus(void);

		/* Width is negative for fixed width widgets, positive for proportion */
		void push_back(widget_t *widget, int _width, bool takes_focus, bool send_keys);
		//FIXME: allow iteration and removal
		void pop_back(void);
		widget_t *back(void);
		bool empty(void);
		void resize_widgets(void);
};

#endif
