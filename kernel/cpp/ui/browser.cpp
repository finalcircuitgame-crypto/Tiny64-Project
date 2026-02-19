#include "../../c/drivers/hid/input.h"
#include "browser.h"
#include "../../c/graphics/colors.h"
#include "../../c/lib/string.h"
#include "../../c/drivers/platform/serial.h"
#include "../../c/network/http_client.h"

static int pt_in_rect(int px, int py, int rx, int ry, int rw, int rh) {
    return (px >= rx && px < rx + rw && py >= ry && py < ry + rh);
}

static char to_lower_ascii(char c) {
    if (c >= 'A' && c <= 'Z') return (char)(c + 32);
    return c;
}

static int is_space_ascii(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static int html_entity_decode(const char* s, int* consumed, char* out) {
    *consumed = 0;
    *out = 0;
    if (!s || s[0] != '&') return 0;

    int i = 1;
    while (s[i] && s[i] != ';' && i < 16) i++;
    if (s[i] != ';') return 0;

    // &amp; &lt; &gt; &quot; &nbsp;
    if (i == 4 && s[1] == 'a' && s[2] == 'm' && s[3] == 'p') { *out = '&'; *consumed = 5; return 1; }
    if (i == 3 && s[1] == 'l' && s[2] == 't') { *out = '<'; *consumed = 4; return 1; }
    if (i == 3 && s[1] == 'g' && s[2] == 't') { *out = '>'; *consumed = 4; return 1; }
    if (i == 5 && s[1] == 'q' && s[2] == 'u' && s[3] == 'o' && s[4] == 't') { *out = '"'; *consumed = 6; return 1; }
    if (i == 5 && s[1] == 'n' && s[2] == 'b' && s[3] == 's' && s[4] == 'p') { *out = ' '; *consumed = 6; return 1; }

    // numeric: &#123; or &#x7B;
    if (s[1] == '#') {
        int base = 10;
        int j = 2;
        if (s[j] == 'x' || s[j] == 'X') { base = 16; j++; }
        int v = 0;
        for (; j < i; j++) {
            char c = s[j];
            int d = -1;
            if (c >= '0' && c <= '9') d = c - '0';
            else if (base == 16 && c >= 'a' && c <= 'f') d = 10 + (c - 'a');
            else if (base == 16 && c >= 'A' && c <= 'F') d = 10 + (c - 'A');
            else return 0;
            v = v * base + d;
            if (v > 255) break;
        }
        if (v <= 0 || v > 255) return 0;
        *out = (char)v;
        *consumed = i + 1;
        return 1;
    }

    return 0;
}

static void append_char(char* out, int out_sz, int* out_i, char c) {
    if (*out_i >= out_sz - 1) return;
    out[*out_i] = c;
    (*out_i)++;
}

static void append_str(char* out, int out_sz, int* out_i, const char* s) {
    while (s && *s) {
        append_char(out, out_sz, out_i, *s++);
    }
}

static void set_url_from_cstr(char* url, int url_sz, int* url_len, int* url_cursor, const char* s) {
    memset(url, 0, url_sz);
    if (!s) s = "";
    strncpy(url, s, url_sz - 1);
    *url_len = (int)strlen(url);
    *url_cursor = *url_len;
}

BrowserWindow::BrowserWindow(int x, int y, int w, int h) : Window("Browser", x, y, w, h) {
    set_url_from_cstr(url, (int)sizeof(url), &url_len, &url_cursor, "http://example.com/");
    url_active = 1;

    memset(history, 0, sizeof(history));
    history_count = 0;
    history_index = -1;

    memset(page_text, 0, sizeof(page_text));
    strcpy(page_text,
           "Tiny64 Browser\n\n"
           "- Type a URL in the address bar\n"
           "- Press Enter to navigate\n\n"
           "(Networking/HTTP hookup is next — UI is ready.)\n");
    page_scroll = 0;

    cursor_blink = 0;
    cursor_on = 1;
}

void BrowserWindow::Tick() {
    // Simple cursor blink (no real timer API; this is frame-based)
    cursor_blink++;
    if (cursor_blink > 30) {
        cursor_blink = 0;
        cursor_on = !cursor_on;
    }
}

void BrowserWindow::SetPageTextFromHtml(const char* html) {
    if (!html) return;

    char out_buf[8192];
    int out_i = 0;

    char title[128];
    int title_i = 0;
    int in_title = 0;
    int in_script = 0;
    int in_style = 0;

    int last_space = 1;
    int newlines = 0;

    for (int i = 0; html[i] && out_i < (int)sizeof(out_buf) - 1; ) {
        char c = html[i];

        if (!in_script && !in_style && c == '&') {
            int consumed = 0;
            char dec = 0;
            if (html_entity_decode(&html[i], &consumed, &dec)) {
                if (in_title) {
                    if (title_i < (int)sizeof(title) - 1) title[title_i++] = dec;
                } else {
                    if (is_space_ascii(dec)) {
                        if (!last_space) {
                            append_char(out_buf, (int)sizeof(out_buf), &out_i, ' ');
                            last_space = 1;
                        }
                    } else {
                        append_char(out_buf, (int)sizeof(out_buf), &out_i, dec);
                        last_space = 0;
                        newlines = 0;
                    }
                }
                i += consumed;
                continue;
            }
        }

        if (c == '<') {
            int j = i + 1;
            int closing = 0;
            if (html[j] == '/') { closing = 1; j++; }

            while (html[j] && is_space_ascii(html[j])) j++;

            char tag[16];
            int ti = 0;
            while (html[j] && ti < (int)sizeof(tag) - 1) {
                char tc = html[j];
                if (tc == '>' || is_space_ascii(tc) || tc == '/') break;
                tag[ti++] = to_lower_ascii(tc);
                j++;
            }
            tag[ti] = 0;

            // advance to end of tag
            while (html[j] && html[j] != '>') j++;
            if (html[j] == '>') j++;

            if (tag[0]) {
                if (!closing) {
                    if (strcmp(tag, "br") == 0) {
                        if (out_i < (int)sizeof(out_buf) - 1) {
                            append_char(out_buf, (int)sizeof(out_buf), &out_i, '\n');
                            last_space = 1;
                            newlines++;
                        }
                    } else if (strcmp(tag, "p") == 0 || strcmp(tag, "div") == 0 || strcmp(tag, "section") == 0) {
                        if (newlines < 2) append_char(out_buf, (int)sizeof(out_buf), &out_i, '\n');
                        if (newlines < 2) append_char(out_buf, (int)sizeof(out_buf), &out_i, '\n');
                        last_space = 1;
                        newlines = 2;
                    } else if (strcmp(tag, "li") == 0) {
                        if (newlines < 1) append_char(out_buf, (int)sizeof(out_buf), &out_i, '\n');
                        append_str(out_buf, (int)sizeof(out_buf), &out_i, "- ");
                        last_space = 0;
                        newlines = 1;
                    } else if (strcmp(tag, "h1") == 0 || strcmp(tag, "h2") == 0 || strcmp(tag, "h3") == 0) {
                        if (newlines < 2) append_char(out_buf, (int)sizeof(out_buf), &out_i, '\n');
                        if (newlines < 2) append_char(out_buf, (int)sizeof(out_buf), &out_i, '\n');
                        newlines = 2;
                        last_space = 1;
                    } else if (strcmp(tag, "title") == 0) {
                        in_title = 1;
                        title_i = 0;
                    } else if (strcmp(tag, "script") == 0) {
                        in_script = 1;
                    } else if (strcmp(tag, "style") == 0) {
                        in_style = 1;
                    }
                } else {
                    if (strcmp(tag, "title") == 0) {
                        in_title = 0;
                    } else if (strcmp(tag, "script") == 0) {
                        in_script = 0;
                    } else if (strcmp(tag, "style") == 0) {
                        in_style = 0;
                    }
                }
            }

            i = j;
            continue;
        }

        if (in_script || in_style) {
            i++;
            continue;
        }

        if (in_title) {
            if (c != '\r' && c != '\n') {
                if (title_i < (int)sizeof(title) - 1) title[title_i++] = c;
            }
            i++;
            continue;
        }

        if (c == '\r') { i++; continue; }
        if (c == '\n' || c == '\t' || c == ' ') {
            if (!last_space) {
                append_char(out_buf, (int)sizeof(out_buf), &out_i, ' ');
                last_space = 1;
            }
            i++;
            continue;
        }

        append_char(out_buf, (int)sizeof(out_buf), &out_i, c);
        last_space = 0;
        newlines = 0;
        i++;
    }

    title[title_i] = 0;
    out_buf[out_i] = 0;

    memset(page_text, 0, sizeof(page_text));
    if (title[0]) {
        strncpy(page_text, title, sizeof(page_text) - 1);
        strncat(page_text, "\n\n", sizeof(page_text) - strlen(page_text) - 1);
    }
    if (out_buf[0]) {
        strncat(page_text, out_buf, sizeof(page_text) - strlen(page_text) - 1);
    } else {
        // If parsing produced nothing (e.g. unusual markup), show the raw response.
        strncat(page_text, "[raw]\n\n", sizeof(page_text) - strlen(page_text) - 1);
        // Filter ANSI/control sequences so this is readable.
        int in_esc = 0;
        for (int i = 0; html[i] && (int)strlen(page_text) < (int)sizeof(page_text) - 2; i++) {
            char c = html[i];
            if (!in_esc && c == 0x1B) { in_esc = 1; continue; }
            if (in_esc) {
                // Consume until a likely terminator for CSI sequences.
                if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) in_esc = 0;
                continue;
            }
            if (c == '\r') continue;
            if (c == '\t') c = ' ';
            if (c < 32 && c != '\n') continue;
            int cur_len = (int)strlen(page_text);
            page_text[cur_len] = c;
            page_text[cur_len + 1] = 0;
        }
    }
}

