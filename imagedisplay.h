
typedef struct _Imagedisplay {
	GtkDrawingArea parent_instance;

	VipsImage *image;
	VipsImage *display;
	VipsRegion *region;

	int mag;
} Imagedisplay;

typedef GtkDrawingAreaClass ImagedisplayClass;

int imagedisplay_set_file( Imagedisplay *imagedisplay, GFile *file );

void imagedisplay_set_mag( Imagedisplay *imagedisplay, int mag );
int imagedisplay_get_mag( Imagedisplay *imagedisplay );

Imagedisplay *imagedisplay_new( void ); 
