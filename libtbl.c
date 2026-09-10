#define _DEFAULT_SOURCE
#define _GNU_SOURCE
#define _XOPEN_SOURCE 800

/* use alloca() to allocate on the stack for (small) temporary allocations */
#ifndef USE_ALLOCA
# define USE_ALLOCA 1
#endif

/* enable compatibility with libtbl4 */
#ifndef TBL4_COMPAT
# define TBL4_COMPAT 1
#endif

#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <locale.h>
#include <stdlib.h>
#include <stdint.h>
#include <wctype.h>
#include <wchar.h>
#include "libtbl.h"

#if USE_ALLOCA
# include <alloca.h>
# define lalloc(n) (memset (alloca ((n)), 0, (n)))
# define lcalloc(s, n) (lalloc ((s) * (n)))
# define lfree(p) ((void)p)
#else
# define lalloc(n) (calloc (1, (n)))
# define lcalloc(s, n) (calloc ((s), (n)))
# define lfree(p) (free ((p)))
#endif

/* DEFAULT TYPE OPS */

#define DEFINE_INT(name, type, fmt)				\
static void							\
stringify_##name (FILE *file, const void *ptr, int flags)	\
{								\
	(void)flags;						\
	fprintf (file, fmt, *(const type *)ptr);		\
}								\
static void							\
add_##name (void *sum, const void *right)			\
{								\
	*(type *)sum += *(const type *)right;			\
}								\
const struct tbl_type_ops _tbl_ops_##name = {			\
	.to_deflags	= TBLC_ARIGHT,				\
	.to_stringify	= stringify_##name,			\
	.to_add		= add_##name,				\
}

DEFINE_INT (short,	short,			"%hd");
DEFINE_INT (int,	int,			"%d");
DEFINE_INT (long,	long,			"%ld");
DEFINE_INT (llong,	long long,		"%lld");

DEFINE_INT (ushort,	unsigned short,		"%hu");
DEFINE_INT (uint,	unsigned int,		"%u");
DEFINE_INT (ulong,	unsigned long,		"%lu");
DEFINE_INT (ullong,	unsigned long long,	"%llu");

static int
tbl_strwidth (const char *s)
{
	const char	*locale;
	mbstate_t	 ss;
	wchar_t		 w;
	int		 vlen = 0, n;

	locale = setlocale (LC_CTYPE, NULL);
	if (strcmp (locale, "POSIX") == 0 || strcmp (locale, "C") == 0)
		return strlen (s);

	memset (&ss, 0, sizeof (ss));
	while ((n = mbrtowc (&w, s, MB_CUR_MAX, &ss)) > 0) {
		vlen += wcwidth (w);
		s += n;
	}

	return vlen;
}

int
tbl_write_json (FILE *file, const char *s)
{
	static const wchar_t	 from[] = L"\"\\'\b\f\n\r\t";
	static const wchar_t	 to[]	= L"\"\\'bfnrt";
	const wchar_t		*esc;
	size_t			 n, len;
	mbstate_t		 ss;
	wchar_t			 w;

	fputc ('"', file);
	memset (&ss, 0, sizeof (ss));
	for (len = 1; (n = mbrtowc (&w, s, MB_CUR_MAX, &ss)) > 0; s += n) {
		esc = w != L'\0' ? wcschr (from, w) : NULL;
		if (esc != NULL) {
			len += fprintf (file, "\\%lc", to[esc - from]);
		} else if (iswprint (w)) {
			fprintf (file, "%lc", w);
			len += wcwidth (w);
		} else {
			len += fprintf (file, "\\u%04x", (unsigned)w);
		}
	}
	fputc ('"', file);
	return len + 1;
}

void
tbl_stringify_string (FILE *file, const void *ptr, int flags)
{
	const char *s = *(const char *const *)ptr;
	
	switch (flags) {
	case TBLS_RAW:
	case TBLS_HUMAN:
		(void)fputs (s, file);
		break;
	case TBLS_JSON:
		tbl_write_json (file, s);
		break;
	}
}

const struct tbl_type_ops _tbl_ops_string = {
	.to_deflags	= TBLC_ALEFT,
	.to_stringify	= tbl_stringify_string,
};

/* Helper functions */

size_t
tbl_coli_ (const struct tbl_td *td, const char *name, size_t namelen)
{
	const struct tbl_cd	*cd;
	size_t			 coli;

	for (coli = 0; coli < td->td_ncol; ++coli) {
		cd = &td->td_cols[coli];
		if (namelen != strlen (cd->cd_name))
			continue;
		if (memcmp (name, cd->cd_name, namelen) == 0)
			return coli;
	}
	return TBL_CNUL;
}

