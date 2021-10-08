/*
#define DEBUG
 */

#include "vipsdisp.h"

struct _ImageWindow
{
        GtkApplicationWindow parent;

        Conversion *conversion;

        TileSource *tile_source;
        TileCache *tile_cache;

        /* Last known mouse postion, in image coordinates.
         */
        int last_x;
        int last_y;

        /* For drag, the window position where we started, in gtk coordinates.
         */
        int drag_start_x;
        int drag_start_y;

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
        GtkWidget *conversion_bar;
        GtkWidget *info_bar;

        /* Throttle progress bar updates to a few per second with this.
         */
        GTimer *progress_timer;
        double last_progress_time;

        GSettings *settings;
};

G_DEFINE_TYPE( ImageWindow, image_window, GTK_TYPE_APPLICATION_WINDOW );

/* Our signals. 
 */
enum {
        SIG_POSITION_CHANGED,
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

        VIPS_UNREF( win->conversion );
        VIPS_UNREF( win->tile_source );
        VIPS_UNREF( win->tile_cache );
        VIPS_FREEF( g_timer_destroy, win->progress_timer );

        G_OBJECT_CLASS( image_window_parent_class )->dispose( object );
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
        if( win->conversion->image )
                vips_image_set_kill( win->conversion->image, TRUE );
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
image_window_conversion_changed( Conversion *conversion, ImageWindow *win )
{
        const char *title;
        char str[256];
        VipsBuf buf = VIPS_BUF_STATIC( str );

        GVariant *state;
        const char *str_mode;

#ifdef DEBUG
        printf( "image_window_conversion_changed:\n" );
#endif /*DEBUG*/

        if( !(title = conversion_get_path( conversion )) ) 
                title = "Untitled";
        gtk_label_set_text( GTK_LABEL( win->title ), title );

        if( conversion->image ) {
                VipsImage *image = conversion->image;

                vips_object_summary( VIPS_OBJECT( image ), &buf );
                vips_buf_appendf( &buf, ", " );
                vips_buf_append_size( &buf, VIPS_IMAGE_SIZEOF_IMAGE( image ) );
                vips_buf_appendf( &buf, ", %g x %g p/mm",
                        image->Xres, image->Yres );
        }
        gtk_label_set_text( GTK_LABEL( win->subtitle ), vips_buf_all( &buf ) );

        state = g_variant_new_boolean( conversion->falsecolour );
        change_state( GTK_WIDGET( win ), "falsecolour", state );

        state = g_variant_new_boolean( conversion->log );
        change_state( GTK_WIDGET( win ), "log", state );

        if( conversion->mode == CONVERSION_MODE_TOILET_ROLL )
                str_mode = "toilet-roll";
        else if( conversion->mode == CONVERSION_MODE_MULTIPAGE )
                str_mode = "multipage";
        else if( conversion->mode == CONVERSION_MODE_ANIMATED )
                str_mode = "animated";
        else if( conversion->mode == CONVERSION_MODE_PAGES_AS_BANDS )
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
image_window_get_window_position( ImageWindow *win,
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
        printf( "image_window_get_window_position: %d %d %d %d\n",
                *left, *top, *width, *height );
#endif /*DEBUG*/
}

static void
image_window_position_changed( ImageWindow *win )
{
        g_signal_emit( win, 
                image_window_signals[SIG_POSITION_CHANGED], 0 );
}

static int
image_window_set_mag( ImageWindow *win, int mag )
{
#ifdef DEBUG
        printf( "image_window_set_mag: %d\n", mag );
#endif /*DEBUG*/

        g_object_set( win->imagedisplay, 
		"scale", mag < 0 ? 1.0 / -mag : mag, 
		NULL );

        return( 0 );
}

void
image_window_set_window_position( ImageWindow *win, int left, int top )
{
        GtkAdjustment *hadj = gtk_scrolled_window_get_hadjustment( 
                GTK_SCROLLED_WINDOW( win->scrolled_window ) );
        GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment( 
                GTK_SCROLLED_WINDOW( win->scrolled_window ) );

#ifdef DEBUG
        printf( "image_window_set_window_position: %d %d\n", left, top ); 
#endif /*DEBUG*/

        gtk_adjustment_set_value( hadj, left );
        gtk_adjustment_set_value( vadj, top );

}

/* Set a new mag, keeping the pixel at x/y in image coordinates at the same
 * position on the screen, if we can.
 */
static void     
image_window_set_mag_position( ImageWindow *win, int mag, int x, int y )
{                       
        Conversion *conversion = win->conversion;

        VipsRect old_point;
        VipsRect new_point;

#ifdef DEBUG
#endif /*DEBUG*/ 
        printf( "image_window_set_mag_position: %d %d %d\n", mag, x, y );
  
        /* Map the image pixel at (x, y) to gtk space, ie. mouse coordinates.
         */
        conversion_to_display_cods( conversion->mag,
                x, y,
                &old_point.left, &old_point.top ); 
        imagedisplay_image_to_gtk( VIPSDISP_IMAGEDISPLAY( win->imagedisplay ), 
                &old_point );

	printf( "  old point in screen space %d, %d\n", 
                old_point.left, old_point.top ); 
        
        /* Mag set can be out of range.
         */
        if( image_window_set_mag( win, mag ) ) {
		printf( "  mag out of range\n" );
                return;
	}

        /* Map image (x, y) to display coordinates with our new magnification,
	 * then to keep the point in the same position we must translate by 
	 * the difference.
         */
        conversion_to_display_cods( conversion->mag,
                x, y,
                &new_point.left, &new_point.top );

	printf( "  new point in screen space %d, %d\n", 
                new_point.left, new_point.top ); 
	printf( "  shifting view by %d, %d\n", 
                new_point.left - old_point.left,
                new_point.top - old_point.top );

        image_window_set_window_position( win,
                new_point.left - old_point.left,
                new_point.top - old_point.top );
}

/* Zoom in, keeping the pixel at x/y in image coordinates at the same position
 * on the screen.
 */
static void
image_window_magin_point( ImageWindow *win, int x, int y )
{
        int mag;

#ifdef DEBUG
        printf( "image_window_magin_point: %d %d\n", x, y );
#endif /*DEBUG*/

        g_object_get( win->conversion, "mag", &mag, NULL );
        if( mag <= 0 ) {
                if( mag >= -2 )
                        image_window_set_mag_position( win, 1, x, y );
                else
                        image_window_set_mag_position( win, mag / 2, x, y );
        }
        else
                image_window_set_mag_position( win, mag * 2, x, y );
}

/* Zoom out, keeping the pixel at x/y in image coordinates at the same position
 * on the screen.
 */
void
image_window_magout( ImageWindow *win, int x, int y )
{
        Conversion *conversion = win->conversion;

        int image_width;
        int image_height;
        int mag;

#ifdef DEBUG
        printf( "image_window_magout:\n" ); 
#endif /*DEBUG*/

        /* Don't let the image get too small.
         */
        if( !conversion_get_display_image_size( conversion, 
                &image_width, &image_height ) ) 
                return;
        if( image_width == 1 ||
                image_height == 1 )
                return;

        g_object_get( conversion, "mag", &mag, NULL ); 
        if( mag >= 0 )  {
                if( mag < 2 ) 
                        image_window_set_mag_position( win, -2, x, y );
                else
                        image_window_set_mag_position( win, mag / 2, x, y );
        }
        else 
                image_window_set_mag_position( win, mag * 2, x, y );
}

void
image_window_bestfit( ImageWindow *win )
{
        Conversion *conversion = win->conversion;

        int image_width;
        int image_height;

#ifdef DEBUG
        printf( "image_window_bestfit:\n" ); 
#endif /*DEBUG*/

        if( conversion_get_image_size( conversion, 
                &image_width, &image_height ) ) {
                int allocated_width = 
                        gtk_widget_get_allocated_width( win->imagedisplay );
                int allocated_height = 
                        gtk_widget_get_allocated_height( win->imagedisplay );
                double hfac = (double) allocated_width / image_width;
                double vfac = (double) allocated_height / image_height;
                double fac = VIPS_MIN( hfac, vfac );

                int mag;

                /* 0.999 means we don't round up on an exact fit.
                 *
                 * FIXME ... yuk
                 */
                mag = fac >= 1 ? fac : -((int) (0.99999999 + 1.0 / fac));

                image_window_set_mag( win, mag );
        }
}

static void
image_window_magin_action( GSimpleAction *action,
        GVariant *parameter, gpointer user_data )
{
        ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );

        int window_left;
        int window_top;
        int window_width;
        int window_height;
        int image_x;
        int image_y;

        image_window_get_window_position( win,
                &window_left, &window_top, &window_width, &window_height );
        conversion_to_image_cods( win->conversion->mag,
                window_left + window_width / 2, window_top + window_height / 2,
                &image_x, &image_y );

        image_window_magin_point( win, image_x, image_y );
}

static void
image_window_magout_action( GSimpleAction *action, 
        GVariant *parameter, gpointer user_data )
{
        ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );

