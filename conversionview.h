
typedef struct _Conversionview {
	GtkBox parent_instance;

	Conversion *conversion;

} Conversionview;

typedef GtkBoxClass ConversionviewClass;

Conversionview *conversionview_new( Conversion *conversion );
