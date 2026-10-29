#include <gtest/gtest.h>
#include "TestHelpers.h"
#include "core/memory/MemoryTracker.h"
#include "core/memory/MemoryTrackable.h"
#include "core/scene/Scene.h"

// MemoryTracker is a process wide singleton, so every test compares against a baseline taken at its start
// instead of expecting absolute numbers.

#if MEMORY_TRACKING_ENABLED

namespace {

MemoryTracker& tracker() { return MemoryTracker::getInstance(); }

class TrackedThing : public MemoryTrackable {
public:
	TrackedThing() { setFootprint("TrackedThing", sizeof(TrackedThing), MemorySystem::Particle); }

private:
	char payload[64] {};
};

class CountingAllocator : public Allocator {
public:
	int allocations = 0;
	int deallocations = 0;

	void* allocate(size_t size, size_t alignment) override
	{
		allocations++;
		return ::operator new(size, std::align_val_t{ alignment });
	}

	void deallocate(void* ptr, size_t size, size_t alignment) override
	{
		deallocations++;
		::operator delete(ptr, size, std::align_val_t{ alignment });
	}
};

}

TEST(MemoryTracker, AllocateAndFreeUpdateStats)
{
	auto before = tracker().getStats(MemorySystem::Animation);

	void* ptr = MEMORY_ALLOC(256, MemorySystem::Animation, "test buffer");
	auto during = tracker().getStats(MemorySystem::Animation);
	EXPECT_EQ(during.allocatedBytes, before.allocatedBytes + 256);
	EXPECT_EQ(during.allocationCount, before.allocationCount + 1);

	MEMORY_FREE(ptr, 256, MemorySystem::Animation);
	auto after = tracker().getStats(MemorySystem::Animation);
	EXPECT_EQ(after.allocatedBytes, before.allocatedBytes);
	EXPECT_EQ(after.allocationCount, before.allocationCount);
}

TEST(MemoryTracker, PeakKeepsHighestValue)
{
	size_t baseline = tracker().getStats(MemorySystem::Animation).allocatedBytes;

	void* a = MEMORY_ALLOC(1000, MemorySystem::Animation, "a");
	void* b = MEMORY_ALLOC(500, MemorySystem::Animation, "b");
	MEMORY_FREE(a, 1000, MemorySystem::Animation);
	MEMORY_FREE(b, 500, MemorySystem::Animation);

	auto stats = tracker().getStats(MemorySystem::Animation);
	EXPECT_EQ(stats.allocatedBytes, baseline);
	EXPECT_GE(stats.peakAllocatedBytes, baseline + 1500);
}

TEST(MemoryTracker, TotalAddsUpEverySystem)
{
	size_t before = tracker().getTotalAllocatedBytes();

	void* a = MEMORY_ALLOC(128, MemorySystem::Animation, "a");
	void* b = MEMORY_ALLOC(64, MemorySystem::Physics, "b");
	EXPECT_EQ(tracker().getTotalAllocatedBytes(), before + 192);

	MEMORY_FREE(a, 128, MemorySystem::Animation);
	MEMORY_FREE(b, 64, MemorySystem::Physics);
	EXPECT_EQ(tracker().getTotalAllocatedBytes(), before);
}

TEST(MemoryTracker, TrackRecordsForeignMemoryWithoutOwningIt)
{
	auto before = tracker().getStats(MemorySystem::Graphics);
	std::vector<char> external(4096);

	MEMORY_TRACK(external.data(), external.size(), MemorySystem::Graphics, "external block");
	EXPECT_EQ(tracker().getStats(MemorySystem::Graphics).allocatedBytes, before.allocatedBytes + 4096);

	auto allocations = tracker().getAllocations();
	bool found = std::any_of(allocations.begin(), allocations.end(), [](const MemoryTracker::Footprint& f) {
		return std::string_view(f.name) == "external block";
	});
	EXPECT_TRUE(found);

	MEMORY_UNTRACK(external.data());
	EXPECT_EQ(tracker().getStats(MemorySystem::Graphics).allocatedBytes, before.allocatedBytes);
}

TEST(MemoryTracker, UntrackUnknownPointerIsIgnored)
{
	size_t before = tracker().getTotalAllocatedBytes();
	int notTracked = 0;

	MEMORY_UNTRACK(&notTracked);
	EXPECT_EQ(tracker().getTotalAllocatedBytes(), before);
}

