#include "vipsdisp.h"

/*
#define DEBUG_VERBOSE
#define DEBUG
 */

/* How much to scale view by each frame.
 */
#define SCALE_STEP (1.05)

/* Duration of discrete zoom in secs.
 */
#define SCALE_DURATION (0.5)

struct _Imageui {
	GtkWidget parent_instance;

	TileSource *tile_source;

	/* Last known mouse position, in gtk coordinates. We keep these in gtk
	 * cods so we don't need to update them on pan / zoom.
	 */
	double last_x_gtk;
	double last_y_gtk;

	/* For pinch zoom, zoom position that we started.
	 */
	double last_scale;
	double scale_cx;
	double scale_cy;

	/* For animating zoom.
	 */
	guint tick_handler;
	double scale_rate;
	double scale_target;
	double scale_start;
	double scale_progress;

	/* During zoom, keep this pixel (in image coordinates) at the same
	 * position on the screen.
	 */
	double scale_x;
	double scale_y;

	/* Mouse panning.
	 */
	double drag_start_x;
	double drag_start_y;

	/* TRUE for an eased zoom (eg. magin), FALSE for a continuous zoom (eg.
	 * 'i').
	 */
	gboolean eased;

	GtkWidget *scrolled_window;
	GtkWidget *imagedisplay;

	gint64 last_frame_time;

	gboolean should_animate;

};

G_DEFINE_TYPE(Imageui, imageui, GTK_TYPE_WIDGET);

enum {
	PROP_TILE_SOURCE = 1,
	PROP_BACKGROUND,

	PROP_LAST
};

/* Our signals.
 */
enum {
	SIG_CHANGED,		/* New mouse position, magnification, etc. */

	SIG_LAST
};

static guint imageui_signals[SIG_LAST] = { 0 };

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

static void
imageui_set_tile_source(Imageui *imageui, TileSource *tile_source)
{
	// not a ref ... the real one is held by imagedisplay
	imageui->tile_source = tile_source;

	g_object_set(imageui->imagedisplay,
		"tile-source", tile_source,
		NULL);
}

