
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

/* Conversion modes. This is how we turn pages into images.
 *
 * MULTIPAGE
 * 	If there's more than one page and pages are not all the same size, we
 * 	have a select-page gtk_spin_button.
 *
 * ANIMATION
 * 	If there's more than one page, they are all the same size, and there's
 * 	a "delay" metadata item, we show pages as a looped animation.
 *
 * PYRAMID
 * 	If the image has a pyramid structure (subifd or page based), use pages
 * 	or layers to implement zooming. 
 *
 * TOILET_ROLL
 * 	If there's more than one page, and they are all the same size, we show 
 * 	the image as a very tall, thin strip. 
 *
 * The user can sometimes override the default conversion mode, eg. you can use
 * the select-page box to view layers of a pyramid, or toilet roll mode to
 * examine frames of an animation.
 */
typedef enum _ConversionMode {
	CONVERSION_MULTIPAGE,
	CONVERSION_ANIMATION,
	CONVERSION_PYRAMID,
	CONVERSION_TOILET_ROLL
} ConversionMode;

typedef struct _Conversion {
	GObject parent_instance;

	/* The loader and the source we have loaded. We may need to reload on
	 * a zoom change, so we need to keep the source.
	 */
	const char *loader;
	VipsSource *source;

	/* Basic image geometry.
	 */
        int width;
        int height;
        int n_pages;
        int n_subifds;

	/* The viewing mode.
	 */
	ConversionMode mode;

        /* For TIFF sources, open subifds to get pyr layers.
         */
        gboolean subifd_pyramid;

        /* For TIFF sources, open pages to get pyr layers.
         */
        gboolean page_pyramid;

	/* If all the pages are the same size, we can display as a toilet roll
	 * or animation.
	 */
	gboolean pages_same_size;

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
	guint page_flip_timer;

	/* TRUE when the image has fully loaded (ie. postload has fired) and we
	 * can start looking at pixels.
	 */
	gboolean loaded;

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

	/* Part of the output image has changed.
	 */
	void (*area_changed)( Conversion *conversion, VipsRect *dirty );

} ConversionClass;

GType conversion_get_type( void );

int conversion_set_source( Conversion *conversion, VipsSource *source );
int conversion_set_file( Conversion *conversion, GFile *file );

gboolean conversion_get_image_size( Conversion *conversion, 
	int *width, int *height );
gboolean conversion_get_display_image_size( Conversion *conversion, 
	int *width, int *height );

void conversion_to_image_cods( Conversion *conversion,
	int display_x, int display_y, int *image_x, int *image_y );
void conversion_to_display_cods( Conversion *conversion,
	int image_x, int image_y, int *display_x, int *display_y ); 

VipsPel *conversion_get_ink( Conversion *conversion, int x, int y );

const char *conversion_get_path( Conversion *conversion );

int conversion_write_to_file( Conversion *conversion, const char *file );

Conversion *conversion_new( void ); 
