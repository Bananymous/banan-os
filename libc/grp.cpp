#include <BAN/StringView.h>
#include <BAN/Vector.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>

static struct stat s_group_st;
static char* s_group_mmap = nullptr;
static int s_group_fd = -1;

static struct group s_group;

id_t parse_id(BAN::StringView string)
{
	id_t id = 0;
	for (char c : string)
	{
		if (!isdigit(c))
			return -1;
		id = (id * 10) + (c - '0');
	}
	return id;
}

static bool open_group_file()
{
	if (s_group_fd == -1)
		s_group_fd = open("/etc/group", O_RDONLY);
	if (s_group_fd == -1)
		return false;

	if (fstat(s_group_fd, &s_group_st) == -1)
		return false;

	if (s_group_mmap == nullptr || s_group_mmap == MAP_FAILED)
		s_group_mmap = (char*)mmap(nullptr, s_group_st.st_size, PROT_READ, MAP_PRIVATE, s_group_fd, 0);
	if (s_group_mmap == MAP_FAILED)
		return false;

	s_group.gr_name = nullptr;
	s_group.gr_mem = nullptr;

	return true;
}

struct group* fill_group(const BAN::Vector<BAN::StringView>& parts)
{
	if (parts.size() != 4)
		return nullptr;

	if (s_group.gr_name)
	{
		free(s_group.gr_name);
		s_group.gr_name = nullptr;
	}

	if (s_group.gr_mem)
	{
		for (size_t i = 0; s_group.gr_mem && s_group.gr_mem[i]; i++)
			free(s_group.gr_mem[i]);
		free(s_group.gr_mem);
		s_group.gr_mem = nullptr;
	}

	auto groups_or_error = parts[3].split(',');	
	if (groups_or_error.is_error())
		return nullptr;
	auto groups = groups_or_error.release_value();

	s_group.gr_gid = parse_id(parts[2]);
	if (s_group.gr_gid == -1)
		return nullptr;

	s_group.gr_name = (char*)malloc(parts[0].size() + 1);
	if (s_group.gr_name == nullptr)
		return nullptr;
	memcpy(s_group.gr_name, parts[0].data(), parts[0].size());	
	s_group.gr_name[parts[0].size()] = '\0';

	s_group.gr_mem = (char**)malloc((groups.size() + 1) * sizeof(char*));
	if (s_group.gr_mem == nullptr)
		return nullptr;

	for (size_t i = 0; i < groups.size(); i++)
	{
		s_group.gr_mem[i] = (char*)malloc(groups[i].size() + 1);
		if (s_group.gr_mem[i] == nullptr)
		{
			for (size_t j = 0; j < i; j++)
				free(s_group.gr_mem[j]);
			free(s_group.gr_mem);
			s_group.gr_mem = nullptr;
			return nullptr;
		}
		memcpy(s_group.gr_mem[i], groups[i].data(), groups[i].size());
		s_group.gr_mem[i][groups[i].size()] = '\0';
	}	
	s_group.gr_mem[groups.size()] = nullptr;

	return &s_group;
}

struct group* getgrnam(const char* name)
{
	if (s_group_mmap == nullptr || s_group_mmap == MAP_FAILED)
		if (!open_group_file())
			return nullptr;
	
	off_t start = 0;
	off_t end = 0;
	while (start < s_group_st.st_size)
	{
		while (end < s_group_st.st_size && s_group_mmap[end] != '\n')
			end++;

		BAN::StringView line(s_group_mmap + start, end - start);
		start = ++end;

		auto parts_or_error = line.split(':', true);
		if (parts_or_error.is_error())
			return nullptr;

		auto parts = parts_or_error.release_value();
		if (parts.size() == 4 && parts[0] == name)
			return fill_group(parts);
	}

	return nullptr;
}

struct group* getgrgid(gid_t gid)
{
	if (s_group_mmap == nullptr || s_group_mmap == MAP_FAILED)
		if (!open_group_file())
			return nullptr;
	
	off_t start = 0;
	off_t end = 0;
	while (start < s_group_st.st_size)
	{
		while (end < s_group_st.st_size && s_group_mmap[end] != '\n')
			end++;

		BAN::StringView line(s_group_mmap + start, end - start);
		start = ++end;

		auto parts_or_error = line.split(':', true);
		if (parts_or_error.is_error())
			return nullptr;

		auto parts = parts_or_error.release_value();
		if (parts.size() == 4 && parse_id(parts[2]) == gid)
			return fill_group(parts);
	}

	return nullptr;
}
