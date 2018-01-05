/* Display an image with gtk3 and libvips. 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#include <vips/vips.h>

#include "disp.h"

/* The size of the tiles that we use for the libvips cache.
 */
static const int tile_size = 128;

/* imagedisplay is actually a drawing area the size of the widget on screen: we 
 * do all scrolling ourselves.
 */
G_DEFINE_TYPE_WITH_CODE( Imagedisplay, imagedisplay, GTK_TYPE_DRAWING_AREA,
	G_IMPLEMENT_INTERFACE( GTK_TYPE_SCROLLABLE, NULL ) );

enum {
	/* Set the image with this.
	 */
	PROP_IMAGE,

	/* The props we implement for the scrollable interface.
	 */
	PROP_HADJUSTMENT,
	PROP_HSCROLL_POLICY,
	PROP_VADJUSTMENT,
	PROP_VSCROLL_POLICY,

	SIG_LAST
};

static guint imagedisplay_signals[SIG_LAST] = { 0 };

static void
imagedisplay_adjustment_changed( GtkAdjustment *adjustment, 
	Imagedisplay *imagedisplay )
{
	if( gtk_widget_get_realized( GTK_WIDGET( imagedisplay ) ) ) {
		double left = 
			gtk_adjustment_get_value( imagedisplay->hadjustment );
		double top = 
			gtk_adjustment_get_value( imagedisplay->vadjustment );

		printf( "imagedisplay_adjustment_changed: %g x %g\n", 
			left, top ); 

		imagedisplay->left = left;
		imagedisplay->top = top;

		gtk_widget_queue_draw( GTK_WIDGET( imagedisplay ) ); 
	}
}

static gboolean
imagedisplay_set_adjustment( Imagedisplay *imagedisplay,
	GtkAdjustment **adjustment_slot, GtkAdjustment *new_adjustment )
{
	printf( "imagedisplay_set_adjustment:\n" ); 

	if( new_adjustment && 
		*adjustment_slot == new_adjustment )
		return( FALSE );

	if( *adjustment_slot ) {
		g_signal_handlers_disconnect_by_func( *adjustment_slot,
			    imagedisplay_adjustment_changed, imagedisplay );
		VIPS_UNREF( *adjustment_slot );
	}

	if( !new_adjustment )
		new_adjustment = 
			gtk_adjustment_new( 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 );

	g_signal_connect( new_adjustment, "value-changed",
		G_CALLBACK( imagedisplay_adjustment_changed ), imagedisplay );
	*adjustment_slot = g_object_ref_sink( new_adjustment );

	return( TRUE );
}

static void
imagedisplay_set_adjustment_values( Imagedisplay *imagedisplay, 
	GtkAdjustment *adjustment, int axis_size, int window_size ) 
{
	double old_value;
	double new_value;
	double new_upper;
	double page_size;

	printf( "imagedisplay_set_adjustment_values: window_size = %d, "
		"axis_size = %d\n", axis_size, window_size );

	old_value = gtk_adjustment_get_value( adjustment );
	page_size = window_size;
	new_upper = VIPS_MAX( axis_size, page_size );

	g_object_set( adjustment,
		"lower", 0.0,
		"upper", new_upper,
		"page-size", page_size,
		"step-increment", page_size * 0.1,
		"page-increment", page_size * 0.9,
		NULL );

	new_value = VIPS_CLIP( 0, old_value, new_upper - page_size );
	if( new_value != old_value )
		gtk_adjustment_set_value( adjustment, new_value );
}

static void
imagedisplay_set_hadjustment_values( Imagedisplay *imagedisplay ) 
{
	imagedisplay_set_adjustment_values( imagedisplay, 
		imagedisplay->hadjustment, 
		imagedisplay->image_rect.width, 
		imagedisplay->paint_rect.width ); 
}

static void
imagedisplay_set_vadjustment_values( Imagedisplay *imagedisplay ) 
{
	imagedisplay_set_adjustment_values( imagedisplay, 
		imagedisplay->vadjustment, 
		imagedisplay->image_rect.height, 
		imagedisplay->paint_rect.height ); 
}

