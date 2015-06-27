/* Display an image with gtk3 and libvips. 
 */

#include <stdio.h>
#include <stdlib.h>

#include <gtk/gtk.h>

#include <vips/vips.h>

#include "disp.h"

G_DEFINE_TYPE( Window, window, GTK_TYPE_APPLICATION_WINDOW );

static void
window_destroy( GtkWidget *widget )
{
	Window *window = (Window *) widget;

	printf( "window_destroy:\n" ); 

	if( window->region ) {
		g_object_unref( window->region );
		window->region = NULL;
	}

	if( window->display ) {
		g_object_unref( window->display );
		window->display = NULL;
	}

	if( window->image ) {
		g_object_unref( window->image );
		window->image = NULL;
	}

	g_object_set( window,
		"application", NULL,
		NULL );

	GTK_WIDGET_CLASS( window_parent_class )->destroy( widget );
}

static void
window_init( Window *disp )
{
	printf( "window_init:\n" ); 
}

static void
window_class_init( WindowClass *class )
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS( class );

	printf( "window_class_init:\n" ); 

	widget_class->destroy = window_destroy;

}

typedef struct _WindowUpdate {
	Window *window;
	VipsRect rect;
} WindowUpdate;

/* The main GUI thread runs this when it's idle and there are tiles that need
 * painting. 
 */
static gboolean
window_render_cb( WindowUpdate *update )
{
	gtk_widget_queue_draw_area( update->window->drawing_area,
		update->rect.left, update->rect.top,
		update->rect.width, update->rect.height );

	g_free( update );

	return( FALSE );
}

/* Come here from the vips_sink_screen() background thread when a tile has been
 * calculated. We can't paint the screen directly since the main GUI thread
 * might be doing something. Instead, we add an idle callback which will be
 * run by the main GUI thread when it next hits the mainloop.
 */
static void
window_render_notify( VipsImage *image, VipsRect *rect, void *client )
{
	Window *window = (Window *) client;
	WindowUpdate *update = g_new( WindowUpdate, 1 );

	update->window = window;
	update->rect = *rect;

	g_idle_add( (GSourceFunc) window_render_cb, update );
}

/* Make the image for display from the raw disc image. Could do
 * anything here, really. Uncomment sections to try different effects. Convert
 * to 8-bit RGB would be a good idea.
 */
static VipsImage *
window_display_image( Window *window, VipsImage *in )
{
	VipsImage *image;
	VipsImage *x;

	/* Edit these to add or remove things from the pipeline we build. These
	 * should be wired up to something in a GUI.
	 */
	const gboolean zoom_in = FALSE;
	const gboolean zoom_out = TRUE;

	/* image represents the head of the pipeline. Hold a ref to it as we
	 * work.
	 */
	image = in;
	g_object_ref( image ); 

	if( zoom_out ) {
		if( vips_subsample( image, &x, 4, 4, NULL ) ) {
			g_object_unref( image );
			return( NULL ); 
		}
		g_object_unref( image );
		image = x;
	}

	if( zoom_in ) {
		if( vips_zoom( image, &x, 4, 4, NULL ) ) {
			g_object_unref( image );
			return( NULL ); 
		}
		g_object_unref( image );
		image = x;
	}

	/* This won't work for CMYK, you need to mess about with ICC profiles
	 * for that, but it will do everything else.
	 */
	if( vips_colourspace( image, &x, VIPS_INTERPRETATION_sRGB, NULL ) ) {
		g_object_unref( image );
		return( NULL ); 
	}
	g_object_unref( image );
	image = x;

	/* Drop any alpha.
	 */
	if( vips_extract_band( image, &x, 0, "n", 3, NULL ) ) {
		g_object_unref( image );
		return( NULL ); 
	}
	g_object_unref( image );
	image = x;

	x = vips_image_new();
	if( vips_sink_screen( image, x, NULL, 128, 128, 400, 0, 
		window_render_notify, window ) ) {
		g_object_unref( image );
		g_object_unref( x );
		return( NULL );
	}
	g_object_unref( image );
	image = x;

	return( image );
}

static void
window_draw_rect( Window *window, cairo_t *cr, VipsRect *expose )
{
	VipsRect image;
	VipsRect clip;
	unsigned char *cairo_buffer;
	int x, y;
	cairo_surface_t *surface;

	/*
	printf( "window_draw_rect: "
		"left = %d, top = %d, width = %d, height = %d\n",
		expose->left, expose->top,
		expose->width, expose->height );
	 */

	/* Clip against the image size ... we don't want to try painting 
	 * outside the image area.
	 */
	image.left = 0;
	image.top = 0;
	image.width = window->region->im->Xsize;
	image.height = window->region->im->Ysize;
	vips_rect_intersectrect( &image, expose, &clip );
	if( vips_rect_isempty( &clip ) ||
		vips_region_prepare( window->region, &clip ) )
		return;

	/* libvips is RGB, cairo is ARGB, we have to repack the data.
	 */
	cairo_buffer = g_malloc( clip.width * clip.height * 4 );

	for( y = 0; y < clip.height; y++ ) {
		VipsPel *p = VIPS_REGION_ADDR( window->region, 
			clip.left, clip.top + y );
		unsigned char *q = cairo_buffer + clip.width * 4 * y;

		for( x = 0; x < clip.width; x++ ) {
			q[0] = p[2];
			q[1] = p[1];
			q[2] = p[0];
			q[3] = 0;

			p += 3;
			q += 4;
		}
	}

	surface = cairo_image_surface_create_for_data( cairo_buffer, 
		CAIRO_FORMAT_RGB24, clip.width, clip.height, clip.width * 4 );

	cairo_set_source_surface( cr, surface, clip.left, clip.top );

	cairo_paint( cr );

	g_free( cairo_buffer ); 

	cairo_surface_destroy( surface ); 
}

