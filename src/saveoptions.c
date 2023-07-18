/*
#define DEBUG
 */

#include "vipsdisp.h"

struct _SaveOptions
{
	GtkDialog parent_instance;

	VipsOperation *save_operation;

	// a box we can fill with widgets for the save options
	GtkWidget *options_box;
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

static void
save_options_init( SaveOptions *options )
{
	printf( "save_options_init:\n" );

	gtk_widget_init_template( GTK_WIDGET( options ) );

	// walk save_operation and fill options_box with widgets

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

	BIND(options_box);
}

SaveOptions *
save_options_new( GtkWindow *parent_window, const char *title,
	VipsImage *image, const char *filename )
{
	const char *saver;
	SaveOptions *options;

	if( !(saver = vips_foreign_find_save( filename )) ) {
		// this will display the error back in the main window, which
		// is a bit odd
		//
		// maybe display the save options, but put the error
		// message in the dialog?
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

	return( options );
}
