
#include "common.h"

#include <sbv_patches.h>
#include <loadfile.h>
#include <stdlib.h>
#include <iopcontrol.h>
#include <iopheap.h>
#include <libhdd.h>
#include <libpwroff.h>
#include <sifrpc.h>
#include <sys/fcntl.h>

#define DPRINTF(x...) printf(x)
//#define DPRINTF(x...)
//#define DEBUG

static char padBuf_t[2][256] __attribute__((aligned(64)));

extern u8 iomanX_irx[];
extern int size_iomanX_irx;

extern u8 usbhdfsd_irx[];
extern int size_usbhdfsd_irx;

extern u8 usbd_irx[];
extern int size_usbd_irx;

extern u8 freesd_irx[];
extern int size_freesd_irx;

extern u8 audsrv_irx[];
extern int size_audsrv_irx;

extern u8 fileXio_irx[];
extern int size_fileXio_irx;

extern u8 ps2atad_irx[];
extern int size_ps2atad_irx;

extern u8 ps2fs_irx[];
extern int size_ps2fs_irx;

extern u8 ps2hdd_irx[];
extern int size_ps2hdd_irx;

extern u8 ps2dev9_irx[];
extern int size_ps2dev9_irx;

extern u8 poweroff_irx[];
extern int size_poweroff_irx;

// extern u8 smscdvd_irx[];
// extern int size_smscdvd_irx;

static int initdirs();
// static int get_part_list();
// static void load_hddmodules();
// static int hddinit();
// static int mountParty(char *party);
// static void unmountAllParts();
// static void unmountParty(int i);
// static int fix_hddpath(char *name);

static void load_modules()
{
	SifLoadModule("rom0:SIO2MAN", 0, NULL);
	SifLoadModule("rom0:MCMAN", 0, NULL);
	SifLoadModule("rom0:MCSERV", 0, NULL);
	SifLoadModule("rom0:PADMAN", 0, NULL);
	
	SifExecModuleBuffer(iomanX_irx, size_iomanX_irx, 0, NULL, NULL);
	SifExecModuleBuffer(fileXio_irx, size_fileXio_irx, 0, NULL, NULL);
	// SifExecModuleBuffer(smscdvd_irx, size_smscdvd_irx, 0, NULL, NULL);
	SifExecModuleBuffer(usbd_irx, size_usbd_irx, 0, NULL, NULL);
	SifExecModuleBuffer(usbhdfsd_irx, size_usbhdfsd_irx, 0, NULL, NULL);      
	SifExecModuleBuffer(freesd_irx, size_freesd_irx, 0, NULL, NULL);
	SifExecModuleBuffer(audsrv_irx, size_audsrv_irx, 0, NULL, NULL);
}

void ps2delay(int count) 
{
   int i, ret;
   for (i = 0; i < count; i++) 
   {
      ret = 0x01000000;
      while ( ret-- ) 
      {
         asm("nop\nnop\nnop\nnop");
      }
   }
}
int ps2quit()
{
    // unmountAllParts();
	
	Exit(0);
    return 1;
}

int ps2init()
{
   SifInitRpc(0);
#ifndef DEBUG
   while(!SifIopReset(NULL, 0)){};
#endif
   while(!SifIopSync()){};

	fioExit();
	SifExitIopHeap();
	SifLoadFileExit();
	SifExitRpc();
	SifExitCmd();

	SifInitRpc(0);
	FlushCache(0);
	FlushCache(2);
   sbv_patch_enable_lmb();          
   //sbv_patch_disable_prefix_check();
   
   load_modules();

   ps2delay(5);
// #ifndef HOST 
//    fileXioInit();
// #endif

   //init_handles();
   initdirs();
//    ps2time_init();
   ps2delay(2);
   DPRINTF("-- PS2 inited --\n");
   return 1;
}

