#include "vipsdisp.h"

/*
#define DEBUG_VERBOSE
#define DEBUG
 */

struct _Conversionview {
	GtkWidget parent_instance;

	/* The conversion whose output we display.
	 */
	Conversion *conversion;

	GtkWidget *action_bar;
	GtkWidget *gears;
	GtkWidget *page;
	GtkWidget *scale;
	GtkWidget *offset;

};

G_DEFINE_TYPE( Conversionview, conversionview, GTK_TYPE_WIDGET );

enum {
	/* Set the conversion with this.
	 */
	PROP_CONVERSION = 1,

	SIG_LAST
};

static void
conversionview_conversion_changed( Conversion *conversion, 
	Conversionview *conversionview ) 
{
#ifdef DEBUG
	printf( "conversionview_conversion_changed:\n" ); 
#endif /*DEBUG*/

	if( TSLIDER( conversionview->scale )->value != conversion->scale ) {
		TSLIDER( conversionview->scale )->value = conversion->scale;
		tslider_changed( TSLIDER( conversionview->scale ) );
	}

	if( TSLIDER( conversionview->offset )->value != conversion->offset ) {
		TSLIDER( conversionview->offset )->value = conversion->offset;
		tslider_changed( TSLIDER( conversionview->offset ) );
	}

	if( conversion->image ) {
		int n_pages = vips_image_get_n_pages( conversion->image );

		gtk_spin_button_set_range( 
			GTK_SPIN_BUTTON( conversionview->page ), 
			0, n_pages - 1 );
		gtk_widget_set_sensitive( conversionview->page, 
			n_pages > 1 && 
			conversion->mode == CONVERSION_MODE_MULTIPAGE );
	}
}

static void
conversionview_page_changed( Conversion *conversion, 
	Conversionview *conversionview )
{
#ifdef DEBUG
	printf( "conversionview_page_changed:\n" );
#endif /*DEBUG*/

	gtk_spin_button_set_value( 
		GTK_SPIN_BUTTON( conversionview->page ), 
		conversion->page );
}

static void
conversionview_set_conversion( Conversionview *conversionview, 
	Conversion *conversion )
{
	g_assert( !conversionview->conversion );

#ifdef DEBUG
	printf( "conversionview_set_conversion: %p\n", conversion ); 
#endif /*DEBUG*/

	conversionview->conversion = conversion;
	g_object_ref( conversionview->conversion );

        g_signal_connect_object( conversion, "changed", 
                G_CALLBACK( conversionview_conversion_changed ), 
		conversionview, 0 );
	g_signal_connect_object( conversion, "page-changed",
		G_CALLBACK( conversionview_page_changed ), 
		conversionview, 0 );
}

static void
conversionview_set_property( GObject *object, 
	guint prop_id, const GValue *value, GParamSpec *pspec )
{
	Conversionview *conversionview = (Conversionview *) object;

	switch( prop_id ) {
	case PROP_CONVERSION:
		conversionview_set_conversion( conversionview, 
			g_value_get_object( value ) );
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
	case PROP_CONVERSION:
		g_value_set_object( value, conversionview->conversion );
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

	VIPS_UNREF( conversionview->conversion );

	VIPS_FREEF( gtk_widget_unparent, conversionview->action_bar );

	G_OBJECT_CLASS( conversionview_parent_class )->dispose( object );
}

static void
conversionview_page_value_changed( GtkSpinButton *spin_button,
	Conversionview *conversionview )
{
	int new_page = gtk_spin_button_get_value_as_int( spin_button );

#ifdef DEBUG
	printf( "conversionview_page_value_changed: %d\n", new_page );
#endif /*DEBUG*/

	g_object_set( conversionview->conversion,
		"page", new_page,
		NULL );
}

static void
conversionview_scale_value_changed( Tslider *slider, 
	Conversionview *conversionview )
{
	g_object_set( conversionview->conversion,
		"scale", slider->value,
		NULL );
}

static void
conversionview_offset_value_changed( Tslider *slider, 
	Conversionview *conversionview )
{
	g_object_set( conversionview->conversion,
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
		"/org/libvips/vipsdisp/conversionview-menu.ui" );
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
		"/org/libvips/vipsdisp/conversionview.ui");

	BIND( action_bar );
	BIND( gears );
	BIND( page );
	BIND( scale );
	BIND( offset );

	gobject_class->set_property = conversionview_set_property;
	gobject_class->get_property = conversionview_get_property;

	g_object_class_install_property( gobject_class, PROP_CONVERSION,
		g_param_spec_object( "conversion",
			_( "conversion" ),
			_( "The conversion to be displayed" ),
			TYPE_CONVERSION,
			G_PARAM_READWRITE ) );

}

Conversionview *
conversionview_new( Conversion *conversion ) 
{
	Conversionview *conversionview;

#ifdef DEBUG
	printf( "conversionview_new:\n" ); 
#endif /*DEBUG*/

	conversionview = g_object_new( conversionview_get_type(),
		"conversion", conversion,
		NULL );

	return( conversionview ); 
}

void 
conversionview_set_reveal( Conversionview *conversionview, 
	gboolean reveal )
{
#ifdef DEBUG
	printf( "conversionview_set_reveal: %d\n", reveal ); 
#endif /*DEBUG*/

	gtk_action_bar_set_revealed( 
		GTK_ACTION_BAR( conversionview->action_bar ), reveal );
}
