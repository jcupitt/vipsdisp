/* A bar displaying the image info, plus mouse position and pixel value.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <gtk/gtk.h>

#include <vips/vips.h>

#include "disp.h"

G_DEFINE_TYPE( Infobar, infobar, GTK_TYPE_BOX );

static void
infobar_init( Infobar *infobar )
{
	infobar->imagepresent = NULL;

	gtk_orientable_set_orientation( GTK_ORIENTABLE( infobar ), 
		GTK_ORIENTATION_HORIZONTAL );
	gtk_container_set_border_width( GTK_CONTAINER( infobar ), 6 );

	infobar->coord_label = gtk_label_new( "" );
	gtk_label_set_xalign( GTK_LABEL( infobar->coord_label ), 0 );
	gtk_box_pack_start( GTK_BOX( infobar ), 
		infobar->coord_label, FALSE, FALSE, 0 );
	gtk_widget_show( infobar->coord_label );

	infobar->value_label = gtk_label_new( "" );
	gtk_label_set_xalign( GTK_LABEL( infobar->value_label ), 0 );
	gtk_box_pack_start( GTK_BOX( infobar ), 
		infobar->value_label, TRUE, TRUE, 0 );
	gtk_widget_show( infobar->value_label );

	infobar->mag_label = gtk_label_new( "" );
	gtk_label_set_xalign( GTK_LABEL( infobar->mag_label ), 0 );
	gtk_box_pack_end( GTK_BOX( infobar ), 
		infobar->mag_label, FALSE, FALSE, 0 );
	gtk_widget_show( infobar->mag_label );
}

static void
infobar_class_init( InfobarClass *class )
{
}

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
	VipsImage *image = infobar->imagepresent->conversion->image;

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
	VipsImage *image = infobar->imagepresent->conversion->image;
	Conversion *conversion = infobar->imagepresent->conversion;

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
	Conversion *conversion = infobar->imagepresent->conversion;

	char str[256];
	VipsBuf buf = VIPS_BUF_STATIC( str );
	int image_x;
	int image_y;
	int image_width;
	int image_height;
	int mag;

	conversion_to_image_cods( conversion,
		infobar->imagepresent->last_x, 
		infobar->imagepresent->last_y,
		&image_x, &image_y );

	if( conversion_get_image_size( conversion, 
		&image_width, &image_height ) ) {
		image_x = VIPS_CLIP( 0, image_x, image_width - 1 );
		image_y = VIPS_CLIP( 0, image_y, image_height - 1 );

		vips_buf_appendf( &buf, "(%7d, %7d)", image_x, image_y ); 
		gtk_label_set_text( GTK_LABEL( infobar->coord_label ), 
			vips_buf_all( &buf ) ); 
		vips_buf_rewind( &buf ); 

		infobar_status_value( infobar, &buf, image_x, image_y ); 
		gtk_label_set_text( GTK_LABEL( infobar->value_label ), 
			vips_buf_all( &buf ) ); 
	}

	vips_buf_rewind( &buf ); 
	vips_buf_appendf( &buf, "Magnification " );
	g_object_get( infobar->imagepresent->conversion, "mag", &mag, NULL ); 
	if( mag >= 0 )
		vips_buf_appendf( &buf, "%d:1", mag );
	else
		vips_buf_appendf( &buf, "1:%d", -mag );
	gtk_label_set_text( GTK_LABEL( infobar->mag_label ), 
		vips_buf_all( &buf ) ); 

}

static void
infobar_position_changed( Imagepresent *imagepresent, Infobar *infobar )
{
	infobar_status_update( infobar ); 
}

Infobar *
infobar_new( Imagepresent *imagepresent )
{
	Infobar *infobar;

	infobar = g_object_new( infobar_get_type(), 
		NULL );

	infobar->imagepresent = imagepresent;

	g_signal_connect( infobar->imagepresent, "position_changed", 
		G_CALLBACK( infobar_position_changed ), infobar );

	return( infobar ); 
}
