
typedef struct _Conversion {
	GObject parent_instance;

	VipsImage *image;

	int mag;

	/* A region on the input image for the status bar. 
	 */
	VipsRegion *image_region;

	/* The image resized for the display, ie. including shrink & zoom.
	 */
	VipsImage *display;
	VipsRegion *display_region;

	/* The image to sRGB for painting.
	 */
	VipsImage *srgb;
	VipsRegion *srgb_region;

	/* Watch image load with these.
	 */
	guint preeval_sig;
	guint eval_sig;
	guint posteval_sig;

} Conversion;

typedef struct _ConversionClass {
	GtkDrawingAreaClass parent_class;

	/* Track image load.
	 */
	void (*preload)( Conversion *conversion, VipsProgress *progress );
	void (*load)( Conversion *conversion, VipsProgress *progress );
	void (*postload)( Conversion *conversion, VipsProgress *progress );

} ConversionClass;

int conversion_set_file( Conversion *conversion, GFile *file );

void conversion_set_mag( Conversion *conversion, int mag );
int conversion_get_mag( Conversion *conversion );

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
