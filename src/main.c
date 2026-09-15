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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <unistd.h>

#include <iup.h>

#include "compline.h"
#include "config_prefs.h"

enum
{
   W_TEXT_STYLE_NORMAL,
   W_TEXT_STYLE_NOTFOUND,
   W_TEXT_STYLE_NOTUNIQUE,
   W_TEXT_STYLE_UNIQUE,
};

static char * gmrun_text = NULL;

static Ihandle * dialog = NULL;
static Ihandle * wlabel = NULL;
static Ihandle * wlabel_search = NULL;
static Ihandle * search_timer = NULL;
static CompLine * compline = NULL;

struct _geometry
{
   int x, y, width, height;
};
static struct _geometry window_geom = { -1, -1, -1, -1 };

/* preferences */
static int SHELL_RUN = 1;

static void gmrun_exit (void);
static int search_off_timeout (Ihandle * timer);
static void search_off (void);

/// BEGIN: TIMEOUT MANAGEMENT

static void remove_search_off_timeout (void)
{
   if (search_timer) {
      IupSetAttribute (search_timer, "RUN", "NO");
   }
}

static void add_search_off_timeout (int timeout, Icallback func)
{
   remove_search_off_timeout ();
   if (!search_timer) {
      search_timer = IupTimer ();
   }
   IupSetCallback (search_timer, "ACTION_CB",
                   func ? func : (Icallback) search_off_timeout);
   IupSetInt (search_timer, "TIME", timeout);
   IupSetAttribute (search_timer, "RUN", "YES");
}

/// END: TIMEOUT MANAGEMENT

static void set_info_text_color (Ihandle * w, const char * text, int spec)
{
   static const char * colors[] = {
      NULL,          /* W_TEXT_STYLE_NORMAL */
      "255 0 0",     /* W_TEXT_STYLE_NOTFOUND */
      "0 0 255",     /* W_TEXT_STYLE_NOTUNIQUE */
      "0 128 0",     /* W_TEXT_STYLE_UNIQUE */
   };

   IupSetStrAttribute (w, "TITLE", text);
   IupSetStrAttribute (w, "FGCOLOR", colors[spec]);
}


static void run_the_command (const char * cmd)
{
   int ret;

   if (SHELL_RUN)
   {
      /* need to add extra &   */
      char * cmd2 = str_concat (cmd, " &", NULL);
      ret = system (cmd2);
      free (cmd2);
      if (ret != -1) {
         gmrun_exit ();
      } else {
         char * errmsg = str_concat ("ERROR: ", strerror (errno), NULL);
         set_info_text_color (wlabel, errmsg, W_TEXT_STYLE_NOTFOUND);
         add_search_off_timeout (3000, NULL);
         free (errmsg);
      }
   }
   else
   {
      const char * params = NULL;
      const char * p;
      char * command = str_shell_first_word (cmd, &params);
      char * errmsg;
      char * word;

      for (p = params; command && p && *p; free (word)) {
         word = str_shell_first_word (p, &p);
         if (!word) {
            free (command);
            command = NULL;
            break;
         }
      }

      if (!command) {
         set_info_text_color (wlabel, "Text was empty or ended before a matching quote",
                              W_TEXT_STYLE_NOTFOUND);
         add_search_off_timeout (3000, NULL);
         return;
      }

      ret = IupExecute (command, params);

      if (ret == 1) {
         free (command);
         gmrun_exit ();
         return;
      }

      if (ret == -2) {
         errmsg = str_concat ("Failed to execute child process \"", command, "\" (",
                              strerror (ENOENT), ")", NULL);
      } else {
         errmsg = str_concat ("Failed to execute child process \"", command, "\"", NULL);
      }
      set_info_text_color (wlabel, errmsg, W_TEXT_STYLE_NOTFOUND);
      add_search_off_timeout (3000, NULL);
      free (errmsg);
      free (command);
   }
}


