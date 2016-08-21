#define _LARGEFILE_SOURCE
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <fcntl.h>
#include <stdint.h>

/**
 * Name........: keyboard-walk-processor (kwp)
 * Description.: Advanced keyboard-walk generator with configureable basechars, keymap and routes
 * Autor.......: Jens Steube <jens.steube@gmail.com>
 * License.....: MIT
 */

#define VERSION_BIN           100

#define DIST_CNT              3 // this can affect performance

#define MOD_BASIC             0
#define MOD_SHIFT             1
#define MOD_ALTGR             2
#define MOD_CNT               3

#define DIR_SOUTH_WEST        0
#define DIR_SOUTH             1
#define DIR_SOUTH_EAST        2
#define DIR_WEST              3
#define DIR_REPEAT            4
#define DIR_EAST              5
#define DIR_NORTH_WEST        6
#define DIR_NORTH             7
#define DIR_NORTH_EAST        8
#define DIR_CNT               9

#define KEYMAP_WIDTH          14
#define KEYMAP_HEIGHT         4
#define KEYMAP_ENTRIES        (KEYMAP_WIDTH * KEYMAP_HEIGHT)

#define SELECTION_CNT         (DIST_CNT * MOD_CNT * DIR_CNT)

#define RC_OK                 0
#define RC_INVALID            -1

#define ROUTE_LENGTH_MIN      1
#define ROUTE_LENGTH_MAX      32

#define ROUTE_REPEAT_MIN      1
#define ROUTE_REPEAT_MAX      16

#define KEYBOARD_BASIC        1
#define KEYBOARD_SHIFT        0
#define KEYBOARD_ALTGR        0
#define KEYBOARD_ALL          0

#define KEYWALK_SOUTH_WEST    0
#define KEYWALK_SOUTH         1
#define KEYWALK_SOUTH_EAST    0
#define KEYWALK_WEST          1
#define KEYWALK_REPEAT        0
#define KEYWALK_EAST          1
#define KEYWALK_NORTH_WEST    0
#define KEYWALK_NORTH         1
#define KEYWALK_NORTH_EAST    0
#define KEYWALK_ALL           0

#define KEYWALK_DISTANCE_MIN  1
#define KEYWALK_DISTANCE_MAX  1

// types

typedef uint64_t u64;

typedef struct
{
  FILE *fp;

  char buf[BUFSIZ];
  int  len;

} out_t;

typedef struct
{
  int x;
  int y;

} co_t;

typedef struct
{
  int map[DIST_CNT][MOD_CNT][DIR_CNT];

  int is_basic;
  int is_shift;
  int is_altgr;

} cs_t;

typedef struct
{
  int repeat[ROUTE_LENGTH_MAX];
  int changes;

} route_t;

// functions

static const char *USAGE_MINI[] =
{
  "Usage: %s [options]... basechars-file keymap-file routes-file",
  "",
  "Try --help for more help.",
  NULL
};