void WaitPadReady(int port, int slot)
{
	int state, lastState;
	char stateString[16];

	state = padGetState(port, slot);
	lastState = -1;
	while((state != PAD_STATE_DISCONN)
		&& (state != PAD_STATE_STABLE)
		&& (state != PAD_STATE_FINDCTP1)){
		if (state != lastState)
			padStateInt2String(state, stateString);
		lastState = state;
		state=padGetState(port, slot);
	}
}
void Wait_Pad_Ready(void)
{
	int state_1, state_2;

	state_1 = padGetState(0, 0);
	state_2 = padGetState(1, 0);
	while((state_1 != PAD_STATE_DISCONN) && (state_2 != PAD_STATE_DISCONN)
		&& (state_1 != PAD_STATE_STABLE) && (state_2 != PAD_STATE_STABLE)
		&& (state_1 != PAD_STATE_FINDCTP1) && (state_2 != PAD_STATE_FINDCTP1)){
		state_1 = padGetState(0, 0);
		state_2 = padGetState(1, 0);
	}
}
int Setup_Pad(void)
{
	int ret, i, port, state, modes;

	padInit(0);

	for(port=0; port<2; port++){
		if((ret = padPortOpen(port, 0, &padBuf_t[port][0])) == 0)
			return 0;
		WaitPadReady(port, 0);
		state = padGetState(port, 0);
		if(state != PAD_STATE_DISCONN){
			modes = padInfoMode(port, 0, PAD_MODETABLE, -1);
			if (modes != 0){
				i = 0;
				do{
					if (padInfoMode(port, 0, PAD_MODETABLE, i) == PAD_TYPE_DUALSHOCK){
						padSetMainMode(port, 0, PAD_MMODE_DUALSHOCK, PAD_MMODE_LOCK);
						break;
					}
					i++;
				} while (i < modes);
			}
		}
	}
	return 1;
}

// int fileXio_mode =  FIO_S_IRUSR | FIO_S_IWUSR | FIO_S_IXUSR | FIO_S_IRGRP | FIO_S_IWGRP | FIO_S_IXGRP | FIO_S_IROTH | FIO_S_IWOTH | FIO_S_IXOTH;

#define MOUNT_LIMIT 4
#define MAX_PARTITIONS 20
#define MAX_NAME 256

static char partlist[MAX_PARTITIONS][MAX_NAME];
static char mountedParty[MOUNT_LIMIT][MAX_NAME];
static int allparts = 0;
static int alwaysMounted = 0;
static int latestMount = -1;
static char tmp[MAX_NAME];

static int hddinited = 0;
static char mainPath[MAX_NAME];

// PS2DIR * ps2Opendir(char *path)
// {
//     int fd = -1;
// 	PS2DIR *ptr;
	
// 	DPRINTF("opendir %s\n", path);
    
//     if(!strncmp(path, "MAIN", 4) || !strcmp(path, "hdd0:/"))
//     goto end;
	
// 	fd = ps2Dopen(path);
	
// 	DPRINTF("fd %d\n", fd);
		
// 	if(fd < 0)
// 		return NULL;
	
// 	end:
// 	ptr = (PS2DIR *)malloc(sizeof(PS2DIR));
// 	ptr->d_entry = NULL;
	
// 	if(ptr == NULL)
// 		return NULL;
	
// 	ptr->d_fd = fd;
// 	strcpy(ptr->d_name, path);
	
// 	return ptr;
// }

// int ps2Closedir(PS2DIR *d)
// {
// 	int ret = 1;
	
// 	if(d != NULL)
// 	{
// 		DPRINTF("ps2dclose %d \n", d->d_fd);
		
// 		if(d->d_fd > 0)
// 			ret = ps2Dclose(d->d_fd);
		
// 		free(d->d_entry);
// 		free(d);
// 	}
	
// 	return ret;
// }

static int dir_ctr;
static int part_ctr;

// struct ps2dirent *ps2Readdir(PS2DIR *d)
// {   
//     int ret;
	
// 	DPRINTF("ps2readdir %s \n", d->d_name);
	
// 	if(d->d_entry == NULL)
// 	{
// 		d->d_entry = (struct ps2dirent *)malloc(sizeof(struct ps2dirent));
// 	}

