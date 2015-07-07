/* Display an image with gtk3 and libvips. 
 */

#include <stdio.h>
#include <stdlib.h>

#include <gtk/gtk.h>

#include <vips/vips.h>

#include "disp.h"

G_DEFINE_TYPE( Imageview, imageview, GTK_TYPE_APPLICATION_WINDOW );

static void
imageview_init( Imageview *Imageview )
{
	printf( "imageview_init:\n" ); 
}

static void
imageview_class_init( ImageviewClass *class )
{
	printf( "imageview_class_init:\n" ); 
}

static void
imageview_magin( GSimpleAction *action, 
	GVariant *parameter, gpointer user_data )
{
	Imageview *imageview = (Imageview *) user_data;

	int window_left;
	int window_top;
	int window_width;
	int window_height;
	int image_x;
	int image_y;

	imagepresent_get_window_position( imageview->imagepresent, 
		&window_left, &window_top, &window_width, &window_height );
	imagedisplay_to_image_cods( imageview->imagepresent->imagedisplay,
		window_left + window_width / 2, window_top + window_height / 2, 
		&image_x, &image_y ); 

	imagepresent_magin( imageview->imagepresent, image_x, image_y ); 
}

static void
imageview_magout( GSimpleAction *action, 
	GVariant *parameter, gpointer user_data )
{
	Imageview *imageview = (Imageview *) user_data;

	imagepresent_magout( imageview->imagepresent ); 
}

static void
imageview_normal( GSimpleAction *action, 
	GVariant *parameter, gpointer user_data )
{
	Imageview *imageview = (Imageview *) user_data;

	imagepresent_set_mag( imageview->imagepresent, 1 );
}

static void
imageview_bestfit( GSimpleAction *action, 
	GVariant *parameter, gpointer user_data )
{
	Imageview *imageview = (Imageview *) user_data;

	imagepresent_bestfit( imageview->imagepresent );
}

static GActionEntry imageview_entries[] = {
	{ "magin", imageview_magin },
	{ "magout", imageview_magout },
	{ "normal", imageview_normal },
	{ "bestfit", imageview_bestfit }
};

static int
imageview_update_header( Imageview *imageview )
{
	char *path;

	if( (path = imagepresent_get_path( imageview->imagepresent )) ) { 
		char *basename;

		basename = g_path_get_basename( path );
		g_free( path ); 
		gtk_header_bar_set_title( 
			GTK_HEADER_BAR( imageview->header_bar ), basename );
		g_free( basename ); 
	}
	else
		gtk_header_bar_set_title( 
			GTK_HEADER_BAR( imageview->header_bar ), 
			"Untitled" );

	return( 0 );
}

static void
imageview_open_clicked( GtkWidget *button, Imageview *imageview )
{
	GtkWidget *dialog;
	char *path;
	int result;

	dialog = gtk_file_chooser_dialog_new( "Select a file",
		GTK_WINDOW( imageview ) , 
		GTK_FILE_CHOOSER_ACTION_OPEN,
		"_Cancel", GTK_RESPONSE_CANCEL,
		"_Open", GTK_RESPONSE_ACCEPT,
		NULL );

	if( (path = imagepresent_get_path( imageview->imagepresent )) ) {
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
		imagepresent_set_file( imageview->imagepresent, file ); 
		g_object_unref( file ); 

		imageview_update_header( imageview ); 
	}

	gtk_widget_destroy( dialog );
}

static void
imageview_preload( Imagedisplay *imagedisplay, 
	VipsProgress *progress, Imageview *imageview )
{
	gtk_widget_show( imageview->info );
}

static void
imageview_load( Imagedisplay *imagedisplay, 
	VipsProgress *progress, Imageview *imageview )
{
	static int previous_precent = -1;

	if( progress->percent != previous_precent ) {
		gtk_progress_bar_set_fraction( 
			GTK_PROGRESS_BAR( imageview->progress ), 
			progress->percent / 100.0 ); 

		previous_precent = progress->percent;
	}
}

static void
imageview_postload( Imagedisplay *imagedisplay, 
	VipsProgress *progress, Imageview *imageview )
{
	gtk_widget_hide( imageview->info );
}

