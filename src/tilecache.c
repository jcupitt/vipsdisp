/* a sparse pyramid of tiles
 */

/*

	Copyright (C) 1991-2003 The National Gallery

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License along
	with this program; if not, write to the Free Software Foundation, Inc.,
	51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

 */

/*

	These files are distributed with VIPS - http://www.vips.ecs.soton.ac.uk

 */

#include "vipsdisp.h"

/*
#define DEBUG_RENDER_TIME
#define DEBUG_VERBOSE
#define DEBUG
 */

enum {
	/* Properties.
	 */
	PROP_BACKGROUND = 1,
	PROP_TILESOURCE,

	/* Signals.
	 */
	SIG_CHANGED,
	SIG_TILES_CHANGED,
	SIG_AREA_CHANGED,

	SIG_LAST
};

static guint tilecache_signals[SIG_LAST] = { 0 };

G_DEFINE_TYPE(Tilecache, tilecache, G_TYPE_OBJECT);

static void
tilecache_free_pyramid(Tilecache *tilecache)
{
	int i;

	for (i = 0; i < tilecache->n_levels; i++) {
		GSList *p;

		for (p = tilecache->tiles[i]; p; p = p->next) {
			Tile *tile = TILE(p->data);

			VIPS_UNREF(tile);
		}

		VIPS_FREEF(g_slist_free, tilecache->tiles[i]);
		VIPS_FREEF(g_slist_free, tilecache->visible[i]);
		VIPS_FREEF(g_slist_free, tilecache->free[i]);

		VIPS_UNREF(tilecache->levels[i]);
	}

	VIPS_FREE(tilecache->levels);
	VIPS_FREE(tilecache->tiles);
	VIPS_FREE(tilecache->visible);
	VIPS_FREE(tilecache->free);

	tilecache->n_levels = 0;
}

static void
tilecache_dispose(GObject *object)
{
	Tilecache *tilecache = (Tilecache *) object;

#ifdef DEBUG
	printf("tilecache_dispose: %p\n", object);
#endif /*DEBUG*/

	FREESID(tilecache->tilesource_changed_sid, tilecache->tilesource);
	FREESID(tilecache->tilesource_loaded_sid, tilecache->tilesource);
	FREESID(tilecache->tilesource_tiles_changed_sid, tilecache->tilesource);
	FREESID(tilecache->tilesource_collect_sid, tilecache->tilesource);
	VIPS_UNREF(tilecache->tilesource);
	VIPS_UNREF(tilecache->background_texture);

	tilecache_free_pyramid(tilecache);

	G_OBJECT_CLASS(tilecache_parent_class)->dispose(object);
}

static void
tilecache_changed(Tilecache *tilecache)
{
	g_signal_emit(tilecache, tilecache_signals[SIG_CHANGED], 0);
}

static void
tilecache_tiles_changed(Tilecache *tilecache)
{
	g_signal_emit(tilecache, tilecache_signals[SIG_TILES_CHANGED], 0);
}

static void
tilecache_area_changed(Tilecache *tilecache, VipsRect *dirty, int z)
{
	g_signal_emit(tilecache, tilecache_signals[SIG_AREA_CHANGED], 0, dirty, z);
}

static void
tilecache_checkerboard_destroy_notify(guchar *pixels, gpointer data)
{
	g_free(pixels);
}

/* Make a GdkTexture for the checkerboard pattern we use for compositing.
 */
static GdkTexture *
tilecache_texture(TilecacheBackground background)
{
	int x, y, z;

	VipsPel *data = g_malloc(TILE_SIZE * TILE_SIZE * 3);
	for (y = 0; y < TILE_SIZE; y++)
		for (x = 0; x < TILE_SIZE; x++)
			for (z = 0; z < 3; z++) {
				int v;

				switch (background) {
				case TILECACHE_BACKGROUND_CHECKERBOARD:
					v = ((x >> 4) + (y >> 4)) % 2 == 0 ? 128 : 180;
					break;

				case TILECACHE_BACKGROUND_WHITE:
					v = 255;
					break;

				case TILECACHE_BACKGROUND_BLACK:
				default:
					v = 0;
					break;
				}

				data[y * TILE_SIZE * 3 + x * 3 + z] = v;
			}

	g_autoptr(GdkPixbuf) pixbuf = gdk_pixbuf_new_from_data(data,
		GDK_COLORSPACE_RGB,
		FALSE, 8,
		TILE_SIZE, TILE_SIZE, TILE_SIZE * 3,
		tilecache_checkerboard_destroy_notify, NULL);

	return gdk_texture_new_for_pixbuf(pixbuf);
}

