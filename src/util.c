/*
 * This is free and unencumbered software released into the public domain.
 *
 * For more information, please refer to <https://unlicense.org>
 */

#include <ctype.h>
#include <stdarg.h>
#include <pwd.h>
#include <unistd.h>

#include "util.h"

void strlist_init (StrList * list)
{
   list->items = NULL;
   list->count = 0;
   list->size  = 0;
}

void strlist_add (StrList * list, char * item)
{
   if (list->count == list->size) {
      list->size = list->size ? list->size * 2 : 64;
      list->items = (char **) realloc (list->items, list->size * sizeof (char *));
   }
   list->items[list->count++] = item;
}

void strlist_add_copy (StrList * list, const char * item)
{
   strlist_add (list, str_dup (item));
}

int strlist_find (StrList * list, const char * item)
{
   int i;
   for (i = 0; i < list->count; i++) {
      if (strcmp (list->items[i], item) == 0) {
         return i;
      }
   }
   return -1;
}

static int strlist_compare (const void * a, const void * b)
{
   return strcmp (*(const char **) a, *(const char **) b);
}

void strlist_sort (StrList * list)
{
   if (list->count > 1) {
      qsort (list->items, list->count, sizeof (char *), strlist_compare);
   }
}

void strlist_clear (StrList * list)
{
   int i;
   for (i = 0; i < list->count; i++) {
      free (list->items[i]);
   }
   free (list->items);
   strlist_init (list);
}

char * str_dup (const char * str)
{
   return str ? strdup (str) : NULL;
}

char * str_concat (const char * first, ...)
{
   va_list args;
   const char * str;
   size_t len = 0;
   char * out;

   if (!first) {
      return NULL;
   }

   len = strlen (first);
   va_start (args, first);
   while ((str = va_arg (args, const char *)) != NULL) {
      len += strlen (str);
   }
   va_end (args);

   out = (char *) malloc (len + 1);
   strcpy (out, first);

   va_start (args, first);
   while ((str = va_arg (args, const char *)) != NULL) {
      strcat (out, str);
   }
   va_end (args);

   return out;
}

char * str_strip (char * str)
{
   char * end;

   while (*str && isspace ((unsigned char) *str)) {
      str++;
   }
   if (!*str) {
      return str;
   }
   end = str + strlen (str) - 1;
   while (end > str && isspace ((unsigned char) *end)) {
      *end-- = 0;
   }
   return str;
}

char * str_home_dir (void)
{
   char * home = getenv ("HOME");
   if (!home || !*home) {
      struct passwd * pw = getpwuid (getuid ());
      home = pw ? pw->pw_dir : NULL;
   }
   return home;
}

char * str_shell_first_word (const char * text, const char ** rest)
{
   char * word = (char *) malloc (strlen (text) + 1);
   char * dst = word;
   char quote = 0;
   int found = 0;

   while (*text == ' ' || *text == '\t' || *text == '\n') {
      text++;
   }

   while (*text && (quote || (*text != ' ' && *text != '\t' && *text != '\n')))
   {
      if (quote == '\'') {
         if (*text == '\'') quote = 0;
         else *dst++ = *text;
      } else if (quote == '"') {
         if (*text == '"') {
            quote = 0;
         } else if (*text == '\\' && text[1] && strchr ("\"\\`$\n", text[1])) {
            text++;
            if (*text != '\n') *dst++ = *text;
         } else {
            *dst++ = *text;
         }
      } else if (*text == '\\' && text[1] == '\n') {
         text++;
         text++;
         continue;
      } else if (*text == '\'' || *text == '"') {
         quote = *text;
      } else if (*text == '\\') {
         text++;
         if (!*text) {
            quote = '\\';
            break;
         }
         *dst++ = *text;
      } else {
         *dst++ = *text;
      }
      found = 1;
      text++;
   }
   *dst = 0;

   if (!found || quote) {
      free (word);
      return NULL;
   }

   while (*text == ' ' || *text == '\t' || *text == '\n') {
      text++;
   }
   *rest = text;
   return word;
}