// 	if(!strncmp(d->d_name, "MAIN", 4))
//     {
//         if(dir_ctr > 4)
//         {
//             dir_ctr = 0;
//             return NULL;      
//         }
                                      
//         switch(dir_ctr)
//         {
//               case 0:
//                    sprintf(d->d_entry->d_name, "mc0:/"); break;
//               case 1:
//                    sprintf(d->d_entry->d_name, "mc1:/"); break;
//               case 2:
//                    sprintf(d->d_entry->d_name, "mass:/"); break;
//               case 3:
//                    sprintf(d->d_entry->d_name, "cdfs:/"); break;
//               case 4:
//                    sprintf(d->d_entry->d_name, "hdd0:/"); break;
//               default:
//                       break;     
//         }  
// 		d->d_entry->d_type = DT_DIR;
		
// 		dir_ctr++;                     
//     }
//     else if(!strncmp(d->d_name, "hdd", 3))
//     {
//     	if(part_ctr < 0)
//     	{
//     		part_ctr = allparts;
//       		return NULL;
// 		}
// 		strcpy(d->d_entry->d_name, partlist[part_ctr]);
//     	d->d_entry->d_type = DT_DIR;
    	
//     	part_ctr--;
//     }
// 	else
// 	{
// 		ret = readdir(d->d_fd, d->d_entry);

// 		if(ret <= 0)
// 		{
// 			return NULL;
// 		}
// 	}
	
// 	return d->d_entry;
// }

int ps2Chdir(char *path)
{
    char *p;
	
    DPRINTF("chdir %s mainPath %s\n", path, mainPath);
    
    if((path == NULL) || !strncmp(path, "MAIN", 4))
    {
		strcpy(mainPath, "MAIN");
		return 1;
    }
    
    if(!strcmp(path, ".."))
    {
		if((p=strrchr(mainPath, '/'))!=NULL)
		{
			if(*(p-1) == ':')
			{
				if(*(p+1) != 0)
					*(p+1) = 0;
				else if(!strcmp(mainPath, "pfs0:/"))
					strcpy(mainPath, "hdd0:/");
				else
					strcpy(mainPath, "MAIN");
			}
			else
			{
				*p = 0;
			}
		}
		else
			strcpy(mainPath, "MAIN");
	}
    else
    {
    	if(!strcmp(mainPath, "MAIN"))
    	{
    		// if(!strcmp(path, "hdd0:/"))
			// {
			// 	if(!hddinited)
        	// 	{
			// 		load_hddmodules();
			// 		hddinit();
			// 		get_part_list();       
			// 		hddinited = 1;            
        	// 	}
    		// }
    		
    		// strcpy(mainPath, path);
		}
		else
		{
			if(!strcmp(mainPath, "hdd0:/"))
			{
        		// sprintf(mainPath, "hdd0:%s", path);
        		
				// if(!fix_hddpath(mainPath))
				// 	strcpy(mainPath, "MAIN");
			}
			else
			{
				p=strrchr(mainPath, '/');
			
				if(*(p - 1) == ':' && *(p + 1) == 0)
					sprintf(mainPath, "%s%s", mainPath, path);
				else
					sprintf(mainPath, "%s/%s", mainPath, path);
			}
		}	
	}

    return 1;
}

// int ps2Getcwd(char *current_dir_name, int max_path)
// { 
//     strcpy(current_dir_name, mainPath);
    
//     return 1;
// }

// static int fix_hddpath(char *name)
// {
//      char *p;
//      int i;
	 
// 	 memset(tmp, 0, MAX_NAME);
	 
//      if((p = strchr(name, '/')) != NULL) //partition name with directory
//      {
// 		  for(i = 0; i < MAX_PARTITIONS; i++)
//           {
//               if(!strncmp((name + 5), partlist[i], strlen(partlist[i]) - 1)) //is partition on the list
//               {   
// 					i = mountParty(partlist[i]);
					
// 					if(i == -1)
// 					return 0;
					
// 					sprintf(tmp, "pfs%d:/%s", i, p+1);
// 					strcpy(name, tmp);
					
