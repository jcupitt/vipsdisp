#ifndef __MATCH_H
#define __MATCH_H 

/* A GList of pointers to this struct represent an exact or inexact match of a
 * pattern string @patt in a text string @text.
 *
 * Exact matches are GLists of Match objects with @exact=true, while inexact
 * matches are GLists of Match objects with @exact=false.
 *
 * An exact Match indicates the zero-indexed position of the match with @i. Its
 *  @ld value should not be used.
 *
 * An inexact Match indicates the LEVENSHTEIN DISTANCE between @text and @patt
 * with @ld. Its @i value should not be used.
 *
 * @exact:	Exact match (TRUE) or inexact (FALSE)
 * @i:		Offset from @text to @patt. Used for exact matching.
 * @ld:		Distance between @text and @patt. Used for inexact matching.
 * @text:	Text to search in
 * @patt:	Search pattern
 */
typedef struct Match Match;
struct Match {
	gboolean exact;
	int i;
	int ld;
	guint n_text;
	gchar *text;
	guint n_patt;
	gchar *patt;
};

Match * Match_new( gboolean exact, int i, int ld, gchar *text, gchar *patt );
void Match_free( gpointer match_, gpointer user_data );
void Match_print( gpointer match_, gpointer user_data );
gint Match_comp( gconstpointer a_, gconstpointer b_ );
gint Match_list_comp( gconstpointer a_, gconstpointer b_ );
GList* Match_fuzzy_list( char *text, char *patt, gboolean ignore_case, guint *v );
GList * Match_substr( GList *text_list, gchar *patt, gboolean ignore_case, guint *v );
guint glev( guint n1, gchar s1[n1], guint n2, gchar s2[n2], guint v[n1 + 1], gboolean ignore_case );

#endif /* __MATCH_H */
