/*
#define DEBUG
 */

#include "vipsdisp.h"

/* The Match struct and its methods are used for string matching in the
 * search-as-you-type feature of the Metadata widget. See "match.h" for a more
 * detailed explanation of the Match struct.
 */

/* Create a new Match object.
 *
 * Expects NULL-terminated strings.
 *
 * Must be freed with Match_free.
 *
 * @exact:	Exact match (TRUE) or inexact (FALSE)
 * @i:		Offset from @text to @patt. Used for exact matching.
 * @ld:		Distance between @text and @patt. Used for inexact matching.
 * @text:	Text to search in
 * @patt:	Search pattern
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

/* Clean up a Match object. Usable as a callback function in a g_list_foreach
 * call.
 * 
 * @match_	gpointer (Match *)
 * @user_data_	gpointer		Boilerplate argument
 */
void
Match_free( gpointer match_, gpointer user_data_ )
{
	Match *match;

	match = (Match *) match_;

	if ( match ) {
		g_free( match->text );
		g_free( match );
	}		
}

/* Print a string representation of a Match object. Usable as a callback
 * function in a g_list_foreach call.
 *
 * @match_	gpointer (Match *)
 * @user_data_	gpointer		Boilerplate argument
 */
void
Match_print( gpointer match_, gpointer user_data )
{
	Match *match = (Match *) match_;
	printf( "position: %d, errors: %d, text: \"%s\", patt: \"%s\", n_patt: %d\n",
		match->i, match->ld, match->text, match->patt,
		match->n_patt );
}

/* This convenient macro gets the minimum value of three values.
 */
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
 * @n1:			Character array 1 size
 *
 * @s1:			Character array 1
 *
 * @n2:			Character array 2 size
 *
 * @s2:			Character array 2
 *
 * @v:			Buffer where column of Levenshtein distances are written
 * 			during computation.
 *
 * @ignore_case:	TRUE if case is ignored. 
 * 			FALSE if case is NOT ignored.	
 */
guint
glev( guint n1, gchar s1[n1], guint n2, gchar s2[n2], guint v[n1 + 1], gboolean ignore_case ) {
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

			g_assert(ignore_case);

			k = ignore_case ?
				((g_ascii_tolower(s1[y - 1]) == g_ascii_tolower(s2[x - 1])) ? 0 : 1)
				: ((s1[y - 1] == s2[x - 1]) ? 0 : 1);

			v[y] = MIN3( v[y] + 1, v[y - 1] + 1, t1 + k );
			t1 = t0;
		}
	}

	// Return Levenshtein Distance.
	return v[y-1];
}

/* Compare two Match objects.
 *
 * @a_	gconstpointer (Match *)		The lefthand Match object.
 * @b_ 	gconstpointer (Match *)		The righthand Match object.
 */
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

/* Version of Match_comp that can be used with g_list_sort on a GList of Match
 * pointers.
 *
 * @a_	gconstpointer (Match *)		The lefthand Match object.
 * @b_ 	gconstpointer (Match *)		The righthand Match object.
 */
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
 * the Levenshtein Distance between @text and @patt.
 *
 * @text - NULL-terminated string; the target of the search
 * @patt - NULL-terminated string; the pattern to search for
 */
GList*
Match_fuzzy_list( char *text, char *patt, gboolean ignore_case )
{
	char *comp_patt, *comp_text, *s;
	GList *r;
	Match *ma;
	guint *v;

	r = NULL;
	if ( !patt || !*patt) {
		ma = Match_new( TRUE, 0, 0, text, patt );
		r = g_list_append( r, ma );
		return r;
	}

	comp_patt = ignore_case ? g_ascii_strdown( patt, -1 ) : g_strdup( patt );
	comp_text = ignore_case ? g_ascii_strdown( text, -1 ) : g_strdup( text );

	s = comp_text;
	while( *s && (s = strstr( s, comp_patt )) ) {
		ma = Match_new( TRUE, s - comp_text, 0, text, patt );
		r = g_list_append( r, ma );
		s += ma->n_patt;
	}

	if ( !r ) {
		ma = Match_new( FALSE, 0, 0, text, patt );
		v = g_malloc( (ma->n_patt + 1) * sizeof( guint ) );
		ma->ld = glev( ma->n_patt, ma->patt, ma->n_text, ma->text, v, ignore_case );
		g_free( v );
		r = g_list_append( r, ma );
	}

	g_free( comp_patt );
	g_free( comp_text );

	return r;
}

/* Find exact and inexact matches of a pattern within a GList of text strings.
 *
 * @text_list:	List of text strings to search in
 *
 * @patt:	Search pattern
 */
GList *
Match_substr( GList *text_list, gchar *patt, gboolean ignore_case )
{
	GList *found = NULL, *t0, *t1;

	if ( !text_list )
		return NULL;

	if ( !(t0 = g_list_first( text_list )) )
		return NULL;

	/* Iterate over elements of @text_list, which have type gchar*.
	 */
	do {
		/* Append a GList of Match objects to the list of lists.
		 */
		t1 = Match_fuzzy_list( (gchar *) t0->data, patt, ignore_case );
		found = g_list_append( found, t1 );
	} while ( (t0 = t0->next) );

	return found;
}