        int window_left;
        int window_top;
        int window_width;
        int window_height;
        int image_x;
        int image_y;

        image_window_get_window_position( win,
                &window_left, &window_top, &window_width, &window_height );
        conversion_to_image_cods( win->conversion->mag,
                window_left + window_width / 2, window_top + window_height / 2,
                &image_x, &image_y );

        image_window_magout( win, image_x, image_y ); 
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

        image_window_set_mag( win, 1 );
}

static void
image_window_duplicate_action( GSimpleAction *action, 
        GVariant *parameter, gpointer user_data )
{
        ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );

        VipsdispApp *app;
        ImageWindow *new;
        int width, height;

        g_object_get( win, "application", &app, NULL );
        new = image_window_new( app ); 
        gtk_window_present( GTK_WINDOW( new ) );

        if( conversion_set_conversion( new->conversion, win->conversion ) ) {
                image_window_error( new ); 
                return;
        }

        gtk_window_get_default_size( GTK_WINDOW( win ), &width, &height );
        gtk_window_set_default_size( GTK_WINDOW( new ), width, height );

        /* falsecolour etc. are copied when we copy the conversion. We
         * just copy the window state here.
         */
        copy_state( GTK_WIDGET( new ), GTK_WIDGET( win ), "control" );
        copy_state( GTK_WIDGET( new ), GTK_WIDGET( win ), "info" );

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
        Conversion *conversion = win->conversion;

        GtkWidget *dialog;
        GFile *file;

        dialog = gtk_file_chooser_dialog_new( "Replace from file",
                GTK_WINDOW( win ) , 
                GTK_FILE_CHOOSER_ACTION_OPEN,
                "_Cancel", GTK_RESPONSE_CANCEL,
                "_Replace", GTK_RESPONSE_ACCEPT,
                NULL );
        gtk_window_set_modal( GTK_WINDOW( dialog ), TRUE );

        if( (file = conversion_get_file( conversion )) ) {
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
        Conversion *conversion = win->conversion;

        GFile *file;

        /* We need to pop down immediately so we expose the cancel
         * button.
         */
        file = gtk_file_chooser_get_file( GTK_FILE_CHOOSER( dialog ) );
        image_window_error_hide( win ); 
        gtk_window_destroy( GTK_WINDOW( dialog ) );

        if( response_id == GTK_RESPONSE_ACCEPT &&
                conversion_write_to_file( conversion, file ) ) 
                image_window_error( win );

        VIPS_UNREF( file ); 
}