void BrowserWindow::Navigate() {
    serial_write_str("Browser: Navigate -> ");
    serial_write_str(url);
    serial_write_str("\r\n");

    PushHistory();
    NavigateNoHistory();
}

void BrowserWindow::NavigateNoHistory() {

    char body[8192];
    memset(body, 0, sizeof(body));

    int r = http_get_url(url, body, (int)sizeof(body));
    if (r == 0) {
        SetPageTextFromHtml(body);
    } else {
        memset(page_text, 0, sizeof(page_text));
        strcpy(page_text, "[HTTP] failed\n\n");
        strncat(page_text, body, sizeof(page_text) - strlen(page_text) - 1);
    }
    page_scroll = 0;
}

void BrowserWindow::PushHistory() {
    if (url_len <= 0) return;

    if (history_count > 0 && history_index >= 0) {
        if (strncmp(history[history_index], url, sizeof(history[0]) - 1) == 0) return;
    }

    // If we're not at the end, truncate forward history
    if (history_index >= 0 && history_index < history_count - 1) {
        history_count = history_index + 1;
    }

    if (history_count < 16) {
        strncpy(history[history_count], url, sizeof(history[0]) - 1);
        history_index = history_count;
        history_count++;
        return;
    }

    // Drop oldest
    for (int i = 0; i < 15; i++) {
        strncpy(history[i], history[i + 1], sizeof(history[0]) - 1);
        history[i][sizeof(history[0]) - 1] = 0;
    }
    strncpy(history[15], url, sizeof(history[0]) - 1);
    history[15][sizeof(history[0]) - 1] = 0;
    history_count = 16;
    history_index = 15;
}

