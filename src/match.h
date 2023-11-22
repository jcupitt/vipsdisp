#ifndef __MATCH_H
#define __MATCH_H 

/* A Match object represents an exact or inexact match of a pattern string @patt
 * in a text string @text. Using the same struct for both exact and inexact
 * matches allows the caller to process a GList of Match objects in a uniform
 * way. This is convenient when writing GFunc functions used with
 * g_list_foreach.
 *
 * Exact Match objects have @exact=true, while inexact Match objects have
 * @exact=false.
 *
 * An exact Match indicates the zero-indexed position of the match with @i. Its
 * @ld value is set to 0 and should not be used.
 *
 * An inexact Match indicates the LEVENSHTEIN DISTANCE between @text and @patt
 * with @ld. Its @i value is set to 0 and should not be used. The parameters of
 * the LEVENSHTEIN_DISTANCE algorithm are chosen so that an inexact match of
 * @patt can occur anywhere in @text.
 *
 * @exact:	Exact match (TRUE) or inexact (FALSE).
 * @i:		Offset from @text to @patt. Used for exact matching.
 * @ld:		Distance between @text and @patt. Used for inexact matching.
 * @n_text	String length of @text.
 * @text:	Text to search in.
 * @n_patt	String length of @patt.
 * @patt:	Search pattern.
 */
typedef struct Match Match;
struct Match {
	gboolean exact;
	gint i;
	gint ld;
	guint n_text;
	const gchar *text;
	guint n_patt;
	const gchar *patt;
};

Match * match_new( gboolean exact, gint i, gint ld, const gchar *text, const gchar *patt );
void match_free( gpointer match_, gpointer user_data );
guint glev( guint n1, const gchar s1[n1], guint n2, const gchar s2[n2], guint v[n1 + 1], gboolean ignore_case );
gint match_comp( gconstpointer a_, gconstpointer b_ );
gint match_list_comp( gconstpointer a_, gconstpointer b_ );
GList* match_exact( const gchar *text, const gchar *patt, gboolean ignore_case );
GList* match_inexact( const gchar *text, const gchar *patt, gboolean ignore_case, guint *v );
GList * match_list( gboolean exact, GList *text_list, const gchar *patt, gboolean ignore_case, guint *v );
void match_list_free( GList *l );

#endif /* __MATCH_H */