static void on_ext_handler (CompLine * cl, const char * filename)
{
   const char * ext;
   const char * handler;
   char * tmp;

   (void) cl;

   if (!filename || !*filename) {
      return;
   }

   ext = strrchr (filename, '.');
   if (!ext) {
      search_off ();
      return;
   }
   handler = config_get_handler_for_extension (ext);
   if (handler) {
      tmp = str_concat ("Program: ", handler, NULL);
      IupSetStrAttribute (wlabel_search, "TITLE", tmp);
      IupSetAttribute (wlabel_search, "VISIBLE", "YES");
      free (tmp);
   }
}


static void on_compline_runwithterm (CompLine * cl)
{
   char * cmd;
   char * term;
   char * entry_text = str_dup (compline_get_text (cl));
   char * stripped = str_strip (entry_text);

   if (*stripped) {
      if (config_get_string_expanded ("TermExec", &term)) {
         cmd = str_concat (term, " ", stripped, NULL);
         free (term);
      } else {
         cmd = str_concat ("xterm -e ", stripped, NULL);
      }
   } else {
      if (config_get_string ("Terminal", &term)) {
         cmd = str_dup (term);
      } else {
         cmd = str_dup ("xterm");
      }
   }

   history_append (cl->hist, cmd);
   run_the_command (cmd);
   free (entry_text);
   free (cmd);
}

static void search_off (void)
{
   set_info_text_color (wlabel, "Run:", W_TEXT_STYLE_NORMAL);
   IupSetAttribute (wlabel_search, "VISIBLE", "NO");
}

static int search_off_timeout (Ihandle * timer)
{
   IupSetAttribute (timer, "RUN", "NO");
   search_off ();
   return IUP_DEFAULT;
}

static void on_compline_unique (CompLine * cl)
{
   (void) cl;
   set_info_text_color (wlabel, "unique", W_TEXT_STYLE_UNIQUE);
   add_search_off_timeout (1000, NULL);
}

static void on_compline_notunique (CompLine * cl)
{
   (void) cl;
   set_info_text_color (wlabel, "not unique", W_TEXT_STYLE_NOTUNIQUE);
   add_search_off_timeout (1000, NULL);
}

static void on_compline_incomplete (CompLine * cl)
{
   (void) cl;
   set_info_text_color (wlabel, "not found", W_TEXT_STYLE_NOTFOUND);
   add_search_off_timeout (1000, NULL);
}

static void on_search_mode (CompLine * cl)
{
   if (cl->hist_search_mode) {
      IupSetAttribute (wlabel_search, "VISIBLE", "YES");
      set_info_text_color (wlabel, "Search:", W_TEXT_STYLE_NORMAL);
      IupSetStrAttribute (wlabel_search, "TITLE", cl->hist_word);
   } else {
      IupSetAttribute (wlabel_search, "VISIBLE", "NO");
      set_info_text_color (wlabel, "Search OFF", W_TEXT_STYLE_NORMAL);
      add_search_off_timeout (1000, NULL);
   }
}

static void on_search_letter (CompLine * cl)
{
   IupSetStrAttribute (wlabel_search, "TITLE", cl->hist_word);
}

static int search_fail_timeout (Ihandle * timer)
{
   IupSetAttribute (timer, "RUN", "NO");
   set_info_text_color (wlabel, "Search:", W_TEXT_STYLE_NOTUNIQUE);
   return IUP_DEFAULT;
}

static void on_search_not_found (CompLine * cl)
{
   (void) cl;
   set_info_text_color (wlabel, "Not Found!", W_TEXT_STYLE_NOTFOUND);
   add_search_off_timeout (1000, (Icallback) search_fail_timeout);
}


// =============================================================

