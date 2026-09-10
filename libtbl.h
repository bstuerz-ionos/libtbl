#ifndef FILE_LIBTBL5_H
#define FILE_LIBTBL5_H
#include <stddef.h>
#include <stdio.h>

#define TBL_ARRAY_SIZE(a) (sizeof (a) / sizeof (*(a)))

enum tbl_format {
	TBLF_TABLE,
	TBLF_ELEMENT,
	TBLF_JSON,
	TBLF_CSV,
	TBLF_XML,
};

/* returned by to_colorize() */
enum tbl_color {
	TBL_COLOR_RESET		= 0,
	TBL_COLOR_BLACK		= 30,
	TBL_COLOR_RED		= 31,
	TBL_COLOR_GREEN		= 32,
	TBL_COLOR_YELLOW	= 33,
	TBL_COLOR_BLUE		= 34,
	TBL_COLOR_MAGENTA	= 35,
	TBL_COLOR_CYAN		= 36,
	TBL_COLOR_WHITE		= 37,
	TBL_COLOR_BOLD		= 0x100,
	TBL_COLOR_INTENSE	= 0x200,
};

/* flags for print_table() */
enum tbl_flags {
	TBL_DEFAULT		= 0x00, /* turn on everything */
	TBL_RAW			= 0x01,	/* do not humanize the output */
	TBL_NOSUM		= 0x02,	/* do not print the sum */
	TBL_NOHEADER		= 0x04,	/* do not print the header */
	TBL_NOCOLOR		= 0x08,	/* do not colorize the output */
};

/* flags for cd_flags */
enum tblc_flags {
	TBLC_DEFAULT		= -1,	/* use the default flags suplied by type_ops */
	TBLC_SUM		= 0x01,	/* print a sum in the end */
	TBLC_ALEFT		= 0x00,	/* align to the left in table (default) */
	TBLC_ARIGHT		= 0x02,	/* align to the right in table */
};

enum tbl_stringify_type {
	TBLS_RAW,
	TBLS_HUMAN,
	TBLS_JSON,
};


/* column descriptor */
struct tbl_cd {
	const char			*cd_name;
	const char			*cd_header;
	const char			*cd_help;
	int				 cd_flags;	/* see tblc_flags */
	ptrdiff_t			 cd_offset;	/* offsetof (struct, member) */
	size_t				 cd_size;	/* sizeof (struct.member) */
	const struct tbl_type_ops	*cd_ops;	/* vtable */
};

/* table descruptor */
struct tbl_td {
	const struct tbl_cd	*td_cols;	/* column descriptors */
	const char		*td_name;	/* name of the type */
	size_t			 td_ncol;	/* total number of columns */
	size_t			 td_size;	/* size of an element */
};

extern const struct tbl_type_ops {
	int	  to_deflags;
	void	(*to_stringify)(FILE *, const void *, int);
	int	(*to_colorize)(const void *);
	void	(*to_add)(void *, const void *);
}	_tbl_ops_short,		_tbl_ops_ushort,
       	_tbl_ops_int,		_tbl_ops_uint,
       	_tbl_ops_long,		_tbl_ops_ulong,
       	_tbl_ops_llong,		_tbl_ops_ullong,
	_tbl_ops_string,	_tbl_ops_undefined;

extern int tbl_write_json (FILE *, const char *);
extern void tbl_stringify_string (FILE *, const void *, int flags);

/* infer a default implementation of tbl_type_ops for x	*/
/* if none available, expand to undefined, which will	*/
/* turn into a linker error, to prevent misuse		*/
#define _tbl_infer_ops(x) _Generic((x),			\
	short:			&_tbl_ops_short,	\
	int:			&_tbl_ops_int,		\
	long:			&_tbl_ops_long,		\
	long long:		&_tbl_ops_llong,	\
	unsigned short:		&_tbl_ops_ushort,	\
	unsigned int:		&_tbl_ops_uint,		\
	unsigned long:		&_tbl_ops_ulong,	\
	unsigned long long:	&_tbl_ops_ullong,	\
	char *:			&_tbl_ops_string,	\
	const char *:		&_tbl_ops_string,	\
	default:		&_tbl_ops_undefined	\
)

