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

	TileSource *tile_source;

	GtkWidget *properties;
	GtkWidget *scrolled_window;
	GtkWidget *search_entry;
	GtkWidget *search_warning;
	GtkWidget *main_box;
	GtkWidget *left_column;
	GtkWidget *right_column;

	GList *field_list;
	int field_list_length;
	gboolean ignore_case;
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
	PROP_TILE_SOURCE = 1,
	PROP_REVEALED,

	SIG_LAST
};

/* Clean up any existing children of @left_column and @right_column and any
 * existing elements of the GList @field_list.
 *
 * @m	Properties *	this
 */
static void
properties_clear_main_box( Properties *p )
{
	VIPS_UNREF( p->left_column );
	p->left_column = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );
	gtk_box_append( GTK_BOX( p->main_box ), p->left_column );

	VIPS_UNREF( p->right_column );
	p->right_column = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );
	gtk_box_append( GTK_BOX( p->main_box ), p->right_column );

	g_list_free_full( g_steal_pointer( &p->field_list ), g_free );
}

static void
properties_add_row(Properties *p, const char *field)
{
	VipsImage *image = tile_source_get_image( p->tile_source );

	GtkWidget *t;

	p->field_list = g_list_append( p->field_list, g_strdup( field ) );

	t = properties_util_create_simple_label_box( image, field );
	gtk_widget_add_css_class( t, p->field_list_length % 2 ? "odd" : "even" );
	gtk_box_append( GTK_BOX( p->left_column ), t );

	t = properties_util_create_input_box( image, field );
	gtk_widget_add_css_class( t, p->field_list_length % 2 ? "odd" : "even" );
	gtk_box_append( GTK_BOX( p->right_column ), t );

	p->field_list_length++;
}

/* Create a new main_box. 
 *
 * @m	Properties *	this
 */
