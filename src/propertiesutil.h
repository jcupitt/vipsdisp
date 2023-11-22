#ifndef __PROPERTIESUTIL_H
#define __PROPERTIESUTIL_H

GtkWidget * create_spin_button( double min, double max, double step,
		double value, bool scroll );

void properties_util_apply_input( GtkWidget *t, VipsImage *image, const gchar* field );

void properties_util_free_label_box( GtkWidget *label_box );

GtkWidget * properties_util_create_label_box( GList *ma_list );

void properties_util_free_input_box( GtkWidget *input_box );

GtkWidget * properties_util_create_input_box( VipsImage *image, const gchar* field_name );

GtkWidget * properties_util_create_simple_label_box( const gchar *s );

#endif /* __PROPERTIESUTIL_H */
