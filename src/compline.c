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

#include <ctype.h>
#include <iupkey.h>

#include "compline.h"
#include "config_prefs.h"

#define HISTORY_FILE "gmrun_history"

#define COMPLETION_LIST_HEIGHT 150

static const char * (* history_search_first_func) (HistoryFile *) = history_first;
static const char * (* history_search_next_func)  (HistoryFile *) = history_next;
static int searching_history = 0;

static void complete_from_list (CompLine * cl, const char * cword);
static int tab_pressed (CompLine * cl);

const char * compline_get_text (CompLine * cl)
{
   return IupGetAttribute (cl->text, "VALUE");
}

void compline_set_text (CompLine * cl, const char * text)
{
   IupSetStrAttribute (cl->text, "VALUE", text ? text : "");
   IupSetInt (cl->text, "CARETPOS", (int) strlen (text ? text : ""));
}

static int compline_get_caret (CompLine * cl)
{
   return IupGetInt (cl->text, "CARETPOS");
}

void compline_clear_selection (CompLine * cl)
{
   IupSetAttribute (cl->text, "SELECTIONPOS", "NONE");
}

void compline_last_history_item (CompLine * cl)
{
   const char * text = history_last (cl->hist);
   if (text) {
      compline_set_text (cl, text);
   }
}

static void parse_tilda (CompLine * cl)
{
   char * expanded = compl_expand_tilde (compline_get_text (cl));
   if (expanded) {
      compline_set_text (cl, expanded);
      free (expanded);
   }
}

static void destroy_completion_window (CompLine * cl)
{
   if (!cl->popup) {
      return;
   }
   IupSetAttribute (cl->popup, "VISIBLE", "NO");
   IupDestroy (cl->popup);
   cl->popup = NULL;
   cl->list = NULL;
   strlist_clear (&cl->matches);
   cl->match_index = 0;
}

static int list_action_cb (Ihandle * list, char * text, int item, int state)
{
   CompLine * cl = (CompLine *) IupGetAttribute (list, "_GMRUN_COMPLINE");

   (void) text;

   if (cl && state) {
      cl->match_index = item - 1;
      complete_from_list (cl, NULL);
   }
   return IUP_DEFAULT;
}

static void create_completion_window (CompLine * cl)
{
   int i;

   cl->list = IupList ();
   IupSetAttribute (cl->list, "EXPAND", "YES");
   IupSetAttribute (cl->list, "CANFOCUS", "NO");
   IupSetAttribute (cl->list, "SCROLLBAR", "YES");
   IupSetAttribute (cl->list, "AUTOHIDE", "YES");
   IupSetAttribute (cl->list, "_GMRUN_COMPLINE", (char *) cl);
   IupSetCallback (cl->list, "ACTION", (Icallback) list_action_cb);

   for (i = 0; i < cl->matches.count; i++) {
      IupSetAttributeId (cl->list, "", i + 1, cl->matches.items[i]);
   }

   cl->popup = IupPopover (cl->list);
   IupSetAttributeHandle (cl->popup, "ANCHOR", cl->text);
   IupSetAttribute (cl->popup, "POSITION", "BOTTOMLEFT");
   IupSetAttribute (cl->popup, "AUTOHIDE", "NO");
   IupSetAttribute (cl->popup, "ARROW", "NO");
   IupSetInt (cl->popup, "RASTERSIZE_H", COMPLETION_LIST_HEIGHT);
   IupSetStrf (cl->popup, "RASTERSIZE", "%dx%d",
               IupGetInt (cl->text, "RASTERSIZE"), COMPLETION_LIST_HEIGHT);

   cl->match_index = 0;
   IupSetAttribute (cl->popup, "VISIBLE", "YES");
   IupSetInt (cl->list, "VALUE", 1);

   IupSetFocus (cl->text);
}

static void select_match (CompLine * cl, int index)
{
   if (!cl->popup || !cl->matches.count) {
      return;
   }
   if (index < 0) {
      index = cl->matches.count - 1;
   } else if (index >= cl->matches.count) {
      index = 0;
   }
   cl->match_index = index;
   IupSetInt (cl->list, "VALUE", index + 1);
   IupSetInt (cl->list, "TOPITEM", index + 1);
   complete_from_list (cl, NULL);
}

