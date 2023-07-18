/*
#define DEBUG
 */

#include "vipsdisp.h"

struct _SaveOptions
{
	GtkDialog parent_instance;

	VipsImage *image;
	VipsOperation *save_operation;

	// a box we can fill with widgets for the save options
	GtkWidget *options_grid;

	// the progress and error indicators we show
	GtkWidget *progress_bar;
	GtkWidget *progress;
	GtkWidget *progress_cancel;
	GtkWidget *error_bar;
	GtkWidget *error_label;

	// hash property names to the widghet for that property ... we fetch 
	// values from here when we make the saver
	GHashTable *value_widgets;

	/* Throttle progress bar updates to a few per second with this.
	 */
	GTimer *progress_timer;
	double last_progress_time;
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

	VIPS_UNREF( options->image );
	VIPS_UNREF( options->save_operation );
	VIPS_FREEF( g_timer_destroy, options->progress_timer );
	VIPS_FREEF( g_hash_table_destroy, options->value_widgets );

	G_OBJECT_CLASS( save_options_parent_class )->dispose( object );
}

static void
save_options_error( SaveOptions *options )
{
	char *err;
	int i;

	/* Remove any trailing \n.
	 */
	err = vips_error_buffer_copy();
	for( i = strlen( err ); i > 0 && err[i - 1] == '\n'; i-- )
		err[i - 1] = '\0';
	gtk_label_set_text( GTK_LABEL( options->error_label ), err );
	g_free( err );

	gtk_info_bar_set_revealed( GTK_INFO_BAR( options->error_bar ), TRUE );
}

static void
save_options_error_hide( SaveOptions *options )
{
	gtk_info_bar_set_revealed( GTK_INFO_BAR( options->error_bar ), FALSE );
}

static void
save_options_error_response( GtkWidget *button, int response, 
	SaveOptions *options )
{
	save_options_error_hide( options );
}

static void
save_options_preeval( VipsImage *image, 
	VipsProgress *progress, SaveOptions *options )
{
	gtk_action_bar_set_revealed( GTK_ACTION_BAR( options->progress_bar ), 
		TRUE );
}

static void
save_options_eval( VipsImage *image, 
	VipsProgress *progress, SaveOptions *options )
{
	double time_now;
	char str[256];
	VipsBuf buf = VIPS_BUF_STATIC( str );

	/* We can be ^Q'd during load. This is NULLed in _dispose.
	 */
	if( !options->progress_timer )
		return;

	time_now = g_timer_elapsed( options->progress_timer, NULL );

	/* Throttle to 10Hz.
	 */
	if( time_now - options->last_progress_time < 0.1 )
		return;
	options->last_progress_time = time_now;

	vips_buf_appendf( &buf, "%d%% complete, %d seconds to go",
		progress->percent, progress->eta );
	gtk_progress_bar_set_text( GTK_PROGRESS_BAR( options->progress ),
		vips_buf_all( &buf ) );

	gtk_progress_bar_set_fraction( GTK_PROGRESS_BAR( options->progress ),
		progress->percent / 100.0 );

	// run the main loop for a while
	process_events();
}

static void
save_options_posteval( VipsImage *image, 
	VipsProgress *progress, SaveOptions *options )
{
	gtk_action_bar_set_revealed( GTK_ACTION_BAR( options->progress_bar ), 
		FALSE );
}

