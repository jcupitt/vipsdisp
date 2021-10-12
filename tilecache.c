#include "vipsdisp.h"

/*
#define DEBUG
#define DEBUG_SNAPSHOT
 */

/* Keep this many non-visible tiles around as a cache. Enough to fill a 4k 
 * screen 2x over.
 */
#define TILE_KEEP (2 * (4096 / TILE_SIZE) * (2048 / TILE_SIZE))

enum {
        /* Signals. 
         */
        SIG_CHANGED,            
        SIG_TILES_CHANGED,            
        SIG_AREA_CHANGED,            

        SIG_LAST
};

static guint tile_cache_signals[SIG_LAST] = { 0 };

G_DEFINE_TYPE( TileCache, tile_cache, G_TYPE_OBJECT );

static void
tile_cache_free_pyramid( TileCache *tile_cache )
{
        int i;

        for( i = 0; i < tile_cache->n_levels; i++ ) {
                GSList *p;

                for( p = tile_cache->tiles[i]; p; p = p->next ) {
                        Tile *tile = TILE( p->data );

                        VIPS_UNREF( tile );
                }

                VIPS_FREEF( g_slist_free, tile_cache->tiles[i] );
                VIPS_FREEF( g_slist_free, tile_cache->valid[i] );
                VIPS_FREEF( g_slist_free, tile_cache->visible[i] );
                VIPS_FREEF( g_slist_free, tile_cache->free[i] );

                VIPS_UNREF( tile_cache->levels[i] );
        }

        VIPS_FREE( tile_cache->levels );
        tile_cache->n_levels = 0;

        tile_cache->z = 0;
        tile_cache->viewport.left = 0;
        tile_cache->viewport.top = 0;
        tile_cache->viewport.width = 0;
        tile_cache->viewport.height = 0;
}

static void
tile_cache_dispose( GObject *object )
{
        TileCache *tile_cache = (TileCache *) object;

#ifdef DEBUG
        printf( "tile_cache_dispose: %p\n", object );
#endif /*DEBUG*/

        tile_cache_free_pyramid( tile_cache );

        VIPS_UNREF( tile_cache->tile_source );
	VIPS_UNREF( tile_cache->checkerboard );

        G_OBJECT_CLASS( tile_cache_parent_class )->dispose( object );
}

static void
tile_cache_changed( TileCache *tile_cache )
{
        g_signal_emit( tile_cache, 
                tile_cache_signals[SIG_CHANGED], 0 );
}

static void
tile_cache_tiles_changed( TileCache *tile_cache )
{
        g_signal_emit( tile_cache, 
                tile_cache_signals[SIG_TILES_CHANGED], 0 );
}

static void
tile_cache_area_changed( TileCache *tile_cache, VipsRect *dirty, int z )
{
        g_signal_emit( tile_cache, 
                tile_cache_signals[SIG_AREA_CHANGED], 0, dirty, z );
}

static void
tile_cache_checkerboard_destroy_notify( guchar* pixels, gpointer data )
{
        g_free( pixels );
}

/* Make a GdkTexture for the checkerboard pattern we use for compositing.
 */
static GdkTexture *
tile_cache_checkerboard( void )
{
        VipsPel *data;
        GdkPixbuf *pixbuf;
        GdkTexture *texture;
        int x, y, z;

        data = g_malloc( TILE_SIZE * TILE_SIZE * 3 );
        for( y = 0; y < TILE_SIZE; y++ )
                for( x = 0; x < TILE_SIZE; x++ )
                        for( z = 0; z < 3; z++ )
                                data[y * TILE_SIZE * 3 + x * 3 + z] =
                                       ((x >> 4) + (y >> 4)) % 2 == 0 ? 
                                               128 : 180;

        pixbuf = gdk_pixbuf_new_from_data( data, GDK_COLORSPACE_RGB,
                FALSE, 8,
                TILE_SIZE, TILE_SIZE, TILE_SIZE * 3,
                tile_cache_checkerboard_destroy_notify, NULL );

        texture = gdk_texture_new_for_pixbuf( pixbuf );

        g_object_unref( pixbuf );

        return( texture );
}

