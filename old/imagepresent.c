/* Display an image with gtk3 and libvips. 
 */

#include <stdio.h>
#include <stdlib.h>

#include <gtk/gtk.h>

#include <vips/vips.h>

#include "disp.h"

/*
#define DEBUG_VERBOSE
#define DEBUG
 */

G_DEFINE_TYPE( Imagepresent, imagepresent, GTK_TYPE_SCROLLED_WINDOW );

/* Our signals. 
 */
enum {
	SIG_POSITION_CHANGED,
	SIG_LAST
};

static guint imagepresent_signals[SIG_LAST] = { 0 };

static void
imagepresent_destroy( GtkWidget *widget )
{
	Imagepresent *imagepresent = (Imagepresent *) widget;

#ifdef DEBUG
	printf( "imagepresent_destroy: %p\n", imagepresent ); 
#endif /*DEBUG*/

	VIPS_UNREF( imagepresent->conversion );

	GTK_WIDGET_CLASS( imagepresent_parent_class )->destroy( widget );
}

gboolean
imagepresent_get_image_size( Imagepresent *imagepresent, 
	int *width, int *height )
{
	return( conversion_get_image_size( imagepresent->conversion,
		width, height ) ); 
}

gboolean
imagepresent_get_display_image_size( Imagepresent *imagepresent, 
	int *width, int *height )
{
	return( conversion_get_display_image_size( imagepresent->conversion,
		width, height ) ); 
}

static void
imagepresent_position_changed( Imagepresent *imagepresent )
{
	g_signal_emit( imagepresent, 
		imagepresent_signals[SIG_POSITION_CHANGED], 0 );
}

void
imagepresent_get_window_position( Imagepresent *imagepresent, 
	int *left, int *top, int *width, int *height )
{
	GtkAdjustment *hadj = gtk_scrolled_window_get_hadjustment( 
		GTK_SCROLLED_WINDOW( imagepresent ) );
	GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment( 
		GTK_SCROLLED_WINDOW( imagepresent ) );

	*left = gtk_adjustment_get_value( hadj );
	*top = gtk_adjustment_get_value( vadj );
	*width = gtk_adjustment_get_page_size( hadj );
	*height = gtk_adjustment_get_page_size( vadj );

#ifdef DEBUG
	printf( "imagepresent_get_window_position: %d %d %d %d\n", 
		*left, *top, *width, *height ); 
#endif /*DEBUG*/
}

void
imagepresent_set_window_position( Imagepresent *imagepresent, 
	int left, int top )
{
	GtkAdjustment *hadj = gtk_scrolled_window_get_hadjustment( 
		GTK_SCROLLED_WINDOW( imagepresent ) );
	GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment( 
		GTK_SCROLLED_WINDOW( imagepresent ) );

#ifdef DEBUG
	printf( "imagepresent_set_window_position: %d %d\n", left, top ); 
#endif /*DEBUG*/

	gtk_adjustment_set_value( hadj, left );
	gtk_adjustment_set_value( vadj, top );
}

void
imagepresent_set_menu( Imagepresent *imagepresent, GtkMenu *menu )
{
	g_assert( !imagepresent->menu );

	imagepresent->menu = menu;
	gtk_menu_attach_to_widget( menu, GTK_WIDGET( imagepresent ), NULL );
}

int
imagepresent_set_mag( Imagepresent *imagepresent, int mag )
{
	Conversion *conversion = imagepresent->conversion;

	int display_width;
	int display_height;
	int image_x;
	int image_y;
	int width;
	int height;

#ifdef DEBUG
	printf( "imagepresent_set_mag: %d\n", mag ); 
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

	/* We need to update last_x/_y ... go via image cods.
	 */
	conversion_to_image_cods( conversion->mag,
		imagepresent->last_x, imagepresent->last_y, 
		&image_x, &image_y ); 

	g_object_set( conversion, "mag", mag, NULL ); 

	/* Update the adjustment range.
	 *
	 * After we change the drawingarea size, the adjustments won't get 
	 * updated until we hit the gtk+ main loop again, but that will be too
	 * late, and any imagepresent_set_window_position after this will fail
	 * to work correctly.
	 */
	if( imagepresent_get_display_image_size( imagepresent, 
		&width, &height ) ) { 
		GtkAdjustment *hadj = gtk_scrolled_window_get_hadjustment( 
			GTK_SCROLLED_WINDOW( imagepresent ) );
		GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment( 
			GTK_SCROLLED_WINDOW( imagepresent ) );
#ifdef DEBUG
		printf( "imagepresent_init_adjustments: new size %d %d\n", 
			width, height ); 
#endif /*DEBUG*/

		gtk_adjustment_set_upper( hadj, width );
		gtk_adjustment_set_upper( vadj, height );
	}

	conversion_to_display_cods( conversion->mag,
		image_x, image_y,
		&imagepresent->last_x, &imagepresent->last_y );

	/* mag has changed.
	 */
	imagepresent_position_changed( imagepresent );

	return( 0 );
}

