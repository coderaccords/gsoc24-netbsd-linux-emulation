/*
 * MACHINE GENERATED: DO NOT EDIT
 * (BUT I'M A REBEL AND I'VE EDITED!)
 */

#include <sys/param.h>
#include <sys/conf.h>

/* device conversion table */
struct devsw_conv devsw_conv0[] = {
	{ "crypto", -1, 160, DEVNODE_SINGLE, 0, { 0, 0 }},
	{ "pf", -1, 161, DEVNODE_SINGLE, 0, { 0, 0 }},
	{ "fss", 163, 163, DEVNODE_VECTOR, DEVNODE_FLAG_LINKZERO, { 4, 0 }},
	{ "pps", -1, 164, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "ptm", -1, 165, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "atabus", -1, 166, DEVNODE_VECTOR, 0, { 4, 0 }},
	{ "drvctl", -1, 167, DEVNODE_SINGLE, 0, { 0, 0 }},
	{ "dk", 168, 168, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "tap", -1, 169, DEVNODE_VECTOR, 0, { 4, 0 }},
	{ "veriexec", -1, 170, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "fw", -1, 171, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "ucycom", -1, 172, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "gpio", -1, 173, DEVNODE_VECTOR, DEVNODE_FLAG_LINKZERO, { 8, 0 }},
	{ "utoppy", -1, 174, DEVNODE_VECTOR, 0, { 2, 0 }},
	{ "bthub", -1, 175, DEVNODE_SINGLE, 0, { 0, 0 }},
	{ "amr", -1, 176, DEVNODE_VECTOR, 0, { 1, 0 }},
	{ "lockstat", -1, 177, DEVNODE_SINGLE, 0, { 0, 0 }},
	{ "putter", -1, 178, DEVNODE_SINGLE, 0, { 0, 0 }},
	{ "srt", -1, 179, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "drm", -1, 180, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "bio", -1, 181, DEVNODE_SINGLE, 0, { 0, 0 }},
	{ "altmem", 182, 182, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "twa", -1, 187, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "cpuctl", -1, 188, DEVNODE_SINGLE, 0, { 0, 0 }},
	{ "pad", -1, 189, DEVNODE_VECTOR, DEVNODE_FLAG_LINKZERO, { 4, 0 }},
	{ "zfs", 190, 190, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "tprof", -1, 191, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "isv", -1, 192, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "video", -1, 193, DEVNODE_VECTOR, 0, { 4, 0 }},
	{ "dm", 169, 194, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "hdaudio", -1, 195, DEVNODE_VECTOR, 0, { 4, 0 }},
	{ "uhso", -1, 196, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "rumpblk", 197, 197, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "cons", -1, 0, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "ctty", -1, 1, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "null", -1, 2, DEVNODE_SINGLE, DEVNODE_FLAG_ISMINOR0, { DEV_NULL,0 }},
	{ "zero", -1, 2, DEVNODE_SINGLE, DEVNODE_FLAG_ISMINOR0, { DEV_ZERO,0 }},
	{ "wd", 0, 3, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "swap", 1, 4, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "pts", -1, 5, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "ptc", -1, 6, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "log", -1, 7, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "com", -1, 8, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "fd", 2, 9, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "wt", 3, 10, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "pc", -1, 12, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "sd", 4, 13, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "st", 5, 14, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "cd", 6, 15, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "lpt", -1, 16, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "ch", -1, 17, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "ccd", 16, 18, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "ss", -1, 19, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "uk", -1, 20, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "apm", -1, 21, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "filedesc", -1, 22, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "bpf", -1, 23, DEVNODE_VECTOR, DEVNODE_FLAG_LINKZERO, { 8, 0 }},
	{ "md", 17, 24, DEVNODE_VECTOR, 0, { 2, 8 }},
	{ "joy", -1, 26, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "spkr", -1, 27, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "cy", -1, 38, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "mcd", 7, 39, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "tun", -1, 40, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "vnd", 14, 41, DEVNODE_VECTOR, 0, { 2, 8 }},
	{ "audio", -1, 42, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "svr4_net", -1, 43, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "ipl", -1, 44, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "satlink", -1, 45, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "rnd", -1, 46, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "wsdisplay", -1, 47, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "wskbd", -1, 48, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "wsmouse", -1, 49, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "isdn", -1, 50, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "isdnctl", -1, 51, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "isdnbchan", -1, 52, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "isdntrc", -1, 53, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "isdntel", -1, 54, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "usb", -1, 55, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "uhid", -1, 56, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "ulpt", -1, 57, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "midi", -1, 58, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "sequencer", -1, 59, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "vcoda", -1, 60, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "scsibus", -1, 61, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "raid", 18, 62, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "esh", -1, 63, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "ugen", -1, 64, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "wsmux", -1, 65, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "ucom", -1, 66, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "sysmon", -1, 67, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "vmegeneric", -1, 68, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "ld", 19, 69, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "urio", -1, 70, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "bktr", -1, 71, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "cz", -1, 73, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "ses", -1, 74, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "uscanner", -1, 75, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "iop", -1, 76, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "altq", -1, 77, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "mlx", -1, 78, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "ed", 20, 79, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "mly", -1, 80, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "wsfont", -1, 81, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "agp", -1, 82, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "pci", -1, 83, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "dpti", -1, 84, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "irframe", -1, 85, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "cir", -1, 86, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "radio", -1, 87, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "cmos", -1, 88, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "clockctl", -1, 89, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "kttcp", -1, 91, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "dmoverio", -1, 92, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "cgd", 21, 93, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "dpt", -1, 96, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "twe", -1, 97, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "nsmb", -1, 98, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "vmmon", -1, 99, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "vmnet", -1, 100, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "ksyms", -1, 101, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "icp", -1, 102, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "gpib", -1, 103, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "ppi", -1, 104, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "rd", 22, 105, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "ct", 23, 106, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "mt", 24, 107, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "xenevt", -1, 141, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "xbd", 142, 142, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
	{ "xencons", -1, 143, DEVNODE_DONTBOTHER, 0, { 0, 0 }},
};

struct devsw_conv *devsw_conv = devsw_conv0;
int max_devsw_convs = __arraycount(devsw_conv0);
