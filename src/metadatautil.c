#include "vipsdisp.h"

/*
#define EXPERIMENTAL_METADATA_EDIT
*/

/* This file defines utility functions for "metadata.c".
 *
 * Functions in this file do not use methods or types defined in "metadata.c".
 */

/* No-op to prevent @w from propagating "scroll" events it receives.
 *
 * @w_	Boilerplate GTK argument.
 */
void disable_scroll_cb( GtkWidget *w_ ) {
	/* Do nothing. */
}

/* Disable scroll on a widget by adding a capture phase event handler and
 * connecting a no-op callback to the "scroll" event.
 *
 * @w	The target widget.
 */
static GtkWidget *
disable_scroll( GtkWidget *w )
{
	GtkEventController *ec;

	ec = gtk_event_controller_scroll_new( GTK_EVENT_CONTROLLER_SCROLL_VERTICAL );
	gtk_event_controller_set_propagation_phase( ec, GTK_PHASE_CAPTURE );
	g_signal_connect( ec, "scroll", G_CALLBACK( disable_scroll_cb ), w );
	gtk_widget_add_controller( w, ec );

	return w;
}

/* Create a spin button with range, default value, and optionally enabled
 * scrolling.
 *
 * @min		The minimum value the spin button can hold.
 * @max		The maximum value the spin button can hold.
 * @step	The size of the step each time a scroll event is received.
 * @value	The initial value held by the spin button.
 * @scroll 	TRUE if the spin button is scrollable. FALSE otherwise.
 */
GtkWidget *
create_spin_button( double min, double max, double step,
		double value, bool scroll )
{
	GtkWidget *sb;

	sb = gtk_spin_button_new_with_range( min, max, step );
	gtk_spin_button_set_value( GTK_SPIN_BUTTON( sb ), value );

	return scroll ? sb : disable_scroll( sb );
}

/* Create an empty label and configure it.
 */
static GtkWidget *
create_empty_label() {
	GtkWidget *t;

#ifdef DEBUG
	printf("Unknown type for property \"%s\" in metadata_util_create_input_box\n", field);
#endif /* DEBUG */

	t = gtk_label_new( "" );
	gtk_label_set_selectable( GTK_LABEL( t ), TRUE );

	return t;
}

/* Create and configure a simple label from a string @s.
 *
 * @s	Contents of the label
 */
static GtkWidget *
create_simple_label( const gchar *s )
{
	GtkWidget *t;

	t = create_empty_label();
	gtk_label_set_label( GTK_LABEL( t ), s );
	/* See gtk/metadata.css
	 */
	gtk_widget_add_css_class( t, "metadata-label" );
	gtk_widget_set_halign( t, GTK_ALIGN_START );

	return t;
}

/* Create an empty label box. A label box is a GtkBox with no children
 * children, configured a certain way.
 */
static GtkWidget *
create_empty_label_box()
{
	GtkWidget *t;

	t = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_widget_set_halign( t, GTK_ALIGN_FILL );
	gtk_widget_set_hexpand( t, TRUE );
	gtk_widget_add_css_class( t, "metadata-label-box" );

	return t;
}

/* Create a simple label box. A simple label box is a GtkBox with 1 GtkLabel
 * child, configured a certain way.
 */
GtkWidget *
metadata_util_create_simple_label_box( const gchar *s )
{
	GtkWidget *t;

	t = create_empty_label_box();
	gtk_box_append( GTK_BOX( t ), create_simple_label( s ) );

	return t;
}

/* Create a new label box for the given GList of Match objects. The list has all
 * exact or all inexact matches - not both. An exact match has a label box with
 * multiple labels, where each label contains the matching substring or the
 * substrings between matching substrings. An inexact match has a label box with
 * only one label containing the field.
 *
 * The matching substrings of an exact match label box are styled by applying
 * the CSS class "matching-substring" to the labels containing the matching
 * substrings.
 *
 * The label-box node has CSS class "metadata-label-box".
 *
 * The labels have CSS class "metadata-label".
 *
 * These classes are defined in "gtk/metadata.css".
 *
 * @ma_list	A GList of Match objects.
 */