/* Set a new mag, keeping the pixel at x/y in image coordinates at the same 
 * position on the screen, if we can.
 */
static void
imagepresent_set_mag_position( Imagepresent *imagepresent, 
	int mag, int x, int y )
{
	Conversion *conversion = imagepresent->conversion;

	VipsRect old_point;
	VipsRect new_point;

#ifdef DEBUG
	printf( "imagepresent_set_mag_position: %d %d %d\n", mag, x, y ); 
#endif /*DEBUG*/

	/* Map the image pixel at (x, y) to gtk space, ie. the space the user
	 * sees.
	 */
	conversion_to_display_cods( conversion->mag,
		x, y,
		&old_point.left, &old_point.top ); 
	imagedisplay_image_to_gtk( imagepresent->imagedisplay, &old_point );

	/* Mag set can be out of range.
	 */
	if( imagepresent_set_mag( imagepresent, mag ) )
		return;

	/* Map image (x, y) to display coordinates, then move up and left by
	 * the cursor poition to leave the same point at the same place.
	 */
	conversion_to_display_cods( conversion->mag,
		x, y,
		&new_point.left, &new_point.top ); 

	imagepresent_set_window_position( imagepresent,
		new_point.left - old_point.left,
		new_point.top - old_point.top );
}

/* Zoom in, keeping the pixel at x/y in image coordinates at the same position
 * on the screen.
 */
void
imagepresent_magin( Imagepresent *imagepresent, int x, int y )
{
	int mag;

#ifdef DEBUG
	printf( "imagepresent_magin: %d %d\n", x, y ); 
#endif /*DEBUG*/

	g_object_get( imagepresent->conversion, "mag", &mag, NULL ); 
	if( mag <= 0 ) {
		if( mag >= -2 )
			imagepresent_set_mag_position( imagepresent, 1, x, y );
		else
			imagepresent_set_mag_position( imagepresent, 
				mag / 2, x, y );
	}
	else 
		imagepresent_set_mag_position( imagepresent, mag * 2, x, y );
}

/* Zoom out, keeping the pixel at x/y in image coordinates at the same position
 * on the screen.
 */
void
imagepresent_magout( Imagepresent *imagepresent, int x, int y )
{
	int image_width;
	int image_height;
	int mag;

#ifdef DEBUG
	printf( "imagepresent_magout:\n" ); 
#endif /*DEBUG*/

	/* Don't let the image get too small.
	 */
	if( !imagepresent_get_display_image_size( imagepresent, 
		&image_width, &image_height ) ) 
		return;
	if( image_width == 1 ||
		image_height == 1 )
		return;

	g_object_get( imagepresent->conversion, "mag", &mag, NULL ); 
	if( mag >= 0 )  {
		if( mag < 2 ) 
			imagepresent_set_mag_position( imagepresent, -2, x, y );
		else
			imagepresent_set_mag_position( imagepresent, 
				mag / 2, x, y );
	}
	else 
		imagepresent_set_mag_position( imagepresent, mag * 2, x, y );
}

