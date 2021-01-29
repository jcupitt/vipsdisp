/* Manage conversion of images for display: zoom, colour, load, etc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#include <vips/vips.h>

#include "disp.h"

/*
#define DEBUG
 */

/* Use this threadpool to do background loads of images.
 */
static GThreadPool *conversion_background_load_pool = NULL;

G_DEFINE_TYPE( Conversion, conversion, G_TYPE_OBJECT );

enum {
        /* Properties.
         */
        PROP_SOURCE = 1,
        PROP_IMAGE,
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
        SIG_CHANGED,		/* The whole conversion has changed */
        SIG_DISPLAY_CHANGED,	/* The whole display has changed */
        SIG_AREA_CHANGED,	/* A tile has changed in the image */

        SIG_LAST
};

static guint conversion_signals[SIG_LAST] = { 0 };

static void
conversion_area_changed( Conversion *conversion, VipsRect *dirty )
{
        g_signal_emit( conversion, 
                conversion_signals[SIG_AREA_CHANGED], 0, dirty );
}

static void
conversion_display_changed( Conversion *conversion )
{
        g_signal_emit( conversion, 
                conversion_signals[SIG_DISPLAY_CHANGED], 0 );
}

static void
conversion_changed( Conversion *conversion )
{
        g_signal_emit( conversion, 
                conversion_signals[SIG_CHANGED], 0 );
}

/* Release everything ready for a new image.
 */
static void
conversion_disconnect( Conversion *conversion )
{
        if( !conversion->image ) {
                g_assert( !conversion->preeval_sig );
                return;
        }

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

        VIPS_UNREF( conversion->image );
        VIPS_UNREF( conversion->source );
        VIPS_UNREF( conversion->image_region );
        VIPS_UNREF( conversion->display );
        VIPS_UNREF( conversion->mask );
        VIPS_UNREF( conversion->rgb );
}

static void
conversion_dispose( GObject *object )
{
        Conversion *conversion = (Conversion *) object;

#ifdef DEBUG
        printf( "conversion_dispose: %p\n", object );
#endif /*DEBUG*/

        conversion_disconnect( conversion ); 

        G_OBJECT_CLASS( conversion_parent_class )->dispose( object );
}

/* Fetch a string-encoded int image header field, eg. from openslide. These 
 * are all represented as strings. Return the default value if there's any 
 * problem.
 */
static int
get_int( VipsImage *image, const char *field, int default_value )
{
        const char *str;

        if( vips_image_get_typeof( image, field ) &&
                !vips_image_get_string( image, field, &str ) )
                return( atoi( str ) );

        return( default_value );
}

static VipsImage *
conversion_open( Conversion *conversion, int level )
{
        VipsImage *image;

        if( vips_isprefix( "VipsForeignLoadOpenslide", conversion->loader ) ) {
                image = vips_image_new_from_source( conversion->source, 
			"", 
                        "level", level,
                        NULL );
        }
        else if( vips_isprefix( "VipsForeignLoadTiff", conversion->loader ) ) {
                /* We support three modes: subifd pyramids, page-based
                 * pyramids, and simple multi-page TIFFs (no pyramid).
                 */
                if( conversion->subifd_pyramid )
                        image = vips_image_new_from_source( conversion->source,
                                "", 
                                "subifd", level,
                                NULL );
                else if( conversion->page_pyramid )
                        image = vips_image_new_from_source( conversion->source,
                                "", 
                                "page", level,
                                NULL );
                else
                        image = vips_image_new_from_source( conversion->source,
                                "", 
                                NULL );
        }
        else 
                image = vips_image_new_from_source( conversion->source, 
                        "", 
			NULL );

        return( image );
}

/* Detect a TIFF pyramid made of subifds following a roughly /2 shrink.
 *
 * This may not be a pyr tiff, so no error if we can't find the layers.
 */
static void
conversion_get_tiff_pyramid_subifd( Conversion *conversion )
{
        int i;

#ifdef DEBUG
        printf( "conversion_get_tiff_pyramid_subifd:\n" );
#endif /*DEBUG*/

        for( i = 0; i < conversion->n_subifds; i++ ) {
                VipsImage *page;
                int level_width;
                int level_height;
                int expected_level_width;
                int expected_level_height;

                if( !(page = conversion_open( conversion, i )) )
                        return;
                level_width = page->Xsize;
                level_height = page->Ysize;
                VIPS_UNREF( page );

                /* The main image is size 1, subifd 0 is half that.
                 */
                expected_level_width = conversion->width / (2 << i);
                expected_level_height = conversion->height / (2 << i);

                /* This won't be exact due to rounding etc.
                 */
                if( abs( level_width - expected_level_width ) > 5 ||
                        level_width < 2 )
                        return;
                if( abs( level_height - expected_level_height ) > 5 ||
                        level_height < 2 )
                        return;

                conversion->level_width[i] = level_width;
                conversion->level_height[i] = level_height;
        }

        /* Now set level_count. This signals that we've found a pyramid.
         */
        conversion->level_count = conversion->n_subifds;
}