static void
save_options_fetch_option( SaveOptions *options, GParamSpec *pspec ) 
{
	const gchar *name = g_param_spec_get_name( pspec );
	GType otype = G_PARAM_SPEC_VALUE_TYPE( pspec );

	GtkWidget *t;

	if( !(t = g_hash_table_lookup( options->value_widgets, name )) )
		return;

	/* Fetch the value from the widget.
	 */
	if( G_IS_PARAM_SPEC_STRING( pspec ) ) {
		GParamSpecString *pspec_string = G_PARAM_SPEC_STRING( pspec );
		GtkEntryBuffer *buffer = gtk_entry_get_buffer( GTK_ENTRY( t ) );
		const char *value = gtk_entry_buffer_get_text( buffer );

		/* Only if the value has changed.
		 */
		if( (!pspec_string->default_value &&
			strcmp( value, "" ) != 0) ||
		    (pspec_string->default_value &&
			strcmp( value, pspec_string->default_value ) != 0) ) {

			g_object_set( options->save_operation,
				name, value,
				NULL );
		}
	}
	else if( G_IS_PARAM_SPEC_BOOLEAN( pspec ) ) {
		gboolean value = 
			gtk_check_button_get_active( GTK_CHECK_BUTTON( t ) );

		g_object_set( options->save_operation, 
			name, value,
			NULL );
	}
	else if( G_IS_PARAM_SPEC_ENUM( pspec ) ) {
		GParamSpecEnum *pspec_enum = G_PARAM_SPEC_ENUM( pspec );
		int index = gtk_drop_down_get_selected( GTK_DROP_DOWN( t ) );
		int value = pspec_enum->enum_class->values[index].value;

		g_object_set( options->save_operation,
			name, value,
			NULL );
	}
	else if( G_IS_PARAM_SPEC_INT64( pspec ) ) {
		gint64 value = 
			gtk_spin_button_get_value( GTK_SPIN_BUTTON( t ) );

		g_object_set( options->save_operation,
			name, value,
			NULL );
	}
	else if( G_IS_PARAM_SPEC_INT( pspec )) {
		int value = gtk_spin_button_get_value( GTK_SPIN_BUTTON( t ) );

		g_object_set( options->save_operation,
			name, value,
			NULL );
	}
	else if( G_IS_PARAM_SPEC_UINT64( pspec ) ) {
		guint64 value = 
			gtk_spin_button_get_value( GTK_SPIN_BUTTON( t ) );

		g_object_set( options->save_operation,
			name, value,
			NULL );
	}
	else if( G_IS_PARAM_SPEC_DOUBLE( pspec ) ) {
		gdouble value = 
			gtk_spin_button_get_value( GTK_SPIN_BUTTON( t ) );

		g_object_set( options->save_operation,
			name, value,
			NULL );
	}
	else if( G_IS_PARAM_SPEC_BOXED( pspec ) ) {
		if( g_type_is_a( otype, VIPS_TYPE_ARRAY_INT ) ) {
			int value;
			VipsArrayInt *array;

			/* For now just pretend every array-type parameter has
			 * one element.
			 * TODO handle arrays with two or more elements
			 */
			value = gtk_spin_button_get_value( GTK_SPIN_BUTTON( t ) );
			array = vips_array_int_newv( 1, value );

			g_object_set( options->save_operation,
				name, array,
				NULL );

			vips_area_unref( VIPS_AREA( array ) );
		}
		else if( g_type_is_a( otype, VIPS_TYPE_ARRAY_DOUBLE ) ) {
			gdouble value;
			VipsArrayDouble *array;
			value = gtk_spin_button_get_value( GTK_SPIN_BUTTON( t ) );

			/* For now just pretend every array-type parameter has
			 * one element.
			 * TODO handle arrays with two or more elements
			 */
			array = vips_array_double_newv( 1, value );
			g_object_set( options->save_operation,
				name, array,
				NULL );

			vips_area_unref( VIPS_AREA( array ) );
		}
	}
}

static void *
save_options_response_map_fn( VipsObject *operation,
	GParamSpec *pspec, VipsArgumentClass *argument_class,
	VipsArgumentInstance *argument_instance, void *a, void *b )
{
	VipsArgumentFlags flags = argument_class->flags;
	SaveOptions *options = (SaveOptions *) a;

	/* Include arguments listed in the constructor.
	 *
	 * Exclude required (we've set these already) or deprecated arguments.
	 */
	if ( !(flags & VIPS_ARGUMENT_DEPRECATED) &&
		(flags & VIPS_ARGUMENT_CONSTRUCT) &&
		!(flags & VIPS_ARGUMENT_REQUIRED) )
		save_options_fetch_option( options, pspec );

	return NULL;
}

static void
save_options_response( GtkWidget *dialog, int response, void *user_data )
{
	SaveOptions *options = VIPSDISP_SAVE_OPTIONS( dialog );

	if( response == GTK_RESPONSE_OK ) {
		vips_argument_map( VIPS_OBJECT( options->save_operation ),
			save_options_response_map_fn,
			options, NULL );

		// this will trigger the save and loop while we write ... the
		// UI will stay live thanks to event processing in the eval
		// handler
		if( vips_cache_operation_buildp( &options->save_operation ) ) 
			save_options_error( options );
		else 
			// everything worked, we can post success back to
			// our caller
			gtk_dialog_response( GTK_DIALOG( dialog ), 
				GTK_RESPONSE_ACCEPT );
	}
}