GtkWidget *
metadata_util_create_label_box( GList *ma_list )
{
	Match *ma;
	GtkWidget *box, *t;
	int i;
	gchar *s;

	ma = (Match *) ma_list->data;

	/* If this is an inexact match, return a simple label box containing
	 * just the field name.
	 */
	if ( !ma->exact )
		return metadata_util_create_simple_label_box( ma->text );

	/* Otherwise, it's an a exact match. Create the empty label box that
	 * will hold the labels.
	 */
	box = create_empty_label_box();

	/* Put each matching substring in a GtkLabel with a certain class. Use
	 * those classes to style the matching substrings. 
	 */
	i = 0;
	while ( ma_list != NULL ) {
		ma = (Match *) ma_list->data;

		/* A label containing the substring before a match.
		 */
		s = g_utf8_substring( ma->text, i, ma->i );
		gtk_box_append( GTK_BOX( box ), create_simple_label( s ) );
		
		/* A label containing the match.
		 */
		s = g_utf8_substring( ma->text, ma->i, ma->i + ma->n_patt );
		t = create_simple_label( s );
		gtk_widget_add_css_class( t, "matching-substring" );
		gtk_box_append( GTK_BOX( box ), t );
		
		i = ma->i + ma->n_patt;

		ma_list = ma_list->next;
	}

	/* A label containing the substring after the last match.
	 */
	s = g_utf8_substring( ma->text, ma->i + ma->n_patt, ma->n_text );
	gtk_box_append( GTK_BOX( box ), create_simple_label( s ) );

	return box;
}

/* Create an input widget appropriate for a string value.
 *
 * @image	The VipsImage
 * @field	The name of the VipsImage property
 * @pspec	The GParamSpec for @field
 */
GtkWidget *
create_string_input( VipsImage *image, const gchar *field, GParamSpec *pspec ) {
	GtkWidget *t;
	const gchar *value;

	if ( !strcmp( field, "filename" ) )
		g_object_get( image, field, &value, NULL );
	else
		vips_image_get_string( image, field, &value );

#ifdef EXPERIMENTAL_METADATA_EDIT
	value = g_strdup( value );
	GtkEntryBuffer* buffer =
		gtk_entry_buffer_new( value, -1 );

	t = gtk_text_new();
	gtk_text_set_buffer( GTK_TEXT( t ), buffer );

#else
	t = gtk_label_new( g_strdup_printf( "%s", value ) );
	gtk_label_set_selectable( GTK_LABEL( t ), TRUE );
#endif /* EXPERIMENTAL_METADATA_EDIT */

	return t;
}

/* Create an input widget appropriate for a boolean value.
 *
 * @image	The VipsImage
 * @field	The name of the VipsImage property
 * @pspec	The GParamSpec for @field
 */
GtkWidget *
create_boolean_input( VipsImage *image, const gchar *field, GParamSpec *pspec ) {
	GtkWidget *t;
	int d;

	vips_image_get_int( image, field, &d );

#ifdef EXPERIMENTAL_METADATA_EDIT
	t = gtk_check_button_new();
	gtk_check_button_set_active( GTK_CHECK_BUTTON( t ), d );
#else
	t = gtk_label_new( g_strdup_printf( "%s",
				d ? "true" : "false" ) );
	gtk_label_set_selectable( GTK_LABEL( t ), TRUE );
#endif /* EXPERIMENTAL_METADATA_EDIT */

	return t;
}

/* Create an input widget appropriate for an enum value.
 *
 * @image	The VipsImage
 * @field	The name of the VipsImage property
 * @pspec	The GParamSpec for @field
 */
