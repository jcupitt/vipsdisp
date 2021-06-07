/* Manage conversion of images for display: zoom, colour, load, etc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#include <vips/vips.h>

#include "disp.h"

/*
#define DEBUG_VERBOSE
#define DEBUG
 */

/* Use this threadpool to do background loads of images.
 */
static GThreadPool *conversion_background_load_pool = NULL;

G_DEFINE_TYPE( Conversion, conversion, G_TYPE_OBJECT );

enum {
        /* Properties.
         */
        PROP_RGB = 1,
        PROP_MODE,
        PROP_MAG,
        PROP_SCALE,
        PROP_OFFSET,
        PROP_PAGE,
        PROP_FALSECOLOUR,
        PROP_LOG,
        PROP_LOADED,

        /* Our signals. 
         */
        SIG_PREEVAL,
        SIG_EVAL,
        SIG_POSTEVAL,
        SIG_PAGE_CHANGED,	/* The page number has changed (eg. animate) */
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
conversion_page_changed( Conversion *conversion )
{
        g_signal_emit( conversion, 
                conversion_signals[SIG_PAGE_CHANGED], 0 );
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
#ifdef DEBUG
	printf( "conversion_disconnect:\n" );
#endif /*DEBUG*/

        if( conversion->image ) {
                g_assert( conversion->preeval_sig );
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

        VIPS_FREE( conversion->delay );
        conversion->n_delay = 0;
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

	if( conversion->page_flip_id )
		VIPS_FREEF( g_source_remove, conversion->page_flip_id );

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

/* Open at a specified magnification level. Take page from the conversion.
 */
static VipsImage *
conversion_open( Conversion *conversion, int level )
{
	/* In toilet-roll mode, we open all pages.
	 */
	int n = conversion->type == CONVERSION_TYPE_TOILET_ROLL ? 
		-1 : 1;
	int page = conversion->type == CONVERSION_TYPE_TOILET_ROLL ? 
		0 : conversion->page;

        VipsImage *image;

        if( vips_isprefix( "openslide", conversion->loader ) ) {
		/* These only have a "level" dimension.
		 */
                image = vips_image_new_from_source( conversion->source, 
			"", 
                        "level", level,
                        NULL );
        }
        else if( vips_isprefix( "tiff", conversion->loader ) ) {
                /* We support three modes: subifd pyramids, page-based
                 * pyramids, and simple multi-page TIFFs (no pyramid).
                 */
                if( conversion->subifd_pyramid )
                        image = vips_image_new_from_source( conversion->source,
                                "", 
				"page", page,
                                "subifd", level,
				"n", n,
                                NULL );
                else if( conversion->page_pyramid )
			/* No "n" here since pages are mag levels.
			 */
                        image = vips_image_new_from_source( conversion->source,
                                "", 
                                "page", level,
                                NULL );
                else
			/* Pages are regular pages.
			 */
                        image = vips_image_new_from_source( conversion->source,
                                "", 
				"page", page,
				"n", n,
                                NULL );
        }
	else if( vips_isprefix( "jp2k", conversion->loader ) ) {
		/* These only have a "page" param.
		 */
		image = vips_image_new_from_source( conversion->source,
			"", 
			"page", level,
			NULL );
	}
	else if( vips_isprefix( "pdf", conversion->loader ) ||
		vips_isprefix( "webp", conversion->loader ) ||
		vips_isprefix( "gif", conversion->loader ) ) {
		/* Support page and n.
		 */
		image = vips_image_new_from_source( conversion->source,
			"", 
			"page", level,
			"n", n,
			NULL );
	}
        else 
		/* Don't support any page spec.
		 */
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
conversion_get_pyramid_subifd( Conversion *conversion )
{
        int i;

#ifdef DEBUG
        printf( "conversion_get_pyramid_subifd:\n" );
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

/* Detect a pyramid made of pages following a roughly /2 shrink.
 *
 * This may not be a pyr tiff, so no error if we can't find the layers.
 */
static void
conversion_get_pyramid_page( Conversion *conversion )
{
        int i;

#ifdef DEBUG
        printf( "conversion_get_pyramid_page:\n" );
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

static void
conversion_preeval( VipsImage *image, 
        VipsProgress *progress, Conversion *conversion )
{
        g_signal_emit( conversion, 
                conversion_signals[SIG_PREEVAL], 0, progress );
}

static void
conversion_eval( VipsImage *image, 
        VipsProgress *progress, Conversion *conversion )
{
        g_signal_emit( conversion, 
                conversion_signals[SIG_EVAL], 0, progress );
}

static void
conversion_posteval( VipsImage *image, 
        VipsProgress *progress, Conversion *conversion )
{
        g_signal_emit( conversion, 
                conversion_signals[SIG_POSTEVAL], 0, progress );
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

/* image needs to have been opened with n == -1, ie. a toilet roll.
 */
static int
conversion_set_image( Conversion *conversion, 
	const char *loader, VipsImage *image )
{
	ConversionMode mode;

#ifdef DEBUG
        printf( "conversion_set_image: starting ..\n" );
#endif /*DEBUG*/

	/* You must conversion_disconnect() before calling this.
	 */
	g_assert( !conversion->image );

        conversion->image = image;
	g_object_ref( image );
        conversion->loader = loader;
        conversion->width = image->Xsize;
        conversion->height = vips_image_get_page_height( image );
        conversion->n_pages = vips_image_get_n_pages( image );
        conversion->n_subifds = vips_image_get_n_subifds( image );

        if( vips_image_get_typeof( image, "delay" ) ) {
		int *delay;
		int n_delay;

		if( vips_image_get_array_int( image, "delay",
                        &delay, &n_delay ) )
			return( -1 );

		conversion->delay = g_new( int, n_delay );
		memcpy( conversion->delay, delay, n_delay * sizeof( int ) );
		conversion->n_delay = n_delay;
	}

	/* Are all pages the same size? We can only use animation and 
	 * toilet-roll mode in this case.
	 */
	if( image->Ysize == conversion->height * conversion->n_pages )
		conversion->pages_same_size = TRUE;

        /* For openslide, read out the level structure.
         */
        if( vips_isprefix( "openslide", conversion->loader ) ) {
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
        if( vips_isprefix( "tiff", conversion->loader ) ) {
                /* Test for a subifd pyr first, since we can do that from just
                 * one page.
                 */
                conversion->subifd_pyramid = TRUE;
                conversion_get_pyramid_subifd( conversion );

                if( conversion->level_count == 0 ) {
                        conversion->subifd_pyramid = FALSE;
                        conversion->page_pyramid = TRUE;

                        conversion_get_pyramid_page( conversion );

                        if( conversion->level_count == 0 )
                                conversion->page_pyramid = FALSE;
                }
        }

        /* jp2k is always page-based.
         */
        if( vips_isprefix( "jp2k", conversion->loader ) ) {
		conversion->page_pyramid = TRUE;

		conversion_get_pyramid_page( conversion );

		if( conversion->level_count == 0 )
			conversion->page_pyramid = FALSE;
        }

	if( conversion->pages_same_size )
		conversion->type = CONVERSION_TYPE_TOILET_ROLL;
	else {
		if( conversion->page_pyramid )
			conversion->type = CONVERSION_TYPE_PAGE_PYRAMID;
		else
			conversion->type = CONVERSION_TYPE_MULTIPAGE;
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

        printf( "\tpages_same_size = %d\n", conversion->pages_same_size );
        printf( "\ttype = %d\n", conversion->type );
        printf( "\tmode = %d\n", conversion->mode );
        printf( "\tdelay = %p\n", conversion->delay );
        printf( "\tn_delay = %d\n", conversion->n_delay );
}
#endif /*DEBUG*/

        conversion->image_region = vips_region_new( conversion->image );

	if( conversion->delay &&
		conversion->type == CONVERSION_TYPE_TOILET_ROLL )
		mode = CONVERSION_MODE_ANIMATED;
	else
		mode = CONVERSION_MODE_MULTIPAGE;

        g_object_set( conversion, 
		"mode", mode, 
		NULL );

        /* We ref this conversion so it won't die before the
         * background load is done. The matching unref is at the end
         * of bg load.
         */
        g_object_ref( conversion );

        /* This will be set TRUE again at the end of the background
         * load. This will trigger conversion_update_display() for us.
         */
        g_object_set( conversion, 
		"loaded", FALSE, 
		NULL );

        conversion_attach_progress( conversion ); 

        g_thread_pool_push( conversion_background_load_pool, conversion, NULL );

        return( 0 );
}

/* Duplicate another conversion.
 */
int
conversion_set_conversion( Conversion *conversion, Conversion *old_conversion )
{
	/* Don't update if we're still loading.
	 */
	if( conversion->image &&
		!conversion->loaded )
		return( 0 );

        conversion_disconnect( conversion );

	/* Always set the source so we can display a filename in the header
	 * bar even if load fails.
	 */
	conversion->source = old_conversion->source; 
	g_object_ref( conversion->source );

	if( conversion_set_image( conversion, 
		old_conversion->loader, old_conversion->image ) )
		return( -1 );

	g_object_set( conversion,
		"falsecolour", old_conversion->falsecolour,
		"log", old_conversion->log,
		"mag", old_conversion->mag,
		"scale", old_conversion->scale,
		"offset", old_conversion->offset,
		NULL );

	return( 0 );
}

/* All libvips loaders which support page/n arguments. jp2k has page, but not
 * n.
 *
 * FIXME ... could use introspection to make this list.
 */
static const char *
conversion_page_formats[] = {
	"tiff",
	"pdf",
	"webp",
	"gif"
};

int
conversion_set_source( Conversion *conversion, VipsSource *source )
{
	const char *loader;
        VipsImage *image;
	int i;

	/* Don't update if we're still loading.
	 */
	if( conversion->image &&
		!conversion->loaded )
		return( 0 );

#ifdef DEBUG
        printf( "conversion_set_source: starting ..\n" );
#endif /*DEBUG*/

        conversion_disconnect( conversion );

	/* Always set the source so we can display a filename in the header
	 * bar even if load fails.
	 */
	conversion->source = source; 
	g_object_ref( source );

	if( !(loader = vips_foreign_find_load_source( source )) ) {
		/* Signal changed so eg. header bars can update.
		 */
		conversion_changed( conversion );
		return( -1 );
	}

	/* vips_foreign_find_load_source() gives us eg.
	 * "VipsForeignLoadNsgifFile", but we need "gifload_source", the
	 * generic name.
	 */
	loader = vips_nickname_find( g_type_from_name( loader ) );

	for( i = 0; i < VIPS_NUMBER( conversion_page_formats ); i++ ) {
		if( vips_isprefix( conversion_page_formats[i], loader ) ) {
			image = vips_image_new_from_source( source, "",
				"n", -1, NULL );
			break;
		}
	}
	if( i == VIPS_NUMBER( conversion_page_formats ) )
                image = vips_image_new_from_source( source, "", NULL );

	if( !image )
		return( -1 );

	if( conversion_set_image( conversion, loader, image ) ) {
		g_object_unref( image );
		return( -1 );
	}
	g_object_unref( image );

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

        if( conversion_set_source( conversion, source ) ) {
		VIPS_UNREF( source );
		return( -1 );
	}

        VIPS_UNREF( source );

        return( 0 );
}

/* Make a checkerboard background for showing transparent images.
 */
static VipsImage *
conversion_checkerboard( int width, int height )
{
	const int size = 20;
	VipsObject *context = VIPS_OBJECT( vips_image_new() );
	VipsImage **t = (VipsImage **) vips_object_local_array( context, 6 );

	VipsImage *out;

	if( !(t[0] = vips_image_new_matrixv( 2, 2, 
		128.0, 204.0, 204.0, 128.0 )) ||
		vips_cast_uchar( t[0], &t[1], NULL ) ||
		vips_zoom( t[1], &t[2], size / 2, size / 2, NULL ) ||
		vips_replicate( t[2], &t[3], 
			(width + size) / size, 
			(height + size) / size, NULL ) ||
		vips_crop( t[3], &t[4], 0, 0, width, height, NULL ) ||
		vips_copy( t[4], &t[5], 
			"interpretation", VIPS_INTERPRETATION_B_W, NULL ) ) {
		g_object_unref( context );
		return( NULL );
	}

	out = t[5];
	g_object_ref( out );
	g_object_unref( context );

	return( out );
}

/* Make the rgb image we paint with. This runs synchronously and is not
 * threaded.
 */
static VipsImage *
conversion_rgb_image( Conversion *conversion, VipsImage *in ) 
{
        VipsImage *image;
        VipsImage *x;

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

        if( conversion->falsecolour ) {
                if( vips_falsecolour( image, &x, NULL ) ) {
                        g_object_unref( image );
                        return( NULL ); 
                }
                g_object_unref( image );
                image = x;
	}

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

#ifdef DEBUG_VERBOSE
        printf( "conversion_render_notify_idle: "
                "left = %d, top = %d, width = %d, height = %d\n",
                update->rect.left, update->rect.top,
                update->rect.width, update->rect.height );
#endif /*DEBUG_VERBOSE*/

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

#ifdef DEBUG
		printf( "conversion_display_image: loading level %d\n", 
			level ); 
#endif /*DEBUG*/

		if( !(image = conversion_open( conversion, level )) )
			return( NULL );
	}
	else if( conversion->type == CONVERSION_TYPE_MULTIPAGE ) {
#ifdef DEBUG
		printf( "conversion_display_image: loading page %d\n", 
			conversion->page ); 
#endif /*DEBUG*/

		if( !(image = conversion_open( conversion, conversion->page )) )
			return( NULL );
	}
	else {
		image = conversion->image;
		g_object_ref( image ); 
	}

	/* In multipage display mode, crop out the page we want. We need to
	 * crop using the page size on image, since it might have been shrunk
	 * by shrink-on-load above ^^
	 */
	if( conversion->mode != CONVERSION_MODE_TOILET_ROLL &&
		conversion->type == CONVERSION_TYPE_TOILET_ROLL ) {
		int page_width = image->Xsize;
		int page_height = vips_image_get_page_height( image );

		VipsImage *x;

		if( vips_crop( image, &x, 
			0, conversion->page * page_height, 
			page_width, page_height, NULL ) ) {
			VIPS_UNREF( image );
			return( NULL );
		}
		VIPS_UNREF( image );
		image = x;
	}

	/* Histogram type ... plot the histogram. 
         */
        if( image->Type == VIPS_INTERPRETATION_HISTOGRAM &&
                (image->Xsize == 1 || image->Ysize == 1) ) {
		VipsImage *context = vips_image_new();
		VipsImage **t = (VipsImage **) 
			vips_object_local_array( VIPS_OBJECT( context ), 7 );

		/* So image will be unreffed when we unref context.
		 */
		t[0] = image;
		x = t[0];

                if( x->Coding == VIPS_CODING_LABQ ) {
                        if( vips_LabQ2Lab( x, &t[1], NULL ) ) {
                                VIPS_UNREF( context );
                                return( NULL );
                        }
                        x = t[1];
                }

                if( x->Coding == VIPS_CODING_RAD ) {
                        if( vips_rad2float( x, &t[2], NULL ) ) {
                                VIPS_UNREF( context );
                                return( NULL );
                        }
                        x = t[2];
                }

                if( vips_hist_norm( x, &t[3], NULL ) ||
                        vips_hist_plot( t[3], &t[4], NULL ) ) {
			VIPS_UNREF( context );
                        return( NULL );
                }
                x = t[4];

                /* Scale to a sensible size ... aim for a height of 256
                 * elements.
                if( in->Xsize == 1 && t[1]->Xsize > 256 ) {
                        if( im_subsample( t[1], t[2], t[1]->Xsize / 256, 1 ) ) {
                                im_close( out );
                                return( NULL );
                        }
                }
                else if( in->Ysize == 1 && t[1]->Ysize > 256 ) {
                        if( im_subsample( t[1], t[2], 1, t[1]->Ysize / 256 ) ) {
                                im_close( out );
                                return( NULL );
                        }
                }
                else
                        t[2] = t[1];
                 */

		image = x;
		g_object_ref( image ); 
		VIPS_UNREF( context );
        }

	if( conversion->mag < 0 ) {
		/* We may have already zoomed out a bit because we've loaded
		 * some layer other than the base one. Recalculate the
		 * subsample as (current_width / required_width).
		 */
		int subsample = image->Xsize / 
			(conversion->width / -conversion->mag);

		if( vips_subsample( image, &x, subsample, subsample, NULL ) ) {
			VIPS_UNREF( image );
			return( NULL ); 
		}
		VIPS_UNREF( image );
		image = x;
	}
	else { 
		if( vips_zoom( image, &x, 
			conversion->mag, conversion->mag, NULL ) ) {
			VIPS_UNREF( image );
			return( NULL ); 
		}
		VIPS_UNREF( image );
		image = x;
	}

        if( vips_colourspace( image, &x, VIPS_INTERPRETATION_sRGB, NULL ) ) {
                VIPS_UNREF( image );
                return( NULL ); 
        }
        VIPS_UNREF( image );
        image = x;

        /* If there's an alpha, composite over a checkerboard.
         */
        if( vips_image_hasalpha( image ) ) {
		VipsImage *bg;

		if( !(bg = conversion_checkerboard( 
			image->Xsize, image->Ysize )) ) {
			VIPS_UNREF( image );
			return( NULL );
		}

		if( vips_composite2( bg, image, &x, 
			VIPS_BLEND_MODE_OVER, NULL ) ) {
			VIPS_UNREF( bg );
			VIPS_UNREF( image );
			return( NULL );
		}
		VIPS_UNREF( bg );
		VIPS_UNREF( image );
		image = x;

		if( vips_extract_band( image, &x, 0, "n", 3, NULL ) ) {
			VIPS_UNREF( image );
			return( NULL ); 
		}
		VIPS_UNREF( image );
		image = x;
        }

        if( conversion->log ||
		image->Type == VIPS_INTERPRETATION_FOURIER ) { 
		static const double power = 0.25;
		const double scale = 255.0 / 
			log10( 1.0 + pow( 255.0, power ) );

		VipsImage *context = vips_image_new();
		VipsImage **t = (VipsImage **) 
			vips_object_local_array( VIPS_OBJECT( context ), 7 );

                if( vips_pow_const1( image, &t[0], power, NULL ) ||
                        vips_linear1( t[0], &t[1], 1.0, 1.0, NULL ) ||
                        vips_log10( t[1], &t[2], NULL ) ||
                        /* Add 0.5 to get round to nearest.
                         */
                        vips_linear1( t[2], &x, scale, 0.5, NULL ) ) {
                        g_object_unref( context );
                        return( NULL ); 
                }
		VIPS_UNREF( context );
                VIPS_UNREF( image );
                image = x;
	}

        x = vips_image_new();
        mask = vips_image_new();
        if( vips_sink_screen( image, 
                x, mask, 
                tile_size, tile_size, 400, 0, 
                conversion_render_notify, conversion ) ) {
                VIPS_UNREF( x );
                VIPS_UNREF( mask );
                VIPS_UNREF( image );
                return( NULL );
        }
        VIPS_UNREF( image );
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

/* Each timeout fires once, sets the next timeout, and flips the page.
 */
static gboolean
conversion_page_flip( void *user_data )
{
	Conversion *conversion = (Conversion *) user_data;
	int page = VIPS_CLIP( 0, conversion->page, conversion->n_pages - 1 );

	int timeout;

	/* By convention, GIFs default to 10fps.
	 */
	timeout = 100;

	if( conversion->delay ) {
		int i = VIPS_MIN( page, conversion->n_delay - 1 );

		/* By GIF convention, timeout 0 means unset.
		 */
		if( conversion->delay[i] )
			timeout = conversion->delay[i];
	}

	/* vipsdisp struggles at more than 30fps.
	 */
	timeout = VIPS_CLIP( 33, timeout, 100000 );

	conversion->page_flip_id = 
		g_timeout_add( timeout, conversion_page_flip, conversion );

#ifdef DEBUG
	printf( "conversion_page_flip: timeout %d ms\n", timeout ); 
#endif /*DEBUG*/

	g_object_set( conversion,
		"page", (page + 1) % conversion->n_pages,
		NULL );

	return( FALSE );
}

#ifdef DEBUG
static const char *
conversion_property_name( guint prop_id )
{
	switch( prop_id ) {
	case PROP_RGB:
		return( "RGB" );
		break;

	case PROP_MODE:
		return( "MODE" );
		break;

	case PROP_MAG:
		return( "MAG" );
		break;

	case PROP_SCALE:
		return( "SCALE" );
		break;

	case PROP_OFFSET:
		return( "OFFSET" );
		break;

	case PROP_PAGE:
		return( "PAGE" );
		break;

	case PROP_FALSECOLOUR:
		return( "FALSECOLOUR" );
		break;

	case PROP_LOG:
		return( "LOG" );
		break;

	case PROP_LOADED:
		return( "LOADED" );
		break;

	default:
		return( "<unknown>" );
	}
}
#endif /*DEBUG*/

static void
conversion_set_property( GObject *object, 
        guint prop_id, const GValue *value, GParamSpec *pspec )
{
        Conversion *conversion = (Conversion *) object;

        int i;
        double d;
        gboolean b;

#ifdef DEBUG
{
	char *str;

	str = g_strdup_value_contents( value );
	printf( "conversion_set_property: %s %s\n", 
		conversion_property_name( prop_id ), str ); 
	g_free( str );
}
#endif /*DEBUG*/


        switch( prop_id ) {
        case PROP_RGB:
                VIPS_UNREF( conversion->rgb ); 
                conversion->rgb = g_value_get_object( value );
                g_object_ref( conversion->rgb );
                break;

        case PROP_MODE:
                i = g_value_get_int( value );
                if( i >= 0 &&
                        i < CONVERSION_MODE_LAST &&
                        conversion->mode != i ) {
                        conversion->mode = i;
			conversion_changed( conversion );
                        conversion_update_display( conversion );

			/* In animation mode, create the page flip timeout.
			 */
			if( conversion->page_flip_id )
				VIPS_FREEF( g_source_remove, 
					conversion->page_flip_id );
			if( conversion->mode == CONVERSION_MODE_ANIMATED )
				conversion->page_flip_id = g_timeout_add( 100, 
					conversion_page_flip, conversion );
                }
                break;

        case PROP_MAG:
                i = g_value_get_int( value );
                if( i >= -600 &&
                        i <= 1000000 &&
                        conversion->mag != i ) {
                        conversion->mag = i;
			conversion_changed( conversion );
                        conversion_update_display( conversion );
                }
                break;

        case PROP_SCALE:
                d = g_value_get_double( value );
                if( d > 0 &&
                        d <= 1000000 &&
                        conversion->scale != d ) { 
                        conversion->scale = d;
			conversion_changed( conversion );
                        conversion_update_rgb( conversion );
                }
                break;

        case PROP_OFFSET:
                d = g_value_get_double( value );
                if( d >= -100000 &&
                        d <= 1000000 &&
                        conversion->offset != d ) { 
                        conversion->offset = d;
			conversion_changed( conversion );
                        conversion_update_rgb( conversion );
                }
                break;

        case PROP_PAGE:
                i = g_value_get_int( value );
                if( i >= 0 &&
                        i <= 1000000 &&
                        conversion->page != i ) {
                        conversion->page = i;
			conversion_page_changed( conversion );
                        conversion_update_display( conversion );
                }
                break;

        case PROP_FALSECOLOUR:
                b = g_value_get_boolean( value );
                if( conversion->falsecolour != b ) { 
                        conversion->falsecolour = b;
			conversion_changed( conversion );
                        conversion_update_rgb( conversion );
                }
                break;

        case PROP_LOG:
                b = g_value_get_boolean( value );
                if( conversion->log != b ) { 
                        conversion->log = b;
			conversion_changed( conversion );
			conversion_update_display( conversion );
                }
                break;

        case PROP_LOADED:
                b = g_value_get_boolean( value );
                if( conversion->loaded != b ) { 
                        conversion->loaded = b;
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
        case PROP_RGB:
                g_value_set_object( value, conversion->rgb );
                break;

        case PROP_MODE:
                g_value_set_int( value, conversion->mode );
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

        case PROP_PAGE:
                g_value_set_int( value, conversion->page );
                break;

        case PROP_FALSECOLOUR:
                g_value_set_boolean( value, conversion->falsecolour );
                break;

        case PROP_LOG:
                g_value_set_boolean( value, conversion->log );
                break;

        case PROP_LOADED:
                g_value_set_boolean( value, conversion->loaded );
                break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID( object, prop_id, pspec );
                break;
        }

#ifdef DEBUG
{
	char *str;

	str = g_strdup_value_contents( value );
	printf( "conversion_get_property: %s %s\n", 
		conversion_property_name( prop_id ), str ); 
	g_free( str );
}
#endif /*DEBUG*/
}

static void
conversion_init( Conversion *conversion )
{
        conversion->mag = 1;
        conversion->scale = 1.0;
	conversion->type = CONVERSION_TYPE_MULTIPAGE;
	conversion->mode = CONVERSION_MODE_MULTIPAGE;
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
conversion_background_load_done_idle( void *user_data )
{
        Conversion *conversion = (Conversion *) user_data;

#ifdef DEBUG
        printf( "conversion_background_load_done_cb:\n" );
#endif /*DEBUG*/

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

        g_idle_add( conversion_background_load_done_idle, conversion );

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

        g_object_class_install_property( gobject_class, PROP_RGB,
                g_param_spec_object( "rgb",
                        _( "RGB" ),
                        _( "The converted image" ),
                        VIPS_TYPE_IMAGE,
                        G_PARAM_READWRITE ) );

        g_object_class_install_property( gobject_class, PROP_MODE,
                g_param_spec_int( "mode",
                        _( "Mode" ),
                        _( "Display mode" ),
                        0, CONVERSION_MODE_LAST - 1, CONVERSION_MODE_MULTIPAGE,
                        G_PARAM_READWRITE ) );

        g_object_class_install_property( gobject_class, PROP_MAG,
                g_param_spec_int( "mag",
                        _( "Mag" ),
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

        g_object_class_install_property( gobject_class, PROP_PAGE,
                g_param_spec_int( "page",
                        _( "Page" ),
                        _( "Page number" ),
                        0, 1000000, 0,
                        G_PARAM_READWRITE ) );

        g_object_class_install_property( gobject_class, PROP_FALSECOLOUR,
                g_param_spec_boolean( "falsecolour",
                        _( "falsecolour" ),
                        _( "False colour" ),
                        FALSE,
                        G_PARAM_READWRITE ) );

        g_object_class_install_property( gobject_class, PROP_LOG,
                g_param_spec_boolean( "log",
                        _( "log" ),
                        _( "Log" ),
                        FALSE,
                        G_PARAM_READWRITE ) );

        g_object_class_install_property( gobject_class, PROP_LOADED,
                g_param_spec_boolean( "loaded",
                        _( "loaded" ),
                        _( "Image has finished loading" ),
                        FALSE,
                        G_PARAM_READWRITE ) );

        conversion_signals[SIG_PREEVAL] = g_signal_new( "preeval",
                G_TYPE_FROM_CLASS( class ),
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET( ConversionClass, preeval ), 
                NULL, NULL,
                g_cclosure_marshal_VOID__POINTER,
                G_TYPE_NONE, 1,
                G_TYPE_POINTER );

        conversion_signals[SIG_EVAL] = g_signal_new( "eval",
                G_TYPE_FROM_CLASS( class ),
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET( ConversionClass, eval ), 
                NULL, NULL,
                g_cclosure_marshal_VOID__POINTER,
                G_TYPE_NONE, 1,
                G_TYPE_POINTER );

        conversion_signals[SIG_POSTEVAL] = g_signal_new( "posteval",
                G_TYPE_FROM_CLASS( class ),
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET( ConversionClass, posteval ), 
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

        conversion_signals[SIG_PAGE_CHANGED] = g_signal_new( "page_changed",
                G_TYPE_FROM_CLASS( class ),
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET( ConversionClass, page_changed ), 
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

int
conversion_write_to_file( Conversion *conversion, const char *file )
{
	int result;

        vips_image_set_progress( conversion->image, TRUE ); 
	result = vips_image_write_to_file( conversion->image, file, NULL );

	return( result );
}

Conversion *
conversion_new( void ) 
{
        Conversion *conversion;

        conversion = g_object_new( conversion_get_type(),
                NULL );

        return( conversion ); 
}
