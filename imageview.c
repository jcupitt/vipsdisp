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

/* Display a LABPACK value.
 */
static void
imageview_status_value_labpack( Imageview *imageview, VipsBuf *buf, VipsPel *p )
{
	unsigned int iL = (p[0] << 2) | (p[3] >> 6);
	float L = 100.0 * iL / 1023.0;
	signed int ia = ((signed char) p[1] << 3) | ((p[3] >> 3) & 0x7);
	float a = 0.125 * ia;
	signed int ib = ((signed char) p[2] << 3) | (p[3] & 0x7);
	float b = 0.125 * ib;

	vips_buf_appendf( buf, "%5g ", L );
	vips_buf_appendf( buf, "%5g ", a );
	vips_buf_appendf( buf, "%5g ", b );
}

/* Diplay a RAD. 
 */
static void
imageview_status_value_rad( Imageview *imageview, VipsBuf *buf, VipsPel *p )
{
	double f = ldexp( 1.0, p[3] - (128 + 8) );
	float r = (p[0] + 0.5) * f;
	float g = (p[1] + 0.5) * f;
	float b = (p[2] + 0.5) * f;

	vips_buf_appendf( buf, "%5g ", r );
	vips_buf_appendf( buf, "%5g ", g );
	vips_buf_appendf( buf, "%5g ", b );
}

static void 
imageview_status_value_uncoded( Imageview *imageview, 
	VipsBuf *buf, VipsPel *p )
{
	VipsImage *image = imageview->imagepresent->imagedisplay->image;

	int i;

	for( i = 0; i < image->Bands; i++ ) {
		switch( image->BandFmt ) {
		case VIPS_FORMAT_UCHAR:
			vips_buf_appendf( buf, 
				"%3d ", ((unsigned char *)p)[0] );
			break;

		case VIPS_FORMAT_CHAR:
			vips_buf_appendf( buf, 
				"%4d ", ((char *)p)[0] );
			break;

		case VIPS_FORMAT_USHORT:
			vips_buf_appendf( buf, 
				"%7d ", ((unsigned short *)p)[0] );
			break;

		case VIPS_FORMAT_SHORT:
			vips_buf_appendf( buf, 
				"%7d ", ((short *)p)[0] );
			break;

		case VIPS_FORMAT_UINT:
			vips_buf_appendf( buf, 
				"%8d ", ((unsigned int *)p)[0] );
			break;

		case VIPS_FORMAT_INT:
			vips_buf_appendf( buf, 
				"%8d ", ((int *)p)[0] );
			break;

		case VIPS_FORMAT_FLOAT:
			vips_buf_appendf( buf, 
				"%10g ", ((float *)p)[0] );
			break;

		case VIPS_FORMAT_COMPLEX:
			vips_buf_appendf( buf, 
				"(%7g,%7g) ", 
				((float *)p)[0], ((float *)p)[1] );
			break;

		case VIPS_FORMAT_DOUBLE:
			vips_buf_appendf( buf, 
				"%10g ", ((double *)p)[0] );
			break;

		case VIPS_FORMAT_DPCOMPLEX:
			vips_buf_appendf( buf, "(%7g,%7g) ", 
				((double *)p)[0], 
				((double *)p)[1] );
			break;

		default:
			vips_buf_appendf( buf, " " );
			break;
		}

		p += VIPS_IMAGE_SIZEOF_ELEMENT( image );
	}
}

void 
imageview_status_value( Imageview *imageview, VipsBuf *buf, int x, int y ) 
{
	VipsImage *image = imageview->imagepresent->imagedisplay->image;
	Imagedisplay *imagedisplay = imageview->imagepresent->imagedisplay;

	VipsPel *ink;

	if( image &&
		(ink = imagedisplay_get_ink( imagedisplay, x, y )) ) { 
		switch( image->Coding ) { 
		case VIPS_CODING_LABQ:
			imageview_status_value_labpack( imageview, buf, ink );
			break;

		case VIPS_CODING_RAD:
			imageview_status_value_rad( imageview, buf, ink );
			break;

		case VIPS_CODING_NONE:
			imageview_status_value_uncoded( imageview, buf, ink );
			break;

		default:
			break;
		}
	}
}

void
imageview_status_update( Imageview *imageview )
{
	Imagedisplay *imagedisplay = imageview->imagepresent->imagedisplay;

	char str[256];
	VipsBuf buf = VIPS_BUF_STATIC( str );
	int image_x;
	int image_y;
	int image_width;
	int image_height;
	int mag;

	imagedisplay_to_image_cods( imagedisplay,
		imageview->imagepresent->last_x, 
		imageview->imagepresent->last_y,
		&image_x, &image_y );

	if( imagedisplay_get_image_size( imagedisplay, 
		&image_width, &image_height ) ) {
		image_x = VIPS_CLIP( 0, image_x, image_width - 1 );
		image_y = VIPS_CLIP( 0, image_y, image_height - 1 );

		vips_buf_appendf( &buf, "(%7d, %7d)", image_x, image_y ); 
		gtk_label_set_text( GTK_LABEL( imageview->coord_label ), 
			vips_buf_all( &buf ) ); 
		vips_buf_rewind( &buf ); 

		imageview_status_value( imageview, &buf, image_x, image_y ); 
		gtk_label_set_text( GTK_LABEL( imageview->value_label ), 
			vips_buf_all( &buf ) ); 
	}

	vips_buf_rewind( &buf ); 
	vips_buf_appendf( &buf, "Magnification " );
	mag = imagedisplay_get_mag( imageview->imagepresent->imagedisplay ); 
	if( mag >= 0 )
		vips_buf_appendf( &buf, "%d:1", mag );
	else
		vips_buf_appendf( &buf, "1:%d", -mag );
	gtk_label_set_text( GTK_LABEL( imageview->mag_label ), 
		vips_buf_all( &buf ) ); 

}