static void complete_from_list (CompLine * cl, const char * cword)
{
   StrList words;
   const char * word = NULL;
   char * joined;
   int pos, caret;

   parse_tilda (cl);

   strlist_init (&words);
   pos = compl_split_words (compline_get_text (cl), compline_get_caret (cl), &words);

   if (cl->popup && cl->matches.count) {
      word = cl->matches.items[cl->match_index];
   } else {
      word = cword;
   }

   if (word) {
      free (words.items[pos]);
      words.items[pos] = str_dup (word);
   }

   joined = compl_join_words (&words, pos, &caret);
   IupSetStrAttribute (cl->text, "VALUE", joined);
   IupSetInt (cl->text, "CARETPOS", caret);
   IupSetStrf (cl->text, "SELECTIONPOS", "%d:%d", cl->pos_in_text, caret);

   if (words.count == 1 && cl->cb_ext_handler) {
      cl->cb_ext_handler (cl, words.items[0]);
   }

   free (joined);
   strlist_clear (&words);
}

// called by tab_pressed() only if completion window doesn't exist
static void complete_line (CompLine * cl)
{
   StrList words;
   int pos, num_items;
   char * word;

   parse_tilda (cl);

   strlist_init (&words);
   pos = compl_split_words (compline_get_text (cl), compline_get_caret (cl), &words);
   word = words.items[pos];

   strlist_init (&cl->matches);
   num_items = compl_generate (word, cl->show_dot_files, &cl->matches);

   if (num_items == 1) {
      complete_from_list (cl, cl->matches.items[0]);
      if (cl->cb_unique) cl->cb_unique (cl);
      compline_clear_selection (cl);
      strlist_clear (&cl->matches);
      strlist_clear (&words);
      return;
   } else if (num_items == 0) {
      if (cl->cb_incomplete) cl->cb_incomplete (cl);
      strlist_clear (&cl->matches);
      strlist_clear (&words);
      return;
   }

   /*** num_items > 1 ***/
   if (cl->cb_notunique) cl->cb_notunique (cl);

   cl->pos_in_text = compline_get_caret (cl);
   create_completion_window (cl);
   complete_from_list (cl, NULL);

   strlist_clear (&words);
}

static void up_history (CompLine * cl)
{
   static int pause = 0;
   const char * text_up;

   if (pause == 1) {
      text_up = history_last (cl->hist);
      pause = 0;
   } else {
      text_up = history_prev (cl->hist);
      if (!text_up) {  // empty, set a flag, next time we'll get something
         pause = 1;
         text_up = "";
      }
   }
   if (text_up) {
      compline_set_text (cl, text_up);
   }
}

static void down_history (CompLine * cl)
{
   static int pause = 0;
   const char * text_down;

   if (pause == 1) {
      text_down = history_first (cl->hist);
      pause = 0;
   } else {
      text_down = history_next (cl->hist);
      if (!text_down) {  // empty, set a flag, next time we'll get something
         pause = 1;
         text_down = "";
      }
   }
   if (text_down) {
      compline_set_text (cl, text_down);
   }
}

static void search_off (CompLine * cl)
{
   cl->hist_search_mode = 0;
   memset (cl->hist_word, 0, sizeof (cl->hist_word));
   cl->hist_word_count = 0;
   if (cl->cb_search_mode) cl->cb_search_mode (cl);
   history_unset_current (cl->hist);
}

