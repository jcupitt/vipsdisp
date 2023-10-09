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

GtkWidget  *
create_input( VipsImage *image, char* field_name )
{
	GtkWidget *input_box, *t, *box;
	VipsObjectClass *oclass;
	GType otype;
	GParamSpec *pspec;
	VipsArgumentClass *argument_class;
	VipsArgumentInstance *argument_instance;

	/* It is crucial to zero the GValue whose address we pass to
	 * vips_image_get. Otherwise, we will get runtime errors.
	 */
	GValue value = { 0 };

	input_box = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );

	gtk_widget_set_margin_start( input_box, 20 );

	/* Get the value of the given field from the image.
	 * Check the GType of @value, and select the appropriate user input
         * widget. Initialize the widget with @value.
	 * TODO
 	 */
	gboolean use_string = FALSE; 
	if( vips_object_get_argument( VIPS_OBJECT( image ), field_name,
		&pspec, &argument_class, &argument_instance ) ) {
		//g_warning( "%s", vips_error_buffer() );
		vips_error_clear();
		//return;
		vips_image_get( image, field_name, &value );
		use_string = TRUE;
	} else {
		otype = G_PARAM_SPEC_VALUE_TYPE( pspec );

		/* For now, skip VipsImage and VipsObject types.
		*/
		if( g_type_is_a( otype, VIPS_TYPE_IMAGE ) )
			return NULL;
		else if( g_type_is_a( otype, VIPS_TYPE_OBJECT ) &&
			(oclass = g_type_class_ref( otype )) )
			return NULL;
	}

	/* Add a user input widget for this property to the input_box. The widget
	 * chosen depends on the type of the property. Set the initial value of
	 * the user input widget to the default value for the property.
	 */
	if( use_string ) {
		GType type = G_VALUE_TYPE( &value );
		if ( type == G_TYPE_STRING ) {
			char* string_value = g_strdup( g_value_get_string( &value ) );
			GtkEntryBuffer* buffer =
				gtk_entry_buffer_new( string_value, -1 );

			t = gtk_text_new_with_buffer( buffer );
		} else if ( type == G_TYPE_ENUM ) {
			t = gtk_spin_button_new_with_range( -G_MAXINT + 1, G_MAXINT, 1 );
			gtk_spin_button_set_value( GTK_SPIN_BUTTON( t ),
				g_value_get_int( &value ) );
		} else if ( type == G_TYPE_INT ) {
			t = gtk_spin_button_new_with_range( -G_MAXINT + 1, G_MAXINT, 1 );
			gtk_spin_button_set_value( GTK_SPIN_BUTTON( t ),
				g_value_get_int( &value ) );
		} else if ( type == G_TYPE_UINT ) {
			t = gtk_spin_button_new_with_range( -G_MAXINT + 1, G_MAXINT, 1 );
			gtk_spin_button_set_value( GTK_SPIN_BUTTON( t ),
				g_value_get_int( &value ) );
		} else if ( type == G_TYPE_INT64 ) {
			t = gtk_spin_button_new_with_range( -G_MAXINT + 1, G_MAXINT, 1 );
			gtk_spin_button_set_value( GTK_SPIN_BUTTON( t ),
				g_value_get_int( &value ) );
		} else if ( type == G_TYPE_UINT64 ) {
			t = gtk_spin_button_new_with_range( -G_MAXINT + 1, G_MAXINT, 1 );
			gtk_spin_button_set_value( GTK_SPIN_BUTTON( t ),
				g_value_get_int( &value ) );
		} else if ( type == G_TYPE_LONG ) {
			t = gtk_spin_button_new_with_range( -G_MAXINT + 1, G_MAXINT, 1 );
			gtk_spin_button_set_value( GTK_SPIN_BUTTON( t ),
				g_value_get_int( &value ) );
		} else if ( type == G_TYPE_ULONG ) {
			t = gtk_spin_button_new_with_range( -G_MAXINT + 1, G_MAXINT, 1 );
			gtk_spin_button_set_value( GTK_SPIN_BUTTON( t ),
				g_value_get_int( &value ) );
		} else if ( type == G_TYPE_BOOLEAN ) {
			t = gtk_check_button_new();
			gtk_check_button_set_active( GTK_CHECK_BUTTON( t ),
				g_value_get_boolean( &value ) );
		} else if ( type == G_TYPE_FLOAT ) {
			t = gtk_spin_button_new_with_range( -G_MAXFLOAT + 1, G_MAXFLOAT, 1 );
			gtk_spin_button_set_value( GTK_SPIN_BUTTON( t ),
				g_value_get_int( &value ) );
		} else if ( type == G_TYPE_DOUBLE ) {
			t = gtk_spin_button_new_with_range( -G_MAXDOUBLE + 1, G_MAXDOUBLE, 1 );
			gtk_spin_button_set_value( GTK_SPIN_BUTTON( t ),
				g_value_get_int( &value ) );
		} else if ( type == G_TYPE_FLAGS ) {
			t = gtk_spin_button_new_with_range( -G_MAXINT + 1, G_MAXINT, 1 );
			gtk_spin_button_set_value( GTK_SPIN_BUTTON( t ),
				g_value_get_int( &value ) );
		} else if ( type == G_TYPE_LONG ) {
			t = gtk_spin_button_new_with_range( -G_MAXINT + 1, G_MAXINT, 1 );
			gtk_spin_button_set_value( GTK_SPIN_BUTTON( t ),
				g_value_get_int( &value ) );
		} else if ( type == G_TYPE_ULONG ) {
			t = gtk_spin_button_new_with_range( -G_MAXINT + 1, G_MAXINT, 1 );
			gtk_spin_button_set_value( GTK_SPIN_BUTTON( t ),
				g_value_get_int( &value ) );
		} else if ( type == G_TYPE_BOOLEAN ) {
			t = gtk_check_button_new();
			gtk_check_button_set_active( GTK_CHECK_BUTTON( t ),
				g_value_get_boolean( &value ) );
		} else if ( type == G_TYPE_FLOAT ) {
			t = gtk_spin_button_new_with_range( -G_MAXFLOAT + 1, G_MAXFLOAT, 1 );
			gtk_spin_button_set_value( GTK_SPIN_BUTTON( t ),
				g_value_get_int( &value ) );
		} else if ( type == G_TYPE_DOUBLE ) {
			t = gtk_spin_button_new_with_range( -G_MAXDOUBLE + 1, G_MAXDOUBLE, 1 );
			gtk_spin_button_set_value( GTK_SPIN_BUTTON( t ),
				g_value_get_int( &value ) );
		} else if ( type == G_TYPE_BOXED ) {
			if( g_type_is_a( otype, VIPS_TYPE_ARRAY_INT ) ) {
				/* No default values exist for ParamSpecBoxed, so make
				 * some up for now.
				 */
				t = gtk_spin_button_new_with_range( 0, 1000, 1 );
				gtk_spin_button_set_value( GTK_SPIN_BUTTON( t ), 0 );
			}
			else if( g_type_is_a( otype, VIPS_TYPE_ARRAY_DOUBLE ) ) {
				/* No default values exist for ParamSpecBoxed, so make
				 * some up for now.
				 */
				t = gtk_spin_button_new_with_range( 0, 1000, .1 );
				gtk_spin_button_set_value( GTK_SPIN_BUTTON( t ), 0 );
			}
			else if( g_type_is_a( otype, VIPS_TYPE_ARRAY_IMAGE ) ) {
				/* Ignore VipsImage-type parameters for now.
				 */
				return NULL;
			}
			else {
				/* Ignore parameters of unrecognized type for now.
				 */
				return NULL;
			}
		} else if ( (type = VIPS_TYPE_REF_STRING) ) {
			if ( !strstr( field_name, "thumbnail" ) ) {
				VipsRefString *ref_string;
				ref_string = g_value_get_boxed( &value );
				char *string_value = g_strdup( vips_ref_string_get( ref_string, NULL ) );
				GtkEntryBuffer* buffer =
					gtk_entry_buffer_new( string_value, -1 );
				t = gtk_text_new();
				gtk_text_set_buffer( GTK_TEXT( t ), buffer );
			} else {
				// Then just make @t an empty label.
				t = gtk_label_new( NULL );
			}
		} else {
			// Must be a VipsBlob then

			// Ignore metadata field names that contain the substring "thumbnail"
			if ( !strstr( field_name, "thumbnail" ) ) {
				VipsRefString *ref_string;
				ref_string = g_value_get_boxed( &value );
				char *string_value = g_strdup( vips_ref_string_get( ref_string, NULL ) );
				GtkEntryBuffer* buffer =
					gtk_entry_buffer_new( string_value, -1 );
				t = gtk_text_new();
				gtk_text_set_buffer( GTK_TEXT( t ), buffer );
			}
		}
	}
	else if ( G_IS_PARAM_SPEC_STRING( pspec ) ) {
		GParamSpecString *pspec_string = G_PARAM_SPEC_STRING( pspec );
		GtkEntryBuffer* buffer =
			gtk_entry_buffer_new( pspec_string->default_value, -1 );

		t = gtk_text_new_with_buffer( buffer );
	}
	else if( G_IS_PARAM_SPEC_BOOLEAN( pspec ) ) {
		GParamSpecBoolean *pspec_boolean = G_PARAM_SPEC_BOOLEAN( pspec );
		t = gtk_check_button_new();
		gtk_check_button_set_active( GTK_CHECK_BUTTON( t ),
			pspec_boolean->default_value );
	}
	else if( G_IS_PARAM_SPEC_ENUM( pspec ) ) {
		GParamSpecEnum *pspec_enum = G_PARAM_SPEC_ENUM( pspec );
		const char **property_nicknames =
			g_malloc( (pspec_enum->enum_class->n_values + 1) * sizeof( char * ) );

		for( int i = 0; i < pspec_enum->enum_class->n_values; ++i ) {
			property_nicknames[i] =
				pspec_enum->enum_class->values[i].value_nick;
		}
		property_nicknames[pspec_enum->enum_class->n_values] = NULL;
		t = gtk_drop_down_new_from_strings( property_nicknames );
		gtk_drop_down_set_selected( GTK_DROP_DOWN( t ),
			pspec_enum->default_value );
	}
	else if( G_IS_PARAM_SPEC_INT64( pspec ) ) {
		GParamSpecInt64 *pspec_int64 = G_PARAM_SPEC_INT64( pspec );
		t = gtk_spin_button_new_with_range( pspec_int64->minimum,
			pspec_int64->maximum, 1 );

		gtk_spin_button_set_value( GTK_SPIN_BUTTON( t ),
			(gint64)pspec_int64->default_value );
	}
	else if( G_IS_PARAM_SPEC_INT( pspec )) {
		GParamSpecInt *pspec_int = G_PARAM_SPEC_INT( pspec );
		t = gtk_spin_button_new_with_range( pspec_int->minimum,
			pspec_int->maximum, 1 );

		gtk_spin_button_set_value( GTK_SPIN_BUTTON( t ),
			(int)pspec_int->default_value );
	}
	else if( G_IS_PARAM_SPEC_UINT64( pspec ) ) {
		GParamSpecUInt64 *pspec_uint64 = G_PARAM_SPEC_UINT64( pspec );
		t = gtk_spin_button_new_with_range( pspec_uint64->minimum,
			pspec_uint64->maximum, 1 );

		gtk_spin_button_set_value( GTK_SPIN_BUTTON( t ),
			(guint64)pspec_uint64->default_value );
	}
	else if( G_IS_PARAM_SPEC_DOUBLE( pspec ) ) {
		GParamSpecDouble *pspec_double = G_PARAM_SPEC_DOUBLE( pspec );

		t = gtk_spin_button_new_with_range( pspec_double->minimum,
			pspec_double->maximum, 1 );

		gtk_spin_button_set_value( GTK_SPIN_BUTTON( t ),
			pspec_double->default_value );
	}
	else if( G_IS_PARAM_SPEC_BOXED( pspec ) ) {	
		if( g_type_is_a( otype, VIPS_TYPE_ARRAY_INT ) ) {
			/* No default values exist for ParamSpecBoxed, so make
			 * some up for now.
			 */
			t = gtk_spin_button_new_with_range( 0, 1000, 1 );
			gtk_spin_button_set_value( GTK_SPIN_BUTTON( t ), 0 );
		}
		else if( g_type_is_a( otype, VIPS_TYPE_ARRAY_DOUBLE ) ) {
			/* No default values exist for ParamSpecBoxed, so make
			 * some up for now.
			 */
			t = gtk_spin_button_new_with_range( 0, 1000, .1 );
			gtk_spin_button_set_value( GTK_SPIN_BUTTON( t ), 0 );
		}
		else if( g_type_is_a( otype, VIPS_TYPE_ARRAY_IMAGE ) ) {
			/* Ignore VipsImage-type parameters for now.
			 */
			return NULL;
		}
		else {
			/* Ignore parameters of unrecognized type for now.
			 */
			return NULL;
		}
	}
	else {
		printf("Unknown type for property \"%s\"\n", field_name);
		g_object_ref_sink( input_box );
		return NULL;
	}

	/* Create a box to contain the user input widget "t", and add a tooltip
	 * to the box. The tooltip contains the "blurb" for the property.
	 * Make the user input widget "t" fill the box horizontally.
	 * Append that container box to the "input_box".
	 */
	box = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
	if ( !use_string )
		gtk_widget_set_tooltip_text( GTK_WIDGET( box ),
			g_param_spec_get_blurb( pspec ) );

	gtk_widget_set_hexpand( t, TRUE );
	gtk_box_append( GTK_BOX( box ), t );
	gtk_box_append( GTK_BOX( input_box ), box );

	return input_box;
}

