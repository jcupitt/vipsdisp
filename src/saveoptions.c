#include "vipsdisp.h"
#include <assert.h>

#define DEFAULT_SPACING 10

/* Clean up the memory and widgets held by the SaveOptions object.
 */
void
save_options_free( SaveOptions *save_options )
{
	gtk_box_remove( save_options->parent_box,
		GTK_WIDGET( save_options->content_box ) );
	g_free( save_options );
}

/* Create a new empty SaveOptions object.
 */
SaveOptions *
save_options_new_empty()
{
	SaveOptions *save_options;

	save_options = g_malloc( sizeof( SaveOptions ) );
	save_options->parent_box = NULL;
	save_options->image_window = NULL;
	save_options->content_box = NULL;

	return save_options;
}

/* Initialize a given empty SaveOptions object using a parent GtkBox and
 * ImageWindow.
 */
void
save_options_init( SaveOptions *save_options,
	GtkBox *parent_box,
	ImageWindow *image_window )
{
	GtkBox *content_box;

	save_options->image_window = image_window;

	content_box = GTK_BOX( gtk_box_new( GTK_ORIENTATION_VERTICAL,
		DEFAULT_SPACING ) );
	
	gtk_box_set_homogeneous( content_box, TRUE );

	gtk_widget_set_halign( GTK_WIDGET( content_box ), GTK_ALIGN_FILL );

	gtk_widget_set_margin_top( GTK_WIDGET( content_box ), DEFAULT_SPACING );
	gtk_widget_set_margin_end( GTK_WIDGET( content_box ), DEFAULT_SPACING );
	gtk_widget_set_margin_bottom( GTK_WIDGET( content_box ), DEFAULT_SPACING );
	gtk_widget_set_margin_start( GTK_WIDGET( content_box ), DEFAULT_SPACING );

	save_options->content_box = content_box;

	save_options->parent_box = parent_box;

	g_assert( parent_box );

	gtk_box_append( parent_box, GTK_WIDGET( content_box ) );
}

/* Create a new, initialized SaveOptions object from a parent GtkBox and an
 * ImageWindow.
 */
SaveOptions *
save_options_new( GtkBox *parent_box, ImageWindow *image_window ) 
{
        SaveOptions *save_options;
		
	save_options = save_options_new_empty();
	save_options_init( save_options, parent_box, image_window );

        return( save_options ); 
}

/* Get the the image window currently held by a SaveOptions object.
 *
 * @returns ImageWindow * (may be NULL)
 */
ImageWindow *
save_options_get_image_window( SaveOptions *save_options )
{
        return save_options->image_window;
}

/* This is a helper function used by:
 *
 * 	save_options_build_save_operation_argument_map_fn
 *
 * to process a single property of the save operation.
 *
 * It sets the property to the value held by the user input widget pointed to
 * by the widget iterator. 
 */