static void
tile_cache_init( TileCache *tile_cache )
{
        tile_cache->checkerboard = tile_cache_checkerboard();
}

static void
tile_cache_class_init( TileCacheClass *class )
{
        GObjectClass *gobject_class = G_OBJECT_CLASS( class );

        gobject_class->dispose = tile_cache_dispose;

        tile_cache_signals[SIG_CHANGED] = g_signal_new( "changed",
                G_TYPE_FROM_CLASS( class ),
                G_SIGNAL_RUN_LAST,
                0,
                NULL, NULL,
                g_cclosure_marshal_VOID__VOID,
                G_TYPE_NONE, 0 ); 

        tile_cache_signals[SIG_TILES_CHANGED] = g_signal_new( "tiles-changed",
                G_TYPE_FROM_CLASS( class ),
                G_SIGNAL_RUN_LAST,
                0,
                NULL, NULL,
                g_cclosure_marshal_VOID__VOID,
                G_TYPE_NONE, 0 ); 

        tile_cache_signals[SIG_AREA_CHANGED] = g_signal_new( "area-changed",
                G_TYPE_FROM_CLASS( class ),
                G_SIGNAL_RUN_LAST,
                0,
                NULL, NULL,
                vipsdisp_VOID__POINTER_INT,
                G_TYPE_NONE, 2,
                G_TYPE_POINTER,
                G_TYPE_INT );

}

static void
tile_cache_build_pyramid( TileCache *tile_cache )
{
        int n_levels;
        int level_width;
        int level_height;
        int i;

#ifdef DEBUG
        printf( "tile_cache_build_pyramid:\n" ); 
#endif /*DEBUG*/

        tile_cache_free_pyramid( tile_cache );

        /* How many levels? Keep shrinking until we get down to one tile on
         * one axis.
         */
        level_width = tile_cache->tile_source->width;
        level_height = tile_cache->tile_source->height;
        n_levels = 1;
        for(;;) {
                if( level_width <= TILE_SIZE ||
                        level_height <= TILE_SIZE )
                        break;

                level_width >>= 1;
                level_height >>= 1;
                n_levels += 1;
        }

        tile_cache->n_levels = n_levels;

        tile_cache->levels = VIPS_ARRAY( NULL, n_levels, VipsImage * );
        level_width = tile_cache->tile_source->width;
        level_height = tile_cache->tile_source->height;
        for( i = 0; i < n_levels; i++ ) {
                tile_cache->levels[i] = vips_image_new();

                vips_image_init_fields( tile_cache->levels[i],
                        level_width,
                        level_height,
                        tile_cache->tile_source->rgb->Bands,
                        tile_cache->tile_source->rgb->BandFmt,
                        tile_cache->tile_source->rgb->Coding,
                        tile_cache->tile_source->rgb->Type,
                        tile_cache->tile_source->rgb->Xres,
                        tile_cache->tile_source->rgb->Yres );

                level_width >>= 1;
                level_height >>= 1;
        }

        tile_cache->tiles = VIPS_ARRAY( NULL, n_levels, GSList * );
        tile_cache->valid = VIPS_ARRAY( NULL, n_levels, GSList * );
        tile_cache->visible = VIPS_ARRAY( NULL, n_levels, GSList * );
        tile_cache->free = VIPS_ARRAY( NULL, n_levels, GSList * );

#ifdef DEBUG
        printf( "        %d pyr levels\n", n_levels );
        for( i = 0; i < n_levels; i++ ) 
                printf( "        %d) %d x %d\n", 
                        i, 
                        tile_cache->levels[i]->Xsize,
                        tile_cache->levels[i]->Ysize );
#endif /*DEBUG*/
}

/* Expand a rect out to the set of tiles it touches on this level.
 */
