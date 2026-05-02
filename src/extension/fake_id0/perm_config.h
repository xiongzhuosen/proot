#ifndef PERM_CONFIG_H
#define PERM_CONFIG_H

#include <sys/types.h>
#include <stdbool.h>

#define MAX_PERM_RULES 256
#define MAX_PATH_LEN 4096

typedef enum {
	MATCH_EXACT,    /* exact path match */
	MATCH_PREFIX,   /* directory and all children (path/**) */
} MatchType;

typedef struct {
	char path[MAX_PATH_LEN];
	MatchType match_type;
	uid_t uid;
	gid_t gid;
	mode_t mode;
	bool has_uid;
	bool has_gid;
	bool has_mode;
} PermRule;

typedef struct {
	PermRule rules[MAX_PERM_RULES];
	int rule_count;
	char config_path[MAX_PATH_LEN];
} PermConfig;

extern int load_perm_config(PermConfig *config, const char *path);
extern bool find_perm_rule(const PermConfig *config, const char *filepath,
			   PermRule *out_rule);

#endif /* PERM_CONFIG_H */
