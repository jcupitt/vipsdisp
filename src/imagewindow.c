/*
#define DEBUG
*/

/*
#define EXPERIMENTAL_PROPERTIES_EDIT
*/

#include "vipsdisp.h"

/* How much to scale view by each frame.
 */
#define SCALE_STEP (1.05)

struct _ImageWindow
{
	GtkApplicationWindow parent;

	TileSource *tile_source;
	TileCache *tile_cache;

	/* Last known mouse postion, in gtk coordinates. We keep these in gtk
	 * cods so we don't need to update them on pan / zoom.
	 */
	double last_x_gtk;
	double last_y_gtk;

	/* For drag, the window position where we started, in gtk coordinates.
	 */
	int drag_start_x;
	int drag_start_y;

	/* For pinch zoom, zoom position that we started.
	 */
	double last_scale;
	double scale_cx;
	double scale_cy;

	/* For animating zoom. 
	 */
	guint tick_handler;
	double scale_rate;
	double scale_target;

	/* Position of the GtkPaned separator.
	 */
	guint paned_position;

	/* This flag is TRUE when the properties show/hide animation is in
	 * progress.
	 */
	gboolean paned_is_animating;

	/* TRUE if image_window_paned_init has been called at least
	 * once.
	 */
	gboolean is_paned_init;

	GtkWidget *right_click_menu;
	GtkWidget *title;
	GtkWidget *subtitle;
	GtkWidget *gears;
	GtkWidget *progress_bar;
	GtkWidget *progress;
	GtkWidget *progress_cancel;
	GtkWidget *error_bar;
	GtkWidget *error_label;
	GtkWidget *scrolled_window;
	GtkWidget *imagedisplay;
	GtkWidget *display_bar;
	GtkWidget *info_bar;
	GtkWidget *paned;
	GtkWidget *properties;

	/* Throttle progress bar updates to a few per second with this.
	 */
	GTimer *progress_timer;
	double last_progress_time;

	gint64 last_frame_time;

	GSettings *settings;

	GtkCssProvider *provider;
	GdkDisplay *display;
	GtkSettings *gtk_settings;
};

G_DEFINE_TYPE( ImageWindow, image_window, GTK_TYPE_APPLICATION_WINDOW );

/* Our signals. 
 */
enum {
	SIG_CHANGED,			/* A new tile_source */
	SIG_STATUS_CHANGED,		/* New mouse position */
	SIG_LAST
};

static guint image_window_signals[SIG_LAST] = { 0 };

static void
image_window_dispose( GObject *object )
{
	ImageWindow *win = VIPSDISP_IMAGE_WINDOW( object );

#ifdef DEBUG
	printf( "image_window_dispose:\n" ); 
#endif /*DEBUG*/

	/* Remove the GtkSyleProvider ( used for CSS ) from the GdkDisplay
	 * @m->display.
	 */
	gtk_style_context_remove_provider_for_display( win->display,
			GTK_STYLE_PROVIDER( win->provider ) );

	VIPS_UNREF( win->tile_source );
	VIPS_UNREF( win->tile_cache );
	VIPS_FREEF( gtk_widget_unparent, win->right_click_menu );
	VIPS_FREEF( g_timer_destroy, win->progress_timer );

	G_OBJECT_CLASS( image_window_parent_class )->dispose( object );
}

static void
image_window_status_changed( ImageWindow *win )
{
	g_signal_emit( win, 
		image_window_signals[SIG_STATUS_CHANGED], 0 );
}

static void
image_window_changed( ImageWindow *win )
{
	g_signal_emit( win,
		image_window_signals[SIG_CHANGED], 0 );
}

static void
image_window_set_position( ImageWindow *win, double x, double y )
{
	GtkAdjustment *hadj = gtk_scrolled_window_get_hadjustment( 
		GTK_SCROLLED_WINDOW( win->scrolled_window ) );
	GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment( 
		GTK_SCROLLED_WINDOW( win->scrolled_window ) );

#ifdef DEBUG
	printf( "image_window_set_position: x = %g, y = %g\n", x, y ); 
#endif /*DEBUG*/

	gtk_adjustment_set_value( hadj, x );
	gtk_adjustment_set_value( vadj, y );

}

static void
image_window_get_position( ImageWindow *win,
	int *left, int *top, int *width, int *height )
{
	GtkAdjustment *hadj = gtk_scrolled_window_get_hadjustment(
		GTK_SCROLLED_WINDOW( win->scrolled_window ) );
	GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(
		GTK_SCROLLED_WINDOW( win->scrolled_window ) );

	*left = gtk_adjustment_get_value( hadj );
	*top = gtk_adjustment_get_value( vadj );
	*width = gtk_adjustment_get_page_size( hadj );
	*height = gtk_adjustment_get_page_size( vadj );

#ifdef DEBUG
	printf( "image_window_get_position: %d %d %d %d\n",
		*left, *top, *width, *height );
#endif /*DEBUG*/
}

static void
image_window_set_scale( ImageWindow *win, double scale )
{
#ifdef DEBUG
	printf( "image_window_set_scale: %g\n", scale );
#endif /*DEBUG*/

	/* Scale by the zoom factor (SVG etc. scale) we picked on load.
	 */
	scale /= win->tile_source->zoom;

	g_object_set( win->imagedisplay, 
		"scale", scale,
		NULL );

	if( win->tile_source &&
		win->tile_source->loaded )
		image_window_status_changed( win );
}

double
image_window_get_scale( ImageWindow *win )
{
	double scale;

	g_object_get( win->imagedisplay, 
		"scale", &scale,
		NULL );

	/* Scale by the zoom factor (SVG etc. scale) we picked on load.
	 */
	scale *= win->tile_source->zoom;

#ifdef DEBUG
	printf( "image_window_get_scale: %g\n", scale );
#endif /*DEBUG*/

	return( scale );
}

/* Set a new mag, keeping the pixel at x/y in the image at the same position 
 * on the screen.
 */
static void	
image_window_set_scale_position( ImageWindow *win, 
	double scale, double x_image, double y_image )
{			
	double old_x, old_y;
	double new_x, new_y;
	int left, top, width, height;

#ifdef DEBUG
	printf( "image_window_set_scale_position: %g %g %g\n", scale, x, y );
#endif /*DEBUG*/ 

	/* Map the image pixel at (x, y) to gtk space, ie. mouse coordinates.
	 */
	imagedisplay_image_to_gtk( VIPSDISP_IMAGEDISPLAY( win->imagedisplay ), 
		x_image, y_image, &old_x, &old_y ); 

	image_window_set_scale( win, scale );

	/* Map image (x, y) to display coordinates with our new magnification,
	 * then to keep the point in the same position we must translate by 
	 * the difference.
	 */
	imagedisplay_image_to_gtk( VIPSDISP_IMAGEDISPLAY( win->imagedisplay ),
		x_image, y_image, &new_x, &new_y );

	/* Add 0.5 since we (in effect) cast to int here and we want round to
	 * nearest.
	 */
	image_window_get_position( win, &left, &top, &width, &height );
	image_window_set_position( win, 
		left + new_x - old_x + 0.5, top + new_y - old_y + 0.5 );
}

static void
image_window_set_scale_centre( ImageWindow *win, double scale )
{
	double current_scale = image_window_get_scale( win );

	int window_left;
	int window_top;
	int window_width;
	int window_height;

	image_window_get_position( win,
		&window_left, &window_top, &window_width, &window_height );

	window_left /= current_scale;
	window_top /= current_scale;
	window_width /= current_scale;
	window_height /= current_scale;

	image_window_set_scale_position( win, 
		scale,
		window_left + window_width / 2, 
		window_top + window_height / 2 );
}

