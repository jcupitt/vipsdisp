/*
#define DEBUG
 */

#include "vipsdisp.h"

struct _SaveOptions
{
	GtkDialog parent_instance;

	VipsOperation *save_operation;

	// a box we can fill with widgets for the save options
	GtkWidget *options_grid;

	// hash property names to the widghet for that property ... we fetch 
	// values from here when we make the saver
	GHashTable *value_widgets;
};

struct _SaveOptionsClass
{
	GtkDialogClass parent_class;
};

G_DEFINE_TYPE( SaveOptions, save_options, GTK_TYPE_DIALOG );

static void
save_options_dispose( GObject *object )
{
	SaveOptions *options = VIPSDISP_SAVE_OPTIONS( object );

	printf( "save_options_dispose:\n" ); 

	VIPS_UNREF( options->save_operation );
	VIPS_FREEF( g_hash_table_destroy, options->value_widgets );

	G_OBJECT_CLASS( save_options_parent_class )->dispose( object );
}

static void
save_options_response( GtkWidget *dialog, int response, void *user_data )
{
	SaveOptions *options = VIPSDISP_SAVE_OPTIONS( dialog );

	printf( "save_options_response: %d\n", response );

	if( response == GTK_RESPONSE_OK ) { 
		// walk save_operation, fetch matching widget state, set
		// options 

		// then this will trigger the save
		printf( "save_options_response: saving ...\n" );
		if( vips_object_build( VIPS_OBJECT( options->save_operation ) ) ) {
			// maybe display an error popup? I'm not sure
			// we could also use gtk_dialog_response() to send an
			// error response code back to our caller
		}
	}
}

/* This function is used by:
 *
 * 	save_options_build_content_box_argument_map_fn_helper
 *
 * to process one property of the save operation. The property type and name
 * are used to create a labelled user input element for that property.
 */
static void
save_options_add_option( SaveOptions *options,
	GParamSpec *pspec, VipsArgumentClass *argument_class, int *row )
{
	/* Get the nickname of the property of the save operation currently
	 * being processed. For VIPS, this is the user-facing name of the
	 * property.
	 */
	const gchar *name = g_param_spec_get_nick( pspec );

	GType otype = G_PARAM_SPEC_VALUE_TYPE( pspec );

	GtkWidget *t, *label;

	/* For now, skip properties of type VipsImage or VipsObject.
	 */
	if( g_type_is_a( otype, VIPS_TYPE_IMAGE ) ||
		g_type_is_a( otype, VIPS_TYPE_OBJECT ) )
		return;

	/* Make a widget for this value. The widget
	 * chosen depends on the type of the property. Set the initial value of
	 * the user input widget to the default value for the property.
	 */
	if( G_IS_PARAM_SPEC_STRING( pspec ) ) {
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
		int n_values = pspec_enum->enum_class->n_values - 1;
		const char **property_nicknames =
			g_malloc( (n_values + 1) * sizeof( char * ) );

		for( int i = 0; i < n_values; ++i )
			property_nicknames[i] =
				pspec_enum->enum_class->values[i].value_nick;
		property_nicknames[n_values] = NULL;

		t = gtk_drop_down_new_from_strings( property_nicknames );
		gtk_drop_down_set_selected( GTK_DROP_DOWN( t ),
			pspec_enum->default_value );
		// FIXME ... free property_nicknames
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
			return;
		}
		else {
			/* Ignore parameters of unrecognized type for now.
			 */
			return;
		}
	}
	else {
		printf( "Unknown type for property \"%s\"\n", name );
		return;
	}

	printf( "save_options_add_option: adding widget for %s\n", name );

	/* Label for setting, with a tooltip.
	 */
	label = gtk_label_new( name );
	gtk_widget_set_halign( label, GTK_ALIGN_START );
	gtk_widget_set_tooltip_text( GTK_WIDGET( label ),
		g_param_spec_get_blurb( pspec ) );
	gtk_grid_attach( GTK_GRID( options->options_grid ), label, 
		0, *row, 1, 1 );

	g_object_set( t, "hexpand", true, NULL );
	gtk_grid_attach( GTK_GRID( options->options_grid ), t, 
		1, *row, 1, 1 );
	gtk_widget_set_tooltip_text( GTK_WIDGET( label ),
		g_param_spec_get_blurb( pspec ) );

	g_hash_table_insert( options->value_widgets, (gpointer) name, t );

	*row += 1;
}

/* This is the function used by save_options_build_content_box to process a
 * single property of the save operation.
 *
 * See also save_options_build_content_box_argument_map_fn_helper.
 */
static void *
save_options_build_content_box_argument_map_fn( VipsObject *operation,
	GParamSpec *pspec, VipsArgumentClass *argument_class,
	VipsArgumentInstance *argument_instance, void *a, void *b )
{
	VipsArgumentFlags flags = argument_class->flags;
	SaveOptions *options = (SaveOptions *) a;
	int *row = (int *) b;

	/* Include arguments listed in the constructor.
	 *
	 * Exclude required (we've set these already) or deprecated arguments.
	 */
	if ( !(flags & VIPS_ARGUMENT_DEPRECATED) &&
		(flags & VIPS_ARGUMENT_CONSTRUCT) &&
		!(flags & VIPS_ARGUMENT_REQUIRED) )
		save_options_add_option( options, pspec, argument_class, row );

	return NULL;
}

static void
save_options_init( SaveOptions *options )
{
	printf( "save_options_init:\n" );

	gtk_widget_init_template( GTK_WIDGET( options ) );

	options->value_widgets = g_hash_table_new( g_str_hash, g_str_equal );

	g_signal_connect_object( options, "response", 
		G_CALLBACK( save_options_response ), options, 0 );
}

#define BIND( field ) \
	gtk_widget_class_bind_template_child( GTK_WIDGET_CLASS( class ), \
		SaveOptions, field );

static void
save_options_class_init( SaveOptionsClass *class )
{
	GObjectClass *gobject_class = G_OBJECT_CLASS( class );
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS( class );

	gobject_class->dispose = save_options_dispose;

	gtk_widget_class_set_template_from_resource( widget_class,
		APP_PATH "/saveoptions.ui");

	BIND( options_grid );
}

SaveOptions *
save_options_new( GtkWindow *parent_window, const char *title,
	VipsImage *image, const char *filename )
{
	const char *saver;
	SaveOptions *options;
	int row;

	if( !(saver = vips_foreign_find_save( filename )) ) {
		// this will display the error back in the main window, which
		// is a bit odd
		//
		// maybe display the save options, but put the error
		// message in the dialog?
		//
		// could put progress in the dialog too
		return( NULL );
	}

	options = g_object_new( SAVE_OPTIONS_TYPE, 
		// we have to set this here, not in the ui file, for some reason
		"use-header-bar", true, 
		"transient-for", parent_window, 
		"title", title,
		NULL );

	options->save_operation = vips_operation_new( saver );
	g_object_set( options->save_operation,
		"in", image,
		"filename", filename,
		NULL );

	row = 0;
	vips_argument_map( VIPS_OBJECT( options->save_operation ),
		save_options_build_content_box_argument_map_fn,
		options, &row );

        //<property name="max-content-height">500</property>
        //<property name="min-content-height">600</property>

	return( options );
}
