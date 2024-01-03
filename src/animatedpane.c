/*
*/
#define DEBUG

#include "vipsdisp.h"

/* Duration of the GtkPaned enter/leave animations
 */
#define PANED_DURATION (0.5)

struct _Animatedpane
{
	GtkWidget parent_instance;

	GtkWidget *paned;

	gboolean revealed;

	/* Saved position of the GtkPaned separator.
	 */
	guint paned_position;

	/* Time in seconds since the GtkPaned enter/leave animation started.
	 */
	double paned_progress;

	/* During GtkPaned enter/leave animation, this is the time in seconds
	 * since the last animation frame. When GtkPaned is not animating, this
	 * is set to the sentinel value -1.
	 */
	double paned_last_frame_time;

	/* This flag is set to TRUE before the GtkPaned separator is moved
	 * programatically for animation purposes, and is set to its initial
	 * value FALSE afterwards. It prevents image_window_properties and
	 * image_window_paned_changed from firing.
	 */
	gboolean is_paned_animating;

	/* TRUE if image_window_paned_init has been called at least once,
	 * i.e., if the first image has been opened.
	 */
	gboolean is_paned_init;

	/* The Properties widget is hidden before a new image is opened, to
	 * ensure that it renders full-size. This flag is set to remind
	 * image_window_paned_init to show the Properties widget again.
	 */
	gboolean refresh_paned;

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

	if( prop_id == PROP_REVEALED ) {
		// start to animate open or closed using
		// g_value_get_boolean( value )
	}
	else if( prop_id < PROP_REVEALED ) 
		g_object_set_property( G_OBJECT( animatedpane->paned ), 
				prop_names[prop_id], value );
	else
		G_OBJECT_WARN_INVALID_PROPERTY_ID( object, prop_id, pspec );
}

static void
animatedpane_get_property( GObject *object, 
	guint prop_id, GValue *value, GParamSpec *pspec )
{
	Animatedpane *animatedpane = ANIMATEDPANE( object );

	if( prop_id == PROP_REVEALED ) 
		g_value_set_boolean( value, animatedpane->revealed ); 
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

#ifdef DEBUG
	printf( "animatedpane_position_notify:\n" );
#endif /* DEBUG */

	if( animatedpane->revealed &&
		!animatedpane->is_paned_animating ) {
		// FIXME ... could get from pspec?
		animatedpane->paned_position = 
			gtk_paned_get_position( GTK_PANED( paned ) );

#ifdef DEBUG
		printf( "animatedpane_position_notify: new position %d\n",
			 animatedpane->paned_position );
#endif /* DEBUG */
	}
}

static void
animatedpane_init( Animatedpane *animatedpane )
{
#ifdef DEBUG
	printf( "animatedpane_init:\n" ); 
#endif /*DEBUG*/

	// it'd be nice to create it in animatedpane.ui, but we need this
	// pointer to be valid sooner than that
	animatedpane->paned = gtk_paned_new( GTK_ORIENTATION_HORIZONTAL );
	gtk_widget_set_parent( animatedpane->paned, GTK_WIDGET( animatedpane ) );

	gtk_widget_init_template( GTK_WIDGET( animatedpane ) );

	g_signal_connect( animatedpane->paned, "notify::position",
		G_CALLBACK( animatedpane_position_notify ), animatedpane );
}

// copy-paste from gtkpaned to get the GTK property flags
#define GTK_PARAM_READABLE G_PARAM_READABLE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB
#define GTK_PARAM_READWRITE G_PARAM_READWRITE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB

static void
animatedpane_class_init( AnimatedpaneClass *class )
{
	GObjectClass *gobject_class = G_OBJECT_CLASS( class );
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS( class );

#ifdef DEBUG
	printf( "animatedpane_class_init:\n" ); 
#endif /*DEBUG*/

	// FIXME ... do we really need this?
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

