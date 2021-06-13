/* Display an image with gtk3 and libvips. 
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <gtk/gtk.h>

#include <vips/vips.h>

#include "disp.h"

/*
#define DEBUG_VERBOSE
#define DEBUG
 */

G_DEFINE_TYPE( Imageview, imageview, GTK_TYPE_APPLICATION_WINDOW );

static void
imageview_destroy( GtkWidget *widget )
{
	Imageview *imageview = (Imageview *) widget;

#ifdef DEBUG
	printf( "imageview_destroy: %p\n", widget ); 
#endif /*DEBUG*/

	if( imageview->imagepresent &&
		imageview->imagepresent->conversion ) {
		Conversion *conversion = imageview->imagepresent->conversion;

		if( imageview->preeval_sig ) { 
			g_signal_handler_disconnect( conversion, 
				imageview->preeval_sig ); 
			imageview->preeval_sig = 0;
		}

		if( imageview->eval_sig ) { 
			g_signal_handler_disconnect( conversion, 
				imageview->eval_sig ); 
			imageview->eval_sig = 0;
		}

		if( imageview->posteval_sig ) { 
			g_signal_handler_disconnect( conversion, 
				imageview->posteval_sig ); 
			imageview->posteval_sig = 0;
		}

		if( imageview->changed_sig ) { 
			g_signal_handler_disconnect( conversion, 
				imageview->changed_sig ); 
			imageview->changed_sig = 0;
		}

	}

	GTK_WIDGET_CLASS( imageview_parent_class )->destroy( widget );
}

static void
imageview_show_error( Imageview *imageview )
{
	char *err;
	int i;

	/* Remove any trailing \n.
	 */
	err = vips_error_buffer_copy();
	for( i = strlen( err ); i > 0 && err[i - 1] == '\n'; i-- )
		err[i - 1] = '\0';
	gtk_label_set_text( GTK_LABEL( imageview->error_label ), err );
	g_free( err );

	gtk_widget_show( imageview->error_box );
}

static void
imageview_hide_error( Imageview *imageview )
{
	gtk_widget_hide( imageview->error_box );
}

static void
imageview_preeval( Conversion *conversion, 
	VipsProgress *progress, Imageview *imageview )
{
#ifdef DEBUG
	printf( "imageview_preeval:\n" ); 
#endif /*DEBUG*/

	gtk_widget_show( imageview->progress_box );
}

static void
imageview_eval( Conversion *conversion, 
	VipsProgress *progress, Imageview *imageview )
{
	static int previous_percent = -1;

#ifdef DEBUG_VERBOSE
	printf( "imageview_eval: %d%%\n", progress->percent ); 
#endif /*DEBUG_VERBOSE*/

	if( progress->percent != previous_percent ) {
		char str[256];
		VipsBuf buf = VIPS_BUF_STATIC( str );

		gtk_progress_bar_set_fraction( 
			GTK_PROGRESS_BAR( imageview->progress ), 
			progress->percent / 100.0 ); 
		vips_buf_appendf( &buf, "%d%% complete, %d seconds to go", 
			progress->percent, progress->eta );
		gtk_progress_bar_set_text( 
			GTK_PROGRESS_BAR( imageview->progress ), 
			vips_buf_all( &buf ) );

		previous_percent = progress->percent;

		/* Run one loop iteration, don't block.
		 */
		(void) gtk_main_iteration_do( FALSE );
	}
}

static void
imageview_posteval( Conversion *conversion, 
	VipsProgress *progress, Imageview *imageview )
{
#ifdef DEBUG
	printf( "imageview_posteval:\n" ); 
#endif /*DEBUG*/

	gtk_widget_hide( imageview->progress_box );
}

