#include "vipsdisp.h"

/*
#define DEBUG_VERBOSE
#define DEBUG
 */

struct _Conversionview {
        GtkWidget parent_instance;

        /* The image_window we attach to.
         */
        ImageWindow *win;

        GtkWidget *action_bar;
        GtkWidget *gears;
        GtkWidget *page;
        GtkWidget *scale;
        GtkWidget *offset;

};

G_DEFINE_TYPE( Conversionview, conversionview, GTK_TYPE_WIDGET );

enum {
	PROP_IMAGE_WINDOW = 1,
        PROP_REVEALED,

        SIG_LAST
};

static void
conversionview_tile_source_changed( TileSource *tile_source, 
        Conversionview *conversionview ) 
{
#ifdef DEBUG
        printf( "conversionview_tile_source_changed:\n" ); 
#endif /*DEBUG*/

        if( TSLIDER( conversionview->scale )->value != tile_source->scale ) {
                TSLIDER( conversionview->scale )->value = tile_source->scale;
                tslider_changed( TSLIDER( conversionview->scale ) );
        }

        if( TSLIDER( conversionview->offset )->value != tile_source->offset ) {
                TSLIDER( conversionview->offset )->value = tile_source->offset;
                tslider_changed( TSLIDER( conversionview->offset ) );
        }

	gtk_spin_button_set_range( 
		GTK_SPIN_BUTTON( conversionview->page ), 
		0, tile_source->n_pages - 1 );
	gtk_widget_set_sensitive( conversionview->page, 
		tile_source->n_pages > 1 && 
		tile_source->mode == TILE_SOURCE_MODE_MULTIPAGE );
}

static void
conversionview_page_changed( TileSource *tile_source, 
        Conversionview *conversionview )
{
#ifdef DEBUG
        printf( "conversionview_page_changed:\n" );
#endif /*DEBUG*/

        gtk_spin_button_set_value( 
                GTK_SPIN_BUTTON( conversionview->page ), 
                tile_source->page );
}

/* Imagewindow has a new tile_source.
 */
static void
conversionview_image_window_changed( ImageWindow *win, 
	Conversionview *conversionview )
{
	TileSource *tile_source = image_window_get_tilesource( win );

        g_signal_connect_object( tile_source, "changed", 
                G_CALLBACK( conversionview_tile_source_changed ), 
                conversionview, 0 );
        g_signal_connect_object( tile_source, "tiles-changed", 
                G_CALLBACK( conversionview_tile_source_changed ), 
                conversionview, 0 );
        g_signal_connect_object( tile_source, "page-changed",
                G_CALLBACK( conversionview_page_changed ), 
                conversionview, 0 );
}

static void
conversionview_set_image_window( Conversionview *conversionview, 
	ImageWindow *win )
{
	/* No need to ref ... win holds a ref to us.
	 */
	conversionview->win = win;

        g_signal_connect_object( win, "changed", 
                G_CALLBACK( conversionview_image_window_changed ), 
		conversionview, 0 );
}

static void
conversionview_set_property( GObject *object, 
        guint prop_id, const GValue *value, GParamSpec *pspec )
{
        Conversionview *conversionview = (Conversionview *) object;

        switch( prop_id ) {
        case PROP_IMAGE_WINDOW:
                conversionview_set_image_window( conversionview, 
                        g_value_get_object( value ) );
                break;

        case PROP_REVEALED:
                gtk_action_bar_set_revealed( 
                        GTK_ACTION_BAR( conversionview->action_bar ), 
                        g_value_get_boolean( value ) );
                break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID( object, prop_id, pspec );
                break;
        }
}

static void
conversionview_get_property( GObject *object, 
        guint prop_id, GValue *value, GParamSpec *pspec )
{
        Conversionview *conversionview = (Conversionview *) object;

        switch( prop_id ) {
        case PROP_IMAGE_WINDOW:
                g_value_set_object( value, conversionview->win );
                break;

        case PROP_REVEALED:
                g_value_set_boolean( value, gtk_action_bar_get_revealed( 
                        GTK_ACTION_BAR( conversionview->action_bar ) ) ); 
                break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID( object, prop_id, pspec );
                break;
        }
}

static void
conversionview_dispose( GObject *object )
{
        Conversionview *conversionview = (Conversionview *) object;

#ifdef DEBUG
        printf( "conversionview_dispose:\n" ); 
#endif /*DEBUG*/

        VIPS_FREEF( gtk_widget_unparent, conversionview->action_bar );

        G_OBJECT_CLASS( conversionview_parent_class )->dispose( object );
}