/* Detect a TIFF pyramid made of pages following a roughly /2 shrink.
 *
 * This may not be a pyr tiff, so no error if we can't find the layers.
 */
static void
conversion_get_tiff_pyramid_page( Conversion *conversion )
{
        int i;

#ifdef DEBUG
        printf( "conversion_get_tiff_pyramid_page:\n" );
#endif /*DEBUG*/

        /* Single-page docs can't be pyramids.
         */
        if( conversion->n_pages < 2 )
                return;

        for( i = 0; i < conversion->n_pages; i++ ) {
                VipsImage *page;
                int level_width;
                int level_height;
                int expected_level_width;
                int expected_level_height;

                if( !(page = conversion_open( conversion, i )) )
                        return;
                level_width = page->Xsize;
                level_height = page->Ysize;
                VIPS_UNREF( page );

                expected_level_width = conversion->width / (1 << i);
                expected_level_height = conversion->height / (1 << i);

                /* This won't be exact due to rounding etc.
                 */
                if( abs( level_width - expected_level_width ) > 5 ||
                        level_width < 2 )
                        return;
                if( abs( level_height - expected_level_height ) > 5 ||
                        level_height < 2 )
                        return;

                conversion->level_width[i] = level_width;
                conversion->level_height[i] = level_height;
        }

        /* Now set level_count. This signals that we've found a pyramid.
         */
        conversion->level_count = conversion->n_pages;
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
conversion_attach_progress( Conversion *conversion )
{
#ifdef DEBUG
        printf( "conversion_attach_progress:\n" ); 
#endif /*DEBUG*/

        g_assert( conversion->preeval_sig == 0 ); 

        vips_image_set_progress( conversion->image, TRUE ); 
        conversion->preeval_sig = g_signal_connect( conversion->image, 
                "preeval", 
                G_CALLBACK( conversion_preeval ), 
                conversion );
        conversion->eval_sig = g_signal_connect( conversion->image, 
                "eval",
                G_CALLBACK( conversion_eval ), 
                conversion );
        conversion->posteval_sig = g_signal_connect( conversion->image, 
                "posteval",
                G_CALLBACK( conversion_posteval ), 
                conversion );
}

static int
conversion_set_image( Conversion *conversion, VipsImage *image )
{
#ifdef DEBUG
        printf( "conversion_set_image: starting ..\n" );
#endif /*DEBUG*/

        conversion_disconnect( conversion );

        conversion->image = image;
	g_object_ref( image );
        conversion->width = image->Xsize;
        conversion->height = image->Ysize;
        conversion->n_pages = vips_image_get_n_pages( image );
        conversion->n_subifds = vips_image_get_n_subifds( image );

        /* For openslide, read out the level structure too.
         */
        if( vips_isprefix( "VipsForeignLoadOpenslide", conversion->loader ) ) {
                int level_count;
                int level;

                level_count = get_int( image, "openslide.level-count", 1 );
                level_count = VIPS_CLIP( 1, level_count, MAX_LEVELS );
                conversion->level_count = level_count;

                for( level = 0; level < level_count; level++ ) {
                        char name[256];

                        vips_snprintf( name, 256,
                                "openslide.level[%d].width", level );
                        conversion->level_width[level] =
                                 get_int( image, name, 0 );
                        vips_snprintf( name, 256,
                                "openslide.level[%d].height", level );
                        conversion->level_height[level] =
                                get_int( image, name, 0 );
                }
        }

        /* For tiff, scan the image and try to spot page-based and ifd-based
         * pyramids.
         */
        if( vips_isprefix( "VipsForeignLoadTiff", conversion->loader ) ) {
                /* Test for a subifd pyr first, since we can do that from just
                 * one page.
                 */
                conversion->subifd_pyramid = TRUE;
                conversion_get_tiff_pyramid_subifd( conversion );

                if( conversion->level_count == 0 ) {
                        conversion->subifd_pyramid = FALSE;
                        conversion->page_pyramid = TRUE;

                        conversion_get_tiff_pyramid_page( conversion );

                        if( conversion->level_count == 0 )
                                conversion->page_pyramid = FALSE;
                }
        }
        
#ifdef DEBUG
{
        int i;

        printf( "conversion_set_image:\n" );
        printf( "\tloader = %s\n", conversion->loader );
        printf( "\twidth = %d\n", conversion->width );
        printf( "\theight = %d\n", conversion->height );
        printf( "\tn_pages = %d\n", conversion->n_pages );
        printf( "\tn_subifds = %d\n", conversion->n_subifds );
        printf( "\tsubifd_pyramid = %d\n", conversion->subifd_pyramid );
        printf( "\tpage_pyramid = %d\n", conversion->page_pyramid );
        printf( "\tlevel_count = %d\n", conversion->level_count );

        for( i = 0; i < conversion->level_count; i++ )
                printf( "\t%2d) %d x %d\n", 
                        i,
                        conversion->level_width[i], 
                        conversion->level_height[i] ); 
}
#endif /*DEBUG*/

        conversion->image_region = vips_region_new( conversion->image );

        /* This will be set TRUE again at the end of the background
         * load. This will trigger conversion_update_display() for us.
         */
        g_object_set( conversion, "loaded", FALSE, NULL );

        /* We ref this conversion so it won't die before the
         * background load is done. The matching unref is at the end
         * of bg load.
         */
        g_object_ref( conversion );

        conversion_attach_progress( conversion ); 

        g_thread_pool_push( conversion_background_load_pool, conversion, NULL );

        return( 0 );
}

int
conversion_set_file( Conversion *conversion, GFile *file )
{
        GError *error = NULL;

        VipsSource *source;
        char *path;

        if( !file )
                return( 0 );

        if( (path = g_file_get_path( file )) ) {
                /* If this GFile is a path to a file on disc, we can
                 * make a source directly from it. This will allow
                 * things like mmap and openslide to work.
                 */

#ifdef DEBUG
                printf( "conversion_set_file: connecting via path\n" );
#endif /*DEBUG*/

                if( !(source = vips_source_new_from_file( path )) )
                        return( -1 );
                g_free( path );
        }
        else {
                /* Otherwise, this is perhaps a pipe or an area of
                 * memory. We can connect via g_input_stream.
                 */
                GInputStream *stream;

#ifdef DEBUG
                printf( "conversion_set_file: connecting via "
                        "ginputstream\n" );
#endif /*DEBUG*/

                if( !(stream = G_INPUT_STREAM( 
                        g_file_read( file, NULL, &error ) )) ) {
                        vips_error_g( &error );
                        return( -1 );
                }

                if( !(source = VIPS_SOURCE( 
                        vips_source_g_input_stream_new( stream ) )) ) {
			VIPS_UNREF( stream );
                        return( -1 );
                }
                VIPS_UNREF( stream );
        }
        
        g_object_set( conversion, "source", source, NULL ); 

        VIPS_UNREF( source );

        return( 0 );
}

/* Make the rgb image we paint with. This runs synchronously and is not
 * threaded.
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
                        conversion->scale, conversion->offset, 
                        "uchar", TRUE, 
                        NULL ) ) {
                        g_object_unref( image );
                        return( NULL ); 
                }
                g_object_unref( image );
                image = x;
        }
        else {
                /* To uchar.
                 */
                if( vips_cast_uchar( image, &x, NULL ) ) {
                        g_object_unref( image );
                        return( NULL ); 
                }
                g_object_unref( image );
                image = x;
        }

        /* falsecolour would go here.
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

                conversion_display_changed( conversion );
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
        printf( "conversion_render_notify_idle: "
                "left = %d, top = %d, width = %d, height = %d\n",
                update->rect.left, update->rect.top,
                update->rect.width, update->rect.height );
#endif /*DEBUG*/

        /* Again, stuff can run here long after the image has vanished, check
         * before we update.
         */
        if( update->image == conversion->display ) 
                conversion_area_changed( conversion, &update->rect );

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
conversion_display_image( Conversion *conversion, VipsImage **mask_out )
{
        VipsImage *image;
        VipsImage *x;
        VipsImage *mask;

        g_assert( mask_out ); 

        if( conversion->level_count ) {
		/* There's a pyramid ... compute the size of image we need,
		 * then find the layer which is one larger.
		 */
		int required_width = conversion->mag < 0 ? 
			conversion->width / -conversion->mag : 
			conversion->width * conversion->mag;

		int i;
		int level;

		for( i = 0; i < conversion->level_count; i++ ) 
			if( conversion->level_width[i] < required_width )
				break;
		level = VIPS_CLIP( 0, i - 1, conversion->level_count - 1 );

		if( !(image = conversion_open( conversion, level )) )
			return( NULL );
	}
	else {
		image = conversion->image;
		g_object_ref( image ); 
	}

	if( conversion->mag < 0 ) {
		/* We may have already zoomed out a bit because we've loaded
		 * some layer other than the base one. Recalculate the
		 * subsample as (current_width / required_width).
		 */
		int subsample = image->Xsize / 
			(conversion->width / -conversion->mag);

		if( vips_subsample( image, &x, subsample, subsample, NULL ) ) {
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

        /* Do a huge blur .. this is a slow operation, and handy for
         * debugging.
        if( vips_gaussblur( image, &x, 100.0, NULL ) ) {
                g_object_unref( image );
                return( NULL ); 
        }
        g_object_unref( image );
        image = x;
         */

        x = vips_image_new();
        mask = vips_image_new();
        if( vips_sink_screen( image, 
                x, mask, 
                tile_size, tile_size, 400, 0, 
                conversion_render_notify, conversion ) ) {
                g_object_unref( x );
                g_object_unref( mask );
                g_object_unref( image );
                return( NULL );
        }
        g_object_unref( image );
        image = x;

        *mask_out = mask;

        return( image );
}

/* Rebuild the entire pipeline from zoom onwards.
 */
static int
conversion_update_display( Conversion *conversion )
{
	/* Don't update if we're still loading.
	 */
	if( !conversion->loaded )
		return( 0 );

        VIPS_UNREF( conversion->display );
        VIPS_UNREF( conversion->mask );

        if( conversion->image ) {
                if( !(conversion->display = conversion_display_image( 
			conversion, &conversion->mask )) ) 
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
        case PROP_SOURCE:
{
                VipsSource *source;
                const char *loader;
                VipsImage *image;

                source = VIPS_SOURCE( g_value_get_object( value ) );
                if( !(loader = vips_foreign_find_load_source( source )) ||
                        !(image = vips_image_new_from_source( source, "", 
				NULL )) ) {
                        g_warning( "unable to set source: %s", 
                                vips_error_buffer() );
                        vips_error_clear();
                }
		else {
			/* We have a new source and image -- we're about to 
			 * junk everything.
			 */
			conversion_disconnect( conversion );

			conversion->loader = loader; 
			conversion->source = source; 
			g_object_ref( source );

			g_object_set( conversion, "image", image, NULL );

			g_object_unref( image );
		}
		break;
}

        case PROP_IMAGE:
                if( conversion_set_image( conversion, 
                        g_value_get_object( value ) ) ) {
                        g_warning( "unable to set image: %s", 
                                vips_error_buffer() );
                        vips_error_clear();
                }
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
                        conversion->mag != mag ) {
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
                        conversion->scale != d ) { 
#ifdef DEBUG
                        printf( "conversion_set_scale: %g\n", d ); 
#endif /*DEBUG*/

                        conversion->scale = d;
                        conversion_update_rgb( conversion );
                }
                break;

        case PROP_OFFSET:
                d = g_value_get_double( value );
                if( d >= -100000 &&
                        d <= 1000000 &&
                        conversion->offset != d ) { 
#ifdef DEBUG
                        printf( "conversion_set_offset: %g\n", d ); 
#endif /*DEBUG*/

                        conversion->offset = d;
                        conversion_update_rgb( conversion );
                }
                break;

        case PROP_LOADED:
                loaded = g_value_get_boolean( value );
                if( conversion->loaded != loaded ) { 
#ifdef DEBUG
                        printf( "conversion_set_loaded: %d\n", loaded ); 
#endif /*DEBUG*/

                        conversion->loaded = loaded;

			conversion_changed( conversion );
                        conversion_update_display( conversion );
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
        case PROP_SOURCE:
                g_value_set_object( value, conversion->source );
                break;

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

        g_assert( conversion->image_region ); 

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

        g_object_class_install_property( gobject_class, PROP_SOURCE,
                g_param_spec_object( "source",
                        _( "source" ),
                        _( "The source to be displayed" ),
                        VIPS_TYPE_SOURCE,
                        G_PARAM_READWRITE ) );

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

        conversion_signals[SIG_CHANGED] = g_signal_new( "changed",
                G_TYPE_FROM_CLASS( class ),
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET( ConversionClass, changed ), 
                NULL, NULL,
                g_cclosure_marshal_VOID__VOID,
                G_TYPE_NONE, 0 ); 

        conversion_signals[SIG_DISPLAY_CHANGED] = g_signal_new( "display_changed",
                G_TYPE_FROM_CLASS( class ),
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET( ConversionClass, display_changed ), 
                NULL, NULL,
                g_cclosure_marshal_VOID__VOID,
                G_TYPE_NONE, 0 ); 

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

        if( !conversion->loaded ) 
                return( NULL ); 

        rect.left = x;
        rect.top = y;
        rect.width = 1;
        rect.height = 1;
        if( vips_region_prepare( conversion->image_region, &rect ) )
                return( NULL );

        return( VIPS_REGION_ADDR( conversion->image_region, x, y ) );  
}

const char *
conversion_get_path( Conversion *conversion )
{
	if( conversion->source )
		return( vips_connection_filename( 
			VIPS_CONNECTION( conversion->source ) ) );

	return( NULL );
}

Conversion *
conversion_new( void ) 
{
        Conversion *conversion;

        conversion = g_object_new( conversion_get_type(),
                NULL );

        return( conversion ); 
}
