/* Manage conversion of images for display: zoom, colour, load, etc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#include <vips/vips.h>

#include "disp.h"

/* Use this threadpool to do background loads of images.
 */
static GThreadPool *conversion_background_load_pool = NULL;

G_DEFINE_TYPE( Conversion, conversion, G_TYPE_OBJECT );

enum {
	/* Properties.
	 */
	PROP_IMAGE = 1,
	PROP_RGB,
	PROP_MAG,
	PROP_SCALE,
	PROP_OFFSET,
	PROP_LOADED,

	/* Our signals. 
	 */
	SIG_PRELOAD,
	SIG_LOAD,
	SIG_POSTLOAD,
	SIG_AREA_CHANGED,

	SIG_LAST
};

static guint conversion_signals[SIG_LAST] = { 0 };

static void
conversion_disconnect( Conversion *conversion )
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
}

static void
conversion_dispose( GObject *object )
{
	Conversion *conversion = (Conversion *) object;

	conversion_disconnect( conversion ); 
	VIPS_UNREF( conversion->rgb );
	VIPS_UNREF( conversion->display );
	VIPS_UNREF( conversion->image_region );
	VIPS_UNREF( conversion->image );

	G_OBJECT_CLASS( conversion_parent_class )->dispose( object );
}

/* Make the rgb image we paint with. 
 */
