#include <assert.h>
#include <stdlib.h>
#include "libtbl.h"
#include "bytes.h"

/* NETIF EXAMPLE */

#define NETIF_STATES	\
	X(UP, "up")		 \
	X(DOWN, "down")

enum netif_state {
#define X(name, string) NET_##name,
	NETIF_STATES
#undef X
};

static const char *netif_state_string[] = {
#define X(name, string) [NET_##name] = string,
	NETIF_STATES
#undef X
};

struct netif {
	char			*name;
	enum netif_state	 state;
	int			 mtu;
	struct {
		long			 rx_packets;
		unsigned long long	 rx_bytes;
	} rx;
};

static void
stringify_netif_state (FILE *file, const void *ptr, int flags)
{
	const enum netif_state *state = ptr;
	const char *str = netif_state_string[*state];
	fprintf (file, flags == TBLS_JSON ? "\"%s\"" : "%s", str);
}

static int
colorize_netif_state (const void *ptr)
{
	const enum netif_state *state = ptr;
	switch (*state) { 
	case NET_UP:
		return TBL_COLOR_GREEN;
	case NET_DOWN:
		return TBL_COLOR_RED;
	default:
		abort ();
	}
}

static const struct tbl_type_ops ops_netif_state = {
	.to_deflags	= TBLC_ALEFT,
	.to_stringify	= stringify_netif_state,
	.to_colorize	= colorize_netif_state,
};

static int
colorize_mtu (const void *ptr)
{
	const int *mtu = ptr;
	return *mtu < 9000 ? TBL_COLOR_BLACK | TBL_COLOR_INTENSE : TBL_COLOR_YELLOW;
}

static void
stringify_mtu (FILE *file, const void *ptr, int flags)
{
	const int *mtu = ptr;
	(void)flags;
	fprintf (file, "%d", *mtu);
}

static const struct tbl_type_ops ops_mtu = {
	.to_deflags	= TBLC_ARIGHT,
	.to_stringify	= stringify_mtu,
	.to_colorize	= colorize_mtu,
};

#define COLUMNS_NETIF(X)												\
	X(netif, name,	  	"Interface",	"Network Interface",		TBLC_DEFAULT, NULL)			\
	X(netif, mtu,	   	"MTU",		"Maximum Transmission Size",	TBLC_DEFAULT, &ops_mtu)			\
	X(netif, rx.rx_packets,	"RX Packets",	"Number of recieved packets",	TBLC_SUM | TBLC_ARIGHT, NULL)		\
	X(netif, rx.rx_bytes,	"RX Bytes",	"Number of received bytes",	TBLC_SUM | TBLC_ARIGHT, &ops_bytes)	\
	X(netif, state,		"State",	"up/down",			TBLC_DEFAULT, &ops_netif_state)		\

TBL_DEFINE_STATIC(netif, COLUMNS_NETIF);

int main (void)
{
	const struct netif nifs[] = {
		{
			.name		= "eth0",
			.state		= NET_UP,
			.mtu		= 1500,
			.rx.rx_packets	= 12543,
			.rx.rx_bytes	= 15 << 20,
		},
		{
			.name		= "wlan0",
			.state		= NET_DOWN,
			.mtu		= 1500,
			.rx.rx_packets	= 3821,
			.rx.rx_bytes	= 2 << 20,
		},
		{
			.name		= "lo",
			.state		= NET_DOWN,
			.mtu		= 65536,
			.rx.rx_packets	= 118244,
			.rx.rx_bytes	= 42 << 20,
		},
		{
			.name		= "tun0",
			.state		= NET_UP,
			.mtu		= 9000,
			.rx.rx_packets	= 256,
			.rx.rx_bytes	= 1234L,
		}
	};
	printf ("netif descr:\n");
	tbl_print_descr (stdout, netif, NULL, TBL_DEFAULT);
	printf ("\ntbl_cd descr:\n");
	tbl_print_descr (stdout, tbl_cd, NULL, TBL_DEFAULT);
	printf ("\nwith everything:\n");
	tbl_print_table (stdout, netif, nifs, TBL_ARRAY_SIZE (nifs), NULL, TBL_DEFAULT);
	printf ("\nwithout everything:\n");
	tbl_print_table (stdout, netif, nifs, TBL_ARRAY_SIZE (nifs), NULL, TBL_NOHEADER | TBL_NOSUM | TBL_RAW | TBL_NOCOLOR);
	printf ("\nfirst row:\n");
	tbl_print_elem (stdout, netif, &nifs[0], NULL, TBL_DEFAULT);
	printf ("\nJSON: [\n");
	tbl_print_json (stdout, netif, nifs, TBL_ARRAY_SIZE (nifs), NULL);
	printf ("]\n");
	printf ("\nCSV:\n");
	tbl_print_csv (stdout, netif, nifs, TBL_ARRAY_SIZE (nifs), NULL, ',');
	printf ("\nXML:\n");
	tbl_print_xml (stdout, netif, nifs, TBL_ARRAY_SIZE (nifs), NULL);

	{
		size_t csel[TBL_NCOLS (netif) + 1];

		printf ("\nname-selected columns (\"name,rx.rx_bytes\"):\n");
		assert (tbl_select_cols (netif, "name,rx.rx_bytes", ',', csel) == 0);
		tbl_print_table (stdout, netif, nifs, TBL_ARRAY_SIZE (nifs), csel, TBL_DEFAULT);

		printf ("\n...minus \"name\" (\"-name\"):\n");
		assert (tbl_select_cols (netif, "-name", ',', csel) == 0);
		tbl_print_table (stdout, netif, nifs, TBL_ARRAY_SIZE (nifs), csel, TBL_DEFAULT);

		printf ("\n...plus \"mtu\" (\"+mtu\"):\n");
		assert (tbl_select_cols (netif, "+mtu", ',', csel) == 0);
		tbl_print_table (stdout, netif, nifs, TBL_ARRAY_SIZE (nifs), csel, TBL_DEFAULT);

		printf ("\nunknown column name is rejected: %d\n",
			tbl_select_cols (netif, "no_such_column", ',', csel));

		tbl_select_cols (netif, NULL, 0, csel);
		tbl_print_table (stdout, netif, nifs, TBL_ARRAY_SIZE (nifs), csel, TBL_DEFAULT);
	}
}