static void
image_window_saveas_action( GSimpleAction *action, 
        GVariant *parameter, gpointer user_data )
{
        ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );
        Conversion *conversion = win->conversion;

        GtkWidget *dialog;
        GFile *file;

        if( !conversion->image )
                return;

        dialog = gtk_file_chooser_dialog_new( "Save file",
                GTK_WINDOW( win ) , 
                GTK_FILE_CHOOSER_ACTION_SAVE,
                "_Cancel", GTK_RESPONSE_CANCEL,
                "_Save", GTK_RESPONSE_ACCEPT,
                NULL );
        gtk_window_set_modal( GTK_WINDOW( dialog ), TRUE );

        if( (file = conversion_get_file( conversion )) ) {
                gtk_file_chooser_set_file( GTK_FILE_CHOOSER( dialog ), 
                        file, NULL );
                VIPS_UNREF( file );
        }

        g_signal_connect( dialog, "response", 
                G_CALLBACK( image_window_saveas_response ), win );

        gtk_widget_show( dialog );
}

static void
image_window_close_action( GSimpleAction *action, 
        GVariant *parameter, gpointer user_data )
{
        ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );

        gtk_window_destroy( GTK_WINDOW( win ) );
}

static struct {
        int keyval;
        int mag;
} magnify_keys[] = {
        { GDK_KEY_1, 1 },
        { GDK_KEY_2, 2 },
        { GDK_KEY_3, 3 },
        { GDK_KEY_4, 4 },
        { GDK_KEY_5, 5 },
        { GDK_KEY_6, 6 },
        { GDK_KEY_7, 7 },
        { GDK_KEY_8, 8 },
        { GDK_KEY_9, 9 }
};