static void
tile_cache_tiles_for_rect( TileCache *tile_cache, 
        VipsRect *rect, VipsRect *touches )
{
        int z = tile_cache->z;
        int size0 = TILE_SIZE << z;

        int left = VIPS_ROUND_DOWN( rect->left, size0 );
        int top = VIPS_ROUND_DOWN( rect->top, size0 );
        int right = VIPS_ROUND_UP( VIPS_RECT_RIGHT( rect ), size0 );
        int bottom = VIPS_ROUND_UP( VIPS_RECT_BOTTOM( rect ), size0 );

        touches->left = left;
        touches->top = top;
        touches->width = right - left;
        touches->height = bottom - top;

        /* We can have rects outside the image. Make sure they stay empty.
         */
        if( vips_rect_isempty( rect ) ) {
                touches->width = 0;
                touches->height = 0;
        }
}

/* Find the first visible tile in a hole.
 */
static void
tile_cache_fill_hole( TileCache *tile_cache, VipsRect *bounds, int z )
{
        int i;

        for( i = z; i < tile_cache->n_levels; i++ ) {
                GSList *p;

                for( p = tile_cache->valid[i]; p; p = p->next ) {
                        Tile *tile = TILE( p->data );

                        if( vips_rect_overlapsrect( &tile->bounds, bounds ) ) {
                                tile_touch( tile );
                                tile_cache->visible[z] = 
                                        g_slist_prepend( tile_cache->visible[z],
                                                tile );
                                return;
                        }
                }
        }
}

static void
tile_cache_free_tile( TileCache *tile_cache, Tile *tile )
{
        int z = tile->z;

        g_assert( g_slist_find( tile_cache->tiles[z], tile ) );

        tile_cache->tiles[z] = g_slist_remove( tile_cache->tiles[z], tile );
        tile_cache->valid[z] = g_slist_remove( tile_cache->valid[z], tile );
        VIPS_UNREF( tile );
}

static int
tile_cache_sort_lru( const void *a, const void *b )
{
        Tile *t1 = TILE( a );
        Tile *t2 = TILE( b );

        return( t1->time - t2->time );
}

static void
tile_cache_free_oldest( TileCache *tile_cache, int z )
{
        int n_free = g_slist_length( tile_cache->free[z] );

        if( n_free > TILE_KEEP ) {
                int i;

                tile_cache->free[z] = g_slist_sort( tile_cache->free[z], 
                        tile_cache_sort_lru );

                for( i = 0; i < n_free - TILE_KEEP; i++ ) {
                        Tile *tile = TILE( tile_cache->free[z]->data );

                        tile_cache_free_tile( tile_cache, tile );
                        tile_cache->free[z] = 
                                g_slist_remove( tile_cache->free[z], tile );
                }
        }
}

