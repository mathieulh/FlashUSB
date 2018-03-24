#include <pspsdk.h>
#include <pspkernel.h>
#include <pspctrl.h>
#include <pspusb.h>
#include <pspusbstor.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

PSP_MODULE_INFO("FlashUSB", 0x1000, 1, 0);
PSP_MAIN_THREAD_ATTR(0);

#define printf	pspDebugScreenPrintf

void ErrorExit(int milisecs, char *fmt, ...)
{
	va_list list;
	char msg[256];	

	va_start(list, fmt);
	vsprintf(msg, fmt, list);
	va_end(list);

	printf(msg);

	sceKernelDelayThread(milisecs*1000);
	sceKernelExitGame();
}

/* 1.50 specific function */
PspIoDrv *FindDriver(char *drvname)
{
	u32 *mod = (u32 *)sceKernelFindModuleByName("sceIOFileManager");

	if (!mod)
	{
		return NULL;
	}

	u32 text_addr = *(mod+27);

	u32 *(* GetDevice)(char *) = (void *)(text_addr+0x16D4);
	u32 *u;

	u = GetDevice(drvname);

	if (!u)
	{
		return NULL;
	}

	return (PspIoDrv *)u[1];
}

PspIoDrv *lflash_driver;
PspIoDrv *msstor_driver;

int (* Old_IoIoctl)(PspIoDrvFileArg *arg, unsigned int cmd, void *indata, int inlen, void *outdata, int outlen);

//PspIoDrvFuncs funcs;

static int New_IoOpen(PspIoDrvFileArg *arg, char *file, int flags, SceMode mode)
{
	//arg->fs_num = 1; // for flash 1

	if (!lflash_driver->funcs->IoOpen)
		return -1;

	return lflash_driver->funcs->IoOpen(arg, file, flags, mode);
}

static int New_IoClose(PspIoDrvFileArg *arg)
{
	//arg->fs_num = 1; // for flash 1

	if (!lflash_driver->funcs->IoClose)
		return -1;

	return lflash_driver->funcs->IoClose(arg);
}

static int New_IoRead(PspIoDrvFileArg *arg, char *data, int len)
{
	//arg->fs_num = 1; // for flash 1

	if (!lflash_driver->funcs->IoRead)
		return -1;

	return lflash_driver->funcs->IoRead(arg, data, len);
}
static int New_IoWrite(PspIoDrvFileArg *arg, const char *data, int len)
{
	//arg->fs_num = 1; // for flash 1

	if (!lflash_driver->funcs->IoWrite)
		return -1;

	return lflash_driver->funcs->IoWrite(arg, data, len);
}

static SceOff New_IoLseek(PspIoDrvFileArg *arg, SceOff ofs, int whence)
{
	//arg->fs_num = 1; // for flash 1

	if (!lflash_driver->funcs->IoLseek)
		return -1;

	return lflash_driver->funcs->IoLseek(arg, ofs, whence);
}

