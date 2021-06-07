
typedef struct _Imagedisplay {
	GtkDrawingArea parent_instance;

	/* The conversion whose output we display.
	 */
	Conversion *conversion;

	/* Watch the conversion with these.
	 */
	guint conversion_display_changed_sig;
	guint conversion_area_changed_sig;

	/* We implement a scrollable interface.
	 */
	GtkAdjustment *hadjustment;
	GtkAdjustment *vadjustment;
	guint hscroll_policy;
	guint vscroll_policy;

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

	/* The regions we use for fetching pixels from the rgb image and from
	 * the mask.
	 */
	VipsRegion *rgb_region;
	VipsRegion *mask_region;

} Imagedisplay;

typedef struct _ImagedisplayClass {
	GtkDrawingAreaClass parent_class;

} ImagedisplayClass;

void imagedisplay_image_to_gtk( Imagedisplay *imagedisplay, VipsRect *rect );
void imagedisplay_gtk_to_image( Imagedisplay *imagedisplay, VipsRect *rect );

Imagedisplay *imagedisplay_new( Conversion *conversion ); 
