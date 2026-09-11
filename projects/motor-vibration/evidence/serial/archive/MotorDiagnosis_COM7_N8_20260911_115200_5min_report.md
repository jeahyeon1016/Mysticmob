# COM7 N8 115200 5-minute hardware report

- Capture: `MotorDiagnosis_COM7_N8_20260911_115200_5min.log`
- Duration: 300.046 s (UTC 2026-09-10 16:10:01.470098 to 16:15:01.525159)
- Firmware: current `D:\github\MotorDiagnosis` worktree, PlatformIO `esp32-s3-devkitc-1-n8`; no source edits, commit, or push.
- Serial: COM7 / 115200; log contains 1,260 lines / 126,838 bytes.

## Quantitative results

| Item | Result |
|---|---|
| Boot/reset | boot sessions 1721, 1722; 2 POWERON lines and 1 `RTC_SW_CPU_RST`; one LittleFS assert/backtrace reboot |
| AUDIO overwrite/drop | No AUDIO overwrite/drop counter emitted; no audio-overrun/drop line. `audioRing=ok` on all 10 canary checks. Persistent `[BUFFER] dropped=0` (not an audio counter). |
| Processing drops | `[WINDOW] Dropped windows`: 26 first observed, 395 final/max; 101 cumulative log updates. |
| `pending` / `raw_queue` | 250 `pending_full` diagnostics; pending stayed 8/8; raw_queue ranged 0..8 and saturated at 8. pending_full count reached 1,640. |
| Raw HTTP | 102 attempts: 83x HTTP 200, 1x 202, 17x -1, 1x -11. Positive HTTP request_ms min/mean/p95/max = 448/744.2/923/3603 ms. All 84 positive responses were `fail_stage=ack`; actual accepted ACK was 0. |
| Raw reuse | 83 `reuse_candidate=yes`, 19 `no`; successful steady-state requests were reuse=yes. |
| Raw total difference | Successful HTTP responses had `total_ms-request_ms` min/mean/p95/max = 26/39/54/57 ms, so the former 13–22 s auxiliary inflation was removed. Persistent queue 1,968 -> 1,987 (+19); processing drops reached 395. |
| ACK index | No `ACK ... through index` line was emitted because every 200/202 response failed strict ACK validation. |
| Telemetry Queue | No `[TELEMETRY-*]`, telemetry queue, or telemetry HTTP status lines emitted; not measurable from this capture. |
| `/api/health` | Backend health fallback logged HTTP error -1 twice; no HTTP-DIAG health status/request_ms line emitted. |
| `/api/telemetry` | No telemetry HTTP status/request_ms line emitted. |
| LittleFS | Mount succeeded; queue restored; then `assert failed: lfs_file_close lfs.c:6080 (lfs_mlist_isopen(...))`, backtrace, reboot. |
| Heap/stack | Internal free 188,988..190,100 B; internal minimum 124,784..178,968 B. PSRAM free 8,209,007..8,209,439 B; PSRAM minimum 8,135,983..8,168,503 B. All sampled heap flags true. |
| Canary | 10 checks; rawHold/rawBytes/encodedRaw/audioRing all `ok`; no bad canary. |
| Wi-Fi/NTP | Wi-Fi connected twice; IP 172.20.10.2; RSSI -52 then -49 dBm. NTP synchronized in both sessions; durable anchors recorded. |
| Panic/reset | One LittleFS assert + backtrace and subsequent `RTC_SW_CPU_RST`; no `[CRITICAL]` line. |

## Causal sequence and assessment

1. Session 1721 booted offline, then Wi-Fi connected; NTP/backend fallback initially failed (-1). `pending=8` and raw_queue grew to 8 while raw persistence/upload was active.
2. At capture line 131 the LittleFS `lfs_file_close` assertion fired, followed by backtrace and reboot. Session 1722 restored the ring at 1,969 records and resumed acquisition.
3. Session 1722 repeated queue saturation and raw post failures (-1, then one -11), then the transport recovered to HTTP 202 followed by HTTP 200 responses, mostly with socket reuse. Firmware rejected every positive response at `fail_stage=ack`, retained the same batch, and never emitted `[WINDOW] ACK`; processing drops therefore rose monotonically to 395 while pending/raw queues remained full.
4. Coordinator source comparison found the protocol mismatch: the server returns integer `accepted` (new-window count, including `0` for duplicate HTTP 200), while firmware `strictRawAck()` requires boolean `accepted=true`. This makes both valid 202 and duplicate 200 responses fail locally.
5. ELF address resolution maps the reboot path to `syncTimeFromBackend()` -> `BackendHttp::~BackendHttp()` -> `HTTPClient::~HTTPClient()` -> `WiFiClientSecure::stop()` -> VFS `close()` -> `lfs_file_close`, indicating a stale/double network descriptor close colliding with a LittleFS file descriptor rather than audio/raw-buffer corruption.

## Evaluation

- Improved/working: N8 upload and boot, sensor acquisition, Wi-Fi/NTP recovery, LittleFS mount/recovery before the assert, HTTP raw success recovery, socket reuse, heap health, and canaries.
- Resolved: `[AUDIO] Requested synchronized window was overwritten.` fell to 0 occurrences; all sampled buffer canaries remained healthy. Raw diagnostic `total_ms` no longer includes auxiliary traffic on successful requests.
- Improved: TLS transport recovered and connection reuse worked, but this did not result in data consumption because ACK validation rejected the responses.
- Remaining: integer-vs-boolean strict ACK contract mismatch blocks all Raw consumption; LittleFS close assert causes a reboot; queue backpressure remains severe (`pending=8`, `raw_queue=8`) with 395 dropped windows. Telemetry and independent health HTTP metrics were starved/unobservable during the blocked Raw batch.
