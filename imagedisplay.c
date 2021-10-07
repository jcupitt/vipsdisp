#include "vipsdisp.h"

/*
#define DEBUG_VERBOSE
#define DEBUG
 */

/* The size of the checkerboard pattern we use for compositing. Must be a
 * power of two.
 */
#define CHECK_SIZE 16

struct _Imagedisplay {
	GtkDrawingArea parent_instance;

	/* The tilecache whose output we display.
	 */
	TileCache *tile_cache;

	/* We implement a scrollable interface.
	 */
	GtkAdjustment *hadj;
	GtkAdjustment *vadj;
	guint hscroll_policy;
	guint vscroll_policy;

	/* Our geometry. 
	 *
	 * image_rect is the bounds of image space .. 0,0 to image->Xsize,
	 * image->Ysize
	 *
	 * widget_rect is the bounds of the widget .. 0,0 to GtkAllocation
	 *
	 * paint_rect is the sub-part of the widget that we paint to .. if we
	 * zoom out a long way, for example, we display the image centred in
	 * the widget. 
	 */
	VipsRect image_rect;
	VipsRect widget_rect;
	VipsRect paint_rect;

	/* Our viewport. This is the area of the tilecache we render to
	 * widget_rect. Left / top can be negative if we're zoomed out a long
	 * way. Width / height can be very small if we're zxooomed right in,
	 * or very large if we're zoomed out.
	 */
	double left;
	double top;
	double width;
	double height;

	/* left/top is the position of the top-left corner of paint_rect within
	 * the image. Set from our adjustments. 
	 */
	int left;
	int top;

};

/* imagedisplay is actually a drawing area the size of the widget on screen: we 
 * do all scrolling ourselves.
 */
G_DEFINE_TYPE_WITH_CODE( Imagedisplay, imagedisplay, GTK_TYPE_DRAWING_AREA,
	G_IMPLEMENT_INTERFACE( GTK_TYPE_SCROLLABLE, NULL ) );

enum {
	/* Set the tile_cache we display.
	 */
	PROP_TILE_CACHE = 1,

	/* The props we implement for the scrollable interface.
	 */
	PROP_HADJUSTMENT,
	PROP_HSCROLL_POLICY,
	PROP_VADJUSTMENT,
	PROP_VSCROLL_POLICY,

	SIG_LAST
};

static void
imagedisplay_dispose( GObject *object )
{
	Imagedisplay *imagedisplay = (Imagedisplay *) object;

#ifdef DEBUG
	printf( "imagedisplay_dispose:\n" ); 
#endif /*DEBUG*/

	VIPS_UNREF( imagedisplay->tile_cache );

	G_OBJECT_CLASS( imagedisplay_parent_class )->dispose( object );
}

static void
imagedisplay_adjustment_changed( GtkAdjustment *adjustment, 
	Imagedisplay *imagedisplay )
{
	if( gtk_widget_get_realized( GTK_WIDGET( imagedisplay ) ) ) {
		double left = 
			gtk_adjustment_get_value( imagedisplay->hadj );
		double top = 
			gtk_adjustment_get_value( imagedisplay->vadj );
		double width = 
			gtk_adjustment_get_page_size( imagedisplay->hadj );
		double height = 
			gtk_adjustment_get_page_size( imagedisplay->vadj );

		VipsRect viewport;

#ifdef DEBUG
		printf( "imagedisplay_adjustment_changed: %g x %g\n", 
			left, top ); 
#endif /*DEBUG*/

		imagedisplay->left = left;
		imagedisplay->top = top;

		viewport.left = left;
		viewport.top = top;
		viewport.width = width;
		viewport.height = height;

		tile_cache_set_viewport( imagedisplay->tile_cache, 
			&viewport, 0 );
	}
}

static gboolean
imagedisplay_set_adjustment( Imagedisplay *imagedisplay,
	GtkAdjustment **adjustment_slot, GtkAdjustment *new_adjustment )
{
#ifdef DEBUG
	printf( "imagedisplay_set_adjustment:\n" ); 
#endif /*DEBUG*/

	if( new_adjustment && 
		*adjustment_slot == new_adjustment )
		return( FALSE );

	if( *adjustment_slot ) {
		g_signal_handlers_disconnect_by_func( *adjustment_slot,
			    imagedisplay_adjustment_changed, imagedisplay );
		VIPS_UNREF( *adjustment_slot );
	}

	if( !new_adjustment )
		new_adjustment = 
			gtk_adjustment_new( 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 );

	g_signal_connect( new_adjustment, "value-changed",
		G_CALLBACK( imagedisplay_adjustment_changed ), imagedisplay );
	*adjustment_slot = g_object_ref_sink( new_adjustment );

	return( TRUE );
}

