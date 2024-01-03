#ifndef __ANIMATEDPANE_H
#define __ANIMATEDPANE_H

#define ANIMATEDPANE_TYPE (animatedpane_get_type())

// not correct, but gtk does not export this
typedef struct _GtkPanedClass { 
	GtkWidgetClass parent_class;

	// true for gtk master at least
	void *used_by_gtk[6];
} GtkPanedClass;

G_DECLARE_FINAL_TYPE( Animatedpane, animatedpane, 
	VIPSDISP, ANIMATEDPANE, GtkPaned )

#define ANIMATEDPANE( obj ) \
	(G_TYPE_CHECK_INSTANCE_CAST( (obj), \
		ANIMATEDPANE_TYPE, Animatedpane ))

Animatedpane *animatedpane_new( void );

#endif /* __ANIMATEDPANE_H */
