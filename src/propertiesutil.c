#include "vipsdisp.h"

/*
#define EXPERIMENTAL_PROPERTIES_EDIT
*/

/* This file defines utility functions for "properties.c".
 *
 * Functions in this file do not use methods or types defined in "properties.c".
 */

/* Set a tooltip on a widget, if we can.
 */
static void
properties_util_set_tooltip( GtkWidget *widget, 
	VipsImage *image, const gchar *field )
{
    GObjectClass *class = G_OBJECT_GET_CLASS( image );
	GParamSpec *pspec = g_object_class_find_property( class, field );
	GType gtype = vips_image_get_typeof( image, field );

	if( pspec ) 
		gtk_widget_set_tooltip_text( widget, 
			g_param_spec_get_blurb( pspec ) );
	else if( gtype ) {
		char *tooltip;

		tooltip = g_strdup_printf( "Property \"%s\" of type %s", 
			field, g_type_name( gtype ) );
		gtk_widget_set_tooltip_text( widget, tooltip );
		g_free( tooltip );
	}
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
	if( !scroll )
		block_scroll( sb );

	return( sb );
}

/* Create an empty label and configure it.
 */
static GtkWidget *
create_empty_label() 
{
	GtkWidget *t;

	t = gtk_label_new( "" );
	gtk_label_set_selectable( GTK_LABEL( t ), TRUE );

	return( t );
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
	/* See gtk/properties.css
	 */
	gtk_widget_add_css_class( t, "properties-label" );
	gtk_widget_set_halign( t, GTK_ALIGN_START );

	return( t );
}

/* Create an empty label box. A label box is a GtkBox with no 
 * children, configured a certain way.
 */
static GtkWidget *
create_empty_label_box()
{
	GtkWidget *t;

	t = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_widget_set_halign( t, GTK_ALIGN_FILL );
	gtk_widget_set_hexpand( t, TRUE );
	gtk_widget_add_css_class( t, "properties-label-box" );

	return( t );
}

/* Create a simple label box. A simple label box is a GtkBox with 1 GtkLabel
 * child, configured a certain way.
 */
GtkWidget *
properties_util_create_simple_label_box( VipsImage *image, const gchar *s )
{
	GtkWidget *t;

	t = create_empty_label_box();
	gtk_box_append( GTK_BOX( t ), create_simple_label( s ) );
	properties_util_set_tooltip( t, image, s );

	return( t );
}

