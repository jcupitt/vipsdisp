
typedef struct _Imagedisplay {
	GtkDrawingArea parent_instance;

	VipsImage *image;
	VipsImage *display;
	VipsRegion *region;
} Imagedisplay;

typedef GtkDrawingAreaClass ImagedisplayClass;

int imagedisplay_set_file( Imagedisplay *imagedisplay, GFile *file );
Imagedisplay *imagedisplay_new( void ); 
