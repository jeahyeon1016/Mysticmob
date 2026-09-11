# ESP32-S3-WROOM N16R8 legacy LittleFS 파티션 복구 실장비 재검증

- 일시: 2026-09-11
- 대상: ESP32-S3-WROOM N16R8, COM7
- 펌웨어 환경: esp32-s3-devkitc-1-n16r8
- 범위: 기존 LittleFS 데이터 보존을 위한 파티션 복구, 업로드, 약 3분 serial 관찰
- 원본 로그: coordinator serial monitor 세션에서만 확인. 장치·네트워크 식별정보가 포함될 수 있어 저장소에는 비식별 결과만 기록

## 1. 13-14 번호별 처리 결과

| 번호 | 결과 | 요약 |
|---|---|---|
| 13-14K | PASS | 기존 spiffs 주소와 크기를 유지하는 N16R8 custom CSV 적용 |
| 13-14L | PASS | 파티션 중첩, LittleFS 라벨, 앱 크기, 데이터 보존 조건 독립 검토 |
| 13-14M | PASS | ignored CSV를 추적 가능하게 정확한 .gitignore 예외 1줄 추가 |
| 13-14N | PASS | generated partitions.bin과 업로드 조건 최종 검토 |

## 2. 업로드

- COM7 포트 확인: Silicon Labs CP210x USB to UART Bridge
- 일반 firmware upload 성공
- esptool이 ESP32-S3 QFN56 revision v0.2 및 Embedded PSRAM 8MB를 확인
- bootloader, partition table, boot app descriptor, application image 기록 및 hash 검증 성공
- filesystem image upload, LittleFS format, erase-all, storage 삭제는 수행하지 않음

## 3. 파티션 보존 근거

N16R8 custom map은 기존 n8_raw_8MB.csv와 다음 영역을 동일하게 유지한다.

| 파티션 | Offset | Size |
|---|---:|---:|
| nvs | 0x9000 | 0x5000 |
| otadata | 0xE000 | 0x2000 |
| app0 | 0x10000 | 0x330000 |
| app1 | 0x340000 | 0x200000 |
| spiffs | 0x540000 | 0x2A0000 |
| coredump | 0x7E0000 | 0x20000 |

기존 데이터가 있는 spiffs의 시작 주소와 끝 주소를 바꾸지 않았고, 새 파티션은 서로 겹치지 않으며 16MB flash 범위 안에 있다. 상위 flash 영역은 데이터 이동을 방지하기 위해 의도적으로 사용하지 않는다. LittleFS.begin(false)와 esp_littlefs_info('spiffs')의 라벨도 일치한다.

## 4. 부팅 및 3분 관찰

- LittleFS mount 성공
- LittleFS: total 2,752,512 / used 2,752,512 / free 0 bytes
- 기존 binary ring: 2,747 queued records 복원
- Raw spool catalog: scanned=512, present=348, readable=348, legacy=0, next=348, full=no
- catalog 복구 elapsed: 약 45.07초
- PSRAM: found=yes, size=8,386,215 bytes, free=8,320,267 bytes
- rawQueue high-water=1, pending=8, rawHold=8
- capture_queue_full=0
- slot=512 false-full marker: 0회
- panic, Guru Meditation, watchdog, reboot: 0회

## 5. 관측된 잔여 차단

LittleFS가 실제로 가득 차 있어 raw spool admission이 suspended=yes로 고정됐다. 따라서 저장 timing sample은 생성되지 않았고, pre-send persistence가 진행되지 않았다.

- spool_capacity_full: 1,498까지 증가
- processing_pending_full_events: 209
- actual_drop_total: 201
- raw_hold_full: 201
- pending_depth=8, raw_hold_depth=8
- HTTP entry/result, ACK, delete: 0회
- Wi-Fi reconnect timeout은 관측됐으나 이번 구간에서 Raw HTTP/TLS 진입은 확인하지 못함

이번 결과는 cursor 충돌 및 slot=512 false-full 수정의 실장비 PASS다. 동시에 기존 2.75MB 저장공간 포화 때문에 capture 이후 bounded pending/rawHold가 소진되어 실제 window drop이 발생하는 별도 BLOCKED 상태를 확인했다. 전용 writer 격리로 capture_queue_full은 0이었지만, 저장공간이 가득 찬 상태에서는 downstream 보류 큐 자체가 무한 손실 방지가 될 수 없다.

## 6. 결론과 다음 번호

- 13-14 목표: PASS — 충돌한 slot 하나 때문에 빈 슬롯을 무시하고 full 판정하는 오류는 재현되지 않음
- 현재 firmware rollout health: BLOCKED — LittleFS 실제 full, HTTP 미진입, actual drop 201
- 다음 작업은 파티션/포맷/삭제가 아니라, 먼저 백업·복구 검증을 포함한 저장공간 마이그레이션 계획을 별도 승인해야 한다.
- MotorDiagnosis는 commit/push/PR하지 않았다.