static void
imageview_conversion_changed( Conversion *conversion, Imageview *imageview )
{
	const char *path;
	GVariant *state;
	const char *str;

#ifdef DEBUG
	printf( "imageview_conversion_changed:\n" ); 
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

	if( conversion->mode == CONVERSION_MODE_TOILET_ROLL ) 
		str = "toilet-roll";
	else if( conversion->mode == CONVERSION_MODE_MULTIPAGE )
		str = "multipage";
	else if( conversion->mode == CONVERSION_MODE_ANIMATED )
		str = "animated";
	else
		str = NULL;
	if( str ) {
		state = g_variant_new_string( str );
		change_state( GTK_WIDGET( imageview ), "mode", state );
	}

	state = g_variant_new_boolean( conversion->falsecolour );
	change_state( GTK_WIDGET( imageview ), "falsecolour", state );

	state = g_variant_new_boolean( conversion->log );
	change_state( GTK_WIDGET( imageview ), "log", state );
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
	conversion_to_image_cods( imageview->imagepresent->conversion->mag,
		window_left + window_width / 2, window_top + window_height / 2, 
		&image_x, &image_y ); 

	imagepresent_magin( imageview->imagepresent, image_x, image_y ); 
}

static void
imageview_magout( GSimpleAction *action, 
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
	conversion_to_image_cods( imageview->imagepresent->conversion->mag,
		window_left + window_width / 2, window_top + window_height / 2, 
		&image_x, &image_y ); 

	imagepresent_magout( imageview->imagepresent, image_x, image_y ); 
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
imageview_duplicate( GSimpleAction *action, 
	GVariant *parameter, gpointer user_data )
{
	Imageview *imageview = (Imageview *) user_data;

	Imageview *new_imageview;
	int width, height;

	new_imageview = imageview_new( GTK_APPLICATION( imageview->disp ) );

	imageview_set_conversion( new_imageview, 
		imageview->imagepresent->conversion );

	gtk_window_get_size( GTK_WINDOW( imageview ), &width, &height );
	gtk_window_resize( GTK_WINDOW( new_imageview ), width, height );

	/* falsecolour etc. are copied when we copy the converion. We
	 * just copy the window state here.
	 */
	copy_state( GTK_WIDGET( new_imageview ), 
		GTK_WIDGET( imageview ), "control" );
	copy_state( GTK_WIDGET( new_imageview ), 
		GTK_WIDGET( imageview ), "info" );
	copy_state( GTK_WIDGET( new_imageview ), 
		GTK_WIDGET( imageview ), "fullscreen" );

	/* We want to init the scroll position, but we can't do that until the
	 * adj range is set, and that won't happen until the image is loaded.
	 *
	 * Just copy the adj settings from the current window.
	 */
	copy_adj( 
		gtk_scrolled_window_get_hadjustment( 
			GTK_SCROLLED_WINDOW( new_imageview->imagepresent ) ),
		gtk_scrolled_window_get_hadjustment( 
			GTK_SCROLLED_WINDOW( imageview->imagepresent ) ) );
	copy_adj( 
		gtk_scrolled_window_get_vadjustment( 
			GTK_SCROLLED_WINDOW( new_imageview->imagepresent ) ),
		gtk_scrolled_window_get_vadjustment( 
			GTK_SCROLLED_WINDOW( imageview->imagepresent ) ) );
}

static void
imageview_replace( GSimpleAction *action, GVariant *state, gpointer user_data )
{
	Imageview *imageview = (Imageview *) user_data;
	Conversion *conversion = imageview->imagepresent->conversion;

	GtkWidget *dialog;
	const char *path;
	int result;

	dialog = gtk_file_chooser_dialog_new( "Replace from file",
		GTK_WINDOW( imageview ) , 
		GTK_FILE_CHOOSER_ACTION_OPEN,
		"_Cancel", GTK_RESPONSE_CANCEL,
		"_Replace", GTK_RESPONSE_ACCEPT,
		NULL );

	if( (path = conversion_get_path( conversion )) )
		gtk_file_chooser_set_filename( GTK_FILE_CHOOSER( dialog ),
			path );

	result = gtk_dialog_run( GTK_DIALOG( dialog ) );
	if( result == GTK_RESPONSE_ACCEPT ) {
		char *path;
		VipsSource *source;

		imageview_hide_error( imageview ); 
		path = gtk_file_chooser_get_filename( 
			GTK_FILE_CHOOSER( dialog ) );
		source = vips_source_new_from_file( path );
		g_free( path );
		imageview_set_source( imageview, source );
		g_object_unref( source ); 
	}

	gtk_widget_destroy( dialog );
}

static void
imageview_saveas( GSimpleAction *action, GVariant *state, gpointer user_data )
{
	Imageview *imageview = (Imageview *) user_data;
	Conversion *conversion = imageview->imagepresent->conversion;

	GtkWidget *dialog;
	const char *path;
	char *selected_filename;
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

	/* We need to pop down immediately so we expose the cancel
	 * button.
	 */
	imageview_hide_error( imageview ); 
	result = gtk_dialog_run( GTK_DIALOG( dialog ) );
	selected_filename = gtk_file_chooser_get_filename( 
		GTK_FILE_CHOOSER( dialog ) );
	gtk_widget_destroy( dialog );

	if( result == GTK_RESPONSE_ACCEPT &&
		conversion_write_to_file( conversion, selected_filename ) ) 
		imageview_show_error( imageview );

	g_free( selected_filename );
}

static void
imageview_close( GSimpleAction *action, GVariant *state, gpointer user_data )
{
	Imageview *imageview = (Imageview *) user_data;

	gtk_widget_destroy( GTK_WIDGET( imageview ) );  
}

static void
imageview_next( GSimpleAction *action, GVariant *state, gpointer user_data )
{
	Imageview *imageview = (Imageview *) user_data;
	Conversion *conversion = imageview->imagepresent->conversion;
	int page = VIPS_CLIP( 0, conversion->page, conversion->n_pages - 1 );

	g_object_set( conversion,
		"page", (page + 1) % conversion->n_pages,
		NULL );
}

static void
imageview_prev( GSimpleAction *action, GVariant *state, gpointer user_data )
{
	Imageview *imageview = (Imageview *) user_data;
	Conversion *conversion = imageview->imagepresent->conversion;
	int page = VIPS_CLIP( 0, conversion->page, conversion->n_pages - 1 );

	g_object_set( conversion,
		"page", page == 0 ? conversion->n_pages - 1 : page - 1,
		NULL );
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
	settings_setb( "image-display",
		"fullscreen", g_variant_get_boolean( state ) );

	g_simple_action_set_state( action, state );
}

static void
imageview_control( GSimpleAction *action, 
	GVariant *state, gpointer user_data )
{
	Imageview *imageview = (Imageview *) user_data;

	gtk_widget_set_visible( GTK_WIDGET( imageview->conversionview ), 
		g_variant_get_boolean( state ) );
	settings_setb( "image-display", 
		"show-display-control-bar", g_variant_get_boolean( state ) );

	g_simple_action_set_state( action, state );
}

static void
imageview_info( GSimpleAction *action, 
	GVariant *state, gpointer user_data )
{
	Imageview *imageview = (Imageview *) user_data;

	gtk_widget_set_visible( GTK_WIDGET( imageview->infobar ), 
		g_variant_get_boolean( state ) );
	settings_setb( "image-display", 
		"show-display-info-bar", g_variant_get_boolean( state ) );

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

	conversion_to_image_cods( conversion->mag, 
		left, top, &left, &top );
	conversion_to_image_cods( conversion->mag, 
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
	ConversionMode mode;

	str = g_variant_get_string( state, NULL );
	if( g_str_equal( str, "toilet-roll" ) ) 
		mode = CONVERSION_MODE_TOILET_ROLL;
	else if( g_str_equal( str, "multipage" ) ) 
		mode = CONVERSION_MODE_MULTIPAGE;
	else if( g_str_equal( str, "animated" ) ) 
		mode = CONVERSION_MODE_ANIMATED;
	else
		/* Ignore attempted change.
		 */
		return;

	g_object_set( imageview->imagepresent->conversion,
		"mode", mode,
		NULL );

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

static void
imageview_hide_display_control_bar( GSimpleAction *action, 
	GVariant *state, gpointer user_data )
{
	Imageview *imageview = (Imageview *) user_data;
	GActionMap *map = G_ACTION_MAP( imageview );

	GAction *control;

	control = g_action_map_lookup_action( map, "control" );
	if( control )
		g_action_change_state( control, 
			g_variant_new_boolean( FALSE ) );
}

static GActionEntry imageview_entries[] = {
	{ "magin", imageview_magin },
	{ "magout", imageview_magout },
	{ "normal", imageview_normal },
	{ "bestfit", imageview_bestfit },
	{ "duplicate", imageview_duplicate },
	{ "replace", imageview_replace },
	{ "saveas", imageview_saveas },
	{ "close", imageview_close },

	{ "fullscreen", imageview_toggle, NULL, "false", imageview_fullscreen },
	{ "control", imageview_toggle, NULL, "false", imageview_control },
	{ "info", imageview_toggle, NULL, "false", imageview_info },

	{ "next", imageview_next },
	{ "prev", imageview_prev },
	{ "scale", imageview_scale },
	{ "log", imageview_toggle, NULL, "false", imageview_log },
	{ "falsecolour", 
		imageview_toggle, NULL, "false", imageview_falsecolour },
	{ "mode", imageview_radio, "s", "'multipage'", imageview_mode },
	{ "reset", imageview_reset },
	{ "hide_display_control_bar", imageview_hide_display_control_bar },
};

static void
imageview_replace_clicked( GtkWidget *button, Imageview *imageview )
{
	g_action_activate( 
		g_action_map_lookup_action( G_ACTION_MAP( imageview ), 
			"replace" ),
		NULL );
}

static void
imageview_cancel_clicked( GtkWidget *button, Imageview *imageview )
{
	Conversion *conversion = imageview->imagepresent->conversion;

	if( conversion->image )
		vips_image_set_kill( conversion->image, TRUE );
}

static void
imageview_error_close_clicked( GtkWidget *button, Imageview *imageview )
{
	imageview_hide_error( imageview );
}

static void
imageview_size_allocate( GtkWidget *widget, GtkAllocation *allocation )
{
	int new_width, new_height;

	gtk_window_get_size( GTK_WINDOW( widget ), &new_width, &new_height );
	settings_seti( "image-display", "window-width", new_width );
	settings_seti( "image-display", "window-height", new_height );
}

static void
imageview_init( Imageview *imageview )
{
	Conversion *conversion;
	GtkWidget *button;
	GtkWidget *image;
	GtkWidget *menu_button;
	GtkBuilder *builder;
	GMenuModel *model;
	GtkWidget *grid;
	GVariant *state;

	if( disp_icon )
		gtk_window_set_icon( GTK_WINDOW( imageview ), disp_icon );

	g_action_map_add_action_entries( G_ACTION_MAP( imageview ), 
		imageview_entries, G_N_ELEMENTS( imageview_entries ), 
		imageview );

	g_signal_connect( imageview, "size-allocate", 
		G_CALLBACK( imageview_size_allocate ), imageview );

	/* Header bar.
	 */
	imageview->header_bar = gtk_header_bar_new(); 

	gtk_header_bar_set_show_close_button( 
		GTK_HEADER_BAR( imageview->header_bar ), TRUE );

	button = gtk_button_new_with_label( "Replace ..." );
	gtk_header_bar_pack_start( 
		GTK_HEADER_BAR( imageview->header_bar ), button ); 
	g_signal_connect( button, "clicked", 
		G_CALLBACK( imageview_replace_clicked ), imageview );
	gtk_widget_show( button );

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
	model = G_MENU_MODEL( gtk_builder_get_object( builder, 
		"imageview-popover-menu" ) );
	gtk_menu_button_set_menu_model( GTK_MENU_BUTTON( menu_button ), model );
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
	imageview->progress_box = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_widget_set_margin_top( imageview->progress_box, 3 );
	gtk_widget_set_margin_bottom( imageview->progress_box, 3 );
	imageview->progress = gtk_progress_bar_new();
	gtk_progress_bar_set_show_text( 
		GTK_PROGRESS_BAR( imageview->progress ), TRUE );
	gtk_box_pack_start( GTK_BOX( imageview->progress_box ), 
		imageview->progress, TRUE, TRUE, 3 );
	gtk_widget_set_valign( imageview->progress, GTK_ALIGN_CENTER );
	gtk_widget_show( imageview->progress );
	button = gtk_button_new_with_label( "Cancel" );
	g_signal_connect( button, "clicked", 
		G_CALLBACK( imageview_cancel_clicked ), imageview );
	gtk_box_pack_end( GTK_BOX( imageview->progress_box ), 
		button, FALSE, FALSE, 3 );
	gtk_widget_show( button );
	gtk_grid_attach( GTK_GRID( grid ), 
		imageview->progress_box, 0, 0, 1, 1 );

	/* Error display.
	 */
	imageview->error_box = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_widget_set_margin_top( imageview->error_box, 3 );
	gtk_widget_set_margin_bottom( imageview->error_box, 3 );
	image = gtk_image_new_from_icon_name( "dialog-error", 
		GTK_ICON_SIZE_LARGE_TOOLBAR );
	gtk_widget_show( image );
	gtk_box_pack_start( GTK_BOX( imageview->error_box ), 
		image, FALSE, FALSE, 3 );
	imageview->error_label = gtk_label_new( "" );
	gtk_box_pack_start( GTK_BOX( imageview->error_box ), 
		imageview->error_label, TRUE, TRUE, 3 );
	gtk_widget_show( imageview->error_label );
	button = gtk_button_new_with_label( "OK" );
	g_signal_connect( button, "clicked", 
		G_CALLBACK( imageview_error_close_clicked ), imageview );
	gtk_widget_set_valign( button, GTK_ALIGN_END );
	gtk_box_pack_end( GTK_BOX( imageview->error_box ), 
		button, FALSE, FALSE, 3 );
	gtk_widget_show( button );
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

	conversion = imageview->imagepresent->conversion;
	imageview->preeval_sig = g_signal_connect( conversion, "preeval",
		G_CALLBACK( imageview_preeval ), imageview );
	imageview->eval_sig = g_signal_connect( conversion, "eval",
		G_CALLBACK( imageview_eval ), imageview );
	imageview->posteval_sig = g_signal_connect( conversion, "posteval",
		G_CALLBACK( imageview_posteval ), imageview );
	imageview->changed_sig = g_signal_connect( conversion, "changed",
		G_CALLBACK( imageview_conversion_changed ), imageview );

	imagepresent_set_menu( imageview->imagepresent, 
		GTK_MENU( gtk_menu_new_from_model( model ) ) );

	/* Display control.
	 */
	imageview->conversionview = conversionview_new( conversion );
	gtk_grid_attach( GTK_GRID( grid ), 
		GTK_WIDGET( imageview->conversionview ), 0, 3, 1, 1 );

	/* Info bar.
	 */
	imageview->infobar = infobar_new( imageview->imagepresent );
	gtk_grid_attach( GTK_GRID( grid ), 
		GTK_WIDGET( imageview->infobar ), 0, 4, 1, 1 );

	/* Initial state from settings.
	 */
	state = g_variant_new_boolean( 
		settings_getb( "image-display", "show-display-control-bar" ) );
	change_state( GTK_WIDGET( imageview ), "control", state );
	state = g_variant_new_boolean( 
		settings_getb( "image-display", "show-display-info-bar" ) );
	change_state( GTK_WIDGET( imageview ), "info", state );
	state = g_variant_new_boolean( 
		settings_getb( "image-display", "fullscreen" ) );
	change_state( GTK_WIDGET( imageview ), "fullscreen", state );

	gtk_window_set_default_size( GTK_WINDOW( imageview ), 
		settings_geti( "image-display", "window-width" ),
		settings_geti( "image-display", "window-height" ) );
}

static void
imageview_class_init( ImageviewClass *class )
{
	GtkWidgetClass *widget_class = (GtkWidgetClass*) class;

	widget_class->destroy = imageview_destroy;
}

Imageview *
imageview_new( GtkApplication *application )
{
	Disp *disp = (Disp *) application;

	Imageview *imageview;

#ifdef DEBUG
	printf( "imageview_new:\n" );
#endif /*DEBUG*/

	imageview = g_object_new( imageview_get_type(),
		"application", application,
		NULL );

	imageview->disp = disp;

	gtk_widget_show( GTK_WIDGET( imageview ) );

	return( imageview ); 
}

void
imageview_set_file( Imageview *imageview, GFile *file )
{
	if( conversion_set_file( imageview->imagepresent->conversion, file ) )
		imageview_show_error( imageview ); 
}

void
imageview_set_source( Imageview *imageview, VipsSource *source )
{
	if( conversion_set_source( imageview->imagepresent->conversion, 
		source ) )
		imageview_show_error( imageview ); 
}

void
imageview_set_conversion( Imageview *imageview, Conversion *conversion )
{
	if( conversion_set_conversion( imageview->imagepresent->conversion,
		conversion ) ) 
		imageview_show_error( imageview ); 
}