static size_t *
csel_find (const size_t *csel, size_t cs)
{
	for (; *csel != TBL_CNUL && *csel != cs; ++csel);
	return (size_t *)csel;
}

int
tbl_select_cols_ (
	const struct tbl_td	*td,
	const char		*names,
	char			 delim,
	size_t			*csel
) {
	const char	del[] = { delim, '\0' };
	size_t	 	coli, cs, *pcs, n;
	bool	 	rm;

	if (names == NULL) {
		for (coli = 0; coli < td->td_ncol; ++coli)
			csel[coli] = coli;
		csel[coli] = TBL_CNUL;
		return 0;
	}

	if ((rm = *names == '-') || *names == '+') {
		++names;
	} else {
		*csel = TBL_CNUL;
	}

	for (; *names != '\0'; names += n + (names[n] == delim)) {
		n = strcspn (names, del);
		cs = tbl_coli_ (td, names, n);
		if (cs == TBL_CNUL)
			return -1;

		pcs = csel_find (csel, cs);
		if (rm) {
			for (; *pcs != TBL_CNUL; ++pcs)
				*pcs = pcs[1];
		} else if (*pcs == TBL_CNUL) {
			pcs[0] = cs;
			pcs[1] = TBL_CNUL;
		}
	}
	return 0;
}

#define DASH "─"
#define xcd_flags(cd) ((cd)->cd_flags != TBLC_DEFAULT ? (cd)->cd_flags : (cd)->cd_ops->to_deflags)
#define ccount(csel) ((size_t)(csel_find ((csel), TBL_CNUL) - (csel)))
#define td_ccount(td, csel)	((csel) != NULL ? ccount ((csel)) : (td)->td_ncol)
#define td_cd(td, csel, coli)	((td)->td_cols[(csel) != NULL ? (csel)[(coli)] : (coli)])
#define getrow(rows, rowi, td)	((const void *)((uintptr_t)(rows) + (rowi) * (td)->td_size))
#define getcol(row, cd)		((const void *)((uintptr_t)(row) + (cd)->cd_offset))
#define tbl_foreach_col(td, csel, ncols, coli, cd)	\
	for (coli = 0; coli < (ncols) && ((cd) = &td_cd ((td), (csel), coli), 1); ++coli)
#define tbl_foreach_colf(td, csel, ncols, coli, cd, flags)	\
	for (coli = 0; coli < (ncols) && ((cd) = &td_cd ((td), (csel), coli), (flags) = xcd_flags (cd),  1); ++coli)
#define tbl_foreach_colc(td, csel, ncols, coli, row, col, cd)	\
	for (coli = 0; coli < (ncols) && ((cd) = &td_cd ((td), (csel), coli), (col) = getcol ((row), (cd)), 1); ++coli)
#define tbl_foreach_row(td, row, rowi, rows, nrows)		\
	for (rowi = 0; rowi < (nrows) && (row = getrow ((rows), rowi, (td)), 1); ++rowi)
#define cbold(out, tflags) ((tflags) & TBL_NOCOLOR ? 0 : fputs ("\033[1m", (out)))
#define creset(out, tflags) ((tflags) & TBL_NOCOLOR ? 0 : fputs ("\033[0m", (out)))

/* JSON implementation */

int
tbl_print_json_ (
	FILE			*out,
	const struct tbl_td	*td,
	const void		*rows,
	size_t			 nrows,
	const size_t		*csel
) {
	const struct tbl_cd	*cd;
	const void		*row, *col;
	size_t			 rowi, coli, ncols;
	int			 sflags;

	sflags = TBLS_JSON;
	ncols = td_ccount (td, csel);

	fputs ("{\n", out);
	fputs ("\t\"rows\": [", out);

	tbl_foreach_row (td, row, rowi, rows, nrows) {
		fprintf (out, "%s\n\t\t{", rowi != 0 ? "," : "");

		tbl_foreach_colc (td, csel, ncols, coli, row, col, cd) {
			fprintf (out, "%s\n\t\t\t\"%s\": ", coli != 0 ? "," : "", cd->cd_name);
			cd->cd_ops->to_stringify (out, col, sflags);
		}

		fputs ("\n\t\t}", out);
	}

	fputs ("\n\t]\n}\n", out);
	return 0;
}

/* CSV implementation */

