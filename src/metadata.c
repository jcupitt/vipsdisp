/*
#define DEBUG
 */

#include "vipsdisp.h"

struct _Metadata
{
	GtkDialog parent_instance;

	VipsImage *image;
	VipsOperation *save_operation;

	// a box we can fill with widgets for the save options
	GtkWidget *options_grid;

	// the error indicator we show
	GtkWidget *error_bar;
	GtkWidget *error_label;

	GtkWidget *metadata_label;
	GtkWidget *metadata_close_button;
	GtkWidget *metadata_apply_button;
	GtkWidget *metadata_window;
	GtkWidget *metadata_search_entry;
	
	GtkGrid *metadata_grid;
	GList *field_list;
	int field_list_length;

	// hash property names to the widghet for that property ... we fetch 
	// values from here when we make the saver
	GHashTable *value_widgets;
};

struct _MetadataClass
{
	GtkDialogClass parent_class;
};

G_DEFINE_TYPE( Metadata, metadata, GTK_TYPE_DIALOG );

static void
metadata_dispose( GObject *object )
{
	Metadata *options = VIPSDISP_SAVE_OPTIONS( object );

	VIPS_UNREF( options->image );
	VIPS_UNREF( options->save_operation );
	VIPS_FREEF( g_hash_table_destroy, options->value_widgets );

	G_OBJECT_CLASS( metadata_parent_class )->dispose( object );
}

static void
metadata_error( Metadata *options )
{
	char *err;
	int i;

	/* Remove any trailing \n.
	 */
	err = vips_error_buffer_copy();
	for( i = strlen( err ); i > 0 && err[i - 1] == '\n'; i-- )
		err[i - 1] = '\0';
	gtk_label_set_text( GTK_LABEL( options->error_label ), err );
	g_free( err );

	gtk_info_bar_set_revealed( GTK_INFO_BAR( options->error_bar ), TRUE );
}

static void
metadata_error_hide( Metadata *options )
{
	gtk_info_bar_set_revealed( GTK_INFO_BAR( options->error_bar ), FALSE );
}

static void
metadata_error_response( GtkWidget *button, int response, 
	Metadata *options )
{
	metadata_error_hide( options );
}

static void
metadata_fetch_option( Metadata *options, GParamSpec *pspec ) 
{
	const gchar *name = g_param_spec_get_name( pspec );
	GType otype = G_PARAM_SPEC_VALUE_TYPE( pspec );

	GtkWidget *t;

	if( !(t = g_hash_table_lookup( options->value_widgets, name )) )
		return;

	/* Fetch the value from the widget.
	 */
	if( G_IS_PARAM_SPEC_STRING( pspec ) ) {
		GParamSpecString *pspec_string = G_PARAM_SPEC_STRING( pspec );
		GtkEntryBuffer *buffer = gtk_entry_get_buffer( GTK_ENTRY( t ) );
		const char *value = gtk_entry_buffer_get_text( buffer );

		/* Only if the value has changed.
		 */
		if( (!pspec_string->default_value &&
			strcmp( value, "" ) != 0) ||
		    (pspec_string->default_value &&
			strcmp( value, pspec_string->default_value ) != 0) ) {

			g_object_set( options->save_operation,
				name, value,
				NULL );
		}
	}
	else if( G_IS_PARAM_SPEC_BOOLEAN( pspec ) ) {
		gboolean value = 
			gtk_check_button_get_active( GTK_CHECK_BUTTON( t ) );

		g_object_set( options->save_operation, 
			name, value,
			NULL );
	}
	else if( G_IS_PARAM_SPEC_ENUM( pspec ) ) {
		GParamSpecEnum *pspec_enum = G_PARAM_SPEC_ENUM( pspec );
		int index = gtk_drop_down_get_selected( GTK_DROP_DOWN( t ) );
		int value = pspec_enum->enum_class->values[index].value;

		g_object_set( options->save_operation,
			name, value,
			NULL );
	}
	else if( G_IS_PARAM_SPEC_INT64( pspec ) ) {
		gint64 value = 
			gtk_spin_button_get_value( GTK_SPIN_BUTTON( t ) );

		g_object_set( options->save_operation,
			name, value,
			NULL );
	}
	else if( G_IS_PARAM_SPEC_INT( pspec )) {
		int value = gtk_spin_button_get_value( GTK_SPIN_BUTTON( t ) );

		g_object_set( options->save_operation,
			name, value,
			NULL );
	}
	else if( G_IS_PARAM_SPEC_UINT64( pspec ) ) {
		guint64 value = 
			gtk_spin_button_get_value( GTK_SPIN_BUTTON( t ) );

		g_object_set( options->save_operation,
			name, value,
			NULL );
	}
	else if( G_IS_PARAM_SPEC_DOUBLE( pspec ) ) {
		gdouble value = 
			gtk_spin_button_get_value( GTK_SPIN_BUTTON( t ) );

		g_object_set( options->save_operation,
			name, value,
			NULL );
	}
	else if( G_IS_PARAM_SPEC_BOXED( pspec ) ) {
		if( g_type_is_a( otype, VIPS_TYPE_ARRAY_INT ) ) {
			int value;
			VipsArrayInt *array;

			/* For now just pretend every array-type parameter has
			 * one element.
			 * TODO handle arrays with two or more elements
			 */
			value = gtk_spin_button_get_value( GTK_SPIN_BUTTON( t ) );
			array = vips_array_int_newv( 1, value );

			g_object_set( options->save_operation,
				name, array,
				NULL );

			vips_area_unref( VIPS_AREA( array ) );
		}
		else if( g_type_is_a( otype, VIPS_TYPE_ARRAY_DOUBLE ) ) {
			gdouble value;
			VipsArrayDouble *array;
			value = gtk_spin_button_get_value( GTK_SPIN_BUTTON( t ) );

			/* For now just pretend every array-type parameter has
			 * one element.
			 * TODO handle arrays with two or more elements
			 */
			array = vips_array_double_newv( 1, value );
			g_object_set( options->save_operation,
				name, array,
				NULL );

			vips_area_unref( VIPS_AREA( array ) );
		}
	}
}

