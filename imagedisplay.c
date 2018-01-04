/* Display an image with gtk3 and libvips. 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#include <vips/vips.h>

#include "disp.h"

/* imagedisplay is actually a drawing area the size of the widget on screen: we 
 * do all scrolling ourselves.
 */
G_DEFINE_TYPE_WITH_CODE( Imagedisplay, imagedisplay, GTK_TYPE_DRAWING_AREA,
	G_IMPLEMENT_INTERFACE( GTK_TYPE_SCROLLABLE, NULL ) );

enum {
	/* Our signals. 
	 */
	SIG_PRELOAD,
	SIG_LOAD,
	SIG_POSTLOAD,

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

	old_value = gtk_adjustment_get_value( adjustment );
	page_size = window_size;
	new_upper = VIPS_MAX( axis_size, page_size );

	printf( "imagedisplay_set_adjustment_values: window_size = %d, "
		"axis_size = %d, new_upper = %g\n", 
		axis_size, window_size, new_upper );

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
	int width = imagedisplay->display ? 
		imagedisplay->display->Xsize : 0;

	GtkAllocation allocation;

	gtk_widget_get_allocation( GTK_WIDGET( imagedisplay ), &allocation );
	imagedisplay_set_adjustment_values( imagedisplay, 
		imagedisplay->hadjustment, width, allocation.width ); 
}

static void
imagedisplay_set_vadjustment_values( Imagedisplay *imagedisplay ) 
{
	int height = imagedisplay->display ? 
		imagedisplay->display->Ysize : 0;

	GtkAllocation allocation;

	gtk_widget_get_allocation( GTK_WIDGET( imagedisplay ), &allocation );
	imagedisplay_set_adjustment_values( imagedisplay, 
		imagedisplay->vadjustment, height, allocation.height ); 
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

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID( object, prop_id, pspec );
		break;
	}
}

static void
imagedisplay_empty( Imagedisplay *imagedisplay )
{
	if( imagedisplay->image ) { 
		if( imagedisplay->preeval_sig ) { 
			g_signal_handler_disconnect( imagedisplay->image, 
				imagedisplay->preeval_sig ); 
			imagedisplay->preeval_sig = 0;
		}

		if( imagedisplay->eval_sig ) { 
			g_signal_handler_disconnect( imagedisplay->image, 
				imagedisplay->eval_sig ); 
			imagedisplay->eval_sig = 0;
		}

		if( imagedisplay->posteval_sig ) { 
			g_signal_handler_disconnect( imagedisplay->image, 
				imagedisplay->posteval_sig ); 
			imagedisplay->posteval_sig = 0;
		}
	}


	VIPS_UNREF( imagedisplay->srgb_region );
	VIPS_UNREF( imagedisplay->srgb );
	VIPS_UNREF( imagedisplay->mask_region );
	VIPS_UNREF( imagedisplay->mask );
	VIPS_UNREF( imagedisplay->display_region );
	VIPS_UNREF( imagedisplay->display );
	VIPS_UNREF( imagedisplay->image_region );

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
	int width = imagedisplay->display ? imagedisplay->display->Xsize : 0;

	printf( "imagedisplay_get_preferred_width: %d\n", width ); 

	*minimum = *natural = width;
}

static void
imagedisplay_get_preferred_height( GtkWidget *widget, 
	gint *minimum, gint *natural )
{
	Imagedisplay *imagedisplay = (Imagedisplay *) widget;
	int height = imagedisplay->display ? imagedisplay->display->Ysize : 0;

	printf( "imagedisplay_get_preferred_height: %d\n", height ); 

	*minimum = *natural = height;
}