/* Handler for URLs  */
static int url_check (CompLine * cl, char * entry_text)
{
   // <url_type> <delim>  <url>
   // http          :     //www.fsf.org
   //  <f  u  l  l     u  r  l>
   // config: URL_<url_type>
   // handler %s (format 1) = run handler with <url>
   // handler %u (format 2) = run handler with <full url>
   char * cmd = NULL;
   char * tmp, * delim, * p;
   char * url, * url_type, * full_url, * chosen_url;
   char * url_handler = NULL;
   char * config_key;

   delim = strchr (entry_text, ':');
   if (!delim || !*(delim + 1)) {
      return 0;
   }
   tmp    = str_dup (entry_text);
   delim  = strchr (tmp, ':');
   *delim = 0;
   url_type = tmp;       // http
   url      = delim + 1; // //www.fsf.org
   full_url = entry_text;

   config_key = str_concat ("URL_", url_type, NULL);
   if (config_get_string_expanded (config_key, &url_handler))
   {
      chosen_url = url;
      p = strchr (url_handler, '%');
      if (p) { // handler %s
         p++;
         if (*p == 'u') { // handler %u
            *p = 's';    // convert %u to %s (for printf)
            chosen_url = full_url;
         }
         cmd = (char *) malloc (strlen (url_handler) + strlen (chosen_url) + 1);
         sprintf (cmd, url_handler, chosen_url);
      } else {
         cmd = str_concat (url_handler, " ", url, NULL);
      }
      free (url_handler);
   }

   free (config_key);
   free (tmp);
   if (cmd) {
      history_append (cl->hist, entry_text);
      run_the_command (cmd);
      free (cmd);
      return 1;
   }
   return 0;
}


static char * unescape_spaces (const char * text)
{
   char * out = (char *) malloc (strlen (text) + 1);
   char * o = out;

   while (*text) {
      if (*text == '\\' && *(text + 1)) {
         text++;
      }
      *o++ = *text++;
   }
   *o = 0;
   return out;
}


/* Handler for extensions */
static int ext_check (CompLine * cl, char * entry_text)
{
   // example: file.html | xdg-open '%s' -> xdg-open 'file.html'
   char * cmd;
   char * unescaped = unescape_spaces (entry_text); /* unescape chars */
   char * ext = strrchr (entry_text, '.');
   char * handler_format = NULL;

   if (access (unescaped, F_OK) == -1) {
      // entry_text must be a valid filename
      free (unescaped);
      return 0;
   }
   if (ext) {
      handler_format = config_get_handler_for_extension (ext);
   }
   if (handler_format) {
      if (strstr (handler_format, "%s")) {
         cmd = (char *) malloc (strlen (handler_format) + strlen (unescaped) + 1);
         sprintf (cmd, handler_format, unescaped);
      }
      else { // xdg-open
         cmd = str_concat (handler_format, " '", unescaped, "'", NULL);
      }
      history_append (cl->hist, entry_text);
      run_the_command (cmd);
      free (cmd);
      free (unescaped);
      return 1;
   }

   free (unescaped);
   return 0;
}

// =============================================================

static void on_compline_activated (CompLine * cl)
{
   char * entry_text = str_dup (compline_get_text (cl));
   char * stripped = str_strip (entry_text);
   char * cmd;
   char * always_in_term = NULL;
   char * selected_term_prog = NULL;

   if (url_check (cl, stripped) || ext_check (cl, stripped)) {
      free (entry_text);
      return;
   }

   if (config_get_string ("AlwaysInTerm", &always_in_term))
   {
      char * progs = str_dup (always_in_term);
      char * prog = strtok (progs, " ");
      while (prog) {
         if (strcmp (prog, stripped) == 0) {
            selected_term_prog = str_dup (prog);
            break;
         }
         prog = strtok (NULL, " ");
      }
      free (progs);
   }

   if (selected_term_prog) {
      char * term_exec;
      if (config_get_string_expanded ("TermExec", &term_exec)) {
         cmd = str_concat (term_exec, " ", selected_term_prog, NULL);
         free (term_exec);
      } else {
         cmd = str_concat ("xterm -e ", selected_term_prog, NULL);
      }
      free (selected_term_prog);
   } else {
      cmd = str_dup (stripped);
   }
   free (entry_text);

   history_append (cl->hist, cmd);
   run_the_command (cmd);
   free (cmd);
}


