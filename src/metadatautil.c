#include "vipsdisp.h"

/* No-op to prevent @w from propagating "scroll" events it receives.
 */
void disable_scroll_cb( GtkWidget *w ) {}

/* Disable scroll on a widget by adding a capture phase event handler and
 * connecting a no-op callback to the "scroll" event.
 */
static GtkWidget *
disable_scroll( GtkWidget *w )
{
	GtkEventController *ec;

	ec = gtk_event_controller_scroll_new(
			GTK_EVENT_CONTROLLER_SCROLL_VERTICAL );

	gtk_event_controller_set_propagation_phase( ec, GTK_PHASE_CAPTURE );
	g_signal_connect( ec, "scroll", G_CALLBACK( disable_scroll_cb ), w );
	gtk_widget_add_controller( w, ec );

	return w;
}

/* Create a spin button with range, default value, and optionally enabled
 * scrolling.
 */
GtkWidget *
create_spin_button( double min, double max, double step,
		double value, bool scroll )
{
	GtkWidget *sb;

	sb = gtk_spin_button_new_with_range( min, max, step );
	gtk_spin_button_set_value( GTK_SPIN_BUTTON( sb ), value );

	return scroll ? sb : disable_scroll( sb );
}

GtkWidget *
create_string_input( VipsImage *image, const gchar *field, GParamSpec *pspec ) {
	GtkWidget *t;
	const char *string_value;

	if ( !strcmp( field, "filename" ) )
		g_object_get( image, field, &string_value, NULL );
	else
		vips_image_get_string( image, field, &string_value );

	string_value = g_strdup( string_value );
	GtkEntryBuffer* buffer =
		gtk_entry_buffer_new( string_value, -1 );
	t = gtk_text_new();
	gtk_text_set_buffer( GTK_TEXT( t ), buffer );

	return t;
}

GtkWidget *
create_boolean_input( VipsImage *image, const gchar *field, GParamSpec *pspec ) {
	GtkWidget *t;
	int d;

	vips_image_get_int( image, field, &d );
	t = gtk_check_button_new();
	gtk_check_button_set_active( GTK_CHECK_BUTTON( t ), d );

	return t;
}

GtkWidget *
create_enum_input( VipsImage *image, const gchar *field, GParamSpec *pspec )
{
	GtkWidget *t;

	if ( pspec && G_IS_PARAM_SPEC_ENUM( pspec ) ) {
		GParamSpecEnum *pspec_enum = G_PARAM_SPEC_ENUM( pspec );
		int d;
		const char **property_nicknames;
		
		property_nicknames = g_malloc( (pspec_enum->enum_class->n_values + 1) * sizeof( char * ) );

		for( int i = 0; i < pspec_enum->enum_class->n_values; ++i )
			property_nicknames[i] = pspec_enum->enum_class->values[i].value_nick;
		property_nicknames[pspec_enum->enum_class->n_values] = NULL;
		t = gtk_drop_down_new_from_strings( property_nicknames );
		g_object_get( image, field, &d, NULL );
		gtk_drop_down_set_selected( GTK_DROP_DOWN( t ), d );
	} else { 
		int d;

		vips_image_get_int( image, field, &d );
		t = create_spin_button( -G_MAXINT + 1, G_MAXINT, 1, d, FALSE );
	}

	return t;
}

GtkWidget *
create_int_input( VipsImage *image, const gchar *field, GParamSpec *pspec ) {
	GtkWidget *t;
	int d;

	vips_image_get_int( image, field, &d );
	t = create_spin_button( -G_MAXINT + 1, G_MAXINT, 1, d, FALSE );

	return t;
}

GtkWidget *
create_double_input( VipsImage *image, const gchar *field, GParamSpec *pspec )
{
	GtkWidget *t;
	double d;

	vips_image_get_double( image, field, &d );
	t = create_spin_button( -G_MAXDOUBLE + 1, G_MAXDOUBLE, 1, d, FALSE );

	return t;
}

