#include "vipsdisp.h"
#include <assert.h>

/*
#define DEBUG_VERBOSE
#define DEBUG
 */

#define DEFAULT_SPACING 10

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
        //Saveoptions *saveoptions = (Saveoptions *) object;
        //VIPS_FREEF( gtk_widget_unparent, saveoptions->content_area );
        G_OBJECT_CLASS( saveoptions_parent_class )->dispose( object );
}

/* TileSource has a new image.
 */
static void
saveoptions_tile_source_changed( TileSource *tile_source, Saveoptions *saveoptions ) 
{
	/* */
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
saveoptions_build_save_operation_argument_map_fn_helper( GParamSpec *pspec,
	VipsArgumentClass *argument_class, Saveoptions *saveoptions,
	VipsObject *operation )
{
	GType otype = G_PARAM_SPEC_VALUE_TYPE( pspec );
	GtkWidget *it, *box, *parent;
	const gchar *property_name;

	parent = saveoptions->content_area;

	property_name = g_param_spec_get_name( pspec );

	if( g_type_is_a( otype, VIPS_TYPE_IMAGE )) {
	}
	else if( g_type_is_a( otype, VIPS_TYPE_OBJECT ) ) {
		//VipsObjectClass *oclass;
		//oclass = g_type_class_ref( otype ));
		//if ( oclass ) {
		// 	...
		//}
	}
	else if( G_IS_PARAM_SPEC_STRING( pspec ) ) {
		//GParamSpecString *pspec_string = G_PARAM_SPEC_STRING( pspec );

		box = gtk_widget_get_first_child( parent );
		it = gtk_widget_get_last_child( box );

		GtkEntryBuffer *buffer = gtk_text_get_buffer( GTK_TEXT( it ) );
		const char* text = gtk_entry_buffer_get_text( buffer );
		const char* none = "none";

		if( !text || !strlen( text ) )
			g_object_set( VIPS_OBJECT( operation ),
				property_name, none,
				NULL );
	
		else
			g_object_set( VIPS_OBJECT( operation ),
				property_name, text,
				NULL );
			

		gtk_widget_unparent( box );
	}
	else if( G_IS_PARAM_SPEC_BOOLEAN( pspec ) ) {
		it = gtk_widget_get_first_child( parent );

		gboolean active = gtk_check_button_get_active( GTK_CHECK_BUTTON( it ) );

		g_object_set( VIPS_OBJECT( operation ),
			property_name, active,
			NULL );

		gtk_widget_unparent( it );
	}
	else if( G_IS_PARAM_SPEC_ENUM( pspec ) ) {
		return;
		GParamSpecEnum *pspec_enum = G_PARAM_SPEC_ENUM( pspec );

		box = gtk_widget_get_first_child( parent );
		it = gtk_widget_get_last_child( box );

		gint index = gtk_drop_down_get_selected( GTK_DROP_DOWN( it ) );

		guint value = pspec_enum->enum_class->values[index].value;

		g_object_set( VIPS_OBJECT( operation ),
			property_name, value,
			NULL );

		gtk_widget_unparent( box );
	}
	else if( G_IS_PARAM_SPEC_INT64( pspec ) ) {
		box = gtk_widget_get_first_child( parent );
		it = gtk_widget_get_last_child( box );

		gint64 value = (gint64) gtk_spin_button_get_value( GTK_SPIN_BUTTON( it ) );

		g_object_set( VIPS_OBJECT( operation ),
			property_name, value,
			NULL );

		gtk_widget_unparent( box );
	}
	else if( G_IS_PARAM_SPEC_INT( pspec )) {
		box = gtk_widget_get_first_child( parent );
		it = gtk_widget_get_last_child( box );

		gint64 value = (gint) gtk_spin_button_get_value( GTK_SPIN_BUTTON( it ) );

		g_object_set( VIPS_OBJECT( operation ),
			property_name, value,
			NULL );

		gtk_widget_unparent( box );
	}
	else if( G_IS_PARAM_SPEC_UINT64( pspec ) ) {
		box = gtk_widget_get_first_child( parent );
		it = gtk_widget_get_last_child( box );

		guint64 value = (guint64) gtk_spin_button_get_value( GTK_SPIN_BUTTON( it ) );

		g_object_set( VIPS_OBJECT( operation ),
			property_name, value,
			NULL );

		gtk_widget_unparent( box );
	}
	else if( G_IS_PARAM_SPEC_DOUBLE( pspec ) ) {
		box = gtk_widget_get_first_child( parent );
		it = gtk_widget_get_last_child( box );

		gdouble value = gtk_spin_button_get_value( GTK_SPIN_BUTTON( it ) );

		g_object_set( VIPS_OBJECT( operation ),
			property_name, value,
			NULL );

		gtk_widget_unparent( box );
	}
	else if( G_IS_PARAM_SPEC_BOXED( pspec ) ) {
		if( g_type_is_a( otype, VIPS_TYPE_ARRAY_INT ) ) {
		}
		else if( g_type_is_a( otype, VIPS_TYPE_ARRAY_DOUBLE ) ) {
		}
		else if( g_type_is_a( otype, VIPS_TYPE_ARRAY_IMAGE ) ) {
		}
		else {
		}
	}
	else {
	}
}

static void *
saveoptions_build_save_operation_argument_map_fn( VipsObject *operation,
	GParamSpec *pspec,
	VipsArgumentClass *argument_class,
	VipsArgumentInstance *argument_instance,
	void *a,
	void *b )
{
	VipsArgumentFlags flags = argument_class->flags;
	Saveoptions *saveoptions = (Saveoptions *)a; 

	if ( !(flags & VIPS_ARGUMENT_DEPRECATED) &&
		(flags & VIPS_ARGUMENT_CONSTRUCT) &&
		!(flags & VIPS_ARGUMENT_REQUIRED) )
		saveoptions_build_save_operation_argument_map_fn_helper( pspec,
			argument_class, saveoptions, operation );

	return NULL;
}

void
saveoptions_build_save_operation( Saveoptions *saveoptions,
	VipsOperation *operation )
{
	vips_argument_map( VIPS_OBJECT( operation ),
		saveoptions_build_save_operation_argument_map_fn,
		SAVEOPTIONS( saveoptions ),
		NULL);
}

static void
saveoptions_build_ui_argument_map_fn_helper( GParamSpec *pspec,
	VipsArgumentClass *argument_class, GtkWidget *parent,
	VipsObject *operation )
{
	VipsObjectClass *oclass;
	GType otype = G_PARAM_SPEC_VALUE_TYPE( pspec );
	GtkWidget *it;
	const gchar *property_name;

	property_name = g_param_spec_get_name( pspec );

	if( g_type_is_a( otype, VIPS_TYPE_IMAGE )) {
	}
	else if( g_type_is_a( otype, VIPS_TYPE_OBJECT ) &&
		(oclass = g_type_class_ref( otype )) ) {
	}
	else if( G_IS_PARAM_SPEC_STRING( pspec ) ) {
		//GParamSpecString *pspec_string = G_PARAM_SPEC_STRING( pspec );

		it = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, DEFAULT_SPACING );
		gtk_widget_set_parent( gtk_label_new( property_name ), it );
		gtk_widget_set_parent( gtk_text_new(), it );
		gtk_widget_set_parent( it, parent );
	}
	else if( G_IS_PARAM_SPEC_BOOLEAN( pspec ) ) {
		//GParamSpecBoolean *pspec_bool = G_PARAM_SPEC_BOOLEAN( pspec );

		it = gtk_check_button_new_with_label( property_name );
		gtk_widget_set_parent( it, parent );
	}
	else if( G_IS_PARAM_SPEC_ENUM( pspec ) ) {
		return;
		GParamSpecEnum *pspec_enum = G_PARAM_SPEC_ENUM( pspec );

		const char **property_nicknames =
			g_malloc( (pspec_enum->enum_class->n_values + 1) * sizeof( pspec_enum->default_value ) );

		for( int i = 0; i < pspec_enum->enum_class->n_values; ++i ) {
			property_nicknames[i] =
				pspec_enum->enum_class->values[i].value_nick;
		}

		property_nicknames[pspec_enum->enum_class->n_values] = NULL;

		it = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, DEFAULT_SPACING );
		gtk_widget_set_parent( gtk_label_new( property_name ), it );
		gtk_widget_set_parent(
			gtk_drop_down_new_from_strings( property_nicknames ), it );
		gtk_widget_set_parent( it, parent );
	}
	else if( G_IS_PARAM_SPEC_INT64( pspec ) ) {
		GParamSpecInt64 *pspec_int64 = G_PARAM_SPEC_INT64( pspec );

		it = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, DEFAULT_SPACING );
		gtk_widget_set_parent( gtk_label_new( property_name ), it );
		gtk_widget_set_parent(
			gtk_spin_button_new_with_range( pspec_int64->minimum,
				pspec_int64->maximum, 1 ),
			it);
		gtk_widget_set_parent( it, parent );
	}
	else if( G_IS_PARAM_SPEC_INT( pspec )) {
		GParamSpecInt *pspec_int = G_PARAM_SPEC_INT( pspec );

		it = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, DEFAULT_SPACING );
		gtk_widget_set_parent( gtk_label_new( property_name ), it );
		gtk_widget_set_parent(
			gtk_spin_button_new_with_range( pspec_int->minimum,
				pspec_int->maximum, 1 ),
			it );
		gtk_widget_set_parent( it, parent );
	}
	else if( G_IS_PARAM_SPEC_UINT64( pspec ) ) {
		GParamSpecUInt64 *pspec_uint64 = G_PARAM_SPEC_UINT64( pspec );

		it = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, DEFAULT_SPACING );
		gtk_widget_set_parent( gtk_label_new( property_name ), it );
		gtk_widget_set_parent(
			gtk_spin_button_new_with_range( pspec_uint64->minimum,
				pspec_uint64->maximum, 1 ),
			it);
		gtk_widget_set_parent( it, parent );
	}
	else if( G_IS_PARAM_SPEC_DOUBLE( pspec ) ) {
		GParamSpecDouble *pspec_double = G_PARAM_SPEC_DOUBLE( pspec );

		it = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, DEFAULT_SPACING );
		gtk_widget_set_parent( gtk_label_new( property_name ), it );
		gtk_widget_set_parent(
			gtk_spin_button_new_with_range( pspec_double->minimum,
				pspec_double->maximum, .01 ),
			it);
		gtk_widget_set_parent( it, parent );
	}
	else if( G_IS_PARAM_SPEC_BOXED( pspec ) ) {
		if( g_type_is_a( otype, VIPS_TYPE_ARRAY_INT ) ) {
		}
		else if( g_type_is_a( otype, VIPS_TYPE_ARRAY_DOUBLE ) ) {
		}
		else if( g_type_is_a( otype, VIPS_TYPE_ARRAY_IMAGE ) ) {
		}
		else {
		}
	}
	else {
	}
}

