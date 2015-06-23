/* Tiny display-an-image demo program. 
 *
 * This is not supposed to be a complete image viewer, it's just to 
 * show how to display a VIPS image (or the result of a VIPS computation) in a
 * window.
 *
 * Compile with:

	cc -g -Wall `pkg-config vips gtk+-3.0 --cflags --libs` \
		vipsdisp.c -o vipsdisp

 */

#include <stdio.h>

#include <gtk/gtk.h>
#include <vips/vips.h>

/* Just to demo progress feedback. This should be used to update a widget
 * somewhere.
 */
static void
vipsdisp_preeval( VipsImage *image, 
	VipsProgress *progress, const char *filename )
{
	printf( "load starting for %s ...\n", filename );
}

static void
vipsdisp_eval( VipsImage *image, 
	VipsProgress *progress, const char *filename )
{
	static int previous_precent = -1;

	if( progress->percent != previous_precent ) {
		printf( "%s %s: %d%% complete\r", 
			g_get_prgname(), filename, progress->percent );
		previous_precent = progress->percent;
	}
}

static void
vipsdisp_posteval( VipsImage *image, 
	VipsProgress *progress, const char *filename )
{
	printf( "\nload done in %g seconds\n", 
		g_timer_elapsed( progress->start, NULL ) );
}

static VipsImage *
vipsdisp_load( const char *filename )
{
	VipsImage *image;

	if( !(image = vips_image_new_from_file( filename, NULL ))) 
		return NULL;

	/* Attach an eval callback: this will tick down if we open this image
	 * via a temp file.
	 */
	vips_image_set_progress( image, TRUE ); 
	g_signal_connect( image, "preeval",
		G_CALLBACK( vipsdisp_preeval ), (void *) filename);
	g_signal_connect( image, "eval",
		G_CALLBACK( vipsdisp_eval ), (void *) filename);
	g_signal_connect( image, "posteval",
		G_CALLBACK( vipsdisp_posteval ), (void *) filename);

	return image;
}

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
vipsdisp_display_image( VipsImage *in, GtkWidget *drawing_area )
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
vipsdisp_draw_rect( GtkWidget *drawing_area, 
	cairo_t *cr, VipsRegion *region, VipsRect *expose )
{
	VipsRect image;
	VipsRect clip;
	unsigned char *cairo_buffer;
	int x, y;
	cairo_surface_t *surface;

	printf( "vipsdisp_draw_rect: "
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
vipsdisp_draw( GtkWidget *drawing_area, cairo_t *cr, VipsRegion *region )
{
	cairo_rectangle_list_t *rectangle_list = 
		cairo_copy_clip_rectangle_list( cr );

	printf( "vipsdisp_draw:\n" ); 

	if( rectangle_list->status == CAIRO_STATUS_SUCCESS ) { 
		int i;

		for( i = 0; i < rectangle_list->num_rectangles; i++ ) {
			VipsRect expose;

			expose.left = rectangle_list->rectangles[i].x;
			expose.top = rectangle_list->rectangles[i].y;
			expose.width = rectangle_list->rectangles[i].width;
			expose.height = rectangle_list->rectangles[i].height;

			vipsdisp_draw_rect( drawing_area, cr, region, &expose );
		}
	}

	cairo_rectangle_list_destroy( rectangle_list );
}

int
main( int argc, char **argv )
{
	VipsImage *image;
	VipsImage *display;
	VipsRegion *region;

	GtkWidget *window;
	GtkWidget *scrolled_window;
	GtkWidget *drawing_area;

	if( VIPS_INIT( argv[0] ) )
		vips_error_exit( "unable to start VIPS" );
	gtk_init( &argc, &argv );

	if( argc != 2 )
		vips_error_exit( "usage: %s <filename>", argv[0] );

	if( !(image = vipsdisp_load( argv[1] )) )
		vips_error_exit( "unable to load %s", argv[1] );

	window = gtk_window_new( GTK_WINDOW_TOPLEVEL );
	g_signal_connect( window, "destroy", 
		G_CALLBACK( gtk_main_quit ), NULL );

	scrolled_window = gtk_scrolled_window_new( NULL, NULL );
	gtk_container_add( GTK_CONTAINER( window ), scrolled_window );

	drawing_area = gtk_drawing_area_new();
	if( !(display = vipsdisp_display_image( image, drawing_area )) ||
		!(region = vips_region_new( display )) )
		vips_error_exit( "unable to build display image" );
	g_signal_connect( drawing_area, "draw", 
		G_CALLBACK( vipsdisp_draw ), region );
	gtk_widget_set_size_request( drawing_area, 
		display->Xsize, display->Ysize );
	gtk_container_add( GTK_CONTAINER( scrolled_window ), drawing_area );

	gtk_window_set_default_size( GTK_WINDOW( window ), 250, 250 );
	gtk_widget_show_all( window );

	gtk_main();

	g_object_unref( region ); 
	g_object_unref( display ); 
	g_object_unref( image ); 

	return( 0 );
}