GtkWidget *
create_enum_input( VipsImage *image, const gchar *field, GParamSpec *pspec )
{
	GtkWidget *t;

	if ( pspec && G_IS_PARAM_SPEC_ENUM( pspec ) ) {
		GParamSpecEnum *pspec_enum = G_PARAM_SPEC_ENUM( pspec );
		int d;
		const gchar **nicks;
		
		nicks = g_malloc( (pspec_enum->enum_class->n_values + 1) * sizeof( gchar * ) );

		for( int i = 0; i < pspec_enum->enum_class->n_values; ++i )
			nicks[i] = pspec_enum->enum_class->values[i].value_nick;
		nicks[pspec_enum->enum_class->n_values] = NULL;

		g_object_get( image, field, &d, NULL );

#ifdef EXPERIMENTAL_METADATA_EDIT
		t = gtk_drop_down_new_from_strings( nicks );
		gtk_drop_down_set_selected( GTK_DROP_DOWN( t ), d );
#else
	        t = gtk_label_new( vips_enum_nick( pspec->value_type, d ) );

		gtk_label_set_selectable( GTK_LABEL( t ), TRUE );
#endif /* EXPERIMENTAL_METADATA_EDIT */

	} else { 
		int d;

		vips_image_get_int( image, field, &d );
#ifdef EXPERIMENTAL_METADATA_EDIT
		t = create_spin_button( -G_MAXINT + 1, G_MAXINT, 1, d, FALSE );
#else
		t = gtk_label_new( g_strdup_printf( "%d", d ) );
		gtk_label_set_selectable( GTK_LABEL( t ), TRUE );
#endif /* EXPERIMENTAL_METADATA_EDIT */
	}

	return t;
}

/* Create an input widget appropriate for a flags value.
 *
 * @image	The VipsImage
 * @field	The name of the VipsImage property
 * @pspec	The GParamSpec for @field
 */
GtkWidget *
create_flags_input( VipsImage *image, const gchar *field, GParamSpec *pspec )
{
	GParamSpecFlags *pspec_flags = G_PARAM_SPEC_FLAGS( pspec );
	GFlagsClass *flags = G_FLAGS_CLASS( pspec_flags->flags_class );
	guint value = pspec_flags->default_value;
	GtkWidget *t;

	t = gtk_box_new( GTK_ORIENTATION_VERTICAL, 5 );

	for( int i = 0; i < flags->n_values; i++ ) {
		GtkWidget *check;

		// not useful in a GUI
		if( strcmp( flags->values[i].value_nick, "none" ) == 0 ||
			strcmp( flags->values[i].value_nick, "all" ) == 0 )
			continue;

		check = gtk_check_button_new();
		gtk_check_button_set_label( GTK_CHECK_BUTTON( check ), 
				flags->values[i].value_nick );

		// can't be 0 (would match everything), and all bits
		// should match all bits in the value, or "all" would always match
		// everything
		if (flags->values[i].value &&
			(value & flags->values[i].value) == flags->values[i].value) 
			gtk_check_button_set_active( GTK_CHECK_BUTTON( check ), TRUE );

		gtk_box_append( GTK_BOX( t ), check );
	}

	return t;
}

/* Create an input widget appropriate for an int value.
 *
 * @image	The VipsImage
 * @field	The name of the VipsImage property
 * @pspec	The GParamSpec for @field
 */
GtkWidget *
create_int_input( VipsImage *image, const gchar *field, GParamSpec *pspec ) {
	GtkWidget *t;
	int d;

	vips_image_get_int( image, field, &d );

#ifdef EXPERIMENTAL_METADATA_EDIT
	t = create_spin_button( -G_MAXINT + 1, G_MAXINT, 1, d, FALSE );
#else
	t = gtk_label_new( g_strdup_printf( "%d", d ) );
	gtk_label_set_selectable( GTK_LABEL( t ), TRUE );
#endif /* EXPERIMENTAL_METADATA_EDIT */

	return t;
}

/* Create an input widget appropriate for a double value.
 *
 * @image	The VipsImage
 * @field	The name of the VipsImage property
 * @pspec	The GParamSpec for @field
 */