static void
imagedisplay_set_adjustment_values( Imagedisplay *imagedisplay, 
	GtkAdjustment *adjustment, int axis_size, int window_size ) 
{
	double old_value;
	double new_value;
	double new_upper;
	double page_size;

#ifdef DEBUG
	printf( "imagedisplay_set_adjustment_values: window_size = %d, "
		"axis_size = %d\n", axis_size, window_size );
#endif /*DEBUG*/

	old_value = gtk_adjustment_get_value( adjustment );
	page_size = window_size;
	new_upper = VIPS_MAX( axis_size, page_size );

	g_object_set( adjustment,
		"lower", 0.0,
		"upper", new_upper,
		"page-size", page_size,
		"step-increment", page_size * 0.1,
		"page-increment", page_size * 0.9,
		NULL );

	new_value = VIPS_CLIP( 0, old_value, new_upper - page_size );
	if( new_value != old_value )
		gtk_adjustment_set_value( adjustment, new_value );
}

static void
imagedisplay_set_hadjustment_values( Imagedisplay *imagedisplay ) 
{
	imagedisplay_set_adjustment_values( imagedisplay, 
		imagedisplay->hadj, 
		imagedisplay->image_rect.width, 
		imagedisplay->paint_rect.width ); 
}

static void
imagedisplay_set_vadjustment_values( Imagedisplay *imagedisplay ) 
{
	imagedisplay_set_adjustment_values( imagedisplay, 
		imagedisplay->vadj, 
		imagedisplay->image_rect.height, 
		imagedisplay->paint_rect.height ); 
}

static void
imagedisplay_layout( Imagedisplay *imagedisplay )
{
	int buffer_width;
	int buffer_height;

#ifdef DEBUG
	printf( "imagedisplay_layout:\n" );
#endif /*DEBUG*/

	buffer_width = VIPS_MIN( imagedisplay->widget_rect.width,
                imagedisplay->image_rect.width );
	buffer_height = VIPS_MIN( imagedisplay->widget_rect.height,
                imagedisplay->image_rect.height );

	/* If the image is smaller than the widget, centre it.
	 */
	imagedisplay->paint_rect.width = buffer_width;
	imagedisplay->paint_rect.height = buffer_height;
	imagedisplay->paint_rect.left = VIPS_MAX( 0,
		(imagedisplay->widget_rect.width - buffer_width) / 2 ); 
	imagedisplay->paint_rect.top = VIPS_MAX( 0,
		(imagedisplay->widget_rect.height - buffer_height) / 2 ); 

	imagedisplay_set_hadjustment_values( imagedisplay );
	imagedisplay_set_vadjustment_values( imagedisplay );
}

/* Large change, we need to relayout.
 */
static void
imagedisplay_tile_cache_changed( TileCache *tile_cache, 
	Imagedisplay *imagedisplay ) 
{
#ifdef DEBUG
	printf( "imagedisplay_conversion_display_changed:\n" ); 
#endif /*DEBUG*/

	imagedisplay->image_rect.width = tile_cache->tile_source->width;
	imagedisplay->image_rect.height = tile_cache->tile_source->height;

	imagedisplay_layout( imagedisplay );

	gtk_widget_queue_draw( GTK_WIDGET( imagedisplay ) ); 
}

static void
imagedisplay_tile_cache_area_changed( TileCache *tile_cache, VipsRect *dirty, 
	int z, Imagedisplay *imagedisplay ) 
{
#ifdef DEBUG_VERBOSE
	printf( "imagedisplay_tile_cache_area_changed: "
		"left = %d, top = %d, width = %d, height = %dm z = %d\n",
		dirty->left, dirty->top,
		dirty->width, dirty->height,
	     	z );
#endif /*DEBUG_VERBOSE*/

	/* Sadly, gtk4 only has this and we can't redraw areas. Perhaps we
	 * could just renegerate part of the snapshot?
	 */
	gtk_widget_queue_draw( GTK_WIDGET( imagedisplay ) );
}

