/* Display an image with gtk3 and libvips. 
 */

#include <stdio.h>
#include <stdlib.h>

#include <gtk/gtk.h>

#include <vips/vips.h>

#include "disp.h"

G_DEFINE_TYPE( Imagedisplay, imagedisplay, GTK_TYPE_DRAWING_AREA );

static void
imagedisplay_empty( Imagedisplay *imagedisplay )
{
	if( imagedisplay->region ) {
		g_object_unref( imagedisplay->region );
		imagedisplay->region = NULL;
	}

	if( imagedisplay->display ) {
		g_object_unref( imagedisplay->display );
		imagedisplay->display = NULL;
	}

	if( imagedisplay->image ) {
		g_object_unref( imagedisplay->image );
		imagedisplay->image = NULL;
	}
}

static void
imagedisplay_destroy( GtkWidget *widget )
{
	Imagedisplay *imagedisplay = (Imagedisplay *) widget;

	printf( "imagedisplay_destroy:\n" ); 

	imagedisplay_empty( imagedisplay );

	GTK_WIDGET_CLASS( imagedisplay_parent_class )->destroy( widget );
}
static void
imagedisplay_draw_rect( Imagedisplay *imagedisplay, 
	cairo_t *cr, VipsRect *expose )
{
	VipsRect image;
	VipsRect clip;
	unsigned char *cairo_buffer;
	int x, y;
	cairo_surface_t *surface;

	/*
	printf( "imagedisplay_draw_rect: "
		"left = %d, top = %d, width = %d, height = %d\n",
		expose->left, expose->top,
		expose->width, expose->height );
	 */

	/* Clip against the image size ... we don't want to try painting 
	 * outside the image area.
	 */
	image.left = 0;
	image.top = 0;
	image.width = imagedisplay->region->im->Xsize;
	image.height = imagedisplay->region->im->Ysize;
	vips_rect_intersectrect( &image, expose, &clip );
	if( vips_rect_isempty( &clip ) ||
		vips_region_prepare( imagedisplay->region, &clip ) )
		return;

	/* libvips is RGB, cairo is ARGB, we have to repack the data.
	 */
	cairo_buffer = g_malloc( clip.width * clip.height * 4 );

	for( y = 0; y < clip.height; y++ ) {
		VipsPel *p = VIPS_REGION_ADDR( imagedisplay->region, 
			clip.left, clip.top + y );
		unsigned char *q = cairo_buffer + clip.width * 4 * y;

		for( x = 0; x < clip.width; x++ ) {
			q[0] = p[2];
			q[1] = p[1];
			q[2] = p[0];
			q[3] = 0;

			p += 3;
			q += 4;
		}
	}

	surface = cairo_image_surface_create_for_data( cairo_buffer, 
		CAIRO_FORMAT_RGB24, clip.width, clip.height, clip.width * 4 );

	cairo_set_source_surface( cr, surface, clip.left, clip.top );

	cairo_paint( cr );

	g_free( cairo_buffer ); 

	cairo_surface_destroy( surface ); 
}

static gboolean
imagedisplay_draw( GtkWidget *widget, cairo_t *cr )
{
	Imagedisplay *imagedisplay = (Imagedisplay *) widget;

	if( imagedisplay->region ) {
		cairo_rectangle_list_t *rectangle_list = 
			cairo_copy_clip_rectangle_list( cr );

		//printf( "disp_draw:\n" ); 

		if( rectangle_list->status == CAIRO_STATUS_SUCCESS ) { 
			int i;

			for( i = 0; i < rectangle_list->num_rectangles; i++ ) {
				cairo_rectangle_t *rectangle = 
					&rectangle_list->rectangles[i];
				VipsRect expose;

				expose.left = rectangle->x;
				expose.top = rectangle->y;
				expose.width = rectangle->width;
				expose.height = rectangle->height;

				imagedisplay_draw_rect( imagedisplay, 
					cr, &expose );
			}
		}

		cairo_rectangle_list_destroy( rectangle_list );
	}

	return( TRUE ); 
}

static void
imagedisplay_init( Imagedisplay *imagedisplay )
{
	printf( "imagedisplay_init:\n" ); 

	imagedisplay->mag = 1;
}

static void
imagedisplay_class_init( ImagedisplayClass *class )
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS( class );

	printf( "imagedisplay_class_init:\n" ); 

	widget_class->destroy = imagedisplay_destroy;
	widget_class->draw = imagedisplay_draw;

}

static void
imagedisplay_close_memory( VipsImage *image, gpointer contents )
{
	g_free( contents );
}

typedef struct _ImagedisplayUpdate {
	Imagedisplay *imagedisplay;
	VipsRect rect;
} ImagedisplayUpdate;

/* The main GUI thread runs this when it's idle and there are tiles that need
 * painting. 
 */
static gboolean
imagedisplay_render_cb( ImagedisplayUpdate *update )
{
	gtk_widget_queue_draw_area( GTK_WIDGET( update->imagedisplay ),
		update->rect.left, update->rect.top,
		update->rect.width, update->rect.height );

	g_free( update );

	return( FALSE );
}

/* Come here from the vips_sink_screen() background thread when a tile has been
 * calculated. We can't paint the screen directly since the main GUI thread
 * might be doing something. Instead, we add an idle callback which will be
 * run by the main GUI thread when it next hits the mainloop.
 */