static void
tilecache_init(Tilecache *tilecache)
{
#ifdef DEBUG
	printf("tilecache_init:\n");
#endif /*DEBUG*/

	tilecache->background = TILECACHE_BACKGROUND_CHECKERBOARD;
	tilecache->background_texture = tilecache_texture(tilecache->background);
}

static void
tilecache_build_pyramid(Tilecache *tilecache)
{
	Tilesource *tilesource = tilecache->tilesource;

	int n_levels;
	int level_width;
	int level_height;

#ifdef DEBUG
	printf("tilecache_build_pyramid:\n");
#endif /*DEBUG*/

	/* No image? Nothing to do.
	 */
	if (!tilesource ||
		!tilesource->rgb)
		return;

	/* If we have a pyr with the same geometry, there's no need to rebuild.
	 */
	if (tilecache->levels &&
		tilecache->levels[0] &&
		tilesource->image_width == tilecache->levels[0]->Xsize &&
		tilesource->image_height == tilecache->levels[0]->Ysize) {
#ifdef DEBUG
		printf("\tno geometry change, skipping pyr rebuild\n");
#endif /*DEBUG*/
		return;
	}

	tilecache_free_pyramid(tilecache);

	/* How many levels? Keep shrinking until we get both axes in one tile. We
	 * need to handle very lopsided images, like LUTs and multi-page images,
	 * do we must shrink both dimensions.
	 */
	level_width = tilesource->image_width;
	level_height = tilesource->image_height;
	n_levels = 1;
	for (;;) {
		if (level_width <= TILE_SIZE &&
			level_height <= TILE_SIZE)
			break;

		level_width >>= 1;
		level_height >>= 1;
		n_levels += 1;
	}

	tilecache->n_levels = n_levels;

	tilecache->levels = VIPS_ARRAY(NULL, n_levels, VipsImage *);
	level_width = tilesource->image_width;
	level_height = tilesource->image_height;
	for (int i = 0; i < n_levels; i++) {
		tilecache->levels[i] = vips_image_new();

		vips_image_init_fields(tilecache->levels[i],
			level_width,
			level_height,
			tilesource->rgb->Bands,
			tilesource->rgb->BandFmt,
			tilesource->rgb->Coding,
			tilesource->rgb->Type,
			tilesource->rgb->Xres,
			tilesource->rgb->Yres);

		level_width = VIPS_MAX(1, level_width >> 1);
		level_height = VIPS_MAX(1, level_height >> 1);
	}

	tilecache->tiles = VIPS_ARRAY(NULL, n_levels, GSList *);
	tilecache->visible = VIPS_ARRAY(NULL, n_levels, GSList *);
	tilecache->free = VIPS_ARRAY(NULL, n_levels, GSList *);

#ifdef DEBUG
	printf("	 %d pyr levels\n", n_levels);
	for (int i = 0; i < n_levels; i++)
		printf("	 %d) %d x %d\n",
			i,
			tilecache->levels[i]->Xsize,
			tilecache->levels[i]->Ysize);
#endif /*DEBUG*/
}

/* All tiles need refetching, perhaps after eg. "falsecolour" etc. Mark
 * all tiles invalid and reemit.
 */
void
tilecache_source_tiles_changed(Tilesource *tilesource,
	Tilecache *tilecache)
{
	int i;

#ifdef DEBUG
	printf("tilecache_source_tiles_changed: %p\n", tilecache);
#endif /*DEBUG*/

	for (i = 0; i < tilecache->n_levels; i++) {
		GSList *p;

		for (p = tilecache->tiles[i]; p; p = p->next) {
			Tile *tile = TILE(p->data);

			/* We must refetch.
			 */
			tile->valid = FALSE;
		}
	}

	tilecache_tiles_changed(tilecache);
}

