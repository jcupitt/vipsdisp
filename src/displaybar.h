#ifndef __DISPLAYBAR_H
#define __DISPLAYBAR_H

#define DISPLAYBAR_TYPE (displaybar_get_type())

G_DECLARE_FINAL_TYPE( Displaybar, displaybar, 
	VIPSDISP, DISPLAYBAR, GtkWidget )

#define DISPLAYBAR( obj ) \
	(G_TYPE_CHECK_INSTANCE_CAST( (obj), \
		DISPLAYBAR_TYPE, Displaybar ))

Displaybar *displaybar_new( ImageWindow *win );

#endif /* __DISPLAYBAR_H */
