/*
#define DEBUG
 */

#include "vipsdisp.h"

/* How much to scale view by each frame.
 */
#define SCALE_STEP (1.05)

/* Duration of discrete zoom in secs.
 */
#define SCALE_DURATION (0.5)

struct _ImageWindow
{
	GtkApplicationWindow parent;

	TileSource *tile_source;
	TileCache *tile_cache;

	/* Last known mouse position, in gtk coordinates. We keep these in gtk
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
	double scale_start;
	double scale_progress;

	/* The current save and load directories.
	 */
	GFile *save_folder;
	GFile *load_folder;

	GtkWidget *right_click_menu;
	GtkWidget *title;
	GtkWidget *subtitle;
	GtkWidget *gears;
	GtkWidget *progress_bar;
	GtkWidget *progress;
	GtkWidget *progress_cancel;
	GtkWidget *error_bar;
	GtkWidget *error_label;
	GtkWidget *main_box;
	GtkWidget *scrolled_window;
	GtkWidget *imagedisplay;
	GtkWidget *properties;
	GtkWidget *display_bar;
	GtkWidget *info_bar;

	/* Throttle progress bar updates to a few per second with this.
	 */
	GTimer *progress_timer;
	double last_progress_time;

	gint64 last_frame_time;

	GSettings *settings;

	GtkCssProvider *css_provider;
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
	ImageWindow *win = IMAGE_WINDOW( object );

#ifdef DEBUG
	printf( "image_window_dispose:\n" ); 
#endif /*DEBUG*/

	gtk_style_context_remove_provider_for_display(
		gdk_display_get_default(),
		GTK_STYLE_PROVIDER( win->css_provider ) );

