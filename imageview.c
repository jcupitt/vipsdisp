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
#ifdef DEBUG
	printf( "imageview_postload:\n" ); 
#endif /*DEBUG*/

	gtk_widget_hide( imageview->progress_box );
}

static void
imageview_changed( Conversion *conversion, Imageview *imageview )
{
	const char *path;

#ifdef DEBUG
	printf( "imageview_changed:\n" ); 
#endif /*DEBUG*/

	if( (path = conversion_get_path( conversion )) ) { 
		char *basename;

		basename = g_path_get_basename( path );
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

static void
imageview_copy_state( Imageview *new_imageview, Imageview *old_imageview, 
	const char *action_name )
{
	GAction *old_action;
	GAction *new_action;

	old_action = g_action_map_lookup_action( G_ACTION_MAP( old_imageview ), 
		action_name );
	new_action = g_action_map_lookup_action( G_ACTION_MAP( new_imageview ), 
		action_name );

	if( old_action &&
		new_action ) {
		GVariant *state;

		state = g_action_get_state( old_action );
		g_action_change_state( new_action, state );
		VIPS_FREEF( g_variant_unref, state );
	}
}

static void
imageview_duplicate( GSimpleAction *action, 
	GVariant *parameter, gpointer user_data )
{
	Imageview *imageview = (Imageview *) user_data;
	Conversion *conversion = imageview->imagepresent->conversion;

	Imageview *new_imageview;
	int left, top, width, height;

	new_imageview = imageview_new_from_source( 
		GTK_APPLICATION( imageview->disp ), 
		conversion->source );

	g_object_set( new_imageview->imagepresent->conversion,
		"falsecolour", conversion->falsecolour,
		"log", conversion->log,
		"mag", conversion->mag,
		"scale", conversion->scale,
		"offset", conversion->offset,
		NULL );

	imageview_copy_state( new_imageview, imageview, "control" );
	imageview_copy_state( new_imageview, imageview, "info" );
	imageview_copy_state( new_imageview, imageview, "fullscreen" );

	gtk_window_get_size( GTK_WINDOW( imageview ), &width, &height );
	gtk_window_resize( GTK_WINDOW( new_imageview ), width, height );

	imagepresent_get_window_position( imageview->imagepresent, 
		&left, &top, &width, &height );
	imagepresent_set_window_position( new_imageview->imagepresent, 
		left, top );
}

static void
imageview_saveas( GSimpleAction *action, 
	GVariant *state, gpointer user_data )
{
	Imageview *imageview = (Imageview *) user_data;
	Conversion *conversion = imageview->imagepresent->conversion;

	GtkWidget *dialog;
	const char *path;
	int result;

	if( !conversion->image )
		return;

	dialog = gtk_file_chooser_dialog_new( "Save file",
		GTK_WINDOW( imageview ) , 
		GTK_FILE_CHOOSER_ACTION_SAVE,
		"_Cancel", GTK_RESPONSE_CANCEL,
		"_Save", GTK_RESPONSE_ACCEPT,
		NULL );

	if( (path = conversion_get_path( conversion )) )
		gtk_file_chooser_set_filename( GTK_FILE_CHOOSER( dialog ),
			path );
	else
		gtk_file_chooser_set_current_name( GTK_FILE_CHOOSER( dialog ),
			"untitled document" );

	result = gtk_dialog_run( GTK_DIALOG( dialog ) );
	if( result == GTK_RESPONSE_ACCEPT ) {
		char *file;

		imageview_hide_error( imageview ); 
		file = gtk_file_chooser_get_filename( 
			GTK_FILE_CHOOSER( dialog ) );
		if( conversion_write_to_file( conversion, file ) ) {
			imageview_show_error( imageview );
			g_free( file );
		}
		g_free( file );
	}

	gtk_widget_destroy( dialog );
}

static void
imageview_close( GSimpleAction *action, 
	GVariant *state, gpointer user_data )
{
	Imageview *imageview = (Imageview *) user_data;

	gtk_widget_destroy( GTK_WIDGET( imageview ) );  
}

static void
imageview_toggle( GSimpleAction *action, 
	GVariant *parameter, gpointer user_data )
{
	GVariant *state;

	state = g_action_get_state( G_ACTION( action ) );
	g_action_change_state( G_ACTION( action ), 
		g_variant_new_boolean( !g_variant_get_boolean( state ) ) );
	g_variant_unref( state );
} 

static void
imageview_fullscreen( GSimpleAction *action, 
	GVariant *state, gpointer user_data )
{
	Imageview *imageview = (Imageview *) user_data;

	if( g_variant_get_boolean( state ) )
		gtk_window_fullscreen( GTK_WINDOW( imageview ) );
	else
		gtk_window_unfullscreen( GTK_WINDOW( imageview ) );

	g_simple_action_set_state( action, state );
}

static void
imageview_control( GSimpleAction *action, 
	GVariant *state, gpointer user_data )
{
	Imageview *imageview = (Imageview *) user_data;

	gtk_widget_set_visible( GTK_WIDGET( imageview->conversionview ), 
		g_variant_get_boolean( state ) );

	g_simple_action_set_state( action, state );
}

static void
imageview_info( GSimpleAction *action, 
	GVariant *state, gpointer user_data )
{
	Imageview *imageview = (Imageview *) user_data;

	gtk_widget_set_visible( GTK_WIDGET( imageview->infobar ), 
		g_variant_get_boolean( state ) );

	g_simple_action_set_state( action, state );
}

static int
imageview_find_scale( Imageview *imageview, VipsObject *context, 
	int left, int top, int width, int height,
	double *scale, double *offset )
{
	Conversion *conversion = imageview->imagepresent->conversion;
	VipsImage **t = (VipsImage **) vips_object_local_array( context, 7 );

	double min, max;

	if( vips_extract_area( conversion->image, &t[0], 
		left, top, width, height, NULL ) ||
		vips_stats( t[0], &t[1], NULL ) )
		return( -1 );

	min = *VIPS_MATRIX( t[1], 0, 0 );
	max = *VIPS_MATRIX( t[1], 1, 0 );
	if( max == min ) {
		vips_error( "Find scale", _( "Min and max are equal" ) );
		return( -1 );
	}

	*scale = 255.0 / (max - min);
	*offset = -(min * *scale) + 0.5;

	return( 0 );
}

static void
imageview_scale( GSimpleAction *action, 
	GVariant *state, gpointer user_data )
{
	Imageview *imageview = (Imageview *) user_data;
	Conversion *conversion = imageview->imagepresent->conversion;

	int left, top, width, height;
	int right, bottom;
	VipsImage *context;
	double scale, offset;

	if( !conversion->image )
		return;

	imagepresent_get_window_position( imageview->imagepresent, 
		&left, &top, &width, &height );

	right = left + width;
	bottom = top + height;

	conversion_to_image_cods( conversion, 
		left, top, &left, &top );
	conversion_to_image_cods( conversion, 
		right, bottom, &right, &bottom );

	width = right - left;
	height = bottom - top;

	context = vips_image_new();
	if( imageview_find_scale( imageview, VIPS_OBJECT( context ), 
		left, top, width, height, &scale, &offset ) ) {
		imageview_show_error( imageview );
		g_object_unref( context );
		return;
	}
	g_object_unref( context );

	g_object_set( conversion,
		"scale", scale,
		"offset", offset,
		NULL );
}

static void
imageview_falsecolour( GSimpleAction *action, 
	GVariant *state, gpointer user_data )
{
	Imageview *imageview = (Imageview *) user_data;

	g_object_set( imageview->imagepresent->conversion,
		"falsecolour", g_variant_get_boolean( state ),
		NULL );
	
	g_simple_action_set_state( action, state );
}

static void
imageview_log( GSimpleAction *action, 
	GVariant *state, gpointer user_data )
{
	Imageview *imageview = (Imageview *) user_data;

	g_object_set( imageview->imagepresent->conversion,
		"log", g_variant_get_boolean( state ),
		NULL );
	
	g_simple_action_set_state( action, state );
}

static void
imageview_radio( GSimpleAction *action,
	GVariant *parameter, gpointer user_data )
{
	g_action_change_state( G_ACTION( action ), parameter );
}

static void
imageview_mode( GSimpleAction *action,
	GVariant *state, gpointer user_data )
{
	Imageview *imageview = (Imageview *) user_data;

	const gchar *str;

	str = g_variant_get_string( state, NULL );

	//if( g_str_equal( str, "toilet" ) ) 
	//   ...
	//else
	//   /* Ignore attempted change */
	//   return;

	g_simple_action_set_state( action, state );
}

static void
imageview_reset( GSimpleAction *action, 
	GVariant *state, gpointer user_data )
{
	Imageview *imageview = (Imageview *) user_data;

	g_object_set( imageview->imagepresent->conversion,
		"falsecolour", FALSE,
		"log", FALSE,
		"scale", 1.0,
		"offset", 0.0,
		NULL );
}

static GActionEntry imageview_entries[] = {
	{ "magin", imageview_magin },
	{ "magout", imageview_magout },
	{ "normal", imageview_normal },
	{ "bestfit", imageview_bestfit },
	{ "duplicate", imageview_duplicate },
	{ "saveas", imageview_saveas },
	{ "close", imageview_close },

	{ "fullscreen", imageview_toggle, NULL, "false", imageview_fullscreen },
	{ "control", imageview_toggle, NULL, "false", imageview_control },
	{ "info", imageview_toggle, NULL, "false", imageview_info },

	{ "scale", imageview_scale },
	{ "log", imageview_toggle, NULL, "false", imageview_log },
	{ "falsecolour", 
		imageview_toggle, NULL, "false", imageview_falsecolour },
	{ "mode", imageview_radio, "s", "'toilet'", imageview_mode },
	{ "reset", imageview_reset },
};

static void
imageview_open_clicked( GtkWidget *button, Imageview *imageview )
{
	Conversion *conversion = imageview->imagepresent->conversion;

	GtkWidget *dialog;
	const char *path;
	int result;

	dialog = gtk_file_chooser_dialog_new( "Select a file",
		GTK_WINDOW( imageview ) , 
		GTK_FILE_CHOOSER_ACTION_OPEN,
		"_Cancel", GTK_RESPONSE_CANCEL,
		"_Open", GTK_RESPONSE_ACCEPT,
		NULL );

	if( (path = conversion_get_path( conversion )) )
		gtk_file_chooser_set_filename( GTK_FILE_CHOOSER( dialog ),
			path );

	result = gtk_dialog_run( GTK_DIALOG( dialog ) );
	if( result == GTK_RESPONSE_ACCEPT ) {
		char *path;
		GFile *file;

		imageview_hide_error( imageview ); 
		path = gtk_file_chooser_get_filename( 
			GTK_FILE_CHOOSER( dialog ) );
		file = g_file_new_for_path( path );
		g_free( path );
		if( conversion_set_file( conversion, file ) )
			imageview_show_error( imageview ); 
		g_object_unref( file ); 
	}

	gtk_widget_destroy( dialog );
}

static void
imageview_init( Imageview *imageview )
{
	GtkWidget *open;
	GtkWidget *image;
	GtkWidget *menu_button;
	GtkBuilder *builder;
	GMenuModel *menu;
	GtkWidget *grid;

	g_action_map_add_action_entries( G_ACTION_MAP( imageview ), 
		imageview_entries, G_N_ELEMENTS( imageview_entries ), 
		imageview );

	/* Header bar.
	 */
	imageview->header_bar = gtk_header_bar_new(); 

	gtk_header_bar_set_show_close_button( 
		GTK_HEADER_BAR( imageview->header_bar ), TRUE );

	open = gtk_button_new_with_label( "Load ..." );
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

	/* Body.
	 */
	grid = gtk_grid_new();
	gtk_container_add( GTK_CONTAINER( imageview ), grid ); 
	gtk_widget_show( grid );

	/* Progress.
	 */
	imageview->progress_box = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );
	imageview->progress = gtk_progress_bar_new();
	gtk_widget_set_hexpand( imageview->progress, TRUE ); 
	gtk_box_pack_start( GTK_BOX( imageview->progress_box ), 
		imageview->progress, TRUE, TRUE, 0 );
	gtk_widget_show( imageview->progress );
	gtk_grid_attach( GTK_GRID( grid ), 
		imageview->progress_box, 0, 0, 1, 1 );

	/* Error display.
	 */
	imageview->error_box = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );
	imageview->error_label = gtk_label_new( "hello" );
	gtk_box_pack_start( GTK_BOX( imageview->error_box ), 
		imageview->error_label, TRUE, TRUE, 0 );
	gtk_widget_show( imageview->error_label );
	gtk_grid_attach( GTK_GRID( grid ), 
		imageview->error_box, 0, 1, 1, 1 );

	/* Image view.
	 */
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
	g_signal_connect( imageview->imagepresent->conversion, "changed",
		G_CALLBACK( imageview_changed ), imageview );

	/* Display control.
	 */
	imageview->conversionview = 
		conversionview_new( imageview->imagepresent->conversion );
	gtk_grid_attach( GTK_GRID( grid ), 
		GTK_WIDGET( imageview->conversionview ), 0, 3, 1, 1 );

	/* Info bar.
	 */
	imageview->infobar = infobar_new( imageview->imagepresent );
	gtk_grid_attach( GTK_GRID( grid ), 
		GTK_WIDGET( imageview->infobar ), 0, 4, 1, 1 );

	gtk_window_set_default_size( GTK_WINDOW( imageview ), 800, 800 ); 
}

static void
imageview_class_init( ImageviewClass *class )
{
}

Imageview *
imageview_new( GtkApplication *application, GFile *file )
{
	Disp *disp = (Disp *) application;

	Imageview *imageview;

#ifdef DEBUG
	printf( "imageview_new: file = %p\n", file ); 
#endif /*DEBUG*/

	imageview = g_object_new( imageview_get_type(),
		"application", application,
		NULL );

	imageview->disp = disp;

	gtk_widget_show( GTK_WIDGET( imageview ) );

	if( conversion_set_file( imageview->imagepresent->conversion, file ) )
		imageview_show_error( imageview ); 

	return( imageview ); 
}

Imageview *
imageview_new_from_source( GtkApplication *application, VipsSource *source )
{
	Disp *disp = (Disp *) application;

	Imageview *imageview;

#ifdef DEBUG
	printf( "imageview_new_from_source: source = %p\n", source ); 
#endif /*DEBUG*/

	imageview = g_object_new( imageview_get_type(),
		"application", application,
		NULL );

	imageview->disp = disp;

	gtk_widget_show( GTK_WIDGET( imageview ) );

	if( source )
		g_object_set( imageview->imagepresent->conversion, 
			"source", source, 
			NULL ); 

	return( imageview ); 
}
