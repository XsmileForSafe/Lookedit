#include "lookedit.h"
#include <sys/select.h>
#include <time.h>

static void exit_restore_screen(void)
{
    write(STDOUT_FILENO, "\033[2J\033[H", 7);
    write(STDOUT_FILENO, "\033[?1049l", 8);
}

int main(int argc, char** argv) {
    setup_paths();
    load_history();
    signal(SIGSEGV, crash_handler);

    if(argc > 1) {
        tab_init(&tabs[0], argv[1]);
        file_load(&tabs[0], argv[1], 0);
        for(int i = 2; i < argc; i++) {
            tab_new(argv[i]);
            Tab *t = &tabs[current_tab];
            file_load(t, argv[i], 0);
        }
    } else {
        tab_init(&tabs[0], "untitled.txt");
        load_content(&tabs[0], "");
    }

    log_write("lookedit started");
    tty_raw_enable();
    atexit(tty_raw_disable);
    atexit(exit_restore_screen);

    write(STDOUT_FILENO, "\033[?1049h", 8);
    write(STDOUT_FILENO, "\033[?25h", 5);

    time_t last_time_tick = 0;
    dirty_redraw = 1;

    for(;;) {
        fd_set rfds;
        struct timeval tv;
        FD_ZERO(&rfds);
        FD_SET(STDIN_FILENO, &rfds);
        tv.tv_sec = 0;
        tv.tv_usec = 100000;
        int ret = select(STDIN_FILENO+1, &rfds, NULL, NULL, &tv);

        time_t now_t = time(NULL);
        if(now_t != last_time_tick){
            last_time_tick = now_t;
            dirty_redraw = 1;
        }

        if (ret == 0) {
            blink_state = !blink_state;
            if (blink_state) {
                write(STDOUT_FILENO, "\033[?25h", 5);
            } else {
                write(STDOUT_FILENO, "\033[?25l", 5);
            }
            if(dirty_redraw) draw();
            continue;
        }

        char c;
        ssize_t r = read(STDIN_FILENO, &c, 1);
        if (r <= 0) continue;

        write(STDOUT_FILENO, "\033[?25h", 5);
        blink_state = 1;

        if (c == 27) {
            fd_set rfds_esc;
            struct timeval tv_esc;
            FD_ZERO(&rfds_esc);
            FD_SET(STDIN_FILENO, &rfds_esc);
            tv_esc.tv_sec = 0;
            tv_esc.tv_usec = 0;
            int has_seq = select(STDIN_FILENO + 1, &rfds_esc, NULL, NULL, &tv_esc);
            if (has_seq <= 0) {
                cmd_mode = !cmd_mode;
                if (!cmd_mode) {
                    memset(cmd_buf,0,sizeof(cmd_buf));
                    cmd_pos = 0;
                    cmd_cursor = 0;
                }
                dirty_redraw = 1;
                if(dirty_redraw) draw();
                continue;
            }
            char seq;
            r = read(STDIN_FILENO, &seq, 1);
            if (r <= 0 || seq != '[') {
                cmd_mode = !cmd_mode;
                if (!cmd_mode) {
                    memset(cmd_buf,0,sizeof(cmd_buf));
                    cmd_pos = 0;
                    cmd_cursor = 0;
                }
                dirty_redraw = 1;
                if(dirty_redraw) draw();
                continue;
            }
            char key;
            read(STDIN_FILENO, &key, 1);
            if (!cmd_mode) {
                if (key == 'A') cursor_up();
                else if (key == 'B') cursor_down();
                else if (key == 'C') cursor_right();
                else if (key == 'D') cursor_left();
                else if (key == 'H') {
                    Tab *t = &tabs[current_tab];
                    t->cx = 0;
                    dirty_redraw = 1;
                }
                else if (key == 'F') {
                    Tab *t = &tabs[current_tab];
                    t->cx = strlen(t->buf[t->cy]);
                    dirty_redraw = 1;
                }
                else if (key == '5') {
                    char end;
                    read(STDIN_FILENO, &end, 1);
                    if(end == '~') {
                        cursor_pageup();
                        dirty_redraw = 1;
                    }
                }
                else if (key == '6') {
                    char end;
                    read(STDIN_FILENO, &end, 1);
                    if(end == '~') {
                        cursor_pagedown();
                        dirty_redraw = 1;
                    }
                }
                else if (key == '1') {
                    char tmp;
                    read(STDIN_FILENO, &tmp, 1);
                    if(tmp == ';'){
                        read(STDIN_FILENO, &tmp, 1);
                        if(tmp == '5'){
                            read(STDIN_FILENO, &tmp, 1);
                            if(tmp == 'D'){
                                if(current_tab > 0){
                                    current_tab--;
                                    if(current_tab < tab_scroll_x) tab_scroll_x = current_tab;
                                    dirty_redraw = 1;
                                }
                            }else if(tmp == 'C'){
                                int tw,th;
                                get_term_size(&tw,&th);
                                if(current_tab < tab_count - 1){
                                    current_tab++;
                                    int w = 0;
                                    for(int i = tab_scroll_x; i <= current_tab; i++){
                                        w += strlen(tabs[i].name) + 2;
                                    }
                                    while(w > tw) {
                                        tab_scroll_x++;
                                        w = 0;
                                        for(int i = tab_scroll_x; i <= current_tab; i++){
                                            w += strlen(tabs[i].name) + 2;
                                        }
                                    }
                                    dirty_redraw = 1;
                                }
                            }
                        }
                    }
                }
            } else {
                if (key == 'D') {
                    if(cmd_cursor > 0) cmd_cursor--;
                    dirty_redraw = 1;
                } else if (key == 'C') {
                    if(cmd_cursor < cmd_pos) cmd_cursor++;
                    dirty_redraw = 1;
                } else if (key == 'A') {
                    if (history_idx > 0) {
                        history_idx--;
                        strncpy(cmd_buf, cmd_history[history_idx], MAX_CMD_BUF-1);
                        cmd_pos = strlen(cmd_buf);
                        cmd_cursor = cmd_pos;
                        dirty_redraw = 1;
                    }
                } else if (key == 'B') {
                    if (history_idx < history_cnt) {
                        history_idx++;
                        if(history_idx == history_cnt) {
                            memset(cmd_buf,0,sizeof(cmd_buf));
                            cmd_pos = 0;
                            cmd_cursor = 0;
                        } else {
                            strncpy(cmd_buf, cmd_history[history_idx], MAX_CMD_BUF-1);
                            cmd_pos = strlen(cmd_buf);
                            cmd_cursor = cmd_pos;
                        }
                        dirty_redraw = 1;
                    }
                }
                else if (key == 'H') {
                    cmd_cursor = 0;
                    dirty_redraw = 1;
                }
                else if (key == 'F') {
                    cmd_cursor = cmd_pos;
                    dirty_redraw = 1;
                }
                else if (key == '5') {
                    char end;
                    read(STDIN_FILENO, &end, 1);
                }
                else if (key == '6') {
                    char end;
                    read(STDIN_FILENO, &end, 1);
                }
            }
            if(dirty_redraw) draw();
            continue;
        }
        if (cmd_mode) {
            cmd_input_handle(c);
            if(dirty_redraw) draw();
            continue;
        }
        if (c == '\t') {
            insert_tab();
            if(dirty_redraw) draw();
            continue;
        }
        if (c == '\r' || c == '\n') {
            newline();
            if(dirty_redraw) draw();
            continue;
        }
        if (c == 127 || c == '\b') {
            backspace();
            if(dirty_redraw) draw();
            continue;
        }
        if (c >= 32 && c <= 126) {
            insert_char(c);
            if(dirty_redraw) draw();
        }
    }
    return 0;
}
