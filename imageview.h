
typedef struct _Imageview {
	GtkApplicationWindow parent_instance;

	Disp *disp;

	GtkWidget *header_bar;
	Imagepresent *imagepresent;
	GtkWidget *progress_info;
	GtkWidget *progress;
	GtkWidget *error_info;
	GtkWidget *error_label;

} Imageview;

typedef GtkApplicationWindowClass ImageviewClass;

Imageview *imageview_new( GtkApplication *application, GFile *file );
