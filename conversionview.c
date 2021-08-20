#include "vipsdisp.h"

/*
#define DEBUG_VERBOSE
 */
#define DEBUG

struct _Conversionview {
	GtkWidget parent_instance;

	/* The conversion whose output we display.
	 */
	Conversion *conversion;

	GtkWidget *action_bar;
	GtkWidget *gears;

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
conversionview_init( Conversionview *conversionview )
{
	GtkBuilder *builder;
	GMenuModel *menu;

#ifdef DEBUG
	printf( "conversionview_init:\n" ); 
#endif /*DEBUG*/

	gtk_widget_init_template( GTK_WIDGET( conversionview ) );

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
