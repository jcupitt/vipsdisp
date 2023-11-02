#ifndef __MATCH_H
#define __MATCH_H 

/*
 * @exact:	Exact match (TRUE) or inexact (FALSE)
 * @i:		Offset from @text to @patt.
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

GList* Match_fuzzy_list( char *text, char *patt );

void Match_substr_cb( gpointer data, gpointer user_data );

GList * Match_substr( GList *text_list, gchar *patt );

#endif /* __MATCH_H */
