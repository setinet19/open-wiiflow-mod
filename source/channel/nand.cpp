/***************************************************************************
 * Copyright (C) 2011 by Miigotu
 *           (C) 2012 by OverjoY for Wiiflow-mod
 *
 * Rewritten code from Mighty Channels and Triiforce
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any
 * damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any
 * purpose, including commercial applications, and to alter it and
 * redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you
 * must not claim that you wrote the original software. If you use
 * this software in a product, an acknowledgment in the product
 * documentation would be appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and
 * must not be misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source
 * distribution.
 *
 * Nand/Emulation Handling Class
 *
 * for wiiflow 2011
 ***************************************************************************/
 
#include <stdio.h>
#include <ogcsys.h>
#include <malloc.h>
#include <string.h>
#include <cstdlib>

#include "nand.hpp"
#include "utils.h"
#include "gecko.h"
#include "mem2.hpp"

u8 *confbuffer;
u8 CCode[0x1008];
char SCode[4];
char *txtbuffer;

config_header *cfg_hdr;

bool tbdec = false;
bool configloaded = false;

static NandDevice NandDeviceList[] = {
	{ "Disable",						0,	0x00,	0x00 },
	{ "SD/SDHC Card",					1,	0xF0,	0xF1 },
	{ "USB 2.0 Mass Storage Device",	2,	0xF2,	0xF3 },
};

Nand * Nand::instance = NULL;

Nand * Nand::Instance()
{
	if(instance == NULL)
		instance = new Nand();
	return instance;
}

void Nand::DestroyInstance()
{
	if(instance) delete instance;
	instance = NULL;
}

void Nand::Init(string path, u32 partition, bool disable)
{
	EmuDevice = disable ? REAL_NAND : partition == 0 ? EMU_SD : EMU_USB;
	Partition = disable ? REAL_NAND : partition > 0 ? partition - 1 : partition;
	Set_NandPath(path);
	Disabled = disable;
}

s32 Nand::Nand_Mount(NandDevice *Device)
{
	s32 fd = IOS_Open("fat", 0);
	if (fd < 0) return fd;

	static ioctlv vector[1] ATTRIBUTE_ALIGN(32);	
	
	vector[0].data = &Partition;
	vector[0].len = sizeof(u32);

	s32 ret = IOS_Ioctlv(fd, Device->Mount, 1, 0, vector);
	IOS_Close(fd);

	return ret;
}

s32 Nand::Nand_Unmount(NandDevice *Device)
{
	s32 fd = IOS_Open("fat", 0);
	if (fd < 0) return fd;

	s32 ret = IOS_Ioctlv(fd, Device->Unmount, 0, 0, NULL);
	IOS_Close(fd);

	return ret;
}

s32 Nand::Nand_Enable(NandDevice *Device)
{
	gprintf("Enabling NAND Emulator\n");
	s32 fd = IOS_Open("/dev/fs", 0);
	if (fd < 0) return fd;

	int NandPathlen = strlen(NandPath) + 1;

	static ioctlv vector[2] ATTRIBUTE_ALIGN(32);

	static u32 mode ATTRIBUTE_ALIGN(32) = Device->Mode | FullMode;

	vector[0].data = &mode;
	vector[0].len = sizeof(u32);
	vector[1].data = NandPath;
	vector[1].len = NandPathlen;

	s32 ret = IOS_Ioctlv(fd, 100, 2, 0, vector);
	IOS_Close(fd);

	return ret;
} 

s32 Nand::Nand_Disable(void)
{
	gprintf("Disabling NAND Emulator\n");
	s32 fd = IOS_Open("/dev/fs", 0);
	if (fd < 0) return fd;

	u32 inbuf ATTRIBUTE_ALIGN(32) = 0;
	s32 ret = IOS_Ioctl(fd, 100, &inbuf, sizeof(inbuf), NULL, 0);
	IOS_Close(fd);

	return ret;
} 

s32 Nand::Enable_Emu()
{
	if(MountedDevice == EmuDevice || Disabled)
		return 0;

	Disable_Emu();

	NandDevice *Device = &NandDeviceList[EmuDevice];

	s32 ret = Nand_Mount(Device);
	if (ret < 0) return ret;

	ret = Nand_Enable(Device);
	if (ret < 0) return ret;

	MountedDevice = EmuDevice;

	return 0;
}	

