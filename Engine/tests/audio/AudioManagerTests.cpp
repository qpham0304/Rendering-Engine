#include <gtest/gtest.h>
#include "TestHelpers.h"
#include "audio/framework/AudioManagerMiniaudio.h"
#include "core/events/EventManager.h"
#include <cstdint>
#include <fstream>

// the device never starts, it only starts while the game is playing, so nothing is heard.
// a machine with no audio device skips these

namespace {

// 0.1s of 16 bit mono silence
std::string writeSilentWav(const TestHelpers::TempDir& temp, std::string_view fileName)
{
	const uint32_t rate = 8000;
	const uint32_t dataSize = rate / 10 * 2;
	auto write32 = [](std::ofstream& out, uint32_t value) { out.write(reinterpret_cast<const char*>(&value), 4); };
	auto write16 = [](std::ofstream& out, uint16_t value) { out.write(reinterpret_cast<const char*>(&value), 2); };

	std::string path = temp.file(fileName);
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	out.write("RIFF", 4); write32(out, 36 + dataSize); out.write("WAVE", 4);
	out.write("fmt ", 4); write32(out, 16); write16(out, 1); write16(out, 1);
	write32(out, rate); write32(out, rate * 2); write16(out, 2); write16(out, 16);
	out.write("data", 4); write32(out, dataSize);
	std::string silence(dataSize, '\0');
	out.write(silence.data(), silence.size());
	return path;
}

// one manager for the suite, a service's logger name can only be registered once per process
class AudioManagerTest : public ::testing::Test {
protected:
	static inline std::unique_ptr<AudioManager> s_audio;
	static inline bool s_initialized = false;

	TestHelpers::TempDir temp;
	AudioManager& audio = *s_audio;

	static void SetUpTestSuite()
	{
		s_audio = std::make_unique<AudioManagerMiniaudio>();
		s_initialized = s_audio->init(WindowConfig{});
	}

	static void TearDownTestSuite()
	{
		s_audio->onClose();
		s_audio.reset();
	}

	void SetUp() override
	{
		if (!s_initialized) {
			GTEST_SKIP() << "no audio device";
		}
	}

	void TearDown() override
	{
		audio.stopAll();
	}
};

}

TEST_F(AudioManagerTest, MissingFileReturnsZero)
{
	EXPECT_EQ(audio.play(temp.file("missing.wav")), 0u);
	EXPECT_TRUE(audio.listIDs().empty());
}

TEST_F(AudioManagerTest, PlayAndStopSound)
{
	std::string path = writeSilentWav(temp, "silence.wav");

	uint32_t id = audio.play(path, { .loop = true, .spatial = true, .position = glm::vec3(1.0f, 0.0f, 0.0f) });
	ASSERT_NE(id, 0u);
	EXPECT_TRUE(audio.isPlaying(id));

	audio.stop(id);
	EXPECT_FALSE(audio.isPlaying(id));
	EXPECT_TRUE(audio.listIDs().empty());
}

TEST_F(AudioManagerTest, PlayAudioEventPlaysSound)
{
	std::string path = writeSilentWav(temp, "hit.wav");

	PlayAudioEvent event(path, glm::vec3(0.0f, 2.0f, 0.0f), 0.5f);
	EventManager::getInstance().publish(event);

	ASSERT_EQ(audio.listIDs().size(), 1u);
	EXPECT_TRUE(audio.isPlaying(audio.listIDs().front()));
}

TEST_F(AudioManagerTest, MusicReplacesPreviousTrack)
{
	std::string first = writeSilentWav(temp, "first.wav");
	std::string second = writeSilentWav(temp, "second.wav");

	audio.playMusic(first);
	audio.playMusic(second);
	EXPECT_EQ(audio.listIDs().size(), 1u);

	audio.stopMusic();
	EXPECT_TRUE(audio.listIDs().empty());
}