static void
save_options_build_save_operation_argument_map_fn_helper( GParamSpec *pspec,
	VipsArgumentClass *argument_class, GtkWidget **widget_iterator,
	VipsObject *operation )
{
	if ( !widget_iterator || !*widget_iterator )
		return;

	VipsObjectClass *oclass;
	GType otype = G_PARAM_SPEC_VALUE_TYPE( pspec );
	GtkWidget *t;
	const gchar *property_name;

	property_name = g_param_spec_get_name( pspec );

	t = gtk_widget_get_last_child( *widget_iterator );

	/* Not handling VipsImage or VipsObject types yet.
	*/
	if( g_type_is_a( otype, VIPS_TYPE_IMAGE )) {
		return;
	}
	else if( g_type_is_a( otype, VIPS_TYPE_OBJECT ) &&
		(oclass = g_type_class_ref( otype )) ) {
		return;
	}

	/* Handle types that are not VipsImage or VipsObject.
	 */
	if( G_IS_PARAM_SPEC_STRING( pspec ) ) {
		GtkEntryBuffer *buffer = gtk_text_get_buffer( GTK_TEXT( t ) );
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
	}
	else if( G_IS_PARAM_SPEC_BOOLEAN( pspec ) ) {
		gboolean active = gtk_check_button_get_active( GTK_CHECK_BUTTON( t ) );

		g_object_set( VIPS_OBJECT( operation ),
			property_name, active,
			NULL );
	}
	else if( G_IS_PARAM_SPEC_ENUM( pspec ) ) {
		GParamSpecEnum *pspec_enum = G_PARAM_SPEC_ENUM( pspec );

		gint index = gtk_drop_down_get_selected( GTK_DROP_DOWN( t ) );

		guint value = pspec_enum->enum_class->values[index].value;

		g_object_set( VIPS_OBJECT( operation ),
			property_name, value,
			NULL );
	}
	else if( G_IS_PARAM_SPEC_INT64( pspec ) ) {
		gint64 value = (gint64) gtk_spin_button_get_value( GTK_SPIN_BUTTON( t ) );

		g_object_set( VIPS_OBJECT( operation ),
			property_name, value,
			NULL );
	}
	else if( G_IS_PARAM_SPEC_INT( pspec )) {
		gint64 value = (gint) gtk_spin_button_get_value( GTK_SPIN_BUTTON( t ) );

		g_object_set( VIPS_OBJECT( operation ),
			property_name, value,
			NULL );
	}
	else if( G_IS_PARAM_SPEC_UINT64( pspec ) ) {
		guint64 value = (guint64) gtk_spin_button_get_value( GTK_SPIN_BUTTON( t ) );

		g_object_set( VIPS_OBJECT( operation ),
			property_name, value,
			NULL );
	}
	else if( G_IS_PARAM_SPEC_DOUBLE( pspec ) ) {
		gdouble value = gtk_spin_button_get_value( GTK_SPIN_BUTTON( t ) );

		g_object_set( VIPS_OBJECT( operation ),
			property_name, value,
			NULL );
	}
	else if( G_IS_PARAM_SPEC_BOXED( pspec ) ) {
		if( g_type_is_a( otype, VIPS_TYPE_ARRAY_INT ) ) {
			gint value;
			VipsArrayInt *array_int;

			value = (gint) gtk_spin_button_get_value( GTK_SPIN_BUTTON( t ) );

			/* For now just pretend every array-type parameter has
			 * one element.
			 */
			array_int = vips_array_int_newv( 1, value );

			g_object_set( VIPS_OBJECT( operation ),
				property_name, array_int,
				NULL );
		}
		else if( g_type_is_a( otype, VIPS_TYPE_ARRAY_DOUBLE ) ) {
			gdouble value;
			VipsArrayDouble *array_double;

			value = gtk_spin_button_get_value( GTK_SPIN_BUTTON( t ) );

			/* For now just pretend every array-type parameter has
			 * one element.
			 */
			array_double = vips_array_double_newv( 1, value );

			g_object_set( VIPS_OBJECT( operation ),
				property_name, array_double,
				NULL );
		}
		else if( g_type_is_a( otype, VIPS_TYPE_ARRAY_IMAGE ) ) {
			/* Ignore VipsImage-type parameters for now.
			 */
			return;
		}
		else {
			/* Ignore parameters of unrecognized type for now.
			 */
			return;
		}
	}
	else {
		return;
	}

	*widget_iterator = gtk_widget_get_next_sibling( *widget_iterator );
}

/* This is the function used by save_options_build_save_operation to process
 * a single property of the save operation.
 *
 * See also save_options_build_save_operation_argument_map_fn_helper.
 */
static void *
save_options_build_save_operation_argument_map_fn( VipsObject *operation,
	GParamSpec *pspec,
	VipsArgumentClass *argument_class,
	VipsArgumentInstance *argument_instance,
	void *a,
	void *b )
{
	VipsArgumentFlags flags = argument_class->flags;
	GtkWidget **widget_iterator = (GtkWidget **)a;

	/* Include arguments listed in the constructor.
	 *
	 * Exclude required or deprecated arguments.
	 */
	if ( !(flags & VIPS_ARGUMENT_DEPRECATED) &&
		(flags & VIPS_ARGUMENT_CONSTRUCT) &&
		!(flags & VIPS_ARGUMENT_REQUIRED) )
		save_options_build_save_operation_argument_map_fn_helper( pspec,
			argument_class, widget_iterator, operation );

	return NULL;
}

/* Finish building the VipsOperation, which should already have it's
 * "filename" property set to the source file path
 */
