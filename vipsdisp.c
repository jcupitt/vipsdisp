/* Display an image with gtk3 and libvips. 
 */

#include <stdio.h>
#include <stdlib.h>

#include <gtk/gtk.h>

#include <vips/vips.h>

#include "disp.h"

G_DEFINE_TYPE( Disp, disp, GTK_TYPE_APPLICATION );

typedef struct _Update {
	GtkWidget *drawing_area;
	VipsRect rect;
} Update;

/* The main GUI thread runs this when it's idle and there are tiles that need
 * painting. 
 */
static gboolean
render_cb( Update *update )
{
  gtk_widget_queue_draw_area( update->drawing_area,
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
render_notify( VipsImage *image, VipsRect *rect, void *client )
{
	GtkWidget *drawing_area = GTK_WIDGET( client );
	Update *update = g_new( Update, 1 );

	update->rect = *rect;
	update->drawing_area = drawing_area;

	g_idle_add( (GSourceFunc) render_cb, update );
}

/* Make the image for display from the raw disc image. Could do
 * anything here, really. Uncomment sections to try different effects. Convert
 * to 8-bit RGB would be a good idea.
 */
static VipsImage *
disp_display_image( VipsImage *in, GtkWidget *drawing_area )
{
	VipsImage *image;
	VipsImage *x;

	/* Edit these to add or remove things from the pipeline we build. These
	 * should be wired up to something in a GUI.
	 */
	const gboolean zoom_in = FALSE;
	const gboolean zoom_out = TRUE;

	/* image represents the head of the pipeline. Hold a ref to it as we
	 * work.
	 */
	image = in;
	g_object_ref( image ); 

	if( zoom_out ) {
		if( vips_subsample( image, &x, 4, 4, NULL ) ) {
			g_object_unref( image );
			return( NULL ); 
		}
		g_object_unref( image );
		image = x;
	}

	if( zoom_in ) {
		if( vips_zoom( image, &x, 4, 4, NULL ) ) {
			g_object_unref( image );
			return( NULL ); 
		}
		g_object_unref( image );
		image = x;
	}

	/* This won't work for CMYK, you need to mess about with ICC profiles
	 * for that, but it will do everything else.
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
		render_notify, drawing_area ) ) {
		g_object_unref( image );
		g_object_unref( x );
		return( NULL );
	}
	g_object_unref( image );
	image = x;

	return( image );
}

static void
disp_draw_rect( GtkWidget *drawing_area, 
	cairo_t *cr, VipsRegion *region, VipsRect *expose )
{
	VipsRect image;
	VipsRect clip;
	unsigned char *cairo_buffer;
	int x, y;
	cairo_surface_t *surface;

	printf( "disp_draw_rect: "
		"left = %d, top = %d, width = %d, height = %d\n",
		expose->left, expose->top,
		expose->width, expose->height );

	/* Clip against the image size ... we don't want to try painting 
	 * outside the image area.
	 */
	image.left = 0;
	image.top = 0;
	image.width = region->im->Xsize;
	image.height = region->im->Ysize;
	vips_rect_intersectrect( &image, expose, &clip );
	if( vips_rect_isempty( &clip ) ||
		vips_region_prepare( region, &clip ) )
		return;

	/* libvips is RGB, cairo is ARGB, we have to repack the data.
	 */
	cairo_buffer = g_malloc( clip.width * clip.height * 4 );

	for( y = 0; y < clip.height; y++ ) {
		VipsPel *p = 
			VIPS_REGION_ADDR( region, clip.left, clip.top + y );
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

static void
disp_draw( GtkWidget *drawing_area, cairo_t *cr, VipsRegion *region )
{
	cairo_rectangle_list_t *rectangle_list = 
		cairo_copy_clip_rectangle_list( cr );

	printf( "disp_draw:\n" ); 

	if( rectangle_list->status == CAIRO_STATUS_SUCCESS ) { 
		int i;

		for( i = 0; i < rectangle_list->num_rectangles; i++ ) {
			VipsRect expose;

			expose.left = rectangle_list->rectangles[i].x;
			expose.top = rectangle_list->rectangles[i].y;
			expose.width = rectangle_list->rectangles[i].width;
			expose.height = rectangle_list->rectangles[i].height;

			disp_draw_rect( drawing_area, cr, region, &expose );
		}
	}

	cairo_rectangle_list_destroy( rectangle_list );
}




static void
disp_init( Disp *disp )
{
}

static void
disp_class_init( DispClass *class )
{
	GObjectClass *object_class = G_OBJECT_CLASS( class );
	GApplicationClass *application_class = G_APPLICATION_CLASS( class );

	object_class->finalize = bloat_pad_finalize;

	application_class->startup = disp_startup;
	application_class->shutdown = disp_shutdown;
	application_class->activate = disp_activate;
	application_class->open = disp_open;
}

Disp *
disp_new( void )
{
	Disp *disp;

	g_set_application_name( "vipsdisp" );

	disp = g_object_new( disp_get_type(),
		"application-id", "vips.vipsdisp",
		"flags", G_APPLICATION_HANDLES_OPEN,
		"inactivity-timeout", 30000,
		"register-session", TRUE,
		NULL );

	return( disp ); 
}

int
main( int argc, char **argv )
{
	Disp *disp;
	int status;
	const gchar *accels[] = { "F11", NULL };

	disp = disp_new();

	gtk_application_set_accels_for_action( GTK_APPLICATION( disp ),
		"win.fullscreen", accels );

	status = g_application_run( G_APPLICATION( disp ), argc, argv );

	g_object_unref( disp );

	return( status );
}
