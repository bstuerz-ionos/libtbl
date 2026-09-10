#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <dirent.h>
#include <locale.h>
#include <libgen.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <fcntl.h>
#include <err.h>
#include "libtbl.h"
#include "bytes.h"

static bool sflag = false, rflag = false;
static enum tbl_format fmt = TBLF_TABLE;

struct file {
	struct file_name {
		char	*prefix;	/* tree drawing, "" for the root */
		char	*name;
	} name;
	struct stat	 st;
};

static void
stringify_name (FILE *file, const void *ptr, int flags)
{
	const struct file_name	*name = ptr;

	switch (flags) {
	case TBLS_HUMAN:
		fputs (name->prefix, file);
		/* fallthrough */
	case TBLS_RAW:
		fputs (name->name, file);
		break;
	case TBLS_JSON:
		tbl_write_json (file, name->name);
		break;
	}
}

static const struct tbl_type_ops ops_name = {
	.to_deflags	= TBLC_ALEFT,
	.to_stringify	= stringify_name,
};

static void
stringify_mode (FILE *file, const void *ptr, int flags)
{
	const mode_t *mode = ptr;
	(void)flags;
	fprintf (file, "%o", (unsigned)*mode);
}

static int
colorize_mode (const void *ptr)
{
	const mode_t *mode = ptr;

	if (S_ISDIR (*mode)) {
		return TBL_COLOR_GREEN;
	} else if (S_ISREG (*mode)) {
		return TBL_COLOR_BLACK | TBL_COLOR_INTENSE;
	} else {
		return TBL_COLOR_RESET;
	}
}

static const struct tbl_type_ops ops_mode = {
	.to_deflags	= TBLC_ARIGHT,
	.to_colorize	= colorize_mode,
	.to_stringify	= stringify_mode,
};

#define COLUMNS_FILE(X)													\
	X(file, name,		"Name",		"File name",			TBLC_DEFAULT, &ops_name)		\
	X(file, st.st_ino,	"Inode",	"Inode number",			TBLC_DEFAULT, NULL)			\
	X(file, st.st_nlink,	"Links",	"Number of hard links",		TBLC_DEFAULT, NULL)			\
	X(file, st.st_mode,	"Mode",		"Type and Permissions",		TBLC_DEFAULT, &ops_mode)		\
	X(file, st.st_uid,	"UID",		"User ID",			TBLC_DEFAULT, NULL)			\
	X(file, st.st_gid,	"GID",		"Group ID",			TBLC_DEFAULT, NULL)			\
	X(file, st.st_size,	"Size",		"File size",			TBLC_SUM | TBLC_ARIGHT, &ops_bytes)	\
	X(file, st.st_blocks,	"Blocks",	"Number of filesystem blocks",	TBLC_SUM | TBLC_ARIGHT, NULL)		\

TBL_DEFINE_STATIC(file, COLUMNS_FILE);

struct build {
	struct file	*files;
	size_t		 len, cap;
};

static void
build_add (struct build *b, const char *prefix, const char *name,
    const struct stat *st)
{
	if (b->len == b->cap) {
		b->cap *= 2;
		b->files = reallocarray (b->files, b->cap + 1, sizeof (struct file));
		if (b->files == NULL)
			err (1, "reallocarray");
	}

	b->files[b->len].name.prefix = strdup (prefix);
	b->files[b->len].name.name = strdup (name);
	b->files[b->len].st = *st;
	++b->len;
}

