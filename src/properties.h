#ifndef __PROPERTIES_H
#define __PROPERTIES_H

#define PROPERTIES_TYPE (properties_get_type())

G_DECLARE_FINAL_TYPE(Properties, properties, VIPSDISP, PROPERTIES, GtkWidget);

#define PROPERTIES VIPSDISP_PROPERTIES

#endif /* __PROPERTIES_H */
