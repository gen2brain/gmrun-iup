/*
 * This is free and unencumbered software released into the public domain.
 *
 * For more information, please refer to <https://unlicense.org>
 */

#include <strings.h>

#include <iup.h>

#include "config_prefs.h"

#define APP_CONFIG_FILE "gmrunrc"

// ============================================================
//                        PRIVATE
// ============================================================

struct _pref_item
{
   char * key;
   char * value;
};
typedef struct _pref_item pref_item;

struct _pref_list
{
   pref_item ** items;
   int count;
   int size;
};
typedef struct _pref_list pref_list;

static pref_list Prefs;
static pref_list Extensions;

static void pref_item_free (pref_item * item)
{
   if (item) {
      if (item->key)   free (item->key);
      if (item->value) free (item->value);
      free (item);
   }
}

static void pref_list_clear (pref_list * list)
{
   int i;
   for (i = 0; i < list->count; i++) {
      pref_item_free (list->items[i]);
   }
   free (list->items);
   list->items = NULL;
   list->count = 0;
   list->size  = 0;
}

static pref_item * config_find_key (pref_list * list, const char * key)
{
   int i;

   if (!key || !*key) { /* ignore empty keys (strings) */
      return (NULL);
   }

   for (i = 0; i < list->count; i++) {
      if (strcasecmp (key, list->items[i]->key) == 0) {
         return (list->items[i]);
      }
   }
   return (NULL); /* key not found */
}


static void config_replace_key (pref_list * list, pref_item * item)
{
   pref_item * found = config_find_key (list, item->key);
   if (found) {
      /* only update found item */
      if (strcmp (found->value, item->value) == 0) {
         pref_item_free (item);
         return; /* values are equal, nothing to update */
      }
      free (found->value);
      found->value = str_dup (item->value);
      pref_item_free (item);
   } else {
      /* append item */
      if (list->count == list->size) {
         list->size = list->size ? list->size * 2 : 32;
         list->items = (pref_item **) realloc (list->items, list->size * sizeof (pref_item *));
      }
      list->items[list->count++] = item;
   }
}


/** get value, it's always a string **/
static char * config_get_item_value (pref_list * list, const char * key)
{
   pref_item * item = config_find_key (list, key);
   if (item) {
      return (item->value);
   }
   return (NULL); /* key not found */
}


static void config_load_from_file (const char * filename, pref_list * out_list)
{
   FILE *fp;
   char buf[1024];

   char * stripped;
   char * delim;
   pref_item * item;

   fp = fopen (filename, "r");
   if (!fp) {
      return;
   }

   /* Read file line by line */
   while (fgets (buf, sizeof (buf), fp))
   {
      stripped = buf;
      while (*stripped && *stripped <= 0x20) { // 32 = space
         stripped++;
      }
      if (strlen (stripped) < 3 || *stripped == '#') {
         continue;
      }
      delim = strchr (stripped, '=');
      if (!delim) {
         continue;
      }

      *delim = 0;
      item = (pref_item *) calloc (1, sizeof (pref_item));
      item->key   = str_dup (str_strip (stripped));
      item->value = str_dup (str_strip (delim + 1));

      if (!*item->key || !*item->value) {
         pref_item_free (item);
         continue;
      }

      /* Insert or replace item */
      config_replace_key (out_list, item);
    }

    fclose (fp);
    return;
}


static void create_extension_handler_list (void)
{
   int i;
   pref_item * item, * item_out;
   char * extensions, * ext, * next;

   for (i = 0; i < Prefs.count; i++)
   {
      item = Prefs.items[i];
      if (strncasecmp (item->key, "EXT:", 4) == 0)
      {
         extensions = str_dup (item->key + 4);
         ext = extensions;
         while (ext && *ext)
         {
            next = strchr (ext, ',');
            if (next) {
               *next = 0;
               next++;
            }
            item_out = (pref_item *) calloc (1, sizeof (pref_item));
            item_out->key   = str_dup (str_strip (ext));
            item_out->value = str_dup (item->value);
            config_replace_key (&Extensions, item_out);
            ext = next;
         }
         free (extensions);
      }
   }
}