static gboolean 
image_window_key_pressed( GtkEventControllerKey *self,
        guint keyval, guint keycode, GdkModifierType state, gpointer user_data )
{
        ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );
        GtkAdjustment *hadj = gtk_scrolled_window_get_hadjustment(
                GTK_SCROLLED_WINDOW( win->scrolled_window ) );
        GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(
                GTK_SCROLLED_WINDOW( win->scrolled_window ) );
        int hstep = gtk_adjustment_get_step_increment( hadj );
        int vstep = gtk_adjustment_get_step_increment( vadj );

        gboolean handled;
        int window_left;
        int window_top;
        int window_width;
        int window_height;
        int image_width;
        int image_height;

#ifdef DEBUG
        printf( "image_window_key_pressed: keyval = %d, state = %d\n", 
                keyval, state );
#endif /*DEBUG*/

        image_window_get_window_position( win, 
                &window_left, &window_top, &window_width, &window_height );
        if( !conversion_get_display_image_size( win->conversion, 
                &image_width, &image_height ) )
                return( FALSE );

        handled = FALSE;

        switch( keyval ) {
        case GDK_KEY_plus:
        case GDK_KEY_i:
                image_window_magin_point( win, win->last_x, win->last_y );
                handled = TRUE;
                break;

        case GDK_KEY_o:
        case GDK_KEY_minus:
                image_window_magout( win, win->last_x, win->last_y );
                handled = TRUE;
                break;

        case GDK_KEY_Left:
                if( state & GDK_SHIFT_MASK )
                        image_window_set_window_position( win, 
                                window_left - window_width, window_top );
                else if( state & GDK_CONTROL_MASK )
                        image_window_set_window_position( win, 
                                0, window_top );
                else
                        image_window_set_window_position( win, 
                                window_left - hstep, window_top );
                handled = TRUE;
                break;

        case GDK_KEY_Right:
                if( state & GDK_SHIFT_MASK )
                        image_window_set_window_position( win, 
                                window_left + window_width, window_top );
                else if( state & GDK_CONTROL_MASK )
                        image_window_set_window_position( win, 
                                image_width - window_width, window_top );
                else
                        image_window_set_window_position( win, 
                                window_left + hstep, window_top );
                handled = TRUE;
                break;

        case GDK_KEY_Up:
                if( state & GDK_SHIFT_MASK )
                        image_window_set_window_position( win, 
                                window_left, window_top - window_height );
                else if( state & GDK_CONTROL_MASK )
                        image_window_set_window_position( win, 
                                window_left, 0 );
                else
                        image_window_set_window_position( win, 
                                window_left, window_top - vstep );
                handled = TRUE;
                break;

        case GDK_KEY_Down:
                if( state & GDK_SHIFT_MASK )
                        image_window_set_window_position( win, 
                                window_left, window_top + window_height );
                else if( state & GDK_CONTROL_MASK )
                        image_window_set_window_position( win, 
                                window_left, image_height - window_height );
                else
                        image_window_set_window_position( win, 
                                window_left, window_top + vstep );
                handled = TRUE;
                break;

        case GDK_KEY_0:
                image_window_bestfit( win );
                handled = TRUE;
                break;

        default:
                break;
        }

        if( !handled ) {
                int i;

                for( i = 0; i < VIPS_NUMBER( magnify_keys ); i++ )
                        if( magnify_keys[i].keyval == keyval ) {
                                int mag;

                                mag = magnify_keys[i].mag;
                                if( state & GDK_CONTROL_MASK )
                                        mag *= -1;

                                image_window_set_mag( win, mag );
                                handled = TRUE;
                                break;
                        }
        }

        return( handled );
}

static void
image_window_gtk_to_image( ImageWindow *win, 
        int gtk_x, int gtk_y, int *image_x, int *image_y )
{
        VipsRect point;

        point.left = gtk_x;
        point.top = gtk_y;
        point.width = 0;
        point.height = 0;
        imagedisplay_gtk_to_image( (Imagedisplay *) win->imagedisplay, &point );
        conversion_to_image_cods( win->conversion->mag,
                point.left, point.top, image_x, image_y );
}

static void
image_window_motion( GtkEventControllerMotion *self,
        gdouble x, gdouble y, gpointer user_data )
{
        ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );

        image_window_gtk_to_image( win, x, y, &win->last_x, &win->last_y );
        image_window_position_changed( win );
}

