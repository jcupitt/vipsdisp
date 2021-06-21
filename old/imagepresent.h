
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
	GtkWidget parent_instance;

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

	/* Rightclick menu we display.
	GtkMenu *menu;
	 */

} Imagepresent;

typedef struct _ImagepresentClass {
	GtkWidgetClass parent_class;

	void (*position_changed)( Imagepresent *imagepresent ); 

} ImagepresentClass;

void imagepresent_get_window_position( Imagepresent *imagepresent, 
	int *left, int *top, int *width, int *height );
void imagepresent_set_window_position( Imagepresent *imagepresent, 
	int left, int top );

gboolean imagepresent_get_image_size( Imagepresent *imagepresent, 
	int *width, int *height );

// void imagepresent_set_menu( Imagepresent *imagepresent, GtkMenu *menu );

int imagepresent_set_mag( Imagepresent *imagepresent, int mag );
void imagepresent_magin( Imagepresent *imagepresent, int x, int y );
void imagepresent_magout( Imagepresent *imagepresent, int x, int y );
void imagepresent_bestfit( Imagepresent *imagepresent );

Imagepresent *imagepresent_new();
