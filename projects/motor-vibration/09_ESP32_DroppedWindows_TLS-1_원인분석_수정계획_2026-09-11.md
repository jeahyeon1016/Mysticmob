# ESP32 `Dropped windows` 및 TLS `-1` 원인분석·수정계획

- 작성일: 2026-09-11
- 대상 저장소: `D:\github\MotorDiagnosis`
- 대상 firmware: `firmware\esp32_edge_node`
- 대상 환경: `esp32-s3-devkitc-1-n8`
- 대상 장치: `COM7`
- 선행 문서: `08_ESP32_업로드후_오류_원인분석_수정지침_2026-09-11.md`
- 목적: 최종 3분 실장비 로그에 남은 `Dropped windows=222`와 간헐적 TLS `status=-1`을 추측으로 단정하지 않고, 계측으로 원인을 분리한 뒤 누락 없이 수정·검증한다.

## 0. 작업 원칙

1. 이 문서의 `확정`은 현재 코드 또는 실장비 로그로 직접 확인된 사실만 뜻한다.
2. `강한 정황`은 여러 증거가 같은 방향을 가리키지만 세부 단계가 아직 계측되지 않은 상태를 뜻한다.
3. `미확정` 항목은 테스트 없이 원인으로 단정하거나 바로 수정하지 않는다.
4. `DROP-*`, `TLS-*`, `INT-*` 순서로 진행하며, 각 번호의 완료 조건을 충족하지 못하면 다음 번호를 완료 처리하지 않는다.
5. MotorDiagnosis의 기존 dirty 변경을 되돌리거나 덮어쓰지 않는다.
6. `secrets.h`, 토큰, 인증서 원문, Wi-Fi 비밀번호를 출력·문서화·커밋하지 않는다.
7. LittleFS format, 전체 erase, partition 변경은 이 계획의 범위가 아니다.
8. 수정 전후 같은 N8 환경과 같은 장치에서 비교한다.
9. 단순 queue 확대나 timeout 확대만으로 완료 처리하지 않는다. 병목을 늦출 수는 있지만 제거했다는 증거가 아니기 때문이다.

## 1. 관측 사실

### 1.1 최종 실장비 로그에서 확정된 사실

1. ESP32 부팅, ADXL345 초기화, Wi-Fi 연결, NTP 또는 backend time 동기화는 성공했다.
2. 최종 3분 관찰 중 stack canary, Guru Meditation, watchdog, 의도하지 않은 reboot는 발생하지 않았다.
3. `WindowFeatures` stack high-water는 task stack 확장 후 약 11,892 bytes로 관측됐다.
4. Raw spool 저장 로그는 계속 출력됐다.
5. 최종 관찰 중 `RAW-SPOOL-FULL`은 출력되지 않았다.
6. `Dropped windows`는 0에서 시작해 222까지 증가했다.
7. 저장된 `windowIndex`가 연속 `0~10` 이후 `15`, `30`, `36`, `53`처럼 벌어졌다. 누락된 index는 전송 단계가 아니라 capture/processing 진입 전에 이미 유실됐음을 나타낸다.
8. 다른 재시험에서는 TLS `status=-1`이 여러 번 나온 뒤 동일 Raw batch가 HTTP 202, strict ACK 3/3으로 성공했다.
9. TLS 실패 때 `begin_result=ok`, `fail_stage=post`, `secure.lastError=-1`, 문자열은 `ERROR - Generic error`였다.
10. TLS 재시도는 2, 4, 8, 16초 계열의 bounded backoff로 동작했고, 과거의 500 ms hot loop는 재현되지 않았다.
11. PC에서 `motordiagnosis-api.duckdns.org:443` TCP 연결과 HTTPS 응답은 성공했다. 이는 확인 시점에 서버 443 포트가 전역적으로 완전히 중단된 상태는 아니었다는 뜻일 뿐, ESP32 경로의 정상성을 증명하지는 않는다.

### 1.2 현재 코드에서 확정된 사실