static void
tile_cache_compute_visibility( TileCache *tile_cache )
{
        int z = tile_cache->z;
        int size0 = TILE_SIZE << z;
        int start_time = tile_get_time();

        int i;
        VipsRect touches;
        int x, y;
        VipsRect bounds;
        GSList *p;

#ifdef DEBUG
        printf( "tile_cache_compute_visibility:\n" ); 
#endif /*DEBUG*/

        /* We're rebuilding these.
         */
        for( i = 0; i < tile_cache->n_levels; i++ ) {
                VIPS_FREEF( g_slist_free, tile_cache->visible[i] );
                VIPS_FREEF( g_slist_free, tile_cache->free[i] );
        }

        /* The rect of tiles touched by the viewport.
         */
        tile_cache_tiles_for_rect( tile_cache, 
                &tile_cache->viewport, &touches );

        /* Search for the highest res tile for every position in the 
         * viewport.
         */
        bounds.width = size0;
        bounds.height = size0;
        for( y = 0; y < touches.height; y += size0 ) 
                for( x = 0; x < touches.width; x += size0 ) {
                        bounds.left = x + touches.left;
                        bounds.top = y + touches.top;

                        tile_cache_fill_hole( tile_cache, &bounds, z );
                }

        /* So any tiles we've not touched must be invisible and therefore 
         * candidates for freeing.
         */
        for( i = 0; i < tile_cache->n_levels; i++ ) {
                for( p = tile_cache->tiles[i]; p; p = p->next ) {
                        Tile *tile = TILE( p->data );

                        if( tile->time < start_time ) 
                                tile_cache->free[i] = 
                                        g_slist_prepend( tile_cache->free[i], 
                                                tile );
                }
        }

        /* Free the oldest few unused tiles in each level. 
         *
         * Never free tiles in the lowest-res few levels. They are useful for 
         * filling in holes and take little memory.
         */
        for( i = 0; i < tile_cache->n_levels - 3; i++ ) 
                tile_cache_free_oldest( tile_cache, i );

#ifdef DEBUG

        for( i = 0; i < tile_cache->n_levels; i++ ) {
                printf( "  level %d, %d tiles, %d valid, %d visible, %d free\n",
                        i, 
                        g_slist_length( tile_cache->tiles[i] ),
                        g_slist_length( tile_cache->valid[i] ),
                        g_slist_length( tile_cache->visible[i] ),
                        g_slist_length( tile_cache->free[i] ) );
        }

        for( i = 0; i < tile_cache->n_levels; i++ ) {
                printf( "  level %d tiles:\n", i ); 
                for( p = tile_cache->tiles[i]; p; p = p -> next ) {
                        Tile *tile = TILE( p->data );

                        printf( "    @ %d x %d, %d x %d, valid = %d\n",
                                tile->bounds.left,
                                tile->bounds.top,
                                tile->bounds.width,
                                tile->bounds.height,
                                tile->valid );
                }

                printf( "  level %d visible:\n", i ); 
                for( p = tile_cache->visible[i]; p; p = p -> next ) {
                        Tile *tile = TILE( p->data );

                        printf( "    @ %d x %d, %d x %d, valid = %d\n",
                                tile->bounds.left,
                                tile->bounds.top,
                                tile->bounds.width,
                                tile->bounds.height,
                                tile->valid );
                }

        }

#endif /*DEBUG*/
}

static Tile *
tile_cache_find( TileCache *tile_cache, VipsRect *bounds, int z )
{
        GSList *p;
        Tile *tile;

        for( p = tile_cache->tiles[z]; p; p = p->next ) {
                tile = TILE( p->data );

                if( vips_rect_overlapsrect( &tile->bounds, bounds ) ) 
                        return( tile );
        }

        return( NULL );
}

/* Fetch a single tile.
 */
static void
tile_cache_get( TileCache *tile_cache, VipsRect *bounds )
{
        int z = tile_cache->z;

        Tile *tile;

#ifdef DEBUG
        printf( "tile_cache_get: left = %d, top = %d, "
                "width = %d, height = %d\n", 
                bounds->left, bounds->top,
                bounds->width, bounds->height );
#endif /*DEBUG*/

        /* Look for an existing tile, or make a new one.
         */
        if( !(tile = tile_cache_find( tile_cache, bounds, z )) ) {
                if( !(tile = tile_new( tile_cache->levels[z], 
                        bounds->left >> z, bounds->top >> z, z )) )
                        return;

                tile_cache->tiles[z] = 
                        g_slist_prepend( tile_cache->tiles[z], tile );
        }

        if( !tile->valid ) {
                tile_source_fill_tile( tile_cache->tile_source, tile );

                /* It might now be valid, if pixels have come
                 * in from the pipeline.
                 */
                if( tile->valid ) 
                        tile_cache->valid[z] = 
                                g_slist_prepend( tile_cache->valid[z], tile );
        }
}

/* Fetch the tiles in an area.
 *
 * render processes tiles in FIFO order, so we need to add in reverse order
 * of processing. We want repaint to happen in a spiral from the centre out,
 * so we have to add in a spiral from the outside in.
 */
