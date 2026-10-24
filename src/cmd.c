#include "lookedit.h"

int cmd_mode = 0;
char cmd_buf[MAX_CMD_BUF];
int cmd_pos = 0;
int cmd_cursor = 0;
char cmd_history[MAX_HISTORY][MAX_CMD_BUF];
int history_cnt = 0;
int history_idx = 0;

typedef struct {
    const char *cmd;
    const char *fullname;
    const char *desc;
} CmdInfo;

static const CmdInfo cmd_list[] = {
    {"abb","full",           "desc"},
    {"w",  "write",          "write, save file (no overwrite)"},
    {"us", "unsafe write",   "unsafe write, force overwrite save"},
    {"q",  "quit (safe exit)", "safe quit, reject if unsaved changes"},
    {"e",  "safe exit (alias of q)", "safe exit, fallback alias, reject if unsaved changes"},
    {"ue", "unsafe exit",    "unsafe exit, force exit without dirty check"},
    {"uq", "unsafe quit",    "unsafe quit, force exit without dirty check"},
    {"es", "exit save",      "save (overwrite) then exit"},
    {"l",  "load",           "load file in current tab"},
    {"ul", "unsafe load",    "unsafe load, force load file in current tab"},
    {"o",  "open",           "open file in new tab"},
    {"c",  "close tab",      "close current tab, reject if unsaved changes"},
    {"uc", "unsafe close",   "force close current tab, ignore dirty"},
    {"h",  "help",           "show help page"},
    {NULL, NULL, NULL}
};

static const CmdInfo* cmd_lookup(const char *buf)
{
    for (int i = 0; cmd_list[i].cmd != NULL; i++) {
        if (strcmp(buf, cmd_list[i].cmd) == 0) {
            return &cmd_list[i];
        }
    }
    return NULL;
}

void run_command(void) {
    Tab *t = &tabs[current_tab];
    if (cmd_buf[0] != '\0') {
        const CmdInfo *info = cmd_lookup(cmd_buf);
        if (info) {
            char logline[256];
            snprintf(logline, sizeof(logline), "Input command: %s (%s)", cmd_buf, info->fullname);
            log_write(logline);
        } else {
            char logline[256];
            snprintf(logline, sizeof(logline), "Input command: %s", cmd_buf);
            log_write(logline);
        }
    }
    save_history(cmd_buf);
    if (strcmp(cmd_buf, "w") == 0) {
        file_save(t, 0);
    } else if (strstr(cmd_buf, "w ") == cmd_buf) {
        char *filename = cmd_buf + 2;
        while(*filename == ' ') filename++;
        if(*filename != '\0') {
            strncpy(t->name, filename, sizeof(t->name)-1);
            t->name[sizeof(t->name)-1] = '\0';
            file_save(t, 0);
        }
    } else if (strcmp(cmd_buf, "us") == 0) {
        file_save(t, 1);
    } else if (strstr(cmd_buf, "us ") == cmd_buf) {
        char *filename = cmd_buf + 3;
        while(*filename == ' ') filename++;
        if(*filename != '\0') {
            strncpy(t->name, filename, sizeof(t->name)-1);
            t->name[sizeof(t->name)-1] = '\0';
            file_save(t, 1);
        }
    } else if (strcmp(cmd_buf, "e") == 0 || strcmp(cmd_buf, "q") == 0) {
        if (!t->dirty) {
            log_write("Exit safe");
            tty_raw_disable();
            exit(0);
        } else {
            log_error("Exit failed: file has unsaved changes");
        }
    } else if (strcmp(cmd_buf, "ue") == 0 || strcmp(cmd_buf, "uq") == 0) {
        log_write("Force exit");
        tty_raw_disable();
        exit(0);
    } else if (strcmp(cmd_buf, "es") == 0) {
        file_save(t,1);
        log_write("Save and exit");
        tty_raw_disable();
        exit(0);
    } else if (strstr(cmd_buf, "l ") == cmd_buf) {
        char *filename = cmd_buf + 2;
        while(*filename == ' ') filename++;
        if(*filename == '\0') return;
        tab_new(filename);
        Tab *newtab = &tabs[current_tab];
        file_load(newtab, filename, 0);
        dirty_redraw = 1;
    } else if (strstr(cmd_buf, "ul ") == cmd_buf) {
        char *filename = cmd_buf + 3;
        while(*filename == ' ') filename++;
        if(*filename == '\0') return;
        tab_new(filename);
        Tab *newtab = &tabs[current_tab];
        file_load(newtab, filename, 1);
        dirty_redraw = 1;
    } else if (strstr(cmd_buf, "o ") == cmd_buf) {
        char *filename = cmd_buf + 2;
        while(*filename == ' ') filename++;
        if(*filename == '\0') return;
        tab_new(filename);
        Tab *newtab = &tabs[current_tab];
        file_load(newtab, filename, 0);
        dirty_redraw = 1;
    } else if (strcmp(cmd_buf, "c") == 0) {
        tab_close(0);
    } else if (strcmp(cmd_buf, "uc") == 0) {
        tab_close(1);
    } else if (strcmp(cmd_buf, "h") == 0) {
        tab_new("helper.txt");
        Tab *ht = &tabs[current_tab];
        char help_buf[1024];
        int off = 0;
        off += snprintf(help_buf + off, sizeof(help_buf)-off, "Commands:\n");
        for (int i = 0; cmd_list[i].cmd != NULL; i++) {
            off += snprintf(help_buf + off, sizeof(help_buf)-off,
                "%-4s %-22s %s\n", cmd_list[i].cmd, cmd_list[i].fullname, cmd_list[i].desc);
        }
        off += snprintf(help_buf + off, sizeof(help_buf)-off,
           "\nESC cmd mode\nControl + Left or right change tab\nHome / End: jump to line start / end\nPageUp / PageDown: scroll one page");
        load_content(ht, help_buf);
        dirty_redraw = 1;
    } else {
        char err[256];
        snprintf(err, sizeof(err), "Unknown command: %s", cmd_buf);
        log_error(err);
    }
    memset(cmd_buf,0,sizeof(cmd_buf));
    cmd_pos = 0;
    cmd_cursor = 0;
    cmd_mode = 0;
    dirty_redraw = 1;
}

void cmd_input_handle(char c) {
    if (c == '\r' || c == '\n') {
        run_command();
        return;
    }
    if (c == 127 || c == '\b') {
        if (cmd_cursor > 0) {
            for(int i = cmd_cursor-1; i < cmd_pos; i++)
                cmd_buf[i] = cmd_buf[i+1];
            cmd_pos--;
            cmd_cursor--;
            cmd_buf[cmd_pos] = '\0';
        }
        dirty_redraw = 1;
        return;
    }
    if (c >= 32 && c <= 126) {
        if (cmd_pos < MAX_CMD_BUF - 1) {
            for(int i = cmd_pos; i > cmd_cursor; i--)
                cmd_buf[i] = cmd_buf[i-1];
            cmd_buf[cmd_cursor] = c;
            cmd_pos++;
            cmd_cursor++;
            cmd_buf[cmd_pos] = '\0';
        }
        dirty_redraw = 1;
    }
}
