/*
#define DEBUG
 */

#include "vipsdisp.h"

/* The Properties widget displays the properties of the VipsImage. This includes
 * geometric values such as width and height as well as any EXIF metadata.
 */

/* This structure defines the properties of a Properties object.
 * (GObject boilerplate)
 */
struct _Properties
{
	GtkWidget parent_instance;

	ImageWindow *image_window;

	GtkWidget *scrolled_window;
	GtkWidget *search_entry;
	GtkWidget *search_warning;

	GtkWidget *main_box;
	GtkWidget *left_column;
	GtkWidget *right_column;
	GList *field_list;
	int field_list_length;
	gboolean ignore_case;

	GdkDisplay *display;
	GtkCssProvider *provider;
};

/* This macro defines the Properties type.
 * (GObject boilerplate)
 */
G_DEFINE_TYPE( Properties, properties, GTK_TYPE_WIDGET );

/* This enum defines the signals used to get and set custom properties.
 * See:
 * 	- properties_get_property
 * 	- properties_set_property
 *
 * (GObject boilerplate).
 */
enum {
	PROP_IMAGE_WINDOW = 1,
	PROP_REVEALED,

	SIG_LAST
};

/* Clean up any existing children of @left_column and @right_column and any
 * existing elements of the GList @field_list.
 *
 * @m	Properties *	this
 */
static void
properties_clear_main_box( Properties *m )
{
	GtkWidget *t0, *t1;

	t0 = gtk_widget_get_last_child( m->left_column );
	while ( t0 ) {
		t1 = gtk_widget_get_prev_sibling( t0 );
		properties_util_free_label_box( t0 );
		t0 = t1;
	}

	t0 = gtk_widget_get_last_child( m->right_column );
	while ( t0 ) {
		t1 = gtk_widget_get_prev_sibling( t0 );
		properties_util_free_input_box( t0 );
		t0 = t1;
	}

	if( m->field_list ) {
		g_list_free( m->field_list );
		m->field_list = NULL;
		m->field_list_length = 0;
	}
}

/* Create a new main_box. The caller should use properties_clear_main_box to clean
 * up existing input widgets first.
 *
 * @m	Properties *	this
 */
static void
properties_create_main_box( Properties *m )
{
	TileSource *tile_source;
	GtkWidget *t;
	VipsImage *image;
	gchar **fields, *field;

	/* If there is no TileSource, clean up any existing main_box and do
	 * nothing.
	 */
	if( !(tile_source = image_window_get_tile_source( m->image_window )) ) {
		properties_clear_main_box( m );
		return;
	}

	/* If there is a TileSource, get its VipsImage @image and continue.
	 */
	image = tile_source_get_image( tile_source );

	/* Clean up the old GtkGrid @m->main_box and GList @m->field_list. If
	 * there is nothing to clean up, nothing happens.
	 */
	properties_clear_main_box( m );

	/* Get the array of image property name character arrays. We are
	 * responsible for freeing this with g_strfreev. The Match methods do
	 * not modify/free the character arrays in @fields, so we only free them
	 * once the relevant data has been copied into widgets with their
	 * character array setter methods.
	 */
	fields = vips_image_get_fields( image );

	/* Make new GList @m->field_list using the fields from the VipsImage
	 * @image.
	 */
	m->field_list = NULL;
	for ( int i=0; (field = fields[i]); i++ )
		m->field_list = g_list_append( m->field_list, field );
	m->field_list_length = g_list_length( m->field_list );

	/* Add @m->field_list_length rows and 2 columns to the main_box. There
	 * is a row for every field, i.e., every property of the VipsImage
	 * @image.
	 */
	for ( int i = 0; (field = g_list_nth_data( m->field_list, i )); i++ ) {
		t = properties_util_create_simple_label_box( field );
		gtk_widget_add_css_class( t, i % 2 ? "odd" : "even" );
		gtk_box_append( GTK_BOX( m->left_column ), t );

		t = properties_util_create_input_box( image, field );
		gtk_widget_add_css_class( t, i % 2 ? "odd" : "even" );
		gtk_box_append( GTK_BOX( m->right_column ), t );
	}

	/* Clean up the array we got from vips_image_get_fields using the
	 * method recommended by the VIPS docs (see vips_image_get_fields).
	 */
	g_strfreev( fields );
}

