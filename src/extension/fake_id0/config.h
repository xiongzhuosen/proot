#ifndef FAKE_ID0_CONFIG_H
#define FAKE_ID0_CONFIG_H

#include <sys/types.h>   /* uid_t, gid_t */
#include "extension/fake_id0/perm_config.h"

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

	/* permission configuration for specific paths */
	PermConfig perm_config;
	bool has_perm_config;
} Config;

#endif /* FAKE_ID0_CONFIG_H */