GtkWidget *
create_double_input( VipsImage *image, const gchar *field, GParamSpec *pspec )
{
	GtkWidget *t;
	double d;

	vips_image_get_double( image, field, &d );

#ifdef EXPERIMENTAL_METADATA_EDIT
	t = create_spin_button( -G_MAXDOUBLE + 1, G_MAXDOUBLE, 1, d, FALSE );

#else
	t = gtk_label_new( g_strdup_printf( "%f", d ) );
	gtk_label_set_selectable( GTK_LABEL( t ), TRUE );
#endif /* EXPERIMENTAL_METADATA_EDIT */

	return t;
}

/* Create an input widget appropriate for a boxed value.
 *
 * @image	The VipsImage
 * @field	The name of the VipsImage property
 * @pspec	The GParamSpec for @field
 */
GtkWidget *
create_boxed_input( VipsImage *image, const gchar *field, GParamSpec *pspec )
{
	GtkWidget *t;

#ifdef DEBUG
	printf( "G_TYPE_BOXED for property \"%s\" in metadata_util_create_input_box\n", field );
#endif /* DEBUG */

	t = gtk_label_new( "" );
	gtk_label_set_selectable( GTK_LABEL( t ), TRUE );

	return t;
}

/* Use introspection on VipsImage to create a UI input element for @field of
 * @image.
 *
 * @image	The VipsImage
 * @field	The name of the VipsImage property
 */
GtkWidget *
metadata_util_create_input_box( VipsImage *image, const gchar* field )
{
	GtkWidget *input_box, *t;
	GType type;
	GParamSpec *pspec;
	VipsArgumentClass *arg_class;
	VipsArgumentInstance *arg_instance;

	/* Zero the GValue whose address we pass to vips_image_get. Otherwise, we
	 * will get runtime errors.
	 */
	GValue value = { 0 };

	/* Use introspection to get information about this field on the image.
	 * We want the paramspec ( if there is one ) in case this field is an
	 * enum, and also to get the blurb.
	 */
	vips_image_get( image, field, &value );
	if ( vips_object_get_argument( VIPS_OBJECT( image ), field,
			&pspec, &arg_class, &arg_instance ) )
		pspec = NULL;

	/* Create the actual user input widget @t for this property. The widget
	 * chosen depends on the type of the property. The value is the current
	 * value on the image.
	 */
	type = G_VALUE_TYPE( &value );
	if ( strstr( "thumbnail", field ) )
		t = gtk_label_new( "" );
	else if ( type == VIPS_TYPE_REF_STRING )
		t = create_string_input( image, field, pspec );
	else if ( pspec && G_IS_PARAM_SPEC_ENUM( pspec ) )
		t = create_enum_input( image, field, pspec );
	else switch( type ) {
	case G_TYPE_STRING:
		t = create_string_input( image, field, pspec );
		break;
	case G_TYPE_BOOLEAN:
		t = create_boolean_input( image, field, pspec );
		break;
	case G_TYPE_ENUM:
		t = create_enum_input( image, field, pspec );
		break;
	case G_TYPE_FLAGS:
		t = create_flags_input( image, field, pspec );
		break;
	case G_TYPE_INT:
	case G_TYPE_INT64:
	case G_TYPE_UINT:
	case G_TYPE_UINT64:
	case G_TYPE_LONG:
	case G_TYPE_ULONG:
		t = create_int_input( image, field, pspec );
		break;
	case G_TYPE_FLOAT:
	case G_TYPE_DOUBLE:
		t = create_double_input( image, field, pspec );
		break;
	case G_TYPE_BOXED:
		t = create_boxed_input( image, field, pspec );
		break;
	default:
		t = create_empty_label( image, field, pspec );
	} /* end switch( type ) */

	/* Create the box we will return, @input_box.
	 */
	input_box = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );
	//gtk_widget_set_margin_start( input_box, 20 );

	/* If there was a GParamSpec for this property, use the blurb as a
	 * tooltip.
	 */
	if ( pspec )
		gtk_widget_set_tooltip_text( GTK_WIDGET( input_box ),
				g_param_spec_get_blurb( pspec ) );

	/* Set hexpand and halign on @t and @input_box.
	 */
	gtk_widget_set_halign( input_box, GTK_ALIGN_FILL );
	gtk_widget_set_halign( t, GTK_ALIGN_START );
	gtk_widget_set_hexpand( input_box, TRUE );

