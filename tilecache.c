#include "vipsdisp.h"

/*
 */
#define DEBUG

/* Keep this many non-visible tiles around as a cache.
 */
#define TILE_KEEP (100)

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

        G_OBJECT_CLASS( tile_cache_parent_class )->dispose( object );
}

static void
tile_cache_init( TileCache *tile_cache )
{
}

static void
tile_cache_class_init( TileCacheClass *class )
{
        GObjectClass *gobject_class = G_OBJECT_CLASS( class );

        gobject_class->dispose = tile_cache_dispose;

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

        /* All the tiles in this viewport.
         */
        int left = VIPS_ROUND_DOWN( rect->left, size0 );
        int top = VIPS_ROUND_DOWN( rect->top, size0 );
        int right = VIPS_ROUND_UP( VIPS_RECT_RIGHT( rect ), size0 );
        int bottom = VIPS_ROUND_UP( VIPS_RECT_BOTTOM( rect ), size0 );

        touches->left = left;
        touches->top = top;
        touches->width = right - left;
        touches->height = bottom - top;
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

        /* Search for the first visible tile for every position in the 
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

        /* So any tiles we've not touched must be candidates for freeing.
         */
        for( i = 0; i < tile_cache->n_levels; i++ ) {
                for( p = tile_cache->tiles[i]; p; p = p->next ) {
                        Tile *tile = TILE( p->data );

                        if( tile->time < start_time ) 
                                tile_cache->free[z] = 
                                        g_slist_prepend( tile_cache->free[z], 
                                                tile );
                }
        }

        /* Keep recent tiles in the lowest-res level that has any tiles, and
         * in the current level. Free unused tiles everywhere else.
         */
        for( i = 0; i < z; i++ ) 
                if( tile_cache->free[i] ) {
                        tile_cache_free_oldest( tile_cache, i );
                        break;
                }
        for( ; i < z; i++ ) {
                for( p = tile_cache->free[i]; p; p = p->next ) {
                        Tile *tile = TILE( p->data );

                        tile_cache_free_tile( tile_cache, tile );
                }
                
                VIPS_FREEF( g_slist_free, tile_cache->free[i] );
        }
        tile_cache_free_oldest( tile_cache, z );

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

static void
tile_cache_fetch_area( TileCache *tile_cache, VipsRect *area )
{
        int z = tile_cache->z;
        int size0 = TILE_SIZE << z;

	int x, y;
	VipsRect bounds;

	bounds.width = size0;
	bounds.height = size0;

	for( y = 0; y < area->height; y += size0 )
		for( x = 0; x < area->width; x += size0 ) {
			bounds.left = x + area->left;
			bounds.top = y + area->top;

			tile_cache_get( tile_cache, &bounds );
		}
}

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
                viewport->width, viewport->height, z );
#endif /*DEBUG*/

        /* The rect of tiles touched by the current viewport.
         */
        tile_cache_tiles_for_rect( tile_cache, 
                &tile_cache->viewport, &old_touches );

        /* Save viewport in level0 coordinates.
         */
        tile_cache->viewport.left = viewport->left << z;
        tile_cache->viewport.top = viewport->top << z;
        tile_cache->viewport.width = viewport->width << z;
        tile_cache->viewport.height = viewport->height << z;
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
tile_cache_changed( TileSource *tile_source, TileCache *tile_cache )
{
        VipsRect old_viewport = tile_cache->viewport;
        int old_z = tile_cache->z;

#ifdef DEBUG
        printf( "tile_cache_changed:\n" );
#endif /*DEBUG*/

        tile_cache_build_pyramid( tile_cache );
        tile_cache_set_viewport( tile_cache, &old_viewport, old_z );
}

/* All tiles need refetching, perhaps after eg. "falsecolour" etc. Mark 
 * all tiles invalid and refetch the viewport.
 */
static void
tile_cache_tiles_changed( TileSource *tile_source, TileCache *tile_cache )
{
	int i;

#ifdef DEBUG
        printf( "tile_cache_tiles_changed:\n" );
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
}

/* Some tiles have been computed. Fetch any invalid tiles in this rect.
 */
static void
tile_cache_area_changed( TileSource *tile_source, 
        VipsRect *area, int z, TileCache *tile_cache )
{
        VipsRect bounds;

#ifdef DEBUG
        printf( "tile_cache_area_changed: left = %d, top = %d, "
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
                G_CALLBACK( tile_cache_changed ), tile_cache, 0 );
        g_signal_connect_object( tile_cache->tile_source, "tiles_changed", 
                G_CALLBACK( tile_cache_tiles_changed ), tile_cache, 0 );
        g_signal_connect_object( tile_cache->tile_source, "area_changed", 
                G_CALLBACK( tile_cache_area_changed ), tile_cache, 0 );

        return( tile_cache ); 
}

void 
tile_cache_snapshot( TileCache *tile_cache, GtkSnapshot *snapshot )
{
        int i;

#ifdef DEBUG
        printf( "tile_cache_snapshot:\n" );
#endif /*DEBUG*/

        for( i = tile_cache->n_levels - 1; i >= tile_cache->z; i-- ) { 
                int scale = i - tile_cache->z;

                GSList *p;

                for( p = tile_cache->visible[i]; p; p = p->next ) {
                        Tile *tile = TILE( p->data );

                        graphene_rect_t snapshot_bounds;

                        snapshot_bounds.origin.x = 
                                (tile->bounds.left - 
                                 tile_cache->viewport.left) << scale;
                        snapshot_bounds.origin.y = 
                                (tile->bounds.top - 
                                 tile_cache->viewport.top) << scale;
                        snapshot_bounds.size.width = 
                                tile->bounds.width << scale;
                        snapshot_bounds.size.height = 
                                tile->bounds.height << scale;

                        gtk_snapshot_append_texture( snapshot, 
                                 tile_get_texture( tile ), 
                                 &snapshot_bounds );
                }
        }
}
