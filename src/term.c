#include "lookedit.h"
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

static struct termios old_tty;

int tab_scroll_x = 0;
int view_h_global = 0;

void cursor_pageup(void) {
    Tab *t = &tabs[current_tab];
    int step = view_h_global > 0 ? view_h_global : 10;
    t->cy -= step;
    if(t->cy < 0) t->cy = 0;
    int maxcol = strlen(t->buf[t->cy]);
    if(t->cx > maxcol) t->cx = maxcol;
}

void cursor_pagedown(void) {
    Tab *t = &tabs[current_tab];
    int step = view_h_global > 0 ? view_h_global : 10;
    t->cy += step;
    if(t->cy >= t->line_count) t->cy = t->line_count - 1;
    int maxcol = strlen(t->buf[t->cy]);
    if(t->cx > maxcol) t->cx = maxcol;
}

void tty_raw_enable(void) {
    struct termios t;
    tcgetattr(STDIN_FILENO, &old_tty);
    t = old_tty;
    t.c_lflag &= ~(ICANON | ECHO);
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
}

void tty_raw_disable(void) {
    tcsetattr(STDIN_FILENO, TCSANOW, &old_tty);
}

void get_term_size(int *w, int *h) {
    struct winsize ws;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
    *w = ws.ws_col;
    *h = ws.ws_row;
}

void clear_screen(void) {
    write(STDOUT_FILENO, "\033[2J\033[H", 6);
}

void draw(void) {
    int tw, th;
    get_term_size(&tw, &th);
    Tab *t = &tabs[current_tab];
    adjust_scroll(t, tw, th);

    write(STDOUT_FILENO, "\033[?2026h", 8);
    clear_screen();

    printf("\033[1;1H");
    printf("\033[K");
    int print_pos = 0;
    for(int i = tab_scroll_x; i < tab_count; i++)
    {
        char buf[128];
        if(i == current_tab)
        {
            snprintf(buf, sizeof(buf), "\033[7m %s \033[0m", tabs[i].name);
        }else{
            snprintf(buf, sizeof(buf), " %s ", tabs[i].name);
        }
        int len = strlen(buf);
        if(print_pos + len > tw) break;
        printf("%s", buf);
        print_pos += len;
    }

    int reserve_lines = 3;
    int view_h = th - reserve_lines;
    if (view_h < 1) view_h = 1;
    int line_num_width = 5;

    for (int i = 0; i < view_h; i++) {
        int line_idx = t->view_y + i;
        printf("\033[%d;1H", i + 2);
        printf("\033[K");
        if (line_idx >= t->line_count) {
            printf("\033[37m     \033[0m");
            continue;
        }
        if (line_idx == t->cy) {
            printf("\033[32m%4d \033[0m", line_idx + 1);
        } else {
            printf("\033[37m%4d \033[0m", line_idx + 1);
        }
        const char *line = t->buf[line_idx];
        int printed = 0;
        for (int p = t->view_x; line[p] && printed < (tw - line_num_width); p++) {
            putchar(line[p]);
            printed++;
        }
    }
    
    view_h_global = view_h;

    char text_buf[512];
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char dirty_mark = t->dirty ? '*' : ' ';
    snprintf(text_buf, sizeof(text_buf), "%c %s | Ln:%d Col:%d | %04d-%02d-%02d",
        dirty_mark, t->name, t->cy + 1, t->cx + 1,
        tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);

    char visible[256];
    int max_visible = tw - 2;
    int len = strlen(text_buf);
    if (len > max_visible) len = max_visible;
    strncpy(visible, text_buf, len);
    visible[len] = '\0';

    int status_line = view_h + 2;
    int cmd_line = view_h + 3;

    printf("\033[%d;1H", status_line);
    printf("\033[K");
    printf("\033[47;30m%s", visible);
    int fill = tw - strlen(visible);
    for(int i = 0; i < fill; i++) putchar(' ');
    printf("\033[0m");

    if (cmd_mode) {
        printf("\033[%d;1H", cmd_line);
        printf("\033[K>");
        fputs(cmd_buf, stdout);
        printf("\033[%d;%dH", cmd_line, 2 + cmd_cursor);
    } else {
        int screen_y = t->cy - t->view_y + 1 + 1;
        int screen_x = t->cx - t->view_x + line_num_width + 1;
        printf("\033[%d;%dH", screen_y, screen_x);
    }
    fflush(stdout);
    write(STDOUT_FILENO, "\033[?2026l", 8);
    dirty_redraw = 0;
}