#define _tbl_typeof(str, mem) ((struct str *)0)->mem
#define _TBL_DEFCOL(str, mem, hdr, help, flags, ops)			\
(struct tbl_cd) {							\
	.cd_name	= #mem,                                  	\
	.cd_header	= (hdr),					\
	.cd_help	= (help),					\
	.cd_flags	= (flags),					\
	.cd_offset	= offsetof(struct str, mem),			\
	.cd_size	= sizeof (_tbl_typeof (str, mem)),		\
	.cd_ops		= (ops) != NULL ? (ops)				\
			: _tbl_infer_ops (_tbl_typeof (str, mem))	\
},
#define _TBL_DEFINE(sclass, name, f)			\
sclass const struct tbl_cd _tbl_cols_##name[] = {	\
    f(_TBL_DEFCOL)					\
};							\
sclass const struct tbl_td _tbl_td_##name = {		\
	.td_cols = _tbl_cols_##name,			\
	.td_name = #name,				\
	.td_ncol = TBL_ARRAY_SIZE (_tbl_cols_##name),	\
	.td_size = sizeof (struct name),		\
}

#define TBL_CNUL ((size_t)-1)
#define TBL_NCOLS(name) (TBL_ARRAY_SIZE (_tbl_cols_##name))

#define TBL_DEFINE(name, f) _TBL_DEFINE(, name, f)
#define TBL_DEFINE_STATIC(name, f) _TBL_DEFINE(static, name, f)

#define _tbl_td(str) &_tbl_td_##str

/* Get the column index for a particular field		*/
/* The offsetof() checks if the field actually exists	*/
#define tbl_coli(str, field)					\
	((void)offsetof (struct str, field),			\
	tbl_coli_ (_tbl_td (str), #field, sizeof (#field) - 1))

/* Select columns by name, at runtime (e.g. from a --columns=... argument).	*/
/* @names is a list of column names separated by @delim; a leading '+' or	*/
/* '-' in @names adds to, or removes from, the selection already present in	*/
/* @csel, otherwise @csel is emptied first. Names already selected are		*/
/* ignored, so a column never appears twice. @csel must not be NULL, and	*/
/* must have room for at least @csel_len entries, including the trailing	*/
/* TBL_CNUL. Returns 0 on success, -1 if a name is unknown or @csel is too	*/
/* small.									*/
#define tbl_select_cols(str, names, delim, csel)			\
	((void)sizeof (struct { int dummy; _Static_assert (		\
		TBL_ARRAY_SIZE (csel) > TBL_NCOLS (str));		\
	}), tbl_select_cols_ (_tbl_td (str), (names), (delim), (csel)))

/* print a table */
#define tbl_print_table(file, str, rows, nrows, csel, flags)	\
	(tbl_print_table_ ((file), _tbl_td (str), (rows), (nrows), (csel), (flags)))

/* print only one element, as an inverted table */
#define tbl_print_elem(file, str, row, csel, flags)	\
	(tbl_print_elem_ ((file), _tbl_td (str), (row), (csel), (flags)))

/* print JSON */
#define tbl_print_json(file, str, rows, nrows, csel)	\
	(tbl_print_json_ ((file), _tbl_td (str), (rows), (nrows), (csel)))

/* print CSV */
#define tbl_print_csv(file, str, rows, nrows, csel, sep)	\
	(tbl_print_csv_ ((file), _tbl_td (str), (rows), (nrows), (csel), (sep)))

/* print XML */
#define tbl_print_xml(file, str, rows, nrows, csel)	\
	(tbl_print_xml_ ((file), _tbl_td (str), (rows), (nrows), (csel)))

/* print table description */
#define tbl_print_descr(file, str, csel, tflags)	\
	(tbl_print_descr_ ((file), _tbl_td (str), (csel), (tflags)))

#define tbl_print_descr_(file, td, csel, tflags)	\
	(tbl_print_table ((file), tbl_cd, (td)->td_cols, (td)->td_ncol, (csel), (tflags)))

#define tbl_print(file, str, rows, nrows, csel, tflags, fmt)	\
	(tbl_print_ ((file), _tbl_td (str), (rows), (nrows), (csel), (tflags), (fmt)))

extern const struct tbl_td _tbl_td_tbl_cd;

extern size_t tbl_coli_ (const struct tbl_td *, const char *, size_t);

extern int tbl_select_cols_ (
	const struct tbl_td	*td,
	const char		*names,
	char			 delim,
	size_t			*csel
);

#define TBL_PRINT_ARGS FILE *out, const struct tbl_td *td, const void *rows, size_t nrows, const size_t *csel
extern int tbl_print_table_ (TBL_PRINT_ARGS, int tflags);
extern int tbl_print_elem_ (FILE *, const struct tbl_td *, const void *, const size_t *, int);
extern int tbl_print_json_ (TBL_PRINT_ARGS);
extern int tbl_print_xml_ (TBL_PRINT_ARGS);
extern int tbl_print_csv_ (TBL_PRINT_ARGS, char sep);
extern int tbl_print_ (TBL_PRINT_ARGS, int tflags, enum tbl_format fmt);
#undef TBL_PRINT_ARGS

#endif /* FILE_LIBTBL5_H */
