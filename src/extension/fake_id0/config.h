#ifndef FAKE_ID0_CONFIG_H
#define FAKE_ID0_CONFIG_H

#include <sys/types.h>   /* uid_t, gid_t */
#include "extension/fake_id0/permconfig.h"

typedef struct {
	uid_t ruid;
	uid_t euid;
	uid_t suid;
	uid_t fsuid;

	gid_t rgid;
	gid_t egid;
	gid_t sgid;
	gid_t fsgid;

	mode_t umask;

	/* Permission configuration for specific paths */
	PermConfig perm_config;
	int has_perm_config; /* 1 if perm config is loaded */
} Config;

#endif /* FAKE_ID0_CONFIG_H */
