/* Stub header for native build - ashmem is Android-specific */
#ifndef _LINUX_ASHMEM_H
#define _LINUX_ASHMEM_H

#include <linux/ioctl.h>

#define ASHMEM_NAME_LEN		256

#define __ASHMEM_IOC		0x77

#define ASHMEM_SET_NAME		_IOW(__ASHMEM_IOC, 1, char[ASHMEM_NAME_LEN])
#define ASHMEM_GET_NAME		_IOR(__ASHMEM_IOC, 2, char[ASHMEM_NAME_LEN])
#define ASHMEM_SET_SIZE		_IO(__ASHMEM_IOC, 3)
#define ASHMEM_GET_SIZE		_IO(__ASHMEM_IOC, 4)
#define ASHMEM_SET_PROT_MASK	_IO(__ASHMEM_IOC, 5)
#define ASHMEM_GET_PROT_MASK	_IO(__ASHMEM_IOC, 6)
#define ASHMEM_PIN		_IOW(__ASHMEM_IOC, 7, struct ashmem_pin)
#define ASHMEM_UNPIN		_IOW(__ASHMEM_IOC, 8, struct ashmem_pin)
#define ASHMEM_GET_PIN_STATUS	_IO(__ASHMEM_IOC, 9)
#define ASHMEM_PURGE_ALL_CACHES	_IO(__ASHMEM_IOC, 10)

struct ashmem_pin {
	unsigned int offset;
	unsigned int len;
};

#endif
