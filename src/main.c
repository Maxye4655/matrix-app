#define _XOPEN_SOURCE_EXTENDED 1
#include <ncurses.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <getopt.h>
#include <signal.h>
#include <wchar.h>

#define VERSION "1.0.0"
#define TICK_MS 16

typedef enum { MATRIX, RAIN, SNOW, FIRE, STREAM } Pattern;
typedef enum { CLR_GREEN, CLR_RED, CLR_BLUE, CLR_CYAN,
               CLR_WHITE, CLR_YELLOW, CLR_MAGENTA,
               CLR_RAINBOW, CLR_RANDOM } Color;
typedef enum { CS_JAPANESE, CS_ASCII, CS_HEX, CS_BINARY } Charset;

typedef struct {
	Pattern pat;
	Color col;
	Charset cs;
	int spd;
	int den;
	int bld;
	int fade;
	char *cust;
} Cfg;

typedef struct {
	double yf;
	int len, timer, active, pair;
} Drop;

static Cfg cfg = {
	.pat = MATRIX, .col = CLR_GREEN, .cs = CS_JAPANESE,
	.spd = 5, .den = 5, .bld = 1, .fade = 1, .cust = NULL,
};

static Drop *drops = NULL;
static int cols = 0, rows = 0;
static int paused = 0, help = 0;
static volatile sig_atomic_t run = 1;

static const wchar_t *jp =
	L"ｦｧｨｩｪｫｬｭｮｯｰｱｲｳｴｵｶｷｸｹｺｻｼｽｾｿ"
	L"ﾀﾁﾂﾃﾄﾅﾆﾇﾈﾉﾊﾋﾌﾍﾎﾏﾐﾑﾒﾓﾔﾕﾖﾗﾘﾙﾚﾛﾜﾝ";

static const char *ascii =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	"abcdefghijklmnopqrstuvwxyz"
	"0123456789!@#$%^&*()";

static const char *hexc = "0123456789ABCDEF";
static wchar_t *pool = NULL;
static int pool_n = 0;

static const int pairs[] = {
	[CLR_GREEN]   = 1, [CLR_RED]    = 2, [CLR_BLUE]  = 3,
	[CLR_CYAN]    = 4, [CLR_WHITE]  = 5, [CLR_YELLOW] = 6,
	[CLR_MAGENTA] = 7,
};

static const char *pat_names[] = {
	[MATRIX] = "matrix", [RAIN] = "rain", [SNOW] = "snow",
	[FIRE] = "fire", [STREAM] = "stream",
};
static const char *col_names[] = {
	[CLR_GREEN] = "green", [CLR_RED] = "red", [CLR_BLUE] = "blue",
	[CLR_CYAN] = "cyan", [CLR_WHITE] = "white", [CLR_YELLOW] = "yellow",
	[CLR_MAGENTA] = "magenta", [CLR_RAINBOW] = "rainbow", [CLR_RANDOM] = "random",
};
static const char *cs_names[] = {
	[CS_JAPANESE] = "japanese", [CS_ASCII] = "ascii",
	[CS_HEX] = "hex", [CS_BINARY] = "binary",
};

static void on_sig(int s) { (void)s; run = 0; }

static int rng(int lo, int hi) {
	return lo + rand() % (hi - lo + 1);
}

static void init_pool(void) {
	free(pool); pool = NULL; pool_n = 0;
	wchar_t *b = malloc(sizeof(wchar_t) * 512);
	int n = 0;

	switch (cfg.cs) {
	case CS_JAPANESE:
		for (const wchar_t *p = jp; *p; p++) b[n++] = *p;
		break;
	case CS_ASCII:
		for (const char *p = ascii; *p; p++) b[n++] = (wchar_t)(unsigned char)*p;
		break;
	case CS_HEX:
		for (const char *p = hexc; *p; p++) b[n++] = (wchar_t)(unsigned char)*p;
		break;
	case CS_BINARY:
		b[n++] = L'0'; b[n++] = L'1';
		break;
	}

	if (cfg.cust) {
		size_t l = mbstowcs(NULL, cfg.cust, 0);
		if (l != (size_t)-1) {
			wchar_t *w = malloc(sizeof(wchar_t) * (l + 1));
			mbstowcs(w, cfg.cust, l + 1);
			for (size_t i = 0; i < l; i++) b[n++] = w[i];
			free(w);
		}
	}

	pool = b; pool_n = n > 0 ? n : 1;
	if (n == 0) pool[0] = L' ';
}

static wchar_t rc(void) {
	return pool[rng(0, pool_n - 1)];
}

static void setup_cols(void) {
	if (!has_colors()) return;
	start_color();
	use_default_colors();
	init_pair(1, COLOR_GREEN,   -1);
	init_pair(2, COLOR_RED,     -1);
	init_pair(3, COLOR_BLUE,    -1);
	init_pair(4, COLOR_CYAN,    -1);
	init_pair(5, COLOR_WHITE,   -1);
	init_pair(6, COLOR_YELLOW,  -1);
	init_pair(7, COLOR_MAGENTA, -1);
}

