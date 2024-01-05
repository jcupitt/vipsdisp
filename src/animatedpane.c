/*
#define DEBUG
 */

#include "vipsdisp.h"

/* Duration of the GtkPaned enter/leave animations
 */
#define PANED_DURATION (0.5)

struct _Animatedpane
{
	GtkWidget parent_instance;

	GtkWidget *paned;

	// right hand pane is visible
	gboolean revealed;

	// distance of the divider from the RIGHT edge of the window
	int position;

	// animation state.
	double elapsed;
	double last_frame_time;
	gboolean is_animating;

	/* The positions we animate between.
	 */
	int start;
	int stop;
};

enum {
	// all the gtkpaned properties ... we don't use the same numbering
	PROP_POSITION = 1,
	PROP_POSITION_SET,
	PROP_MIN_POSITION,
	PROP_MAX_POSITION,
	PROP_WIDE_HANDLE,
	PROP_RESIZE_START_CHILD,
	PROP_RESIZE_END_CHILD,
	PROP_SHRINK_START_CHILD,
	PROP_SHRINK_END_CHILD,
	PROP_START_CHILD,
	PROP_END_CHILD,
	PROP_ORIENTATION,

	// the one we add
	PROP_REVEALED,

	PROP_LAST
};

static void animatedpane_buildable_iface_init( GtkBuildableIface *iface );

/* We need to support buildable too.
 */
G_DEFINE_TYPE_WITH_CODE( Animatedpane, animatedpane, GTK_TYPE_WIDGET,
	G_IMPLEMENT_INTERFACE( GTK_TYPE_BUILDABLE, 
		animatedpane_buildable_iface_init ) )

static GtkBuildableIface *parent_buildable_iface;

// map prop enums to and from gtk names
static const char *prop_names[] = {
	"<unused>",	// no prop_id 0
	"position",
	"position-set",
	"min-position",
	"max-position",
	"wide-handle",
	"resize-start-child",
	"resize-end-child",
	"shrink-start-child",
	"shrink-end-child",
	"start-child",
	"end-child",
	"orientation",
	"revealed"
};

// try to respect the animations on/off flag
static gboolean
animatedpaned_enable_animations( Animatedpane *animatedpane )
{
    gboolean enable_animations;

    g_object_get( gtk_widget_get_settings( GTK_WIDGET( animatedpane ) ), 
		"gtk-enable-animations", &enable_animations, 
		NULL );

    return( enable_animations );
}

static void
animatedpaned_set_child_visibility( Animatedpane *animatedpane, 
	gboolean revealed )
{
#ifdef DEBUG
    printf( "animatedpaned_set_child_visibility: %d\n", revealed );
#endif /* DEBUG */

	GtkWidget *child = 
		gtk_paned_get_end_child( GTK_PANED( animatedpane->paned ) );

	if( revealed ) 
		gtk_widget_show( child );
	else
		gtk_widget_hide( child );

}

static void
animatedpaned_set_child_position( Animatedpane *animatedpane, int position )
{
	// our position is distance from the right edge -- we must swap this
	int widget_width = gtk_widget_get_width( GTK_WIDGET( animatedpane ) );
	// widget_width will be zero on startup and we want the thing to come in
	// from the right, so we need a large value
	int paned_position = widget_width == 0 ? 10000 : widget_width - position;

	gtk_paned_set_position( GTK_PANED( animatedpane->paned ), 
		paned_position );
}

/* From clutter-easing.c, based on Robert Penner's infamous easing equations,
 * MIT license.
 */
static double
ease_out_cubic( double t )
{
	double p = t - 1;

	return( p * p * p + 1 );
}

