#include "vipsdisp.h"

/*
#define DEBUG_VERBOSE
#define DEBUG
 */

struct _Infobar {
	GtkWidget parent_instance;

	/* The imagewindow whose info we display.
	 */
	ImageWindow *win;

	GtkWidget *action_bar;
	GtkWidget *x;
	GtkWidget *y;
	GtkWidget *values;
	GtkWidget *mag;
};

G_DEFINE_TYPE( Infobar, infobar, GTK_TYPE_WIDGET );

enum {
	PROP_IMAGE_WINDOW = 1,
	PROP_REVEALED,

	SIG_LAST
};

/* Display a LABPACK value.
 */
static void
infobar_status_value_labpack( Infobar *infobar, VipsBuf *buf, VipsPel *p )
{
	unsigned int iL = (p[0] << 2) | (p[3] >> 6);
	float L = 100.0 * iL / 1023.0;
	signed int ia = ((signed char) p[1] << 3) | ((p[3] >> 3) & 0x7);
	float a = 0.125 * ia;
	signed int ib = ((signed char) p[2] << 3) | (p[3] & 0x7);
	float b = 0.125 * ib;

	vips_buf_appendf( buf, "%5g ", L );
	vips_buf_appendf( buf, "%5g ", a );
	vips_buf_appendf( buf, "%5g ", b );
}

/* Diplay a RAD. 
 */
static void
infobar_status_value_rad( Infobar *infobar, VipsBuf *buf, VipsPel *p )
{
	double f = ldexp( 1.0, p[3] - (128 + 8) );
	float r = (p[0] + 0.5) * f;
	float g = (p[1] + 0.5) * f;
	float b = (p[2] + 0.5) * f;

	vips_buf_appendf( buf, "%5g ", r );
	vips_buf_appendf( buf, "%5g ", g );
	vips_buf_appendf( buf, "%5g ", b );
}

static void 
infobar_status_value_uncoded( Infobar *infobar, 
	VipsBuf *buf, VipsPel *p )
{
	Conversion *conversion = image_window_get_conversion( infobar->win );
	VipsImage *image = conversion->image;

	int i;

	for( i = 0; i < image->Bands; i++ ) {
		switch( image->BandFmt ) {
		case VIPS_FORMAT_UCHAR:
			vips_buf_appendf( buf, 
				"%3d ", ((unsigned char *)p)[0] );
			break;

		case VIPS_FORMAT_CHAR:
			vips_buf_appendf( buf, 
				"%4d ", ((char *)p)[0] );
			break;

		case VIPS_FORMAT_USHORT:
			vips_buf_appendf( buf, 
				"%7d ", ((unsigned short *)p)[0] );
			break;

		case VIPS_FORMAT_SHORT:
			vips_buf_appendf( buf, 
				"%7d ", ((short *)p)[0] );
			break;

		case VIPS_FORMAT_UINT:
			vips_buf_appendf( buf, 
				"%8d ", ((unsigned int *)p)[0] );
			break;

		case VIPS_FORMAT_INT:
			vips_buf_appendf( buf, 
				"%8d ", ((int *)p)[0] );
			break;

		case VIPS_FORMAT_FLOAT:
			vips_buf_appendf( buf, 
				"%10g ", ((float *)p)[0] );
			break;

		case VIPS_FORMAT_COMPLEX:
			vips_buf_appendf( buf, 
				"(%7g,%7g) ", 
				((float *)p)[0], ((float *)p)[1] );
			break;

		case VIPS_FORMAT_DOUBLE:
			vips_buf_appendf( buf, 
				"%10g ", ((double *)p)[0] );
			break;

		case VIPS_FORMAT_DPCOMPLEX:
			vips_buf_appendf( buf, "(%7g,%7g) ", 
				((double *)p)[0], 
				((double *)p)[1] );
			break;

		default:
			vips_buf_appendf( buf, " " );
			break;
		}

		p += VIPS_IMAGE_SIZEOF_ELEMENT( image );
	}
}

void 
infobar_status_value( Infobar *infobar, VipsBuf *buf, int x, int y ) 
{
	Conversion *conversion = image_window_get_conversion( infobar->win );
	VipsImage *image = conversion->image;

	VipsPel *ink;

	if( image &&
		(ink = conversion_get_ink( conversion, x, y )) ) { 
		switch( image->Coding ) { 
		case VIPS_CODING_LABQ:
			infobar_status_value_labpack( infobar, buf, ink );
			break;

		case VIPS_CODING_RAD:
			infobar_status_value_rad( infobar, buf, ink );
			break;

		case VIPS_CODING_NONE:
			infobar_status_value_uncoded( infobar, buf, ink );
			break;

		default:
			break;
		}
	}
}