void
image_window_bestfit( ImageWindow *win )
{
#ifdef DEBUG
	printf( "image_window_bestfit:\n" ); 
#endif /*DEBUG*/

	if( win->tile_source ) {
		int widget_width = gtk_widget_get_width( win->imagedisplay );
		int widget_height = gtk_widget_get_height( win->imagedisplay );
		double hscale = (double) widget_width / 
			win->tile_source->display_width;
		double vscale = (double) widget_height / 
			win->tile_source->display_height;
		double scale = VIPS_MIN( hscale, vscale );

		image_window_set_scale( win, scale * win->tile_source->zoom );
	}
}

static void
image_window_preeval( VipsImage *image, 
	VipsProgress *progress, ImageWindow *win )
{
	gtk_action_bar_set_revealed( GTK_ACTION_BAR( win->progress_bar ), 
		TRUE );
}

typedef struct _EvalUpdate {
	ImageWindow *win;
	int eta;
	int percent;
} EvalUpdate;

static gboolean
image_window_eval_idle( void *user_data )
{
	EvalUpdate *update = (EvalUpdate *) user_data;
	ImageWindow *win = update->win;

	char str[256];
	VipsBuf buf = VIPS_BUF_STATIC( str );

	vips_buf_appendf( &buf, "%d%% complete, %d seconds to go",
		update->percent, update->eta );
	gtk_progress_bar_set_text( GTK_PROGRESS_BAR( win->progress ),
		vips_buf_all( &buf ) );

	gtk_progress_bar_set_fraction( GTK_PROGRESS_BAR( win->progress ),
		update->percent / 100.0 );

	g_object_unref( win );

	g_free( update );
	
	return( FALSE );
}

static void
image_window_eval( VipsImage *image, 
	VipsProgress *progress, ImageWindow *win )
{
	double time_now;
	EvalUpdate *update;

	/* We can be ^Q'd during load. This is NULLed in _dispose.
	 */
	if( !win->progress_timer )
		return;

	time_now = g_timer_elapsed( win->progress_timer, NULL );

	/* Throttle to 10Hz.
	 */
	if( time_now - win->last_progress_time < 0.1 )
		return;
	win->last_progress_time = time_now;

#ifdef DEBUG_VERBOSE
	printf( "image_window_eval: %d%%\n", progress->percent );
#endif /*DEBUG_VERBOSE*/

	/* This can come from the background load thread, so we can't update 
	 * the UI directly.
	 */

	update = g_new( EvalUpdate, 1 );

	update->win = win;
	update->percent = progress->percent;
	update->eta = progress->eta;

	/* We don't want win to vanish before we process this update. The
	 * matching unref is in the handler above.
	 */
	g_object_ref( win );

	g_idle_add( image_window_eval_idle, update );
}

static void
image_window_posteval( VipsImage *image, 
	VipsProgress *progress, ImageWindow *win )
{
	gtk_action_bar_set_revealed( GTK_ACTION_BAR( win->progress_bar ), 
		FALSE );
}

static void
image_window_cancel_clicked( GtkWidget *button, ImageWindow *win )
{
	VipsImage *image;

	if( win->tile_source &&
		(image = tile_source_get_image( win->tile_source )) ) 
		vips_image_set_kill( image, TRUE );
}

static void
image_window_error( ImageWindow *win )
{
	char *err;
	int i;

	/* Remove any trailing \n.
	 */
	err = vips_error_buffer_copy();
	for( i = strlen( err ); i > 0 && err[i - 1] == '\n'; i-- )
		err[i - 1] = '\0';
	gtk_label_set_text( GTK_LABEL( win->error_label ), err );
	g_free( err );

	gtk_info_bar_set_revealed( GTK_INFO_BAR( win->error_bar ), TRUE );
}

static void
image_window_error_hide( ImageWindow *win )
{
	gtk_info_bar_set_revealed( GTK_INFO_BAR( win->error_bar ), FALSE );
}

static void
image_window_tile_source_changed( TileSource *tile_source, ImageWindow *win )
{
	GVariant *state;
	const char *str_mode;

#ifdef DEBUG
	printf( "image_window_tile_source_changed:\n" );
#endif /*DEBUG*/

	state = g_variant_new_boolean( tile_source->falsecolour );
	change_state( GTK_WIDGET( win ), "falsecolour", state );

	state = g_variant_new_boolean( tile_source->log );
	change_state( GTK_WIDGET( win ), "log", state );

	state = g_variant_new_boolean( tile_source->icc );
	change_state( GTK_WIDGET( win ), "icc", state );

	if( tile_source->mode == TILE_SOURCE_MODE_TOILET_ROLL )
		str_mode = "toilet-roll";
	else if( tile_source->mode == TILE_SOURCE_MODE_MULTIPAGE )
		str_mode = "multipage";
	else if( tile_source->mode == TILE_SOURCE_MODE_ANIMATED )
		str_mode = "animated";
	else if( tile_source->mode == TILE_SOURCE_MODE_PAGES_AS_BANDS )
		str_mode = "pages-as-bands";
	else
		str_mode = NULL;

	if( str_mode ) {
		state = g_variant_new_string( str_mode );
		change_state( GTK_WIDGET( win ), "mode", state );
	}
}

static void
image_window_error_response( GtkWidget *button, int response, ImageWindow *win )
{
	image_window_error_hide( win );
}

static void
image_window_toggle_debug( ImageWindow *win )
{
	gboolean debug;

	g_object_get( win->imagedisplay, 
		"debug", &debug,
		NULL );

	g_object_set( win->imagedisplay, 
		"debug", !debug,
		NULL );
}

static void
image_window_magin_action( GSimpleAction *action,
	GVariant *parameter, gpointer user_data )
{
	ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );

	image_window_set_scale_centre( win,
		SCALE_STEP * image_window_get_scale( win ) );
}

static void
image_window_magout_action( GSimpleAction *action, 
	GVariant *parameter, gpointer user_data )
{
	ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );

	image_window_set_scale_centre( win,
		(1.0 / SCALE_STEP) * image_window_get_scale( win ) );
}

static void
image_window_bestfit_action( GSimpleAction *action,
	GVariant *parameter, gpointer user_data )
{
	ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );

	image_window_bestfit( win );
}

static void
image_window_oneone_action( GSimpleAction *action, 
	GVariant *parameter, gpointer user_data )
{
	ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );

	image_window_set_scale( win, 1.0 );
}

static void
image_window_duplicate_action( GSimpleAction *action, 
	GVariant *parameter, gpointer user_data )
{
	ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );

	VipsdispApp *app;
	TileSource *tile_source;
	ImageWindow *new;
	int width, height;

	g_object_get( win, "application", &app, NULL );
	new = image_window_new( app ); 
	gtk_window_present( GTK_WINDOW( new ) );

	if( win->tile_source ) {
		if( !(tile_source = 
			tile_source_duplicate( win->tile_source )) ) {
			image_window_error( new ); 
			return;
		}
		image_window_set_tile_source( new, tile_source );
		g_object_unref( tile_source );
	}

	gtk_window_get_default_size( GTK_WINDOW( win ), &width, &height );
	gtk_window_set_default_size( GTK_WINDOW( new ), width, height );

	copy_state( GTK_WIDGET( new ), GTK_WIDGET( win ), "control" );
	copy_state( GTK_WIDGET( new ), GTK_WIDGET( win ), "info" );
	copy_state( GTK_WIDGET( new ), GTK_WIDGET( win ), "properties" );
	copy_state( GTK_WIDGET( new ), GTK_WIDGET( win ), "background" );

	/* We want to init the scroll position, but we can't do that until the
	 * adj range is set, and that won't happen until the image is loaded.
	 *
	 * Just copy the adj settings from the current window.
	 */
	copy_adj( 
		gtk_scrolled_window_get_hadjustment( 
			GTK_SCROLLED_WINDOW( new->scrolled_window ) ),
		gtk_scrolled_window_get_hadjustment( 
			GTK_SCROLLED_WINDOW( win->scrolled_window ) ) );
	copy_adj( 
		gtk_scrolled_window_get_vadjustment( 
			GTK_SCROLLED_WINDOW( new->scrolled_window ) ),
		gtk_scrolled_window_get_vadjustment( 
			GTK_SCROLLED_WINDOW( win->scrolled_window ) ) );
}

