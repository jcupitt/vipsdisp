#ifndef __TILESOURCE_H
#define __TILESOURCE_H

#define TILESOURCE_TYPE (tilesource_get_type())
#define TILESOURCE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), TILESOURCE_TYPE, Tilesource))
#define TILESOURCE_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), TILESOURCE_TYPE, TilesourceClass))
#define IS_TILESOURCE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), TILESOURCE_TYPE))
#define IS_TILESOURCE_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), TILESOURCE_TYPE))
#define TILESOURCE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), TILESOURCE_TYPE, TilesourceClass))

/* The three basic types of image we support.
 *
 * MULTIPAGE
 *
 *	Pages differ in size or perhaps format, so must be loaded as separate
 *	images. Pages can have subifd pyramids. Includes single-page images.
 *	Reload on page change.
 *
 * PAGE_PYRAMID
 *
 *	"page" param is pyr levels. We load a single page and reload on
 *	magnification change.
 *
 * TOILET_ROLL
 *
 *	All pages are the identical, so we open as a single, tall, thin strip
 *	and the viewer does any presenting as pages / animation / etc. during
 *	conversion to the screen display image.
 *	These images can have subifd pyramids.
 */
typedef enum _TilesourceType {
	TILESOURCE_TYPE_MULTIPAGE,
	TILESOURCE_TYPE_PAGE_PYRAMID,
	TILESOURCE_TYPE_TOILET_ROLL,
} TilesourceType;

/* The modes of image display we support.
 *
 * TOILET_ROLL
 *
 *	Just show the whole image (no crop). Page control disabled. Reload on
 *	mag change if there's a pyramid.
 *
 * MULTIPAGE
 *
 *	Behaviour depends on TilesourceImage:
 *
 *	TILESOURCE_TYPE_PAGE_PYRAMID
 *
 *		Disable page controls. No crop. Reload on mag change.
 *
 *	TILESOURCE_TYPE_TOILET_ROLL
 *
 *		Enable page control iff > 1 page. Crop in display conversion
 *		to select page.
 *
 *	TILESOURCE_TYPE_MULTIPAGE
 *
 *		Enable page control iff > 1 page.
 *
 * ANIMATED
 *
 *	Just like MULTIPAGE, except page flip is driven by a timeout.
 *
 * PAGES_AS_BANDS
 *
 *	Just like toilet roll, exccept that we chop the image into pages and
 *	bandjoin them all. Handy for OME-TIFF, which has a one-band image
 *	in each page.
 */
typedef enum _TilesourceMode {
	TILESOURCE_MODE_TOILET_ROLL,
	TILESOURCE_MODE_MULTIPAGE,
	TILESOURCE_MODE_ANIMATED,
	TILESOURCE_MODE_PAGES_AS_BANDS,
	TILESOURCE_MODE_LAST
} TilesourceMode;

/* Max number of levels we allow in a pyramidal image.
 */
#define MAX_LEVELS (256)

typedef struct _Tilesource {
	GObject parent_instance;

	/* The loader and the file we have loaded from. We may need to reload
	 * on a zoom or page change.
	 *
	 * We can't use a VipsSource since they are not cached and we'd get
	 * repeated decode on page change.
	 *
	 * We can also display a VipsImage, but we can't reload those, of
	 * course.
	 */
	const char *loader;
	char *filename;
	VipsImage *base;

	/* The image we are displaying, and something to fetch pixels from it
	 * with.
	 */
	VipsImage *image;
	VipsRegion *image_region;

	/* What sort of image we have, and how we are displaying it.
	 */
	TilesourceType type;
	TilesourceMode mode;

	/* This is a TIFF subifd pyramid.
	 */
	gboolean subifd_pyramid;

	/* This is a page pyramid (TIFF, jp2k etc.).
	 */
	gboolean page_pyramid;

	/* Basic image geometry. The tilecache pyramid is based on this.
	 */
	int width;
	int height;
	int bands;
	int n_pages;
	int n_subifds;
	int *delay;
	int n_delay;
	double zoom;

	/* If all the pages are the same size and format, we can load as a
	 * toilet roll.
	 */
	gboolean pages_same_size;

	/* If all the pages are the same size and format, and also all mono,
	 * we can display pages as bands.
	 */
	gboolean all_mono;

	/* For pyramidal formats, we need to read out the size of each level.
	 * Largest level first.
	 */
	int level_count;
	int level_width[MAX_LEVELS];
	int level_height[MAX_LEVELS];

	/* Display transform parameters.
	 */
	int page;
	gboolean active;
	double scale;
	double offset;
	gboolean falsecolour;
	gboolean log;
	gboolean icc;

	/* The size of the image with this view mode. So in toilet-roll mode
	 * (for example), display_height is height * n_pages.
	 */
	int display_width;
	int display_height;

	/* The current z for display, mask, rgb. We need to rebuild the
	 * pipeline on z changes.
	 */
	int current_z;

	/* The image resized for the display, ie. including shrink & zoom, and
	 * a cache mask.
	 */
	VipsImage *display;
	VipsImage *mask;

	/* The display image converted to display RGB for painting.
	 */
	VipsImage *rgb;
	VipsRegion *rgb_region;
	VipsRegion *mask_region;

	/* For animations, the timeout we use for page flip.
	 */
	guint page_flip_id;

	/* TRUE when the image has fully loaded (ie. postload has fired) and we
	 * can start looking at pixels.
	 */
	gboolean loaded;

	/* TRUE when the image is visible. GIF animations (for example) pause for
	 * invisible images.
	 */
	gboolean visible;

	/* Set on a background load error, eg. cancel etc.
	 */
	int load_error;
	char *load_message;

} Tilesource;

typedef struct _TilesourceClass {
	GObjectClass parent_class;

	/* Signal image load.
	 */
	void (*preeval)(Tilesource *tilesource, VipsProgress *progress);
	void (*eval)(Tilesource *tilesource, VipsProgress *progress);
	void (*posteval)(Tilesource *tilesource, VipsProgress *progress);

	/* Everything has changed, so image geometry and pixels. Perhaps a
	 * new page in a multi-page TIFF where pages change in size.
	 */
	void (*changed)(Tilesource *tilesource);

	/* All tiles have changed, but image geometry has not. Falsecolour,
	 * page flip in a GIF, etc.
	 */
	void (*tiles_changed)(Tilesource *tilesource);

	/* A set of tiles on a certain level have new pixels now that a
	 * background render has completed.
	 */
	void (*area_changed)(Tilesource *tilesource, VipsRect *area, int z);

	/* The page has changed. Just for updating the page number display.
	 */
	void (*page_changed)(Tilesource *tilesource);

} TilesourceClass;

G_DEFINE_AUTOPTR_CLEANUP_FUNC(Tilesource, g_object_unref)

GType tilesource_get_type(void);

Tilesource *tilesource_new_from_file(const char *filename);
Tilesource *tilesource_new_from_image(VipsImage *image);

void tilesource_background_load(Tilesource *tilesource);

int tilesource_fill_tile(Tilesource *tilesource, Tile *tile);

const char *tilesource_get_path(Tilesource *tilesource);
GFile *tilesource_get_file(Tilesource *tilesource);

VipsImage *tilesource_get_image(Tilesource *tilesource);
VipsImage *tilesource_get_base_image(Tilesource *tilesource);
gboolean tilesource_get_pixel(Tilesource *tilesource,
	int image_x, int image_y, double **vector, int *n);
Tilesource *tilesource_duplicate(Tilesource *tilesource);
void tilesource_changed(Tilesource *tilesource);

#endif /*__TILESOURCE_H*/
