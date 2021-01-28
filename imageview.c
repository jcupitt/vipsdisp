/* Display an image with gtk3 and libvips. 
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <gtk/gtk.h>

#include <vips/vips.h>

#include "disp.h"

G_DEFINE_TYPE( Imageview, imageview, GTK_TYPE_APPLICATION_WINDOW );

static void
imageview_init( Imageview *Imageview )
{
}

static void
imageview_class_init( ImageviewClass *class )
{
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
	conversion_to_image_cods( imageview->imagepresent->conversion,
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
	{ "bestfit", imageview_bestfit },
};

static void
imageview_show_error( Imageview *imageview )
{
	gtk_label_set_text( GTK_LABEL( imageview->error_label ), 
		vips_error_buffer() ); 
	vips_error_clear();
	gtk_widget_show( imageview->error_box );
}

static void
imageview_hide_error( Imageview *imageview )
{
	gtk_widget_hide( imageview->error_box );
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

		imageview_hide_error( imageview ); 
		path = gtk_file_chooser_get_filename( 
			GTK_FILE_CHOOSER( dialog ) );
		file = g_file_new_for_path( path );
		g_free( path );
		if( imagepresent_set_file( imageview->imagepresent, file ) )
			imageview_show_error( imageview ); 
		g_object_unref( file ); 
	}

	gtk_widget_destroy( dialog );
}

static void
imageview_preload( Conversion *conversion, 
	VipsProgress *progress, Imageview *imageview )
{
#ifdef DEBUG
	printf( "imageview_preload:\n" ); 
#endif /*DEBUG*/

	gtk_widget_show( imageview->progress_box );
}

static void
imageview_load( Conversion *conversion, 
	VipsProgress *progress, Imageview *imageview )
{
	static int previous_precent = -1;

	if( progress->percent != previous_precent ) {
#ifdef DEBUG
		printf( "imageview_load: %d%%\n", progress->percent ); 
#endif /*DEBUG*/

		gtk_progress_bar_set_fraction( 
			GTK_PROGRESS_BAR( imageview->progress ), 
			progress->percent / 100.0 ); 
		previous_precent = progress->percent;
	}
}

static void
imageview_postload( Conversion *conversion, 
	VipsProgress *progress, Imageview *imageview )
{
	char *path;

#ifdef DEBUG
	printf( "imageview_postload:\n" ); 
#endif /*DEBUG*/

	gtk_widget_hide( imageview->progress_box );

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

	if( conversion->image ) {
		VipsImage *image = conversion->image;

		char str[256];
		VipsBuf buf = VIPS_BUF_STATIC( str );

		vips_object_summary( VIPS_OBJECT( image ), &buf );
		vips_buf_appendf( &buf, ", " );
		vips_buf_append_size( &buf, VIPS_IMAGE_SIZEOF_IMAGE( image ) );
		vips_buf_appendf( &buf, ", %g x %g p/mm",
			image->Xres, image->Yres );
		gtk_header_bar_set_subtitle( 
			GTK_HEADER_BAR( imageview->header_bar ),
			vips_buf_all( &buf ) ); 
	}
}

Imageview *
imageview_new( GtkApplication *application, GFile *file )
{
	Disp *disp = (Disp *) application;

	Imageview *imageview;
	GtkWidget *open;
	GtkWidget *image;
	GtkWidget *menu_button;
	GtkBuilder *builder;
	GMenuModel *menu;
	GtkWidget *grid;

#ifdef DEBUG
	printf( "imageview_new: file = %p\n", file ); 
#endif /*DEBUG*/

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
	image = gtk_image_new_from_icon_name( "open-menu-symbolic", 
		GTK_ICON_SIZE_SMALL_TOOLBAR );
	gtk_widget_show( image );
	gtk_container_add( GTK_CONTAINER( menu_button ), image );
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

	imageview->progress_box = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );
	imageview->progress = gtk_progress_bar_new();
	gtk_widget_set_hexpand( imageview->progress, TRUE ); 
	gtk_box_pack_start( GTK_BOX( imageview->progress_box ), 
		imageview->progress, TRUE, TRUE, 0 );
	gtk_widget_show( imageview->progress );
	gtk_grid_attach( GTK_GRID( grid ), 
		imageview->progress_box, 0, 0, 1, 1 );

	imageview->error_box = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );
	imageview->error_label = gtk_label_new( "hello" );
	gtk_box_pack_start( GTK_BOX( imageview->error_box ), 
		imageview->error_label, TRUE, TRUE, 0 );
	gtk_widget_show( imageview->error_label );
	gtk_grid_attach( GTK_GRID( grid ), 
		imageview->error_box, 0, 1, 1, 1 );

	imageview->imagepresent = imagepresent_new();
	gtk_widget_set_hexpand( GTK_WIDGET( imageview->imagepresent ), TRUE ); 
	gtk_widget_set_vexpand( GTK_WIDGET( imageview->imagepresent ), TRUE ); 
	gtk_grid_attach( GTK_GRID( grid ), 
		GTK_WIDGET( imageview->imagepresent ), 0, 2, 1, 1 ); 
	gtk_widget_show( GTK_WIDGET( imageview->imagepresent ) );

	g_signal_connect( imageview->imagepresent->conversion, "preload",
		G_CALLBACK( imageview_preload ), imageview );
	g_signal_connect( imageview->imagepresent->conversion, "load",
		G_CALLBACK( imageview_load ), imageview );
	g_signal_connect( imageview->imagepresent->conversion, "postload",
		G_CALLBACK( imageview_postload ), imageview );

	/* Display control.
	 */

	imageview->conversionview = 
		conversionview_new( imageview->imagepresent->conversion );
	gtk_grid_attach( GTK_GRID( grid ), 
		GTK_WIDGET( imageview->conversionview ), 0, 3, 1, 1 );
	gtk_widget_show( GTK_WIDGET( imageview->conversionview ) );

	/* Info bar.
	 */
	imageview->infobar = infobar_new( imageview->imagepresent );
	gtk_grid_attach( GTK_GRID( grid ), 
		GTK_WIDGET( imageview->infobar ), 0, 4, 1, 1 );
	gtk_widget_show( GTK_WIDGET( imageview->infobar ) );

	/* Final size and show.
	 */

	gtk_window_set_default_size( GTK_WINDOW( imageview ), 800, 800 ); 

	gtk_widget_show( GTK_WIDGET( imageview ) );

	if( imagepresent_set_file( imageview->imagepresent, file ) )
		imageview_show_error( imageview ); 

	imagepresent_bestfit( imageview->imagepresent );

	return( imageview ); 
}
