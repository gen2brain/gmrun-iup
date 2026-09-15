/*
 * This is free and unencumbered software released into the public domain.
 *
 * For more information, please refer to <https://unlicense.org>
 */

#include <ctype.h>
#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>

#include "completion.h"

static StrList path_dirs;
static int path_dirs_loaded = 0;

static const char * scan_prefix = NULL;
static int scan_dot_files = 0;

static const char * get_token (const char * str, char * out_buf, int out_buf_len)
{
   int escaped = 0;
   int x = 0;

   *out_buf = 0;
   while (*str != '\0')
   {
      if (escaped) {
         escaped = 0;
         out_buf[x++] = *str;
      } else if (*str == '\\') {
         escaped = 1;
      } else if (isspace ((unsigned char) *str)) {
         while (isspace ((unsigned char) *str)) str++;
         break;
      } else {
         out_buf[x++] = *str;
      }
      if (x >= out_buf_len) {
         break;
      }
      str++;
   }
   out_buf[x] = 0;
   return str;
}


int compl_split_words (const char * text, int caret, StrList * words)
{
   const char * i = text;
   int n_w = 0;
   char tmp[2048] = "";

   while (*i != '\0')
   {
      i = get_token (i, tmp, 2000);
      strlist_add_copy (words, tmp);
      if (*i && (i - text < caret) && (i != text)) {
         n_w++;
      }
   }

   if (!words->count) { // must add empty string otherwise a segfault awaits
      strlist_add_copy (words, "");
   }

   return n_w;
}


char * compl_join_words (StrList * words, int pos, int * out_caret)
{
   int w, len = 0;
   int i = 0, caret = 0;
   char * out, * word;

   for (w = 0; w < words->count; w++) {
      len += strlen (words->items[w]) * 2 + 2;
   }

   out = (char *) calloc (1, len + 1);

   if (pos == -1) {
      pos = words->count - 1;
   }

   for (w = 0; w < words->count; w++)
   {
      // replace ' ' with '\ ' [escape]
      word = words->items[w];
      while (*word) {
         if (*word == ' ') {
            out[i++] = '\\';
         }
         out[i++] = *word;
         word++;
      }

      // add space if not the last word
      if (w != words->count - 1) {
         out[i++] = ' ';
      }
      if (!pos && !caret) {
         caret = i;
      } else {
         --pos;
      }
   }
   out[i] = 0;

   if (out_caret) {
      *out_caret = caret;
   }
   return out;
}


char * compl_expand_tilde (const char * text)
{
   const char * match = strchr (text, '~');
   const char * home;
   char * out;
   int cur;

   if (!match) {
      return NULL;
   }
   cur = match - text;
   if (cur > 0 && text[cur - 1] != ' ') {
      return NULL;
   }
   if ((size_t) cur < strlen (text) - 1 && text[cur + 1] != '/') {
      // FIXME: Parse another user's home
      return NULL;
   }

   home = str_home_dir ();
   if (!home) {
      return NULL;
   }

   out = (char *) malloc (strlen (text) + strlen (home) + 1);
   memcpy (out, text, cur);
   out[cur] = 0;
   strcat (out, home);
   strcat (out, text + cur + 1);
   return out;
}


static int select_matching (const struct dirent * dent)
{
   int len = strlen (dent->d_name);
   int lenp = scan_prefix ? strlen (scan_prefix) : 0;

   if (dent->d_name[0] == '.') {
      if (!scan_dot_files)
         return 0;
      if (dent->d_name[1] == '\0')
         return 0;
      if ((dent->d_name[1] == '.') && (dent->d_name[2] == '\0'))
         return 0;
   }
   if (dent->d_name[len - 1] == '~')
      return 0;
   if (lenp == 0)
      return 1;
   if (lenp > len)
      return 0;

   if (strncmp (dent->d_name, scan_prefix, lenp) == 0)
      return 1;

   return 0;
}


static void load_path_dirs (void)
{
   char * path_env, * path, * dir, * next;
   char resolved[PATH_MAX];
   struct stat sb;

   if (path_dirs_loaded) {
      return;
   }
   path_dirs_loaded = 1;
   strlist_init (&path_dirs);

   path_env = getenv ("PATH");
   if (!path_env) {
      return;
   }

   path = str_dup (path_env);
   dir = path;
   while (dir && *dir)
   {
      next = strchr (dir, ':');
      if (next) {
         *next = 0;
         next++;
      }

      // deal with syminks and duplicate dirs
      *resolved = 0;
      if (lstat (dir, &sb) == 0) {
         if (S_ISLNK (sb.st_mode)) {
            if (!realpath (dir, resolved)) {
               *resolved = 0;
            }
         } else if (!S_ISDIR (sb.st_mode)) {
            dir = next;
            continue;
         }
         if (*resolved) {
            dir = resolved;
         }
         if (strlist_find (&path_dirs, dir) < 0) {
            strlist_add_copy (&path_dirs, dir);
         }
      }
      dir = next;
   }

   free (path);
}


/* Iterates though PATH and list all executables */
static void generate_execs_list (const char * pfix, StrList * out)
{
   struct dirent ** eps;
   int d, j, n;

   load_path_dirs ();

   scan_prefix = pfix;
   for (d = 0; d < path_dirs.count; d++)
   {
      n = scandir (path_dirs.items[d], &eps, select_matching, NULL);
      if (n >= 0) {
         for (j = 0; j < n; j++) {
            if (strlist_find (out, eps[j]->d_name) < 0) {
               strlist_add_copy (out, eps[j]->d_name);
            }
            free (eps[j]);
         }
         free (eps);
      }
   }
   scan_prefix = NULL;
}


static void generate_dirlist (const char * path, StrList * out)
{
   struct dirent ** eps;
   struct stat filestatus;
   char * str = str_dup (path);
   char * filename = strrchr (str, '/');
   char * dir, * file;
   int j, n, slashes = 0;
   char * p = str;

   while (*p) {
      if (*p == '/') slashes++;
      p++;
   }

   if (slashes == 1) {
      dir = "/";
   } else {
      dir = str;
   }

   *filename = '\0';
   filename++;
   scan_prefix = filename;

   n = scandir (dir, &eps, select_matching, NULL);
   if (n >= 0) {
      for (j = 0; j < n; j++)
      {
         file = str_concat (str, "/", eps[j]->d_name, NULL);
         if (stat (file, &filestatus) == 0 && S_ISDIR (filestatus.st_mode)) {
            char * with_slash = str_concat (file, "/", NULL);
            free (file);
            file = with_slash;
         }
         strlist_add (out, file);
         free (eps[j]);
      }
      free (eps);
   }

   scan_prefix = NULL;
   free (str);
}


int compl_generate (const char * word, int show_dot_files, StrList * out)
{
   scan_dot_files = show_dot_files;

   if (word[0] != '/') {
      generate_execs_list (word, out);
   } else {
      generate_dirlist (word, out);
   }

   strlist_sort (out);
   return out->count;
}


void compl_cleanup (void)
{
   if (path_dirs_loaded) {
      strlist_clear (&path_dirs);
      path_dirs_loaded = 0;
   }
}
