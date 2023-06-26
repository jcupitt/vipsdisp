/*
#define DEBUG
 */

#include "vipsdisp.h"

/* How much to scale view by each frame.
 */
#define SCALE_STEP (1.1)

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

	int og_width;
	int og_height;

	/* For pinch zoom, zoom position that we started.
	 */
	double last_scale;
	double scale_cx;
	double scale_cy;

	/* For animating zoom. 
	 */
	guint tick_handler;
	double scale_rate;

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
	GtkWidget *main_box;
	GtkWidget *metadata;
	GtkWidget *metadata_label;
	GtkWidget *metadata_close_button;
	GtkWidget *metadata_window;
	GtkWidget *metadata_search_entry;

	GtkStringList *list_model;

	/* Throttle progress bar updates to a few per second with this.
	 */
	GTimer *progress_timer;
	double last_progress_time;

	gint64 last_frame_time;

	GSettings *settings;
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

	/* You'd think we could just update the progress bar now, but it
	 * seems to trigger a lot of races. Instead, set an idle handler and 
	 * do the update there. 
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
	gtk_info_bar_set_revealed( GTK_INFO_BAR( win->error_bar ), FALSE );
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
		VIPS_UNREF( tile_source );
	}

	gtk_window_get_default_size( GTK_WINDOW( win ), &width, &height );
	gtk_window_set_default_size( GTK_WINDOW( new ), width, height );

	copy_state( GTK_WIDGET( new ), GTK_WIDGET( win ), "control" );
	copy_state( GTK_WIDGET( new ), GTK_WIDGET( win ), "info" );
	copy_state( GTK_WIDGET( new ), GTK_WIDGET( win ), "metadata" );
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

static void
image_window_replace_action( GSimpleAction *action, 
	GVariant *parameter, gpointer user_data )
{
	ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );

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
		VIPS_UNREF( file );
	}

	g_signal_connect( dialog, "response", 
		G_CALLBACK( image_window_replace_response ), win );

	gtk_widget_show( dialog );
}

static void
image_window_saveas_response( GtkDialog *dialog, 
	gint response_id, gpointer user_data )
{
	ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );

	GFile *file;

	/* We need to pop down immediately so we expose the cancel
	 * button.
	 */
	file = gtk_file_chooser_get_file( GTK_FILE_CHOOSER( dialog ) );
	image_window_error_hide( win ); 
	gtk_window_destroy( GTK_WINDOW( dialog ) );

	if( response_id == GTK_RESPONSE_ACCEPT &&
		tile_source_write_to_file( win->tile_source, file ) ) 
		image_window_error( win );

	VIPS_UNREF( file ); 
}