static void
imagedisplay_set_tile_cache( Imagedisplay *imagedisplay, 
	TileCache *tile_cache )
{
	g_assert( !imagedisplay->tile_cache );

	imagedisplay->tile_cache = tile_cache;
	g_object_ref( imagedisplay->tile_cache );

	g_signal_connect_object( tile_cache, "changed", 
		G_CALLBACK( imagedisplay_tile_cache_changed ), 
		imagedisplay, 0 );
	g_signal_connect_object( tile_cache, "area-changed", 
		G_CALLBACK( imagedisplay_tile_cache_area_changed ), 
		imagedisplay, 0 );
}

static void
imagedisplay_set_property( GObject *object, 
	guint prop_id, const GValue *value, GParamSpec *pspec )
{
	Imagedisplay *imagedisplay = (Imagedisplay *) object;

	switch( prop_id ) {
	case PROP_HADJUSTMENT:
		if( imagedisplay_set_adjustment( imagedisplay, 
			&imagedisplay->hadj, 
			g_value_get_object( value ) ) ) { 
			imagedisplay_set_hadjustment_values( imagedisplay );
			g_object_notify( G_OBJECT( imagedisplay ), 
				"hadjustment" );
		}
		break;

	case PROP_VADJUSTMENT:
		if( imagedisplay_set_adjustment( imagedisplay, 
			&imagedisplay->vadj, 
			g_value_get_object( value ) ) ) { 
			imagedisplay_set_vadjustment_values( imagedisplay );
			g_object_notify( G_OBJECT( imagedisplay ), 
				"vadjustment" );
		}
		break;

	case PROP_HSCROLL_POLICY:
		if( imagedisplay->hscroll_policy != 
			g_value_get_enum( value ) ) {
			imagedisplay->hscroll_policy = 
				g_value_get_enum( value );
			gtk_widget_queue_resize( GTK_WIDGET( imagedisplay ) );
			g_object_notify_by_pspec( object, pspec );
		}
		break;

	case PROP_VSCROLL_POLICY:
		if( imagedisplay->vscroll_policy != 
			g_value_get_enum( value ) ) {
			imagedisplay->vscroll_policy = 
				g_value_get_enum( value );
			gtk_widget_queue_resize( GTK_WIDGET( imagedisplay ) );
			g_object_notify_by_pspec( object, pspec );
		}
		break;

	case PROP_TILE_CACHE:
		imagedisplay_set_tile_cache( imagedisplay, 
			g_value_get_object( value ) );
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID( object, prop_id, pspec );
		break;
	}
}

static void
imagedisplay_get_property( GObject *object, 
	guint prop_id, GValue *value, GParamSpec *pspec )
{
	Imagedisplay *imagedisplay = (Imagedisplay *) object;

	switch( prop_id ) {
	case PROP_HADJUSTMENT:
		g_value_set_object( value, imagedisplay->hadj );
		break;

	case PROP_VADJUSTMENT:
		g_value_set_object( value, imagedisplay->vadj );
		break;

	case PROP_HSCROLL_POLICY:
		g_value_set_enum( value, imagedisplay->hscroll_policy );
		break;

	case PROP_VSCROLL_POLICY:
		g_value_set_enum( value, imagedisplay->vscroll_policy );
		break;

	case PROP_TILE_CACHE:
		g_value_set_object( value, imagedisplay->tile_cache );
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID( object, prop_id, pspec );
		break;
	}
}

static void
imagedisplay_snapshot( GtkWidget *widget, GtkSnapshot *snapshot )
{
	Imagedisplay *imagedisplay = VIPSDISP_IMAGEDISPLAY( widget );

	tile_cache_snapshot( imagedisplay->tile_cache, snapshot );
}

/* Transform between our coordinate spaces:
 *
 * image: libvips coordinates ... we clip against the image size when going to 
 * libvips space.
 *
 * gtk: coordinates for gtk and for cairo drawing .. no clipping.
 *
 * buffer: coordinates for our backing buffer ... clip against buffer size when
 * going to this space.
 */

void
imagedisplay_image_to_gtk( Imagedisplay *imagedisplay, VipsRect *rect )
{
	rect->left -= imagedisplay->left;
	rect->top -= imagedisplay->top;

	rect->left += imagedisplay->paint_rect.left;
	rect->top += imagedisplay->paint_rect.top;
}

