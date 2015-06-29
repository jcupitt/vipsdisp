
typedef struct _Imagepresent {
	GtkApplicationWindow parent_instance;

	Disp *disp;

	VipsImage *image;
	VipsImage *display;
	VipsRegion *region;

	GtkWidget *drawing_area;
} Imagepresent;

typedef GtkApplicationWindowClass ImagepresentClass;

Imagepresent *imagepresent_new( GtkApplication *application, GFile *file );