static gboolean
animatedpane_animate_tick( GtkWidget *widget, GdkFrameClock *frame_clock,
    gpointer client_data )
{
	Animatedpane *animatedpane = ANIMATEDPANE( widget );
    gint64 frame_time = gdk_frame_clock_get_frame_time( frame_clock );

	double t;

    gint64 dt = animatedpane->last_frame_time > 0 ?
        frame_time - animatedpane->last_frame_time :
        0;
	animatedpane->last_frame_time = frame_time;

    animatedpane->elapsed += (double) dt / G_TIME_SPAN_SECOND;

    t = ease_out_cubic( animatedpane->elapsed / PANED_DURATION );

#ifdef DEBUG
    printf( "animatedpane_animate_tick: elapsed = %g\n", 
			animatedpane->elapsed );
#endif /* DEBUG */

	if( t >= 0.99 ) {
		// all done
		animatedpaned_set_child_position( animatedpane, animatedpane->stop );
		if( !animatedpane->revealed )
			animatedpaned_set_child_visibility( animatedpane, FALSE );
		animatedpane->is_animating = FALSE;

		return( G_SOURCE_REMOVE );
	}
	else {
		gint position = animatedpane->start + 
			t * (animatedpane->stop - animatedpane->start);

        animatedpaned_set_child_position( animatedpane, position );

        return( G_SOURCE_CONTINUE );
    }
}

static void
animatedpaned_set_revealed( Animatedpane *animatedpane, gboolean revealed )
{
#ifdef DEBUG
    printf( "animatedpaned_set_revealed: %d\n", revealed );
#endif /* DEBUG */

	if( animatedpane->revealed != revealed ) {
		animatedpane->revealed = revealed;

		if( animatedpaned_enable_animations( animatedpane ) ) {
			animatedpane->last_frame_time = -1;
			animatedpane->elapsed = 0.0;
			animatedpane->is_animating = TRUE;

			animatedpane->start = animatedpane->position;
			animatedpane->stop = 0;
			if( revealed )
				VIPS_SWAP( int, animatedpane->start, animatedpane->stop );

			animatedpaned_set_child_position( animatedpane,
				animatedpane->start );

			if( revealed )
				animatedpaned_set_child_visibility( animatedpane, TRUE );

			gtk_widget_add_tick_callback( GTK_WIDGET( animatedpane ),
				animatedpane_animate_tick, NULL, NULL );
		}
		else 
			animatedpaned_set_child_visibility( animatedpane, revealed );
	}
}

static void
animatedpane_dispose( GObject *object )
{
	Animatedpane *animatedpane = ANIMATEDPANE( object );

#ifdef DEBUG
	printf( "animatedpane_dispose:\n" ); 
#endif /*DEBUG*/

	VIPS_FREEF( gtk_widget_unparent, animatedpane->paned );

	G_OBJECT_CLASS( animatedpane_parent_class )->dispose( object );
}

#ifdef DEBUG
static const char *
animatedpane_property_name( guint prop_id )
{
	if( prop_id < PROP_LAST )
		return( prop_names[prop_id] );
	else
		return( "<unknown>" );
}
#endif /*DEBUG*/

static void
animatedpane_set_property( GObject *object, 
	guint prop_id, const GValue *value, GParamSpec *pspec )
{
	Animatedpane *animatedpane = ANIMATEDPANE( object );

#ifdef DEBUG
{
	char *str;

	str = g_strdup_value_contents( value );
	printf( "animatedpane_set_property: %s %s\n", 
		animatedpane_property_name( prop_id ), str ); 
	g_free( str );
}
#endif /*DEBUG*/

	if( prop_id == PROP_REVEALED ) 
		animatedpaned_set_revealed( animatedpane, 
			g_value_get_boolean( value ) );
	else if( prop_id == PROP_POSITION ) {
		animatedpane->position = g_value_get_int( value );
		animatedpaned_set_child_position( animatedpane, 
			animatedpane->position );
	}
	else if( prop_id < PROP_REVEALED ) 
		g_object_set_property( G_OBJECT( animatedpane->paned ), 
			prop_names[prop_id], value );
	else
		G_OBJECT_WARN_INVALID_PROPERTY_ID( object, prop_id, pspec );

	// if we just added the child, set the initial visibility
	if( prop_id == PROP_END_CHILD ) 
		animatedpaned_set_child_visibility( animatedpane, 
			animatedpane->revealed );
}

