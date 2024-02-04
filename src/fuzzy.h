#ifndef __FUZZY_H
#define __FUZZY_H

typedef struct _Fuzzy {
	const char *field;
	guint distance;
} Fuzzy;

GSList *fuzzy_match(char **fields, const char *pattern);

#endif /* __FUZZY_H */
