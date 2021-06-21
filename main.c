/*
 */
#define DEBUG

#include "vipsdisp.h"

int
main( int argc, char **argv )
{
	VipsdispApp *app;
	int status;

	if( VIPS_INIT( argv[0] ) )
		vips_error_exit( "unable to start libvips" );
	vips_leak_set( TRUE );

#ifdef DEBUG
	g_log_set_always_fatal( 
		G_LOG_FLAG_RECURSION |
		G_LOG_FLAG_FATAL |
		G_LOG_LEVEL_ERROR |
		G_LOG_LEVEL_CRITICAL |
		// G_LOG_LEVEL_WARNING  |
		0 );
#endif /*DEBUG*/

	app = vipsdisp_app_new();

	status = g_application_run( G_APPLICATION( app ), argc, argv );

	printf( "TODO: save settings\n" );
	/*
	if( settings_save() )
		vips_error_exit( "unable to save settings" );
	 */

	return( status );
}