static void on_compline_cancel (CompLine * cl)
{
   (void) cl;
   gmrun_exit ();
}

// =============================================================

static void gmrun_activate (void)
{
   Ihandle * hbox, * vbox;
   int shows_last_history_item;
   int tmp;

   compline = compline_new ();
   compline->cb_cancel      = on_compline_cancel;
   compline->cb_activate    = on_compline_activated;
   compline->cb_runwithterm = on_compline_runwithterm;
   compline->cb_unique      = on_compline_unique;
   compline->cb_notunique   = on_compline_notunique;
   compline->cb_incomplete  = on_compline_incomplete;
   compline->cb_search_mode = on_search_mode;
   compline->cb_search_letter = on_search_letter;
   compline->cb_search_not_found = on_search_not_found;
   compline->cb_ext_handler = on_ext_handler;

   if (!config_get_int ("SHELL_RUN", &SHELL_RUN)) {
      SHELL_RUN = 1;
   }
   // don't show files starting with "." by default
   if (!config_get_int ("ShowDotFiles", &compline->show_dot_files)) {
      compline->show_dot_files = 0;
   }
   if (config_get_int ("TabTimeout", &tmp)) {
      compline->tabtimeout = tmp;
   }

   wlabel = IupLabel ("Run:");
   wlabel_search = IupLabel ("");
   IupSetAttribute (wlabel_search, "VISIBLE", "NO");

   hbox = IupHbox (wlabel, wlabel_search, NULL);
   IupSetAttribute (hbox, "ALIGNMENT", "ACENTER");
   IupSetAttribute (hbox, "GAP", "10");

   vbox = IupVbox (hbox, compline->text, NULL);
   IupSetAttribute (vbox, "MARGIN", "4x4");
   IupSetAttribute (vbox, "GAP", "2");

   dialog = IupDialog (vbox);
   IupSetAttribute (dialog, "TITLE", "gmrun");
   IupSetAttribute (dialog, "HIDETITLEBAR", "YES");
   IupSetAttribute (dialog, "RESIZE", "NO");
   IupSetAttribute (dialog, "MAXBOX", "NO");
   IupSetAttribute (dialog, "MINBOX", "NO");
   IupSetAttribute (dialog, "MENUBOX", "NO");
   IupSetAttribute (dialog, "TOPMOST", "YES");
   IupSetAttribute (dialog, "ICON", PACKAGE_DATADIR "/pixmaps/" PACKAGE ".png");
   IupSetCallback (dialog, "CLOSE_CB", (Icallback) gmrun_exit);

   if (!config_get_int ("ShowLast", &shows_last_history_item)) {
      shows_last_history_item = 0;
   }

   // geometry: window size
   if (window_geom.height > 0) {
      IupSetStrf (dialog, "RASTERSIZE", "%dx%d",
                  window_geom.width > -1 ? window_geom.width : 500, window_geom.height);
   } else {
      IupSetStrf (dialog, "RASTERSIZE", "%dx",
                  window_geom.width > -1 ? window_geom.width : 500);
   }

   // geometry: window position
   if (window_geom.x > -1 || window_geom.y > -1) {
      IupShowXY (dialog, window_geom.x, window_geom.y);
   } else {
      IupShowXY (dialog, IUP_CENTER, IUP_CENTER);
   }

   IupSetAttribute (dialog, "RASTERSIZE", NULL);

   if (gmrun_text) {
      compline_set_text (compline, gmrun_text);
   } else if (shows_last_history_item) {
      compline_last_history_item (compline);
   }

   IupSetFocus (compline->text);

   if (gmrun_text) {
      // clear selection if command (text) is supplied as a parameter
      compline_clear_selection (compline);
      free (gmrun_text);
      gmrun_text = NULL;
   }
}

// =============================================================

