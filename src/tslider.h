/* a slider with an editable value
 */

/*

	Copyright (C) 1991-2003 The National Gallery

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License along
	with this program; if not, write to the Free Software Foundation, Inc.,
	51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

 */

/*

	These files are distributed with VIPS - http://www.vips.ecs.soton.ac.uk

*/

#ifndef __TSLIDER_H
#define __TSLIDER_H

#define TSLIDER_TYPE (tslider_get_type())
#define TSLIDER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), TSLIDER_TYPE, Tslider))
#define TSLIDER_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), TSLIDER_TYPE, TsliderClass))
#define IS_TSLIDER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), TSLIDER_TYPE))
#define IS_TSLIDER_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), TSLIDER_TYPE))
#define TSLIDER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), TSLIDER_TYPE, TsliderClass))

typedef double (*TsliderFn)(double from, double to, double value);

typedef struct _Tslider {
	GtkWidget parent_instance;

	/* Our state.
	 */
	double from;
	double to;

	double value;  /* Real value, as displayed in text */
	double svalue; /* Slider value ... secret linear scale */
	int digits;	   /* How many sf to display */

	/* Keep last from/to/value settings here. Can't
	 * use from/to since double and float don't compare reliably.
	 */
	double last_from, last_to, last_svalue;

	GtkAdjustment *adj;
	GtkWidget *box;
	GtkWidget *entry;
	GtkWidget *scale;

	/* Optional functions ... how to make a value from a slider
	 * position, how to make a slider position from a value.
	 * If these are defined, text and slider are linked for you.
	 */
	gboolean auto_link;
	TsliderFn value_to_slider;
	TsliderFn slider_to_value;

} Tslider;

typedef struct _TsliderClass {
	GtkWidgetClass parent_class;

	void (*changed)(struct _Tslider *tslider);

	void *padding[12];
} TsliderClass;

GType tslider_get_type();
void tslider_changed(Tslider *);
void tslider_set_conversions(Tslider *tslider,
	TsliderFn value_to_slider, TsliderFn slider_to_value);
GtkWidget *tslider_new(double from, double to, int digits);
double tslider_log_value_to_slider(double from, double to, double value);
double tslider_log_slider_to_value(double from, double to, double value);

#endif /* __TSLIDER_H */
