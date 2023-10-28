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
	const char *value;

	if ( !strcmp( field, "filename" ) )
		g_object_get( image, field, &value, NULL );
	else
		vips_image_get_string( image, field, &value );

	value = g_strdup( value );

#ifdef EXPERIMENTAL_METADATA_EDIT
	GtkEntryBuffer* buffer =
		gtk_entry_buffer_new( value, -1 );

	t = gtk_text_new();
	gtk_text_set_buffer( GTK_TEXT( t ), buffer );

#else
	t = gtk_label_new( g_strdup_printf( "%s", value ) );
#endif /* EXPERIMENTAL_METADATA_EDIT */

	return t;
}

GtkWidget *
create_boolean_input( VipsImage *image, const gchar *field, GParamSpec *pspec ) {
	GtkWidget *t;
	int d;

	vips_image_get_int( image, field, &d );

#ifdef EXPERIMENTAL_METADATA_EDIT
	t = gtk_check_button_new();
	gtk_check_button_set_active( GTK_CHECK_BUTTON( t ), d );
#else
	t = gtk_label_new( g_strdup_printf( "%s",
				d ? "true" : "false" ) );
#endif /* EXPERIMENTAL_METADATA_EDIT */

	return t;
}

GtkWidget *
create_enum_input( VipsImage *image, const gchar *field, GParamSpec *pspec )
{
	GtkWidget *t;

	if ( pspec && G_IS_PARAM_SPEC_ENUM( pspec ) ) {
		GParamSpecEnum *pspec_enum = G_PARAM_SPEC_ENUM( pspec );
		int d;
		const char **nicks;
		
		nicks = g_malloc( (pspec_enum->enum_class->n_values + 1) * sizeof( char * ) );

		for( int i = 0; i < pspec_enum->enum_class->n_values; ++i )
			nicks[i] = pspec_enum->enum_class->values[i].value_nick;
		nicks[pspec_enum->enum_class->n_values] = NULL;

		g_object_get( image, field, &d, NULL );

#ifdef EXPERIMENTAL_METADATA_EDIT
		t = gtk_drop_down_new_from_strings( nicks );
		gtk_drop_down_set_selected( GTK_DROP_DOWN( t ), d );
#else
		t = gtk_label_new( g_strdup_printf( "%s", nicks[d] ) );
#endif /* EXPERIMENTAL_METADATA_EDIT */

	} else { 
		int d;

		vips_image_get_int( image, field, &d );
#ifdef EXPERIMENTAL_METADATA_EDIT
		t = create_spin_button( -G_MAXINT + 1, G_MAXINT, 1, d, FALSE );
#else
		t = gtk_label_new( g_strdup_printf( "%d", d ) );
#endif /* EXPERIMENTAL_METADATA_EDIT */
	}

	return t;
}

GtkWidget *
create_int_input( VipsImage *image, const gchar *field, GParamSpec *pspec ) {
	GtkWidget *t;
	int d;

	vips_image_get_int( image, field, &d );

#ifdef EXPERIMENTAL_METADATA_EDIT
	t = create_spin_button( -G_MAXINT + 1, G_MAXINT, 1, d, FALSE );
#else
	t = gtk_label_new( g_strdup_printf( "%d", d ) );
#endif /* EXPERIMENTAL_METADATA_EDIT */

	return t;
}

GtkWidget *
create_double_input( VipsImage *image, const gchar *field, GParamSpec *pspec )
{
	GtkWidget *t;
	double d;

	vips_image_get_double( image, field, &d );

#ifdef EXPERIMENTAL_METADATA_EDIT
	t = create_spin_button( -G_MAXDOUBLE + 1, G_MAXDOUBLE, 1, d, FALSE );

#else
	t = gtk_label_new( g_strdup_printf( "%f", d ) );
#endif /* EXPERIMENTAL_METADATA_EDIT */

	return t;
}

GtkWidget *
create_boxed_input( VipsImage *image, const gchar *field, GParamSpec *pspec )
{
	GtkWidget *t;

#ifdef DEBUG
	printf( "G_TYPE_BOXED for property \"%s\" in metadata_util_create_input\n", field );
#endif /* DEBUG */

	t = gtk_label_new( "" );

	return t;
}

GtkWidget *
create_empty_label() {
	GtkWidget *t;

#ifdef DEBUG
	printf("Unknown type for property \"%s\" in metadata_util_create_input\n", field);
#endif /* DEBUG */

		t = gtk_label_new( "" );

	return t;
}

/* Use introspection on VipsImage to create a UI input elements for each
 * property.
 */