// 					DPRINTF("%s\n", name);
					
// 					return 1;
//               }   
//           }             
// 	 }
// 	 else //only partition name
// 	 {
// 		i = mountParty(name + 5);
		
// 		DPRINTF("%s i = %d\n", name, i);
					
// 		if(i == -1)
// 		return 0;
					
// 		sprintf(tmp, "pfs%d:/", i);
// 		strcpy(name, tmp);
		
// 		return 1;
// 	 }
	 
// 	 return 0;
// }

static int initdirs()
{
   memset((void *)mainPath, 0, MAX_NAME);
   memset((void *)partlist, 0, MAX_PARTITIONS * MAX_NAME);
   memset((void *)mountedParty, 0, MOUNT_LIMIT * MAX_NAME);
   
   strcpy(mainPath, "MAIN");
   
   return 1;
}

// static int get_part_list()
// {
//    iox_dirent_t dirEnt;
//    int fd;     
   
//    fd = fileXioDopen("hdd0:");
   
//    part_ctr = -1;
               
//     while(fileXioDread(fd, &dirEnt) > 0 )
//     {             
//         if(part_ctr >= MAX_PARTITIONS)
// 		{
//            part_ctr = MAX_PARTITIONS;
//            break;         
//         }
		
//         if((dirEnt.stat.attr != ATTR_MAIN_PARTITION) 
// 				|| (dirEnt.stat.mode != FS_TYPE_PFS))
// 			continue;

// 		//Patch this to see if new CB versions use valid PFS format
// 		//NB: All CodeBreaker versions up to v9.3 use invalid formats
// 		if(!strncmp(dirEnt.name, "PP.",3))
//         {
// 			int len = strlen(dirEnt.name);
// 			if(!strcmp(dirEnt.name+len-4, ".PCB"))
// 				continue;
// 		}

// 		if(!strncmp(dirEnt.name, "__", 2) &&
// 			strcmp(dirEnt.name, "__boot") &&
// 			strcmp(dirEnt.name, "__net") &&
// 			strcmp(dirEnt.name, "__system") &&
// 			strcmp(dirEnt.name, "__sysconf") &&
// 			strcmp(dirEnt.name, "__common"))
// 			continue;
		
// 		part_ctr++;
// 		allparts = part_ctr;
// 		strcpy(partlist[part_ctr], dirEnt.name);
//    }
   
//    fileXioDclose(fd);
   
//    return 1;
// }

// static void load_hddmodules()
// {
//    	// set the arguments for loading 'ps2fs'
// 	// -m 4  (maxmounts 4)
// 	// -o 10 (maxopen 10)
// 	// -n 40 (number of buffers 40)
//    static char pfsarg[] = "-m" "\0" "4" "\0" "-o" "\0" "10" "\0" "-n" "\0" "40";
//     // set the arguments for loading 'ps2hdd'
// 	// -o 4 (maxopen 4)
// 	// -n 20 (cachesize 20) 
//    static char hddarg[] = "-o" "\0" "4" "\0" "-n" "\0" "20";
 
// #ifndef DEBUG
//     SifExecModuleBuffer(poweroff_irx, size_poweroff_irx, 0, NULL, NULL);
//     SifExecModuleBuffer(ps2dev9_irx, size_ps2dev9_irx, 0, NULL, NULL);
// #endif
//     SifExecModuleBuffer(ps2atad_irx, size_ps2atad_irx, 0, NULL, NULL);
// 	SifExecModuleBuffer(ps2hdd_irx, size_ps2hdd_irx, sizeof(hddarg), hddarg, NULL);
// 	SifExecModuleBuffer(ps2fs_irx, size_ps2fs_irx, sizeof(pfsarg), pfsarg, NULL);
// }

// static int hddinit()
// {
// 	if(hddCheckPresent() < 0)
// 	{
// 		DPRINTF("NO HDD FOUND!\n");
//     	return -1;
//     }
// 	else
// 	{
// 		DPRINTF("Found HDD!\n");
// 	}

