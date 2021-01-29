
typedef struct _Conversionview {
	GtkBox parent_instance;

	Conversion *conversion;

	Tslider *scale;
	Tslider *offset;

} Conversionview;

typedef GtkBoxClass ConversionviewClass;

Conversionview *conversionview_new( Conversion *conversion );