static char * replace_variable (char * txt) /* config_get_string_expanded() */
{
   // pre${variable}post  : ${Terminal} -e ...
   char * pre = NULL, * post = NULL;
   char * variable = NULL;
   char * variable_value = NULL;
   char * new_text = NULL;
   char * p, * p2;

   if (strlen (txt) < 5) { // at least ${xx}
      return (NULL);
   }
   p  = strstr (txt, "${");
   if (!p) {
      return (NULL);  // syntax error
   }
   if (!strchr (p + 3, '}')) {
      return (NULL);  // syntax error
   }

   if (txt[0] != '$' && txt[1] != '$') {
      pre = str_dup (txt);
      p2 = strchr (pre, '$');
      if (p2) *p2 = 0;
   }

   variable = str_dup (p + 2); // variable start
   p2 = strchr (variable, '}'); // variable end
   *p2 = 0;                     // `Terminal`
   post = strchr (p, '}') + 1;  // ` -e ...`

   variable_value = config_get_item_value (&Prefs, variable); // xterm

   if (variable_value) {
      if (pre) {
         // pre xterm -e ...
         new_text = str_concat (pre, variable_value, post, NULL);
      } else {
         // xterm -e ...
         new_text = str_concat (variable_value, post, NULL);
      }
   }

   if (pre)      free (pre);
   if (variable) free (variable);

   return (new_text);
}


// ============================================================
//                     PUBLIC
// ============================================================

void config_init ()
{
   char config_file[1024];
   char * dir;

   if (Prefs.count) {
      return;
   }

   snprintf (config_file, sizeof (config_file), "/etc/%s", APP_CONFIG_FILE);
   config_load_from_file (config_file, &Prefs);

#ifdef FOLLOW_XDG_SPEC
   dir = IupGetGlobal ("CONFIGDIR");
   if (dir) {
      snprintf (config_file, sizeof (config_file), "%s/%s", dir, APP_CONFIG_FILE);
      config_load_from_file (config_file, &Prefs);
   }
#else
   dir = str_home_dir ();
   if (dir) {
      snprintf (config_file, sizeof (config_file), "%s/.%s", dir, APP_CONFIG_FILE);
      config_load_from_file (config_file, &Prefs);
   }
#endif

   create_extension_handler_list ();
}


void config_destroy ()
{
   pref_list_clear (&Prefs);
   pref_list_clear (&Extensions);
}


void config_reload ()
{
   config_destroy ();
   config_init ();
}


void config_print ()
{
   int i;
   for (i = 0; i < Prefs.count; i++) {
      printf ("%s = %s\n", Prefs.items[i]->key, Prefs.items[i]->value);
   }
   for (i = 0; i < Extensions.count; i++) {
      printf ("%s = %s\n", Extensions.items[i]->key, Extensions.items[i]->value);
   }
}


int config_get_int (const char * key, int * out_int)
{
   char * value;
   value = config_get_item_value (&Prefs, key);
   if (value) {
      *out_int = (int) strtoll (value, NULL, 0);
      return 1;
   } else {
      *out_int = -1;
      return 0;
   }
}


// returns a string that must be freed with free()
int config_get_string_expanded (const char * key, char ** out_str)
{
   char * value1, * value2, * value = NULL;

   value1 = config_get_item_value (&Prefs, key);
   if (value1 && strstr (value1, "${")) {
      value2 = replace_variable (value1);
      value = value2;
      // expand variable up to 2 times
      if (value2 && strstr (value2, "${")) {
         value = replace_variable (value2);
         free (value2);
      }
   } else if (value1) {
      value = str_dup (value1);
   }

   if (value) {
      *out_str = value;
      return 1;
   } else {
      *out_str = NULL;
      return 0;
   }
}


int config_get_string (const char * key, char ** out_str)
{
   char * value;
   value = config_get_item_value (&Prefs, key);
   if (value) {
      *out_str = value;
      return 1;
   } else {
      *out_str = NULL;
      return 0;
   }
}


char * config_get_handler_for_extension (const char * extension)
{
   char * handler;
   if (extension && *extension == '.') {
      extension++; // .html -> html
   }
   handler = config_get_item_value (&Extensions, extension);
   return (handler);
}
