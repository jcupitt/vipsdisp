
typedef struct _Conversionview {
	GtkBox parent_instance;

	Conversion *conversion;

	GtkWidget *page_select;

	Tslider *scale;
	Tslider *offset;

	guint changed_sig;
	guint page_changed_sig;

} Conversionview;

typedef GtkBoxClass ConversionviewClass;

Conversionview *conversionview_new( Conversion *conversion );
