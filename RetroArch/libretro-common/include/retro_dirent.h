/* Copyright  (C) 2010-2016 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (retro_dirent.h).
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

#ifndef __RETRO_DIRENT_H
#define __RETRO_DIRENT_H

#include <retro_common_api.h>

#include <boolean.h>

#if defined(_WIN32)
#  ifdef _MSC_VER
#    define setmode _setmode
#  endif
#  ifdef _XBOX
#    include <xtl.h>
#    define INVALID_FILE_ATTRIBUTES -1
#  else
#    include <io.h>
#    include <fcntl.h>
#    include <direct.h>
#    include <windows.h>
#  endif
#elif defined(VITA)
#  include <psp2/io/fcntl.h>
#  include <psp2/io/dirent.h>
#else
#  if defined(PSP)
#    include <pspiofilemgr.h>
#  endif
#  include <sys/types.h>
#  include <sys/stat.h>
#  include <dirent.h>
#  include <unistd.h>
#endif

#ifdef __CELLOS_LV2__
#include <cell/cell_fs.h>
#endif

RETRO_BEGIN_DECLS

struct RDIR
{
#if defined(_WIN32)
   WIN32_FIND_DATA entry;
   HANDLE directory;
   bool next;
#elif defined(VITA) || defined(PSP)
   SceUID directory;
   SceIoDirent entry;
#elif defined(__CELLOS_LV2__)
   CellFsErrno error;
   int directory;
   CellFsDirent entry;
#else
   DIR *directory;
   const struct dirent *entry;
#endif
};

struct RDIR *retro_opendir(const char *name);

int retro_readdir(struct RDIR *rdir);

bool retro_dirent_error(struct RDIR *rdir);

const char *retro_dirent_get_name(struct RDIR *rdir);

/**
 *
 * retro_dirent_is_dir:
 * @rdir         : pointer to the directory entry.
 *
 * Is the directory listing entry a directory?
 *
 * Returns: true if directory listing entry is
 * a directory, false if not.
 */
bool retro_dirent_is_dir(struct RDIR *rdir, const char *path);

void retro_closedir(struct RDIR *rdir);

RETRO_END_DECLS

#endif