static void
image_window_saveas_action( GSimpleAction *action, 
	GVariant *parameter, gpointer user_data )
{
	ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );

	if( win->tile_source ) {
		GtkWidget *dialog;
		GFile *file;

		dialog = gtk_file_chooser_dialog_new( "Save file",
			GTK_WINDOW( win ) , 
			GTK_FILE_CHOOSER_ACTION_SAVE,
			"_Cancel", GTK_RESPONSE_CANCEL,
			"_Save", GTK_RESPONSE_ACCEPT,
			NULL );
		gtk_window_set_modal( GTK_WINDOW( dialog ), TRUE );

		if( (file = tile_source_get_file( win->tile_source )) ) {
			gtk_file_chooser_set_file( GTK_FILE_CHOOSER( dialog ), 
				file, NULL );
			VIPS_UNREF( file );
		}

		g_signal_connect( dialog, "response", 
			G_CALLBACK( image_window_saveas_response ), win );

		gtk_widget_show( dialog );
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

	double x_image;
	double y_image;

#ifdef DEBUG
	printf( "image_window_tick: dt = %g\n", dt );
#endif /*DEBUG*/

	image_window_get_mouse_position( win, &x_image, &y_image );

	if( win->scale_rate != 1.0 ) {
		double scale = image_window_get_scale( win );
		double new_scale = (dt * (win->scale_rate - 1.0) + 1.0) * scale;

		image_window_set_scale_position( win, 
			new_scale, x_image, y_image );
	}

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
		image_window_bestfit( win );
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

				image_window_set_scale_centre( win, scale );

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

/* Size of alphabet index array. This number should be twice the size
 * of the alphabet, which is 128 in this case for ASCII (not extended).
 * The purpose of this extra space is explained later.
 */
#define ALPHABET_SIZE 256

/* The Match object will hold the index @i of the first character of the match
 * and the number of mismatched characters @k within that match. The Match
 * objects are also linked list nodes.
 */
typedef struct Match Match;
struct Match {
	int i;
	int k;
	gchar *text;
	gchar *pattern;
	int pattern_length;
};

Match *
Match_new( int i, int k, gchar *text, gchar *pattern )
{
	Match *t;
	t = g_malloc( sizeof( Match ) );
	t->i = i;
	t->k = k;
	g_assert( text );
	t->text = g_strdup( text );
	t->pattern = pattern;
	t->pattern_length = strlen( pattern );
	return t;
}

void
Match_free( gpointer match_, gpointer user_data )
{
	Match *match = (Match *) match_;
	if ( match ) {
		g_free( match->text );
		g_free( match );
	}		
}

void
Match_print( gpointer match_, gpointer user_data )
{
	Match *match = (Match *) match_;
	printf( "position: %d, errors: %d, text: \"%s\", pattern: \"%s\", pattern_length: %d\n",
		match->i, match->k, match->text, match->pattern,
		match->pattern_length );
}

/* An array of lists. There are 128 linked lists in total - one for each
 * character in our 128-character alphabet. The last 128 lists characters are
 * added to the linked lists as needed. Using a power of lets us quickly access
 * the array in a cyclic manner by modding using the bitwise & operator. 
 *
 * Each list will contain the offsets for each occurence of that character, or a
 * single placeholder offset of -1 if no occurences are found.
 *
 * The offset is the distance of the character to the left of the end of
 * pattern, (i.e., measured by counting to the left from the end of pattern),
 * for a given character and a given instance of pattern.
 */
GList *alpha[ALPHABET_SIZE];

/* This function initializes the @alpha and @count arrays to look like this:
 *
 *     alpha = [ [ -1 ] ] * ALPHABET_SIZE 	where the inner brackets are a GList.
 *
 *     count = [m] * m
 *
 * @alpha will be an array of linked lists. Characters in the pattern
 * that do not occur or that occur exactly once in the text will have
 * corresponding linked lists with length one. Characters in the pattern that
 * occur in the text more than once will have corresponding linked lists with
 * length greater than one.
 *
 * The first m - 1  elements of @count will be skipped on the first iteration of
 * the cyclic array (since no match can be shorter than the @pattern). Note that
 * the values in @count are reset to m once they are no longer needed, until the
 * next loop around @count.
 *
 * @p - pattern string
 * @m - pattern length
 * @alpha - array of GList. See above.
 * @count - circular buffer for counts of matches
 */
void
preprocess( char *p, int m, GList *alpha[], int count[], int max_pattern_size )
{
	int i, j;

	for ( i = 0; i < ALPHABET_SIZE; i++ ) {
		alpha[i] = NULL;
		alpha[i] = g_list_append( alpha[i], GINT_TO_POINTER( -1 ) );
	}

	for ( i = 0, j = 128; i < m; i++, p++ ) {
		if ( GPOINTER_TO_INT( alpha[*p]->data ) == -1 )
			alpha[*p]->data = GINT_TO_POINTER( m - i - 1 );
		else
			alpha[*p] = g_list_append( alpha[*p],
				GINT_TO_POINTER( m - i - 1 ) );
	}

	for ( i = 0; i < max_pattern_size; i++ )
		count[i] = m;
}

void
increment_offset( gpointer off_, gpointer args_ )
{
	gpointer *args = (gpointer *) args_;
	int i = GPOINTER_TO_INT( args[0] );
	int max_pattern_size = GPOINTER_TO_INT( args[1] );
	int *count = (int *) args[2];
	gint off = GPOINTER_TO_INT( off_ ) ;
	count[(i + off) % max_pattern_size]--;
}

gint
match_compare( gconstpointer a_, gconstpointer b_ )
{
	Match *a = (Match *) a_;
	Match *b = (Match *) b_;

	if ( a->k == b->k )
		return 0;
	else if (a->k < b->k )
		return -1;
	else return 1;
}

/* Find the position of the first character and number of mismatches of every
 * fuzzy match in a string @t with @k or fewer mismatches. Uses the array of
 * GList @alpha and the array of counts @count prepared by the preprocess
 * function.
 * @t - text string
 * @n - length of text string
 * @m - length of the pattern used to create @alpha and @count
 * @k - maximum number of allowed mismatches
 * @alpha - array of GList. See above.
 * @count - circular buffer for counts of matches
 */
GList *
search( char *t, int n, int m, int k, GList *alpha[], int count[], char *pattern, int max_pattern_size )
{
	int i, off, j;
	Match *match;
	GList *l0 = NULL, *l1 = NULL;
	char *text = t;

	/* Walk the text @t, which has length @n.
	 */
	for ( i = 0; i < n; i++ ) {
		/* If the current character in @t is in pattern, its
		 * corresponding list in @alpha will have a non-negative offset,
		 * thanks to the workdone by the preprocess function. If so, we
		 * need to decrement the counts in the circular buffer @count
		 * corresponding to the index of the character in the text and
		 * the offsets the lists corresponding to those characters,
		 * which the preprocess function prepared.
		 * 
		 * Note that we will only ever need m counts at a time, and
		 * we reset them to @m when we are done with them, in case
		 * they are needed when the text wraps 256 characters.
		 */
		l0 = alpha[*t++];
		off = GPOINTER_TO_INT( l0->data );
		if ( off >= 0 ) {
			gpointer t[3] = {
				GINT_TO_POINTER( i ),
				GINT_TO_POINTER( max_pattern_size ),
				(gpointer) count,
			};
			g_list_foreach( l0, increment_offset, t );

		}

		/* If the count in @count corresponding to the current index in
		 * the text is no greater than @k, the number of mismatches we
		 * allow, then the pattern instance is reported as a fuzzy
		 * match. The position of the first letter in the match is
		 * calculated using the pattern length and the index of the last
		 * character in the match The number of mismatches is calculated
		 * from the number of matches.
		 */
		if ( i >= m - 1 && count[i % max_pattern_size] <= k ) {
			// DBG
			//printf( "i: %d, m: %d\n", i, m );
			g_assert( i - m + 1 >= 0 );
			g_assert( text );
			match = Match_new( i - m + 1, count[i % max_pattern_size], text, pattern );
			l1 = g_list_append( l1, match );
		}

		/* The count in @count corresponding to the current index in
		 * text is no longer needed, so we reset it to @m until we
		 * need it on the next wraparound.
		 */
		count[i % max_pattern_size] = m;
	}

	/* Sort by increasing k.
	 */
	l1 = g_list_sort( l1, match_compare );

	return l1;
}


/* TODO: 
 * Produce a custom widget for cells in the second column (there are only two
 * columns).
 *
 * The type of widget used will depend on the GType of the GValue corresponding
 * to the field.
 *
 * For now, complicated data types can be ignored.
 *
 * A GtkBox should be used as the generic container.
 */
static void
factory_setup( GtkListItemFactory *factory, GtkListItem *list_item )
{
	GtkWidget *box;

	box = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );
	gtk_widget_set_halign( box, GTK_ALIGN_START );
	gtk_list_item_set_child( list_item, box );
}