typedef struct _ImagedisplayUpdate {
	Imagedisplay *imagedisplay;
	VipsImage *image;
	VipsRect rect;
} ImagedisplayUpdate;

/* The main GUI thread runs this when it's idle and there are tiles that need
 * painting. 
 */
static gboolean
imagedisplay_render_cb( ImagedisplayUpdate *update )
{
	Imagedisplay *imagedisplay = update->imagedisplay;

	/*
	printf( "imagedisplay_render_cb: "
		"left = %d, top = %d, width = %d, height = %d\n",
		update->rect.left, update->rect.top,
		update->rect.width, update->rect.height );
	 */

	/* Again, stuff can run here long after the image has vanished, check
	 * before we update.
	 */
	if( update->image == imagedisplay->display ) {
		imagedisplay_to_buffer( imagedisplay, &update->rect );
		update->rect.left += imagedisplay->paint_rect.left;
		update->rect.top += imagedisplay->paint_rect.top;

		gtk_widget_queue_draw_area( GTK_WIDGET( update->imagedisplay ),
			update->rect.left, update->rect.top,
			update->rect.width, update->rect.height );
	}

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

	/* We can come here after imagedisplay has junked this image and
	 * started displaying another. Check the image is still correct.
	 */
	if( image == imagedisplay->display ) { 
		ImagedisplayUpdate *update = g_new( ImagedisplayUpdate, 1 );

		update->imagedisplay = imagedisplay;
		update->image = image;
		update->rect = *rect;

		g_idle_add( (GSourceFunc) imagedisplay_render_cb, update );
	}
}

static int
imagedisplay_set_image( Imagedisplay *imagedisplay, VipsImage *image )
{
	if( imagedisplay->image ) { 
		VIPS_UNREF( imagedisplay->mask_region );
		VIPS_UNREF( imagedisplay->mask );
		VIPS_UNREF( imagedisplay->display_region );
		VIPS_UNREF( imagedisplay->display );
		VIPS_UNREF( imagedisplay->image );
	}

	if( image ) { 
		imagedisplay->image = image;
		g_object_ref( image ); 
		imagedisplay->mask = vips_image_new();
		imagedisplay->display = vips_image_new();

		if( vips_sink_screen( imagedisplay->image, 
			imagedisplay->display, imagedisplay->mask, 
			tile_size, tile_size, 400, 0, 
			imagedisplay_render_notify, imagedisplay ) ) {
			return( -1 );
		}

		if( !(imagedisplay->display_region = 
			vips_region_new( imagedisplay->display )) )
			return( -1 ); 
		if( !(imagedisplay->mask_region = 
			vips_region_new( imagedisplay->mask )) )
			return( -1 ); 

		imagedisplay->image_rect.width = imagedisplay->display->Xsize;
		imagedisplay->image_rect.height = imagedisplay->display->Ysize;

		printf( "imagedisplay_update_conversion: image size %d x %d\n", 
			imagedisplay->image_rect.width, 
			imagedisplay->image_rect.height );
		printf( "** display image %p\n", imagedisplay->display );
		printf( "** new region %p\n", imagedisplay->display_region );

		gtk_widget_set_size_request( GTK_WIDGET( imagedisplay ),
			imagedisplay->image_rect.width, 
			imagedisplay->image_rect.height );
	}

	return( 0 );
}

