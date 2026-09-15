/*
 * This is free and unencumbered software released into the public domain.
 *
 * For more information, please refer to <https://unlicense.org>
 */

#ifndef __UTIL_H
#define __UTIL_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct _StrList
{
   char ** items;
   int count;
   int size;
} StrList;

void strlist_init (StrList * list);
void strlist_add (StrList * list, char * item);
void strlist_add_copy (StrList * list, const char * item);
int  strlist_find (StrList * list, const char * item);
void strlist_sort (StrList * list);
void strlist_clear (StrList * list);

char * str_dup (const char * str);
char * str_concat (const char * first, ...);
char * str_strip (char * str);
char * str_home_dir (void);
char * str_shell_first_word (const char * text, const char ** rest);

#ifdef __cplusplus
}
#endif

#endif
