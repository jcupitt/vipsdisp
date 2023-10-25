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

/* Use introspection on VipsImage to create a UI input elements for each
 * property.
 */
GtkWidget *
create_input( VipsImage *image, char* field_name )
{
	GtkWidget *input_box, *t, *box;
	GType type;
	GParamSpec *pspec;
	VipsArgumentClass *argument_class;
	VipsArgumentInstance *argument_instance;

	box = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );

	/* It is crucial to zero the GValue whose address we pass to
	 * vips_image_get. Otherwise, we will get runtime errors.
	 */
	GValue value = { 0 };

	input_box = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );
	gtk_widget_set_margin_start( input_box, 20 );

	vips_image_get( image, field_name, &value );
	if ( vips_object_get_argument( VIPS_OBJECT( image ), field_name,
			&pspec, &argument_class, &argument_instance ) ) {
		pspec = NULL;
	}

	/* Add a user input widget for this property to the input_box. The widget
	 * chosen depends on the type of the property. Set the initial value of
	 * the user input widget to the default value for the property.
	 */
	type = G_VALUE_TYPE( &value );
	if ( type == G_TYPE_STRING || (type == VIPS_TYPE_REF_STRING
				&& !strstr( field_name, "thumbnail" )) ) {
		const char *string_value;
		if ( !strcmp( field_name, "filename" )) {
			g_object_get( image, field_name, &string_value, NULL );
		} else {
			vips_image_get_string( image, field_name, &string_value );
		}
		string_value = g_strdup( string_value );
		GtkEntryBuffer* buffer =
			gtk_entry_buffer_new( string_value, -1 );
		t = gtk_text_new();
		gtk_text_set_buffer( GTK_TEXT( t ), buffer );
	} else if ( type == G_TYPE_BOOLEAN ) {
		int d;
		vips_image_get_int( image, field_name, &d );
		t = gtk_check_button_new();
		gtk_check_button_set_active( GTK_CHECK_BUTTON( t ),
			d );
	} else if ( type == G_TYPE_ENUM || G_IS_PARAM_SPEC_ENUM( pspec ) ) {
		if ( pspec ) {
			GParamSpecEnum *pspec_enum = G_PARAM_SPEC_ENUM( pspec );
			const char **property_nicknames =
				g_malloc( (pspec_enum->enum_class->n_values + 1) * sizeof( char * ) );

			for( int i = 0; i < pspec_enum->enum_class->n_values; ++i ) {
				property_nicknames[i] =
					pspec_enum->enum_class->values[i].value_nick;
			}
			property_nicknames[pspec_enum->enum_class->n_values] = NULL;
			t = gtk_drop_down_new_from_strings( property_nicknames );
			int d;
			g_object_get( image, field_name, &d, NULL );
			gtk_drop_down_set_selected( GTK_DROP_DOWN( t ), d );
		} else { 
			int d;
			vips_image_get_int( image, field_name, &d );
			t = create_spin_button( -G_MAXINT + 1, G_MAXINT, 1,
					d, FALSE );
		}
	} else if ( type == G_TYPE_INT || type == G_TYPE_INT64 ||
			type == G_TYPE_UINT || type == G_TYPE_UINT64 ||
			type == G_TYPE_LONG || type == G_TYPE_ULONG ||
			type == G_TYPE_FLAGS ) {
		int d;
		vips_image_get_int( image, field_name, &d );
		t = create_spin_button( -G_MAXINT + 1, G_MAXINT, 1,
				d, FALSE );
	} else if ( type == G_TYPE_FLOAT || type == G_TYPE_DOUBLE ) {
		double d;
		vips_image_get_double( image, field_name, &d );
		t = create_spin_button( -G_MAXDOUBLE + 1, G_MAXDOUBLE, 1,
				d, FALSE );
	} else if ( type == G_TYPE_BOXED ) {
		//printf("G_TYPE_BOXED for property \"%s\" in create_input\n");
		t = gtk_label_new( "" );
	} else {
		//printf("Unknown type for property \"%s\" in create_input\n", field_name);
		t = gtk_label_new( "" );
	}

	/* Create a box to contain the user input widget "t", and add a tooltip
	 * to the box. The tooltip contains the "blurb" for the property.
	 * Make the user input widget "t" fill the box horizontally.
	 * Append that container box to the "input_box".
	 */
	if ( pspec ) {
		gtk_widget_set_tooltip_text( GTK_WIDGET( box ),
				g_param_spec_get_blurb( pspec ) );
	}

	/* Fill out the horizontal space.
	 */
	gtk_widget_set_hexpand( t, TRUE );
	gtk_widget_set_halign( t, GTK_ALIGN_FILL );
	gtk_widget_set_hexpand( box, TRUE );
	gtk_widget_set_halign( box, GTK_ALIGN_FILL );
	gtk_widget_set_hexpand( input_box, TRUE );
	gtk_widget_set_halign( input_box, GTK_ALIGN_FILL );


	/* Style the input widget.
	 */
	gtk_widget_add_css_class( t, "metadata-input" );

	/* Nest the input widget in two boxes.
	 */
	gtk_box_append( GTK_BOX( box ), t );
	gtk_box_append( GTK_BOX( input_box ), box );

	return input_box;
}

