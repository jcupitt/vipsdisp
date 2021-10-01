#ifndef __CONVERSION_H
#define __CONVERSION_H

#define TYPE_CONVERSION (conversion_get_type())
#define CONVERSION( obj ) \
	(G_TYPE_CHECK_INSTANCE_CAST( (obj), TYPE_CONVERSION, Conversion ))
#define CONVERSION_CLASS( klass ) \
	(G_TYPE_CHECK_CLASS_CAST( (klass), TYPE_CONVERSION, ConversionClass))
#define IS_CONVERSION( obj ) \
	(G_TYPE_CHECK_INSTANCE_TYPE( (obj), TYPE_CONVERSION ))
#define IS_CONVERSION_CLASS( klass ) \
	(G_TYPE_CHECK_CLASS_TYPE( (klass), TYPE_CONVERSION ))
#define CONVERSION_GET_CLASS( obj ) \
	(G_TYPE_INSTANCE_GET_CLASS( (obj), TYPE_CONVERSION, ConversionClass ))

/* Should be plenty. We parse out any pyramid structure on source set.
 */
#define MAX_LEVELS (256)

/* The three basic types of image we support.
 *
 * PAGE_PYRAMID 
 *
 * 	"page" param is pyr levels. We load a single page and reload on
 * 	magnification change.
 *
 * TOILET_ROLL
 *
 * 	We load as a single, tall, thin strip and the viewer does any
 * 	presenting as pages during conversion to the screen display image.
 * 	These images can have subifd pyramids. Includes single-page images.
 *
 * MULTIPAGE
 *
 * 	Pages differ in size or perhaps format, so must be loaded as separate
 * 	images. Pages can have subifd pyramids. Includes single-page images.
 *
 */
typedef enum _ConversionType {
	CONVERSION_TYPE_PAGE_PYRAMID,
	CONVERSION_TYPE_TOILET_ROLL,
	CONVERSION_TYPE_MULTIPAGE
} ConversionType;

/* The modes of image display we support.
 *
 * TOILET_ROLL
 *
 * 	Just show the whole image (no crop). Page control disabled. Reload on
 * 	mag change if there's a pyramid.
 *
 * MULTIPAGE
 *
 * 	Behaviour depends on ConversionImage:
 *
 * 	CONVERSION_TYPE_PAGE_PYRAMID
 * 		
 * 		Disable page controls. No crop. Reload on mag change.
 *
 * 	CONVERSION_TYPE_TOILET_ROLL
 *
 * 		Enable page control iff > 1 page. Crop in display conversion
 * 		to select page.
 *
 * 	CONVERSION_TYPE_MULTIPAGE
 *
 * 		Enable page control iff > 1 page.
 *
 * ANIMATED
 *
 * 	Just like MULTIPAGE, except page flip is driven by a timeout.
 *
 * PAGES_AS_BANDS
 *
 *      Just like toilet roll, exccept that we chop the image into pages and
 *      bandjoin them all. Handy for OME-TIFF, which has a one-band image
 *      in each page.
 */
typedef enum _ConversionMode {
	CONVERSION_MODE_TOILET_ROLL,
	CONVERSION_MODE_MULTIPAGE,
	CONVERSION_MODE_ANIMATED,
	CONVERSION_MODE_PAGES_AS_BANDS,
	CONVERSION_MODE_LAST
} ConversionMode;

typedef struct _Conversion {
	GObject parent_instance;

	/* The loader and the source we have loaded. We may need to reload on
	 * a zoom or page change, so we need to keep the source.
	 */
	const char *loader;
	VipsSource *source;

	/* Basic image geometry.
	 */
        int width;
        int height;
        int n_pages;
        int n_subifds;
        int *delay;
        int n_delay;

	/* The image class and the viewing mode.
	 */
	ConversionType type;
	ConversionMode mode;

        /* For TIFF sources, open subifds to get pyr layers.
         */
        gboolean subifd_pyramid;

        /* For TIFF sources, open pages to get pyr layers.
         */
        gboolean page_pyramid;

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

	/* The image we are displaying. image_region is used to fetch pixels
	 * values for the status bar.
	 */
	VipsImage *image;
	VipsRegion *image_region;

	/* Display transform parameters.
	 */
	gboolean active;
	int mag;
	int page;
	double scale;
	double offset;
	gboolean falsecolour;
	gboolean log;

	/* The image resized for the display, ie. including shrink & zoom, and
	 * a cache mask.
	 */
	VipsImage *display;
	VipsImage *mask;

	/* The display image converted to display RGB for painting.
	 */
	VipsImage *rgb;

	/* Watch image load with these.
	 */
	guint preeval_sig;
	guint eval_sig;
	guint posteval_sig;

	/* For animations, the timeout we use for page flip.
	 */
	guint page_flip_id;

	/* TRUE when the image has fully loaded (ie. postload has fired) and we
	 * can start looking at pixels.
	 */
	gboolean loaded;

	GSettings *settings;

} Conversion;

typedef struct _ConversionClass {
	GObjectClass parent_class;

	/* Signal image eval.
	 */
	void (*preeval)( Conversion *conversion, VipsProgress *progress );
	void (*eval)( Conversion *conversion, VipsProgress *progress );
	void (*posteval)( Conversion *conversion, VipsProgress *progress );

	/* The whole conversion has changed, eg. new image loaded.
	 */
	void (*changed)( Conversion *conversion );

	/* The display has changed, eg. change of offset or mag.
	 */
	void (*display_changed)( Conversion *conversion );

	/* The page has flipped.
	 */
	void (*page_changed)( Conversion *conversion );

	/* Part of the output image has changed.
	 */
	void (*area_changed)( Conversion *conversion, VipsRect *dirty );

} ConversionClass;

GType conversion_get_type( void );

int conversion_set_conversion( Conversion *conversion, 
	Conversion *old_conversion );
int conversion_set_source( Conversion *conversion, VipsSource *source );
int conversion_set_file( Conversion *conversion, GFile *file );

gboolean conversion_get_image_size( Conversion *conversion, 
	int *width, int *height );
gboolean conversion_get_display_image_size( Conversion *conversion, 
	int *width, int *height );

void conversion_to_image_cods( int mag,
	int display_x, int display_y, int *image_x, int *image_y );
void conversion_to_display_cods( int mag,
	int image_x, int image_y, int *display_x, int *display_y ); 

VipsPel *conversion_get_ink( Conversion *conversion, int x, int y );

const char *conversion_get_path( Conversion *conversion );
GFile *conversion_get_file( Conversion *conversion );

int conversion_write_to_file( Conversion *conversion, GFile *file );

Conversion *conversion_new( void ); 

#endif /* __CONVERSION_H */