TEST(MemoryTracker, FreeNullIsSafe)
{
	size_t before = tracker().getTotalAllocatedBytes();
	MEMORY_FREE(nullptr, 16, MemorySystem::General);
	EXPECT_EQ(tracker().getTotalAllocatedBytes(), before);
}

TEST(MemoryTracker, TrackingSameAddressTwiceDoesNotDoubleCount)
{
	auto before = tracker().getStats(MemorySystem::Graphics);
	int block = 0;

	MEMORY_TRACK(&block, 100, MemorySystem::Graphics, "first");
	MEMORY_TRACK(&block, 40, MemorySystem::Graphics, "reused");
	auto during = tracker().getStats(MemorySystem::Graphics);
	EXPECT_EQ(during.allocatedBytes, before.allocatedBytes + 40);
	EXPECT_EQ(during.allocationCount, before.allocationCount + 1);

	MEMORY_UNTRACK(&block);
	EXPECT_EQ(tracker().getStats(MemorySystem::Graphics).allocatedBytes, before.allocatedBytes);
}

TEST(MemoryTracker, CustomAllocatorIsUsedForItsSystem)
{
	CountingAllocator counting;
	tracker().setAllocator(MemorySystem::Particle, &counting);

	void* ptr = MEMORY_ALLOC(32, MemorySystem::Particle, "pooled");
	MEMORY_FREE(ptr, 32, MemorySystem::Particle);
	tracker().setAllocator(MemorySystem::Particle, nullptr);

	EXPECT_EQ(counting.allocations, 1);
	EXPECT_EQ(counting.deallocations, 1);
}

TEST(MemoryTrackable, ObjectRegistersForItsLifetime)
{
	auto before = tracker().getStats(MemorySystem::Particle);
	{
		TrackedThing thing;
		auto during = tracker().getStats(MemorySystem::Particle);
		EXPECT_EQ(during.objectCount, before.objectCount + 1);
		EXPECT_EQ(during.objectBytes, before.objectBytes + sizeof(TrackedThing));
		EXPECT_STREQ(thing.getMemoryName(), "TrackedThing");
	}
	auto after = tracker().getStats(MemorySystem::Particle);
	EXPECT_EQ(after.objectCount, before.objectCount);
	EXPECT_EQ(after.objectBytes, before.objectBytes);
}

TEST(MemoryTrackable, SetFootprintMovesObjectOutOfDefaultSystem)
{
	// the base constructor registers under General before the derived constructor calls setFootprint
	auto generalBefore = tracker().getStats(MemorySystem::General);
	TrackedThing thing;
	EXPECT_EQ(tracker().getStats(MemorySystem::General).objectCount, generalBefore.objectCount);
	EXPECT_EQ(thing.getMemorySystem(), MemorySystem::Particle);
}

TEST(MemoryTrackable, CopyRegistersSeparately)
{
	auto before = tracker().getStats(MemorySystem::Particle);
	TrackedThing original;
	TrackedThing copy(original);

	EXPECT_EQ(tracker().getStats(MemorySystem::Particle).objectCount, before.objectCount + 2);
}

TEST(MemoryTracker, SceneReleasesItsTrackedObjects)
{
	auto before = tracker().getStats(MemorySystem::Scene);
	{
		Scene scene("tracked-scene");
		scene.addEntity("A");
		scene.onUpdate(0.0f);
		EXPECT_GT(tracker().getStats(MemorySystem::Scene).objectCount, before.objectCount);
	}
	auto after = tracker().getStats(MemorySystem::Scene);
	EXPECT_EQ(after.objectCount, before.objectCount);
	EXPECT_EQ(after.objectBytes, before.objectBytes);
	EXPECT_EQ(after.allocatedBytes, before.allocatedBytes);
}

TEST(MemoryTracker, DumpJsonListsLiveAllocations)
{
	TestHelpers::TempDir temp;
	void* ptr = MEMORY_ALLOC(77, MemorySystem::Animation, "dump-me");

	std::string path = temp.file("stats.json");
	ASSERT_TRUE(tracker().dumpJson(path));
	MEMORY_FREE(ptr, 77, MemorySystem::Animation);

	std::ifstream in(path);
	std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	EXPECT_NE(text.find("dump-me"), std::string::npos);
	EXPECT_NE(text.find("Animation"), std::string::npos);
}

#else

TEST(MemoryTracker, OnlyTrackedInDebug)
{
	GTEST_SKIP() << "MemoryTracker records nothing in Release (MEMORY_TRACKING_ENABLED is 0), run the Debug build";
}

#endif
