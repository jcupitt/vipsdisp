/* Display an image with gtk3 and libvips. 
 */

#include <stdio.h>
#include <stdlib.h>

#include <gtk/gtk.h>

#include <vips/vips.h>

#include "disp.h"

G_DEFINE_TYPE( Disp, disp, GTK_TYPE_APPLICATION );

static void
disp_finalize( GObject *object )
{
	//Disp *disp = (Disp *) object;

#ifdef DEBUG
	printf( "disp_finalize:\n" ); 
#endif /*DEBUG*/

	G_OBJECT_CLASS( disp_parent_class )->finalize( object );
}

static void
disp_quit_activated( GSimpleAction *action, 
	GVariant *parameter, gpointer user_data )
{
	GApplication *app = G_APPLICATION( user_data );

#ifdef DEBUG
	printf( "disp_quit_activated:\n" ); 
#endif /*DEBUG*/

	g_application_quit( app );
}

static GActionEntry app_entries[] = {
	{ "quit", disp_quit_activated }
};

static void
disp_startup( GApplication *application )
{
	//Disp *disp = (Disp *) application;
	GtkApplication *app = GTK_APPLICATION( application );

	int i;

	struct {
		const gchar *action_and_target;
		const gchar *accelerators[2];
	} accels[] = {
		{ "app.quit", { "<Primary>q", NULL } }
	};

#ifdef DEBUG
	printf( "disp_startup:\n" ); 
#endif /*DEBUG*/

	G_APPLICATION_CLASS( disp_parent_class )->startup( application );

	g_action_map_add_action_entries( G_ACTION_MAP( application ), 
		app_entries, G_N_ELEMENTS( app_entries ), application );

	for( i = 0; i < G_N_ELEMENTS( accels ); i++)
		gtk_application_set_accels_for_action( app, 
			accels[i].action_and_target, accels[i].accelerators );

	const gchar *new_accels[] = { "<Primary>n", "<Primary>t", NULL };
	gtk_application_set_accels_for_action( GTK_APPLICATION( application ), 
		"app.new", new_accels );

}

static void
disp_shutdown( GApplication *application )
{
	//Disp *disp = (Disp *) application;
	GList *windows;

#ifdef DEBUG
	printf( "disp_shutdown:\n" ); 
#endif /*DEBUG*/

	/* Force down all our windows ... this will not happen automatically
	 * on _quit().
	 */
	while( (windows = gtk_application_get_windows( 
		GTK_APPLICATION( application ) )) ) {
		Imageview *imageview = (Imageview *) windows->data;

		gtk_widget_destroy( GTK_WIDGET( imageview ) );
	}

	G_APPLICATION_CLASS( disp_parent_class )->shutdown( application );
}

static void
disp_activate( GApplication *application )
{
#ifdef DEBUG
	printf( "disp_activate:\n" ); 
#endif /*DEBUG*/

	imageview_new( GTK_APPLICATION( application ), NULL );
}

static void
disp_open( GApplication *application, 
	GFile **files, gint n_files, const gchar *hint )
{
	gint i;

#ifdef DEBUG
	printf( "disp_open:\n" ); 
#endif /*DEBUG*/

	for( i = 0; i < n_files; i++ )
		imageview_new( GTK_APPLICATION( application ), files[i] );
}

static void
disp_init( Disp *disp )
{
}

static void
disp_class_init( DispClass *class )
{
	GObjectClass *object_class = G_OBJECT_CLASS( class );
	GApplicationClass *application_class = G_APPLICATION_CLASS( class );

	object_class->finalize = disp_finalize;

	application_class->startup = disp_startup;
	application_class->shutdown = disp_shutdown;
	application_class->activate = disp_activate;
	application_class->open = disp_open;
}

Disp *
disp_new( void )
{
	Disp *disp;

#ifdef DEBUG
	printf( "disp_new:\n" ); 
#endif /*DEBUG*/

	disp = g_object_new( disp_get_type(),
		"application-id", "vips.disp",
		"flags", G_APPLICATION_HANDLES_OPEN,
		"inactivity-timeout", 30000,
		"register-session", TRUE,
		NULL );

	return( disp ); 
}

int
main( int argc, char **argv )
{
	Disp *disp;
	int status;
	const gchar *accels[] = { "F11", NULL };

	if( VIPS_INIT( argv[0] ) )
		vips_error_exit( "unable to start VIPS" );

	vips_leak_set( TRUE );

	/*
	g_log_set_always_fatal( 
		G_LOG_FLAG_RECURSION |
		G_LOG_FLAG_FATAL |
		G_LOG_LEVEL_ERROR |
		G_LOG_LEVEL_CRITICAL |
		G_LOG_LEVEL_WARNING );
	 */

	disp = disp_new();

	gtk_application_set_accels_for_action( GTK_APPLICATION( disp ),
		"win.fullscreen", accels );

	status = g_application_run( G_APPLICATION( disp ), argc, argv );

	g_object_unref( disp );

	return( status );
}