1. Raw window 생성 주기는 `512 samples / 800 Hz = 0.64초`다.
2. `RawQueueCapacity=8`, `PendingCapacity=8`, `RawHoldCapacity=8`이다.
3. capture task에서 `xQueueSend(rawQueue, ..., 0)`이 실패하면 `processingDrops`가 증가한다.
4. processing task가 저장과 fallback에 모두 실패해도 `processingDrops`가 증가한다.
5. `[WINDOW] Dropped windows`는 `pending.dropped + processingDrops`다.
6. processing task는 Raw 1건마다 `writeRawSpool()`을 호출한다.
7. `writeRawSpool()`은 최대 512개 slot을 0번부터 검사해 첫 빈 slot을 찾는다.
8. 저장은 개별 파일의 temp 생성, header+3,072-byte sample write, flush, close, rename 순서다.
9. Raw spool과 기존 telemetry binary ring은 같은 LittleFS와 같은 LittleFS lock을 공유한다.
10. network task는 spool file을 다시 scan/read/CRC 검증한 뒤 batch를 만든다.
11. Raw HTTPS는 handshake timeout 3초, connect/read timeout 1,500 ms를 설정한다.
12. 다른 telemetry 경로에는 handshake 10초, connect/read 5~10초 설정이 존재한다.
13. HTTP 200/202와 전체 strict ACK가 일치할 때만 Raw spool record를 삭제한다.
14. TLS `-1`에서는 batch와 spool record를 보존한다.

## 2. 문제별 현재 판정

### 2.1 `Dropped windows=222`

판정: **직접 실패 지점은 확정, 내부 병목 세부 단계는 미확정**

확정된 직접 실패 지점:

1. `Dropped windows`는 서버가 데이터를 거부해서 증가하는 값이 아니다.
2. capture가 `rawQueue`에 넣지 못했거나 processing fallback이 실패할 때 증가한다.
3. 최종 로그에서 spool이 가득 차지 않았는데도 drop이 증가했다.
4. 따라서 최종 시험의 직접 실패는 LittleFS 용량 부족보다 capture→processing 처리량 부족 또는 해당 경로의 대기다.

강한 정황:

1. 0.64초마다 Raw가 생성되지만 spool 저장 index가 점차 크게 벌어졌다.
2. file-per-window 저장은 slot 수가 늘수록 첫 빈 slot을 찾기 위한 검사 횟수가 증가한다.
3. temp/write/flush/rename과 다른 LittleFS 사용자가 같은 lock을 공유한다.
4. 따라서 현재 저장 방식의 scan, lock wait 또는 file lifecycle 중 하나 이상이 평균 생성 속도를 따라가지 못하는 정황이 강하다.

아직 확정하지 않는 사항:

1. slot scan이 가장 큰 병목인지 아직 모른다.
2. `flush`, `rename`, 기존 telemetry ring lock 점유 중 무엇이 가장 큰지 아직 모른다.
3. 단순히 queue를 크게 하면 장시간 무손실이 보장된다고 볼 수 없다.
4. TLS 지연이 drop을 악화시킬 수는 있지만, 현재 로그만으로 TLS를 drop의 유일한 직접 원인이라고 할 수 없다.

### 2.2 TLS `status=-1`

판정: **전송 계층 실패는 확정, 하위 원인은 미확정**

확정된 내용:

1. `status=-1`은 HTTP status code를 받기 전에 전송이 실패했다는 뜻이다.
2. 현재 로그에서는 `http.begin()` 이후 `POST()` 구간에서 실패했다.
3. 같은 endpoint와 payload 계약으로 이후 HTTP 202가 성공했으므로 URL, 인증서, 인증 정보가 항상 틀린 영구 오류로 단정할 수 없다.
4. 실패 후 batch는 삭제되지 않고 동일 내용으로 재시도됐다.

현재 로그만으로 구분할 수 없는 후보:

1. DNS 지연 또는 일시 실패
2. TCP connect 지연 또는 reset
3. TLS handshake timeout 또는 TLS socket 오류
4. Wi-Fi 순간 손실
5. Caddy/backend 앞단의 일시 지연
6. reusable connection의 stale socket
7. ESP32 내부 네트워크 자원 부족

