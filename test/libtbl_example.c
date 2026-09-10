// SPDX-License-Identifier: GPL-2.0-or-later
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <err.h>
#include "libtbl.h"

struct test {
	char *name;
	int age;
	int marks;
	char *country;
};

#define COLUMNS_TEST(X)	\
	X(test, name,	"Student Name",	"", TBLC_DEFAULT, NULL)	\
	X(test, age,	"Age",		"", TBLC_DEFAULT, NULL)	\
	X(test, marks,	"Marks",	"", TBLC_DEFAULT, NULL)	\
	X(test, country,"Country",	"", TBLC_DEFAULT, NULL) \

static void
stringify_marks (FILE *file, const void *ptr, int flags)
{
	fprintf (file, "%d", *(const int *)ptr);
}

static int
colorize_marks (const void *ptr)
{
	const int *marks = ptr;

	return *marks > 1000 ? TBL_COLOR_RED : TBL_COLOR_GREEN;
}

static const struct tbl_type_ops ops_marks = {
	.to_stringify	= stringify_marks,
	.to_colorize	= colorize_marks,
};

TBL_DEFINE (test, COLUMNS_TEST);


static const struct test rows[] = {
	{"Alice", 25, 90, "United Kingdom"},
	{"Bob", 28, 85, "Australia"},
	{"Charles", 23, 68, "Denmark"},
	{"Frank", 24, 70, "India"},
};

static int is_terminal;

static void print_usage(const char *prog)
{
	printf ("usage: %s [option]\n", prog);
	printf ("Options:\n");
	printf ("Select one of the following formats to print table: \n");
	printf (" table, json, xml, csv, help \n");
}

int main(int argc, char **argv)
{
	enum tbl_format	 fmt;
	const char	*path = "-", *sfmt, *prog;
	FILE		*file;
	int		 option, flags;

	prog = argv[0];

	while ((option = getopt (argc, argv, "o:")) != -1) {
		switch (option) {
		case 'o':
			path = optarg;
			break;
		default:
			return 1;
		}
	}

	argv += optind;
	argc -= optind;

	file = strcmp (path, "-") == 0 ? stdout : fopen (path, "w");
	if (file == NULL)
		err (1, "open('%s')", path);

	is_terminal = (isatty (fileno (file)) == 1);
	flags = is_terminal ? TBL_DEFAULT : TBL_NOCOLOR;

	if (argc < 1) {
		fmt = TBLF_TABLE;
		goto print;
	}

	sfmt = argv[0];

	if (!strcmp(sfmt, "terminal") || !strcmp(sfmt,"table")) {
		fmt = TBLF_TABLE;
	} else if (!strcmp(sfmt, "help")) {
		print_usage(prog);
		return 1;
	} else if (!strcmp(sfmt, "csv")) {
		fmt = TBLF_CSV;
	} else if (!strcmp(sfmt, "json")) {
		fmt = TBLF_JSON;
	} else if (!strcmp(sfmt, "xml")) {
		fmt = TBLF_XML;
	} else {
		print_usage(prog);
		return 1;
	}

print:
	tbl_print (file, test, rows, TBL_ARRAY_SIZE (rows), NULL, flags, fmt);
	return 0;
}
