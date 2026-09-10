#pragma once
// Included by main.cpp after sensor helpers: one SPI owner, separate compute
// and network tasks. No LittleFS, NVS, HTTPS or feature FFT in the FIFO reader.
#include "vibration_window.h"
#include "continuous_vibration_health.h"
#include <mbedtls/base64.h>
#include <esp_heap_caps.h>
#include <cstring>
#include <new>

#ifndef RAW_VIBRATION_ENABLED
#define RAW_VIBRATION_ENABLED 0
#endif

#ifndef CONTINUOUS_VIBRATION_ENABLED
#define CONTINUOUS_VIBRATION_ENABLED 0
#endif
#ifndef RAW_VIBRATION_DEBUG
#define RAW_VIBRATION_DEBUG 0
#endif

#if CONTINUOUS_VIBRATION_ENABLED
bool beginBackendHttp(BackendHttp&, WiFiClientSecure&, const char*);
void logStorageMemoryDiagnostic(const char* phase);
bool checkAudioRingCanary();
namespace ContinuousVibration {
using namespace VibrationWindow;
// Keep several completed windows available so capture never immediately
// blocks behind feature extraction or a temporarily full pending queue.
constexpr unsigned RawQueueCapacity = 8;
#if RAW_VIBRATION_ENABLED
constexpr unsigned PendingCapacity = 8; // PSRAM-backed processing keeps capture from stalling.
constexpr unsigned RawHoldCapacity = 8;
// Server accepts up to four raw windows per request (64 KiB body limit).
constexpr unsigned BatchCapacity = 4;
constexpr size_t MaxRawRequestBytes = 64 * 1024;
using PendingWindow = Raw;
constexpr size_t RawBytesCapacity = Samples*3*2;
constexpr size_t EncodedRawCapacity = RawBytesCapacity*4/3+1;
constexpr unsigned RawSpoolSlots = 16;
constexpr std::uint32_t RawSpoolMagic = 0x52535031UL; // "RSP1"
constexpr std::uint16_t RawSpoolVersion = 1;
struct __attribute__((packed)) RawSpoolHeader {
    std::uint32_t magic = RawSpoolMagic;
    std::uint16_t version = RawSpoolVersion;
    std::uint16_t count = 0;
    std::uint32_t index = 0;
    std::uint64_t startUs = 0;
    std::uint8_t quality = 0;
    std::uint8_t reserved[3] = {};
    char bootId[33] = {};
    std::uint32_t crc = 0;
};
constexpr size_t RawSpoolSlotBytes = sizeof(RawSpoolHeader) + RawBytesCapacity;
static_assert(sizeof(RawSpoolHeader) == 61, "Raw spool header must remain fixed.");
#else
constexpr unsigned PendingCapacity = 64; // 40.96 s, internal RAM only.
constexpr unsigned BatchCapacity = 8;
using PendingWindow = Features;
constexpr unsigned BatchIntervalMs = 1500;
#endif
Queue<PendingWindow, PendingCapacity> pending;
// Static network-task-owned buffers: do not place raw batches on its stack.
PendingWindow* batch = nullptr;
#if RAW_VIBRATION_ENABLED
// SPSC fallback for a full pending queue. Only processingTask writes here;
// captureTask records a bounded rawQueue drop instead of becoming a second
// producer. The processing task merges both sources by window index.
Raw* rawHold = nullptr;
std::atomic<std::uint32_t> rawHoldWrite{0};
std::atomic<std::uint32_t> rawHoldRead{0};
unsigned char* rawBytes = nullptr;
unsigned char* encodedRaw = nullptr;
unsigned char* rawHoldAllocation = nullptr;
unsigned char* rawBytesAllocation = nullptr;
unsigned char* encodedRawAllocation = nullptr;
constexpr size_t DiagnosticCanaryBytes = 16;
constexpr unsigned char DiagnosticCanaryHead = 0xA5;
constexpr unsigned char DiagnosticCanaryTail = 0x5A;
extern char boot[33];

void* allocateGuardedRawBuffer(size_t payloadBytes) {
    const size_t allocationBytes = payloadBytes + DiagnosticCanaryBytes * 2;
    void* allocation = heap_caps_malloc(allocationBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!allocation) allocation = heap_caps_malloc(allocationBytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!allocation) return nullptr;
    auto* bytes = static_cast<unsigned char*>(allocation);
    std::memset(bytes, DiagnosticCanaryHead, DiagnosticCanaryBytes);
    std::memset(bytes + DiagnosticCanaryBytes + payloadBytes, DiagnosticCanaryTail, DiagnosticCanaryBytes);
    return bytes;
}

bool guardedRawBufferIntact(const unsigned char* allocation, size_t payloadBytes) {
    if (!allocation) return false;
    for (size_t i = 0; i < DiagnosticCanaryBytes; ++i)
        if (allocation[i] != DiagnosticCanaryHead ||
            allocation[DiagnosticCanaryBytes + payloadBytes + i] != DiagnosticCanaryTail)
            return false;
    return true;
}

void logRawBufferCanaries(const char* phase) {
    const bool rawHoldOk = guardedRawBufferIntact(rawHoldAllocation, sizeof(Raw) * RawHoldCapacity);
    const bool rawBytesOk = guardedRawBufferIntact(rawBytesAllocation, RawBytesCapacity);
    const bool encodedRawOk = guardedRawBufferIntact(encodedRawAllocation, EncodedRawCapacity);
    const bool audioRingOk = checkAudioRingCanary();
    Serial.printf("[BUFFER-CANARY] phase=%s rawHold=%s rawBytes=%s encodedRaw=%s audioRing=%s\n",
                  phase ? phase : "unknown", rawHoldOk ? "ok" : "CORRUPT",
                  rawBytesOk ? "ok" : "CORRUPT", encodedRawOk ? "ok" : "CORRUPT",
                  audioRingOk ? "ok" : "CORRUPT");
}

bool holdRaw(const Raw& raw) {
    if (!rawHold) return false;
    const auto write = rawHoldWrite.load(std::memory_order_relaxed);
    const auto read = rawHoldRead.load(std::memory_order_acquire);
    if (write - read >= RawHoldCapacity) return false;
    rawHold[write % RawHoldCapacity] = raw;
    rawHoldWrite.store(write + 1, std::memory_order_release);
    return true;
}

bool takeHeldRaw(Raw& raw) {
    const auto read = rawHoldRead.load(std::memory_order_relaxed);
    if (read == rawHoldWrite.load(std::memory_order_acquire)) return false;
    raw = rawHold[read % RawHoldCapacity];
    rawHoldRead.store(read + 1, std::memory_order_release);
    return true;
}

bool encodeRaw(const Raw& raw, JsonObject w) {
    if (!rawBytes || !encodedRaw || !serializeCountsLE(raw,rawBytes,RawBytesCapacity)) return false;
    size_t length=0;
    if (mbedtls_base64_encode(encodedRaw,EncodedRawCapacity,&length,rawBytes,raw.count*6)) return false;
    encodedRaw[length]=0;
    // Own each string; subsequent windows reuse encodedRaw.
    w["samples"] = String(reinterpret_cast<const char*>(encodedRaw));
    return true;
}

String rawSpoolPath(unsigned slot) {
    return String("/raw-spool-v2-") + slot + ".bin";
}
String legacyRawSpoolPath(unsigned slot) {
    return String("/raw-spool-") + slot + ".bin";
}

bool rawSpoolHasPending() {
    LittleFsLock fsLock;
    unsigned legacy = 0;
    bool current = false;
    for (unsigned i = 0; i < RawSpoolSlots; ++i)
        if (LittleFS.exists(rawSpoolPath(i))) current = true;
        else if (LittleFS.exists(legacyRawSpoolPath(i))) ++legacy;
    if (legacy) Serial.printf("[RAW-SPOOL] legacy spool pending=%u; unreadable for current boot and held (header has no bootId)\n", legacy);
    return current || legacy != 0;
}

bool rawSpoolHeaderUnlocked(unsigned slot, RawSpoolHeader& header) {
    File file = LittleFS.open(rawSpoolPath(slot), "r");
    if (!file || file.size() != RawSpoolSlotBytes ||
        file.read(reinterpret_cast<std::uint8_t*>(&header), sizeof(header)) != sizeof(header)) {
        if (file) file.close();
        return false;
    }
    file.close();
    return header.magic == RawSpoolMagic && header.version == RawSpoolVersion &&
        header.count > 0 && header.count <= Samples &&
        header.quality <= static_cast<std::uint8_t>(Quality::ProcessingOverflow);
}

bool readRawSpoolUnlocked(unsigned slot, Raw& raw) {
    auto* bytes = static_cast<std::uint8_t*>(heap_caps_malloc(RawBytesCapacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!bytes) bytes = static_cast<std::uint8_t*>(heap_caps_malloc(RawBytesCapacity, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    if (!bytes) return false;
    std::memset(bytes, 0, RawBytesCapacity);
    RawSpoolHeader header;
    {
        File file = LittleFS.open(rawSpoolPath(slot), "r");
        if (!file || file.size() != RawSpoolSlotBytes ||
            file.read(reinterpret_cast<std::uint8_t*>(&header), sizeof(header)) != sizeof(header) ||
            file.read(bytes, RawBytesCapacity) != RawBytesCapacity) {
            if (file) file.close();
            heap_caps_free(bytes);
            return false;
        }
        file.close();
    }
    if (header.magic != RawSpoolMagic || header.version != RawSpoolVersion ||
        header.count == 0 || header.count > Samples ||
        header.quality > static_cast<std::uint8_t>(Quality::ProcessingOverflow) ||
        header.crc != EdgeAnalysis::checksum(reinterpret_cast<const char*>(bytes), RawBytesCapacity)) {
        heap_caps_free(bytes);
        return false;
    }
    raw = Raw{};
    raw.index = header.index;
    raw.startUs = header.startUs;
    raw.count = header.count;
    raw.quality = static_cast<Quality>(header.quality);
    const bool valid = deserializeCountsLE(raw, bytes, RawBytesCapacity);
    heap_caps_free(bytes);
    return valid;
}

bool readRawSpool(unsigned slot, Raw& raw) {
    LittleFsLock fsLock;
    return readRawSpoolUnlocked(slot, raw);
}

bool sameRaw(const Raw& left, const Raw& right) {
    return left.index == right.index && left.startUs == right.startUs &&
        left.count == right.count && left.quality == right.quality &&
        std::memcmp(left.xyz, right.xyz, sizeof(left.xyz)) == 0;
}

bool writeRawSpool(const Raw& raw) {
    auto* bytes = static_cast<std::uint8_t*>(heap_caps_malloc(RawBytesCapacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!bytes) bytes = static_cast<std::uint8_t*>(heap_caps_malloc(RawBytesCapacity, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    if (!bytes) return false;
    std::memset(bytes, 0, RawBytesCapacity);
    if (!serializeCountsLE(raw, bytes, RawBytesCapacity)) { heap_caps_free(bytes); return false; }
    unsigned slot = RawSpoolSlots;
    {
        LittleFsLock fsLock;
        for (unsigned i = 0; i < RawSpoolSlots; ++i)
            if (!LittleFS.exists(rawSpoolPath(i))) { slot = i; break; }
        const size_t totalBytes = LittleFS.totalBytes();
        const size_t usedBytes = LittleFS.usedBytes();
        const size_t freeBytes = totalBytes > usedBytes ? totalBytes - usedBytes : 0;
        if (slot == RawSpoolSlots || freeBytes < RawSpoolSlotBytes) {
            Serial.printf("[RAW-SPOOL-FULL] slot=%u total=%u used=%u free=%u need=%u windowIndex=%lu.\n",
                          slot, static_cast<unsigned>(totalBytes), static_cast<unsigned>(usedBytes),
                          static_cast<unsigned>(freeBytes), static_cast<unsigned>(RawSpoolSlotBytes),
                          static_cast<unsigned long>(raw.index));
            heap_caps_free(bytes);
            return false;
        }
        RawSpoolHeader header;
        header.index = raw.index;
        header.startUs = raw.startUs;
        header.count = raw.count;
        header.quality = static_cast<std::uint8_t>(raw.quality);
        std::strncpy(header.bootId, boot, sizeof(header.bootId) - 1);
        header.crc = EdgeAnalysis::checksum(reinterpret_cast<const char*>(bytes), RawBytesCapacity);
        const String temp = rawSpoolPath(slot) + ".tmp";
        File file = LittleFS.open(temp, "w");
        const bool written = file &&
            file.write(reinterpret_cast<const std::uint8_t*>(&header), sizeof(header)) == sizeof(header) &&
            file.write(bytes, RawBytesCapacity) == RawBytesCapacity;
        if (file) { file.flush(); file.close(); }
        if (!written || !LittleFS.rename(temp, rawSpoolPath(slot))) {
            if (LittleFS.exists(temp)) LittleFS.remove(temp);
            Serial.printf("[RAW-SPOOL] write failed; windowIndex=%lu retained in RAM.\n",
                          static_cast<unsigned long>(raw.index));
            heap_caps_free(bytes);
            return false;
        }
    }
    auto* verified = static_cast<Raw*>(heap_caps_malloc(sizeof(Raw), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!verified) verified = static_cast<Raw*>(heap_caps_malloc(sizeof(Raw), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    const bool verifiedOk = verified && readRawSpool(slot, *verified) && sameRaw(raw, *verified);
    if (verified) heap_caps_free(verified);
    heap_caps_free(bytes);
    if (!verifiedOk) {
        Serial.printf("[RAW-SPOOL] read-back verification failed; windowIndex=%lu retained.\n",
                      static_cast<unsigned long>(raw.index));
        return false;
    }
    Serial.printf("[RAW-SPOOL] stored slot=%u windowIndex=%lu.\n", slot,
                  static_cast<unsigned long>(raw.index));
    return true;
}

unsigned loadRawSpoolBatch(Raw* destination, unsigned* slots, unsigned capacity) {
    if (!destination || !slots || !capacity) return 0;
    LittleFsLock fsLock;
    bool used[RawSpoolSlots] = {};
    unsigned loaded = 0;
    while (loaded < capacity) {
        unsigned selected = RawSpoolSlots;
        std::uint32_t selectedIndex = UINT32_MAX;
        for (unsigned i = 0; i < RawSpoolSlots; ++i) {
            if (used[i] || !LittleFS.exists(rawSpoolPath(i))) continue;
            RawSpoolHeader header;
            if (!rawSpoolHeaderUnlocked(i, header)) {
                Serial.printf("[RAW-SPOOL] invalid/read-failed slot=%u; retaining spool.\n", i);
                return 0;
            }
            if (std::strncmp(header.bootId, boot, sizeof(header.bootId)) != 0) continue;
            if (selected == RawSpoolSlots || header.index < selectedIndex) {
                selected = i;
                selectedIndex = header.index;
            }
        }
        if (selected == RawSpoolSlots) break;
        if (!readRawSpoolUnlocked(selected, destination[loaded])) {
            Serial.printf("[RAW-SPOOL] restore failed slot=%u; retaining spool.\n", selected);
            return 0;
        }
        slots[loaded++] = selected;
        used[selected] = true;
    }
    return loaded;
}

bool oldestRawSpoolIndex(std::uint32_t& index) {
    // Hold one filesystem transaction across the complete slot scan.  Taking
    // the lock per header left other VFS work able to interleave between slots.
    LittleFsLock fsLock;
    bool found = false;
    for (unsigned i = 0; i < RawSpoolSlots; ++i) {
        RawSpoolHeader header;
        if (rawSpoolHeaderUnlocked(i, header) &&
            std::strncmp(header.bootId, boot, sizeof(header.bootId)) == 0 &&
            (!found || header.index < index)) {
            index = header.index;
            found = true;
        }
    }
    return found;
}

bool removeRawSpoolBatch(const unsigned* slots, unsigned count) {
    LittleFsLock fsLock;
    bool removed = true;
    for (unsigned i = 0; i < count; ++i)
        if (LittleFS.exists(rawSpoolPath(slots[i])) && !LittleFS.remove(rawSpoolPath(slots[i]))) removed = false;
    return removed;
}

bool strictRawAck(int status, const String& response, const Raw* windows, unsigned count, const char* bootId) {
    if ((status != 200 && status != 202) || !windows || !count) return false;
    JsonDocument document;
    if (deserializeJson(document, response, DeserializationOption::NestingLimit(5))) return false;
    if (!document["accepted"].is<bool>() || !document["accepted"].as<bool>()) return false;
    const auto acknowledged = document["acknowledged"].as<JsonArray>();
    if (document["deviceId"] != DEVICE_ID || acknowledged.size() != count) return false;
    for (unsigned i = 0; i < count; ++i)
        if (acknowledged[i]["bootId"] != bootId ||
            !acknowledged[i]["windowIndex"].is<std::uint32_t>() ||
            acknowledged[i]["windowIndex"].as<std::uint32_t>() != windows[i].index) return false;
    return true;
}
#endif
Workspace* workspace = nullptr;
Raw capturing;
QueueHandle_t rawQueue = nullptr;
SemaphoreHandle_t mutex = nullptr;
Features latest;
AcousticFeatures latestAudio;
bool latestAudioValid = false;
std::uint32_t latestAudioGeneration = 0;
bool hasLatest = false;
std::atomic<bool> sensorReady{false};
std::atomic<std::uint32_t> processingDrops{0};
std::atomic<std::uint32_t> audioDrops{0};
std::atomic<bool> rawNetworkBusy{false};
std::atomic<bool> healthNetworkRequested{false};
std::atomic<bool> telemetryReplayRequested{false};
// 0=free, 1=raw reserved/in flight, 2=health reserved, 3=telemetry reserved.
// A single atomic
// reservation closes the check-then-acquire race without splitting the gate.
std::atomic<std::uint8_t> networkReservation{0};
char boot[33];
CaptureHealth captureHealth;

bool tryReserveRawNetwork() {
    if (healthNetworkRequested.load(std::memory_order_acquire) ||
        telemetryReplayRequested.load(std::memory_order_acquire)) return false;
    std::uint8_t free = 0;
    return networkReservation.compare_exchange_strong(free, 1,
        std::memory_order_acq_rel, std::memory_order_acquire);
}

bool rawQueueIsEmpty() {
    xSemaphoreTake(mutex, portMAX_DELAY);
    const bool pendingEmpty = pending.size() == 0;
    xSemaphoreGive(mutex);
    const bool rawQueueEmpty = !rawQueue || uxQueueMessagesWaiting(rawQueue) == 0;
#if RAW_VIBRATION_ENABLED
    const bool rawHoldEmpty = rawHoldRead.load(std::memory_order_acquire) ==
                              rawHoldWrite.load(std::memory_order_acquire);
#else
    const bool rawHoldEmpty = true;
#endif
    return pendingEmpty && rawQueueEmpty && rawHoldEmpty;
}

void releaseRawNetwork() {
    networkReservation.store(0, std::memory_order_release);
}

bool tryReserveHealthNetwork() {
    std::uint8_t free = 0;
    return networkReservation.compare_exchange_strong(free, 2,
        std::memory_order_acq_rel, std::memory_order_acquire);
}

bool tryReserveTelemetryNetwork() {
    if (healthNetworkRequested.load(std::memory_order_acquire)) return false;
    std::uint8_t free = 0;
    return networkReservation.compare_exchange_strong(free, 3,
        std::memory_order_acq_rel, std::memory_order_acquire);
}

void releaseTelemetryNetwork() {
    std::uint8_t expected = 3;
    networkReservation.compare_exchange_strong(expected, 0,
        std::memory_order_acq_rel, std::memory_order_acquire);
}

void releaseHealthNetwork() {
    std::uint8_t expected = 2;
    networkReservation.compare_exchange_strong(expected, 0,
        std::memory_order_acq_rel, std::memory_order_acquire);
}

void reportCaptureHealth(CaptureHealth::State state) {
    captureHealth.record(state, healthUptimeMs(),
        [](DeviceHealth::Fault fault, bool active, std::uint64_t observedMs) {
            const SensorObservation observation{fault, active, observedMs};
            // A health transition must never pause the sensor reader.
            return xQueueSend(sensorObservations, &observation, 0) == pdTRUE;
        });
}

void resetFifo() {
    adxlWrite(0x38, 0); // Bypass clears stale FIFO; then continuous stream.
    adxlWrite(0x38, 0x80);
}
void finish(Quality quality) {
    capturing.quality = quality;
    if (quality == Quality::Valid) reportCaptureHealth(CaptureHealth::State::Healthy);
    if (capturing.audioWindow) {
        heap_caps_free(capturing.audioWindow);
        capturing.audioWindow = nullptr;
    }
    // Copy the exact synchronized window once, while capture still owns the
    // Raw. The bounded wait only covers the audio samples trailing vibration.
    if (capturing.count > 0 && audioReady.load() &&
        capturing.audioGeneration == audioErrorGeneration.load()) {
        auto* window = static_cast<std::int32_t*>(heap_caps_malloc(
            sizeof(std::int32_t) * COMMON_AUDIO_SAMPLES,
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (!window) window = static_cast<std::int32_t*>(heap_caps_malloc(
            sizeof(std::int32_t) * COMMON_AUDIO_SAMPLES,
            MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
        const auto readyAt = millis();
        while (window && getAudioTotalSamples() < capturing.audioStart + COMMON_AUDIO_SAMPLES &&
               millis() - readyAt < 20) vTaskDelay(1);
        if (window && copyAudioWindow(capturing.audioStart, COMMON_AUDIO_SAMPLES, window))
            capturing.audioWindow = window;
        else if (window) heap_caps_free(window);
    }
    // Never block the sensor reader. rawHold is owned by processingTask only;
    // a full rawQueue is recorded as a bounded drop rather than a second
    // producer racing the hold queue.
    const bool queued = xQueueSend(rawQueue, &capturing, 0) == pdTRUE;
    if (queued) {
        // The queue copy now owns the captured audio buffer.
        capturing.audioWindow = nullptr;
    } else {
        if (capturing.audioWindow) {
            heap_caps_free(capturing.audioWindow);
            capturing.audioWindow = nullptr;
        }
        ++processingDrops;
    }
    ++capturing.index; // Includes invalid/dropped windows, never renumber.
    capturing.count = 0;
    capturing.quality = Quality::Valid;
}
void captureTask(void*) {
    Serial.printf("[STACK] VibrationFIFO free=%u bytes\n",
                  static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr) * sizeof(StackType_t)));
    std::uint64_t lastData = esp_timer_get_time();
    while (true) {
        if (!sensorReady.load()) {
            if (!initADXL345()) {
                reportCaptureHealth(CaptureHealth::State::InitFailed);
                capturing.startUs = esp_timer_get_time();
                finish(Quality::SensorUnavailable);
                vTaskDelay(pdMS_TO_TICKS(640));
                continue;
            }
            resetFifo();
            sensorReady.store(true);
            lastData = esp_timer_get_time();
        }
        const auto now = static_cast<std::uint64_t>(esp_timer_get_time());
        const auto readStarted = static_cast<std::uint64_t>(esp_timer_get_time());
        const unsigned entries = adxlRead(0x39) & 0x3f;
        const unsigned devid = adxlRead(REG_DEVID);
        const auto readDuration = static_cast<std::uint64_t>(esp_timer_get_time()) - readStarted;
        // ADXL345 FIFO_ENTRIES is a 6-bit count with 32 as the valid full state;
        // full means imminent loss if servicing remains late, not loss by itself.
        const bool disconnected = devid != 0xE5 || entries > 32;
        if (entries > 32 || devid != 0xE5) {
            Serial.printf("[FIFO] fault=%s rawEntries=%u devid=0x%02X readUs=%llu captureGapUs=%llu rawQueue=%u windowIndex=%lu count=%u startUs=%llu\n",
                          devid != 0xE5 ? "sensor_unavailable" : "fifo_invalid",
                          entries,
                          devid,
                          static_cast<unsigned long long>(readDuration),
                          static_cast<unsigned long long>(now - lastData),
                          rawQueue ? static_cast<unsigned>(uxQueueMessagesWaiting(rawQueue)) : 0,
                          static_cast<unsigned long>(capturing.index),
                          capturing.count,
                          static_cast<unsigned long long>(capturing.startUs));
            reportCaptureHealth(disconnected ? CaptureHealth::State::ChannelFailed :
                                              CaptureHealth::State::TimedOut);
            if (!capturing.count) capturing.startUs = now;
            finish(disconnected ? Quality::SensorUnavailable : Quality::FifoOverrun);
            sensorReady.store(false);
            continue;
        }
        if (!entries) {
            if (now - lastData > 100000) {
                reportCaptureHealth(CaptureHealth::State::TimedOut);
                if (!capturing.count) capturing.startUs = now;
                finish(Quality::SampleGap);
                sensorReady.store(false);
            }
            vTaskDelay(1);
            continue;
        }
        lastData = now;
        for (unsigned n=0; n<entries; ++n) {
            if (!capturing.count) {
                // Estimate first sample instant from FIFO depth, not POST time.
                capturing.startUs = now - (entries-n-1)*1250ULL;
                const auto audioNow = getAudioTotalSamples();
                const auto audioLag = (entries-n-1)*20ULL;
                capturing.audioStart = audioNow > audioLag ? audioNow-audioLag : 0;
                capturing.audioGeneration = audioErrorGeneration.load();
            }
            auto& xyz = capturing.xyz[capturing.count];
            adxlReadXYZ(xyz[0], xyz[1], xyz[2]);
            // ADXL345 datasheet: minimum 5 us before the next FIFO access.
            delayMicroseconds(5);
            ++capturing.count;
            if (capturing.count == Samples) finish(Quality::Valid);
        }
        // The 5 us inter-read delay above is required by the ADXL345; do not
        // add another delay after a successful FIFO drain.
    }
}
void processingTask(void*) {
    Serial.printf("[STACK] WindowFeatures free=%u bytes\n",
                  static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr) * sizeof(StackType_t)));
    Raw raw;
    std::uint32_t pendingFullEvents = 0;
    std::uint32_t lastPendingFullLogMs = 0;
    while (true) {
        bool received = false;
#if RAW_VIBRATION_ENABLED
        // Only processingTask writes rawHold, and it writes back the item it
        // just removed from rawQueue when pending is full. It is therefore
        // older than anything still waiting in rawQueue.
        received = takeHeldRaw(raw);
        if (!received) received = xQueueReceive(rawQueue, &raw, pdMS_TO_TICKS(20)) == pdTRUE;
#else
        received = xQueueReceive(rawQueue, &raw, portMAX_DELAY) == pdTRUE;
#endif
        if (!received) continue;
        auto features = extract(raw, *workspace);
#if RAW_VIBRATION_DEBUG
        static unsigned debugWindows = 0;
        if (raw.count > 0 && (++debugWindows % 10 == 1)) {
            double sum[3] = {}, sumSquares[3] = {};
            std::int16_t minimum[3] = {raw.xyz[0][0], raw.xyz[0][1], raw.xyz[0][2]};
            std::int16_t maximum[3] = {raw.xyz[0][0], raw.xyz[0][1], raw.xyz[0][2]};
            for (unsigned i = 0; i < raw.count; ++i) {
                for (unsigned axis = 0; axis < 3; ++axis) {
                    const auto value = raw.xyz[i][axis];
                    minimum[axis] = std::min(minimum[axis], value);
                    maximum[axis] = std::max(maximum[axis], value);
                    sum[axis] += value;
                    sumSquares[axis] += static_cast<double>(value) * value;
                }
            }
            Serial.printf("[RAW-DEBUG] samples=%u quality=%s X[min=%d max=%d mean=%.1f rms=%.1f] Y[min=%d max=%d mean=%.1f rms=%.1f] Z[min=%d max=%d mean=%.1f rms=%.1f]\n",
                raw.count, qualityName(features.quality), minimum[0], maximum[0], sum[0] / raw.count, std::sqrt(sumSquares[0] / raw.count),
                minimum[1], maximum[1], sum[1] / raw.count, std::sqrt(sumSquares[1] / raw.count), minimum[2], maximum[2], sum[2] / raw.count, std::sqrt(sumSquares[2] / raw.count));
        }
#endif
        const auto* audioWindow = raw.audioWindow;
        // Vibration quality is independent from acoustic validity. A FIFO
        // overrun must not turn a valid/silent audio window into audio_invalid.
        bool audioValid = audioWindow && audioReady.load() &&
            raw.audioGeneration==audioErrorGeneration.load();
        AcousticFeatures acoustic;
        if (audioValid) acoustic=analyzeCommonAudioWindow(audioWindow,COMMON_AUDIO_SAMPLES);
        audioValid = audioValid && raw.audioGeneration==audioErrorGeneration.load();
        if (raw.audioWindow) {
            heap_caps_free(raw.audioWindow);
            raw.audioWindow = nullptr;
        }
        xSemaphoreTake(mutex, portMAX_DELAY);
#if RAW_VIBRATION_ENABLED
        if (pending.size() == PendingCapacity) {
            xSemaphoreGive(mutex);
            raw.quality = features.quality;
            const bool held = holdRaw(raw);
            if (!held) ++processingDrops;
            ++pendingFullEvents;
            const auto nowMs = millis();
            if (pendingFullEvents == 1 || nowMs - lastPendingFullLogMs >= 1000U) {
                Serial.printf("[PROCESSING-DIAG] pending_full %s count=%lu windowIndex=%lu raw_queue=%u pending=%u\n",
                              held ? "held" : "unavailable",
                              static_cast<unsigned long>(pendingFullEvents),
                              static_cast<unsigned long>(raw.index),
                              rawQueue ? static_cast<unsigned>(uxQueueMessagesWaiting(rawQueue)) : 0U,
                              static_cast<unsigned>(PendingCapacity));
                lastPendingFullLogMs = nowMs;
            }
            vTaskDelay(1);
            continue;
        }
        raw.quality = features.quality;
        pending.push(raw);
#else
        pending.push(features);
#endif
        latest = features;
        latestAudio=acoustic; latestAudioValid=audioValid;
        latestAudioGeneration = raw.audioGeneration;
        hasLatest = true;
        xSemaphoreGive(mutex);
#if RAW_VIBRATION_ENABLED
        vTaskDelay(1); // Keep raw backlog processing from starving the other tasks.
#endif
    }
}
String timestamp(std::uint64_t sampleUs, std::int64_t epochOffsetUs) {
    const std::int64_t epochUs = epochOffsetUs + sampleUs;
    const time_t seconds = epochUs / 1000000;
    struct tm utc;
    gmtime_r(&seconds, &utc);
    char date[32], value[40];
    strftime(date, sizeof(date), "%Y-%m-%dT%H:%M:%S", &utc);
    snprintf(value, sizeof(value), "%s.%06ldZ", date, static_cast<long>(epochUs % 1000000));
    return String(value);
}
void networkTask(void*) {
    // Frozen bytes/IDs remain identical across timeouts, auth errors and ACK loss.
    String body;
    unsigned size = 0;
    std::int64_t epochOffset = 0;
    std::uint32_t lastBatch = millis();
    std::uint32_t retryDelayMs = 2000;
    std::uint32_t retryCount = 0;
    std::uint32_t rawPostCount = 0;
    std::uint32_t lastPendingAttemptLogMs = 0;
    std::uint32_t lastHealthHandoffCloseLogMs = 0;
    bool pendingAttemptLogInitialized = false;
    std::uint64_t reportedDrops = 0, reportedAudioDrops = 0;
    bool detailedRawLogPrinted = false;
#if RAW_VIBRATION_ENABLED
    bool batchFromSpool = false;
    unsigned spoolSlots[BatchCapacity] = {};
#endif
    // Keep one owner for the Raw HTTP client.  The task never exits, so the
    // TLS client is not destroyed between requests and cannot close a shared
    // VFS handle from a per-request scope.
    WiFiClientSecure secure;
    BackendHttp http;
    while (true) {
#if RAW_VIBRATION_ENABLED
        // Raw batches are sent as soon as processing publishes one; this is
        // the only scheduler polling delay in the raw path.
        vTaskDelay(pdMS_TO_TICKS(20));
#else
        vTaskDelay(pdMS_TO_TICKS(200));
#endif
        xSemaphoreTake(mutex, portMAX_DELAY);
        const auto dropped = pending.dropped + processingDrops.load();
        xSemaphoreGive(mutex);
        if (dropped != reportedDrops) {
            Serial.printf("[WINDOW] Dropped windows=%llu (queue overflow; sequence gaps retained)\n", dropped);
            reportedDrops = dropped;
        }
        const auto droppedAudio = audioDrops.load();
        if (droppedAudio != reportedAudioDrops) {
            Serial.printf("[WINDOW] Dropped audio windows=%lu (best-effort metadata; vibration retained)\n",
                          static_cast<unsigned long>(droppedAudio));
            reportedAudioDrops = droppedAudio;
        }
        if (WiFi.status() != WL_CONNECTED) continue;
        struct timeval tv;
        gettimeofday(&tv, nullptr);
        if (tv.tv_sec < 1700000000) {
            // The API requires an RFC3339 timestamp. Reuse the existing
            // backend-time fallback instead of fabricating UTC or discarding
            // the pending window. A failed fallback leaves the batch intact.
            Serial.println("[TIME] Raw blocked: UTC invalid; trying backend fallback.");
            if (!syncTimeFromBackend()) {
                vTaskDelay(pdMS_TO_TICKS(250));
                continue;
            }
            gettimeofday(&tv, nullptr);
            if (tv.tv_sec < 1700000000) {
                Serial.println("[TIME] Raw deferred: backend fallback returned invalid UTC.");
                vTaskDelay(pdMS_TO_TICKS(250));
                continue;
            }
        }
        // One UTC anchor per boot: replays and buffered windows retain the same
        // mapping even if NTP adjusts the wall clock between batches.
        if (!epochOffset) epochOffset=static_cast<std::int64_t>(tv.tv_sec)*1000000 + tv.tv_usec - esp_timer_get_time();
        if (!size) {
#if !RAW_VIBRATION_ENABLED
            if (millis() - lastBatch < BatchIntervalMs) continue;
#endif
            xSemaphoreTake(mutex, portMAX_DELAY);
            size = std::min(pending.size(), BatchCapacity);
            for (unsigned i=0; i<size; ++i) batch[i] = pending.at(i);
            xSemaphoreGive(mutex);
#if RAW_VIBRATION_ENABLED
            // A legacy spool has no bootId and is held aside. Current-boot
            // RAM data must go first so a legacy backlog cannot stall capture.
            if (!size) {
                batchFromSpool = false;
                std::uint32_t oldestSpoolIndex = 0;
                const bool spoolAvailable = oldestRawSpoolIndex(oldestSpoolIndex);
                if (spoolAvailable) {
                    size = loadRawSpoolBatch(batch, spoolSlots, BatchCapacity);
                    batchFromSpool = size != 0;
                    if (!batchFromSpool) {
                        size = 0;
                        continue;
                    }
                }
            }
            if (!size) {
                xSemaphoreTake(mutex, portMAX_DELAY);
                size = std::min(pending.size(), BatchCapacity);
                for (unsigned i=0; i<size; ++i) batch[i] = pending.at(i);
                xSemaphoreGive(mutex);
            }
#endif
            if (!size) {
                serviceNetworkAuxiliary();
                continue;
            }
            JsonDocument doc;
            auto windows = doc["windows"].to<JsonArray>();
            for (unsigned i=0; i<size; ++i) {
                const auto& f = batch[i];
                auto w = windows.add<JsonObject>();
                w["schemaVersion"]=1; w["deviceId"]=DEVICE_ID;
                w["siteId"]=SITE_ID; w["assetId"]=ASSET_ID; w["bootId"]=boot;
                w["windowIndex"]=f.index; w["startUptimeUs"]=f.startUs;
                w["timestamp"]=timestamp(f.startUs, epochOffset);
                w["profileId"]=Profile; w["sampleRateHz"]=Rate; w["sampleCount"]=f.count;
                w["unit"]="g";
                auto axes=w["axes"].to<JsonArray>(); axes.add("X"); axes.add("Y"); axes.add("Z");
                w["quality"]=qualityName(f.quality);
#if RAW_VIBRATION_ENABLED
                w["profileId"]="adxl345-800hz-xyz-counts-v1";
                w["unit"]="count"; w["gPerCount"]=0.0039;
                w["encoding"]="base64-int16le-xyz";
                if (!encodeRaw(f,w)) {size=0; break;}
#else
                if (f.quality==Quality::Valid) {
                    auto values=w["features"].to<JsonArray>();
                    for (double v:f.values) values.add(v);
                } else w["features"]=nullptr;
#endif
            }
            if (!size || doc.overflowed()) {size=0; batchFromSpool=false; continue;}
            body="";
            serializeJson(doc, body);
#if RAW_VIBRATION_ENABLED
            if (body.length() > MaxRawRequestBytes) {
                Serial.printf("[WINDOW] batch rejected locally: %u bytes exceeds %u-byte limit\n",
                              static_cast<unsigned>(body.length()),
                              static_cast<unsigned>(MaxRawRequestBytes));
                size=0;
                continue;
            }
#endif
        }
        const String ingest(INGEST_URL);
        const String suffix("/api/telemetry/ingest");
        if (!ingest.endsWith(suffix)) continue;
        const String url=ingest.substring(0,ingest.length()-suffix.length())+
#if RAW_VIBRATION_ENABLED
                         "/api/devices/"+DEVICE_ID+"/raw-vibration-windows";
#else
                         "/api/devices/"+DEVICE_ID+"/vibration-windows";
#endif
        secure.setHandshakeTimeout(3); http.setConnectTimeout(1500); http.setTimeout(1500);
        // HTTPClient preserves a reusable connection when the response allows it.
        // Keep ownership in this task; HTTPClient::end() owns socket teardown.
        http.setReuse(true);
        http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
        HttpDiag httpDiag;
        xSemaphoreTake(mutex, portMAX_DELAY);
        const unsigned pendingBefore = pending.size();
        xSemaphoreGive(mutex);
        const std::uint32_t pendingAttemptNow = millis();
        if (!pendingAttemptLogInitialized ||
            pendingAttemptNow - lastPendingAttemptLogMs >= 1000U) {
            Serial.printf("[RAW-ATTEMPT] raw_attempt=1 begin_result=pending gate_wait_ms=0 request_ms=0 status=pending fail_stage=none pending_before=%u pending_after=%u retry_count=%lu\n",
                          pendingBefore, pendingBefore,
                          static_cast<unsigned long>(retryCount));
            lastPendingAttemptLogMs = pendingAttemptNow;
            pendingAttemptLogInitialized = true;
        }
        rawNetworkBusy.store(true);
        if (!tryReserveRawNetwork()) {
            rawNetworkBusy.store(false);
            continue;
        }
        const bool reuseCandidate = secure.connected();
        if (!beginBackendHttp(http, secure, url.c_str())) {
            xSemaphoreTake(mutex, portMAX_DELAY);
            const unsigned pendingAfterBegin = pending.size();
            xSemaphoreGive(mutex);
            Serial.printf("[RAW-ATTEMPT] raw_attempt=1 reuse_candidate=%s begin_result=fail gate_wait_ms=%lu request_ms=0 status=-1 fail_stage=begin pending_before=%u pending_after=%u retry_count=%lu\n",
                          reuseCandidate ? "yes" : "no",
                          static_cast<unsigned long>(http.lastGateWaitMs), pendingBefore,
                          pendingAfterBegin, static_cast<unsigned long>(retryCount));
            logHttpDiag("raw", url.c_str(), false, httpDiag, -1, 0, "begin");
            releaseRawNetwork();
            rawNetworkBusy.store(false);
            // Do not park the raw consumer for seconds while the producer
            // continues. The batch remains intact and is retried shortly.
            vTaskDelay(pdMS_TO_TICKS(std::min<std::uint32_t>(retryDelayMs, 500)));
            retryDelayMs = std::min<std::uint32_t>(500, retryDelayMs * 2);
            ++retryCount;
            continue;
        }
        http.addHeader("Authorization", String("Bearer ")+INGEST_TOKEN);
        http.addHeader("Content-Type", "application/json");
        const bool logRawStorageDiagnostic =
            ++rawPostCount <= 4U || (rawPostCount % 64U) == 0U;
        if (logRawStorageDiagnostic) {
            logRawBufferCanaries("raw_post_before");
            logStorageMemoryDiagnostic("raw_post_before");
        }
        const uint64_t requestStartedMs = static_cast<uint64_t>(esp_timer_get_time()) / 1000ULL;
        const int status=http.POST(body);
        httpDiag.requestMs = static_cast<uint64_t>(esp_timer_get_time()) / 1000ULL - requestStartedMs;
        if (logRawStorageDiagnostic) {
            logRawBufferCanaries("raw_post_after");
            logStorageMemoryDiagnostic("raw_post_after");
        }
        String ackBody;
        bool accepted=false;
        if (status > 0 && http.getSize()>=0 && http.getSize()<8192) {
            const uint64_t bodyStartedMs = static_cast<uint64_t>(esp_timer_get_time()) / 1000ULL;
            ackBody=http.getString();
            httpDiag.bodyMs = static_cast<uint64_t>(esp_timer_get_time()) / 1000ULL - bodyStartedMs;
            accepted = strictRawAck(status, ackBody, batch, size, boot);
        }
        const bool healthHandoffRequested =
            healthNetworkRequested.load(std::memory_order_acquire);
        if (healthHandoffRequested) {
            // Let HTTPClient close the idle Raw socket exactly once so the
            // auxiliary Health TLS connection can acquire resources.
            http.setReuse(false);
            const std::uint32_t now = millis();
            if (now - lastHealthHandoffCloseLogMs >= 1000U) {
                Serial.println("[RAW] health handoff: closing reusable socket");
                lastHealthHandoffCloseLogMs = now;
            }
        }
        http.end();
        if (healthHandoffRequested) http.setReuse(true);
        releaseRawNetwork();
        rawNetworkBusy.store(false);
        xSemaphoreTake(mutex, portMAX_DELAY);
        const unsigned pendingAfter = pending.size();
        xSemaphoreGive(mutex);
        Serial.printf("[RAW-ATTEMPT] raw_attempt=1 reuse_candidate=%s begin_result=ok gate_wait_ms=%lu request_ms=%llu status=%d fail_stage=%s pending_before=%u pending_after=%u retry_count=%lu\n",
                      reuseCandidate ? "yes" : "no",
                      static_cast<unsigned long>(http.lastGateWaitMs),
                      static_cast<unsigned long long>(httpDiag.requestMs), status,
                      accepted ? "none" : (status > 0 ? "ack" : "post"), pendingBefore,
                      pendingAfter, static_cast<unsigned long>(retryCount));
        logHttpDiag("raw", url.c_str(), true, httpDiag, status, ackBody.length(), status > 0 ? "none" : "post");
        if (accepted) {
            if (!detailedRawLogPrinted) {
                Serial.printf("[WINDOW-DETAIL] URL=%s bootId=%s windowIndex=", url.c_str(), boot);
                for (unsigned i=0; i<size; ++i) {
                    if (i) Serial.print(',');
                    Serial.print(batch[i].index);
                }
                Serial.printf(" HTTP=%d ACK=%s\n", status, ackBody.c_str());
                detailedRawLogPrinted = true;
            }
            bool cleared = true;
#if RAW_VIBRATION_ENABLED
            if (batchFromSpool && accepted && (status == 200 || status == 202))
                cleared = removeRawSpoolBatch(spoolSlots, size);
            else
#endif
            {
                xSemaphoreTake(mutex, portMAX_DELAY);
                cleared = pending.acknowledge(size);
                xSemaphoreGive(mutex);
            }
            if (!cleared) Serial.println("[RAW-SPOOL] ACK received but file removal failed; retained for retry.");
            Serial.printf("[WINDOW] ACK %u windows through index %lu\n",size,static_cast<unsigned long>(batch[size-1].index));
            size=0; body=""; batchFromSpool=false; lastBatch=millis();
            retryDelayMs = 2000;
            retryCount = 0;
            // Give the equal-priority auxiliary network tasks a bounded
            // reservation opportunity between successful Raw batches.
            if (rawQueueIsEmpty()) serviceNetworkAuxiliary();
            vTaskDelay(pdMS_TO_TICKS(300));
        } else {
            Serial.printf("[WINDOW] HTTP %d; batch retained\n",status);
            if (status == 409 && ackBody.length())
                Serial.printf("[RAW-409-BODY] %s\n", ackBody.c_str());
            vTaskDelay(pdMS_TO_TICKS(std::min<std::uint32_t>(retryDelayMs, 500)));
            retryDelayMs = std::min<std::uint32_t>(500, retryDelayMs * 2);
            ++retryCount;
        }
    }
}
bool snapshot(VibrationFeatures& vib, AcousticFeatures& audio) {
    Features f;
    std::uint32_t generation;
    bool available;
    bool hasLatestSnapshot;
    bool latestAudioValidSnapshot;
    xSemaphoreTake(mutex, portMAX_DELAY);
    available=hasLatest;
    hasLatestSnapshot=hasLatest;
    latestAudioValidSnapshot=latestAudioValid;
    f=latest; generation=latestAudioGeneration;
    audio=latestAudio;
    xSemaphoreGive(mutex);
    // The raw capture already copied the exact audio window at completion.
    // Processing/network backpressure may make the cached feature older than
    // 1.5 s; age alone does not make this vibration/audio pair unsynchronized.
    const bool validVibration=f.quality==Quality::Valid;
    const bool ready=audioReady.load();
    const std::uint32_t currentGeneration=audioErrorGeneration.load();
    if (!available || !validVibration || !ready || generation!=currentGeneration) {
        const char* reason=!hasLatestSnapshot ? "no_latest" :
            !latestAudioValidSnapshot ? "audio_invalid" :
            !validVibration ? "vibration_invalid" :
            !ready ? "audio_not_ready" : "audio_generation_changed";
        static std::uint32_t lastLogMs=0;
        const std::uint32_t now=millis();
        if (now-lastLogMs>=1000) {
            Serial.printf("[SYNC] snapshot unavailable: reason=%s generation=%lu current=%lu ready=%s quality=%u\n",
                          reason, static_cast<unsigned long>(generation),
                          static_cast<unsigned long>(currentGeneration),
                          ready ? "yes" : "no", static_cast<unsigned>(f.quality));
            lastLogMs=now;
        }
        return false;
    }
    vib.rmsX=f.values[0]; vib.rmsY=f.values[7]; vib.rmsZ=f.values[14];
    vib.totalRms=std::sqrt(vib.rmsX*vib.rmsX+vib.rmsY*vib.rmsY+vib.rmsZ*vib.rmsZ);
    unsigned axis=vib.rmsY>vib.rmsX?1:0;
    if (vib.rmsZ>f.values[axis*7]) axis=2;
    vib.fftAxis="XYZ"[axis]; vib.peakHz=f.peakHz[axis];
    if (generation!=audioErrorGeneration.load()) {
        Serial.printf("[SYNC] snapshot invalidated during copy: generation=%lu current=%lu\n",
                      static_cast<unsigned long>(generation),
                      static_cast<unsigned long>(audioErrorGeneration.load()));
        return false;
    }
    return true;
}
bool start() {
    Serial.printf("[PSRAM] found=%s size=%u free=%u\n",
                  psramFound() ? "yes" : "no",
                  static_cast<unsigned>(ESP.getPsramSize()),
                  static_cast<unsigned>(ESP.getFreePsram()));
    snprintf(boot,sizeof(boot),"%08lx%08lx%08lx%08lx",
             static_cast<unsigned long>(esp_random()),static_cast<unsigned long>(esp_random()),
             static_cast<unsigned long>(esp_random()),static_cast<unsigned long>(esp_random()));
    mutex=xSemaphoreCreateMutex();
    rawQueue=xQueueCreate(RawQueueCapacity,sizeof(Raw));
    const auto alloc = [](size_t bytes) {
        void* value = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        return value ? value : heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    };
    workspace = static_cast<Workspace*>(alloc(sizeof(Workspace)));
    batch = static_cast<PendingWindow*>(alloc(sizeof(PendingWindow) * BatchCapacity));
#if RAW_VIBRATION_ENABLED
    rawHoldAllocation = static_cast<unsigned char*>(allocateGuardedRawBuffer(sizeof(Raw) * RawHoldCapacity));
    rawHold = rawHoldAllocation ? reinterpret_cast<Raw*>(rawHoldAllocation + DiagnosticCanaryBytes) : nullptr;
    if (rawHold) for (unsigned i = 0; i < RawHoldCapacity; ++i)
        ::new (static_cast<void*>(rawHold + i)) Raw();
    rawBytesAllocation = static_cast<unsigned char*>(allocateGuardedRawBuffer(RawBytesCapacity));
    rawBytes = rawBytesAllocation ? rawBytesAllocation + DiagnosticCanaryBytes : nullptr;
    encodedRawAllocation = static_cast<unsigned char*>(allocateGuardedRawBuffer(EncodedRawCapacity));
    encodedRaw = encodedRawAllocation ? encodedRawAllocation + DiagnosticCanaryBytes : nullptr;
#endif
    Serial.printf("[PSRAM] buffers rawQueue=%u processingQueue=%u workspace=%s batch=%s",
                  RawQueueCapacity, PendingCapacity,
                  workspace && esp_ptr_external_ram(workspace) ? "psram" : "internal",
                  batch && esp_ptr_external_ram(batch) ? "psram" : "internal");
#if RAW_VIBRATION_ENABLED
    Serial.printf(" rawHold=%u(%s) rawBytes=%u(%s) encodedRaw=%u(%s)",
                  RawHoldCapacity,
                  rawHold && esp_ptr_external_ram(rawHold) ? "psram" : "internal",
                  static_cast<unsigned>(RawBytesCapacity),
                  rawBytes && esp_ptr_external_ram(rawBytes) ? "psram" : "internal",
                  static_cast<unsigned>(EncodedRawCapacity),
                  encodedRaw && esp_ptr_external_ram(encodedRaw) ? "psram" : "internal");
#endif
    Serial.println();
    if (!mutex || !rawQueue || !workspace || !batch
#if RAW_VIBRATION_ENABLED
        || !rawHold || !rawBytes || !encodedRaw
#endif
    ) return false;
    return xTaskCreatePinnedToCore(processingTask,"WindowFeatures",8192,nullptr,2,nullptr,0)==pdPASS &&
           xTaskCreatePinnedToCore(captureTask,"VibrationFIFO",8192,nullptr,4,nullptr,0)==pdPASS &&
           xTaskCreatePinnedToCore(networkTask,"WindowHTTPS",12288,nullptr,1,nullptr,1)==pdPASS;
}
}
#endif
