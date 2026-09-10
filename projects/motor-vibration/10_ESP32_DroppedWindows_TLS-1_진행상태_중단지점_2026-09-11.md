# ESP32 `Dropped windows` / TLS `-1` 수정 진행상태 및 중단 지점

- 작성일: 2026-09-11
- 기준 계획: `09_ESP32_DroppedWindows_TLS-1_원인분석_수정계획_2026-09-11.md`
- 대상: `D:\github\MotorDiagnosis\firmware\esp32_edge_node`
- 보드/포트: `esp32-s3-devkitc-1-n8` / `COM7`
- 상태: **중단 저장 — 최종 PASS 아님**

## 01. 이번 작업의 관리 단위

1. `DROP-01`: drop 원인을 queue/spool 단계별 counter로 분리
2. `DROP-02`: LittleFS 저장 단계를 lock wait, slot lookup, open, write, flush/close, rename, total로 계측
3. `DROP-03`: 저장 병목과 HTTPS 영향을 분리
4. `DROP-04`: 계측 결과가 확인된 경로만 최소 수정
5. `TLS-01`: TLS `-1` 발생 시 Wi-Fi, RSSI, heap, HTTP 단계, secure error를 기록
6. `INT-01`: native/N8 빌드 및 실제 COM7 3분 로그로 최종 판정

## 02. 에이전트 보고 및 통합 결과

1. Drop 담당 에이전트 보고를 반영하여 다음 counter와 1초 주기 summary를 추가했다.
   - `capture_queue_full`
   - `processing_pending_full`
   - `raw_hold_full`
   - `spool_write_fail`
   - `spool_capacity_full`
   - `spool_corrupt_or_unreadable`
   - raw/pending/hold queue depth 및 high-water
   - LittleFS 저장 단계별 최근값/max
2. TLS 담당 에이전트 보고를 반영하여 Wi-Fi 상태/RSSI/IP, heap/PSRAM, HTTP status/error, secure client 재사용 상태를 진단 로그에 넣었다.
3. 두 작업을 통합한 뒤 별도 에이전트는 추가로 사용하지 않았다. 현재까지 최대 2개 에이전트 제한을 지켰다.

## 03. 현재 펌웨어에 적용된 수정

1. Raw spool 빈 slot 탐색에 `rawSpoolNextSlot` cursor를 추가했다.
   - 첫 부팅의 기존 파일 scan은 남아 있다.
   - 이후 연속 write의 slot lookup은 수 ms 수준으로 줄어든 로그가 있다.
2. `priorityRawPending()`와 `loadRawSpoolBatch()`의 LittleFS lock 범위를 파일 1개 단위로 줄였다.
3. ACK 직후 실행되던 512-slot 상태 전체 scan을 제거했다. 해당 상태 로그는 `deferred`로 표시한다.
4. telemetry ring backlog가 있을 때 main loop가 새 measurement/ring append를 만들지 않도록 defer guard를 추가했다.
5. raw queue가 비어 있지 않으면 network task가 spool batch 512-slot scan을 시작하지 않도록 우선순위 guard를 추가했다.
6. telemetry ring의 `flush()`는 내구성 경계이므로 임의로 제거하지 않고 복원했다.
7. 위 변경은 제품 저장소에 커밋하지 않았다. 기존 dirty 변경을 포함한 `D:\github\MotorDiagnosis` 상태를 그대로 보존한다.

## 04. 빌드/업로드 결과

1. N8 빌드: **성공**
   - RAM: 99,460 / 327,680 (30.4%)
   - Flash: 1,088,809 / 3,342,336 (32.6%)
2. COM7 업로드: **성공**
   - 일반 firmware/partition/app 업로드만 수행
   - LittleFS format, 전체 erase, partition 변경은 수행하지 않음
   - flash hash verification 성공
3. native 재실행: **실행 환경 문제로 미실행**
   - 현재 세션에서 `gcc`/`g++`가 PATH에 없어 0/12가 compile 단계에서 종료됨
   - 이전 동일 소스 계열 검증 기록의 native 171/171 성공과 혼동하지 않는다.
4. 업로드 펌웨어 ELF SHA256: `9763217aac6547bf`

## 05. COM7 3분 관찰 결과

### 05-1. 확정적으로 재현된 현상

1. 부팅、Wi-Fi 연결、NTP 동기화는 성공했다.
2. LittleFS는 다음 상태로 시작했다.
   - total: `2,752,512`
   - used: `2,748,416`
   - 남은 공간: `4,096` bytes
   - telemetry ring recovery: `2747` records
3. raw queue는 `0 -> 8`로 증가한 뒤 계속 `8`에 머물렀다.
4. `capture_queue_full`가 `0 -> 1 -> ...`로 반복 증가했다.
5. 첫 raw spool 저장에서 다음 값이 관찰됐다.
   - `storage_slot_lookup_us=8,796,622`
   - `storage_open_us=109,298`
   - `storage_write_us=35,233`
   - `storage_flush_close_us=389,676`
   - `storage_rename_us=340,990`
   - `storage_total_us=11,045,872`
   - `storage_lock_wait_us=8`