static gboolean
image_window_scroll( GtkEventControllerMotion *self,
        double dx, double dy, gpointer user_data )
{
        ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );

        if( dy > 0 ) 
                image_window_magout( win, win->last_x, win->last_y );
        else 
                image_window_magin_point( win, win->last_x, win->last_y );

        return( TRUE );
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

        image_window_get_window_position( win, 
                &window_left, &window_top, &window_width, &window_height );

        win->drag_start_x = window_left;
        win->drag_start_y = window_top;
}

static void
image_window_drag_update( GtkEventControllerMotion *self,
        gdouble offset_x, gdouble offset_y, gpointer user_data )
{
        ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );

        image_window_set_window_position( win, 
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

        g_object_set( win->conversion_bar,
                "revealed", g_variant_get_boolean( state ),
                NULL );

        /* Disable most display conversion if the controls are hidden. It's
         * much too confusing.
         */
        g_object_set( win->conversion,
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
        Conversion *conversion = win->conversion;
        int page = VIPS_CLIP( 0, conversion->page, conversion->n_pages - 1 );

        g_object_set( conversion,
                "page", (page + 1) % conversion->n_pages,
                NULL );
}

static void
image_window_prev( GSimpleAction *action, GVariant *state, gpointer user_data )
{
        ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );
        Conversion *conversion = win->conversion;
        int page = VIPS_CLIP( 0, conversion->page, conversion->n_pages - 1 );

        g_object_set( conversion,
                "page", page == 0 ? conversion->n_pages - 1 : page - 1,
                NULL );
}

static int
image_window_find_scale( ImageWindow *win, VipsObject *context, 
        int left, int top, int width, int height,
        double *scale, double *offset )
{
        Conversion *conversion = win->conversion;
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
image_window_scale( GSimpleAction *action, 
        GVariant *state, gpointer user_data )
{
        ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );
        Conversion *conversion = win->conversion;

        int left, top, width, height;
        int right, bottom;
        VipsImage *context;
        double scale, offset;

        if( !conversion->image )
                return;

        image_window_get_window_position( win, &left, &top, &width, &height );
        right = left + width;
        bottom = top + height;

        conversion_to_image_cods( conversion->mag, 
                left, top, &left, &top );
        conversion_to_image_cods( conversion->mag, 
                right, bottom, &right, &bottom );
        width = right - left;
        height = bottom - top;

        context = vips_image_new();
        if( image_window_find_scale( win, VIPS_OBJECT( context ), 
                left, top, width, height, &scale, &offset ) ) {
                image_window_error( win );
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
image_window_log( GSimpleAction *action, GVariant *state, gpointer user_data )
{
        ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );

        g_object_set( win->conversion,
                "log", g_variant_get_boolean( state ),
                NULL );
        
        g_simple_action_set_state( action, state );
}

static void
image_window_falsecolour( GSimpleAction *action, 
        GVariant *state, gpointer user_data )
{
        ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );

        g_object_set( win->conversion,
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
        ConversionMode mode;

        str = g_variant_get_string( state, NULL );
        if( g_str_equal( str, "toilet-roll" ) ) 
                mode = CONVERSION_MODE_TOILET_ROLL;
        else if( g_str_equal( str, "multipage" ) ) 
                mode = CONVERSION_MODE_MULTIPAGE;
        else if( g_str_equal( str, "animated" ) ) 
                mode = CONVERSION_MODE_ANIMATED;
        else if( g_str_equal( str, "pages-as-bands" ) ) 
                mode = CONVERSION_MODE_PAGES_AS_BANDS;
        else
                /* Ignore attempted change.
                 */
                return;

        g_object_set( win->conversion,
                "mode", mode,
                NULL );

        g_simple_action_set_state( action, state );
}

static void
image_window_reset( GSimpleAction *action, 
        GVariant *state, gpointer user_data )
{
        ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );

        g_object_set( win->conversion,
                "falsecolour", FALSE,
                "log", FALSE,
                "scale", 1.0,
                "offset", 0.0,
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

        { "next", image_window_next },
        { "prev", image_window_prev },
        { "scale", image_window_scale },
        { "log", image_window_toggle, NULL, "false", image_window_log },
        { "falsecolour",
                image_window_toggle, NULL, "false", image_window_falsecolour },
        { "mode", image_window_radio, "s", "'multipage'", image_window_mode },

        { "reset", image_window_reset },
};

static void
image_window_init( ImageWindow *win )
{
        GtkBuilder *builder;
        GMenuModel *menu;
        GtkEventController *controller;

        win->progress_timer = g_timer_new();
        win->last_progress_time = -1;
        win->settings = g_settings_new( APP_ID );

        gtk_widget_init_template( GTK_WIDGET( win ) );

        builder = gtk_builder_new_from_resource( 
                APP_PATH "/imagewindow-menu.ui" );
        menu = G_MENU_MODEL( gtk_builder_get_object( builder, 
                "imagewindow-menu" ) );
        gtk_menu_button_set_menu_model( GTK_MENU_BUTTON( win->gears ), menu );
        g_object_unref( builder );

        win->conversion = conversion_new();
        g_object_set( win->conversion_bar,
                "conversion", win->conversion,
                NULL );
        g_object_set( win->info_bar,
                "image_window", win,
                NULL );

        g_signal_connect_object( win->progress_cancel, "clicked", 
                G_CALLBACK( image_window_cancel_clicked ), win, 0 );

        g_signal_connect_object( win->conversion, "changed", 
                G_CALLBACK( image_window_conversion_changed ), win, 0 );

        g_signal_connect_object( win->error_bar, "response", 
                G_CALLBACK( image_window_error_response ), win, 0 );

        g_action_map_add_action_entries( G_ACTION_MAP( win ),
                image_window_entries, G_N_ELEMENTS( image_window_entries ),
                win );

        controller = GTK_EVENT_CONTROLLER( gtk_event_controller_key_new() );
        g_signal_connect( controller, "key-pressed", 
                G_CALLBACK( image_window_key_pressed ), win );
        gtk_widget_add_controller( win->imagedisplay, controller );

        controller = GTK_EVENT_CONTROLLER( gtk_event_controller_motion_new() );
        g_signal_connect( controller, "motion", 
                G_CALLBACK( image_window_motion ), win );
        gtk_widget_add_controller( win->imagedisplay, controller );

        /* Panning windows should use scroll to zoom, according to the HIG.
         */
        controller = GTK_EVENT_CONTROLLER( gtk_event_controller_scroll_new( 
                GTK_EVENT_CONTROLLER_SCROLL_DISCRETE |
                GTK_EVENT_CONTROLLER_SCROLL_VERTICAL ) );
        g_signal_connect( controller, "scroll", 
                G_CALLBACK( image_window_scroll ), win );
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
                G_OBJECT( win->conversion_bar ),
                "revealed", 
                G_SETTINGS_BIND_DEFAULT );

        g_settings_bind( win->settings, "info",
                G_OBJECT( win->info_bar ),
                "revealed", 
                G_SETTINGS_BIND_DEFAULT );

        /* Initial menu state from settings.
         */
        change_state( GTK_WIDGET( win ), "control", 
                g_settings_get_value( win->settings, "control" ) );
        change_state( GTK_WIDGET( win ), "info", 
                g_settings_get_value( win->settings, "info" ) );

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
        BIND( conversion_bar );
        BIND( info_bar );

        image_window_signals[SIG_POSITION_CHANGED] = g_signal_new( 
                "position-changed",
                G_TYPE_FROM_CLASS( class ),
                G_SIGNAL_RUN_LAST,
                0, NULL, NULL,
                g_cclosure_marshal_VOID__VOID,
                G_TYPE_NONE, 0 ); 
}

