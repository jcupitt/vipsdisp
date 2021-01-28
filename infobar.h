
typedef struct _Infobar {
	GtkBox parent_instance;

	Imagepresent *imagepresent;

	GtkWidget *info_label;
	GtkWidget *coord_label;
	GtkWidget *value_label;
	GtkWidget *mag_label;

} Infobar;

typedef GtkVBoxClass InfobarClass;

Infobar *infobar_new( Imagepresent *imagepresent );
