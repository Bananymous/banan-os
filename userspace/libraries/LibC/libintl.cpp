#include <langinfo.h>
#include <libintl.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>

struct text_domain_t
{
	char* domain_name;
	char* codeset;
	char* dir_name;
};

static text_domain_t* s_text_domains = nullptr;
static size_t s_text_domain_count = 0;
static char* s_current_domain = nullptr;

static text_domain_t* find_text_domain(const char* domainname)
{
	for (size_t i = 0; i < s_text_domain_count; i++)
	{
		if (strcmp(s_text_domains[i].domain_name, domainname) != 0)
			continue;
		return &s_text_domains[i];
	}

	return nullptr;
}

static text_domain_t* bind_new_textdomain(const char* domainname, const char* codeset, const char* dirname)
{
	auto* new_text_domains = static_cast<text_domain_t*>(realloc(s_text_domains, (s_text_domain_count + 1) * sizeof(text_domain_t)));
	if (new_text_domains == nullptr)
		return nullptr;
	s_text_domains = new_text_domains;

	auto& new_text_domain = new_text_domains[s_text_domain_count];

	new_text_domain.domain_name = strdup(domainname);
	new_text_domain.dir_name = strdup(dirname);
	new_text_domain.codeset = strdup(codeset);

	if (!new_text_domain.domain_name || !new_text_domain.codeset || !new_text_domain.dir_name)
	{
		if (new_text_domain.domain_name)
			free(new_text_domain.domain_name);
		if (new_text_domain.codeset)
			free(new_text_domain.codeset);
		if (new_text_domain.dir_name)
			free(new_text_domain.dir_name);
		return nullptr;
	}

	s_text_domain_count++;

	return &new_text_domain;
}

char* bindtextdomain(const char* domainname, const char* dirname)
{
	if (domainname == nullptr || *domainname == '\0')
		return nullptr;

	auto* text_domain = find_text_domain(domainname);

	if (dirname == nullptr)
	{
		if (text_domain != nullptr)
			return text_domain->dir_name;
		return const_cast<char*>("");
	}

	if (text_domain)
	{
		char* dirname_copy = strdup(dirname);
		if (dirname_copy == nullptr)
			return nullptr;

		if (text_domain->dir_name)
			free(text_domain->dir_name);

		text_domain->dir_name = dirname_copy;
		return text_domain->dir_name;
	}

	auto* new_current_domain = bind_new_textdomain(domainname, nl_langinfo(CODESET), dirname);
	if (new_current_domain == nullptr)
		return nullptr;
	return new_current_domain->domain_name;
}

char* bind_textdomain_codeset(const char* domainname, const char* codeset)
{
	if (domainname == nullptr || *domainname == '\0')
		return nullptr;

	auto* text_domain = find_text_domain(domainname);

	if (codeset == nullptr || *codeset == '\0')
	{
		if (text_domain == nullptr)
			return nl_langinfo(CODESET);
		return text_domain->codeset;
	}

	if (text_domain)
	{
		char* codeset_copy = strdup(codeset);
		if (codeset_copy == nullptr)
			return nullptr;

		free(text_domain->codeset);
		text_domain->codeset = codeset_copy;
		return codeset_copy;
	}

	auto* new_text_domain = bind_new_textdomain(domainname, codeset, "");
	if (new_text_domain == nullptr)
		return nullptr;
	return new_text_domain->codeset;
}

char* textdomain(const char* domainname)
{
	if (domainname == nullptr || *domainname == '\0')
	{
		if (s_current_domain == nullptr)
			s_current_domain = strdup("messages");
		return s_current_domain;
	}

	char* new_current_domain = strdup(domainname);
	if (new_current_domain == nullptr)
		return nullptr;

	if (s_current_domain != nullptr)
		free(s_current_domain);

	s_current_domain = new_current_domain;
	return s_current_domain;
}

char* dgettext(const char* domainname, const char* msgid)
{
	return dgettext_l(domainname, msgid, __getlocale(LC_MESSAGES));
}

char* dgettext_l(const char* domainname, const char* msgid, locale_t locale)
{
	return dngettext_l(domainname, msgid, nullptr, 1, locale);
}

char* dcgettext(const char* domainname, const char* msgid, int category)
{
	return dcgettext_l(domainname, msgid, category, __getlocale(LC_MESSAGES));
}

char* dcgettext_l(const char* domainname, const char* msgid, int category, locale_t locale)
{
	return dcngettext_l(domainname, msgid, nullptr, 1, category, locale);
}

char* dngettext(const char* domainname, const char* msgid, const char* msgid_plural, unsigned long int n)
{
	return dngettext_l(domainname, msgid, msgid_plural, n, __getlocale(LC_MESSAGES));
}

char* dngettext_l(const char* domainname, const char* msgid, const char* msgid_plural, unsigned long int n, locale_t locale)
{
	return dcngettext_l(domainname, msgid, msgid_plural, n, 0, locale);
}

char* dcngettext(const char* domainname, const char* msgid, const char* msgid_plural, unsigned long int n, int category)
{
	return dcngettext_l(domainname, msgid, msgid_plural, n, category, __getlocale(LC_MESSAGES));
}

char* dcngettext_l(const char* domainname, const char* msgid, const char* msgid_plural, unsigned long int n, int category, locale_t locale)
{
	// TODO: do actual translations :D
	// FIXME: handle domain codeset

	(void)locale;
	(void)domainname;
	(void)category;
	return const_cast<char*>((n == 1) ? msgid : msgid_plural);
}

char* gettext(const char* msgid)
{
	return gettext_l(msgid, __getlocale(LC_MESSAGES));
}

char* gettext_l(const char* msgid, locale_t locale)
{
	return dgettext_l(s_current_domain, msgid, locale);
}

char* ngettext(const char* msgid, const char* msgid_plural, unsigned long int n)
{
	return ngettext_l(msgid, msgid_plural, n, __getlocale(LC_MESSAGES));
}

char* ngettext_l(const char* msgid, const char* msgid_plural, unsigned long int n, locale_t locale)
{
	return dngettext_l(s_current_domain, msgid, msgid_plural, n, locale);
}