static void
imagedisplay_size_allocate( GtkWidget *widget, GtkAllocation *allocation )
{
	Imagedisplay *imagedisplay = (Imagedisplay *) widget;
	int new_buffer_width = VIPS_MIN( 
		imagedisplay->display ? imagedisplay->display->Xsize : 0,
		allocation->width );
	int new_buffer_height = VIPS_MIN( 
		imagedisplay->display ? imagedisplay->display->Ysize : 0,
		allocation->height );

	/* Chain up first so we set the widget allocation.
	 */
	GTK_WIDGET_CLASS( imagedisplay_parent_class )->
		size_allocate( widget, allocation );

	printf( "imagedisplay_size_allocate: %d x %d\n",
		allocation->width, allocation->height ); 

	imagedisplay_set_hadjustment_values( imagedisplay );
	imagedisplay_set_vadjustment_values( imagedisplay );

	/* Reallocate the backing buffer, if necessary.
	 */
	if( !imagedisplay->cairo_buffer ||
		new_buffer_width != imagedisplay->cairo_buffer_width ||
		new_buffer_height != imagedisplay->cairo_buffer_height ) {
		size_t size = new_buffer_width * new_buffer_height * 4;
		unsigned char *new_buffer = g_malloc0( size );

		if( imagedisplay->cairo_buffer ) {
			/* Copy over any pixels from the old buffer.
			 */
			int common_height = VIPS_MIN( 
				imagedisplay->cairo_buffer_height,
				new_buffer_height );
			int old_stride = 4 * imagedisplay->cairo_buffer_width;
			int new_stride = 4 * new_buffer_width;

			int y;

			for( y = 0; y < common_height; y++ ) 
				memcpy( new_buffer + y * new_stride,
					imagedisplay->cairo_buffer + 
						y * old_stride, 
					sizeof( unsigned char ) );
		}

		g_free( imagedisplay->cairo_buffer );
		imagedisplay->cairo_buffer = new_buffer;
		imagedisplay->cairo_buffer_width = new_buffer_width;
		imagedisplay->cairo_buffer_height = new_buffer_height;
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

/* Transform between image space and screen space.
 */

static void
imagedisplay_to_screen( Imagedisplay *imagedisplay, VipsRect *rect )
{
	int image_width = imagedisplay->display ? 
		imagedisplay->display->Xsize : 0;
	int image_height = imagedisplay->display ? 
		imagedisplay->display->Ysize : 0;

	GtkAllocation allocation;

	rect->left -= imagedisplay->left;
	rect->top -= imagedisplay->top;

	/* If the image is smaller than the screen, we centre.
	 */
	gtk_widget_get_allocation( GTK_WIDGET( imagedisplay ), &allocation );
	rect->left += VIPS_MIN( allocation.width - image_width / 2, 0 );
	rect->top += VIPS_MIN( allocation.height - image_height / 2, 0 );
}

static void
imagedisplay_to_image( Imagedisplay *imagedisplay, VipsRect *rect )
{
	int image_width = imagedisplay->display ? 
		imagedisplay->display->Xsize : 0;
	int image_height = imagedisplay->display ? 
		imagedisplay->display->Ysize : 0;

	GtkAllocation allocation;

	/* If the image is smaller than the screen, we centre.
	 */
	gtk_widget_get_allocation( GTK_WIDGET( imagedisplay ), &allocation );
	rect->left -= VIPS_MIN( allocation.width - image_width / 2, 0 );
	rect->top -= VIPS_MIN( allocation.height - image_height / 2, 0 );

	rect->left += imagedisplay->left;
	rect->top += imagedisplay->top;
}

/* Fill a single tile from libvips. The tile fits within a single tile 
 * cache entry, and within the image.
 */
static void
imagedisplay_fill_tile( Imagedisplay *imagedisplay, VipsRect *tile )
{
	VipsRect screen;
	VipsRect target;
	VipsRect clip;
	int cairo_stride;
	unsigned char *cairo_start;

	/* Must fit in a single tile.
	 */
	g_assert( tile->width <= 128 );
	g_assert( tile->height <= 128 );
	g_assert( VIPS_ROUND_UP( tile->left, 128 ) - 
		VIPS_ROUND_DOWN( VIPS_RECT_RIGHT( tile ), 128 ) <= 128 );
	g_assert( VIPS_ROUND_UP( tile->top, 128 ) - 
		VIPS_ROUND_DOWN( VIPS_RECT_BOTTOM( tile ), 128 ) <= 128 );

	/* Map into screen space.
	 */
	target = *tile;
	imagedisplay_to_screen( imagedisplay, &target );

	/* Clip against window size.
	 */
	screen.left = 0;
	screen.top = 0;
	screen.width = imagedisplay->cairo_buffer_width;
	screen.height = imagedisplay->cairo_buffer_height;
	vips_rect_intersectrect( &target, &screen, &target );

	/* Back to image space.
	 */
	clip = target;
	imagedisplay_to_image( imagedisplay, &clip ); 
	if( vips_rect_isempty( &clip ) )
		return;

	g_assert( imagedisplay->srgb_region->im == imagedisplay->srgb ); 
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

	if( vips_region_prepare( imagedisplay->srgb_region, &clip ) ) {
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
	printf( "imagedisplay_paint_image: painting %d x %d pixels\n", 
		clip.width, clip.height );
	 */

	cairo_stride = 4 * imagedisplay->cairo_buffer_width;
	cairo_start = imagedisplay->cairo_buffer +
		target.top * cairo_stride + target.left * 4;

	imagedisplay_vips_to_cairo( imagedisplay, 
		cairo_start,
		VIPS_REGION_ADDR( imagedisplay->srgb_region, 
			clip.left, clip.top ),
		clip.width, clip.height,
		cairo_stride,
		VIPS_REGION_LSKIP( imagedisplay->srgb_region ) );
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

	left = VIPS_ROUND_DOWN( expose->left, 128 );
	top = VIPS_ROUND_DOWN( expose->top, 128 );
	right = VIPS_ROUND_UP( VIPS_RECT_RIGHT( expose ), 128 );
	bottom = VIPS_ROUND_UP( VIPS_RECT_BOTTOM( expose ), 128 );

	for( y = top; y < bottom; y += 128 ) 
		for( x = left; x < right; x += 128 ) {
			VipsRect tile;

			tile.left = x;
			tile.top = y;
			tile.width = 128;
			tile.height = 128;
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
	VipsRect screen;
	int cairo_stride;
	unsigned char *cairo_start;
	cairo_surface_t *surface;

	/* Map into screen space.
	 */
	target = *expose;
	imagedisplay_to_screen( imagedisplay, &target );

	/* Clip against the buffer ... this shouldn't be necessary, but do it
	 * anyway for safety.
	 */
	screen.left = 0;
	screen.top = 0;
	screen.width = imagedisplay->cairo_buffer_width;
	screen.height = imagedisplay->cairo_buffer_height;
	vips_rect_intersectrect( &target, &screen, &target ); 

	cairo_stride = 4 * imagedisplay->cairo_buffer_width;
	cairo_start = imagedisplay->cairo_buffer +
		target.top * cairo_stride + target.left * 4;

	surface = cairo_image_surface_create_for_data( cairo_start, 
		CAIRO_FORMAT_RGB24, target.width, target.height, 
		cairo_stride );  

	cairo_set_source_surface( cr, surface, target.left, target.top );

	cairo_paint( cr );

	cairo_surface_destroy( surface ); 
}

static gboolean
imagedisplay_draw( GtkWidget *widget, cairo_t *cr )
{
	Imagedisplay *imagedisplay = (Imagedisplay *) widget;

	VipsRect image;

	image.left = 0;
	image.top = 0;
	image.width = imagedisplay->display->Xsize;
	image.height = imagedisplay->display->Ysize;

	if( imagedisplay->srgb_region ) {
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
				expose.left = rectangle->x;
				expose.top = rectangle->y;
				expose.width = rectangle->width;
				expose.height = rectangle->height;
				imagedisplay_to_image( imagedisplay, &expose ); 
				vips_rect_intersectrect( &expose, 
					&image, &expose );

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

	imagedisplay_signals[SIG_PRELOAD] = g_signal_new( "preload",
		G_TYPE_FROM_CLASS( class ),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET( ImagedisplayClass, preload ), 
		NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER );

	imagedisplay_signals[SIG_LOAD] = g_signal_new( "load",
		G_TYPE_FROM_CLASS( class ),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET( ImagedisplayClass, load ), 
		NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER );

	imagedisplay_signals[SIG_POSTLOAD] = g_signal_new( "postload",
		G_TYPE_FROM_CLASS( class ),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET( ImagedisplayClass, postload ), 
		NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER );

	g_object_class_override_property( gobject_class, 
		PROP_HADJUSTMENT, "hadjustment" );
	g_object_class_override_property( gobject_class, 
		PROP_VADJUSTMENT, "vadjustment" );
	g_object_class_override_property( gobject_class, 
		PROP_HSCROLL_POLICY, "hscroll-policy" );
	g_object_class_override_property( gobject_class, 
		PROP_VSCROLL_POLICY, "vscroll-policy" );

}

static void
imagedisplay_close_memory( VipsImage *image, gpointer contents )
{
	g_free( contents );
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

	/* Again, stuff can run here long after the image has vanished, check
	 * before we update.
	 */
	if( update->image == imagedisplay->srgb ) {
		imagedisplay_to_screen( imagedisplay, &update->rect );

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
	if( image == imagedisplay->srgb ) { 
		ImagedisplayUpdate *update = g_new( ImagedisplayUpdate, 1 );

		update->imagedisplay = imagedisplay;
		update->image = image;
		update->rect = *rect;

		g_idle_add( (GSourceFunc) imagedisplay_render_cb, update );
	}
}

/* Make the screen image. This is the thing we display pixel values from in
 * the status bar.
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

	return( image );
}

/* Make the srgb image we paint with. 
 */
static VipsImage *
imagedisplay_srgb_image( Imagedisplay *imagedisplay, VipsImage *in, 
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
		if( imagedisplay->srgb_region )
			printf( "** junking region %p\n", 
				imagedisplay->srgb_region );

		VIPS_UNREF( imagedisplay->mask_region );
		VIPS_UNREF( imagedisplay->mask );
		VIPS_UNREF( imagedisplay->srgb_region );
		VIPS_UNREF( imagedisplay->srgb );
		VIPS_UNREF( imagedisplay->display_region );
		VIPS_UNREF( imagedisplay->display );
		VIPS_UNREF( imagedisplay->image_region );

		if( !(imagedisplay->image_region = 
			vips_region_new( imagedisplay->image )) )
			return( -1 ); 

		if( !(imagedisplay->display = 
			imagedisplay_display_image( imagedisplay, 
				imagedisplay->image )) ) 
			return( -1 ); 
		if( !(imagedisplay->display_region = 
			vips_region_new( imagedisplay->display )) )
			return( -1 ); 

		if( !(imagedisplay->srgb = 
			imagedisplay_srgb_image( imagedisplay, 
				imagedisplay->display, &imagedisplay->mask )) ) 
			return( -1 ); 
		if( !(imagedisplay->srgb_region = 
			vips_region_new( imagedisplay->srgb )) )
			return( -1 ); 
		if( !(imagedisplay->mask_region = 
			vips_region_new( imagedisplay->mask )) )
			return( -1 ); 

		printf( "imagedisplay_update_conversion: image size %d x %d\n", 
			imagedisplay->display->Xsize, 
			imagedisplay->display->Ysize );
		printf( "** srgb image %p\n", imagedisplay->srgb );
		printf( "** new region %p\n", imagedisplay->srgb_region );

		gtk_widget_set_size_request( GTK_WIDGET( imagedisplay ),
			imagedisplay->display->Xsize, 
			imagedisplay->display->Ysize );

		//gtk_widget_queue_draw( GTK_WIDGET( imagedisplay ) ); 
	}

	return( 0 );
}

static void
imagedisplay_preeval( VipsImage *image, 
	VipsProgress *progress, Imagedisplay *imagedisplay )
{
	g_signal_emit( imagedisplay, 
		imagedisplay_signals[SIG_PRELOAD], 0, progress );
}

static void
imagedisplay_eval( VipsImage *image, 
	VipsProgress *progress, Imagedisplay *imagedisplay )
{
	g_signal_emit( imagedisplay, 
		imagedisplay_signals[SIG_LOAD], 0, progress );
}

static void
imagedisplay_posteval( VipsImage *image, 
	VipsProgress *progress, Imagedisplay *imagedisplay )
{
	g_signal_emit( imagedisplay, 
		imagedisplay_signals[SIG_POSTLOAD], 0, progress );
}

static void
imagedisplay_attach_progress( Imagedisplay *imagedisplay )
{
	g_assert( !imagedisplay->preeval_sig );
	g_assert( !imagedisplay->eval_sig );
	g_assert( !imagedisplay->posteval_sig );

	/* Attach an eval callback: this will tick down if we 
	 * have to decode this image.
	 */
	vips_image_set_progress( imagedisplay->image, TRUE ); 
	imagedisplay->preeval_sig = 
		g_signal_connect( imagedisplay->image, "preeval",
			G_CALLBACK( imagedisplay_preeval ), 
			imagedisplay );
	imagedisplay->eval_sig = 
		g_signal_connect( imagedisplay->image, "eval",
			G_CALLBACK( imagedisplay_eval ), 
			imagedisplay );
	imagedisplay->posteval_sig = 
		g_signal_connect( imagedisplay->image, "posteval",
			G_CALLBACK( imagedisplay_posteval ), 
			imagedisplay );
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

		imagedisplay_attach_progress( imagedisplay ); 
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
	if( mag > -600 &&
		mag < 1000000 &&
		imagedisplay->mag != mag ) { 
		printf( "imagedisplay_set_mag: %d\n", mag ); 

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

gboolean
imagedisplay_get_display_image_size( Imagedisplay *imagedisplay, 
	int *width, int *height )
{
	if( imagedisplay->display ) {
		*width = imagedisplay->display->Xsize;
		*height = imagedisplay->display->Ysize;

		return( TRUE ); 
	}
	else
		return( FALSE );
}

/* Map to underlying image coordinates from display image coordinates.
 */
void
imagedisplay_to_image_cods( Imagedisplay *imagedisplay,
	int display_x, int display_y, int *image_x, int *image_y )
{
	if( imagedisplay->mag > 0 ) {
		*image_x = display_x / imagedisplay->mag;
		*image_y = display_y / imagedisplay->mag;
	}
	else {
		*image_x = display_x * -imagedisplay->mag;
		*image_y = display_y * -imagedisplay->mag;
	}
}

/* Map to display cods from underlying image coordinates.
 */
void
imagedisplay_to_display_cods( Imagedisplay *imagedisplay,
	int image_x, int image_y, int *display_x, int *display_y )
{
	if( imagedisplay->mag > 0 ) {
		*display_x = image_x * imagedisplay->mag;
		*display_y = image_y * imagedisplay->mag;
	}
	else {
		*display_x = image_x / -imagedisplay->mag;
		*display_y = image_y / -imagedisplay->mag;
	}
}

VipsPel *
imagedisplay_get_ink( Imagedisplay *imagedisplay, int x, int y )
{
	VipsRect rect;

	rect.left = x;
	rect.top = y;
	rect.width = 1;
	rect.height = 1;
	if( vips_region_prepare( imagedisplay->image_region, &rect ) )
		return( NULL );

	return( VIPS_REGION_ADDR( imagedisplay->image_region, x, y ) );  
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
