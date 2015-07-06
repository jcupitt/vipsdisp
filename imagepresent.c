/* Display an image with gtk3 and libvips. 
 */

#include <stdio.h>
#include <stdlib.h>

#include <gtk/gtk.h>

#include <vips/vips.h>

#include "disp.h"

G_DEFINE_TYPE( Imagepresent, imagepresent, GTK_TYPE_SCROLLED_WINDOW );

static void
imagepresent_init( Imagepresent *Imagepresent )
{
	printf( "imagepresent_init:\n" ); 
}

static void
imagepresent_class_init( ImagepresentClass *class )
{
	printf( "imagepresent_class_init:\n" ); 
}

void
imagepresent_get_window_size( Imagepresent *imagepresent, 
	int *width, int *height )
{
	GtkAdjustment *hadj = gtk_scrolled_window_get_hadjustment( 
		GTK_SCROLLED_WINDOW( imagepresent ) );
	GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment( 
		GTK_SCROLLED_WINDOW( imagepresent ) );

	*width = gtk_adjustment_get_page_size( hadj );
	*height = gtk_adjustment_get_page_size( vadj );
}

gboolean
imagepresent_get_image_size( Imagepresent *imagepresent, 
	int *width, int *height )
{
	return( imagedisplay_get_image_size( imagepresent->imagedisplay,
		width, height ) ); 
}

gboolean
imagepresent_get_display_image_size( Imagepresent *imagepresent, 
	int *width, int *height )
{
	return( imagedisplay_get_display_image_size( imagepresent->imagedisplay,
		width, height ) ); 
}

void
imagepresent_set_mag( Imagepresent *imagepresent, int mag )
{
	imagedisplay_set_mag( imagepresent->imagedisplay, mag ); 
}

void
imagepresent_magin( Imagepresent *imagepresent )
{
	int mag = imagedisplay_get_mag( imagepresent->imagedisplay );

	printf( "imagepresent_magin:\n" ); 

	if( mag <= 0 )  {
		if( mag >= -2 )
			imagepresent_set_mag( imagepresent, 1 );
		else
			imagepresent_set_mag( imagepresent, mag / 2 );
	}
	else 
		imagepresent_set_mag( imagepresent, mag * 2 );
}

void
imagepresent_magout( Imagepresent *imagepresent )
{
	int mag = imagedisplay_get_mag( imagepresent->imagedisplay );

	printf( "imagepresent_magout:\n" ); 

	if( mag >= 0 )  {
		if( mag < 2 ) 
			imagepresent_set_mag( imagepresent, -2 );
		else
			imagepresent_set_mag( imagepresent, mag / 2 );
	}
	else 
		imagepresent_set_mag( imagepresent, mag * 2 );
}

void
imagepresent_bestfit( Imagepresent *imagepresent )
{
	int image_width;
	int image_height;

	printf( "imagepresent_bestfit:\n" ); 

	if( imagepresent_get_image_size( imagepresent, 
		&image_width, &image_height ) ) {
		int window_width;
		int window_height;
		double hfac;
		double vfac;
		double fac;

		imagepresent_get_window_size( imagepresent, 
			&window_width, &window_height ); 
		hfac = (double) window_width / image_width;
		vfac = (double) window_width / image_height;
		fac = VIPS_MIN( hfac, vfac );

		if( fac >= 1 )
			imagepresent_set_mag( imagepresent, (int) fac );
		else
			/* 0.999 means we don't round up on an exact fit.
			 *
			 * FIXME ... yuk
			 */
			imagepresent_set_mag( imagepresent, 
				-((int) (0.99999999 + 1.0 / fac)) );
	}
}

char *
imagepresent_get_path( Imagepresent *imagepresent ) 
{
	char *path;

	if( imagepresent->file && 
		(path = g_file_get_path( imagepresent->file )) ) 
		return( path );

	return( NULL );
}

int
imagepresent_set_file( Imagepresent *imagepresent, GFile *file )
{
	VIPS_UNREF( imagepresent->file );

	imagepresent->file = file;
	g_object_ref( file ); 

	if( imagedisplay_set_file( imagepresent->imagedisplay, 
		imagepresent->file ) )
	       return( -1 ); 	

	return( 0 );
}

static void
imagepresent_get_position( Imagepresent *imagepresent, int *x, int *y )
{
	GtkAdjustment *hadj = gtk_scrolled_window_get_hadjustment( 
		GTK_SCROLLED_WINDOW( imagepresent ) );
	GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment( 
		GTK_SCROLLED_WINDOW( imagepresent ) );

	*x = gtk_adjustment_get_value( hadj );
	*y = gtk_adjustment_get_value( vadj );
}

