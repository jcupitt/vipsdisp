/* Convert an image for display.
 */

#include <stdio.h>
#include <stdlib.h>

#include <gtk/gtk.h>

#include <vips/vips.h>

#include "disp.h"

G_DEFINE_TYPE( Conversion, conversion, G_OBJECT );

/* Our signals. 
 */
enum {
	SIG_CHANGED,
	SIG_AREA_CHANGED,
	SIG_IMAGE_CHANGED,
	SIG_LAST
};

static guint conversion_signals[SIG_LAST] = { 0 };

static void
conversion_empty( Conversion *conversion )
{
	if( conversion->image ) { 
		if( conversion->preeval_sig ) { 
			g_signal_handler_disconnect( conversion->image, 
				conversion->preeval_sig ); 
			conversion->preeval_sig = 0;
		}

		if( conversion->eval_sig ) { 
			g_signal_handler_disconnect( conversion->image, 
				conversion->eval_sig ); 
			conversion->eval_sig = 0;
		}

		if( conversion->posteval_sig ) { 
			g_signal_handler_disconnect( conversion->image, 
				conversion->posteval_sig ); 
			conversion->posteval_sig = 0;
		}
	}

	VIPS_UNREF( conversion->srgb_region );
	VIPS_UNREF( conversion->srgb );
	VIPS_UNREF( conversion->mask_region );
	VIPS_UNREF( conversion->mask );
	VIPS_UNREF( conversion->display_region );
	VIPS_UNREF( conversion->display );
	VIPS_UNREF( conversion->image_region );

	VIPS_UNREF( conversion->image );
}

static void
conversion_destroy( GtkWidget *widget )
{
	Conversion *conversion = (Conversion *) widget;

	printf( "conversion_destroy:\n" ); 

	conversion_empty( conversion );

	GTK_WIDGET_CLASS( conversion_parent_class )->destroy( widget );
}

static void
conversion_draw_rect( Conversion *conversion, 
	cairo_t *cr, VipsRect *expose )
{
	VipsRect image;
	VipsRect clip;
	gboolean found;
	VipsPel * restrict buf;
	int lsk;
	int x, y;
	unsigned char *cairo_buffer;
	cairo_surface_t *surface;

	/*
	printf( "conversion_draw_rect: "
		"left = %d, top = %d, width = %d, height = %d\n",
		expose->left, expose->top,
		expose->width, expose->height );
	 */

	/* Clip against the image size ... we don't want to try painting 
	 * outside the image area.
	 */
	image.left = 0;
	image.top = 0;
	image.width = conversion->srgb_region->im->Xsize;
	image.height = conversion->srgb_region->im->Ysize;
	vips_rect_intersectrect( &image, expose, &clip );
	if( vips_rect_isempty( &clip ) )
		return;

	g_assert( conversion->srgb_region->im == conversion->srgb ); 
	g_assert( conversion->mask_region->im == conversion->mask ); 

	/* Request pixels. We ask the mask first, to get an idea of what's 
	 * currently in cache, then request tiles of pixels. We must always
	 * request pixels, even if the mask is blank, because the request
	 * will trigger a notify later which will reinvoke us.
	 */
	if( vips_region_prepare( conversion->mask_region, &clip ) ) {
		printf( "vips_region_prepare: %s\n", vips_error_buffer() ); 
		vips_error_clear();
		abort();
		return;
	}

	if( vips_region_prepare( conversion->srgb_region, &clip ) ) {
		printf( "vips_region_prepare: %s\n", vips_error_buffer() ); 
		vips_error_clear();
		abort();
		return;
	}

	/* If the mask is all blank, skip the paint.
	 */
	buf = VIPS_REGION_ADDR( conversion->mask_region, 
		clip.left, clip.top );
	lsk = VIPS_REGION_LSKIP( conversion->mask_region );
	found = FALSE;

	for( y = 0; y < clip.height; y++ ) {
		for( x = 0; x < clip.width; x++ )
			if( buf[x] ) {
				found = TRUE;
				break;
			}

		if( found )
			break;

		buf += lsk;
	}

	if( !found ) {
		printf( "conversion_paint_image: zero mask\n" );
		return;
	}

	/* libvips is RGB, cairo is ARGB, we have to repack the data.
	 */
	cairo_buffer = g_malloc( clip.width * clip.height * 4 );

	for( y = 0; y < clip.height; y++ ) {
		VipsPel * restrict p = 
			VIPS_REGION_ADDR( conversion->srgb_region, 
			clip.left, clip.top + y );
		unsigned char * restrict q = cairo_buffer + clip.width * 4 * y;

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
conversion_draw( GtkWidget *widget, cairo_t *cr )
{
	Conversion *conversion = (Conversion *) widget;

	//printf( "conversion_draw:\n" ); 

	if( conversion->srgb_region ) {
		cairo_rectangle_list_t *rectangle_list = 
			cairo_copy_clip_rectangle_list( cr );

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

				conversion_draw_rect( conversion, 
					cr, &expose );
			}
		}

		cairo_rectangle_list_destroy( rectangle_list );
	}

	return( FALSE ); 
}

