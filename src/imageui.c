/* a scrolled window containing an imagedisplay
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

/*
#define DEBUG
 */

#include "vipsdisp.h"

/*
#define DEBUG_VERBOSE
#define DEBUG
 */

/* How much to zoom view by each frame.
 */
#define ZOOM_STEP (1.05)

/* Duration of discrete zoom in secs.
 */
#define ZOOM_DURATION (0.5)

struct _Imageui {
	GtkWidget parent_instance;

	Tilesource *tilesource;

	/* The zoom factor that was used at load time, handy for scaling SVGs.
	 */
	double zoom_load;

	/* Last known mouse position, in gtk coordinates. We keep these in gtk
	 * cods so we don't need to update them on pan / zoom.
	 */
	double last_x_gtk;
	double last_y_gtk;

	/* For pinch zoom, zoom position that we started.
	 */
	double last_zoom;
	double zoom_cx;
	double zoom_cy;

	/* For animating zoom.
	 */
	guint tick_handler;
	double zoom_rate;
	double zoom_target;
	double zoom_start;
	double zoom_progress;

	/* During zoom, keep this pixel (in image coordinates) at the same
	 * position on the screen.
	 */
	double zoom_x;
	double zoom_y;

	/* TRUE for an eased zoom (eg. magin), FALSE for a continuous zoom (eg.
	 * 'i').
	 */
	gboolean eased;

	/* Drag scroll.
	 */
	int window_left;	 	/* Window position at start of scroll */
	int window_top;
	int start_x; 			/* Mouse position at start of scroll */
	int start_y;

	GtkWidget *scrolled_window;
	GtkWidget *imagedisplay;

	gint64 last_frame_time;

	gboolean should_animate;

};

G_DEFINE_TYPE(Imageui, imageui, GTK_TYPE_WIDGET);

enum {
	PROP_TILESOURCE = 1,
	PROP_BACKGROUND,
	PROP_ZOOM,
	PROP_X,
	PROP_Y,

	PROP_LAST
};

/* Our signals.
 */
enum {
	SIG_CHANGED, /* New mouse position, magnification, etc. */

	SIG_LAST
};

static guint imageui_signals[SIG_LAST] = { 0 };

void
imageui_queue_draw(Imageui *imageui)
{
	gtk_widget_queue_draw(GTK_WIDGET(imageui->imagedisplay));
}

static void
imageui_dispose(GObject *object)
{
	Imageui *imageui = (Imageui *) object;

#ifdef DEBUG
	printf("imageui_dispose:\n");
#endif /*DEBUG*/

	VIPS_FREEF(gtk_widget_unparent, imageui->scrolled_window);

	G_OBJECT_CLASS(imageui_parent_class)->dispose(object);
}

static void
imageui_changed(Imageui *imageui)
{
#ifdef DEBUG_VERBOSE
	printf("imageui_changed:\n");
#endif /*DEBUG_VERBOSE*/

	g_signal_emit(imageui, imageui_signals[SIG_CHANGED], 0);
}

#ifdef DEBUG_VERBOSE
static const char *
imageui_property_name(guint prop_id)
{
	switch (prop_id) {
	case PROP_TILESOURCE:
		return "TILESOURCE";
		break;

	case PROP_BACKGROUND:
		return "BACKGROUND";
		break;

	case PROP_ZOOM:
		return "ZOOM";
		break;

	case PROP_X:
		return "X";
		break;

	case PROP_Y:
		return "Y";
		break;

	default:
		return "<unknown>";
	}
}
#endif /*DEBUG_VERBOSE*/

