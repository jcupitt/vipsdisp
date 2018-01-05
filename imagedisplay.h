
typedef struct _Imagedisplay {
	GtkDrawingArea parent_instance;

	/* We implement a scrollable interface.
	 */
	GtkAdjustment *hadjustment;
	GtkAdjustment *vadjustment;
	guint hscroll_policy;
	guint vscroll_policy;

	/* The image we display. Change with the "image" property.
	 */
	VipsImage *image;

	/* Our geometry. 
	 *
	 * image_rect is the bounds of image space .. 0,0 to image->Xsize,
	 * image->Ysize
	 *
	 * widget_rect is the bounds of the widget .. 0,0 to GtkAllocation
	 *
	 * paint_rect is the sub-part of the widget that we paint to .. if we
	 * zoom out a long way, for example, we display the image centred in
	 * the widget. 
	 */
	VipsRect image_rect;
	VipsRect widget_rect;
	VipsRect paint_rect;

	/* A backing buffer the size of paint_rect. We use
	 * this from the draw handler to paint the screen, and we also paint to
	 * this from libvips as it calculates pixels.
	 *
	 * This is always Cairo-style ARGB.
	 */
	unsigned char *cairo_buffer;

	/* left/top is the position of the top-left corner of paint_rect within
	 * the image. Set from our adjustments. 
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