static void *
metadata_response_map_fn( VipsObject *operation,
	GParamSpec *pspec, VipsArgumentClass *argument_class,
	VipsArgumentInstance *argument_instance, void *a, void *b )
{
	VipsArgumentFlags flags = argument_class->flags;
	Metadata *options = (Metadata *) a;

	/* Include arguments listed in the constructor.
	 *
	 * Exclude required (we've set these already) or deprecated arguments.
	 */
	if ( !(flags & VIPS_ARGUMENT_DEPRECATED) &&
		(flags & VIPS_ARGUMENT_CONSTRUCT) &&
		!(flags & VIPS_ARGUMENT_REQUIRED) )
		metadata_fetch_option( options, pspec );

	return NULL;
}

static void
metadata_response( GtkWidget *dialog, int response, void *user_data )
{
	Metadata *options = VIPSDISP_SAVE_OPTIONS( dialog );

	if( response == GTK_RESPONSE_OK ) {
		vips_argument_map( VIPS_OBJECT( options->save_operation ),
			metadata_response_map_fn,
			options, NULL );

		// this will trigger the save and loop while we write ... the
		// UI will stay live thanks to event processing in the eval
		// handler
		if( vips_cache_operation_buildp( &options->save_operation ) ) 
			metadata_error( options );
		else 
			// everything worked, we can post success back to
			// our caller
			gtk_dialog_response( GTK_DIALOG( dialog ), 
				GTK_RESPONSE_ACCEPT );
	}
}

static void
metadata_cancel_clicked( GtkWidget *button, Metadata *options )
{
	vips_image_set_kill( options->image, TRUE );
}

static void
metadata_init( Metadata *metadata )
{
	gtk_widget_init_template( GTK_WIDGET( metadata ) );

	g_signal_connect_object( metadata->error_bar, "response", 
		G_CALLBACK( metadata_error_response ), metadata, 0 );

	metadata->value_widgets = g_hash_table_new( g_str_hash, g_str_equal );

	g_signal_connect_object( metadata, "response", 
		G_CALLBACK( metadata_response ), metadata, 0 );

	gtk_label_set_markup( GTK_LABEL( win->metadata_label ), "<b>Metadata</b>");

	g_signal_connect( win->metadata_close_button, "clicked",
		G_CALLBACK( metadata_close_button_cb ), win );

	g_signal_connect( win->metadata_apply_button, "clicked",
		G_CALLBACK( on_metadata_apply_button_pressed ), win );
}

#define BIND( field ) \
	gtk_widget_class_bind_template_child( GTK_WIDGET_CLASS( class ), \
		Metadata, field );

static void
metadata_class_init( MetadataClass *class )
{
	GObjectClass *gobject_class = G_OBJECT_CLASS( class );
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS( class );

	gobject_class->dispose = metadata_dispose;

	gtk_widget_class_set_template_from_resource( widget_class,
		APP_PATH "/metadata.ui");

	BIND( error_bar );
	BIND( error_label );
	BIND( options_grid );

	BIND( metadata_label );
	BIND( metadata_close_button );
	BIND( metadata_apply_button );
	BIND( metadata_window );
	BIND( metadata_search_entry );

}

/* This function is used by:
 *
 * 	metadata_build_content_box_argument_map_fn_helper
 *
 * to process one property of the save operation. The property type and name
 * are used to create a labelled user input element for that property.
 */
static void
metadata_add_option( Metadata *options, GParamSpec *pspec, int *row )
{
	const gchar *name = g_param_spec_get_name( pspec );
	GType otype = G_PARAM_SPEC_VALUE_TYPE( pspec );

	GtkWidget *t, *label;

	/* For now, skip properties of type VipsImage or VipsObject.
	 */
	if( g_type_is_a( otype, VIPS_TYPE_IMAGE ) ||
		g_type_is_a( otype, VIPS_TYPE_OBJECT ) )
		return;

	/* Make a widget for this value. The widget
	 * chosen depends on the type of the property. Set the initial value of
	 * the user input widget to the default value for the property.
	 */
	if( G_IS_PARAM_SPEC_STRING( pspec ) ) {
		GParamSpecString *pspec_string = G_PARAM_SPEC_STRING( pspec );
		GtkEntryBuffer *buffer =
			gtk_entry_buffer_new( pspec_string->default_value, -1 );

		t = gtk_entry_new_with_buffer( buffer );
	}
	else if( G_IS_PARAM_SPEC_BOOLEAN( pspec ) ) {
		GParamSpecBoolean *pspec_boolean = 
			G_PARAM_SPEC_BOOLEAN( pspec );

		t = gtk_check_button_new();
		gtk_check_button_set_active( GTK_CHECK_BUTTON( t ),
			pspec_boolean->default_value );
	}
	else if( G_IS_PARAM_SPEC_ENUM( pspec ) ) {
		GParamSpecEnum *pspec_enum = G_PARAM_SPEC_ENUM( pspec );
		int n_values = pspec_enum->enum_class->n_values - 1;
		const char **nicknames = 
			VIPS_ARRAY( NULL, n_values + 1, const char * );

		for( int i = 0; i < n_values; ++i )
			nicknames[i] =
				pspec_enum->enum_class->values[i].value_nick;
		nicknames[n_values] = NULL;

		t = gtk_drop_down_new_from_strings( nicknames );
		gtk_drop_down_set_selected( GTK_DROP_DOWN( t ),
			pspec_enum->default_value );
		
		g_free( nicknames );
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
			(int)pspec_int->default_value );
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
		printf( "Unknown type for property \"%s\"\n", name );
		return;
	}

	/* Label for setting, with a tooltip. The nick is the i18n name.
	 */
	label = gtk_label_new( g_param_spec_get_nick( pspec ) );
	gtk_widget_set_halign( label, GTK_ALIGN_START );
	gtk_widget_set_tooltip_text( GTK_WIDGET( label ),
		g_param_spec_get_blurb( pspec ) );
	gtk_grid_attach( GTK_GRID( options->options_grid ), label, 
		0, *row, 1, 1 );

	g_object_set( t, "hexpand", true, NULL );
	gtk_grid_attach( GTK_GRID( options->options_grid ), t, 
		1, *row, 1, 1 );
	gtk_widget_set_tooltip_text( GTK_WIDGET( label ),
		g_param_spec_get_blurb( pspec ) );

	g_hash_table_insert( options->value_widgets, (gpointer) name, t );

	*row += 1;
}