/* The bind function for the field name factory, corresponding to the first
 * column. In this case, the field name is the metadata field name.
 * This column not editable - it is a column of labels.
 */
static void
field_name_factory_bind( GtkListItemFactory *factory, GtkListItem *list_item )
{
	GtkWidget *box, *label;
	GtkStringObject *string_object;
	const char *markup;

	box  = gtk_list_item_get_child( list_item );
	label = gtk_label_new( NULL );
	gtk_box_append( GTK_BOX( box ), label );

	string_object = GTK_STRING_OBJECT( gtk_list_item_get_item( list_item ) );

	markup = gtk_string_object_get_string( string_object );

	gtk_label_set_markup( GTK_LABEL( label ), markup );
}

/* The bind function for the other factories, corresponding to columns after
 * the field name column.
 *
 * TODO: 
 * Produce a custom widget for cells in the second column (there are only two
 * columns).
 *
 * The type of widget used will depend on the GType of the GValue corresponding
 * to the field.
 *
 * For now, complicated data types can be ignored.
 */

static void
value_factory_bind( GtkListItemFactory *factory, GtkListItem *list_item, gpointer user_data )
{
	GtkWidget *input_box, *t, *box;
	GtkStringObject *string_object;
	char *field_name;
	GString *field_name_string;
	VipsImage *image; 
	char str[256];
	VipsObjectClass *oclass;
	GType otype;
	GParamSpec *pspec;
	VipsArgumentClass *argument_class;
	VipsArgumentInstance *argument_instance;


	image = VIPS_IMAGE( user_data );

	/* It is crucial to zero the GValue whose address we pass to
	 * vips_image_get. Otherwise, we will get runtime errors.
	 */
	GValue value = { 0 };

	input_box = gtk_list_item_get_child( list_item );
	string_object = GTK_STRING_OBJECT( gtk_list_item_get_item( list_item ) );
	field_name = gtk_string_object_get_string( string_object );
	field_name_string = g_string_new( field_name );
	g_string_replace( field_name_string, "<b>", "", 0 );
	g_string_replace( field_name_string, "</b>", "", 0 );
	field_name = field_name_string->str;

	/* Get the value of the given field from the image.
	 * Check the GType of @value, and select the appropriate user input
         * widget. Initialize the widget with @value.
	 * TODO
 	 */
	gboolean use_string = FALSE; 
	if( vips_object_get_argument( VIPS_OBJECT( image ), field_name,
		&pspec, &argument_class, &argument_instance ) ) {
		//g_warning( "%s", vips_error_buffer() );
		vips_error_clear();
		//return;
		vips_image_get( image, field_name, &value );
		use_string = TRUE;
	} else {
		otype = G_PARAM_SPEC_VALUE_TYPE( pspec );

		/* For now, skip VipsImage and VipsObject types.
		*/
		if( g_type_is_a( otype, VIPS_TYPE_IMAGE ) )
			return;
		else if( g_type_is_a( otype, VIPS_TYPE_OBJECT ) &&
			(oclass = g_type_class_ref( otype )) )
			return;
	}

	/* Add a user input widget for this property to the input_box. The widget
	 * chosen depends on the type of the property. Set the initial value of
	 * the user input widget to the default value for the property.
	 */
	if( use_string || G_IS_PARAM_SPEC_STRING( pspec ) ) {
		if ( !use_string ) {
			GParamSpecString *pspec_string = G_PARAM_SPEC_STRING( pspec );
			GtkEntryBuffer* buffer =
				gtk_entry_buffer_new( pspec_string->default_value, -1 );

			t = gtk_text_new_with_buffer( buffer );
		} else {
			GType type = G_VALUE_TYPE( &value );
			if ( type == G_TYPE_STRING ) {
				const string_value = g_value_get_string( &value );
				GtkEntryBuffer* buffer =
					gtk_entry_buffer_new( string_value, -1 );

				t = gtk_text_new_with_buffer( buffer );
			
			} else if ( type == G_TYPE_ENUM ) {
				//puts("enum");
			} else if ( type == G_TYPE_INT ) {
				//puts("int");
				t = gtk_spin_button_new_with_range( -G_MAXINT + 1, G_MAXINT, 1 );
				gtk_spin_button_set_value( GTK_SPIN_BUTTON( t ),
					g_value_get_int( &value ) );
			} else if ( type == G_TYPE_UINT ) {
				//puts("uint");
			} else if ( type == G_TYPE_INT64 ) {
				//puts("int64");
			} else if ( type == G_TYPE_UINT64 ) {
				//puts("int64");
			} else if ( type == G_TYPE_LONG ) {
				//puts("long");
			} else if ( type == G_TYPE_ULONG ) {
				//puts("ulong");
			} else if ( type == G_TYPE_BOOLEAN ) {
				//puts("boolean");
			} else if ( type == G_TYPE_FLOAT ) {
				//puts("float");
			} else if ( type == G_TYPE_DOUBLE ) {
				//puts("double");
			} else if ( type == G_TYPE_FLAGS ) {
				//puts("flags");
			} else if ( type == G_TYPE_BOXED ) {
				//puts("boxed");
			} else if ( type = VIPS_TYPE_REF_STRING ) {
				VipsArea *area;
				area = g_value_get_boxed( &value );
				char *string_value = g_strdup( vips_ref_string_get( area, NULL ) );
				GtkEntryBuffer* buffer =
					gtk_entry_buffer_new( string_value, -1 );
				t = gtk_text_new();
				gtk_text_set_buffer( GTK_TEXT( t ), buffer );
			} else {
				puts("unknown type");
			}
		}
	}
	else if( G_IS_PARAM_SPEC_BOOLEAN( pspec ) ) {
		GParamSpecBoolean *pspec_boolean = G_PARAM_SPEC_BOOLEAN( pspec );
		t = gtk_check_button_new();
		gtk_check_button_set_active( GTK_CHECK_BUTTON( t ),
			pspec_boolean->default_value );
	}
	else if( G_IS_PARAM_SPEC_ENUM( pspec ) ) {
		GParamSpecEnum *pspec_enum = G_PARAM_SPEC_ENUM( pspec );
		const char **property_nicknames =
			g_malloc( (pspec_enum->enum_class->n_values + 1) * sizeof( char * ) );

		for( int i = 0; i < pspec_enum->enum_class->n_values; ++i ) {
			property_nicknames[i] =
				pspec_enum->enum_class->values[i].value_nick;
		}
		property_nicknames[pspec_enum->enum_class->n_values] = NULL;
		t = gtk_drop_down_new_from_strings( property_nicknames );
		gtk_drop_down_set_selected( GTK_DROP_DOWN( t ),
			pspec_enum->default_value );
	}
	else if( G_IS_PARAM_SPEC_INT64( pspec ) ) {
		GParamSpecInt64 *pspec_int64 = G_PARAM_SPEC_INT64( pspec );
		t = gtk_spin_button_new_with_range( pspec_int64->minimum,
			pspec_int64->maximum, 1 );

		gtk_spin_button_set_value( GTK_SPIN_BUTTON( t ),
			(gint64)pspec_int64->default_value );
	}
	else if( G_IS_PARAM_SPEC_INT( pspec )) {
		GParamSpecInt *pspec_int = G_PARAM_SPEC_INT( pspec );
		t = gtk_spin_button_new_with_range( pspec_int->minimum,
			pspec_int->maximum, 1 );

		gtk_spin_button_set_value( GTK_SPIN_BUTTON( t ),
			(int)pspec_int->default_value );
	}
	else if( G_IS_PARAM_SPEC_UINT64( pspec ) ) {
		GParamSpecUInt64 *pspec_uint64 = G_PARAM_SPEC_UINT64( pspec );
		t = gtk_spin_button_new_with_range( pspec_uint64->minimum,
			pspec_uint64->maximum, 1 );

		gtk_spin_button_set_value( GTK_SPIN_BUTTON( t ),
			(guint64)pspec_uint64->default_value );
	}
	else if( G_IS_PARAM_SPEC_DOUBLE( pspec ) ) {
		GParamSpecDouble *pspec_double = G_PARAM_SPEC_DOUBLE( pspec );

		t = gtk_spin_button_new_with_range( pspec_double->minimum,
			pspec_double->maximum, 1 );

		gtk_spin_button_set_value( GTK_SPIN_BUTTON( t ),
			pspec_double->default_value );
	}
	else if( G_IS_PARAM_SPEC_BOXED( pspec ) ) {	
		if( g_type_is_a( otype, VIPS_TYPE_ARRAY_INT ) ) {
			/* No default values exist for ParamSpecBoxed, so make
			 * some up for now.
			 */
			t = gtk_spin_button_new_with_range( 0, 1000, 1 );
			gtk_spin_button_set_value( GTK_SPIN_BUTTON( t ), 0 );
		}
		else if( g_type_is_a( otype, VIPS_TYPE_ARRAY_DOUBLE ) ) {
			/* No default values exist for ParamSpecBoxed, so make
			 * some up for now.
			 */
			t = gtk_spin_button_new_with_range( 0, 1000, .1 );
			gtk_spin_button_set_value( GTK_SPIN_BUTTON( t ), 0 );
		}
		else if( g_type_is_a( otype, VIPS_TYPE_ARRAY_IMAGE ) ) {
			/* Ignore VipsImage-type parameters for now.
			 */
			return;
		}
		else {
			/* Ignore parameters of unrecognized type for now.
			 */
			return;
		}
	}
	else {
		printf("Unknown type for property \"%s\"\n", field_name);
		g_object_ref_sink( input_box );
		return;
	}

	/* Create a box to contain the user input widget "t", and add a tooltip
	 * to the box. The tooltip contains the "blurb" for the property.
	 * Make the user input widget "t" fill the box horizontally.
	 * Append that container box to the "input_box".
	 */
	box = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
	if ( !use_string )
		gtk_widget_set_tooltip_text( GTK_WIDGET( box ),
			g_param_spec_get_blurb( pspec ) );

	gtk_widget_set_hexpand( t, TRUE );
	gtk_box_append( GTK_BOX( box ), t );
	gtk_box_append( GTK_BOX( input_box ), box );

}