static const char *USAGE_BIG[] =
{
  "Advanced keyboard-walk generator with configureable basechars, keymap and routes",
  "",
  "Usage: %s [options]... basechars-file keymap-file routes-file",
  "",
  " Options Short / Long        | Type | Description                                                 | Default",
  "=============================+======+=============================================================+=========",
  "  -V, --version              |      | Print version                                               |",
  "  -h, --help                 |      | Print help                                                  |",
  "  -o, --output-file          | FILE | Output-file                                                 |",
  "  -b, --keyboard-basic       | BOOL | Include characters reachable without holding shift or altgr | 1",
  "  -s, --keyboard-shift       | BOOL | Include characters reachable by holding shift               | 0",
  "  -a, --keyboard-altgr       | BOOL | Include characters reachable by holding altgr (non-english) | 0",
  "  -z, --keyboard-all         |      | Shortcut to enable all --keyboard-* modifier                |",
  "  -1, --keywalk-south-west   | BOOL | Include routes heading diagonale south-west                 | 0",
  "  -2, --keywalk-south        | BOOL | Include routes heading straight south                       | 1",
  "  -3, --keywalk-south-east   | BOOL | Include routes heading diagonale south-east                 | 0",
  "  -4, --keywalk-west         | BOOL | Include routes heading straight west                        | 1",
  "  -5, --keywalk-repeat       | BOOL | Include routes repeating character                          | 0",
  "  -6, --keywalk-east         | BOOL | Include routes heading straight east                        | 1",
  "  -7, --keywalk-north-west   | BOOL | Include routes heading diagonale north-west                 | 0",
  "  -8, --keywalk-north        | BOOL | Include routes heading straight north                       | 1",
  "  -9, --keywalk-north-east   | BOOL | Include routes heading diagonale north-east                 | 0",
  "  -0, --keywalk-all          |      | Shortcut to enable all --keywalk-* directions               |",
  "  -n, --keywalk-distance-min | NUM  | Minimum allowed distance between keys                       | 1",
  "  -x, --keywalk-distance-max | NUM  | Maximum allowed distance between keys                       | 1",
  "",
  NULL
};

static void usage_mini_print (const char *progname)
{
  int i;

  for (i = 0; USAGE_MINI[i] != NULL; i++)
  {
    printf (USAGE_MINI[i], progname);

    #ifdef __FreeBSD__
    putchar ('\n');
    #endif

    #ifdef __APPLE__
    putchar ('\n');
    #endif

    #ifdef __linux__
    putchar ('\n');
    #endif

    #ifdef WINDOWS
    putchar ('\r');
    putchar ('\n');
    #endif
  }
}

static void usage_big_print (const char *progname)
{
  int i;

  for (i = 0; USAGE_BIG[i] != NULL; i++)
  {
    printf (USAGE_BIG[i], progname);

    #ifdef __FreeBSD__
    putchar ('\n');
    #endif

    #ifdef __APPLE__
    putchar ('\n');
    #endif

    #ifdef __linux__
    putchar ('\n');
    #endif

    #ifdef WINDOWS
    putchar ('\r');
    putchar ('\n');
    #endif
  }
}

int count_lines (FILE *fp)
{
  int cnt = 0;

  char *buf = (char *) malloc (BUFSIZ);

  char prev = '\n';

  while (!feof (fp))
  {
    size_t nread = fread (buf, sizeof (char), BUFSIZ - 1, fp);

    if (nread < 1) continue;

    size_t i;

    for (i = 0; i < nread; i++)
    {
      if (prev == '\n') cnt++;

      prev = buf[i];
    }
  }

  free (buf);

  return cnt;
}

char hex_convert (const char c)
{
  return (c & 15) + (c >> 6) * 9;
}

void out_flush (out_t *out)
{
  if (out->len == 0) return;

  fwrite (out->buf, 1, out->len, out->fp);

  out->len = 0;
}

void out_push (out_t *out, const char *pw_buf, const int pw_len)
{
  char *ptr = out->buf + out->len;

  memcpy (ptr, pw_buf, pw_len);

  ptr[pw_len] = '\n';

  out->len += pw_len + 1;

  if (out->len >= BUFSIZ - 100)
  {
    out_flush (out);
  }
}

int co_to_chr (const int keymap[KEYMAP_WIDTH][KEYMAP_HEIGHT], const int x, const int y)
{
  if (x < 0) return RC_INVALID;
  if (y < 0) return RC_INVALID;

  if (x > KEYMAP_WIDTH  - 1) return RC_INVALID;
  if (y > KEYMAP_HEIGHT - 1) return RC_INVALID;

  return keymap[x][y];
}

