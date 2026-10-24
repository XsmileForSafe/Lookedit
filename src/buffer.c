#include "lookedit.h"

Tab tabs[MAX_TABS];
int tab_count = 1;
int current_tab = 0;
int dirty_redraw = 1;
int blink_state = 1;


void tab_init(Tab *tab, const char *fname) {
    memset(tab, 0, sizeof(Tab));
    strncpy(tab->name, fname, sizeof(tab->name)-1);
    tab->line_count = 1;
    tab->buf[0][0] = '\0';
    tab->dirty = 0;
    tab->cx = 0;
    tab->cy = 0;
    tab->view_x = 0;
    tab->view_y = 0;
}

void tab_new(const char *fname) {
    if (tab_count >= MAX_TABS) return;
    tab_init(&tabs[tab_count], fname);
    tab_count++;
    current_tab = tab_count - 1;
    char logmsg[256];
    snprintf(logmsg, sizeof(logmsg), "New tab: %s", fname);
    log_write(logmsg);
    dirty_redraw = 1;
}

void tab_switch(int delta) {
    current_tab += delta;
    if (current_tab < 0) current_tab = tab_count - 1;
    if (current_tab >= tab_count) current_tab = 0;
    char logmsg[256];
    snprintf(logmsg, sizeof(logmsg), "Switch tab to %d: %s", current_tab, tabs[current_tab].name);
    log_write(logmsg);
    dirty_redraw = 1;
}

void adjust_scroll(Tab *t, int tw, int th)
{
    int view_h = th - 2;
    int half = view_h / 2;
    int target = t->cy - half;
    if(target < 0) target = 0;
    t->view_y = target;

    int view_w = tw - 5;
    if(t->cx < t->view_x)
    {
        t->view_x = t->cx;
    }
    if(t->cx >= t->view_x + view_w)
    {
        t->view_x = t->cx - view_w + 1;
    }
}

void insert_char(char c) {
    Tab *t = &tabs[current_tab];
    char *line = t->buf[t->cy];
    int len = strlen(line);
    if (len >= MAX_LINE_LEN - 1) return;
    for (int i = len; i >= t->cx; i--)
        line[i+1] = line[i];
    line[t->cx] = c;
    t->cx++;
    t->dirty = 1;
    dirty_redraw = 1;
}

void insert_tab(void) {
    Tab *t = &tabs[current_tab];
    char *line = t->buf[t->cy];
    int len = strlen(line);
    if (len + TAB_WIDTH >= MAX_LINE_LEN - 1) return;
    memmove(line + t->cx + TAB_WIDTH, line + t->cx, len - t->cx);
    memcpy(line + t->cx, TAB_SPACE, TAB_WIDTH);
    t->cx += TAB_WIDTH;
    t->dirty = 1;
    dirty_redraw = 1;
}

int tab_close(int force)
{
    if (tab_count <= 1)
    {
        log_error("Cannot close last tab");
        return 1;
    }
    Tab *t = &tabs[current_tab];
    if (!force && t->dirty)
    {
        log_error("Close failed: file has unsaved changes");
        return 1;
    }
    for (int i = current_tab; i < tab_count - 1; i++)
    {
        tabs[i] = tabs[i+1];
    }
    tab_count--;
    if (current_tab >= tab_count)
    {
        current_tab = tab_count - 1;
    }
    char logmsg[256];
    snprintf(logmsg, sizeof(logmsg), "Closed tab, now active: %d, name: %s", current_tab, tabs[current_tab].name);
    log_write(logmsg);
    dirty_redraw = 1;
    return 0;
}

void buffer_free(Tab *t)
{
    t->line_count = 0;
}