/* TODO
 * To save, loop over the list items and get their values, and use them update
 * the VipsImage. This will happen when a button is clicked. There will also be
 * a cancel button. This will look a lot like the save options UI.
 */

void
markup_in_string_by_match( gpointer match_, gpointer markup_ )
{
	Match *match;
	GString *markup;
	gchar *raw_str;
	GRegex *regex;
	gchar *replacement;

	match = (Match *) match_;
	markup = (GString *) markup_;

	if ( match->pattern && match->pattern[0] && !match->k ) {
		replacement = g_strdup_printf( "<>%s</>", match->pattern );
		g_string_replace( markup, match->pattern, replacement, 0 );
		g_free( replacement );
	}
}

gchar *
get_markup_from_match( GList *match )
{
	const gchar *raw_str;
	GString *markup;
	gchar *str;
	Match *first = (Match *) match->data;

	markup = g_string_new( g_strdup( first->text ) );

	g_list_foreach( match, markup_in_string_by_match, markup );

	g_string_replace( markup, "<>", "<b>", 0 );
	g_string_replace( markup, "</>", "</b>", 0 );

	return markup->str;
}

void
append_field_name( gpointer data, gpointer user_data )
{
	ImageWindow *win;
	const gchar *markup;
	gchar *str;
	GList *match_list;
	Match *match;

	match_list = (GList *) data;
	match = (Match *) match_list->data;
	win = VIPSDISP_IMAGE_WINDOW( user_data );
	gtk_string_list_append( win->list_model, match->text );
}

