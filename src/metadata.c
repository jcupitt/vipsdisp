/*
#define DEBUG
 */

#include "vipsdisp.h"

/* The Metadata widget displays the properties of the VipsImage. This includes
 * geometric values such as width and height as well as any exif metadata.
 */

/* This structure defines the properties of a Metadata object.
 * (GObject boilerplate)
 */
struct _Metadata
{
	GtkWidget parent_instance;

	ImageWindow *image_window;

	GtkWidget *scrolled_window;
	GtkWidget *search_bar;
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

/* This macro defines the Metadata type.
 * (GObject boilerplate)
 */
G_DEFINE_TYPE( Metadata, metadata, GTK_TYPE_WIDGET );

/* This enum defines the signals used to get and set custom properties.
 * See:
 * 	- metadata_get_property
 * 	- metadata_set_property
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
 * @m	Metadata *	this
 */
static void
metadata_clear_main_box( Metadata *m )
{
	GtkWidget *t0, *t1;

	/* Clean up the old main_box contents ( if any ) and old field list
	 * ( if of non-zero length ).
	 */
	if ( m->field_list ) {
		t0 = gtk_widget_get_last_child( m->left_column );
		while ( t0 ) {
			t1 = gtk_widget_get_prev_sibling( t0 );
			gtk_widget_unparent( t0 );
			t0 = t1;
		}

		t0 = gtk_widget_get_last_child( m->right_column );
		while ( t0 ) {
			t1 = gtk_widget_get_prev_sibling( t0 );
			gtk_widget_unparent( t0 );
			t0 = t1;
		}

		g_list_free( m->field_list );
		m->field_list = NULL;
		m->field_list_length = 0;
	}
}

/* Create a new main_box. The caller should use metadata_clear_main_box to clean
 * up existing input widgets first.
 *
 * @m	Metadata *	this
 */
static void
metadata_create_main_box( Metadata *m )
{
	TileSource *tile_source;
	GtkWidget *t;
	VipsImage *image;
	char **fields, *field;

	/* If there is no TileSource, clean up any existing main_box and do
	 * nothing.
	 */
	if ( !(tile_source = image_window_get_tile_source( m->image_window )) ) {
		metadata_clear_main_box( m );
		return;
	}

	/* If there is a TileSource, get its VipsImage @image and continue.
	 */
	image = tile_source_get_image( tile_source );

	/* Clean up the old GtkGrid @m->main_box and GList @m->field_list. If
	 * there is nothing to clean up, nothing happens.
	 */
	metadata_clear_main_box( m );

	/* Make new GList @m->field_list using the fields from the VipsImage
	 * @image.
	 */
	fields = vips_image_get_fields( image );
	while ( (field = *fields++) )
		m->field_list = g_list_append( m->field_list, field );
	m->field_list_length = g_list_length( m->field_list );

	/* Add @m->field_list_length rows and 2 columns to the main_box. There
	 * is a row for every field, i.e., every property of the VipsImage
	 * @image.
	 */
	for ( int i = 0; (field = g_list_nth_data( m->field_list, i )); i++ ) {
		t = metadata_util_create_simple_label_box( field );
		gtk_widget_add_css_class( t, i % 2 ? "odd" : "even" );
		gtk_box_append( GTK_BOX( m->left_column ), t );

		t = metadata_util_create_input_box( image, field );
		gtk_widget_add_css_class( t, i % 2 ? "odd" : "even" );
		gtk_box_append( GTK_BOX( m->right_column ), t );
	}
}

/* This is called when the TileSource changes. In particular, a new VipsImage
 * might have been loaded, or there might no image loaded. Clean up any
 * existing input widgets and create new ones. This is the callback for the
 * "changed" event on @m->image_window->tile_source.
 *
 * @tile_source		The new tile_source, which is currently held by
 * 			m->image_window.
 *
 * @m	Metadata *	this
 */
static void
metadata_tile_source_changed( TileSource *tile_source, Metadata *m )
{

#ifdef DEBUG
	puts( "metadata_tile_source_changed" );
#endif

	/* If there is a new VipsImage on the tile source, use it to create
	 * the new main_box of user input widgets.
	 */
	if ( tile_source->image )
		/* The metadata_create_main_box function uses the VipsImage to
		 * dynamically create a GtkGrid of user input widgets for
		 * viewing and editing image metadata. It cleans up the old
		 * main_box and gets the image from the new TileSource ( which is
		 * why @tile_source isn't passed as an argument here ).
		 */
		metadata_create_main_box( m );
}

/* This function is called when ImageWindow changes. It is connected to the
 * "changed" signal on @m->image_window.
 *
 * @image_window	The new ImageWindow *
 * @m	Metadata *	this
 */
static void
metadata_image_window_changed( ImageWindow *image_window, Metadata *m )
{
#ifdef DEBUG
	puts( "metadata_image_window_changed" );
#endif

	g_signal_connect_object( image_window_get_tile_source( image_window ),
			"changed", G_CALLBACK( metadata_tile_source_changed ),
			m, 0 );
}

/* Change @m->image_window, and watch it for "changed" events. Only used
 * privately by Metadata methods.
 *
 * @m	Metadata *	this
 * @image_window	The new ImageWindow *
 */
static void
metadata_set_image_window( Metadata *m, ImageWindow *image_window )
{
#ifdef DEBUG
	puts( "metadata_set_image_window" );
#endif

	m->image_window = image_window;

	g_signal_connect_object( image_window, "changed",
		G_CALLBACK( metadata_image_window_changed ),
		m, 0 );
}

/* Apply the values currently held by the input widgets to their corresponding
 * properties on the.
 *
 * @m	Metadata *	this
 *
 */
void
metadata_apply( Metadata *m )
{
	VipsImage *image;
	TileSource *tile_source;
	GtkWidget *label_box, *input_box, *t;
	char *field;

#ifdef DEBUG
	puts("metadata_apply");
#endif

	/* Do nothing if there is no TileSource.
	 */
	if ( !(tile_source = image_window_get_tile_source( m->image_window )) )
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
		field = g_strdup( gtk_label_get_text( GTK_LABEL( t ) ) );

		/* Get the input widget child of the input_box.
		 */
		t = gtk_widget_get_first_child( input_box );

		/* Apply the value of the corresponding input widget to the
		 * VipsImage.
		 */
		metadata_util_apply_input( t, image, field );

		/* Iterate to the next row.
		 */
		label_box = gtk_widget_get_next_sibling( label_box );
		input_box = gtk_widget_get_next_sibling( input_box );
	}
}

