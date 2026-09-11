# COM7 N8 hotspot reboot 5-minute hardware report

- Capture: `MotorDiagnosis_COM7_N8_20260911_hotspot_reboot_5min.log`
- Capture time: 2026-09-11 15:39:49 +0900, 300 seconds, COM7 / 115200 baud
- Firmware: `D:\github\MotorDiagnosis`, PlatformIO `esp32-s3-devkitc-1-n8`
- Reset: GPIO0 high 유지 후 EN pulse; 정상 애플리케이션 부팅
- Hotspot: Wi-Fi `jmc`, IP `172.20.10.2`, RSSI `-41 dBm`
- Scope: firmware-only reset/log capture. Upload, format, erase, partition change 없음.

## Result

**결론: BLOCKED. 핫스팟으로 Wi-Fi/NTP는 정상화됐지만 TLS health 요청은 `-1`로 실패했고, Raw selector는 `priority_scan_begin` 이후 HTTP 단계로 진행하지 못했다. queue/storage backpressure로 실제 window 손실도 계속 발생했다.**

## Boot and recovery

| Item | Result |
|---|---|
| ROM boot | `rst:0x1 (POWERON), boot:0x2b (SPI_FAST_FLASH_BOOT)` |
| Firmware | `v1.3-signal-analysis.1` |
| Boot session | `1947` |
| LittleFS | mount success, automatic format disabled |
| Total / used | `2,752,512` / `2,367,488` bytes at admission |
| Restored queue | `2747 / 24999` |
| Invalid slots | CRC/schema-invalid `0`, legacy v1 `0` |
| Active unresolved timestamps | `24` |
| PSRAM | found, free `8,320,267` bytes at boot |
| Sensor | ADXL345 ID `0xE5` |

## Runtime evidence

- Raw samples continued with `samples=512`, `quality=valid`.
- Two new spool records were stored, then the raw spool repeatedly reported `slot=512` full.
- Full condition: total `2,752,512`, used `2,375,680`, free `376,832`, required `3,133` bytes.
- `raw_queue_depth=8`, `pending_depth=8`; both queues reached and remained at capacity.
- Final `capture_queue_full=419`, `actual_drop_total=419`.
- Final `processing_pending_full_events=174`, `processing_pending_full_unique=1`.
- `spool_capacity_full=182`, `spool_write_fail=0`, `spool_corrupt_or_unreadable=0`.
- Audio overwrite message count: `0`.
- Storage timing: 184 samples, all in `storage_total_gt_1280ms`; maximum `2,463,322 us`.
- No panic, Guru Meditation, watchdog, assert, or reboot occurred after boot.

## Network and transmission

- Wi-Fi connected successfully; RSSI was `-41 dBm`.
- NTP synchronized successfully and a durable timestamp anchor was recorded.
- Backend health HTTPS failed once with `start_ssl_client: -1` / HTTP error `-1`.
- Raw TX reached `loop_ready`, `time_ready`, and `priority_scan_begin`.
- No `RAW-SELECT-DIAG`, `http_begin`, HTTP 200/202/409, Raw ACK, or delete event appeared.

## Assessment

The hotspot removed the previous Wi-Fi-offline blocker, so this run proves the device can associate and synchronize time. It also confirms that the remaining failure is not only Wi-Fi availability: TLS health still fails, Raw selection stalls before HTTP, and the 512-slot spool plus slow LittleFS path saturates both queues and causes actual drops. TLS lower-level cause is not fully proven because the Raw HTTP path was not reached.

Do not format, erase, enlarge queues speculatively, or change partitions from this result alone. The next run should correlate the selector stall with the TLS health failure while preserving the current backlog.
