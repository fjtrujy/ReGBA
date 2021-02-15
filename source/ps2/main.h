/* gameplaySP
 *
 * Copyright (C) 2006 Exophase <exophase@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
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

#ifndef MAIN_H
#define MAIN_H

#include "cpu.h"
#include "memory.h"

typedef enum
{
  auto_frameskip,
  manual_frameskip,
  no_frameskip
} frameskip_type;

extern uint32_t cpu_ticks_;
extern uint32_t frame_ticks;
extern uint32_t execute_cycles;
extern frameskip_type current_frameskip_type;
extern uint32_t frameskip_value;
extern uint32_t random_skip;
extern uint32_t global_cycles_per_instruction;
extern uint32_t synchronize_flag;
extern uint32_t skip_next_frame;

extern uint64_t base_timestamp;

extern uint32_t clock_speed;

uint32_t update_gba();
void reset_gba();
void synchronize();
void quit();
void error_quit();
void change_ext(const char *src, char *buffer, char *extension);
void main_write_mem_savestate();
void main_read_mem_savestate();

#define count_timer(timer_number)                                             \
  timer[timer_number].reload = 0x10000 - value;                               \
  if(timer_number < 2)                                                        \
  {                                                                           \
    uint32_t timer_reload =                                                   \
     timer[timer_number].reload << timer[timer_number].prescale;              \
    sound_update_frequency_step(timer_number);                                \
  }                                                                           \

#define adjust_sound_buffer(timer_number, channel)                            \
  if(timer[timer_number].direct_sound_channels & (0x01 << channel))           \
  {                                                                           \
    direct_sound_channel[channel].buffer_index =                              \
     (direct_sound_channel[channel].buffer_index + buffer_adjust) %           \
     BUFFER_SIZE;                                                             \
  }                                                                           \

#endif


