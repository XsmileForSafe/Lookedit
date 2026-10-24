#include "lookedit.h"
#include <time.h>
#include <string.h>

char base_dir[512];
char log_dir[512];
char crash_dir[512];
char history_path[512];
char last_log_path[512];

static FILE* log_fp = NULL;
static char log_cached_date[16] = {0};

void mkdir_p(const char *path) {
    char tmp[512];
    char *p = NULL;
    size_t len;
    strncpy(tmp, path, sizeof(tmp)-1);
    len = strlen(tmp);
    if(len == 0) return;
    if(tmp[len-1] == '/') tmp[len-1] = 0;
    for(p = tmp + 1; *p; p++) {
        if(*p == '/') {
            *p = 0;
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    mkdir(tmp, 0755);
}

void setup_paths(void) {
    const char *home = getenv("HOME");
    if(!home) return;
    snprintf(base_dir, sizeof(base_dir), "%s/.local/share/%s", home, APP_NAME);
    snprintf(log_dir, sizeof(log_dir), "%s/log", base_dir);
    snprintf(crash_dir, sizeof(crash_dir), "%s/crash", base_dir);
    snprintf(history_path, sizeof(history_path), "%s/lookedit_history.txt", base_dir);
    snprintf(last_log_path, sizeof(last_log_path), "%s/last_log.log", base_dir);
    mkdir_p(log_dir);
    mkdir_p(crash_dir);
}

void log_close(void)
{
    if (log_fp)
    {
        fclose(log_fp);
        log_fp = NULL;
        log_cached_date[0] = '\0';
    }
}

void log_write(const char *msg) {
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char today[16];
    snprintf(today, sizeof(today), "%04d%02d%02d",
        tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday);

    if (log_fp == NULL || strcmp(today, log_cached_date) != 0)
    {
        log_close();
        char ts_path[512];
        snprintf(ts_path, sizeof(ts_path), "%s/log_%s.log", log_dir, today);
        log_fp = fopen(ts_path, "a");
        if (!log_fp) return;
        strcpy(log_cached_date, today);
    }

    fprintf(log_fp, "[%04d-%02d-%02d %02d:%02d:%02d] %s\n",
        tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
        tm->tm_hour, tm->tm_min, tm->tm_sec, msg);
    fflush(log_fp);

    FILE *fp_last = fopen(last_log_path, "w");
    if(fp_last) {
        fprintf(fp_last, "[%04d-%02d-%02d %02d:%02d:%02d] %s\n",
            tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
            tm->tm_hour, tm->tm_min, tm->tm_sec, msg);
        fclose(fp_last);
    }
}

void log_error(const char *msg) {
    char errmsg[1024];
    snprintf(errmsg, sizeof(errmsg), "[ERROR] %s", msg);
    log_write(errmsg);
}

void crash_handler(int sig) {
    tty_raw_disable();
    char path[512];
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    snprintf(path, sizeof(path), "%s/crash_%04d%02d%02d_%02d%02d%02d.log",
        crash_dir,
        tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
        tm->tm_hour, tm->tm_min, tm->tm_sec);
    FILE *fp = fopen(path, "w");
    if(fp) {
        fprintf(fp, "lookedit crash, signal: %d\n", sig);
        fclose(fp);
    }
    raise(sig);
}

int file_save(Tab *t, int overwrite) {
    FILE *check_fp = fopen(t->name, "r");
    if (check_fp) {
        fclose(check_fp);
        if (!overwrite) {
            log_error("Save failed: file exists, no overwrite");
            return -1;
        }
    }

    FILE *fp = fopen(t->name, "w");
    if (!fp) {
        log_error("Save failed: cannot open file");
        return -1;
    }

    for (int i = 0; i < t->line_count; i++) {
        if (i > 0)
            fputc('\n', fp);
        fputs(t->buf[i], fp);
    }

    t->dirty = 0;
    fclose(fp);

    char logmsg[256];
    snprintf(logmsg, sizeof(logmsg), "Save file %s, overwrite=%d", t->name, overwrite);
    log_write(logmsg);
    dirty_redraw = 1;
    return 0;
}

void load_history(void) {
    FILE *fp = fopen(history_path, "r");
    if (!fp) return;
    char line[MAX_CMD_BUF];
    history_cnt = 0;
    while (fgets(line, sizeof(line), fp) && history_cnt < MAX_HISTORY) {
        size_t l = strlen(line);
        if (l && line[l-1] == '\n') line[l-1] = '\0';
        strncpy(cmd_history[history_cnt], line, MAX_CMD_BUF-1);
        history_cnt++;
    }
    fclose(fp);
    history_idx = history_cnt;
}

void save_history(const char *cmd) {
    if (!cmd || !*cmd) return;
    FILE *fp = fopen(history_path, "a");
    if (!fp) return;
    fprintf(fp, "%s\n", cmd);
    fclose(fp);
    if (history_cnt < MAX_HISTORY) {
        strncpy(cmd_history[history_cnt], cmd, MAX_CMD_BUF-1);
        history_cnt++;
    } else {
        for(int i=0;i<MAX_HISTORY-1;i++) strcpy(cmd_history[i], cmd_history[i+1]);
        strncpy(cmd_history[MAX_HISTORY-1], cmd, MAX_CMD_BUF-1);
    }
    history_idx = history_cnt;
}
