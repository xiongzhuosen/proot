#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fnmatch.h>
#include <errno.h>
#include <sys/types.h>

#include "extension/fake_id0/permconfig.h"

/**
 * Parse mode string in various formats:
 *   "0755", "755"           -> octal
 *   "rwxr-xr-x"             -> symbolic (9 chars)
 *   "rwsr-xr-x"             -> symbolic with setuid
 *   "rwxr-sr-x"             -> symbolic with setgid
 *   "rwxrwxrwt"             -> symbolic with sticky
 *   "-" or empty            -> keep original (returns -1)
 */
int parse_mode_string(const char *str, mode_t *mode)
{
	if (str == NULL || str[0] == '\0')
		return -1;

	/* "-" means keep original */
	if (str[0] == '-' && str[1] == '\0')
		return -1;

	/* Try octal format: "0755", "1755", "4755", "2755", etc. */
	if (isdigit((unsigned char)str[0])) {
		long val = strtol(str, NULL, 8);
		if (val > 0 && val < 0100000) {
			*mode = (mode_t)val;
			return 0;
		}
	}

	/* Try symbolic format: "rwxr-xr-x" (9 chars) */
	{
		size_t len = strlen(str);
		const char *p = str;
		
		/* Skip leading file type char if present (d, l, c, b, s, p, -) */
		if (len == 10)
			p++;
		else if (len != 9)
			return -1;

		mode_t m = 0;

		/* Owner permissions */
		if (p[0] == 'r') m |= S_IRUSR;
		if (p[1] == 'w') m |= S_IWUSR;
		if (p[2] == 'x') m |= S_IXUSR;
		else if (p[2] == 's') m |= S_IXUSR | S_ISUID;
		else if (p[2] == 'S') m |= S_ISUID;

		/* Group permissions */
		if (p[3] == 'r') m |= S_IRGRP;
		if (p[4] == 'w') m |= S_IWGRP;
		if (p[5] == 'x') m |= S_IXGRP;
		else if (p[5] == 's') m |= S_IXGRP | S_ISGID;
		else if (p[5] == 'S') m |= S_ISGID;

		/* Other permissions */
		if (p[6] == 'r') m |= S_IROTH;
		if (p[7] == 'w') m |= S_IWOTH;
		if (p[8] == 'x') m |= S_IXOTH;
		else if (p[8] == 't') m |= S_IXOTH | S_ISVTX;
		else if (p[8] == 'T') m |= S_ISVTX;

		*mode = m;
		return 0;
	}

	return -1;
}

/**
 * Load username to UID mapping from rootfs /etc/passwd
 * Format: username:x:UID:GID:gecos:home:shell
 */
static int load_passwd_file(PermConfig *config)
{
	char path[PATH_MAX];
	FILE *fp;
	char line[512];

	if (config->rootfs[0] != '\0')
		snprintf(path, sizeof(path), "%s/etc/passwd", config->rootfs);
	else
		strncpy(path, "/etc/passwd", sizeof(path) - 1);

	fp = fopen(path, "r");
	if (fp == NULL)
		return -1;

	config->uid_map_count = 0;
	while (fgets(line, sizeof(line), fp) != NULL &&
	       config->uid_map_count < MAX_NAME_MAP) {
		char *colon, *colon2, *colon3;
		unsigned long uid_val;

		/* Skip comments and empty lines */
		if (line[0] == '#' || line[0] == '\n')
			continue;

		/* Parse: name:x:uid:gid:... */
		colon = strchr(line, ':');
		if (colon == NULL)
			continue;
		*colon = '\0';

		colon2 = strchr(colon + 1, ':');
		if (colon2 == NULL)
			continue;
		
		colon3 = strchr(colon2 + 1, ':');
		if (colon3 == NULL)
			continue;

		uid_val = strtoul(colon3 + 1, NULL, 10);

		strncpy(config->uid_map[config->uid_map_count].name, line,
			sizeof(config->uid_map[0].name) - 1);
		config->uid_map[config->uid_map_count].uid = (uid_t)uid_val;
		config->uid_map_count++;
	}

	fclose(fp);
	return 0;
}