u8 data_5803[96] = 
{
	0x02, 0x00, 0x08, 0x00, 0x08, 0x00, 0x07, 0x9F, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x21, 0x21, 0x00, 0x00, 0x20, 0x01, 0x08, 0x00, 0x02, 0x00, 0x02, 0x00, 
	0x00, 0x00, 0x00, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static int New_IoIoctl(PspIoDrvFileArg *arg, unsigned int cmd, void *indata, int inlen, void *outdata, int outlen)
{
	//arg->fs_num = 1; // for flash 1

	Kprintf("IoIoctl: 0x%08X  %d  %d\n", cmd, inlen, outlen);

	if (cmd == 0x02125008)
	{
		u32 *x = (u32 *)outdata;
		*x = 1; /* Enable writing */
		return 0;
	}
	else if (cmd == 0x02125803)
	{
		memcpy(outdata, data_5803, 96);
		return 0;
	}

	return -1;
}

static int New_IoDevctl(PspIoDrvFileArg *arg, const char *devname, unsigned int cmd, void *indata, int inlen, void *outdata, int outlen)
{
	//arg->fs_num = 1;

	Kprintf("IoDevctl: 0x%08X  %d  %d\n", cmd, inlen, outlen);

	if (cmd == 0x02125801)
	{
		u8 *data8 = (u8 *)outdata;

		data8[0] = 1;
		data8[1] = 0;
		data8[2] = 0,
		data8[3] = 1;
		data8[4] = 0;
				
		return 0;
	}

	return -1;
}

int main()
{
	int retVal;
	
	pspDebugScreenInit();
	pspDebugScreenClear();

	pspDebugSioInit();
	pspDebugSioSetBaud(115200);
	pspDebugSioInstallKprintf();
	Kprintf("Kprintf installed.\n");

	printf("Press [HOME] to exit.\n");

	lflash_driver = FindDriver("lflash");
	
	pspSdkLoadStartModule("flash0:/kd/semawm.prx", PSP_MEMORY_PARTITION_KERNEL);
    pspSdkLoadStartModule("flash0:/kd/usbstor.prx", PSP_MEMORY_PARTITION_KERNEL);
    pspSdkLoadStartModule("flash0:/kd/usbstormgr.prx", PSP_MEMORY_PARTITION_KERNEL);
    pspSdkLoadStartModule("flash0:/kd/usbstorms.prx", PSP_MEMORY_PARTITION_KERNEL);
    pspSdkLoadStartModule("flash0:/kd/usbstorboot.prx", PSP_MEMORY_PARTITION_KERNEL);
	
	if (!lflash_driver)
	{
		ErrorExit(7000, "Cannot find lflash driver.\n");
	}

	msstor_driver = FindDriver("msstor");
	if (!msstor_driver)
	{
		ErrorExit(7000, "Cannot find msstor driver.\n");
	}

	msstor_driver->funcs->IoOpen = (void *)(0x80000000 | (u32)New_IoOpen);
	msstor_driver->funcs->IoClose = (void *)(0x80000000 | (u32)New_IoClose);
	msstor_driver->funcs->IoRead = (void *)(0x80000000 | (u32)New_IoRead);
	msstor_driver->funcs->IoWrite = (void *)(0x80000000 | (u32)New_IoWrite);
	msstor_driver->funcs->IoLseek = (void *)(0x80000000 | (u32)New_IoLseek);
	msstor_driver->funcs->IoIoctl = (void *)(0x80000000 | (u32)New_IoIoctl);
	msstor_driver->funcs->IoDevctl = (void *)(0x80000000 | (u32)New_IoDevctl);
	
	retVal = sceUsbStart(PSP_USBBUS_DRIVERNAME, 0, 0);
    if (retVal != 0) 
	{
		printf("Error starting USB Bus driver (0x%08X)\n", retVal);
		sceKernelSleepThread();
    }
    
	retVal = sceUsbStart(PSP_USBSTOR_DRIVERNAME, 0, 0);
    if (retVal != 0) 
	{
		printf("Error starting USB Mass Storage driver (0x%08X)\n", retVal);
		sceKernelSleepThread();
    }
    
	retVal = sceUsbstorBootSetCapacity(0x800000);
    if (retVal != 0) 
	{
		printf("Error setting capacity with USB Mass Storage driver (0x%08X)\n", retVal);
		sceKernelSleepThread();
    }

	sceUsbActivate(0x1c8);

	while (1)
	{
		SceCtrlData pad;

		sceCtrlReadBufferPositive(&pad, 1);

		if (pad.Buttons & PSP_CTRL_HOME)
			break;

		sceKernelDelayThread(10000);
	}	

	retVal = sceUsbDeactivate(0);
	if (retVal != 0)
	{
	    printf("Error calling sceUsbDeactivate (0x%08X)\n", retVal);
    }
    retVal = sceUsbStop(PSP_USBSTOR_DRIVERNAME, 0, 0);
    if (retVal != 0)
	{
		printf("Error stopping USB Mass Storage driver (0x%08X)\n", retVal);
	}

    retVal = sceUsbStop(PSP_USBBUS_DRIVERNAME, 0, 0);
    if (retVal != 0)
	{
		printf("Error stopping USB BUS driver (0x%08X)\n", retVal);
	}
	
	sceKernelExitGame();
	
	return 0;
}