Imageview *
imageview_new( GtkApplication *application, GFile *file )
{
	Disp *disp = (Disp *) application;

	Imageview *imageview;
	GtkWidget *open;
	GtkWidget *menu_button;
	GtkBuilder *builder;
	GMenuModel *menu;
	GtkWidget *grid;
	GtkWidget *content_area;
	int width;
	int height;

	printf( "imageview_new: file = %p\n", file ); 

	imageview = g_object_new( imageview_get_type(),
		"application", application,
		NULL );
	g_action_map_add_action_entries( G_ACTION_MAP( imageview ), 
		imageview_entries, G_N_ELEMENTS( imageview_entries ), 
		imageview );

	imageview->disp = disp;

	imageview->header_bar = gtk_header_bar_new(); 

	gtk_header_bar_set_show_close_button( 
		GTK_HEADER_BAR( imageview->header_bar ), TRUE );

	open = gtk_button_new_with_label( "Open" );
	gtk_header_bar_pack_start( 
		GTK_HEADER_BAR( imageview->header_bar ), open ); 
	g_signal_connect( open, "clicked", 
		G_CALLBACK( imageview_open_clicked ), imageview );
	gtk_widget_show( open );

	menu_button = gtk_menu_button_new();
	gtk_header_bar_pack_end( 
		GTK_HEADER_BAR( imageview->header_bar ), menu_button ); 
	gtk_widget_show( menu_button );

	builder = gtk_builder_new_from_resource( 
		"/vips/disp/gtk/imageview-popover.ui" ); 
	menu = G_MENU_MODEL( gtk_builder_get_object( builder, 
		"imageview-popover-menu" ) );
	gtk_menu_button_set_menu_model( GTK_MENU_BUTTON( menu_button ), menu );
	g_object_unref( builder );

	gtk_window_set_titlebar( GTK_WINDOW( imageview ), 
		imageview->header_bar ); 
	gtk_widget_show( imageview->header_bar );

	grid = gtk_grid_new();
	gtk_container_add( GTK_CONTAINER( imageview ), grid ); 
	gtk_widget_show( grid );

	imageview->info = gtk_info_bar_new();
	imageview->progress = gtk_progress_bar_new();
	gtk_widget_set_hexpand( imageview->progress, TRUE ); 
	content_area = gtk_info_bar_get_content_area( 
		GTK_INFO_BAR( imageview->info ) );
	gtk_container_add( GTK_CONTAINER( content_area ), imageview->progress );
	gtk_widget_show( imageview->progress );
	gtk_grid_attach( GTK_GRID( grid ), 
		imageview->info, 0, 0, 1, 1 );

	imageview->imagepresent = imagepresent_new();
	gtk_widget_set_hexpand( GTK_WIDGET( imageview->imagepresent ), TRUE ); 
	gtk_widget_set_vexpand( GTK_WIDGET( imageview->imagepresent ), TRUE ); 
	gtk_grid_attach( GTK_GRID( grid ), 
		GTK_WIDGET( imageview->imagepresent ), 0, 1, 1, 1 ); 
	gtk_widget_show( GTK_WIDGET( imageview->imagepresent ) );

	g_signal_connect( imageview->imagepresent->imagedisplay, "preload",
		G_CALLBACK( imageview_preload ), imageview );
	g_signal_connect( imageview->imagepresent->imagedisplay, "load",
		G_CALLBACK( imageview_load ), imageview );
	g_signal_connect( imageview->imagepresent->imagedisplay, "postload",
		G_CALLBACK( imageview_postload ), imageview );

	imagepresent_set_file( imageview->imagepresent, file ); 

	/* 83 is a magic number for the height of the top 
	 * bar on my laptop. 
	 */
	if( imagepresent_get_image_size( imageview->imagepresent, 
		&width, &height ) )  
		gtk_window_set_default_size( GTK_WINDOW( imageview ), 
			VIPS_MIN( 800, width ),
			VIPS_MIN( 800, height + 83 ) ); 

	gtk_widget_show( GTK_WIDGET( imageview ) );

	return( imageview ); 
}
