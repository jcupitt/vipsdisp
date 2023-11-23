/*
#define DEBUG
 */

#include "vipsdisp.h"

/* The Match struct and its methods are used for string matching in the
 * search-as-you-type feature of the Properties widget. See "match.h" for a more
 * detailed explanation of the Match struct.
 */

/* Create a new Match object.
 *
 * Must be freed with match_free.
 *
 * The caller is responsible for freeing @text. 
 *
 * @exact:	Exact match (TRUE) or inexact (FALSE)
 * @i:		Offset from @text to @patt. Used for exact matching.
 * @ld:		Distance between @text and @patt. Used for inexact matching.
 * @text:	Text to search in. Owned by the caller. 
 * @patt:	Search pattern. Owned by the caller.
 */
Match *
match_new( gboolean exact, gint i, const gint ld, const gchar *text, const gchar *patt )
{
	Match *t;
	t = g_malloc( sizeof( Match ) );
	t->exact = exact;
	t->i = i;
	t->ld = ld;
	t->text = text;
	t->n_text = text ? strlen( text ) : 0;
	t->patt = patt;
	t->n_patt = patt ? strlen( patt ) : 0;
	return( t );
}

/* Clean up a Match object. Usable as a callback function in a g_list_foreach
 * call.
 *
 * @match_	gpointer (Match *)
 * @user_data_	gpointer		Boilerplate argument
 */
void
match_free( gpointer match_, gpointer user_data_ )
{
	Match *match;

	match = (Match *) match_;

	if( match )
		g_free( match );
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
 * entry is the LEVENSHTEIN DISTANCE between the prefix of @s1 of size y-1 and
 * the prefix of @s2 of size x-1.
 *
 * This implementation uses only a single @v column, updating it in place, since
 * only the elements at (x-1, y-1) and (x, y-1) are needed to compute the
 * element at (x, y). When the three costs defined by this dynamic algorithm are
 * equal to 1, this is expressed by the recursion relation:
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
 * This array @v is reusable, so it can be allocated beforehand.
 *
 * The first column of the table T is unused, and the first row is
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
 * LD: LEVENSHTEIN DISTANCE
 *
 * SOURCES
 *
 * https://en.wikipedia.org/wiki/Levenshtein_distance
 * https://en.wikibooks.org/wiki/Algorithm_Implementation/Strings/Levenshtein_distance#C
 */

/* Compute the LEVENSHTEIN DISTANCE (LD) between two strings, @s1 and @s2.
 *
 * @n1:			Character array 1 size
 *
 * @s1:			Character array 1
 *
 * @n2:			Character array 2 size
 *
 * @s2:			Character array 2
 *
 * @v:			Buffer where column of LEVENSHTEIN DISTANCE values are
 * 			written during computation. Must be allocated
 * 			beforehand by the caller. Owned by the caller.
 *
 * @ignore_case:	TRUE if case is ignored.
 * 			FALSE if case is NOT ignored.
 */

/* The insertion, deletion, and substitution costs are set to allow for long
 * strings containing matches that are only a few deletions and/or
 * substitutions away from any position. There is 0 cost for inserting
 * characters to shift the smaller pattern toward the end in order to almost
 * match with a substring near the end of the longer string.
 * 
 * This is ideal for the use case in the Properties widget (the only use case),
 * where it is used to suggest inexact matches as a user types, whenever
 * no exact matches are available for the current search pattern.
 *
 * The point is to be forgiving. The costs are not high for deletion and
 * insertion, but insertion cost is zero. The cost of swapping two
 * adjacent letters is DEL_COST + INS_COST = 1 + 0 = 1, so the costs of
 * all common mistakes are balanced except insertion, which is free.
 *
 * This allows the user to type quickly without worrying about small mistakes.
 *
 * The costs can be tweaked for other use cases if they arise.
 */
#define INS_COST 0
#define DEL_COST 1
#define SUB_COST 1

guint
glev( guint n1, const gchar s1[n1], guint n2, const gchar s2[n2], guint v[n1 + 1], gboolean ignore_case ) {
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
				: ((s1[y - 1] == s2[x - 1]) ? 0 : SUB_COST );

			v[y] = MIN3( v[y] + INS_COST, v[y - 1] + DEL_COST, t1 + k );
			t1 = t0;
		}
	}

	// Return LEVENSHTEIN DISTANCE.
	return( v[y-1] );
}

/* Compare two Match objects by LEVENSHTEIN DISTANCE @ld.
 *
 * @a_	gconstpointer (Match *)		The lefthand Match object.
 * @b_ 	gconstpointer (Match *)		The righthand Match object.
 */
gint
match_comp( gconstpointer a_, gconstpointer b_ )
{
	Match *a = (Match *) a_;
	Match *b = (Match *) b_;

	if( a->ld > b->ld )
		return( 1 );
	else if( a->ld < b->ld )
		return( -1 );
	else
		return( 0 );
}

/* Version of match_comp that can be used with g_list_sort on a GList of Match
 * pointers.
 *
 * @a_	gconstpointer (Match *)		The lefthand Match object.
 * @b_ 	gconstpointer (Match *)		The righthand Match object.
 */
gint
match_list_comp( gconstpointer a_, gconstpointer b_ )
{
	GList *a = (GList *) a_;
	GList *b = (GList *) b_;

	Match *match_a = (Match *) a->data;
	Match *match_b = (Match *) b->data;

	return( match_comp( match_a, match_b ) );
}

