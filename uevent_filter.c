#include "afrd.h"
#include "uevent_filter.h"

const char *spaces = " \t\r\n";

void strip_trailing_spaces (char *eol, const char *start)
{
	while (eol > start) {
		eol--;
		if (strchr (spaces, *eol) == NULL) {
			eol++;
			break;
		}
	}

	*eol = 0;
}

static bool append_rex (uevent_filter_t *uevf, char *str)
{
	if (uevf->size >= ARRAY_SIZE (uevf->attr))
		return false;

	str += strspn (str, spaces);
	if (!*str)
		return false;

	char *eq = strchr (str, '=');
	if (!eq)
		return false;

	trace (2, "\t+ %s\n", str);

	strip_trailing_spaces (eq, str);
	eq++;
	eq += strspn (eq, spaces);

	strip_trailing_spaces (strchr (eq, 0), eq);

	uevf->attr [uevf->size] = str;
	if (regcomp (&uevf->rex [uevf->size], eq, REG_EXTENDED) != 0) {
		trace (1, "\t  ignoring bad regex: %s\n", eq);
		return false;
	}

	uevf->size++;
	return true;
}

bool uevent_filter_init (uevent_filter_t *uevf, const char *name, const char *filter)
{
	memset (uevf, 0, sizeof (*uevf));

	uevf->name = strdup (name);
	uevf->filter = strdup (filter);
	char *cur = uevf->filter;
	while (*cur) {
		char *comma = strchr (cur, ',');
		bool last = !comma;
		if (last)
			comma = strchr (cur, 0);

		strip_trailing_spaces (comma, cur);
		append_rex (uevf, cur);

		if (last)
			break;

		cur = comma + 1;
	}

	return (uevf->size > 0);
}

void uevent_filter_fini (uevent_filter_t *uevf)
{
	if (uevf->filter)
		free (uevf->filter);

	for (int i = 0; i < uevf->size; i++)
		regfree (&uevf->rex [i]);

	memset (uevf, 0, sizeof (*uevf));
}

bool uevent_filter_load (uevent_filter_t *uevf, const char *kw)
{
	const char *val = cfg_get_str (kw, NULL);
	if (!val)
		return false;

	trace (1, "\tloading filter %s:\n", kw);
	return uevent_filter_init (uevf, kw, val);
}

void uevent_filter_reset (uevent_filter_t *uevf)
{
	uevf->matches = 0;
}

bool uevent_filter_match (uevent_filter_t *uevf, const char *attr, const char *value)
{
	for (int i = 0; i < uevf->size; i++)
		if (strcmp (uevf->attr [i], attr) == 0) {
			regmatch_t match [1];
			if (regexec (&uevf->rex [i], value, 1, match, 0) == REG_NOMATCH)
				continue;
			// must match whole line
			if (match [0].rm_so != 0 || match [0].rm_eo != strlen (value))
				continue;

			trace (3, "\t  matched filter %s\n", uevf->name);

			uevf->matches++;
			return true;
		}

	return false;
}

bool uevent_filter_matched (uevent_filter_t *uevf)
{
	return uevf->size && (uevf->matches == uevf->size);
}