void
properties_util_free_label_box( GtkWidget *label_box )
{
	GtkWidget *t;

	while( (t = gtk_widget_get_first_child( label_box )) )
		gtk_box_remove( GTK_BOX( label_box ), t );

	gtk_widget_unparent( label_box );
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
 * The label-box node has CSS class "properties-label-box".
 *
 * The labels have CSS class "properties-label".
 *
 * These classes are defined in "gtk/properties.css".
 *
 * @ma_list	A GList of Match objects. The list has all exact or all inexact
 * 		matches - not both. Thus, it is sufficient to check the type
 * 		of first Match.
 */
GtkWidget *
properties_util_create_label_box( VipsImage *image, GList *ma_list )
{
	Match *ma;
	GtkWidget *box, *t;
	int i;
	gchar *s;

	ma = (Match *) ma_list->data;

	/* If this is an inexact match, or if the search pattern is NULL or an
	 * empty string, return a simple label box containing just the field
	 * name.
	 */
	if( !ma->exact || !ma->patt || !*ma->patt ) {
		box = properties_util_create_simple_label_box( image, ma->text );
		return( box );
	}

	/* Otherwise, it's an a exact match. Create the empty label box that
	 * will hold the labels.
	 */
	box = create_empty_label_box();

	properties_util_set_tooltip( box, image, ma->text );

	/* Put each matching substring in a GtkLabel with a certain class. Use
	 * those classes to style the matching substrings.
	 */
	i = 0;
	while( ma_list != NULL ) {
		ma = (Match *) ma_list->data;

		/* A label containing the substring before a match.
		 */
		s = g_utf8_substring( ma->text, i, ma->i );
		gtk_box_append( GTK_BOX( box ), create_simple_label( s ) );
		g_free( s );

		/* A label containing the match.
		 */
		s = g_utf8_substring( ma->text, ma->i, ma->i + ma->n_patt );
		t = create_simple_label( s );
		g_free( s );
		gtk_widget_add_css_class( t, "matching-substring" );
		gtk_box_append( GTK_BOX( box ), t );

		i = ma->i + ma->n_patt;

		ma_list = ma_list->next;
	}

	/* A label containing the substring after the last match.
	 */
	s = g_utf8_substring( ma->text, ma->i + ma->n_patt, ma->n_text );
	gtk_box_append( GTK_BOX( box ), create_simple_label( s ) );
	g_free( s );

	return( box );
}

/* Create an input widget appropriate for a string value.
 *
 * @image	The VipsImage
 * @field	The name of the VipsImage metadata item
 */
GtkWidget *
create_string_input( VipsImage *image, const gchar *field )
{
	char *value;
	GtkWidget *t;

	vips_image_get_as_string( image, field, &value );

#ifdef EXPERIMENTAL_PROPERTIES_EDIT
	GtkEntryBuffer* buffer = gtk_entry_buffer_new( value, -1 );
	t = gtk_text_new();
	gtk_text_set_buffer( GTK_TEXT( t ), buffer );
#else
	t = gtk_label_new( value );
	gtk_label_set_selectable( GTK_LABEL( t ), TRUE );
#endif /* EXPERIMENTAL_PROPERTIES_EDIT */

	g_free( value );

	return( t );
}

/* Create an input widget appropriate for a boolean value.
 *
 * @image	The VipsImage
 * @field	The name of the VipsImage property
 */
GtkWidget *
create_boolean_input( VipsImage *image, const gchar *field )
{
	GtkWidget *t;
	int d;

	vips_image_get_int( image, field, &d );

#ifdef EXPERIMENTAL_PROPERTIES_EDIT
	t = gtk_check_button_new();
	gtk_check_button_set_active( GTK_CHECK_BUTTON( t ), d );
#else
	t = gtk_label_new( d ? "true" : "false" );
	gtk_label_set_selectable( GTK_LABEL( t ), TRUE );
#endif /* EXPERIMENTAL_PROPERTIES_EDIT */

	return( t );
}

/* Create an input widget appropriate for an enum value.
 *
 * @image	The VipsImage
 * @field	The name of the VipsImage metadata item
 */
GtkWidget *
create_enum_input( VipsImage *image, const gchar *field )
{
	GType type = vips_image_get_typeof( image, field );

	GtkWidget *t;
	int d;

	vips_image_get_int( image, field, &d );

#ifdef EXPERIMENTAL_PROPERTIES_EDIT
	GEnumClass *enum_class = G_ENUM_CLASS( g_type_class_ref( type ) );
	int n_values = enum_class->n_values;

	const gchar **nicks;

	nicks = VIPS_ARRAY( NULL, n_values + 1, gchar * );
	for( int i = 0; i < n_values; i++ )
		nicks[i] = enum_class->values[i].value_nick;

	t = gtk_drop_down_new_from_strings( nicks );
	gtk_drop_down_set_selected( GTK_DROP_DOWN( t ), d );

	g_free( nicks );
#else
	t = gtk_label_new( vips_enum_nick( type, d ) );
	gtk_label_set_selectable( GTK_LABEL( t ), TRUE );
#endif /* EXPERIMENTAL_PROPERTIES_EDIT */

	return( t );
}

/* Create an input widget appropriate for a flags value.
 *
 * @image	The VipsImage
 * @field	The name of the VipsImage property
 */
GtkWidget *
create_flags_input( VipsImage *image, const gchar *field )
{
	GType type = vips_image_get_typeof( image, field );
	GTypeClass *class = g_type_class_ref( type );
	GFlagsClass *flags_class = G_FLAGS_CLASS( class );
	int n_values = flags_class->n_values;

	GtkWidget *t;
	int d;

	vips_image_get_int( image, field, &d );

	t = gtk_box_new( GTK_ORIENTATION_VERTICAL, 5 );

	for( int i = 0; i < n_values; i++ ) {
		GtkWidget *check;

		// not useful in a GUI
		if( strcmp( flags_class->values[i].value_nick, "none" ) == 0 ||
			strcmp( flags_class->values[i].value_nick, "all" ) == 0 )
			continue;

		check = gtk_check_button_new();
		gtk_check_button_set_label( GTK_CHECK_BUTTON( check ), 
				flags_class->values[i].value_nick );

		// can't be 0 (would match everything), and all bits
		// should match all bits in the value, or "all" would always match
		// everything
		if( flags_class->values[i].value &&
			(d & flags_class->values[i].value) == flags_class->values[i].value )
			gtk_check_button_set_active( GTK_CHECK_BUTTON( check ), TRUE );

		gtk_box_append( GTK_BOX( t ), check );
	}

	return( t );
}

/* Create an input widget appropriate for an int value.
 *
 * @image	The VipsImage
 * @field	The name of the VipsImage property
 */
GtkWidget *
create_int_input( VipsImage *image, const gchar *field )
{
	GtkWidget *t;
	int d;
	gchar *s;

	vips_image_get_int( image, field, &d );

#ifdef EXPERIMENTAL_PROPERTIES_EDIT
	t = create_spin_button( -G_MAXINT + 1, G_MAXINT, 1, d, FALSE );
#else
	s = g_strdup_printf( "%d", d );
	t = gtk_label_new( s );
	g_free( s );
	gtk_label_set_selectable( GTK_LABEL( t ), TRUE );
#endif /* EXPERIMENTAL_PROPERTIES_EDIT */

	return( t );
}

/* Create an input widget appropriate for a double value.
 *
 * @image	The VipsImage
 * @field	The name of the VipsImage property
 */
GtkWidget *
create_double_input( VipsImage *image, const gchar *field )
{
	GtkWidget *t;
	double d;
	gchar *s;

	vips_image_get_double( image, field, &d );

#ifdef EXPERIMENTAL_PROPERTIES_EDIT
	t = create_spin_button( -G_MAXDOUBLE + 1, G_MAXDOUBLE, 1, d, FALSE );
#else
	s = g_strdup_printf( "%f", d );
	t = gtk_label_new( s );
	g_free( s );
	gtk_label_set_selectable( GTK_LABEL( t ), TRUE );
#endif /* EXPERIMENTAL_PROPERTIES_EDIT */

	return( t );
}

/* Create an input widget appropriate for a boxed value.
 *
 * @image	The VipsImage
 * @field	The name of the VipsImage property
 */
GtkWidget *
create_boxed_input( VipsImage *image, const gchar *field )
{
	GtkWidget *t;

	t = gtk_label_new( "" );
	gtk_label_set_selectable( GTK_LABEL( t ), TRUE );

	return( t );
}

void
properties_util_free_input_box( GtkWidget *input_box )
{
	GtkWidget *t;

	t = gtk_widget_get_first_child( input_box );

	gtk_box_remove( GTK_BOX( input_box ), t );

	gtk_widget_unparent( input_box );
}

/* Use introspection on VipsImage to create a UI input element for @field of
 * @image.
 *
 * @image	The VipsImage
 * @field	The name of the VipsImage property
 */
GtkWidget *
properties_util_create_input_box( VipsImage *image, const gchar* field )
{
	GType type = vips_image_get_typeof( image, field );
	GType fundamental = G_TYPE_FUNDAMENTAL( type );

	GtkWidget *input_box, *t;

	if( strstr( "thumbnail", field ) )
		t = gtk_label_new( "" );
	else if( type == VIPS_TYPE_REF_STRING || type == G_TYPE_STRING )
		t = create_string_input( image, field );
	else if( fundamental == G_TYPE_ENUM )
		t = create_enum_input( image, field );
	else if( fundamental == G_TYPE_FLAGS )
		t = create_flags_input( image, field );
	else switch( type ) {
	case G_TYPE_BOOLEAN:
		t = create_boolean_input( image, field );
		break;
	case G_TYPE_INT:
	case G_TYPE_INT64:
	case G_TYPE_UINT:
	case G_TYPE_UINT64:
	case G_TYPE_LONG:
	case G_TYPE_ULONG:
		t = create_int_input( image, field );
		break;
	case G_TYPE_FLOAT:
	case G_TYPE_DOUBLE:
		t = create_double_input( image, field );
		break;
	case G_TYPE_BOXED:
		t = create_boxed_input( image, field );
		break;
	default:
		t = create_empty_label( image, field );
	} /* end switch( type ) */

	/* Create the box we will return, @input_box.
	 */
	input_box = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );

	properties_util_set_tooltip( input_box, image, field );

	/* Set hexpand and halign on @t and @input_box.
	 */
	gtk_widget_set_halign( input_box, GTK_ALIGN_FILL );
	gtk_widget_set_halign( t, GTK_ALIGN_START );
	gtk_widget_set_hexpand( input_box, TRUE );

#ifdef EXPERIMENTAL_PROPERTIES_EDIT
	gtk_widget_set_hexpand( t, TRUE );
	gtk_widget_set_halign( t, GTK_ALIGN_FILL );
	gtk_widget_set_halign( input_box, GTK_ALIGN_FILL );
#endif /* EXPERIMENTAL_PROPERTIES_EDIT */

	/* Style @input_box using CSS from "gtk/properties.css".
	 */
	gtk_widget_add_css_class( input_box, "properties-input-box" );

	/* Append the input widget @t to @input_box.
	 */
	gtk_box_append( GTK_BOX( input_box ), t );

	return( input_box );
}

