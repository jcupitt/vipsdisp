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
	VipsObjectClass *oclass;
	GType otype;
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

	/* Get the value of the given field from the image.
	 * Check the GType of @value, and select the appropriate user input
         * widget. Initialize the widget with @value.
	 * TODO
 	 */
	if( vips_object_get_argument( VIPS_OBJECT( image ), field_name,
			&pspec, &argument_class, &argument_instance ) ) {
		vips_image_get( image, field_name, &value );
		vips_error_clear();
		pspec = NULL;
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
	if( !pspec ) {
		GType type = G_VALUE_TYPE( &value );
		if ( type == G_TYPE_STRING ) {
			char* string_value = g_strdup( g_value_get_string( &value ) );
			GtkEntryBuffer* buffer =
				gtk_entry_buffer_new( string_value, -1 );

			t = gtk_text_new_with_buffer( buffer );
		} else if ( type == G_TYPE_ENUM ) {
			t = create_spin_button( -G_MAXINT + 1, G_MAXINT, 1,
					g_value_get_int( &value ), FALSE );
		} else if ( type == G_TYPE_INT ) {
			t = create_spin_button( -G_MAXINT + 1, G_MAXINT, 1,
					g_value_get_int( &value ), FALSE );
		} else if ( type == G_TYPE_UINT ) {
			t = create_spin_button( 0, G_MAXUINT, 1,
					g_value_get_int( &value ), FALSE );
		} else if ( type == G_TYPE_INT64 ) {
			t = create_spin_button( -G_MAXINT64 + 1, G_MAXINT64, 1,
					g_value_get_int( &value ), FALSE );
		} else if ( type == G_TYPE_UINT64 ) {
			t = create_spin_button( 0, G_MAXUINT64, 1,
					g_value_get_int( &value ), FALSE );
		} else if ( type == G_TYPE_LONG ) {
			t = create_spin_button( -G_MAXINT + 1, G_MAXINT, 1,
					g_value_get_int( &value ), FALSE );
		} else if ( type == G_TYPE_ULONG ) {
			t = create_spin_button( 0, G_MAXULONG, 1,
					g_value_get_int( &value ), FALSE );
		} else if ( type == G_TYPE_FLOAT ) {
			t = create_spin_button( -G_MAXFLOAT + 1, G_MAXFLOAT, 1,
					g_value_get_int( &value ), FALSE );
		} else if ( type == G_TYPE_DOUBLE ) {
			g_print( "double %s\n", field_name );
			t = create_spin_button( -G_MAXDOUBLE + 1, G_MAXDOUBLE, 1,
					g_value_get_int( &value ), FALSE );
		} else if ( type == G_TYPE_FLAGS ) {
			t = create_spin_button( -G_MAXINT + 1, G_MAXINT, 1,
					g_value_get_int( &value ), FALSE );
		} else if ( type == G_TYPE_BOOLEAN ) {
			t = gtk_check_button_new();
			gtk_check_button_set_active( GTK_CHECK_BUTTON( t ),
				g_value_get_boolean( &value ) );
		} else if ( type == G_TYPE_BOXED ) {
			if( g_type_is_a( otype, VIPS_TYPE_ARRAY_INT ) ) {
				/* No default values exist for ParamSpecBoxed, so make
				 * some up for now.
				 */
				t = create_spin_button( 0, 1000, 1, 0, FALSE );
			}
			else if( g_type_is_a( otype, VIPS_TYPE_ARRAY_DOUBLE ) ) {
				/* No default values exist for ParamSpecBoxed, so make
				 * some up for now.
				 */
				t = create_spin_button( 0, 1000, 1, .1, FALSE );
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
		const char *s;
		g_object_get( image, field_name, &s, NULL );
		GtkEntryBuffer* buffer =
			gtk_entry_buffer_new( g_strdup( s ), -1 );
		t = gtk_text_new_with_buffer( buffer );
	}
	else if( G_IS_PARAM_SPEC_BOOLEAN( pspec ) ) {
		t = gtk_check_button_new();
		int d;
		g_object_get( image, field_name, &d, NULL );
		gtk_check_button_set_active( GTK_CHECK_BUTTON( t ), d );
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
		int d;
		g_object_get( image, field_name, &d, NULL );
		gtk_drop_down_set_selected( GTK_DROP_DOWN( t ), d );
	}
	else if( G_IS_PARAM_SPEC_INT64( pspec ) ) {
		GParamSpecInt64 *pspec_int64 = G_PARAM_SPEC_INT64( pspec );
		int d;
		g_object_get( image, field_name, &d, NULL );
		t = create_spin_button( pspec_int64->minimum,
				pspec_int64->maximum, 1, d, FALSE );
	}
	else if( G_IS_PARAM_SPEC_INT( pspec )) {
		GParamSpecInt *pspec_int = G_PARAM_SPEC_INT( pspec );
		int d;
		g_object_get( image, field_name, &d, NULL );
		t = create_spin_button( pspec_int->minimum,
				pspec_int->maximum, 1, d, FALSE );
	}
	else if( G_IS_PARAM_SPEC_UINT64( pspec ) ) {
		GParamSpecUInt64 *pspec_uint64 = G_PARAM_SPEC_UINT64( pspec );
		int d;
		g_object_get( image, field_name, &d, NULL );
		t = create_spin_button( pspec_uint64->minimum,
				pspec_uint64->maximum, 1, d, FALSE );
	}
	else if( G_IS_PARAM_SPEC_DOUBLE( pspec ) ) {
		GParamSpecDouble *pspec_double = G_PARAM_SPEC_DOUBLE( pspec );
		double d;
		g_object_get( image, field_name, &d, NULL );
		t = create_spin_button( pspec_double->minimum,
				pspec_double->maximum, 1, d, FALSE );
	}
	else if( G_IS_PARAM_SPEC_BOXED( pspec ) ) {
		if( g_type_is_a( otype, VIPS_TYPE_ARRAY_INT ) ) {
			/* No default values exist for ParamSpecBoxed, so make
			 * some up for now.
			 */

			t = create_spin_button( 0, 1000, 1, 0, FALSE );
		}
		else if( g_type_is_a( otype, VIPS_TYPE_ARRAY_DOUBLE ) ) {
			/* No default values exist for ParamSpecBoxed, so make
			 * some up for now.
			 */
			t = create_spin_button( 0, 1000, .1, 0, FALSE );
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
	if ( pspec )
		gtk_widget_set_tooltip_text( GTK_WIDGET( box ),
			g_param_spec_get_blurb( pspec ) );

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

