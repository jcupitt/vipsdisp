/*
#define DEBUG
 */

#include "vipsdisp.h"

struct _Metadata
{
	GtkWidget parent_instance;

	ImageWindow *image_window;

	GtkWidget *scrolled_window;
	GtkWidget *search_bar;
	GtkWidget *search_entry;
	GtkWidget *search_warning;

	GtkGrid *grid;
	GList *field_list;
	int field_list_length;

	GdkDisplay *display;
	GtkCssProvider *provider;
};

G_DEFINE_TYPE( Metadata, metadata, GTK_TYPE_WIDGET );

enum {
	PROP_IMAGE_WINDOW = 1,
	PROP_REVEALED,

	SIG_LAST
};

static void
metadata_dispose( GObject *m_ )
{
	GtkWidget *child;
	Metadata *m;

#ifdef DEBUG
	puts( "metadata_dispose" );
#endif

	if ( (child = gtk_widget_get_first_child( GTK_WIDGET( m_ ) )) )
		gtk_widget_unparent( child );

	m = VIPSDISP_METADATA( m_ );

	gtk_style_context_remove_provider_for_display( m->display,
			GTK_STYLE_PROVIDER( m->provider ) );

	G_OBJECT_CLASS( metadata_parent_class )->dispose( m_ );
}

static void
metadata_clear_grid( Metadata *m )
{
	// Clean up the old grid
	for ( int i = 0; i < m->field_list_length; i++ )
		gtk_grid_remove_row( m->grid, m->field_list_length - 1 + i );

	// Clean up the old field_list
	if ( m->field_list ) {
		g_list_free( m->field_list );
		m->field_list = NULL;
		m->field_list_length = 0;
	}
}

static GtkGrid *
metadata_create_grid( Metadata *m )
{
	VipsImage *image;
	char **fields, *field;
	GtkGrid *grid;
	GtkWidget *label, *input;

	image = image_window_get_tile_source( m->image_window )->image;

	metadata_clear_grid( m );

	// Make new field list
	fields = vips_image_get_fields( image );
	while ( (field = *fields++) )
		m->field_list = g_list_append( m->field_list, field );

	m->field_list_length = g_list_length( m->field_list );

	// Make 2-column grid
	grid = GTK_GRID( gtk_grid_new() );
	for ( int i = 0; (field = g_list_nth_data( m->field_list, i )); i++ ) {
		label = gtk_label_new( field );
		gtk_widget_set_halign( label, GTK_ALIGN_END );
		gtk_widget_add_css_class( label, "metadata-label" );
		gtk_grid_attach( grid, label, 0, i, 1, 1 );

		input = metadata_util_create_input( image, field );
		gtk_grid_attach( grid, input, 1, i, 1, 1 );
	}

	return grid;
}

/* This is called when the TileSource changes. In particular, a new VipsImage
 * might have been loaded, or there might no image loaded. Destroy and - if
 * needed - recreate the grid.
 */
static void
metadata_tile_source_changed( TileSource *tile_source, Metadata *m )
{

#ifdef DEBUG
	puts( "metadata_tile_source_changed" );
#endif

	/* If there is a new VipsImage on the tile source, use it to create
	 * the new grid of user input widgets.
	 */
	if ( tile_source->image ) {
		/* The metadata_create_grid function uses the VipsImage - on the
		 * TileSource of the ImageWindow pointed to by the Metadata
		 * widget - to dynamically create a GtkGrid of user input
		 * widgets for viewing and editing image metadata.
		 */
		m->grid = metadata_create_grid( m );

		/* We make this grid scrollable by putting it in a
		 * GtkScrolledWindow.
	 	 */
		gtk_scrolled_window_set_child(
			GTK_SCROLLED_WINDOW( m->scrolled_window),
			GTK_WIDGET( m->grid ) );
	}
}

/* ImageWindow has a new TileSource.
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

void
metadata_apply( Metadata *m )
{
	GtkWidget *t, *label;
	char *field;
	GString *field_string;
	VipsImage *image;

#ifdef DEBUG
	puts("metadata_apply");
#endif

	image = image_window_get_tile_source( m->image_window )->image;

	// Walk through the labels in the UI ( the VipsImage property names )
	// and corresponding input widgets.
	for ( int i = 0; (label = gtk_grid_get_child_at( m->grid, 0, i )); i++ ) {
		// Get first child of grid item in the row @i, column 2. This
		// is the input corresponding to the property name label in the
		// first column in the same row.
		t = gtk_widget_get_first_child(
				gtk_grid_get_child_at( m->grid, 1, i ) );

		// Get the string name of the VipsImage field from the label in
		// the UI
		field = g_strdup( gtk_label_get_text( GTK_LABEL( label ) ) );
		field_string = g_string_new( field );
		g_string_replace( field_string, "<b>", "", 0 );
		g_string_replace( field_string, "</b>", "", 0 );
		g_free( field );
		field = g_strdup( field_string->str );
		g_string_free( field_string, 1 );

		// Apply the value of the corresponding input widget to the
		// VipsImage.
		metadata_util_apply_input( t, image, field );
	}
}

static void
metadata_append_field( gpointer data, gpointer user_data )
{
	Metadata *m;
	GList *ma_list;
	GtkWidget *label, *t;
	Match *match;

	ma_list = (GList *) data;
	match = (Match *) ma_list->data;

	m = VIPSDISP_METADATA( user_data );
	m->field_list = g_list_append( m->field_list, match->text );

	label = gtk_label_new( match->text );
	gtk_widget_add_css_class( label, "metadata-label" );
	gtk_widget_set_halign( label, GTK_ALIGN_END );
	gtk_grid_attach( m->grid, label, 0, m->field_list_length, 1, 1 );

	t = metadata_util_create_input( image_window_get_tile_source(
				m->image_window )->image, match->text );
	if ( t )
		gtk_grid_attach( m->grid, t,
				1, m->field_list_length, 1, 1 );

	m->field_list_length++;
}

/* Append fields names to the UI with markup.
 *
 * Used as a callback in a foreach loop in metadata_search_changed.
 *
 * @data:	GList of Match
 * @m_:		Metadata
 */