void
append_markup_field_name( gpointer data, gpointer user_data )
{
	ImageWindow *win;
	gchar *str, *markup;
	GList *match_list;
	Match *match;

	match_list = (GList *) data;
	win = VIPSDISP_IMAGE_WINDOW( user_data );
	markup = get_markup_from_match( match_list );
	g_assert( win->list_model );
	gtk_string_list_append( win->list_model, markup );
}

gint
match_list_compare( gconstpointer a_, gconstpointer b_ )
{
	GList *a = (GList *) a_;
	GList *b = (GList *) b_;

	Match *match_a = (Match *) a->data;
	Match *match_b = (Match *) b->data;

	return match_compare( match_a, match_b );
}

void
append_if_match( gpointer data, gpointer user_data )
{
	GList *first, *match;
	gchar *haystack, *needle;
	GList **list_ptr;
	int n, m, k;

	list_ptr = (GList **) user_data;

	haystack = (gchar *) data;

	first = g_list_first( *list_ptr );
	needle = (gchar *) first->data;

	gssize haystack_len = -1;

	n = strlen( haystack );
	int count[n];
	m = strlen( needle );
	k = n;

	preprocess( needle, m, alpha, count, n );

	match = search( haystack, n, m, k, alpha, count, needle, n );

	if ( g_list_length( match ) )
		*list_ptr = g_list_append( *list_ptr, match );
}

