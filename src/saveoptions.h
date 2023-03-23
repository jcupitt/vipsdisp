#ifndef __SAVEOPTIONS_H
#define __SAVEOPTIONS_H

#define SAVEOPTIONS_TYPE (saveoptions_get_type())

G_DECLARE_FINAL_TYPE( Saveoptions, saveoptions, VIPSDISP, SAVEOPTIONS, GtkWidget )

#define SAVEOPTIONS( obj ) \
	(G_TYPE_CHECK_INSTANCE_CAST( (obj), SAVEOPTIONS_TYPE, Saveoptions ))

Saveoptions *saveoptions_new( ImageWindow *win );

void
saveoptions_build_save_operation( Saveoptions *saveoptions,
	VipsOperation *operation );

#endif /* __SAVEOPTIONS_H */
