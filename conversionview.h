#ifndef __CONVERSIONVIEW_H
#define __CONVERSIONVIEW_H

#define CONVERSIONVIEW_TYPE (conversionview_get_type())

G_DECLARE_FINAL_TYPE( Conversionview, conversionview, 
	VIPSDISP, CONVERSIONVIEW, GtkWidget )

#define CONVERSIONVIEW( obj ) \
	(G_TYPE_CHECK_INSTANCE_CAST( (obj), \
		CONVERSIONVIEW_TYPE, Conversionview ))

Conversionview *conversionview_new( ImageWindow *win );

#endif /* __CONVERSIONVIEW_H */
