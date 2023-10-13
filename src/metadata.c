/*
#define DEBUG
 */

#include "vipsdisp.h"

struct _Metadata
{
	GtkWidget parent_instance;

	ImageWindow *image_window;

	GtkWidget *error_bar;
	GtkWidget *error_label;
	GtkWidget *metadata_label;
	GtkWidget *scrolled_window;
	GtkWidget *search_bar;
	GtkWidget *search_entry;
	GtkWidget *search_warning;
	
	GtkGrid *grid;
	GList *field_list;
	int field_list_length;
	gboolean revealed;
};

G_DEFINE_TYPE( Metadata, metadata, GTK_TYPE_WIDGET );

enum {
	PROP_IMAGE_WINDOW = 1,
	PROP_REVEALED,

	SIG_LAST
};

static void
metadata_dispose( GObject *object )
{
	GtkWidget *child;

#ifdef DEBUG
	puts( "metadata_dispose" );
#endif

	if ( (child = gtk_widget_get_first_child( GTK_WIDGET( object ) )) )
		gtk_widget_unparent( child );

	G_OBJECT_CLASS( metadata_parent_class )->dispose( object );
}

GtkGrid *
metadata_create_grid( Metadata *m )
{
	GtkWidget *label, *input;
	GtkGrid *grid;
	VipsImage *image;
	char **fields, *field;

	image = image_window_get_tile_source( m->image_window )->image;

	fields = vips_image_get_fields( image );

	while ( (field = *fields++) )
		m->field_list = g_list_append( m->field_list, field );

	m->field_list_length = g_list_length( m->field_list );

	grid = GTK_GRID( gtk_grid_new() );

	for ( int i = 0; (field = g_list_nth_data( m->field_list, i )); i++ ) {
		label = gtk_label_new( field );	
		gtk_grid_attach( grid, label, 0, i, 1, 1 );
		input = create_input( image, field );
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
metadata_set_property( GObject *object,
	guint prop_id, const GValue *value, GParamSpec *pspec )
{
#ifdef DEBUG
	puts( "metadata_set_property" );
#endif

	Metadata *m = (Metadata *) object;

	switch( prop_id ) {
	case PROP_IMAGE_WINDOW:
		metadata_set_image_window( m,
			VIPSDISP_IMAGE_WINDOW( g_value_get_object( value ) ) );
		break;
	case PROP_REVEALED:
		gboolean revealed;
		revealed = g_value_get_boolean( value );
		if ( revealed ) {
			metadata_show( m );
		} else {
			metadata_hide( m );
		}
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID( object, prop_id, pspec );
	}
}

static void
metadata_get_property( GObject *object,
	guint prop_id, GValue *value, GParamSpec *pspec )
{
#ifdef DEBUG
	puts("metadata_get_property");
#endif

	Metadata *m = (Metadata *) object;

	switch( prop_id ) {
	case PROP_IMAGE_WINDOW:
		g_value_set_object( value, m->image_window );
		break;
	case PROP_REVEALED:
		g_value_set_boolean( value, m->revealed );
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID( object, prop_id, pspec );
	}
}

//static void
//metadata_error( Metadata *options )
//{
//	char *err;
//	int i;
//
//	// Remove any trailing \n. 
//	err = vips_error_buffer_copy();
//	for( i = strlen( err ); i > 0 && err[i - 1] == '\n'; i-- )
//		err[i - 1] = '\0';
//	gtk_label_set_text( GTK_LABEL( options->error_label ), err );
//	g_free( err );
//
//	gtk_info_bar_set_revealed( GTK_INFO_BAR( options->error_bar ), TRUE );
//}

static void
metadata_error_hide( Metadata *options )
{
#ifdef DEBUG
	puts( "metadata_error_hide" );
#endif

	gtk_info_bar_set_revealed( GTK_INFO_BAR( options->error_bar ), FALSE );
}

static void
metadata_error_response( GtkWidget *button, int response, 
	Metadata *options )
{
#ifdef DEBUG
	puts( "metadata_error_response" );
#endif

	metadata_error_hide( options );
}

#define SMALLER_X .9
#define SMALLER_Y .79
#define BIGGER_X 1.9
#define SHORT_WAIT_MS 100

void
metadata_show( Metadata *m )
{
	int og_width, og_height, new_width, new_height;
	GtkWidget *search_bar_box, *main_box, *revealer;

#ifdef DEBUG
	puts("metadata_show");
#endif

	m->revealed = TRUE;

	main_box = image_window_get_main_box( m->image_window );

	/* Make the metadata widget appear to the right of the image.
	 */
	gtk_orientable_set_orientation( GTK_ORIENTABLE( main_box ),
		GTK_ORIENTATION_HORIZONTAL );

	/* Center the metadata widget vertically.
	 */
	gtk_widget_set_valign( main_box, GTK_ALIGN_CENTER );
	
	/* Get the original dimensions of the ImageWindow.
	 */
	og_width = image_window_get_og_width( m->image_window );
	og_height = image_window_get_og_height( m->image_window );

	/* The width of the ImageWindow is increased to accomodate the metadata
	 * widget.
	 */
	gtk_window_set_default_size( GTK_WINDOW( m->image_window ),
		BIGGER_X * og_width, og_height );
	
	/* The metadata widget is sized relative to the ImageWindow.
	 */
	new_width = og_width * SMALLER_X;
	new_height = og_height * SMALLER_Y;
	gtk_widget_set_size_request( GTK_WIDGET( m ), new_width, new_height );

	/* Each GtkSearchBar has a child accessible through the GtkSearchBar API
	 * via gtk_search_bar_get_child. This child might not be a direct
	 * descendant. For the metadata widget, this child is a GtkBox
	 * called search_bar_box that is used to control orientation and size.
	 * The search_bar_box is sized the same as the metadata widget that
	 * contains it.
	 */
	search_bar_box = gtk_search_bar_get_child(
		GTK_SEARCH_BAR( m->search_bar ) );

	gtk_widget_set_size_request( search_bar_box, new_width, new_height );
	
	/* The scrolled_window is a GtkScrolledWindow that is sized to match the
	 * search_bar_box that contains it.
	 */
	gtk_widget_set_size_request( m->scrolled_window,
		new_width, new_height );

	gtk_scrolled_window_set_max_content_width(
		GTK_SCROLLED_WINDOW( m->scrolled_window ), new_width );

	gtk_scrolled_window_set_max_content_height(
		GTK_SCROLLED_WINDOW( m->scrolled_window ), new_height );

	/* The m window should be ready to go, since its
	 * grid of input widgets is updated whenever a new
	 * VipsImage is loaded. Reveal the m widget.
	 */
	gtk_widget_show( GTK_WIDGET( m ) );
	revealer = gtk_widget_get_first_child( m->search_bar );
	gtk_revealer_set_reveal_child( GTK_REVEALER( revealer ), TRUE );
}

gboolean
metadata_shrink_window( gpointer user_data )
{
	Metadata *m; 
	GtkWidget *main_box;
	int og_width, og_height;

#ifdef DEBUG
	puts("metadata_shrink_window");
#endif

	m = VIPSDISP_METADATA( user_data );
	
	gtk_widget_set_size_request( GTK_WIDGET( m ), 0, 0 );

	gtk_widget_hide( GTK_WIDGET( m ) );

	/* Restore the original orientation and alignment of the ImageWindow
	 * main_box.
	 */
	main_box = image_window_get_main_box( m->image_window );
	gtk_orientable_set_orientation( GTK_ORIENTABLE( main_box ),
		GTK_ORIENTATION_VERTICAL );
	gtk_widget_set_valign( main_box, GTK_ALIGN_FILL );

	/* Restore the original dimensions of the ImageWindow.
	 */
	og_width = image_window_get_og_width( m->image_window );
	og_height = image_window_get_og_width( m->image_window );
	gtk_window_set_default_size( GTK_WINDOW( m->image_window ),
		og_width, og_height );

	return FALSE;
}

void
metadata_hide( Metadata *m )
{
	GtkWidget *revealer;

#ifdef DEBUG
	puts( "metadata_hide" );
#endif

	m->revealed = FALSE;

	/* The first child of a GtkSearchBar is a GtkRevealer.
	 */
	revealer = gtk_widget_get_first_child( m->search_bar );

	/* Hide the metadata widget with a sliding animation.
	 */
	gtk_revealer_set_reveal_child( GTK_REVEALER( revealer ), FALSE );

	/* Shrink the ImageWindow back to its original size,
	 * after a short wait to allow time for the animation to
	 * show.
	 */
	g_timeout_add( SHORT_WAIT_MS,
		(GSourceFunc) metadata_shrink_window, m );
}

void
metadata_apply( Metadata *m )
{
	GtkWidget *t, *label;
	char *field_name;
	GString *field_name_string;
	VipsImage *image; 
	VipsObjectClass *oclass;
	GType otype;
	GParamSpec *pspec;
	VipsArgumentClass *argument_class;
	VipsArgumentInstance *argument_instance;

#ifdef DEBUG
	puts("metadata_apply");
#endif

	image = image_window_get_tile_source( m->image_window )->image;
	for ( int i = 0; (label = gtk_grid_get_child_at( m->grid,
					0, i )); i++ ) {
		// Get first child of first child of ith grid item.
		t = gtk_grid_get_child_at( m->grid, 1, i );
		t = gtk_widget_get_first_child( t );
		t = gtk_widget_get_first_child( t );

		GValue value = { 0 }, v = { 0 };

		field_name = g_strdup( gtk_label_get_text( GTK_LABEL( label ) ) );
		field_name_string = g_string_new( field_name );
		g_string_replace( field_name_string, "<b>", "", 0 );
		g_string_replace( field_name_string, "</b>", "", 0 );
		g_free( field_name );
		field_name = field_name_string->str;

		gboolean use_string = FALSE; 
		if( vips_object_get_argument( VIPS_OBJECT( image ), field_name,
			&pspec, &argument_class, &argument_instance ) ) {
			vips_error_clear();
			vips_image_get( image, field_name, &value );
			use_string = TRUE;
		} else {
			otype = G_PARAM_SPEC_VALUE_TYPE( pspec );
			if( g_type_is_a( otype, VIPS_TYPE_IMAGE ) )
				return;
			else if( g_type_is_a( otype, VIPS_TYPE_OBJECT ) &&
				(oclass = g_type_class_ref( otype )) )
				return;
		}

		if( use_string ) {
			GType type = G_VALUE_TYPE( &value );
			if ( type == G_TYPE_STRING ) {
				GtkEntryBuffer* buffer = gtk_text_get_buffer( GTK_TEXT( t ) );
				char *text = g_strdup( gtk_entry_buffer_get_text( buffer ) );
				vips_image_set_string( image, field_name, text );
			} else if ( type == G_TYPE_ENUM ) {
			} else if ( type == G_TYPE_INT ) {
				int d = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( t ) );
				vips_image_set_int( image, field_name, d );
			} else if ( type == G_TYPE_UINT ) {
				int d = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( t ) );
				vips_image_set_int( image, field_name, d );
			} else if ( type == G_TYPE_INT64 ) {
				int d = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( t ) );
				vips_image_set_int( image, field_name, d );
			} else if ( type == G_TYPE_UINT64 ) {
				int d = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( t ) );
				vips_image_set_int( image, field_name, d );
			} else if ( type == G_TYPE_LONG ) {
				int d = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( t ) );
				vips_image_set_int( image, field_name, d );
			} else if ( type == G_TYPE_ULONG ) {
				int d = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( t ) );
				vips_image_set_int( image, field_name, d );
			} else if ( type == G_TYPE_BOOLEAN ) {
				gboolean b = gtk_check_button_get_active( GTK_CHECK_BUTTON( t ) );
				g_value_init( &v, G_TYPE_BOOLEAN );
				g_value_set_boolean( &v, b );
				vips_image_set( image, field_name, &v );
				g_value_unset( &v );
			} else if ( type == G_TYPE_FLOAT ) {
				int d = gtk_spin_button_get_value( GTK_SPIN_BUTTON( t ) );
				vips_image_set_double( image, field_name, d );
			} else if ( type == G_TYPE_DOUBLE ) {
				int d = gtk_spin_button_get_value( GTK_SPIN_BUTTON( t ) );
				vips_image_set_double( image, field_name, d );
			} else if ( type == G_TYPE_FLAGS ) {
				int d = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( t ) );
				vips_image_set_int( image, field_name, d );
			} else if ( type == G_TYPE_BOXED ) {
			} else if ( (type == VIPS_TYPE_REF_STRING) ) {
				GtkEntryBuffer* buffer = gtk_text_get_buffer( GTK_TEXT( t ) );
				const char *text = gtk_entry_buffer_get_text( buffer );
				vips_image_set_blob_copy( image, field_name, text, strlen( text ) );
			} else {
				// Must be a VipsBlob then

				// Ignore m field names that contain the substring "thumbnail"
				if ( !strstr( field_name, "thumbnail" ) ) {
					GtkEntryBuffer* buffer = gtk_text_get_buffer( GTK_TEXT( t ) );
					const char *text = gtk_entry_buffer_get_text( buffer );
					vips_image_set_blob_copy( image, field_name, text, strlen( text ) );
				}
			}
		}
		else if ( G_IS_PARAM_SPEC_STRING( pspec ) ) {
			GtkEntryBuffer* buffer = gtk_text_get_buffer( GTK_TEXT( t ) );
			char *text = g_strdup( gtk_entry_buffer_get_text( buffer ) );
			vips_image_set_string( image, field_name, text );
		}
		else if( G_IS_PARAM_SPEC_BOOLEAN( pspec ) ) {
			gboolean b = gtk_check_button_get_active( GTK_CHECK_BUTTON( t ) );
			g_value_init( &v, G_TYPE_BOOLEAN );
			g_value_set_boolean( &v, b );
			vips_image_set( image, field_name, &v );
			g_value_unset( &v );
		}
		else if( G_IS_PARAM_SPEC_ENUM( pspec ) ) {
			int d = gtk_drop_down_get_selected( GTK_DROP_DOWN( t ) );
			g_value_init( &v, G_TYPE_ENUM );
			g_value_set_enum( &v, d );
			vips_image_set( image, field_name, &v );
			g_value_unset( &v );
		
		}
		else if( G_IS_PARAM_SPEC_INT64( pspec ) ) {
			int d = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( t ) );
			vips_image_set_int( image, field_name, d );
		}
		else if( G_IS_PARAM_SPEC_INT( pspec )) {
			int d = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( t ) );
			vips_image_set_int( image, field_name, d );
		}
		else if( G_IS_PARAM_SPEC_UINT64( pspec ) ) {
			int d = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( t ) );
			vips_image_set_int( image, field_name, d );
		}
		else if( G_IS_PARAM_SPEC_DOUBLE( pspec ) ) {
			int d = gtk_spin_button_get_value( GTK_SPIN_BUTTON( t ) );
			vips_image_set_double( image, field_name, d );
		}
		else if( G_IS_PARAM_SPEC_BOXED( pspec ) ) {	
			if( g_type_is_a( otype, VIPS_TYPE_ARRAY_INT ) ) {
				return;
			}
			else if( g_type_is_a( otype, VIPS_TYPE_ARRAY_DOUBLE ) ) {
				return;
			}
			else if( g_type_is_a( otype, VIPS_TYPE_ARRAY_IMAGE ) ) {
				return;
			}
			else {
				return;
			}
		}
		else {
			printf("Unknown type for property \"%s\"\n", field_name);
			g_object_ref_sink( t );
			return;
		}
	}
}

