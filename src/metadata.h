#ifndef __METADATA_H
#define __METADATA_H

#include "match.h"
#include "metadatautil.h"

#define METADATA_TYPE (metadata_get_type())

G_DECLARE_FINAL_TYPE( Metadata, metadata, VIPSDISP, METADATA, GtkWidget );

#define METADATA( obj ) \
	(G_TYPE_CHECK_INSTANCE_CAST( (obj), METADATA_TYPE, Metadata ))

void metadata_apply( Metadata *m );

Metadata * metadata_new( ImageWindow *image_window );

#endif /* __METADATA_H */