/* This is the function used by metadata_build_content_box to process a
 * single property of the save operation.
 *
 * See also metadata_build_content_box_argument_map_fn_helper.
 */
static void *
metadata_add_options_fn( VipsObject *operation,
	GParamSpec *pspec, VipsArgumentClass *argument_class,
	VipsArgumentInstance *argument_instance, void *a, void *b )
{
	VipsArgumentFlags flags = argument_class->flags;
	Metadata *options = (Metadata *) a;
	int *row = (int *) b;

	/* Include arguments listed in the constructor.
	 *
	 * Exclude required (we've set these already) or deprecated arguments.
	 */
	if ( !(flags & VIPS_ARGUMENT_DEPRECATED) &&
		(flags & VIPS_ARGUMENT_CONSTRUCT) &&
		!(flags & VIPS_ARGUMENT_REQUIRED) )
		metadata_add_option( options, pspec, row );

	return NULL;
}

Metadata *
metadata_new( GtkWindow *parent_window, 
	VipsImage *image, const char *filename )
{
	const char *saver;
	Metadata *options;

	char *base = g_path_get_basename( filename );
	char *title = g_strdup_printf( "Save image to \"%s\"", base );
	options = g_object_new( SAVE_OPTIONS_TYPE, 
		// we have to set this here, not in the ui file, for some reason
		"use-header-bar", true, 
		"transient-for", parent_window, 
		"title", title,
		NULL );
	g_free( title );
	g_free( base );

	options->image = image;
	g_object_ref( image );

	if( options->image ) { 
//		vips_image_set_progress( options->image, TRUE ); 
//		g_signal_connect_object( options->image, "eval", 
//			G_CALLBACK( metadata_eval ), options, 0 );
//		g_signal_connect_object( options->image, "posteval", 
//			G_CALLBACK( metadata_posteval ), options, 0 );
	}

	if( !(saver = vips_foreign_find_save( filename )) ) 
		metadata_error( options );

	if( saver && options->image ) {
		int row;

		options->save_operation = vips_operation_new( saver );
		g_object_set( options->save_operation,
			"in", options->image,
			"filename", filename,
			NULL );

		row = 0;
		vips_argument_map( VIPS_OBJECT( options->save_operation ),
			metadata_add_options_fn, options, &row );
	}

	return( options );
}

////

/* Size of alphabet index array. This number should be twice the size
 * of the alphabet, which is 128 in this case for ASCII (not extended).
 * The purpose of this extra space is explained later.
 */
#define ALPHABET_SIZE 256

/* The Match object will hold the index @i of the first character of the match
 * and the number of mismatched characters @k within that match. The Match
 * objects are also linked list nodes.
 */
typedef struct Match Match;
struct Match {
	int i;
	int k;
	gchar *text;
	gchar *pattern;
	int pattern_length;
};

Match *
Match_new( int i, int k, gchar *text, gchar *pattern )
{
	Match *t;
	t = g_malloc( sizeof( Match ) );
	t->i = i;
	t->k = k;
	g_assert( text );
	t->text = g_strdup( text );
	t->pattern = pattern;
	t->pattern_length = strlen( pattern );
	return t;
}

void
Match_free( gpointer match_, gpointer user_data )
{
	Match *match = (Match *) match_;
	if ( match ) {
		g_free( match->text );
		g_free( match );
	}		
}

void
Match_print( gpointer match_, gpointer user_data )
{
	Match *match = (Match *) match_;
	printf( "position: %d, errors: %d, text: \"%s\", pattern: \"%s\", pattern_length: %d\n",
		match->i, match->k, match->text, match->pattern,
		match->pattern_length );
}

/* An array of lists. There are 128 linked lists in total - one for each
 * character in our 128-character alphabet. The last 128 lists characters are
 * added to the linked lists as needed. Using a power of lets us quickly access
 * the array in a cyclic manner by modding using the bitwise & operator. 
 *
 * Each list will contain the offsets for each occurence of that character, or a
 * single placeholder offset of -1 if no occurences are found.
 *
 * The offset is the distance of the character to the left of the end of
 * pattern, (i.e., measured by counting to the left from the end of pattern),
 * for a given character and a given instance of pattern.
 */
GList *alpha[ALPHABET_SIZE];

/* This function initializes the @alpha and @count arrays to look like this:
 *
 *     alpha = [ [ -1 ] ] * ALPHABET_SIZE 	where the inner brackets are a GList.
 *
 *     count = [m] * m
 *
 * @alpha will be an array of linked lists. Characters in the pattern
 * that do not occur or that occur exactly once in the text will have
 * corresponding linked lists with length one. Characters in the pattern that
 * occur in the text more than once will have corresponding linked lists with
 * length greater than one.
 *
 * The first m - 1  elements of @count will be skipped on the first iteration of
 * the cyclic array (since no match can be shorter than the @pattern). Note that
 * the values in @count are reset to m once they are no longer needed, until the
 * next loop around @count.
 *
 * @p - pattern string
 * @m - pattern length
 * @alpha - array of GList. See above.
 * @count - circular buffer for counts of matches
 */
