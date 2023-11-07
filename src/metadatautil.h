#ifndef __METADATAUTIL_H
#define __METADATAUTIL_H

GtkWidget * create_spin_button( double min, double max, double step,
		double value, bool scroll );

void metadata_util_apply_input( GtkWidget *t, VipsImage *image, const gchar* field );

GtkWidget * metadata_util_create_label_box( GList *ma_list );

GtkWidget * metadata_util_create_input_box( VipsImage *image, const gchar* field_name );

GtkWidget * metadata_util_create_simple_label_box( const gchar *s );

#endif /* __METADATAUTIL_H */
