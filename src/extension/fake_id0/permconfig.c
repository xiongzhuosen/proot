#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fnmatch.h>
#include <errno.h>

#include "extension/fake_id0/permconfig.h"

/**
 * Load permission configuration from file.
 * File format:
 *   # comment lines start with #
 *   path_pattern uid gid
 *   uid/gid can be '-' to keep default behavior
 *   path patterns support glob wildcards (* ? [...])
 */
int load_perm_config(PermConfig *perm_config, const char *path)
{
	FILE *fp;
	char line[PATH_MAX + 64];
	char uid_str[32], gid_str[32];
	int line_num = 0;

	if (path == NULL || strlen(path) == 0)
		return -1;

	memset(perm_config, 0, sizeof(PermConfig));
	strncpy(perm_config->config_path, path, PATH_MAX - 1);
	perm_config->config_path[PATH_MAX - 1] = '\0';

	fp = fopen(path, "r");
	if (fp == NULL) {
		return -errno;
	}

	while (fgets(line, sizeof(line), fp) != NULL) {
		char *ptr;
		int len;

		line_num++;

		/* Remove trailing newline */
		len = strlen(line);
		if (len > 0 && line[len - 1] == '\n')
			line[len - 1] = '\0';

		/* Skip empty lines and comments */
		ptr = line;
		while (*ptr == ' ' || *ptr == '\t')
			ptr++;
		if (*ptr == '#' || *ptr == '\0')
			continue;

		if (perm_config->count >= MAX_PERM_RULES) {
			fprintf(stderr, "proot: perm-config: max rules (%d) reached at line %d\n",
				MAX_PERM_RULES, line_num);
			break;
		}

		/* Parse: path_pattern uid gid */
		if (sscanf(ptr, "%s %31s %31s",
			   perm_config->rules[perm_config->count].pattern,
			   uid_str, gid_str) != 3) {
			fprintf(stderr, "proot: perm-config: invalid format at line %d\n", line_num);
			continue;
		}

		/* Parse uid */
		if (strcmp(uid_str, "-") == 0) {
			perm_config->rules[perm_config->count].has_uid = -1;
			perm_config->rules[perm_config->count].uid = 0;
		} else {
			perm_config->rules[perm_config->count].has_uid = 1;
			perm_config->rules[perm_config->count].uid = (uid_t)atoi(uid_str);
		}

		/* Parse gid */
		if (strcmp(gid_str, "-") == 0) {
			perm_config->rules[perm_config->count].has_gid = -1;
			perm_config->rules[perm_config->count].gid = 0;
		} else {
			perm_config->rules[perm_config->count].has_gid = 1;
			perm_config->rules[perm_config->count].gid = (gid_t)atoi(gid_str);
		}

		perm_config->count++;
	}

	fclose(fp);
	return 0;
}

/**
 * Match a path against glob pattern. Supports *, ?, [...] wildcards.
 */
int path_matches_pattern(const char *path, const char *pattern)
{
	/* Try exact match first */
	if (strcmp(path, pattern) == 0)
		return 1;

	/* Try prefix match (pattern is a directory prefix) */
	{
		size_t pattern_len = strlen(pattern);
		size_t path_len = strlen(path);
		if (path_len > pattern_len &&
		    strncmp(path, pattern, pattern_len) == 0 &&
		    path[pattern_len] == '/')
			return 1;
	}

	/* Try glob match */
	if (strchr(pattern, '*') != NULL || strchr(pattern, '?') != NULL ||
	    strchr(pattern, '[') != NULL) {
		if (fnmatch(pattern, path, FNM_PATHNAME | FNM_PERIOD) == 0)
			return 1;
	}

	return 0;
}

/**
 * Find a matching permission rule for the given path.
 * Returns 0 if a rule is found (uid/gid set), -1 if no match.
 */
int match_perm_rule(PermConfig *perm_config, const char *path,
		    uid_t *uid, gid_t *gid, int *has_uid, int *has_gid)
{
	int i;

	if (perm_config == NULL || perm_config->count == 0 || path == NULL)
		return -1;

	/* Search rules in reverse order (last match wins, like .gitignore) */
	for (i = perm_config->count - 1; i >= 0; i--) {
		if (path_matches_pattern(path, perm_config->rules[i].pattern)) {
			*uid = perm_config->rules[i].uid;
			*gid = perm_config->rules[i].gid;
			*has_uid = perm_config->rules[i].has_uid;
			*has_gid = perm_config->rules[i].has_gid;
			return 0;
		}
	}

	return -1;
}
