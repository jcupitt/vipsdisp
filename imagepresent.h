
typedef struct _Imagepresent {
	GtkApplicationWindow parent_instance;

	Disp *disp;

	Imagedisplay *imagedisplay;
} Imagepresent;

typedef GtkApplicationWindowClass ImagepresentClass;

Imagepresent *imagepresent_new( GtkApplication *application, GFile *file );