#ifdef EXPERIMENTAL_METADATA_EDIT
	gtk_widget_set_hexpand( t, TRUE );
	gtk_widget_set_halign( t, GTK_ALIGN_FILL );
	gtk_widget_set_halign( input_box, GTK_ALIGN_FILL );
#endif /* EXPERIMENTAL_METADATA_EDIT */

	/* Style @input_box using CSS from "gtk/metadata.css".
	 */
	gtk_widget_add_css_class( input_box, "metadata-input-box" );

	/* Append the input widget @t to @input_box.
	 */
	gtk_box_append( GTK_BOX( input_box ), t );

	return input_box;
}

/* Apply the string value for @field from the UI to @image.
 *
 * @image	The VipsImage
 * @field	The name of the VipsImage property
 * @pspec	The GParamSpec for @field
 */
void
metadata_util_apply_string_input( GtkWidget *t, VipsImage *image, const gchar* field, GParamSpec *pspec )
{
	GtkEntryBuffer* buffer;
	const char *text;

	buffer = gtk_text_get_buffer( GTK_TEXT( t ) );
	text = gtk_entry_buffer_get_text( buffer );
	vips_image_set_string( image, field, text );

}

/* Apply the boolean value for @field from the UI to @image.
 *
 * @image	The VipsImage
 * @field	The name of the VipsImage property
 * @pspec	The GParamSpec for @field
 */
void
metadata_util_apply_boolean_input( GtkWidget *t, VipsImage *image, const gchar* field, GParamSpec *pspec )
{
	gboolean b;
	GValue v = { 0 };

	b = gtk_check_button_get_active( GTK_CHECK_BUTTON( t ) );
	g_value_init( &v, G_TYPE_BOOLEAN );
	g_value_set_boolean( &v, b );
	vips_image_set( image, field, &v );
	g_value_unset( &v );

}

/* Apply the enum value for @field from the UI to @image.
 *
 * @image	The VipsImage
 * @field	The name of the VipsImage property
 * @pspec	The GParamSpec for @field
 */
void
metadata_util_apply_enum_input( GtkWidget *t, VipsImage *image, const gchar* field, GParamSpec *pspec )
{
	int d;
	GValue v = { 0 };

	if ( pspec ) {
		d = gtk_drop_down_get_selected( GTK_DROP_DOWN( t ) );
		g_value_init( &v, G_TYPE_ENUM );
		g_value_set_enum( &v, d );
		vips_image_set( image, field, &v );
		g_value_unset( &v );
	} else { 
		d = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( t ) );
		vips_image_set_int( image, field, d );
	}
}

/* Apply the flags value for @field from the UI to @image.
 *
 * @image	The VipsImage
 * @field	The name of the VipsImage property
 * @pspec	The GParamSpec for @field
 */
void
metadata_util_apply_flags_input( GtkWidget *t, VipsImage *image, const gchar* field, GParamSpec *pspec )
{
	GParamSpecFlags *pspec_flags;
	GFlagsClass *flags;
	guint value;
	GtkWidget *child;

	pspec_flags = G_PARAM_SPEC_FLAGS( pspec );
	flags = G_FLAGS_CLASS( pspec_flags->flags_class );

	value = 0;
	child = gtk_widget_get_first_child( t ); 
	for( int i = 0; i < flags->n_values; i++ ) {
		// we skip these. not useful in UI.
		if( strcmp( flags->values[i].value_nick, "none" ) == 0 ||
			strcmp( flags->values[i].value_nick, "all" ) == 0 )
			continue;

		if( child ) {
			if( gtk_check_button_get_active( GTK_CHECK_BUTTON( child ) ) ) 
				value |= flags->values[i].value;

			child = gtk_widget_get_next_sibling( child );
		}
	}

	vips_image_set_int( image, field, value );
}
	
