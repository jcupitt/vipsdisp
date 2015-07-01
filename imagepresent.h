
typedef struct _Imagepresent {
	GtkApplicationWindow parent_instance;

	Disp *disp;
	GFile *file;

	Imagedisplay *imagedisplay;
	GtkWidget *header_bar;

} Imagepresent;

typedef GtkApplicationWindowClass ImagepresentClass;

Imagepresent *imagepresent_new( GtkApplication *application, GFile *file );
