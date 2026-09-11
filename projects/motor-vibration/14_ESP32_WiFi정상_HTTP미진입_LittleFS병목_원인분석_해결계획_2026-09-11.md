# ESP32 Wi-Fi 정상 후 HTTP 미진입 및 LittleFS 병목 원인분석·해결계획

- 작성일: 2026-09-11
- 실제 펌웨어 기준: `D:\github\MotorDiagnosis\firmware\esp32_edge_node`
- 대상 장비: ESP32-S3 DevKitC-1, ESP32-S3-WROOM N16R8
- 현재 판정: **진단 부분 완료 / rollout BLOCKED**
- 보존 조건: 외부 JSON/API, strict ACK, ACK 후 삭제, bounded queue, temp-write/flush/rename, 기존 데이터 비파괴 원칙

## 14-00 이번 문서의 결론

1. Wi-Fi 연결과 NTP 동기화는 정상이다.
2. 190초 관측은 300초 periodic cutoff보다 짧으므로, HTTP 0건만으로 networkTask·TLS·서버 장애를 확정할 수 없다.
3. LittleFS 저장 지연은 Wi-Fi 대기시간이 아니다. `writeRawSpool()` 내부의 로컬 파일 처리 시간이며, 연결 상태에서 최대 25.52초까지 증가했다.
4. 전용 writer task 덕분에 `capture_queue_full=0`은 유지됐지만, 저장 처리량이 생성 속도를 따라가지 못해 bounded handoff/pending/rawHold가 포화되고 `actual_drop_total=215`가 발생했다.
5. 연결 전후의 LittleFS 점유율과 파일 수가 달라 이번 두 실행만으로 Wi-Fi가 저장시간을 악화시켰다고 단정할 수 없다.
6. 다음 수정 전에는 먼저 5분 cutoff를 포함하는 올바른 end-to-end 관측과 저장공간 수용량 계산을 완료해야 한다.

## 14-01 수행 번호와 상태

| 번호 | 상태 | 내용 |
|---|---|---|
| 14-01 | 완료 | LittleFS 초기화 후 Wi-Fi 오프라인 190초 저장 기준선 수집 |
| 14-02 | 완료 | Wi-Fi 연결·NTP 동기화 후 리부팅 190초 비교 로그 수집 |
| 14-03 | 완료 | 펌웨어 호출 흐름과 저장·전송 소유권 재검토 |
| 14-04 | 완료 | 확정 원인, 강한 가설, 미검증 항목 분리 |
| 14-05 | 계획 | 5분 cutoff 포함 end-to-end 진단 보강 |
| 14-06 | 계획 | N16R8 저장공간 수용량 및 migration gate 확정 |
| 14-07 | 계획 | writer/selector 파일시스템 공정성 최소 수정 및 회귀검증 |
| 14-08 | 계획 | build → firmware-only upload → 8분 이상 실장비 로그 → 최종 문서화 |

## 14-02 Wi-Fi 연결 전후 실측 비교

두 실행 모두 기존 펌웨어를 사용했고 source 수정, firmware upload, format, erase, uploadfs, 파일 삭제를 하지 않았다.

| 항목 | 13-15C Wi-Fi 오프라인 | 13-15D Wi-Fi 연결 | 판정 |
|---|---:|---:|---|
| 관측 시간 | 190초 | 190초 | 동일 |
| Wi-Fi/NTP | reconnect timeout / UTC 미확정 | IP `172.20.10.2`, RSSI `-47 dBm`, NTP 성공 | 연결 조건 제거 |
| LittleFS used/free | 1,552,384 / 1,200,128 bytes | 2,072,576 / 679,936 bytes | 동일 저장상태 아님 |
| 부팅 catalog present | 77 | 196 | 연결 실행의 파일 수가 더 많음 |
| storage samples | 57 | 16 | 연결 실행의 처리량 감소 |
| write max | 76.17 ms | 87.26 ms | 큰 차이 없음 |
| flush/close max | 641.02 ms | 2,089.87 ms | 악화 |
| rename max | 2,289.95 ms | 7,279.01 ms | 악화 |
| storage total max | 6,091.71 ms | 25,522.76 ms | 악화 |
| 1.28초 초과 | 56/57 | 16/16 | 병목 지속 |
| capture queue full | 0 | 0 | 캡처 직접 정지는 격리됨 |
| actual drop / rawHold full | 187 / 187 | 215 / 215 | 처리 결과 유실 증가 |
| pending/rawHold high-water | 8 / 8 | 8 / 8 | bounded buffer 포화 |
| HTTP/ACK/delete | 0 / 0 / 0 | 0 / 0 / 0 | end-to-end 미검증 |
| panic/watchdog/corruption | 0 | 0 | 안전성 오류 없음 |