void
preprocess( char *p, int m, GList *alpha[], int count[], int max_pattern_size )
{
	int i;

	for ( i = 0; i < ALPHABET_SIZE; i++ ) {
		alpha[i] = NULL;
		alpha[i] = g_list_append( alpha[i], GINT_TO_POINTER( -1 ) );
	}

	for ( i = 0; i < m; i++, p++ ) {
		if ( GPOINTER_TO_INT( alpha[(int)*p]->data ) == -1 )
			alpha[(int)*p]->data = GINT_TO_POINTER( m - i - 1 );
		else
			alpha[(int)*p] = g_list_append( alpha[(int)*p],
				GINT_TO_POINTER( m - i - 1 ) );
	}

	for ( i = 0; i < max_pattern_size; i++ )
		count[i] = m;
}

void
increment_offset( gpointer off_, gpointer args_ )
{
	gpointer *args = (gpointer *) args_;
	int i = GPOINTER_TO_INT( args[0] );
	int max_pattern_size = GPOINTER_TO_INT( args[1] );
	int *count = (int *) args[2];
	gint off = GPOINTER_TO_INT( off_ ) ;
	count[(i + off) % max_pattern_size]--;
}

gint
match_compare( gconstpointer a_, gconstpointer b_ )
{
	Match *a = (Match *) a_;
	Match *b = (Match *) b_;

	if ( a->k == b->k )
		return 0;
	else if (a->k < b->k )
		return -1;
	else return 1;
}

/* Find the position of the first character and number of mismatches of every
 * fuzzy match in a string @t with @k or fewer mismatches. Uses the array of
 * GList @alpha and the array of counts @count prepared by the preprocess
 * function.
 * @t - text string
 * @n - length of text string
 * @m - length of the pattern used to create @alpha and @count
 * @k - maximum number of allowed mismatches
 * @alpha - array of GList. See above.
 * @count - circular buffer for counts of matches
 */
GList *
search( char *t, int n, int m, int k, GList *alpha[], int count[], char *pattern, int max_pattern_size )
{
	int i, off;
	Match *match;
	GList *l0 = NULL, *l1 = NULL;
	char *text = t;

	/* Walk the text @t, which has length @n.
	 */
	for ( i = 0; i < n; i++ ) {
		/* If the current character in @t is in pattern, its
		 * corresponding list in @alpha will have a non-negative offset,
		 * thanks to the workdone by the preprocess function. If so, we
		 * need to decrement the counts in the circular buffer @count
		 * corresponding to the index of the character in the text and
		 * the offsets the lists corresponding to those characters,
		 * which the preprocess function prepared.
		 * 
		 * Note that we will only ever need m counts at a time, and
		 * we reset them to @m when we are done with them, in case
		 * they are needed when the text wraps 256 characters.
		 */
		l0 = alpha[(int)*t++];
		off = GPOINTER_TO_INT( l0->data );
		if ( off >= 0 ) {
			gpointer t[3] = {
				GINT_TO_POINTER( i ),
				GINT_TO_POINTER( max_pattern_size ),
				(gpointer) count,
			};
			g_list_foreach( l0, increment_offset, t );

		}

		/* If the count in @count corresponding to the current index in
		 * the text is no greater than @k, the number of mismatches we
		 * allow, then the pattern instance is reported as a fuzzy
		 * match. The position of the first letter in the match is
		 * calculated using the pattern length and the index of the last
		 * character in the match The number of mismatches is calculated
		 * from the number of matches.
		 */
		if ( i >= m - 1 && count[i % max_pattern_size] <= k ) {
			// DBG
			//printf( "i: %d, m: %d\n", i, m );
			g_assert( i - m + 1 >= 0 );
			g_assert( text );
			match = Match_new( i - m + 1, count[i % max_pattern_size], text, pattern );
			l1 = g_list_append( l1, match );
		}

		/* The count in @count corresponding to the current index in
		 * text is no longer needed, so we reset it to @m until we
		 * need it on the next wraparound.
		 */
		count[i % max_pattern_size] = m;
	}

	/* Sort by increasing k.
	 */
	l1 = g_list_sort( l1, match_compare );

	return l1;
}

void
markup_in_string_by_match( gpointer match_, gpointer markup_ )
{
	Match *match;
	GString *markup;
	gchar *replacement;

	match = (Match *) match_;
	markup = (GString *) markup_;

	if ( match->pattern && match->pattern[0] && !match->k ) {
		replacement = g_strdup_printf( "<>%s</>", match->pattern );
		g_string_replace( markup, match->pattern, replacement, 0 );
		g_free( replacement );
	}
}

gchar *
get_markup_from_match( GList *match )
{
	GString *markup;
	Match *first = (Match *) match->data;

	markup = g_string_new( g_strdup( first->text ) );

	g_list_foreach( match, markup_in_string_by_match, markup );

	g_string_replace( markup, "<>", "<b>", 0 );
	g_string_replace( markup, "</>", "</b>", 0 );

	return markup->str;
}

void
append_field_name( gpointer data, gpointer user_data )
{
	ImageWindow *win;
	GList *match_list;
	GtkWidget *label, *input;
	Match *first_match;

	match_list = (GList *) data;
	first_match = (Match *) match_list->data;

	win = VIPSDISP_IMAGE_WINDOW( user_data );
	win->field_list = g_list_append( win->field_list, first_match->text );

	label = gtk_label_new( first_match->text );	
	gtk_grid_attach( win->metadata_grid, label, 0, win->field_list_length, 1, 1 );

	if ( (input = create_input( win->tile_source->image, first_match->text )) ) {
		gtk_grid_attach( win->metadata_grid, input, 1, win->field_list_length, 1, 1 );
		win->field_list_length++;
	}
}