static void
imagedisplay_render_notify( VipsImage *image, VipsRect *rect, void *client )
{
	Imagedisplay *imagedisplay = (Imagedisplay *) client;
	ImagedisplayUpdate *update = g_new( ImagedisplayUpdate, 1 );

	update->imagedisplay = imagedisplay;
	update->rect = *rect;

	g_idle_add( (GSourceFunc) imagedisplay_render_cb, update );
}

/* Make the image for display from the raw disc image. Could do
 * anything here, really. Uncomment sections to try different effects. Convert
 * to 8-bit RGB would be a good idea.
 */
static VipsImage *
imagedisplay_display_image( Imagedisplay *imagedisplay, VipsImage *in )
{
	VipsImage *image;
	VipsImage *x;

	/* image redisplays the head of the pipeline. Hold a ref to it as we
	 * work.
	 */
	image = in;
	g_object_ref( image ); 

	if( imagedisplay->mag < 0 ) {
		if( vips_subsample( image, &x, 
			-imagedisplay->mag, -imagedisplay->mag, NULL ) ) {
			g_object_unref( image );
			return( NULL ); 
		}
		g_object_unref( image );
		image = x;
	}
	else { 
		if( vips_zoom( image, &x, 
			imagedisplay->mag, imagedisplay->mag, NULL ) ) {
			g_object_unref( image );
			return( NULL ); 
		}
		g_object_unref( image );
		image = x;
	}

	/* This won't work for CMYK, you need to mess about with ICC profiles
	 * for that, but it will work for everything else.
	 */
	if( vips_colourspace( image, &x, VIPS_INTERPRETATION_sRGB, NULL ) ) {
		g_object_unref( image );
		return( NULL ); 
	}
	g_object_unref( image );
	image = x;

	/* Drop any alpha.
	 */
	if( vips_extract_band( image, &x, 0, "n", 3, NULL ) ) {
		g_object_unref( image );
		return( NULL ); 
	}
	g_object_unref( image );
	image = x;

	x = vips_image_new();
	if( vips_sink_screen( image, x, NULL, 128, 128, 400, 0, 
		imagedisplay_render_notify, imagedisplay ) ) {
		g_object_unref( image );
		g_object_unref( x );
		return( NULL );
	}
	g_object_unref( image );
	image = x;

	return( image );
}

static int
imagedisplay_update_conversion( Imagedisplay *imagedisplay )
{
	if( imagedisplay->image ) { 
		VIPS_UNREF( imagedisplay->display );
		VIPS_UNREF( imagedisplay->region );

		if( !(imagedisplay->display = 
			imagedisplay_display_image( imagedisplay, 
				imagedisplay->image )) ) 
			return( -1 ); 
		if( !(imagedisplay->region = 
			vips_region_new( imagedisplay->display )) )
			return( -1 ); 

		gtk_widget_set_size_request( GTK_WIDGET( imagedisplay ),
			imagedisplay->display->Xsize, 
			imagedisplay->display->Ysize );

		gtk_widget_queue_draw( GTK_WIDGET( imagedisplay ) ); 
	}

	return( 0 );
}

int
imagedisplay_set_file( Imagedisplay *imagedisplay, GFile *file )
{
	imagedisplay_empty( imagedisplay );

	if( file != NULL ) {
		gchar *path;
		gchar *contents;
		gsize length;

		if( (path = g_file_get_path( file )) ) {
			if( !(imagedisplay->image = 
				vips_image_new_from_file( path, NULL )) ) {
				g_free( path ); 
				return( -1 );
			}

			g_free( path ); 
		}
		else if( g_file_load_contents( file, NULL, 
			&contents, &length, NULL, NULL ) ) {
			if( !(imagedisplay->image =
				vips_image_new_from_buffer( contents, length, 
					"", NULL )) ) {
				g_free( contents );
				return( -1 ); 
			}

			g_signal_connect( imagedisplay->image, "close",
				G_CALLBACK( imagedisplay_close_memory ), 
				contents );
		}
		else {
			vips_error( "imagedisplay", 
				"unable to load GFile object" );
			return( -1 );
		}
	}

	imagedisplay_update_conversion( imagedisplay );

	return( 0 );
}

int
imagedisplay_get_mag( Imagedisplay *imagedisplay )
{
	return( imagedisplay->mag );
}

void
imagedisplay_set_mag( Imagedisplay *imagedisplay, int mag )
{
	if( imagedisplay->mag != mag ) { 
		imagedisplay->mag = mag;
		imagedisplay_update_conversion( imagedisplay );
	}
}

gboolean
imagedisplay_get_image_size( Imagedisplay *imagedisplay, 
	int *width, int *height )
{
	if( imagedisplay->image ) {
		*width = imagedisplay->image->Xsize;
		*height = imagedisplay->image->Ysize;

		return( TRUE ); 
	}
	else
		return( FALSE );
}

Imagedisplay *
imagedisplay_new( void ) 
{
	Imagedisplay *imagedisplay;

	printf( "imagedisplay_new:\n" ); 

	imagedisplay = g_object_new( imagedisplay_get_type(),
		NULL );

	return( imagedisplay ); 
}