static void
imagedisplay_set_property( GObject *object, 
	guint prop_id, const GValue *value, GParamSpec *pspec )
{
	Imagedisplay *imagedisplay = (Imagedisplay *) object;

	switch( prop_id ) {
	case PROP_HADJUSTMENT:
		if( imagedisplay_set_adjustment( imagedisplay, 
			&imagedisplay->hadjustment, 
			g_value_get_object( value ) ) ) { 
			imagedisplay_set_hadjustment_values( imagedisplay );
			g_object_notify( G_OBJECT( imagedisplay ), 
				"hadjustment" );
		}
		break;

	case PROP_VADJUSTMENT:
		if( imagedisplay_set_adjustment( imagedisplay, 
			&imagedisplay->vadjustment, 
			g_value_get_object( value ) ) ) { 
			imagedisplay_set_vadjustment_values( imagedisplay );
			g_object_notify( G_OBJECT( imagedisplay ), 
				"vadjustment" );
		}
		break;

	case PROP_HSCROLL_POLICY:
		if( imagedisplay->hscroll_policy != 
			g_value_get_enum( value ) ) {
			imagedisplay->hscroll_policy = 
				g_value_get_enum( value );
			gtk_widget_queue_resize( GTK_WIDGET( imagedisplay ) );
			g_object_notify_by_pspec( object, pspec );
		}
		break;

	case PROP_VSCROLL_POLICY:
		if( imagedisplay->vscroll_policy != 
			g_value_get_enum( value ) ) {
			imagedisplay->vscroll_policy = 
				g_value_get_enum( value );
			gtk_widget_queue_resize( GTK_WIDGET( imagedisplay ) );
			g_object_notify_by_pspec( object, pspec );
		}
		break;

	case PROP_IMAGE:
		imagedisplay_set_image( imagedisplay, g_value_get_object( value ) );
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID( object, prop_id, pspec );
		break;
	}
}

static void
imagedisplay_get_property( GObject *object, 
	guint prop_id, GValue *value, GParamSpec *pspec )
{
	Imagedisplay *imagedisplay = (Imagedisplay *) object;

	switch( prop_id ) {
	case PROP_HADJUSTMENT:
		g_value_set_object( value, imagedisplay->hadjustment );
		break;

	case PROP_VADJUSTMENT:
		g_value_set_object( value, imagedisplay->vadjustment );
		break;

	case PROP_HSCROLL_POLICY:
		g_value_set_enum( value, imagedisplay->hscroll_policy );
		break;

	case PROP_VSCROLL_POLICY:
		g_value_set_enum( value, imagedisplay->vscroll_policy );
		break;

	case PROP_IMAGE:
		g_value_set_object( value, imagedisplay->image );
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID( object, prop_id, pspec );
		break;
	}
}