## 14-03 실제 제어 흐름

```text
ADXL345 capture
  -> rawQueue (8)
  -> processingTask
     -> PSRAM rawSpoolHandoff (8, non-blocking)
     -> 실패 시 pending/rawHold (각 8)
  -> rawSpoolWriterTask
     -> LittleFS temp open/write/flush/close/rename
     -> RAM catalog 갱신

networkTask
  -> Wi-Fi 연결 확인
  -> UTC 확인
  -> priority selector
  -> periodic selector (현재 boot uptime - 300초 cutoff)
  -> 선택된 RAM batch는 전송 전 LittleFS 저장
  -> HTTP/TLS POST
  -> strict ACK 일치 확인
  -> ACK 성공 후에만 spool 삭제
```

코드 근거:

1. 캡처→rawQueue: `continuous_vibration_runtime.h:975-1008`
2. 처리→spool handoff/pending/rawHold: `continuous_vibration_runtime.h:1087-1215`
3. 전용 writer: `continuous_vibration_runtime.h:650-665`
4. 로컬 저장 타이머와 temp/write/flush/rename: `continuous_vibration_runtime.h:542-647`
5. Wi-Fi·UTC·selector: `continuous_vibration_runtime.h:1305-1425`
6. 5분 cutoff: `periodic_scheduler.h:6-18`
7. 전송 전 내구화: `continuous_vibration_runtime.h:1501-1525`
8. HTTP와 strict ACK: `continuous_vibration_runtime.h:1592-1687`
9. ACK 후 삭제: `continuous_vibration_runtime.h:1688-1731`
10. 보조 HTTP starvation guard: `main.cpp:7287-7315`

## 14-04 원인 등급

### 14-04A 확정

1. **190초 테스트는 periodic HTTP 판정 시간이 아니다.** `PeriodicScheduler::CutoffUs=300000000`이므로 일반 periodic window는 현재 boot 5분 전에는 cutoff를 통과하지 않는다.
2. **저장 병목은 실제다.** window 생성 주기는 약 0.64초인데 모든 연결 실행 storage sample이 1.28초를 초과했고 최대 25.52초였다.
3. **Wi-Fi/TLS 시간이 storage timer에 직접 포함되지는 않는다.** 타이머는 `writeRawSpool()` 진입부터 로컬 저장 완료까지이며 HTTP 시작보다 앞선다.
4. **기능 격리는 부분 성공이다.** capture queue overflow는 0이지만 bounded downstream buffer가 모두 차서 실제 처리 결과 215개가 유실됐다.
5. **서버 장애는 미입증이다.** HTTP attempt/HTTP-DIAG가 없으므로 DNS, TLS, POST, ACK 응답을 이번 실행으로 평가할 수 없다.

### 14-04B 강한 가설

1. 2.75MB LittleFS에 5분치 raw window를 개별 파일로 temp-write/flush/rename하는 구조가 첫 periodic drain 전에 용량·metadata 지연 임계점에 도달한다.
2. writer와 selector가 같은 LittleFS lock을 사용하고 core 1의 동일 priority task로 실행되므로, 연속 저장 시 selector와 보조 health가 파일시스템 턴을 늦게 받을 수 있다.
3. 논리 슬롯은 512개지만 실제 수용량은 파일 payload 외 block·directory·copy-on-write 비용 때문에 512개보다 작을 수 있다.

### 14-04C 미검증

1. 5분 이후 Raw HTTP/TLS/서버 ACK의 정상 여부
2. 연결 실행의 25.52초 중 lock wait와 LittleFS 내부 metadata 작업의 정확한 비율
3. 파티션 확대만으로 지연까지 해결되는지 여부
4. restored record의 이전 boot `startUs`를 현재 boot cutoff/UTC에 연결하는 정책

## 14-05 해결 순서

### 14-05A 올바른 end-to-end 기준선

- Wi-Fi 연결과 NTP 성공 시점부터 최소 8분을 관측한다.
- `loop_ready → time_ready → priority_scan_end/periodic_scan_end → payload_ready → pre_send_persist_end → http_begin → HTTP 결과 → ACK → delete`를 동일 event/batch로 연결한다.
- 5분 전 HTTP 0건은 오류로 판정하지 않는다.
- 5분 이후에도 HTTP 미진입이면 마지막 stage와 `RAW-SELECT-DIAG`의 rejected/eligible/lock wait로 차단점을 확정한다.

