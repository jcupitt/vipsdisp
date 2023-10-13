#include "vipsdisp.h"

#define MIN3(a, b, c) ((a) < (b) ? ((a) < (c) ? (a) : (c)) : ((b) < (c) ? (b) : (c)))

/* LEVENSHTEIN DISTANCE
 *
 * @s1 and @s2 are character arrays of size @n1 and @n2, which represent two
 * strings. Any null bytes are treated like a normal character, so strings
 * without a terminating null byte are valid input.
 * 
 * The LEVENSHTEIN DISTANCE between @s1 and @s2 is the smallest number of
 * insertions, deletions, or substitutions needed to transform @s1 into @s2.
 *
 * The algorithm dynamically builds a table T[n1+1]][n2+1] where the (x, y)
 * entry is the Levenshtein distance between the prefix of @s1 of size y-1 and
 * the prefix of @s2 of size x-1.
 *
 * This implementation uses only a single @v column, updating it in place, since
 * only the elements at (x-1, y-1) and (x, y-1) are needed to compute the
 * element at (x, y). This is expressed by the recursion relation:
 *
 *	k = (s1[y-1] == s2[x-2] ? 0 : 1)
 *
 * 	T[x][y] = MIN3( T[x-1][y] + 1, T[x][y - 1] + 1, T[x-1][y-1] + k )
 *
 * If we let @t1 hold the previous diagonal value, we can rewrite this
 * in terms of the single column @v:
 *
 *	k = (s1[y-1] == s2[x-2] ? 0 : 1)
 *
 * 	v[y] = MIN3( v[y] + 1, v[y-1] + 1, t1 + k )
 *
 * The irst column of the table T is unused, and the first row is
 * used as the base case for the recursion relation.
 *
 *                         s2
 *
 *     *----l--e--v--e--n--s--h--t--e--i--n--- x
 *     | .  1  2  3  4  5  6  7  8  9  10 11
 *     |
 *     d .  1  2  3  4  5  6  7  8  9  10 11
 *     |
 *     a .  2  2  3  4  5  6  7  8  9  10 11
 *     |
 *     m .  3  3  3  4  5  6  7  8  9  10 11
 * s1  |
 *     e .  4  3  4  3  4  5  6  7  8  9  10
 *     |
 *     r .  5  4  4  4  4  5  6  7  8  9  10
 *     |
 *     a .  6  5  5  5  5  5  6  7  8  9  10
 *     |
 *     u .  7  6  6  6  6  6  6  7  8  9  10
 *     | 
 *
 *     y
 *
 * SYMBOLS
 *
 * @v:	Column vector. Reused and updated in place. The @t1 and @t0 values keep
 *	track of the most recent and second most recent diagonal values.
 * @x:	Row Index.
 * @y:	Column Index.
 * @t0:	Second most recent diagonal value.
 * @t1:	Most recent diagonal value
 * @k:	Test inqeuality in s1[y-1] and s2[x-1], the pair of characters
 * 	corresponding to the table entry T[x][y]. Equals 0 if these characters
 *	are equal, and 1 if they are different.
 *
 * TERMS
 *
 * LD: Levenshtein Distance
 *
 * SOURCES
 *
 * https://en.wikipedia.org/wiki/Levenshtein_distance
 * https://en.wikibooks.org/wiki/Algorithm_Implementation/Strings/Levenshtein_distance#C
 */

/* Compute the Levenshtein Distance (LD) between two strings, @s1 and @s2.
 *
 * @n1:		Character array 1 size
 *
 * @s1:		Character array 1
 *
 * @n2:		Character array 2 size
 *
 * @s2:		Character array 2
 *
 * @v:		Buffer where column of Levenshtein distances are written
 * 		during computation.
 */
guint
glev( guint n1, gchar s1[n1], guint n2, gchar s2[n2], guint v[n1 + 1] ) {
	guint x, y, t0, t1, k;

	// Initialize the column.
	for ( y = 1; y <= n1; y++ )
		v[y] = y;

	// Ignore and don't even bother to initialize the first column. Walk
	// through columns after the first.
	for ( x = 1; x <= n2; x++ ) {
		// The first row (ignoring the first entry) is just the column
		// indices from 1 to n2.
		v[0] = x;

		// The recursion relation defined above and the base case
		// conditions { y = 1, t1 = x - 1 } are used to build the
		// Table T column by column. Only one column @v is needed
		// in memory at a time, and it can be operated on in
		// place, as long as temporary variables @t0 and @t1 are used
		// to keep track of the last diagonal when @v is updated.
		for ( y = 1, t1 = x - 1; y <= n1; y++ ) {
			t0 = v[y];
			k = s1[y - 1] == s2[x - 1] ? 0 : 1;
			v[y] = MIN3( v[y] + 1, v[y - 1] + 1, t1 + k );
			t1 = t0;
		}
	}

	// Return Levenshtein Distance.
	return v[y-1];
}

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

		t = create_spin_button( pspec_int64->minimum,
				pspec_int64->maximum, 1,
				pspec_int64->default_value, FALSE );
	}
	else if( G_IS_PARAM_SPEC_INT( pspec )) {
		GParamSpecInt *pspec_int = G_PARAM_SPEC_INT( pspec );

		t = create_spin_button( pspec_int->minimum,
				pspec_int->maximum, 1,
				pspec_int->default_value, FALSE );
	}
	else if( G_IS_PARAM_SPEC_UINT64( pspec ) ) {
		GParamSpecUInt64 *pspec_uint64 = G_PARAM_SPEC_UINT64( pspec );

		t = create_spin_button( pspec_uint64->minimum,
				pspec_uint64->maximum, 1,
				pspec_uint64->default_value, FALSE );
	}
	else if( G_IS_PARAM_SPEC_DOUBLE( pspec ) ) {
		GParamSpecDouble *pspec_double = G_PARAM_SPEC_DOUBLE( pspec );

		t = create_spin_button( pspec_double->minimum,
				pspec_double->maximum, 1,
				pspec_double->default_value, FALSE );
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
	if ( !use_string )
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

	/* Nest the input widget in two boxes.
	 */
	gtk_box_append( GTK_BOX( box ), t );
	gtk_box_append( GTK_BOX( input_box ), box );

	return input_box;
}