GtkGrid *
create_input_grid( Metadata *m )
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
		/* The create_input_grid function uses the VipsImage - on the
		 * TileSource of the ImageWindow pointed to by the Metadata
		 * widget - to dynamically create a GtkGrid of user input
		 * widgets for viewing and editing image metadata.
		 */
		m->grid = create_input_grid( m );

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

/* Size of alphabet index array. This number should be twice the size
 * of the alphabet, which is 128 in this case for ASCII (not extended).
 * The purpose of this extra space is explained later.
 */
#define ALPHABET_SIZE 256

/* The Match object will hold the index @i of the first character of the match
 * and the number of mismatched characters @k within that match. The Match
 * objects are also linked list nodes.
 */
typedef struct Match Match;
struct Match {
	int i;
	int k;
	gchar *text;
	gchar *pattern;
	int pattern_length;
};

Match *
Match_new( int i, int k, gchar *text, gchar *pattern )
{
	Match *t;
	t = g_malloc( sizeof( Match ) );
	t->i = i;
	t->k = k;
	//g_assert( text );
	t->text = g_strdup( text );
	t->pattern = pattern;
	t->pattern_length = strlen( pattern );
	return t;
}

void
Match_free( gpointer match_, gpointer user_data )
{
	Match *match = (Match *) match_;
	if ( match ) {
		g_free( match->text );
		g_free( match );
	}		
}