int
tbl_print_csv_ (
	FILE			*out,
	const struct tbl_td	*td,
	const void		*rows,
	size_t			 nrows,
	const size_t		*csel,
	char			 sep
) {
	const struct tbl_cd	*cd;
	const void		*row, *col;
	size_t			 rowi, coli, ncols;
	int			 sflags;

#if TBL4_COMPAT
	sflags = TBLS_JSON;
#else
	sflags = TBLS_RAW;
#endif

	ncols = td_ccount (td, csel);

	/* print header */
	tbl_foreach_col (td, csel, ncols, coli, cd) {
		if (coli != 0)
			fputc (sep, out);
		fputs (cd->cd_name, out);
	}
	fputc ('\n', out);

	/* print rows */
	tbl_foreach_row (td, row, rowi, rows, nrows) {
		tbl_foreach_colc (td, csel, ncols, coli, row, col, cd) {
			if (coli != 0)
				fputc (sep, out);
			cd->cd_ops->to_stringify (out, col, sflags);
		}
		fputc ('\n', out);
	}

	return 0;
}

/* XML implementation */

int
tbl_print_xml_ (
	FILE			*out,
	const struct tbl_td	*td,
	const void		*rows,
	size_t			 nrows,
	const size_t		*csel
) {
	const struct tbl_cd	*cd;
	const void		*row, *col;
	const char		*tag;
	size_t			 rowi, coli, ncols;
	int			 sflags;

#if TBL4_COMPAT
	sflags = TBLS_JSON;
	tag = "columns";
#else
	sflags = TBLS_RAW;
	tag = td->td_name;
#endif

	ncols = td_ccount (td, csel);

	fputs ("<rows>\n", out);
	tbl_foreach_row (td, row, rowi, rows, nrows) {
		fprintf (out, "\t<%s>\n", tag);

		tbl_foreach_colc (td, csel, ncols, coli, row, col, cd) {
			fprintf (out, "\t\t<%s>", cd->cd_name);
			cd->cd_ops->to_stringify (out, col, sflags);
			fprintf (out, "</%s>\n", cd->cd_name);
		}
		fprintf (out, "\t</%s>\n", tag);
	}
	fputs ("</rows>\n", out);
	return 0;
}

/* TABLE implementation */

static void
frepeat (FILE *file, const char *s, int n)
{
	while (n-- > 0)
		fputs (s, file);
}

static void
do_colorize (FILE *out, const struct tbl_cd *cd, const void *col)
{
	int c;

	if (cd->cd_ops->to_colorize != NULL) {
		c = cd->cd_ops->to_colorize (col);
		fputs ("\033[", out);
		if (c & TBL_COLOR_BOLD)
			fputs ("1;", out);
		if (c & TBL_COLOR_INTENSE)
			c += 60;
		fprintf (out, "%dm", c & 0xff);
	}
}

static int
tflags2sflags (int tflags)
{
	return tflags & TBL_RAW ? TBLS_RAW : TBLS_HUMAN;
}

struct cookie {
	FILE		*file;
	size_t		 vis;
	bool		 uni;
	mbstate_t	 ss;
};

static ssize_t
cookie_write (void *ptr, const char *buf, size_t num)
{
	struct cookie	*cookie = ptr;
	ssize_t		 n;
	wchar_t		 w;
	
	if (cookie->uni) {
		for (; (n = mbrtowc (&w, buf, num, &cookie->ss)) > 0; buf += n, num -= n)
			cookie->vis += wcwidth (w);
	} else {
		cookie->vis += num;
	}
	return num;
}

static size_t
cookie_stringify (struct cookie *cookie, const struct tbl_cd *cd, const void *ptr, int sflags)
{
	cookie->vis = 0;
	memset (&cookie->ss, 0, sizeof (mbstate_t));
	cd->cd_ops->to_stringify (cookie->file, ptr, sflags);
	return cookie->vis;
}

static FILE *
open_wrapped (struct cookie *cookie)
{
	cookie_io_functions_t	 funcs;
	const char		*locale;
	FILE			*file;

	funcs.read	= NULL;
	funcs.write	= cookie_write;
	funcs.seek	= NULL;
	funcs.close	= NULL;

	locale = setlocale (LC_CTYPE, NULL);
	cookie->uni = strcmp (locale, "POSIX") != 0 && strcmp (locale, "C") != 0;

	file = fopencookie (cookie, "w", funcs);
	/* avoid buffering */
	if (file != NULL)
		setvbuf (file, NULL, _IONBF, 0);
	return file;
}