#if GTK_CHECK_VERSION(4, 10, 0)
static void
image_window_on_file_open_cb( GObject* source_object,
	GAsyncResult* res, gpointer user_data )
{
	ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );
	GtkFileDialog *dialog = GTK_FILE_DIALOG( source_object );
	GFile *file;

	file = gtk_file_dialog_open_finish( dialog, res, NULL );
	if( file ) {
		image_window_error_hide( win );
		image_window_open( win, file );
		g_object_unref( file );
	}
}
#else
static void
image_window_replace_response( GtkDialog *dialog, 
	gint response_id, gpointer user_data )
{
	ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );

	if( response_id == GTK_RESPONSE_ACCEPT ) {
		GFile *file;

		file = gtk_file_chooser_get_file( GTK_FILE_CHOOSER( dialog ) );
		image_window_error_hide( win ); 
		image_window_open( win, file );
		VIPS_UNREF( file ); 
	}

	gtk_window_destroy( GTK_WINDOW( dialog ) );
}
#endif

static void
image_window_replace_action( GSimpleAction *action, 
	GVariant *parameter, gpointer user_data )
{
	ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );

#if GTK_CHECK_VERSION(4, 10, 0)
	GtkFileDialog *dialog;
	GFile *file;

	dialog = gtk_file_dialog_new();
	gtk_file_dialog_set_title( dialog, "Replace from file" );
	gtk_file_dialog_set_accept_label( dialog, "Replace" );
	gtk_file_dialog_set_modal( dialog, TRUE );

	if( win->tile_source &&
		(file = tile_source_get_file( win->tile_source )) ) {
		gtk_file_dialog_set_initial_file( dialog, file );
		g_object_unref( file );
	}

	gtk_file_dialog_open( dialog, GTK_WINDOW( win ), NULL,
		&image_window_on_file_open_cb, win );
#else
	GtkWidget *dialog;
	GFile *file;

	dialog = gtk_file_chooser_dialog_new( "Replace from file",
		GTK_WINDOW( win ) , 
		GTK_FILE_CHOOSER_ACTION_OPEN,
		"_Cancel", GTK_RESPONSE_CANCEL,
		"_Replace", GTK_RESPONSE_ACCEPT,
		NULL );
	gtk_window_set_modal( GTK_WINDOW( dialog ), TRUE );

	if( win->tile_source &&
		(file = tile_source_get_file( win->tile_source )) ) {
		gtk_file_chooser_set_file( GTK_FILE_CHOOSER( dialog ), 
			file, NULL );
		g_object_unref( file );
	}

	g_signal_connect( dialog, "response", 
		G_CALLBACK( image_window_replace_response ), win );

	gtk_widget_show( dialog );
#endif
}

static void
image_window_saveas_options_response( GtkDialog *dialog, 
	gint response, gpointer user_data )
{
#if !GTK_CHECK_VERSION(4, 10, 0)
	GtkWidget *file_chooser = GTK_WIDGET( user_data );
#endif

	// final save and everything worked OK, we can all pop down
	if( response == GTK_RESPONSE_ACCEPT ) {
		gtk_window_destroy( GTK_WINDOW( dialog ) );

#if !GTK_CHECK_VERSION(4, 10, 0)
		gtk_window_destroy( GTK_WINDOW( file_chooser ) );
#endif
	}

	// save options was cancelled, just pop that down
	if( response == GTK_RESPONSE_CANCEL ) 
		gtk_window_destroy( GTK_WINDOW( dialog ) );

	// other return codes are intermediate stages of processing and we
	// should do nothing
}

#if GTK_CHECK_VERSION(4, 10, 0)
static void
image_window_on_file_save_cb( GObject* source_object,
	GAsyncResult* res, gpointer user_data )
{
	ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );
	GtkFileDialog *dialog = GTK_FILE_DIALOG( source_object );
	GFile *file;

	file = gtk_file_dialog_save_finish( dialog, res, NULL );
	if( file ) {
		char *filename;
		SaveOptions *options;

		filename = g_file_get_path( file );
		g_object_unref( file );

		options = save_options_new( GTK_WINDOW( win ),
			win->tile_source->image, filename );

		g_free( filename );

		if( !options ) {
			image_window_error( win );
			return;
		}

		g_signal_connect_object( options, "response",
			G_CALLBACK( image_window_saveas_options_response ),
			NULL, 0 );

		gtk_window_present( GTK_WINDOW( options ) );
	}
}
#else
static void
image_window_saveas_response( GtkDialog *dialog,
	gint response, gpointer user_data )
{
	ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );

	if( response == GTK_RESPONSE_ACCEPT ) {
		GFile *file;
		char *filename;
		SaveOptions *options;

		file = gtk_file_chooser_get_file( GTK_FILE_CHOOSER( dialog ) );
		filename = g_file_get_path( file );
		VIPS_UNREF( file ); 

		options = save_options_new( GTK_WINDOW( dialog ),
			win->tile_source->image, filename );

		g_free( filename ); 

		if( !options ) {
			image_window_error( win );
			return;
		}

		g_signal_connect_object( options, "response", 
			G_CALLBACK( image_window_saveas_options_response ), 
			dialog, 0 );

		gtk_window_present( GTK_WINDOW( options ) );
	}

	if( response == GTK_RESPONSE_CANCEL )
		gtk_window_destroy( GTK_WINDOW( dialog ) );
}
#endif

static void
image_window_saveas_action( GSimpleAction *action, 
	GVariant *parameter, gpointer user_data )
{
	ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );

	if( win->tile_source ) {
#if GTK_CHECK_VERSION(4, 10, 0)
		GtkFileDialog *dialog;
		GFile *file;

		dialog = gtk_file_dialog_new();
		gtk_file_dialog_set_title( dialog, "Save file" );
		gtk_file_dialog_set_modal( dialog, TRUE );

		if( (file = tile_source_get_file( win->tile_source )) ) {
			gtk_file_dialog_set_initial_file( dialog, file );
			g_object_unref( file );
		}

		gtk_file_dialog_save( dialog, GTK_WINDOW( win ), NULL,
			&image_window_on_file_save_cb, win );
#else
		GtkWidget *file_chooser;
		GFile *file;

#ifdef EXPERIMENTAL_PROPERTIES_EDIT
		TileSource *tile_source;
		VipsImage *image = NULL;
		VipsImage *image_copy = NULL;

		tile_source = image_window_get_tile_source( win );
		image = tile_source_get_image( tile_source );
		vips_copy( image, &image_copy, NULL );
		tile_source_set_image( tile_source, image_copy );
		VIPS_UNREF( image );

		properties_apply( VIPSDISP_PROPERTIES( win->properties ) );
#endif /* EXPERIMENTAL_PROPERTIES_EDIT */

		file_chooser = gtk_file_chooser_dialog_new( "Save file",
			GTK_WINDOW( win ) , 
			GTK_FILE_CHOOSER_ACTION_SAVE,
			"_Cancel", GTK_RESPONSE_CANCEL,
			"_Save", GTK_RESPONSE_ACCEPT,
			NULL );

		gtk_window_set_modal( GTK_WINDOW( file_chooser ), true );

		if( (file = tile_source_get_file( win->tile_source )) ) {
			gtk_file_chooser_set_file( 
				GTK_FILE_CHOOSER( file_chooser ), 
				file, NULL );
			g_object_unref( file );
		}

		g_signal_connect( file_chooser, "response", 
			G_CALLBACK( image_window_saveas_response ), win );

		gtk_widget_show( file_chooser );
#endif /* GTK_CHECK_VERSION(4, 10, 0) */
	}
}