GtkWidget *
metadata_util_create_input( VipsImage *image, char* field )
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
		t = create_empty_label( image, field, pspec );
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

#ifdef EXPERIMENTAL_METADATA_EDIT
	gtk_widget_set_hexpand( t, TRUE );
	gtk_widget_set_halign( t, GTK_ALIGN_FILL );
#endif /* EXPERIMENTAL_METADATA_EDIT */

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

void
metadata_util_apply_string_input( GtkWidget *t, VipsImage *image, char* field, GParamSpec *pspec )
{
	GtkEntryBuffer* buffer;
	char *text;

	buffer = gtk_text_get_buffer( GTK_TEXT( t ) );
	text = g_strdup( gtk_entry_buffer_get_text( buffer ) );
	vips_image_set_string( image, field, text );

}

void
metadata_util_apply_boolean_input( GtkWidget *t, VipsImage *image, char* field, GParamSpec *pspec )
{
	gboolean b;
	GValue v = { 0 };

	b = gtk_check_button_get_active( GTK_CHECK_BUTTON( t ) );
	g_value_init( &v, G_TYPE_BOOLEAN );
	g_value_set_boolean( &v, b );
	vips_image_set( image, field, &v );
	g_value_unset( &v );

}

void
metadata_util_apply_enum_input( GtkWidget *t, VipsImage *image, char* field, GParamSpec *pspec )
{
	int d;
	GValue v = { 0 };

	if ( pspec ) {
		d = gtk_drop_down_get_selected( GTK_DROP_DOWN( t ) );
		g_value_init( &v, G_TYPE_ENUM );
		g_value_set_enum( &v, d );
		vips_image_set( image, field, &v );
		g_value_unset( &v );
	} else { 
		d = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( t ) );
		vips_image_set_int( image, field, d );
	}
}
	
void
metadata_util_apply_int_input( GtkWidget *t, VipsImage *image, char* field, GParamSpec *pspec )
{
	int d;

	d = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( t ) );
	vips_image_set_int( image, field, d );
}

void
metadata_util_apply_double_input( GtkWidget *t, VipsImage *image, char* field, GParamSpec *pspec )
{
	double d;

	d = gtk_spin_button_get_value( GTK_SPIN_BUTTON( t ) );
	vips_image_set_double( image, field, d );
}

void
metadata_util_apply_boxed_input( GtkWidget *t, VipsImage *image, char* field, GParamSpec *pspec )
{
#ifdef DEBUG
	printf("G_TYPE_BOXED for property \"%s\" in metadata_apply\n");
#endif /* DEBUG */

	/* do nothing */

	return;
}

void
metadata_util_apply_input( GtkWidget *t, VipsImage *image, char* field )
{
	GValue value = { 0 };
	GParamSpec *pspec;
	GType type;
	VipsArgumentClass *arg_class;
	VipsArgumentInstance *arg_instance;

	vips_image_get( image, field, &value );
	if( vips_object_get_argument( VIPS_OBJECT( image ), field,
		&pspec, &arg_class, &arg_instance ) )
		pspec = NULL;

	type = G_VALUE_TYPE( &value );

	if ( strstr( "thumbnail", field ) ) {
		/* do nothing */
	} else if ( type == VIPS_TYPE_REF_STRING )
		metadata_util_apply_string_input( t, image, field, pspec );
	else if ( pspec && G_IS_PARAM_SPEC_ENUM( pspec ) )
		metadata_util_apply_enum_input( t, image, field, pspec );
	else switch( type ) {
	case G_TYPE_STRING:
		metadata_util_apply_string_input( t, image, field, pspec );
		break;
	case G_TYPE_BOOLEAN:
		metadata_util_apply_boolean_input( t, image, field, pspec );
		break;
	case G_TYPE_ENUM:
		metadata_util_apply_enum_input( t, image, field, pspec );
		break;
	case G_TYPE_INT:
	case G_TYPE_INT64:
	case G_TYPE_UINT:
	case G_TYPE_UINT64:
	case G_TYPE_LONG:
	case G_TYPE_ULONG:
	case G_TYPE_FLAGS:
		metadata_util_apply_int_input( t, image, field, pspec );
		break;
	case G_TYPE_FLOAT:
	case G_TYPE_DOUBLE:
		metadata_util_apply_double_input( t, image, field, pspec );
		break;
	case G_TYPE_BOXED:
		metadata_util_apply_boxed_input( t, image, field, pspec );
		break;
	default:
#ifdef DEBUG
		printf("Type of property \"%s\" unknown.", field);
#endif /* DEBUG */
		/* do nothing */
	} /* end switch( type ) */
}