void load_content(Tab *t, const char *text)
{
    buffer_free(t);
    t->cx = 0;
    t->cy = 0;
    t->view_x = 0;
    t->view_y = 0;
    t->dirty = 0;

    char *dup = strdup(text);
    if(!dup) return;
    char *p = dup;
    char *line_start = p;
    while(1)
    {
        if(*p == '\n' || *p == '\0')
        {
            size_t len = p - line_start;
            if(t->line_count < MAX_LINES && len < 1024)
            {
                strncpy(t->buf[t->line_count], line_start, len);
                t->buf[t->line_count][len] = '\0';
                t->line_count++;
            }
            if(*p == '\0') break;
            p++;
            line_start = p;
        }
        else
        {
            p++;
        }
    }
    free(dup);
}

void file_load(Tab *t, const char *path, int force)
{
    if(!force && t->dirty)
    {
        return;
    }
    FILE *f = fopen(path, "r");
    if(!f)
    {
        load_content(t, "");
        return;
    }
    fseek(f,0,SEEK_END);
    long sz = ftell(f);
    rewind(f);
    char tmpbuf[4096];
    if(sz >= sizeof(tmpbuf))
    {
        fclose(f);
        load_content(t, "");
        return;
    }
    fread(tmpbuf,1,sz,f);
    tmpbuf[sz] = '\0';
    fclose(f);
    load_content(t, tmpbuf);
    strncpy(t->name, path, sizeof(t->name)-1);
    t->name[sizeof(t->name)-1] = '\0';
}

void backspace(void) {
    Tab *t = &tabs[current_tab];
    if (t->cx == 0) {
        if (t->cy == 0) return;
        int prev = t->cy - 1;
        char *pline = t->buf[prev];
        char *cline = t->buf[t->cy];
        int pl = strlen(pline);
        strncat(pline, cline, MAX_LINE_LEN - pl - 1);
        for (int i = t->cy; i < t->line_count - 1; i++)
            strcpy(t->buf[i], t->buf[i+1]);
        t->line_count--;
        t->cy = prev;
        t->cx = pl;
        t->dirty = 1;
        dirty_redraw = 1;
        return;
    }
    char *line = t->buf[t->cy];
    int len = strlen(line);
    for (int i = t->cx - 1; i < len; i++)
        line[i] = line[i+1];
    t->cx--;
    t->dirty = 1;
    dirty_redraw = 1;
}

void newline(void) {
    Tab *t = &tabs[current_tab];
    if (t->line_count >= MAX_LINES - 1) return;
    char *line = t->buf[t->cy];
    int len = strlen(line);
    int split = t->cx;
    for (int i = t->line_count; i > t->cy + 1; i--)
        strcpy(t->buf[i], t->buf[i-1]);
    t->buf[t->cy+1][0] = '\0';
    strncpy(t->buf[t->cy+1], line + split, MAX_LINE_LEN - 1);
    line[split] = '\0';
    t->cy++;
    t->cx = 0;
    t->line_count++;
    t->dirty = 1;
    dirty_redraw = 1;
}

void cursor_up(void) {
    Tab *t = &tabs[current_tab];
    if (t->cy <= 0) return;
    t->cy--;
    int l = strlen(t->buf[t->cy]);
    if (t->cx > l) t->cx = l;
    dirty_redraw = 1;
}

void cursor_down(void) {
    Tab *t = &tabs[current_tab];
    if (t->cy >= t->line_count - 1) return;
    t->cy++;
    int l = strlen(t->buf[t->cy]);
    if (t->cx > l) t->cx = l;
    dirty_redraw = 1;
}

void cursor_left(void) {
    Tab *t = &tabs[current_tab];
    if (t->cx > 0) {
        t->cx--;
        dirty_redraw = 1;
        return;
    }
    if (t->cy <= 0) return;
    t->cy--;
    t->cx = strlen(t->buf[t->cy]);
    dirty_redraw = 1;
}

void cursor_right(void) {
    Tab *t = &tabs[current_tab];
    int l = strlen(t->buf[t->cy]);
    if (t->cx < l) {
        t->cx++;
        dirty_redraw = 1;
        return;
    }
    if (t->cy >= t->line_count - 1) return;
    t->cy++;
    t->cx = 0;
    dirty_redraw = 1;
}