/* Attach a new row with a label and an input widget to the main_box
 * for the provided VipsImage field name.
 *
 * Used as a callback in a foreach loop in metadata_search_changed.
 *
 * @ma_list_:	gpointer (GList*)	List of Match *
 * @m_:		gpointer (Metadata *)	this
 */
static void
metadata_append_field( gpointer ma_list_, gpointer m_ )
{
	GList *ma_list;
	Metadata *m;
	Match *ma;
	const gchar *field;
	GtkWidget *t;

#ifdef DEBUG
	puts("metadata_append_field")
#endif

	ma_list = (GList *) ma_list_;
	m = VIPSDISP_METADATA( m_ );

	ma = (Match *) ma_list->data;

	field = ma->text;

	m->field_list = g_list_append( m->field_list, (gpointer) field );

	/* Create a label box.
	 */
	t = metadata_util_create_label_box( ma_list );

	/* Add "even" or "odd" CSS class to label_box based on parity of the row
	 * index.
	 */
	gtk_widget_add_css_class( t, m->field_list_length % 2 ? "odd" : "even" );

	/* Append a label box to @left_column.
	 */
	gtk_box_append( GTK_BOX( m->left_column ), t );

	/* Create a label box.
	 */
	t = metadata_util_create_input_box( image_window_get_tile_source( m->image_window )->image, field );

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
 * by the user. Has loops or append_field callback function.
 *
 * Gets the list of matches. Separates it into two lists: exact and inexact.
 * Sorts inexact matches by increasing Levenshtein distance.
 *
 * @search_entry	GtkWidget * (GtkSearchEntry *)
 * @m_			gpointer (Metadata *)
 */
#define NUM_INEXACT_MATCHES 15
static void
metadata_search_changed( GtkWidget *search_entry, gpointer m_ )
{
	Metadata *m;
	char **fields, *patt, *field;
	GList *field_list;
	Match *match;
	GList *found, *found0, *found1, *s0, *s1, *t;
	guint *v;

	/* Initialize GList pointers to NULL, which means they are empty.
	 */
	found = found0 = found1 = s0 = s1 = t = NULL;

	/* Cast @m_ to Metadata *
	 */
	m = VIPSDISP_METADATA( m_ );

	/* Clean up any existing children of left_column and right_column.
	 */
	metadata_clear_main_box( m );

	/* Get the C-style list of image field strings.
	 */
	fields = vips_image_get_fields( image_window_get_tile_source(
			m->image_window )->image );

	/* Make a GList out of them, and search through it for the user input
	 * pattern.
	 *
	 * The results of the search are stored in @found - a GList of Match
	 * objects (see match.h and match.c).
	 */
	field_list = NULL;
	for ( int i=0; (field = fields[i]); i++ )
		field_list = g_list_append( field_list, field );

	patt = g_strdup( gtk_editable_get_text( GTK_EDITABLE( search_entry ) ) );

	/* Reuse the same buffer for the fuzzy matching algorithm.
	 */
	v = g_malloc( (strlen( patt ) + 1) * sizeof( guint ) );

	/* Get a GList of GLists of Match objects. Each GList is either all
	 * exact matches or all inexact matches. We'll separate them below.
	 */
	found = Match_get_exact_and_inexact_matches( field_list,
			(gchar *) patt, m->ignore_case, v );

	g_free( v );

	/* Create two GList of GList: one with exact matches @found0, and one
	 * with inexact matches @found1. Iterate through each GList @t contained
	 * in the GList @found, building up GList @s0 and GList @s1 with exact
	 * and inexact Match objects, respectively, and then appending @s0
	 * to @found0 and @s1 to @found1.
	 */
	while ( found ) {
		t = (GList *) found->data;
		s0 = s1 = NULL;

		/* Iterate through the GList @t, adding exact matches to @s0
		 * and inexact matches to @s1.
		 */
		while ( t ) {
			match = (Match *) t->data;

			if ( match->exact )
				s0 = g_list_append( s0, match );
			else
				s1 = g_list_append( s1, match );

			t = t->next;
		}

		/* Don't append NULL lists to the lists of lists @found0 and
		 * @found1, since that means the GList is empty, so there is
		 * nothing to show.
		 */
		if ( s0 )
			found0 = g_list_append( found0, s0 );
		if ( s1 )
			found1 = g_list_append( found1, s1 );

		found = found->next;
	}

	/* Add the exact matches to the UI.
	 */
	if ( g_list_length( found0 ) ) {
		/* Don't show the search warning, since there are exact matches
		 * to show.
		 */
		gtk_widget_set_visible( m->search_warning, FALSE );

		/* Add the exact matches to the UI.
		 */
		g_list_foreach( found0, metadata_append_field, m );
	}

	/* If there are no exact matches, then add the inexact matches, if any,
	 * to the UI.
	 */
	if ( !g_list_length( found0 ) && g_list_length( found1 ) ) {
		/* Tell the user there are no exact matches.
		 */
		gtk_widget_set_visible( m->search_warning, TRUE );

		/* Sort by increasing Levenshtein Distance.
		 */
		found1 = g_list_sort( found1, Match_list_comp );

		/* Truncate the list at NUM_INEXACT_MATCHES elements.
		 */
		t = g_list_nth( found1, NUM_INEXACT_MATCHES );
		t->prev->next = NULL;
		g_list_foreach( t, Match_free, NULL );
		g_list_free( t );

		/* Add the inexact matches to the UI.
		 */
		g_list_foreach( found1, metadata_append_field, m );
	}

	/* Clean up the array we got from vips_image_get_fields using the
	 * method recommended by the VIPS docs (see vips_image_get_fields).
	 */
	g_strfreev( fields );
}

/* This is like the "destructor" method of GObject. It is called to clean up
 * your object near the end of its lifetime.
 *
 * @m_	GObject * (Metadata *)	this
 *
 * (GObject boilerplate).
 */
static void
metadata_dispose( GObject *m_ )
{
	GtkWidget *child;
	Metadata *m;

#ifdef DEBUG
	puts( "metadata_dispose" );
#endif

	/* Clean up the straggling child widget if there is one.
	 */
	if ( (child = gtk_widget_get_first_child( GTK_WIDGET( m_ ) )) )
		gtk_widget_unparent( child );

	/* Cast to Metadata *.
	 */
	m = VIPSDISP_METADATA( m_ );

	/* Remove the GtkSyleProvider ( used for CSS ) from the GdkDisplay
	 * @m->display.
	 */
	gtk_style_context_remove_provider_for_display( m->display,
			GTK_STYLE_PROVIDER( m->provider ) );

	/* "Chain up" to the @dispose method of Metadata's parent class
	 * ( GtkWidget ). GObject defines the metadata_parent_class macro, so
	 * there is no need to reference the class name explicitly.
	 */
	G_OBJECT_CLASS( metadata_parent_class )->dispose( m_ );
}

/* This function lets you change the custom properties "image-window" and
 * "revealed".
 *
 * @m_		gpointer (Metadata *)	this
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
metadata_set_property( GObject *m_, guint prop_id,
		const GValue *v, GParamSpec *pspec )
{
#ifdef DEBUG
	puts( "metadata_set_property" );
#endif

	switch( prop_id ) {
	case PROP_IMAGE_WINDOW:
		metadata_set_image_window( VIPSDISP_METADATA( m_ ),
			VIPSDISP_IMAGE_WINDOW( g_value_get_object( v ) ) );
		break;
	case PROP_REVEALED:
		if ( g_value_get_boolean( v ) )
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
 * @m_		gpointer (Metadata *)	this
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
metadata_get_property( GObject *m_,
	guint prop_id, GValue *v, GParamSpec *pspec )
{
	Metadata *m;

#ifdef DEBUG
	puts("metadata_get_property");
#endif

	m = VIPSDISP_METADATA( m_ );

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

/* Initialize a Metadata object.
 *
 * @m	GObject * (Metadata *)	this
 *
 * (GObject boilerplate).
 */
static void
metadata_init( Metadata *m )
{
#ifdef DEBUG
	puts("metadata_init");
#endif

	/* Ignore case during metadata field search.
	 */
	m->ignore_case = TRUE;

	m->display = gdk_display_get_default();
	m->provider = gtk_css_provider_new();
	gtk_css_provider_load_from_resource( m->provider,
		      APP_PATH "/metadata.css" );
	gtk_style_context_add_provider_for_display( m->display,
			GTK_STYLE_PROVIDER( m->provider ),
			GTK_STYLE_PROVIDER_PRIORITY_APPLICATION );

	gtk_widget_init_template( GTK_WIDGET( m ) );

	/* The only child of the metadata widget is a GtkSearchBar. The static
	 * parts of the metadata widget are defined in gtk/metadata.ui.
	 */
	gtk_search_bar_set_search_mode( GTK_SEARCH_BAR( m->search_bar ), TRUE );

	/* Tell the metadata (GtkSearchBar) which GtkEditable widget will be
	 * providing user input text for the search query.
	 */
	gtk_search_bar_connect_entry( GTK_SEARCH_BAR( m->search_bar ),
		GTK_EDITABLE( m->search_entry ) );

	/* Connect the handler that gets called when the user modifies the
	 * search query.
	 */
	g_signal_connect( m->search_entry,
		"search-changed",
		G_CALLBACK( metadata_search_changed ), m );
}

/* This convenient macro binds pointers on a Metadata instance to XML nodes
 * defined in a .ui template. The property name must match the value of the "id"
 * attribute of the node. See "gtk/metadata.ui"
 *
 * (GObject boilerplate).
 */
#define BIND( field ) \
	gtk_widget_class_bind_template_child( GTK_WIDGET_CLASS( class ), \
		Metadata, field );

/* Initialize MetadataClass, which GObject defines for us if we used the
 * boilerplate macros and code correctly.
 *
 * @class	MetadataClass *	this class
 *
 * (GObject boilerplate).
 */
static void
metadata_class_init( MetadataClass *class )
{
	GObjectClass *gobject_class = G_OBJECT_CLASS( class );
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS( class );

#ifdef DEBUG
	printf( "metadata_class_init:\n" );
#endif

	gobject_class->dispose = metadata_dispose;

	gtk_widget_class_set_layout_manager_type( widget_class,
		GTK_TYPE_BIN_LAYOUT );
	gtk_widget_class_set_template_from_resource( widget_class,
		APP_PATH "/metadata.ui");

	BIND( scrolled_window );
	BIND( search_bar );
	BIND( search_entry );
	BIND( search_warning );
	BIND( main_box );
	BIND( left_column );
	BIND( right_column );

	gobject_class->set_property = metadata_set_property;
	gobject_class->get_property = metadata_get_property;

	g_object_class_install_property( gobject_class, PROP_IMAGE_WINDOW,
		g_param_spec_object( "image-window",
			_( "Image window" ),
			_( "The image window we display" ),
			IMAGE_WINDOW_TYPE,
			G_PARAM_READWRITE ) );

	g_object_class_install_property( gobject_class, PROP_REVEALED,
		g_param_spec_boolean( "revealed",
			_( "Revealed" ),
			_( "Show metadata." ),
			FALSE,
			G_PARAM_READWRITE ) );

}

/* Create a new Metadata widget.
 *
 * @image_window	The ImageWindow that will own Metadata widget.
 *
 * (GObject boilerplate).
 */
Metadata *
metadata_new( ImageWindow *image_window )
{
	Metadata *m;

#ifdef DEBUG
	printf( "metadata_new:\n" );
#endif

	/* Create a new Metadata object with its "image-window" property set to
	 * @image_window.
	 */
	m = g_object_new( metadata_get_type(),
		"image-window", image_window,
		NULL );

	return m;
}