static void
print_column_ (FILE *out, struct cookie *cookie, const struct tbl_cd *cd, const void *col, int csize, int tflags, int sflags)
{
	int n, flags;

	flags = xcd_flags (cd);

	n = cookie_stringify (cookie, cd, col, sflags);
	assert (n <= csize);
	if (flags & TBLC_ARIGHT)
		frepeat (out, " ", csize - n);

	if (!(tflags & TBL_NOCOLOR))
		do_colorize (out, cd, col);
	cd->cd_ops->to_stringify (out, col, sflags);
	creset (out, tflags);

	if (!(flags & TBLC_ARIGHT))
		frepeat (out, " ", csize - n);
}

int
tbl_print_table_ (
	FILE			*out,
	const struct tbl_td	*td,
	const void		*rows,
	size_t			 nrows,
	const size_t		*csel,
	int			 tflags
) {
	const struct tbl_cd	*cd;
	const void		*row, *col;
	struct cookie		 cookie;
	void			*sum = NULL;
	short			*csizes;
	int			 linelen, ret = -1, n, flags, sflags;
	bool			 hassum = false;
	size_t			 rowi, coli, ncols, maxssize;

	cookie.file = open_wrapped (&cookie);
	if (cookie.file == NULL)
		return -1;

	sflags = tflags2sflags (tflags);
	ncols = td_ccount (td, csel);
	csizes = lcalloc (ncols, sizeof (short));
	if (csizes == NULL)
		goto close_null;

	/* calculate header sizes */
	maxssize = 0;
	tbl_foreach_colf (td, csel, ncols, coli, cd, flags) {
		csizes[coli] = tbl_strwidth (cd->cd_header);

		/* calculate the minimum space necessary to allocate for sum */
		if (flags & TBLC_SUM) {
			hassum = true;
			if (cd->cd_size > maxssize)
				maxssize = cd->cd_size;
		}
	}

	/* calculate the width of the sums */
	hassum &= !(tflags & TBL_NOSUM);
	if (hassum) {
		sum = lalloc (maxssize);
		if (sum == NULL) {
			hassum = false;
			goto nosum;
		}

		tbl_foreach_colf (td, csel, ncols, coli, cd, flags) {
			if (!(flags & TBLC_SUM))
				continue;

			memset (sum, 0, cd->cd_size);
			tbl_foreach_row (td, row, rowi, rows, nrows) {
				col = getcol (row, cd);
				cd->cd_ops->to_add (sum, col);
			}

			n = cookie_stringify (&cookie, cd, sum, sflags);
			if (n > csizes[coli])
				csizes[coli] = n;
		}
	}

nosum:
	/* calculate column sizes */
	tbl_foreach_row (td, row, rowi, rows, nrows) {
		tbl_foreach_colc (td, csel, ncols, coli, row, col, cd) {
			n = cookie_stringify (&cookie, cd, col, sflags);
			if (n > csizes[coli])
				csizes[coli] = n;
		}
	}

	/* print header */
	if (tflags & TBL_NOHEADER)
		goto skip_header;

	cbold (out, tflags);

	linelen = 0;
	tbl_foreach_colf (td, csel, ncols, coli, cd, flags) {
		if (coli != 0) {
			fputc (' ', out);
			++linelen;
		}
		linelen += fprintf (out, flags & TBLC_ARIGHT ? "%*s" : "%-*s", csizes[coli], cd->cd_header);
	}
	creset (out, tflags);
	fputc ('\n', out);
	frepeat (out, DASH, linelen);
	fputc ('\n', out);

skip_header:
	/* print rows */
	tbl_foreach_row (td, row, rowi, rows, nrows) {
		tbl_foreach_colc (td, csel, ncols, coli, row, col, cd) {
			if (coli != 0)
				fputc (' ', out);

			print_column_ (out, &cookie, cd, col, csizes[coli], tflags, sflags);
		}
		fputc ('\n', out);
	}

	if (!hassum)
		goto done;

	/* print sum headers */
	tbl_foreach_colf (td, csel, ncols, coli, cd, flags) {
		if (coli != 0)
			fputc (' ', out);
		frepeat (out, flags & TBLC_SUM ? DASH : " ", csizes[coli]);
	}
	fputc ('\n', out);

	/* print sums */
	tbl_foreach_colf (td, csel, ncols, coli, cd, flags) {
		if (coli != 0)
			fputc (' ', out);
		if (!(flags & TBLC_SUM)) {
		skip:
			frepeat (out, " ", csizes[coli]);
			continue;
		}

		if (sum == NULL)
			goto skip;

		memset (sum, 0, cd->cd_size);
		tbl_foreach_row (td, row, rowi, rows, nrows) {
			col = getcol (row, cd);
			cd->cd_ops->to_add (sum, col);
		}

		print_column_ (out, &cookie, cd, sum, csizes[coli], tflags, sflags);
	}
	fputc ('\n', out);
	lfree (sum);

done:
	ret = 0;
	lfree (csizes);

close_null:
	fclose (cookie.file);
	return ret;
}

