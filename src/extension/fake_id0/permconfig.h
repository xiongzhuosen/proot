#ifndef FAKE_ID0_PERMCONFIG_H
#define FAKE_ID0_PERMCONFIG_H

#include <sys/types.h>
#include <sys/stat.h>
#include <linux/limits.h>

#define MAX_PERM_RULES 2048
#define MAX_NAME_MAP 1024

typedef struct {
	char name[128];
	uid_t uid;
} UidMapEntry;

typedef struct {
	char name[128];
	gid_t gid;
} GidMapEntry;

typedef struct {
	char pattern[PATH_MAX];
	char user_str[128];
	char group_str[128];
	char mode_str[16];
	uid_t uid;
	gid_t gid;
	mode_t mode;
	int has_uid;
	int has_gid;
	int has_mode;
} PermRule;

typedef struct {
	PermRule rules[MAX_PERM_RULES];
	int count;
	UidMapEntry uid_map[MAX_NAME_MAP];
	int uid_map_count;
	GidMapEntry gid_map[MAX_NAME_MAP];
	int gid_map_count;
	char rootfs[PATH_MAX];
} PermConfig;

int load_perm_config(PermConfig *config, const char *path);
int load_user_group_maps(PermConfig *config);
int match_perm_rule(PermConfig *config, const char *path,
		    uid_t *uid, gid_t *gid, mode_t *mode,
		    int *has_uid, int *has_gid, int *has_mode);
int path_matches_pattern(const char *path, const char *pattern);
int parse_mode_string(const char *str, mode_t *mode);
int resolve_uid(PermConfig *config, const char *user, uid_t *uid);
int resolve_gid(PermConfig *config, const char *group, gid_t *gid);

#endif
