
typedef struct _Imageview {
	GtkApplicationWindow parent_instance;

	Disp *disp;

	GtkWidget *header_bar;
	Imagepresent *imagepresent;

} Imageview;

typedef GtkApplicationWindowClass ImageviewClass;

Imageview *imageview_new( GtkApplication *application, GFile *file );
