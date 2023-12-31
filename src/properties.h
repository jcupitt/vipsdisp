#ifndef __PROPERTIES_H
#define __PROPERTIES_H

#define PROPERTIES_TYPE (properties_get_type())

G_DECLARE_FINAL_TYPE( Properties, properties, VIPSDISP, PROPERTIES, GtkWidget );

#define PROPERTIES( obj ) \
	(G_TYPE_CHECK_INSTANCE_CAST( (obj), PROPERTIES_TYPE, Properties ))

void properties_apply( Properties *p );

#endif /* __PROPERTIES_H */