void
append_markup_field_name( gpointer data, gpointer user_data )
{
	ImageWindow *win;
	gchar *markup;
	GString *field_name;
	GList *match_list;
	GtkWidget *label, *input;

	match_list = (GList *) data;
	win = VIPSDISP_IMAGE_WINDOW( user_data );
	markup = get_markup_from_match( match_list );
	win->field_list = g_list_append( win->field_list, markup );

	label = gtk_label_new( NULL );	
	gtk_label_set_markup( GTK_LABEL( label ), markup );

	gtk_grid_attach( win->metadata_grid, label, 0, win->field_list_length, 1, 1 );

	field_name = g_string_new( markup );
	g_string_replace( field_name, "<b>", "", 0 );
	g_string_replace( field_name, "</b>", "", 0 );

	if ( (input = create_input( win->tile_source->image, field_name->str )) ) {
		gtk_grid_attach( win->metadata_grid, input, 1, win->field_list_length, 1, 1 );
		win->field_list_length++;
	}
}

gint
match_list_compare( gconstpointer a_, gconstpointer b_ )
{
	GList *a = (GList *) a_;
	GList *b = (GList *) b_;

	Match *match_a = (Match *) a->data;
	Match *match_b = (Match *) b->data;

	return match_compare( match_a, match_b );
}

void
append_if_match( gpointer data, gpointer user_data )
{
	GList *first, *match;
	gchar *haystack, *needle;
	GList **list_ptr;
	int n, m, k;

	list_ptr = (GList **) user_data;

	haystack = (gchar *) data;

	first = g_list_first( *list_ptr );
	needle = (gchar *) first->data;

	n = strlen( haystack );
	int count[n];
	m = strlen( needle );
	k = n;

	preprocess( needle, m, alpha, count, n );

	match = search( haystack, n, m, k, alpha, count, needle, n );

	if ( g_list_length( match ) )
		*list_ptr = g_list_append( *list_ptr, match );
}

GList *
find_strings_with_substring( GList *haystacks, gchar *needle )
{
	GList *found = NULL;

	found = g_list_append( found, needle );

	g_list_foreach( haystacks, append_if_match, &found );

	found = g_list_remove( found, (gconstpointer) needle );

	return found;
}

/* This is the callback function called whenever the GtkSearchEntry is modified
 * by the user.
 */
static void
search_changed( GtkWidget *search_entry, gpointer user_data )
{
	ImageWindow *win;
	char *text, *field_name;
	char** field_names;
	int i = 0;
	GList *all_field_list = NULL;

	/* Initialize GList pointers to NULL.
	 */
	GList *found, *found0, *found1, *s0, *s1, *t;
	found = found0 = found1 = s0 = s1 = t = NULL;

	win = VIPSDISP_IMAGE_WINDOW( user_data );

	g_assert( win->field_list );
	g_list_free( win->field_list );
	win->field_list = NULL;
	win->field_list_length = 0;

	gtk_scrolled_window_set_child( GTK_SCROLLED_WINDOW( win->metadata_window ), NULL );
	win->metadata_grid = GTK_GRID( gtk_grid_new() );
	gtk_scrolled_window_set_child( GTK_SCROLLED_WINDOW( win->metadata_window ), GTK_WIDGET( win->metadata_grid ) );

	found = NULL;
	field_names = vips_image_get_fields( win->tile_source->image );

	while ( (field_name = field_names[i++]) )
		all_field_list = g_list_append( all_field_list, field_name );
		
	text = g_strdup( gtk_editable_get_text( GTK_EDITABLE( search_entry) ) );

	found = find_strings_with_substring( all_field_list, (gchar *) text );

	if ( ! g_list_length( found ) )
		return;

	/* Sort by increasing k, the number of errors.
	 */
	found = g_list_sort( found, match_list_compare );

	/* Get two GLists of GLists - one with the k=0 matches, and another with
	 * the k>0 matches.
	 */
	while ( found ) {
		t = (GList *) found->data;
		s0 = s1 = NULL;

		while ( t ) {
			Match *match = (Match *) t->data;
			if ( match ) {
				if ( match->k )
					s1 = g_list_append( s1, match );
				else
					s0 = g_list_append( s0, match );
				t = t->next;
			}
		}

		if ( s0 )
			found0 = g_list_append( found0, s0 );

		if ( s1 )
			found1 = g_list_append( found1, s1 );

		found = found->next;
	}


	/* Add the exact (k=0) matches, if any. These will have markup.
	 */
	if ( g_list_length( found0 ) )
		g_list_foreach( found0, append_markup_field_name, win );

	/* If there are no exact matches, then add the fuzzy (k>0) matches, if
	 * any. These will not have markup.
	 */
	if ( !g_list_length( found0 ) && g_list_length( found1 ) )
		g_list_foreach( found1, append_field_name, win );
}

GtkGrid *
create_input_grid( ImageWindow *win )
{
	GtkWidget *label, *input;
	GtkGrid *grid;
	char *field_name;
	char **field_names;
	int i = 0;

	field_names = vips_image_get_fields( win->tile_source->image );

	while ( (field_name = field_names[i++]) )
		win->field_list = g_list_append( win->field_list, field_name );

	win->field_list_length = g_list_length( win->field_list );

	grid = GTK_GRID( gtk_grid_new() );

	i = 0;

	while ( (field_name = g_list_nth_data( win->field_list, i )) ) {
		label = gtk_label_new( field_name );	
		gtk_grid_attach( grid, label, 0, i, 1, 1 );
		input = create_input( win->tile_source->image, field_name );
		gtk_grid_attach( grid, input, 1, i, 1, 1 );
		i++;
	}

	return grid;
}