static int get_pair(int col) {
	switch (cfg.col) {
	case CLR_RAINBOW: return col % 7 + 1;
	case CLR_RANDOM:  return rng(1, 7);
	default:          return pairs[cfg.col];
	}
}

static int get_len(void) {
	switch (cfg.pat) {
	case MATRIX: return rng(5, 15);
	case RAIN:   return rng(2, 5);
	case SNOW:   return 1;
	case FIRE:   return rng(1, 4);
	case STREAM: return rng(10, 25);
	}
	return 5;
}

static int get_delay(void) {
	int d = cfg.pat == STREAM ? rng(1, 5) : rng(10, 120);
	return d / (cfg.den > 0 ? cfg.den : 1);
}

static void tick(void) {
	double adv = cfg.spd * 0.08;
	if (adv < 0.02) adv = 0.02;

	for (int c = 0; c < cols; c++) {
		Drop *d = &drops[c];
		if (d->active) {
			if (cfg.pat == SNOW) {
				d->yf += adv * 0.5;
				int y = (int)d->yf;
				if (y >= rows) {
					d->active = 0;
					d->timer = get_delay();
					continue;
				}
				if (y > 0 && y % 3 == 0) {
					int dx = rng(-1, 1);
					if (dx && c + dx >= 0 && c + dx < cols
					    && !drops[c + dx].active) {
						drops[c + dx] = *d;
						d->active = 0;
						continue;
					}
				}
				continue;
			}

			d->yf += adv;
			if ((int)d->yf - d->len >= rows) {
				d->active = 0;
				d->timer = get_delay();
			}
		} else {
			if (d->timer > 0) d->timer--;
			else {
				d->active = 1;
				d->len = get_len();
				d->yf = -(double)rng(1, d->len + 10);
				d->pair = get_pair(c);
				d->timer = get_delay();
			}
		}
	}
}

static void draw(void) {
	erase();

	for (int c = 0; c < cols; c++) {
		Drop *d = &drops[c];
		if (!d->active) continue;

		int hy = (int)d->yf;

		for (int i = 0; i < d->len; i++) {
			int y = hy - i;
			if (y < 0 || y >= rows) continue;

			wchar_t wc = rc();
			cchar_t cc;
			attr_t a = 0;

			if (i == 0)
				a = A_BOLD;
			else if (cfg.fade && i >= d->len * 2 / 3)
				a = A_DIM;

			setcchar(&cc, &wc, a, (short)d->pair, NULL);
			mvadd_wch(y, c, &cc);
		}
	}

	if (paused) {
		const char *m = " PAUSED ";
		int mx = (cols - (int)strlen(m)) / 2;
		attron(A_BOLD | COLOR_PAIR(5));
		mvprintw(rows / 2, mx < 0 ? 0 : mx, m);
		attroff(A_BOLD | COLOR_PAIR(5));
	}

	if (help) {
		int w = 44, h = 14;
		int bx = (cols - w) / 2;
		int by = (rows - h) / 2;
		if (bx < 0) bx = 0;
		if (by < 0) by = 0;

		for (int y = 0; y < h; y++) {
			move(by + y, bx);
			for (int x = 0; x < w; x++)
				addch(' ');
		}

		attr_t bold = A_BOLD | COLOR_PAIR(5);

		attron(bold);
		mvprintw(by, bx + (w - 7) / 2, " MATRIX ");
		attroff(bold);

		mvprintw(by + 1, bx,  " Options");
		mvprintw(by + 2, bx,  "  -c --color    %s", col_names[cfg.col]);
		mvprintw(by + 3, bx,  "  -s --speed    %d", cfg.spd);
		mvprintw(by + 4, bx,  "  -d --density  %d", cfg.den);
		mvprintw(by + 5, bx,  "  -C --charset  %s", cs_names[cfg.cs]);
		mvprintw(by + 6, bx,  "  -p --pattern  %s", pat_names[cfg.pat]);
		mvprintw(by + 7, bx,  "  -b --bold     %s", cfg.bld ? "on" : "off");
		mvprintw(by + 8, bx,  "  -f --fade     %s", cfg.fade ? "on" : "off");

		attron(bold);
		mvprintw(by + 10, bx, " Keys");
		attroff(bold);
		mvprintw(by + 11, bx, "  q  quit   space  pause");
		mvprintw(by + 12, bx, "  r  reset  +/-    speed");
		mvprintw(by + 13, bx, "  h  help");
	}

	refresh();
}