static void
image_window_close_action( GSimpleAction *action, 
	GVariant *parameter, gpointer user_data )
{
	ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );

	gtk_window_destroy( GTK_WINDOW( win ) );
}

static gboolean
image_window_tick( GtkWidget *widget, 
	GdkFrameClock *frame_clock, gpointer user_data )
{
	ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );
	gint64 frame_time = gdk_frame_clock_get_frame_time( frame_clock );
	double dt = win->last_frame_time > 0 ?
		(double) (frame_time - win->last_frame_time) / 
			G_TIME_SPAN_SECOND : 
		1.0 / G_TIME_SPAN_SECOND;
	double scale = image_window_get_scale( win );

	double x_image;
	double y_image;
	double new_scale;

#ifdef DEBUG
	printf( "image_window_tick: dt = %g\n", dt );
#endif /*DEBUG*/

	image_window_get_mouse_position( win, &x_image, &y_image );

	new_scale = scale;

	if( win->scale_rate != 1.0 )
		new_scale = (dt * (win->scale_rate - 1.0) + 1.0) * scale;

	if( win->scale_target != 0 ) {
		if( (win->scale_rate > 1.0 && 
				new_scale >= win->scale_target) ||
			(win->scale_rate < 1.0 && 
			 new_scale <= win->scale_target) ) {
			win->scale_rate = 1.0;
			new_scale = win->scale_target;
			win->scale_target = 0.0;
		}
	}

	image_window_set_scale_position( win, new_scale, x_image, y_image );

	win->last_frame_time = frame_time;

	return( G_SOURCE_CONTINUE );
}

static gboolean
image_window_is_animating( ImageWindow *win )
{
	return( win->scale_rate != 1.0 );
}

static void
image_window_start_animation( ImageWindow *win )
{
	if( image_window_is_animating( win ) &&
		!win->tick_handler ) {
		win->last_frame_time = -1;
		win->tick_handler = gtk_widget_add_tick_callback( 
			GTK_WIDGET( win ),
			image_window_tick, win, NULL );
	}
}

static void
image_window_stop_animation( ImageWindow *win )
{
	if( !image_window_is_animating( win ) &&
		win->tick_handler ) {
		gtk_widget_remove_tick_callback( GTK_WIDGET( win ), 
			win->tick_handler );
		win->tick_handler = 0;
	}
}

static void
image_window_animate_scale_to( ImageWindow *win, double scale_target )
{
	// use a bigger number for faster zoom
	static const double animation_speed = 0.5;

	double scale = image_window_get_scale( win );

	win->scale_rate = log( scale_target / scale ) / animation_speed;
	win->scale_target = scale_target;
}

static void
image_window_animate_bestfit( ImageWindow *win )
{
	int widget_width = gtk_widget_get_width( win->imagedisplay );
	int widget_height = gtk_widget_get_height( win->imagedisplay );
	double hscale = (double) widget_width / 
		win->tile_source->display_width;
	double vscale = (double) widget_height / 
		win->tile_source->display_height;
	double scale = VIPS_MIN( hscale, vscale );

	image_window_animate_scale_to( win, scale * win->tile_source->zoom );
}

static struct {
	int keyval;
	double scale;
} magnify_keys[] = {
	{ GDK_KEY_1, 1.0 },
	{ GDK_KEY_2, 2.0 },
	{ GDK_KEY_3, 3.0 },
	{ GDK_KEY_4, 4.0 },
	{ GDK_KEY_5, 5.0 },
	{ GDK_KEY_6, 6.0 },
	{ GDK_KEY_7, 7.0 },
	{ GDK_KEY_8, 8.0 },
	{ GDK_KEY_9, 9.0 }
};

static gboolean 
image_window_key_pressed( GtkEventControllerKey *self,
	guint keyval, guint keycode, GdkModifierType state, gpointer user_data )
{
	ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );
	GtkScrolledWindow *scrolled_window = 
		GTK_SCROLLED_WINDOW( win->scrolled_window );

	gboolean handled;
	gboolean ret;

#ifdef DEBUG
	printf( "image_window_key_pressed: keyval = %d, state = %d\n", 
		keyval, state );
#endif /*DEBUG*/

	handled = FALSE;

	switch( keyval ) {
	case GDK_KEY_plus:
	case GDK_KEY_i:
		win->scale_rate = 1.5 * SCALE_STEP;
		handled = TRUE;
		break;

	case GDK_KEY_o:
	case GDK_KEY_minus:
		win->scale_rate = 0.2 / SCALE_STEP;
		handled = TRUE;
		break;

	case GDK_KEY_Left:
		if( state & GDK_SHIFT_MASK )
			g_signal_emit_by_name( scrolled_window, "scroll-child",
				GTK_SCROLL_PAGE_BACKWARD, TRUE, &ret);
		else if( state & GDK_CONTROL_MASK )
			g_signal_emit_by_name( scrolled_window, "scroll-child",
				GTK_SCROLL_START, TRUE, &ret);
		else
			g_signal_emit_by_name( scrolled_window, "scroll-child",
				GTK_SCROLL_STEP_LEFT, TRUE, &ret);
		handled = TRUE;
		break;

	case GDK_KEY_Right:
		if( state & GDK_SHIFT_MASK )
			g_signal_emit_by_name( scrolled_window, "scroll-child",
				GTK_SCROLL_PAGE_FORWARD, TRUE, &ret);
		else if( state & GDK_CONTROL_MASK )
			g_signal_emit_by_name( scrolled_window, "scroll-child",
				GTK_SCROLL_END, TRUE, &ret);
		else
			g_signal_emit_by_name( scrolled_window, "scroll-child",
				GTK_SCROLL_STEP_RIGHT, TRUE, &ret);
		handled = TRUE;
		break;

	case GDK_KEY_Up:
		if( state & GDK_SHIFT_MASK )
			g_signal_emit_by_name( scrolled_window, "scroll-child",
				GTK_SCROLL_PAGE_UP, FALSE, &ret);
		else if( state & GDK_CONTROL_MASK )
			g_signal_emit_by_name( scrolled_window, "scroll-child",
				GTK_SCROLL_START, FALSE, &ret);
		else
			g_signal_emit_by_name( scrolled_window, "scroll-child",
				GTK_SCROLL_STEP_UP, FALSE, &ret);
		handled = TRUE;
		break;

	case GDK_KEY_Down:
		if( state & GDK_SHIFT_MASK )
			g_signal_emit_by_name( scrolled_window, "scroll-child",
				GTK_SCROLL_PAGE_DOWN, FALSE, &ret);
		else if( state & GDK_CONTROL_MASK )
			g_signal_emit_by_name( scrolled_window, "scroll-child",
				GTK_SCROLL_END, FALSE, &ret);
		else
			g_signal_emit_by_name( scrolled_window, "scroll-child",
				GTK_SCROLL_STEP_DOWN, FALSE, &ret);
		handled = TRUE;
		break;

	case GDK_KEY_0:
		image_window_animate_bestfit( win );
		handled = TRUE;
		break;

	case GDK_KEY_d:
		image_window_toggle_debug( win );
		break;

	default:
		break;
	}

	if( !handled ) {
		int i;

		for( i = 0; i < VIPS_NUMBER( magnify_keys ); i++ )
			if( magnify_keys[i].keyval == keyval ) {
				double scale;

				scale = magnify_keys[i].scale;
				if( state & GDK_CONTROL_MASK )
					scale = 1.0 / scale;

				image_window_animate_scale_to( win, scale );

				handled = TRUE;
				break;
			}
	}

	if( handled )
		image_window_start_animation( win );

	return( handled );
}