	VIPS_UNREF( win->tile_source );
	VIPS_UNREF( win->tile_cache );
	VIPS_UNREF( win->save_folder );
	VIPS_UNREF( win->load_folder );
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
image_window_set_error( ImageWindow *win, const char *message )
{
	char *err;
	int i;

	/* Remove any trailing \n.
	 */
	err = g_strdup( message );
	for( i = strlen( err ); i > 0 && err[i - 1] == '\n'; i-- )
		err[i - 1] = '\0';
	gtk_label_set_text( GTK_LABEL( win->error_label ), err );
	g_free( err );

	gtk_info_bar_set_revealed( GTK_INFO_BAR( win->error_bar ), TRUE );
}

static void
image_window_error( ImageWindow *win )
{
	image_window_set_error( win, vips_error_buffer_copy() );
}

static void
image_window_gerror( ImageWindow *win, GError **error )
{
	if( error && *error ) {
		image_window_set_error( win, (*error)->message );
		g_error_free( *error );
	}
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

/* From clutter-easing.c, based on Robert Penner's infamous easing equations,
 * MIT license.
 */
static double
ease_out_cubic( double t )
{
  double p = t - 1;

  return( p * p * p + 1 );
}

static void image_window_stop_animation( ImageWindow *win );

static gboolean
image_window_tick( GtkWidget *widget, 
	GdkFrameClock *frame_clock, gpointer user_data )
{
	ImageWindow *win = IMAGE_WINDOW( user_data );

	gint64 frame_time = gdk_frame_clock_get_frame_time( frame_clock );
	double dt = win->last_frame_time > 0 ?
		(double) (frame_time - win->last_frame_time) / 
			G_TIME_SPAN_SECOND : 
		1.0 / G_TIME_SPAN_SECOND;
	double scale = image_window_get_scale( win );

	double x_image;
	double y_image;
	double new_scale;

	image_window_get_mouse_position( win, &x_image, &y_image );

#ifdef DEBUG
	printf( "image_window_tick: dt = %g\n", dt );
#endif /*DEBUG*/

	new_scale = scale;

	// i/o/etc. continuous zoom
	if( win->scale_rate != 1.0 )
		new_scale = (dt * (win->scale_rate - 1.0) + 1.0) * scale;

	// 0/1/etc. discrete zoom
	if( win->scale_target != 0 ) {
		win->scale_progress += dt;

		// 0-1 progress in zoom animation
		double t = ease_out_cubic( win->scale_progress / SCALE_DURATION );

		// so current scale must be
		new_scale = win->scale_start + 
			t * (win->scale_target - win->scale_start);

		if( t >= 1.0 ) {
			new_scale = win->scale_target;
			win->scale_target = 0;
			image_window_stop_animation( win );
		}
	}

	image_window_set_scale_position( win, new_scale, x_image, y_image );

	win->last_frame_time = frame_time;

	return( G_SOURCE_CONTINUE );
}

static void
image_window_start_animation( ImageWindow *win )
{
	if( !win->tick_handler ) {
		win->last_frame_time = -1;
		win->tick_handler = gtk_widget_add_tick_callback( 
			GTK_WIDGET( win ),
			image_window_tick, win, NULL );
	}
}

static void
image_window_stop_animation( ImageWindow *win )
{
	if( win->tick_handler ) {
		gtk_widget_remove_tick_callback( GTK_WIDGET( win ), 
			win->tick_handler );
		win->tick_handler = 0;
	}
}

static void
image_window_animate_scale_to( ImageWindow *win, double scale_target )
{
	win->scale_rate = 1.0;
	win->scale_target = scale_target;
	win->scale_start = image_window_get_scale( win );
	win->scale_progress = 0.0;
}

static void
image_window_animate_bestfit( ImageWindow *win )
{
	// size by whole image area, including the props pane
	int widget_width = gtk_widget_get_width( win->main_box );
	int widget_height = gtk_widget_get_height( win->main_box );

	double hscale = (double) widget_width / win->tile_source->display_width;
	double vscale = (double) widget_height / win->tile_source->display_height;
	double scale = VIPS_MIN( hscale, vscale );
	
	image_window_animate_scale_to( win, scale * win->tile_source->zoom );
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

static GdkTexture *
texture_new_from_image( VipsImage *image )
{
	GdkTexture *texture;

	// don't convert huge images to textures, we'll kill the machine
	if( (gint64) image->Xsize * image->Ysize > 5000 * 5000 ) {
		vips_error( "Convert to texture", 
			_( "Image too large to convert to texture" ) );
		return( NULL );
	}

	void *buf;
   	size_t len;
	if( vips_tiffsave_buffer( image, &buf, &len, NULL ) )
		return( NULL );

	GBytes *bytes = g_bytes_new_take( buf, len );
	GError *error = NULL;
	texture = gdk_texture_new_from_bytes( bytes, &error );
	g_bytes_unref( bytes );
	if( !texture ) {
		vips_error( "Convert to texture", "%s", error->message );
		g_error_free( error );
	}

	return( texture );
}

static void
image_window_copy_action( GSimpleAction *action,
	GVariant *parameter, gpointer user_data )
{
	ImageWindow *win = IMAGE_WINDOW( user_data );

	if( win->tile_source ) {
		GdkClipboard *clipboard = gtk_widget_get_clipboard( GTK_WIDGET( win ) );

		GFile *file;
		VipsImage *image;

		if( (file = tile_source_get_file( win->tile_source )) ) {
			gdk_clipboard_set( clipboard, G_TYPE_FILE, file );
			VIPS_UNREF( file );
		}
		else if( (image = tile_source_get_image( win->tile_source )) ) {
			GdkTexture *texture;

			if( (texture = texture_new_from_image( image )) ) {
				gdk_clipboard_set( clipboard, GDK_TYPE_TEXTURE, texture );
				VIPS_UNREF( texture );
			}
			else
				image_window_error( win );
        }
	}
}

static void
image_new_from_texture_free( VipsImage *image, GBytes *bytes )
{
	g_bytes_unref( bytes );
}

static VipsImage *
image_new_from_texture( GdkTexture *texture )
{
	GBytes *bytes = gdk_texture_save_to_tiff_bytes( texture );
	if( (bytes = gdk_texture_save_to_tiff_bytes( texture )) ) {
		gsize len;
		gconstpointer data = g_bytes_get_data( bytes, &len );

		VipsImage *image;
		if( (image = vips_image_new_from_buffer( data, len, "", NULL )) ) {
			g_signal_connect( image, "close", 
				G_CALLBACK( image_new_from_texture_free ), bytes );
			g_bytes_ref( bytes );

			return( image );
		}

		VIPS_FREEF( g_bytes_unref, bytes );
	}
	else 
		vips_error( "Convert to TIFF", 
			_( "unable to convert texture to TIFF" ) );

	return( NULL );
}

static void
image_window_paste_action_ready( GObject *source_object, 
	GAsyncResult *res, gpointer user_data )
{
	GdkClipboard *clipboard = GDK_CLIPBOARD( source_object );
	ImageWindow *win = IMAGE_WINDOW( user_data );

	const GValue *value;
	GError *error = NULL;
	value = gdk_clipboard_read_value_finish( clipboard, res, &error );
	if( error ) 
		image_window_gerror( win, &error );
	else if( value ) {
		if( G_VALUE_TYPE( value ) == G_TYPE_FILE ) {
			GFile *file = g_value_get_object( value );
			image_window_open( win, file );
		}
		else if( G_VALUE_TYPE( value ) == G_TYPE_STRING ) {
			const char *text = g_value_get_string( value );
			GFile *file = g_file_new_for_path( text );

			image_window_open( win, file );

			VIPS_UNREF( file );
		}
		else if( G_VALUE_TYPE( value ) == GDK_TYPE_TEXTURE ) {
			GdkTexture *texture = g_value_get_object( value );
			VipsImage *image;
			if( (image = image_new_from_texture( texture )) ) {
				TileSource *tile_source;
				if( (tile_source = tile_source_new_from_image( image )) ) {
					image_window_set_tile_source( win, tile_source );

					VIPS_UNREF( tile_source );
				}

				VIPS_UNREF( image );
			}
			else
				image_window_error( win );
		}
	}
}

static void
image_window_paste_action( GSimpleAction *action,
	GVariant *parameter, gpointer user_data )
{
	ImageWindow *win = IMAGE_WINDOW( user_data );
	GdkClipboard *clipboard = gtk_widget_get_clipboard( GTK_WIDGET( win ) );

	GdkContentFormats *formats = gdk_clipboard_get_formats( clipboard );
	gsize n_types;
	const GType *types = gdk_content_formats_get_gtypes( formats, &n_types );

	/*
	printf( "clipboard in %lu formats\n", n_types );
	for( gsize i = 0; i < n_types; i++ ) 
		printf( "%lu - %s\n", i, g_type_name( types[i] ) );
	 */

	// GTypes we handle in paste ... these are in the order we try, so a GFile
	// is preferred to a simple string

	// gnome file manager pastes as GFileList, GFile, gchararray
	// print-screen button pastes as GdkTexture, GdkPixbuf

	GType supported_types[] = {
		G_TYPE_FILE,
		G_TYPE_STRING,
		GDK_TYPE_TEXTURE,
	};

	gboolean handled = FALSE;
	for( gsize i = 0; i < n_types; i++ ) {
		for( int j = 0; j < VIPS_NUMBER( supported_types ); j++ ) 
			if( types[i] == supported_types[j] ) {
				gdk_clipboard_read_value_async( clipboard,
					supported_types[j],
					G_PRIORITY_DEFAULT,
					NULL,
					image_window_paste_action_ready,
					win );
				handled = TRUE;
				break;
			}
				
		if( handled )
			break;
	}
}

static void
image_window_magin_action( GSimpleAction *action,
	GVariant *parameter, gpointer user_data )
{
	ImageWindow *win = IMAGE_WINDOW( user_data );

	image_window_animate_scale_to( win, 2 * image_window_get_scale( win ) );
	image_window_start_animation( win );
}

static void
image_window_magout_action( GSimpleAction *action, 
	GVariant *parameter, gpointer user_data )
{
	ImageWindow *win = IMAGE_WINDOW( user_data );

	image_window_animate_scale_to( win, 0.5 * image_window_get_scale( win ) );
	image_window_start_animation( win );
}

static void
image_window_bestfit_action( GSimpleAction *action,
	GVariant *parameter, gpointer user_data )
{
	ImageWindow *win = IMAGE_WINDOW( user_data );

	image_window_animate_bestfit( win );
	image_window_start_animation( win );
}

static void
image_window_oneone_action( GSimpleAction *action, 
	GVariant *parameter, gpointer user_data )
{
	ImageWindow *win = IMAGE_WINDOW( user_data );

	image_window_set_scale( win, 1.0 );
}

static void
image_window_duplicate_action( GSimpleAction *action, 
	GVariant *parameter, gpointer user_data )
{
	ImageWindow *win = IMAGE_WINDOW( user_data );

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

static GFile *
get_parent( GFile *file )
{
	GFile *parent = g_file_get_parent( file );

	return parent ? parent : g_file_new_for_path( "/" );
}

static void
image_window_replace_result( GObject *source_object,
	GAsyncResult *res, gpointer user_data )
{
	ImageWindow *win = IMAGE_WINDOW( user_data );
	GtkFileDialog *dialog = GTK_FILE_DIALOG( source_object );
	GFile *file;

	file = gtk_file_dialog_open_finish( dialog, res, NULL );
	if( file ) {
		// note the directory for next time
		VIPS_UNREF( win->load_folder );
		win->load_folder = get_parent( file );

		image_window_error_hide( win );
		image_window_open( win, file );
		g_object_unref( file );
	}
}

static void
image_window_replace_action( GSimpleAction *action, 
	GVariant *parameter, gpointer user_data )
{
	ImageWindow *win = IMAGE_WINDOW( user_data );

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
	else if( win->load_folder )
		gtk_file_dialog_set_initial_folder( dialog, win->load_folder );

	gtk_file_dialog_open( dialog, GTK_WINDOW( win ), NULL,
		image_window_replace_result, win );
}

static void
image_window_saveas_options_response( GtkDialog *dialog, 
	gint response, gpointer user_data )
{
	if( response == GTK_RESPONSE_ACCEPT ||
		response == GTK_RESPONSE_CANCEL ) 
		gtk_window_destroy( GTK_WINDOW( dialog ) );

	// other return codes are intermediate stages of processing and we
	// should do nothing
}

static void
image_window_on_file_save_cb( GObject* source_object,
	GAsyncResult* res, gpointer user_data )
{
	ImageWindow *win = IMAGE_WINDOW( user_data );
	GtkFileDialog *dialog = GTK_FILE_DIALOG( source_object );
	GFile *file;

	file = gtk_file_dialog_save_finish( dialog, res, NULL );
	if( file ) {
		char *filename;
		SaveOptions *options;

		// note the save directory for next time
		VIPS_UNREF( win->save_folder );
		win->save_folder = get_parent( file );

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

static void
image_window_saveas_action( GSimpleAction *action, 
	GVariant *parameter, gpointer user_data )
{
	ImageWindow *win = IMAGE_WINDOW( user_data );

	if( win->tile_source ) {
		GtkFileDialog *dialog;
		GFile *file;

		dialog = gtk_file_dialog_new();
		gtk_file_dialog_set_title( dialog, "Save file" );
		gtk_file_dialog_set_modal( dialog, TRUE );

		if( (file = tile_source_get_file( win->tile_source )) ) {
			gtk_file_dialog_set_initial_file( dialog, file );
			g_object_unref( file );
		}
		else if( win->save_folder )
			gtk_file_dialog_set_initial_folder( dialog, win->save_folder );

		gtk_file_dialog_save( dialog, GTK_WINDOW( win ), NULL,
			&image_window_on_file_save_cb, win );
	}
}

static void
image_window_close_action( GSimpleAction *action, 
	GVariant *parameter, gpointer user_data )
{
	ImageWindow *win = IMAGE_WINDOW( user_data );

	gtk_window_destroy( GTK_WINDOW( win ) );
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
	ImageWindow *win = IMAGE_WINDOW( user_data );
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
	ImageWindow *win = IMAGE_WINDOW( user_data );

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

	if( handled )
		image_window_stop_animation( win );

	return( handled );
}

static void
image_window_motion( GtkEventControllerMotion *self,
	gdouble x, gdouble y, gpointer user_data )
{
	ImageWindow *win = IMAGE_WINDOW( user_data );

	win->last_x_gtk = x;
	win->last_y_gtk = y;

	image_window_status_changed( win );
}

static gboolean
image_window_scroll( GtkEventControllerMotion *self,
	double dx, double dy, gpointer user_data )
{
	ImageWindow *win = IMAGE_WINDOW( user_data );

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
	ImageWindow *win = IMAGE_WINDOW( user_data );

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
	ImageWindow *win = IMAGE_WINDOW( user_data );

	image_window_set_scale_position( win, 
		scale * win->last_scale, win->scale_cx, win->scale_cy );
}

static void
image_window_drag_begin( GtkEventControllerMotion *self,
	gdouble start_x, gdouble start_y, gpointer user_data )
{
	ImageWindow *win = IMAGE_WINDOW( user_data );

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
	ImageWindow *win = IMAGE_WINDOW( user_data );

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
	ImageWindow *win = IMAGE_WINDOW( user_data );

	g_object_set( win, 
		"fullscreened", g_variant_get_boolean( state ),
		NULL );

	g_simple_action_set_state( action, state );
}

static void
image_window_control( GSimpleAction *action, 
	GVariant *state, gpointer user_data )
{
	ImageWindow *win = IMAGE_WINDOW( user_data );

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
	ImageWindow *win = IMAGE_WINDOW( user_data );

	g_object_set( win->info_bar,
		"revealed", g_variant_get_boolean( state ),
		NULL );

	g_simple_action_set_state( action, state );
}

static void
image_window_next( GSimpleAction *action, GVariant *state, gpointer user_data )
{
	ImageWindow *win = IMAGE_WINDOW( user_data );

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
	ImageWindow *win = IMAGE_WINDOW( user_data );

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
	 *
	 * Alternatively, run this in a BG thread.
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
	ImageWindow *win = IMAGE_WINDOW( user_data );

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
	ImageWindow *win = IMAGE_WINDOW( user_data );

	if( win->tile_source )
		g_object_set( win->tile_source,
			"log", g_variant_get_boolean( state ),
			NULL );

	g_simple_action_set_state( action, state );
}

static void
image_window_icc( GSimpleAction *action, GVariant *state, gpointer user_data )
{
	ImageWindow *win = IMAGE_WINDOW( user_data );

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
	ImageWindow *win = IMAGE_WINDOW( user_data );

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
	ImageWindow *win = IMAGE_WINDOW( user_data );

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
	ImageWindow *win = IMAGE_WINDOW( user_data );
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
	ImageWindow *win = IMAGE_WINDOW( user_data );

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

static void
image_window_properties( GSimpleAction *action, 
	GVariant *state, gpointer user_data )
{
	ImageWindow *win = IMAGE_WINDOW( user_data );
	gboolean revealed = g_variant_get_boolean( state );

#ifdef DEBUG
	puts("image_window_properties");
#endif /* DEBUG */

	g_object_set( win->properties,
		"revealed", revealed,
		NULL );

	g_simple_action_set_state( action, state );
}

static GActionEntry image_window_entries[] = {
	{ "copy", image_window_copy_action },
	{ "paste", image_window_paste_action },

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

static void
image_window_properties_leave( GtkEventControllerFocus *self, 
		gpointer user_data )
{
	ImageWindow *win = IMAGE_WINDOW( user_data );

	gboolean revealed;

	g_object_get( win->properties,
		"revealed", &revealed,
		NULL );

	// if the props pane had the focus, and it's being hidden, we must refocus
	if( !revealed )
		gtk_widget_grab_focus( win->imagedisplay );
}

static gboolean
image_window_dnd_drop( GtkDropTarget *target,
	const GValue *value, double x, double y, gpointer user_data )
{
	ImageWindow *win = IMAGE_WINDOW( user_data );

	printf( "image_window_dnd_drop:\n" );

	if( G_VALUE_HOLDS( value, G_TYPE_FILE ) )
		image_window_open( win, g_value_get_object( value ));
	else if( G_VALUE_HOLDS( value, GDK_TYPE_PIXBUF ) )
		printf( "image_window_drop: implement GDK_TYPE_PIXBUF load\n" );
	else
		return FALSE;

	return TRUE;
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
	char *cwd = g_get_current_dir();
	win->save_folder = g_file_new_for_path( cwd );
	win->load_folder = g_file_new_for_path( cwd );
	g_free( cwd );

	gtk_widget_init_template( GTK_WIDGET( win ) );

	win->css_provider = gtk_css_provider_new();
	gtk_css_provider_load_from_resource( win->css_provider,
		APP_PATH "/imagewindow.css" );
	gtk_style_context_add_provider_for_display( gdk_display_get_default(),
		GTK_STYLE_PROVIDER( win->css_provider ),
		GTK_STYLE_PROVIDER_PRIORITY_APPLICATION );

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

	/* We need to know if the props pane has the focus so we can refocus on
	 * hide.
	 */
	controller = GTK_EVENT_CONTROLLER( gtk_event_controller_focus_new() );
	g_signal_connect( controller, "leave", 
		G_CALLBACK( image_window_properties_leave ), win );
	gtk_widget_add_controller( win->properties, controller );

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

	/* We are a drop target for filenames and images.
	 */
	controller = GTK_EVENT_CONTROLLER( gtk_drop_target_new( 
			G_TYPE_INVALID, GDK_ACTION_COPY ) );
	gtk_drop_target_set_gtypes( GTK_DROP_TARGET( controller ), (GType [2]) {
		G_TYPE_FILE,
		GDK_TYPE_PIXBUF,
	}, 2);
	g_signal_connect( controller, "drop",
        G_CALLBACK( image_window_dnd_drop ), win );
	gtk_widget_add_controller( win->imagedisplay, controller );

	/* We can't be a drag source, we use drag for pan. Copy/paste images out
	 * instead.
	 */

	/* Initialise from settings.
	 */
	change_state( GTK_WIDGET( win ), "properties", 
		g_settings_get_value( win->settings, "properties" ) );

	/* Initial menu state from settings.
	 */
	change_state( GTK_WIDGET( win ), "control", 
		g_settings_get_value( win->settings, "control" ) );
	change_state( GTK_WIDGET( win ), "info", 
		g_settings_get_value( win->settings, "info" ) );

	// some kind of gtk bug? hepand on properties can't be set from .ui or in
	// properties.c, but must be set after adding to a parent
	g_object_set( win->properties, "hexpand", FALSE, NULL );
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
	BIND( main_box );
	BIND( scrolled_window );
	BIND( imagedisplay );
	BIND( properties );
	BIND( display_bar );
	BIND( info_bar );

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

	g_object_set( win->properties,
		"tile-source", win->tile_source,
		NULL );

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

GSettings *image_window_get_settings( ImageWindow *win )
{
	return( win->settings );
}

void
image_window_open( ImageWindow *win, GFile *file )
{
	char *path;
	TileSource *tile_source;

#ifdef DEBUG
	printf( "image_window_open:\n" );
#endif /* DEBUG */

	path = g_file_get_path( file );

	if( !(tile_source = tile_source_new_from_file( path )) ) {
		image_window_error( win ); 
		g_free( path );
		return;
	}

	image_window_set_tile_source( win, tile_source );

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

