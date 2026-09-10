#ifndef FILE_BYTES_H
#define FILE_BYTES_H
#include <stdio.h>
#include "libtbl.h"

static void
stringify_bytes (FILE *file, const void *ptr, int flags)
{
	static const struct unit {
		const char  *str;
		int		  shift;
	} units[] = {
		{ "TB", 40 },
		{ "GB", 30 },
		{ "MB", 20 },
		{ "KB", 10 },
		{ "B", 0 }
	};
	unsigned long long bytes = *(unsigned long long *)ptr;
	const struct unit *u;

	if (flags == TBLS_HUMAN) {
		for (u = units; ; ++u) {
			if (bytes >= (1ULL << u->shift) || u->shift == 0) {
				bytes >>= u->shift;
				fprintf (file, "%u %s", (unsigned)bytes, u->str);
				return;
			}
		}
	} else {
		fprintf (file, "%llu", bytes);
	}
}

static void
add_bytes (void *d, const void *r)
{
	*(unsigned long long *)d += *(const unsigned long long *)r;
}

static const struct tbl_type_ops ops_bytes = {
	.to_deflags	= TBLC_ARIGHT,
	.to_stringify	= stringify_bytes,
	.to_add		= add_bytes,
};

#endif /* FILE_BYTES_H */