static void
tile_cache_fetch_area( TileCache *tile_cache, VipsRect *rect )
{
        int z = tile_cache->z;
        int size0 = TILE_SIZE << z;

        /* All the tiles rect touches in this pyr level.
         */
        int left = VIPS_ROUND_DOWN( rect->left, size0 );
        int top = VIPS_ROUND_DOWN( rect->top, size0 );
        int right = VIPS_ROUND_UP( VIPS_RECT_RIGHT( rect ), size0 );
        int bottom = VIPS_ROUND_UP( VIPS_RECT_BOTTOM( rect ), size0 );

        /* Do the four edges, then step in. Loop until the centre is empty.
         */
        for(;;) {
                VipsRect tile;
                int x, y;

                tile.width = size0;
                tile.height = size0;

                if( right - left <= 0 ||
                        bottom - top <= 0 )
                        break;

                /* Top edge.
                 */
                for( x = left; x < right; x += size0 ) {
                        tile.left = x;
                        tile.top = top;
                        tile_cache_get( tile_cache, &tile );
                }

                top += size0;
                if( right - left <= 0 ||
                        bottom - top <= 0 )
                        break;

                /* Bottom edge.
                 */
                for( x = left; x < right; x += size0 ) {
                        tile.left = x;
                        tile.top = bottom - size0;
                        tile_cache_get( tile_cache, &tile );
                }

                bottom -= size0;
                if( right - left <= 0 ||
                        bottom - top <= 0 )
                        break;

                /* Left edge.
                 */
                for( y = top; y < bottom; y += size0 ) {
                        tile.left = left;
                        tile.top = y;
                        tile_cache_get( tile_cache, &tile );
                }

                left += size0;
                if( right - left <= 0 ||
                        bottom - top <= 0 )
                        break;

                /* Right edge.
                 */
                for( y = top; y < bottom; y += size0 ) {
                        tile.left = right - size0;
                        tile.top = y;
                        tile_cache_get( tile_cache, &tile );
                }

                right -= size0;
                if( right - left <= 0 ||
                        bottom - top <= 0 )
                        break;
        }
}

/* Set the layer and the rect within that layer that we want to display.
 * viewport in level0 coordinates.
 */
void 
tile_cache_set_viewport( TileCache *tile_cache, VipsRect *viewport, int z )
{
        int old_z = tile_cache->z;

        VipsRect old_touches;
        VipsRect touches;

#ifdef DEBUG
        printf( "tile_cache_set_viewport: left = %d, top = %d, "
                "width = %d, height = %d, z = %d\n", 
                viewport->left, viewport->top,
                viewport->width, viewport->height, 
                z );
#endif /*DEBUG*/

        /* The pyramid may not have loaded hyet.
         */
        if( !tile_cache->levels )
                return;

        g_assert( z >= 0 && z < tile_cache->n_levels );

        /* The rect of tiles touched by the current viewport.
         */
        tile_cache_tiles_for_rect( tile_cache, 
                &tile_cache->viewport, &old_touches );

        /* Save viewport in level0 coordinates.
         */
        tile_cache->viewport = *viewport;
        tile_cache->z = z;

        /* The rect of tiles touched by the new viewport.
         */
        tile_cache_tiles_for_rect( tile_cache, 
                &tile_cache->viewport, &touches );

        /* Has z changed, or has the set of tiles in the viewport changed?
         */
        if( z != old_z ||
                !vips_rect_equalsrect( &old_touches, &touches ) ) {
                tile_cache_fetch_area( tile_cache, &touches );
                tile_cache_compute_visibility( tile_cache );
        }
}

static void
tile_cache_source_changed( TileSource *tile_source, TileCache *tile_cache )
{
        VipsRect old_viewport = tile_cache->viewport;
        int old_z = tile_cache->z;

#ifdef DEBUG
        printf( "tile_cache_source_changed:\n" );
#endif /*DEBUG*/

        tile_cache_build_pyramid( tile_cache );

        tile_cache_set_viewport( tile_cache, &old_viewport, old_z );

        /* Tell our clients.
         */
        tile_cache_changed( tile_cache );
}