static void
imageui_set_property(GObject *object,
	guint prop_id, const GValue *value, GParamSpec *pspec)
{
	Imageui *imageui = (Imageui *) object;

	double zoom;

#ifdef DEBUG_VERBOSE
	{
		g_autofree char *str = g_strdup_value_contents(value);
		printf("imageui_set_property: %s %s\n",
			imageui_property_name(prop_id), str);
	}
#endif /*DEBUG_VERBOSE*/

	switch (prop_id) {
	case PROP_TILESOURCE:
		g_object_set_property(G_OBJECT(imageui->imagedisplay),
			"tilesource", value);

		// not a ref ... the real one is held by imagedisplay
		imageui->tilesource = TILESOURCE(g_value_get_object(value));
		if (imageui->tilesource)
			imageui->zoom_load = imageui->tilesource->zoom;
		break;

	case PROP_BACKGROUND:
		g_object_set_property(G_OBJECT(imageui->imagedisplay),
			"background", value);
		break;

	case PROP_ZOOM:
		/* Scale by the zoom factor (SVG etc. zoom) we picked on load.
		 */
		zoom = g_value_get_double(value);
		zoom /= imageui->tilesource->zoom;

		g_object_set(imageui->imagedisplay,
			"zoom", zoom,
			NULL);
		break;

	case PROP_X:
		g_object_set_property(G_OBJECT(imageui->imagedisplay),
			"x", value);
		break;

	case PROP_Y:
		g_object_set_property(G_OBJECT(imageui->imagedisplay),
			"y", value);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
imageui_get_property(GObject *object,
	guint prop_id, GValue *value, GParamSpec *pspec)
{
	Imageui *imageui = IMAGEUI(object);

	double zoom;

	switch (prop_id) {
	case PROP_TILESOURCE:
		g_value_set_object(value, imageui->tilesource);
		break;

	case PROP_BACKGROUND:
		g_object_get_property(G_OBJECT(imageui->imagedisplay),
			"background", value);
		break;

	case PROP_ZOOM:
		g_object_get(imageui->imagedisplay,
			"zoom", &zoom,
			NULL);

		/* Scale by the zoom factor (SVG etc. zoom) we picked on load.
		 */
		zoom *= imageui->zoom_load;
		g_value_set_double(value, zoom);
		break;

	case PROP_X:
		g_object_get_property(G_OBJECT(imageui->imagedisplay), "x", value);
		break;

	case PROP_Y:
		g_object_get_property(G_OBJECT(imageui->imagedisplay), "y", value);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}

#ifdef DEBUG_VERBOSE
	{
		g_autofree char *str = g_strdup_value_contents(value);
		printf("imageui_get_property: %s %s\n",
			imageui_property_name(prop_id), str);
	}
#endif /*DEBUG_VERBOSE*/
}

Tilesource *
imageui_get_tilesource(Imageui *imageui)
{
	return imageui->tilesource;
}

static void
imageui_get_position(Imageui *imageui,
	int *left, int *top, int *width, int *height)
{
	GtkAdjustment *hadj = gtk_scrolled_window_get_hadjustment(
		GTK_SCROLLED_WINDOW(imageui->scrolled_window));
	GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(
		GTK_SCROLLED_WINDOW(imageui->scrolled_window));

	*left = gtk_adjustment_get_value(hadj);
	*top = gtk_adjustment_get_value(vadj);
	*width = gtk_adjustment_get_page_size(hadj);
	*height = gtk_adjustment_get_page_size(vadj);

#ifdef DEBUG_VERBOSE
	printf("imageui_get_position: %d %d %d %d\n", *left, *top, *width, *height);
#endif /*DEBUG_VERBOSE*/
}

static void
imageui_set_position(Imageui *imageui, double x, double y)
{
	GtkAdjustment *hadj = gtk_scrolled_window_get_hadjustment(
		GTK_SCROLLED_WINDOW(imageui->scrolled_window));
	GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(
		GTK_SCROLLED_WINDOW(imageui->scrolled_window));

#ifdef DEBUG_VERBOSE
	printf("imagewindow_set_position: x = %g, y = %g\n", x, y);
#endif /*DEBUG_VERBOSE*/

	gtk_adjustment_set_value(hadj, x);
	gtk_adjustment_set_value(vadj, y);
}

static void
imageui_set_zoom(Imageui *imageui, double zoom)
{
	g_object_set(imageui,
		"zoom", zoom,
		NULL);
}

/* Set a new mag, keeping the pixel at x/y in the image at the same position
 * on the screen.
 */
static void
imageui_set_zoom_position(Imageui *imageui,
	double zoom, double x_image, double y_image)
{
	double old_x, old_y;
	double new_x, new_y;
	int left, top, width, height;

#ifdef DEBUG_VERBOSE
	printf("imageui_set_zoom_position: %g %g %g\n", zoom, x_image, y_image);
#endif /*DEBUG_VERBOSE*/

	/* Map the image pixel at (x, y) to gtk space, ie. mouse coordinates.
	 */
	imagedisplay_image_to_gtk(IMAGEDISPLAY(imageui->imagedisplay),
		x_image, y_image, &old_x, &old_y);

	imageui_set_zoom(imageui, zoom);

	/* Map image (x, y) to display coordinates with our new magnification,
	 * then to keep the point in the same position we must translate by
	 * the difference.
	 */
	imagedisplay_image_to_gtk(IMAGEDISPLAY(imageui->imagedisplay),
		x_image, y_image, &new_x, &new_y);

	/* Add 0.5 since we (in effect) cast to int here and we want round to
	 * nearest.
	 */
	imageui_get_position(imageui, &left, &top, &width, &height);
	imageui_set_position(imageui,
		left + new_x - old_x + 0.5, top + new_y - old_y + 0.5);
}

/* From clutter-easing.c, based on Robert Penner's infamous easing equations,
 * MIT license.
 */
static double
ease_out_cubic(double t)
{
	double p = t - 1;

	return p * p * p + 1;
}

static void
imageui_stop_animation(Imageui *imageui)
{
	if (imageui->tick_handler) {
		gtk_widget_remove_tick_callback(GTK_WIDGET(imageui),
			imageui->tick_handler);
		imageui->tick_handler = 0;
	}
}

double
imageui_get_zoom(Imageui *imageui)
{
	double zoom;

	g_object_get(imageui,
		"zoom", &zoom,
		NULL);

	return zoom;
}

static gboolean
imageui_tick(GtkWidget *widget, GdkFrameClock *frame_clock, gpointer user_data)
{
	Imageui *imageui = IMAGEUI(user_data);

	gint64 frame_time = gdk_frame_clock_get_frame_time(frame_clock);
	double dt = imageui->last_frame_time > 0 ? (double) (frame_time - imageui->last_frame_time) / G_TIME_SPAN_SECOND : 1.0 / G_TIME_SPAN_SECOND;
	double zoom = imageui_get_zoom(imageui);

	double new_zoom;

#ifdef DEBUG_VERBOSE
	// FIXME could display FPS from this?
	printf("imageui_tick: dt = %g\n", dt);
#endif /*DEBUG_VERBOSE*/

	new_zoom = zoom;

	if (imageui->eased) {
		// 0/1/etc. discrete zoom
		imageui->zoom_progress += dt;

		double duration = imageui->should_animate ? ZOOM_DURATION : imageui->zoom_progress;

		// 0-1 progress in zoom animation
		double t = ease_out_cubic(imageui->zoom_progress / duration);

		// so current zoom must be
		new_zoom = imageui->zoom_start +
			t * (imageui->zoom_target - imageui->zoom_start);

		if (t >= 1.0) {
			new_zoom = imageui->zoom_target;
			imageui->zoom_target = 0;
			imageui_stop_animation(imageui);
		}
	}
	else {
		// i/o/etc. continuous zoom
		new_zoom = (dt * (imageui->zoom_rate - 1.0) + 1.0) * zoom;

		if (imageui->zoom_rate == 1.0)
			imageui_stop_animation(imageui);
	}

	imageui_set_zoom_position(imageui,
		new_zoom, imageui->zoom_x, imageui->zoom_y);

	imageui->last_frame_time = frame_time;

	return G_SOURCE_CONTINUE;
}

static void
imageui_start_animation(Imageui *imageui)
{
	if (!imageui->tick_handler) {
		imageui->last_frame_time = -1;
		imageui->tick_handler =
			gtk_widget_add_tick_callback(GTK_WIDGET(imageui),
				imageui_tick, imageui, NULL);
	}
}

static void
imageui_zoom_to_eased(Imageui *imageui, double zoom_target)
{
	int widget_width = gtk_widget_get_width(imageui->scrolled_window);
	int widget_height = gtk_widget_get_height(imageui->scrolled_window);

	imageui->eased = TRUE;
	imageui->zoom_target = zoom_target;
	imageui->zoom_start = imageui_get_zoom(imageui);
	imageui->zoom_progress = 0.0;
	imagedisplay_gtk_to_image(IMAGEDISPLAY(imageui->imagedisplay),
		widget_width / 2, widget_height / 2,
		&imageui->zoom_x, &imageui->zoom_y);

	imageui_start_animation(imageui);
}

static void
imageui_zoom_continuous(Imageui *imageui,
	double zoom_rate, double zoom_x, double zoom_y)
{
	imageui->eased = FALSE;
	imageui->zoom_rate = zoom_rate;
	imageui->zoom_x = zoom_x;
	imageui->zoom_y = zoom_y;

	imageui_start_animation(imageui);
}

void
imageui_bestfit(Imageui *imageui)
{
	// size by whole image area, including the props pane
	int widget_width = gtk_widget_get_width(imageui->scrolled_window);
	int widget_height = gtk_widget_get_height(imageui->scrolled_window);

	double hzoom = (double) widget_width /
		imageui->tilesource->display_width;
	double vzoom = (double) widget_height /
		imageui->tilesource->display_height;
	double zoom = VIPS_MIN(hzoom, vzoom);

	imageui_zoom_to_eased(imageui, zoom * imageui->tilesource->zoom);
}

void
imageui_magin(Imageui *imageui)
{
	imageui_zoom_to_eased(imageui, 2 * imageui_get_zoom(imageui));
}

void
imageui_magout(Imageui *imageui)
{
	imageui_zoom_to_eased(imageui, 0.5 * imageui_get_zoom(imageui));
}

void
imageui_oneone(Imageui *imageui)
{
	imageui_zoom_to_eased(imageui, 1.0);
}

static void
imageui_toggle_debug(Imageui *imageui)
{
	gboolean debug;

	g_object_get(imageui->imagedisplay,
		"debug", &debug,
		NULL);

	g_object_set(imageui->imagedisplay,
		"debug", !debug,
		NULL);
}

static int
imageui_find_scale(VipsImage *image,
	int left, int top, int width, int height, double *scale, double *offset)
{
	double min, max;

	g_autoptr(VipsImage) t1 = NULL;
	g_autoptr(VipsImage) t2 = NULL;

	/* FIXME ... this should only look at visible tilecache pixels ...
	 * don't render any new pixels.
	 *
	 * Alternatively, run this in a BG thread.
	 */
	if (vips_extract_area(image, &t1, left, top, width, height, NULL) ||
		vips_stats(t1, &t2, NULL))
		return -1;

	min = *VIPS_MATRIX(t2, 0, 0);
	max = *VIPS_MATRIX(t2, 1, 0);
	if (max == min) {
		vips_error("Find scale", _("Min and max are equal"));
		return -1;
	}

	*scale = 255.0 / (max - min);
	*offset = -(min * *scale) + 0.5;

	return 0;
}

gboolean
imageui_scale(Imageui *imageui)
{
	VipsImage *image;

	if ((image = tilesource_get_image(imageui->tilesource))) {
		double image_zoom;
		int left, top, width, height;
		double scale, offset;

		image_zoom = imageui_get_zoom(imageui);
		imageui_get_position(imageui, &left, &top, &width, &height);
		left /= image_zoom;
		top /= image_zoom;
		width /= image_zoom;
		height /= image_zoom;

		/* FIXME ... this will be incredibly slow, esp. for large
		 * images. Instead, it would be better to just search the
		 * cached tiles we have.
		 */
		if (imageui_find_scale(image,
				left, top, width, height, &scale, &offset))
			return FALSE;

		g_object_set(imageui->tilesource,
			"scale", scale,
			"offset", offset,
			NULL);
	}

	return TRUE;
}

void
imageui_get_mouse_position(Imageui *imageui,
	double *x_image, double *y_image)
{
	imagedisplay_gtk_to_image(IMAGEDISPLAY(imageui->imagedisplay),
		imageui->last_x_gtk, imageui->last_y_gtk, x_image, y_image);
}

static struct {
	int keyval;
	double zoom;
} magnify_keys[] = {
	{ GDK_KEY_1, 1.0 },
	{ GDK_KEY_2, 2.0 },
	{ GDK_KEY_3, 3.0 },
	{ GDK_KEY_4, 4.0 },
	{ GDK_KEY_5, 5.0 },
	{ GDK_KEY_6, 6.0 },
	{ GDK_KEY_7, 7.0 },
	{ GDK_KEY_8, 8.0 },
	{ GDK_KEY_9, 9.0 }
};

static gboolean
imageui_key_pressed(GtkEventControllerKey *self,
	guint keyval, guint keycode, GdkModifierType state, gpointer user_data)
{
	Imageui *imageui = IMAGEUI(user_data);
	GtkScrolledWindow *scrolled_window =
		GTK_SCROLLED_WINDOW(imageui->scrolled_window);

	gboolean handled;
	double zoom_x;
	double zoom_y;
	gboolean ret;

#ifdef DEBUG_VERBOSE
	printf("imageui_key_pressed: keyval = %d, state = %d\n",
		keyval, state);
#endif /*DEBUG_VERBOSE*/

	handled = FALSE;

	switch (keyval) {
	case GDK_KEY_plus:
		imageui_magin(imageui);
		handled = TRUE;
		break;

	case GDK_KEY_minus:
		imageui_magout(imageui);
		handled = TRUE;
		break;

	case GDK_KEY_0:
		imageui_bestfit(imageui);
		handled = TRUE;
		break;

	case GDK_KEY_i:
		imageui_get_mouse_position(imageui, &zoom_x, &zoom_y);
		imageui_zoom_continuous(imageui, 1.5 * ZOOM_STEP, zoom_x, zoom_y);
		handled = TRUE;
		break;

	case GDK_KEY_o:
		imageui_get_mouse_position(imageui, &zoom_x, &zoom_y);
		imageui_zoom_continuous(imageui, 0.2 * ZOOM_STEP, zoom_x, zoom_y);
		handled = TRUE;
		break;

	case GDK_KEY_Left:
		if (state & GDK_SHIFT_MASK)
			g_signal_emit_by_name(scrolled_window, "scroll-child",
				GTK_SCROLL_PAGE_BACKWARD, TRUE, &ret);
		else if (state & GDK_CONTROL_MASK)
			g_signal_emit_by_name(scrolled_window, "scroll-child",
				GTK_SCROLL_START, TRUE, &ret);
		else
			g_signal_emit_by_name(scrolled_window, "scroll-child",
				GTK_SCROLL_STEP_LEFT, TRUE, &ret);
		handled = TRUE;
		break;

	case GDK_KEY_Right:
		if (state & GDK_SHIFT_MASK)
			g_signal_emit_by_name(scrolled_window, "scroll-child",
				GTK_SCROLL_PAGE_FORWARD, TRUE, &ret);
		else if (state & GDK_CONTROL_MASK)
			g_signal_emit_by_name(scrolled_window, "scroll-child",
				GTK_SCROLL_END, TRUE, &ret);
		else
			g_signal_emit_by_name(scrolled_window, "scroll-child",
				GTK_SCROLL_STEP_RIGHT, TRUE, &ret);
		handled = TRUE;
		break;

	case GDK_KEY_Up:
		if (state & GDK_SHIFT_MASK)
			g_signal_emit_by_name(scrolled_window, "scroll-child",
				GTK_SCROLL_PAGE_UP, FALSE, &ret);
		else if (state & GDK_CONTROL_MASK)
			g_signal_emit_by_name(scrolled_window, "scroll-child",
				GTK_SCROLL_START, FALSE, &ret);
		else
			g_signal_emit_by_name(scrolled_window, "scroll-child",
				GTK_SCROLL_STEP_UP, FALSE, &ret);
		handled = TRUE;
		break;

	case GDK_KEY_Down:
		if (state & GDK_SHIFT_MASK)
			g_signal_emit_by_name(scrolled_window, "scroll-child",
				GTK_SCROLL_PAGE_DOWN, FALSE, &ret);
		else if (state & GDK_CONTROL_MASK)
			g_signal_emit_by_name(scrolled_window, "scroll-child",
				GTK_SCROLL_END, FALSE, &ret);
		else
			g_signal_emit_by_name(scrolled_window, "scroll-child",
				GTK_SCROLL_STEP_DOWN, FALSE, &ret);
		handled = TRUE;
		break;

	case GDK_KEY_d:
		imageui_toggle_debug(imageui);
		handled = TRUE;
		break;

	default:
		break;
	}

	if (!handled) {
		int i;

		for (i = 0; i < VIPS_NUMBER(magnify_keys); i++)
			if (magnify_keys[i].keyval == keyval) {
				double zoom;

				zoom = magnify_keys[i].zoom;
				if (state & GDK_CONTROL_MASK)
					zoom = 1.0 / zoom;

				imageui_zoom_to_eased(imageui, zoom);

				handled = TRUE;
				break;
			}
	}

	return handled;
}

static gboolean
imageui_key_released(GtkEventControllerKey *self,
	guint keyval, guint keycode, GdkModifierType state, gpointer user_data)
{
	Imageui *imageui = IMAGEUI(user_data);

	gboolean handled;

	handled = FALSE;

	switch (keyval) {
	case GDK_KEY_i:
	case GDK_KEY_o:
		imageui->zoom_rate = 1.0;
		handled = TRUE;
		break;

	default:
		break;
	}

	if (handled)
		imageui_stop_animation(imageui);

	return handled;
}

static void
imageui_drag_begin(GtkEventControllerMotion *self,
	gdouble start_x, gdouble start_y, gpointer user_data)
{
	Imageui *imageui = IMAGEUI(user_data);

#ifdef DEBUG_VERBOSE
	printf("imageui_drag_begin: start_x = %g, start_y = %g\n",
		start_x, start_y);
#endif /*DEBUG_VERBOSE*/

	int window_left;
	int window_top;
	int window_width;
	int window_height;
	imageui_get_position(imageui,
		&window_left, &window_top, &window_width, &window_height);
	imageui->window_left = window_left;
	imageui->window_top = window_top;
	imageui->start_x = start_x;
	imageui->start_y = start_y;
}

static void
imageui_drag_update(GtkEventControllerMotion *self,
	gdouble offset_x, gdouble offset_y, gpointer user_data)
{
	Imageui *imageui = IMAGEUI(user_data);

#ifdef DEBUG_VERBOSE
	printf("imageui_drag_update: offset_x = %g, offset_y = %g\n",
		offset_x, offset_y);
#endif /*DEBUG_VERBOSE*/

	imageui_set_position(imageui,
		imageui->window_left - offset_x, imageui->window_top - offset_y);
}

static void
imageui_motion(GtkEventControllerMotion *self,
	gdouble x, gdouble y, gpointer user_data)
{
	Imageui *imageui = IMAGEUI(user_data);

#ifdef DEBUG_VERBOSE
	printf("imageui_motion: x = %g, y = %g\n", x, y);
#endif /*DEBUG_VERBOSE*/

	imageui->last_x_gtk = x;
	imageui->last_y_gtk = y;

	imageui_changed(imageui);
}

static gboolean
imageui_scroll(GtkEventControllerMotion *self,
	double dx, double dy, gpointer user_data)
{
	Imageui *imageui = IMAGEUI(user_data);

	double x_image;
	double y_image;

#ifdef DEBUG_VERBOSE
	printf("imageui_scroll: dx = %g, dy = %g\n", dx, dy);
#endif /*DEBUG_VERBOSE*/

	imageui_get_mouse_position(imageui, &x_image, &y_image);

	if (dy < 0)
		imageui_set_zoom_position(imageui,
			ZOOM_STEP * imageui_get_zoom(imageui), x_image, y_image);
	else
		imageui_set_zoom_position(imageui,
			(1.0 / ZOOM_STEP) * imageui_get_zoom(imageui), x_image, y_image);

	return TRUE;
}

static void
imageui_init(Imageui *imageui)
{
#ifdef DEBUG
	printf("imageui_init:\n");
#endif /*DEBUG*/

	gtk_widget_init_template(GTK_WIDGET(imageui));

	imageui->zoom_rate = 1.0;

	/* Uncomment to test our animation disable
	g_object_set(gtk_widget_get_settings(GTK_WIDGET(win)),
		"gtk-enable-animations", FALSE, NULL);
	 */

	// read the gtk animation setting preference
	imageui->should_animate = widget_should_animate(GTK_WIDGET(imageui));
}

static void
imageui_class_init(ImageuiClass *class)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(class);

#ifdef DEBUG
	printf("imageui_class_init:\n");
#endif /*DEBUG*/

	BIND_RESOURCE("imageui.ui");
	BIND_LAYOUT();

	BIND_VARIABLE(Imageui, scrolled_window);
	BIND_VARIABLE(Imageui, imagedisplay);

	BIND_CALLBACK(imageui_drag_begin);
	BIND_CALLBACK(imageui_drag_update);
	BIND_CALLBACK(imageui_key_pressed);
	BIND_CALLBACK(imageui_key_released);
	BIND_CALLBACK(imageui_motion);
	BIND_CALLBACK(imageui_scroll);

	gobject_class->dispose = imageui_dispose;
	gobject_class->set_property = imageui_set_property;
	gobject_class->get_property = imageui_get_property;

	g_object_class_install_property(gobject_class, PROP_TILESOURCE,
		g_param_spec_object("tilesource",
			_("Tile source"),
			_("The tile source we display"),
			TILESOURCE_TYPE,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_BACKGROUND,
		g_param_spec_int("background",
			_("Background"),
			_("Background mode"),
			0, TILECACHE_BACKGROUND_LAST - 1,
			TILECACHE_BACKGROUND_CHECKERBOARD,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_ZOOM,
		g_param_spec_double("zoom",
			_("Zoom"),
			_("Zoom of viewport"),
			-VIPS_MAX_COORD, VIPS_MAX_COORD, 0,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_X,
		g_param_spec_double("x",
			_("x"),
			_("Horizontal position of viewport"),
			-VIPS_MAX_COORD, VIPS_MAX_COORD, 0,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_Y,
		g_param_spec_double("y",
			_("y"),
			_("Vertical position of viewport"),
			-VIPS_MAX_COORD, VIPS_MAX_COORD, 0,
			G_PARAM_READWRITE));

	imageui_signals[SIG_CHANGED] = g_signal_new("changed",
		G_TYPE_FROM_CLASS(class),
		G_SIGNAL_RUN_LAST,
		0,
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

Imageui *
imageui_new(Tilesource *tilesource)
{
	Imageui *imageui;

#ifdef DEBUG
	printf("imageui_new:\n");
#endif /*DEBUG*/

	imageui = g_object_new(IMAGEUI_TYPE,
		"tilesource", tilesource,
		NULL);

	return imageui;
}

Imageui *
imageui_duplicate(Tilesource *tilesource, Imageui *old_imageui)
{
	Imageui *new_imageui = imageui_new(tilesource);

	/* We want to copy position and zoom, so no bestfit.
	 */
	g_object_set(new_imageui->imagedisplay,
		"bestfit", FALSE,
		NULL);

	double zoom = imageui_get_zoom(old_imageui);
	imageui_set_zoom(new_imageui, zoom);

	// this won't work until imagedisplay has had a layout :( think about this
	// again
	int left, top, width, height;
	imageui_get_position(old_imageui, &left, &top, &width, &height);
	imageui_set_position(new_imageui, left, top);

	return new_imageui;
}

void
imageui_image_to_gtk(Imageui *imageui,
	double x_image, double y_image, double *x_gtk, double *y_gtk)
{
	imagedisplay_image_to_gtk(IMAGEDISPLAY(imageui->imagedisplay),
		x_image, y_image, x_gtk, y_gtk);
}

void
imageui_gtk_to_image(Imageui *imageui,
	double x_gtk, double y_gtk, double *x_image, double *y_image)
{
	imagedisplay_gtk_to_image(IMAGEDISPLAY(imageui->imagedisplay),
		x_gtk, y_gtk, x_image, y_image);
}

void
imageui_image_to_gtk_rect(Imageui *imageui, VipsRect *in, VipsRect *out)
{
	VipsRect rect;
	double x_gtk;
	double y_gtk;

	imageui_image_to_gtk(imageui, in->left, in->top, &x_gtk, &y_gtk);
	rect.left = x_gtk;
	rect.top = y_gtk;

	imageui_image_to_gtk(imageui,
		VIPS_RECT_RIGHT(in), VIPS_RECT_BOTTOM(in), &x_gtk, &y_gtk);
	rect.width = ceil(x_gtk) - rect.left;
	rect.height = ceil(y_gtk) - rect.top;

	*out = rect;
}

void
imageui_gtk_to_image_rect(Imageui *imageui, VipsRect *in, VipsRect *out)
{
	VipsRect rect;
	double x_image;
	double y_image;

	imageui_gtk_to_image(imageui, in->left, in->top, &x_image, &y_image);
	rect.left = x_image;
	rect.top = y_image;

	imageui_gtk_to_image(imageui,
		VIPS_RECT_RIGHT(in), VIPS_RECT_BOTTOM(in), &x_image, &y_image);
	rect.width = ceil(x_image) - rect.left;
	rect.height = ceil(y_image) - rect.top;

	*out = rect;
}