void
imagedisplay_gtk_to_image( Imagedisplay *imagedisplay, VipsRect *rect )
{
	rect->left -= imagedisplay->paint_rect.left;
	rect->top -= imagedisplay->paint_rect.top;

	rect->left += imagedisplay->left;
	rect->top += imagedisplay->top;

	vips_rect_intersectrect( rect, &imagedisplay->image_rect, rect ); 
}

static void
imagedisplay_resize( GtkWidget *widget, int width, int height )
{
	Imagedisplay *imagedisplay = (Imagedisplay *) widget;

#ifdef DEBUG
	printf( "imagedisplay_resize: %d x %d\n",
		width, height ); 
#endif /*DEBUG*/

	imagedisplay->widget_rect.width = width;
	imagedisplay->widget_rect.height = height;
	imagedisplay_layout( imagedisplay );
}

static void
imagedisplay_focus_enter( GtkEventController *controller, 
	Imagedisplay *imagedisplay )
{
	gtk_widget_queue_draw( GTK_WIDGET( imagedisplay ) ); 
}

static void
imagedisplay_focus_leave( GtkEventController *controller, 
	Imagedisplay *imagedisplay )
{
	gtk_widget_queue_draw( GTK_WIDGET( imagedisplay ) ); 
}

static void 
imagedisplay_click( GtkEventController *controller, 
	int n_press, double x, double y, Imagedisplay *imagedisplay )
{
	gtk_widget_grab_focus( GTK_WIDGET( imagedisplay ) );
}

static void
imagedisplay_init( Imagedisplay *imagedisplay )
{
	GtkEventController *controller;

#ifdef DEBUG
	printf( "imagedisplay_init:\n" ); 
#endif /*DEBUG*/

	gtk_widget_set_focusable( GTK_WIDGET( imagedisplay ), TRUE );
	g_signal_connect( GTK_DRAWING_AREA( imagedisplay ), "resize",
		G_CALLBACK( imagedisplay_resize ), NULL );

	controller = gtk_event_controller_focus_new();
	g_signal_connect( controller, "enter", 
		G_CALLBACK( imagedisplay_focus_enter ), imagedisplay );
	g_signal_connect( controller, "leave", 
		G_CALLBACK( imagedisplay_focus_leave ), imagedisplay );
	gtk_widget_add_controller( GTK_WIDGET( imagedisplay ), controller );

	controller = GTK_EVENT_CONTROLLER( gtk_gesture_click_new() );
	g_signal_connect( controller, "pressed", 
		G_CALLBACK( imagedisplay_click ), imagedisplay );
	gtk_widget_add_controller( GTK_WIDGET( imagedisplay ), controller );
}

static void
imagedisplay_class_init( ImagedisplayClass *class )
{
	GObjectClass *gobject_class = G_OBJECT_CLASS( class );
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS( class );

#ifdef DEBUG
	printf( "imagedisplay_class_init:\n" ); 
#endif /*DEBUG*/

	gobject_class->dispose = imagedisplay_dispose;
	gobject_class->set_property = imagedisplay_set_property;
	gobject_class->get_property = imagedisplay_get_property;

	widget_class->snapshot = imagedisplay_snapshot;

	g_object_class_install_property( gobject_class, PROP_TILE_CACHE,
		g_param_spec_object( "tile-cache",
			_( "Tile cache" ),
			_( "The tile cache to be displayed" ),
			TILE_CACHE_TYPE,
			G_PARAM_READWRITE ) );

	g_object_class_override_property( gobject_class, 
		PROP_HADJUSTMENT, "hadjustment" );
	g_object_class_override_property( gobject_class, 
		PROP_VADJUSTMENT, "vadjustment" );
	g_object_class_override_property( gobject_class, 
		PROP_HSCROLL_POLICY, "hscroll-policy" );
	g_object_class_override_property( gobject_class, 
		PROP_VSCROLL_POLICY, "vscroll-policy" );
}

Imagedisplay *
imagedisplay_new( TileCache *tile_cache ) 
{
	Imagedisplay *imagedisplay;

#ifdef DEBUG
	printf( "imagedisplay_new:\n" ); 
#endif /*DEBUG*/

	imagedisplay = g_object_new( imagedisplay_get_type(),
		"tile-cache", tile_cache,
		NULL );

	return( imagedisplay ); 
}