/* All tiles need refetching, perhaps after eg. "falsecolour" etc. Mark 
 * all tiles invalid and refetch the viewport.
 */
static void
tile_cache_source_tiles_changed( TileSource *tile_source, 
        TileCache *tile_cache )
{
        int i;

#ifdef DEBUG
        printf( "tile_cache_source_tiles_changed:\n" );
#endif /*DEBUG*/

        for( i = 0; i < tile_cache->n_levels; i++ ) {
                GSList *p;

                for( p = tile_cache->valid[i]; p; p = p->next ) {
                        Tile *tile = TILE( p->data );

                        tile->valid = FALSE;
                }

                VIPS_FREEF( g_slist_free, tile_cache->valid[i] );
        }

        tile_cache_fetch_area( tile_cache, &tile_cache->viewport );

        tile_cache_compute_visibility( tile_cache );

        tile_cache_tiles_changed( tile_cache );
}

/* Some tiles have been computed. Fetch any invalid tiles in this rect.
 */
static void
tile_cache_source_area_changed( TileSource *tile_source, 
        VipsRect *area, int z, TileCache *tile_cache )
{
        VipsRect bounds;

#ifdef DEBUG
        printf( "tile_cache_source_area_changed: left = %d, top = %d, "
                "width = %d, height = %d, z = %d\n", 
                area->left, area->top,
                area->width, area->height, z );
#endif /*DEBUG*/

        bounds.left = area->left << z;
        bounds.top = area->top << z;
        bounds.width = area->width << z;
        bounds.height = area->height << z;
        tile_cache_fetch_area( tile_cache, &bounds );

        tile_cache_compute_visibility( tile_cache );

        /* Tell our clients.
         */
        tile_cache_area_changed( tile_cache, area, z );
}

TileCache *
tile_cache_new( TileSource *tile_source )
{
        TileCache *tile_cache = g_object_new( TILE_CACHE_TYPE, NULL );

        tile_cache->tile_source = tile_source;
        g_object_ref( tile_source );

        /* Don't build the pyramid yet -- the source probably hasn't loaded.
         * Wait for "changed".
         */

        g_signal_connect_object( tile_cache->tile_source, "changed", 
                G_CALLBACK( tile_cache_source_changed ), tile_cache, 0 );
        g_signal_connect_object( tile_cache->tile_source, "tiles_changed", 
                G_CALLBACK( tile_cache_source_tiles_changed ), tile_cache, 0 );
        g_signal_connect_object( tile_cache->tile_source, "area_changed", 
                G_CALLBACK( tile_cache_source_area_changed ), tile_cache, 0 );

        return( tile_cache ); 
}

/* The tile_cale->viewport is the rect of tiles we have prepared for viewing.
 * The x, y, scale here is how we'd like them translated and scaled for the
 * screen.
 */