void
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
	gtk_grid_attach( m->grid, label, 0, m->field_list_length, 1, 1 );

	t = create_input( image_window_get_tile_source(
				m->image_window )->image, match->text );
	if ( t )
		gtk_grid_attach( m->grid, t,
				1, m->field_list_length++, 1, 1 );
}

/* Append fields names to the UI with markup.
 *
 * Used as a callback in a foreach loop in metadata_search_changed.
 *
 * @data:	GList of Match
 * @m_:		Metadata
 */
void
metadata_append_markup_field( gpointer data, gpointer m_ )
{
	Metadata *m;
	gchar *markup;
	GString *field_name;
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
	gtk_label_set_markup( GTK_LABEL( label ), markup );

	gtk_grid_attach( m->grid, label, 0, m->field_list_length, 1, 1 );

	field_name = g_string_new( markup );
	g_string_replace( field_name, "<b>", "", 0 );
	g_string_replace( field_name, "</b>", "", 0 );

	input = create_input( image_window_get_tile_source(
				m->image_window )->image, field_name->str );

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

	/* Free the old field_list Glist.
	 */
	g_list_free( m->field_list );
	m->field_list = NULL;
	m->field_list_length = 0;

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
	char *s;
	GtkWidget *revealer;

	guint v[256] = {0};
	gchar *s1 = "damerau";
	gchar *s2 = "levenshtein";
	guint ld = glev( strlen(s1), s1, strlen(s2), s2, v );
	printf( " The LD between %s and %s is %u.\n", s1, s2, ld );

#ifdef DEBUG
	puts("metadata_init");
#endif

	gtk_widget_init_template( GTK_WIDGET( m ) );

	/* Connect signals to child widgets of the Metadata widget.
	 *
	 * error_response: controls the error_bar, which displays VIPS errors
	 * 	that may occur.
	 */
	g_signal_connect_object( m->error_bar, "response", 
		G_CALLBACK( metadata_error_response ), m, 0 );

	/* Make the Metadata widget title bold.
	 */
	s = g_strdup_printf( "<b>%s</b>", gtk_label_get_label(
				GTK_LABEL( m->metadata_label ) )  );
	gtk_label_set_markup( GTK_LABEL( m->metadata_label ), s );
	g_free( s );

	/* The only child of the metadata widget is a GtkSearchBar. The static
	 * parts of the metadata widget are defined in gtk/metadata.ui.
	 */
	gtk_search_bar_set_search_mode( GTK_SEARCH_BAR( m->search_bar ), TRUE );

	/* The first child of a GtkSearchBar is a GtkRevealer. This is not the
	 * same as the child accessible through the GtkSearchBar API via
	 * gtk_search_bar_get_child.
	 */
	revealer = gtk_widget_get_first_child( GTK_WIDGET( m->search_bar ) );

	gtk_revealer_set_transition_type( GTK_REVEALER( revealer ),
		GTK_REVEALER_TRANSITION_TYPE_SLIDE_LEFT );

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

	BIND( error_bar );
	BIND( error_label );
	BIND( metadata_label );
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