// 	if(hddCheckFormatted() < 0)
//     {
// 		DPRINTF("HDD Not Formatted!\n");
// 		return -1;	
// 	}
//     else
//     {
//     	DPRINTF("HDD Is Formatted!\n");
// 	}

    
//     return 1;
// }

// static int mountParty(char *party)
// {
// 	int i, j;
// 	char pfs_str[6];
	
// 	for(i = 0; i < MOUNT_LIMIT; i++) //check already mounted PFS indexes
// 	{
// 		if(!strcmp(party, mountedParty[i]))
// 			goto return_i;
// 	}

// 	for(i = 0, j = -1; i < MOUNT_LIMIT; i++) //search for a free PFS index
// 	{ 
// 		if(mountedParty[i][0] == 0)
// 		{
// 			j = i;
// 			break;
// 		}
// 	}

// 	if(j == -1) //search for a suitable PFS index to unmount
// 	{
// 		for(i = 0; i < MOUNT_LIMIT; i++)
// 		{
// 			if((i != latestMount) && (i != alwaysMounted))
// 			{
// 				j = i;
// 				break;
// 			}
// 		}
// 		unmountParty(j);
// 	}

// 	i = j;
// 	strcpy(pfs_str, "pfs0:");

// 	pfs_str[3] = '0' + i;
	
// 	memset(tmp, 0, MAX_NAME);
	
// 	sprintf(tmp, "hdd0:%s", party);
	
// 	DPRINTF("Mount %s as %s\n", tmp, pfs_str);
	
// 	if(fileXioMount(pfs_str, tmp, FIO_MT_RDWR) < 0)
// 	{
// 		DPRINTF("Mount failed 1\n");
// 		for(i = 0; i <= MOUNT_LIMIT; i++)
// 		{
// 			if((i != latestMount) && (i != alwaysMounted))
// 			{
// 				DPRINTF("Mount try again %d\n", i);
// 				unmountParty(i);
// 				pfs_str[3] = '0' + i;
// 				if(fileXioMount(pfs_str, tmp, FIO_MT_RDWR) >= 0)
// 					break;
// 			}
// 		}
// 		DPRINTF("Mount failed 2\n");
// 		if(i > MOUNT_LIMIT)
// 			return -1;
// 	}
// 	strcpy(mountedParty[i], party);
// return_i:
// 	if(i != alwaysMounted)
// 	latestMount = i;
// 	return i;
// }

// static void unmountParty(int party_ix)
// {
// 	char pfs_str[6];

// 	strcpy(pfs_str, "pfs0:");
// 	pfs_str[3] += party_ix;
	
// 	DPRINTF("Umount party %s\n", pfs_str);
	
// 	if(fileXioUmount(pfs_str) < 0)
// 		return;
		
// 	if(party_ix < MOUNT_LIMIT)
// 		mountedParty[party_ix][0] = 0;

// 	if(latestMount == party_ix)
// 		latestMount = -1;
// }

// static void unmountAllParts()
// {
//     int i;
	
//     for(i = 0; i < MOUNT_LIMIT; i++)
// 		unmountParty(i);
    
//     fileXioStop();
// }

int check_dir(char *path, int is_main)
{
	int i, fd;
	char *p;
	
	memset(tmp, 0, MAX_NAME);
	//strcpy(tmp, path);
	
	// if(!strncmp(path, "hdd0:", 5))
    // {
    	
    //     if(!hddinited)
    //     {
    //        load_hddmodules();
    //        hddinit();
    //        get_part_list();       
    //        hddinited = 1;            
    //     }
		
	// 	if(path[5] == '/')
	// 		sprintf(path, "hdd0:%s", (path + 6));
			
	// 	//sprintf(partlist[0], "+Test");
		
    //     for(i = 0; i < MAX_PARTITIONS; i++)
    //     {
    //         if(!strncmp((path + 5), partlist[i], strlen(partlist[i]) - 1))
    //         {		
	// 				if(!fix_hddpath(path))
	// 				return 0;
					
	// 				if(is_main)
	// 				alwaysMounted = i;
					
	// 				break;
    //         }    
    //     }                               
    // }

	
	fd = opendir(path);

	DPRINTF("opendir %s fd %d\n", path, fd);

	if(fd > 0)
	{
		//DPRINTF("close %s\n", path);
		closedir(fd);
		
		if((p=strrchr(path, '/'))!=NULL && *(p+1) == 0)
    		*p = 0;
		
		//DPRINTF("argv %s\n", path);
		
		return 1;
	}
	
	return 0;
}