GList *
find_strings_with_substring( GList *haystacks, gchar *needle )
{
	GList *found = NULL;

	found = g_list_append( found, needle );

	g_list_foreach( haystacks, append_if_match, &found );

	found = g_list_remove( found, (gconstpointer) needle );

	return found;
}

/* This is the callback function called whenever the GtkSearchEntry is modified
 * by the user.
 */
static void
search_changed( GtkWidget *search_entry, gpointer user_data )
{
	ImageWindow *win;
	GtkStringList *list_model;
	char **field_names;
	GList *field_names_list = NULL;
	char *text;

	/* Initialize GList pointers to NULL.
	 */
	GList *found, *found0, *found1, *s0, *s1, *t;
	found = found0 = found1 = s0 = s1 = t = NULL;

	win = VIPSDISP_IMAGE_WINDOW( user_data );

	while ( gtk_string_list_get_string( win->list_model, 0 ) )
		gtk_string_list_remove( win->list_model, 0 );

	found = NULL;
	field_names = vips_image_get_fields( win->tile_source->image );
	char **p;
	p = field_names;
	while ( *p ) {
		field_names_list = g_list_append( field_names_list, *p );
		p++;
	}
		
	text = gtk_editable_get_text( GTK_EDITABLE( search_entry) );
	found = find_strings_with_substring( field_names_list, (gchar *) text );

	if ( ! g_list_length( found ) )
		return;

	/* Sort by increasing k.
	 */
	found = g_list_sort( found, match_list_compare );

	/* DBG: Print all matches.
	 */
	//GList *t = found, *s;
	//g_assert( t->data );
	//while ( t ) {
	//	s = (GList *) t->data; 
	//	g_list_foreach( s, Match_print, NULL );
	//	t = t->next;
	//	puts("");
	//}

	/* Get two GLists of GLists - one with the k=0 matches, and another with
	 * the k>0 matches.
	 */
	while ( found ) {
		t = (GList *) found->data;
		s0 = s1 = NULL;

		while ( t ) {
			Match *match = (Match *) t->data;
			if ( match ) {
				if ( match->k )
					s1 = g_list_append( s1, match );
				else
					s0 = g_list_append( s0, match );
				t = t->next;
			}
		}

		if ( s0 )
			found0 = g_list_append( found0, s0 );

		if ( s1 )
			found1 = g_list_append( found1, s1 );

		found = found->next;
	}


	/* Add the exact (k=0) matches, if any.
	 */
	if ( g_list_length( found0 ) ) {
		g_list_foreach( found0, append_markup_field_name, win );
	}

	/* Add the fuzzy (k>0) matches, if any.
	 */
	if ( !g_list_length( found0 ) && g_list_length( found1 ) ) {
		g_list_foreach( found1, append_field_name, win );
	}
}

gboolean
shrink_window( gpointer user_data )
{
	ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );
	if ( !gtk_widget_get_visible( win->metadata ) )
		return;
	gtk_widget_set_size_request( win->metadata, 0, 0 );
	gtk_widget_hide( win->metadata );
	gtk_orientable_set_orientation( GTK_ORIENTABLE( win->main_box ), GTK_ORIENTATION_VERTICAL );
	gtk_window_set_default_size( GTK_WINDOW( win ), win->og_width, win->og_height );

	g_settings_set_value( win->settings, "metadata", g_variant_new_boolean( FALSE ) );

	change_state( GTK_WIDGET( win ), "metadata", 
		g_settings_get_value( win->settings, "metadata" ) );

	return TRUE;
}


