# ESP32 N8 Raw spool cursor 충돌 수정 실장비 재검증

- 일시: 2026-09-11
- 대상: `esp32-s3-devkitc-1-n8`, COM7
- 범위: agent 수정 결과의 native/N8 검증, firmware-only upload, 약 246초 serial 로그 분석
- 원본 로그: 로컬 `evidence/serial/logs/device-monitor-260911-172600.log` (장치·네트워크 식별정보 포함으로 push 제외)

## 1. Agent 수정 결과

- `RawSpoolIndex::nextEmpty(startSlot)`가 현재 cursor부터 512개 catalog를 원형 탐색한다.
- 판독 불가 파일은 `Unreadable`로 격리하고 빈 슬롯으로 오인하지 않는다.
- 기존 파일 형식, CRC, atomic rename, ACK 후 삭제, 외부 API와 queue 계약은 유지했다.

## 2. 검증 및 업로드

- Native: `178/178` 통과
- N8 build: 성공
- `git diff --check`: 오류 없음(기존 줄바꿈 경고만 존재)
- COM7 firmware-only upload 및 flash hash 검증: 성공
- filesystem image upload, format, erase-all, partition 변경: 없음

## 3. 실장비 관측

- Boot catalog: `scanned=512`, `present=269`, `readable=269`, `next=62`, `full=no`
- Boot catalog 복구 시간: `32,644,041 us`
- 신규 spool 저장: 79회. slot 62부터 충돌 슬롯을 건너뛰며 저장.
- `slot=512` false-full marker: 0회
- HTTP 진입/HTTP 결과/ACK/delete marker: 0회
- TLS `start_ssl_client: -1`: 3회. Raw HTTP 미진입이라 Raw TLS 원인으로 확정하지 않음.
- panic, Guru Meditation, watchdog, 추가 reboot: 0회

## 4. 실제 차단 원인

관측 중 LittleFS가 `total=2,752,512`, `used=2,752,512`, `free=0`에 도달했다. 이후 `spool_capacity_full`이 증가했고, `actual_drop_total`은 233, `capture_queue_full`은 233, `processing_pending_full_events`는 11, `raw_queue`와 `pending`은 각각 8로 포화됐다.

이는 cursor false-full과 다르다. 이번 수정은 논리 슬롯 충돌을 해결했지만, 동기식 LittleFS 저장이 1.28초를 초과하는 기존 병목과 물리 저장공간 고갈은 남아 있다. pre-send durability가 실패해 HTTP로 진행하지 못한 것도 별도 경계다.

## 5. 판정

- cursor 충돌 수정: **PASS**
- 업로드 및 부팅: **PASS**
- 무손실 capture/processing: **FAIL**
- HTTP/ACK end-to-end: **미검증**
- rollout: **BLOCKED**

다음 수정은 저장 writer 지연이 capture task를 정지시키지 않는 최소 경계부터 agent에 요청한다. 고정 ring 확대, format/erase, partition 변경, 무제한 queue, TLS 동작 변경은 별도 승인 전 보류한다.