s32 Nand::Disable_Emu()
{
	if(MountedDevice == 0)
		return 0;

	NandDevice * Device = &NandDeviceList[MountedDevice];

	Nand_Disable();
	Nand_Unmount(Device);

	MountedDevice = 0;

	return 0;
}

void Nand::Set_NandPath(string path)
{
	if(isalnum(*(path.begin()))) path.insert(path.begin(), '/');
	else *(path.begin()) = '/';

	if(isalnum(*(path.end()))) path.push_back('/');
	else *(path.end()) = '/';

	if(path.size() <= 32)
		memcpy(NandPath, path.c_str(), path.size());
}

void Nand::__Dec_Enc_TB(void) 
{	
	u32 key = 0x73B5DBFA;
    int i;
    
	for( i=0; i < 0x100; ++i ) 
	{    
		txtbuffer[i] ^= key&0xFF;
		key = (key<<1) | (key>>31);
	}
	
	tbdec = tbdec ? false : true;
}

void Nand::__configshifttxt(char *str)
{
	const char *ptr = str;
	char *ctr = str;
	int i;
	int j = strlen(str);
	
	for( i=0; i<j; ++i )
	{		
		if( strncmp( str+(i-3), "PALC", 4 ) == 0 )
			*ctr = 0x0d;
		else if( strncmp( str+(i-2), "LUH", 3 ) == 0 )
			*ctr = 0x0d;
		else if( strncmp( str+(i-2), "LUM", 3 ) == 0 )	
			*ctr = 0x0d;
		else 
			*ctr = str[i];

		ctr++;
	}	
	*ctr = *ptr;
	*ctr = '\0';
}

s32 Nand::__configread(void)
{
	confbuffer = (u8 *)MEM2_alloc(0x4000);
	txtbuffer = (char *)MEM2_alloc(0x100);
	cfg_hdr = (config_header *)NULL;
	
	FILE *f = fopen(cfgpath, "rb");
	if(f)
	{
		fread(confbuffer, 1, 0x4000, f);
		gprintf("SYSCONF readed from: %s \n", cfgpath);
		fclose(f);
	}
		
	f = fopen(settxtpath, "rb");
	if(f)
	{
		fread(txtbuffer, 1, 0x100, f);
		gprintf("setting.txt readed from: %s \n", settxtpath);
		fclose(f);
	}
		
	cfg_hdr = (config_header *)confbuffer;
		
	__Dec_Enc_TB();
	
	configloaded = configloaded ? false : true;
	
	if(tbdec && configloaded)
		return 1;		
	
	return 0;	
}

s32 Nand::__configwrite(void)
{
	if(configloaded)
	{
		__Dec_Enc_TB();
		
		if(!tbdec)
		{
			FILE *f = fopen(cfgpath, "wb");
			if(f)
			{
				fwrite(confbuffer, 1, 0x4000, f);
				gprintf("SYSCONF written to: %s \n", cfgpath);
				fclose(f);
			}
				
			f = fopen(settxtpath, "wb");
			if(f)
			{
				fwrite(txtbuffer, 1, 0x100, f);
				gprintf("setting.txt written to: %s \n", settxtpath);
				fclose(f);
			}		
				
			configloaded = configloaded ? false : true;
			
			if(!tbdec && !configloaded)
				return 1;			
		}
	}
	MEM2_free(confbuffer);
	MEM2_free(txtbuffer);
	return 0;			
} 

u32 Nand::__configsetbyte(const char *item, u8 val)
{
	u32 i;
	for(i=0; i<cfg_hdr->ncnt; ++i)
	{
		if(memcmp(confbuffer+(cfg_hdr->noff[i] + 1), item, strlen(item)) == 0)
		{
			*(u8*)(confbuffer+cfg_hdr->noff[i] + 1 + strlen(item)) = val;
			break;
		}
	}
	return 0;
}