static gboolean 
image_window_key_released( GtkEventControllerKey *self,
	guint keyval, guint keycode, GdkModifierType state, gpointer user_data )
{
	ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );

	gboolean handled;

	handled = FALSE;

	switch( keyval ) {
	case GDK_KEY_plus:
	case GDK_KEY_i:
	case GDK_KEY_o:
	case GDK_KEY_minus:
		win->scale_rate = 1.0;
		handled = TRUE;
		break;

	default:
		break;
	}

	image_window_stop_animation( win );

	return( handled );
}

static void
image_window_motion( GtkEventControllerMotion *self,
	gdouble x, gdouble y, gpointer user_data )
{
	ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );

	win->last_x_gtk = x;
	win->last_y_gtk = y;

	image_window_status_changed( win );
}

static gboolean
image_window_scroll( GtkEventControllerMotion *self,
	double dx, double dy, gpointer user_data )
{
	ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );

	double x_image;
	double y_image;

	image_window_get_mouse_position( win, &x_image, &y_image );

	if( dy < 0 ) 
		image_window_set_scale_position( win, 
			SCALE_STEP * image_window_get_scale( win ), 
			x_image, y_image );
	else 
		image_window_set_scale_position( win, 
			(1.0 / SCALE_STEP) * image_window_get_scale( win ), 
			x_image, y_image );

	return( TRUE );
}

static void
image_window_scale_begin( GtkGesture* self, 
	GdkEventSequence* sequence, gpointer user_data )
{
	ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );

	double finger_cx;
	double finger_cy;

	win->last_scale = image_window_get_scale( win );
	gtk_gesture_get_bounding_box_center( self, &finger_cx, &finger_cy );

	imagedisplay_gtk_to_image( VIPSDISP_IMAGEDISPLAY( win->imagedisplay ),
		finger_cx, finger_cy, &win->scale_cx, &win->scale_cy );
}

static void
image_window_scale_changed( GtkGestureZoom *self, 
	gdouble scale, gpointer user_data )
{
	ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );

	image_window_set_scale_position( win, 
		scale * win->last_scale, win->scale_cx, win->scale_cy );
}

static void
image_window_drag_begin( GtkEventControllerMotion *self,
	gdouble start_x, gdouble start_y, gpointer user_data )
{
	ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );

	int window_left;
	int window_top;
	int window_width;
	int window_height;

	image_window_get_position( win, 
		&window_left, &window_top, &window_width, &window_height );

	win->drag_start_x = window_left;
	win->drag_start_y = window_top;
}

static void
image_window_drag_update( GtkEventControllerMotion *self,
	gdouble offset_x, gdouble offset_y, gpointer user_data )
{
	ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );

	image_window_set_position( win, 
		win->drag_start_x - offset_x,
		win->drag_start_y - offset_y );
}

static void
image_window_toggle( GSimpleAction *action, 
	GVariant *parameter, gpointer user_data )
{
	GVariant *state;

	state = g_action_get_state( G_ACTION( action ) );
	g_action_change_state( G_ACTION( action ), 
		g_variant_new_boolean( !g_variant_get_boolean( state ) ) );
	g_variant_unref( state );
} 

static void
image_window_fullscreen( GSimpleAction *action, 
	GVariant *state, gpointer user_data )
{
	ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );

	g_object_set( win, 
		"fullscreened", g_variant_get_boolean( state ),
		NULL );

	g_simple_action_set_state( action, state );
}

static void
image_window_control( GSimpleAction *action, 
	GVariant *state, gpointer user_data )
{
	ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );

	g_object_set( win->display_bar,
		"revealed", g_variant_get_boolean( state ),
		NULL );

	/* Disable most visualisation settings if the controls are hidden. It's
	 * much too confusing.
	 */
	if( win->tile_source )
		g_object_set( win->tile_source,
			"active", g_variant_get_boolean( state ),
			NULL );

	g_simple_action_set_state( action, state );
}

static void
image_window_info( GSimpleAction *action, 
	GVariant *state, gpointer user_data )
{
	ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );

	g_object_set( win->info_bar,
		"revealed", g_variant_get_boolean( state ),
		NULL );

	g_simple_action_set_state( action, state );
}

static void
image_window_next( GSimpleAction *action, GVariant *state, gpointer user_data )
{
	ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );

	if( win->tile_source ) {
		int n_pages = win->tile_source->n_pages;
		int page = VIPS_CLIP( 0, win->tile_source->page, n_pages - 1 );

		g_object_set( win->tile_source,
			"page", (page + 1) % n_pages,
			NULL );
	}
}

static void
image_window_prev( GSimpleAction *action, GVariant *state, gpointer user_data )
{
	ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );

	if( win->tile_source ) {
		int n_pages = win->tile_source->n_pages;
		int page = VIPS_CLIP( 0, win->tile_source->page, n_pages - 1 );

		g_object_set( win->tile_source,
			"page", page == 0 ? n_pages - 1 : page - 1,
			NULL );
	}
}

static int
image_window_find_scale( ImageWindow *win, VipsObject *context, 
	VipsImage *image,
	int left, int top, int width, int height,
	double *scale, double *offset )
{
	VipsImage **t = (VipsImage **) vips_object_local_array( context, 7 );

	double min, max;


	/* FIXME ... this should only look at visible tile_cache pixels ...
	 * don't render any new pixels.
	 */

	if( vips_extract_area( image, &t[0], 
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
image_window_scale( GSimpleAction *action, 
	GVariant *state, gpointer user_data )
{
	ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );

	VipsImage *image;

	if( win->tile_source &&
		(image = tile_source_get_image( win->tile_source )) ) {
		double image_scale;
		int left, top, width, height;
		VipsImage *context;
		double scale, offset;

		image_scale = image_window_get_scale( win );
		image_window_get_position( win, &left, &top, &width, &height );
		left /= image_scale;
		top /= image_scale;
		width /= image_scale;
		height /= image_scale;

		/* FIXME ... this will be incredibly slow, esp. for large 
		 * images. Instead, it would be better to just search the 
		 * cached tiles we have.
		 */

		context = vips_image_new();
		if( image_window_find_scale( win, VIPS_OBJECT( context ), image,
			left, top, width, height, &scale, &offset ) ) {
			image_window_error( win );
			g_object_unref( context );
			return;
		}
		g_object_unref( context );

		g_object_set( win->tile_source,
			"scale", scale,
			"offset", offset,
			NULL );
	}
}

static void
image_window_log( GSimpleAction *action, GVariant *state, gpointer user_data )
{
	ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );

	if( win->tile_source )
		g_object_set( win->tile_source,
			"log", g_variant_get_boolean( state ),
			NULL );

	g_simple_action_set_state( action, state );
}

static void
image_window_icc( GSimpleAction *action, GVariant *state, gpointer user_data )
{
	ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );

	if( win->tile_source )
		g_object_set( win->tile_source,
			"icc", g_variant_get_boolean( state ),
			NULL );

	g_simple_action_set_state( action, state );
}

