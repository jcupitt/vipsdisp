#include "vipsdisp.h"
#include <assert.h>

#define DEFAULT_SPACING 10

/* See the SaveOptions class interface file "saveoptions.h" for a high-level
 * description of each SaveOptions class method.
 */

void
save_options_free( SaveOptions *save_options )
{
	GtkWidget *it = gtk_widget_get_first_child(
		GTK_WIDGET( save_options->parent_box ) );
	if ( it )
		gtk_box_remove( save_options->parent_box, it );
	g_free( save_options );
}

SaveOptions *
save_options_new_empty()
{
	SaveOptions *save_options;

	save_options = g_malloc( sizeof( SaveOptions ) );
	save_options->parent_box = NULL;
	save_options->image_window = NULL;
	save_options->content_box = NULL;
	save_options->row_count = 0;

	return save_options;
}

void
save_options_init( SaveOptions *save_options,
	GtkBox *parent_box,
	ImageWindow *image_window )
{
	GtkBox *content_box;
	GtkWidget *scrolled_window;

	save_options->image_window = image_window;

	content_box = GTK_BOX( gtk_box_new( GTK_ORIENTATION_VERTICAL,
		0 ) );

	save_options->content_box = content_box;

	save_options->parent_box = parent_box;

	save_options->row_count = 0;

	g_assert( parent_box );

	scrolled_window = gtk_scrolled_window_new();

	gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( scrolled_window ),
		GTK_POLICY_NEVER,
		GTK_POLICY_ALWAYS );

	gtk_scrolled_window_set_min_content_height(
		GTK_SCROLLED_WINDOW( scrolled_window ),
		400);

	gtk_scrolled_window_set_max_content_height(
		GTK_SCROLLED_WINDOW( scrolled_window ),
		500);

	gtk_scrolled_window_set_child( GTK_SCROLLED_WINDOW( scrolled_window ), GTK_WIDGET( content_box ) );

	gtk_box_append( parent_box, scrolled_window );
}

SaveOptions *
save_options_new( GtkBox *parent_box, ImageWindow *image_window )
{
	SaveOptions *save_options;
		
	save_options = save_options_new_empty();
	save_options_init( save_options, parent_box, image_window );

	return( save_options );
}

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
	VipsArgumentClass *argument_class, guint *row_index,
	SaveOptions *save_options, VipsObject *operation )
{
	if ( *row_index == save_options->row_count )
		return;

	VipsObjectClass *oclass;
	GType otype = G_PARAM_SPEC_VALUE_TYPE( pspec );
	GtkWidget *t, *grid, *grid_cell, *grid_cell_first_child;
	const gchar *property_name;

	/* Not handling VipsImage or VipsObject types yet.
	*/
	if( g_type_is_a( otype, VIPS_TYPE_IMAGE )) {
		return;
	}
	else if( g_type_is_a( otype, VIPS_TYPE_OBJECT ) &&
		(oclass = g_type_class_ref( otype )) ) {
		return;
	}

	property_name = g_param_spec_get_name( pspec );

	grid = gtk_widget_get_first_child( GTK_WIDGET( save_options->content_box ) );

	g_assert( grid );

	/* Use the current value of the row_index to determine the next row. The
	 * rows are attached in increasing index order, from top to bottom. Thus
	 * the order of the widgets matches the order of the properties of the
	 * VipsOperation, and the order of the widgets is always predictable.
	 */
	grid_cell = gtk_grid_get_child_at( GTK_GRID( grid ), 2, *row_index );

	/* Peel off the layers of containing widgets to get to the user input widget
	 * for the property in the iteration. The variable named
	 * "grid_cell_first_child" holds a GtkBox widget. That widget contains
	 * the actual user input widget we are interested in, which is held in
	 * the variable "t", which stands for "temporary".
	 */
	grid_cell_first_child = gtk_widget_get_first_child( grid_cell );
	t = gtk_widget_get_first_child( grid_cell_first_child );

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
		printf( "Unknown type for \"%s\"", property_name );
		return;
	}

	*row_index = *row_index + 1;
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
	guint *row_index = (guint *)a;
	SaveOptions *save_options = (SaveOptions *)b;

	/* Include arguments listed in the constructor.
	 *
	 * Exclude required or deprecated arguments.
	 */
	if ( !(flags & VIPS_ARGUMENT_DEPRECATED) &&
		(flags & VIPS_ARGUMENT_CONSTRUCT) &&
		!(flags & VIPS_ARGUMENT_REQUIRED) )
		save_options_build_save_operation_argument_map_fn_helper( pspec,
			argument_class, row_index, save_options,
			operation );

	return NULL;
}

