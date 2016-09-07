/* Copyright  (C) 2010-2016 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (dir_list.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __LIBRETRO_SDK_DIR_LIST_H
#define __LIBRETRO_SDK_DIR_LIST_H

#include <retro_common_api.h>

#include <lists/string_list.h>

RETRO_BEGIN_DECLS

/**
 * dir_list_new:
 * @dir                : directory path.
 * @ext                : allowed extensions of file directory entries to include.
 * @include_dirs       : include directories as part of the finished directory listing?
 * @include_hidden     : include hidden files and directories as part of the finished directory listing?
 * @include_compressed : include compressed files, even when not part of ext.
 * @recursive          : list directory contents recursively
 *
 * Create a directory listing.
 *
 * Returns: pointer to a directory listing of type 'struct string_list *' on success,
 * NULL in case of error. Has to be freed manually.
 **/
struct string_list *dir_list_new(const char *dir, const char *ext,
      bool include_dirs, bool include_hidden, bool include_compressed, bool recursive);

/**
 * dir_list_sort:
 * @list      : pointer to the directory listing.
 * @dir_first : move the directories in the listing to the top?
 *
 * Sorts a directory listing.
 *
 **/
void dir_list_sort(struct string_list *list, bool dir_first);

/**
 * dir_list_free:
 * @list : pointer to the directory listing
 *
 * Frees a directory listing.
 *
 **/
void dir_list_free(struct string_list *list);

/**
 * dir_list_read:
 * @dir                : directory path.
 * @list               : the string list to add files to
 * @ext_list           : the string list of extensions to include
 * @include_dirs       : include directories as part of the finished directory listing?
 * @include_hidden     : include hidden files and directories as part of the finished directory listing?
 * @include_compressed : Only include files which match ext. Do not try to match compressed files, etc.
 * @recursive          : list directory contents recursively
 *
 * Add files within a directory to an existing string list
 *
 * Returns: -1 on error, 0 on success.
 **/
int dir_list_read(const char *dir, struct string_list *list, struct string_list *ext_list,
      bool include_dirs, bool include_hidden, bool include_compressed, bool recursive);

RETRO_END_DECLS

#endif