int chr_to_co (const int keymap[KEYMAP_WIDTH][KEYMAP_HEIGHT], const int chr, co_t *co)
{
  for (int x = 0; x < KEYMAP_WIDTH; x++)
  {
    for (int y = 0; y < KEYMAP_HEIGHT; y++)
    {
      if (keymap[x][y] == chr)
      {
        co->x = x;
        co->y = y;

        return RC_OK;
      }
    }
  }

  return RC_INVALID;
}

void add_keymap_to_map (int *map, const int keymap[KEYMAP_WIDTH][KEYMAP_HEIGHT], const co_t *co, const int distance, const int keywalk_south_west, const int keywalk_south, const int keywalk_south_east, const int keywalk_west, const int keywalk_repeat, const int keywalk_east, const int keywalk_north_west, const int keywalk_north, const int keywalk_north_east)
{
  if (keywalk_south_west == 1) map[DIR_SOUTH_WEST] = co_to_chr (keymap, co->x - distance, co->y + distance);
  if (keywalk_south      == 1) map[DIR_SOUTH]      = co_to_chr (keymap, co->x           , co->y + distance);
  if (keywalk_south_east == 1) map[DIR_SOUTH_EAST] = co_to_chr (keymap, co->x + distance, co->y + distance);
  if (keywalk_west       == 1) map[DIR_WEST]       = co_to_chr (keymap, co->x - distance, co->y           );
  if (keywalk_repeat     == 1) map[DIR_REPEAT]     = co_to_chr (keymap, co->x           , co->y           );
  if (keywalk_east       == 1) map[DIR_EAST]       = co_to_chr (keymap, co->x + distance, co->y           );
  if (keywalk_north_west == 1) map[DIR_NORTH_WEST] = co_to_chr (keymap, co->x - distance, co->y - distance);
  if (keywalk_north      == 1) map[DIR_NORTH]      = co_to_chr (keymap, co->x           , co->y - distance);
  if (keywalk_north_east == 1) map[DIR_NORTH_EAST] = co_to_chr (keymap, co->x + distance, co->y - distance);
}

void setup_cs (cs_t *cs, const int c, const int keymap_basic[KEYMAP_WIDTH][KEYMAP_HEIGHT], const int keymap_shift[KEYMAP_WIDTH][KEYMAP_HEIGHT], const int keymap_altgr[KEYMAP_WIDTH][KEYMAP_HEIGHT], const int keyboard_basic, const int keyboard_shift, const int keyboard_altgr, const int keywalk_south_west, const int keywalk_south, const int keywalk_south_east, const int keywalk_west, const int keywalk_repeat, const int keywalk_east, const int keywalk_north_west, const int keywalk_north, const int keywalk_north_east, const int keywalk_distance_min, const int keywalk_distance_max)
{
  for (int i = 0; i < DIST_CNT; i++)
  {
    for (int j = 0; j < MOD_CNT; j++)
    {
      for (int k = 0; k < DIR_CNT; k++)
      {
        cs->map[i][j][k] = RC_INVALID;
      }
    }
  }

  cs->is_basic = 0;
  cs->is_shift = 0;
  cs->is_altgr = 0;

  co_t co;

  int rc = RC_INVALID;

  if (rc == RC_INVALID) { rc = chr_to_co (keymap_basic, c, &co); cs->is_basic = 1; }
  if (rc == RC_INVALID) { rc = chr_to_co (keymap_shift, c, &co); cs->is_shift = 1; }
  if (rc == RC_INVALID) { rc = chr_to_co (keymap_altgr, c, &co); cs->is_altgr = 1; }

  if (rc == RC_INVALID) return;

  for (int distance = keywalk_distance_min; distance <= keywalk_distance_max; distance++)
  {
    if (keyboard_basic == 1) add_keymap_to_map (cs->map[distance][MOD_BASIC], keymap_basic, &co, distance + 1, keywalk_south_west, keywalk_south, keywalk_south_east, keywalk_west, keywalk_repeat, keywalk_east, keywalk_north_west, keywalk_north, keywalk_north_east);
    if (keyboard_shift == 1) add_keymap_to_map (cs->map[distance][MOD_SHIFT], keymap_shift, &co, distance + 1, keywalk_south_west, keywalk_south, keywalk_south_east, keywalk_west, keywalk_repeat, keywalk_east, keywalk_north_west, keywalk_north, keywalk_north_east);
    if (keyboard_altgr == 1) add_keymap_to_map (cs->map[distance][MOD_ALTGR], keymap_altgr, &co, distance + 1, keywalk_south_west, keywalk_south, keywalk_south_east, keywalk_west, keywalk_repeat, keywalk_east, keywalk_north_west, keywalk_north, keywalk_north_east);
  }
}

