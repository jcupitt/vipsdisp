#ifndef __METADATA_H
#define __METADATA_H

#define METADATA_TYPE (metadata_get_type())

G_DECLARE_FINAL_TYPE( Metadata, metadata, VIPSDISP, METADATA, GtkWidget );

#define METADATA( obj ) \
	(G_TYPE_CHECK_INSTANCE_CAST( (obj), METADATA_TYPE, Metadata ))

Metadata *metadata_new( ImageWindow *win );

gboolean metadata_shrink_window( gpointer user_data );

void metadata_show( Metadata *metadata );
void metadata_hide( Metadata *metadata );

#endif /* __METADATA_H */

