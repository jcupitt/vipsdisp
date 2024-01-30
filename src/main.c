/*
#define DEBUG
 */

#include "vipsdisp.h"

int
main( int argc, char **argv )
{
	VipsdispApp *app;
	int status;

	if( VIPS_INIT( argv[0] ) )
		vips_error_exit( "unable to start libvips" );

#ifdef DEBUG
	printf( "DEBUG on in main.c\n" );
	vips_leak_set( TRUE );

	g_log_set_always_fatal( 
		G_LOG_FLAG_RECURSION |
		G_LOG_FLAG_FATAL |
		G_LOG_LEVEL_ERROR |
		G_LOG_LEVEL_CRITICAL |
		G_LOG_LEVEL_WARNING  |
		0 );

	g_setenv( "G_DEBUG", "fatal-warnings", FALSE );
#endif /*DEBUG*/

	/* Magickload will lock up on eg. AVI files.
	 */
	printf( "vipsdisp.main: blocking VipsForeignLoadMagick\n" );
	vips_operation_block_set( "VipsForeignLoadMagick", TRUE );

	app = vipsdisp_app_new();

	status = g_application_run( G_APPLICATION( app ), argc, argv );

	vips_shutdown();

	return( status );
}