static void
imagedisplay_empty( Imagedisplay *imagedisplay )
{
	VIPS_UNREF( imagedisplay->display_region );
	VIPS_UNREF( imagedisplay->display );
	VIPS_UNREF( imagedisplay->mask_region );
	VIPS_UNREF( imagedisplay->mask );
	VIPS_UNREF( imagedisplay->image );
	VIPS_FREE( imagedisplay->cairo_buffer ); 
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
imagedisplay_get_preferred_width( GtkWidget *widget, 
	gint *minimum, gint *natural )
{
	Imagedisplay *imagedisplay = (Imagedisplay *) widget;
	int width = imagedisplay->image_rect.width;

	printf( "imagedisplay_get_preferred_width: %d\n", width ); 

	*minimum = *natural = width;
}

static void
imagedisplay_get_preferred_height( GtkWidget *widget, 
	gint *minimum, gint *natural )
{
	Imagedisplay *imagedisplay = (Imagedisplay *) widget;
	int height = imagedisplay->image_rect.height;

	printf( "imagedisplay_get_preferred_height: %d\n", height ); 

	*minimum = *natural = height;
}

static void
imagedisplay_size_allocate( GtkWidget *widget, GtkAllocation *allocation )
{
	Imagedisplay *imagedisplay = (Imagedisplay *) widget;
	int old_buffer_width = imagedisplay->paint_rect.width;
	int old_buffer_height = imagedisplay->paint_rect.height;

	printf( "imagedisplay_size_allocate: %d x %d\n",
		allocation->width, allocation->height ); 

	GTK_WIDGET_CLASS( imagedisplay_parent_class )->
		size_allocate( widget, allocation );

	imagedisplay->widget_rect.width = allocation->width;
	imagedisplay->widget_rect.height = allocation->height;

	/* If the image is smaller than the widget, centre it.
	 */
	imagedisplay->paint_rect.width = VIPS_MIN( 
		imagedisplay->widget_rect.width, 
		imagedisplay->image_rect.width );
	imagedisplay->paint_rect.height = VIPS_MIN( 
		imagedisplay->widget_rect.height, 
		imagedisplay->image_rect.height );
	imagedisplay->paint_rect.left = VIPS_MAX( 0,
		(imagedisplay->widget_rect.width - 
			imagedisplay->paint_rect.width) / 2 ); 
	imagedisplay->paint_rect.top = VIPS_MAX( 0,
		(imagedisplay->widget_rect.height - 
			imagedisplay->paint_rect.height) / 2 ); 

	imagedisplay_set_hadjustment_values( imagedisplay );
	imagedisplay_set_vadjustment_values( imagedisplay );

	/* Reallocate the backing buffer, if necessary.
	 */
	if( !imagedisplay->cairo_buffer ||
		imagedisplay->paint_rect.width != old_buffer_width ||
		imagedisplay->paint_rect.height != old_buffer_height ) {
		unsigned char *new_buffer = g_malloc0( 
			imagedisplay->paint_rect.width * 
			imagedisplay->paint_rect.height * 4 );

		if( imagedisplay->cairo_buffer ) {
			/* Copy over any pixels from the old buffer.
			 */
			int common_height = VIPS_MIN( 
				old_buffer_height,
				imagedisplay->paint_rect.height );
			int common_size = 4 * VIPS_MIN( 
				old_buffer_width,
				imagedisplay->paint_rect.width );
			int old_stride = 4 * old_buffer_width;
			int new_stride = 4 * imagedisplay->paint_rect.width;

			int y;

			for( y = 0; y < common_height; y++ ) 
				memcpy( new_buffer + y * new_stride,
					imagedisplay->cairo_buffer + 
						y * old_stride, 
					common_size );
		}

		g_free( imagedisplay->cairo_buffer );
		imagedisplay->cairo_buffer = new_buffer;
	}
}

/* libvips is RGB, cairo is ABGR, so we have to repack the data.
 */
static void
imagedisplay_vips_to_cairo( Imagedisplay *imagedisplay, 
	unsigned char *cairo, VipsPel *vips, 
	int width, int height,
	int cairo_stride, int vips_stride )
{
	int x, y;

	for( y = 0; y < height; y++ ) {
		VipsPel * restrict p = vips + y * vips_stride;
		unsigned char * restrict q = cairo + y * cairo_stride;

		for( x = 0; x < width; x++ ) {
			q[0] = p[2];
			q[1] = p[1];
			q[2] = p[0];
			q[3] = 0;

			p += 3;
			q += 4;
		}
	}
}

/* Transform between image space and buffer space. In buffer space, (0, 0) is
 * the first pixel in ->cairo_buffer.
 */

static void
imagedisplay_to_buffer( Imagedisplay *imagedisplay, VipsRect *rect )
{
	/* This goes to buffer space ..
	 */
	rect->left -= imagedisplay->left;
	rect->top -= imagedisplay->top;

	/* .. but we want to clip against paint_rect, so we need to go on to
	 * paint space.
	 */ 
	rect->left += imagedisplay->paint_rect.left;
	rect->top += imagedisplay->paint_rect.top;

	vips_rect_intersectrect( rect, &imagedisplay->paint_rect, rect ); 

	/* And back to buffer space.
	 */
	rect->left -= imagedisplay->paint_rect.left;
	rect->top -= imagedisplay->paint_rect.top;
}

static void
imagedisplay_to_image( Imagedisplay *imagedisplay, VipsRect *rect )
{
	rect->left += imagedisplay->left;
	rect->top += imagedisplay->top;

	/* This should always be inside the image, but clip anyway.
	 */
	vips_rect_intersectrect( rect, &imagedisplay->image_rect, rect ); 
}

/* Fill a single tile from libvips. The tile fits within a single tile 
 * cache entry, and within the image.
 */
static void
imagedisplay_fill_tile( Imagedisplay *imagedisplay, VipsRect *tile )
{
	VipsRect target;
	VipsRect clip;
	int cairo_stride;
	unsigned char *cairo_start;

	/* Must fit in a single tile.
	 */
	g_assert( tile->width <= tile_size );
	g_assert( tile->height <= tile_size );
	g_assert( VIPS_ROUND_UP( tile->left, tile_size ) - 
		VIPS_ROUND_DOWN( VIPS_RECT_RIGHT( tile ), tile_size ) <= 
			tile_size );
	g_assert( VIPS_ROUND_UP( tile->top, tile_size ) - 
		VIPS_ROUND_DOWN( VIPS_RECT_BOTTOM( tile ), tile_size ) <= 
			tile_size );

	/* Map into screen space and clip.
	 */
	target = *tile;
	imagedisplay_to_buffer( imagedisplay, &target );

	/* Back to image space.
	 */
	clip = target;
	imagedisplay_to_image( imagedisplay, &clip ); 
	if( vips_rect_isempty( &clip ) )
		return;

	g_assert( imagedisplay->display_region->im == imagedisplay->display ); 
	g_assert( imagedisplay->mask_region->im == imagedisplay->mask ); 

	/* Request pixels. We ask the mask first, to get an idea of what's 
	 * currently in cache, then request tiles of pixels. We must always
	 * request pixels, even if the mask is blank, because the request
	 * will trigger a notify later which will reinvoke us.
	 */
	if( vips_region_prepare( imagedisplay->mask_region, &clip ) ) {
		printf( "vips_region_prepare: %s\n", vips_error_buffer() ); 
		vips_error_clear();
		abort();
		return;
	}

	if( vips_region_prepare( imagedisplay->display_region, &clip ) ) {
		printf( "vips_region_prepare: %s\n", vips_error_buffer() ); 
		vips_error_clear();
		abort();
		return;
	}

	/* tile is within a single tile, so we only need to test the first byte
	 * of the mask. 
	 */
	if( !VIPS_REGION_ADDR( imagedisplay->mask_region, 
		clip.left, clip.top )[0] ) 
		return;

	/*
	printf( "imagedisplay_fill_tile: painting %d x %d pixels\n", 
		clip.width, clip.height );
	 */

	cairo_stride = 4 * imagedisplay->paint_rect.width;
	cairo_start = imagedisplay->cairo_buffer +
		target.top * cairo_stride + target.left * 4;

	imagedisplay_vips_to_cairo( imagedisplay, 
		cairo_start,
		VIPS_REGION_ADDR( imagedisplay->display_region, 
			clip.left, clip.top ),
		clip.width, clip.height,
		cairo_stride,
		VIPS_REGION_LSKIP( imagedisplay->display_region ) );
}

/* Fill a rectangle with a set of libvips tiles.
 */
static void
imagedisplay_fill_rect( Imagedisplay *imagedisplay, VipsRect *expose )
{
	int left, top, right, bottom;
	int x, y;

	/*
	printf( "imagedisplay_fill_rect: "
		"left = %d, top = %d, width = %d, height = %d\n",
		expose->left, expose->top,
		expose->width, expose->height );
	 */

	left = VIPS_ROUND_DOWN( expose->left, tile_size );
	top = VIPS_ROUND_DOWN( expose->top, tile_size );
	right = VIPS_ROUND_UP( VIPS_RECT_RIGHT( expose ), tile_size );
	bottom = VIPS_ROUND_UP( VIPS_RECT_BOTTOM( expose ), tile_size );

	for( y = top; y < bottom; y += tile_size ) 
		for( x = left; x < right; x += tile_size ) {
			VipsRect tile;

			tile.left = x;
			tile.top = y;
			tile.width = tile_size;
			tile.height = tile_size;
			vips_rect_intersectrect( &tile, expose, &tile );

			if( !vips_rect_isempty( &tile ) )
				imagedisplay_fill_tile( imagedisplay, &tile );
		}
}

/* Draw a rectangle of the image from the backing buffer.
 */
static void
imagedisplay_draw_cairo( Imagedisplay *imagedisplay, 
	cairo_t *cr, VipsRect *expose )
{
	VipsRect target;
	int cairo_stride;
	unsigned char *cairo_start;
	cairo_surface_t *surface;

	target = *expose;
	imagedisplay_to_buffer( imagedisplay, &target );

	cairo_stride = 4 * imagedisplay->paint_rect.width;
	cairo_start = imagedisplay->cairo_buffer +
		target.top * cairo_stride + target.left * 4;

	surface = cairo_image_surface_create_for_data( cairo_start, 
		CAIRO_FORMAT_RGB24, target.width, target.height, 
		cairo_stride );  

	cairo_set_source_surface( cr, surface, 
		target.left + imagedisplay->paint_rect.left, 
		target.top + imagedisplay->paint_rect.top );

	cairo_paint( cr );

	cairo_surface_destroy( surface ); 
}

static gboolean
imagedisplay_draw( GtkWidget *widget, cairo_t *cr )
{
	Imagedisplay *imagedisplay = (Imagedisplay *) widget;

	if( imagedisplay->display_region ) {
		cairo_rectangle_list_t *rectangle_list = 
			cairo_copy_clip_rectangle_list( cr );

		if( rectangle_list->status == CAIRO_STATUS_SUCCESS ) { 
			int i;

			for( i = 0; i < rectangle_list->num_rectangles; i++ ) {
				cairo_rectangle_t *rectangle = 
					&rectangle_list->rectangles[i];
				VipsRect expose;

				/* Turn into an image-space rect.
				 */
				expose.left = rectangle->x - 
					imagedisplay->paint_rect.left;
				expose.top = rectangle->y - 
					imagedisplay->paint_rect.top;
				expose.width = rectangle->width;
				expose.height = rectangle->height;
				imagedisplay_to_image( imagedisplay, &expose ); 

				if( !vips_rect_isempty( &expose ) ) {
					imagedisplay_fill_rect( imagedisplay, 
						&expose );
					imagedisplay_draw_cairo( imagedisplay, 
						cr, &expose );
				}
			}
		}

		cairo_rectangle_list_destroy( rectangle_list );
	}

	return( FALSE ); 
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
	GObjectClass *gobject_class = G_OBJECT_CLASS( class );
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS( class );

	printf( "imagedisplay_class_init:\n" ); 

	gobject_class->set_property = imagedisplay_set_property;
	gobject_class->get_property = imagedisplay_get_property;

	widget_class->destroy = imagedisplay_destroy;
	widget_class->get_preferred_width = imagedisplay_get_preferred_width;
	widget_class->get_preferred_height = imagedisplay_get_preferred_height;
	widget_class->size_allocate = imagedisplay_size_allocate;
	widget_class->draw = imagedisplay_draw;

	g_object_class_override_property( gobject_class, 
		PROP_HADJUSTMENT, "hadjustment" );
	g_object_class_override_property( gobject_class, 
		PROP_VADJUSTMENT, "vadjustment" );
	g_object_class_override_property( gobject_class, 
		PROP_HSCROLL_POLICY, "hscroll-policy" );
	g_object_class_override_property( gobject_class, 
		PROP_VSCROLL_POLICY, "vscroll-policy" );

}

we will need image cods <-> cairo cods functions for the statusbar etc.

conversion will need display image cods <-> real image

Imagedisplay *
imagedisplay_new( void ) 
{
	Imagedisplay *imagedisplay;

	printf( "imagedisplay_new:\n" ); 

	imagedisplay = g_object_new( imagedisplay_get_type(),
		NULL );

	return( imagedisplay ); 
}