gboolean
shrink_window( gpointer user_data )
{
	ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );
	if ( !gtk_widget_get_visible( win->metadata ) )
		return FALSE;
	gtk_widget_set_size_request( win->metadata, 0, 0 );
	gtk_widget_hide( win->metadata );
	gtk_orientable_set_orientation( GTK_ORIENTABLE( win->main_box ), GTK_ORIENTATION_VERTICAL );
	gtk_window_set_default_size( GTK_WINDOW( win ), win->og_width, win->og_height );

	g_settings_set_value( win->settings, "metadata", g_variant_new_boolean( FALSE ) );

	change_state( GTK_WIDGET( win ), "metadata", 
		g_settings_get_value( win->settings, "metadata" ) );

	return TRUE;
}

void
on_metadata_apply_button_pressed( GtkWidget *_button, gpointer user_data )
{
	GtkWidget *t, *label, *revealer;
	char *field_name;
	GString *field_name_string;
	VipsImage *image; 
	VipsObjectClass *oclass;
	GType otype;
	GParamSpec *pspec;
	VipsArgumentClass *argument_class;
	VipsArgumentInstance *argument_instance;
	ImageWindow *win;
	int row_index = 0;

        win = VIPSDISP_IMAGE_WINDOW( user_data );
	image = win->tile_source->image;

	while ( (label = gtk_grid_get_child_at( win->metadata_grid, 0, row_index )) ) {
		t = gtk_grid_get_child_at( win->metadata_grid, 1, row_index++ );
		g_assert( t );
		t = gtk_widget_get_first_child( t );
		g_assert( t );
		t = gtk_widget_get_first_child( t );
		g_assert( t );

		GValue value = { 0 }, v = { 0 };

		field_name = g_strdup( gtk_label_get_text( GTK_LABEL( label ) ) );
		field_name_string = g_string_new( field_name );
		g_string_replace( field_name_string, "<b>", "", 0 );
		g_string_replace( field_name_string, "</b>", "", 0 );
		g_free( field_name );
		field_name = field_name_string->str;

		gboolean use_string = FALSE; 
		if( vips_object_get_argument( VIPS_OBJECT( image ), field_name,
			&pspec, &argument_class, &argument_instance ) ) {
			vips_error_clear();
			vips_image_get( image, field_name, &value );
			use_string = TRUE;
		} else {
			otype = G_PARAM_SPEC_VALUE_TYPE( pspec );
			if( g_type_is_a( otype, VIPS_TYPE_IMAGE ) )
				return;
			else if( g_type_is_a( otype, VIPS_TYPE_OBJECT ) &&
				(oclass = g_type_class_ref( otype )) )
				return;
		}

		if( use_string ) {
			GType type = G_VALUE_TYPE( &value );
			if ( type == G_TYPE_STRING ) {
				GtkEntryBuffer* buffer = gtk_text_get_buffer( GTK_TEXT( t ) );
				char *text = g_strdup( gtk_entry_buffer_get_text( buffer ) );
				vips_image_set_string( image, field_name, text );
			} else if ( type == G_TYPE_ENUM ) {
			} else if ( type == G_TYPE_INT ) {
				int d = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( t ) );
				vips_image_set_int( image, field_name, d );
			} else if ( type == G_TYPE_UINT ) {
				int d = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( t ) );
				vips_image_set_int( image, field_name, d );
			} else if ( type == G_TYPE_INT64 ) {
				int d = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( t ) );
				vips_image_set_int( image, field_name, d );
			} else if ( type == G_TYPE_UINT64 ) {
				int d = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( t ) );
				vips_image_set_int( image, field_name, d );
			} else if ( type == G_TYPE_LONG ) {
				int d = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( t ) );
				vips_image_set_int( image, field_name, d );
			} else if ( type == G_TYPE_ULONG ) {
				int d = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( t ) );
				vips_image_set_int( image, field_name, d );
			} else if ( type == G_TYPE_BOOLEAN ) {
				gboolean b = gtk_check_button_get_active( GTK_CHECK_BUTTON( t ) );
				g_value_init( &v, G_TYPE_BOOLEAN );
				g_value_set_boolean( &v, b );
				vips_image_set( image, field_name, &v );
				g_value_unset( &v );
			} else if ( type == G_TYPE_FLOAT ) {
				int d = gtk_spin_button_get_value( GTK_SPIN_BUTTON( t ) );
				vips_image_set_double( image, field_name, d );
			} else if ( type == G_TYPE_DOUBLE ) {
				int d = gtk_spin_button_get_value( GTK_SPIN_BUTTON( t ) );
				vips_image_set_double( image, field_name, d );
			} else if ( type == G_TYPE_FLAGS ) {
				int d = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( t ) );
				vips_image_set_int( image, field_name, d );
			} else if ( type == G_TYPE_BOXED ) {
			} else if ( (type == VIPS_TYPE_REF_STRING) ) {
				GtkEntryBuffer* buffer = gtk_text_get_buffer( GTK_TEXT( t ) );
				const char *text = gtk_entry_buffer_get_text( buffer );
				vips_image_set_blob_copy( image, field_name, text, strlen( text ) );
			} else {
				// Must be a VipsBlob then

				// Ignore metadata field names that contain the substring "thumbnail"
				if ( !strstr( field_name, "thumbnail" ) ) {
					GtkEntryBuffer* buffer = gtk_text_get_buffer( GTK_TEXT( t ) );
					const char *text = gtk_entry_buffer_get_text( buffer );
					vips_image_set_blob_copy( image, field_name, text, strlen( text ) );
				}
			}
		}
		else if ( G_IS_PARAM_SPEC_STRING( pspec ) ) {
			GtkEntryBuffer* buffer = gtk_text_get_buffer( GTK_TEXT( t ) );
			char *text = g_strdup( gtk_entry_buffer_get_text( buffer ) );
			vips_image_set_string( image, field_name, text );
		}
		else if( G_IS_PARAM_SPEC_BOOLEAN( pspec ) ) {
			gboolean b = gtk_check_button_get_active( GTK_CHECK_BUTTON( t ) );
			g_value_init( &v, G_TYPE_BOOLEAN );
			g_value_set_boolean( &v, b );
			vips_image_set( image, field_name, &v );
			g_value_unset( &v );
		}
		else if( G_IS_PARAM_SPEC_ENUM( pspec ) ) {
			int d = gtk_drop_down_get_selected( GTK_DROP_DOWN( t ) );
			g_value_init( &v, G_TYPE_ENUM );
			g_value_set_enum( &v, d );
			vips_image_set( image, field_name, &v );
			g_value_unset( &v );
		
		}
		else if( G_IS_PARAM_SPEC_INT64( pspec ) ) {
			int d = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( t ) );
			vips_image_set_int( image, field_name, d );
		}
		else if( G_IS_PARAM_SPEC_INT( pspec )) {
			int d = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( t ) );
			vips_image_set_int( image, field_name, d );
		}
		else if( G_IS_PARAM_SPEC_UINT64( pspec ) ) {
			int d = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( t ) );
			vips_image_set_int( image, field_name, d );
		}
		else if( G_IS_PARAM_SPEC_DOUBLE( pspec ) ) {
			int d = gtk_spin_button_get_value( GTK_SPIN_BUTTON( t ) );
			vips_image_set_double( image, field_name, d );
		}
		else if( G_IS_PARAM_SPEC_BOXED( pspec ) ) {	
			if( g_type_is_a( otype, VIPS_TYPE_ARRAY_INT ) ) {
				return;
			}
			else if( g_type_is_a( otype, VIPS_TYPE_ARRAY_DOUBLE ) ) {
				return;
			}
			else if( g_type_is_a( otype, VIPS_TYPE_ARRAY_IMAGE ) ) {
				return;
			}
			else {
				return;
			}
		}
		else {
			printf("Unknown type for property \"%s\"\n", field_name);
			g_object_ref_sink( t );
			return;
		}
	}

	revealer = gtk_widget_get_first_child( win->metadata );
	gtk_revealer_set_reveal_child( GTK_REVEALER( revealer ), FALSE );
	g_timeout_add( 200 , (GSourceFunc) shrink_window, win );
}

