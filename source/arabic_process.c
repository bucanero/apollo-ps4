#include "arabic_process.h"
#include <string.h>
#include <stdlib.h>

typedef struct {
    uint32_t isolated;
    uint32_t initial;
    uint32_t medial;
    uint32_t final;
} ArabicForm;

typedef struct {
    uint32_t key;
    ArabicForm form;
} ArabicMapEntry;

static const ArabicMapEntry arabic_forms[] = {
    {0x0621, {0xFE80, 0xFE80, 0xFE80, 0xFE80}},
    {0x0622, {0xFE81, 0xFE81, 0xFE82, 0xFE82}},
    {0x0623, {0xFE83, 0xFE83, 0xFE84, 0xFE84}},
    {0x0624, {0xFE85, 0xFE85, 0xFE86, 0xFE86}},
    {0x0625, {0xFE87, 0xFE87, 0xFE88, 0xFE88}},
    {0x0626, {0xFE89, 0xFE8B, 0xFE8C, 0xFE8A}},
    {0x0627, {0xFE8D, 0xFE8D, 0xFE8E, 0xFE8E}},
    {0x0628, {0xFE8F, 0xFE91, 0xFE92, 0xFE90}},
    {0x0629, {0xFE93, 0xFE93, 0xFE94, 0xFE94}},
    {0x062A, {0xFE95, 0xFE97, 0xFE98, 0xFE96}},
    {0x062B, {0xFE99, 0xFE9B, 0xFE9C, 0xFE9A}},
    {0x062C, {0xFE9D, 0xFE9F, 0xFEA0, 0xFE9E}},
    {0x062D, {0xFEA1, 0xFEA3, 0xFEA4, 0xFEA2}},
    {0x062E, {0xFEA5, 0xFEA7, 0xFEA8, 0xFEA6}},
    {0x062F, {0xFEA9, 0xFEA9, 0xFEAA, 0xFEAA}},
    {0x0630, {0xFEAB, 0xFEAB, 0xFEAC, 0xFEAC}},
    {0x0631, {0xFEAD, 0xFEAD, 0xFEAE, 0xFEAE}},
    {0x0632, {0xFEAF, 0xFEAF, 0xFEB0, 0xFEB0}},
    {0x0633, {0xFEB1, 0xFEB3, 0xFEB4, 0xFEB2}},
    {0x0634, {0xFEB5, 0xFEB7, 0xFEB8, 0xFEB6}},
    {0x0635, {0xFEB9, 0xFEBB, 0xFEBC, 0xFEBA}},
    {0x0636, {0xFEBD, 0xFEBF, 0xFEC0, 0xFEBE}},
    {0x0637, {0xFEC1, 0xFEC3, 0xFEC4, 0xFEC2}},
    {0x0638, {0xFEC5, 0xFEC7, 0xFEC8, 0xFEC6}},
    {0x0639, {0xFEC9, 0xFECB, 0xFECC, 0xFECA}},
    {0x063A, {0xFECD, 0xFECF, 0xFED0, 0xFECE}},
    {0x0641, {0xFED1, 0xFED3, 0xFED4, 0xFED2}},
    {0x0642, {0xFED5, 0xFED7, 0xFED8, 0xFED6}},
    {0x0643, {0xFED9, 0xFEDB, 0xFEDC, 0xFEDA}},
    {0x0644, {0xFEDD, 0xFEDF, 0xFEE0, 0xFEDE}},
    {0x0645, {0xFEE1, 0xFEE3, 0xFEE4, 0xFEE2}},
    {0x0646, {0xFEE5, 0xFEE7, 0xFEE8, 0xFEE6}},
    {0x0647, {0xFEE9, 0xFEEB, 0xFEEC, 0xFEEA}},
    {0x0648, {0xFEED, 0xFEED, 0xFEEE, 0xFEEE}},
    {0x0649, {0xFEEF, 0xFEEF, 0xFEF0, 0xFEF0}},
    {0x064A, {0xFEF1, 0xFEF3, 0xFEF4, 0xFEF2}},
    {0x067E, {0xFB56, 0xFB58, 0xFB59, 0xFB57}},
    {0x0686, {0xFB7A, 0xFB7C, 0xFB7D, 0xFB7B}},
    {0x0698, {0xFB8A, 0xFB8A, 0xFB8B, 0xFB8B}},
    {0x06AF, {0xFB92, 0xFB94, 0xFB95, 0xFB93}},
};

static const size_t arabic_forms_count = sizeof(arabic_forms) / sizeof(arabic_forms[0]);

static const ArabicForm* get_arabic_form(uint32_t cp) {
    for (size_t i = 0; i < arabic_forms_count; i++) {
        if (arabic_forms[i].key == cp) {
            return &arabic_forms[i].form;
        }
    }
    return NULL;
}

static size_t utf8_to_utf32(const char* text, uint32_t** out_cps) {
    size_t len = strlen(text);
    uint32_t* cps = malloc((len + 1) * sizeof(uint32_t));
    size_t count = 0;
    const unsigned char* str = (const unsigned char*)text;

    while (*str) {
        uint32_t cp = 0;
        if ((*str & 0x80) == 0) cp = *str++;
        else if ((*str & 0xE0) == 0xC0) {
            cp = (*str++ & 0x1F) << 6;
            cp |= (*str++ & 0x3F);
        } else if ((*str & 0xF0) == 0xE0) {
            cp = (*str++ & 0x0F) << 12;
            cp |= (*str++ & 0x3F) << 6;
            cp |= (*str++ & 0x3F);
        } else if ((*str & 0xF8) == 0xF0) {
            cp = (*str++ & 0x07) << 18;
            cp |= (*str++ & 0x3F) << 12;
            cp |= (*str++ & 0x3F) << 6;
            cp |= (*str++ & 0x3F);
        } else {
            str++;
            continue;
        }
        cps[count++] = cp;
    }
    cps[count] = 0;
    *out_cps = cps;
    return count;
}