static void
conversionview_page_value_changed( GtkSpinButton *spin_button,
        Conversionview *conversionview )
{
	TileSource *tile_source = 
		image_window_get_tilesource( conversionview->win );
        int new_page = gtk_spin_button_get_value_as_int( spin_button );

#ifdef DEBUG
        printf( "conversionview_page_value_changed: %d\n", new_page );
#endif /*DEBUG*/

        g_object_set( tile_source,
                "page", new_page,
                NULL );
}

static void
conversionview_scale_value_changed( Tslider *slider, 
        Conversionview *conversionview )
{
	TileSource *tile_source = 
		image_window_get_tilesource( conversionview->win );

        g_object_set( tile_source,
                "scale", slider->value,
                NULL );
}

static void
conversionview_offset_value_changed( Tslider *slider, 
        Conversionview *conversionview )
{
	TileSource *tile_source = 
		image_window_get_tilesource( conversionview->win );

        g_object_set( tile_source,
                "offset", slider->value,
                NULL );
}

static void
conversionview_init( Conversionview *conversionview )
{
        GtkBuilder *builder;
        GMenuModel *menu;
        Tslider *tslider;

#ifdef DEBUG
        printf( "conversionview_init:\n" ); 
#endif /*DEBUG*/

        gtk_widget_init_template( GTK_WIDGET( conversionview ) );

        set_tooltip( GTK_WIDGET( conversionview->page ), _( "Page select" ) );

        tslider = TSLIDER( conversionview->scale );
        tslider_set_conversions( tslider,
                tslider_log_value_to_slider, tslider_log_slider_to_value );
        tslider->from = 0.001;
        tslider->to = 255;
        tslider->value = 1.0;
        tslider->svalue = 128;
        tslider->digits = 3;
        tslider_changed( tslider );
        set_tooltip( GTK_WIDGET( tslider ), _( "Brightness scale factor" ) );

        tslider = TSLIDER( conversionview->offset );
        tslider->from = -128;
        tslider->to = 128;
        tslider->value = 0;
        tslider->svalue = 0;
        tslider->digits = 1;
        tslider_changed( tslider );
        set_tooltip( GTK_WIDGET( tslider ), _( "Brightness offset" ) );

        g_signal_connect( conversionview->page, "value-changed",
                G_CALLBACK( conversionview_page_value_changed ), 
                conversionview );
        g_signal_connect( conversionview->scale, "changed",
                G_CALLBACK( conversionview_scale_value_changed ), 
                conversionview );
        g_signal_connect( conversionview->offset, "changed",
                G_CALLBACK( conversionview_offset_value_changed ), 
                conversionview );

        builder = gtk_builder_new_from_resource( 
                APP_PATH "/conversionview-menu.ui" );
        menu = G_MENU_MODEL( gtk_builder_get_object( builder, 
                "conversionview-menu" ) );
        gtk_menu_button_set_menu_model( 
                GTK_MENU_BUTTON( conversionview->gears ), menu );
        g_object_unref( builder );

}

#define BIND( field ) \
        gtk_widget_class_bind_template_child( GTK_WIDGET_CLASS( class ), \
                Conversionview, field );

static void
conversionview_class_init( ConversionviewClass *class )
{
        GObjectClass *gobject_class = G_OBJECT_CLASS( class );
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS( class );

#ifdef DEBUG
        printf( "conversionview_class_init:\n" ); 
#endif /*DEBUG*/

        G_OBJECT_CLASS( class )->dispose = conversionview_dispose;

        gtk_widget_class_set_layout_manager_type( widget_class, 
                GTK_TYPE_BIN_LAYOUT );
        gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( class ),
                APP_PATH "/conversionview.ui");

        BIND( action_bar );
        BIND( gears );
        BIND( page );
        BIND( scale );
        BIND( offset );

        gobject_class->set_property = conversionview_set_property;
        gobject_class->get_property = conversionview_get_property;

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

Conversionview *
conversionview_new( ImageWindow *win ) 
{
        Conversionview *conversionview;

#ifdef DEBUG
        printf( "conversionview_new:\n" ); 
#endif /*DEBUG*/

        conversionview = g_object_new( conversionview_get_type(),
                "image-window", win,
                NULL );

        return( conversionview ); 
}