char *fgetl (FILE *fp, char *buf, int len)
{
  char *line_buf = fgets (buf, len - 1, fp);

  if (line_buf == NULL) return NULL;

  size_t line_len = strlen (line_buf);

  while (line_len)
  {
    if (line_buf[line_len - 1] == '\n')
    {
      line_len--;

      continue;
    }

    if (line_buf[line_len - 1] == '\r')
    {
      line_len--;

      continue;
    }

    break;
  }

  line_buf[line_len] = 0;

  return line_buf;
}

int parse_keymap_file (FILE *fp, int keymap_basic[KEYMAP_WIDTH][KEYMAP_HEIGHT], int keymap_shift[KEYMAP_WIDTH][KEYMAP_HEIGHT], int keymap_altgr[KEYMAP_WIDTH][KEYMAP_HEIGHT])
{
  char *tmp = (char *) malloc (BUFSIZ);

  // first 4 lines are basic

  for (int y = 0; y < 4; y++)
  {
    char *line_buf = fgetl (fp, tmp, BUFSIZ);

    if (line_buf == NULL) continue;

    const size_t line_len = strlen (line_buf);

    for (size_t x = 0; x < line_len; x++)
    {
      unsigned char c = line_buf[x];

      if (c == ' ') continue;

      keymap_basic[x][y] = c;
    }
  }

  // next 4 lines are shift

  for (int y = 0; y < 4; y++)
  {
    char *line_buf = fgetl (fp, tmp, BUFSIZ);

    if (line_buf == NULL) continue;

    const size_t line_len = strlen (line_buf);

    for (size_t x = 0; x < line_len; x++)
    {
      unsigned char c = line_buf[x];

      if (c == ' ') continue;

      keymap_shift[x][y] = c;
    }
  }

  // next 4 lines are altgr

  for (int y = 0; y < 4; y++)
  {
    char *line_buf = fgetl (fp, tmp, BUFSIZ);

    if (line_buf == NULL) continue;

    const size_t line_len = strlen (line_buf);

    for (size_t x = 0; x < line_len; x++)
    {
      unsigned char c = line_buf[x];

      if (c == ' ') continue;

      keymap_altgr[x][y] = c;
    }
  }

  free (tmp);

  return 0;
}

int parse_basechars_file (FILE *fp, char *basechars_buf, int *basechars_int)
{
  char *tmp = (char *) malloc (BUFSIZ);

  char *line_buf = fgetl (fp, tmp, BUFSIZ);

  if (line_buf == NULL) return -1;

  const size_t line_len = strlen (line_buf);

  if (line_len <   1) return -1;
  if (line_len > 256) return -1;

  memcpy (basechars_buf, line_buf, line_len);

  *basechars_int = line_len;

  free (tmp);

  return 0;
}

