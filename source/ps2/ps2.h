
#define DT_UNKNOWN       0
#define DT_FIFO          1
#define DT_CHR           2
#define DT_DIR           4
#define DT_BLK           6
#define DT_REG           8
#define DT_LNK          10
#define DT_SOCK         12
#define DT_WHT          14 

int Setup_Pad(void);
void Wait_Pad_Ready(void);
void WaitPadReady(int port, int slot);
int ps2init();
void ps2delay(int count);
int ps2quit();

// extern int fileXio_mode;

int ps2Chdir(char *path);

void copy_path(char *main_path, char *file_path, char *out);
int check_dir(char *path, int is_main);

// int ps2DebugScreenInit();

int ps2GetMainPath(char *path, char *argv);
char *ps2fgets(char *str, int size, FILE_TAG_TYPE stream);

