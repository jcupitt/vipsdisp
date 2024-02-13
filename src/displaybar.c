#include "vipsdisp.h"

/*
#define DEBUG_VERBOSE
#define DEBUG
 */

struct _Displaybar {
	GtkWidget parent_instance;

	/* The image_window we attach to.
	 */
	ImageWindow *win;

	GtkWidget *action_bar;
	GtkWidget *gears;
	GtkWidget *page;
	GtkWidget *scale;
	GtkWidget *offset;
};

G_DEFINE_TYPE(Displaybar, displaybar, GTK_TYPE_WIDGET);

enum {
	PROP_IMAGE_WINDOW = 1,
	PROP_REVEALED,

	SIG_LAST
};

static void
displaybar_tile_source_changed(TileSource *tile_source,
	Displaybar *displaybar)
{
#ifdef DEBUG
	printf("displaybar_tile_source_changed:\n");
#endif /*DEBUG*/

	if (TSLIDER(displaybar->scale)->value != tile_source->scale) {
		TSLIDER(displaybar->scale)->value = tile_source->scale;
		tslider_changed(TSLIDER(displaybar->scale));
	}

	if (TSLIDER(displaybar->offset)->value != tile_source->offset) {
		TSLIDER(displaybar->offset)->value = tile_source->offset;
		tslider_changed(TSLIDER(displaybar->offset));
	}

	gtk_spin_button_set_range(
		GTK_SPIN_BUTTON(displaybar->page),
		0, tile_source->n_pages - 1);
	gtk_widget_set_sensitive(displaybar->page,
		tile_source->n_pages > 1 &&
			tile_source->mode == TILE_SOURCE_MODE_MULTIPAGE);
}

static void
displaybar_page_changed(TileSource *tile_source,
	Displaybar *displaybar)
{
#ifdef DEBUG
	printf("displaybar_page_changed:\n");
#endif /*DEBUG*/

	gtk_spin_button_set_value(
		GTK_SPIN_BUTTON(displaybar->page),
		tile_source->page);
}

/* Imagewindow has a new tile_source.
 */
static void
displaybar_image_window_changed(ImageWindow *win,
	Displaybar *displaybar)
{
	TileSource *tile_source;

	if ((tile_source = image_window_get_tile_source(win))) {
		g_signal_connect_object(tile_source, "changed",
			G_CALLBACK(displaybar_tile_source_changed),
			displaybar, 0);
		g_signal_connect_object(tile_source, "tiles-changed",
			G_CALLBACK(displaybar_tile_source_changed),
			displaybar, 0);
		g_signal_connect_object(tile_source, "page-changed",
			G_CALLBACK(displaybar_page_changed),
			displaybar, 0);
	}
}

static void
displaybar_set_image_window(Displaybar *displaybar,
	ImageWindow *win)
{
	/* No need to ref ... win holds a ref to us.
	 */
	displaybar->win = win;

	g_signal_connect_object(win, "changed",
		G_CALLBACK(displaybar_image_window_changed),
		displaybar, 0);
}