void
Match_print( gpointer match_, gpointer user_data )
{
	Match *match = (Match *) match_;
	printf( "position: %d, errors: %d, text: \"%s\", pattern: \"%s\", pattern_length: %d\n",
		match->i, match->k, match->text, match->pattern,
		match->pattern_length );
}

/* An array of lists. There are 128 linked lists in total - one for each
 * character in our 128-character alphabet. The last 128 lists characters are
 * added to the linked lists as needed. Using a power of lets us quickly access
 * the array in a cyclic manner by modding using the bitwise & operator. 
 *
 * Each list will contain the offsets for each occurence of that character, or a
 * single placeholder offset of -1 if no occurences are found.
 *
 * The offset is the distance of the character to the left of the end of
 * pattern, (i.e., measured by counting to the left from the end of pattern),
 * for a given character and a given instance of pattern.
 */
GList *alpha[ALPHABET_SIZE];

/* This function initializes the @alpha and @count arrays to look like this:
 *
 *     alpha = [ [ -1 ] ] * ALPHABET_SIZE 	where the inner brackets are a GList.
 *
 *     count = [m] * m
 *
 * @alpha will be an array of linked lists. Characters in the pattern
 * that do not occur or that occur exactly once in the text will have
 * corresponding linked lists with length one. Characters in the pattern that
 * occur in the text more than once will have corresponding linked lists with
 * length greater than one.
 *
 * The first m - 1  elements of @count will be skipped on the first iteration of
 * the cyclic array (since no match can be shorter than the @pattern). Note that
 * the values in @count are reset to m once they are no longer needed, until the
 * next loop around @count.
 *
 * @p - pattern string
 * @m - pattern length
 * @alpha - array of GList. See above.
 * @count - circular buffer for counts of matches
 */