/* Everything has changed, eg. page turn and the image geometry has changed.
 */
static void
tilecache_source_changed(Tilesource *tilesource, Tilecache *tilecache)
{
#ifdef DEBUG
	printf("tilecache_source_changed:\n");
#endif /*DEBUG*/

	/* Junk all tiles on geometry change.
	 */
	tilecache_build_pyramid(tilecache);

	/* All tiles must be invalidated.
	 */
	tilecache_source_tiles_changed(tilesource, tilecache);

	/* All views must update.
	 */
	tilecache_changed(tilecache);
}

/* background load is done ... no pixels have changed, but we should repaint
 * in casewe have any missing tiles.
 */
static void
tilecache_source_loaded(Tilesource *tilesource, Tilecache *tilecache)
{
#ifdef DEBUG
	printf("tilecache_source_loaded:\n");
#endif /*DEBUG*/

	/* Repaint to trigger a request (if necessary).
	 */
	tilecache_changed(tilecache);
}

static Tile *
tilecache_find(Tilecache *tilecache, VipsRect *tile_rect, int z)
{
	GSList *p;
	Tile *tile;

	for (p = tilecache->tiles[z]; p; p = p->next) {
		tile = TILE(p->data);

		if (vips_rect_overlapsrect(&tile->bounds, tile_rect))
			return tile;
	}

	return NULL;
}

/* Request a single tile. If we have this tile already, refresh if there are new
 * pixels available.
 */
static void
tilecache_request(Tilecache *tilecache, VipsRect *tile_rect, int z)
{
	Tile *tile;

	/* Look for an existing tile, or make a new one.
	 *
	 * FIXME ... this could be a hash. Could other lookups be hashes as
	 * well, if we rescale x/y for changes in z?
	 */
	if (!(tile = tilecache_find(tilecache, tile_rect, z))) {
		if (!(tile = tile_new(tilecache->levels[z],
				  tile_rect->left >> z, tile_rect->top >> z, z)))
			return;

		tilecache->tiles[z] = g_slist_prepend(tilecache->tiles[z], tile);
	}

	if (!tile->valid) {
#ifdef DEBUG_VERBOSE
		printf("tilecache_request: fetching left = %d, top = %d, "
			   "width = %d, height = %d, z = %d\n",
			tile_rect->left, tile_rect->top,
			tile_rect->width, tile_rect->height,
			z);
#endif /*DEBUG_VERBOSE*/

		tilesource_request_tile(tilecache->tilesource, tile);
	}
}

/* Request tiles from an area. If they are not in cache, they will be computed
 * in the bg and delivered via _collect().
 *
 * render processes tiles in FIFO order, so we need to add in reverse order
 * of processing. We want repaint to happen in a spiral from the centre out,
 * so we have to add in a spiral from the outside in.
 *
 * We must be careful not to change tilesource if we have all these tiles
 * already (very common for thumbnails, for example).
 */
