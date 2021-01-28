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
conversionview_init( Conversionview *conversionview )
{
	Tslider *scale;
	Tslider *offset;

	gtk_orientable_set_orientation( GTK_ORIENTABLE( conversionview ), 
		GTK_ORIENTATION_HORIZONTAL );
	gtk_container_set_border_width( GTK_CONTAINER( conversionview ), 3 );

	scale = tslider_new();
	tslider_set_conversions( scale,
		tslider_log_value_to_slider, tslider_log_slider_to_value );
        scale->from = 0.001;
        scale->to = 255.0;
        scale->value = 1.0;
        scale->svalue = 128;
        scale->digits = 3;
        tslider_changed( scale );
	g_signal_connect( scale, "changed", 
		G_CALLBACK( conversionview_scale_value_changed ), 
		conversionview );
	gtk_box_pack_start( GTK_BOX( conversionview ), 
		GTK_WIDGET( scale ), TRUE, TRUE, 2 );
	gtk_widget_show( GTK_WIDGET( scale ) );

	offset = tslider_new();
        offset->from = -128;
        offset->to = 128;
        offset->value = 0;
        offset->svalue = 0;
        offset->digits = 1;
        tslider_changed( offset );
	g_signal_connect( offset, "changed", 
		G_CALLBACK( conversionview_offset_value_changed ), 
		conversionview );
	gtk_box_pack_start( GTK_BOX( conversionview ), 
		GTK_WIDGET( offset ), TRUE, TRUE, 2 );
	gtk_widget_show( GTK_WIDGET( offset ) );
}

static void
conversionview_class_init( ConversionviewClass *class )
{
}

Conversionview *
conversionview_new( Conversion *conversion )
{
	Conversionview *conversionview;

	conversionview = g_object_new( conversionview_get_type(),
		NULL );

	conversionview->conversion = conversion;

	return( conversionview ); 
}