통과 조건:

1. 5분 cutoff 이후 `RAW-ATTEMPT` 또는 `HTTP-DIAG op=raw`가 1회 이상 발생한다.
2. ACK 없는 delete는 0건이다.
3. accepted ACK의 식별자와 삭제 대상이 일치한다.

### 14-05B 저장 수용량 gate

- `512 slots` 선언값이 아니라 실측 파일당 flash 증가량으로 5분치 필요 공간과 안전 reserve를 계산한다.
- N16R8의 현재 legacy map은 LittleFS가 `0x540000/0x2A0000`이고 flash `0x800000` 이후를 사용하지 않는다.
- 5분치 + 전송 재시도 reserve를 담지 못하면, 코드 queue 확대가 아니라 N16R8 상위 flash를 사용하는 partition migration을 별도 승인·백업·readback·rollback 절차로 진행한다.
- partition 확대 후에도 fresh/headroom 상태에서 저장 p95가 0.64초를 넘으면 파일 구조 병목으로 판정한다.

### 14-05C writer와 selector 독립성

- 기존 LittleFS mutex 하나를 유지하되 writer가 한 transaction 완료 후 대기 중인 selector/network에 bounded turn을 제공한다.
- lock 밖에서 가능한 직렬화와 JSON/TLS 작업은 계속 lock 밖에 둔다.
- queue 크기 확대, 무제한 retry, 별도 HTTP owner 추가는 하지 않는다.
- 우선 stage/block-reason 로그로 starvation을 증명한 뒤 한 경계만 수정한다.

통과 조건:

1. selector lock wait에 상한이 생기고 5분 이후 HTTP 단계에 도달한다.
2. storage total p95 ≤ 640ms, max ≤ 1,280ms를 목표로 한다.
3. `actual_drop_total=0`, `capture_queue_full=0`, pending/rawHold high-water가 각 capacity 미만이다.

### 14-05D 파일 구조 후속 조건

- 충분한 LittleFS headroom에서도 write/flush/rename이 계속 0.64초 생성 주기를 넘을 때만 개별 파일 구조를 변경한다.
- 그때 최소 후보는 최대 4개 window를 하나의 내구 batch로 묶어 create/flush/rename 횟수를 줄이는 방식이다.
- 기존 v1 단일 파일 read compatibility, CRC, strict ACK, partial ACK 보존, ACK 전 삭제 금지를 먼저 native test로 고정한다.
- append journal이나 대규모 storage abstraction은 batch 방식이 실측 목표를 못 맞출 때만 검토한다.

## 14-06 중단 및 rollback 조건

아래 하나라도 발생하면 업로드·확대를 멈추고 해당 번호에서 보고한다.

1. LittleFS mount 실패, CRC/schema corruption, panic, watchdog, reboot
2. ACK 이전 삭제 또는 ACK 식별자 불일치
3. 외부 JSON/API 변경
4. 기존 파일을 읽지 못하는 migration
5. capture queue overflow 또는 actual drop 증가
6. 승인되지 않은 format, erase-all, uploadfs, 데이터 삭제

## 14-07 다음 실행 순서

1. **14-05A**: 현재 firmware로 8분 이상 Wi-Fi 연결 로그를 수집해 실제 HTTP 차단 stage를 확정한다.
2. **14-05B**: 실측 파일당 flash 비용과 5분 backlog 필요량을 계산해 현재 2.75MB partition의 적합성을 판정한다.
3. **14-05C**: starvation이 확인될 때만 writer/selector bounded turn을 최소 수정하고 native test와 N16R8 build를 수행한다.
4. partition 부족이 확인되면 별도 migration 번호와 사용자 승인 후 N16R8 상위 flash를 사용한다.
5. 모든 gate 통과 후 firmware-only upload, 8분 이상 로그, HTTP/ACK/delete 검증을 수행한다.
6. 최종 결론이 나온 뒤 구조도와 상태 문서를 다시 최신화한다.

## 14-08 하지 않은 작업

- MotorDiagnosis source 수정
- firmware upload
- LittleFS format/erase/uploadfs/데이터 삭제
- partition 변경
- MotorDiagnosis commit/push/PR
- 서버/TLS 원인 확정

## 14-09 병렬 분석·진단 구현·N16R8 실장비 결과

### 14-09A 설계 에이전트 보고