/* Get a GList of exact Match objects. Each Match object contains the start and
 * end index of an exactly matching substring, as well as the pointers to the
 * search pattern and to the text that was searched through. The @ld variable
 * of an exact Match is set to 0 and should not be used. The @exact variable of
 * an exact Match is TRUE.
 *
 * @text - NULL-terminated string; the target of the search
 *
 * @patt - NULL-terminated string; the pattern to search for
 *
 * @ignore_case:	TRUE if case is ignored.
 * 			FALSE if case is NOT ignored.
 *
 * Return a GList of exact Match objects, or NULL if no exact matches were
 * found.
 */
GList*
match_exact( const gchar *text, const gchar *patt, gboolean ignore_case )
{
	gchar *comp_patt, *comp_text, *s;
	GList *r;
	Match *ma;

	/* If @patt is null or the empty string, return a GList with one exact
	 * Match object for the full @text string.
	 */
	r = NULL;
	if( !patt || !*patt) {
		ma = match_new( TRUE, 0, 0, text, patt );
		r = g_list_append( r, ma );
		return( r );
	}

	/* Use these for comparison, which changes depending on @ignore_case.
	 * Note we only copy the string if we need to lowercase.
	 */
	comp_patt = ignore_case ? g_ascii_strdown( patt, -1 ) : (gchar *) patt;
	comp_text = ignore_case ? g_ascii_strdown( text, -1 ) : (gchar *) text;

	/* Find all exact matches
	 */
	s = comp_text;
	while( *s && (s = strstr( s, comp_patt )) ) {
		ma = match_new( TRUE, s - comp_text, 0, text, patt );
		r = g_list_append( r, ma );
		s += ma->n_patt;
	}

	/* If no lowercasing was required, don't free anything. 
	 */
	if( ignore_case ) {
		g_free( comp_patt );
		g_free( comp_text );
	}

	return( r );

}

/* Get a GList containing one inexact Match object. An inexact Match object
 * contains the LEVENSHTEIN DISTANCE between @pattern and @text in its @ld
 * variable, which can be used to sort matches from best to worst. The smaller
 * @ld is, the better the match. The parameters of the LEVENSHTEIN DISTANCE
 * algorithm are selected so that the inexactly matching substring can appear
 * anywhere in @text. The @i variable of an inexact match is set to 0 and
 * should not be used. The @exact variable of an inexact match is FALSE.
 *
 * @text - NULL-terminated string; the target of the search
 *
 * @patt - NULL-terminated string; the pattern to search for
 *
 * @ignore_case:	TRUE if case is ignored.
 * 			FALSE if case is NOT ignored.
 *
 * @v:			Buffer where column of LEVENSHTEIN DISTANCE values are
 * 			written during computation. Must be allocated
 * 			beforehand by the caller. Owned by the caller.
 */
GList*
match_inexact( const gchar *text, const gchar *patt, gboolean ignore_case,
		guint *v )
{
	gchar *comp_patt, *comp_text;
	GList *r;
	Match *ma;

	/* If @patt is null or the empty string return NULL, since the caller
	 * should have used the result of match_exact.
	 */
	if( !patt || !*patt)
		return( NULL );

	/* Use these for comparison, which changes depending on @ignore_case.
	 * Note we only copy the string if we need to lowercase.
	 */
	comp_patt = ignore_case ? g_ascii_strdown( patt, -1 ) : (gchar *) patt;
	comp_text = ignore_case ? g_ascii_strdown( text, -1 ) : (gchar *) text;

	/* Create a GList containing one inexact Match object.
	 */
	r = NULL;
	ma = match_new( FALSE, 0, 0, text, patt );
	ma->ld = glev( ma->n_patt, ma->patt, ma->n_text, ma->text, v, ignore_case );
	r = g_list_append( r, ma );

	/* If no lowercasing was required, don't free anything. 
	 */
	if( ignore_case ) {
		g_free( comp_patt );
		g_free( comp_text );
	}

	return( r );
}

/* Find exact or inexact matches of a pattern within a GList of text strings,
 * depending on the value of @exact. Return a GList of GList of Match objects,
 * so that exact and inexact matches can be processed in a uniform way. 
 *
 * @exact:	If TRUE, return a GList of exact Match objects. If FALSE, return
 * 		a GList containing one inexact Match object.
 *
 * @text_list:	List of text strings to search in
 *
 * @patt:	Search pattern
 *
 * @ignore_case:	TRUE if case is ignored. FALSE if case is NOT ignored.
 *
 * @v:	Buffer where column of LEVENSHTEIN DISTANCE values are
 * 	written during computation. Must be allocated
 * 	beforehand by the caller. Owned by the caller.
 *
 * Return a GList of GLists of Match objects. Each inner GList contains at
 * least one Match object. If @exact is TRUE, and there are no exact matches,
 * return NULL. If @exact is FALSE, there will always be matches.
 */
GList *
match_list( gboolean exact, GList *text_list, const gchar *patt,
	gboolean ignore_case, guint *v )
{
	GList *found, *t0, *t1;
	gchar *text;

	if( !text_list || !(t0 = g_list_first( text_list )) )
		return( NULL );

	/* Iterate over elements of @text_list, which have type gchar*,
	 * appending each GList of Match objects to @found, a GList of GLists.
	 */
	found = NULL;
	do {
		text = (gchar *) t0->data;
		t1 = exact ? match_exact( text, patt, ignore_case )
			: match_inexact( text, patt, ignore_case, v );
		if( t1 )
			found = g_list_append( found, t1 );
	} while ( (t0 = t0->next) );

	return( found );
}

/* Fully clean up a GList of GList of Match objects, like the one returned by
 * match_list.
 *
 * @l	A GList of GList of Match objects. Can be NULL.
 */
void 
match_list_free( GList *l )
{
	GList *t;

	t = l;
	while ( t ) {
		g_list_foreach( (GList *) t->data, match_free, NULL );
		g_list_free( (GList *) t->data );
		t = t->next;
	}
	g_list_free( l );
}