/* This is called when the TileSource changes. In particular, a new VipsImage
 * might have been loaded, or there might no image loaded. Clean up any
 * existing input widgets and create new ones. This is the callback for the
 * "changed" event on @m->image_window->tile_source.
 *
 * @tile_source		The new tile_source, which is currently held by
 * 			m->image_window.
 *
 * @m	Properties *	this
 */
static void
properties_tile_source_changed( TileSource *tile_source, Properties *m )
{

#ifdef DEBUG
	puts( "properties_tile_source_changed" );
#endif

	/* If there is a new VipsImage on the tile source, use it to create
	 * the new main_box of user input widgets.
	 */
	if( tile_source->image )
		/* The properties_create_main_box function uses the VipsImage to
		 * dynamically create a GtkGrid of user input widgets for
		 * viewing and editing image properties. It cleans up the old
		 * main_box and gets the image from the new TileSource ( which is
		 * why @tile_source isn't passed as an argument here ).
		 */
		properties_create_main_box( m );
}

/* This function is called when ImageWindow changes. It is connected to the
 * "changed" signal on @m->image_window.
 *
 * @image_window	The new ImageWindow *
 * @m	Properties *	this
 */
static void
properties_image_window_changed( ImageWindow *image_window, Properties *m )
{
#ifdef DEBUG
	puts( "properties_image_window_changed" );
#endif

	g_signal_connect_object( image_window_get_tile_source( image_window ),
			"changed", G_CALLBACK( properties_tile_source_changed ),
			m, 0 );
}

/* Change @m->image_window, and watch it for "changed" events. Only used
 * privately by Properties methods.
 *
 * @m	Properties *	this
 * @image_window	The new ImageWindow *
 */
static void
properties_set_image_window( Properties *m, ImageWindow *image_window )
{
#ifdef DEBUG
	puts( "properties_set_image_window" );
#endif

	m->image_window = image_window;

	g_signal_connect_object( image_window, "changed",
		G_CALLBACK( properties_image_window_changed ),
		m, 0 );
}

/* Apply the values currently held by the input widgets to their corresponding
 * properties on the.
 *
 * @m	Properties *	this
 *
 */
void
properties_apply( Properties *m )
{
	VipsImage *image;
	TileSource *tile_source;
	GtkWidget *label_box, *input_box, *t;
	const char *field;

#ifdef DEBUG
	puts("properties_apply");
#endif

	/* Do nothing if there is no TileSource.
	 */
	if( !(tile_source = image_window_get_tile_source( m->image_window )) )
		return;

	/* If there is a TileSource, get its VipsImage @image and continue.
	 */
	image = tile_source_get_image( tile_source );

	/* Walk through the labels in the UI ( the VipsImage property names )
	 * and corresponding input widgets.
	 */
	label_box = gtk_widget_get_first_child( m->left_column );
	input_box = gtk_widget_get_first_child( m->right_column );
	while ( label_box ) {
		g_assert( input_box );

		/* Get the GtkLabel child of the label_box.
		 */
		t = gtk_widget_get_first_child( label_box );

		/* Get the string name of the VipsImage field from the label.
		 */
		field = gtk_label_get_text( GTK_LABEL( t ) );

		/* Get the input widget child of the input_box.
		 */
		t = gtk_widget_get_first_child( input_box );

		/* Apply the value of the corresponding input widget to the
		 * VipsImage.
		 */
		properties_util_apply_input( t, image, field );


		/* Iterate to the next row.
		 */
		label_box = gtk_widget_get_next_sibling( label_box );
		input_box = gtk_widget_get_next_sibling( input_box );
	}
}

/* Attach a new row with a label and an input widget to the main_box
 * for the provided VipsImage field name.
 *
 * This is a GFunc that should only be called by g_list_foreach.
 *
 * @ma_list_:	gpointer (GList*)	List of Match *.
 *
 * @m_:		gpointer (Properties *)	this
 */