int
tbl_print_elem_ (
	FILE			*out,
	const struct tbl_td	*td,
	const void		*row,
	const size_t		*csel,
	int			 tflags
) {
	const struct tbl_cd	*cd;
	const void		*col;
	size_t			 coli, ncols;
	int			 n, sflags, hsize = 0;

	sflags = tflags2sflags (tflags);
	ncols = td_ccount (td, csel);

	/* calculate header sizes */
	tbl_foreach_col (td, csel, ncols, coli, cd) {
		n = tbl_strwidth (cd->cd_header);
		if (n > hsize)
			hsize = n;
	}

	/* print columns */
	tbl_foreach_colc (td, csel, ncols, coli, row, col, cd) {
		cbold (out, tflags);
		fprintf (out, "%-*s ", hsize, cd->cd_header);

		if (!(tflags & TBL_NOCOLOR)) {
			creset (out, tflags);
			do_colorize (out, cd, col);
		}

		cd->cd_ops->to_stringify (out, col, sflags);
		creset (out, tflags);
		fputc ('\n', out);
	}
	return 0;
}

int tbl_print_ (
	FILE			*out,
	const struct tbl_td	*td,
	const void		*rows,
	size_t			 nrows,
	const size_t		*csel,
	int			 tflags,
	enum tbl_format		 fmt
) {
	const void	*row;
	size_t		 rowi;

	switch (fmt) {
	case TBLF_TABLE:
		return tbl_print_table_ (out, td, rows, nrows, csel, tflags);
	case TBLF_ELEMENT:
		tbl_foreach_row (td, row, rowi, rows, nrows)
			if (tbl_print_elem_ (out, td, row, csel, tflags) != 0)
				return -1;
		return 0;
	case TBLF_JSON:
		return tbl_print_json_ (out, td, rows, nrows, csel);
	case TBLF_CSV:
		return tbl_print_csv_ (out, td, rows, nrows, csel, ',');
	case TBLF_XML:
		return tbl_print_xml_ (out, td, rows, nrows, csel);
	}
	abort ();
}

static void
stringify_flags (FILE *file, const void *ptr, int sflags)
{
	int flags;

	flags = *(const int *)ptr;

	if (sflags != TBLS_HUMAN) {
		fprintf (file, "0x%02x", flags);
		return;
	}
	if (flags == TBLC_DEFAULT) {
		fprintf (file, "default");
		return;
	}
	fprintf (file, "%s", flags & TBLC_ARIGHT ? "aright" : "aleft");
	if (flags & TBLC_SUM)
		fprintf (file, " sum");
}

static int
colorize_flags (const void *ptr)
{
	const int *flags = ptr;
	return *flags == TBLC_DEFAULT ? TBL_COLOR_BLACK | TBL_COLOR_INTENSE : TBL_COLOR_RESET;
}

static const struct tbl_type_ops ops_flags = {
	.to_deflags	= TBLC_ALEFT,
	.to_stringify	= stringify_flags,
	.to_colorize	= colorize_flags,
	.to_add		= NULL,
};

#define TBL_CD_COLUMNS(X)							\
	X(tbl_cd, cd_name,	"Name",		"Field name",		TBLC_DEFAULT, NULL)		\
	X(tbl_cd, cd_header,	"Header",	"Description",		TBLC_DEFAULT, NULL)		\
	X(tbl_cd, cd_size,	"Size",		"Size of the field",	TBLC_SUM | TBLC_ARIGHT, NULL)	\
	X(tbl_cd, cd_offset,	"Offset",	"Offset in the struct",	TBLC_DEFAULT, NULL)		\
	X(tbl_cd, cd_flags,	"Flags",	"Printing flags",	TBLC_DEFAULT, &ops_flags)	\
	X(tbl_cd, cd_help,	"Help",		"Help message",		TBLC_DEFAULT, NULL)		\

TBL_DEFINE (tbl_cd, TBL_CD_COLUMNS);