static char* utf32_to_utf8(const uint32_t* cps, size_t count) {
    char* result = malloc(count * 4 + 1);
    char* p = result;
    for (size_t i = 0; i < count; i++) {
        uint32_t cp = cps[i];
        if (cp <= 0x7F) *p++ = (char)cp;
        else if (cp <= 0x7FF) {
            *p++ = (char)(0xC0 | (cp >> 6));
            *p++ = (char)(0x80 | (cp & 0x3F));
        } else if (cp <= 0xFFFF) {
            *p++ = (char)(0xE0 | (cp >> 12));
            *p++ = (char)(0x80 | ((cp >> 6) & 0x3F));
            *p++ = (char)(0x80 | (cp & 0x3F));
        } else if (cp <= 0x10FFFF) {
            *p++ = (char)(0xF0 | (cp >> 18));
            *p++ = (char)(0x80 | ((cp >> 12) & 0x3F));
            *p++ = (char)(0x80 | ((cp >> 6) & 0x3F));
            *p++ = (char)(0x80 | (cp & 0x3F));
        }
    }
    *p = '\0';
    return result;
}

bool contains_arabic(const char* text) {
    if (!text) return false;
    uint32_t* cps = NULL;
    size_t count = utf8_to_utf32(text, &cps);
    bool found = false;
    for (size_t i = 0; i < count; i++) {
        uint32_t cp = cps[i];
        if ((cp >= 0x0600 && cp <= 0x06FF) || (cp >= 0x0750 && cp <= 0x077F) ||
            (cp >= 0x08A0 && cp <= 0x08FF) || (cp >= 0xFB50 && cp <= 0xFDFF) ||
            (cp >= 0xFE70 && cp <= 0xFEFF)) {
            found = true;
            break;
        }
    }
    free(cps);
    return found;
}

static bool connects_to_next(uint32_t cp) {
    if (!get_arabic_form(cp)) return false;
    if (cp == 0x0621 || cp == 0x0622 || cp == 0x0623 || cp == 0x0624 || cp == 0x0625 || 
        cp == 0x0627 || cp == 0x062F || cp == 0x0630 || cp == 0x0631 || cp == 0x0632 || 
        cp == 0x0648 || cp == 0x0649) {
        return false;
    }
    return true;
}

static bool connects_to_prev(uint32_t cp) {
    return get_arabic_form(cp) != NULL;
}

static void apply_shaping(uint32_t* cps, size_t count) {
    for (size_t i = 0; i < count; i++) {
        uint32_t cp = cps[i];
        const ArabicForm* f = get_arabic_form(cp);
        if (f) {
            bool prev = (i > 0 && connects_to_next(cps[i-1]));
            bool next = (i < count - 1 && connects_to_prev(cps[i+1]));
            if (prev && next) cps[i] = f->medial;
            else if (prev) cps[i] = f->final;
            else if (next) cps[i] = f->initial;
            else cps[i] = f->isolated;
        }
    }
}

static void reverse_range(uint32_t* start, uint32_t* end) {
    while (start < end) {
        uint32_t temp = *start;
        *start++ = *end;
        *end-- = temp;
    }
}

static void apply_bidi(uint32_t* cps, size_t count) {
    size_t i = 0;
    while (i < count) {
        uint32_t cp = cps[i];
        bool is_arabic = (cp >= 0x0600 && cp <= 0x06FF) || (cp >= 0xFB50 && cp <= 0xFEFF);
        if (is_arabic) {
            size_t start = i;
            while (i < count) {
                uint32_t next_cp = cps[i];
                bool next_is_arabic = (next_cp >= 0x0600 && next_cp <= 0x06FF) || (next_cp >= 0xFB50 && next_cp <= 0xFEFF);
                bool is_punc = (next_cp == '(' || next_cp == ')' || next_cp == '[' || next_cp == ']' || next_cp == '{' || next_cp == '}' || next_cp == '<' || next_cp == '>');
                bool is_space = (next_cp == ' ');
                if (next_is_arabic || is_punc || is_space) i++;
                else break;
            }
            size_t end = i;
            while (end > start && cps[end-1] == ' ') end--;
            if (end > start) reverse_range(&cps[start], &cps[end-1]);
        } else {
            i++;
        }
    }
}

static void fix_parentheses(uint32_t* cps, size_t count) {
    bool has_arabic = false;
    for (size_t i = 0; i < count; i++) {
        if ((cps[i] >= 0x0600 && cps[i] <= 0x06FF) || (cps[i] >= 0xFB50 && cps[i] <= 0xFEFF)) {
            has_arabic = true;
            break;
        }
    }
    if (has_arabic) {
        for (size_t i = 0; i < count; i++) {
            switch (cps[i]) {
                case '(': cps[i] = ')'; break;
                case ')': cps[i] = '('; break;
                case '[': cps[i] = ']'; break;
                case ']': cps[i] = '['; break;
                case '{': cps[i] = '}'; break;
                case '}': cps[i] = '{'; break;
                case '<': cps[i] = '>'; break;
                case '>': cps[i] = '<'; break;
            }
        }
    }
}

char* process_arabic(const char* text) {
    if (!text) return NULL;
    if (!contains_arabic(text)) return strdup(text);

    uint32_t* cps = NULL;
    size_t count = utf8_to_utf32(text, &cps);
    
    apply_shaping(cps, count);
    apply_bidi(cps, count);
    fix_parentheses(cps, count);
    
    char* result = utf32_to_utf8(cps, count);
    free(cps);
    return result;
}