static void
build_free (struct build *b)
{
	size_t i;

	for (i = 0; i < b->len; ++i) {
		free (b->files[i].name.prefix);
		free (b->files[i].name.name);
	}
	free (b->files);
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

struct ent {
	char		*name;
	struct stat	 st;
};

static int
entcmp (const void *a, const void *b)
{
	const struct ent *x = a, *y = b;

	return strcmp (x->name, y->name);
}

/* Collect the visible entries of @dir (open as @fd) into a sorted array,	*/
/* storing the count in @cntp. Entries that cannot be statted are dropped, so	*/
/* that the caller knows up front which entry ends the list.			*/
static struct ent *
readents (int fd, DIR *dir, size_t *cntp)
{
	struct ent	*v = NULL;
	struct dirent	*de;
	size_t		 len = 0, cap = 0;

	while ((de = readdir (dir)) != NULL) {
		if (de->d_name[0] == '.')
			continue;

		if (len == cap) {
			cap = cap != 0 ? cap * 2 : 16;
			v = reallocarray (v, cap, sizeof (struct ent));
			if (v == NULL)
				err (1, "reallocarray");
		}

		/* stat without opening: never blocks on a fifo, and	*/
		/* works for entries we may not open ourselves		*/
		if (fstatat (fd, de->d_name, &v[len].st, AT_SYMLINK_NOFOLLOW) != 0)
			continue;

		v[len].name = strdup (de->d_name);
		++len;
	}

	qsort (v, len, sizeof (struct ent), entcmp);
	*cntp = len;
	return v;
}

/* @prefix holds the stems of the ancestors of @name and is @len bytes long;	*/
/* @last tells whether @name is the final entry of its parent, and is ignored	*/
/* for the root (@depth == 0). @path names the entry relative to @dirfd.		*/
static void
traverse (struct build *b, int dirfd, const char *path, const char *name,
    const struct stat *st, char *prefix, size_t len, size_t cap,
    int depth, int last)
{
	struct ent	*v;
	DIR		*dir;
	size_t		 i, cnt, n;
	int		 fd;

	/* the entry's own line is drawn with a branch glyph ... */
	if (depth > 0)
		append (prefix, len, cap, last ? TREE_ELBOW : TREE_TEE);
	build_add (b, prefix, name, st);
	prefix[len] = '\0';

	if (!S_ISDIR (st->st_mode))
		return;

	fd = openat (dirfd, path, O_RDONLY | O_DIRECTORY);
	if (fd < 0) {
		warn ("open('%s')", name);
		return;
	}

	dir = fdopendir (fd);
	if (dir == NULL) {
		close (fd);
		return;
	}

	v = readents (fd, dir, &cnt);

	/* ... which turns into a stem for its descendants */
	n = len;
	if (depth > 0)
		n = append (prefix, len, cap, last ? TREE_BLANK : TREE_PIPE);

	for (i = 0; i < cnt; ++i) {
		traverse (b, fd, v[i].name, v[i].name, &v[i].st, prefix, n, cap,
		    depth + 1, i + 1 == cnt);
		free (v[i].name);
	}
	prefix[len] = '\0';

	free (v);
	closedir (dir);
}

static int
tree (const char *path)
{
	struct build	 build;
	struct stat	 st;
	char		*bn;
	char		 prefix[512];
	size_t		*csel = NULL;

	/* the root is followed if it is a symlink, its entries are not */
	if (stat (path, &st) != 0) {
		warn ("stat('%s')", path);
		return -1;
	}

	build.len = 0;
	build.cap = 16;
	build.files = calloc (build.cap + 1, sizeof (struct file));

	bn = strdup (path);
	prefix[0] = '\0';
	traverse (&build, AT_FDCWD, path, basename (bn), &st, prefix, 0,
	    sizeof prefix, 0, 1);
	free (bn);

	if (sflag) {
		csel = calloc (TBL_NCOLS (file), sizeof (size_t));
		csel[0] = tbl_coli (file, name);
		csel[1] = TBL_CNUL;
	}

	tbl_print (stdout, file, build.files, build.len, csel, rflag ? TBL_RAW : TBL_DEFAULT, fmt);

	free (csel);
	build_free (&build);
	return 0;
}

int
main (int argc, char *argv[])
{
	int i, option, ret = 0;

	while ((option = getopt (argc, argv, "csdjrx")) != -1) {
		switch (option) {
		case 's':
			sflag = true;
			break;
		case 'r':
			rflag = true;
			break;
		case 'd':
			tbl_print_descr (stdout, file, NULL, TBL_DEFAULT);
			return 0;
		case 'c':
			fmt = TBLF_CSV;
			break;
		case 'j':
			fmt = TBLF_JSON;
			break;
		case 'x':
			fmt = TBLF_XML;
			break;
		case '-':
			break;
		default:
			return 1;
		}
	}

	argc -= optind;
	argv += optind;

	setlocale (LC_CTYPE, "");

	if (argc == 0)
		return tree (".") == 0 ? 0 : 1;

	for (i = 0; i < argc; ++i)
		if (tree (argv[i]) != 0)
			ret = 1;

	return ret;
}
