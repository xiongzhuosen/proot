#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include "extension/fake_id0/perm_config.h"

/* skip leading whitespace */
static const char *skip_ws(const char *s)
{
	while (*s && isspace((unsigned char)*s))
		s++;
	return s;
}

/* parse a line like:
 *   path=/var/lib/postgresql uid=999 gid=999 mode=0700
 *   /var/lib/postgresql/** uid=999 gid=999
 *   /var/lib/postgresql/data uid=999
 *
 * supports:
 *   path=VALUE   - exact match (default)
 *   VALUE        - path, if ends with /** it's prefix match
 *   uid=NUMBER
 *   gid=NUMBER
 *   mode=OCTAL
 */
static int parse_perm_line(const char *line, PermRule *rule)
{
	const char *p = line;
	const char *end;
	char buf[MAX_PATH_LEN];

	memset(rule, 0, sizeof(*rule));

	while (1) {
		p = skip_ws(p);
		if (*p == '\0' || *p == '#')
			break;

		/* uid=NUMBER */
		if (strncmp(p, "uid=", 4) == 0) {
			p += 4;
			rule->uid = (uid_t)strtoul(p, (char **)&end, 10);
			if (end == p)
				return -1;
			p = end;
			rule->has_uid = true;
			continue;
		}

		/* gid=NUMBER */
		if (strncmp(p, "gid=", 4) == 0) {
			p += 4;
			rule->gid = (gid_t)strtoul(p, (char **)&end, 10);
			if (end == p)
				return -1;
			p = end;
			rule->has_gid = true;
			continue;
		}

		/* mode=OCTAL */
		if (strncmp(p, "mode=", 5) == 0) {
			p += 5;
			rule->mode = (mode_t)strtoul(p, (char **)&end, 8);
			if (end == p)
				return -1;
			p = end;
			rule->has_mode = true;
			continue;
		}

		/* path=VALUE */
		if (strncmp(p, "path=", 5) == 0) {
			p += 5;
			end = p;
			while (*end && !isspace((unsigned char)*end))
				end++;
			size_t len = end - p;
			if (len >= MAX_PATH_LEN)
				return -1;
			memcpy(buf, p, len);
			buf[len] = '\0';
			p = end;
			goto set_path;
		}

		/* bare path (until whitespace) */
		end = p;
		while (*end && !isspace((unsigned char)*end))
			end++;
		size_t len = end - p;
		if (len >= MAX_PATH_LEN)
			return -1;
		memcpy(buf, p, len);
		buf[len] = '\0';
		p = end;

	set_path:
		/* check for /** suffix => prefix match */
		size_t blen = strlen(buf);
		if (blen >= 3 && strcmp(buf + blen - 3, "/**") == 0) {
			buf[blen - 3] = '\0';
			rule->match_type = MATCH_PREFIX;
		} else {
			rule->match_type = MATCH_EXACT;
		}
		strncpy(rule->path, buf, MAX_PATH_LEN - 1);
		rule->path[MAX_PATH_LEN - 1] = '\0';
	}

	if (rule->path[0] == '\0')
		return -1; /* no path specified */

	return 0;
}

int load_perm_config(PermConfig *config, const char *path)
{
	FILE *fp;
	char line[4096];
	int line_num = 0;

	memset(config, 0, sizeof(*config));
	strncpy(config->config_path, path, MAX_PATH_LEN - 1);
	config->config_path[MAX_PATH_LEN - 1] = '\0';

	fp = fopen(path, "r");
	if (!fp) {
		return -errno;
	}

	while (fgets(line, sizeof(line), fp)) {
		line_num++;

		/* strip trailing newline */
		size_t len = strlen(line);
		if (len > 0 && line[len - 1] == '\n')
			line[len - 1] = '\0';

		/* skip empty lines and comments */
		const char *p = skip_ws(line);
		if (*p == '\0' || *p == '#')
			continue;

		if (config->rule_count >= MAX_PERM_RULES) {
			fclose(fp);
			return -ENOMEM;
		}

		if (parse_perm_line(line, &config->rules[config->rule_count]) < 0) {
			/* skip malformed lines silently */
			continue;
		}

		config->rule_count++;
	}

	fclose(fp);
	return config->rule_count;
}

/* Find the most specific matching rule for the given filepath.
 * Returns true if a rule matches, false otherwise.
 * Exact matches take priority over prefix matches.
 */
bool find_perm_rule(const PermConfig *config, const char *filepath,
		    PermRule *out_rule)
{
	int i;
	const PermRule *best_exact = NULL;
	const PermRule *best_prefix = NULL;
	size_t best_prefix_len = 0;

	if (!config || !filepath || config->rule_count <= 0)
		return false;

	size_t flen = strlen(filepath);

	for (i = 0; i < config->rule_count; i++) {
		const PermRule *rule = &config->rules[i];

		if (rule->match_type == MATCH_EXACT) {
			if (strcmp(rule->path, filepath) == 0) {
				best_exact = rule;
				/* exact match found, can't get better */
				goto found;
			}
		} else {
			/* MATCH_PREFIX: check if filepath starts with rule->path/ */
			size_t plen = strlen(rule->path);
			if (plen < flen &&
			    strncmp(filepath, rule->path, plen) == 0 &&
			    filepath[plen] == '/') {
				if (plen > best_prefix_len) {
					best_prefix = rule;
					best_prefix_len = plen;
				}
			}
		}
	}

	if (best_exact) {
found:
		if (out_rule)
			*out_rule = *best_exact;
		return true;
	}

	if (best_prefix) {
		if (out_rule)
			*out_rule = *best_prefix;
		return true;
	}

	return false;
}