static void print_help(void) {
	printf("matrix %s - terminal matrix rain\n\n", VERSION);
	printf("Usage: matrix [options]\n\n");
	printf("Options:\n");
	printf("  -c, --color <scheme>    Color scheme: green, red, blue, cyan,\n");
	printf("                          white, yellow, magenta, rainbow, random\n");
	printf("                          (default: green)\n");
	printf("  -s, --speed <1-10>      Fall speed (default: 5)\n");
	printf("  -d, --density <1-10>    Drop density (default: 5)\n");
	printf("  -C, --charset <set>     Character set: japanese, ascii, hex,\n");
	printf("                          binary (default: japanese)\n");
	printf("  -p, --pattern <type>    Fall pattern: matrix, rain, snow, fire,\n");
	printf("                          stream (default: matrix)\n");
	printf("  -b, --bold              Enable bold text (default: on)\n");
	printf("  -f, --fade              Enable fade effect (default: on)\n");
	printf("      --custom <chars>    Custom character string\n");
	printf("  -h, --help              Show this help\n");
	printf("\nKeys:\n");
	printf("  q       Quit\n");
	printf("  Space   Pause/resume\n");
	printf("  r       Reset screen\n");
	printf("  + / -   Increase/decrease speed\n");
	printf("  h       Toggle help overlay\n");
}

static int parse_color(const char *s) {
	for (int i = 0; i < 9; i++)
		if (!strcmp(s, col_names[i])) return i;
	return -1;
}

static int parse_charset(const char *s) {
	for (int i = 0; i < 4; i++)
		if (!strcmp(s, cs_names[i])) return i;
	return -1;
}

static int parse_pattern(const char *s) {
	for (int i = 0; i < 5; i++)
		if (!strcmp(s, pat_names[i])) return i;
	return -1;
}

static void parse_args(int argc, char **argv) {
	static struct option opts[] = {
		{"color",   required_argument, NULL, 'c'},
		{"speed",   required_argument, NULL, 's'},
		{"density", required_argument, NULL, 'd'},
		{"charset", required_argument, NULL, 'C'},
		{"pattern", required_argument, NULL, 'p'},
		{"bold",    no_argument,       NULL, 'b'},
		{"no-bold", no_argument,       NULL, 'B'},
		{"fade",    no_argument,       NULL, 'f'},
		{"no-fade", no_argument,       NULL, 'F'},
		{"custom",  required_argument, NULL, 1},
		{"help",    no_argument,       NULL, 'h'},
		{0, 0, 0, 0}
	};

	int c;
	while ((c = getopt_long(argc, argv, "c:s:d:C:p:bBfFh", opts, NULL)) != -1) {
		switch (c) {
		case 'c': {
			int v = parse_color(optarg);
			if (v < 0) { fprintf(stderr, "Unknown color: %s\n", optarg); exit(1); }
			cfg.col = v;
			break;
		}
		case 's':
			cfg.spd = atoi(optarg);
			if (cfg.spd < 1) cfg.spd = 1;
			if (cfg.spd > 10) cfg.spd = 10;
			break;
		case 'd':
			cfg.den = atoi(optarg);
			if (cfg.den < 1) cfg.den = 1;
			if (cfg.den > 10) cfg.den = 10;
			break;
		case 'C': {
			int v = parse_charset(optarg);
			if (v < 0) { fprintf(stderr, "Unknown charset: %s\n", optarg); exit(1); }
			cfg.cs = v;
			break;
		}
		case 'p': {
			int v = parse_pattern(optarg);
			if (v < 0) { fprintf(stderr, "Unknown pattern: %s\n", optarg); exit(1); }
			cfg.pat = v;
			break;
		}
		case 'b': cfg.bld = 1; break;
		case 'B': cfg.bld = 0; break;
		case 'f': cfg.fade = 1; break;
		case 'F': cfg.fade = 0; break;
		case 1:  cfg.cust = optarg; break;
		case 'h': print_help(); exit(0);
		default: print_help(); exit(1);
		}
	}
}

int main(int argc, char **argv) {
	parse_args(argc, argv);

	srand((unsigned)time(NULL));
	setlocale(LC_ALL, "");
	signal(SIGINT, on_sig);
	signal(SIGTERM, on_sig);

	initscr();
	cbreak();
	noecho();
	curs_set(0);
	nodelay(stdscr, TRUE);
	keypad(stdscr, TRUE);

	setup_cols();
	init_pool();

	getmaxyx(stdscr, rows, cols);
	drops = calloc((size_t)cols, sizeof(Drop));
	for (int c = 0; c < cols; c++)
		drops[c].timer = rng(0, 100);

	while (run) {
		int ch = getch();
		switch (ch) {
		case 'q': case 'Q': run = 0; break;
		case ' ': paused = !paused; break;
		case 'r': case 'R':
			memset(drops, 0, (size_t)cols * sizeof(Drop));
			for (int c = 0; c < cols; c++)
				drops[c].timer = rng(0, 50);
			break;
		case 'h': case 'H': help = !help; break;
		case '+': case '=':
			if (cfg.spd < 10) cfg.spd++;
			break;
		case '-': case '_':
			if (cfg.spd > 1) cfg.spd--;
			break;
		case KEY_RESIZE:
			getmaxyx(stdscr, rows, cols);
			free(drops);
			drops = calloc((size_t)cols, sizeof(Drop));
			for (int c = 0; c < cols; c++)
				drops[c].timer = rng(0, 50);
			break;
		}

		if (run && !paused) {
			tick();
			draw();
		} else if (paused) {
			draw();
		}

		napms(TICK_MS);
	}

	curs_set(1);
	clear();
	refresh();
	endwin();
	free(drops);
	free(pool);

	return 0;
}