void
save_options_build_save_operation( SaveOptions *save_options,
	VipsOperation *operation )
{
	gchar *filename;
	guint *row_index;

	g_assert( save_options->row_count );

	row_index = g_malloc( sizeof( guint ) );

	*row_index = 0;

	GtkWidget *content_box, *label_box, *input_box,
		*grid;

	g_object_get( G_OBJECT( operation ),
		"filename", &filename, NULL );

	g_assert( filename );

	/* Get the pointer to the content box widget, held by the SaveOptions
	 * object.
	 */
	content_box = GTK_WIDGET( save_options->content_box );

	g_assert( content_box );

	grid = gtk_widget_get_first_child( GTK_WIDGET( save_options->content_box ) );

	g_assert( grid );

	/* Get the GtkBox widget that contains the GtkLabel widget for the
	 * current property in the iteration.
	 */
	label_box = gtk_grid_get_child_at( GTK_GRID( grid ), 0, 0 );

	g_assert( label_box );

	/* Get the GtkBox widget that contains the user input widget for
	* the current property in the iteration.
	 */
	input_box = gtk_grid_get_child_at( GTK_GRID( grid ), 2, 0 );

	g_assert( input_box );

	g_assert( label_box != input_box );

	/* Loop over the properties of the save operation. Apply the values from
	 * each widget to the save operation.
	 *
	 * See also "save_options_build_save_operation_argument_map_fn".
	 */
	vips_argument_map( VIPS_OBJECT( operation ),
		save_options_build_save_operation_argument_map_fn,
		row_index, save_options );
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
	GtkWidget *t, *label, *box, *label_box, *input_box, *grid;

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

	grid = gtk_widget_get_first_child( GTK_WIDGET( save_options->content_box ) );

	g_assert( grid );

	/* Create the GtkBox widget containing the user input widget appropriate
	 * for the current property in the iteration.
	 */

	input_box = gtk_box_new( GTK_ORIENTATION_VERTICAL,
		0 );

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
		printf("Unknown type for property \"%s\"\n", property_name);
		g_object_ref_sink( input_box );
		return;
	}

	box = gtk_box_new( GTK_ORIENTATION_HORIZONTAL,
		0 );
	gtk_widget_set_tooltip_text( GTK_WIDGET( box ),
		g_param_spec_get_blurb( pspec ) );

	gtk_widget_set_hexpand( t, TRUE );

	g_assert( box );

	gtk_box_append( GTK_BOX( box ), t );

	g_assert( input_box );

	gtk_box_append( GTK_BOX( input_box ), box );

	gtk_grid_attach( GTK_GRID( grid ), input_box, 2, save_options->row_count, 1, 1 );

	/* Create the GtkBox widget containing the GtkLabel widget with the
	 * user-facing name of the current property in the iteration.
	 */

	label_box = gtk_box_new( GTK_ORIENTATION_VERTICAL,
		0 );

	gtk_widget_set_valign( label_box, GTK_ALIGN_CENTER );
		
	label = gtk_label_new( property_name );
	gtk_widget_set_hexpand( label, FALSE );

	gtk_box_append( GTK_BOX( label_box ), label );

	gtk_grid_attach( GTK_GRID( grid ), label_box, 0, save_options->row_count, 1, 1 );

	gtk_widget_set_tooltip_text( GTK_WIDGET( label ),
		g_param_spec_get_blurb( pspec ) );

	save_options->row_count += 1;
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

/* Clean up the old content_box widget, and create a new one.
 */
static gint
save_options_reset_content_box( SaveOptions *save_options )
{
	GtkWidget *content_box, *grid, *scrolled_window;

	content_box = GTK_WIDGET( save_options->content_box );

	if( content_box ) {
		GtkWidget *it = gtk_widget_get_first_child(
			GTK_WIDGET( save_options->parent_box ) );
		if ( it )
			gtk_box_remove( save_options->parent_box, it );
	}

	/* Create a new content box
	 */
	content_box = gtk_box_new( GTK_ORIENTATION_HORIZONTAL,
		0 );

	/* Give the SaveOptions the pointer to the new content box.
	 */
	save_options->content_box = GTK_BOX( content_box );

	scrolled_window = gtk_scrolled_window_new();

	gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( scrolled_window ),
		GTK_POLICY_NEVER,
		GTK_POLICY_ALWAYS );

	gtk_scrolled_window_set_min_content_height(
		GTK_SCROLLED_WINDOW( scrolled_window ),
		400);

	gtk_scrolled_window_set_max_content_height(
		GTK_SCROLLED_WINDOW( scrolled_window ),
		500);

	gtk_scrolled_window_set_child( GTK_SCROLLED_WINDOW( scrolled_window ), GTK_WIDGET( content_box ) );

	gtk_box_append( save_options->parent_box, scrolled_window );

	grid = gtk_grid_new();

	gtk_grid_set_row_spacing( GTK_GRID( grid ), 20 );

	gtk_grid_set_column_spacing( GTK_GRID( grid ), 20 );

	gtk_widget_set_margin_top( grid, 10 );
	gtk_widget_set_margin_end( grid, 10 );
	gtk_widget_set_margin_bottom( grid, 10 );
	gtk_widget_set_margin_start( grid, 10 );

	gtk_box_append( GTK_BOX( content_box ), grid );

	save_options->row_count = 0;

	/* Return success code
	 */
	return 0;
}

gint
save_options_show( SaveOptions *save_options )
{
	GFile *target_file;
	gchar *path, *filename_suffix, *operation_name;
	VipsOperation *operation;

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

	/* Return EXIT_SUCCESS code.
	 */
	return 0;
}

gint
save_options_hide( SaveOptions *save_options )
{
	if( !save_options )
		return -1;

	save_options_free( save_options );

	return 0;
}