void
save_options_build_save_operation( SaveOptions *save_options,
	VipsOperation *operation )
{
	gchar *filename;
	GtkWidget *widget_iterator;
	GtkWidget *content_box;
	GtkWidget *labels_box, *inputs_box;

	g_object_get( G_OBJECT( operation ),
		"filename", &filename, NULL );

	g_assert( filename );

	/* Get the pointer to the content box widget, held by the SaveOptions
	 * object.
	 */
	content_box = GTK_WIDGET( save_options->content_box );

	g_assert( content_box );

	labels_box = gtk_widget_get_first_child( content_box );

	g_assert( labels_box );

	inputs_box = gtk_widget_get_last_child( content_box );

	g_assert( inputs_box );

	widget_iterator =
		gtk_widget_get_first_child( inputs_box );

	g_assert( widget_iterator );

	/* Loop over the properties of the save operation, while also advancing
	 * the widget iterator. Apply the values from each widget to the save
	 * operation.
	 *
	 * See also "save_options_build_save_operation_argument_map_fn". 
	 */
	vips_argument_map( VIPS_OBJECT( operation ),
		save_options_build_save_operation_argument_map_fn,
		widget_iterator,
		NULL);
}

/* This function is used by:
 *
 * 	save_options_build_content_box_argument_map_fn_helper
 *
 * to process one property of the save operation. The property type and name
 * are used to create a labelled user input element for that property.
 */
