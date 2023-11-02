#ifndef __METADATAUTIL_H
#define __METADATAUTIL_H

guint glev( guint n1, gchar s1[n1], guint n2, gchar s2[n2], guint v[n1 + 1] );

GtkWidget * create_spin_button( double min, double max, double step,
		double value, bool scroll );

void metadata_util_apply_input( GtkWidget *t, VipsImage *image, char* field );

GtkWidget * metadata_util_create_label_box( GList *ma_list );

GtkWidget * metadata_util_create_input_box( VipsImage *image, char* field_name );

GtkWidget * metadata_util_create_simple_label_box( gchar *s );

#endif /* __METADATAUTIL_H */