/**
 * Load groupname to GID mapping from rootfs /etc/group
 * Format: groupname:x:GID:userlist
 */
static int load_group_file(PermConfig *config)
{
	char path[PATH_MAX];
	FILE *fp;
	char line[512];

	if (config->rootfs[0] != '\0')
		snprintf(path, sizeof(path), "%s/etc/group", config->rootfs);
	else
		strncpy(path, "/etc/group", sizeof(path) - 1);

	fp = fopen(path, "r");
	if (fp == NULL)
		return -1;

	config->gid_map_count = 0;
	while (fgets(line, sizeof(line), fp) != NULL &&
	       config->gid_map_count < MAX_NAME_MAP) {
		char *colon, *colon2;
		unsigned long gid_val;

		if (line[0] == '#' || line[0] == '\n')
			continue;

		colon = strchr(line, ':');
		if (colon == NULL)
			continue;
		*colon = '\0';

		colon2 = strchr(colon + 1, ':');
		if (colon2 == NULL)
			continue;

		gid_val = strtoul(colon2 + 1, NULL, 10);

		strncpy(config->gid_map[config->gid_map_count].name, line,
			sizeof(config->gid_map[0].name) - 1);
		config->gid_map[config->gid_map_count].gid = (gid_t)gid_val;
		config->gid_map_count++;
	}

	fclose(fp);
	return 0;
}

/**
 * Load user/group name resolution maps from rootfs.
 */
int load_user_group_maps(PermConfig *config)
{
	load_passwd_file(config);
	load_group_file(config);
	return 0;
}

/**
 * Resolve username to UID.
 * Supports: numeric string ("1000") or username ("postgres")
 */
int resolve_uid(PermConfig *config, const char *user, uid_t *uid)
{
	char *endptr;

	if (user == NULL || user[0] == '\0')
		return -1;

	/* "-" means keep original */
	if (user[0] == '-' && user[1] == '\0')
		return -1;

	/* Try numeric first */
	*uid = (uid_t)strtoul(user, &endptr, 10);
	if (*endptr == '\0' && user[0] != '\0')
		return 0;

	/* Look up in passwd map */
	{
		int i;
		for (i = 0; i < config->uid_map_count; i++) {
			if (strcmp(config->uid_map[i].name, user) == 0) {
				*uid = config->uid_map[i].uid;
				return 0;
			}
		}
	}

	return -1;
}

/**
 * Resolve groupname to GID.
 */
int resolve_gid(PermConfig *config, const char *group, gid_t *gid)
{
	char *endptr;

	if (group == NULL || group[0] == '\0')
		return -1;

	/* "-" means keep original */
	if (group[0] == '-' && group[1] == '\0')
		return -1;

	/* Try numeric first */
	*gid = (gid_t)strtoul(group, &endptr, 10);
	if (*endptr == '\0' && group[0] != '\0')
		return 0;

	/* Look up in group map */
	{
		int i;
		for (i = 0; i < config->gid_map_count; i++) {
			if (strcmp(config->gid_map[i].name, group) == 0) {
				*gid = config->gid_map[i].gid;
				return 0;
			}
		}
	}

	return -1;
}

/**
 * Match a path against glob pattern.
 */
int path_matches_pattern(const char *path, const char *pattern)
{
	/* Exact match */
	if (strcmp(path, pattern) == 0)
		return 1;

	/* Directory prefix match: pattern is parent dir of path */
	{
		size_t pattern_len = strlen(pattern);
		size_t path_len = strlen(path);
		if (path_len > pattern_len &&
		    strncmp(path, pattern, pattern_len) == 0 &&
		    path[pattern_len] == '/')
			return 1;
	}

	/* Glob match */
	if (strchr(pattern, '*') != NULL || strchr(pattern, '?') != NULL ||
	    strchr(pattern, '[') != NULL) {
		if (fnmatch(pattern, path, FNM_PATHNAME | FNM_PERIOD) == 0)
			return 1;
	}

	return 0;
}