6. 위 결과는 단순 lock wait만의 문제라고 보기 어렵다. 파일 생성/commit 수명주기와 거의 가득 찬 LittleFS가 실제 병목 후보로 남는다.
7. TLS `-1`도 재현됐다.
   - `[WiFiClientSecure.cpp:144] connect(): start_ssl_client: -1`
   - `[TIME] Backend health HTTP error: -1`
   - 다만 이번 로그만으로 TLS 하위 원인을 확정하지 않는다.
8. `IntegerDivideByZero` panic이 여러 차례 재현됐다. 따라서 이 후보는 3분 PASS가 아니다.

### 05-2. panic 주소 해석

현재 ELF로 backtrace를 해석한 결과:

1. `lfs_alloc`
2. `lfs_dir_alloc` / directory split/commit
3. `lfs_file_sync_`
4. `vfs_littlefs_open`
5. `fopen`
6. `VFSImpl::open`
7. `fs::FS::open`
8. `ContinuousVibration::writeRawSpool()` line 406: `LittleFS.open(temp, "w")`
9. `ContinuousVibration::processingTask()` line 954: `writeRawSpool(raw)`

현재 가장 강한 직접 근거는 **LittleFS 사용량이 100%에 가까운 상태에서 raw spool temp 파일을 열어 metadata allocation을 시도하다 LittleFS 내부에서 panic한 것**이다. `spool_capacity_full=0`이었던 이유는 현재 guard가 `freeBytes < RawSpoolSlotBytes`만 검사하고 temp 파일/metadata overhead까지 admission하지 않기 때문일 가능성이 높다. 이 부분은 다음 작업의 첫 검증 대상이며, 아직 새 수정으로 확정하지 않는다.

## 06. 중단 시점의 판정

1. `DROP-01`: **완료** — 직접 drop counter가 증가하는 경로 확인
2. `DROP-02`: **부분 완료** — 저장 단계 계측 완료, LittleFS near-full 및 file lifecycle 병목 근거 확보
3. `DROP-03`: **미완료** — offline/online A/B/C 동일 SHA 분리 실험은 하지 않음
4. `DROP-04`: **부분 완료** — cursor/lock 범위/scan 제거/priority guard 적용했지만 drop과 panic을 제거하지 못함
5. `TLS-01`: **부분 완료** — 진단 정보 추가 및 `-1` 재현, 하위 원인은 미확정
6. `INT-01`: **실패** — 3분 관찰에서 drop과 panic 재현
7. 결론: 현재 업로드된 펌웨어를 정상 rollout으로 판정하지 않는다. 더 이상의 추측성 수정은 여기서 중단한다.

## 07. 다음 재개 시 순서

1. **FS-01**: `writeRawSpool()`의 capacity admission을 먼저 검증한다. `RawSpoolSlotBytes`만이 아니라 temp 생성과 LittleFS directory/metadata 여유를 포함해야 한다.
2. **FS-02**: 여유 공간 부족 시 `LittleFS.open(temp, "w")`를 호출하지 않고 `spool_capacity_full`로 안전하게 종료되는지 확인한다. 목적은 panic 제거이며, 임의의 삭제/format/overwrite는 금지한다.
3. **FS-03**: 동일 firmware에서 raw queue depth, capture drop, `storage_total_us`를 재측정한다. 이 단계에서 drop이 계속되면 file-per-window lifecycle을 다음 원인으로 분리한다.
4. **PANIC-01**: 3분 동안 `Guru Meditation`, `IntegerDivideByZero`, watchdog, reboot가 0인지 먼저 확인한다.
5. **DROP-05**: panic이 사라진 뒤에만 capture drop 0 여부를 3분 확인한다. queue 크기만 키우는 임시 처방은 하지 않는다.
6. **TLS-02**: drop/panic이 안정된 뒤 TLS `-1`의 발생 stage와 Wi-Fi reconnect 시점을 상관 분석한다. TLS 원인을 현재 근거 이상으로 단정하지 않는다.
7. **INT-02**: native toolchain PATH를 복원한 뒤 native 전체 테스트, N8 build, `git diff --check`, 업로드, 동일 조건 3분 로그를 다시 실행한다.

## 08. 재개 금지사항

1. 이 문서의 FS-01 검증 전에는 새 storage architecture나 fixed ring을 추가하지 않는다.
2. LittleFS format, 전체 erase, partition 변경, 기존 record 삭제를 하지 않는다.
3. 제품 저장소 `MotorDiagnosis`에 commit/push하지 않는다.
4. `Dropped windows=0`, panic 0, TLS 안정성을 모두 확인하기 전에는 최종 PASS/rollout healthy를 선언하지 않는다.
