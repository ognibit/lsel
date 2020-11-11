#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

struct {
    char *(*fstrstr)(const char *, const char *);
    bool autoselect;
    bool multiselect;
    bool numbers;
    char *prompt;
} config;

#include "config.h"

static int height;           /* terminal window rows */
static int width;            /* terminal window columns */
static int print_offset;     /* number of char used to print linenum */

const char * help_msg = "Help \n\
version: %s \n\
usage: %s [-a|--autoselect] [-i|--insensitive] [-m|--multiselect] [-n|--numbers] [-p|--prompt prompt] [-h|--help] \n\
\n\
Options: \n\
    -a, --autoselect         the line with cursor is also selected (no effect with -m) \n\
    -i, --insensitive        enable case insensitive matching \n\
    -m, --multiselect        select and output more than one line \n\
    -n, --numbers            display line numbers \n\
    -p, --prompt prompt      the prompt displayed to the search bar \n\
    -h, --help               display this help message \n\
\n\
Example: \n\
    cat file.txt | lsel -m -i > custom_selection.txt \n\
";


static struct option const long_options[] = {
    {"autoselect",     no_argument, NULL, 'a'},
    {"insensitive",    no_argument, NULL, 'i'},
    {"multiselect",    no_argument, NULL, 'm'},
    {"numbers",        no_argument, NULL, 'n'},
    {"prompt",         required_argument, NULL, 'p'},
    {"help",           no_argument, NULL, 'h'},
    {NULL, 0, NULL, 0}
};

enum {
    LINES_INIT_LEN   = 1024,  /* initial number of input lines */
    INPUT_LEN        = 80,    /* max length for match string */
    WIN_ROWS_DEFAULT = 24,    /* default terminal height */
    WIN_COLS_DEFAULT = 80,    /* default terminal width */
};

/* each input line is stored with metadata */
typedef struct {
    bool selected; 
    char *line;    /* string without \n */
} MenuLine;


/* put selected in all items in lines */
void menuline_select(MenuLine lines[], size_t nlines, bool selected);

/* 
 * put in matches ther reference to lines that contains s
 * mlines is modified as len of matches
 *
 * return true if the matches changes
 */
bool menuline_match(MenuLine lines[],
                    size_t nlines,
                    char *s,
                    MenuLine *matches[],
                    size_t *mlines);

/* print the matches around the cursor in the terminal page */
void menuline_print(MenuLine *ml[], size_t n, size_t cur, char *fmt);

/* Translate a printable char to a control char */
char ctocntrl(char c);

/* returns the number of digits to represent the number */
int numdigits(size_t n);

void clean_line();

void clean_screen(int height);

void win_dimesions();

void die(const char *msg);

/*
 * Do the operations to understand the control character read from the stdin
 * and return if the caller (prompt) need to continue looping.
 * The cursor argument is modified based on the control.
 * mlines is the upper limit for the cursor
 */
bool handle_control(size_t *cursor, const size_t mlines);


/*
 * The core function, print the line on stderr and manage the stdin filter
 * lines: all the string to display
 * nlines: the length of the complete lines set
 * height: page dimension, to diplay
 */
void prompt(MenuLine lines[], size_t nlines, size_t height);

void print_help(char *name);