void
preprocess( char *p, int m, GList *alpha[], int count[], int max_pattern_size )
{
	int i;

	for ( i = 0; i < ALPHABET_SIZE; i++ ) {
		alpha[i] = NULL;
		alpha[i] = g_list_append( alpha[i], GINT_TO_POINTER( -1 ) );
	}

	for ( i = 0; i < m; i++, p++ ) {
		if ( GPOINTER_TO_INT( alpha[(int)*p]->data ) == -1 )
			alpha[(int)*p]->data = GINT_TO_POINTER( m - i - 1 );
		else
			alpha[(int)*p] = g_list_append( alpha[(int)*p],
				GINT_TO_POINTER( m - i - 1 ) );
	}

	for ( i = 0; i < max_pattern_size; i++ )
		count[i] = m;
}

void
increment_offset( gpointer off_, gpointer args_ )
{
	gpointer *args = (gpointer *) args_;
	int i = GPOINTER_TO_INT( args[0] );
	int max_pattern_size = GPOINTER_TO_INT( args[1] );
	int *count = (int *) args[2];
	gint off = GPOINTER_TO_INT( off_ ) ;
	count[(i + off) % max_pattern_size]--;
}

gint
match_compare( gconstpointer a_, gconstpointer b_ )
{
	Match *a = (Match *) a_;
	Match *b = (Match *) b_;

	if ( a->k == b->k )
		return 0;
	else if (a->k < b->k )
		return -1;
	else return 1;
}