주의:

1. `secure.lastError=-1 / Generic error`만으로 위 후보 중 하나를 선택하면 과도한 추측이다.
2. PC의 HTTPS 성공은 서버가 살아 있다는 참고 증거이며, ESP32 hotspot·무선·TLS 경로를 대체 검증하지 않는다.

## 3. 용량 관련 별도 게이트

1. 최종 부팅 로그의 LittleFS 값은 total 2,752,512 bytes, used 1,519,616 bytes, free 약 1,232,896 bytes였다.
2. 현재 Raw slot 크기는 header 61 bytes + samples 3,072 bytes = 약 3,133 bytes다.
3. filesystem overhead를 제외한 단순 계산에서도 당시 추가 저장 가능량은 약 393개, 약 251초 분량이다.
4. 5분 분량은 `ceil(300 / 0.64) = 469`개가 필요하다.
5. 따라서 기존 spool이 남아 있던 최종 관측 시점의 여유 공간만으로는 “추가 5분 무손실”을 증명할 수 없다.
6. 이는 partition이 영구적으로 부족하다는 결론이 아니다. 기존 Raw spool이 ACK 후 삭제되면 free space가 회복될 수 있으므로, backlog drain 후 다시 측정해야 한다.

## 4. 번호별 수정 계획 — Drop 경로

### DROP-01 — 실패 카운터를 원인별로 분리

목적: 하나의 `Dropped windows` 숫자를 실제 실패 지점으로 나눈다.

대상:

- `include\continuous_vibration_runtime.h`
- 필요하면 작은 pure counter helper와 native test 1개

수정:

1. `capture_queue_full`
2. `processing_pending_full`
3. `raw_hold_full`
4. `spool_write_fail`
5. `spool_capacity_full`
6. `spool_corrupt_or_unreadable`

로그 조건:

1. 값이 변할 때만 출력한다.
2. 1초에 한 번 이하로 rate limit한다.
3. 총합과 원인별 값이 일치해야 한다.

완료 조건:

1. 어떤 코드 분기에서 222건이 증가했는지 다음 실장비 로그 한 번으로 구분할 수 있다.
2. 기존 `windowIndex` 증가 및 gap 보존 동작은 바뀌지 않는다.

### DROP-02 — 저장 단계 시간 계측

목적: 추측 없이 실제 병목 단계를 찾는다.

계측 항목:

1. `spool_lock_wait_us`
2. `spool_slot_lookup_us`
3. `spool_temp_open_us`
4. `spool_write_us`
5. `spool_flush_close_us`
6. `spool_rename_us`
7. `spool_total_us`
8. `raw_queue_depth_before/after`
9. `spool_ready_count`

출력 방식:

1. 매 window 전체 로그는 금지한다.
2. 10초 단위 count, average, max 또는 고정 bucket histogram만 출력한다.
3. 640,000 us를 넘긴 횟수를 별도 출력한다.

완료 조건:

1. offline 3분과 online 3분에서 병목 상위 단계가 숫자로 확인된다.
2. 계측 자체 때문에 drop이 증가하지 않도록 serial 출력량을 제한한다.

### DROP-03 — 통제 시험으로 TLS 영향 분리

목적: 저장 처리량 문제와 HTTPS 지연 영향을 분리한다.

시험:

1. 시험 A: Wi-Fi 연결, Raw POST를 의도적으로 보내지 않는 진단 모드에서 3분 capture+spool.
2. 시험 B: 정상 endpoint와 기존 timeout으로 3분 capture+spool+POST.
3. 시험 C: spool occupancy가 낮을 때와 높을 때 각각 3분.

판정:

1. A에서도 drop이 증가하면 TLS 없이도 저장 경로가 생성 속도를 못 따라간다.
2. A는 통과하고 B만 실패하면 network ownership 또는 공유 lock 영향을 우선 조사한다.
3. occupancy가 높을수록 `slot_lookup_us`가 증가하면 선형 slot scan 병목이 확인된다.