static void
conversion_init( Conversion *conversion )
{
	printf( "conversion_init:\n" ); 

	conversion->mag = 1;
}

static void
conversion_class_init( ConversionClass *class )
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS( class );

	printf( "conversion_class_init:\n" ); 

	widget_class->destroy = conversion_destroy;
	widget_class->draw = conversion_draw;

	conversion_signals[SIG_PRELOAD] = g_signal_new( "preload",
		G_TYPE_FROM_CLASS( class ),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET( ConversionClass, preload ), 
		NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER );

	conversion_signals[SIG_LOAD] = g_signal_new( "load",
		G_TYPE_FROM_CLASS( class ),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET( ConversionClass, load ), 
		NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER );

	conversion_signals[SIG_POSTLOAD] = g_signal_new( "postload",
		G_TYPE_FROM_CLASS( class ),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET( ConversionClass, postload ), 
		NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER );

}

static void
conversion_close_memory( VipsImage *image, gpointer contents )
{
	g_free( contents );
}

typedef struct _ConversionUpdate {
	Conversion *conversion;
	VipsImage *image;
	VipsRect rect;
} ConversionUpdate;

/* The main GUI thread runs this when it's idle and there are tiles that need
 * painting. 
 */
static gboolean
conversion_render_cb( ConversionUpdate *update )
{
	Conversion *conversion = update->conversion;

	/* Again, stuff can run here long after the image has vanished, check
	 * before we update.
	 */
	if( update->image == conversion->srgb )  
		gtk_widget_queue_draw_area( GTK_WIDGET( update->conversion ),
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
conversion_render_notify( VipsImage *image, VipsRect *rect, void *client )
{
	Conversion *conversion = (Conversion *) client;

	/* We can come here after conversion has junked this image and
	 * started displaying another. Check the image is still correct.
	 */
	if( image == conversion->srgb ) { 
		ConversionUpdate *update = g_new( ConversionUpdate, 1 );

		update->conversion = conversion;
		update->image = image;
		update->rect = *rect;

		g_idle_add( (GSourceFunc) conversion_render_cb, update );
	}
}

/* Make the screen image. This is the thing we display pixel values from in
 * the status bar.
 */
static VipsImage *
conversion_display_image( Conversion *conversion, VipsImage *in )
{
	VipsImage *image;
	VipsImage *x;

	/* image redisplays the head of the pipeline. Hold a ref to it as we
	 * work.
	 */
	image = in;
	g_object_ref( image ); 

	if( conversion->mag < 0 ) {
		if( vips_subsample( image, &x, 
			-conversion->mag, -conversion->mag, NULL ) ) {
			g_object_unref( image );
			return( NULL ); 
		}
		g_object_unref( image );
		image = x;
	}
	else { 
		if( vips_zoom( image, &x, 
			conversion->mag, conversion->mag, NULL ) ) {
			g_object_unref( image );
			return( NULL ); 
		}
		g_object_unref( image );
		image = x;
	}

	return( image );
}

/* Make the srgb image we paint with. 
 */
static VipsImage *
conversion_srgb_image( Conversion *conversion, VipsImage *in, 
	VipsImage **mask )
{
	VipsImage *image;
	VipsImage *x;

	/* image redisplays the head of the pipeline. Hold a ref to it as we
	 * work.
	 */
	image = in;
	g_object_ref( image ); 

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
	if( mask )
		*mask = vips_image_new();
	if( vips_sink_screen( image, x, *mask, 128, 128, 400, 0, 
		conversion_render_notify, conversion ) ) {
		g_object_unref( image );
		g_object_unref( x );
		return( NULL );
	}
	g_object_unref( image );
	image = x;

	return( image );
}

static int
conversion_update_conversion( Conversion *conversion )
{
	if( conversion->image ) { 
		if( conversion->srgb_region )
			printf( "** junking region %p\n", 
				conversion->srgb_region );

		VIPS_UNREF( conversion->mask_region );
		VIPS_UNREF( conversion->mask );
		VIPS_UNREF( conversion->srgb_region );
		VIPS_UNREF( conversion->srgb );
		VIPS_UNREF( conversion->display_region );
		VIPS_UNREF( conversion->display );
		VIPS_UNREF( conversion->image_region );

		if( !(conversion->image_region = 
			vips_region_new( conversion->image )) )
			return( -1 ); 

		if( !(conversion->display = 
			conversion_display_image( conversion, 
				conversion->image )) ) 
			return( -1 ); 
		if( !(conversion->display_region = 
			vips_region_new( conversion->display )) )
			return( -1 ); 

		if( !(conversion->srgb = 
			conversion_srgb_image( conversion, 
				conversion->display, &conversion->mask )) ) 
			return( -1 ); 
		if( !(conversion->srgb_region = 
			vips_region_new( conversion->srgb )) )
			return( -1 ); 
		if( !(conversion->mask_region = 
			vips_region_new( conversion->mask )) )
			return( -1 ); 

		printf( "conversion_update_conversion: image size %d x %d\n", 
			conversion->display->Xsize, 
			conversion->display->Ysize );
		printf( "** srgb image %p\n", conversion->srgb );
		printf( "** new region %p\n", conversion->srgb_region );

		gtk_widget_set_size_request( GTK_WIDGET( conversion ),
			conversion->display->Xsize, 
			conversion->display->Ysize );

		gtk_widget_queue_draw( GTK_WIDGET( conversion ) ); 
	}

	return( 0 );
}

static void
conversion_preeval( VipsImage *image, 
	VipsProgress *progress, Conversion *conversion )
{
	g_signal_emit( conversion, 
		conversion_signals[SIG_PRELOAD], 0, progress );
}

static void
conversion_eval( VipsImage *image, 
	VipsProgress *progress, Conversion *conversion )
{
	g_signal_emit( conversion, 
		conversion_signals[SIG_LOAD], 0, progress );
}

static void
conversion_posteval( VipsImage *image, 
	VipsProgress *progress, Conversion *conversion )
{
	g_signal_emit( conversion, 
		conversion_signals[SIG_POSTLOAD], 0, progress );
}

static void
conversion_attach_progress( Conversion *conversion )
{
	g_assert( !conversion->preeval_sig );
	g_assert( !conversion->eval_sig );
	g_assert( !conversion->posteval_sig );

	/* Attach an eval callback: this will tick down if we 
	 * have to decode this image.
	 */
	vips_image_set_progress( conversion->image, TRUE ); 
	conversion->preeval_sig = 
		g_signal_connect( conversion->image, "preeval",
			G_CALLBACK( conversion_preeval ), 
			conversion );
	conversion->eval_sig = 
		g_signal_connect( conversion->image, "eval",
			G_CALLBACK( conversion_eval ), 
			conversion );
	conversion->posteval_sig = 
		g_signal_connect( conversion->image, "posteval",
			G_CALLBACK( conversion_posteval ), 
			conversion );
}

int
conversion_set_file( Conversion *conversion, GFile *file )
{
	conversion_empty( conversion );

	if( file != NULL ) {
		gchar *path;
		gchar *contents;
		gsize length;

		if( (path = g_file_get_path( file )) ) {
			if( !(conversion->image = 
				vips_image_new_from_file( path, NULL )) ) {
				g_free( path ); 
				return( -1 );
			}
			g_free( path ); 
		}
		else if( g_file_load_contents( file, NULL, 
			&contents, &length, NULL, NULL ) ) {
			if( !(conversion->image =
				vips_image_new_from_buffer( contents, length, 
					"", NULL )) ) {
				g_free( contents );
				return( -1 ); 
			}

			g_signal_connect( conversion->image, "close",
				G_CALLBACK( conversion_close_memory ), 
				contents );
		}
		else {
			vips_error( "conversion", 
				"unable to load GFile object" );
			return( -1 );
		}

		conversion_attach_progress( conversion ); 
	}

	conversion_update_conversion( conversion );

	return( 0 );
}

int
conversion_get_mag( Conversion *conversion )
{
	return( conversion->mag );
}

void
conversion_set_mag( Conversion *conversion, int mag )
{
	if( mag > -600 &&
		mag < 1000000 &&
		conversion->mag != mag ) { 
		printf( "conversion_set_mag: %d\n", mag ); 

		conversion->mag = mag;
		conversion_update_conversion( conversion );
	}
}

gboolean
conversion_get_image_size( Conversion *conversion, 
	int *width, int *height )
{
	if( conversion->image ) {
		*width = conversion->image->Xsize;
		*height = conversion->image->Ysize;

		return( TRUE ); 
	}
	else
		return( FALSE );
}

gboolean
conversion_get_display_image_size( Conversion *conversion, 
	int *width, int *height )
{
	if( conversion->display ) {
		*width = conversion->display->Xsize;
		*height = conversion->display->Ysize;

		return( TRUE ); 
	}
	else
		return( FALSE );
}

/* Map to underlying image coordinates from display image coordinates.
 */
void
conversion_to_image_cods( Conversion *conversion,
	int display_x, int display_y, int *image_x, int *image_y )
{
	if( conversion->mag > 0 ) {
		*image_x = display_x / conversion->mag;
		*image_y = display_y / conversion->mag;
	}
	else {
		*image_x = display_x * -conversion->mag;
		*image_y = display_y * -conversion->mag;
	}
}

/* Map to display cods from underlying image coordinates.
 */
void
conversion_to_display_cods( Conversion *conversion,
	int image_x, int image_y, int *display_x, int *display_y )
{
	if( conversion->mag > 0 ) {
		*display_x = image_x * conversion->mag;
		*display_y = image_y * conversion->mag;
	}
	else {
		*display_x = image_x / -conversion->mag;
		*display_y = image_y / -conversion->mag;
	}
}

VipsPel *
conversion_get_ink( Conversion *conversion, int x, int y )
{
	VipsRect rect;

	rect.left = x;
	rect.top = y;
	rect.width = 1;
	rect.height = 1;
	if( vips_region_prepare( conversion->image_region, &rect ) )
		return( NULL );

	return( VIPS_REGION_ADDR( conversion->image_region, x, y ) );  
}

Conversion *
conversion_new( void ) 
{
	Conversion *conversion;

	printf( "conversion_new:\n" ); 

	conversion = g_object_new( conversion_get_type(),
		NULL );

	return( conversion ); 
}