static void
window_draw( GtkWidget *drawing_area, cairo_t *cr, Window *window )
{
	cairo_rectangle_list_t *rectangle_list = 
		cairo_copy_clip_rectangle_list( cr );

	if( !window->region )
		return;

	//printf( "disp_draw:\n" ); 

	if( rectangle_list->status == CAIRO_STATUS_SUCCESS ) { 
		int i;

		for( i = 0; i < rectangle_list->num_rectangles; i++ ) {
			VipsRect expose;

			expose.left = rectangle_list->rectangles[i].x;
			expose.top = rectangle_list->rectangles[i].y;
			expose.width = rectangle_list->rectangles[i].width;
			expose.height = rectangle_list->rectangles[i].height;

			window_draw_rect( window, cr, &expose );
		}
	}

	cairo_rectangle_list_destroy( rectangle_list );
}

static void
window_activate_toggle( GSimpleAction *action, 
	GVariant *parameter, gpointer user_data )
{
	GVariant *state;

	printf( "window_activate_toggle:\n" ); 

	state = g_action_get_state( G_ACTION( action ) );
	g_action_change_state( G_ACTION( action ), 
		g_variant_new_boolean( !g_variant_get_boolean( state ) ) );
	g_variant_unref( state );
}

static void
window_change_fullscreen_state( GSimpleAction *action, 
	GVariant *state, gpointer user_data )
{
	Window *window = (Window *) user_data;

	printf( "window_change_fullscreen_state:\n" ); 

	if( g_variant_get_boolean( state ) )
		gtk_window_fullscreen( GTK_WINDOW( window ) );
	else
		gtk_window_unfullscreen( GTK_WINDOW( window ) );

	g_simple_action_set_state( action, state );
}

static GActionEntry window_entries[] = {
	{ "fullscreen", window_activate_toggle, NULL, "false", 
		window_change_fullscreen_state }
};

static void
window_close_memory( VipsImage *image, gpointer contents )
{
	g_free( contents );
}

Window *
window_new( GtkApplication *application, GFile *file )
{
	Disp *disp = (Disp *) application;
	Window *window;

	GtkWidget *grid, *scrolled;
	GtkWidget *toolbar;
	GtkToolItem *button;
	GtkWidget *sw, *box, *label;

	printf( "window_new: file = %p\n", file ); 

	window = g_object_new( window_get_type(),
		"application", application,
		NULL );
	gtk_window_set_default_size( GTK_WINDOW( window ), 640, 480);
	g_action_map_add_action_entries( G_ACTION_MAP( window ), 
		window_entries, G_N_ELEMENTS( window_entries ), 
		window );
	gtk_window_set_title( GTK_WINDOW( window ), "vipsdisp" );

	window->disp = disp;

	grid = gtk_grid_new();
	gtk_container_add( GTK_CONTAINER( window ), grid );

	toolbar = gtk_toolbar_new();

	button = gtk_separator_tool_item_new();
	gtk_separator_tool_item_set_draw( GTK_SEPARATOR_TOOL_ITEM( button ), 
		FALSE );
	gtk_tool_item_set_expand( GTK_TOOL_ITEM( button), TRUE );
	gtk_container_add( GTK_CONTAINER( toolbar ), GTK_WIDGET( button ) );

	button = gtk_tool_item_new();
	box = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 6 );
	gtk_container_add( GTK_CONTAINER( button ), box );
	label = gtk_label_new( "Fullscreen:" );
	gtk_container_add( GTK_CONTAINER( box ), label );
	sw = gtk_switch_new();
	gtk_actionable_set_action_name( GTK_ACTIONABLE( sw ), 
		"win.fullscreen" );
	gtk_container_add( GTK_CONTAINER( box ), sw );
	gtk_container_add( GTK_CONTAINER( toolbar ), GTK_WIDGET( button ) );

	gtk_grid_attach( GTK_GRID( grid ), toolbar, 0, 0, 1, 1 );

	scrolled = gtk_scrolled_window_new( NULL, NULL );
	gtk_widget_set_hexpand( scrolled, TRUE );
	gtk_widget_set_vexpand( scrolled, TRUE );

	window->drawing_area = gtk_drawing_area_new();
	g_signal_connect( window->drawing_area, "draw", 
		G_CALLBACK( window_draw ), window );
	gtk_container_add( GTK_CONTAINER( scrolled ), window->drawing_area );

	gtk_grid_attach( GTK_GRID( grid ), scrolled, 0, 1, 1, 1 );

	if( file != NULL ) {
		gchar *path;
		gchar *contents;
		gsize length;

		if( (path = g_file_get_path( file )) ) {
			if( !(window->image = 
				vips_image_new_from_file( path, NULL )) )
				vips_error_exit( NULL );

			g_free( path ); 
		}
		else if( g_file_load_contents( file, NULL, 
			&contents, &length, NULL, NULL ) ) {
			if( !(window->image =
				vips_image_new_from_buffer( contents, length, 
					"", NULL )) )
				vips_error_exit( NULL );

			g_signal_connect( window->image, "close",
				G_CALLBACK( window_close_memory ), 
				contents );
		}
		else {
			vips_error_exit( "unable to load GFile object" );
		}
	}

	if( window->image ) { 
		if( !(window->display = window_display_image( window, 
			window->image )) ) 
			vips_error_exit( NULL ); 
		if( !(window->region = vips_region_new( window->display )) )
			vips_error_exit( "unable to build display image" );

		gtk_widget_set_size_request( window->drawing_area, 
			window->display->Xsize, window->display->Ysize );
	}

	gtk_widget_show_all( GTK_WIDGET( window ) );

	return( window ); 
}