완료 조건:

1. A/B/C 결과와 firmware SHA를 같은 보고서에 기록한다.
2. 결과 없이 DROP-04 구현안을 확정하지 않는다.

### DROP-04 — 확인된 병목의 최소 수정

DROP-03 결과에 따라 아래 한 경로만 선택한다.

경로 A — slot scan 병목이 확인된 경우:

1. 매 write마다 0~511 전체를 찾지 않는다.
2. 부팅 시 한 번만 기존 slot 상태를 복구한다.
3. 이후 next-write cursor와 free-slot metadata로 O(1) 또는 bounded lookup을 사용한다.
4. cursor metadata는 전원 차단 후 복구 가능해야 하며 기존 record를 덮어쓰지 않는다.

경로 B — temp/flush/rename lifecycle이 병목인 경우:

1. 기존 `main.cpp`의 고정 크기 binary ring 패턴을 재사용한다.
2. Raw 전용 고정 slot ring을 한 개 파일 또는 고정 offset 구조로 만든다.
3. 개별 파일 생성·rename을 window마다 반복하지 않는다.
4. physical spare slot을 1개 두고 새 record write+CRC 검증 후에만 이전 slot을 재사용한다.
5. 별도 범용 storage framework는 만들지 않는다.

경로 C — LittleFS lock wait가 병목인 경우:

1. capture task는 계속 LittleFS를 호출하지 않는다.
2. Raw persistence owner를 하나로 제한한다.
3. 기존 telemetry ring의 긴 lock 구간을 계측 결과가 가리키는 최소 범위만 줄인다.
4. lock 제거 대신 ownership과 임계 구역을 명시한다.

완료 조건:

1. 저장 평균뿐 아니라 p95와 max가 640 ms 생성 주기를 감당한다.
2. 기존 record hash와 ACK 삭제 규칙이 유지된다.

### DROP-05 — burst buffer는 보조 수단으로만 사용

조건: 평균 저장 속도는 640 ms보다 빠르지만 순간 지연 때문에 drop이 생길 때만 수행한다.

수정 원칙:

1. PSRAM SPSC buffer를 사용한다.
2. producer와 consumer를 각각 하나로 유지한다.
3. buffer 크기는 측정된 최대 burst 시간으로 계산한다.
4. buffer가 durable storage를 대체한다고 문서화하지 않는다.
5. 평균 저장 속도가 느린 상태에서 capacity만 확대하지 않는다.

완료 조건:

1. buffer high-water가 capacity에 닿지 않는다.
2. 10분 시험 후 backlog가 계속 증가하지 않고 회복된다.

### DROP-06 — 저장 용량 및 전원 차단 검증

검증:

1. 기존 backlog를 정상 ACK로 drain한 뒤 LittleFS free bytes를 다시 측정한다.
2. 469개 Raw record와 metadata/filesystem overhead가 실제로 들어가는지 확인한다.
3. record 생성 직후, batch 선택 직후, ACK 직전의 세 지점에서 전원 차단 복구를 시험한다.
4. reboot 후 bootId, windowIndex, startUs, sample CRC, mode, reason이 같아야 한다.
5. strict ACK 전에는 record를 삭제하지 않는다.

완료 조건:

1. 5분 용량이 실제 장치에서 증명된다.
2. 용량이 부족하면 자동 format이나 overwrite를 하지 않고 명시적 blocked 상태로 보고한다.

## 5. 번호별 수정 계획 — TLS `-1`

### TLS-01 — 진단 정보 보강

목적: `Generic error`를 실제 실패 단계로 좁힌다.

추가 로그:

1. attempt ID와 동일 batch ID
2. `WiFi.status`, RSSI, local IP 존재 여부
3. request 전후 internal heap/PSRAM free 및 minimum free
4. `http.begin()` 결과와 소요시간
5. `POST()` 소요시간
6. HTTPClient error string
7. `secure.lastError` code/string
8. reuse candidate 여부와 socket connected 상태
9. 실패 직전 DNS lookup을 수행했다면 lookup 결과와 시간