static void parse_geometry (char * geometry_str)
{
   // --geometry WxH+X+Y
   // width x height + posX + posY
   char *Wstr, *Hstr, *Xstr, *Ystr;
   Wstr = Hstr = Xstr = Ystr = NULL;

   Xstr = strchr (geometry_str, '+');
   if (Xstr) { // +posX+posY
      *Xstr = 0;
      Xstr++; // posX+posY
      Ystr = strchr (Xstr, '+');
      if (Ystr) { // +posY
         *Ystr = 0;
         Ystr++; // posY
      }
   }
   if (Xstr && Ystr && *Xstr && *Ystr) {
      window_geom.x = (int) strtoll (Xstr, NULL, 0);
      window_geom.y = (int) strtoll (Ystr, NULL, 0);
   }

   Hstr = strchr (geometry_str, 'x');
   if (Hstr) { // WxH
      *Hstr = 0;
      Hstr++; // H
      Wstr = geometry_str;
      window_geom.width  = (int) strtoll (Wstr, NULL, 0);
      window_geom.height = (int) strtoll (Hstr, NULL, 0);
   }
}


static void parse_command_line (int argc, char ** argv)
{
   char * geometry_str = NULL;
   char * geomstr;
   char * tmp = NULL;
   const char * value;
   int i, options_done = 0;

   for (i = 1; i < argc; i++)
   {
      if (!options_done && argv[i][0] == '-' && argv[i][1] != 0)
      {
         value = NULL;
         if (strcmp (argv[i], "--") == 0) {
            options_done = 1;
            continue;
         }
         if (strncmp (argv[i], "--geometry=", 11) == 0) {
            value = argv[i] + 11;
         } else if (strncmp (argv[i], "-g", 2) == 0 && argv[i][2] != 0) {
            value = argv[i] + 2;
         } else if (strcmp (argv[i], "--geometry") == 0 || strcmp (argv[i], "-g") == 0) {
            if (i + 1 >= argc) {
               fprintf (stderr, "option parsing failed: Missing argument for %s\n", argv[i]);
               exit (1);
            }
            value = argv[++i];
         }
         if (value) {
            free (geometry_str);
            geometry_str = str_dup (value);
            continue;
         }
         if (strcmp (argv[i], "--version") == 0 || strcmp (argv[i], "-v") == 0) {
#ifdef HAVE_CONFIG_H
            puts (VERSION);
#endif
            exit (0);
         }
         if (strcmp (argv[i], "--help") == 0 || strcmp (argv[i], "-h") == 0
             || strcmp (argv[i], "-?") == 0) {
            printf ("Usage:\n"
                    "  gmrun [OPTION...]\n\n"
                    "Help Options:\n"
                    "  -h, --help                  Show help options\n\n"
                    "Application Options:\n"
                    "  -g, --geometry=WxH+X+Y      This option specifies the initial size and location of the window.\n"
                    "  -v, --version               Show version\n");
            exit (0);
         }
         fprintf (stderr, "option parsing failed: Unknown option %s\n", argv[i]);
         exit (1);
      }
      // all cli arguments are part of the same line
      if (tmp) {
         gmrun_text = str_concat (tmp, " ", argv[i], NULL);
         free (tmp);
         tmp = gmrun_text;
      } else {
         tmp = str_dup (argv[i]);
         gmrun_text = tmp;
      }
   }

   if (!geometry_str)
   {
      // --geometry was not specified, see config file
      if (config_get_string ("Geometry", &geomstr)) {
         geometry_str = str_dup (geomstr);
      }
   }

   if (geometry_str)
   {
      parse_geometry (geometry_str);
      free (geometry_str);
   }
}


// =============================================================
//                           MAIN

void gmrun_exit (void)
{
   if (compline) {
      compline_destroy (compline);
      compline = NULL;
   }
   config_destroy ();
   compl_cleanup ();
   IupExitLoop ();
}


int main (int argc, char ** argv)
{
   IupOpen (&argc, &argv);
   IupSetGlobal ("APPID", PACKAGE);

   config_init ();
   parse_command_line (argc, argv);

   gmrun_activate ();

   IupMainLoop ();
   IupClose ();

   return 0;
}
