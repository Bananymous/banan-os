#include <BAN/IPv4.h>
#include <BAN/String.h>
#include <BAN/Vector.h>

#include <arpa/inet.h>
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <stropts.h>
#include <sys/banan-os.h>
#include <sys/framebuffer.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <unistd.h>

#define COLOR_ON "\e[33m"
#define COLOR_OFF "\e[m"

static constexpr const char* s_banana_art1[] {
	"                X. ",
	"               :;  ",
	"              .:;  ",
	"              ::::.",
	"            .::::;.",
	".;.      ..::::;;; ",
	";:::;::::::::;;;.  ",
	" x;;;;;;;;;;;;.    ",
	"    ..x+;..        "
};
static constexpr size_t s_banana_art1_width = 19;
static constexpr size_t s_banana_art1_height = 9;

static constexpr const char* s_banana_art2[] {
	"                Z¨ ",
	"               )r  ",
	"              `¬ï  ",
	"              «/¿7`",
	"            `»!}[ì`",
	"”<`      ``«•×}ï1= ",
	">**¿<+)//(†×¿vîr´  ",
	" U==íï<<ííîcoc´    ",
	"    ´¨kC‰·`        "
};
static constexpr size_t s_banana_art2_width = 19;
static constexpr size_t s_banana_art2_height = 9;

const char* get_cpu_manufacturer()
{
	uint32_t max_extended;
	asm volatile("cpuid" : "=a"(max_extended) : "a"(0x80000000));
	if (max_extended >= 0x80000004)
	{
		static char string[49];
		for (int i = 0; i < 3; i++)
		{
			uint32_t eax, ebx, ecx, edx;
			asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0x80000002 + i));
			memcpy(string + i * 16 +  0, &eax, 4);
			memcpy(string + i * 16 +  4, &ebx, 4);
			memcpy(string + i * 16 +  8, &ecx, 4);
			memcpy(string + i * 16 + 12, &edx, 4);
		}
		string[48] = '\0';
		return string;
	}
	else
	{
		uint32_t ebx, ecx, edx;
		asm volatile("cpuid" : "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0));

		char string[13];
		memcpy(string + 0, &ebx, 4);
		memcpy(string + 4, &edx, 4);
		memcpy(string + 8, &ecx, 4);
		string[12] = '\0';

		if (strcmp(string, "AuthenticAMD") == 0) return "AMD";
		if (strcmp(string, "CentaurHauls") == 0) return "Centaur";
		if (strcmp(string, "CyrixInstead") == 0) return "Cyrix";
		if (strcmp(string, "GenuineIntel") == 0) return "Intel";
		if (strcmp(string, "GenuineIotel") == 0) return "Intel";
		if (strcmp(string, "TransmetaCPU") == 0) return "Transmeta";
		if (strcmp(string, "GenuineTMx86") == 0) return "Transmeta";
		if (strcmp(string, "Geode by NSC") == 0) return "National Semiconductor";
		if (strcmp(string, "NexGenDriven") == 0) return "NexGen";
		if (strcmp(string, "RiseRiseRise") == 0) return "Rise";
		if (strcmp(string, "SiS SiS SiS ") == 0) return "Sis";
		if (strcmp(string, "UMC UMC UMC ") == 0) return "UMC";
		if (strcmp(string, "Vortex86 SoC") == 0) return "Vortex86";
		if (strcmp(string, "  Shanghai  ") == 0) return "Zhaoxin";
		if (strcmp(string, "HygonGenuine") == 0) return "Hygon";
		if (strcmp(string, "Genuine  RDC") == 0) return "RDC Semiconductor";
		if (strcmp(string, "E2K MACHINE ") == 0) return "MCST Elbrus";
		if (strcmp(string, "VIA VIA VIA ") == 0) return "VIA";
		if (strcmp(string, "AMD ISBETTER") == 0) return "AMD";

		if (strcmp(string, "GenuineAO486") == 0) return "ao486";
		if (strcmp(string, "MiSTer AO486") == 0) return "ao486";

		if (strcmp(string, "MicrosoftXTA") == 0) return "Microsoft x86-to-ARM";
		if (strcmp(string, "VirtualApple") == 0) return "Apple Rosetta 2";

		return "<unknown>";
	}
}

