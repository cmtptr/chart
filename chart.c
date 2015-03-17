#include <curses.h>
#include <errno.h>
#include <fcntl.h>
#include <float.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *const usage_short = "\
Try '%s --help' for more information.\n\
";

static const char *const usage_long = "\
usage: %s [<option>...] [<filename>]\n\
Draw a chart in the terminal.\n\
\n\
options:\n\
  -h, --help                           Display this text.\n\
  -r <min>,<max>, --range=<min>,<max>  Use a fixed y-axis from <min> to <max>,\n\
                                       inclusive.\n\
  -s <style>, --style=<style>          Set the data point style, where <style>\n\
                                       may be one of \"dot\" (the default),\n\
                                       \"plus\", or \"ohlc\".\n\
  -t <title>, --title=<title>          Set the displayed title.\n\
\n\
chart reads newline-delimited numeric values from <filename> (or in the absence\n\
of the <filename> argument, or if <filename> is '-', stdin) and plots them on a\n\
chart that is drawn directly in the terminal.\n\
";

static size_t dsize, dlen, dptr = -1;
static struct ohlc {
	double open;
	double high;
	double low;
	double close;
} *data;

/* grow the data buffer to fill the screen */
static void resize(void)
{
	int width = getmaxx(stdscr);
	size_t newsize = dsize ? dsize : 1;
	while (newsize < (size_t)width)  /* round up to the next binary number */
		newsize <<= 1;
	if (newsize != dsize) {
		struct ohlc *newdata = malloc(newsize * sizeof *newdata);
		size_t len = dptr + 1;
		memcpy(newdata, data, len * sizeof *newdata);
		memcpy(newdata + newsize - dsize + len, data + len,
				(dlen - len) * sizeof *newdata);
		free(data);
		dsize = newsize;
		data = newdata;
	}
}

/* add a new point to the data set */
static void addpoint(double y)
{
	dptr = (dptr + 1) % dsize;
	data[dptr] = (struct ohlc){
		.open = y,
		.high = y,
		.low = y,
		.close = y,
	};
	if (dlen < dsize)
		++dlen;
}

/* update the most recent point already in the data set */
static void updpoint(double y)
{
	if (y > data[dptr].high)
		data[dptr].high = y;
	else if (y < data[dptr].low)
		data[dptr].low = y;
	data[dptr].close = y;
}

/* find some metrics to make everything fit in the terminal nicely */
static struct draw {
	int maxy, maxx;
	size_t begin, end;
	double dmin, dmax, drange;
	int margin;
} initdraw(void)
{
	struct draw drw;
	getmaxyx(stdscr, drw.maxy, drw.maxx);
	drw.end = (dptr + 1) % dsize;

	/* find the domain */
	size_t maxx = drw.maxx - 2;  /* -2 to account for the border */
	drw.begin = (drw.end - (dlen < maxx ? dlen : maxx)) % dsize;

	/* find the range */
	drw.dmin = DBL_MAX;
	drw.dmax = -DBL_MAX;
	double dlast = 0.0;
	for (size_t i = drw.begin; i != drw.end; i = (i + 1) % dsize) {
		dlast = data[i].close;
		if (data[i].low < drw.dmin)
			drw.dmin = data[i].low;
		if (data[i].high > drw.dmax)
			drw.dmax = data[i].high;
	}
	drw.drange = drw.dmax - drw.dmin;

	/* find the maximum width of the margin for the scale on the right */
	drw.margin = snprintf(0, 0, "%g", drw.dmax);
	int n = snprintf(0, 0, "%g", drw.dmin);
	if (n > drw.margin)
		drw.margin = n;
	n = snprintf(0, 0, "%g", dlast);
	if (n > drw.margin)
		drw.margin = n;

	/* adjust the domain with the newly-found chart width (still
	 * accounting for the border) */
	maxx = drw.maxx - drw.margin - 2;
	drw.begin = (drw.end - (dlen < maxx ? dlen : maxx)) % dsize;

	return drw;
}