static void
animatedpane_get_property( GObject *object, 
	guint prop_id, GValue *value, GParamSpec *pspec )
{
	Animatedpane *animatedpane = ANIMATEDPANE( object );

	if( prop_id == PROP_REVEALED ) 
		g_value_set_boolean( value, animatedpane->revealed ); 
	else if( prop_id == PROP_POSITION ) 
		g_value_set_int( value, animatedpane->position ); 
	else if( prop_id < PROP_REVEALED ) 
		g_object_get_property( G_OBJECT( animatedpane->paned ), 
				prop_names[prop_id], value );
	else
		G_OBJECT_WARN_INVALID_PROPERTY_ID( object, prop_id, pspec );

#ifdef DEBUG
{
	char *str;

	str = g_strdup_value_contents( value );
	printf( "animatedpane_get_property: %s %s\n", 
			animatedpane_property_name( prop_id ), str ); 
	g_free( str );
}
#endif /*DEBUG*/
}

static void
animatedpane_position_notify( GtkWidget *paned,
	GParamSpec *pspec, gpointer user_data )
{
	GtkWidget *parent = gtk_widget_get_parent( paned );
	Animatedpane *animatedpane = ANIMATEDPANE( parent );

	if( animatedpane->revealed &&
		!animatedpane->is_animating ) {
		// FIXME ... could get from pspec?
		int paned_position = gtk_paned_get_position( GTK_PANED( paned ) );

		// our position is distance from the right edge -- we must swap this
		int widget_width = gtk_widget_get_width( GTK_WIDGET( animatedpane ) );
		int position = widget_width - paned_position;

#ifdef DEBUG
		printf( "animatedpane_position_notify: new position %d\n", position );
#endif /* DEBUG */

		if( animatedpane->position != position )
			g_object_set( animatedpane, "position", position, NULL );
	}
}

static void
animatedpane_init( Animatedpane *animatedpane )
{
#ifdef DEBUG
	printf( "animatedpane_init:\n" ); 
#endif /*DEBUG*/

	// distance of the divider from the RIGHT edge of the window
	animatedpane->position = 500;

	// it'd be nice to create our gtkpaned in animatedpane.ui, but we need 
	// this pointer during builder 
	animatedpane->paned = gtk_paned_new( GTK_ORIENTATION_HORIZONTAL );
	gtk_widget_set_parent( animatedpane->paned, GTK_WIDGET( animatedpane ) );

	gtk_widget_init_template( GTK_WIDGET( animatedpane ) );

	g_signal_connect( animatedpane->paned, "notify::position",
		G_CALLBACK( animatedpane_position_notify ), animatedpane );
}

// copy-paste from gtkpaned to get the GTK property flags
#define GTK_PARAM_READABLE \
	(G_PARAM_READABLE | \
	 G_PARAM_STATIC_NAME | \
	 G_PARAM_STATIC_NICK | \
	 G_PARAM_STATIC_BLURB)

#define GTK_PARAM_READWRITE \
	(G_PARAM_READWRITE | \
	 G_PARAM_STATIC_NAME | \
	 G_PARAM_STATIC_NICK | \
	 G_PARAM_STATIC_BLURB)