static void search_history (CompLine * cl, int next)
{ // must only be called if cl->hist_search_mode = TRUE
   searching_history = 1;

   if (cl->hist_word[0])
   {
      const char * history_current_item;
      const char * search_str = cl->hist_word;
      int search_str_len = 0;
      int search_match_start = cl->hist_search_match_start;

      if (next) {
         history_current_item = history_search_next_func (cl->hist);
      } else {
         history_current_item = history_search_first_func (cl->hist);
      }

      if (search_match_start) {  /* ! */
         search_str_len = strlen (search_str);
      }

      while (1)
      {
         const char * s = NULL;
         if (history_current_item) {
            if (search_match_start) { /* ! */
               if (strncmp (history_current_item, search_str, search_str_len) == 0) {
                  s = history_current_item;
               }
            } else { /* CTRL-R / CTRL-S */
               s = strstr (history_current_item, search_str);
            }
         }
         if (s) {
            compline_set_text (cl, history_current_item);
            if (cl->cb_search_letter) cl->cb_search_letter (cl);
            searching_history = 0;
            return;
         }
         history_current_item = history_search_next_func (cl->hist);
         if (history_current_item == NULL) {
            if (cl->cb_search_not_found) cl->cb_search_not_found (cl);
            break;
         }
      }
   }

   if (cl->cb_search_letter) cl->cb_search_letter (cl);
   searching_history = 0;
}

static int tab_timer_cb (Ihandle * timer)
{
   CompLine * cl = (CompLine *) IupGetAttribute (timer, "_GMRUN_COMPLINE");
   IupSetAttribute (timer, "RUN", "NO");
   if (cl) {
      tab_pressed (cl);
   }
   return IUP_DEFAULT;
}

static void tab_timer_stop (CompLine * cl)
{
   if (cl->timer) {
      IupSetAttribute (cl->timer, "RUN", "NO");
   }
}

static void tab_timer_start (CompLine * cl)
{
   if (!cl->tabtimeout) {
      return;
   }
   if (!cl->timer) {
      cl->timer = IupTimer ();
      IupSetAttribute (cl->timer, "_GMRUN_COMPLINE", (char *) cl);
      IupSetCallback (cl->timer, "ACTION_CB", (Icallback) tab_timer_cb);
   }
   IupSetInt (cl->timer, "TIME", cl->tabtimeout);
   IupSetAttribute (cl->timer, "RUN", "YES");
}

static int tab_pressed (CompLine * cl)
{
   if (cl->hist_search_mode) {
      search_off (cl);
   }
   if (cl->popup) {
      // completion window exists, avoid calling complete_line()
      select_match (cl, cl->match_index + 1);
   } else {
      complete_line (cl);
   }
   return 0;
}