/* Apply the int value for @field from the UI to @image.
 *
 * @image	The VipsImage
 * @field	The name of the VipsImage property
 * @pspec	The GParamSpec for @field
 */
void
metadata_util_apply_int_input( GtkWidget *t, VipsImage *image, const gchar* field, GParamSpec *pspec )
{
	int d;

	d = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( t ) );
	vips_image_set_int( image, field, d );
}

/* Apply the double value for @field from the UI to @image.
 *
 * @image	The VipsImage
 * @field	The name of the VipsImage property
 * @pspec	The GParamSpec for @field
 */
void
metadata_util_apply_double_input( GtkWidget *t, VipsImage *image, const gchar* field, GParamSpec *pspec )
{
	double d;

	d = gtk_spin_button_get_value( GTK_SPIN_BUTTON( t ) );
	vips_image_set_double( image, field, d );
}

/* Apply the boxed value for @field from the UI to @image.
 *
 * @image	The VipsImage
 * @field	The name of the VipsImage property
 * @pspec	The GParamSpec for @field
 */
void
metadata_util_apply_boxed_input( GtkWidget *t, VipsImage *image, const gchar* field, GParamSpec *pspec )
{
#ifdef DEBUG
	printf("G_TYPE_BOXED for property \"%s\" in metadata_util_apply_input\n");
#endif /* DEBUG */

	/* do nothing */

	return;
}

/* Apply the value for @field from the input widget @t to @image. 
 *
 * @t		The GtkWidget
 * @image	The VipsImage
 * @field	The name of the VipsImage property
 */
void
metadata_util_apply_input( GtkWidget *t, VipsImage *image, const gchar* field )
{
	GValue value = { 0 };
	GParamSpec *pspec;
	GType type;
	VipsArgumentClass *arg_class;
	VipsArgumentInstance *arg_instance;

	vips_image_get( image, field, &value );
	if( vips_object_get_argument( VIPS_OBJECT( image ), field,
		&pspec, &arg_class, &arg_instance ) )
		pspec = NULL;

	type = G_VALUE_TYPE( &value );

	if ( strstr( "thumbnail", field ) ) {
		/* do nothing */
	} else if ( type == VIPS_TYPE_REF_STRING )
		metadata_util_apply_string_input( t, image, field, pspec );
	else if ( pspec && G_IS_PARAM_SPEC_ENUM( pspec ) )
		metadata_util_apply_enum_input( t, image, field, pspec );
	else switch( type ) {
	case G_TYPE_STRING:
		metadata_util_apply_string_input( t, image, field, pspec );
		break;
	case G_TYPE_BOOLEAN:
		metadata_util_apply_boolean_input( t, image, field, pspec );
		break;
	case G_TYPE_ENUM:
		metadata_util_apply_enum_input( t, image, field, pspec );
		break;
	case G_TYPE_FLAGS:
		metadata_util_apply_flags_input( t, image, field, pspec );
		break;
	case G_TYPE_INT:
	case G_TYPE_INT64:
	case G_TYPE_UINT:
	case G_TYPE_UINT64:
	case G_TYPE_LONG:
	case G_TYPE_ULONG:
		metadata_util_apply_int_input( t, image, field, pspec );
		break;
	case G_TYPE_FLOAT:
	case G_TYPE_DOUBLE:
		metadata_util_apply_double_input( t, image, field, pspec );
		break;
	case G_TYPE_BOXED:
		metadata_util_apply_boxed_input( t, image, field, pspec );
		break;
	default:
#ifdef DEBUG
		printf("Type of property \"%s\" unknown.", field);
#endif /* DEBUG */
		/* do nothing */
	} /* end switch( type ) */
}