BAN::ErrorOr<BAN::Vector<BAN::String>> get_info_lines()
{
	BAN::Vector<BAN::String> info_lines;

	struct utsname utsname;
	if (uname(&utsname) == -1)
	{
		perror("uname");
		return BAN::Error::from_errno(errno);
	}

	char hostname[HOST_NAME_MAX];
	if (gethostname(hostname, sizeof(hostname)) == -1)
	{
		perror("gethostname");
		return BAN::Error::from_errno(errno);
	}

	const char* login = getlogin();
	if (login == nullptr)
	{
		perror("getlogin");
		return BAN::Error::from_errno(errno);
	}

	timespec uptime;
	if (clock_gettime(CLOCK_MONOTONIC, &uptime) == -1)
	{
		perror("clock_gettime");
		return BAN::Error::from_errno(errno);
	}

	TRY(info_lines.push_back(TRY(BAN::String::formatted(COLOR_ON "{}" COLOR_OFF "@" COLOR_ON "{}", login, hostname))));

	{
		const size_t host_length = strlen(login) + strlen(hostname) + 1;
		TRY(info_lines.emplace_back());
		TRY(info_lines.back().reserve(host_length));
		for (size_t i = 0; i < host_length; i++)
			MUST(info_lines.back().push_back('-'));
	}

	TRY(info_lines.push_back(TRY(BAN::String::formatted(COLOR_ON "OS" COLOR_OFF ": {} {}", utsname.sysname, utsname.machine))));

	TRY(info_lines.push_back(TRY(BAN::String::formatted(COLOR_ON "Kernel" COLOR_OFF ": {}", utsname.release))));

	{
		const uint32_t uptime_day = uptime.tv_sec / (60 * 60 * 24);
		uptime.tv_sec %= 60 * 60 * 24;

		const uint32_t uptime_hour = uptime.tv_sec / (60 * 60);
		uptime.tv_sec %= 60 * 60;

		const uint32_t uptime_minute = uptime.tv_sec / 60;
		uptime.tv_sec %= 60;

		TRY(info_lines.emplace_back(COLOR_ON "Uptime" COLOR_OFF ": "_sv));
		if (uptime_day)
			TRY(info_lines.back().append(TRY(BAN::String::formatted("{}d ", uptime_day))));
		if (uptime_hour)
			TRY(info_lines.back().append(TRY(BAN::String::formatted("{}h ", uptime_hour))));
		TRY(info_lines.back().append(TRY(BAN::String::formatted("{}m ", uptime_minute))));
	}

	if (DIR* dirp = opendir("/usr/bin"); dirp != nullptr)
	{
		size_t program_count = 0;
		struct dirent* dirent;
		while ((dirent = readdir(dirp)))
		{
			if (dirent->d_type != DT_REG)
				continue;

			struct stat st;
			if (fstatat(dirfd(dirp), dirent->d_name, &st, 0) == -1)
				continue;

			program_count += !!(st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH));
		}

		closedir(dirp);

		TRY(info_lines.push_back(TRY(BAN::String::formatted(COLOR_ON "Programs" COLOR_OFF ": {}", program_count))));
	}

	TRY(info_lines.push_back(TRY(BAN::String::formatted(COLOR_ON "CPU" COLOR_OFF ": {}", get_cpu_manufacturer()))));

	if (int meminfo_fd = open("/proc/meminfo", O_RDONLY); meminfo_fd != -1)
	{
		full_meminfo_t meminfo;
		if (read(meminfo_fd, &meminfo, sizeof(meminfo)) == sizeof(meminfo))
		{
			const size_t total_bytes = (meminfo.free_pages + meminfo.used_pages) * meminfo.page_size;
			const size_t used_bytes = meminfo.used_pages * meminfo.page_size;
			TRY(info_lines.push_back(TRY(BAN::String::formatted(COLOR_ON "Memory" COLOR_OFF ": {}MiB / {}MiB", used_bytes >> 20, total_bytes >> 20))));
		}
		close(meminfo_fd);
	}

	if (int fb_fd = open("/dev/fb0", O_RDONLY); fb_fd != -1)
	{
		framebuffer_info_t fb_info;
		if (pread(fb_fd, &fb_info, sizeof(fb_info), -1) == sizeof(framebuffer_info_t))
			TRY(info_lines.push_back(TRY(BAN::String::formatted(COLOR_ON "Resolution" COLOR_OFF ": {}x{}", fb_info.width, fb_info.height))));
		close(fb_fd);
	}

	if (int socket = ::socket(AF_INET, SOCK_DGRAM, 0); socket != -1)
	{
		sockaddr_in sockaddr;
		sockaddr.sin_family = AF_INET;
		sockaddr.sin_port = 0;
		sockaddr.sin_addr.s_addr = INADDR_ANY;
		if (bind(socket, reinterpret_cast<struct sockaddr*>(&sockaddr), sizeof(sockaddr)) == 0)
		{
			ifreq ifreq;
			if (ioctl(socket, SIOCGIFADDR, &ifreq) == 0)
			{
				auto& ifru_addr = *reinterpret_cast<sockaddr_in*>(&ifreq.ifr_ifru.ifru_addr);
				if (ifru_addr.sin_family == AF_INET)
					TRY(info_lines.push_back(TRY(BAN::String::formatted(COLOR_ON "Local IPv4" COLOR_OFF ": {}", BAN::IPv4Address(ifru_addr.sin_addr.s_addr)))));
			}
		}

		close(socket);
	}

	TRY(info_lines.emplace_back());

	TRY(info_lines.emplace_back());
	for (int color = 40; color <= 47; color++)
		TRY(info_lines.back().append(TRY(BAN::String::formatted("\e[{}m   ", color))));

	TRY(info_lines.emplace_back());
	for (int color = 100; color <= 107; color++)
		TRY(info_lines.back().append(TRY(BAN::String::formatted("\e[{}m   ", color))));

	return info_lines;
}

int main()
{
	constexpr auto& banana_art = s_banana_art2;
	constexpr size_t banana_width = s_banana_art2_width;
	constexpr size_t banana_height = s_banana_art2_height;

	auto info_lines_or_error = get_info_lines();
	if (info_lines_or_error.is_error())
	{
		fprintf(stderr, "Could not get system information: %s\n", info_lines_or_error.error().get_message());
		return 1;
	}
	auto info_lines = info_lines_or_error.release_value();

	for (size_t i = 0; i < BAN::Math::max(banana_height, info_lines.size()); i++)
	{
		if (i < banana_height)
		{
			printf("\e[1G" COLOR_ON);
			printf("%s", banana_art[i]);
		}

		if (i < info_lines.size())
		{
			printf("\e[%zuG" COLOR_OFF, banana_width + 2);
			printf("%s", info_lines[i].data());
		}

		printf(COLOR_OFF "\n");
	}
}