void 
tile_cache_snapshot( TileCache *tile_cache, GtkSnapshot *snapshot, 
        double x, double y, double scale )
{
        int i;

#ifdef DEBUG_SNAPSHOT
        float width = tile_cache->viewport.width * scale;
        float height = tile_cache->viewport.height * scale;
        float size0 = TILE_SIZE * scale;

        float hscale = (float) (width - 2 * size0) / width;
        float vscale = (float) (height - 2 * size0) / height;
        float mscale = VIPS_MIN( hscale, vscale );

        graphene_point_t offset;

        offset.x = (width - width * mscale) / 2;
        offset.y = (height - height * mscale) / 2;
        gtk_snapshot_translate( snapshot, &offset );

        /* Scale down to show render edges
         */
        gtk_snapshot_scale( snapshot, mscale, mscale );
#endif /*DEBUG_SNAPSHOT*/

#ifdef DEBUG
        printf( "tile_cache_snapshot: x = %g, y = %g, scale = %g\n",
                x, y, scale );
#endif /*DEBUG*/

        /* If there's an alpha, we'll need a backdrop.
         */
        if( tile_cache->tile_source->rgb->Bands == 4 ) {
                graphene_rect_t bounds;

#ifdef DEBUG
                printf( "tile_cache_snapshot: drawing checkerboard\n" );
#endif /*DEBUG*/

                bounds.origin.x = tile_cache->viewport.left * scale - x;  
                bounds.origin.y = tile_cache->viewport.top * scale - y;  
                bounds.size.width = tile_cache->viewport.width * scale;  
                bounds.size.height = tile_cache->viewport.height * scale;
                gtk_snapshot_push_repeat( snapshot, &bounds, NULL );

                bounds.origin.x = 0;
                bounds.origin.y = 0;
                bounds.size.width = TILE_SIZE;
                bounds.size.height = TILE_SIZE;
                gtk_snapshot_append_texture( snapshot, 
                        tile_cache->checkerboard,
                        &bounds );

                gtk_snapshot_pop( snapshot );
        }

        for( i = tile_cache->n_levels - 1; i >= tile_cache->z; i-- ) { 
                GSList *p;

                for( p = tile_cache->visible[i]; p; p = p->next ) {
                        Tile *tile = TILE( p->data );

                        graphene_rect_t bounds;

                        bounds.origin.x = tile->bounds.left * scale - x;  
                        bounds.origin.y = tile->bounds.top * scale - y;  
                        bounds.size.width = tile->bounds.width * scale;  
                        bounds.size.height = tile->bounds.height * scale;

                        gtk_snapshot_append_texture( snapshot, 
                                 tile_get_texture( tile ), 
                                 &bounds );

#ifdef DEBUG_SNAPSHOT
{
                        GskRoundedRect outline = GSK_ROUNDED_RECT_INIT( 
                                tile->bounds.left * scale - x,
                                tile->bounds.top * scale - y,
                                tile->bounds.width * scale,
                                tile->bounds.height * scale );
                        float border_width[4] = { 2, 2, 2, 2 };
                        GdkRGBA border_colour[4] = { 
                                { 0, 1, 0, 1 },
                                { 0, 1, 0, 1 },
                                { 0, 1, 0, 1 },
                                { 0, 1, 0, 1 },
                        };

                        cairo_t *cr;
                        char str[256];
                        VipsBuf buf = VIPS_BUF_STATIC( str );

                        gtk_snapshot_append_border( snapshot, 
                                &outline, border_width, border_colour );

                        cr = gtk_snapshot_append_cairo( snapshot, &bounds );

                        cairo_set_source_rgb( cr, 0, 1, 0 );
                        cairo_set_font_size( cr, 12 );

                        cairo_move_to( cr,
                                bounds.origin.x + 0.1 * bounds.size.width,
                                bounds.origin.y + 0.1 * bounds.size.height );
                        vips_buf_appendf( &buf, "%p", tile );
                        cairo_show_text( cr,vips_buf_all( &buf ) );

                        cairo_move_to( cr, 
                                bounds.origin.x + 0.1 * bounds.size.width,
                                bounds.origin.y + 0.2 * bounds.size.height );
                        vips_buf_rewind( &buf );
                        vips_buf_appendf( &buf, "%d", tile->time );
                        cairo_show_text( cr,vips_buf_all( &buf ) );

                        cairo_destroy( cr );
}
#endif /*DEBUG_SNAPSHOT*/
                }
        }

#ifdef DEBUG_SNAPSHOT
{
        #define BORDER ((GdkRGBA) { 1, 0, 0, 1 })

        GskRoundedRect outline;

        gsk_rounded_rect_init_from_rect( &outline, 
                &GRAPHENE_RECT_INIT(
                        tile_cache->viewport.left * scale - x,
                        tile_cache->viewport.top * scale - y,
                        tile_cache->viewport.width * scale,
                        tile_cache->viewport.height * scale
                ), 
                0 );

        gtk_snapshot_append_border( snapshot, 
                &outline, 
                (float[4]) { 2, 2, 2, 2 },
                (GdkRGBA [4]) { BORDER, BORDER, BORDER, BORDER } );

}
#endif /*DEBUG_SNAPSHOT*/
}