u32 Nand::__configsetbigarray(const char *item, void *val, u32 size)
{
	u32 i;
	for(i=0; i<cfg_hdr->ncnt; ++i)
	{
		if(memcmp(confbuffer+(cfg_hdr->noff[i] + 1), item, strlen(item)) == 0)
		{
			memcpy(confbuffer+cfg_hdr->noff[i] + 3 + strlen(item), val, size);
			break;
		}
	}
	return 0;
}

u32 Nand::__configsetsetting(const char *item, const char *val)
{		
	char *curitem = strstr(txtbuffer, item);
	char *curstrt, *curend;
	
	if(curitem == NULL)
		return 0;
	
	curstrt = strchr(curitem, '=');
	curend = strchr(curitem, 0x0d);
	
	if( curstrt && curend )
	{
		curstrt += 1;
		u32 len = curend - curstrt;
		if( strlen( val ) > len )
		{
			static char buffer[0x100];
			u32 nlen;
			nlen = txtbuffer-(curstrt+strlen(val));
			strcpy( buffer, txtbuffer+nlen );
			strncpy( curstrt, val, strlen(val));
			curstrt += strlen(val); 
			strncpy(curstrt, buffer, strlen(buffer));
		}
		else
		{
			strncpy(curstrt, val, strlen(val));
		}

		__configshifttxt(txtbuffer);

		return 1;
	}
	return 0;
}

s32 Nand::Do_Region_Change(string id, char *path)
{
	bzero(cfgpath, MAX_FAT_PATH);	
	bzero(settxtpath, MAX_FAT_PATH);
	
	snprintf(cfgpath, sizeof(cfgpath), "%s/shared2/sys/SYSCONF", path);
	snprintf(settxtpath, sizeof(settxtpath), "%s/title/00000001/00000002/data/setting.txt", path);
	
	if(__configread())
	{
		switch(id[3])
		{
			case 'J':
			{
				gprintf("Switching region to NTSC-j \n");
				CCode[0] = 1;
				__configsetbyte( "IPL.LNG", 0 );				
				__configsetbigarray( "SADR.LNG", CCode, 0x1007 );
				__configsetsetting( "AREA", "JPN" );
				__configsetsetting( "MODEL", "RVL-001(JPN)" );
				__configsetsetting( "CODE", "LJM" );
				__configsetsetting( "VIDEO", "NTSC" );
				__configsetsetting( "GAME", "JP" );								
			} break;
			case 'E':
			{
				gprintf("Switching region to NTSC-u \n");
				CCode[0] = 31;
				__configsetbyte( "IPL.LNG", 1 );				
				__configsetbigarray( "IPL.SADR", CCode, 0x1007 );
				__configsetsetting( "AREA", "USA" );
				__configsetsetting( "MODEL", "RVL-001(USA)" );
				__configsetsetting( "CODE", "LU" );
				__configsetsetting( "VIDEO", "NTSC" );
				__configsetsetting( "GAME", "US" );									
			} break;
			case 'D':
			case 'F':
			case 'I':
			case 'M':
			case 'P':
			case 'S':
			case 'U':					
			{
				gprintf("Switching region to PAL \n");
				CCode[0] = 110;
				__configsetbyte( "IPL.LNG", 6 );				
				__configsetbigarray( "IPL.SADR", CCode, 0x1007 );
				__configsetsetting( "AREA", "EUR" );
				__configsetsetting( "MODEL", "RVL-001(EUR)" );
				__configsetsetting( "CODE", "LEH" );
				__configsetsetting( "VIDEO", "PAL" );
				__configsetsetting( "GAME", "EU" );								
			} break;
			case 'K':					
			{
				gprintf("Switching region to NTSC-k \n");
				CCode[0] = 137;
				__configsetbyte( "IPL.LNG", 9 );				
				__configsetbigarray( "IPL.SADR", CCode, 0x1007 );
				__configsetsetting( "AREA", "KOR" );
				__configsetsetting( "MODEL", "RVL-001(KOR)" );
				__configsetsetting( "CODE", "LKM" );
				__configsetsetting( "VIDEO", "NTSC" );
				__configsetsetting( "GAME", "KR" );
			} break;		
		}
	}
	__configwrite();
	return 1;	
}
	
	
	
	
	
	