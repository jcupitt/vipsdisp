
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

typedef struct _Conversion {
	GObject parent_instance;

	/* The image we are holding.
	 */
	VipsImage *image;
	VipsRegion *image_region;

	/* Display transform parameters.
	 */
	int mag;
	double scale;
	double offset;

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

	/* TRUE when the image has fully loaded (ie. postload has fired) and we
	 * can start looking at pixels.
	 */
	gboolean loaded;

} Conversion;

typedef struct _ConversionClass {
	GObjectClass parent_class;

	/* Track image load.
	 */
	void (*preload)( Conversion *conversion, VipsProgress *progress );
	void (*load)( Conversion *conversion, VipsProgress *progress );
	void (*postload)( Conversion *conversion, VipsProgress *progress );

	/* The image has changed.
	 */
	void (*changed)( Conversion *conversion );

	/* Part of the output image has changed.
	 */
	void (*area_changed)( Conversion *conversion, VipsRect *dirty );

} ConversionClass;

GType conversion_get_type( void );

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

Conversion *conversion_new( void ); 