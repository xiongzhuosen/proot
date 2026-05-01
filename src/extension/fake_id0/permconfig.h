#ifndef FAKE_ID0_PERMCONFIG_H
#define FAKE_ID0_PERMCONFIG_H

#include <sys/types.h>
#include <linux/limits.h>

#define MAX_PERM_RULES 256

typedef struct {
	char pattern[PATH_MAX];
	uid_t uid;
	gid_t gid;
	int has_uid; /* -1 = not set, 0 = use default, 1 = use specific value */
	int has_gid;
} PermRule;

typedef struct {
	PermRule rules[MAX_PERM_RULES];
	int count;
	char config_path[PATH_MAX];
} PermConfig;

int load_perm_config(PermConfig *perm_config, const char *path);
int match_perm_rule(PermConfig *perm_config, const char *path, uid_t *uid, gid_t *gid, int *has_uid, int *has_gid);
int path_matches_pattern(const char *path, const char *pattern);

#endif /* FAKE_ID0_PERMCONFIG_H */