static void
image_window_falsecolour( GSimpleAction *action, 
	GVariant *state, gpointer user_data )
{
	ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );

	if( win->tile_source )
		g_object_set( win->tile_source,
			"falsecolour", g_variant_get_boolean( state ),
			NULL );
	
	g_simple_action_set_state( action, state );
}

static void
image_window_radio( GSimpleAction *action,
	GVariant *parameter, gpointer user_data )
{
	g_action_change_state( G_ACTION( action ), parameter );
}

static void
image_window_mode( GSimpleAction *action,
	GVariant *state, gpointer user_data )
{
	ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );

	const gchar *str;
	TileSourceMode mode;

	str = g_variant_get_string( state, NULL );
	if( g_str_equal( str, "toilet-roll" ) ) 
		mode = TILE_SOURCE_MODE_TOILET_ROLL;
	else if( g_str_equal( str, "multipage" ) ) 
		mode = TILE_SOURCE_MODE_MULTIPAGE;
	else if( g_str_equal( str, "animated" ) ) 
		mode = TILE_SOURCE_MODE_ANIMATED;
	else if( g_str_equal( str, "pages-as-bands" ) ) 
		mode = TILE_SOURCE_MODE_PAGES_AS_BANDS;
	else
		/* Ignore attempted change.
		 */
		return;

	if( win->tile_source )
		g_object_set( win->tile_source,
			"mode", mode,
			NULL );

	g_simple_action_set_state( action, state );
}

static TileCacheBackground
background_to_enum( const char *str )
{
	TileCacheBackground background;

	if( g_str_equal( str, "checkerboard" ) ) 
		background = TILE_CACHE_BACKGROUND_CHECKERBOARD;
	else if( g_str_equal( str, "white" ) ) 
		background = TILE_CACHE_BACKGROUND_WHITE;
	else if( g_str_equal( str, "black" ) ) 
		background = TILE_CACHE_BACKGROUND_BLACK;
	else
		background = TILE_CACHE_BACKGROUND_CHECKERBOARD;

	return( background );
}

static void
image_window_background( GSimpleAction *action,
	GVariant *state, gpointer user_data )
{
	ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );
	TileCacheBackground background = 
		background_to_enum( g_variant_get_string( state, NULL ) );

	if( win->tile_cache ) 
		g_object_set( win->tile_cache,
			"background", background,
			NULL );

	g_simple_action_set_state( action, state );
}

static void
image_window_reset( GSimpleAction *action, 
	GVariant *state, gpointer user_data )
{
	ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );

	if( win->tile_source )
		g_object_set( win->tile_source,
			"falsecolour", FALSE,
			"log", FALSE,
			"icc", FALSE,
			"scale", 1.0,
			"offset", 0.0,
			NULL );

	if( win->tile_cache ) 
		g_object_set( win->tile_cache,
			"background", TILE_CACHE_BACKGROUND_CHECKERBOARD,
			NULL );
}

/* Animate the hiding of the properties widget by moving the paned separator
 * back to the value saved in @paned_position.
 */
static gboolean
image_window_paned_leave( gpointer win_ )
{
	ImageWindow *win;
	guint pos;
	gint max_width;

#ifdef DEBUG
	puts( "image_window_paned_leave" );
#endif /* DEBUG */

	win = VIPSDISP_IMAGE_WINDOW( win_ );
	pos = gtk_paned_get_position( GTK_PANED( win->paned ) );
	max_width = gtk_widget_get_width( GTK_WIDGET( win ) );

	if( pos + 80 < max_width ) {
		gtk_paned_set_position( GTK_PANED( win->paned ), pos + 80 );
		return( G_SOURCE_CONTINUE );
	}
	else {
		gtk_paned_set_position( GTK_PANED( win->paned ), max_width );
		g_object_set( win->properties, "revealed", FALSE, NULL );
		win->paned_is_animating = FALSE;
		return( G_SOURCE_REMOVE );
	}
}

/* Animate the showing of the properties widget by moving the paned separator
 * back to the value saved in @paned_position.
 */
static gboolean
image_window_paned_enter( gpointer win_ )
{
	ImageWindow *win;
	guint pos;

#ifdef DEBUG
	puts( "image_window_paned_enter" );
#endif /* DEBUG */

	win = VIPSDISP_IMAGE_WINDOW( win_ );
	pos = gtk_paned_get_position( GTK_PANED( win->paned ) );

	if( pos - 80 > win->paned_position ) {
		gtk_paned_set_position( GTK_PANED( win->paned ), pos - 80 );
		return( G_SOURCE_CONTINUE );
	}
	else {
		gtk_paned_set_position( GTK_PANED( win->paned ),
			       win->paned_position );
		win->paned_is_animating = FALSE;
		return( G_SOURCE_REMOVE );
	}
}

static gboolean
image_window_get_enable_animations( ImageWindow *win )
{
	gboolean enable_animations;

	g_object_get( win->gtk_settings, 
		"gtk-enable-animations", &enable_animations, 
		NULL );

	return( enable_animations );
}

/* This function is called when the Properties widget visibility is toggled
 * on/off by clicking "Properties" in the dropdown menu. The visibility is
 * backed by a GSetting named "properties", so it will persist after app
 * restart.
 *
 * action - the "properties" action defined in the GActionEntry table
 * 	image_window_entries.
 *
 * state - the new boolean value of the "properties" GSetting after the user
 * 	click.
 *
 * user_data - The ImageWindow. The Properties widget holds a pointer to the
 * 	ImageWindow as a GObject property, just like the InfoBar widget. The
 * 	Properties widget responds to changes in the VipsImage of the TileSource
 * 	held by ImageWindow by updating the grid of user input widgets based
 * 	on the properties of the new VipsImage.
 */
static void
image_window_properties( GSimpleAction *action, 
	GVariant *state, gpointer user_data )
{
	ImageWindow *win;

#ifdef DEBUG
	puts("image_window_properties");
#endif /* DEBUG */

	win = VIPSDISP_IMAGE_WINDOW( user_data );

	if( image_window_get_enable_animations( win ) ) {
		if( win->paned_is_animating )
			return;
			
		win->paned_is_animating = TRUE;

		if( g_variant_get_boolean( state ) ) {
			g_object_set( win->properties, "revealed", TRUE, NULL );
			g_timeout_add( 10,
				(GSourceFunc) image_window_paned_enter, win );
		}
		else
			g_timeout_add( 10,
				(GSourceFunc) image_window_paned_leave, win );
	}
	else
		g_object_set( win->properties, "revealed",
			g_variant_get_boolean( state ), NULL );

	g_simple_action_set_state( action, state );
}

