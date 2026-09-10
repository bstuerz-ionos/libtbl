#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "libtbl.h"

#define newa(n, T) ((T *)calloc ((n), sizeof (T)))
#define new(T) (newa (1, T))

struct tree {
	struct tree	**sub;
	int		 val;
};

struct column {
	struct column_data {
		char	*prefix;
		int	 val;
	} d;
};

static void
stringify_d (FILE *file, const void *ptr, int sflags)
{
	const struct column_data *d = ptr;

	(void)sflags;

	fputs (d->prefix, file);
	fprintf (file, "%d", d->val);
}

static const struct tbl_type_ops ops_d = {
	.to_deflags	= 0,
	.to_stringify	= stringify_d,
};

#define COLUMNS_COLUMN(X)					\
	X(column, d,	"Tree", "", TBLC_DEFAULT, &ops_d)	\

TBL_DEFINE (column, COLUMNS_COLUMN);

static int
sum (const struct tree *t)
{
	struct tree	**c;
	int		  s;

	s = t->val;
	if (t->sub != NULL) {
		for (c = t->sub; *c != NULL; ++c)
			s += sum (*c);
	}
	return s;
}

static size_t
tsize (const struct tree *t)
{
	struct tree	**c;
	size_t		  n;

	if (t->sub == NULL)
		return 1;

	for (n = 0, c = t->sub; *c != NULL; ++c)
		n += tsize (*c);
	return n + 1;
}

#define TREE_TEE		"\u251C\u2500\u2500 "	/* |-- */
#define TREE_ELBOW		"\u2514\u2500\u2500 "	/* `-- */
#define TREE_PIPE		"\u2502   "		/* |   */
#define TREE_BLANK		"    "

/* Append @s to @prefix (@len bytes long), if it fits into @cap bytes	*/
/* including the terminator. Returns the new length.			*/
static size_t
append (char *prefix, size_t len, size_t cap, const char *s)
{
	size_t n;

	n = strlen (s);
	if (len + n + 1 > cap)
		return len;
	memcpy (prefix + len, s, n + 1);
	return len + n;
}

/* @prefix holds the stems of @t's ancestors and is @len bytes long;	*/
/* @last tells whether @t is the final child of its parent, and is	*/
/* ignored for the root (@depth == 0).					*/
static void
build (const struct tree *t, struct column **c, char *prefix, size_t len,
    size_t cap, int depth, int last)
{
	struct tree	**s;
	size_t		  n;

	/* the node's own line is drawn with a branch glyph ... */
	n = len;
	if (depth > 0)
		n = append (prefix, n, cap, last ? TREE_ELBOW : TREE_TEE);

	(*c)->d.val = t->val;
	(*c)->d.prefix = strdup (prefix);
	++*c;

	if (t->sub != NULL) {
		/* ... which turns into a stem for its descendants */
		n = len;
		if (depth > 0)
			n = append (prefix, n, cap, last ? TREE_BLANK : TREE_PIPE);
		for (s = t->sub; *s != NULL; ++s)
			build (*s, c, prefix, n, cap, depth + 1, s[1] == NULL);
	}
	prefix[len] = '\0';
}

static void
flatten (const struct tree *t)
{
	struct column	*c, *o;
	size_t		 n;
	char		 prefix[128];

	n = tsize (t);
	o = c = newa (n, struct column);
	prefix[0] = '\0';
	build (t, &c, prefix, 0, sizeof prefix, 0, 1);

	tbl_print_table (stdout, column, o, n, NULL, TBL_DEFAULT);
}

int
main (void)
{
	struct tree *root;

	setlocale (LC_CTYPE, "");

	root = new (struct tree);
	root->val = 1;
	root->sub = newa (4, struct tree *);
	root->sub[0] = new (struct tree);
	root->sub[0]->val = 2;
	root->sub[1] = new (struct tree);
	root->sub[1]->val = 3;
	root->sub[1]->sub = newa (3, struct tree *);
	root->sub[1]->sub[0] = new (struct tree);
	root->sub[1]->sub[0]->val = 4;
	root->sub[1]->sub[1] = new (struct tree);
	root->sub[1]->sub[1]->val = 5;
	root->sub[1]->sub[1]->sub = newa (2, struct tree *);
	root->sub[1]->sub[1]->sub[0] = new (struct tree);
	root->sub[1]->sub[1]->sub[0]->val = 6;
	root->sub[1]->sub[1]->sub[1] = NULL;
	root->sub[1]->sub[2] = NULL;
	root->sub[2] = new (struct tree);
	root->sub[2]->val = 7;
	root->sub[3] = NULL;

	printf ("sum: %d\n", sum (root));
	flatten (root);
}
