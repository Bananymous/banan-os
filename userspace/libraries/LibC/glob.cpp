#include <dirent.h>
#include <errno.h>
#include <fnmatch.h>
#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

int do_glob(const char* __restrict path, const char* __restrict pattern, size_t pattern_index, int flags, int (*errfunc)(const char* epath, int eerrno), glob_t* __restrict pglob)
{
#define DO_GLOB_RETURN(code) do { error_code = code; goto do_glob_error; } while (false)
	int error_code = 0;

	size_t index_s = pattern_index;
	size_t index_e = pattern_index;
	size_t index = pattern_index;

	if (pattern[index] == '/')
		index++;

	bool needs_fnmatch = false;
	while (pattern[index])
	{
		if (!(flags & GLOB_NOESCAPE) && pattern[index] == '\\')
		{
			index++;
			if (pattern[index] == '\0')
				break;
			continue;
		}

		if (pattern[index] == '/')
		{
			if (needs_fnmatch)
				break;
			index_e = index;
		}

		if (pattern[index] == '?' || pattern[index] == '[' || pattern[index] == '*')
			needs_fnmatch = true;

		index++;
	}

	if (!needs_fnmatch)
		index_e = index;

	const size_t path_buf_len = strlen(path) + (index_e - index_s) + 1;
	char* path_buf = static_cast<char*>(malloc(path_buf_len + 1));

	char* subpattern = static_cast<char*>(malloc(index + 1));

	DIR* dirp = nullptr;
	char* dent_path = nullptr;
	const char* dir_path = path_buf;

	if (path_buf == nullptr || subpattern == nullptr)
		DO_GLOB_RETURN(GLOB_NOSPACE);

	sprintf(path_buf, "%s%.*s",
		path,
		static_cast<int>(index_e - index_s),
		pattern + index_s
	);

	sprintf(subpattern, "%.*s",
		static_cast<int>(index),
		pattern
	);

	if (!needs_fnmatch)
	{
		struct stat st;
		if (stat(path_buf, &st) == -1)
		{
			if ((flags & GLOB_ERR) || (errfunc && errfunc(path_buf, errno)))
				DO_GLOB_RETURN(GLOB_ABORTED);
			DO_GLOB_RETURN(0);
		}

		if ((flags & GLOB_MARK) && S_ISDIR(st.st_mode))
			strcat(path_buf, "/");

		void* new_pathv = realloc(pglob->gl_pathv, sizeof(char*) * (pglob->gl_offs + pglob->gl_pathc + 2));
		if (new_pathv == nullptr)
			DO_GLOB_RETURN(GLOB_NOSPACE);
		pglob->gl_pathv = static_cast<char**>(new_pathv);
		pglob->gl_pathv[pglob->gl_offs + pglob->gl_pathc] = path_buf;
		pglob->gl_pathc++;

		free(subpattern);

		return 0;
	}

	if (dir_path[0] == '\0')
		dir_path = (pattern[0] == '/') ? "/" : ".";
	if ((dirp = opendir(dir_path)) == nullptr)
	{
		if ((flags & GLOB_ERR) || (errfunc && errfunc(path_buf, errno)))
			DO_GLOB_RETURN(GLOB_ABORTED);
		DO_GLOB_RETURN(0);
	}

	for (;;)
	{
		errno = 0;
		dirent* dent = readdir(dirp);
		if (dent == nullptr)
		{
			if ((flags & GLOB_ERR) || (errfunc && errfunc(path_buf, errno)))
				DO_GLOB_RETURN(GLOB_ABORTED);
			DO_GLOB_RETURN(0);
		}

		if (strcmp(dent->d_name, ".") == 0 || strcmp(dent->d_name, "..") == 0)
			continue;

		if ((dent_path = static_cast<char*>(malloc(path_buf_len + 1 + strlen(dent->d_name) + 1))) == nullptr)
			DO_GLOB_RETURN(GLOB_NOSPACE);

		if (path_buf[0] || pattern[0] == '/')
			sprintf(dent_path, "%s/%s", path_buf, dent->d_name);
		else
			strcpy(dent_path, dent->d_name);

		int fnmatch_flags = FNM_PATHNAME | FNM_PERIOD;
		if (flags & GLOB_NOESCAPE)
			fnmatch_flags |= FNM_NOESCAPE;

		if (fnmatch(subpattern, dent_path, fnmatch_flags) == 0)
			if (const int result = do_glob(dent_path, pattern, index, flags, errfunc, pglob))
				DO_GLOB_RETURN(result);

		free(dent_path);
		dent_path = nullptr;
	}

do_glob_error:
	if (dirp)
		closedir(dirp);
	if (path_buf)
		free(path_buf);
	if (subpattern)
		free(subpattern);
	if (dent_path)
		free(dent_path);
	return error_code;
#undef DO_GLOB_ERROR
}

int glob(const char* __restrict pattern, int flags, int (*errfunc)(const char* epath, int eerrno), glob_t* __restrict pglob)
{
	if (!(flags & GLOB_APPEND))
	{
		pglob->gl_pathv = nullptr;
		pglob->gl_pathc = 0;

		if (!(flags & GLOB_DOOFFS))
			pglob->gl_offs = 0;

		pglob->gl_pathv = static_cast<char**>(malloc(sizeof(char*) * (pglob->gl_offs + 1)));
		if (pglob->gl_pathv == nullptr)
			return GLOB_NOSPACE;
		for (size_t i = 0; i < pglob->gl_offs; i++)
			pglob->gl_pathv[i] = nullptr;
	}

	const size_t start_off = pglob->gl_offs + pglob->gl_pathc;

	const int result = do_glob("", pattern, 0, flags, errfunc, pglob);

	const size_t end_off = pglob->gl_offs + pglob->gl_pathc;

	if (!(flags & GLOB_NOSORT) && end_off != start_off)
	{
		qsort(
			pglob->gl_pathv + start_off,
			end_off - start_off,
			sizeof(char*),
			[](const void* a, const void* b) -> int
			{
				const char* a_cstr = *static_cast<char* const*>(a);
				const char* b_cstr = *static_cast<char* const*>(b);
				return strcoll(a_cstr, b_cstr);
			}
		);
	}

	pglob->gl_pathv[pglob->gl_offs + pglob->gl_pathc] = nullptr;

	if (result != 0)
		return result;
	if (end_off != start_off || !(flags & GLOB_NOCHECK))
		return 0;

	void* new_pathv = realloc(pglob->gl_pathv, sizeof(char*) * (pglob->gl_offs + pglob->gl_pathc + 2));
	if (new_pathv == nullptr)
		return GLOB_NOSPACE;

	pglob->gl_pathv = static_cast<char**>(new_pathv);
	pglob->gl_pathv[pglob->gl_offs] = strdup(pattern);
	if (pglob->gl_pathv[pglob->gl_offs] == nullptr)
		return GLOB_NOSPACE;
	pglob->gl_pathc = 1;
	return 0;
}

void globfree(glob_t* pglob)
{
	for (size_t i = 0; i < pglob->gl_pathc; i++)
		free(pglob->gl_pathv[i + pglob->gl_offs]);
	free(pglob->gl_pathv);

	pglob->gl_pathv = nullptr;
}