int main(int argc, char *argv[])
{
    struct termios tio_old, tio_new;

    size_t i;
    ssize_t n;
    size_t len;
    char *line, *p;
    MenuLine *lines; /* original lines from input, array */
    size_t nlines; /* number of element filled into lines */
    char c;

    config_init();

    /* manage options */
    while ((c = getopt_long(argc, argv, "aimnhp:", long_options, NULL)) > 0){
        switch(c){
        case 'a':
            config.autoselect = true;
            break;
        case 'i': /* case insensitive */
            config.fstrstr = strcasestr;
            break;
        case 'm': /* enable multiselect */
            config.multiselect = true;
            break;
        case 'n': /* display line numbers */
            config.numbers = true;
            break;
        case 'h': /* help */
            print_help(argv[0]);
            exit(EXIT_SUCCESS); 
            break;
        case 'p': /* prompt */
            config.prompt = optarg;
            break;
        default:
            fprintf(stderr, "%s --help for usage\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    /* autoselect works only in single selection mode */
    if (config.multiselect){
        config.autoselect = false;
    }

    /* read lines from stdin */
    lines = calloc(LINES_INIT_LEN, sizeof(lines[0]));
    if (lines == NULL)
        die("no memory for input lines");

    nlines = 0;
    line = NULL;
    len = 0;
    while ((n = getline(&line, &len, stdin)) >= 0){
        if ((p = strchr(line, '\n')) != NULL){
            *p = '\0'; /* remove \n if exists */
        }
        lines[nlines].line = strdup(line);
        /* initialy nothing is selected */
        lines[nlines].selected = false; 
        nlines++;
        /* check if it needs more space */
        if (nlines % LINES_INIT_LEN == 0){
            lines = realloc(lines, (nlines + LINES_INIT_LEN) * sizeof(lines[0]));
            if (lines == NULL)
                die("no memory for input lines (expand)");
        }
    }
    free(line);


    /* get the rows and cols of the terminal (width, height) */ 
    win_dimesions();
    /*minus one line for the prompt */
    height--;

    /* re-open stdin to read keyboard */
    if (freopen("/dev/tty", "r", stdin) == NULL) {
        die("Can't reopen tty.");
    }
    /* setup the terminal, saving the previous values */
    tcgetattr(0, &tio_old);
    memcpy ((char *)&tio_new, (char *)&tio_old, sizeof(struct termios));
    tio_new.c_iflag &= ~(BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
    tio_new.c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
    tio_new.c_cflag &= ~(CSIZE|PARENB);
    tio_new.c_cflag |= CS8;
    tio_new.c_cc[VMIN]=1;
    tcsetattr(0, TCSANOW, &tio_new);

    /* search the line */
    prompt(lines, nlines, height);
    /* clean up */
    clean_screen(height);

    /* put the selection to stdout, if any */
    len = 0;
    for (i=0; i<nlines; i++){
        if (lines[i].selected){
            printf("%s\n", lines[i].line);
            len++;
        }
    }


    /* exit */
    for (i=0; i<nlines; i++){
        free(lines[i].line);
    }
    free(lines);

    /* restore the previous terminal setup */
    tcsetattr(0, TCSANOW, &tio_old);

    /* if no lines selected, exit with error to manage the case in shell */
    return len == 0;
}

void menuline_select(MenuLine lines[], size_t nlines, bool selected)
{
    int i;

    for (i=0; i < nlines; i++){
        lines[i].selected = selected;
    }
}

char ctocntrl(char c)
{
    return c ^ 0x40;
}

int numdigits(size_t n)
{
    int digits;

    digits = 1;
    while((n /=10) > 9){
       digits++;
    }

    return digits;
}

void clean_line()
{
    fprintf(stderr, "\033[G"); // begin of line
    fprintf(stderr, "\033[K"); //ERASE LINE from current column
    fprintf(stderr, "\033[1A"); //move to 1 line up
}

void clean_screen(int height)
{
    clean_line(); // the input line

    for (int i=0; i < height; i++){ //the page
        clean_line();
    }
}

void win_dimesions()
{
    int fd, result=-1;
    struct winsize ws;

    height = WIN_ROWS_DEFAULT;
    width  = WIN_COLS_DEFAULT;

    fd = open("/dev/tty", O_RDWR);
    if (fd != -1) {
        result = ioctl(fd, TIOCGWINSZ, &ws);
        close(fd);
        if (result>=0) {
            height = ws.ws_row;
            width  = ws.ws_col;
        }
    }
}

void die(const char *msg)
{
    fprintf(stderr, "FATAL: %s\n", msg);
    exit(EXIT_FAILURE);
}

bool handle_control(size_t *cursor, const size_t mlines)
{
    char c;
    bool finish;

    finish = false;
    c = getchar();
    if (c == '['){
        c = getchar();
        if (c == 'A'){             /* arrow up */
            *cursor = (*cursor > 0)?*cursor -1:0;
        } else if (c == 'B'){      /* arrow down */
            *cursor = (*cursor < mlines-1)?*cursor +1:*cursor;
        } else if (c == '5'){      /* page up */
            getchar(); /* remove the ~ from input, side effect of key */
            *cursor = (*cursor > height)?*cursor -height:0;
        } else if (c == '6'){      /* page down */
            getchar(); /* remove the ~ from input, side effect of key */
            *cursor = (*cursor < mlines-height)?*cursor +height:*cursor;
        }
        /* ignore other controls */
    } else if (c == ctocntrl('[')){/* ESC */
        finish = true; /* exit from prompt, nothing selected */
    }
    
    return finish;
}

bool menuline_match(MenuLine lines[],
                 size_t nlines,
                 char *s,
                 MenuLine *matches[],
                 size_t *mlines)
{
    size_t i;
    size_t mlines_orig;

    mlines_orig = *mlines;
    /* linear search */
    *mlines = 0;
    for (i=0; i < nlines; i++){
        if (config.fstrstr(lines[i].line, s)){
            matches[*mlines] = &lines[i];
            *mlines += 1;
        }
    }

    return mlines_orig != *mlines;
}

/* print the matches around the cursor in the terminal page */
void menuline_print(MenuLine *ml[], size_t n, size_t cur, char *fmt)
{
    size_t cpage;
    char c, s;
    int i;
    char buf[width];

    cpage = cur / height;

    for (i=height*cpage; i < height*(cpage+1); i++){
        c = (i == cur)?'>':' ';
        if (i < n){
            s = (ml[i]->selected)?'*':' ';
            /* keep space for the 2 initial char, \0 and linenum */
            strncpy(buf, ml[i]->line, width-1);
            buf[width-2-print_offset] = '\0';
            if (config.numbers){
                /* line num start from 1 */
                fprintf(stderr, fmt, i+1, s, c, buf);
            } else {
                fprintf(stderr, "%c%c%s\n", s, c, buf);
            }
        } else {
            fprintf(stderr, "\n");
        }
    }
}

/*
 * The core function, print the line on stderr and manage the stdin filter
 * lines: all the string to display
 * nlines: the length of the complete lines set
 * height: page dimension, to diplay
 */
void prompt(MenuLine lines[], size_t nlines, size_t height)
{
    size_t mlines, cursor;
    size_t pages;
    char c;
    char input[INPUT_LEN];
    size_t ii; /* \0 index in input*/
    bool finish;
    MenuLine **matches; /* array of MenuLine pointers */
    MenuLine *psel;     /* pointer to selected line, for autoselection */
    char fmt[20];

    print_offset = 0;
    if (config.numbers){
        print_offset = numdigits(nlines);
        /* build the expression '%Xld%c%c%s\n' where X is the number of digits
        * to have the column of line number larger enough to contains nlines
        */
        sprintf(fmt, "%%%dld%%c%%c%%s\n", print_offset);
    }

    cursor = 0;
    mlines = 0;
    /* no matches is the all the lines */
    matches = calloc(nlines, sizeof(matches[0]));
    if (matches == NULL)
        die("no memory for matching lines");
    /* at the beginig show all the lines (match "") */
    menuline_match(lines, nlines, "", matches, &mlines); 

    ii = 0;
    input[0] = '\0';

    pages = mlines / height;
    if (mlines % height != 0)
        pages++;

    finish = false;
    psel = NULL;

    while (!finish){

        clean_screen(height);

        if (config.autoselect) {
            /* unselect the previous and select the current */
            if (psel != NULL){
                psel->selected = false;
            }
            psel = (mlines > 0)?matches[cursor]:NULL;
            if (psel != NULL){
                psel->selected = true;
            }
        }

        /* print the matches */
        menuline_print(matches, mlines, cursor, fmt);

        /* command line */
        fprintf(stderr, "%ld/%ld %s>", mlines, mlines, config.prompt);
        fprintf(stderr, "%s", input);
        c = getchar();

        /* handle the user input */
        if (c == ctocntrl('[')){
            finish = handle_control(&cursor, mlines);
            /* exit rejecting the selections */
            if (finish){
                /* work on lines, the entire set */
                menuline_select(lines, nlines, false);
            }
        } else {                              /* printable char */
            if (c == 127){                    /* backspace */
                if (ii > 0){
                    input[ii-1] = '\0';
                    ii--;
                }
            } else if (c == '\n' || c == 13){ /* ENTER */
                /* exit the loop, cleaning the stderr */
                finish = true;
            } else if (c == '\t'){            /* TAB for selection */
                if (! config.multiselect){
                    menuline_select(lines, nlines, false);
                }
                matches[cursor]->selected = !matches[cursor]->selected;
            } else {                          /* alphanum */
                /* ignore longer input text */
                if (ii < INPUT_LEN-1){
                    input[ii] = c;
                    input[ii+1] = '\0';
                    ii++;
                }
            }

            /* apply the filter and reset the cursor if matches changed */
            if (menuline_match(lines, nlines, input, matches, &mlines)){
                cursor = 0;
            }
        }


    }/* while not finish */

    free(matches);
}

void print_help(char *name)
{
    fprintf(stderr, help_msg, VERSION, name);
}