static VipsImage *
conversion_rgb_image( Conversion *conversion, VipsImage *in ) 
{
	VipsImage *image;
	VipsImage *x;

	/* image redisplays the head of the pipeline. Hold a ref to it as we
	 * work.
	 */
	image = in;
	g_object_ref( image ); 

	/* Scale and offset.
	 */
	if( conversion->scale != 1.0 ||
		conversion->offset != 0.0 ) {
		if( vips_linear1( image, &x, 
			conversion->scale, conversion->offset, NULL ) ) {
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

	/* To uchar.
	 */
	if( vips_cast_uchar( image, &x, NULL ) ) {
		g_object_unref( image );
		return( NULL ); 
	}
	g_object_unref( image );
	image = x;

	/* Do a huge blur .. this is a slow operation, and handy for
	 * debugging.
	if( vips_gaussblur( image, &x, 100.0, NULL ) ) {
		g_object_unref( image );
		return( NULL ); 
	}
	g_object_unref( image );
	image = x;
	 */

	return( image );
}

/* Regenerate the second half of the conversion: to display RGB.
 */
static int
conversion_update_rgb( Conversion *conversion )
{
	if( conversion->display ) { 
		VipsImage *rgb;

		if( !(rgb = conversion_rgb_image( conversion, 
			conversion->display )) ) 
			return( -1 ); 
		g_object_set( conversion, "rgb", rgb, NULL ); 
		g_object_unref( rgb ); 
	}

	return( 0 );
}

typedef struct _ConversionUpdate {
	Conversion *conversion;
	VipsImage *image;
	VipsRect rect;
} ConversionUpdate;

/* Run by the main GUI thread when an update comes in. 
 */
static gboolean
conversion_render_notify_idle( void *user_data )
{
	ConversionUpdate *update = (ConversionUpdate *) user_data;
	Conversion *conversion = update->conversion;

#ifdef DEBUG
	printf( "conversion_render_cb: "
		"left = %d, top = %d, width = %d, height = %d\n",
		update->rect.left, update->rect.top,
		update->rect.width, update->rect.height );
#endif /*DEBUG*/

	/* Again, stuff can run here long after the image has vanished, check
	 * before we update.
	 */
	if( update->image == conversion->display ) 
		g_signal_emit( conversion, 
			conversion_signals[SIG_AREA_CHANGED], 0, update->rect );

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

	/* We can come here after Conversion has junked this image and
	 * started displaying another. Check the image is still correct.
	 */
	if( image == conversion->display ) { 
		ConversionUpdate *update = g_new( ConversionUpdate, 1 );

		update->conversion = conversion;
		update->image = image;
		update->rect = *rect;

		g_idle_add( conversion_render_notify_idle, update );
	}
}

/* Make the screen image. This is the thing we display pixel values from in
 * the status bar.
 */
static VipsImage *
conversion_display_image( Conversion *conversion, 
	VipsImage *in, VipsImage **mask_out )
{
	VipsImage *image;
	VipsImage *x;
	VipsImage *mask;

	g_assert( mask_out ); 

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

	x = vips_image_new();
	mask = vips_image_new();
	if( vips_sink_screen( image, 
		x, mask, 
		tile_size, tile_size, 400, 0, 
		conversion_render_notify, conversion ) ) {
		return( -1 );
	}
	g_object_unref( image );
	image = x;

	*mask_out = mask;

	return( image );
}

/* Rebuild the pipeline from zoom onwards.
 */
static int
conversion_update_display( Conversion *conversion )
{
	VIPS_UNREF( conversion->image_region );
	VIPS_UNREF( conversion->display );

	if( conversion->image ) {
		if( !(conversion->image_region = 
			vips_region_new( conversion->image )) )
			return( -1 ); 
	
		if( !(conversion->display = 
			conversion_display_image( conversion, 
				conversion->image, &conversion->mask )) ) 
			return( -1 ); 
	}

	conversion_update_rgb( conversion );

	return( 0 );
}

static void
conversion_set_property( GObject *object, 
	guint prop_id, const GValue *value, GParamSpec *pspec )
{
	Conversion *conversion = (Conversion *) object;

	int mag;
	double d;
	gboolean loaded;

	switch( prop_id ) {
	case PROP_IMAGE:
		VIPS_UNREF( conversion->image ); 
		conversion->image = g_value_get_object( value );
		g_object_ref( conversion->image );
		conversion_update_display( conversion );
		break;

	case PROP_RGB:
		VIPS_UNREF( conversion->rgb ); 
		conversion->rgb = g_value_get_object( value );
		g_object_ref( conversion->rgb );
		break;

	case PROP_MAG:
		mag = g_value_get_int( value );
		if( mag >= -600 &&
			mag <= 1000000 &&
			conversion->mag != mag &&
			conversion->loaded ) { 
#ifdef DEBUG
			printf( "conversion_set_mag: %d\n", mag ); 
#endif /*DEBUG*/

			conversion->mag = mag;
			conversion_update_display( conversion );
		}
		break;

	case PROP_SCALE:
		d = g_value_get_double( value );
		if( d > 0 &&
			d <= 1000000 &&
			conversion->scale != d &&
			conversion->loaded ) { 
#ifdef DEBUG
			printf( "conversion_set_scale: %g\n", d ); 
#endif /*DEBUG*/

			conversion->scale = d;
			conversion_update_display( conversion );
		}
		break;

	case PROP_OFFSET:
		d = g_value_get_double( value );
		if( d >= -100000 &&
			d <= 1000000 &&
			conversion->offset != d &&
			conversion->loaded ) { 
#ifdef DEBUG
			printf( "conversion_set_offset: %g\n", d ); 
#endif /*DEBUG*/

			conversion->offset = d;
			conversion_update_display( conversion );
		}
		break;

	case PROP_LOADED:
		loaded = g_value_get_boolean( value );
		if( conversion->loaded != loaded ) { 
#ifdef DEBUG
			printf( "conversion_set_loaded: %d\n", loaded ); 
#endif /*DEBUG*/

			conversion->loaded = loaded;
		}
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID( object, prop_id, pspec );
		break;
	}
}

static void
conversion_get_property( GObject *object, 
	guint prop_id, GValue *value, GParamSpec *pspec )
{
	Conversion *conversion = (Conversion *) object;

	switch( prop_id ) {
	case PROP_IMAGE:
		g_value_set_object( value, conversion->image );
		break;

	case PROP_RGB:
		g_value_set_object( value, conversion->rgb );
		break;

	case PROP_MAG:
		g_value_set_int( value, conversion->mag );
		break;

	case PROP_SCALE:
		g_value_set_double( value, conversion->scale );
		break;

	case PROP_OFFSET:
		g_value_set_double( value, conversion->offset );
		break;

	case PROP_LOADED:
		g_value_set_boolean( value, conversion->loaded );
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID( object, prop_id, pspec );
		break;
	}
}

static void
conversion_init( Conversion *conversion )
{
	conversion->mag = 1;
	conversion->scale = 1.0;
}

static void
conversion_force_load( Conversion *conversion )
{
	if( conversion->image_region &&
		!conversion->loaded ) { 
		VipsRect rect;

		rect.left = 0;
		rect.top = 0;
		rect.width = 1;
		rect.height = 1;
		(void) vips_region_prepare( conversion->image_region, &rect );
	}
}

/* This runs in the main thread when the bg load is done. We can't use
 * postload since that will only fire if we are actually loading, and not if
 * the image is coming from cache.
 */
static gboolean
conversion_background_load_done_cb( void *user_data )
{
	Conversion *conversion = (Conversion *) user_data;

	/* You can now fetch pixels.
	 */
	g_object_set( conversion, "loaded", TRUE, NULL );

	/* Drop the ref that kept this conversion alive during load.
	 */
	g_object_unref( conversion ); 

	return( FALSE ); 
}

/* This runs for the background load threadpool.
 */
static void 
conversion_background_load( void *data, void *user_data )
{
	Conversion *conversion = (Conversion *) data;

#ifdef DEBUG
	printf( "conversion_background_load: starting ..\n" );
#endif /*DEBUG*/

	conversion_force_load( conversion );

	g_idle_add( conversion_background_load_done_cb, conversion );

#ifdef DEBUG
	printf( "conversion_background_load: .. done\n" );
#endif /*DEBUG*/
}

static void
conversion_class_init( ConversionClass *class )
{
	GObjectClass *gobject_class = G_OBJECT_CLASS( class );

	gobject_class->dispose = conversion_dispose;
	gobject_class->set_property = conversion_set_property;
	gobject_class->get_property = conversion_get_property;

	g_object_class_install_property( gobject_class, PROP_IMAGE,
		g_param_spec_object( "image",
			_( "image" ),
			_( "The image to be converted" ),
			VIPS_TYPE_IMAGE,
			G_PARAM_READWRITE ) );

	g_object_class_install_property( gobject_class, PROP_RGB,
		g_param_spec_object( "rgb",
			_( "rgb" ),
			_( "The converted image" ),
			VIPS_TYPE_IMAGE,
			G_PARAM_READWRITE ) );

	g_object_class_install_property( gobject_class, PROP_MAG,
		g_param_spec_int( "mag",
			_( "mag" ),
			_( "Magnification factor" ),
			-1000000, 1000000, 1,
			G_PARAM_READWRITE ) );

	g_object_class_install_property( gobject_class, PROP_SCALE,
		g_param_spec_double( "scale",
			_( "scale" ),
			_( "Scale" ),
			-1000000, 1000000, 1,
			G_PARAM_READWRITE ) );

	g_object_class_install_property( gobject_class, PROP_OFFSET,
		g_param_spec_double( "offset",
			_( "offset" ),
			_( "Offset" ),
			-1000000, 1000000, 0,
			G_PARAM_READWRITE ) );

	g_object_class_install_property( gobject_class, PROP_LOADED,
		g_param_spec_boolean( "loaded",
			_( "loaded" ),
			_( "Image has finished loading" ),
			FALSE,
			G_PARAM_READWRITE ) );

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

	conversion_signals[SIG_AREA_CHANGED] = g_signal_new( "area-changed",
		G_TYPE_FROM_CLASS( class ),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET( ConversionClass, area_changed ), 
		NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER );

	g_assert( !conversion_background_load_pool ); 
	conversion_background_load_pool = g_thread_pool_new( 
		conversion_background_load,
		NULL, -1, FALSE, NULL ); 
}

/* The pre/eval/post callbacks run from the background load thread. We can't
 * call into gtk directly: instead, add an idle callback to trigger the
 * signal from conversion.
 */
typedef struct _ConversionBackgroundLoadUpdate {
	Conversion *conversion;
	VipsImage *image;
	VipsProgress *progress;
} ConversionBackgroundLoadUpdate;

static gboolean
conversion_preeval_cb( void *user_data )
{
	ConversionBackgroundLoadUpdate *update = 
		(ConversionBackgroundLoadUpdate *) user_data;

#ifdef DEBUG
	printf( "conversion_preeval_cb:\n" ); 
#endif /*DEBUG*/

	g_signal_emit( update->conversion, 
		conversion_signals[SIG_PRELOAD], 0, update->progress );

	g_free( update );

	return( FALSE ); 
}

static void
conversion_preeval( VipsImage *image, 
	VipsProgress *progress, Conversion *conversion )
{
	ConversionBackgroundLoadUpdate *update = 
		g_new( ConversionBackgroundLoadUpdate, 1 );

	update->conversion = conversion;
	update->image = image;
	update->progress = progress;

	g_idle_add( conversion_preeval_cb, update );
}

static gboolean
conversion_eval_cb( void *user_data )
{
	ConversionBackgroundLoadUpdate *update = 
		(ConversionBackgroundLoadUpdate *) user_data;

	g_signal_emit( update->conversion, 
		conversion_signals[SIG_LOAD], 0, update->progress );

	g_free( update );

	return( FALSE ); 
}

static void
conversion_eval( VipsImage *image, 
	VipsProgress *progress, Conversion *conversion )
{
	ConversionBackgroundLoadUpdate *update = 
		g_new( ConversionBackgroundLoadUpdate, 1 );

	update->conversion = conversion;
	update->image = image;
	update->progress = progress;

	g_idle_add( conversion_eval_cb, update );
}

static gboolean
conversion_posteval_cb( void *user_data )
{
	ConversionBackgroundLoadUpdate *update = 
		(ConversionBackgroundLoadUpdate *) user_data;

#ifdef DEBUG
	printf( "conversion_posteval_cb:\n" ); 
#endif /*DEBUG*/

	g_signal_emit( update->conversion, 
		conversion_signals[SIG_POSTLOAD], 0, update->progress );

	g_free( update );

	return( FALSE ); 
}

static void
conversion_posteval( VipsImage *image, 
	VipsProgress *progress, Conversion *conversion )
{
	ConversionBackgroundLoadUpdate *update = 
		g_new( ConversionBackgroundLoadUpdate, 1 );

	update->conversion = conversion;
	update->image = image;
	update->progress = progress;

	g_idle_add( conversion_posteval_cb, update );
}

static void
conversion_attach_progress( Conversion *conversion, VipsImage *image )
{
#ifdef DEBUG
	printf( "conversion_attach_progress:\n" ); 
#endif /*DEBUG*/

	conversion_disconnect( conversion ); 

	vips_image_set_progress( image, TRUE ); 
	conversion->preeval_sig = g_signal_connect( image, "preeval",
		G_CALLBACK( conversion_preeval ), 
		conversion );
	conversion->eval_sig = g_signal_connect( image, "eval",
		G_CALLBACK( conversion_eval ), 
		conversion );
	conversion->posteval_sig = g_signal_connect( image, "posteval",
		G_CALLBACK( conversion_posteval ), 
		conversion );
}

static void
conversion_close_memory( VipsImage *image, gpointer contents )
{
	g_free( contents );
}

int
conversion_set_file( Conversion *conversion, GFile *file )
{
#ifdef DEBUG
	printf( "conversion_set_file: starting ..\n" );
#endif /*DEBUG*/

	if( file != NULL ) {
		gchar *path;
		gchar *contents;
		gsize length;
		VipsImage *image;

		if( (path = g_file_get_path( file )) ) {
#ifdef DEBUG
			printf( "conversion_set_file: path = %s\n", path ); 
#endif /*DEBUG*/

			if( !(image = 
				vips_image_new_from_file( path, NULL )) ) {
				g_free( path ); 
				return( -1 );
			}
			g_free( path ); 
		}
		else if( g_file_load_contents( file, NULL, 
			&contents, &length, NULL, NULL ) ) {
#ifdef DEBUG
			printf( "conversion_set_file: buffer of %zd bytes\n", 
			     length ); 
#endif /*DEBUG*/

			if( !(image =
				vips_image_new_from_buffer( contents, length, 
					"", NULL )) ) {
				g_free( contents );
				return( -1 ); 
			}

			g_signal_connect( image, "close",
				G_CALLBACK( conversion_close_memory ), 
				contents );
		}
		else {
			vips_error( "conversion", 
				"unable to load GFile object" );
			return( -1 );
		}

		/* This will be set TRUE again at the end of the background
		 * load..
		 */
		g_object_set( conversion, "loaded", FALSE, NULL );

		/* We ref this conversion so it won't die before the
		 * background load is done. The matching unref is at the end
		 * of bg load.
		 */
		g_object_ref( conversion );

		conversion_attach_progress( conversion, image ); 
		g_object_set( conversion, "image", image, NULL ); 
		VIPS_UNREF( image ); 

		g_thread_pool_push( conversion_background_load_pool, 
			conversion, NULL ); 
	}

#ifdef DEBUG
	printf( "conversion_set_file: .. done\n" );
#endif /*DEBUG*/

	return( 0 );
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
	gboolean loaded;
	VipsRect rect;

	g_object_get( conversion, "loaded", &loaded, NULL ); 
	if( !loaded ) 
		return( NULL ); 

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

	conversion = g_object_new( conversion_get_type(),
		NULL );

	return( conversion ); 
}
