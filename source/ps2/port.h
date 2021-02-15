/* Per-platform settings and headers - gpSP on DSTwo
 *
 * Copyright (C) 2013 GBATemp user Nebuleon
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licens e as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef __GPSP_PORT_H__
#define __GPSP_PORT_H__

#define MAX_PATH 256
#define MAX_FILE 256

typedef int FILE_TAG_TYPE;

typedef struct timespec timespec;

extern int errno;

#include <sys/types.h>
#include <osd_config.h>
#include <debug.h>
#include <gsKit.h>
#include <dmaKit.h>
#include <gsToolkit.h>
#include <libpad.h>
// #include <fileXio_rpc.h>
// #include <fileXio.h>
// #include <fileio.h>
// #include <libps2time.h>

#include "ps2.h"

/* Tuning parameters for the Supercard DSTwo version of gpSP */
/* Its processor is an Ingenic JZ4740 at 360 MHz with 32 MiB of RAM */
#define READONLY_CODE_CACHE_SIZE          (1 * 1024 * 1024)
#define WRITABLE_CODE_CACHE_SIZE          (1 * 1024 * 1024)
/* The following parameter needs to be at least enough bytes to hold
 * the generated code for the largest instruction on your platform.
 * In most cases, that will be the ARM instruction
 * STMDB R0!, {R0,R1,R2,R3,R4,R5,R6,R7,R8,R9,R10,R11,R12,R13,R14,R15} */
#define TRANSLATION_CACHE_LIMIT_THRESHOLD (1024)

#define MAX_AUTO_FRAMESKIP 4

#define FILE_OPEN_APPEND ("a+")

#define FILE_OPEN_READ ("rb")

#define FILE_OPEN_WRITE ("wb")

#define FILE_OPEN(filename_tag, filename, mode)                             \
  filename_tag = fopen(filename, FILE_OPEN_##mode)                          \

#define FILE_CHECK_VALID(filename_tag)                                      \
  (filename_tag != FILE_TAG_INVALID)                                        \

#define FILE_TAG_INVALID                                                    \
  (NULL)                                                                    \

#define FILE_CLOSE(filename_tag)                                            \
  fclose(filename_tag)                                                      \

#define FILE_DELETE(filename)                                               \
  unlink(filename)                                                          \

#define FILE_READ(filename_tag, buffer, size)                               \
  fread(buffer, 1, size, filename_tag)                                      \

#define FILE_WRITE(filename_tag, buffer, size)                              \
  fwrite(buffer, 1, size, filename_tag)                                     \

#define FILE_SEEK(filename_tag, offset, type)                               \
  fseek(filename_tag, offset, type)                                         \

#define FILE_TELL(filename_tag)                                             \
  ftell(filename_tag)                                                       \

#define FILE_GETS(str, size, filename_tag)                                  \
  ps2fgets(str, size, filename_tag)                                         \

#include "ps2input.h"
#include "draw.h"
#include "gui.h"
#include "settings.h"
#include "main.h"
#include "ps2sound.h"
#include "unifont.h"
#include "scalers/scaler.h"

extern uint32_t PerGameBootFromBIOS;
extern uint32_t BootFromBIOS;
extern uint32_t PerGameShowFPS;
extern uint32_t ShowFPS;
extern uint32_t PerGameUserFrameskip;
extern uint32_t UserFrameskip;

extern struct timespec TimeDifference(struct timespec Past, struct timespec Present);
extern void GetFileNameNoExtension(char* Result, const char* Path);

#endif
