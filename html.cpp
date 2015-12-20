#include "html.h"

char *replace_str(const char *str, const char *old, const char *new_str) {

	char *ret, *r;
	const char *p, *q;
	int oldlen = strlen(old);
	int count, retlen, newlen = strlen(new_str);
	int samesize = (oldlen == newlen), l;

	if (!samesize) {
		for (count = 0, p = str; (q = strstr(p, old)) != NULL; p = q + oldlen)
			count++;
		retlen = p - str + strlen(p) + count * (newlen - oldlen);
	}
	else
		retlen = strlen(str);

	if ((ret = (char *)malloc(retlen + 1)) == NULL)
		return NULL;

	r = ret, p = str;
	while (1) {
		if (!samesize && !count--)
			break;
		if ((q = strstr(p, old)) == NULL)
			break;
		l = q - p;
		memcpy(r, p, l);
		r += l;
		memcpy(r, new_str, newlen);
		r += newlen;
		p = q + oldlen;
	}
	strcpy(r, p);

	return ret;

}

char *wrap_html(char *s) {
	char *r;
	r = s;
	r = replace_str(r, "&", "&amp;");
	r = replace_str(r, "\"", "&quot;");
	r = replace_str(r, "<", "&lt;");
	r = replace_str(r, ">", "&gt;");
	r = replace_str(r, "\r\n", "\n");
	r = replace_str(r, "\n", "<br />");
	return r;
}