static int key_cb (Ihandle * text, int c)
{
   CompLine * cl = (CompLine *) IupGetAttribute (text, "_GMRUN_COMPLINE");

   if (!cl) {
      return IUP_DEFAULT;
   }

   switch (c)
   {
      case K_TAB:
         tab_timer_stop (cl);
         tab_pressed (cl);
         return IUP_IGNORE;

      case K_UP:
      case K_cP:
         if (cl->popup) {
            select_match (cl, cl->match_index - 1);
         } else {
            up_history (cl);
         }
         if (cl->hist_search_mode) {
            search_off (cl);
         }
         return IUP_IGNORE;

      case K_DOWN:
      case K_cN:
         if (cl->popup) {
            select_match (cl, cl->match_index + 1);
         } else {
            down_history (cl);
         }
         if (cl->hist_search_mode) {
            search_off (cl);
         }
         return IUP_IGNORE;

      case K_SP:
         if (cl->hist_search_mode) {
            search_off (cl);
         }
         destroy_completion_window (cl);
         return IUP_DEFAULT;

      case K_CR:
      case K_cCR:
         destroy_completion_window (cl);
         if (c == K_cCR) {
            if (cl->cb_runwithterm) cl->cb_runwithterm (cl);
         } else {
            if (cl->cb_activate) cl->cb_activate (cl);
         }
         return IUP_IGNORE;

      case K_cS:
      case K_cR:
         if (!searching_history) {
            /* set proper funcs for forward/backward search */
            if (c == K_cR) {  /* reverse - backward */
               history_search_first_func = history_last;
               history_search_next_func  = history_prev;
            } else { /* from start - forward */
               history_search_first_func = history_first;
               history_search_next_func  = history_next;
            }
         }
         if (!cl->hist_search_mode) {
            compline_set_text (cl, "");
            cl->hist_search_mode = 1;
            cl->hist_search_match_start = 0;
            cl->hist_word[0] = 0;
            cl->hist_word_count = 0;
            if (cl->cb_search_mode) cl->cb_search_mode (cl);
         } else {
            // search next result for `cl->hist_word`
            search_history (cl, 1);
         }
         return IUP_IGNORE;

      case K_exclam:
         if (!cl->hist_search_mode) {
            const char * entry_text = compline_get_text (cl);
            if (!*entry_text) {
               history_search_first_func = history_last;
               history_search_next_func  = history_prev;
               cl->hist_search_mode = 1;
               cl->hist_search_match_start = 1;
               cl->hist_word[0] = 0;
               cl->hist_word_count = 0;
               if (cl->cb_search_mode) cl->cb_search_mode (cl);
               return IUP_IGNORE;
            }
         }
         break;

      case K_BS:
         if (cl->hist_search_mode) {
            if (cl->hist_word[0]) {
               cl->hist_word_count--;
               cl->hist_word[cl->hist_word_count] = 0;
               search_history (cl, 0);
               if (cl->cb_search_letter) cl->cb_search_letter (cl);
            }
            return IUP_IGNORE;
         }
         return IUP_DEFAULT;

      case K_HOME:
      case K_END:
         compline_clear_selection (cl);
         break;

      case K_ESC:
         if (cl->hist_search_mode) {
            search_off (cl);
         } else if (cl->popup) {
            destroy_completion_window (cl);
         } else {
            // user cancelled
            if (cl->cb_cancel) cl->cb_cancel (cl);
         }
         return IUP_IGNORE;

      case K_cG:
         if (cl->hist_search_mode) {
            search_off (cl);
            compline_set_text (cl, "");
            return IUP_IGNORE;
         }
         break;
   }

   destroy_completion_window (cl);

   if (cl->hist_search_mode) {
      if (iup_isCtrlXkey (c)) {
         return IUP_IGNORE;
      }
      if (c > 0 && c < 256 && isprint (c)) {
         if (cl->hist_word_count < MAX_HISTWORD_CHARS - 1) {
            cl->hist_word[cl->hist_word_count] = (char) c;
            cl->hist_word_count++;
            cl->hist_word[cl->hist_word_count] = 0;
            search_history (cl, 0);
         }
         return IUP_IGNORE;
      }
      search_off (cl);
   }

   if (cl->tabtimeout) {
      tab_timer_stop (cl);
      if (c > 0 && c < 256 && isprint (c)) {
         tab_timer_start (cl);
      }
   }

   return IUP_DEFAULT;
}

CompLine * compline_new (void)
{
   CompLine * cl = (CompLine *) calloc (1, sizeof (CompLine));

   cl->text = IupText ();
   IupSetAttribute (cl->text, "EXPAND", "HORIZONTAL");
   IupSetAttribute (cl->text, "_GMRUN_COMPLINE", (char *) cl);
   IupSetCallback (cl->text, "K_ANY", (Icallback) key_cb);

   strlist_init (&cl->matches);

   {
      char history_file[1024];
      char * dir;
      int hist_max_size;

#ifdef FOLLOW_XDG_SPEC
      dir = IupGetGlobal ("DATADIR");
      snprintf (history_file, sizeof (history_file), "%s/" HISTORY_FILE, dir ? dir : ".");
#else
      dir = str_home_dir ();
      snprintf (history_file, sizeof (history_file), "%s/." HISTORY_FILE, dir ? dir : ".");
#endif

      if (!config_get_int ("History", &hist_max_size)) {
         hist_max_size = 20;
      }

      cl->hist = history_new (history_file, hist_max_size);
      // hacks for prev/next will be applied
      history_unset_current (cl->hist);
   }

   return cl;
}

void compline_destroy (CompLine * cl)
{
   if (!cl) {
      return;
   }
   destroy_completion_window (cl);
   if (cl->timer) {
      IupSetAttribute (cl->timer, "RUN", "NO");
      IupDestroy (cl->timer);
   }
   if (cl->hist) {
      history_save (cl->hist, HISTORY_SAVE_IF_CHANGED);
      history_destroy (cl->hist);
      cl->hist = NULL;
   }
   strlist_clear (&cl->matches);
   free (cl);
}
