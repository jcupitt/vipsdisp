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

        GSList *value_widgets;
};

G_DEFINE_TYPE( Infobar, infobar, GTK_TYPE_WIDGET );

enum {
        PROP_IMAGE_WINDOW = 1,
        PROP_REVEALED,

        SIG_LAST
};

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

/* For each format, the label width we need, in characters.
 */
static const int infobar_label_width[] = {
        3,      /* uchar */
        4,      /* char */
        5,      /* ushort */
        6,      /* short */
        8,      /* uint */
        9,      /* int */
        10,     /* float */
        18,     /* complex */
        10,     /* double */
        18,     /* double complex */
};

/* TileSource has a new image. We need a new number of band elemenets and
 * dimensions.
 */
static void
infobar_tile_source_changed( TileSource *tile_source, Infobar *infobar ) 
{
        VipsImage *image = tile_source_get_image( tile_source );

        GSList *p;
        VipsBandFormat format;
        int bands;
        int label_width;
        int max_children;
        int n_children;
        int i;

#ifdef DEBUG
        printf( "infobar_tile_source_changed:\n" ); 
#endif /*DEBUG*/

        /* Remove all existing children of infobar->values.
         */
        for( p = infobar->value_widgets; p; p = p->next ) {
                GtkWidget *label = GTK_WIDGET( p->data );

                gtk_box_remove( GTK_BOX( infobar->values ), label );
        }
        VIPS_FREEF( g_slist_free, infobar->value_widgets ); 

        switch( image->Coding ) { 
        case VIPS_CODING_LABQ:
        case VIPS_CODING_RAD:
                format = VIPS_FORMAT_FLOAT;
                bands = 3;
                break;

        case VIPS_CODING_NONE:
        default:
                format = image->BandFmt;
                bands = image->Bands;
                break;
        }

        label_width = infobar_label_width[format];
        max_children = 40 / label_width;
        n_children = VIPS_MIN( bands, max_children );

        /* Add a new set of labels.
         */
        for( i = 0; i < n_children; i++ ) {
                GtkWidget *label;

                label = gtk_label_new( "123" );
                gtk_label_set_width_chars( GTK_LABEL( label ), label_width );
                gtk_label_set_xalign( GTK_LABEL( label ), 1.0 );
                gtk_box_append( GTK_BOX( infobar->values ), label ); 
                infobar->value_widgets = 
                        g_slist_append( infobar->value_widgets, label );
        }
}

static void 
infobar_status_value_set_array( Infobar *infobar, double *d )
{
        int i;
        GSList *q;

        for( i = 0, q = infobar->value_widgets; q; q = q->next, i++ ) {
                GtkWidget *label = GTK_WIDGET( q->data );

                char str[64];
                VipsBuf buf = VIPS_BUF_STATIC( str );

                vips_buf_appendf( &buf, "%g", d[i] );
                gtk_label_set_text( GTK_LABEL( label ), vips_buf_all( &buf ) );
        }
}

void
infobar_status_update( Infobar *infobar )
{
        double scale = image_window_get_scale( infobar->win );

        char str[64];
        VipsBuf buf = VIPS_BUF_STATIC( str );
        double image_x;
        double image_y;
	double *vector;
	int n;

#ifdef DEBUG
        printf( "infobar_status_update:\n" ); 
#endif /*DEBUG*/

        image_window_get_mouse_position( infobar->win, &image_x, &image_y );

        vips_buf_appendf( &buf, "%d", (int) image_x ); 
        gtk_label_set_text( GTK_LABEL( infobar->x ), 
                vips_buf_all( &buf ) ); 
        vips_buf_rewind( &buf ); 

        vips_buf_appendf( &buf, "%d", (int) image_y ); 
        gtk_label_set_text( GTK_LABEL( infobar->y ), 
                vips_buf_all( &buf ) ); 
        vips_buf_rewind( &buf ); 

	if( image_window_get_pixel( infobar->win, &vector, &n, 
		image_x, image_y ) ) {
		infobar_status_value_set_array( infobar, vector );
		g_free( vector );
	}

        vips_buf_rewind( &buf ); 
        vips_buf_appendf( &buf, "Magnification " );
        if( scale >= 1.0 )
                vips_buf_appendf( &buf, "%d:1", (int) scale );
        else
                vips_buf_appendf( &buf, "1:%d", (int) (1.0 / scale) );
        gtk_label_set_text( GTK_LABEL( infobar->mag ), 
                vips_buf_all( &buf ) ); 

}

static void
infobar_status_changed( ImageWindow *win, Infobar *infobar ) 
{
        if( !gtk_action_bar_get_revealed( 
                GTK_ACTION_BAR( infobar->action_bar ) ) )
                return;

        if( !image_window_get_tile_source( infobar->win ) )
                return;

#ifdef DEBUG
        printf( "infobar_status_changed:\n" ); 
#endif /*DEBUG*/

        infobar_status_update( infobar );
}

/* Imagewindow has a new tile_source.
 */
static void
infobar_image_window_changed( ImageWindow *win, Infobar *infobar )
{
	TileSource *tile_source = image_window_get_tile_source( win );

        g_signal_connect_object( tile_source, "changed", 
                G_CALLBACK( infobar_tile_source_changed ), 
                infobar, 0 );

        g_signal_connect_object( tile_source, "page-changed", 
                G_CALLBACK( infobar_status_changed ), 
                infobar, 0 );
}

static void
infobar_set_image_window( Infobar *infobar, ImageWindow *win )
{
        /* No need to ref ... win holds a ref to us.
         */
        infobar->win = win;

        g_signal_connect_object( win, "changed", 
                G_CALLBACK( infobar_image_window_changed ), 
                infobar, 0 );

        g_signal_connect_object( win, "status-changed", 
                G_CALLBACK( infobar_status_changed ), 
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
                        VIPSDISP_IMAGE_WINDOW( g_value_get_object( value ) ) );
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
                g_param_spec_object( "image-window",
                        _( "Image window" ),
                        _( "The image window we display" ),
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
                "image-window", win,
                NULL );

        return( infobar ); 
}
