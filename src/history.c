/*
 * This is free and unencumbered software released into the public domain.
 *
 * For more information, please refer to <https://unlicense.org>
 */

#include "history.h"

// ============================================================
//                        PRIVATE
// ============================================================

struct _Whistory
{
   long int index;
   unsigned int count;
   unsigned int max;
   char  * filename;
   int has_changed;
   StrList list;
};

static void _history_clear (HistoryFile * history)
{
   // keep history->filename (destroyed in _history_free())
   if (history->list.count) {
      strlist_clear (&history->list);
      history->has_changed = 1;
   }
   history->index = 0;
   history->count = 0;
}


static void _history_free (HistoryFile * history)
{
   _history_clear (history);
   strlist_clear (&history->list);
   if (history->filename) {
      free (history->filename);
      history->filename = NULL;
   }
   free (history);
}


/// load entries from file and initialize private variables
static void _history_load_from_file (HistoryFile * history, const char * filename)
{
   FILE *fp;
   char buf[1024];
   char * p;
   size_t len;
   unsigned int max = history->max;

   fp = fopen (filename, "r");
   if (!fp) {
      return;
   }

   /* Read file line by line */
   while (fgets (buf, sizeof (buf), fp))
   {
      p = buf;
      while (*p && *p <= 0x20) { // 32 = space [ignore spaces]
         p++;
      }
      if (!*p) {
         continue;
      }

      len = strlen (p);
      if (len && p[len-1] == '\n') {
         p[len-1] = 0;
      }
      if (max > 0 && history->list.count >= (int) max) {
         break;
      }
      strlist_add_copy (&history->list, p);
   }

   history->count = history->list.count;
   if (history->list.count) {
      history->index = 1; // current = 1st item
   }
   fclose (fp);
   return;
}


static void _history_write_to_file (HistoryFile * history, const char * filename)
{
   FILE *fp;
   int i;

   fp = fopen (filename, "w");
   if (!fp) {
      return;
   }
   for (i = 0; i < history->list.count; i++) {
      fprintf (fp, "%s\n", history->list.items[i]);
   }
   fclose (fp);
   return;
}


// ============================================================
//                     PUBLIC
// ============================================================

HistoryFile * history_new (const char * filename, unsigned int maxcount)
{
   HistoryFile * history = calloc (1, sizeof (HistoryFile));
   strlist_init (&history->list);
   history->max = maxcount;
   history->index = 0;
   if (filename && *filename) {
      history->filename = strdup (filename);
      _history_load_from_file (history, filename);
   }
   return (history);
}

void history_save (HistoryFile * history, int save_if_changed)
{
   if (history && history->filename) {
      if (save_if_changed) {
         if (history->has_changed) {
            _history_write_to_file (history, history->filename);
         }
      } else {
         _history_write_to_file (history, history->filename);
      }
   } else {
      fprintf (stderr, "history_save(): history or filename is NULL\n");
   }
}

void history_destroy (HistoryFile * history)
{
   if (history) {
      _history_free (history);
   }
}

void history_reload (HistoryFile * history)
{
   if (history) {
      _history_clear (history);
      if (history->filename) {
         _history_load_from_file (history, history->filename);
      }
      history->has_changed = 0;
   }
}


void history_print (HistoryFile * history)
{
   int i;
   if (history) {
      for (i = 0; i < history->list.count; i++) {
         printf ("[%d] %s\n", i + 1, history->list.items[i]);
      }
      printf ("-- list internal count: [%d]\n", history->count);
      if (history->list.count) {
         printf ("** list     : %s\n", history->list.items[0]);
         printf ("** list_end : %s\n", history->list.items[history->list.count - 1]);
      }
      if (history->index > 0) {
         printf ("** list current : %s\n", history->list.items[history->index - 1]);
      }
   }
}


// some apps might want to handle prev/next in a special way
void history_unset_current (HistoryFile * history)
{
   if (history) {
      history->index = -1;
   }
}


const char * history_get_current (HistoryFile * history)
{
   if (history && history->index > 0 && history->index <= history->list.count) {
      return (history->list.items[history->index - 1]);
   }
   return (NULL);
}


int history_get_current_index (HistoryFile * history)
{
   if (history) return (history->index);
   else         return (-1);
}


const char * history_next (HistoryFile * history)
{
   if (history->index > 0 && history->index < history->list.count) {
      history->index++;
      return (history->list.items[history->index - 1]);
   }
   return (NULL);
}


const char * history_prev (HistoryFile * history)
{
   if (history->index > 1) {
      history->index--;
      return (history->list.items[history->index - 1]);
   }
   return (NULL);
}


const char * history_first (HistoryFile * history)
{
   if (history->list.count) {
      history->index = 1;
      return (history->list.items[0]);
   }
   return (NULL);
}


const char * history_last (HistoryFile * history)
{
   if (history->list.count) {
      history->index = history->list.count;
      return (history->list.items[history->index - 1]);
   }
   return (NULL);
}


void history_append (HistoryFile * history, const char * text)
{
   int i;

   if (!text || !*text) {
      return;
   }

   // if new entry = last entry, then abort
   if (history->list.count
       && strcmp (text, history->list.items[history->list.count - 1]) == 0) {
      return;
   }

   // do not allow duplicate entries, remove existing entry
   i = strlist_find (&history->list, text);
   if (i >= 0) {
      free (history->list.items[i]);
      memmove (&history->list.items[i], &history->list.items[i+1],
               (history->list.count - i - 1) * sizeof (char *));
      history->list.count--;
      history->count--;
      if (history->index > i) {
         history->index--;
      }
   }

   strlist_add_copy (&history->list, text);
   history->count = history->list.count;
   history->has_changed = 1;
   if (history->index == 0) {
      history->index = 1;
   }

   // if new entry exceeds the max count, first entry will be removed
   if (history->max > 0 && history->list.count > (int) history->max) {
      free (history->list.items[0]);
      memmove (&history->list.items[0], &history->list.items[1],
               (history->list.count - 1) * sizeof (char *));
      history->list.count--;
      history->count--;
      if (history->index > 1) {
         history->index--;
      }
   }
}


void history_reverse (HistoryFile * history)
{
   int i, j;
   char * tmp;

   if (history && history->list.count) {
      for (i = 0, j = history->list.count - 1; i < j; i++, j--) {
         tmp = history->list.items[i];
         history->list.items[i] = history->list.items[j];
         history->list.items[j] = tmp;
      }
      if (history->index > 0) {
         history->index = history->list.count - history->index + 1;
      }
   }
}
