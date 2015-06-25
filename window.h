
typedef struct _Window {
	GtkApplicationWindow parent_instance;

	Disp *disp;

	VipsImage *image;
	VipsImage *display;
	VipsRegion *region;

	GtkWidget *drawing_area;
} Window;

typedef GtkApplicationWindowClass WindowClass;

Window *window_new( GtkApplication *application, GFile *file );