/* Find the position of the first character and number of mismatches of every
 * fuzzy match in a string @t with @k or fewer mismatches. Uses the array of
 * GList @alpha and the array of counts @count prepared by the preprocess
 * function.
 * @t - text string
 * @n - length of text string
 * @m - length of the pattern used to create @alpha and @count
 * @k - maximum number of allowed mismatches
 * @alpha - array of GList. See above.
 * @count - circular buffer for counts of matches
 */
GList *
search( char *t, int n, int m, int k, GList *alpha[], int count[], char *pattern, int max_pattern_size )
{
	int i, off;
	Match *match;
	GList *l0 = NULL, *l1 = NULL;
	char *text = t;

	/* Walk the text @t, which has length @n.
	 */
	for ( i = 0; i < n; i++ ) {
		/* If the current character in @t is in pattern, its
		 * corresponding list in @alpha will have a non-negative offset,
		 * thanks to the workdone by the preprocess function. If so, we
		 * need to decrement the counts in the circular buffer @count
		 * corresponding to the index of the character in the text and
		 * the offsets the lists corresponding to those characters,
		 * which the preprocess function prepared.
		 * 
		 * Note that we will only ever need m counts at a time, and
		 * we reset them to @m when we are done with them, in case
		 * they are needed when the text wraps 256 characters.
		 */
		l0 = alpha[(int)*t++];
		off = GPOINTER_TO_INT( l0->data );
		if ( off >= 0 ) {
			gpointer t[3] = {
				GINT_TO_POINTER( i ),
				GINT_TO_POINTER( max_pattern_size ),
				(gpointer) count,
			};
			g_list_foreach( l0, increment_offset, t );

		}

		/* If the count in @count corresponding to the current index in
		 * the text is no greater than @k, the number of mismatches we
		 * allow, then the pattern instance is reported as a fuzzy
		 * match. The position of the first letter in the match is
		 * calculated using the pattern length and the index of the last
		 * character in the match The number of mismatches is calculated
		 * from the number of matches.
		 */
		if ( i >= m - 1 && count[i % max_pattern_size] <= k ) {
			// DBG
			//printf( "i: %d, m: %d\n", i, m );
			//g_assert( i - m + 1 >= 0 );
			//g_assert( text );
			match = Match_new( i - m + 1, count[i % max_pattern_size], text, pattern );
			l1 = g_list_append( l1, match );
		}

		/* The count in @count corresponding to the current index in
		 * text is no longer needed, so we reset it to @m until we
		 * need it on the next wraparound.
		 */
		count[i % max_pattern_size] = m;
	}

	/* Sort by increasing k.
	 */
	l1 = g_list_sort( l1, match_compare );

	return l1;
}

