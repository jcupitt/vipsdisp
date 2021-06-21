#include "vipsdisp.h"

struct _ImageWindow
{
	GtkApplicationWindow parent;

	Conversion *conversion;

	GtkWidget *title;
	GtkWidget *subtitle;
	GtkWidget *gears;
	GtkWidget *progress_bar;
	GtkWidget *progress;
	GtkWidget *progress_cancel;
	GtkWidget *error_bar;
	GtkWidget *error_label;
	GtkWidget *scrolled_window;
	GtkWidget *imagedisplay;
	GtkWidget *conversion_bar;
	GtkWidget *info_bar;
};

G_DEFINE_TYPE( ImageWindow, image_window, GTK_TYPE_APPLICATION_WINDOW );

static void
image_window_dispose( GObject *object )
{
	ImageWindow *win = VIPSDISP_IMAGE_WINDOW( object );

#ifdef DEBUG
#endif /*DEBUG*/
	printf( "image_window_dispose:\n" ); 

	VIPS_UNREF( win->conversion );

	G_OBJECT_CLASS( image_window_parent_class )->dispose( object );
}

static void
image_window_preeval( VipsImage *image, 
        VipsProgress *progress, ImageWindow *win )
{
	gtk_action_bar_set_revealed( GTK_ACTION_BAR( win->progress_bar ), 
		TRUE );
}

static void
image_window_eval( VipsImage *image, 
        VipsProgress *progress, ImageWindow *win )
{
	static int previous_percent = -1;

#ifdef DEBUG_VERBOSE
        printf( "image_window_eval: %d%%\n", progress->percent );
#endif /*DEBUG_VERBOSE*/

        if( progress->percent != previous_percent ) {
                char str[256];
                VipsBuf buf = VIPS_BUF_STATIC( str );

                gtk_progress_bar_set_fraction(
                        GTK_PROGRESS_BAR( win->progress ),
                        progress->percent / 100.0 );
                vips_buf_appendf( &buf, "%d%% complete, %d seconds to go",
                        progress->percent, progress->eta );
                gtk_progress_bar_set_text( GTK_PROGRESS_BAR( win->progress ),
                        vips_buf_all( &buf ) );

                previous_percent = progress->percent;

                /* Run one loop iteration, don't block.
                 */
                g_main_context_iteration( NULL, FALSE );
        }
}

static void
image_window_posteval( VipsImage *image, 
        VipsProgress *progress, ImageWindow *win )
{
	gtk_action_bar_set_revealed( GTK_ACTION_BAR( win->progress_bar ), 
		FALSE );
}

static void
image_window_cancel_clicked( GtkWidget *button, ImageWindow *win )
{
        if( win->conversion->image )
                vips_image_set_kill( win->conversion->image, TRUE );
}

static void
image_window_error( ImageWindow *win )
{
	char *err;
        int i;

        /* Remove any trailing \n.
         */
        err = vips_error_buffer_copy();
        for( i = strlen( err ); i > 0 && err[i - 1] == '\n'; i-- )
                err[i - 1] = '\0';
        gtk_label_set_text( GTK_LABEL( win->error_label ), err );
        g_free( err );

	gtk_info_bar_set_revealed( GTK_INFO_BAR( win->error_bar ), TRUE );
}

static void
image_window_error_response( GtkWidget *button, int response, ImageWindow *win )
{
	gtk_info_bar_set_revealed( GTK_INFO_BAR( win->error_bar ), FALSE );
}

static void
image_window_init( ImageWindow *win )
{
	GtkBuilder *builder;
	GMenuModel *menu;

	gtk_widget_init_template( GTK_WIDGET( win ) );

	builder = gtk_builder_new_from_resource( 
		"/org/libvips/vipsdisp/imagewindow-menu.ui" );
	menu = G_MENU_MODEL( gtk_builder_get_object( builder, 
		"imagewindow-menu" ) );
	gtk_menu_button_set_menu_model( GTK_MENU_BUTTON( win->gears ), menu );
	g_object_unref( builder );

	win->conversion = conversion_new();
	g_object_set( win->imagedisplay,
		"conversion", win->conversion,
		NULL );

        g_signal_connect_object( win->conversion, "preeval", 
                G_CALLBACK( image_window_preeval ), win, 0 );
        g_signal_connect_object( win->conversion, "eval", 
                G_CALLBACK( image_window_eval ), win, 0 );
        g_signal_connect_object( win->conversion, "posteval", 
                G_CALLBACK( image_window_posteval ), win, 0 );
        g_signal_connect_object( win->progress_cancel, "clicked", 
                G_CALLBACK( image_window_cancel_clicked ), win, 0 );

        g_signal_connect_object( win->error_bar, "response", 
                G_CALLBACK( image_window_error_response ), win, 0 );

}

#define BIND( field ) \
	gtk_widget_class_bind_template_child( GTK_WIDGET_CLASS( class ), \
		ImageWindow, field );

static void
image_window_class_init( ImageWindowClass *class )
{
	G_OBJECT_CLASS( class )->dispose = image_window_dispose;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( class ),
		"/org/libvips/vipsdisp/imagewindow.ui");

	BIND( title );
	BIND( subtitle );
	BIND( gears );
	BIND( progress_bar );
	BIND( progress );
	BIND( progress_cancel );
	BIND( error_bar );
	BIND( error_label );
	BIND( scrolled_window );
	BIND( imagedisplay );
	BIND( conversion_bar );
	BIND( info_bar );
}

ImageWindow *
image_window_new( VipsdispApp *app )
{
	return( g_object_new( IMAGE_WINDOW_TYPE, "application", app, NULL ) );
}

void
image_window_open( ImageWindow *win, GFile *file )
{
	if( conversion_set_file( win->conversion, file ) )
		image_window_error( win ); 
}
