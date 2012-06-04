#include <gccore.h>
#include <ogcsys.h>
#include <stdio.h>
#include <string.h>
#include <ogc/machine/processor.h>

#define EXECUTE_ADDR	((u8 *) 0x92000000)
#define BOOTER_ADDR		((u8 *) 0x93000000)
#define ARGS_ADDR		((u8 *) 0x93200000)

extern const u8 app_booter_bin[];
extern const u32 app_booter_bin_size;

typedef void (*entrypoint) (void);
void __exception_closeall();

static u8 *homebrewbuffer = EXECUTE_ADDR;
static u32 homebrewsize = 0;
static u8 position = 0;
static char *Arguments[2];

bool bootHB;

bool IsDollZ (u8 *buff)
{
  u8 dollz_stamp[] = {0x3C};
  int dollz_offs = 0x100;

  int ret = memcmp (&buff[dollz_offs], dollz_stamp, sizeof(dollz_stamp));
  if (ret == 0) return true;
  
  return false;
}

void AddBootArgument(char * argv)
{
	Arguments[position] = argv;
	position++;
}

int CopyHomebrewMemory(u8 *temp, u32 pos, u32 len)
{
	homebrewsize += len;
    memcpy(homebrewbuffer+pos, temp, len);
	DCFlushRange(homebrewbuffer+pos, len);
	return 1;
}

void FreeHomebrewBuffer()
{
	homebrewbuffer = EXECUTE_ADDR;
	homebrewsize = 0;
}

int LoadHomebrew(const char * filepath)
{
	if(!filepath) 
		return -1;

	FILE *file = fopen(filepath ,"rb");
	if(!file) 
		return -2;

	fseek(file, 0, SEEK_END);
	u32 filesize = ftell(file);
	rewind(file);

	bool good_read = fread(homebrewbuffer, 1, filesize, file) == filesize;
	if (!good_read)
    { 
	    fclose(file);
		return -4;
	}
	fclose(file);

	homebrewsize += filesize;
	return 1;
}

static int SetupARGV(struct __argv * args)
{
	if(!args) return -1;

	bzero(args, sizeof(struct __argv));
	args->argvMagic = ARGV_MAGIC;

	u32 argc = 0;
	u32 cmdposition = 0;
	u32 stringlength = 1;
	u32 i;

	/** Append Arguments **/
	for(i = 0; i < position; i++)
		stringlength += strlen(Arguments[i])+1;

	args->length = stringlength;
	//! Put the argument into mem2 too, to avoid overwriting it
	args->commandLine = (char *) ARGS_ADDR + sizeof(struct __argv);

	/** Append Arguments **/
	for(i = 0; i < position; i++)
	{
		strcpy(&args->commandLine[cmdposition], Arguments[i]);
		cmdposition += strlen(Arguments[i]) + 1;
		argc++;
	}

	args->argc = argc;

	args->commandLine[args->length - 1] = '\0';
	args->argv = &args->commandLine;
	args->endARGV = args->argv + 1;

	return 0;
}

int BootHomebrew()
{
	if(homebrewsize == 0) return -1;

	struct __argv args;
	if (!IsDollZ(homebrewbuffer))
		SetupARGV(&args);

	memcpy(BOOTER_ADDR, app_booter_bin, app_booter_bin_size);
	DCFlushRange(BOOTER_ADDR, app_booter_bin_size);

	entrypoint entry = (entrypoint) BOOTER_ADDR;

	memmove(ARGS_ADDR, &args, sizeof(args));
	DCFlushRange(ARGS_ADDR, sizeof(args) + args.length);

	SYS_ResetSystem(SYS_SHUTDOWN, 0, 0);
	entry();

	return 0;
}