static void
properties_refresh_main_box( Properties *p )
{
	VipsImage *image;
	char **fields;

	/* If there is no TileSource, clean up any existing main_box and do
	 * nothing.
	 */
	if( !p->tile_source ||
		!(image = tile_source_get_image( p->tile_source )) ) {
		properties_clear_main_box( p );
		return;
	}

	/* Clean up the old GtkGrid @p->main_box and GList @p->field_list. If
	 * there is nothing to clean up, nothing happens.
	 */
	properties_clear_main_box( p );

	/* Get the array of image property name character arrays. We are
	 * responsible for freeing this with g_strfreev. The Match methods do
	 * not modify/free the character arrays in @fields, so we only free them
	 * once the relevant data has been copied into widgets with their
	 * character array setter methods.
	 */
	fields = vips_image_get_fields( image );

	/* Make new GList @p->field_list using the fields from the VipsImage
	 * @image.
	 */
	for (char **f = fields; *f; f++) 
		properties_add_row( p, *f );

	g_strfreev( fields );
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
properties_tile_source_changed( TileSource *tile_source, Properties *p )
{
#ifdef DEBUG
	puts( "properties_tile_source_changed" );
#endif

	properties_refresh_main_box( p );
}

static void
properties_set_tile_source( Properties *p, TileSource *tile_source )
{
#ifdef DEBUG
	puts( "properties_set_image_window" );
#endif

	/* No need to ref ... the enclosing window holds a ref to us.
	 */
	p->tile_source = tile_source;

	g_signal_connect_object( tile_source,
			"changed", G_CALLBACK( properties_tile_source_changed ),
			p, 0 );
}

/* Apply the values currently held by the input widgets to their corresponding
 * properties on the.
 *
 * @m	Properties *	this
 *
 */
void
properties_apply( Properties *p )
{
	VipsImage *image;
	GtkWidget *label_box, *input_box, *t;
	const char *field;

#ifdef DEBUG
	puts("properties_apply");
#endif

	/* If there is a TileSource, get its VipsImage @image and continue.
	 */
	image = tile_source_get_image( p->tile_source );

	/* Walk through the labels in the UI ( the VipsImage property names )
	 * and corresponding input widgets.
	 */
	label_box = gtk_widget_get_first_child( p->left_column );
	input_box = gtk_widget_get_first_child( p->right_column );
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
 * @p_:		gpointer (Properties *)	this
 */
static void
properties_append_field( gpointer ma_list_, gpointer p_ )
{
	GList *ma_list;
	Properties *p;
	Match *ma;
	const gchar *field;
	GtkWidget *t;

#ifdef DEBUG
	puts("properties_append_field")
#endif

	ma_list = (GList *) ma_list_;
	p = VIPSDISP_PROPERTIES( p_ );
	ma = (Match *) ma_list->data;
	field = ma->text;

	p->field_list = g_list_append( p->field_list, (gpointer) field );

	/* Create a label_box for the given list node.
	 */
	t = properties_util_create_label_box( p->tile_source->image, ma_list );

	/* Add "even" or "odd" CSS class to label_box based on parity of the row
	 * index.
	 */
	gtk_widget_add_css_class( t, p->field_list_length % 2 ? "odd" : "even" );

	/* Append a label box to @left_column.
	 */
	gtk_box_append( GTK_BOX( p->left_column ), t );

	/* Create an input_box for the given list node.
	 */
	t = properties_util_create_input_box( p->tile_source->image, field );

	/* Add "even" or "odd" CSS class to input_box based on parity of the row
	 * index.
	 */
	gtk_widget_add_css_class( t, p->field_list_length % 2 ? "odd" : "even" );

	/* Append an input box to @right_column.
	 */
	gtk_box_append( GTK_BOX( p->right_column ), t );

	/* Increment the field list length.
	 */
	p->field_list_length++;
}

/* This is the callback function called whenever the GtkSearchEntry is modified
 * by the user. Adds exact matches - or inexact matches, if there are no exact
 * matches - to the UI.
 *
 * @search_entry	GtkWidget * (GtkSearchEntry *)
 * @p_			gpointer (Properties *)
 */
#define NUM_INEXACT_MATCHES 15
static void
properties_search_changed( GtkWidget *search_entry, gpointer p_ )
{
	Properties *p;
	char **fields, *field;
	const char *patt;
	GList *l, *t;
	guint *v;

	/* Cast @p_ to Properties *
	 */
	p = VIPSDISP_PROPERTIES( p_ );

	/* Clean up any existing children of left_column and right_column.
	 */
	properties_clear_main_box( p );

	/* Get the array of image property name character arrays. We are
	 * responsible for freeing this with g_strfreev. The Match methods do
	 * not modify/free the character arrays in @fields, so we only free them
	 * once the relevant data has been copied into widgets with their
	 * character array setter methods.
	 */
	fields = vips_image_get_fields( p->tile_source->image );

	/* Make new GList @p->field_list using the fields from the VipsImage
	 * @image.
	 */
	p->field_list = NULL;
	for ( int i=0; (field = fields[i]); i++ )
		p->field_list = g_list_append( p->field_list, field );

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
	l = match_list( TRUE, p->field_list, patt, p->ignore_case, v );

	/* If there were exact matches, add them to the UI. If there were no
	 * exact matches, get inexact matches, sort them by
	 * LEVENSHTEIN_DISTANCE, and add them to the UI.
	 */
	if( l )
		/* Don't show the search warning, since there are exact matches
		 * to show.
		 */
		gtk_widget_set_visible( p->search_warning, FALSE );
	else {
		/* Tell the user there are no exact matches.
		 */
		gtk_widget_set_visible( p->search_warning, TRUE );

		/* Get a GList of GList of single inexact Match objects. There
		 * is an inner GList for each property name in @field_list. Each
		 * inner GList contains a single inexact match.
		 */
		l = match_list( FALSE, p->field_list, patt, p->ignore_case, v );

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
	g_list_foreach( l, properties_append_field, p );

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
 * @p_	GObject * (Properties *)	this
 *
 * (GObject boilerplate).
 */
static void
properties_dispose( GObject *p_ )
{
	Properties *p = VIPSDISP_PROPERTIES( p_ );

#ifdef DEBUG
	puts( "properties_dispose" );
#endif

	VIPS_FREEF( gtk_widget_unparent, p->properties );

	/* "Chain up" to the @dispose method of Properties's parent class
	 * ( GtkWidget ). GObject defines the properties_parent_class macro, so
	 * there is no need to reference the class name explicitly.
	 */
	G_OBJECT_CLASS( properties_parent_class )->dispose( p_ );
}

/* This function lets you change the custom properties "image-window" and
 * "revealed".
 *
 * @p_		gpointer (Properties *)	this
 *
 * @prop_id	This is the signal id, e.g. PROP_TILE_SOURCE, defined in the
 * 		enum above.
 *
 * @v		The new value.
 *
 * @pspec	The param spec for the property corresponding to @prop_id.
 *
 * (GObject boilerplate)
 */
static void
properties_set_property( GObject *p_, guint prop_id, 
	const GValue *v, GParamSpec *pspec )
{
#ifdef DEBUG
	puts( "properties_set_property" );
#endif

	switch( prop_id ) {
	case PROP_TILE_SOURCE:
		properties_set_tile_source( VIPSDISP_PROPERTIES( p_ ),
			TILE_SOURCE( g_value_get_object( v ) ) );
		break;

	case PROP_REVEALED:
		if( g_value_get_boolean( v ) )
			gtk_widget_show( GTK_WIDGET( p_ ) );
		else
			gtk_widget_hide( GTK_WIDGET( p_ ) );
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID( p_, prop_id, pspec );
	}
}

/* This function lets you read the custom properties "image-window" and
 * "revealed".
 *
 * @p_		gpointer (Properties *)	this
 *
 * @prop_id	This is the signal id, e.g. PROP_TILE_SOURCE, defined in the
 * 		enum above.
 *
 * @v		The current value of the property.
 *
 * @pspec	The param spec for the property corresponding to @prop_id.
 *
 * (GObject boilerplate)
 */
static void
properties_get_property( GObject *p_,
	guint prop_id, GValue *v, GParamSpec *pspec )
{
	Properties *p;

#ifdef DEBUG
	puts("properties_get_property");
#endif

	p = VIPSDISP_PROPERTIES( p_ );

	switch( prop_id ) {
	case PROP_TILE_SOURCE:
		g_value_set_object( v, p->tile_source );
		break;

	case PROP_REVEALED:
		g_value_set_boolean( v,
				gtk_widget_get_visible( GTK_WIDGET( p_ ) ) );
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID( p_, prop_id, pspec );
	}
}

/* Initialize a Properties object.
 *
 * @m	GObject * (Properties *)	this
 *
 * (GObject boilerplate).
 */
static void
properties_init( Properties *p )
{
#ifdef DEBUG
	puts("properties_init");
#endif

	/* Ignore case during properties field search.
	 */
	p->ignore_case = TRUE;

	gtk_widget_init_template( GTK_WIDGET( p ) );

	/* Connect the handler that gets called when the user modifies the
	 * search query.
	 */
	g_signal_connect( p->search_entry,
		"search-changed",
		G_CALLBACK( properties_search_changed ), p );
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

	BIND( properties );
	BIND( scrolled_window );
	BIND( search_entry );
	BIND( search_warning );
	BIND( main_box );
	BIND( left_column );
	BIND( right_column );

	gobject_class->set_property = properties_set_property;
	gobject_class->get_property = properties_get_property;

	g_object_class_install_property( gobject_class, PROP_TILE_SOURCE,
		g_param_spec_object( "tile-source",
			_( "Tile source" ),
			_( "The tile source whose properties we display" ),
			TILE_SOURCE_TYPE,
			G_PARAM_READWRITE ) );

	g_object_class_install_property( gobject_class, PROP_REVEALED,
		g_param_spec_boolean( "revealed",
			_( "Revealed" ),
			_( "Show properties." ),
			FALSE,
			G_PARAM_READWRITE ) );

}

Properties *
properties_new( TileSource *tile_source )
{
	Properties *p;

#ifdef DEBUG
	printf( "properties_new:\n" );
#endif

	p = g_object_new( properties_get_type(),
		"tile-source", tile_source,
		NULL );

	return( p );
}
