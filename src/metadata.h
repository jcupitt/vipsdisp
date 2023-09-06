#ifndef __METADATA_H
#define __METADATA_H

#define METADATA_TYPE (metadata_get_type())

G_DECLARE_FINAL_TYPE( Metadata, metadata, VIPSDISP, METADATA, GtkWidget );

#define METADATA( obj ) \
	(G_TYPE_CHECK_INSTANCE_CAST( (obj), METADATA_TYPE, Metadata ))

Metadata *metadata_new( ImageWindow *win );

#endif /* __METADATA_H */

