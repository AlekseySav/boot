#ifndef _BOOT_H_
#define _BOOT_H_

// cloned from block.S

#define BOOTSEG     0x07c0

#define SIGNATURE   0xaa55
#define SIGNPOS     510
#define PARTTABLE   446

#define BOOTSIZE    SIGNPOS     // because there's no PARTTABLE

#endif