- LittleFS 선저장 계약과 통신 경로를 compile-time 진단 모드로 분리했다.
- 진단 모드는 PSRAM handoff에서 정확히 4개 Raw window를 읽고 기존 JSON/TLS/HTTP/strict ACK를 재사용한다.
- 성공한 ACK에서만 handoff를 pop하고 실패하면 원본을 유지한다.

### 14-09B HTTP 계약 검토 보고

- Raw payload는 backend 계약에 맞춰 top-level `windows` 배열로 전송하도록 확인했다.
- backend가 반환하는 200/202와 device/boot/window identity를 ACK 기준으로 사용한다.
- 이번 실장비에서는 HTTP 응답 자체에 도달하지 못해 payload/API 계약의 실응답 검증은 미완료다.

### 14-09C 저장량 검토 보고

- Raw payload 3,072B와 header를 고려하면 0.64초 주기에서 하루 약 415MB가 필요하다.
- 2.75MB LittleFS는 24시간 Raw 보존 장치가 아니며, 외부 저장 또는 서버 보존이 필요하다.

### 14-10 구현 및 14-11 검토

- `RAW_TRANSPORT_DIAGNOSTIC` 분기, PSRAM 4-window handoff, no-spool 전송, ACK 후 pop을 구현했다.
- normal mode의 기존 spool/write/ACK/delete 경로는 유지했다.
- RAW=0/continuous 조합의 writer guard, 비연속 모드 auxiliary 호출, 진단 로그 rate limit을 보완했다.
- native `178/178`, N16R8 기본 build, diagnostic build, RAW=0/CONTINUOUS=1 build를 통과했다.

### 14-12 수정 및 14-13 최종 gate

- non-continuous loop에서 network auxiliary가 호출되도록 보완했다.
- 최종 gate에서 target 경로와 production spool 계약이 유지됨을 확인했다.
- 진단 모드 실장비 검증만 남기고 production rollout은 보류했다.

### 14-14 N16R8 실장비 업로드 및 로그 결과

- COM7에 N16R8 diagnostic firmware를 업로드했다. process 환경으로 `-DRAW_TRANSPORT_DIAGNOSTIC=1`만 주입했으며 LittleFS upload/format/erase/partition 변경은 하지 않았다.
- RTS-only 1회 리셋 후 90.125초 수집했다. Wi-Fi IP `172.20.10.2`, RSSI 약 `-32~-35 dBm`, NTP `2026-09-11T15:01:15Z` 동기화는 PASS다.
- `source=psram depth=7 count=4`, indexes `0,1,2,3`, JSON `18,065B`, `pre_send_persist_bypassed`를 확인해 PSRAM→payload→no-spool 경로는 PASS다.
- `storage_samples=0`, storage timer 전부 0, `no_spool_write=1`로 저장 경로가 이번 통신 실험을 막지 않았음은 확인했다.
- 그러나 HTTPS POST가 약 `5.344s` 요청 구간, 총 약 `25.621s` 후 `status=-1`, `connection refused`로 끝났다. ACK `0`, matched `0`, `accepted=no`, handoff `retained=1`, retry `1`이다.
- POST 대기 중 pending capacity 8이 차서 `processing_pending_full_events=53`, `actual_drop_total=45`, `raw_hold_full=45`까지 증가했다. `capture_queue_full=0`이고 PSRAM free 약 8.18MB는 유지됐다.
- panic/watchdog/assert/heap corruption 표시는 없었다. LittleFS는 mount됐지만 기존 상태에서 `used=total=2,752,512`, `free=0`이었다.

## 14-10 최종 해석 및 다음 번호

- **진단 분리 경로:** PASS. LittleFS 선저장 없이 PSRAM에서 4개를 선택하고 payload를 구성했다.
- **통신 end-to-end:** **NO-GO/BLOCKED**. Wi-Fi·NTP·DNS(`TLS-DNS rc=1`) 이후 TLS/서버 연결 단계에서 거절되어 200/202 및 ACK를 확인하지 못했다.
- 이번 결과만으로 firmware payload 오류와 서버 포트/방화벽/백엔드 가용성 중 하나를 단정하지 않는다. `connection refused`는 HTTP status 이전 실패다.
- **다음 14-15:** firmware를 더 수정하지 않고 동일 endpoint에 대한 host/server 측 443 reachability와 backend listener를 read-only로 확인한다. 그 결과가 정상일 때만 TLS client 설정 또는 endpoint 설정을 별도 번호로 검토한다.
- 저장 구조는 별도 트랙으로 유지한다. 현재는 24시간 내부 Flash 보존을 목표로 하지 않고, 전송 실패 시 bounded fallback 정책을 결정할 때까지 production rollout을 승인하지 않는다.
