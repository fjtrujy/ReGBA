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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "common.h"
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>

extern u8 gba_bios_bin[];
extern int size_gba_bios_bin;

int32_t load_bios_mem(u8* data, int size);

extern TIMER_TYPE timer[4];

frameskip_type current_frameskip_type = auto_frameskip;
uint32_t frameskip_value = 4;
uint32_t random_skip = 0;
uint32_t global_cycles_per_instruction = 3;
uint32_t skip_next_frame = 0;

uint32_t frameskip_counter = 0;

uint32_t cpu_ticks = 0;
uint32_t frame_ticks = 0;

uint32_t execute_cycles = 960;
int32_t video_count = 960;
uint32_t ticks;

uint32_t arm_frame = 0;
uint32_t thumb_frame = 0;
uint32_t last_frame = 0;

uint32_t synchronize_flag = 1;

#define check_count(count_var)																\
	if(count_var < execute_cycles)															\
		execute_cycles = count_var;															\

#define check_timer(timer_number)															\
	if(timer[timer_number].status == TIMER_PRESCALE)										\
		check_count(timer[timer_number].count);												\

#define update_timer(timer_number)															\
	if(timer[timer_number].status != TIMER_INACTIVE)										\
	{																						\
		if(timer[timer_number].status != TIMER_CASCADE)										\
		{																					\
			timer[timer_number].count -= execute_cycles;									\
			io_registers[REG_TM##timer_number##D] =											\
			 -(timer[timer_number].count >> timer[timer_number].prescale);					\
		}																					\
																							\
		if(timer[timer_number].count <= 0)													\
		{																					\
			if(timer[timer_number].irq == TIMER_TRIGGER_IRQ)								\
				irq_raised |= IRQ_TIMER##timer_number;										\
																							\
			if((timer_number != 3) &&														\
			 (timer[timer_number + 1].status == TIMER_CASCADE))								\
			{																				\
				timer[timer_number + 1].count--;											\
				io_registers[REG_TM0D + (timer_number + 1) * 2] =							\
				 -(timer[timer_number + 1].count);											\
			}																				\
																							\
			if(timer_number < 2)															\
			{																				\
				if(timer[timer_number].direct_sound_channels & 0x01)						\
					sound_timer(timer[timer_number].frequency_step, 0);						\
																							\
				if(timer[timer_number].direct_sound_channels & 0x02)						\
					sound_timer(timer[timer_number].frequency_step, 1);						\
			}																				\
																							\
			timer[timer_number].count +=													\
			 (timer[timer_number].reload << timer[timer_number].prescale);					\
		}																					\
	}																						\

char *file_ext[] = { ".gba", ".bin", ".zip", NULL };

static bool caches_inited = false;

void init_main()
{
	uint32_t i;

	skip_next_frame = 0;

	for(i = 0; i < 4; i++)
	{
		dma[i].start_type = DMA_INACTIVE;
		dma[i].direct_sound_channel = DMA_NO_DIRECT_SOUND;
		timer[i].status = TIMER_INACTIVE;
		timer[i].reload = 0x10000;
		timer[i].stop_cpu_ticks = 0;
	}

	timer[0].direct_sound_channels = TIMER_DS_CHANNEL_BOTH;
	timer[1].direct_sound_channels = TIMER_DS_CHANNEL_NONE;

	cpu_ticks = 0;
	frame_ticks = 0;

	execute_cycles = 960;
	video_count = 960;

	if (!caches_inited)
	{
		flush_translation_cache(TRANSLATION_REGION_READONLY, FLUSH_REASON_INITIALIZING);
		flush_translation_cache(TRANSLATION_REGION_WRITABLE, FLUSH_REASON_INITIALIZING);
	}
	else
	{
		flush_translation_cache(TRANSLATION_REGION_READONLY, FLUSH_REASON_LOADING_ROM);
		clear_metadata_area(METADATA_AREA_EWRAM, CLEAR_REASON_LOADING_ROM);
		clear_metadata_area(METADATA_AREA_IWRAM, CLEAR_REASON_LOADING_ROM);
		clear_metadata_area(METADATA_AREA_VRAM, CLEAR_REASON_LOADING_ROM);
	}

	caches_inited = true;

	StatsInitGame();
}

int main(int argc, char *argv[])
{
	char load_filename[MAX_PATH];
	char file[MAX_PATH];

	ps2init();
	init_video();
	init_input();
	init_audio();

	getcwd(main_path, MAX_PATH);

	ReGBA_LoadSettings("global_config", false);
	reload_video();

	ReGBA_ProgressInitialise(FILE_ACTION_LOAD_BIOS);
	sprintf(file, "%s/gba_bios.bin", main_path);
	ReGBA_ProgressUpdate(1, 2);
	
	if(load_bios(file) == -1)
	{
		printf("No GBA BIOS was found.\n");

		ReGBA_ProgressUpdate(2, 2);
		ReGBA_ProgressFinalise();

		/*ShowErrorScreen("The GBA BIOS was not found in location: "
			"\n%s\n The file needs "
			"to be named gba_bios.bin.", main_path);

		error_quit();*/
		
		load_bios_mem(gba_bios_bin, size_gba_bios_bin);
	}
	else
	{
		ReGBA_ProgressUpdate(2, 2);
		ReGBA_ProgressFinalise();
	}

	init_main();
	init_sound();

#ifdef HOST
	argc = 2;
	// argv[1] = "host:Dragon Ball Z - Buu's Fury.zip";
	argv[1] = "host:Bomb Jack.gba";
#endif

	if(argc > 1)
	{
		if(load_gamepak(argv[1]) == -1)
		{
			if(errno != 0)
				ShowErrorScreen("Loading ROM failed: %s", strerror(errno));
			else
				ShowErrorScreen("Loading ROM failed: File format invalid");
			
			//error_quit();
			goto loadrom;
		}

		if (IsGameLoaded)
		{
			char FileNameNoExt[MAX_PATH + 1];
			GetFileNameNoExtension(FileNameNoExt, CurrentGamePath);
#ifdef CHEATS
			add_cheats(FileNameNoExt);
#endif
			ReGBA_LoadSettings(FileNameNoExt, true);
		}

		init_cpu(ResolveSetting(BootFromBIOS, PerGameBootFromBIOS) /* in port.c */);
	}
	else
	{
		printf("No ROM was specified was on the command line.\n");
			
loadrom:
	SetMenuResolution();
	
		if(load_file(file_ext, load_filename) == -1)
		{
			if(ReGBA_Menu(REGBA_MENU_ENTRY_REASON_NO_ROM))
				goto loadrom;
		}
		else
		{
			if(load_gamepak(load_filename) == -1)
			{
				ShowErrorScreen("Failed to load gamepak %s, exiting.\n", load_filename);
				ps2delay(5);
				goto loadrom;
			}
			
			if (IsGameLoaded)
			{
				char FileNameNoExt[MAX_PATH + 1];
				GetFileNameNoExtension(FileNameNoExt, CurrentGamePath);
#ifdef CHEATS
				add_cheats(FileNameNoExt);
#endif
				ReGBA_LoadSettings(FileNameNoExt, true);
			}

			init_cpu(ResolveSetting(BootFromBIOS, PerGameBootFromBIOS) /* in port.c */);
		}
	}

	// We'll never actually return from here.
	SetGameResolution();

	execute_arm_translate(execute_cycles);
	return 0;
}

uint32_t update_gba()
{
	IRQ_TYPE irq_raised = IRQ_NONE;
	do
	{
		cpu_ticks += execute_cycles;
		reg[CHANGED_PC_STATUS] = 0;


		if(gbc_sound_update)
		{
			update_gbc_sound(cpu_ticks);
			gbc_sound_update = 0;
		}

		update_timer(0);
		update_timer(1);
		update_timer(2);
		update_timer(3);

		video_count -= execute_cycles;


		if(video_count <= 0)
		{
			uint32_t vcount = io_registers[REG_VCOUNT];
			uint32_t dispstat = io_registers[REG_DISPSTAT];

			if((dispstat & 0x02) == 0)
			{
				// Transition from hrefresh to hblank
				video_count += (272);
				dispstat |= 0x02;

				if((dispstat & 0x01) == 0)
				{
					uint32_t i;

					update_scanline();

					// If in visible area also fire HDMA
					for(i = 0; i < 4; i++)
					{
						if(dma[i].start_type == DMA_START_HBLANK)
							dma_transfer(dma + i);
					}
				}

				if(dispstat & 0x10)
					irq_raised |= IRQ_HBLANK;
			}
			else
			{
				// Transition from hblank to next line
				video_count += 960;
				dispstat &= ~0x02;

				vcount++;

				if(vcount == 160)
				{
					// Transition from vrefresh to vblank
					uint32_t i;

					dispstat |= 0x01;
					if(dispstat & 0x8)
					{
						irq_raised |= IRQ_VBLANK;
					}

					affine_reference_x[0] =
					 (int32_t)(ADDRESS32(io_registers, 0x28) << 4) >> 4;
					affine_reference_y[0] =
					 (int32_t)(ADDRESS32(io_registers, 0x2C) << 4) >> 4;
					affine_reference_x[1] =
					 (int32_t)(ADDRESS32(io_registers, 0x38) << 4) >> 4;
					affine_reference_y[1] =
					 (int32_t)(ADDRESS32(io_registers, 0x3C) << 4) >> 4;
			 
					for(i = 0; i < 4; i++)
					{
						if(dma[i].start_type == DMA_START_VBLANK)
							dma_transfer(dma + i);
					}
			
				}
				else

				if(vcount == 228)
				{
					// Transition from vblank to next screen
					dispstat &= ~0x01;
					frame_ticks++;

					if(update_input())
						continue;

					update_gbc_sound(cpu_ticks);

					Stats.EmulatedFrames++;
					Stats.TotalEmulatedFrames++;

					ReGBA_RenderScreen();
					update_backup();
#ifdef CHEATS
					process_cheats();
#endif

					vcount = 0;
				}

				if(vcount == (dispstat >> 8))
				{
					// vcount trigger
					dispstat |= 0x04;
					if(dispstat & 0x20)
					{
						irq_raised |= IRQ_VCOUNT;
					}
				}
				else
				{
					dispstat &= ~0x04;
				}

				io_registers[REG_VCOUNT] = vcount;
			}
			io_registers[REG_DISPSTAT] = dispstat;
		}

		if(irq_raised)
			raise_interrupt(irq_raised);

		execute_cycles = video_count;

		check_timer(0);
		check_timer(1);
		check_timer(2);
		check_timer(3);
	} while(reg[CPU_HALT_STATE] != CPU_ACTIVE);

	return execute_cycles;
}

static void quit_common()
{
	if(IsGameLoaded)
		update_backup_force();

	ps2quit();
}

void quit()
{
	quit_common();
	exit(0);
}

void error_quit()
{
	quit_common();
	exit(1);
}

void reset_gba()
{
	init_main();
	init_memory();
	init_cpu(ResolveSetting(BootFromBIOS, PerGameBootFromBIOS) /* in port.c */);
	reset_sound();
}

size_t FILE_LENGTH(FILE_TAG_TYPE fp)
{
	size_t length;
	int pos;
	
	//should save file pos
	length = FILE_SEEK(fp, 0, SEEK_END);
	pos = FILE_SEEK(fp, 0, SEEK_SET);
	
	return length;
}

void change_ext(const char *src, char *buffer, char *extension)
{
	char *position;

	strcpy(buffer, main_path);
	strcat(buffer, "/");

	position = strrchr(src, '/');
	if (position)
	src = position+1;

	strcat(buffer, src);
	position = strrchr(buffer, '.');

	if(position)
		strcpy(position, extension);
}

// type = READ / WRITE_MEM
#define MAIN_SAVESTATE_BODY(type)															\
{																							\
	FILE_##type##_VARIABLE(g_state_buffer_ptr, cpu_ticks);									\
	FILE_##type##_VARIABLE(g_state_buffer_ptr, execute_cycles);								\
	FILE_##type##_VARIABLE(g_state_buffer_ptr, video_count);								\
	FILE_##type##_ARRAY(g_state_buffer_ptr, timer);											\
}																							\

void main_read_mem_savestate()
MAIN_SAVESTATE_BODY(READ_MEM);

void main_write_mem_savestate()
MAIN_SAVESTATE_BODY(WRITE_MEM);