static GActionEntry image_window_entries[] = {
	{ "magin", image_window_magin_action },
	{ "magout", image_window_magout_action },
	{ "bestfit", image_window_bestfit_action },
	{ "oneone", image_window_oneone_action },

	{ "duplicate", image_window_duplicate_action },
	{ "replace", image_window_replace_action },
	{ "saveas", image_window_saveas_action },
	{ "close", image_window_close_action },

	{ "fullscreen", image_window_toggle, NULL, "false", 
		image_window_fullscreen },
	{ "control", image_window_toggle, NULL, "false", 
		image_window_control },
	{ "info", image_window_toggle, NULL, "false", 
		image_window_info },
	{ "properties", image_window_toggle, NULL, "false",
		image_window_properties },

	{ "next", image_window_next },
	{ "prev", image_window_prev },
	{ "scale", image_window_scale },
	{ "log", image_window_toggle, NULL, "false", image_window_log },
	{ "icc", image_window_toggle, NULL, "false", image_window_icc },
	{ "falsecolour",
		image_window_toggle, NULL, "false", image_window_falsecolour },
	{ "mode", image_window_radio, "s", "'multipage'", image_window_mode },
	{ "background", image_window_radio, "s", 
		"'checkerboard'", image_window_background },

	{ "reset", image_window_reset },
};

/* This value needs to exceed the every possible window width, in logical
 * pixels. Currently the widest displays on the market boast a width of 7680
 * logical pixels, so it must at least be larger than that. There is no harm
 * in having a much larger value though. GtkPaned will place the separator
 * as far to the right as it can, when given a separator position that would
 * place it outside the bounds of the GtkPaned itself.
 */
#define MAX_WINDOW_WIDTH 100000

/* The initial position of the GtkPaned separator. When revealed, the Properties
 * menu needs to be visible even on the smallest displays. We don't want users
 * to just see the separator on the far right of the window, since it might not
 * be obvious what it is.
 */
#define INITIAL_PANED_POSITION 200

/* Initialize, or re-initialize, the position of the GtkPaned separator.
 * 
 * This function is responsible for setting the position of the separator after
 * the image has been allowed to render full-size. It is called using
 * g_timeout_add when a new image is opened.
 *
 * If animations are enabled, this function resets the separator position
 * to the far right when the Properties menu is hidden, so that it can slide
 * in when revealed.
 *
 * The GtkPaned UI template in imagewindow.ui uses a "position" value
 * large enough to guarantee the separator will be fully extended to the right
 * when the first image is opened. This ensures the ImageDisplay widget has
 * enough room to render the image full-size.
 *
 * GtkPaned will automatically remember the position of its separator when its
 * child ( the Properties widget ) is made visible again. We take advantage
 * of this behavior whenever animations are disabled.
 *
 * However, since we animate Properties widget enter/leave by changing the
 * separator position, we are responsible for remembering it when animations are
 * enabled, so that we know where the animation ends.
 *
 * If animations are enabled, when the user drags the separator, the new 
 * position is saved in ImageWindow::paned_position by the "notify::position"
 * signal handler, image_window_paned_position_changed.
 *
 * If animations are enabled, this function ( image_window_paned_init ) uses
 * the saved position to reset the separator position when a new image is
 * opened.
 *
 * @win_	gpointer (ImageWindow *) A generic pointer to the ImageWindow.
 * 		Must be passed as the user_data argument in g_timeout_add.
 *
 * This is a GSourceFunc that should only be called by g_timeout_add.
 */
static void
image_window_paned_init( gpointer win_ )
{
	ImageWindow *win;
	gboolean revealed;

#ifdef DEBUG
	puts( "image_window_paned_init" );
#endif /* DEBUG */

	win = VIPSDISP_IMAGE_WINDOW( win_ );

	g_object_get( win->properties, "revealed", &revealed, NULL );

	if( image_window_get_enable_animations( win ) ) {
		if( revealed ) {
			if( !win->is_paned_init )
				win->paned_position = INITIAL_PANED_POSITION;

			gtk_paned_set_position( GTK_PANED( win->paned ),
				win->paned_position );
		}
		else
			gtk_paned_set_position( GTK_PANED( win->paned ), MAX_WINDOW_WIDTH );
	}
	else if( !win->is_paned_init )
		gtk_paned_set_position( GTK_PANED( win->paned ), win->paned_position );

	win->is_paned_init = TRUE;
}

static void
image_window_paned_position_changed( GtkWidget *win_ )
{
	ImageWindow *win = VIPSDISP_IMAGE_WINDOW( win_ );

	gboolean revealed;

	if( win->paned_is_animating )
		return;

	g_object_get( win->properties, "revealed", &revealed, NULL );

	if( revealed )
		win->paned_position = gtk_paned_get_position( GTK_PANED( win->paned ) );
}

static void
image_window_init( ImageWindow *win )
{
	GtkEventController *controller;

#ifdef DEBUG
	puts("image_window_init");
#endif /* DEBUG */

	win->progress_timer = g_timer_new();
	win->last_progress_time = -1;
	win->scale_rate = 1.0;
	win->settings = g_settings_new( APPLICATION_ID );
	win->paned_is_animating = FALSE;
	win->paned_position = INITIAL_PANED_POSITION;
	win->is_paned_init = FALSE;

	gtk_widget_init_template( GTK_WIDGET( win ) );

	/* Use CSS styles from "gtk/imagewindow.css".
	 */
	win->display = gdk_display_get_default();
	win->provider = gtk_css_provider_new();
	gtk_css_provider_load_from_resource( win->provider,
		      APP_PATH "/imagewindow.css" );
	gtk_style_context_add_provider_for_display( win->display,
		GTK_STYLE_PROVIDER( win->provider ),
		GTK_STYLE_PROVIDER_PRIORITY_APPLICATION );

	/* Get GtkSettings for this GdkDisplay, so we can check if animations
	 * are enabled. If not, then Properties menu enter/leave will not be
	 * animated.
	 */
	win->gtk_settings = gtk_settings_get_for_display( win->display );

	/* Test to make sure gtk-enable-animations is being respected properly. 
	g_object_set( win->gtk_settings, "gtk-enable-animations",
		FALSE, NULL );
	 */
	
	g_object_set( win->display_bar,
		"image-window", win,
		NULL );
	g_object_set( win->info_bar,
		"image-window", win,
		NULL );
	g_object_set( win->properties,
		"image-window", win,
		NULL );

	g_signal_connect_object( win->progress_cancel, "clicked", 
		G_CALLBACK( image_window_cancel_clicked ), win, 0 );

	g_signal_connect_object( win->error_bar, "response", 
		G_CALLBACK( image_window_error_response ), win, 0 );

	g_action_map_add_action_entries( G_ACTION_MAP( win ),
		image_window_entries, G_N_ELEMENTS( image_window_entries ),
		win );

	controller = GTK_EVENT_CONTROLLER( gtk_event_controller_key_new() );
	g_signal_connect( controller, "key-pressed", 
		G_CALLBACK( image_window_key_pressed ), win );
	g_signal_connect( controller, "key-released", 
		G_CALLBACK( image_window_key_released ), win );
	gtk_widget_add_controller( win->imagedisplay, controller );

	controller = GTK_EVENT_CONTROLLER( gtk_event_controller_motion_new() );
	g_signal_connect( controller, "motion", 
		G_CALLBACK( image_window_motion ), win );
	gtk_widget_add_controller( win->imagedisplay, controller );

	/* Panning windows should use scroll to zoom, according to the HIG.
	 */
	controller = GTK_EVENT_CONTROLLER( gtk_event_controller_scroll_new( 
		GTK_EVENT_CONTROLLER_SCROLL_VERTICAL ) );
	g_signal_connect( controller, "scroll", 
		G_CALLBACK( image_window_scroll ), win );
	gtk_widget_add_controller( win->imagedisplay, controller );

	controller = GTK_EVENT_CONTROLLER( gtk_gesture_zoom_new() );
	g_signal_connect( controller, "begin",
		G_CALLBACK( image_window_scale_begin ), win );
	g_signal_connect( controller, "scale-changed",
		G_CALLBACK( image_window_scale_changed ), win );
	gtk_widget_add_controller( win->imagedisplay, controller );

	/* And drag to pan.
	 */
	controller = GTK_EVENT_CONTROLLER( gtk_gesture_drag_new() );
	g_signal_connect( controller, "drag-begin", 
		G_CALLBACK( image_window_drag_begin ), win );
	g_signal_connect( controller, "drag-update", 
		G_CALLBACK( image_window_drag_update ), win );
	gtk_widget_add_controller( win->imagedisplay, controller );

	g_settings_bind( win->settings, "control",
		G_OBJECT( win->display_bar ),
		"revealed", 
		G_SETTINGS_BIND_DEFAULT );

	g_settings_bind( win->settings, "info",
		G_OBJECT( win->info_bar ),
		"revealed", 
		G_SETTINGS_BIND_DEFAULT );

	g_settings_bind( win->settings, "properties",
		G_OBJECT( win->properties ),
		"revealed", 
		G_SETTINGS_BIND_DEFAULT );

	/* Initial menu state from settings.
	 */
	change_state( GTK_WIDGET( win ), "control", 
		g_settings_get_value( win->settings, "control" ) );
	change_state( GTK_WIDGET( win ), "info", 
		g_settings_get_value( win->settings, "info" ) );
	change_state( GTK_WIDGET( win ), "properties", 
		g_settings_get_value( win->settings, "properties" ) );

	if( image_window_get_enable_animations( win ) )
		/* Connect a signal handler that updates the saved paned
		 * separator position whenever the user drags it around.
	 	 */
		g_signal_connect_swapped( win->paned, "notify::position",
			G_CALLBACK( image_window_paned_position_changed ),
			win );
}