GtkWidget  *
create_input( VipsImage *image, char* field_name )
{
	GtkWidget *input_box, *t, *box;
	VipsObjectClass *oclass;
	GType otype;
	GParamSpec *pspec;
	VipsArgumentClass *argument_class;
	VipsArgumentInstance *argument_instance;

	/* It is crucial to zero the GValue whose address we pass to
	 * vips_image_get. Otherwise, we will get runtime errors.
	 */
	GValue value = { 0 };

	input_box = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );

	gtk_widget_set_margin_start( input_box, 20 );

	/* Get the value of the given field from the image.
	 * Check the GType of @value, and select the appropriate user input
         * widget. Initialize the widget with @value.
	 * TODO
 	 */
	gboolean use_string = FALSE; 
	if( vips_object_get_argument( VIPS_OBJECT( image ), field_name,
		&pspec, &argument_class, &argument_instance ) ) {
		//g_warning( "%s", vips_error_buffer() );
		vips_error_clear();
		//return;
		vips_image_get( image, field_name, &value );
		use_string = TRUE;
	} else {
		otype = G_PARAM_SPEC_VALUE_TYPE( pspec );

		/* For now, skip VipsImage and VipsObject types.
		*/
		if( g_type_is_a( otype, VIPS_TYPE_IMAGE ) )
			return NULL;
		else if( g_type_is_a( otype, VIPS_TYPE_OBJECT ) &&
			(oclass = g_type_class_ref( otype )) )
			return NULL;
	}

	/* Add a user input widget for this property to the input_box. The widget
	 * chosen depends on the type of the property. Set the initial value of
	 * the user input widget to the default value for the property.
	 */
	if( use_string ) {
		GType type = G_VALUE_TYPE( &value );
		if ( type == G_TYPE_STRING ) {
			char* string_value = g_strdup( g_value_get_string( &value ) );
			GtkEntryBuffer* buffer =
				gtk_entry_buffer_new( string_value, -1 );

			t = gtk_text_new_with_buffer( buffer );
		} else if ( type == G_TYPE_ENUM ) {
			t = gtk_spin_button_new_with_range( -G_MAXINT + 1, G_MAXINT, 1 );
			gtk_spin_button_set_value( GTK_SPIN_BUTTON( t ),
				g_value_get_int( &value ) );
		} else if ( type == G_TYPE_INT ) {
			t = gtk_spin_button_new_with_range( -G_MAXINT + 1, G_MAXINT, 1 );
			gtk_spin_button_set_value( GTK_SPIN_BUTTON( t ),
				g_value_get_int( &value ) );
		} else if ( type == G_TYPE_UINT ) {
			t = gtk_spin_button_new_with_range( -G_MAXINT + 1, G_MAXINT, 1 );
			gtk_spin_button_set_value( GTK_SPIN_BUTTON( t ),
				g_value_get_int( &value ) );
		} else if ( type == G_TYPE_INT64 ) {
			t = gtk_spin_button_new_with_range( -G_MAXINT + 1, G_MAXINT, 1 );
			gtk_spin_button_set_value( GTK_SPIN_BUTTON( t ),
				g_value_get_int( &value ) );
		} else if ( type == G_TYPE_UINT64 ) {
			t = gtk_spin_button_new_with_range( -G_MAXINT + 1, G_MAXINT, 1 );
			gtk_spin_button_set_value( GTK_SPIN_BUTTON( t ),
				g_value_get_int( &value ) );
		} else if ( type == G_TYPE_LONG ) {
			t = gtk_spin_button_new_with_range( -G_MAXINT + 1, G_MAXINT, 1 );
			gtk_spin_button_set_value( GTK_SPIN_BUTTON( t ),
				g_value_get_int( &value ) );
		} else if ( type == G_TYPE_ULONG ) {
			t = gtk_spin_button_new_with_range( -G_MAXINT + 1, G_MAXINT, 1 );
			gtk_spin_button_set_value( GTK_SPIN_BUTTON( t ),
				g_value_get_int( &value ) );
		} else if ( type == G_TYPE_BOOLEAN ) {
			t = gtk_check_button_new();
			gtk_check_button_set_active( GTK_CHECK_BUTTON( t ),
				g_value_get_boolean( &value ) );
		} else if ( type == G_TYPE_FLOAT ) {
			t = gtk_spin_button_new_with_range( -G_MAXFLOAT + 1, G_MAXFLOAT, 1 );
			gtk_spin_button_set_value( GTK_SPIN_BUTTON( t ),
				g_value_get_int( &value ) );
		} else if ( type == G_TYPE_DOUBLE ) {
			t = gtk_spin_button_new_with_range( -G_MAXDOUBLE + 1, G_MAXDOUBLE, 1 );
			gtk_spin_button_set_value( GTK_SPIN_BUTTON( t ),
				g_value_get_int( &value ) );
		} else if ( type == G_TYPE_FLAGS ) {
			t = gtk_spin_button_new_with_range( -G_MAXINT + 1, G_MAXINT, 1 );
			gtk_spin_button_set_value( GTK_SPIN_BUTTON( t ),
				g_value_get_int( &value ) );
		} else if ( type == G_TYPE_LONG ) {
			t = gtk_spin_button_new_with_range( -G_MAXINT + 1, G_MAXINT, 1 );
			gtk_spin_button_set_value( GTK_SPIN_BUTTON( t ),
				g_value_get_int( &value ) );
		} else if ( type == G_TYPE_ULONG ) {
			t = gtk_spin_button_new_with_range( -G_MAXINT + 1, G_MAXINT, 1 );
			gtk_spin_button_set_value( GTK_SPIN_BUTTON( t ),
				g_value_get_int( &value ) );
		} else if ( type == G_TYPE_BOOLEAN ) {
			t = gtk_check_button_new();
			gtk_check_button_set_active( GTK_CHECK_BUTTON( t ),
				g_value_get_boolean( &value ) );
		} else if ( type == G_TYPE_FLOAT ) {
			t = gtk_spin_button_new_with_range( -G_MAXFLOAT + 1, G_MAXFLOAT, 1 );
			gtk_spin_button_set_value( GTK_SPIN_BUTTON( t ),
				g_value_get_int( &value ) );
		} else if ( type == G_TYPE_DOUBLE ) {
			t = gtk_spin_button_new_with_range( -G_MAXDOUBLE + 1, G_MAXDOUBLE, 1 );
			gtk_spin_button_set_value( GTK_SPIN_BUTTON( t ),
				g_value_get_int( &value ) );
		} else if ( type == G_TYPE_BOXED ) {
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
				return NULL;
			}
			else {
				/* Ignore parameters of unrecognized type for now.
				 */
				return NULL;
			}
		} else if ( (type = VIPS_TYPE_REF_STRING) ) {
			if ( !strstr( field_name, "thumbnail" ) ) {
				VipsRefString *ref_string;
				ref_string = g_value_get_boxed( &value );
				char *string_value = g_strdup( vips_ref_string_get( ref_string, NULL ) );
				GtkEntryBuffer* buffer =
					gtk_entry_buffer_new( string_value, -1 );
				t = gtk_text_new();
				gtk_text_set_buffer( GTK_TEXT( t ), buffer );
			} else {
				// Then just make @t an empty label.
				t = gtk_label_new( NULL );
			}
		} else {
			// Must be a VipsBlob then

			// Ignore metadata field names that contain the substring "thumbnail"
			if ( !strstr( field_name, "thumbnail" ) ) {
				VipsRefString *ref_string;
				ref_string = g_value_get_boxed( &value );
				char *string_value = g_strdup( vips_ref_string_get( ref_string, NULL ) );
				GtkEntryBuffer* buffer =
					gtk_entry_buffer_new( string_value, -1 );
				t = gtk_text_new();
				gtk_text_set_buffer( GTK_TEXT( t ), buffer );
			}
		}
	}
	else if ( G_IS_PARAM_SPEC_STRING( pspec ) ) {
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
			(int)pspec_int->default_value );
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
			return NULL;
		}
		else {
			/* Ignore parameters of unrecognized type for now.
			 */
			return NULL;
		}
	}
	else {
		printf("Unknown type for property \"%s\"\n", field_name);
		g_object_ref_sink( input_box );
		return NULL;
	}

	/* Create a box to contain the user input widget "t", and add a tooltip
	 * to the box. The tooltip contains the "blurb" for the property.
	 * Make the user input widget "t" fill the box horizontally.
	 * Append that container box to the "input_box".
	 */
	box = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
	if ( !use_string )
		gtk_widget_set_tooltip_text( GTK_WIDGET( box ),
			g_param_spec_get_blurb( pspec ) );

	gtk_widget_set_hexpand( t, TRUE );
	gtk_box_append( GTK_BOX( box ), t );
	gtk_box_append( GTK_BOX( input_box ), box );

	return input_box;
}

static void
metadata_close_button_cb( GtkWidget *widget, gpointer user_data )
{
	ImageWindow *win = VIPSDISP_IMAGE_WINDOW( user_data );
	GtkWidget *revealer = gtk_widget_get_first_child( win->metadata );
	gtk_revealer_set_reveal_child( GTK_REVEALER( revealer ), FALSE );
	g_timeout_add( 200 , (GSourceFunc) shrink_window, win );
}