static void
properties_append_field( gpointer ma_list_, gpointer m_ )
{
	GList *ma_list;
	Properties *m;
	Match *ma;
	const gchar *field;
	GtkWidget *t;
	TileSource *tile_source;

#ifdef DEBUG
	puts("properties_append_field")
#endif

	ma_list = (GList *) ma_list_;
	m = VIPSDISP_PROPERTIES( m_ );

	ma = (Match *) ma_list->data;

	field = ma->text;

	m->field_list = g_list_append( m->field_list, (gpointer) field );

	/* Create a label_box for the given list node.
	 */
	t = properties_util_create_label_box( ma_list );

	/* Add "even" or "odd" CSS class to label_box based on parity of the row
	 * index.
	 */
	gtk_widget_add_css_class( t, m->field_list_length % 2 ? "odd" : "even" );

	/* Append a label box to @left_column.
	 */
	gtk_box_append( GTK_BOX( m->left_column ), t );

	/* Create an input_box for the given list node.
	 */
	tile_source = image_window_get_tile_source( m->image_window );
	t = properties_util_create_input_box( tile_source->image, field );

	/* Add "even" or "odd" CSS class to input_box based on parity of the row
	 * index.
	 */
	gtk_widget_add_css_class( t, m->field_list_length % 2 ? "odd" : "even" );

	/* Append an input box to @right_column.
	 */
	gtk_box_append( GTK_BOX( m->right_column ), t );

	/* Increment the field list length.
	 */
	m->field_list_length++;
}

/* This is the callback function called whenever the GtkSearchEntry is modified
 * by the user. Adds exact matches - or inexact matches, if there are no exact
 * matches - to the UI.
 *
 * @search_entry	GtkWidget * (GtkSearchEntry *)
 * @m_			gpointer (Properties *)
 */
#define NUM_INEXACT_MATCHES 15
static void
properties_search_changed( GtkWidget *search_entry, gpointer m_ )
{
	Properties *m;
	char **fields, *field;
	const char *patt;
	GList *l, *t;
	guint *v;

	/* Cast @m_ to Properties *
	 */
	m = VIPSDISP_PROPERTIES( m_ );

	/* Clean up any existing children of left_column and right_column.
	 */
	properties_clear_main_box( m );

	/* Get the array of image property name character arrays. We are
	 * responsible for freeing this with g_strfreev. The Match methods do
	 * not modify/free the character arrays in @fields, so we only free them
	 * once the relevant data has been copied into widgets with their
	 * character array setter methods.
	 */
	fields = vips_image_get_fields( image_window_get_tile_source(
		m->image_window )->image );

	/* Make new GList @m->field_list using the fields from the VipsImage
	 * @image.
	 */
	m->field_list = NULL;
	for ( int i=0; (field = fields[i]); i++ )
		m->field_list = g_list_append( m->field_list, field );

	/* We don't free @patt, since the GtkSearchEntry owns it. This is
	 * usually the case for GtkWidget getter methods that return character
	 * pointers.
	 *
	 * On the other hand, GtkWidget setter methods that take character
	 * pointers usually require the caller to free the original character
	 * pointer argument, since setter method usually makes its own copy of
	 * the data. 
	 *
	 * We never set GtkSearchEntry::text - it is set when the user edits the
	 * search entry.
	 */
	patt = gtk_editable_get_text( GTK_EDITABLE( search_entry ) );

	/* Reuse the same buffer for the fuzzy matching algorithm.
	 */
	v = g_malloc( (strlen( patt ) + 1) * sizeof( guint ) );

	/* Get a GList of GList of exact Match objects. There is an inner GList
	 * for each property name in @field_list containing one or more exact
	 * matching substrings. Each inner GList contains an exact Match for
	 * each exactly matching substring within the same property name. 
	 */
	l = match_list( TRUE, m->field_list, patt, m->ignore_case, v );

	/* If there were exact matches, add them to the UI. If there were no
	 * exact matches, get inexact matches, sort them by
	 * LEVENSHTEIN_DISTANCE, and add them to the UI.
	 */
	if( l )
		/* Don't show the search warning, since there are exact matches
		 * to show.
		 */
		gtk_widget_set_visible( m->search_warning, FALSE );
	else {
		/* Tell the user there are no exact matches.
		 */
		gtk_widget_set_visible( m->search_warning, TRUE );

		/* Get a GList of GList of single inexact Match objects. There
		 * is an inner GList for each property name in @field_list. Each
		 * inner GList contains a single inexact match.
		 */
		l = match_list( FALSE, m->field_list, patt, m->ignore_case, v );

		/* Sort by increasing Levenshtein Distance.
		 */
		l = g_list_sort( l, match_list_comp );

		/* Truncate the list at NUM_INEXACT_MATCHES elements. Then,
		 * fully clean up the truncated tail of @l, a GList of
		 * GList of Match.
		 */
		t = g_list_nth( l, NUM_INEXACT_MATCHES );
		if( t && t->prev ) {
			t->prev->next = NULL;
			match_list_free( t );
		}
	}

	/* Add the matches to the UI.
	 */
	g_list_foreach( l, properties_append_field, m );

	/* Clean up the buffer used by the fuzzy matching algorithm.
	 */
	g_free( v );

	/* Fully clean up @l, a GList of GList of Match.
	 */
	match_list_free( l );

	/* Clean up the array we got from vips_image_get_fields using the
	 * method recommended by the VIPS docs (see vips_image_get_fields).
	 */
	g_strfreev( fields );
}

