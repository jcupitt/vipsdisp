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

#define MIN3(a, b, c) ((a) < (b) ? ((a) < (c) ? (a) : (c)) : ((b) < (c) ? (b) : (c)))

/* LEVENSHTEIN DISTANCE
 *
 * @s1 and @s2 are character arrays of size @n1 and @n2, which represent two
 * strings. Any null bytes are treated like a normal character, so strings
 * without a terminating null byte are valid input.
 * 
 * The LEVENSHTEIN DISTANCE between @s1 and @s2 is the smallest number of
 * insertions, deletions, or substitutions needed to transform @s1 into @s2.
 *
 * The algorithm dynamically builds a table T[n1+1]][n2+1] where the (x, y)
 * entry is the Levenshtein distance between the prefix of @s1 of size y-1 and
 * the prefix of @s2 of size x-1.
 *
 * This implementation uses only a single @v column, updating it in place, since
 * only the elements at (x-1, y-1) and (x, y-1) are needed to compute the
 * element at (x, y). This is expressed by the recursion relation:
 *
 *	k = (s1[y-1] == s2[x-2] ? 0 : 1)
 *
 * 	T[x][y] = MIN3( T[x-1][y] + 1, T[x][y - 1] + 1, T[x-1][y-1] + k )
 *
 * If we let @t1 hold the previous diagonal value, we can rewrite this
 * in terms of the single column @v:
 *
 *	k = (s1[y-1] == s2[x-2] ? 0 : 1)
 *
 * 	v[y] = MIN3( v[y] + 1, v[y-1] + 1, t1 + k )
 *
 * The irst column of the table T is unused, and the first row is
 * used as the base case for the recursion relation.
 *
 *                         s2
 *
 *     *----l--e--v--e--n--s--h--t--e--i--n--- x
 *     | .  1  2  3  4  5  6  7  8  9  10 11
 *     |
 *     d .  1  2  3  4  5  6  7  8  9  10 11
 *     |
 *     a .  2  2  3  4  5  6  7  8  9  10 11
 *     |
 *     m .  3  3  3  4  5  6  7  8  9  10 11
 * s1  |
 *     e .  4  3  4  3  4  5  6  7  8  9  10
 *     |
 *     r .  5  4  4  4  4  5  6  7  8  9  10
 *     |
 *     a .  6  5  5  5  5  5  6  7  8  9  10
 *     |
 *     u .  7  6  6  6  6  6  6  7  8  9  10
 *     | 
 *
 *     y
 *
 * SYMBOLS
 *
 * @v:	Column vector. Reused and updated in place. The @t1 and @t0 values keep
 *	track of the most recent and second most recent diagonal values.
 * @x:	Row Index.
 * @y:	Column Index.
 * @t0:	Second most recent diagonal value.
 * @t1:	Most recent diagonal value
 * @k:	Test inqeuality in s1[y-1] and s2[x-1], the pair of characters
 * 	corresponding to the table entry T[x][y]. Equals 0 if these characters
 *	are equal, and 1 if they are different.
 *
 * TERMS
 *
 * LD: Levenshtein Distance
 *
 * SOURCES
 *
 * https://en.wikipedia.org/wiki/Levenshtein_distance
 * https://en.wikibooks.org/wiki/Algorithm_Implementation/Strings/Levenshtein_distance#C
 */

/* Compute the Levenshtein Distance (LD) between two strings, @s1 and @s2.
 *
 * @n1:		Character array 1 size
 *
 * @s1:		Character array 1
 *
 * @n2:		Character array 2 size
 *
 * @s2:		Character array 2
 *
 * @v:		Buffer where column of Levenshtein distances are written
 * 		during computation.
 */
guint
glev( guint n1, gchar s1[n1], guint n2, gchar s2[n2], guint v[n1 + 1] ) {
	guint x, y, t0, t1, k;

	// Initialize the column.
	for ( y = 1; y <= n1; y++ )
		v[y] = y;

	// Ignore and don't even bother to initialize the first column. Walk
	// through columns after the first.
	for ( x = 1; x <= n2; x++ ) {
		// The first row (ignoring the first entry) is just the column
		// indices from 1 to n2.
		v[0] = x;

		// The recursion relation defined above and the base case
		// conditions { y = 1, t1 = x - 1 } are used to build the
		// Table T column by column. Only one column @v is needed
		// in memory at a time, and it can be operated on in
		// place, as long as temporary variables @t0 and @t1 are used
		// to keep track of the last diagonal when @v is updated.
		for ( y = 1, t1 = x - 1; y <= n1; y++ ) {
			t0 = v[y];
			k = s1[y - 1] == s2[x - 1] ? 0 : 1;
			v[y] = MIN3( v[y] + 1, v[y - 1] + 1, t1 + k );
			t1 = t0;
		}
	}

	// Return Levenshtein Distance.
	return v[y-1];
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
 * A GList of exact matches contains data needed to locate the exactly
 * matching substrings from start to end.
 *
 * A GList of inexact or "fuzzy" matches contain data needed to order
 * those match from best to worst. In this case the metric used ws
 * the Levenshtein Distance.
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
		ma = Match_new( TRUE, s - text, 0, text, patt );
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