제약:

1. token, certificate, payload samples는 로그에 출력하지 않는다.
2. DNS/TCP/TLS를 분리할 수 없는 HTTPClient 한계를 로그에 명시한다.
3. 상세 ESP-TLS 로그는 진단 build에서만 활성화한다.

완료 조건:

1. 다음 `-1`을 DNS/TCP/TLS/read 또는 여전히 미분류 중 하나로 보고할 수 있다.
2. 미분류이면 억지로 하위 원인을 확정하지 않는다.

### TLS-02 — timeout A/B 시험

목적: 현재 Raw timeout이 실제 hotspot/backend 지연에 비해 너무 짧은지 확인한다.

시험군:

1. A: 현재 Raw 설정 — handshake 3초, connect/read 1.5초
2. B: 기존 health 계열 설정 — handshake 10초, connect/read 5초
3. 각 시험은 같은 firmware 기능, 같은 payload 수, 같은 장소에서 최소 20회 시도한다.

판정:

1. B에서 `-1` 비율이 유의하게 감소하고 heap/drop이 악화되지 않을 때만 timeout 변경을 채택한다.
2. timeout 증가로 network task 대기만 길어지고 성공률이 개선되지 않으면 되돌린다.
3. 성공 Raw 요청이 약 10초 걸린 과거 로그는 후보 근거지만 timeout 부족의 확정 증거로 사용하지 않는다.

완료 조건:

1. 시도 수, 성공 수, `-1` 수, p50/p95 request time을 기록한다.

### TLS-03 — 실패 socket 재사용 여부 A/B 시험

목적: stale reusable socket 가설을 검증한다.

시험:

1. A: 현재 reuse 동작
2. B: `status<=0`일 때만 reuse를 끄고 `http.end()` 및 secure socket 종료 후 backoff
3. 성공 200/202 strict ACK 연결에는 기존 reuse 정책을 유지한다.

판정:

1. B에서 연속 `-1`만 감소할 때 실패 socket 정리가 유효하다고 판정한다.
2. 차이가 없으면 불필요한 lifecycle 변경을 채택하지 않는다.

완료 조건:

1. 실패 후에도 동일 batch body와 ACK identity가 유지된다.
2. retry backoff 2/4/8/16/30초 상한은 유지된다.

### TLS-04 — 서버·네트워크 상관 확인

목적: 장치 원인과 외부 경로 원인을 구분한다.

검증:

1. 같은 시간대의 ESP attempt timestamp를 기록한다.
2. 가능하면 Caddy/backend access log에서 해당 요청 도착 여부를 확인한다.
3. 서버에 요청 흔적이 없으면 DNS/TCP/TLS 전단을 우선한다.
4. 서버에 요청이 도착했지만 응답이 늦으면 reverse proxy/backend 처리시간을 확인한다.
5. Wi-Fi disconnect/reconnect event와 `-1` timestamp를 대조한다.

완료 조건:

1. 최소 3건의 `-1`에 대해 서버 도착 여부를 대응시킨다.
2. 서버 로그 접근 권한이 없으면 해당 항목을 `미확정`으로 남긴다.

### TLS-05 — 채택 수정 및 회귀 보호

수정은 TLS-01~04에서 확인된 항목만 적용한다.

필수 보존 조건:

1. CA 검증을 비활성화하지 않는다.
2. HTTPS를 HTTP로 낮추지 않는다.
3. token을 query string에 넣지 않는다.
4. timeout 또는 socket 실패에서도 batch를 삭제하지 않는다.
5. 200/202 strict ACK 전체 일치 전에는 spool을 삭제하지 않는다.
6. unknown failure는 bounded retry와 durable retain으로 처리한다.

## 6. 에이전트 분배안

### AGENT-01 — Drop 및 storage 담당

담당 번호:

1. DROP-01
2. DROP-02
3. DROP-03
4. 결과가 확정한 DROP-04의 한 경로
5. 필요한 경우에만 DROP-05
6. DROP-06

