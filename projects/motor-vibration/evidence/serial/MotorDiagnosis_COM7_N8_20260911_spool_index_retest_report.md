# ESP32 N8 Raw spool index 실장비 재검증

- 일시: 2026-09-11
- 대상: `esp32-s3-devkitc-1-n8`, COM7
- 범위: firmware upload와 serial 진행 경계 확인
- 원본 로그: 로컬 `evidence/serial/logs/device-monitor-260911-164910.log` (네트워크·장치 식별자 포함으로 push 제외)

## 1. 업로드

- PlatformIO 표준 upload 성공
- bootloader, 동일 partition descriptor, boot app descriptor, application image hash 검증 성공
- filesystem image upload, LittleFS format, erase-all 미실행

## 2. 통과한 경계

- 정상 부팅, LittleFS mount, Wi-Fi/NTP 동기화
- raw spool boot recovery: `scanned=512`, `present=265`, `readable=265`, corrupt=0
- runtime persisted selector: filesystem stat/header read 0회, lock 비경합 시 `elapsed_us=84`
- pending selector: 최대 4개 선택, `elapsed_us=283~397`
- panic, watchdog, Guru Meditation, 추가 reboot 없음

## 3. 실패한 경계

1. boot index recovery가 265개 header를 읽는 데 `32,540,827 us` 소요됐다.
2. boot cursor는 첫 빈 slot `42`를 선택했고 window 0 저장은 성공했다.
3. cursor가 43으로 이동했지만 43이 사용 중이자 다른 빈 slot을 순환 탐색하지 않고 `slot=512` full을 반환했다.
4. 당시 `present=266/512`, filesystem free `335,872 bytes`였으므로 실제 용량 부족이 아닌 cursor 충돌에 의한 false-full이다.
5. pre-send persistence가 9회 실패해 `http_begin=0`, raw ACK/delete=0이었다.
6. 약 85초 시점 `actual_drop_total=59`, `spool_capacity_full=29`, `raw_queue=8`, `pending=8`이었다.
7. backend health에서 TLS `start_ssl_client: -1`이 1회 재현됐다. Raw HTTP는 미진입이므로 Raw TLS 결과는 판정할 수 없다.

## 4. 코드 연결

`continuous_vibration_runtime.h::writeRawSpool()`은 `rawSpoolNextSlot` 한 곳만 `rawSpoolPresent()`로 확인한다. 해당 slot이 사용 중이면 다른 511개 catalog 상태를 확인하지 않고 `RawSpoolSlots` 값을 그대로 full 판정에 사용한다.

## 5. 판정

- selector 리팩토링: **PASS** — 반복 filesystem scan 정체 제거 확인
- 저장 cursor 및 end-to-end 전송: **FAIL** — false-full이 durability gate에서 HTTP를 차단
- rollout: **BLOCKED**
- 다음 최소 변경: RAM catalog에 cursor 기준 순환 empty-slot 선택을 추가하고 native 경계 테스트 후 재업로드