static void
imagepresent_set_position( Imagepresent *imagepresent, int x, int y )
{
	GtkAdjustment *hadj = gtk_scrolled_window_get_hadjustment( 
		GTK_SCROLLED_WINDOW( imagepresent ) );
	GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment( 
		GTK_SCROLLED_WINDOW( imagepresent ) );

	if( gtk_adjustment_get_value( hadj ) != x ) { 
		gtk_adjustment_set_value( hadj, x );
		gtk_adjustment_value_changed( hadj );
	}
	if( gtk_adjustment_get_value( vadj ) != y ) { 
		gtk_adjustment_set_value( vadj, y );
		gtk_adjustment_value_changed( vadj );
	}
}

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
	int window_width;
	int window_height;
	int x;
	int y;
	int image_x;
	int image_y;

	gboolean handled;

	printf( "imagepresent_key_press_event: key = %d\n", event->keyval );

	handled = FALSE;

	if( !imagepresent_get_display_image_size( imagepresent, 
		&image_width, &image_height ) ) 
		return( handled ); 
	imagepresent_get_window_size( imagepresent, 
		&window_width, &window_height ); 
	imagepresent_get_position( imagepresent, &x, &y );

	switch( event->keyval ) {
	case GDK_KEY_Left:
		if( !(event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK)) )
			imagepresent_set_position( imagepresent, 
				x - hstep, y ); 
		else if( event->state & GDK_SHIFT_MASK )
			imagepresent_set_position( imagepresent,
				x - window_width, y ); 
		else if( event->state & GDK_CONTROL_MASK )
			imagepresent_set_position( imagepresent,
				0, y );

		handled = TRUE;
		break;

	case GDK_KEY_Right:
		if( !(event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK)) )
			imagepresent_set_position( imagepresent, 
				x + hstep, y ); 
		else if( event->state & GDK_SHIFT_MASK )
			imagepresent_set_position( imagepresent,
				x + window_width, y ); 
		else if( event->state & GDK_CONTROL_MASK )
			imagepresent_set_position( imagepresent,
				image_width - window_width, y );

		handled = TRUE;
		break;

	case GDK_KEY_Up:
		if( !(event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK)) )
			imagepresent_set_position( imagepresent, 
				x, y - vstep ); 
		else if( event->state & GDK_SHIFT_MASK )
			imagepresent_set_position( imagepresent,
				x, y - window_height ); 
		else if( event->state & GDK_CONTROL_MASK )
			imagepresent_set_position( imagepresent,
				x, 0 );

		handled = TRUE;
		break;

	case GDK_KEY_Down:
		if( !(event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK)) )
			imagepresent_set_position( imagepresent, 
				x, y + vstep ); 
		else if( event->state & GDK_SHIFT_MASK )
			imagepresent_set_position( imagepresent,
				x, y + window_height ); 
		else if( event->state & GDK_CONTROL_MASK )
			imagepresent_set_position( imagepresent,
				x, image_height - window_height );

		handled = TRUE;
		break;

	case GDK_KEY_i:
	case GDK_KEY_plus:
		imagedisplay_to_image_cods( imagepresent->imagedisplay,
			imagepresent->last_x, imagepresent->last_y,
			&image_x, &image_y ); 

		handled = TRUE;
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

	printf( "imagepresent_motion_notify_event: %g, %g\n", 
		event->x, event->y );

	handled = FALSE;

	imagepresent->last_x = event->x;
	imagepresent->last_y = event->y;

	return( handled ); 
}

Imagepresent *
imagepresent_new( void ) 
{
	Imagepresent *imagepresent;

	printf( "imagepresent_new:\n" ); 

	imagepresent = g_object_new( imagepresent_get_type(),
		NULL );

	imagepresent->imagedisplay = imagedisplay_new();

	gtk_widget_set_can_focus( GTK_WIDGET( imagepresent->imagedisplay ), 
		TRUE ); 
	g_signal_connect( imagepresent->imagedisplay, "key-press-event",
		G_CALLBACK( imagepresent_key_press_event ), imagepresent ); 
	g_signal_connect( imagepresent->imagedisplay, "motion-notify-event",
		G_CALLBACK( imagepresent_motion_notify_event ), imagepresent );
	gtk_widget_add_events( GTK_WIDGET( imagepresent->imagedisplay ),
		GDK_POINTER_MOTION_MASK | GDK_KEY_PRESS_MASK );

	gtk_container_add( GTK_CONTAINER( imagepresent ), 
		GTK_WIDGET( imagepresent->imagedisplay ) );

	gtk_widget_show( GTK_WIDGET( imagepresent->imagedisplay ) );

	return( imagepresent ); 
}