void BrowserWindow::GoBack() {
    if (history_index <= 0) return;
    history_index--;
    set_url_from_cstr(url, (int)sizeof(url), &url_len, &url_cursor, history[history_index]);
    NavigateNoHistory();
}

void BrowserWindow::GoForward() {
    if (history_index < 0 || history_index >= history_count - 1) return;
    history_index++;
    set_url_from_cstr(url, (int)sizeof(url), &url_len, &url_cursor, history[history_index]);
    NavigateNoHistory();
}

void BrowserWindow::Reload() {
    NavigateNoHistory();
}

void BrowserWindow::Draw(Renderer& r) {
    if (closed || minimized) return;
    Window::Draw(r);

    // Palette
    uint32_t bg = 0xFFF6F7F9;
    uint32_t surface = 0xFFFFFFFF;
    uint32_t line = 0xFFE3E6EA;
    uint32_t text = 0xFF111111;
    uint32_t subtle = 0xFF6B7280;
    uint32_t blue = 0xFF3B82F6;

    int pad = 10;
    int title_h = 32;
    int toolbar_h = 44;

    int inner_x = x + pad;
    int inner_y = y + title_h + pad;
    int inner_w = w - pad * 2;
    int inner_h = h - title_h - pad * 2;

    // Background surface
    r.fill_rect(inner_x, inner_y, inner_w, inner_h, bg);

    // Toolbar surface
    int tb_x = inner_x;
    int tb_y = inner_y;
    int tb_w = inner_w;
    int tb_h = toolbar_h;
    r.fill_rect(tb_x, tb_y, tb_w, tb_h, surface);
    r.fill_rect(tb_x, tb_y + tb_h - 1, tb_w, 1, line);

    // Buttons
    int btn = 28;
    int btn_r = 6;
    int btn_gap = 8;
    int btn_y = tb_y + (tb_h - btn) / 2;
    int back_x = tb_x + 10;
    int fwd_x = back_x + btn + btn_gap;
    int rel_x = fwd_x + btn + btn_gap;

    uint32_t btn_bg = 0xFFF3F4F6;
    uint32_t btn_fg = 0xFF111827;

    r.draw_rounded_rect(back_x, btn_y, btn, btn, btn_r, btn_bg);
    r.draw_string("<", back_x + 10, btn_y + 6, btn_fg, 1);
    r.draw_rounded_rect(fwd_x, btn_y, btn, btn, btn_r, btn_bg);
    r.draw_string(">", fwd_x + 10, btn_y + 6, btn_fg, 1);
    r.draw_rounded_rect(rel_x, btn_y, btn, btn, btn_r, btn_bg);
    r.draw_string("R", rel_x + 10, btn_y + 6, btn_fg, 1);

    // Omnibox
    int omni_x = rel_x + btn + 12;
    int omni_y = btn_y;
    int omni_h = btn;
    int go_w = 44;
    int omni_w = tb_x + tb_w - omni_x - 10;
    if (omni_w < 100) omni_w = 100;

    // URL input box
    int box_w = omni_w - go_w;
    r.fill_rect(omni_x, omni_y, box_w, omni_h, 0xFFF9FAFB);
    r.draw_rounded_rect(omni_x, omni_y, box_w, omni_h, 10, url_active ? blue : line);

    // Go button (pill)
    int go_x = omni_x + box_w + 6;
    r.draw_rounded_rect(go_x, omni_y, go_w - 6, omni_h, 10, blue);
    r.draw_string("GO", go_x + 10, omni_y + 6, 0xFFFFFFFF, 1);

    int url_text_x = omni_x + 10;
    int url_text_y = omni_y + 7;
    if (url_len > 0) {
        r.draw_string(url, url_text_x, url_text_y, text, 1);
    } else {
        r.draw_string("Search or type a URL", url_text_x, url_text_y, subtle, 1);
    }

    // Cursor
    if (url_active && cursor_on) {
        int char_w = 8;
        int cx = url_text_x + url_cursor * char_w;
        int cy = omni_y + 6;
        r.fill_rect(cx, cy, 2, omni_h - 12, blue);
    }

    // Page view
    int page_x = inner_x;
    int page_y = tb_y + tb_h + 10;
    int page_w = inner_w;
    int page_h = inner_h - tb_h - 10;
    r.fill_rect(page_x, page_y, page_w, page_h, surface);

    // Text rendering with simple wrapping/clipping
    int left_pad = 12;
    int top_pad = 10;
    int line_h = 18;
    int max_cols = (page_w - left_pad * 2 - 8) / 8;
    if (max_cols < 10) max_cols = 10;

    // Skip lines for scroll
    int skip_lines = page_scroll;
    const char* p = page_text;
    while (skip_lines > 0 && *p) {
        if (*p == '\n') skip_lines--;
        p++;
    }

    int draw_y = page_y + top_pad;
    char line_buf[512];
    int lb = 0;
    int col = 0;

    while (*p && draw_y < page_y + page_h - line_h) {
        char c = *p++;

        if (c == '\n') {
            line_buf[lb] = 0;
            if (lb > 0) r.draw_string(line_buf, page_x + left_pad, draw_y, text, 1);
            draw_y += line_h;
            lb = 0;
            col = 0;
            continue;
        }

        if (c == '\t') c = ' ';

        if (c < 32) continue;

        line_buf[lb++] = c;
        if (lb >= (int)sizeof(line_buf) - 1) {
            line_buf[lb] = 0;
            r.draw_string(line_buf, page_x + left_pad, draw_y, text, 1);
            draw_y += line_h;
            lb = 0;
            col = 0;
            continue;
        }

        col++;
        if (col >= max_cols) {
            line_buf[lb] = 0;
            r.draw_string(line_buf, page_x + left_pad, draw_y, text, 1);
            draw_y += line_h;
            lb = 0;
            col = 0;
        }
    }

    // Flush last partial line
    if (lb > 0 && draw_y < page_y + page_h - line_h) {
        line_buf[lb] = 0;
        r.draw_string(line_buf, page_x + left_pad, draw_y, text, 1);
    }

    // Scrollbar
    r.fill_rect(page_x + page_w - 6, page_y, 6, page_h, 0xFFF3F4F6);
}