void
markup_in_string_by_match( gpointer match_, gpointer markup_ )
{
	Match *match;
	GString *markup;
	gchar *replacement;

	match = (Match *) match_;
	markup = (GString *) markup_;

	if ( match->pattern && match->pattern[0] && !match->k ) {
		replacement = g_strdup_printf( "<>%s</>", match->pattern );
		g_string_replace( markup, match->pattern, replacement, 0 );
		g_free( replacement );
	}
}

gchar *
get_markup_from_match( GList *match )
{
	GString *markup;
	Match *first = (Match *) match->data;

	markup = g_string_new( g_strdup( first->text ) );

	g_list_foreach( match, markup_in_string_by_match, markup );

	g_string_replace( markup, "<>", "<b>", 0 );
	g_string_replace( markup, "</>", "</b>", 0 );

	return markup->str;
}

void
append_field_name( gpointer data, gpointer user_data )
{
	Metadata *m;
	GList *match_list;
	GtkWidget *label, *t;
	Match *match;

	match_list = (GList *) data;
	match = (Match *) match_list->data;

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

void
append_markup_field_name( gpointer data, gpointer user_data )
{
	Metadata *m;
	gchar *markup;
	GString *field_name;
	GList *match_list;
	GtkWidget *label, *input;

#ifdef DEBUG
	puts("append_markup_field_name")
#endif

	match_list = (GList *) data;
	markup = get_markup_from_match( match_list );
	m = VIPSDISP_METADATA( user_data );
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

gint
match_list_compare( gconstpointer a_, gconstpointer b_ )
{
	GList *a = (GList *) a_;
	GList *b = (GList *) b_;

	Match *match_a = (Match *) a->data;
	Match *match_b = (Match *) b->data;

	return match_compare( match_a, match_b );
}

void
append_if_match( gpointer data, gpointer user_data )
{
	GList *first, *match;
	gchar *haystack, *needle;
	GList **list_ptr;
	int n, m, k;

	list_ptr = (GList **) user_data;

	haystack = (gchar *) data;

	first = g_list_first( *list_ptr );
	needle = (gchar *) first->data;

	n = strlen( haystack );
	int count[n];
	m = strlen( needle );
	k = n;

	preprocess( needle, m, alpha, count, n );

	match = search( haystack, n, m, k, alpha, count, needle, n );

	if ( g_list_length( match ) )
		*list_ptr = g_list_append( *list_ptr, match );
}

GList *
find_strings_with_substring( GList *haystacks, gchar *needle )
{
	GList *found = NULL;

	found = g_list_append( found, needle );

	g_list_foreach( haystacks, append_if_match, &found );

	found = g_list_remove( found, (gconstpointer) needle );

	return found;
}

/* This is the callback function called whenever the GtkSearchEntry is modified
 * by the user.
 */
static void
search_changed( GtkWidget *search_entry, gpointer user_data )
{
	Metadata *m;
	char **fields, *text, *field;
	GList *all_field_list = NULL;

	/* Initialize GList pointers to NULL.
	 */
	GList *found, *found0, *found1, *s0, *s1, *t;
	found = found0 = found1 = s0 = s1 = t = NULL;

	m = VIPSDISP_METADATA( user_data );

	//g_assert( m->field_list );
	g_list_free( m->field_list );
	m->field_list = NULL;
	m->field_list_length = 0;

	gtk_scrolled_window_set_child( GTK_SCROLLED_WINDOW(
				m->scrolled_window ), NULL );

	m->grid = GTK_GRID( gtk_grid_new() );

	gtk_scrolled_window_set_child( GTK_SCROLLED_WINDOW(
				m->scrolled_window ), GTK_WIDGET( m->grid ) );

	fields = vips_image_get_fields( image_window_get_tile_source( 
			m->image_window )->image );

	for ( int i=0; (field = fields[i]); i++ )
		all_field_list = g_list_append( all_field_list, field );
		
	text = g_strdup( gtk_editable_get_text( GTK_EDITABLE( search_entry) ) );

	found = NULL;
	found = find_strings_with_substring( all_field_list, (gchar *) text );

	if ( ! g_list_length( found ) )
		return;

	/* Sort by increasing k, the number of errors.
	 */
	found = g_list_sort( found, match_list_compare );

	/* Get two GLists of GLists - one with the k=0 matches, and another with
	 * the k>0 matches.
	 */
	while ( found ) {
		t = (GList *) found->data;
		s0 = s1 = NULL;

		while ( t ) {
			Match *match = (Match *) t->data;
			if ( match ) {
				if ( match->k )
					s1 = g_list_append( s1, match );
				else
					s0 = g_list_append( s0, match );
				t = t->next;
			}
		}

		if ( s0 )
			found0 = g_list_append( found0, s0 );

		if ( s1 )
			found1 = g_list_append( found1, s1 );

		found = found->next;
	}


	/* Add the exact (k=0) matches, if any. These will have markup.
	 */
	if ( g_list_length( found0 ) )
		g_list_foreach( found0, append_markup_field_name, m );

	/* If there are no exact matches, then add the fuzzy (k>0) matches, if
	 * any. These will not have markup.
	 */
	if ( !g_list_length( found0 ) && g_list_length( found1 ) )
		g_list_foreach( found1, append_field_name, m );
}
 
static void
metadata_init( Metadata *m )
{
	GtkWidget *revealer;

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

	/* We want the Metadata widget label to be bold, so we need to use
	 * markup instead of plain text, which means we have to do it here
	 * instead of in the .ui file. 
	 */
	gtk_label_set_markup( GTK_LABEL( m->metadata_label ),
		"<b>Metadata</b>");

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

	/* The metadata window is initially hidden.
	 * TODO: make it controlled by settings
	 */
	//gtk_revealer_set_reveal_child( GTK_REVEALER( revealer ), FALSE );

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
		G_CALLBACK( search_changed ), m );

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

////