static void *
saveoptions_build_ui_argument_map_fn( VipsObject *operation,
	GParamSpec *pspec,
	VipsArgumentClass *argument_class,
	VipsArgumentInstance *argument_instance,
	void *a,
	void *b )
{
	VipsArgumentFlags flags = argument_class->flags;
	GtkWidget *parent = (GtkWidget *)a; 

	if ( !(flags & VIPS_ARGUMENT_DEPRECATED) &&
		(flags & VIPS_ARGUMENT_CONSTRUCT) &&
		!(flags & VIPS_ARGUMENT_REQUIRED) )
		saveoptions_build_ui_argument_map_fn_helper( pspec,
			argument_class, parent, operation );

	return NULL;
}

/* Build a widget containing the saveoptions UI, and attach it to the given
 * parent widget.
 */
static void
saveoptions_build_ui( GtkWidget *parent, VipsOperation *operation )
{
	g_assert( parent != NULL );
	g_assert( operation != NULL );

	vips_argument_map( VIPS_OBJECT( operation ),
		saveoptions_build_ui_argument_map_fn, parent, NULL);
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
		GtkWidget *parent, *it;
		GFile *target_file;
		gchar *path, *filename_suffix, *operation_name;
		VipsOperation *operation;

		gboolean visible = g_value_get_boolean( value );

		parent = saveoptions->content_area;

		it = gtk_widget_get_first_child( parent );

		if( it != NULL ) {
			gtk_widget_unparent( it );
			while( (it = gtk_widget_get_first_child( parent )) )
				gtk_widget_unparent( it );
		}

		if( !visible )
			return;

		target_file = image_window_get_target_file( saveoptions->win );

		if( !(path = g_file_get_path( target_file ))
			|| !(filename_suffix = strrchr( path, '.' )) )
			return;

		operation_name = g_strdup_printf( "%ssave", ++filename_suffix );

		operation = vips_operation_new( operation_name );

		saveoptions_build_ui( parent, operation );

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

        saveoptions = g_object_new( saveoptions_get_type(), 
                "image-window", win,
                NULL );

        return( saveoptions ); 
}