void BrowserWindow::OnMouseEvent(int mx, int my, int type) {
    Window::OnMouseEvent(mx, my, type);
    if (closed || minimized) return;

    if (type == MOUSE_EVENT_UP) {
        int pad = 10;
        int title_h = 32;
        int toolbar_h = 44;

        int inner_x = x + pad;
        int inner_y = y + title_h + pad;
        int inner_w = w - pad * 2;
        int inner_h = h - title_h - pad * 2;

        int tb_x = inner_x;
        int tb_y = inner_y;
        int tb_w = inner_w;
        int tb_h = toolbar_h;

        int btn = 28;
        int btn_gap = 8;
        int btn_y = tb_y + (tb_h - btn) / 2;
        int back_x = tb_x + 10;
        int fwd_x = back_x + btn + btn_gap;
        int rel_x = fwd_x + btn + btn_gap;

        int omni_x = rel_x + btn + 12;
        int omni_y = btn_y;
        int omni_h = btn;
        int go_w = 44;
        int omni_w = tb_x + tb_w - omni_x - 10;
        if (omni_w < 100) omni_w = 100;
        int box_w = omni_w - go_w;
        int go_x = omni_x + box_w + 6;

        if (pt_in_rect(mx, my, back_x, btn_y, btn, btn)) {
            GoBack();
            return;
        }
        if (pt_in_rect(mx, my, fwd_x, btn_y, btn, btn)) {
            GoForward();
            return;
        }
        if (pt_in_rect(mx, my, rel_x, btn_y, btn, btn)) {
            Reload();
            return;
        }

        if (pt_in_rect(mx, my, omni_x, omni_y, box_w, omni_h)) {
            url_active = 1;
            cursor_on = 1;
            cursor_blink = 0;
            return;
        }
        if (pt_in_rect(mx, my, go_x, omni_y, go_w - 6, omni_h)) {
            url_active = 1;
            cursor_on = 1;
            cursor_blink = 0;
            Navigate();
            return;
        }

        url_active = 0;

        // Scroll (simple click zones)
        int page_x = inner_x;
        int page_y = tb_y + tb_h + 10;
        int page_w = inner_w;
        int page_h = inner_h - tb_h - 10;
        if (pt_in_rect(mx, my, page_x, page_y, page_w, page_h)) {
            if (my < page_y + page_h / 2) {
                if (page_scroll > 0) page_scroll--;
            } else {
                page_scroll++;
            }
        }
    }
}

void BrowserWindow::HandleKey(uint32_t key_event) {
    if (!url_active) return;

    if (key_event & KB_MOD_ALT) return;

    char c = (char)(key_event & 0xFF);

    // Enter
    if (c == '\n' || c == '\r') {
        Navigate();
        return;
    }

    // Backspace
    if (c == 8) {
        if (url_cursor > 0 && url_len > 0) {
            // shift left
            for (int i = url_cursor - 1; i < url_len; i++) url[i] = url[i + 1];
            url_cursor--;
            url_len--;
        }
        return;
    }

    // Left/Right arrows are not currently delivered as distinct codes; ignore.

    // Printable
    if (c >= 32 && c <= 126) {
        if (url_len < (int)sizeof(url) - 2) {
            // insert at cursor
            for (int i = url_len; i >= url_cursor; i--) url[i + 1] = url[i];
            url[url_cursor] = c;
            url_cursor++;
            url_len++;
            cursor_on = 1;
            cursor_blink = 0;
        }
    }
}