static void
save_options_cancel_clicked( GtkWidget *button, SaveOptions *options )
{
	vips_image_set_kill( options->image, TRUE );
}

static void
save_options_init( SaveOptions *options )
{
	gtk_widget_init_template( GTK_WIDGET( options ) );

	g_signal_connect_object( options->progress_cancel, "clicked", 
		G_CALLBACK( save_options_cancel_clicked ), options, 0 );

	g_signal_connect_object( options->error_bar, "response", 
		G_CALLBACK( save_options_error_response ), options, 0 );

	options->value_widgets = g_hash_table_new( g_str_hash, g_str_equal );

	options->progress_timer = g_timer_new();

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

	BIND( progress_bar );
	BIND( progress );
	BIND( progress_cancel );
	BIND( error_bar );
	BIND( error_label );
	BIND( options_grid );

}

/* This function is used by:
 *
 * 	save_options_build_content_box_argument_map_fn_helper
 *
 * to process one property of the save operation. The property type and name
 * are used to create a labelled user input element for that property.
 */
static void
save_options_add_option( SaveOptions *options, GParamSpec *pspec, int *row )
{
	const gchar *name = g_param_spec_get_name( pspec );
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
		GtkEntryBuffer *buffer =
			gtk_entry_buffer_new( pspec_string->default_value, -1 );

		t = gtk_entry_new_with_buffer( buffer );
	}
	else if( G_IS_PARAM_SPEC_BOOLEAN( pspec ) ) {
		GParamSpecBoolean *pspec_boolean = 
			G_PARAM_SPEC_BOOLEAN( pspec );

		t = gtk_check_button_new();
		gtk_check_button_set_active( GTK_CHECK_BUTTON( t ),
			pspec_boolean->default_value );
	}
	else if( G_IS_PARAM_SPEC_ENUM( pspec ) ) {
		GParamSpecEnum *pspec_enum = G_PARAM_SPEC_ENUM( pspec );
		int n_values = pspec_enum->enum_class->n_values - 1;
		const char **nicknames = 
			VIPS_ARRAY( NULL, n_values + 1, const char * );

		for( int i = 0; i < n_values; ++i )
			nicknames[i] =
				pspec_enum->enum_class->values[i].value_nick;
		nicknames[n_values] = NULL;

		t = gtk_drop_down_new_from_strings( nicknames );
		gtk_drop_down_set_selected( GTK_DROP_DOWN( t ),
			pspec_enum->default_value );
		
		g_free( nicknames );
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

	/* Label for setting, with a tooltip. The nick is the i18n name.
	 */
	label = gtk_label_new( g_param_spec_get_nick( pspec ) );
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
save_options_add_options_fn( VipsObject *operation,
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
		save_options_add_option( options, pspec, row );

	return NULL;
}

SaveOptions *
save_options_new( GtkWindow *parent_window, 
	VipsImage *image, const char *filename )
{
	const char *saver;
	SaveOptions *options;

	char *base = g_path_get_basename( filename );
	char *title = g_strdup_printf( "Save image to \"%s\"", base );
	options = g_object_new( SAVE_OPTIONS_TYPE, 
		// we have to set this here, not in the ui file, for some reason
		"use-header-bar", true, 
		"transient-for", parent_window, 
		"title", title,
		NULL );
	g_free( title );
	g_free( base );

	options->image = image;
	g_object_ref( image );

	if( options->image ) { 
		vips_image_set_progress( options->image, TRUE ); 
		g_signal_connect_object( options->image, "preeval", 
			G_CALLBACK( save_options_preeval ), options, 0 );
		g_signal_connect_object( options->image, "eval", 
			G_CALLBACK( save_options_eval ), options, 0 );
		g_signal_connect_object( options->image, "posteval", 
			G_CALLBACK( save_options_posteval ), options, 0 );
	}

	if( !(saver = vips_foreign_find_save( filename )) ) 
		save_options_error( options );

	if( saver && options->image ) {
		int row;

		options->save_operation = vips_operation_new( saver );
		g_object_set( options->save_operation,
			"in", options->image,
			"filename", filename,
			NULL );

		row = 0;
		vips_argument_map( VIPS_OBJECT( options->save_operation ),
			save_options_add_options_fn, options, &row );
	}

	return( options );
}
