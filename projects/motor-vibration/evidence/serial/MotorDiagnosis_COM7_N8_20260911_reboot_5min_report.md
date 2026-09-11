# COM7 N8 reboot 5-minute hardware report

- Capture: `MotorDiagnosis_COM7_N8_20260911_reboot_5min.log`
- Capture time: 2026-09-11 15:27:03 +0900, 300 seconds, COM7 / 115200 baud
- Firmware: current `D:\github\MotorDiagnosis` worktree, PlatformIO `esp32-s3-devkitc-1-n8`
- Reset: GPIO0 high 유지 후 EN pulse; 정상 애플리케이션 부팅 확인
- Scope: firmware-only reset/log capture. Upload, format, erase, partition change 없음.

## Result

**결론: BLOCKED. 앱은 정상 부팅했지만 Wi-Fi가 연결되지 않아 전송 검증에 진입하지 못했고, 캡처 queue 포화로 실제 window 손실이 계속 발생했다.**

## Boot and recovery

| Item | Result |
|---|---|
| ROM boot | `rst:0x1 (POWERON), boot:0x8 (SPI_FAST_FLASH_BOOT)` |
| Firmware | `v1.3-signal-analysis.1` |
| Boot session | `1946` |
| LittleFS | mount success, automatic format disabled |
| Total / used | `2,752,512` / `2,363,392` bytes at admission |
| Restored queue | `2747 / 24999` |
| Invalid slots | CRC/schema-invalid `0`, legacy v1 `0` |
| Active unresolved timestamps | `24` |
| PSRAM | found, free `8,320,267` bytes at boot |
| Sensor | ADXL345 ID `0xE5` |

## Runtime evidence

- Raw samples continued with `samples=512`, `quality=valid`.
- Initial spool admission was accepted with `free=389,120`, then one new record was stored at slot 0.
- The separate raw spool reached `slot=512`; subsequent writes reported `RAW-SPOOL-FULL` with `free=385,024` and `need=3133`.
- `raw_queue_depth=8`, `pending_depth=8`; both queues remained saturated.
- Final observed `capture_queue_full=331`, `actual_drop_total=331`, and `Dropped windows=332`.
- `processing_pending_full_events=178`, `processing_pending_full_unique=1`.
- `AUDIO Requested synchronized window was overwritten` occurred 76 times.
- Storage timing was consistently slow: 187 samples, all in `storage_total_gt_1280ms`; maximum `1,945,610 us`.
- No panic, Guru Meditation, watchdog, assert, or additional reboot occurred after the initial boot.

## Network and transmission

- Wi-Fi startup stayed offline.
- Reconnect timeout: 27 times.
- No confirmed Wi-Fi connection, NTP synchronization, HTTP request, HTTP 200/202/409, TLS diagnostic, ACK, or delete event.
- Therefore this capture cannot prove or disprove the lower-level TLS `-1` cause.

## Assessment

The current direct failure boundary is queue/storage backpressure, not a confirmed firmware crash. The device restores its backlog and acquires valid samples, but the raw spool/processing path cannot drain while Wi-Fi is unavailable. The 512-slot raw spool limit and multi-second LittleFS timing leave the capture queues full, causing actual window loss.

The next diagnostic run needs a reachable Wi-Fi/backend path before judging selector or TLS behavior. Do not format, erase, enlarge queues speculatively, or change partitions from this result alone.
