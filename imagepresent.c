/* Display an image with gtk3 and libvips. 
 */

#include <stdio.h>
#include <stdlib.h>

#include <gtk/gtk.h>

#include <vips/vips.h>

#include "disp.h"

G_DEFINE_TYPE( Imagepresent, imagepresent, GTK_TYPE_APPLICATION_WINDOW );

static void
imagepresent_init( Imagepresent *Imagepresent )
{
	printf( "imagepresent_init:\n" ); 
}

static void
imagepresent_class_init( ImagepresentClass *class )
{
	printf( "imagepresent_class_init:\n" ); 
}

static void
imagepresent_activate_toggle( GSimpleAction *action, 
	GVariant *parameter, gpointer user_data )
{
	GVariant *state;

	printf( "imagepresent_activate_toggle:\n" ); 

	state = g_action_get_state( G_ACTION( action ) );
	g_action_change_state( G_ACTION( action ), 
		g_variant_new_boolean( !g_variant_get_boolean( state ) ) );
	g_variant_unref( state );
}

static void
imagepresent_change_fullscreen_state( GSimpleAction *action, 
	GVariant *state, gpointer user_data )
{
	Imagepresent *imagepresent = (Imagepresent *) user_data;

	printf( "imagepresent_change_fullscreen_state:\n" ); 

	if( g_variant_get_boolean( state ) )
		gtk_window_fullscreen( GTK_WINDOW( imagepresent ) );
	else
		gtk_window_unfullscreen( GTK_WINDOW( imagepresent ) );

	g_simple_action_set_state( action, state );
}

static GActionEntry imagepresent_entries[] = {
	{ "fullscreen", imagepresent_activate_toggle, NULL, "false", 
		imagepresent_change_fullscreen_state }
};

int
imagepresent_set_file( Imagepresent *imagepresent, GFile *file )
{
	char *path;

	VIPS_UNREF( imagepresent->file );

	imagepresent->file = file;
	g_object_ref( file ); 

	if( imagedisplay_set_file( imagepresent->imagedisplay, 
		imagepresent->file ) )
	       return( -1 ); 	

	if( imagepresent->file && 
		(path = g_file_get_path( imagepresent->file )) ) {
		char *basename;

		basename = g_path_get_basename( path );
		g_free( path ); 
		gtk_header_bar_set_title( 
			GTK_HEADER_BAR( imagepresent->header_bar ), basename );
		g_free( basename ); 
	}
	else 
		gtk_header_bar_set_title( 
			GTK_HEADER_BAR( imagepresent->header_bar ), 
			"vipsdisp" );

	return( 0 );
}

static void
imagepresent_open_clicked( GtkWidget *button, Imagepresent *imagepresent )
{
	GtkWidget *dialog;
	char *path;
	int result;

	dialog = gtk_file_chooser_dialog_new( "Select a file",
		GTK_WINDOW( imagepresent ) , 
		GTK_FILE_CHOOSER_ACTION_OPEN,
		"_Cancel", GTK_RESPONSE_CANCEL,
		"_Open", GTK_RESPONSE_ACCEPT,
		NULL );

	if( imagepresent->file && 
		(path = g_file_get_path( imagepresent->file )) ) {
		gtk_file_chooser_set_filename( GTK_FILE_CHOOSER( dialog ),
			path );
		g_free( path ); 
	}

	result = gtk_dialog_run( GTK_DIALOG( dialog ) );
	if( result == GTK_RESPONSE_ACCEPT ) {
		char *path;
		GFile *file;

		path = gtk_file_chooser_get_filename( 
			GTK_FILE_CHOOSER( dialog ) );
		file = g_file_new_for_path( path );
		g_free( path );
		imagepresent_set_file( imagepresent, file ); 
		g_object_unref( file ); 
	}

	gtk_widget_destroy( dialog );
}

Imagepresent *
imagepresent_new( GtkApplication *application, GFile *file )
{
	Disp *disp = (Disp *) application;

	Imagepresent *imagepresent;
	GtkWidget *open;
	GtkWidget *menu_button;
	GtkBuilder *builder;
	GMenuModel *menu;
	GtkWidget *scrolled;

	printf( "imagepresent_new: file = %p\n", file ); 

	imagepresent = g_object_new( imagepresent_get_type(),
		"application", application,
		NULL );
	g_action_map_add_action_entries( G_ACTION_MAP( imagepresent ), 
		imagepresent_entries, G_N_ELEMENTS( imagepresent_entries ), 
		imagepresent );

	imagepresent->disp = disp;

	imagepresent->header_bar = gtk_header_bar_new(); 

	gtk_header_bar_set_show_close_button( 
		GTK_HEADER_BAR( imagepresent->header_bar ), TRUE );

	open = gtk_button_new_with_label( "Open" );
	gtk_header_bar_pack_start( 
		GTK_HEADER_BAR( imagepresent->header_bar ), open ); 
	g_signal_connect( open, "clicked", 
		G_CALLBACK( imagepresent_open_clicked ), imagepresent );

	menu_button = gtk_menu_button_new();
	gtk_header_bar_pack_end( 
		GTK_HEADER_BAR( imagepresent->header_bar ), menu_button ); 
	builder = gtk_builder_new_from_resource( 
		"/vips/disp/gtk/imagepresent-popover.ui" ); 
	menu = G_MENU_MODEL( gtk_builder_get_object( builder, 
		"imagepresent-popover-menu" ) );
	gtk_menu_button_set_menu_model( GTK_MENU_BUTTON( menu_button ), menu );
	g_object_unref( builder );

	gtk_window_set_titlebar( GTK_WINDOW( imagepresent ), 
		imagepresent->header_bar ); 

	scrolled = gtk_scrolled_window_new( NULL, NULL );
	gtk_widget_set_hexpand( scrolled, TRUE );
	gtk_widget_set_vexpand( scrolled, TRUE );

	imagepresent->imagedisplay = imagedisplay_new();
	gtk_container_add( GTK_CONTAINER( scrolled ), 
		GTK_WIDGET( imagepresent->imagedisplay ) );

	gtk_container_add( GTK_CONTAINER( imagepresent ), scrolled );

	imagepresent_set_file( imagepresent, file ); 

	/* 83 is a magic number for the height of the top 
	 * bar on my laptop. 
	 */
	if( imagepresent->imagedisplay->display ) { 
		VipsImage *display = imagepresent->imagedisplay->display;

		gtk_window_set_default_size( GTK_WINDOW( imagepresent ), 
			VIPS_MIN( 800, display->Xsize ),
			VIPS_MIN( 800, display->Ysize + 83 ) ); 
	}

	gtk_widget_show_all( GTK_WIDGET( imagepresent ) );

	return( imagepresent ); 
}
