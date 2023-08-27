#ifndef __METADATA_H
#define __METADATA_H

#define METADATA_TYPE (metadata_get_type())

G_DECLARE_FINAL_TYPE( Metadata, metadata, 
	VIPSDISP, METADATA, GtkDialog );

Metadata *metadata_new( GtkWindow *parent_window,
	VipsImage *in, const char *filename );

#endif /* __METADATA_H */

