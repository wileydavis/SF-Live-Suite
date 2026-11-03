#pragma once
// SF BUS: one block of shared memory that every SF plugin maps (across plugin binaries and
// processes). Lock-free atomics only. Carries macros, song recall, kick reports (Pocket) and
// per-band source energies / cuts (ALLOC).
#include <atomic>
#include <cstdint>

namespace sf
{
constexpr int kBands = 10;
constexpr int kSources = 32;
constexpr int kKickRing = 32;
constexpr int kMacros = 6;
constexpr int kSongs = 16;
constexpr int kConflictSlots = 3;

// Default ALLOC priority, highest first.
enum Role { ROLE_KICK = 0, ROLE_BASS, ROLE_VOICE, ROLE_LEAD, ROLE_PERC, ROLE_RUMBLE, ROLE_PAD, ROLE_DECK, ROLE_OTHER, kNumRoles };
extern const char* const kRoleNames[kNumRoles];
extern const float kBandCentres[kBands];     // Hz
extern const float kBandEdges[kBands + 1];   // Hz

// wall = steady-clock seconds when reported: lets readers reject reports left over from before
// a transport jump (loop brace, restart, relocate) that happen to share a song position.
struct KickHit { double ppq; float vel, f0, f1, tau, decay, hp, lp; double wall = 0.0; };

struct KickSlot
{
    std::atomic<uint32_t> seq;
    std::atomic<double> ppq, wall;
    std::atomic<float> vel, f0, f1, tau, decay, hp, lp;
};

struct SourceSlot
{
    std::atomic<uint64_t> owner;
    std::atomic<int64_t> seenMs;
    std::atomic<int32_t> role;
    std::atomic<int32_t> enabled;
    std::atomic<uint64_t> name[3];            // up to 24 chars, packed
    std::atomic<float> energy[kBands];        // dBFS, smoothed
    std::atomic<float> cut[kBands];           // dB of cut ALLOC asks for (>= 0)
};

struct ConflictSlot
{
    std::atomic<uint32_t> sources;            // bit mask of source slots
    std::atomic<int32_t> bandLo, bandHi;
    std::atomic<int32_t> active;
};

struct BusData
{
    std::atomic<uint32_t> magic;
    std::atomic<uint32_t> version;
    std::atomic<float> macro[kMacros];
    std::atomic<int32_t> song;                // -1 = none chosen yet
    std::atomic<uint32_t> songSeq;
    std::atomic<uint32_t> storeSeq;
    std::atomic<int64_t> controlSeenMs;

    std::atomic<uint32_t> kickWrite;
    std::atomic<int64_t> kickSeenMs;
    std::atomic<float> pocketOffsetMs;      // SF CONTROL calibration: + = pockets later
    KickSlot kicks[kKickRing];

    SourceSlot src[kSources];
    std::atomic<int64_t> allocSeenMs;
    ConflictSlot conflicts[kConflictSlots];
};

class Bus
{
public:
    static Bus& get();
    static int64_t nowMs();
    static double nowSec();
    static inline double (*testClock)() = nullptr;    // offline tests: simulated time

    BusData& d() { return *data; }
    bool shared() const { return isShared; }

    void publishKick (const KickHit&);
    int recentKicks (KickHit* out, int max) const;     // newest first
    // A report is current if the wall time since it was made fits the musical time since it.
    static bool kickCurrent (const KickHit& h, double dtBeats, double secPerBeat, double lookSec)
    {
        const double elapsed = nowSec() - h.wall;
        return elapsed <= dtBeats * secPerBeat + lookSec + 0.35;
    }
    bool kickAlive (int64_t withinMs = 4000) const;

    int claimSource (uint64_t owner, int role, const char* name);   // -1 if full
    void touchSource (int slot, uint64_t owner, int role, bool enabled);
    void releaseSource (int slot, uint64_t owner);
    bool sourceAlive (int slot, int64_t withinMs = 1500) const;
    static void packName (std::atomic<uint64_t>* dst, const char* s);
    static void unpackName (const std::atomic<uint64_t>* src, char* out24);

    bool allocAlive() const   { return nowMs() - data->allocSeenMs.load() < 1500; }
    bool controlAlive() const { return nowMs() - data->controlSeenMs.load() < 1500; }

private:
    Bus();
    BusData* data = nullptr;
    bool isShared = false;
};
} // namespace sf
