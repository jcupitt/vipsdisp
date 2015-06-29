
typedef struct _Imagedisplay {
	GtkDrawingArea parent_instance;

	VipsImage *image;
	VipsImage *display;
	VipsRegion *region;
} Imagedisplay;

typedef GtkDrawingAreaClass ImagedisplayClass;

Imagedisplay *imagedisplay_new( GFile *file );