/* This is like the "destructor" method of GObject. It is called to clean up
 * your object near the end of its lifetime.
 *
 * @m_	GObject * (Properties *)	this
 *
 * (GObject boilerplate).
 */
static void
properties_dispose( GObject *m_ )
{
	GtkWidget *t;
	Properties *m;

#ifdef DEBUG
	puts( "properties_dispose" );
#endif

	/* Cast to Properties *.
	 */
	m = VIPSDISP_PROPERTIES( m_ );

	/* Clean up any existing children of @left_column and @right_column and
	 * any existing elements of the GList @field_list.
	 */
	properties_clear_main_box( m );

	/* Clean up the straggling child widget if there is one.
	 */
	if( (t = gtk_widget_get_first_child( GTK_WIDGET( m_ ) )) )
		gtk_widget_unparent( t );

	/* Remove the GtkSyleProvider ( used for CSS ) from the GdkDisplay
	 * @m->display.
	 */
	gtk_style_context_remove_provider_for_display( m->display,
			GTK_STYLE_PROVIDER( m->provider ) );

	/* "Chain up" to the @dispose method of Properties's parent class
	 * ( GtkWidget ). GObject defines the properties_parent_class macro, so
	 * there is no need to reference the class name explicitly.
	 */
	G_OBJECT_CLASS( properties_parent_class )->dispose( m_ );
}

/* This function lets you change the custom properties "image-window" and
 * "revealed".
 *
 * @m_		gpointer (Properties *)	this
 *
 * @prop_id	This is the signal id, e.g. PROP_IMAGE_WINDOW, defined in the
 * 		enum above.
 *
 * @v		The new value.
 *
 * @pspec	The param spec for the property corresponding to @prop_id.
 *
 * (GObject boilerplate)
 */
static void
properties_set_property( GObject *m_, guint prop_id,
		const GValue *v, GParamSpec *pspec )
{
#ifdef DEBUG
	puts( "properties_set_property" );
#endif

	switch( prop_id ) {
	case PROP_IMAGE_WINDOW:
		properties_set_image_window( VIPSDISP_PROPERTIES( m_ ),
			VIPSDISP_IMAGE_WINDOW( g_value_get_object( v ) ) );
		break;
	case PROP_REVEALED:
		if( g_value_get_boolean( v ) )
			gtk_widget_show( GTK_WIDGET( m_ ) );
		else
			gtk_widget_hide( GTK_WIDGET( m_ ) );
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID( m_, prop_id, pspec );
	}
}

/* This function lets you read the custom properties "image-window" and
 * "revealed".
 *
 * @m_		gpointer (Properties *)	this
 *
 * @prop_id	This is the signal id, e.g. PROP_IMAGE_WINDOW, defined in the
 * 		enum above.
 *
 * @v		The current value of the property.
 *
 * @pspec	The param spec for the property corresponding to @prop_id.
 *
 * (GObject boilerplate)
 */
