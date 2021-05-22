
typedef struct _Imageview {
	GtkApplicationWindow parent_instance;

	Disp *disp;

	GtkWidget *header_bar;

	GtkWidget *progress_box;
	GtkWidget *progress;

	GtkWidget *error_box;
	GtkWidget *error_label;

	Imagepresent *imagepresent;

	Conversionview *conversionview;

	Infobar *infobar;

} Imageview;

typedef GtkApplicationWindowClass ImageviewClass;

void imageview_status_update( Imageview *imageview );

Imageview *imageview_new( GtkApplication *application );

void imageview_set_file( Imageview *imageview, GFile *file );
void imageview_set_source( Imageview *imageview, VipsSource *source );
void imageview_set_conversion( Imageview *imageview, Conversion *conversion );
