/*
#define DEBUG
 */

#include "vipsdisp.h"

struct _ImageWindow
{
	GtkApplicationWindow parent;

	Conversion *conversion;

	/* Last known mouse poistion, in image coordinates.
	 */
	int last_x;
	int last_y;

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

	G_OBJECT_CLASS( image_window_parent_class )->dispose( object );
}

static void
image_window_preeval( VipsImage *image, 
        VipsProgress *progress, ImageWindow *win )
{
	gtk_action_bar_set_revealed( GTK_ACTION_BAR( win->progress_bar ), 
		TRUE );
}

static void
image_window_eval( VipsImage *image, 
        VipsProgress *progress, ImageWindow *win )
{
	static int previous_percent = -1;

#ifdef DEBUG_VERBOSE
        printf( "image_window_eval: %d%%\n", progress->percent );
#endif /*DEBUG_VERBOSE*/

        if( progress->percent != previous_percent ) {
                char str[256];
                VipsBuf buf = VIPS_BUF_STATIC( str );

                gtk_progress_bar_set_fraction(
                        GTK_PROGRESS_BAR( win->progress ),
                        progress->percent / 100.0 );
                vips_buf_appendf( &buf, "%d%% complete, %d seconds to go",
                        progress->percent, progress->eta );
                gtk_progress_bar_set_text( GTK_PROGRESS_BAR( win->progress ),
                        vips_buf_all( &buf ) );

                previous_percent = progress->percent;

                /* Run one loop iteration, don't block.
                 */
                g_main_context_iteration( NULL, FALSE );
        }
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
image_window_conversion_changed( Conversion *conversion, ImageWindow *win )
{
        const char *title;
	char str[256];
	VipsBuf buf = VIPS_BUF_STATIC( str );

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

	/*
        GVariant *state;
        const char *str;

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
                change_state( GTK_WIDGET( win ), "mode", state );
        }

        state = g_variant_new_boolean( conversion->falsecolour );
        change_state( GTK_WIDGET( win ), "falsecolour", state );

        state = g_variant_new_boolean( conversion->log );
        change_state( GTK_WIDGET( win ), "log", state );

	 */
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
        Conversion *conversion = win->conversion;

        int display_width;
        int display_height;

#ifdef DEBUG
        printf( "image_window_set_mag: %d\n", mag );
#endif /*DEBUG*/

        /* Limit mag range to keep the displayed image within 1 and
         * VIPS_MAX_COORD on both axis.
         */
        conversion_to_display_cods( mag,
                conversion->width, conversion->height,
                &display_width, &display_height );
        if( VIPS_MIN( display_width, display_height ) < 2 ||
                VIPS_MAX( display_width, display_height ) >= VIPS_MAX_COORD )
                return( -1 );

        g_object_set( conversion, "mag", mag, NULL );

        /* mag has changed.
         */
        image_window_position_changed( win );

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
        printf( "image_window_set_mag_position: %d %d %d\n", mag, x, y );
#endif /*DEBUG*/ 
  
        /* Map the image pixel at (x, y) to gtk space, ie. the space the user
         * sees.
         */
        conversion_to_display_cods( conversion->mag,
                x, y,
                &old_point.left, &old_point.top ); 
        imagedisplay_image_to_gtk( VIPSDISP_IMAGEDISPLAY( win->imagedisplay ), 
		&old_point );
        
        /* Mag set can be out of range.
         */
        if( image_window_set_mag( win, mag ) )
                return;

        /* Map image (x, y) to display coordinates, then move up and left by
         * the cursor poition to leave the same point at the same place.
         */
        conversion_to_display_cods( conversion->mag,
                x, y,
                &new_point.left, &new_point.top );

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
image_window_oneone_action( GSimpleAction *action, 
	GVariant *parameter, gpointer user_data )
{
        ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );

	image_window_set_mag( win, 1 );
}

static void
image_window_bestfit_action( GSimpleAction *action,
        GVariant *parameter, gpointer user_data )
{
        ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );

	image_window_bestfit( win );
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

	printf( "image_window_key_pressed: keyval = %d, state = %d\n", 
		keyval, state );

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
image_window_motion( GtkEventControllerMotion* self,
	gdouble x, gdouble y, gpointer user_data )
{
        ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );

	VipsRect point;

	point.left = x;
	point.top = y;
	imagedisplay_gtk_to_image( (Imagedisplay *) win->imagedisplay, &point );
	conversion_to_image_cods( win->conversion->mag,
		point.left, point.top, &win->last_x, &win->last_y );
}

static GActionEntry image_window_entries[] = {
        { "magin", image_window_magin_action },
        { "magout", image_window_magout_action },
        { "bestfit", image_window_bestfit_action },
        { "oneone", image_window_oneone_action },

	/*
        { "duplicate", image_window_duplicate },
        { "replace", image_window_replace },
        { "saveas", image_window_saveas },
        { "close", image_window_close },

        { "fullscreen", 
		image_window_toggle, NULL, "false", image_window_fullscreen },
        { "control", image_window_toggle, NULL, "false", image_window_control },
        { "info", image_window_toggle, NULL, "false", image_window_info },

        { "next", image_window_next },
        { "prev", image_window_prev },
        { "scale", image_window_scale },
        { "log", image_window_toggle, NULL, "false", image_window_log },
        { "falsecolour",
                image_window_toggle, NULL, "false", image_window_falsecolour },
        { "mode", image_window_radio, "s", "'multipage'", image_window_mode },
        { "reset", image_window_reset },
        { "hide_display_control_bar", image_window_hide_display_control_bar },
	 */
};

static void
image_window_init( ImageWindow *win )
{
	GtkBuilder *builder;
	GMenuModel *menu;
	GtkEventController *controller;

	gtk_widget_init_template( GTK_WIDGET( win ) );

	builder = gtk_builder_new_from_resource( 
		"/org/libvips/vipsdisp/imagewindow-menu.ui" );
	menu = G_MENU_MODEL( gtk_builder_get_object( builder, 
		"imagewindow-menu" ) );
	gtk_menu_button_set_menu_model( GTK_MENU_BUTTON( win->gears ), menu );
	g_object_unref( builder );

	win->conversion = conversion_new();
	g_object_set( win->imagedisplay,
		"conversion", win->conversion,
		NULL );

        g_signal_connect_object( win->conversion, "preeval", 
                G_CALLBACK( image_window_preeval ), win, 0 );
        g_signal_connect_object( win->conversion, "eval", 
                G_CALLBACK( image_window_eval ), win, 0 );
        g_signal_connect_object( win->conversion, "posteval", 
                G_CALLBACK( image_window_posteval ), win, 0 );
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

}

#define BIND( field ) \
	gtk_widget_class_bind_template_child( GTK_WIDGET_CLASS( class ), \
		ImageWindow, field );

static void
image_window_class_init( ImageWindowClass *class )
{
	G_OBJECT_CLASS( class )->dispose = image_window_dispose;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( class ),
		"/org/libvips/vipsdisp/imagewindow.ui");

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
}