ImageWindow *
image_window_new( VipsdispApp *app )
{
        return( g_object_new( IMAGE_WINDOW_TYPE, "application", app, NULL ) );
}

void
image_window_open( ImageWindow *win, GFile *file )
{
        if( conversion_set_file( win->conversion, file ) )
                image_window_error( win ); 

        VIPS_UNREF( win->tile_source );
        VIPS_UNREF( win->tile_cache );

        if( !(win->tile_source = tile_source_new_from_file( file )) )
                image_window_error( win ); 
        if( !(win->tile_cache = tile_cache_new( win->tile_source )) )
                image_window_error( win ); 

        g_object_set( win->imagedisplay,
                "tile-cache", win->tile_cache,
                NULL );

        g_signal_connect_object( win->tile_source, "preeval", 
                G_CALLBACK( image_window_preeval ), win, 0 );
        g_signal_connect_object( win->tile_source, "eval", 
                G_CALLBACK( image_window_eval ), win, 0 );
        g_signal_connect_object( win->tile_source, "posteval", 
                G_CALLBACK( image_window_posteval ), win, 0 );
}

Conversion *
image_window_get_conversion( ImageWindow *win )
{
        return( win->conversion );
}

void
image_window_get_last( ImageWindow *win, int *last_x, int *last_y )
{
        *last_x = win->last_x;
        *last_y = win->last_y;
}