static void
tilecache_request_area(Tilecache *tilecache, graphene_rect_t *grect, int z)
{
	int size0 = TILE_SIZE << z;

	// no image ready for paint? try again later
	if (!tilecache->tilesource ||
		!tilecache->tilesource->rgb)
		return;

	/* Only interested in bounding int pixel coordinates.
	 */
	graphene_rect_t bounds;
	graphene_rect_round_extents(grect, &bounds);

	/* All the tiles rect touches in this pyr level.
	 */
	int left = VIPS_ROUND_DOWN((int) bounds.origin.x, size0);
	int top = VIPS_ROUND_DOWN((int) bounds.origin.y, size0);
	int right =
		VIPS_ROUND_UP((int) (bounds.origin.x + bounds.size.width), size0);
	int bottom =
		VIPS_ROUND_UP((int) (bounds.origin.y + bounds.size.height), size0);

	/* Do the four edges, then step in. Loop until the centre is empty.
	 */
	for (;;) {
		VipsRect tile_rect;
		int x, y;

		tile_rect.width = size0;
		tile_rect.height = size0;

		if (right - left <= 0 ||
			bottom - top <= 0)
			break;

		/* Top edge.
		 */
		for (x = left; x < right; x += size0) {
			tile_rect.left = x;
			tile_rect.top = top;
			tilecache_request(tilecache, &tile_rect, z);
		}

		top += size0;
		if (right - left <= 0 ||
			bottom - top <= 0)
			break;

		/* Bottom edge.
		 */
		for (x = left; x < right; x += size0) {
			tile_rect.left = x;
			tile_rect.top = bottom - size0;
			tilecache_request(tilecache, &tile_rect, z);
		}

		bottom -= size0;
		if (right - left <= 0 ||
			bottom - top <= 0)
			break;

		/* Left edge.
		 */
		for (y = top; y < bottom; y += size0) {
			tile_rect.left = left;
			tile_rect.top = y;
			tilecache_request(tilecache, &tile_rect, z);
		}

		left += size0;
		if (right - left <= 0 ||
			bottom - top <= 0)
			break;

		/* Right edge.
		 */
		for (y = top; y < bottom; y += size0) {
			tile_rect.left = right - size0;
			tile_rect.top = y;
			tilecache_request(tilecache, &tile_rect, z);
		}

		right -= size0;
		if (right - left <= 0 ||
			bottom - top <= 0)
			break;
	}
}

/* A new tile is available from the bg render and must be collected.
 */
static void
tilecache_source_collect(Tilesource *tilesource,
	VipsRect *dirty, int z, Tilecache *tilecache)
{
#ifdef DEBUG_VERBOSE
	printf("tilecache_source_collect: left = %d, top = %d, "
		   "width = %d, height = %d, z = %d\n",
		dirty->left, dirty->top,
		dirty->width, dirty->height, z);
#endif /*DEBUG_VERBOSE*/

	Tile *tile = tilecache_find(tilecache, dirty, z);
	if (tile &&
		!tile->valid) {
		tilesource_collect_tile(tilecache->tilesource, tile);

		// things displaying us will need to redraw
		tilecache_area_changed(tilecache, dirty, z);
	}
}

static void
tilecache_set_tilesource(Tilecache *tilecache, Tilesource *tilesource)
{
	FREESID(tilecache->tilesource_changed_sid, tilecache->tilesource);
	FREESID(tilecache->tilesource_loaded_sid, tilecache->tilesource);
	FREESID(tilecache->tilesource_tiles_changed_sid, tilecache->tilesource);
	FREESID(tilecache->tilesource_collect_sid, tilecache->tilesource);
	VIPS_UNREF(tilecache->tilesource);

	tilecache->tilesource = tilesource;
	if (tilesource) {
		g_object_ref(tilesource);

		// this will only rebuild if the image geometry has changed
		tilecache_build_pyramid(tilecache);

		tilecache->tilesource_changed_sid =
			g_signal_connect(tilesource, "changed",
				G_CALLBACK(tilecache_source_changed), tilecache);
		tilecache->tilesource_loaded_sid =
			g_signal_connect(tilesource, "loaded",
				G_CALLBACK(tilecache_source_loaded), tilecache);
		tilecache->tilesource_tiles_changed_sid =
			g_signal_connect(tilesource, "tiles-changed",
				G_CALLBACK(tilecache_source_tiles_changed), tilecache);
		tilecache->tilesource_collect_sid =
			g_signal_connect(tilesource, "collect",
				G_CALLBACK(tilecache_source_collect), tilecache);

		/* Any tiles must be invalidated.
		 */
		tilecache_source_tiles_changed(tilesource, tilecache);
	}
}

