/*
#define DEBUG
 */

#include "vipsdisp.h"

#define NUM_INEXACT_MATCHES (15)

struct _Properties {
	GtkWidget parent_instance;

	TileSource *tile_source;

	GtkWidget *revealer;
	GtkWidget *properties;
	GtkWidget *search_entry;
	GtkWidget *scrolled_window;

	// build a grid of labels here
	GtkWidget *grid;

	// null, or the string we are matching against
	char *pattern;

	int row_number;
};

G_DEFINE_TYPE(Properties, properties, GTK_TYPE_WIDGET);

enum {
	PROP_TILE_SOURCE = 1,
	PROP_REVEALED,

	SIG_LAST
};

#ifdef DEBUG
static const char *
properties_property_name(guint prop_id)
{
	switch (prop_id) {
	case PROP_TILE_SOURCE:
		return "TILE_SOURCE";
		break;

	case PROP_REVEALED:
		return "REVEALED";
		break;

	default:
		return "<unknown>";
	}
}
#endif /*DEBUG*/

/* Clean up any existing children of @left_column and @right_column and any
 * existing elements of the GList @field_list.
 *
 * @m	Properties *	this
 */
static void
properties_clear(Properties *p)
{
	p->grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(p->grid), 3);
	gtk_grid_set_column_spacing(GTK_GRID(p->grid), 10);
	// will unparent and destroy any old child
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(p->scrolled_window),
		p->grid);
	p->row_number = 0;
}

static void
properties_add_row(Properties *p,
	const char *label, GtkWidget *item)
{
	GtkWidget *t;

	t = gtk_label_new(label);
	// can't set alignment in CSS for some reason
	gtk_widget_set_halign(t, GTK_ALIGN_START);
	gtk_label_set_selectable(GTK_LABEL(t), TRUE);
	if (item)
		gtk_widget_add_css_class(t, "properties-label");
	gtk_widget_add_css_class(t, p->row_number % 2 ? "odd" : "even");
	gtk_grid_attach(GTK_GRID(p->grid), t, 0, p->row_number, 1, 1);

	if (item) {
		gtk_widget_add_css_class(t, "properties-value");
		gtk_widget_add_css_class(item, p->row_number % 2 ? "odd" : "even");
		gtk_grid_attach(GTK_GRID(p->grid), item, 1, p->row_number, 1, 1);
	}

	p->row_number++;
}

static void
properties_add_item(Properties *p, const char *field, GValue *value)
{
	char str[256];
	VipsBuf buf = VIPS_BUF_STATIC(str);

	vips_buf_appendgv(&buf, value);
	GtkWidget *label = gtk_label_new(vips_buf_all(&buf));
	// can't set alignment in CSS for some reason
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	properties_add_row(p, field, label);
}

static void *
properties_refresh_add_item_cb(VipsImage *image,
	const char *field, GValue *value, void *client)
{
	Properties *p = PROPERTIES(client);

	properties_add_item(p, field, value);

	return NULL;
}

static void *
properties_refresh_add_item_match_cb(VipsImage *image,
	const char *field, GValue *value, void *client)
{
	Properties *p = PROPERTIES(client);

	if (g_strrstr(field, p->pattern))
		properties_add_item(p, field, value);

	return NULL;
}

static void
properties_refresh(Properties *p)
{
	VipsImage *image;

#ifdef DEBUG
	printf("properties_refresh:\n");
#endif

	properties_clear(p);

	if (p->tile_source &&
		(image = tile_source_get_image(p->tile_source))) {
		if (p->pattern) {
			vips_image_map(image, properties_refresh_add_item_match_cb, p);

			properties_add_row(p, "", NULL);
			properties_add_row(p, "Did you mean ...", NULL);

			char **fields = vips_image_get_fields(image);
			GSList *matches = fuzzy_match(fields, p->pattern);
			int n_displayed;

			n_displayed = 0;
			for (GSList *i = matches; i; i = i->next) {
				Fuzzy *fuzzy = (Fuzzy *) i->data;

				// don't show fields we have already displayed in the
				// main search area
				if (!g_strrstr(fuzzy->field, p->pattern)) {
					GValue value = { 0 };

					vips_image_get(image, fuzzy->field, &value);
					properties_add_item(p, fuzzy->field, &value);
					g_value_unset(&value);

					if (n_displayed++ > NUM_INEXACT_MATCHES)
						break;
				}
			}

			g_slist_free_full(g_steal_pointer(&matches), g_free);
			VIPS_FREEF(g_strfreev, fields);
		}
		else
			vips_image_map(image, properties_refresh_add_item_cb, p);
	}
}

/* This is called when the TileSource changes. In particular, a new VipsImage
 * might have been loaded, or there might no image loaded. Clean up any
 * existing input widgets and create new ones. This is the callback for the
 * "changed" event on @p->image_window->tile_source.
 *
 * @tile_source		The new tile_source, which is currently held by
 * 			p->image_window.
 *
 * @m	Properties *	this
 */
