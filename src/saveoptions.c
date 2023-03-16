#include "vipsdisp.h"
#include <assert.h>

/*
#define DEBUG_VERBOSE
#define DEBUG
 */

struct _Saveoptions {
        GtkWidget parent_instance;

        ImageWindow *win;

        GtkWidget *content_area;

        GSList *value_widgets;

        GSettings *settings;
};

G_DEFINE_TYPE( Saveoptions, saveoptions, GTK_TYPE_WIDGET );

enum {
        PROP_IMAGE_WINDOW = 1,
        PROP_VISIBLE,

        SIG_LAST
};

static void
saveoptions_dispose( GObject *object )
{
        Saveoptions *saveoptions = (Saveoptions *) object;

#ifdef DEBUG
        printf( "saveoptions_dispose:\n" ); 
#endif /*DEBUG*/

        VIPS_FREEF( gtk_widget_unparent, saveoptions->content_area );

        G_OBJECT_CLASS( saveoptions_parent_class )->dispose( object );
}

/* TileSource has a new image.
 */
static void
saveoptions_tile_source_changed( TileSource *tile_source, Saveoptions *saveoptions ) 
{
#ifdef DEBUG
        printf( "saveoptions_tile_source_changed:\n" ); 
#endif /*DEBUG*/
}

/* Imagewindow has a new tile_source.
 */
static void
saveoptions_image_window_changed( ImageWindow *win, Saveoptions *saveoptions )
{
	TileSource *tile_source = image_window_get_tile_source( win );

        g_signal_connect_object( tile_source, "changed", 
                G_CALLBACK( saveoptions_tile_source_changed ), 
                saveoptions, 0 );
}

static void
saveoptions_set_image_window( Saveoptions *saveoptions, ImageWindow *win )
{
        /* No need to ref ... win holds a ref to us.
         */
        saveoptions->win = win;

        g_signal_connect_object( win, "changed", 
                G_CALLBACK( saveoptions_image_window_changed ), 
                saveoptions, 0 );
}

static void
saveoptions_set_property( GObject *object, 
        guint prop_id, const GValue *value, GParamSpec *pspec )
{
        Saveoptions *saveoptions = (Saveoptions *) object;

        switch( prop_id ) {
        case PROP_IMAGE_WINDOW:
                saveoptions_set_image_window( saveoptions, 
                        VIPSDISP_IMAGE_WINDOW( g_value_get_object( value ) ) );
                break;

        case PROP_VISIBLE:
		GtkWidget * parent, * it, * label, * text;

		gboolean visible = g_value_get_boolean( value );

		parent = saveoptions->content_area;

		it = gtk_widget_get_first_child( parent );

		if( it != NULL ){
			// Remove all children of the content_area box
			gtk_widget_unparent( it ); // (first)
			while(( it = gtk_widget_get_first_child( parent ) )){
				// Remove the reference to child held by its parent,
				// cleaning up child.
				gtk_widget_unparent( it );
			}
		}

		if( visible ){
			it = gtk_check_button_new_with_label("Bool 0");
			g_settings_bind( saveoptions->settings, "bool0",
				it,
				"active", 
				G_SETTINGS_BIND_DEFAULT );
			gtk_widget_set_parent( it, parent );

			it = gtk_check_button_new_with_label("Bool 1");
			g_settings_bind( saveoptions->settings, "bool1",
				it,
				"active", 
				G_SETTINGS_BIND_DEFAULT );
			gtk_widget_set_parent( it, parent );

			it = gtk_scale_button_new(0, 100, 1, NULL);
			g_settings_bind( saveoptions->settings, "uint0",
				it,
				"value", 
				G_SETTINGS_BIND_DEFAULT );
			gtk_widget_set_parent( it, parent );


			it = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);

			label = gtk_label_new("String 0:");
			gtk_box_append( GTK_BOX( it ), label );

			text = gtk_text_new();
			gtk_box_append( GTK_BOX( it ), text );
			gtk_widget_set_margin_bottom( it, 20 );

			g_settings_bind( saveoptions->settings, "string0",
				text,
				"text", 
				G_SETTINGS_BIND_DEFAULT );

			gtk_widget_set_parent( it, parent );
		}

                break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID( object, prop_id, pspec );
                break;
        }
}

static void
saveoptions_get_property( GObject *object, 
        guint prop_id, GValue *value, GParamSpec *pspec )
{
        Saveoptions *saveoptions = (Saveoptions *) object;

        switch( prop_id ) {
        case PROP_IMAGE_WINDOW:
                g_value_set_object( value, saveoptions->win );
                break;

        case PROP_VISIBLE:
		GtkWidget* parent = GTK_WIDGET( saveoptions->content_area );
		gboolean visible = gtk_widget_get_first_child( parent ) != NULL;
                g_value_set_boolean( value, visible ); 
                break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID( object, prop_id, pspec );
                break;
        }
}

static void
saveoptions_init( Saveoptions *saveoptions )
{
#ifdef DEBUG
        printf( "saveoptions_init:\n" ); 
#endif /*DEBUG*/

	saveoptions->settings = g_settings_new( APPLICATION_ID );

        gtk_widget_init_template( GTK_WIDGET( saveoptions ) );
}

#define BIND( field ) \
        gtk_widget_class_bind_template_child( GTK_WIDGET_CLASS( class ), \
                Saveoptions, field );

static void
saveoptions_class_init( SaveoptionsClass *class )
{
        GObjectClass *gobject_class = G_OBJECT_CLASS( class );
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS( class );

#ifdef DEBUG
        printf( "saveoptions_class_init:\n" ); 
#endif /*DEBUG*/

        G_OBJECT_CLASS( class )->dispose = saveoptions_dispose;

        gtk_widget_class_set_layout_manager_type( widget_class, 
                GTK_TYPE_BIN_LAYOUT );
        gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( class ),
                APP_PATH "/saveoptions.ui");

        BIND( content_area );

        gobject_class->set_property = saveoptions_set_property;
        gobject_class->get_property = saveoptions_get_property;

        g_object_class_install_property( gobject_class, PROP_IMAGE_WINDOW,
                g_param_spec_object( "image-window",
                        _( "Image window" ),
                        _( "The image window we display" ),
                        IMAGE_WINDOW_TYPE,
                        G_PARAM_READWRITE ) );

        g_object_class_install_property( gobject_class, PROP_VISIBLE,
                g_param_spec_boolean( "visible",
                        _( "visible" ),
                        _( "Show the display control bar" ),
                        FALSE,
                        G_PARAM_READWRITE ) );

}

Saveoptions *
saveoptions_new( ImageWindow *win ) 
{
        Saveoptions *saveoptions;

#ifdef DEBUG
        printf( "saveoptions_new:\n" ); 
#endif /*DEBUG*/

        saveoptions = g_object_new( saveoptions_get_type(), 
                "image-window", win,
                NULL );

        return( saveoptions ); 
}