static void
image_window_metadata( GSimpleAction *action, 
	GVariant *state, gpointer user_data )
{
	ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );
	int width, height;
	GtkWidget *view, *scrolled_window, *box, *label,
		*search_bar_box, *search_entry, *header_bar;
	GtkColumnViewColumn *column;
	VipsImage *image;

	if ( win->tile_source && win->tile_source->image ) {
		image = win->tile_source->image;
		gtk_window_get_default_size( GTK_WINDOW( win ), &width, &height );
		if ( !win->og_width || !win->og_height) {
			win->og_width = width;
			win->og_height = height;
		}

		if ( g_variant_get_boolean( state ) ) {
			gtk_window_set_default_size( GTK_WINDOW( win ), 1.9*width, height );
			gtk_widget_show( win->metadata );

			gtk_orientable_set_orientation( GTK_ORIENTABLE( win->main_box ), GTK_ORIENTATION_HORIZONTAL );

			gtk_widget_set_size_request( win->metadata, width * .75, height * .75 );

			char *column_names[] = { "Field", "Value" };
			const int column_names_length = 2;

			/* Define the list items of the first column. For this
			 * example, these are exactly the names of the fields on
			 * the (global) test image.
			 */
			char** field_names;
			field_names = vips_image_get_fields( image );

			/* Define the list model our selection model will use.
			 */
			GtkStringList *list_model;
			list_model = gtk_string_list_new( NULL );
			char **p;
			p = field_names;

			VipsArgumentClass *argument_class;
			VipsArgumentInstance *argument_instance;
			VipsArgumentFlags flags; 
			GParamSpec *pspec;

			while ( *p ) {
				gtk_string_list_append( list_model, *p );
				p++;
			}
		
			win->list_model = list_model;

			/* Create simple selection model, which does not have
			 * any selection logic. Curiously, you can still see
			 * the mouseover highlight effect and onclick  highlight
			 * effect even for GtkNoSelection, perhaps for
			 * accessibility reasons.
			 */
			GtkNoSelection *selection_model =
				gtk_no_selection_new(
					G_LIST_MODEL( list_model ) );

			/* Initialize the array of GtkListItemFactory - one for
			 * each column.
			 */
			GtkListItemFactory *factories[column_names_length];
			for ( int i = 0; i < column_names_length; i++ )
				factories[i] =
					gtk_signal_list_item_factory_new();

			/* Connect handlers to the field name factory.
			 */
			g_signal_connect( factories[0], "setup",
				G_CALLBACK( factory_setup ), NULL );

			g_signal_connect( factories[0], "bind",
				G_CALLBACK( field_name_factory_bind ),
				column_names[0] );

			/* Connect handlers to the other factories.
			 */
			for ( int i = 1; i < column_names_length; i++ ) {
				g_signal_connect( factories[i], "setup",
					G_CALLBACK( factory_setup ), NULL );

				g_signal_connect( factories[i], "bind",
					G_CALLBACK( value_factory_bind ),
					(gpointer) image );
			}

			gtk_search_bar_set_search_mode(
				GTK_SEARCH_BAR( win->metadata ), TRUE );

			//gtk_search_bar_set_key_capture_widget(
			//	GTK_SEARCH_BAR( win->metadata ),
			//	win );

			GtkWidget *revealer = gtk_widget_get_first_child( win->metadata );
			gtk_revealer_set_transition_type( GTK_REVEALER( revealer ),
				GTK_REVEALER_TRANSITION_TYPE_SLIDE_RIGHT );

			search_bar_box = gtk_search_bar_get_child(
				GTK_SEARCH_BAR( win->metadata ) );

			gtk_widget_set_size_request( search_bar_box,
				width * .75, height * .75 );

			/* Create the search entry.
			 */
			gtk_search_bar_connect_entry( GTK_SEARCH_BAR( win->metadata ),
				GTK_EDITABLE( win->metadata_search_entry ) );

			g_signal_connect( win->metadata_search_entry,
				"search-changed",
				G_CALLBACK( search_changed ), win );

			/* Create the scrolled window.
			 */
			gtk_widget_set_size_request( win->metadata_window,
				width, height * .75 );

			gtk_scrolled_window_set_max_content_height(
				GTK_SCROLLED_WINDOW( win->metadata_window ),
				height * .75);

			gtk_scrolled_window_set_max_content_width(
				GTK_SCROLLED_WINDOW( win->metadata_window ),
				width * .75);

			/* Create the column view.
			 */
			view = gtk_column_view_new(
				GTK_SELECTION_MODEL( selection_model ) );

			gtk_scrolled_window_set_child(
				GTK_SCROLLED_WINDOW( win->metadata_window), view );

			/* Create the columns.
			 */
			for ( int i = 0; i < column_names_length; i++ ) {
				column = gtk_column_view_column_new(
					column_names[i], factories[i] );

				gtk_column_view_column_set_expand( column,
					TRUE );

				gtk_column_view_append_column(
					GTK_COLUMN_VIEW( view ), column );
			}

			gtk_revealer_set_reveal_child( GTK_REVEALER( revealer ),
				TRUE );

			gtk_widget_grab_focus( GTK_WIDGET( win->imagedisplay ) );
		}
		else if ( gtk_widget_get_visible( win->metadata ) ) {
			GtkWidget *revealer = gtk_widget_get_first_child( win->metadata );
			gtk_revealer_set_reveal_child( GTK_REVEALER( revealer ),
				FALSE );
			g_timeout_add( 200 , (GSourceFunc) shrink_window, win );
		}

		g_simple_action_set_state( action, state );
	}
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
			"scale", 1.0,
			"offset", 0.0,
			NULL );

	if( win->tile_cache ) 
		g_object_set( win->tile_cache,
			"background", TILE_CACHE_BACKGROUND_CHECKERBOARD,
			NULL );
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
	{ "metadata", image_window_toggle, NULL, "false",
		image_window_metadata },

	{ "next", image_window_next },
	{ "prev", image_window_prev },
	{ "scale", image_window_scale },
	{ "log", image_window_toggle, NULL, "false", image_window_log },
	{ "falsecolour",
		image_window_toggle, NULL, "false", image_window_falsecolour },
	{ "mode", image_window_radio, "s", "'multipage'", image_window_mode },
	{ "background", image_window_radio, "s", 
		"'checkerboard'", image_window_background },

	{ "reset", image_window_reset },
};

