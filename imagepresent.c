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

Imagepresent *
imagepresent_new( void ) 
{
	Imagepresent *imagepresent;

	printf( "imagepresent_new:\n" ); 

	imagepresent = g_object_new( imagepresent_get_type(),
		NULL );

	imagepresent->imagedisplay = imagedisplay_new();
	gtk_container_add( GTK_CONTAINER( imagepresent ), 
		GTK_WIDGET( imagepresent->imagedisplay ) );

	gtk_widget_show( GTK_WIDGET( imagepresent->imagedisplay ) );

	return( imagepresent ); 
}
