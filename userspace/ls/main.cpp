#include <BAN/Sort.h>
#include <BAN/String.h>
#include <BAN/Time.h>
#include <BAN/Vector.h>

#include <dirent.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <sys/stat.h>

struct config_t
{
	bool list		= false;
	bool all		= false;
	bool directory	= false;
};

struct simple_entry_t
{
	BAN::String name;
	struct stat st;
};

struct full_entry_t
{
	BAN::String access;
	BAN::String hard_links;
	BAN::String owner_name;
	BAN::String owner_group;
	BAN::String size;
	BAN::String month;
	BAN::String day;
	BAN::String time;
	BAN::String full_name;
};

const char* entry_color(mode_t mode)
{
	// TODO: handle suid, sgid, sticky

	if (S_ISFIFO(mode) || S_ISCHR(mode) || S_ISBLK(mode))
		return "\e[33m";
	if (S_ISDIR(mode))
		return "\e[34m";
	if (S_ISSOCK(mode))
		return "\e[35m";
	if (S_ISLNK(mode))
		return "\e[36m";
	if (mode & (S_IXUSR | S_IXGRP | S_IXOTH))
		return "\e[32m";
	return "\e[0m";
}

BAN::String build_access_string(mode_t mode)
{
	BAN::String access;
	MUST(access.resize(10));
	access[0] = S_ISBLK(mode) ? 'b' : S_ISCHR(mode) ? 'c' : S_ISDIR(mode) ? 'd' : S_ISFIFO(mode) ? 'f' : S_ISLNK(mode) ? 'l' : S_ISSOCK(mode) ? 's' : '-';
	access[1] = (mode & S_IRUSR) ? 'r' : '-';
	access[2] = (mode & S_IWUSR) ? 'w' : '-';
	access[3] = (mode & S_ISUID) ? ((mode & S_IXUSR) ? 's' : 'S') : (mode & S_IXUSR) ? 'x' : '-';
	access[4] = (mode & S_IRGRP) ? 'r' : '-';
	access[5] = (mode & S_IWGRP) ? 'w' : '-';
	access[6] = (mode & S_ISGID) ? ((mode & S_IXGRP) ? 's' : 'S') : (mode & S_IXGRP) ? 'x' : '-';
	access[7] = (mode & S_IROTH) ? 'r' : '-';
	access[8] = (mode & S_IWOTH) ? 'w' : '-';
	access[9] = (mode & S_ISVTX) ? ((mode & S_IXOTH) ? 't' : 'T') : (mode & S_IXOTH) ? 'x' : '-';
	return access;
}

BAN::String build_hard_links_string(nlink_t links)
{
	return BAN::String::formatted("{}", links);
}

BAN::String build_owner_name_string(uid_t uid)
{
	struct passwd* passwd = getpwuid(uid);
	if (passwd == nullptr)
		return BAN::String::formatted("{}", uid);
	return BAN::String(BAN::StringView(passwd->pw_name));
}

BAN::String build_owner_group_string(gid_t gid)
{
	return BAN::String::formatted("{}", gid);
}

BAN::String build_size_string(off_t size)
{
	return BAN::String::formatted("{}", size);
}

BAN::String build_month_string(BAN::Time time)
{
	static const char* months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	return BAN::String(BAN::StringView(months[(time.month - 1) % 12]));
}

BAN::String build_day_string(BAN::Time time)
{
	return BAN::String::formatted("{}", time.day);
}

BAN::String build_time_string(BAN::Time time)
{
	static uint32_t current_year = ({ timespec real_time; clock_gettime(CLOCK_REALTIME, &real_time); BAN::from_unix_time(real_time.tv_sec).year; });
	if (time.year != current_year)
		return BAN::String::formatted("{}", time.year);
	return BAN::String::formatted("{2}:{2}", time.hour, time.minute);
}