/**
 * Parse a configuration line.
 * 
 * Supported formats:
 *   Format 1 (recommended):  path_pattern  user:group  mode
 *     Example: /var/lib/postgresql  postgres:postgres  0755
 *     Example: /tmp/*               1000:1000          rwxrwxrwt
 *
 *   Format 2 (ls -l style):   mode_str  user  group  path
 *     Example: drwxr-xr-x  postgres  postgres  /var/lib/postgresql
 *     Example: -rw-r--r--  root      root      /etc/passwd
 *
 *   Format 3 (numeric):     path_pattern  uid:gid  mode_octal
 *     Example: /data  70:70  0700
 */
static int parse_rule_line(PermRule *rule, const char *line)
{
	char field1[PATH_MAX], field2[256], field3[256], field4[PATH_MAX];
	int n;

	memset(rule, 0, sizeof(*rule));
	rule->user_str[0] = '-';
	rule->group_str[0] = '-';
	rule->mode_str[0] = '-';
	rule->has_uid = 0;
	rule->has_gid = 0;
	rule->has_mode = 0;

	/* Try Format 2 (ls -l style) first: "mode_str user group path" */
	n = sscanf(line, "%15s %255s %255s %4095s", field1, field2, field3, field4);
	if (n == 4) {
		/* Check if field1 looks like a mode string */
		if ((field1[0] == 'd' || field1[0] == '-' || field1[0] == 'l' ||
		     field1[0] == 'c' || field1[0] == 'b' || field1[0] == 's' ||
		     field1[0] == 'p') &&
		    strlen(field1) >= 9) {
			/* It's ls -l style */
			strncpy(rule->pattern, field4, PATH_MAX - 1);
			strncpy(rule->user_str, field2, sizeof(rule->user_str) - 1);
			strncpy(rule->group_str, field3, sizeof(rule->group_str) - 1);
			/* Skip the file type character */
			strncpy(rule->mode_str, field1 + 1, sizeof(rule->mode_str) - 1);
			return 0;
		}
	}

	/* Try Format 1/3: "path_pattern user:group mode" or "path_pattern uid:gid mode" */
	n = sscanf(line, "%4095s %255s %255s", field1, field2, field3);
	if (n >= 2) {
		char *colon;

		strncpy(rule->pattern, field1, PATH_MAX - 1);

		/* Check if field2 contains user:group */
		colon = strchr(field2, ':');
		if (colon != NULL) {
			*colon = '\0';
			strncpy(rule->user_str, field2, sizeof(rule->user_str) - 1);
			strncpy(rule->group_str, colon + 1, sizeof(rule->group_str) - 1);
		}
		else {
			/* Maybe field2 is just user, and field3 is group */
			strncpy(rule->user_str, field2, sizeof(rule->user_str) - 1);
			if (n >= 3)
				strncpy(rule->group_str, field3, sizeof(rule->group_str) - 1);
		}

		/* Get mode from remaining field */
		if (n == 3 && colon == NULL) {
			/* field3 might be mode if it looks like one */
			if (isdigit((unsigned char)field3[0]) ||
			    field3[0] == 'r' || field3[0] == 'd') {
				strncpy(rule->mode_str, field3, sizeof(rule->mode_str) - 1);
			}
		}
		else if (n == 3 && colon != NULL) {
			strncpy(rule->mode_str, field3, sizeof(rule->mode_str) - 1);
		}

		return 0;
	}

	return -1;
}

/**
 * Load permission configuration from file.
 *
 * Configuration file format:
 *   # Comment lines start with #
 *   @rootfs /path/to/rootfs    (optional: sets rootfs for name resolution)
 *   
 *   # Recommended format:
 *   path_pattern  user:group  mode
 *   
 *   # ls -l style (directly from `ls -l` output):
 *   mode_str  user  group  path
 *
 * Mode formats:
 *   - Octal: "0755", "4755" (with setuid), "2755" (with setgid), "1755" (with sticky)
 *   - Symbolic: "rwxr-xr-x", "rwsr-xr-x" (setuid), "rwxr-sr-x" (setgid), "rwxrwxrwt" (sticky)
 *   - Use "-" to keep original
 *
 * User/Group:
 *   - Username: "postgres", "root", "www-data"
 *   - UID/GID: "70", "1000"
 *   - Use "-" to keep original
 */