static void
animatedpane_class_init( AnimatedpaneClass *class )
{
	GObjectClass *gobject_class = G_OBJECT_CLASS( class );
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS( class );

#ifdef DEBUG
	printf( "animatedpane_class_init:\n" ); 
#endif /*DEBUG*/

	gtk_widget_class_set_layout_manager_type( widget_class,
        GTK_TYPE_BIN_LAYOUT );
    gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( class ),
        APP_PATH "/animatedpane.ui");

	gobject_class->dispose = animatedpane_dispose;
	gobject_class->set_property = animatedpane_set_property;
	gobject_class->get_property = animatedpane_get_property;

	// the GtkPaned properties we support
	g_object_class_install_property( gobject_class, PROP_POSITION,
		g_param_spec_int( "position", NULL, NULL,
			0, G_MAXINT, 0,
			GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY ) );

	g_object_class_install_property( gobject_class, PROP_POSITION_SET,
		g_param_spec_boolean( "position-set", NULL, NULL,
			FALSE,
			GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY ) );

	g_object_class_install_property( gobject_class, PROP_MIN_POSITION,
		g_param_spec_int( "min-position", NULL, NULL,
			0, G_MAXINT, 0,
			GTK_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY ) );

	g_object_class_install_property( gobject_class, PROP_MAX_POSITION,
		g_param_spec_int( "max-position", NULL, NULL,
			0, G_MAXINT, G_MAXINT,
			GTK_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY ) );

	g_object_class_install_property( gobject_class, PROP_WIDE_HANDLE,
		g_param_spec_boolean( "wide-handle", NULL, NULL,
			FALSE,
			GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY ) );

	g_object_class_install_property( gobject_class, PROP_RESIZE_START_CHILD,
		g_param_spec_boolean( "resize-start-child", NULL, NULL,
			TRUE,
			GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY ) );

	g_object_class_install_property( gobject_class, PROP_RESIZE_END_CHILD,
		g_param_spec_boolean( "resize-end-child", NULL, NULL,
			TRUE,
			GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY ) );

	g_object_class_install_property( gobject_class, PROP_SHRINK_START_CHILD,
		g_param_spec_boolean( "shrink-start-child", NULL, NULL,
			TRUE,
			GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY ) );

	g_object_class_install_property( gobject_class, PROP_SHRINK_END_CHILD,
		g_param_spec_boolean( "shrink-end-child", NULL, NULL,
			TRUE,
			GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY ) );

	g_object_class_install_property( gobject_class, PROP_START_CHILD,
		g_param_spec_object( "start-child", NULL, NULL,
			GTK_TYPE_WIDGET,
			GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY ) );

	g_object_class_install_property( gobject_class, PROP_END_CHILD,
		g_param_spec_object( "end-child", NULL, NULL,
			GTK_TYPE_WIDGET,
			GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY ) );

	// not defined by gtkpaned -- this is from the orientable interface, but
	// we need to forward it anyway
	g_object_class_install_property( gobject_class, PROP_ORIENTATION,
		g_param_spec_enum( "orientation", NULL, NULL,
			GTK_TYPE_ORIENTATION,
			GTK_ORIENTATION_HORIZONTAL,
			GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY ) );

	// the one property we add
	g_object_class_install_property( gobject_class, PROP_REVEALED,
		g_param_spec_boolean( "revealed",
			_( "revealed" ),
			_( "Show the pane child" ),
			FALSE,
			G_PARAM_READWRITE ) );
}

static void
animatedpane_buildable_add_child( GtkBuildable *buildable,
	GtkBuilder *builder, GObject *child, const char *type )
{
#ifdef DEBUG
	printf( "animatedpane_buildable_add_child: %s\n", type ); 
#endif /*DEBUG*/

	Animatedpane *self = ANIMATEDPANE( buildable );
	GtkBuildable *paned_buildable = GTK_BUILDABLE( self->paned );
	GtkBuildableIface *paned_iface = GTK_BUILDABLE_GET_IFACE( self->paned );

    paned_iface->add_child( paned_buildable, builder, child, type );
}

static void
animatedpane_buildable_iface_init( GtkBuildableIface *iface )
{
	parent_buildable_iface = g_type_interface_peek_parent( iface );
	iface->add_child = animatedpane_buildable_add_child;
}