void
imagepresent_bestfit( Imagepresent *imagepresent )
{
	int image_width;
	int image_height;

#ifdef DEBUG
	printf( "imagepresent_bestfit:\n" ); 
#endif /*DEBUG*/

	if( imagepresent_get_display_image_size( imagepresent, 
		&image_width, &image_height ) ) {
		GtkAllocation allocation;
		double hfac;
		double vfac;
		double fac;
		int mag;

		gtk_widget_get_allocation( 
			GTK_WIDGET( imagepresent->imagedisplay ), &allocation );
		hfac = (double) allocation.width / image_width;
		vfac = (double) allocation.height / image_height;
		fac = VIPS_MIN( hfac, vfac );

		/* 0.999 means we don't round up on an exact fit.
		 *
		 * FIXME ... yuk
		 */
		mag = fac >= 1 ? fac : -((int) (0.99999999 + 1.0 / fac));

		imagepresent_set_mag( imagepresent, mag );
	}
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
imagepresent_key_press_event( GtkWidget *widget, GdkEventKey *event, 
	Imagepresent *imagepresent )
{
	GtkAdjustment *hadj = gtk_scrolled_window_get_hadjustment( 
		GTK_SCROLLED_WINDOW( imagepresent ) );
	GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment( 
		GTK_SCROLLED_WINDOW( imagepresent ) );

	int hstep = gtk_adjustment_get_step_increment( hadj );
	int vstep = gtk_adjustment_get_step_increment( vadj );

	int image_width;
	int image_height;
	int window_left;
	int window_top;
	int window_width;
	int window_height;
	int image_x;
	int image_y;

	gboolean handled;

#ifdef DEBUG
	printf( "imagepresent_key_press_event: key = %d\n", event->keyval );
#endif /*DEBUG*/

	handled = FALSE;

	if( !imagepresent_get_display_image_size( imagepresent, 
		&image_width, &image_height ) ) 
		return( handled ); 
	imagepresent_get_window_position( imagepresent, 
		&window_left, &window_top, &window_width, &window_height );

	switch( event->keyval ) {
	case GDK_KEY_Left:
		if( !(event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK)) )
			imagepresent_set_window_position( imagepresent, 
				window_left - hstep, window_top ); 
		else if( event->state & GDK_SHIFT_MASK )
			imagepresent_set_window_position( imagepresent,
				window_left - window_width, window_top ); 
		else if( event->state & GDK_CONTROL_MASK )
			imagepresent_set_window_position( imagepresent,
				0, window_top );

		handled = TRUE;
		break;

	case GDK_KEY_Right:
		if( !(event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK)) )
			imagepresent_set_window_position( imagepresent, 
				window_left + hstep, window_top ); 
		else if( event->state & GDK_SHIFT_MASK )
			imagepresent_set_window_position( imagepresent,
				window_left + window_width, window_top ); 
		else if( event->state & GDK_CONTROL_MASK )
			imagepresent_set_window_position( imagepresent,
				image_width - window_width, window_top );

		handled = TRUE;
		break;

	case GDK_KEY_Up:
		if( !(event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK)) )
			imagepresent_set_window_position( imagepresent, 
				window_left, window_top - vstep ); 
		else if( event->state & GDK_SHIFT_MASK )
			imagepresent_set_window_position( imagepresent,
				window_left, window_top - window_height ); 
		else if( event->state & GDK_CONTROL_MASK )
			imagepresent_set_window_position( imagepresent,
				window_left, 0 );

		handled = TRUE;
		break;

	case GDK_KEY_Down:
		if( !(event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK)) )
			imagepresent_set_window_position( imagepresent, 
				window_left, window_top + vstep ); 
		else if( event->state & GDK_SHIFT_MASK )
			imagepresent_set_window_position( imagepresent,
				window_left, window_top + window_height ); 
		else if( event->state & GDK_CONTROL_MASK )
			imagepresent_set_window_position( imagepresent,
				window_left, image_height - window_height );

		handled = TRUE;
		break;

	case GDK_KEY_i:
	case GDK_KEY_plus:
		conversion_to_image_cods( imagepresent->conversion->mag,
			imagepresent->last_x, imagepresent->last_y,
			&image_x, &image_y ); 
		imagepresent_magin( imagepresent, image_x, image_y );

		handled = TRUE;
		break;

	case GDK_KEY_o:
	case GDK_KEY_minus:
		conversion_to_image_cods( imagepresent->conversion->mag,
			imagepresent->last_x, imagepresent->last_y,
			&image_x, &image_y ); 
		imagepresent_magout( imagepresent, image_x, image_y ); 

		handled = TRUE;
		break;

	case GDK_KEY_0:
		imagepresent_bestfit( imagepresent ); 

		handled = TRUE;
		break;

	default:
		break;
	}

	if( !handled ) {
		int i;

		for( i = 0; i < VIPS_NUMBER( magnify_keys ); i++ ) 
			if( event->keyval == magnify_keys[i].keyval ) {
				int mag;

				int window_left;
				int window_top;
				int window_width;
				int window_height;

				int image_x;
				int image_y;

				mag = magnify_keys[i].mag;
				if( event->state & GDK_CONTROL_MASK )
					mag *= -1;

				imagepresent_get_window_position( imagepresent, 
					&window_left, &window_top, 
					&window_width, &window_height );

				conversion_to_image_cods( 
					imagepresent->conversion->mag,
					window_left + window_width / 2, 
					window_top + window_height / 2,
					&image_x, &image_y ); 

				imagepresent_set_mag_position( imagepresent, 
					mag, image_x, image_y );

				handled = TRUE;
				break;
			}
	}

	return( handled ); 
}