static void
displaybar_set_property(GObject *object,
	guint prop_id, const GValue *value, GParamSpec *pspec)
{
	Displaybar *displaybar = (Displaybar *) object;

	switch (prop_id) {
	case PROP_IMAGE_WINDOW:
		displaybar_set_image_window(displaybar,
			g_value_get_object(value));
		break;

	case PROP_REVEALED:
		gtk_action_bar_set_revealed(
			GTK_ACTION_BAR(displaybar->action_bar),
			g_value_get_boolean(value));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
displaybar_get_property(GObject *object,
	guint prop_id, GValue *value, GParamSpec *pspec)
{
	Displaybar *displaybar = (Displaybar *) object;
	GtkActionBar *action_bar = GTK_ACTION_BAR(displaybar->action_bar);

	switch (prop_id) {
	case PROP_IMAGE_WINDOW:
		g_value_set_object(value, displaybar->win);
		break;

	case PROP_REVEALED:
		g_value_set_boolean(value, gtk_action_bar_get_revealed(action_bar));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
displaybar_dispose(GObject *object)
{
	Displaybar *displaybar = (Displaybar *) object;

#ifdef DEBUG
	printf("displaybar_dispose:\n");
#endif /*DEBUG*/

	VIPS_FREEF(gtk_widget_unparent, displaybar->action_bar);

	G_OBJECT_CLASS(displaybar_parent_class)->dispose(object);
}

static void
displaybar_page_value_changed(GtkSpinButton *spin_button,
	Displaybar *displaybar)
{
	TileSource *tile_source = image_window_get_tile_source(displaybar->win);
	int new_page = gtk_spin_button_get_value_as_int(spin_button);

#ifdef DEBUG
	printf("displaybar_page_value_changed: %d\n", new_page);
#endif /*DEBUG*/

	if (tile_source)
		g_object_set(tile_source,
			"page", new_page,
			NULL);
}

static void
displaybar_scale_value_changed(Tslider *slider,
	Displaybar *displaybar)
{
	TileSource *tile_source =
		image_window_get_tile_source(displaybar->win);

	if (tile_source)
		g_object_set(tile_source,
			"scale", slider->value,
			NULL);
}

static void
displaybar_offset_value_changed(Tslider *slider,
	Displaybar *displaybar)
{
	TileSource *tile_source =
		image_window_get_tile_source(displaybar->win);

	if (tile_source)
		g_object_set(tile_source,
			"offset", slider->value,
			NULL);
}

static void
displaybar_init(Displaybar *displaybar)
{
	Tslider *tslider;

#ifdef DEBUG
	printf("displaybar_init:\n");
#endif /*DEBUG*/

	gtk_widget_init_template(GTK_WIDGET(displaybar));

	set_tooltip(GTK_WIDGET(displaybar->page), _("Page select"));

	tslider = TSLIDER(displaybar->scale);
	tslider_set_conversions(tslider,
		tslider_log_value_to_slider, tslider_log_slider_to_value);
	tslider->from = 0.001;
	tslider->to = 255;
	tslider->value = 1.0;
	tslider->svalue = 128;
	tslider->digits = 3;
	tslider_changed(tslider);
	set_tooltip(GTK_WIDGET(tslider), _("Brightness scale factor"));

	tslider = TSLIDER(displaybar->offset);
	tslider->from = -128;
	tslider->to = 128;
	tslider->value = 0;
	tslider->svalue = 0;
	tslider->digits = 1;
	tslider_changed(tslider);
	set_tooltip(GTK_WIDGET(tslider), _("Brightness offset"));

	g_signal_connect(displaybar->page, "value-changed",
		G_CALLBACK(displaybar_page_value_changed),
		displaybar);
	g_signal_connect(displaybar->scale, "changed",
		G_CALLBACK(displaybar_scale_value_changed),
		displaybar);
	g_signal_connect(displaybar->offset, "changed",
		G_CALLBACK(displaybar_offset_value_changed),
		displaybar);
}

#define BIND(field) \
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), \
		Displaybar, field);

static void
displaybar_class_init(DisplaybarClass *class)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(class);

#ifdef DEBUG
	printf("displaybar_class_init:\n");
#endif /*DEBUG*/

	G_OBJECT_CLASS(class)->dispose = displaybar_dispose;

	gtk_widget_class_set_layout_manager_type(widget_class,
		GTK_TYPE_BIN_LAYOUT);
	gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class),
		APP_PATH "/displaybar.ui");

	BIND(action_bar);
	BIND(gears);
	BIND(page);
	BIND(scale);
	BIND(offset);

	gobject_class->set_property = displaybar_set_property;
	gobject_class->get_property = displaybar_get_property;

	g_object_class_install_property(gobject_class, PROP_IMAGE_WINDOW,
		g_param_spec_object("image-window",
			_("Image window"),
			_("The image window we display"),
			IMAGE_WINDOW_TYPE,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_REVEALED,
		g_param_spec_boolean("revealed",
			_("revealed"),
			_("Show the display control bar"),
			FALSE,
			G_PARAM_READWRITE));
}

Displaybar *
displaybar_new(ImageWindow *win)
{
	Displaybar *displaybar;

#ifdef DEBUG
	printf("displaybar_new:\n");
#endif /*DEBUG*/

	displaybar = g_object_new(displaybar_get_type(),
		"image-window", win,
		NULL);

	return displaybar;
}