void
infobar_status_update( Infobar *infobar )
{
	Conversion *conversion = image_window_get_conversion( infobar->win );

	char str[256];
	VipsBuf buf = VIPS_BUF_STATIC( str );
	int last_x;
	int last_y;
	int image_x;
	int image_y;
	int image_width;
	int image_height;
	int mag;

#ifdef DEBUG
	printf( "infobar_status_update:\n" ); 
#endif /*DEBUG*/

	image_window_get_last( infobar->win, &last_x, &last_y );
	conversion_to_image_cods( conversion->mag, 
		last_x, last_y, &image_x, &image_y );

	if( conversion_get_image_size( conversion, 
		&image_width, &image_height ) ) {
		image_x = VIPS_CLIP( 0, image_x, image_width - 1 );
		image_y = VIPS_CLIP( 0, image_y, image_height - 1 );

		vips_buf_appendf( &buf, "%7d", image_x ); 
		gtk_label_set_text( GTK_LABEL( infobar->x ), 
			vips_buf_all( &buf ) ); 
		vips_buf_rewind( &buf ); 

		vips_buf_appendf( &buf, "%7d", image_y ); 
		gtk_label_set_text( GTK_LABEL( infobar->y ), 
			vips_buf_all( &buf ) ); 
		vips_buf_rewind( &buf ); 

		infobar_status_value( infobar, &buf, image_x, image_y ); 
		gtk_label_set_text( GTK_LABEL( infobar->values ), 
			vips_buf_all( &buf ) ); 
	}

	vips_buf_rewind( &buf ); 
	vips_buf_appendf( &buf, "Magnification " );
	g_object_get( conversion, "mag", &mag, NULL ); 
	if( mag >= 0 )
		vips_buf_appendf( &buf, "%d:1", mag );
	else
		vips_buf_appendf( &buf, "1:%d", -mag );
	gtk_label_set_text( GTK_LABEL( infobar->mag ), 
		vips_buf_all( &buf ) ); 

}

static void
infobar_position_changed( ImageWindow *win, Infobar *infobar ) 
{
	if( !gtk_action_bar_get_revealed( 
		GTK_ACTION_BAR( infobar->action_bar ) ) )
		return;

#ifdef DEBUG
	printf( "infobar_position_changed:\n" ); 
#endif /*DEBUG*/

	infobar_status_update( infobar );
}

static void
infobar_set_image_window( Infobar *infobar, ImageWindow *win )
{
	g_assert( !infobar->win );

#ifdef DEBUG
	printf( "infobar_set_image_window: %p\n", win ); 
#endif /*DEBUG*/

	/* No need to ref ... win holds a ref to us.
	 */
	infobar->win = win;

        g_signal_connect_object( win, "position-changed", 
                G_CALLBACK( infobar_position_changed ), 
		infobar, 0 );
}

static void
infobar_set_property( GObject *object, 
	guint prop_id, const GValue *value, GParamSpec *pspec )
{
	Infobar *infobar = (Infobar *) object;

	switch( prop_id ) {
	case PROP_IMAGE_WINDOW:
		infobar_set_image_window( infobar, 
			g_value_get_object( value ) );
		break;

	case PROP_REVEALED:
		gtk_action_bar_set_revealed( 
			GTK_ACTION_BAR( infobar->action_bar ), 
			g_value_get_boolean( value ) );
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID( object, prop_id, pspec );
		break;
	}
}

static void
infobar_get_property( GObject *object, 
	guint prop_id, GValue *value, GParamSpec *pspec )
{
	Infobar *infobar = (Infobar *) object;

	switch( prop_id ) {
	case PROP_IMAGE_WINDOW:
		g_value_set_object( value, infobar->win );
		break;

	case PROP_REVEALED:
		g_value_set_boolean( value, gtk_action_bar_get_revealed( 
			GTK_ACTION_BAR( infobar->action_bar ) ) ); 
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID( object, prop_id, pspec );
		break;
	}
}

static void
infobar_dispose( GObject *object )
{
	Infobar *infobar = (Infobar *) object;

#ifdef DEBUG
	printf( "infobar_dispose:\n" ); 
#endif /*DEBUG*/

	VIPS_FREEF( gtk_widget_unparent, infobar->action_bar );

	G_OBJECT_CLASS( infobar_parent_class )->dispose( object );
}

static void
infobar_init( Infobar *infobar )
{
#ifdef DEBUG
	printf( "infobar_init:\n" ); 
#endif /*DEBUG*/

	gtk_widget_init_template( GTK_WIDGET( infobar ) );

}

#define BIND( field ) \
	gtk_widget_class_bind_template_child( GTK_WIDGET_CLASS( class ), \
		Infobar, field );

static void
infobar_class_init( InfobarClass *class )
{
	GObjectClass *gobject_class = G_OBJECT_CLASS( class );
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS( class );

#ifdef DEBUG
	printf( "infobar_class_init:\n" ); 
#endif /*DEBUG*/

	G_OBJECT_CLASS( class )->dispose = infobar_dispose;

	gtk_widget_class_set_layout_manager_type( widget_class, 
		GTK_TYPE_BIN_LAYOUT );
	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( class ),
		APP_PATH "/infobar.ui");

	BIND( action_bar );
	BIND( x );
	BIND( y );
	BIND( values );
	BIND( mag );

	gobject_class->set_property = infobar_set_property;
	gobject_class->get_property = infobar_get_property;

	g_object_class_install_property( gobject_class, PROP_IMAGE_WINDOW,
		g_param_spec_object( "image_window",
			_( "Image window" ),
			_( "Image window whose mouse position we display" ),
			IMAGE_WINDOW_TYPE,
			G_PARAM_READWRITE ) );

	g_object_class_install_property( gobject_class, PROP_REVEALED,
		g_param_spec_boolean( "revealed",
			_( "revealed" ),
			_( "Show the display control bar" ),
			FALSE,
			G_PARAM_READWRITE ) );

}

Infobar *
infobar_new( ImageWindow *win ) 
{
	Infobar *infobar;

#ifdef DEBUG
	printf( "infobar_new:\n" ); 
#endif /*DEBUG*/

	infobar = g_object_new( infobar_get_type(),
		"image_window", win,
		NULL );

	return( infobar ); 
}
