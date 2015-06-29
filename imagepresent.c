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
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS( class );

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

	GtkWidget *grid, *scrolled;
	GtkWidget *toolbar;
	GtkToolItem *button;
	GtkWidget *sw, *box, *label;

	printf( "imagepresent_new: file = %p\n", file ); 

	imagepresent = g_object_new( imagepresent_get_type(),
		"application", application,
		NULL );
	g_action_map_add_action_entries( G_ACTION_MAP( imagepresent ), 
		imagepresent_entries, G_N_ELEMENTS( imagepresent_entries ), 
		imagepresent );
	gtk_window_set_title( GTK_WINDOW( imagepresent ), "vipsdisp" );

	imagepresent->disp = disp;

	grid = gtk_grid_new();
	gtk_container_add( GTK_CONTAINER( imagepresent ), grid );

	toolbar = gtk_toolbar_new();

	button = gtk_separator_tool_item_new();
	gtk_separator_tool_item_set_draw( GTK_SEPARATOR_TOOL_ITEM( button ), 
		FALSE );
	gtk_tool_item_set_expand( GTK_TOOL_ITEM( button), TRUE );
	gtk_container_add( GTK_CONTAINER( toolbar ), GTK_WIDGET( button ) );

	button = gtk_tool_item_new();
	box = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 6 );
	gtk_container_add( GTK_CONTAINER( button ), box );
	label = gtk_label_new( "Fullscreen:" );
	gtk_container_add( GTK_CONTAINER( box ), label );
	sw = gtk_switch_new();
	gtk_actionable_set_action_name( GTK_ACTIONABLE( sw ), 
		"win.fullscreen" );
	gtk_container_add( GTK_CONTAINER( box ), sw );
	gtk_container_add( GTK_CONTAINER( toolbar ), GTK_WIDGET( button ) );

	gtk_grid_attach( GTK_GRID( grid ), toolbar, 0, 0, 1, 1 );

	scrolled = gtk_scrolled_window_new( NULL, NULL );
	gtk_widget_set_hexpand( scrolled, TRUE );
	gtk_widget_set_vexpand( scrolled, TRUE );

	imagepresent->imagedisplay = imagedisplay_new( file );
	gtk_container_add( GTK_CONTAINER( scrolled ), 
		GTK_WIDGET( imagepresent->imagedisplay ) );

	gtk_grid_attach( GTK_GRID( grid ), scrolled, 0, 1, 1, 1 );

	gtk_widget_show_all( grid );

	/* 49 is a magic number for the height of the toolbar and top menu
	 * bar on my laptop. 
	 *
	 * To fix this, maybe subclass drawingarea, implement
	 * scrollable and get it to tell scrolledwindow what size it likes.
	 */
	gtk_window_set_default_size( GTK_WINDOW( imagepresent ), 
		VIPS_MIN( 800, imagepresent->imagedisplay->display->Xsize ),
		VIPS_MIN( 800, imagepresent->imagedisplay->display->Ysize + 49 ) ); 

	gtk_widget_show( GTK_WIDGET( imagepresent ) );

	return( imagepresent ); 
}
