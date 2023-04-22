#include "vipsdisp.h"

struct _VipsdispApp
{
	GtkApplication parent;
};

G_DEFINE_TYPE( VipsdispApp, vipsdisp_app, GTK_TYPE_APPLICATION );

static void
vipsdisp_app_init( VipsdispApp *app )
{
}

static void
vipsdisp_app_activate( GApplication *app )
{
	ImageWindow *win;

	win = image_window_new( VIPSDISP_APP( app ) );
	gtk_window_present( GTK_WINDOW( win ) );
}

static void
vipsdisp_app_quit_activated( GSimpleAction *action,
	GVariant *parameter, gpointer app )
{
	g_application_quit( G_APPLICATION( app ) );
}

static void
vipsdisp_app_new_activated( GSimpleAction *action,
	GVariant *parameter, gpointer user_data )
{
	vipsdisp_app_activate( G_APPLICATION( user_data ) ); 
}

static ImageWindow *
vipsdisp_app_win( VipsdispApp *app )
{
	GList *windows = gtk_application_get_windows( GTK_APPLICATION( app ) );

	if( windows )
		return( VIPSDISP_IMAGE_WINDOW( windows->data ) );
	else 
		return( NULL ); 
}

static void
vipsdisp_app_about_activated( GSimpleAction *action,
	GVariant *parameter, gpointer user_data )
{
	VipsdispApp *app = VIPSDISP_APP( user_data );
	ImageWindow *win = vipsdisp_app_win( app );

	static const char *authors[] = {
		"John Cupitt",
		NULL
	};

#ifdef DEBUG
	printf( "vipsdisp_app_about_activated:\n" );
#endif /*DEBUG*/

	gtk_show_about_dialog( win ? GTK_WINDOW( win ) : NULL, 
		"program-name", PACKAGE,
		"logo-icon-name", APPLICATION_ID,
		"title", _( "About vipsdisp" ),
		"authors", authors,
		"version", VERSION,
		"comments", _( "An image viewer for very large images" ),
		"license-type", GTK_LICENSE_MIT_X11,
		"website-label", "Visit vipsdisp on github",
		"website", "https://github.com/jcupitt/vipsdisp",
		NULL );
}

static GActionEntry app_entries[] =
{
	{ "quit", vipsdisp_app_quit_activated },
	{ "new", vipsdisp_app_new_activated },
	{ "about", vipsdisp_app_about_activated },
};

static void
vipsdisp_app_startup( GApplication *app )
{
	int i;
	GtkSettings *settings;

	struct {
		const gchar *action_and_target;
		const gchar *accelerators[2];
	} accels[] = {
		{ "app.quit", { "<Primary>q", NULL } },
		{ "app.new", { "<Primary>n", NULL } },

		{ "win.duplicate", { "<Primary>d", NULL } },
		{ "win.close", { "<Primary>w", NULL } },
		{ "win.replace", { "<Primary>o", NULL } },
		{ "win.prev", { "<primary>comma", NULL } },
		{ "win.next", { "<primary>period", NULL } },
		{ "win.fullscreen", { "F11", NULL } },
	};

	G_APPLICATION_CLASS( vipsdisp_app_parent_class )->startup( app );

	/* Image display programs are supposed to default to a dark theme,
	 * according to the HIG.
	 */
	settings = gtk_settings_get_default(); 
	g_object_set( settings, 
		"gtk-application-prefer-dark-theme", TRUE,
		NULL );

	/* Build our classes.
	 */
	IMAGEDISPLAY_TYPE;
	DISPLAYBAR_TYPE;
	TSLIDER_TYPE;
	INFOBAR_TYPE;

	g_action_map_add_action_entries( G_ACTION_MAP( app ),
		app_entries, G_N_ELEMENTS( app_entries ),
		app );

	for( i = 0; i < G_N_ELEMENTS( accels ); i++)
		gtk_application_set_accels_for_action( GTK_APPLICATION( app ),
			accels[i].action_and_target, accels[i].accelerators );
}

static void
vipsdisp_app_open( GApplication *app, 
	GFile **files, int n_files, const char *hint )
{
	int i;

	for( i = 0; i < n_files; i++ ) {
		ImageWindow *win = image_window_new( VIPSDISP_APP( app ) );

		image_window_open( win, files[i] );
		gtk_window_present( GTK_WINDOW( win ) );
	}
}

static void
vipsdisp_app_shutdown( GApplication *app )
{
	ImageWindow *win; 

#ifdef DEBUG
	printf( "vipsdisp_app_shutdown:\n" );
#endif /*DEBUG*/

	/* Force down all our windows ... this will not happen automatically
	 * on _quit().
	 */
	while( (win = vipsdisp_app_win( VIPSDISP_APP( app ) )) ) 
		gtk_window_destroy( GTK_WINDOW( win ) );

	G_APPLICATION_CLASS( vipsdisp_app_parent_class )->shutdown( app );
}

static void
vipsdisp_app_class_init( VipsdispAppClass *class )
{
	G_APPLICATION_CLASS( class )->startup = vipsdisp_app_startup;
	G_APPLICATION_CLASS( class )->activate = vipsdisp_app_activate;
	G_APPLICATION_CLASS( class )->open = vipsdisp_app_open;
	G_APPLICATION_CLASS( class )->shutdown = vipsdisp_app_shutdown;
}

VipsdispApp *
vipsdisp_app_new( void )
{
	return( g_object_new( VIPSDISP_APP_TYPE, 
		"application-id", APPLICATION_ID,
		"flags", G_APPLICATION_HANDLES_OPEN,
		"inactivity-timeout", 3000,
		"register-session", TRUE,
		NULL ) );
}
