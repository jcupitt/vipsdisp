/* Save and restore app defaultsm like display control bar visibility.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <gtk/gtk.h>

#include <vips/vips.h>

#include "disp.h"

/*
#define DEBUG
 */

static GKeyFile *all_settings = NULL;

static const char *default_settings[][3] = {
	{ "image-display", "window-width", "800" },
	{ "image-display", "window-height", "800" },
};

static void
settings_init( void )
{
	if( !all_settings ) {
		const char *config_dir = g_get_user_config_dir();

		int i;
		char *filename;
		
		all_settings = g_key_file_new();

		for( i = 0; i < VIPS_NUMBER( default_settings ); i++ ) 
			settings_set( default_settings[i][0], 
				default_settings[i][1], 
				default_settings[i][2] ); 

		filename = g_build_filename( config_dir, "vipsdisprc", NULL );
#ifdef DEBUG
		printf( "settings_init: loading from %s\n", filename );
#endif /*DEBUG*/
		(void) g_key_file_load_from_file( all_settings, 
			filename, G_KEY_FILE_NONE, NULL );
		g_free( filename );
	}
}

void
settings_setb( const char *group_name, const char *key, gboolean value )
{
	settings_init();
	g_key_file_set_boolean( all_settings, group_name, key, value );
}

void
settings_set( const char *group_name, const char *key, const char *value )
{
	settings_init();
	g_key_file_set_string( all_settings, group_name, key, value );
}

void
settings_seti( const char *group_name, const char *key, int value )
{
	settings_init();
	g_key_file_set_integer( all_settings, group_name, key, value );
}

void
settings_setd( const char *group_name, const char *key, double value )
{
	settings_init();
	g_key_file_set_double( all_settings, group_name, key, value );
}

gboolean
settings_getb( const char *group_name, const char *key )
{
	settings_init();
	return( g_key_file_get_boolean( all_settings, group_name, key, NULL ) );
}

const char *
settings_get( const char *group_name, const char *key )
{
	settings_init();
	return( g_key_file_get_string( all_settings, group_name, key, NULL ) );
}

int
settings_geti( const char *group_name, const char *key )
{
	settings_init();
	return( g_key_file_get_integer( all_settings, group_name, key, NULL ) );
}

double
settings_getd( const char *group_name, const char *key )
{
	settings_init();
	return( g_key_file_get_double( all_settings, group_name, key, NULL ) );
}

/* Save all settings to the user config file.
 */
int
settings_save( void )
{
	const char *config_dir = g_get_user_config_dir();

	GError *error = NULL;
	char *filename;

	settings_init();
	filename = g_build_filename( config_dir, "vipsdisprc", NULL );
#ifdef DEBUG
	printf( "settings_save: writing to %s\n", filename );
#endif /*DEBUG*/
	if( !g_key_file_save_to_file( all_settings, filename, &error ) ) {
		g_free( filename );
		vips_error( "glib", "%s\n", error->message );
                g_error_free( error );
		return( -1 );
	}

	return( 0 );
}