/* How does gtk4 draw the focus highlight?
 *
static gboolean
imagepresent_draw( GtkWidget *widget, cairo_t *cr )
{
	GdkWindow *window = gtk_widget_get_window( widget );
	GtkStyleContext *context = gtk_widget_get_style_context( widget );

	if( gtk_cairo_should_draw_window( cr, window ) ) {
		if( gtk_widget_has_focus( widget ) ) 
			gtk_render_focus( context, cr, 0, 0, 
				gtk_widget_get_allocated_width( widget ), 
				gtk_widget_get_allocated_height( widget ) ); 
	}

	return( FALSE ); 
}
 *
 */

static gboolean
imagepresent_button_press_event( GtkWidget *widget, GdkEventButton *event, 
	Imagepresent *imagepresent )
{
	gboolean handled;

#ifdef DEBUG
	printf( "imagepresent_button_press_event:\n" ); 
#endif /*DEBUG*/

	handled = FALSE;

	if( event->button == 1 ) {
		gtk_widget_grab_focus( GTK_WIDGET( imagepresent ) );
		handled = TRUE;
	}

	switch( imagepresent->state ) {
	case IMAGEPRESENT_WAIT:
		if( event->button == 1 ) {
			int window_left;
			int window_top;
			int window_width;
			int window_height;

			imagepresent_get_window_position( imagepresent, 
				&window_left, &window_top, 
				&window_width, &window_height );

			imagepresent->state = IMAGEPRESENT_DRAG;
			imagepresent->drag_start_x = 
				window_left + event->x_root;
			imagepresent->drag_start_y = 
				window_top + event->y_root;

			handled = TRUE;
		}
		else if( event->button == 3 && 
			imagepresent->menu ) 
			gtk_menu_popup_at_pointer( imagepresent->menu, 
				(GdkEvent *) event );
		break;
	
	default:
		break;
	}

	return( handled ); 
}

static gboolean
imagepresent_motion_notify_event( GtkWidget *widget, GdkEventMotion *event, 
	Imagepresent *imagepresent )
{
	gboolean handled;
	VipsRect point;

	handled = FALSE;

	point.left = event->x;
	point.top = event->y;
	point.width = 0;
	point.height = 0;
	imagedisplay_gtk_to_image( imagepresent->imagedisplay, &point );
	imagepresent->last_x = point.left;
	imagepresent->last_y = point.top;

#ifdef DEBUG_VERBOSE
	printf( "imagepresent_motion_notify_event: %d, %d\n", 
		point.left, point.top );
#endif /*DEBUG_VERBOSE*/

	switch( imagepresent->state ) {
	case IMAGEPRESENT_DRAG:
		imagepresent_set_window_position( imagepresent,
			imagepresent->drag_start_x - event->x_root, 
			imagepresent->drag_start_y - event->y_root );  
		break;

	default:
		break;
	}

	/* last_x/_y has changed.
	 */
	imagepresent_position_changed( imagepresent );

	return( handled ); 
}

static gboolean
imagepresent_button_release_event( GtkWidget *widget, GdkEventButton *event, 
	Imagepresent *imagepresent )
{
	gboolean handled;

#ifdef DEBUG
	printf( "imagepresent_button_release_event:\n" ); 
#endif /*DEBUG*/

	handled = FALSE;

	switch( imagepresent->state ) {
	case IMAGEPRESENT_DRAG:
		if( event->button == 1 ) {
			imagepresent->state = IMAGEPRESENT_WAIT;

			handled = TRUE;
		}
		break;

	default:
		break;
	}

	return( handled ); 
}

