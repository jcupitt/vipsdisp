
/* Our state. 
 */
typedef enum _ImagepresentState {
	/* Base doing-nothing state.
	 */
	IMAGEPRESENT_WAIT,

	/* Doing a left-mouse-drag action.
	 */
	IMAGEPRESENT_DRAG,

	IMAGEPRESENT_LAST
} ImagepresentState;

typedef struct _Imagepresent {
	GtkScrolledWindow parent_instance;

	GFile *file;

	Imagedisplay *imagedisplay;

	Conversion *conversion;

	/* Last mouse position we saw, in display image coordinates.
	 */
	int last_x;
	int last_y;

	/* Current state.
	 */
	ImagepresentState state;

	/* For DRAG, the mouse x/y when we started the drag, in root window
	 * coordinates.
	 */
	int drag_start_x;
	int drag_start_y;

} Imagepresent;

typedef struct _ImagepresentClass {
	GtkScrolledWindowClass parent_class;

	void (*position_changed)( Imagepresent *imagepresent ); 

} ImagepresentClass;

void imagepresent_get_window_position( Imagepresent *imagepresent, 
	int *left, int *top, int *width, int *height );

gboolean imagepresent_get_image_size( Imagepresent *imagepresent, 
	int *width, int *height );

void imagepresent_set_mag( Imagepresent *imagepresent, int mag );
void imagepresent_magin( Imagepresent *imagepresent, int x, int y );
void imagepresent_magout( Imagepresent *imagepresent );
void imagepresent_bestfit( Imagepresent *imagepresent );

char *imagepresent_get_path( Imagepresent *imagepresent );
int imagepresent_set_file( Imagepresent *imagepresent, GFile *file );

Imagepresent *imagepresent_new();
