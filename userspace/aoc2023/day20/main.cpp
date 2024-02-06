#include <BAN/HashMap.h>
#include <BAN/Queue.h>
#include <BAN/String.h>
#include <BAN/Vector.h>
#include <BAN/UniqPtr.h>

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>

using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

struct Signal
{
	BAN::String target;
	BAN::String sender;
	bool high;
};

struct Module
{
	BAN::String name;
	BAN::Vector<BAN::String> targets;

	virtual void handle_signal(const Signal& signal, BAN::Queue<Signal>& signal_queue) = 0;
};

struct BroadcasterModule : public Module
{
	void handle_signal(const Signal& signal, BAN::Queue<Signal>& signal_queue) override
	{
		for (const auto& target : targets)
			MUST(signal_queue.push({ target, name, signal.high }));
	}
};

struct FlipFlopModule : public Module
{
	bool is_on { false };

	void handle_signal(const Signal& signal, BAN::Queue<Signal>& signal_queue) override
	{
		if (signal.high)
			return;
		is_on = !is_on;
		for (const auto& target : targets)
			MUST(signal_queue.push({ target, name, is_on }));
	}
};

struct ConjunctionModule : public Module
{
	BAN::HashMap<BAN::String, bool> inputs;

	void handle_signal(const Signal& signal, BAN::Queue<Signal>& signal_queue) override
	{
		inputs[signal.sender] = signal.high;
		bool send_value = false;
		for (const auto& input : inputs)
			if (!input.value)
				send_value = true;
		for (const auto& target : targets)
			MUST(signal_queue.push({ target, name, send_value }));
	}
};

BAN::HashMap<BAN::String, BAN::UniqPtr<Module>> parse_modules(FILE* fp)
{
	BAN::HashMap<BAN::String, BAN::UniqPtr<Module>> modules;

	char buffer[128];
	while (fgets(buffer, sizeof(buffer), fp))
	{
		BAN::StringView line(buffer);
		ASSERT(line.back() == '\n');
		line = line.substring(0, line.size() - 1);
		if (line.empty())
			break;

		BAN::UniqPtr<Module> module;
		if (line.front() == '%')
		{
			module = MUST(BAN::UniqPtr<FlipFlopModule>::create());
			line = line.substring(1);
		}
		else if (line.front() == '&')
		{
			module = MUST(BAN::UniqPtr<ConjunctionModule>::create());
			line = line.substring(1);
		}
		else
		{
			module = MUST(BAN::UniqPtr<BroadcasterModule>::create());
		}

		auto name_targets = MUST(line.split('>'));
		auto name = name_targets[0].substring(0, name_targets[0].size() - 2);
		auto target_strs = MUST(name_targets[1].split(','));

		for (auto target : target_strs)
			MUST(module->targets.emplace_back(target.substring(1)));

		module->name = BAN::String(name);
		MUST(modules.insert(module->name, BAN::move(module)));
	}

	for (auto& [name, module] : modules)
	{
		for (auto& target : module->targets)
		{
			if (!modules.contains(target))
				continue;
			if (auto* ptr = dynamic_cast<ConjunctionModule*>(modules[target].ptr()))
				MUST(ptr->inputs.insert(name, false));
		}
	}

	return modules;
}

i64 puzzle1(FILE* fp)
{
	auto modules = parse_modules(fp);

	BAN::Queue<Signal> signal_queue;

	i64 sent_hi = 0;
	i64 sent_lo = 0;

	for (size_t i = 0; i < 1000; i++)
	{
		MUST(signal_queue.push({ "broadcaster"sv, ""sv, false }));
		while (!signal_queue.empty())
		{
			auto signal = signal_queue.front();
			signal_queue.pop();

			if (signal.high)
				sent_hi++;
			else
				sent_lo++;

			if (!modules.contains(signal.target))
				continue;

			auto& module = modules[signal.target];
			module->handle_signal(signal, signal_queue);
		}
	}

	return sent_hi * sent_lo;
}

i64 puzzle2(FILE* fp)
{
	(void)fp;
	return -1;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2023/day20_input.txt";

	if (argc >= 2)
		file_path = argv[1];

	FILE* fp = fopen(file_path, "r");
	if (fp == nullptr)
	{
		perror("fopen");
		return 1;
	}

	printf("puzzle1: %" PRId64 "\n", puzzle1(fp));

	fseek(fp, 0, SEEK_SET);

	printf("puzzle2: %" PRId64 "\n", puzzle2(fp));

	fclose(fp);
}