static void
imageview_position_changed( Imagepresent *imagepresent, Imageview *imageview )
{
	imageview_status_update( imageview ); 
}

static int
imageview_header_update( Imageview *imageview )
{
	VipsImage *image = imageview->imagepresent->imagedisplay->image;

	char *path;
	char str[256];
	VipsBuf buf = VIPS_BUF_STATIC( str );

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

	if( image ) {
		vips_object_summary( VIPS_OBJECT( image ), &buf );
		vips_buf_appendf( &buf, ", " );
		vips_buf_append_size( &buf, VIPS_IMAGE_SIZEOF_IMAGE( image ) );
		vips_buf_appendf( &buf, ", %g x %g p/mm",
			image->Xres, image->Yres );
		gtk_label_set_text( GTK_LABEL( imageview->info_label ), 
			vips_buf_all( &buf ) ); 
	}

	imageview_status_update( imageview ); 

	return( 0 );
}

static void
imageview_show_error( Imageview *imageview )
{
	gtk_label_set_text( GTK_LABEL( imageview->error_label ), 
		vips_error_buffer() ); 
	vips_error_clear();
	gtk_widget_show( imageview->error_info );
}

static void
imageview_hide_error( Imageview *imageview )
{
	gtk_widget_hide( imageview->error_info );
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

		imageview_header_update( imageview ); 
	}

	gtk_widget_destroy( dialog );
}

static void
imageview_preload( Imagedisplay *imagedisplay, 
	VipsProgress *progress, Imageview *imageview )
{
	gtk_widget_show( imageview->progress_info );
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
	gtk_widget_hide( imageview->progress_info );
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
	GtkWidget *hbox;
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

	imageview->progress_info = gtk_info_bar_new();
	imageview->progress = gtk_progress_bar_new();
	gtk_widget_set_hexpand( imageview->progress, TRUE ); 
	content_area = gtk_info_bar_get_content_area( 
		GTK_INFO_BAR( imageview->progress_info ) );
	gtk_container_add( GTK_CONTAINER( content_area ), imageview->progress );
	gtk_widget_show( imageview->progress );
	gtk_grid_attach( GTK_GRID( grid ), 
		imageview->progress_info, 0, 0, 1, 1 );

	imageview->error_info = gtk_info_bar_new();
	gtk_info_bar_set_message_type( GTK_INFO_BAR( imageview->error_info ),
		GTK_MESSAGE_ERROR );
	imageview->error_label = gtk_label_new( "hello" );
	content_area = gtk_info_bar_get_content_area( 
		GTK_INFO_BAR( imageview->error_info ) );
	gtk_container_add( GTK_CONTAINER( content_area ), 
		imageview->error_label );
	gtk_widget_show( imageview->error_label );
	gtk_grid_attach( GTK_GRID( grid ), 
		imageview->error_info, 0, 0, 1, 1 );

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

	imageview->status_bar = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );
	gtk_container_set_border_width( GTK_CONTAINER( imageview->status_bar ),
		10 );

	imageview->info_label = gtk_label_new( "" );
	gtk_label_set_xalign( GTK_LABEL( imageview->info_label ), 0 );
	gtk_box_pack_start( GTK_BOX( imageview->status_bar ), 
		imageview->info_label, TRUE, TRUE, 0 );
	gtk_widget_show( imageview->info_label );

	hbox = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 10 );

	imageview->coord_label = gtk_label_new( "" );
	gtk_label_set_xalign( GTK_LABEL( imageview->coord_label ), 0 );
	gtk_box_pack_start( GTK_BOX( hbox ), 
		imageview->coord_label, FALSE, FALSE, 0 );
	gtk_widget_show( imageview->coord_label );

	imageview->value_label = gtk_label_new( "" );
	gtk_label_set_xalign( GTK_LABEL( imageview->value_label ), 0 );
	gtk_box_pack_start( GTK_BOX( hbox ), 
		imageview->value_label, TRUE, TRUE, 0 );
	gtk_widget_show( imageview->value_label );

	imageview->mag_label = gtk_label_new( "" );
	gtk_label_set_xalign( GTK_LABEL( imageview->mag_label ), 0 );
	gtk_box_pack_end( GTK_BOX( hbox ), 
		imageview->mag_label, FALSE, FALSE, 0 );
	gtk_widget_show( imageview->mag_label );

	gtk_box_pack_start( GTK_BOX( imageview->status_bar ), 
		hbox, TRUE, TRUE, 0 );
	gtk_widget_show( hbox );

	gtk_grid_attach( GTK_GRID( grid ), 
		imageview->status_bar, 0, 2, 1, 1 );
	gtk_widget_show( imageview->status_bar );

	if( imagepresent_set_file( imageview->imagepresent, file ) )
		imageview_show_error( imageview ); 
	imageview_header_update( imageview ); 

	/* 83 is a magic number for the height of the top 
	 * bar on my laptop. 
	 */
	if( imagepresent_get_image_size( imageview->imagepresent, 
		&width, &height ) )  
		gtk_window_set_default_size( GTK_WINDOW( imageview ), 
			VIPS_MIN( 800, width ),
			VIPS_MIN( 800, height + 83 ) ); 

	g_signal_connect( imageview->imagepresent, "position_changed", 
		G_CALLBACK( imageview_position_changed ), imageview );

	gtk_widget_show( GTK_WIDGET( imageview ) );

	return( imageview ); 
}
