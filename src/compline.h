/*
 * Copyright 2020 Mihai Bazon
 *
 * Permission to use, copy, modify, and/or distribute this software
 * for any purpose with or without fee is hereby granted, provided that
 * the above copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR
 * BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR
 * ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#ifndef __COMPLINE_H
#define __COMPLINE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <iup.h>

#include "completion.h"
#include "history.h"

#define MAX_HISTWORD_CHARS 2048

typedef struct _CompLine CompLine;

struct _CompLine
{
   Ihandle * text;
   Ihandle * popup;
   Ihandle * list;
   Ihandle * timer;

   StrList matches;
   int match_index;
   int pos_in_text;

   HistoryFile * hist;
   int hist_search_mode;
   int hist_search_match_start;
   char hist_word[MAX_HISTWORD_CHARS];
   int hist_word_count;

   int tabtimeout;
   int show_dot_files;

   void (* cb_cancel)      (CompLine * cl);
   void (* cb_activate)    (CompLine * cl);
   void (* cb_runwithterm) (CompLine * cl);
   void (* cb_unique)      (CompLine * cl);
   void (* cb_notunique)   (CompLine * cl);
   void (* cb_incomplete)  (CompLine * cl);
   void (* cb_search_mode) (CompLine * cl);
   void (* cb_search_letter) (CompLine * cl);
   void (* cb_search_not_found) (CompLine * cl);
   void (* cb_ext_handler) (CompLine * cl, const char * filename);
};

CompLine * compline_new (void);
void compline_destroy (CompLine * cl);

const char * compline_get_text (CompLine * cl);
void compline_set_text (CompLine * cl, const char * text);
void compline_clear_selection (CompLine * cl);
void compline_last_history_item (CompLine * cl);

#ifdef __cplusplus
}
#endif

#endif