GtkWidget *
create_boxed_input( VipsImage *image, const gchar *field, GParamSpec *pspec )
{
	GtkWidget *t;

#ifdef DEBUG
	printf( "G_TYPE_BOXED for property \"%s\" in create_input\n", field );
#endif /* DEBUG */

	t = gtk_label_new( "" );

	return t;
}

GtkWidget *
create_empty_input() {
	GtkWidget *t;

#ifdef DEBUG
	printf("Unknown type for property \"%s\" in create_input\n", field);
#endif /* DEBUG */

		t = gtk_label_new( "" );

	return t;
}

/* Use introspection on VipsImage to create a UI input elements for each
 * property.
 */
GtkWidget *
create_input( VipsImage *image, char* field )
{
	GtkWidget *input_box, *t, *box;
	GType type;
	GParamSpec *pspec;
	VipsArgumentClass *arg_class;
	VipsArgumentInstance *arg_instance;

	/* Zero the GValue whose address we pass to vips_image_get. Otherwise, we
	 * will get runtime errors.
	 */
	GValue value = { 0 };

	/* Use introspection to get information about this field on the image.
	 * We want the paramspec ( if there is one ) in case this field is an
	 * enum, and also to get the blurb.
	 */
	vips_image_get( image, field, &value );
	if ( vips_object_get_argument( VIPS_OBJECT( image ), field,
			&pspec, &arg_class, &arg_instance ) )
		pspec = NULL;

	/* Create the actual user input widget @t for this property. The widget
	 * chosen depends on the type of the property. The value is the current
	 * value on the image.
	 */
	type = G_VALUE_TYPE( &value );
	if ( strstr( "thumbnail", field ) )
		t = gtk_label_new( "" );
	else if ( type == VIPS_TYPE_REF_STRING )
		t = create_string_input( image, field, pspec );
	else if ( pspec && G_IS_PARAM_SPEC_ENUM( pspec ) )
		t = create_enum_input( image, field, pspec );
	else switch( type ) {
	case G_TYPE_STRING:
		t = create_string_input( image, field, pspec );
		break;
	case G_TYPE_BOOLEAN:
		t = create_boolean_input( image, field, pspec );
		break;
	case G_TYPE_ENUM:
		t = create_enum_input( image, field, pspec );
		break;
	case G_TYPE_INT:
	case G_TYPE_INT64:
	case G_TYPE_UINT:
	case G_TYPE_UINT64:
	case G_TYPE_LONG:
	case G_TYPE_ULONG:
	case G_TYPE_FLAGS:
		t = create_int_input( image, field, pspec );
		break;
	case G_TYPE_FLOAT:
	case G_TYPE_DOUBLE:
		t = create_double_input( image, field, pspec );
		break;
	case G_TYPE_BOXED:
		t = create_boxed_input( image, field, pspec );
		break;
	default:
		t = create_empty_input( image, field, pspec );
	} /* end switch( type ) */

	/* Create the box we will return, @input_box.
	 */
	input_box = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );
	gtk_widget_set_margin_start( input_box, 20 );

	/* Create a box to contain the user input widget @t, and add a tooltip
	 * to the box if one is available.
	 */
	box = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
	if ( pspec )
		gtk_widget_set_tooltip_text( GTK_WIDGET( box ),
				g_param_spec_get_blurb( pspec ) );

	/* Fill out the horizontal space in @t, @box, and @input_box.
	 */
	gtk_widget_set_hexpand( t, TRUE );
	gtk_widget_set_halign( t, GTK_ALIGN_FILL );
	gtk_widget_set_hexpand( box, TRUE );
	gtk_widget_set_halign( box, GTK_ALIGN_FILL );
	gtk_widget_set_hexpand( input_box, TRUE );
	gtk_widget_set_halign( input_box, GTK_ALIGN_FILL );

	/* Style the user input widget @t using CSS provided by
	 * "gtk/metadata.css".
	 */
	gtk_widget_add_css_class( t, "metadata-input" );

	/* Nest the input widget in two boxes. This is the UI structure
	 * metadata_apply expects.
	 */
	gtk_box_append( GTK_BOX( box ), t );
	gtk_box_append( GTK_BOX( input_box ), box );

	/* Return the outermost box @input_box.
	 */
	return input_box;
}

