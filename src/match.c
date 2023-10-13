/*
#define DEBUG
 */

#include "vipsdisp.h"

/* Expects NULL-terminated strings.
 */
Match *
Match_new( gboolean exact, int i, int ld, gchar *text, gchar *patt )
{
	Match *t;
	t = g_malloc( sizeof( Match ) );
	t->exact = exact;
	t->i = i;
	t->ld = ld;
	//g_assert( text );
	t->text = g_strdup( text );
	t->n_text = strlen( text );
	t->patt = patt;
	t->n_patt = strlen( patt );
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
	printf( "position: %d, errors: %d, text: \"%s\", patt: \"%s\", n_patt: %d\n",
		match->i, match->ld, match->text, match->patt,
		match->n_patt );
}

gint
Match_comp( gconstpointer a_, gconstpointer b_ )
{
	Match *a = (Match *) a_;
	Match *b = (Match *) b_;

	if ( a->ld == b->ld )
		return 0;
	else if (a->ld < b->ld )
		return -1;
	else return 1;
}

void
Match_markup_cb( gpointer ma_, gpointer markup_ )
{
	Match *ma;
	GString *markup;
	gchar *replacement;

	ma = (Match *) ma_;
	markup = (GString *) markup_;

	if ( ma->patt && ma->patt[0] && !ma->ld ) {
		replacement = g_strdup_printf( "<>%s</>", ma->patt );
		g_string_replace( markup, ma->patt, replacement, 0 );
		g_free( replacement );
	}
}

gchar *
Match_markup( GList *match )
{
	GString *markup;
	Match *first = (Match *) match->data;

	markup = g_string_new( g_strdup( first->text ) );

	g_list_foreach( match, Match_markup_cb, markup );

	g_string_replace( markup, "<>", "<b>", 0 );
	g_string_replace( markup, "</>", "</b>", 0 );

	return markup->str;
}

gint
Match_list_comp( gconstpointer a_, gconstpointer b_ )
{
	GList *a = (GList *) a_;
	GList *b = (GList *) b_;

	Match *match_a = (Match *) a->data;
	Match *match_b = (Match *) b->data;

	return Match_comp( match_a, match_b );
}

/* Get a GList of exact matches or a G_List containing one inexact match.
 *
 * @text - NULL-terminated string; the target of the search
 * @patt - NULL-terminated string; the pattern to search for
 */
GList*
Match_fuzzy_list( char *text, char *patt )
{
	char *s;
	GList *r;
	Match *ma;
	guint *v;

	r = NULL;
	if ( !patt || !*patt) {
		ma = Match_new( TRUE, 0, 0, text, patt );
		r = g_list_append( r, ma );
		return r;
	}

	s = text;
	while( *s && (s = strstr( s, patt )) ) {
		ma = Match_new( TRUE, text - s, 0, text, patt );
		r = g_list_append( r, ma );
		s += ma->n_patt;
	}

	if ( !r ) {
		ma = Match_new( FALSE, 0, 0, text, patt );
		v = g_malloc( (ma->n_patt + 1) * sizeof( guint ) );
		ma->ld = glev( ma->n_patt, ma->patt, ma->n_text, ma->text, v );
		g_free( v );
		r = g_list_append( r, ma );
	}

	return r;
}

/* Callback for g_list_foreach loop
 *
 * @data:	text to search
 *
 * @user_data:	Pointer to GList of GList of Match objects to append
 * 		to. The first element is the search pattern.
 */
void
Match_substr_cb( gpointer data, gpointer user_data )
{
	// Pointer to GList of GList
	GList **ll;
	gchar *text, *patt;
	GList *first;

	text = (gchar *) data;
	ll = (GList **) user_data;

	// first element is the search pattern
	first = g_list_first( *ll );

	patt = (gchar *) first->data;

	// Append a GList of one or more Match to the list of lists
	*ll = g_list_append( *ll, Match_fuzzy_list( text, patt ) );
}

/* Find exact and inexact matches of a pattern within a GList of text strings.
 * Use g_list_foreach with callback Match_substr_cb. Initialize text_list
 * with the search pattern
 *
 * @text_list:	List of text strings to search in
 *
 * @patt:	Search pattern
 */
GList *
Match_substr( GList *text_list, gchar *patt )
{
	GList *found = NULL;

	found = g_list_append( found, patt );
	g_list_foreach( text_list, Match_substr_cb, &found );
	found = g_list_remove( found, (gconstpointer) patt );

	return found;
}
