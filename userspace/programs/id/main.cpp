#include <assert.h>
#include <getopt.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

static const char* s_argv0 { nullptr };

enum class cb_id_t
{
	any,
	uid,
	gid,
	euid,
	egid,
	sgid,
};

static bool show_id(uid_t uid, uid_t euid, gid_t gid, gid_t egid, void (*callback)(cb_id_t, id_t, const char*, void*), void* callback_arg)
{
	passwd dpwd;
	const auto* pwd = getpwuid(uid);
	const auto* epwd = (uid == euid) ? pwd : ({ dpwd = *pwd; pwd = &dpwd; getpwuid(euid); });
	if (pwd == nullptr || epwd == nullptr)
	{
		fprintf(stderr, "%s: %u: no such user\n", s_argv0, pwd ? euid : uid);
		return false;
	}

	group dgrp;
	const auto* grp = getgrgid(gid);
	const auto* egrp = (gid == egid) ? grp : ({ dgrp = *grp; grp = &dgrp; getgrgid(egid); });
	if (grp == nullptr || egrp == nullptr)
	{
		fprintf(stderr, "%s: %u: no such group\n", s_argv0, grp ? egid : gid);
		return false;
	}

	callback(cb_id_t::uid, uid,   pwd->pw_name,  callback_arg);
	callback(cb_id_t::gid, gid,   grp->gr_name,  callback_arg);
	callback(cb_id_t::euid, euid, epwd->pw_name, callback_arg);
	callback(cb_id_t::egid, egid, egrp->gr_name, callback_arg);

	setgrent();
	while (const auto* sgrp = getgrent())
	{
		bool has_user { false };
		for (size_t i = 0; sgrp->gr_mem[i] && !has_user; i++)
			has_user = (strcmp(sgrp->gr_mem[i], pwd->pw_name) == 0 || strcmp(sgrp->gr_mem[i], epwd->pw_name) == 0);
		if (!has_user)
			continue;
		callback(cb_id_t::sgid, sgrp->gr_gid, sgrp->gr_name, callback_arg);
	}

	return true;
}

int main(int argc, char** argv)
{
	s_argv0 = argv[0];

	bool opt_G { false };
	bool opt_g { false };
	bool opt_n { false };
	bool opt_r { false };
	bool opt_u { false };

	for (;;)
	{
		static option long_options[] {
			{ "groups", no_argument, nullptr, 'G' },
			{ "group",  no_argument, nullptr, 'g' },
			{ "name",   no_argument, nullptr, 'n' },
			{ "real",   no_argument, nullptr, 'r' },
			{ "user",   no_argument, nullptr, 'u' },
			{ "help",   no_argument, nullptr,  0  },
			{}
		};

		int ch = getopt_long(argc, argv, "Ggnru", long_options, nullptr);
		if (ch == -1)
			break;

		switch (ch)
		{
			case 'G': opt_G = true; break;
			case 'g': opt_g = true; break;
			case 'n': opt_n = true; break;
			case 'r': opt_r = true; break;
			case 'u': opt_u = true; break;
			case 0:
				fprintf(stderr, "usage: %s [OPTION]... [USER]...\n", argv[0]);
				fprintf(stderr, "  show information for every USER or current user when omitted\n");
				fprintf(stderr, "OPTIONS:\n");
				fprintf(stderr, "  -G, --groups  output all different group IDs\n");
				fprintf(stderr, "  -g, --group   output only the effective group ID\n");
				fprintf(stderr, "  -n, --name    output the name instead of the numeric ID\n");
				fprintf(stderr, "  -r, --real    output the real ID instead of the effective ID\n");
				fprintf(stderr, "  -u, --user    output only the effective user ID\n");
				fprintf(stderr, "      --help    show this message and exit\n");
				return 0;
			case ':' : case '?':
				fprintf(stderr, "see '%s --help' for usage\n", argv[0]);
				return 1;
		}
	}

	if (opt_G + opt_g + opt_u > 1)
	{
		fprintf(stderr, "%s: only one of -G, -g, -u can be specified\n", s_argv0);
		return 1;
	}

	struct callback_info
	{
		cb_id_t wanted;
		uid_t uid;
		gid_t gid;
		bool print_name;
		bool first;
	};

	const auto callback = [](cb_id_t type, id_t id, const char* name, void* info_) {
		auto& info = *static_cast<callback_info*>(info_);
		if (info.wanted != cb_id_t::any)
		{
			bool should_print { info.wanted == type };

			if (info.wanted == cb_id_t::sgid && (type == cb_id_t::gid || type == cb_id_t::egid))
				should_print = true;
			if (type == cb_id_t::gid)
				info.gid = id;
			if (type == cb_id_t::egid && info.gid == id)
				should_print = false;

			if (should_print)
			{
				if (!info.first)
					printf(" ");
				if (info.print_name)
					printf("%s", name);
				else
					printf("%u", id);
				info.first = false;
			}
		}
		else switch (type)
		{
			case cb_id_t::any:
				assert(false);
			case cb_id_t::uid:
				printf("uid=%u(%s)", id, name);
				info.uid = id;
				break;
			case cb_id_t::gid:
				printf(" gid=%u(%s)", id, name);
				info.gid = id;
				break;
			case cb_id_t::euid:
				if (info.uid != id)
					printf(" euid=%u(%s)", id, name);
				break;
			case cb_id_t::egid:
				if (info.gid != id)
					printf(" egid=%u(%s)", id, name);
				break;
			case cb_id_t::sgid:
				printf(info.first ? " groups=" : ",");
				printf("%u(%s)", id, name);
				info.first = false;
				break;
		}
	};

	const callback_info cb_info {
		.wanted =
			opt_G ? cb_id_t::sgid :
			opt_g ? (opt_r ? cb_id_t::gid : cb_id_t::egid) :
			opt_u ? (opt_r ? cb_id_t::uid : cb_id_t::euid) :
			cb_id_t::any,
		.uid = -1,
		.gid = -1,
		.print_name = opt_n,
		.first = true,
	};

	int ret = 0;

	if (optind >= argc)
	{
		auto info { cb_info };
		if (show_id(getuid(), geteuid(), getgid(), getegid(), callback, &info))
			printf("\n");
		else
			ret = 1;
	}
	else for (int i = optind; i < argc; i++)
	{
		const auto* pwd = getpwnam(argv[i]);
		if (pwd == nullptr)
		{
			fprintf(stderr, "%s: '%s': no such user\n", s_argv0, argv[i]);
			ret = 1;
			continue;
		}

		auto info { cb_info };
		if (show_id(pwd->pw_uid, pwd->pw_uid, pwd->pw_gid, pwd->pw_gid, callback, &info))
			printf("\n");
		else
			ret = 1;
	}

	return ret;
}
