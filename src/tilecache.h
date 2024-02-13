#ifndef __TILE_CACHE_H
#define __TILE_CACHE_H

/* The background modes we support.
 */
typedef enum _TileCacheBackground {
	TILE_CACHE_BACKGROUND_CHECKERBOARD,
	TILE_CACHE_BACKGROUND_WHITE,
	TILE_CACHE_BACKGROUND_BLACK,
	TILE_CACHE_BACKGROUND_LAST
} TileCacheBackground;

#define TILE_CACHE_TYPE (tile_cache_get_type())
#define TILE_CACHE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_TILE_CACHE, TileCache))
#define TILE_CACHE_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), TYPE_TILE_CACHE, TileCacheClass))
#define IS_TILE_CACHE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), TYPE_TILE_CACHE))
#define IS_TILE_CACHE_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), TYPE_TILE_CACHE))
#define TILE_CACHE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), TYPE_TILE_CACHE, TileCacheClass))

typedef struct _TileCache {
	GObject parent_instance;

	/* Background rendering style.
	 */
	TileCacheBackground background;

	/* Fetch tiles from here.
	 */
	TileSource *tile_source;

	/* The levels of the pyramid, indexed by z. 0 is the full res image.
	 * These are RGB or RGBA images, filled by tile_source.
	 */
	VipsImage **levels;
	int n_levels;

	/* For each level, a list of all the tiles on that level. This is the
	 * list that holds the tile references.
	 */
	GSList **tiles;

	/* The result of the visibility test: for each level, the list of
	 * valid tiles which touch the viewport and which are not
	 * obscured.
	 */
	GSList **visible;

	/* For each level, the valid but invisible tiles we are
	 * keeping for now on LRU.
	 */
	GSList **free;

	/* Paint the backdrop with this.
	 */
	GdkTexture *background_texture;

} TileCache;

typedef struct _TileCacheClass {
	GObjectClass parent_class;

} TileCacheClass;

G_DEFINE_AUTOPTR_CLEANUP_FUNC(TileCache, g_object_unref)

GType tile_cache_get_type(void);

TileCache *tile_cache_new(TileSource *tile_source);

/* Render the tiles to a snapshot.
 */
void tile_cache_snapshot(TileCache *tile_cache, GtkSnapshot *snapshot,
	double scale, double x, double y,
	VipsRect *paint_rect,
	gboolean debug);

#endif /*__TILE_CACHE_H*/
