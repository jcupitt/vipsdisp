/* Display an image with gtk3 and libvips. 
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <gtk/gtk.h>

#include <vips/vips.h>

#include "disp.h"

G_DEFINE_TYPE( Conversionview, conversionview, GTK_TYPE_BOX );

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
conversionview_page_value_changed( GtkSpinButton *spin_button,
	Conversionview *conversionview )
{
	g_object_set( conversionview->conversion,
		"page", gtk_spin_button_get_value_as_int( spin_button ),
		NULL );
}

static void
conversionview_init( Conversionview *conversionview )
{
	GtkWidget *menu_button;
	GtkWidget *image;
	GtkBuilder *builder;
	GMenuModel *menu;

	gtk_orientable_set_orientation( GTK_ORIENTABLE( conversionview ), 
		GTK_ORIENTATION_HORIZONTAL );
	gtk_container_set_border_width( GTK_CONTAINER( conversionview ), 3 );

	menu_button = gtk_menu_button_new();
	image = gtk_image_new_from_icon_name( "open-menu-symbolic", 
		GTK_ICON_SIZE_SMALL_TOOLBAR );
	gtk_widget_show( image );
	gtk_container_add( GTK_CONTAINER( menu_button ), image );
	gtk_box_pack_start( GTK_BOX( conversionview ), 
		GTK_WIDGET( menu_button ), FALSE, FALSE, 2 );
	gtk_widget_show( menu_button );

	builder = gtk_builder_new_from_resource( 
		"/vips/disp/gtk/conversionview-popover.ui" ); 
	menu = G_MENU_MODEL( gtk_builder_get_object( builder, 
		"conversionview-popover-menu" ) );
	gtk_menu_button_set_menu_model( GTK_MENU_BUTTON( menu_button ), menu );
	g_object_unref( builder );

	conversionview->page_select = 
		gtk_spin_button_new_with_range( 0, 100, 1 );
	g_signal_connect( conversionview->page_select, "value-changed", 
		G_CALLBACK( conversionview_page_value_changed ), 
		conversionview );
	gtk_box_pack_start( GTK_BOX( conversionview ), 
		conversionview->page_select, FALSE, FALSE, 2 );
	gtk_widget_show( conversionview->page_select );
	set_tooltip( conversionview->page_select, _( "Page select" ) );

	conversionview->scale = tslider_new();
	tslider_set_conversions( conversionview->scale,
		tslider_log_value_to_slider, tslider_log_slider_to_value );
        conversionview->scale->from = 0.001;
        conversionview->scale->to = 255.0;
        conversionview->scale->value = 1.0;
        conversionview->scale->svalue = 128;
        conversionview->scale->digits = 3;
        tslider_changed( conversionview->scale );
	g_signal_connect( conversionview->scale, "changed", 
		G_CALLBACK( conversionview_scale_value_changed ), 
		conversionview );
	gtk_box_pack_start( GTK_BOX( conversionview ), 
		GTK_WIDGET( conversionview->scale ), TRUE, TRUE, 2 );
	gtk_widget_show( GTK_WIDGET( conversionview->scale ) );
	set_tooltip( GTK_WIDGET( conversionview->scale ), 
		_( "Brightness scale factor" ) );

	conversionview->offset = tslider_new();
        conversionview->offset->from = -128;
        conversionview->offset->to = 128;
        conversionview->offset->value = 0;
        conversionview->offset->svalue = 0;
        conversionview->offset->digits = 1;
        tslider_changed( conversionview->offset );
	g_signal_connect( conversionview->offset, "changed", 
		G_CALLBACK( conversionview_offset_value_changed ), 
		conversionview );
	gtk_box_pack_start( GTK_BOX( conversionview ), 
		GTK_WIDGET( conversionview->offset ), TRUE, TRUE, 2 );
	gtk_widget_show( GTK_WIDGET( conversionview->offset ) );
	set_tooltip( GTK_WIDGET( conversionview->offset ), 
		_( "Brightness offset" ) );
}

static void
conversionview_class_init( ConversionviewClass *class )
{
}

static void
conversionview_display_changed( Conversion *conversion, 
	Conversionview *conversionview )
{
	if( conversionview->scale->value != conversion->scale ) {
		conversionview->scale->value = conversion->scale;
		tslider_changed( conversionview->scale );
	}

	if( conversionview->offset->value != conversion->offset ) {
		conversionview->offset->value = conversion->offset;
		tslider_changed( conversionview->offset );
	}

	if( conversion->image ) {
		int n_pages = vips_image_get_n_pages( conversion->image );

		gtk_spin_button_set_range( 
			GTK_SPIN_BUTTON( conversionview->page_select ), 
			0, n_pages - 1 );
		gtk_widget_set_sensitive( conversionview->page_select, 
			n_pages > 1 );
	}
}

Conversionview *
conversionview_new( Conversion *conversion )
{
	Conversionview *conversionview;

	conversionview = g_object_new( conversionview_get_type(),
		NULL );

	conversionview->conversion = conversion;

	/* If the conversion changes, we need to update our widgets.
	 */
	g_signal_connect( conversion,
		"display-changed",
		G_CALLBACK( conversionview_display_changed ), conversionview );

	return( conversionview ); 
}
