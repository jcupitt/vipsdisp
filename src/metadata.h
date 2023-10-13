#ifndef __METADATA_H
#define __METADATA_H

#include "match.h"
#include "metadatautil.h"

#define METADATA_TYPE (metadata_get_type())

G_DECLARE_FINAL_TYPE( Metadata, metadata, VIPSDISP, METADATA, GtkWidget );

#define METADATA( obj ) \
	(G_TYPE_CHECK_INSTANCE_CAST( (obj), METADATA_TYPE, Metadata ))

GtkGrid * metadata_create_grid( Metadata *m );

void metadata_show( Metadata *m );

gboolean metadata_shrink_window( gpointer user_data );

void metadata_hide( Metadata *m );

void metadata_apply( Metadata *m );

void metadata_append_field( gpointer data, gpointer user_data );

void metadata_append_markup_field( gpointer data, gpointer m_ );

Metadata * metadata_new( ImageWindow *image_window );

#endif /* __METADATA_H */