static void
properties_get_property( GObject *m_,
	guint prop_id, GValue *v, GParamSpec *pspec )
{
	Properties *m;

#ifdef DEBUG
	puts("properties_get_property");
#endif

	m = VIPSDISP_PROPERTIES( m_ );

	switch( prop_id ) {
	case PROP_IMAGE_WINDOW:
		g_value_set_object( v, m->image_window );
		break;
	case PROP_REVEALED:
		g_value_set_boolean( v,
				gtk_widget_get_visible( GTK_WIDGET( m_ ) ) );
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID( m_, prop_id, pspec );
	}
}

/* Initialize a Properties object.
 *
 * @m	GObject * (Properties *)	this
 *
 * (GObject boilerplate).
 */
static void
properties_init( Properties *m )
{
#ifdef DEBUG
	puts("properties_init");
#endif

	/* Ignore case during properties field search.
	 */
	m->ignore_case = TRUE;

	m->display = gdk_display_get_default();
	m->provider = gtk_css_provider_new();
	gtk_css_provider_load_from_resource( m->provider,
		      APP_PATH "/properties.css" );
	gtk_style_context_add_provider_for_display( m->display,
			GTK_STYLE_PROVIDER( m->provider ),
			GTK_STYLE_PROVIDER_PRIORITY_APPLICATION );

	gtk_widget_init_template( GTK_WIDGET( m ) );

	/* Connect the handler that gets called when the user modifies the
	 * search query.
	 */
	g_signal_connect( m->search_entry,
		"search-changed",
		G_CALLBACK( properties_search_changed ), m );
}

/* This convenient macro binds pointers on a Properties instance to XML nodes
 * defined in a .ui template. The property name must match the value of the "id"
 * attribute of the node. See "gtk/properties.ui"
 *
 * (GObject boilerplate).
 */
#define BIND( field ) \
	gtk_widget_class_bind_template_child( GTK_WIDGET_CLASS( class ), \
		Properties, field );

/* Initialize PropertiesClass, which GObject defines for us if we used the
 * boilerplate macros and code correctly.
 *
 * @class	PropertiesClass *	this class
 *
 * (GObject boilerplate).
 */
static void
properties_class_init( PropertiesClass *class )
{
	GObjectClass *gobject_class = G_OBJECT_CLASS( class );
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS( class );

#ifdef DEBUG
	printf( "properties_class_init:\n" );
#endif

	gobject_class->dispose = properties_dispose;

	gtk_widget_class_set_layout_manager_type( widget_class,
		GTK_TYPE_BIN_LAYOUT );
	gtk_widget_class_set_template_from_resource( widget_class,
		APP_PATH "/properties.ui");

	BIND( scrolled_window );
	BIND( search_entry );
	BIND( search_warning );
	BIND( main_box );
	BIND( left_column );
	BIND( right_column );

	gobject_class->set_property = properties_set_property;
	gobject_class->get_property = properties_get_property;

	g_object_class_install_property( gobject_class, PROP_IMAGE_WINDOW,
		g_param_spec_object( "image-window",
			_( "Image window" ),
			_( "The image window we display" ),
			IMAGE_WINDOW_TYPE,
			G_PARAM_READWRITE ) );

	g_object_class_install_property( gobject_class, PROP_REVEALED,
		g_param_spec_boolean( "revealed",
			_( "Revealed" ),
			_( "Show properties." ),
			FALSE,
			G_PARAM_READWRITE ) );

}

/* Create a new Properties widget.
 *
 * @image_window	The ImageWindow that will own Properties widget.
 *
 * (GObject boilerplate).
 */
Properties *
properties_new( ImageWindow *image_window )
{
	Properties *m;

#ifdef DEBUG
	printf( "properties_new:\n" );
#endif

	/* Create a new Properties object with its "image-window" property set to
	 * @image_window.
	 */
	m = g_object_new( properties_get_type(),
		"image-window", image_window,
		NULL );

	return m;
}
