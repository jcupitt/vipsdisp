
typedef struct _Imagedisplay {
	GtkDrawingArea parent_instance;

	/* We implement a scrollable interface.
	 */
	GtkAdjustment *hadjustment;
	GtkAdjustment *vadjustment;
	guint hscroll_policy;
	guint vscroll_policy;

	VipsImage *image;

	/* A backing buffer the size of the visible part of the image. We use
	 * this from the draw handler to paint the screen, and we also paint to
	 * this from libvips in a background thread.
	 *
	 * This is always Cairo-style ARGB.
	 */
	unsigned char *cairo_buffer;
	int cairo_buffer_width;
	int cairo_buffer_height;

	/* left/top is the position of the top-left corner of the window within
	 * the image. These can be negative if the image is smaller than the
	 * window and we are displaying the image in the centre.
	 */
	int left;
	int top;

	int mag;

	/* A region on the input image for the status bar. 
	 */
	VipsRegion *image_region;

	/* The image reformatted for the display, ie. including shrink & zoom.
	 */
	VipsImage *display;
	VipsRegion *display_region;

	/* The image to sRGB for painting.
	 */
	VipsImage *srgb;
	VipsRegion *srgb_region;

	/* The mask for detecting empty areas.
	 */
	VipsImage *mask;
	VipsRegion *mask_region;

	/* Watch image load with these.
	 */
	guint preeval_sig;
	guint eval_sig;
	guint posteval_sig;


} Imagedisplay;

typedef struct _ImagedisplayClass {
	GtkDrawingAreaClass parent_class;

	/* Track image load.
	 */
	void (*preload)( Imagedisplay *imagedisplay, VipsProgress *progress );
	void (*load)( Imagedisplay *imagedisplay, VipsProgress *progress );
	void (*postload)( Imagedisplay *imagedisplay, VipsProgress *progress );

} ImagedisplayClass;

int imagedisplay_set_file( Imagedisplay *imagedisplay, GFile *file );

void imagedisplay_set_mag( Imagedisplay *imagedisplay, int mag );
int imagedisplay_get_mag( Imagedisplay *imagedisplay );

gboolean imagedisplay_get_image_size( Imagedisplay *imagedisplay, 
	int *width, int *height );
gboolean imagedisplay_get_display_image_size( Imagedisplay *imagedisplay, 
	int *width, int *height );

void imagedisplay_to_image_cods( Imagedisplay *imagedisplay,
	int display_x, int display_y, int *image_x, int *image_y );
void imagedisplay_to_display_cods( Imagedisplay *imagedisplay,
	int image_x, int image_y, int *display_x, int *display_y ); 

VipsPel *imagedisplay_get_ink( Imagedisplay *imagedisplay, int x, int y );

Imagedisplay *imagedisplay_new( void ); 