static void
metadata_append_markup_field( gpointer data, gpointer m_ )
{
	Metadata *m;
	gchar *markup;
	GString *field;
	GList *ma_list;
	GtkWidget *label, *input;

#ifdef DEBUG
	puts("metadata_append_markup_field")
#endif

	ma_list = (GList *) data;
	markup = Match_markup( ma_list );
	m = VIPSDISP_METADATA( m_ );
	m->field_list = g_list_append( m->field_list, markup );

	label = gtk_label_new( NULL );
	gtk_widget_add_css_class( label, "metadata-label" );
	gtk_widget_set_halign( label, GTK_ALIGN_END );
	gtk_label_set_markup( GTK_LABEL( label ), markup );

	gtk_grid_attach( m->grid, label, 0, m->field_list_length, 1, 1 );

	field = g_string_new( markup );
	g_string_replace( field, "<b>", "", 0 );
	g_string_replace( field, "</b>", "", 0 );

	input = metadata_util_create_input( image_window_get_tile_source(
				m->image_window )->image, field->str );

	if ( input )
		gtk_grid_attach( m->grid, input,
				1, m->field_list_length++, 1, 1 );
}

/* This is the callback function called whenever the GtkSearchEntry is modified
 * by the user. Has loops on append_markup or append_field
 * callback functions.
 *
 * Gets the list of matches. Separates it into two lists: exact and inexact.
 * Sorts inexact matches by increasing Levenshtein distance.
 */
static void
metadata_search_changed( GtkWidget *search_entry, gpointer m_ )
{
	Metadata *m;
	char **fields, *patt, *field;
	GList *all_field_list;

	/* Initialize GList pointers to NULL.
	 */
	GList *found, *found0, *found1, *s0, *s1, *t;
	found = found0 = found1 = s0 = s1 = t = NULL;

	m = VIPSDISP_METADATA( m_ );

	metadata_clear_grid( m );

	gtk_scrolled_window_set_child( GTK_SCROLLED_WINDOW(
				m->scrolled_window ), NULL );

	m->grid = GTK_GRID( gtk_grid_new() );

	gtk_scrolled_window_set_child( GTK_SCROLLED_WINDOW(
				m->scrolled_window ), GTK_WIDGET( m->grid ) );


	/* Get the C-style list of image field strings. Make a GList out of
	 * them, and search through it for the user input pattern.
	 */
	fields = vips_image_get_fields( image_window_get_tile_source(
			m->image_window )->image );

	all_field_list = NULL;
	for ( int i=0; (field = fields[i]); i++ )
		all_field_list = g_list_append( all_field_list, field );

	patt = g_strdup( gtk_editable_get_text( GTK_EDITABLE( search_entry) ) );

	found = Match_substr( all_field_list, (gchar *) patt );

	/* Get two GList of GList: one with exact matches @s0, and another with
	 * the inexact matches @s1.
	 */
	while ( found ) {
		t = (GList *) found->data;
		s0 = s1 = NULL;

		while ( t ) {
			Match *match = (Match *) t->data;

			if ( match->exact )
				s0 = g_list_append( s0, match );
			else
				s1 = g_list_append( s1, match );

			t = t->next;
		}

		/* Don't append NULL lists to the lists of lists @found0 and
		 * @found.
		 */
		if ( s0 )
			found0 = g_list_append( found0, s0 );
		if ( s1 )
			found1 = g_list_append( found1, s1 );

		found = found->next;
	}

	/* Add the exact matches to the UI with markup.
	 */
	if ( g_list_length( found0 ) ) {
		gtk_widget_set_visible( m->search_warning, FALSE );
		g_list_foreach( found0, metadata_append_markup_field, m );
	}

	/* If there are no exact matches, then add the inexact matches, if
	 * any, to the UI with no markup.
	 */
	if ( !g_list_length( found0 ) && g_list_length( found1 ) ) {
		gtk_widget_set_visible( m->search_warning, TRUE );

		/* Sort by increasing Levenshtein Distance.
		 */
		found1 = g_list_sort( found1, Match_list_comp );

		g_list_foreach( found1, metadata_append_field, m );
	}
}

static void
metadata_init( Metadata *m )
{
#ifdef DEBUG
	puts("metadata_init");
#endif

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

#define BIND( field ) \
	gtk_widget_class_bind_template_child( GTK_WIDGET_CLASS( class ), \
		Metadata, field );

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

Metadata *
metadata_new( ImageWindow *image_window )
{
	Metadata *m;

#ifdef DEBUG
	printf( "metadata_new:\n" );
#endif

	m = g_object_new( metadata_get_type(),
		"image-window", image_window,
		NULL );

	return m;
}
