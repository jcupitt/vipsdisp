
typedef struct _Imagepresent {
	GtkScrolledWindow parent_instance;

	GFile *file;

	Imagedisplay *imagedisplay;

	/* Last mouse position we saw, in display image coordinates.
	 */
	int last_x;
	int last_y;

} Imagepresent;

typedef GtkScrolledWindowClass ImagepresentClass;

gboolean imagepresent_get_image_size( Imagepresent *imagepresent, 
	int *width, int *height );
void imagepresent_get_window_size( Imagepresent *imagepresent, 
	int *width, int *height );

void imagepresent_set_mag( Imagepresent *imagepresent, int mag );
void imagepresent_magin( Imagepresent *imagepresent );
void imagepresent_magout( Imagepresent *imagepresent );
void imagepresent_bestfit( Imagepresent *imagepresent );

char *imagepresent_get_path( Imagepresent *imagepresent );
int imagepresent_set_file( Imagepresent *imagepresent, GFile *file );

Imagepresent *imagepresent_new();