int parse_routes_file (FILE *fp, route_t *routes_buf)
{
  char *tmp = (char *) malloc (BUFSIZ);

  int routes_cnt = 0;

  while (!feof (fp))
  {
    char *line_buf = fgetl (fp, tmp, BUFSIZ);

    if (line_buf == NULL) continue;

    const size_t line_len = strlen (line_buf);

    if (line_len < ROUTE_LENGTH_MIN) continue;
    if (line_len > ROUTE_LENGTH_MAX) continue;

    route_t *route = routes_buf + routes_cnt;

    route->changes = 0;

    for (size_t line_pos = 0; line_pos < line_len; line_pos++)
    {
      const char c = line_buf[line_pos];

      const int repeat = hex_convert (c);

      if (repeat < ROUTE_REPEAT_MIN) continue;
      if (repeat > ROUTE_REPEAT_MAX) continue;

      route->repeat[route->changes] = repeat;

      route->changes++;
    }

    routes_cnt++;
  }

  free (tmp);

  return routes_cnt;
}

int process_route (const cs_t *css, const int root, const u64 s, const route_t *route_buf, char *out_buf, int *out_len)
{
  int out_pos = 0;

  out_buf[out_pos] = root;

  out_pos++;

  const cs_t *cs = css + root;

  u64 left = s;

  u64 prev_direction = -1;

  for (int route_pos = 0; route_pos < route_buf->changes; route_pos++)
  {
    const u64 m1 = left % SELECTION_CNT;

    if (prev_direction == m1) return RC_INVALID;

    prev_direction = m1;

    const u64 m_distance  = m1 % DIST_CNT; const u64 m2 = m1 / DIST_CNT;
    const u64 m_modifier  = m2 % MOD_CNT;  const u64 m3 = m2 / MOD_CNT;
    const u64 m_direction = m3;

    for (int r = 0; r < route_buf->repeat[route_pos]; r++)
    {
      const int c = cs->map[m_distance][m_modifier][m_direction];

      if (c == RC_INVALID) return RC_INVALID;

      out_buf[out_pos] = c;

      out_pos++;

      cs = css + c;
    }

    left /= SELECTION_CNT;
  }

  *out_len = out_pos;

  out_buf[out_pos] = 0;

  return RC_OK;
}