int load_perm_config(PermConfig *config, const char *path)
{
	FILE *fp;
	char line[PATH_MAX + 256];
	int line_num = 0;

	if (path == NULL || strlen(path) == 0)
		return -1;

	memset(config, 0, sizeof(PermConfig));
	config->count = 0;

	/* First pass: look for @rootfs directive */
	{
		FILE *fp2 = fopen(path, "r");
		if (fp2 != NULL) {
			while (fgets(line, sizeof(line), fp2) != NULL) {
				char *ptr = line;
				while (*ptr == ' ' || *ptr == '\t')
					ptr++;
				if (strncmp(ptr, "@rootfs", 7) == 0 &&
				    (ptr[7] == ' ' || ptr[7] == '\t' || ptr[7] == '\n' || ptr[7] == '\0')) {
					char *rpath = ptr + 7;
					size_t len;
					while (*rpath == ' ' || *rpath == '\t')
						rpath++;
					len = strlen(rpath);
					if (len > 0 && rpath[len - 1] == '\n')
						rpath[len - 1] = '\0';
					if (len > 0 && rpath[len - 1] == '\r')
						rpath[len - 1] = '\0';
					strncpy(config->rootfs, rpath, PATH_MAX - 1);
					break;
				}
			}
			fclose(fp2);
		}
	}

	/* Load user/group name maps */
	load_user_group_maps(config);

	/* Second pass: parse rules */
	fp = fopen(path, "r");
	if (fp == NULL)
		return -errno;

	while (fgets(line, sizeof(line), fp) != NULL) {
		char *ptr;
		int len;

		line_num++;

		/* Remove trailing newline */
		len = strlen(line);
		if (len > 0 && line[len - 1] == '\n')
			line[len - 1] = '\0';

		/* Skip empty lines, comments, and directives */
		ptr = line;
		while (*ptr == ' ' || *ptr == '\t')
			ptr++;
		if (*ptr == '#' || *ptr == '\0' || *ptr == '@')
			continue;

		if (config->count >= MAX_PERM_RULES) {
			fprintf(stderr, "proot: perm-config: max rules (%d) reached at line %d\n",
				MAX_PERM_RULES, line_num);
			break;
		}

		/* Parse the rule */
		if (parse_rule_line(&config->rules[config->count], ptr) == 0) {
			PermRule *rule = &config->rules[config->count];

			/* Resolve user to UID */
			if (rule->user_str[0] != '-' || rule->user_str[1] != '\0') {
				if (resolve_uid(config, rule->user_str, &rule->uid) == 0)
					rule->has_uid = 1;
			}

			/* Resolve group to GID */
			if (rule->group_str[0] != '-' || rule->group_str[1] != '\0') {
				if (resolve_gid(config, rule->group_str, &rule->gid) == 0)
					rule->has_gid = 1;
			}

			/* Parse mode */
			if (rule->mode_str[0] != '-' || rule->mode_str[1] != '\0') {
				if (parse_mode_string(rule->mode_str, &rule->mode) == 0)
					rule->has_mode = 1;
			}

			config->count++;
		}
		else {
			fprintf(stderr, "proot: perm-config: invalid format at line %d: %s\n",
				line_num, ptr);
		}
	}

	fclose(fp);
	return 0;
}

/**
 * Find a matching permission rule for the given path.
 * Last matching rule wins (like .gitignore).
 */
int match_perm_rule(PermConfig *config, const char *path,
		    uid_t *uid, gid_t *gid, mode_t *mode,
		    int *has_uid, int *has_gid, int *has_mode)
{
	int i;

	if (config == NULL || config->count == 0 || path == NULL)
		return -1;

	/* Search in reverse order (last match wins) */
	for (i = config->count - 1; i >= 0; i--) {
		if (path_matches_pattern(path, config->rules[i].pattern)) {
			*uid = config->rules[i].uid;
			*gid = config->rules[i].gid;
			*mode = config->rules[i].mode;
			*has_uid = config->rules[i].has_uid;
			*has_gid = config->rules[i].has_gid;
			*has_mode = config->rules[i].has_mode;
			return 0;
		}
	}

	return -1;
}