static void
save_options_build_content_box_argument_map_fn_helper( GParamSpec *pspec,
	VipsArgumentClass *argument_class, SaveOptions *save_options,
	VipsObject *operation )
{
	VipsObjectClass *oclass;
	GType otype = G_PARAM_SPEC_VALUE_TYPE( pspec );
	const gchar *property_name;
	GtkWidget *t, *label, *box, *labels_box, *inputs_box;

	/* Get the name of the property of the save operation currently being
	 * processed. 
	 */
	property_name = g_param_spec_get_nick( pspec );

	/* Not handling VipsImage or VipsObject.
	*/
	if( g_type_is_a( otype, VIPS_TYPE_IMAGE )) {
		return;
	}
	else if( g_type_is_a( otype, VIPS_TYPE_OBJECT ) &&
		(oclass = g_type_class_ref( otype )) ) {
		return;
	}

	labels_box = gtk_widget_get_first_child( GTK_WIDGET( save_options->content_box ) );

	g_assert( labels_box );

	inputs_box = gtk_widget_get_last_child( GTK_WIDGET( save_options->content_box ) );

	g_assert( inputs_box );

	/* Handle types that are not VipsImage or VipsObject.
	 */

	box = gtk_box_new( GTK_ORIENTATION_HORIZONTAL,
		DEFAULT_SPACING );
	gtk_widget_set_halign( box, GTK_ALIGN_FILL );
	gtk_widget_set_tooltip_text( GTK_WIDGET( box ),
		g_param_spec_get_blurb( pspec ) );

	/* Add the label for this property to the box. Use the property blurb as
	 * a tooltip.
	 */
	label = gtk_label_new( property_name );
	gtk_widget_set_hexpand( label, FALSE );

	g_assert( labels_box );

	gtk_box_append( GTK_BOX( labels_box ), label );

	gtk_widget_set_tooltip_text( GTK_WIDGET( label ),
		g_param_spec_get_blurb( pspec ) );

	/* Add a user input widget for this property to the box. The widget
	 * chosen depends on the type of the property. Set the initial value of
	 * the user input widget to the default value for the property.
	 */
	if( G_IS_PARAM_SPEC_STRING( pspec ) ) {
		GParamSpecString *pspec_string = G_PARAM_SPEC_STRING( pspec );

		GtkEntryBuffer* buffer =
			gtk_entry_buffer_new( pspec_string->default_value, -1 );

		t = gtk_text_new_with_buffer( buffer );
	}
	else if( G_IS_PARAM_SPEC_BOOLEAN( pspec ) ) {
		GParamSpecBoolean *pspec_boolean = G_PARAM_SPEC_BOOLEAN( pspec );

		t = gtk_check_button_new();

		gtk_check_button_set_active( GTK_CHECK_BUTTON( t ),
			pspec_boolean->default_value );
	}
	else if( G_IS_PARAM_SPEC_ENUM( pspec ) ) {
		GParamSpecEnum *pspec_enum = G_PARAM_SPEC_ENUM( pspec );

		const char **property_nicknames =
			g_malloc( (pspec_enum->enum_class->n_values + 1) * sizeof( char * ) );

		for( int i = 0; i < pspec_enum->enum_class->n_values; ++i ) {
			property_nicknames[i] =
				pspec_enum->enum_class->values[i].value_nick;
		}

		property_nicknames[pspec_enum->enum_class->n_values] = NULL;

		t = gtk_drop_down_new_from_strings( property_nicknames );

		gtk_drop_down_set_selected( GTK_DROP_DOWN( t ),
			pspec_enum->default_value );
	}
	else if( G_IS_PARAM_SPEC_INT64( pspec ) ) {
		GParamSpecInt64 *pspec_int64 = G_PARAM_SPEC_INT64( pspec );

		t = gtk_spin_button_new_with_range( pspec_int64->minimum,
			pspec_int64->maximum, 1 );

		gtk_spin_button_set_value( GTK_SPIN_BUTTON( t ),
			(gint64)pspec_int64->default_value );
	}
	else if( G_IS_PARAM_SPEC_INT( pspec )) {
		GParamSpecInt *pspec_int = G_PARAM_SPEC_INT( pspec );

		t = gtk_spin_button_new_with_range( pspec_int->minimum,
			pspec_int->maximum, 1 );

		gtk_spin_button_set_value( GTK_SPIN_BUTTON( t ),
			(gint)pspec_int->default_value );
	}
	else if( G_IS_PARAM_SPEC_UINT64( pspec ) ) {
		GParamSpecUInt64 *pspec_uint64 = G_PARAM_SPEC_UINT64( pspec );

		t = gtk_spin_button_new_with_range( pspec_uint64->minimum,
			pspec_uint64->maximum, 1 );

		gtk_spin_button_set_value( GTK_SPIN_BUTTON( t ),
			(guint64)pspec_uint64->default_value );
	}
	else if( G_IS_PARAM_SPEC_DOUBLE( pspec ) ) {
		GParamSpecDouble *pspec_double = G_PARAM_SPEC_DOUBLE( pspec );

		t = gtk_spin_button_new_with_range( pspec_double->minimum,
			pspec_double->maximum, 1 );

		gtk_spin_button_set_value( GTK_SPIN_BUTTON( t ),
			pspec_double->default_value );
	}
	else if( G_IS_PARAM_SPEC_BOXED( pspec ) ) {	
		if( g_type_is_a( otype, VIPS_TYPE_ARRAY_INT ) ) {
			/* No default values exist for ParamSpecBoxed, so make
			 * some up for now.
			 */
			t = gtk_spin_button_new_with_range( 0, 1000, 1 );

			gtk_spin_button_set_value( GTK_SPIN_BUTTON( t ), 0 );
		}
		else if( g_type_is_a( otype, VIPS_TYPE_ARRAY_DOUBLE ) ) {
			/* No default values exist for ParamSpecBoxed, so make
			 * some up for now.
			 */
			t = gtk_spin_button_new_with_range( 0, 1000, .1 );

			gtk_spin_button_set_value( GTK_SPIN_BUTTON( t ), 0 );
		}
		else if( g_type_is_a( otype, VIPS_TYPE_ARRAY_IMAGE ) ) {
			/* Ignore VipsImage-type parameters for now.
			 */
			return;
		}
		else {
			/* Ignore parameters of unrecognized type for now.
			 */
			return;
		}
	}
	else {
		return;
	}

	/* Make each user input widget stretch when the window is resized. This
	 * is particularly helpful to the user for text fields, since they will
	 * have more room to see what they typed. 
	 */
	gtk_widget_set_halign( t, GTK_ALIGN_FILL );

	gtk_widget_set_hexpand( t, TRUE );

	g_assert( box );

	gtk_box_append( GTK_BOX( box ), t );

	g_assert( inputs_box );

	gtk_box_append( GTK_BOX( inputs_box ), box );
}

/* This is the function used by save_options_build_content_box to process a
 * single property of the save operation.
 *
 * See also save_options_build_content_box_argument_map_fn_helper.
 */
static void *
save_options_build_content_box_argument_map_fn( VipsObject *operation,
	GParamSpec *pspec,
	VipsArgumentClass *argument_class,
	VipsArgumentInstance *argument_instance,
	void *a,
	void *b )
{
	VipsArgumentFlags flags = argument_class->flags;
	SaveOptions *save_options = (SaveOptions *)a; 

	/* Include arguments listed in the constructor.
	 *
	 * Exclude required or deprecated arguments.
	 */
	if ( !(flags & VIPS_ARGUMENT_DEPRECATED) &&
		(flags & VIPS_ARGUMENT_CONSTRUCT) &&
		!(flags & VIPS_ARGUMENT_REQUIRED) )
		save_options_build_content_box_argument_map_fn_helper( pspec,
			argument_class, save_options, operation );

	return NULL;
}