static void
metadata_close_button_cb( GtkWidget *widget, gpointer user_data )
{
	ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );
	GtkWidget *revealer = gtk_widget_get_first_child( win->metadata );
	gtk_revealer_set_reveal_child( GTK_REVEALER( revealer ), FALSE );
	g_timeout_add( 200 , (GSourceFunc) shrink_window, win );
}

static void
image_window_init( ImageWindow *win )
{
	GtkEventController *controller;

	win->og_width = win->og_height = 0;
	win->progress_timer = g_timer_new();
	win->last_progress_time = -1;
	win->scale_rate = 1.0;
	win->settings = g_settings_new( APPLICATION_ID );

	gtk_widget_init_template( GTK_WIDGET( win ) );

	g_object_set( win->display_bar,
		"image-window", win,
		NULL );
	g_object_set( win->info_bar,
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

	g_settings_bind( win->settings, "metadata",
		G_OBJECT( win->metadata ),
		"visible", 
		G_SETTINGS_BIND_DEFAULT );

	/* Initial menu state from settings.
	 */
	change_state( GTK_WIDGET( win ), "control", 
		g_settings_get_value( win->settings, "control" ) );
	change_state( GTK_WIDGET( win ), "info", 
		g_settings_get_value( win->settings, "info" ) );
	change_state( GTK_WIDGET( win ), "metadata", 
		g_settings_get_value( win->settings, "metadata" ) );

	gtk_label_set_markup( GTK_LABEL( win->metadata_label ), "<b>Metadata</b>");
	g_signal_connect( win->metadata_close_button, "clicked", G_CALLBACK( metadata_close_button_cb ), win );
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
	BIND( main_box );
	BIND( metadata );
	BIND( metadata_label );
	BIND( metadata_close_button );
	BIND( metadata_window );
	BIND( metadata_search_entry );

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

	image_window_changed( win );
}

TileSource *
image_window_get_tile_source( ImageWindow *win )
{
	return( win->tile_source );
}

void
image_window_open( ImageWindow *win, GFile *file )
{
	TileSource *tile_source;

	if( !(tile_source = tile_source_new_from_file( file )) ) {
		image_window_error( win ); 
		return;
	}

	image_window_set_tile_source( win, tile_source );

	VIPS_UNREF( tile_source );
}

void
image_window_get_mouse_position( ImageWindow *win, 
	double *x_image, double *y_image )
{
	imagedisplay_gtk_to_image( VIPSDISP_IMAGEDISPLAY( win->imagedisplay ), 
		win->last_x_gtk, win->last_y_gtk, x_image, y_image );
}
