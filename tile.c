#include "vipsdisp.h"

/*
#define DEBUG_VERBOSE
#define DEBUG
 */

/* Increment this regularly.
 */
static int tile_ticks = 0;

G_DEFINE_TYPE( Tile, tile, G_TYPE_OBJECT );

static void
tile_dispose( GObject *object )
{
        Tile *tile = (Tile *) object;

#ifdef DEBUG
        printf( "tile_dispose: %p\n", object );
#endif /*DEBUG*/

        VIPS_UNREF( tile->texture );
        VIPS_UNREF( tile->pixbuf );
        VIPS_UNREF( tile->region );

        G_OBJECT_CLASS( tile_parent_class )->dispose( object );
}

static void
tile_init( Tile *tile )
{
        tile->time = tile_ticks++;
}

static void
tile_class_init( TileClass *class )
{
        GObjectClass *gobject_class = G_OBJECT_CLASS( class );

        gobject_class->dispose = tile_dispose;

}

/* Get the current time ... handy for mark-sweep.
 */
int
tile_get_time( void )
{
        return( tile_ticks );
}

/* Update the timestamp on a tile.
 */
void
tile_touch( Tile *tile )
{
        tile->time = tile_ticks++;
}

/* Make a tile on an image. left/top in this image's coordinates (not level0
 * coordfinates).
 */
Tile *
tile_new( VipsImage *level, int left, int top, int z )
{
        Tile *tile = g_object_new( TYPE_TILE, NULL );

        VipsRect tile_bounds;
        VipsRect image_bounds;

        tile->region = vips_region_new( level );
        tile->z = z;

        image_bounds.left = 0;
        image_bounds.top = 0;
        image_bounds.width = level->Xsize;
        image_bounds.height = level->Ysize;
        tile_bounds.left = left;
        tile_bounds.top = top;
        tile_bounds.width = TILE_SIZE;
        tile_bounds.height = TILE_SIZE;
        vips_rect_intersectrect( &image_bounds, &tile_bounds, &tile_bounds );
        if( vips_region_buffer( tile->region, &tile_bounds ) ) {
                VIPS_UNREF( tile );
                return( NULL );
        }

        /* Tile bounds in level 0 coordinates.
         */
        tile->bounds.left = tile_bounds.left << z;
        tile->bounds.top = tile_bounds.top << z;
        tile->bounds.width = tile_bounds.width << z;
        tile->bounds.height = tile_bounds.height << z;

        tile_touch( tile );

        return( tile ); 
}

/* NULL means pixels have not arrived from libvips yet.
 */
GdkTexture *
tile_get_texture( Tile *tile )
{
        tile_touch( tile );

        if( !tile->valid )
                return( NULL );

        /* We have to make textures via pixbuf (!?!!?).
         */
        if( !tile->pixbuf ) 
                tile->pixbuf = gdk_pixbuf_new_from_data( 
                        VIPS_REGION_ADDR( tile->region, 0, 0 ),
                        GDK_COLORSPACE_RGB,
                        tile->region->im->Bands == 4,
                        8,
                        tile->region->valid.width,
                        tile->region->valid.height,
                        VIPS_REGION_LSKIP( tile->region ),
                        NULL, NULL );
        if( !tile->texture )
                tile->texture = gdk_texture_new_for_pixbuf( tile->pixbuf );

        return( tile->texture );
}

/* The pixels in the region have changed. We must regenerate the texture on
 * next use.
 */
void
tile_free_texture( Tile *tile )
{
        tile_touch( tile );

        VIPS_UNREF( tile->texture );
        VIPS_UNREF( tile->pixbuf );
}