static void
image_window_pressed_cb( GtkGestureClick *gesture,
	guint n_press, double x, double y, ImageWindow *win )
{
	gtk_popover_set_pointing_to( GTK_POPOVER( win->right_click_menu ),
		&(const GdkRectangle){ x, y, 1, 1 } );

	/* This produces a lot of warnings :( not sure why. I tried calling
	 * gtk_popover_present() in realize to force allocation, but it didn't
	 * help.
	 */
	gtk_popover_popup( GTK_POPOVER( win->right_click_menu ) );
}

#define BIND( field ) \
	gtk_widget_class_bind_template_child( GTK_WIDGET_CLASS( class ), \
		ImageWindow, field );

static void
image_window_class_init( ImageWindowClass *class )
{
	G_OBJECT_CLASS( class )->dispose = image_window_dispose;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( class ),
		APP_PATH "/imagewindow.ui");

	BIND( right_click_menu );
	BIND( title );
	BIND( subtitle );
	BIND( gears );
	BIND( progress_bar );
	BIND( progress );
	BIND( progress_cancel );
	BIND( error_bar );
	BIND( error_label );
	BIND( scrolled_window );
	BIND( imagedisplay );
	BIND( display_bar );
	BIND( info_bar );
	BIND( paned );
	BIND( properties );

	gtk_widget_class_bind_template_callback( GTK_WIDGET_CLASS( class ),
		image_window_pressed_cb );

	image_window_signals[SIG_STATUS_CHANGED] = g_signal_new( 
		"status-changed",
		G_TYPE_FROM_CLASS( class ),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0 ); 

	image_window_signals[SIG_CHANGED] = g_signal_new( "changed",
		G_TYPE_FROM_CLASS( class ),
		G_SIGNAL_RUN_LAST,
		0,
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0 ); 

}

ImageWindow *
image_window_new( VipsdispApp *app )
{
	return( g_object_new( IMAGE_WINDOW_TYPE, "application", app, NULL ) );
}

void
image_window_set_tile_source( ImageWindow *win, TileSource *tile_source )
{
	VipsImage *image;
	char *title;

	VIPS_UNREF( win->tile_source );
	VIPS_UNREF( win->tile_cache );

	win->tile_source = tile_source;
	g_object_ref( tile_source );
	win->tile_cache = tile_cache_new( win->tile_source );

	g_object_set( win->imagedisplay,
		"tile-cache", win->tile_cache,
		NULL );

	g_signal_connect_object( win->tile_source, "preeval", 
		G_CALLBACK( image_window_preeval ), win, 0 );
	g_signal_connect_object( win->tile_source, "eval", 
		G_CALLBACK( image_window_eval ), win, 0 );
	g_signal_connect_object( win->tile_source, "posteval", 
		G_CALLBACK( image_window_posteval ), win, 0 );

	g_signal_connect_object( win->tile_source, "changed", 
		G_CALLBACK( image_window_tile_source_changed ), win, 0 );

	if( !(title = (char *) tile_source_get_path( tile_source )) ) 
		title = "Untitled";
	gtk_label_set_text( GTK_LABEL( win->title ), title );

	if( (image = tile_source_get_base_image( tile_source )) ) {
		char str[256];
		VipsBuf buf = VIPS_BUF_STATIC( str );

		vips_buf_appendf( &buf, "%dx%d, ", 
			image->Xsize, image->Ysize );
		if( tile_source->n_pages > 1 )
			vips_buf_appendf( &buf, "%d pages, ", 
				tile_source->n_pages );
		if( vips_image_get_coding( image ) == VIPS_CODING_NONE ) 
			vips_buf_appendf( &buf,
				g_dngettext( GETTEXT_PACKAGE,
					" %s, %d band, %s",
					" %s, %d bands, %s",
					image->Bands ),
				vips_enum_nick( VIPS_TYPE_BAND_FORMAT,
					image->BandFmt ),
				vips_image_get_bands( image ),
				vips_enum_nick( VIPS_TYPE_INTERPRETATION,
					image->Type ) );
		else
			vips_buf_appendf( &buf, ", %s",
				vips_enum_nick( VIPS_TYPE_CODING,
					vips_image_get_coding( image ) ) );
		vips_buf_appendf( &buf, ", %g x %g p/mm",
			image->Xres, image->Yres );
		gtk_label_set_text( GTK_LABEL( win->subtitle ), 
			vips_buf_all( &buf ) );
	}

	/* Initial state.
	 */
	tile_source->active = 
		g_settings_get_boolean( win->settings, "control" );

	/* Everything is set up ... start loading the image.
	 */
	tile_source_background_load( tile_source );

	image_window_changed( win );
}

TileSource *
image_window_get_tile_source( ImageWindow *win )
{
	return( win->tile_source );
}

GtkWidget *
image_window_get_paned( ImageWindow *win )
{
	return( win->paned );
}

GSettings *image_window_get_settings( ImageWindow *win )
{
	return( win->settings );
}

void
image_window_open( ImageWindow *win, GFile *file )
{
	char *path;
	TileSource *tile_source;

	path = g_file_get_path( file );

	if( !(tile_source = tile_source_new_from_file( path )) ) {
		image_window_error( win ); 
		g_free( path );
		return;
	}

	/* Re-initialize from settings.
	 */
	change_state( GTK_WIDGET( win ), "properties", 
		g_settings_get_value( win->settings, "properties" ) );

	/* Give UI a chance to load before setting the paned separator position.
	 */
	image_window_set_tile_source( win, tile_source );
	g_timeout_add( 500, (GSourceFunc) image_window_paned_init, win );

	g_object_unref( tile_source );
	g_free( path );
}

void
image_window_get_mouse_position( ImageWindow *win, 
	double *x_image, double *y_image )
{
	imagedisplay_gtk_to_image( VIPSDISP_IMAGEDISPLAY( win->imagedisplay ), 
		win->last_x_gtk, win->last_y_gtk, x_image, y_image );
}

