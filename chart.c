#include <curses.h>
#include <float.h>
#include <stdlib.h>
#include <string.h>

static double data[0x100];
static size_t dptr, dlen;
static void addpoint(double y)
{
	dptr = (dptr - 1) % (sizeof data / sizeof *data);
	data[dptr] = y;
	if (dlen < sizeof data / sizeof *data)
		++dlen;
}

static double getpoint(size_t i)
{
	return data[(dptr + i) % (sizeof data / sizeof *data)];
}

static void drawchart(const char *title)
{
	unsigned int ymax, xmax;
	getmaxyx(stdscr, ymax, xmax);
	erase();
	ymax -= 3; xmax -= 2;  /* account for the border */
	int len = dlen < xmax ? dlen : xmax;
	double dmin = DBL_MAX, dmax = -DBL_MAX;
	for (int i = 0; i < len; ++i) {
		double d = getpoint(i);
		if (d < dmin)
			dmin = d;
		if (d > dmax)
			dmax = d;
	}
	int maxn, n = snprintf(0, 0, " %g", dmax);
	maxn = n;
	n = snprintf(0, 0, "<%g", getpoint(0));
	if (n > maxn)
		maxn = n;
	n = snprintf(0, 0, " %g", dmin);
	if (n > maxn)
		maxn = n;
	double drange = dmax - dmin;
	xmax -= maxn;
	for (int i = 0; i < len; ++i) {
		unsigned int y = ymax - (getpoint(i) - dmin) / drange * ymax + 1;
		mvaddch(y, xmax - i, ACS_BULLET | A_BOLD);
	}
	mvaddch(0, 0, ACS_ULCORNER);
	hline(ACS_HLINE, xmax);
	mvaddch(0, xmax + 1, ACS_URCORNER);
	mvvline(1, xmax + 1, ACS_VLINE, ymax + 1);
	mvaddch(ymax + 2, xmax + 1, ACS_LRCORNER);
	mvaddch(ymax + 2, 0, ACS_LLCORNER);
	hline(ACS_HLINE, xmax);
	mvvline(1, 0, ACS_VLINE, ymax + 1);
	mvprintw(1, xmax + 2, " %g", dmax);
	mvprintw(ymax + 1, xmax + 2, " %g", dmin);
	if (len)
		mvprintw(ymax - (getpoint(0) - dmin) / drange * ymax + 1,
				xmax + 2, "<%g", getpoint(0));
	if (title) {
		attron(A_BOLD | A_REVERSE);
		mvprintw(0, 4, " %s ", title);
		attroff(A_BOLD | A_REVERSE);
	}
	doupdate();
}

int main(int argc, char **argv)
{
	const char *title = argc < 2 ? 0 : argv[1];
	initscr();
	curs_set(0);
	noecho();
	char buf[32], *ptr = buf;
	size_t n = sizeof buf;
	int status;
	while (status = getnstr(ptr, n), status != ERR) {
		if (status == KEY_RESIZE) {
			size_t len = strlen(ptr);
			ptr += len;
			n -= len;
			endwin();
		} else {
			ptr = buf;
			n = sizeof buf;
			addpoint(atof(ptr));
		}
		drawchart(title);
	}
	endwin();
	return 0;
}