쓰기 범위:

- `firmware\esp32_edge_node\include\continuous_vibration_runtime.h`
- Raw storage 관련 새 helper/test 파일만
- 기존 binary ring을 수정해야 하면 coordinator 승인 후 해당 최소 범위

금지:

1. TLS/HTTP 정책 변경
2. backend 계약 변경
3. COM7 upload
4. partition/format/erase

### AGENT-02 — TLS 진단 및 전송 담당

담당 번호:

1. TLS-01
2. TLS-02
3. TLS-03
4. TLS-04 보고
5. 증거가 확인한 TLS-05 수정

쓰기 범위:

- `firmware\esp32_edge_node\include\backend_http.h`
- `firmware\esp32_edge_node\include\continuous_vibration_runtime.h`의 HTTP 구간
- 필요한 pure helper/test 파일

충돌 방지:

1. Agent2는 `continuous_vibration_runtime.h` 수정 전 coordinator에게 정확한 line/symbol 범위를 알린다.
2. Agent1의 storage 구간과 Agent2의 HTTP 구간을 동시에 편집하지 않는다.

금지:

1. storage 구조 변경
2. CA 검증 완화
3. token 또는 secret 출력
4. COM7 upload

## 7. 통합 및 최종 게이트

### INT-01 — 에이전트 보고 형식

각 번호마다 다음을 보고한다.

1. 번호
2. 관측 증거
3. 변경 파일
4. 변경 이유
5. 실행한 명령
6. 테스트 결과
7. 남은 위험
8. 다음 번호 진행 가능 여부

### INT-02 — 정적·native 검증

필수:

1. `git diff --check`
2. `pio test -e native`
3. `pio run -e esp32-s3-devkitc-1-n8`
4. backend 파일이 바뀐 경우 관련 Python 회귀 테스트
5. secret 및 local config가 diff에 포함되지 않았는지 확인

### INT-03 — 실장비 3분 1차 게이트

합격 조건:

1. upload와 flash hash 검증 성공
2. panic, stack canary, watchdog, reboot 0
3. `capture_queue_full=0`
4. `raw_hold_full=0`
5. `Dropped windows=0`
6. `RAW-SPOOL-FULL=0`
7. strict ACK 성공 또는 네트워크 실패 시 동일 batch durable retain
8. 같은 permanent 4xx/409 hot loop 0
9. TLS `-1`이 발생하면 분류 가능한 진단 로그와 bounded retry가 존재

### INT-04 — 10분 안정성 및 backlog 회복 게이트

합격 조건:

1. 10분 동안 drop 0
2. spool ready count가 무한 증가하지 않고 성공 ACK 후 감소
3. queue high-water가 capacity 미만
4. storage p95가 생성 주기를 만족
5. TLS 성공률과 `-1` 비율 기록
6. reboot 후 durable record 복구 확인

### INT-05 — 완료 판정

다음 중 하나로만 종료한다.

1. `PASS`: INT-01~04 모두 충족
2. `PARTIAL`: crash/409 등 일부는 해결됐지만 drop 또는 TLS 원인이 남음
3. `BLOCKED`: 실장비 용량, 서버 로그 접근, partition 승인 등 외부 조건이 없어 증명 불가

`Dropped windows>0`인 상태에서는 firmware를 정상 또는 rollout-ready로 판정하지 않는다.

## 8. 현재 결론

1. `Dropped windows=222`의 증가 지점은 capture/processing queue 계층으로 확인됐다.
2. 저장 경로 처리량 부족 정황은 강하지만 scan, lock, file lifecycle 중 지배 단계는 아직 계측되지 않았다.
3. TLS `-1`은 HTTP 응답 전 전송 실패지만 현재 정보만으로 하위 원인을 확정할 수 없다.
4. 동일 endpoint가 이후 202로 성공했으므로 영구 인증·URL 오류로 단정하지 않는다.
5. 다음 수정의 첫 단계는 구조 변경이 아니라 DROP-01/02와 TLS-01 계측이다.
6. 계측 결과가 확인한 병목만 최소 범위로 수정한다.