static void
tilecache_set_property(GObject *object,
	guint prop_id, const GValue *value, GParamSpec *pspec)
{
	Tilecache *tilecache = (Tilecache *) object;

	int i;

	switch (prop_id) {
	case PROP_BACKGROUND:
		i = g_value_get_int(value);
		if (i >= 0 &&
			i < TILECACHE_BACKGROUND_LAST &&
			tilecache->background != i) {
			tilecache->background = i;
			VIPS_UNREF(tilecache->background_texture);
			tilecache->background_texture =
				tilecache_texture(tilecache->background);
			tilecache_tiles_changed(tilecache);
		}
		break;

	case PROP_TILESOURCE:
		tilecache_set_tilesource(tilecache, g_value_get_object(value));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
tilecache_get_property(GObject *object,
	guint prop_id, GValue *value, GParamSpec *pspec)
{
	Tilecache *tilecache = (Tilecache *) object;

	switch (prop_id) {
	case PROP_BACKGROUND:
		g_value_set_int(value, tilecache->background);
		break;

	case PROP_TILESOURCE:
		g_value_set_object(value, tilecache->tilesource);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
tilecache_class_init(TilecacheClass *class)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(class);

	gobject_class->dispose = tilecache_dispose;
	gobject_class->set_property = tilecache_set_property;
	gobject_class->get_property = tilecache_get_property;

	g_object_class_install_property(gobject_class, PROP_BACKGROUND,
		g_param_spec_int("background",
			_("Background"),
			_("Background mode"),
			0, TILECACHE_BACKGROUND_LAST - 1,
			TILECACHE_BACKGROUND_CHECKERBOARD,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_TILESOURCE,
		g_param_spec_object("tilesource",
			_("Tile source"),
			_("The tile source to be displayed"),
			TILESOURCE_TYPE,
			G_PARAM_READWRITE));

	tilecache_signals[SIG_CHANGED] = g_signal_new("changed",
		G_TYPE_FROM_CLASS(class),
		G_SIGNAL_RUN_LAST,
		0,
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	tilecache_signals[SIG_TILES_CHANGED] = g_signal_new("tiles-changed",
		G_TYPE_FROM_CLASS(class),
		G_SIGNAL_RUN_LAST,
		0,
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	tilecache_signals[SIG_AREA_CHANGED] = g_signal_new("area-changed",
		G_TYPE_FROM_CLASS(class),
		G_SIGNAL_RUN_LAST,
		0,
		NULL, NULL,
		vipsdisp_VOID__POINTER_INT,
		G_TYPE_NONE, 2,
		G_TYPE_POINTER,
		G_TYPE_INT);
}

/* Expand a rect out to the set of tiles it touches on this level.
 */
static void
tilecache_tiles_for_rect(Tilecache *tilecache, VipsRect *rect, int z,
	VipsRect *touches)
{
	int size0 = TILE_SIZE << z;
	int left = VIPS_ROUND_DOWN(rect->left, size0);
	int top = VIPS_ROUND_DOWN(rect->top, size0);
	int right = VIPS_ROUND_UP(VIPS_RECT_RIGHT(rect), size0);
	int bottom = VIPS_ROUND_UP(VIPS_RECT_BOTTOM(rect), size0);

	touches->left = left;
	touches->top = top;
	touches->width = right - left;
	touches->height = bottom - top;

	/* We can have rects outside the image. Make sure they stay empty.
	 */
	if (vips_rect_isempty(rect)) {
		touches->width = 0;
		touches->height = 0;
	}
}

/* Find the first visible tile in a hole.
 */
static void
tilecache_fill_hole(Tilecache *tilecache, VipsRect *bounds, int z)
{
	int i;

	for (i = z; i < tilecache->n_levels; i++) {
		GSList *p;

		for (p = tilecache->tiles[i]; p; p = p->next) {
			Tile *tile = TILE(p->data);
			GSList **visible = &tilecache->visible[i];

			/* Ignore tiles with no current or previous pixels.
			 */
			if (!tile->valid &&
				!tile->texture)
				continue;

			/* Ignore tiles we're already drawing.
			 */
			if (g_slist_index(*visible, tile) >= 0)
				continue;

			if (vips_rect_overlapsrect(&tile->bounds, bounds)) {
				tile_touch(tile);
				*visible = g_slist_prepend(*visible, tile);
				return;
			}
		}
	}
}

static int
tilecache_sort_lru(const void *a, const void *b)
{
	Tile *t1 = TILE(a);
	Tile *t2 = TILE(b);

	return t1->time - t2->time;
}

static void
tilecache_free_oldest(Tilecache *tilecache, int z)
{
	int n_free = g_slist_length(tilecache->free[z]);
	int n_to_free = VIPS_MAX(0, n_free - MAX_TILES);

	if (n_to_free > 0) {
		int i;

		tilecache->free[z] = g_slist_sort(tilecache->free[z],
			tilecache_sort_lru);

		for (i = 0; i < n_to_free; i++) {
			g_autoptr(Tile) tile = TILE(tilecache->free[z]->data);

			g_assert(g_slist_find(tilecache->tiles[z], tile));

			tilecache->tiles[z] =
				g_slist_remove(tilecache->tiles[z], tile);
			tilecache->visible[z] =
				g_slist_remove(tilecache->visible[z], tile);
			tilecache->free[z] =
				g_slist_remove(tilecache->free[z], tile);
		}
	}
}

#ifdef DEBUG_VERBOSE
static void
tilecache_print(Tilecache *tilecache)
{
	int i;

	for (i = 0; i < tilecache->n_levels; i++) {
		printf("  level %d, %d tiles, %d visible, %d free\n",
			i,
			g_slist_length(tilecache->tiles[i]),
			g_slist_length(tilecache->visible[i]),
			g_slist_length(tilecache->free[i]));
	}

	for (i = 0; i < tilecache->n_levels; i++) {
		GSList *p;

		printf("  level %d tiles:\n", i);
		for (p = tilecache->tiles[i]; p; p = p->next) {
			Tile *tile = TILE(p->data);
			int visible = g_slist_index(tilecache->visible[i],
							  tile) >= 0;

			printf("    @ %d x %d, %d x %d, "
				   "valid = %d, visible = %d, "
				   "texture = %p\n",
				tile->bounds.left,
				tile->bounds.top,
				tile->bounds.width,
				tile->bounds.height,
				tile->valid,
				visible,
				tile->texture);
		}
	}
}
#endif /*DEBUG_VERBOSE*/

static void
tilecache_compute_visibility(Tilecache *tilecache,
	graphene_rect_t *grect, int z)
{
	int size0 = TILE_SIZE << z;
	int start_time = tile_get_time();

	int i;
	VipsRect touches;
	int x, y;
	VipsRect bounds;
	GSList *p;

#ifdef DEBUG_VERBOSE
	printf("tilecache_compute_visibility: z = %d\n", z);
#endif /*DEBUG_VERBOSE*/

	/* Only interested in bounding int pixel coordinates.
	 */
	graphene_rect_t bounding_grect;
	graphene_rect_round_extents(grect, &bounding_grect);
	VipsRect viewport;
	viewport.left = bounding_grect.origin.x;
	viewport.top = bounding_grect.origin.y;
	viewport.width = bounding_grect.size.width;
	viewport.height = bounding_grect.size.height;

	/* We're rebuilding these.
	 */
	for (i = 0; i < tilecache->n_levels; i++) {
		VIPS_FREEF(g_slist_free, tilecache->visible[i]);
		VIPS_FREEF(g_slist_free, tilecache->free[i]);
	}

	/* The rect of tiles touched by the viewport.
	 */
	tilecache_tiles_for_rect(tilecache, &viewport, z, &touches);

#ifdef DEBUG_VERBOSE
	printf("viewport in level0 coordinates: left = %d, top = %d, "
		   "width = %d, height = %d\n",
		touches.left, touches.top,
		touches.width, touches.height);
#endif /*DEBUG_VERBOSE*/

	/* Search for the highest res tile for every position in the
	 * viewport.
	 */
	bounds.width = size0;
	bounds.height = size0;
	for (y = 0; y < touches.height; y += size0)
		for (x = 0; x < touches.width; x += size0) {
			bounds.left = x + touches.left;
			bounds.top = y + touches.top;

			tilecache_fill_hole(tilecache, &bounds, z);
		}

	/* So any tiles we've not touched must be invisible and therefore
	 * candidates for freeing.
	 */
	for (i = 0; i < tilecache->n_levels; i++) {
		for (p = tilecache->tiles[i]; p; p = p->next) {
			Tile *tile = TILE(p->data);

			if (tile->time < start_time)
				tilecache->free[i] = g_slist_prepend(tilecache->free[i], tile);
		}
	}

	/* Free the oldest few unused tiles in each level.
	 *
	 * Never free tiles in the lowest-res few levels. They are useful for
	 * filling in holes and take little memory.
	 */
	for (i = 0; i < tilecache->n_levels - 3; i++)
		tilecache_free_oldest(tilecache, i);

#ifdef DEBUG_VERBOSE
	tilecache_print(tilecache);
#endif /*DEBUG_VERBOSE*/
}

Tilecache *
tilecache_new(void)
{
	Tilecache *tilecache = g_object_new(TILECACHE_TYPE, NULL);

	return tilecache;
}

static void
tilecache_draw_bounds(GtkSnapshot *snapshot,
	Tile *tile, graphene_rect_t *bounds)
{
	static float border_width[4] = { 2, 2, 2, 2 };
	static GdkRGBA border_colour[4] = {
		{ 0, 1, 0, 1 },
		{ 0, 1, 0, 1 },
		{ 0, 1, 0, 1 },
		{ 0, 1, 0, 1 },
	};

	GskRoundedRect outline = GSK_ROUNDED_RECT_INIT(
		bounds->origin.x,
		bounds->origin.y,
		bounds->size.width,
		bounds->size.height);

	gtk_snapshot_append_border(snapshot, &outline,
		border_width, border_colour);

	/* If we are drawing a low-res tile at the back of the stack, it can
	 * get extremely large with big images. Cairo hates large surfaces,
	 * so skip the text annotation in this case.
	 */
	if (bounds->size.width < 32000 &&
		bounds->size.height < 32000) {
		char str[256];
		VipsBuf buf = VIPS_BUF_STATIC(str);

		g_autoptr(cairo_t) cr = gtk_snapshot_append_cairo(snapshot, bounds);

		cairo_set_source_rgb(cr, 0, 1, 0);
		cairo_set_font_size(cr, 12);

		cairo_move_to(cr,
			bounds->origin.x + 0.1 * bounds->size.width,
			bounds->origin.y + 0.1 * bounds->size.height);
		vips_buf_appendf(&buf, "%p", tile);
		cairo_show_text(cr, vips_buf_all(&buf));

		cairo_move_to(cr, bounds->origin.x + 0.1 * bounds->size.width,
			bounds->origin.y + 0.2 * bounds->size.height);
		vips_buf_rewind(&buf);
		vips_buf_appendf(&buf, "%d", tile->time);
		cairo_show_text(cr, vips_buf_all(&buf));
	}
}

/* Scale is how much the level0 image has been scaled, x/y is the position of
 * the top-left corner of the paint_rect area in the scaled image.
 *
 * paint_rect is the pixel area in gtk coordinates that we paint in the widget.
 *
 * Set debug to draw tile boundaries for debugging.
 */
void
tilecache_snapshot(Tilecache *tilecache, GtkSnapshot *snapshot,
	double scale, double x, double y, graphene_rect_t *paint, gboolean debug)
{
	graphene_rect_t viewport;
	int z;
	int i;

	/* In debug mode, scale and offset so we can see tile clipping.
	 */
	float debug_scale = 0.9;
	graphene_point_t debug_offset = { 32, 32 };

#ifdef DEBUG_RENDER_TIME
	GTimer *snapshot_timer = g_timer_new();
#endif /*DEBUG_RENDER_TIME*/

	g_assert(tilecache->n_levels > 0);

	if (debug) {
		gtk_snapshot_translate(snapshot, &debug_offset);
		gtk_snapshot_scale(snapshot, debug_scale, debug_scale);
	}

#ifdef DEBUG
	printf("tilecache_snapshot: %p scale = %g, x = %g, y = %g\n",
		tilecache, scale, x, y);
#endif /*DEBUG*/

#ifdef DEBUG_VERBOSE
	printf("  paint x = %g, y = %g, "
		   "width = %g, height = %g\n",
		paint->origin.x, paint->origin.y,
		paint->size.width, paint->size.height);
#endif /*DEBUG_VERBOSE*/

#ifdef DEBUG_VERBOSE
	printf("tilecache_snapshot: %p tiles are:\n", tilecache);
	tilecache_print(tilecache);
#endif /*DEBUG_VERBOSE*/

	/* Pick a pyramid layer. For enlarging, we leave the z at 0
	 * (the highest res layer).
	 */
	if (scale > 1.0 ||
		scale == 0)
		z = 0;
	else
		z = VIPS_CLIP(0, log(1.0 / scale) / log(2.0), tilecache->n_levels - 1);

	/* paint_rect in level0 coordinates.
	 */
	viewport.origin.x = x / scale;
	viewport.origin.y = y / scale;
	viewport.size.width = VIPS_MAX(1, paint->size.width / scale);
	viewport.size.height = VIPS_MAX(1, paint->size.height / scale);

	/* Fetch any tiles we are missing, update any tiles we have that have
	 * been flagged as having pixels ready for fetching.
	 */
	tilecache_request_area(tilecache, &viewport, z);

	/* Find the set of visible tiles, sorted back to front.
	 *
	 * FIXME ... we could often skip this, esp. when panning, unless we
	 * cross a tile boundary.
	 */
	tilecache_compute_visibility(tilecache, &viewport, z);

	/* If there's an alpha, we'll need a backdrop.
	 */
	if (vips_image_hasalpha(tilecache->levels[0])) {
		graphene_rect_t bounds;

#ifdef DEBUG_VERBOSE
		printf("tilecache_snapshot: drawing background\n");
#endif /*DEBUG_VERBOSE*/

		bounds = *paint;
		gtk_snapshot_push_repeat(snapshot, &bounds, NULL);

		bounds.origin.x = 0;
		bounds.origin.y = 0;
		bounds.size.width = TILE_SIZE;
		bounds.size.height = TILE_SIZE;
		gtk_snapshot_append_texture(snapshot,
			tilecache->background_texture, &bounds);

		gtk_snapshot_pop(snapshot);
	}

	/* Draw all visible tiles, low res (at the back) to high res (at the
	 * front).
	 */
	for (i = tilecache->n_levels - 1; i >= z; i--) {
		GSList *p;

		for (p = tilecache->visible[i]; p; p = p->next) {
			Tile *tile = TILE(p->data);

			/* If we are zooming in beyond 1:1, we want nearest so we don't
			 * blur the image. For zooming out, we want trilinear to get
			 * mipmaps and antialiasing.
			 */
			GskScalingFilter filter = scale > 1.0 ?
				GSK_SCALING_FILTER_NEAREST : GSK_SCALING_FILTER_TRILINEAR;

			graphene_rect_t bounds;

			// +1 to hide tile boundaries
			bounds.origin.x = tile->bounds.left * scale - x + paint->origin.x;
			bounds.origin.y = tile->bounds.top * scale - y + paint->origin.y;
			bounds.size.width = tile->bounds.width * scale + 1;
			bounds.size.height = tile->bounds.height * scale + 1;

			gtk_snapshot_append_scaled_texture(snapshot,
				tile_get_texture(tile), filter, &bounds);

			/* In debug mode, draw the edges and add text for the
			 * tile pointer and age.
			 */
			if (debug)
				tilecache_draw_bounds(snapshot, tile, &bounds);
		}
	}

	/* Draw a box for the viewport.
	 */
	if (debug) {
#define BORDER ((GdkRGBA){ 1, 0, 0, 1 })

		GskRoundedRect outline;

		gsk_rounded_rect_init_from_rect(&outline,
			&GRAPHENE_RECT_INIT(
				viewport.origin.x * scale - x + paint->origin.x,
				viewport.origin.y * scale - y + paint->origin.y,
				viewport.size.width * scale,
				viewport.size.height * scale),
			0);

		gtk_snapshot_append_border(snapshot,
			&outline,
			(float[4]){ 2, 2, 2, 2 },
			(GdkRGBA[4]){ BORDER, BORDER, BORDER, BORDER });
	}

#ifdef DEBUG_RENDER_TIME
	printf("tilecache_snapshot: %g ms\n",
		g_timer_elapsed(snapshot_timer, NULL) * 1000);
	g_timer_destroy(snapshot_timer);
#endif /*DEBUG_RENDER_TIME*/
}