/* Apply the string value for @field from the UI to @image.
 *
 * @image	The VipsImage
 * @field	The name of the VipsImage property
 */
void
properties_util_apply_string_input( GtkWidget *t, VipsImage *image, const gchar* field )
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
 */
void
properties_util_apply_boolean_input( GtkWidget *t, VipsImage *image, const gchar* field )
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
 */
void
properties_util_apply_enum_input( GtkWidget *t, VipsImage *image, const gchar* field )
{
	GType type = vips_image_get_typeof( image, field );

	int d;
	GValue v = { 0 };

	d = gtk_drop_down_get_selected( GTK_DROP_DOWN( t ) );
	g_value_init( &v, type );
	g_value_set_enum( &v, d );
	vips_image_set( image, field, &v );
	g_value_unset( &v );
}

/* Apply the flags value for @field from the UI to @image.
 *
 * @image	The VipsImage
 * @field	The name of the VipsImage property
 */
void
properties_util_apply_flags_input( GtkWidget *t, VipsImage *image, const gchar* field )
{
	GType type = vips_image_get_typeof( image, field );
	GFlagsClass *flags = G_FLAGS_CLASS( g_type_class_ref( type ) );

	guint value;
	GtkWidget *child;

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
 */
void
properties_util_apply_int_input( GtkWidget *t, VipsImage *image, const gchar* field )
{
	int d;

	d = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( t ) );
	vips_image_set_int( image, field, d );
}