int list_directory(const BAN::String& path, config_t config)
{
	BAN::Vector<simple_entry_t> entries;

	struct stat st;

	auto stat_func = config.directory ? lstat : stat;
	if (stat_func(path.data(), &st) == -1)
	{
		perror("stat");
		return 2;
	}

	int ret = 0;

	if (!S_ISDIR(st.st_mode))
		MUST(entries.emplace_back(path, st));
	else
	{
		DIR* dirp = opendir(path.data());
		if (dirp == NULL)
		{
			perror("opendir");
			return 2;
		}

		struct dirent* dirent;
		while ((dirent = readdir(dirp)))
		{
			if (!config.all && dirent->d_name[0] == '.')
				continue;

			if (fstatat(dirfd(dirp), dirent->d_name, &st, AT_SYMLINK_NOFOLLOW) == -1)
			{
				perror("fstatat");
				ret = 1;
				continue;
			}

			MUST(entries.emplace_back(BAN::StringView(dirent->d_name), st));
		}

		closedir(dirp);
	}

	BAN::sort::sort(entries.begin(), entries.end(),
		[](const simple_entry_t& lhs, const simple_entry_t& rhs)
		{
			// sort directories first
			bool lhs_isdir = S_ISDIR(lhs.st.st_mode);
			bool rhs_isdir = S_ISDIR(rhs.st.st_mode);
			if (lhs_isdir != rhs_isdir)
				return lhs_isdir;

			// sort by name
			for (size_t i = 0; i < BAN::Math::min(lhs.name.size(), rhs.name.size()); i++)
				if (lhs.name[i] != rhs.name[i])
					return lhs.name[i] < rhs.name[i];
			return lhs.name.size() < rhs.name.size();
		}
	);

	if (!config.list)
	{
		for (size_t i = 0; i < entries.size(); i++)
		{
			if (i > 0)
				printf(" ");
			printf("%s%s\e[m", entry_color(entries[i].st.st_mode), entries[i].name.data());
		}
		printf("\n");
		return ret;
	}

	BAN::Vector<full_entry_t> full_entries;
	MUST(full_entries.reserve(entries.size()));

	full_entry_t max_entry;
	for (const simple_entry_t& entry : entries)
	{
		full_entry_t full_entry;

#define GET_ENTRY_STRING(property, input)							\
	full_entry.property = build_ ## property ## _string(input);		\
	if (full_entry.property.size() > max_entry.property.size())		\
		max_entry.property = full_entry.property;

		GET_ENTRY_STRING(access,		entry.st.st_mode);
		GET_ENTRY_STRING(hard_links,	entry.st.st_nlink);
		GET_ENTRY_STRING(owner_name,	entry.st.st_uid);
		GET_ENTRY_STRING(owner_group,	entry.st.st_gid);
		GET_ENTRY_STRING(size,			entry.st.st_size);

		BAN::Time time = BAN::from_unix_time(entry.st.st_mtim.tv_sec);
		GET_ENTRY_STRING(month,	time);
		GET_ENTRY_STRING(day,	time);
		GET_ENTRY_STRING(time,	time);

		full_entry.full_name = BAN::String::formatted("{}{}\e[m", entry_color(entry.st.st_mode), entry.name);

		MUST(full_entries.push_back(BAN::move(full_entry)));
	}

	for (const auto& full_entry : full_entries)
		printf("%*s %*s %*s %*s %*s %*s %*s %*s %s\n",
			(int)max_entry.access.size(),		full_entry.access.data(),
			(int)max_entry.hard_links.size(),	full_entry.hard_links.data(),
			(int)max_entry.owner_name.size(),	full_entry.owner_name.data(),
			(int)max_entry.owner_group.size(),	full_entry.owner_group.data(),
			(int)max_entry.size.size(),			full_entry.size.data(),
			(int)max_entry.month.size(),		full_entry.month.data(),
			(int)max_entry.day.size(),			full_entry.day.data(),
			(int)max_entry.time.size(),			full_entry.time.data(),
												full_entry.full_name.data()
		);

	return ret;
}

int usage(const char* argv0, int ret)
{
	FILE* fout = ret ? stderr : stdout;
	fprintf(fout, "usage: %s [OPTION]... [FILE]...\n", argv0);
	fprintf(fout, "  -a, --all        show hidden files\n");
	fprintf(fout, "  -l, --list       use list format\n");
	fprintf(fout, "  -d, --directory  show directories as directories, don't list their contents\n");
	fprintf(fout, "  -h, --help       show this message and exit\n");
	return ret;
}

int main(int argc, const char* argv[])
{
	config_t config;

	int i = 1;
	for (; i < argc; i++)
	{
		if (argv[i][0] != '-')
			break;
		if (argv[i][1] == '\0')
			break;

		if (argv[i][1] == '-')
		{
			if (strcmp(argv[i], "--help") == 0)
				return usage(argv[0], 0);
			else if (strcmp(argv[i], "--all") == 0)
				config.all = true;
			else if (strcmp(argv[i], "--list") == 0)
				config.list = true;
			else if (strcmp(argv[i], "--directory") == 0)
				config.directory = true;
			else
			{
				fprintf(stderr, "unrecognized option '%s'\n", argv[i]);
				return usage(argv[0], 2);
			}
		}
		else
		{
			for (size_t j = 1; argv[i][j]; j++)
			{
				if (argv[i][j] == 'h')
					return usage(argv[0], 0);
				else if (argv[i][j] == 'a')
					config.all = true;
				else if (argv[i][j] == 'l')
					config.list = true;
				else if (argv[i][j] == 'd')
					config.directory = true;
				else
				{
					fprintf(stderr, "unrecognized option '%c'\n", argv[i][j]);
					return usage(argv[0], 2);
				}
			}
		}
	}

	BAN::Vector<BAN::String> files;

	if (i == argc)
		MUST(files.emplace_back("."sv));
	else for (; i < argc; i++)
		MUST(files.emplace_back(BAN::StringView(argv[i])));

	int ret = 0;
	for (size_t i = 0; i < files.size(); i++)
	{
		if (i > 0)
			printf("\n");
		if (files.size() > 1)
			printf("%s:\n", files[i].data());
		ret = BAN::Math::max(ret, list_directory(files[i], config));
	}

	return ret;
}