## 9. 확정이 어려운 항목의 Debug 판정 계획

### DEBUG-01 — 진단 build 분리

1. `RAW_PIPELINE_DIAGNOSTICS`와 `TLS_TRANSPORT_DIAGNOSTICS` 같은 compile-time flag를 사용한다.
2. 기본 운영 build에서는 두 flag를 끈다.
3. 진단 build도 token, CA 원문, Raw samples, Authorization header를 출력하지 않는다.
4. 진단 flag 외의 기능 설정은 운영 build와 같게 유지한다.

완료 조건:

1. 진단 flag on/off 두 N8 build가 모두 성공한다.
2. flag off에서는 상세 debug 문자열이 firmware에 남지 않는지 확인한다.

### DEBUG-02 — Drop 판정표

다음 조건으로만 원인을 판정한다.

1. `capture_queue_full>0`, `spool_total_us<640000`이면 processing task가 저장 외 다른 작업에서 지연되는지 task runtime과 mutex wait를 확인한다.
2. `spool_lock_wait_us`가 total의 대부분이면 LittleFS ownership/critical section 병목으로 판정한다.
3. `slot_lookup_us`가 occupancy에 비례해 증가하면 선형 slot scan 병목으로 판정한다.
4. `write+flush+rename_us>=640000`이 반복되면 file-per-window lifecycle 처리량 부족으로 판정한다.
5. 평균은 640 ms 미만이지만 max/p95 spike 때만 queue가 차면 burst buffer 부족으로 판정한다.
6. `spool_capacity_full>0`이면 처리량 문제가 아니라 용량 게이트를 별도로 실패 처리한다.
7. 위 조건이 섞이면 가장 먼저 queue를 막은 timestamp 순서로 1차 원인을 정한다.

### DEBUG-03 — TLS 판정표

다음 조건으로만 원인을 판정한다.

1. device DNS lookup 자체가 실패하면 DNS 계층으로 판정한다.
2. DNS는 성공하고 서버 access log에 요청이 없으며 TLS debug가 connect/handshake 실패를 가리키면 TCP/TLS 전단으로 판정한다.
3. TLS handshake timeout code가 확인될 때만 handshake timeout으로 판정한다.
4. 서버 access log에 요청이 도착했고 응답 전 연결이 끊기면 proxy/backend 처리 또는 socket 종료 시점을 대조한다.
5. `reuse_candidate=yes`에서만 연속 실패하고 fresh socket에서 성공하는 반복 증거가 있을 때만 stale reuse로 판정한다.
6. Wi-Fi disconnect event와 같은 timestamp에서만 실패할 때만 무선 연결 문제로 판정한다.
7. heap minimum 저하 또는 allocation failure가 먼저 나타날 때만 네트워크 메모리 압박으로 판정한다.
8. `lastError=Generic error`만 있으면 `unclassified_transport_failure`로 유지한다.

### DEBUG-04 — Debug 데이터 수집량 제한

1. storage timing은 RAM counter/histogram에 누적하고 10초마다 한 줄로 출력한다.
2. TLS 상세 로그는 실패 attempt와 최초 성공 attempt에만 출력한다.
3. 동일 오류 문자열은 10초 이내 반복 출력하지 않는다.
4. serial 출력 전후 시간을 별도 측정해 debug 출력 자체의 지연을 확인한다.
5. debug build에서만 drop이 늘면 해당 결과를 제품 병목 증거로 사용하지 않는다.

### DEBUG-05 — Debug 종료 조건

1. Drop의 지배 단계가 3회 이상 같은 결과로 재현되면 해당 단계는 확정한다.
2. TLS 하위 단계는 명시적 error code 또는 device/server 양쪽 timestamp 대응이 있을 때 확정한다.
3. 확정 후 사용이 끝난 상세 debug 코드는 제거하거나 기본 off로 둔다.
4. 확정되지 않은 후보는 문서에서 삭제하지 않고 `미확정`으로 유지한다.