/* Build a widget containing the save_options UI, and attach it to the
 * parent_box
 */
void
save_options_build_content_box( SaveOptions *save_options,
	VipsOperation *operation )
{
	g_assert( save_options->parent_box );
	g_assert( operation );

	vips_argument_map( VIPS_OBJECT( operation ),
		save_options_build_content_box_argument_map_fn,
		save_options,
		NULL);
}

/* Clean up the content box and all its children, and replace it with a new
 * empty content box.
 */
static gint
save_options_reset_content_box( SaveOptions *save_options )
{
	GtkBox *content_box, *labels_box, *inputs_box;

	content_box = save_options->content_box;

	if( content_box )
		gtk_box_remove( save_options->parent_box, GTK_WIDGET( content_box ) );

	/* Create a new content box 
	 */
	content_box = GTK_BOX( gtk_box_new( GTK_ORIENTATION_HORIZONTAL,
		DEFAULT_SPACING ) );

	gtk_widget_set_halign( GTK_WIDGET( content_box ), GTK_ALIGN_FILL );


	/* Give the SaveOptions the pointer to the new content box.
	 */
	save_options->content_box = content_box;

	labels_box = GTK_BOX( gtk_box_new( GTK_ORIENTATION_VERTICAL,
		DEFAULT_SPACING ) );

	gtk_box_append( content_box, GTK_WIDGET( labels_box ) );

	inputs_box = GTK_BOX( gtk_box_new( GTK_ORIENTATION_VERTICAL,
		DEFAULT_SPACING ) );
	
	gtk_box_append( content_box, GTK_WIDGET( inputs_box ) );

	/* Return success code
	 */
	return 0;
}

/* Returns true if the content box has no children
 */
gboolean
save_options_content_box_empty( SaveOptions *save_options )
{
	GtkWidget *parent_box, *child_box = NULL;

	parent_box = GTK_WIDGET( save_options->content_box );

	if ( parent_box )
		child_box = gtk_widget_get_first_child( parent_box );

	return( !child_box );
}

/* Dynamically generate the save options menu based on the current operation
 * held by the image window.
 */
gint
save_options_show( SaveOptions *save_options )
{
	GFile *target_file;
	gchar *path, *filename_suffix, *operation_name;
	VipsOperation *operation;

	//if( !save_options_content_box_empty( save_options ) )
	//	save_options_reset_content_box( save_options );

	save_options_reset_content_box( save_options );

	target_file = image_window_get_target_file( save_options->image_window );

	/* Return error code if path is bad.
	 */
	if( !(path = g_file_get_path( target_file ))
		|| !(filename_suffix = strrchr( path, '.' ))
		|| !(operation_name = g_strdup_printf( "%ssave", ++filename_suffix )) )
		return -1;

	/* Return error code if failed to created operation.
	 */
	if( !(operation = vips_operation_new( operation_name )) )
		return -2;

	save_options_build_content_box( save_options, operation );

	gtk_box_append( GTK_BOX( save_options->parent_box ),
		GTK_WIDGET( save_options->content_box ) );

	/* Return EXIT_SUCCESS code.
	 */
	return 0;
}

/* Destroy the dynamically generated save options menu.
 */
gint
save_options_hide( SaveOptions *save_options )
{
	if( !save_options )
		return -1;

	save_options_free( save_options );

	return 0;
}

/* TileSource has a new image.
 *
 * Not currently used.
 */
static void
save_options_tile_source_changed( TileSource *tile_source, SaveOptions *save_options ) 
{
	/* ... */
}

/* Imagewindow has a new tile_source.
 * 
 * Not currently used.
 */
void
save_options_image_window_changed( ImageWindow *win, SaveOptions *save_options )
{
	TileSource *tile_source = image_window_get_tile_source( win );
        g_signal_connect_object( tile_source, "changed", 
                G_CALLBACK( save_options_tile_source_changed ), 
                save_options, 0 );
}

/* Set a new image_window
 * 
 * Not currently used.
 */
void
save_options_set_image_window( SaveOptions *save_options,
	ImageWindow *image_window )
{
        save_options->image_window = image_window;
        g_signal_connect_object( image_window, "changed", 
                G_CALLBACK( save_options_image_window_changed ), 
                save_options, 0 );
}
