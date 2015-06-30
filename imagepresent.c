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

Imagepresent *
imagepresent_new( GtkApplication *application, GFile *file )
{
	Disp *disp = (Disp *) application;
	Imagepresent *imagepresent;

	GtkWidget *header;
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
	gtk_window_set_title( GTK_WINDOW( imagepresent ), "vipsdisp" );

	imagepresent->disp = disp;

	header = gtk_header_bar_new(); 
	gtk_header_bar_set_title( GTK_HEADER_BAR( header ), "hello there!" );
	gtk_window_set_titlebar( GTK_WINDOW( imagepresent ), header ); 
	gtk_header_bar_set_show_close_button( GTK_HEADER_BAR( header ), TRUE );
	open = gtk_button_new_with_label( "Open" );
	gtk_header_bar_pack_start( GTK_HEADER_BAR( header ), open ); 
	menu_button = gtk_menu_button_new();
	gtk_header_bar_pack_end( GTK_HEADER_BAR( header ), menu_button ); 

	builder = gtk_builder_new_from_resource( 
			"/vips/disp/gtk/imagepresent-popover.ui" ); 
	menu = G_MENU_MODEL( gtk_builder_get_object( builder, 
			"imagepresent-popover-menu" ) );
	gtk_menu_button_set_menu_model( GTK_MENU_BUTTON( menu_button ), menu );
	g_object_unref( builder );

	gtk_widget_show_all( header );

	scrolled = gtk_scrolled_window_new( NULL, NULL );
	gtk_widget_set_hexpand( scrolled, TRUE );
	gtk_widget_set_vexpand( scrolled, TRUE );

	imagepresent->imagedisplay = imagedisplay_new( file );
	gtk_container_add( GTK_CONTAINER( scrolled ), 
		GTK_WIDGET( imagepresent->imagedisplay ) );

	gtk_container_add( GTK_CONTAINER( imagepresent ), scrolled );

	/* 83 is a magic number for the height of the top 
	 * bar on my laptop. 
	 */
	gtk_window_set_default_size( GTK_WINDOW( imagepresent ), 
		VIPS_MIN( 800, imagepresent->imagedisplay->display->Xsize ),
		VIPS_MIN( 800, imagepresent->imagedisplay->display->Ysize + 83 ) ); 

	gtk_widget_show_all( GTK_WIDGET( imagepresent ) );

	return( imagepresent ); 
}