int main (int argc, char *argv[])
{
  /* usage */

  int   version              = 0;
  int   usage                = 0;
  char *output_file          = NULL;
  int   keyboard_basic       = KEYBOARD_BASIC;
  int   keyboard_shift       = KEYBOARD_SHIFT;
  int   keyboard_altgr       = KEYBOARD_ALTGR;
  int   keyboard_all         = KEYBOARD_ALL;
  int   keywalk_south_west   = KEYWALK_SOUTH_WEST;
  int   keywalk_south        = KEYWALK_SOUTH;
  int   keywalk_south_east   = KEYWALK_SOUTH_EAST;
  int   keywalk_west         = KEYWALK_WEST;
  int   keywalk_repeat       = KEYWALK_REPEAT;
  int   keywalk_east         = KEYWALK_EAST;
  int   keywalk_north_west   = KEYWALK_NORTH_WEST;
  int   keywalk_north        = KEYWALK_NORTH;
  int   keywalk_north_east   = KEYWALK_NORTH_EAST;
  int   keywalk_all          = KEYWALK_ALL;
  int   keywalk_distance_min = KEYWALK_DISTANCE_MIN;
  int   keywalk_distance_max = KEYWALK_DISTANCE_MAX;

  #define IDX_VERSION              'V'
  #define IDX_USAGE                'h'
  #define IDX_OUTPUT_FILE          'o'
  #define IDX_KEYBOARD_BASIC       'b'
  #define IDX_KEYBOARD_SHIFT       's'
  #define IDX_KEYBOARD_ALTGR       'a'
  #define IDX_KEYBOARD_ALL         'z'
  #define IDX_KEYWALK_SOUTH_WEST   '1'
  #define IDX_KEYWALK_SOUTH        '2'
  #define IDX_KEYWALK_SOUTH_EAST   '3'
  #define IDX_KEYWALK_WEST         '4'
  #define IDX_KEYWALK_REPEAT       '5'
  #define IDX_KEYWALK_EAST         '6'
  #define IDX_KEYWALK_NORTH_WEST   '7'
  #define IDX_KEYWALK_NORTH        '8'
  #define IDX_KEYWALK_NORTH_EAST   '9'
  #define IDX_KEYWALK_ALL          '0'
  #define IDX_KEYWALK_DISTANCE_MIN 'n'
  #define IDX_KEYWALK_DISTANCE_MAX 'x'

  struct option long_options[] =
  {
    {"version",               no_argument,       0, IDX_VERSION},
    {"help",                  no_argument,       0, IDX_USAGE},
    {"output-file",           required_argument, 0, IDX_OUTPUT_FILE},
    {"keyboard-basic",        required_argument, 0, IDX_KEYBOARD_BASIC},
    {"keyboard-shift",        required_argument, 0, IDX_KEYBOARD_SHIFT},
    {"keyboard-altgr",        required_argument, 0, IDX_KEYBOARD_ALTGR},
    {"keyboard-all",          no_argument,       0, IDX_KEYBOARD_ALL},
    {"keywalk-south-west",    required_argument, 0, IDX_KEYWALK_SOUTH_WEST},
    {"keywalk-south",         required_argument, 0, IDX_KEYWALK_SOUTH},
    {"keywalk-south-east",    required_argument, 0, IDX_KEYWALK_SOUTH_EAST},
    {"keywalk-west",          required_argument, 0, IDX_KEYWALK_WEST},
    {"keywalk-repeat",        required_argument, 0, IDX_KEYWALK_REPEAT},
    {"keywalk-east",          required_argument, 0, IDX_KEYWALK_EAST},
    {"keywalk-north-west",    required_argument, 0, IDX_KEYWALK_NORTH_WEST},
    {"keywalk-north",         required_argument, 0, IDX_KEYWALK_NORTH},
    {"keywalk-north-east",    required_argument, 0, IDX_KEYWALK_NORTH_EAST},
    {"keywalk-all",           no_argument,       0, IDX_KEYWALK_ALL},
    {"keywalk-distance-min",  required_argument, 0, IDX_KEYWALK_DISTANCE_MIN},
    {"keywalk-distance-max",  required_argument, 0, IDX_KEYWALK_DISTANCE_MAX},
    {0, 0, 0, 0}
  };

  int option_index = 0;

  int c;

  while ((c = getopt_long (argc, argv, "Vho:b:s:a:z1:2:3:4:5:6:7:8:9:0n:x:", long_options, &option_index)) != -1)
  {
    switch (c)
    {
      case IDX_VERSION:               version               = 1;             break;
      case IDX_USAGE:                 usage                 = 1;             break;
      case IDX_OUTPUT_FILE:           output_file           = optarg;        break;
      case IDX_KEYBOARD_BASIC:        keyboard_basic        = atoi (optarg); break;
      case IDX_KEYBOARD_SHIFT:        keyboard_shift        = atoi (optarg); break;
      case IDX_KEYBOARD_ALTGR:        keyboard_altgr        = atoi (optarg); break;
      case IDX_KEYBOARD_ALL:          keyboard_all          = 1;             break;
      case IDX_KEYWALK_SOUTH_WEST:    keywalk_south_west    = atoi (optarg); break;
      case IDX_KEYWALK_SOUTH:         keywalk_south         = atoi (optarg); break;
      case IDX_KEYWALK_SOUTH_EAST:    keywalk_south_east    = atoi (optarg); break;
      case IDX_KEYWALK_WEST:          keywalk_west          = atoi (optarg); break;
      case IDX_KEYWALK_REPEAT:        keywalk_repeat        = atoi (optarg); break;
      case IDX_KEYWALK_EAST:          keywalk_east          = atoi (optarg); break;
      case IDX_KEYWALK_NORTH_WEST:    keywalk_north_west    = atoi (optarg); break;
      case IDX_KEYWALK_NORTH:         keywalk_north         = atoi (optarg); break;
      case IDX_KEYWALK_NORTH_EAST:    keywalk_north_east    = atoi (optarg); break;
      case IDX_KEYWALK_ALL:           keywalk_all           = 1;             break;
      case IDX_KEYWALK_DISTANCE_MIN:  keywalk_distance_min  = atoi (optarg); break;
      case IDX_KEYWALK_DISTANCE_MAX:  keywalk_distance_max  = atoi (optarg); break;

      default: return (-1);
    }
  }

  // some sanity checks

  if (keywalk_distance_min < 1)
  {
    fprintf (stderr, "Keywalk distance minimum can not be smaller than than 1\n");

    return (-1);
  }

  if (keywalk_distance_max < 1)
  {
    fprintf (stderr, "Keywalk distance maximum can not be smaller than than 1\n");

    return (-1);
  }

  if (keywalk_distance_min > DIST_CNT)
  {
    fprintf (stderr, "Keywalk distance minimum can not be greater than than %d\n", DIST_CNT);

    return (-1);
  }

  if (keywalk_distance_max > DIST_CNT)
  {
    fprintf (stderr, "Keywalk distance maximum can not be greater than than %d\n", DIST_CNT);

    return (-1);
  }

  if (keywalk_distance_min > keywalk_distance_max)
  {
    fprintf (stderr, "Keywalk distance minimum can not be greater than maximum\n");

    return (-1);
  }

  // shortcuts always override

  if (keyboard_all)
  {
    keyboard_basic     = 1;
    keyboard_shift     = 1;
    keyboard_altgr     = 1;
  }

  if (keywalk_all)
  {
    keywalk_south_west = 1;
    keywalk_south      = 1;
    keywalk_south_east = 1;
    keywalk_west       = 1;
    keywalk_repeat     = 1;
    keywalk_east       = 1;
    keywalk_north_west = 1;
    keywalk_north      = 1;
    keywalk_north_east = 1;
  }

  // version and usage

  if (usage)
  {
    usage_big_print (argv[0]);

    return (-1);
  }

  if (version)
  {
    printf ("v%4.02f\n", (double) VERSION_BIN / 100);

    return (-1);
  }

  if ((optind + 3) != argc)
  {
    usage_mini_print (argv[0]);

    return (-1);
  }

  /* files out */

  #ifdef WINDOWS
  setmode (fileno (stdout), O_BINARY);
  setmode (fileno (stderr), O_BINARY);
  #endif

  FILE *fp_out = stdout;

  if (output_file)
  {
    if ((fp_out = fopen (output_file, "ab")) == NULL)
    {
      fprintf (stderr, "ERROR: %s: %s\n", output_file, strerror (errno));

      return (-1);
    }
  }

  setbuf (fp_out, NULL);

  out_t *out = (out_t *) malloc (sizeof (out_t));

  out->fp  = fp_out;
  out->len = 0;

  // some stuff

  char *basechar_file = argv[optind + 0];
  char *keymap_file   = argv[optind + 1];
  char *routes_file   = argv[optind + 2];

  // init basechars

  int basechars_cnt = 0;

  char *basechars_buf = (char *) malloc (256);

  FILE *fp = fopen (basechar_file, "r");

  if (fp == NULL)
  {
    fprintf (stderr, "%s: %s\n", basechar_file, strerror (errno));

    return -1;
  }

  const int basechars_pot = count_lines (fp);

  if (basechars_pot != 1)
  {
    fprintf (stderr, "Invalid basechars, not exactly 1 line\n");

    return -1;
  }

  rewind (fp);

  int rc = parse_basechars_file (fp, basechars_buf, &basechars_cnt);

  if (rc == -1)
  {
    fprintf (stderr, "%s: Invalid basechars\n", basechar_file);

    return -1;
  }

  fclose (fp);

  // init keymaps

  int keymap_basic[KEYMAP_WIDTH][KEYMAP_HEIGHT];
  int keymap_shift[KEYMAP_WIDTH][KEYMAP_HEIGHT];
  int keymap_altgr[KEYMAP_WIDTH][KEYMAP_HEIGHT];

  for (int x = 0; x < KEYMAP_WIDTH; x++)
  {
    for (int y = 0; y < KEYMAP_HEIGHT; y++)
    {
      keymap_basic[x][y] = RC_INVALID;
      keymap_shift[x][y] = RC_INVALID;
      keymap_altgr[x][y] = RC_INVALID;
    }
  }

  fp = fopen (keymap_file, "r");

  if (fp == NULL)
  {
    fprintf (stderr, "%s: %s\n", keymap_file, strerror (errno));

    return -1;
  }

  const int keymap_pot = count_lines (fp);

  if (keymap_pot != 12)
  {
    fprintf (stderr, "Invalid keymap, not exactly 12 lines\n");

    return -1;
  }

  rewind (fp);

  rc = parse_keymap_file (fp, keymap_basic, keymap_shift, keymap_altgr);

  if (rc == -1)
  {
    fprintf (stderr, "%s: Invalid keymap\n", keymap_file);

    return -1;
  }

  fclose (fp);

  // init charset

  cs_t *css = (cs_t *) calloc (256, sizeof (cs_t));

  for (int c = 0; c < 256; c++)
  {
    setup_cs (css + c, c, keymap_basic, keymap_shift, keymap_altgr, keyboard_basic, keyboard_shift, keyboard_altgr, keywalk_south_west, keywalk_south, keywalk_south_east, keywalk_west, keywalk_repeat, keywalk_east, keywalk_north_west, keywalk_north, keywalk_north_east, keywalk_distance_min - 1, keywalk_distance_max - 1);
  }

  // init routes

  fp = fopen (routes_file, "r");

  if (fp == NULL)
  {
    fprintf (stderr, "%s: %s\n", routes_file, strerror (errno));

    return -1;
  }

  const int routes_pot = count_lines (fp);

  rewind (fp);

  route_t *routes_buf = (route_t *) calloc (routes_pot, sizeof (route_t));

  int routes_cnt = parse_routes_file (fp, routes_buf);

  if (routes_cnt == 0)
  {
    fprintf (stderr, "%s: no routes load\n", routes_file);

    return -1;
  }

  fclose (fp);

  // main loop

  for (int routes_pos = 0; routes_pos < routes_cnt; routes_pos++)
  {
    route_t *route_buf = routes_buf + routes_pos;

    // from here we're going to bf "a route".
    // there's a total number of "direction changes" (which is like a length for a bf algorithm)
    // but the real password length is the sum of all repeats of all "direction changes"
    // anyway, we brute-force all direction types for each change here to produce something like (route 313):
    // - Iteration 1: "3*North, 1* West, 3*South"
    // - Iteration 2: "3*North, 1* East, 3*South"
    // - Iteration N: "3*South-East-Shifted, 1*North, 3*South-East-Alt"

    u64 keyspace = basechars_cnt;

    for (int i = 0; i < route_buf->changes; i++)
    {
      keyspace *= SELECTION_CNT;
    }

    for (u64 k = 0; k < keyspace; k++)
    {
      const u64 km = k % basechars_cnt;
      const u64 kd = k / basechars_cnt;

      const int c = basechars_buf[km];

      if ((keyboard_basic == 0) && (css[c].is_basic == 1)) continue;
      if ((keyboard_shift == 0) && (css[c].is_shift == 1)) continue;
      if ((keyboard_altgr == 0) && (css[c].is_altgr == 1)) continue;

      char pw_buf[100];

      int pw_len = 0;

      const int rc = process_route (css, c, kd, route_buf, pw_buf, &pw_len);

      if (rc == RC_INVALID) continue;

      out_push (out, pw_buf, pw_len);
    }
  }

  out_flush (out);

  free (routes_buf);
  free (css);
  free (out);

  return 0;
}