// int ps2DebugScreenInit()
// {
// #ifndef HOST
// 	init_scr();
// 	scr_clear();
// 	scr_printf("\n\n\n          ");
	
// 	return 1;
// #endif
// 	return 0;
// }

const char cnf_path_mc[] = "mc0:/PS2GBA/MAIN.CFG";

extern u32 parse_line(char *current_line, char *current_str);

int ps2GetMainPath(char *path, char *argv)
{
	FILE_TAG_TYPE cfg_file;
	char current_line[MAX_NAME];
	char current_str[MAX_NAME];
	char *p;
	int i;

	memset(current_str , 0, MAX_NAME);

	if(argv != NULL)
	{
		strcpy(current_str, argv);
		DPRINTF("argv %s\n", argv);
	}

	FILE_OPEN(cfg_file, cnf_path_mc, READ);
	
	if(FILE_CHECK_VALID(cfg_file))
	{
		ps2fgets(current_line, 256, cfg_file);

		FILE_CLOSE(cfg_file);

		for(i = 0; i < strlen(current_line); i++)
			if(current_line[i] == '\r' || current_line[i] == '\n')
				current_line[i] = 0;

		strcpy(path, current_line);

		DPRINTF("cnf_path %s\n", path);

		if(check_dir(path, 1))
			return 1;
	}
	
	//first check hd boot path
	// if(!strncmp(argv, "hdd0:", 5))
	// {
	// 	//hdd0:__sysconf:pfs:/FMCB/FMCB_configurator.elf
	// 	if((p=strrchr(argv, ':'))!=NULL)
	// 	{
			
	// 		sprintf(current_str + (p - argv) - 4, "%s", argv + (p - argv) + 1);
			
	// 		if((p=strrchr(current_str, '/')) != NULL)
	// 		{
	// 			*p = 0;
	// 		}
			
	// 		strcpy(path, current_str);
	// 	}
	// 	else
	// 		return 0; 
	// }
	// else 
	// if(!strncmp(argv, "cdfs:", 5) || !strncmp(argv, "cdrom", 5))
	// {
	// 	mkdir("mc0:/PS2GBA", 0750);
	// 	mkdir("mc0:/PS2GBA/REGBA", 0750);
	// 	strcpy(path, "mc0:/PS2GBA/REGBA");
	// }
	// else
	{
		memset(path , 0, 255);
		
		if((p=strrchr(argv, '/'))!=NULL)
		{
			memcpy(path, current_str, (p - argv));
		}
		else if((p=strrchr(argv, ':'))!=NULL)
		{
			memcpy(path, current_str, (p - argv) + 1);
		}
		else
			return 0;
	}
	
	DPRINTF("argv %s\n", path);
	
	return check_dir(path, 1);
}

char *ps2fgets(char *str, int size, FILE_TAG_TYPE stream)
{
	char c;
	int i;

	if(str == NULL || stream < 0)
	return NULL;
	
	for(i = 0; i < size - 1; i++)
	{
		if(FILE_READ(stream, &c, 1))
		{
			if(c == '\r')
			{
				FILE_READ(stream, &c, 1);
				
				if(c == '\n')
				{
					*(str + i) = '\r';
					*(str + i + 1) = '\n';
					*(str + i + 2) = '\0';
					return str;
				}
			}
			
			if(c == '\n')
			{
				*(str + i) = '\n';
				*(str + i + 1) = '\0';
				return str;
			}
			
			if(c == '\0')
			{
				*(str + i) = '\0';
				return str;
			}
			
			*(str + i) = c;
		}
		else
		return NULL;	
	}
	
	return NULL;
}

