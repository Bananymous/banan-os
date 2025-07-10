#include <LibAudio/Audio.h>

#include <stdio.h>
#include <unistd.h>

int main(int argc, char** argv)
{
	if (argc != 2)
	{
		fprintf(stderr, "usage: %s FILE\n", argv[0]);
		return 1;
	}

	auto audio_or_error = LibAudio::Audio::load(argv[1]);
	if (audio_or_error.is_error())
	{
		fprintf(stderr, "failed to load %s: %s\n", argv[1], audio_or_error.error().get_message());
		return 1;
	}

	auto audio = audio_or_error.release_value();

	if (auto ret = audio.start(); ret.is_error())
	{
		fprintf(stderr, "failed start playing audio: %s\n", ret.error().get_message());
		return 1;
	}

	while (audio.is_playing())
	{
		usleep(10'000);
		audio.update();
	}

	return 0;
}