#ifdef DEBUG_VERBOSE
static const char *
imageui_property_name(guint prop_id)
{
	switch (prop_id) {
	case PROP_TILE_SOURCE:
		return "TILE_SOURCE";
		break;

	case PROP_BACKGROUND:
		return "BACKGROUND";
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

#ifdef DEBUG_VERBOSE
	{
		g_autofree char *str = g_strdup_value_contents(value);
		printf("imageui_set_property: %s %s\n",
			imageui_property_name(prop_id), str);
	}
#endif /*DEBUG_VERBOSE*/

	switch (prop_id) {
	case PROP_TILE_SOURCE:
		imageui_set_tile_source(imageui,
			TILE_SOURCE(g_value_get_object(value)));
		break;

	case PROP_BACKGROUND:
		g_object_set_property(G_OBJECT(imageui->imagedisplay), 
			"background", value);
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

	switch (prop_id) {
	case PROP_TILE_SOURCE:
		g_value_set_object(value, imageui->tile_source);
		break;

	case PROP_BACKGROUND:
		g_object_get_property(G_OBJECT(imageui->imagedisplay), 
			"background", value);
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

static void
imageui_set_scale(Imageui *imageui, double scale)
{
#ifdef DEBUG_VERBOSE
	printf("imageui_set_scale: %g\n", scale);
#endif /*DEBUG_VERBOSE*/

	/* Scale by the zoom factor (SVG etc. scale) we picked on load.
	 */
	scale /= imageui->tile_source->zoom;

	g_object_set(imageui->imagedisplay,
		"scale", scale,
		NULL);
}

double
imageui_get_scale(Imageui *imageui)
{
	double scale;

	g_object_get(imageui->imagedisplay,
		"scale", &scale,
		NULL);

	/* Scale by the zoom factor (SVG etc. scale) we picked on load.
	 */
	scale *= imageui->tile_source->zoom;

#ifdef DEBUG_VERBOSE
	printf("imageui_get_scale: %g\n", scale);
#endif /*DEBUG_VERBOSE*/

	return scale;
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
	printf("image_window_set_position: x = %g, y = %g\n", x, y);
#endif /*DEBUG_VERBOSE*/

	gtk_adjustment_set_value(hadj, x);
	gtk_adjustment_set_value(vadj, y);
}

/* Set a new mag, keeping the pixel at x/y in the image at the same position
 * on the screen.
 */
static void
imageui_set_scale_position(Imageui *imageui,
	double scale, double x_image, double y_image)
{
	double old_x, old_y;
	double new_x, new_y;
	int left, top, width, height;

#ifdef DEBUG_VERBOSE
	printf("imageui_set_scale_position: %g %g %g\n", scale, x_image, y_image);
#endif /*DEBUG_VERBOSE*/

	/* Map the image pixel at (x, y) to gtk space, ie. mouse coordinates.
	 */
	imagedisplay_image_to_gtk(IMAGEDISPLAY(imageui->imagedisplay),
		x_image, y_image, &old_x, &old_y);

	imageui_set_scale(imageui, scale);

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

static gboolean
imageui_tick(GtkWidget *widget, GdkFrameClock *frame_clock, gpointer user_data)
{
	Imageui *imageui = IMAGEUI(user_data);

	gint64 frame_time = gdk_frame_clock_get_frame_time(frame_clock);
	double dt = imageui->last_frame_time > 0 ? 
		(double) (frame_time - imageui->last_frame_time) / G_TIME_SPAN_SECOND : 
		1.0 / G_TIME_SPAN_SECOND;
	double scale = imageui_get_scale(imageui);

	double new_scale;

#ifdef DEBUG_VERBOSE
	printf("image_window_tick: dt = %g\n", dt);
#endif /*DEBUG_VERBOSE*/

	new_scale = scale;

	if (imageui->eased) {
		// 0/1/etc. discrete zoom
		imageui->scale_progress += dt;

		double duration = imageui->should_animate ? 
			SCALE_DURATION : imageui->scale_progress;

		// 0-1 progress in zoom animation
		double t = ease_out_cubic(imageui->scale_progress / duration);

		// so current scale must be
		new_scale = imageui->scale_start +
			t * (imageui->scale_target - imageui->scale_start);

		if (t >= 1.0) {
			new_scale = imageui->scale_target;
			imageui->scale_target = 0;
			imageui_stop_animation(imageui);
		}
	}
	else {
		// i/o/etc. continuous zoom
		new_scale = (dt * (imageui->scale_rate - 1.0) + 1.0) * scale;

		if (imageui->scale_rate == 1.0)
			imageui_stop_animation(imageui);
	}

	imageui_set_scale_position(imageui, 
		new_scale, imageui->scale_x, imageui->scale_y);

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
imageui_scale_to_eased(Imageui *imageui, double scale_target)
{
	int widget_width = gtk_widget_get_width(imageui->scrolled_window);
	int widget_height = gtk_widget_get_height(imageui->scrolled_window);

	imageui->eased = TRUE;
	imageui->scale_target = scale_target;
	imageui->scale_start = imageui_get_scale(imageui);
	imageui->scale_progress = 0.0;
	imagedisplay_gtk_to_image(IMAGEDISPLAY(imageui->imagedisplay),
		widget_width / 2, widget_height / 2, 
		&imageui->scale_x, &imageui->scale_y);

	imageui_start_animation(imageui);
}

static void
imageui_scale_continuous(Imageui *imageui,
	double scale_rate, double scale_x, double scale_y)
{
	imageui->eased = FALSE;
	imageui->scale_rate = scale_rate;
	imageui->scale_x = scale_x;
	imageui->scale_y = scale_y;

	imageui_start_animation(imageui);
}

void
imageui_bestfit(Imageui *imageui)
{
	// size by whole image area, including the props pane
	int widget_width = gtk_widget_get_width(imageui->scrolled_window);
	int widget_height = gtk_widget_get_height(imageui->scrolled_window);

	double hscale = (double) widget_width / 
		imageui->tile_source->display_width;
	double vscale = (double) widget_height / 
		imageui->tile_source->display_height;
	double scale = VIPS_MIN(hscale, vscale);

	imageui_scale_to_eased(imageui, scale * imageui->tile_source->zoom);
}

void
imageui_magin(Imageui *imageui)
{
	imageui_scale_to_eased(imageui, 2 * imageui_get_scale(imageui));
}

void
imageui_magout(Imageui *imageui)
{
	imageui_scale_to_eased(imageui, 0.5 * imageui_get_scale(imageui));
}

void
imageui_oneone(Imageui *imageui)
{
	imageui_scale_to_eased(imageui, 1.0);
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

	/* FIXME ... this should only look at visible tile_cache pixels ...
	 * don't render any new pixels.
	 *
	 * Alternatively, run this in a BG thread.
	 */
	if (vips_extract_area(image, &t1, left, top, width, height, NULL) ||
		vips_stats(t1, &t2, NULL))
		return -1;

	min = *VIPS_MATRIX(t1, 0, 0);
	max = *VIPS_MATRIX(t1, 1, 0);
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

	if ((image = tile_source_get_image(imageui->tile_source))) {
		double image_scale;
		int left, top, width, height;
		double scale, offset;

		image_scale = imageui_get_scale(imageui);
		imageui_get_position(imageui, &left, &top, &width, &height);
		left /= image_scale;
		top /= image_scale;
		width /= image_scale;
		height /= image_scale;

		/* FIXME ... this will be incredibly slow, esp. for large
		 * images. Instead, it would be better to just search the
		 * cached tiles we have.
		 */
		if (imageui_find_scale(image,
				left, top, width, height, &scale, &offset)) 
			return FALSE;

		g_object_set(imageui->tile_source,
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
	double scale;
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
	double scale_x;
	double scale_y;
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
		imageui_get_mouse_position(imageui, &scale_x, &scale_y);
		imageui_scale_continuous(imageui, 1.5 * SCALE_STEP, scale_x, scale_y);
		handled = TRUE;
		break;

	case GDK_KEY_o:
		imageui_get_mouse_position(imageui, &scale_x, &scale_y);
		imageui_scale_continuous(imageui, 0.2 * SCALE_STEP, scale_x, scale_y);
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
				double scale;

				scale = magnify_keys[i].scale;
				if (state & GDK_CONTROL_MASK)
					scale = 1.0 / scale;

				imageui_scale_to_eased(imageui, scale);

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
		imageui->scale_rate = 1.0;
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

	int window_left;
	int window_top;
	int window_width;
	int window_height;

#ifdef DEBUG_VERBOSE
	printf("imageui_drag_begin: start_x = %g, start_y = %g\n",
		start_x, start_y);
#endif /*DEBUG_VERBOSE*/

	imageui_get_position(imageui,
		&window_left, &window_top, &window_width, &window_height);

	imageui->drag_start_x = window_left;
	imageui->drag_start_y = window_top;
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
		imageui->drag_start_x - offset_x, imageui->drag_start_y - offset_y);
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
		imageui_set_scale_position(imageui,
			SCALE_STEP * imageui_get_scale(imageui), x_image, y_image);
	else
		imageui_set_scale_position(imageui,
			(1.0 / SCALE_STEP) * imageui_get_scale(imageui), x_image, y_image);

	return TRUE;
}

static void
imageui_init(Imageui *imageui)
{
#ifdef DEBUG
	printf("imageui_init:\n");
#endif /*DEBUG*/

	gtk_widget_init_template(GTK_WIDGET(imageui));

	imageui->scale_rate = 1.0;

	/* Uncomment to test animation disable
	g_object_set( gtk_widget_get_settings( GTK_WIDGET( win ) ),
		"gtk-enable-animations", FALSE, NULL );
	 */

	// read the gtk animation setting preference
	imageui->should_animate = widget_should_animate(GTK_WIDGET(imageui));
}

#define BIND(field) \
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), \
		Imageui, field);

static void
imageui_class_init(ImageuiClass *class)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(class);

#ifdef DEBUG
	printf("imageui_class_init:\n");
#endif /*DEBUG*/

	G_OBJECT_CLASS(class)->dispose = imageui_dispose;

	gtk_widget_class_set_layout_manager_type(widget_class,
		GTK_TYPE_BIN_LAYOUT);
	gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class),
		APP_PATH "/imageui.ui");

	BIND(scrolled_window);
	BIND(imagedisplay);

	gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class),
		imageui_drag_begin);
	gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class),
		imageui_drag_update);

	gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class),
		imageui_key_pressed);
	gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class),
		imageui_key_released);

	gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class),
		imageui_motion);

	gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class),
		imageui_scroll);

	gobject_class->set_property = imageui_set_property;
	gobject_class->get_property = imageui_get_property;

	g_object_class_install_property(gobject_class, PROP_TILE_SOURCE,
		g_param_spec_object("tile-source",
			_("Tile source"),
			_("The tile source we display"),
			TILE_SOURCE_TYPE, 
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_BACKGROUND,
		g_param_spec_int("background",
			_("Background"),
			_("Background mode"),
			0, TILE_CACHE_BACKGROUND_LAST - 1,
			TILE_CACHE_BACKGROUND_CHECKERBOARD,
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
imageui_new(TileSource *tile_source)
{
	Imageui *imageui;

#ifdef DEBUG
	printf("imageui_new:\n");
#endif /*DEBUG*/

	imageui = g_object_new(IMAGEUI_TYPE,
		"tile-source", tile_source,
		NULL);

	return imageui;
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
imageui_copy_position(Imageui *new, Imageui *old)
{
	copy_adj(
		gtk_scrolled_window_get_hadjustment(
			GTK_SCROLLED_WINDOW(new->scrolled_window)),
		gtk_scrolled_window_get_hadjustment(
			GTK_SCROLLED_WINDOW(old->scrolled_window)));
	copy_adj(
		gtk_scrolled_window_get_vadjustment(
			GTK_SCROLLED_WINDOW(new->scrolled_window)),
		gtk_scrolled_window_get_vadjustment(
			GTK_SCROLLED_WINDOW(old->scrolled_window)));
}