/* Apply the double value for @field from the UI to @image.
 *
 * @image	The VipsImage
 * @field	The name of the VipsImage property
 */
void
properties_util_apply_double_input( GtkWidget *t, VipsImage *image, const gchar* field )
{
	double d;

	d = gtk_spin_button_get_value( GTK_SPIN_BUTTON( t ) );
	vips_image_set_double( image, field, d );
}

/* Apply the boxed value for @field from the UI to @image.
 *
 * @image	The VipsImage
 * @field	The name of the VipsImage property
 */
void
properties_util_apply_boxed_input( GtkWidget *t, 
	VipsImage *image, const gchar* field )
{
#ifdef DEBUG
	printf("G_TYPE_BOXED for property \"%s\" in properties_util_apply_input\n");
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
properties_util_apply_input( GtkWidget *t, 
	VipsImage *image, const gchar* field )
{
	GType type = vips_image_get_typeof( image, field );
	GType fundamental = G_TYPE_FUNDAMENTAL( type );

	if( strstr( "thumbnail", field ) ) {
		/* do nothing */
	} 
	else if( type == VIPS_TYPE_REF_STRING || type == G_TYPE_STRING )
		properties_util_apply_string_input( t, image, field );
	else if( fundamental == G_TYPE_ENUM )
		properties_util_apply_enum_input( t, image, field );
	else if( fundamental == G_TYPE_FLAGS )
		properties_util_apply_flags_input( t, image, field );
	else switch( type ) {
	case G_TYPE_BOOLEAN:
		properties_util_apply_boolean_input( t, image, field );
		break;
	case G_TYPE_INT:
	case G_TYPE_INT64:
	case G_TYPE_UINT:
	case G_TYPE_UINT64:
	case G_TYPE_LONG:
	case G_TYPE_ULONG:
		properties_util_apply_int_input( t, image, field );
		break;
	case G_TYPE_FLOAT:
	case G_TYPE_DOUBLE:
		properties_util_apply_double_input( t, image, field );
		break;
	case G_TYPE_BOXED:
		properties_util_apply_boxed_input( t, image, field );
		break;
	default:
#ifdef DEBUG
		printf("Type of property \"%s\" unknown.", field);
#endif /* DEBUG */
		/* do nothing */
	} /* end switch( type ) */
}