/* draw it! */
static void drawchart(const char *title)
{
	struct draw drw = initdraw();

	/* draw the chart */
	WINDOW *win = newwin(drw.maxy, drw.maxx - drw.margin, 0, 0);
	int y = 0, x = 1, maxy = drw.maxy - 2;  /* -2 for the border */
	double dlast = 0.0;
	for (size_t i = drw.begin; i != drw.end; i = (i + 1) % dsize) {
		dlast = data[i].close;
		y = maxy - (dlast - drw.dmin) / drw.drange * (maxy - 1) + 0.5;
		mvwaddch(win, y, x++, ACS_BULLET | A_BOLD);
	}
	x = getmaxx(win) - 1;  /* fix x to the right side of the chart */
	box(win, 0, 0);
	if (title) {
		wattron(win, A_BOLD | A_REVERSE);
		mvwprintw(win, 0, 4, " %s ", title);
		wattroff(win, A_BOLD | A_REVERSE);
	}
	mvwaddch(win, 1, x, ACS_RTEE);
	mvwaddch(win, maxy, x, ACS_RTEE);
	mvwaddch(win, y, x, ACS_RTEE);
	wrefresh(win);
	delwin(win);

	/* draw the scale */
	win = newwin(drw.maxy, drw.margin, 0, drw.maxx - drw.margin);
	mvwprintw(win, 1, 0, "%g", drw.dmax);
	mvwprintw(win, maxy, 0, "%g", drw.dmin);
	mvwprintw(win, y, 0, "%g", dlast);
	wrefresh(win);
	delwin(win);
}

int main(int argc, char **argv)
{
	/* parse arguments */
	const char *title = 0;
	while (1) {
		static struct option options[] = {
			{"help", no_argument, 0, 'h'},
			{"range", required_argument, 0, 'r'},
			{"style", required_argument, 0, 's'},
			{"title", required_argument, 0, 't'},
			{0}
		};
		int flag = getopt_long(argc, argv, "hn:r:s:t:", options, 0);
		if (flag < 0)
			break;
		switch (flag) {
		case 'h':
			printf(usage_long, argv[0]);
			return 0;
		case 'r':
			/* TODO -r <min>,<max> */
			break;
		case 's':
			/* TODO -s dot|plus|ohlc */
			(void)updpoint;
			break;
		case 't':
			title = optarg;
			break;
		default:
			fprintf(stderr, usage_short, argv[0]);
			return 1;
		}
	}

	/* if a filename was specified, open it and dup it to STDIN_FILENO */
	int fd_stdin = STDIN_FILENO;
	if (optind < argc && strcmp(argv[optind], "-")) {
		const char *fn = argv[optind];
		if (!title)
			title = fn;
		int fd = open(fn, O_RDONLY);
		if (fd < 0) {
			fprintf(stderr, "%s: %s: %s\n", argv[0], fn,
					strerror(errno));
			return 1;
		}
		fd_stdin = dup(STDIN_FILENO);
		dup2(fd, STDIN_FILENO);
		close(fd);
	}

	/* initialize curses */
	if (!initscr())
		return 1;
	curs_set(0);
	noecho();
	resize();

	/* input loop */
	char buf[32], *ptr = buf;
	size_t n = sizeof buf;
	for (int is_tmo = 0;;) {
		for (int status; status = getnstr(ptr, n), status != ERR;) {
			if (status == KEY_RESIZE) {
				endwin();
				resize();
				break;  /* redraw immediately */
			}
			ptr = buf;
			n = sizeof buf;
			addpoint(atof(ptr));
			/* aggregate input lines that are within 20ms of each
			 * other to avoid flooding the terminal with updates
			 * that are coming too rapidly to be seen */
			timeout(20);
			is_tmo = 1;
		}
		if (!is_tmo)
			/* ERR must have been the result of EOF, not timeout */
			break;
		drawchart(title);
		refresh();
		size_t len = strlen(ptr);
		ptr += len;
		n -= len;
		timeout(-1);
		is_tmo = 0;
	}

	/* if the input was from a file (i.e., this wasn't a live stream), wait
	 * for a keypress before exiting so that the user has a chance to see
	 * the nice chart I made for him or her */
	if (fd_stdin != STDIN_FILENO) {
		dup2(fd_stdin, STDIN_FILENO);
		char *title2 = malloc(snprintf(0, 0, "%s (closed)", title));
		sprintf(title2, "%s (closed)", title);
		do {
			drawchart(title2);
			refresh();
		} while (getch() == KEY_RESIZE);
	}

	/* shutdown */
	endwin();
	return 0;
}