static void
properties_tile_source_changed(TileSource *tile_source, Properties *p)
{
#ifdef DEBUG
	printf("properties_tile_source_changed:\n");
#endif

	properties_refresh(p);
}

static void
properties_set_tile_source(Properties *p, TileSource *tile_source)
{
#ifdef DEBUG
	printf("properties_set_tile_source:\n");
#endif

	/* No need to ref ... the enclosing window holds a ref to us.
	 */
	p->tile_source = tile_source;

	g_signal_connect_object(tile_source,
		"changed", G_CALLBACK(properties_tile_source_changed),
		p, 0);
}

static void
properties_search_changed(GtkWidget *search_entry, gpointer user_data)
{
	Properties *p = PROPERTIES(user_data);
	const char *patt = gtk_editable_get_text(GTK_EDITABLE(search_entry));

	VIPS_FREE(p->pattern);
	if (patt &&
		g_ascii_strcasecmp(patt, "") != 0 &&
		strspn(patt, " \t\n") != strlen(patt)) {
		p->pattern = g_strdup(patt);
	}

#ifdef DEBUG
	printf("properties_search_changed: pattern = %s\n", p->pattern);
#endif

	properties_refresh(p);
}

static void
properties_dispose(GObject *p_)
{
	Properties *p = PROPERTIES(p_);

#ifdef DEBUG
	printf("properties_dispose:\n");
#endif

	VIPS_FREEF(gtk_widget_unparent, p->revealer);
	VIPS_FREE(p->pattern);

	G_OBJECT_CLASS(properties_parent_class)->dispose(p_);
}

static void
properties_set_property(GObject *object, guint prop_id,
	const GValue *value, GParamSpec *pspec)
{
	Properties *p = PROPERTIES(object);

#ifdef DEBUG
	{
		char *str;

		str = g_strdup_value_contents(value);
		printf("properties_set_property: %s %s\n",
			properties_property_name(prop_id), str);
		g_free(str);
	}
#endif /*DEBUG*/

	switch (prop_id) {
	case PROP_TILE_SOURCE:
		properties_set_tile_source(p,
			TILE_SOURCE(g_value_get_object(value)));
		break;

	case PROP_REVEALED: {
		gboolean revealed = g_value_get_boolean(value);
		gboolean current_revealed =
			gtk_revealer_get_reveal_child(GTK_REVEALER(p->revealer));

		if (current_revealed != revealed) {
			gtk_revealer_set_reveal_child(GTK_REVEALER(p->revealer),
				revealed);
			g_object_notify_by_pspec(object, pspec);
		}
	} break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(p, prop_id, pspec);
	}
}

static void
properties_get_property(GObject *p_,
	guint prop_id, GValue *value, GParamSpec *pspec)
{
	Properties *p = PROPERTIES(p_);

	switch (prop_id) {
	case PROP_TILE_SOURCE:
		g_value_set_object(value, p->tile_source);
		break;

	case PROP_REVEALED:
		g_value_set_boolean(value,
			gtk_revealer_get_reveal_child(GTK_REVEALER(p->revealer)));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(p_, prop_id, pspec);
	}

#ifdef DEBUG
	{
		char *str;

		str = g_strdup_value_contents(value);
		printf("properties_get_property: %s %s\n",
			properties_property_name(prop_id), str);
		g_free(str);
	}
#endif /*DEBUG*/
}

static void
properties_init(Properties *p)
{
#ifdef DEBUG
	printf("properties_init:\n");
#endif

	gtk_widget_init_template(GTK_WIDGET(p));

	g_signal_connect(p->search_entry,
		"search-changed",
		G_CALLBACK(properties_search_changed), p);
}

#define BIND(field) \
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), \
		Properties, field);

/* Initialize PropertiesClass, which GObject defines for us if we used the
 * boilerplate macros and code correctly.
 *
 * @class	PropertiesClass *	this class
 *
 * (GObject boilerplate).
 */
static void
properties_class_init(PropertiesClass *class)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(class);

#ifdef DEBUG
	printf("properties_class_init:\n");
#endif

	gobject_class->dispose = properties_dispose;

	gtk_widget_class_set_layout_manager_type(widget_class,
		GTK_TYPE_BIN_LAYOUT);
	gtk_widget_class_set_template_from_resource(widget_class,
		APP_PATH "/properties.ui");

	BIND(revealer);
	BIND(properties);
	BIND(search_entry);
	BIND(scrolled_window);

	gobject_class->set_property = properties_set_property;
	gobject_class->get_property = properties_get_property;

	g_object_class_install_property(gobject_class, PROP_TILE_SOURCE,
		g_param_spec_object("tile-source",
			_("Tile source"),
			_("The tile source whose properties we display"),
			TILE_SOURCE_TYPE,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_REVEALED,
		g_param_spec_boolean("revealed", NULL, NULL,
			TRUE,
			G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));
}
