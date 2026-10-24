#ifndef LOOKEDIT_H
#define LOOKEDIT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <time.h>
#include <signal.h>

#define MAX_TABS        8
#define MAX_LINE_LEN    1024
#define MAX_LINES       1024
#define MAX_CMD_BUF     128
#define MAX_HISTORY     128
#define TAB_SPACE       "    "
#define TAB_WIDTH       4
#define APP_NAME        "lookedit"

typedef struct {
    char name[128];
    char buf[MAX_LINES][MAX_LINE_LEN];
    int line_count;
    int dirty;
    int cx, cy;
    int view_x;
    int view_y;
} Tab;

extern Tab tabs[MAX_TABS];
extern int tab_count;
extern int blink_state;
extern int current_tab;
extern int cmd_mode;
extern char cmd_buf[MAX_CMD_BUF];
extern int cmd_pos;
extern int cmd_cursor;
extern char base_dir[512];
extern char log_dir[512];
extern char crash_dir[512];
extern char history_path[512];
extern char last_log_path[512];
extern int dirty_redraw;
extern char cmd_history[MAX_HISTORY][MAX_CMD_BUF];
extern int history_cnt;
extern int history_idx;
extern int tab_scroll_x;

void tty_raw_enable(void);
void tty_raw_disable(void);
void get_term_size(int *w, int *h);
void clear_screen(void);
void draw(void);
void get_term_size(int *w, int *h);
void cursor_pageup(void);
void cursor_pagedown(void);

void tab_init(Tab *tab, const char *fname);
void tab_new(const char *fname);
void tab_switch(int delta);
void adjust_scroll(Tab *t, int tw, int th);
void insert_char(char c);
void insert_tab(void);
int tab_close(int force);
void backspace(void);
void newline(void);
void cursor_up(void);
void cursor_down(void);
void cursor_left(void);
void cursor_right(void);
void load_content(Tab *t, const char *text);
void file_load(Tab *t, const char *path, int force);
void load_content(Tab *t, const char *text);
void buffer_free(Tab *t);

void mkdir_p(const char *path);
void setup_paths(void);
void log_write(const char *msg);
void log_error(const char *msg);
void crash_handler(int sig);
int file_save(Tab *t, int overwrite);
void load_history(void);
void save_history(const char *cmd);

void run_command(void);
void cmd_input_handle(char c);

#endif
