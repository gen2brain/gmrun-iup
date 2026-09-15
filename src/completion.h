/*
 * This is free and unencumbered software released into the public domain.
 *
 * For more information, please refer to <https://unlicense.org>
 */

#ifndef __COMPLETION_H
#define __COMPLETION_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "util.h"

int compl_split_words (const char * text, int caret, StrList * words);

char * compl_join_words (StrList * words, int pos, int * out_caret);

char * compl_expand_tilde (const char * text);

int compl_generate (const char * word, int show_dot_files, StrList * out);

void compl_cleanup (void);

#ifdef __cplusplus
}
#endif

#endif