static gboolean
imagepresent_scroll_event( GtkWidget *widget, GdkEventScroll *event, 
	Imagepresent *imagepresent )
{
	gboolean handled;
	VipsRect point;
	int image_x;
	int image_y;

#ifdef DEBUG
	printf( "imagepresent_scroll_event:\n" );
#endif /*DEBUG*/

	handled = FALSE;

	switch( event->direction ) {
	case GDK_SCROLL_UP:
		point.left = event->x;
		point.top = event->y;
		point.width = 0;
		point.height = 0;
		imagedisplay_gtk_to_image( imagepresent->imagedisplay, &point );
		conversion_to_image_cods( imagepresent->conversion->mag,
			point.left, point.top,
			&image_x, &image_y ); 
		imagepresent_magin( imagepresent, image_x, image_y );

		handled = TRUE;
		break;

	case GDK_SCROLL_DOWN:
		point.left = event->x;
		point.top = event->y;
		point.width = 0;
		point.height = 0;
		imagedisplay_gtk_to_image( imagepresent->imagedisplay, &point );
		conversion_to_image_cods( imagepresent->conversion->mag,
			point.left, point.top,
			&image_x, &image_y ); 
		imagepresent_magout( imagepresent, image_x, image_y ); 

		handled = TRUE;
		break;

	default:
		break;
	}

	return( handled ); 
}

static void
imagepresent_init( Imagepresent *imagepresent )
{
#ifdef DEBUG
	printf( "imagepresent_init: %p\n", imagepresent ); 
#endif /*DEBUG*/

	gtk_widget_set_can_focus( GTK_WIDGET( imagepresent ), TRUE ); 

	/* The imagepresent takes the focus, so we must listen for keypresses
	 * there. We get the mouse position from (last_x, last_y), which we
	 * record in the motion handler. 
	 */
	g_signal_connect( imagepresent, "key-press-event",
		G_CALLBACK( imagepresent_key_press_event ), imagepresent ); 

	// FIXME ... how do we draw the focus highlight?
	/* Draw the focus indicator after rendering the image.
	g_signal_connect_after( imagepresent, "draw",
		G_CALLBACK( imagepresent_draw ), imagepresent );
	 */

	imagepresent->conversion = conversion_new();
	imagepresent->imagedisplay = 
		imagedisplay_new( imagepresent->conversion );

	g_signal_connect( imagepresent->imagedisplay, "button-press-event",
		G_CALLBACK( imagepresent_button_press_event ), imagepresent ); 
	g_signal_connect( imagepresent->imagedisplay, "motion-notify-event",
		G_CALLBACK( imagepresent_motion_notify_event ), imagepresent );
	g_signal_connect( imagepresent->imagedisplay, "button-release-event",
		G_CALLBACK( imagepresent_button_release_event ), imagepresent );
	g_signal_connect( imagepresent->imagedisplay, "scroll-event",
		G_CALLBACK( imagepresent_scroll_event ), imagepresent );

	gtk_widget_add_events( GTK_WIDGET( imagepresent->imagedisplay ),
		GDK_POINTER_MOTION_MASK | 
		GDK_BUTTON_PRESS_MASK | 
		GDK_BUTTON_RELEASE_MASK | 
		GDK_SCROLL_MASK );

	gtk_container_add( GTK_CONTAINER( imagepresent ), 
		GTK_WIDGET( imagepresent->imagedisplay ) );
}

static void
imagepresent_class_init( ImagepresentClass *class )
{
	GObjectClass *gobject_class = G_OBJECT_CLASS( class );
	GtkWidgetClass *widget_class = (GtkWidgetClass*) class;

	gobject_class->dispose = imagedisplay_dispose;

	imagepresent_signals[SIG_POSITION_CHANGED] = g_signal_new( 
		"position_changed",
		G_TYPE_FROM_CLASS( class ),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET( ImagepresentClass, position_changed ), 
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0 ); 
}

Imagepresent *
imagepresent_new( void ) 
{
	Imagepresent *imagepresent;

	imagepresent = g_object_new( imagepresent_get_type(),
		NULL );

	gtk_widget_show( GTK_WIDGET( imagepresent->imagedisplay ) );

	return( imagepresent ); 
}
