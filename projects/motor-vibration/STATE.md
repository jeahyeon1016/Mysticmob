# 모터 진동 프로젝트 현재 상태

## 기준

- 최신 작업본: `D:\ai agent\projects\motor-vibration`
- 통합 백업 저장소: https://github.com/jeahyeon1016/ai-agent
- 외부 개발 저장소: `D:\github\MotorDiagnosis` (기존 소스 참고용)

## 완료

- 4주차 MVP 완료
- ESP32 운영 HTTPS telemetry 전송 확인
- Native firmware 테스트 56/56 통과
- 실제 장치·서버 연동 및 ACK 처리 확인
- LittleFS 테스트 큐 초기화 및 `Restored Queue : 0 / 24999` 확인
- Raw·Health·Telemetry 전송 구조를 단일 Network Task로 통합하는 로컬 수정안 작성
- N8 빌드 및 COM7 실장비 업로드 확인

## 현재 담당

- IoT: 하드웨어·ESP32 펌웨어
- 협업: Backend·Frontend·AI1·AI2

## 미확정

- 모터 사양
- 실제 설치 환경
- 센서 부착 위치와 최종 측정 목표
- MQTT 전환 여부 및 Broker/Subscriber 서버 구조
- 기존 듀얼 슬롯 OTA 유지 여부와 adaptive N8 파티션 적용 여부

## 다음 작업

1. HTTPS 전송 지연 및 Raw 큐 포화 원인 추가 수정·검증
2. 서버의 `transmission` 메타데이터 계약과 펌웨어 전송 모드 연동 검토
3. MQTT 전환 타당성·Broker·ACK/QoS 명세 협의
4. 모터 사양·설치 환경 확정
5. 실장비 측정 계획 수립

## 최신 검증 요약 (2026-09-10)

- Wi-Fi 연결 및 NTP 동기화는 조건부 성공.
- Raw 샘플은 `samples=512`, `quality=valid`로 생성됨.
- Raw HTTP `202`, Health HTTP `200` 성공 구간이 있으나 요청 지연 및 `HTTP -1/-11`도 반복됨.
- `pending=8`, `raw_queue=8`, `WINDOW dropped`, audio overwrite가 재현됨.
- LittleFS 초기화 직후 큐는 0이었으나 약 4분 실장비 관찰 중 큐가 약 68까지 증가함.
- 단일 Network Task/persistent client 수정안은 N8 빌드·업로드까지 확인했으나, 실장비에서 전송 병목이 해결됐다고 판단할 수 없음.
- `accepted: false/0` Raw ACK를 거부하도록 로컬 검증을 보강했으나, 해당 변경의 실장비 재검증은 남아 있음.
- 변경은 `D:\github\MotorDiagnosis` 로컬 작업본에만 남아 있으며, 공용 저장소 commit/push/PR은 수행하지 않음.

## 2026-09-11 IoT 전송정책 번호별 검증

- 01 부분: 코드 기준선·N8 빌드·native 143/143 확인. 장치 5분 기준선 로그는 없음.
- 02 완료: Raw ACK 202/200 전체 식별자 일치 및 부분 ACK 보존, native/N8 통과.
- 03 완료(소스 기준): TLS 단일 networkTask 소유권·HTTP 종료 순서 보강. 전원차단 실측 미검증.
- 04 완료: adaptive example/local ignore·임계값 검증·`baseline_missing` safe mode 추가.
- 05 완료(순수 로직): priority evaluator 테스트 통과. 런타임 호출 연결은 미완료.
- 06 부분: 전송 전 RawSpool 저장·CRC/read-back·ACK 후 제거 연결. 16 슬롯·재부팅 복원 실측 미검증.
- 07 완료(소스 기준): raw 우선과 bounded auxiliary 턴 추가. 장비 지연 상한 미검증.
- 08 부분: 5분 cutoff·최대 4개·post-cutoff 제외 구현.
- 09 차단: Raw/spool에 priority·trigger·recovery 필드와 서버 payload 계약이 없어 임의 구현 금지.
- 10~15 차단: 09 선행 차단 및 ACK metadata·baseline·OTA/파티션·백업 게이트 미확정.
- 직접 검증: `git diff --check` 통과, PATH에 MSYS2 UCRT64 GCC를 추가한 `pio test -e native` 162/162 통과, `pio run -e esp32-s3-devkitc-1-n8` 통과.
- 업로드 보류: evaluator 런타임 연결 누락, 이전 boot spool 복원 불가, N8 자동 LittleFS 포맷 플래그 유지가 코드 리뷰에서 확인됨.

## 2026-09-11 IoT policy retry 09R-15R final gate

- Retry chain managed sequentially through Luna agent1: 09R `task_f5807311e231`, 10R `task_dca82e863937`, 11R `task_5c402ae79909`, 12R `task_ca3f6166be49`, 13R `task_06f959966ce7`, 14R `task_14be4df965ab`, 15R `task_797c9eb3f541`.
- Corrective follow-ups: 14R-C1 `task_e3750aaf04ff` fixed Unity native test harness and persisted spool boot identity; 14R-C2 `task_f1cbef98cc0b` guarded auxiliary HTTP work when priority raw backlog exists.
- Direct verification in `D:\github\MotorDiagnosis\firmware\esp32_edge_node`: `pio test -e native` with `C:\msys64\ucrt64\bin` prefixed to PATH = 164/164 passed; `pio run -e esp32-s3-devkitc-1-n8` passed; `git diff --check` passed with existing LF/CRLF warnings.
- Policy status: 09R source implementation plus spool-priority guard is present but hardware reboot/priority ordering is unverified; 10R metadata is present but no real server fixture; 11R blocked because the current profile proves 16 slots, not 600; 12R adaptive partition is not applied and OTA/backup/restore approval is absent; 13R diagnostics are source/build verified; 14R native/N8 verified; 15R rollout gate says `coordinator may upload = NO`.
- Upload/log gate: no COM7 upload, reset, format, partition change, or 3-minute log run. Missing real 202/200 ACK fixture, baseline ID/values, LittleFS backup/hash/restore evidence, and hardware serial validation keep rollout blocked.

## 2026-09-11 IoT policy upload and 3-minute log

- Upload authorization was confirmed by the user after local review; only `esp32-s3-devkitc-1-n8` was uploaded to `COM7`. No partition upload, format, or erase command was used. PlatformIO upload completed with ESP32-S3 identification and flash hash verification.
- Three-minute serial observation completed at 115200 baud. Boot and Wi-Fi/NTP synchronization succeeded; LittleFS restored 2511 queued records and continued capture.
- Raw transport initially failed with TLS `status=-1`, retained batches, `pending=8`, and queue overflow. `WINDOW Dropped windows` rose to 295 during observation.
- HTTP `202` ACK paths succeeded for batches 1-3 (`expected=3/4`, `matched=3/4`) and corresponding records were deleted. Later retries for batch 4 returned HTTP `409 WINDOW_SEQUENCE_CONFLICT` (`Expired/out-of-order window`) and remained retained.
- `baseline_missing` safe mode remained active. Repeated LittleFS open errors reported `raw-spool-v2-*.bin does not exist, no permits for creation` while spool records were stored. Final hardware result: upload succeeded, but runtime communication/data-loss criteria failed; do not declare rollout healthy or close the issue.

## 2026-09-11 post-upload root-cause and repair guide

- Added `08_ESP32_업로드후_오류_원인분석_수정지침_2026-09-11.md` with evidence-ranked causes and sequential `FIX-01` through `FIX-11` agent tasks.
- Confirmed failure chain: unlimited `baseline_missing` priority selection skipped older indexes, the server later rejected those indexes with `WINDOW_SEQUENCE_CONFLICT`, 500 ms-capped retry kept the rejected batch active, and non-durable RAM queues overflowed.
- The guide treats initial TLS `-1` as a contributing transient failure pending underlying TLS diagnostics, and treats LittleFS `no permits for creation` as likely existence-probe noise because the following spool write succeeded.
- Full policy compliance requires an explicit server-order decision: allow unseen out-of-order identities, or accept that firmware-only monotonic sending cannot satisfy priority-before-backlog.

## 2026-09-11 agent-integrated upload retest

- Two agents were used and their reports were received: Dalton handled firmware/native FIX-01/02/04/05/06/07; Darwin handled backend FIX-03 and its tests. Both agents were closed after coordinator review.
- Coordinator verification passed: native firmware 171/171, backend regression 80/80 with 8 skips, N8 build, and `git diff --check`.
- COM7 upload was repeated after each runtime repair using firmware only; no partition upload, format, or erase was used. Flash hash verification succeeded.
- First repaired runtime exposed `WindowFeatures` stack-canary overflow and 16-slot spool exhaustion. The repair increased the task stack to 16 KiB, changed raw spool capacity to 512 slots with free-byte enforcement, and replaced noisy missing-file probes with `stat`.
- Final 3-minute observation after the mutex-scope repair: no panic, Guru Meditation, watchdog, or reboot; durable spool records continued to be written. TLS still intermittently returned `-1`, but retries were bounded and 202 ACK was observed in the prior retest.
- Final gate remains **BLOCKED**: `WINDOW Dropped windows` still reached 222 in the final 3-minute run. The firmware is not rollout-healthy until raw capture persistence is decoupled from LittleFS latency or an equivalent lossless buffer is implemented and verified.
- Do not declare completion, do not close the upload issue, and do not push MotorDiagnosis changes. The dirty MotorDiagnosis worktree remains intentionally uncommitted.

## 2026-09-11 remaining Drop/TLS repair plan

- Added `09_ESP32_DroppedWindows_TLS-1_원인분석_수정계획_2026-09-11.md`.
- Confirmed only the direct Drop failure boundary: capture/processing queue overflow increments `processingDrops`; the dominant LittleFS substage is not yet measured.
- TLS `-1` remains an HTTP-pre-response transport failure with an unknown lower-level cause. Intermittent 202 success rules out treating URL/auth/certificate configuration as a proven permanent fault.
- Next gate starts with numbered instrumentation (`DROP-01/02`, `TLS-01`) before selecting a storage or TLS behavior change.

## 2026-09-11 paused checkpoint after 09 plan implementation

- Added `10_ESP32_DroppedWindows_TLS-1_진행상태_중단지점_2026-09-11.md` with numbered task status, exact hardware evidence, and the next safe resume order.
- Integrated the two agent reports and applied the minimum evidence-based changes: spool write cursor, per-file LittleFS locks, removal of the ACK hot-path full spool scan, raw-priority guards, and DROP/TLS diagnostics.
- N8 build and COM7 firmware-only upload succeeded. No format, erase, partition change, or MotorDiagnosis commit/push was performed.
- The latest 3-minute observation failed: raw queue saturated at 8, `capture_queue_full` increased, the first raw spool write took about 11.0 seconds, TLS `-1` recurred, and `IntegerDivideByZero` rebooted the device repeatedly.
- Current strongest direct evidence is `writeRawSpool()` line 406 (`LittleFS.open(temp, "w")`) entering LittleFS allocation/metadata code while the filesystem had only 4,096 bytes free. This is a hypothesis to verify with a safe capacity-admission guard, not permission to delete or format data.
- Work is intentionally paused here. The uploaded firmware is **not rollout-healthy** and the dirty `D:\github\MotorDiagnosis` worktree remains uncommitted.

## 2026-09-11 evidence-only repair boundary after checkpoint 10

- Added `11_ESP32_확정원인_디버그우선_에이전트수정범위_2026-09-11.md`.
- Confirmed three product-code defects from source and logs: the network task's `rawQueueIsEmpty()` guard blocks selection of a new durable spool batch whenever any backlog exists; the boot-reset cursor leaves the first 8.80-second slot scan inside active capture; and `processingTask()` holds the pending mutex across the 11.05-second filesystem write.
- Confirmed the panic call path ends at `writeRawSpool()` calling `LittleFS.open(temp, "w")` with only 4,096 bytes free, but did not claim near-full storage as the sole divide-by-zero cause. The exact bundled `lfs.c:689`, allocator state, and safe reserve are still unknown.
- Preserved the existing two-agent structure: Agent 1 owns Drop/LittleFS runtime, Agent 2 owns TLS diagnostics, and the coordinator alone integrates/uploads/monitors.
- The next run is deliberately bounded at deterministic fixes, fail-closed diagnostics, build/test, one COM7 upload, one 3-minute log, and a stop/report decision. Fixed-ring work, partition/format/erase, queue inflation, TLS behavior changes, and MotorDiagnosis commit/push remain outside scope.

## 2026-09-11 evidence-only repair upload and 3-minute result

- Supervised run `run_877a40df0035` used the existing two-agent structure: Agent 1 completed A1-01 through A1-07 in `continuous_vibration_runtime.h`; Agent 2 completed A2-01 through A2-04 in `backend_http.h`.
- Coordinator verification passed: `pio test -e native` = 171/171, `pio run -e esp32-s3-devkitc-1-n8` succeeded, and target-file `git diff --check` had only the existing LF/CRLF warning.
- Firmware-only upload to COM7 succeeded with flash hash verification. Uploaded ELF SHA256: `EA18B0AA0929D3A7D64C0243718FDE7AAA70723B21DD111910FA40E019E83467`.
- Three-minute serial observation: LittleFS reported total 2,752,512, used 2,748,416, free 4,096; boot admission latched `suspended=yes`; boot cursor reported `scanned=350`, `next=349`; no IntegerDivideByZero, panic, watchdog, reboot, or TLS `-1` was observed during this window.
- The fail-closed gate worked as designed: new spool writes were rejected before `LittleFS.open()`, storage timing buckets remained zero, and the existing backlog was retained. The restored ring backlog and RAM queues saturated; `processing_pending_full` and `capture_queue_full` increased, so this is a diagnostic stabilization result, not a rollout-health pass.
- No HTTP attempt was sufficiently observed in this window to prove TLS resolution. Do not expand the scope to fixed-ring, reserve-threshold tuning, partition/format/erase, or TLS behavior changes. MotorDiagnosis remains dirty and uncommitted/unpushed.

## 2026-09-11 transmission-stall diagnosis and debug-first next plan

- Added `12_ESP32_전송정체_Drop_확정원인_디버그우선_수정지침_2026-09-11.md` after comparing the latest hardware state with the current runtime and scheduler code.
- Confirmed that the prior three-minute observation could not exercise ordinary periodic selection because the scheduler cutoff is five minutes and remains zero before then.
- Confirmed that fail-closed spool admission prevents the unsafe LittleFS write path but also prevents new RAM batches from passing durable-before-send persistence; this is a safety result, not a transmission fix.
- Confirmed that comparing a restored record's boot-relative `startUs` with the current boot's cutoff crosses incompatible monotonic time domains. Immediate restored-record transmission remains blocked because the raw spool does not prove a mapping to its original UTC anchor.
- Clarified that `processing_pending_full` counts repeated pressure events, while `capture_queue_full` is a direct loss boundary; the next diagnostic build must separate event, unique-window, and actual-drop counts.
- Preserved the existing two-agent structure. Agent 1 owns selector/scheduler/Drop instrumentation, Agent 2 owns HTTP/TLS event correlation, and the coordinator alone integrates, verifies, uploads firmware, and runs an active three-minute observation after selector completion or HTTP entry.
- Current direction assessment: safety and evidence discipline are improving, but rollout health is still blocked because transmission progress and actual loss have not been resolved and TLS has not been exercised sufficiently.

## 2026-09-11 plan 12 execution, upload, and progress-gate result

- Supervised Orca run `run_5075220e1944` preserved the two-agent structure. Agent 1 (`task_8c2b9f5a1ba0`) added selector timing/reason diagnostics, rate-limited raw-TX stage markers, cross-boot timestamp evidence, and separate pressure/unique/actual-drop counters. Agent 2 (`task_42741f5c504e`) reviewed the existing HTTP/TLS correlation diagnostics and made no product-code change.
- Coordinator review passed: native tests `172/172`, N8 build succeeded, target diff check had only the existing LF/CRLF warnings, and the final ELF SHA256 was `CF1CBEFF48801BFD65985D4CB6534697429BCF9A5D7DB231F107116E4EFAD600`.
- Firmware-only upload to `COM7` succeeded. Only bootloader, partition table, boot app descriptor, and application firmware were written; no filesystem image, format, erase command, or partition change was used. Flash hashes verified.
- Hardware boot session `1944`: LittleFS total `2,752,512`, used `2,736,128`, free `16,384` at boot; after initial writes free reached `4,096`. Boot scan reported `scanned=10`, `next=9`, `full=no`, and elapsed `245400 us`. Subsequent writes reported storage totals around `1.74-2.44 s`, then the fail-closed admission guard repeatedly rejected writes before `LittleFS.open()` with `need=3133`, `free=4096`.
- NTP eventually synchronized and produced `[RAW-TX-STAGE] event=4` through `time_ready` and `priority_scan_begin`. During more than the plan's 8-minute progress window, no selector end/`RAW-SELECT-DIAG` and no raw `http_begin`, HTTP result, TLS diagnostic, ACK, or delete event appeared. Therefore the active three-minute post-selector observation condition was never reached; the run was stopped by the plan's progress-failure gate.
- Final observed counters: `capture_queue_full=604`, `processing_pending_full_events=215`, `processing_pending_full_unique=1`, `actual_drop_total=604`, `raw_queue_depth=8`, `pending_depth=8`, `raw_hold_depth=0`, `spool_capacity_full=223`, and `spool_write_fail=0`. This confirms queue saturation and actual capture loss while the transmission selector was not progressing; it does not prove the lower-level cause of TLS `-1`.
- No panic, watchdog, reboot, or unsafe LittleFS allocation failure appeared in this run. Cross-boot timestamp mapping remains unproven. The pre-existing `BackendHttp::end()` worktree difference was not changed by plan 12.
- Final gate: **BLOCKED**. The diagnostic changes and fail-closed behavior are useful, but transmission progress and lossless capture are not resolved. Do not declare rollout healthy and do not make speculative TLS behavior, queue-capacity, partition, format, or erase changes. `D:\github\MotorDiagnosis` remains intentionally dirty and uncommitted/unpushed.

## 2026-09-11 cursor collision fix agent run and hardware result

- Agent 1 implemented `RawSpoolIndex::nextEmpty()` with circular search from the current cursor and an `Unreadable` quarantine state. The coordinator received the result, reviewed the diff, and kept the change limited to catalog selection plus its runtime call site and native tests.
- Native tests passed `178/178`; N8 build and target diff check passed. Firmware-only upload to COM7 succeeded with flash hash verification. No filesystem image, format, erase, or partition change was used, and MotorDiagnosis was not committed or pushed.
- The sanitized retest log is documented in `evidence/serial/MotorDiagnosis_COM7_N8_20260911_cursor_fix_retest_report.md`; the raw monitor file remains local because it contains device/network identifiers.
- Boot catalog reported `512 scanned / 269 present / 269 readable / next=62 / full=no` in `32.644 s`. During about 246 seconds, 79 durable writes skipped occupied slots and no `slot=512` false-full marker appeared.
- The run then reached genuine LittleFS exhaustion (`2,752,512 total / 2,752,512 used / 0 free`). Final counters were `actual_drop_total=233`, `capture_queue_full=233`, `processing_pending_full_events=11`, `raw_queue=8`, `pending=8`, and `spool_capacity_full=31`.
- HTTP entry/result, ACK, and delete markers were not observed. TLS `start_ssl_client: -1` appeared three times but cannot be assigned to the Raw path because Raw HTTP was never reached. No panic, watchdog, Guru Meditation, or reboot occurred.
- Current gate: cursor collision fix **PASS**; firmware rollout health **BLOCKED**. Next numbered task is to isolate slow LittleFS persistence from capture/processing without changing the external API, ACK durability, or storage destructive policy.

## 2026-09-11 reboot 후 5분 실장비 로그

- COM7을 GPIO0 high 유지 후 EN pulse 방식으로 재부팅하여 정상 애플리케이션 부팅을 확인했다. 로그는 `evidence/serial/MotorDiagnosis_COM7_N8_20260911_reboot_5min.log`에 저장했다.
- Boot session `1946`, LittleFS mount 성공, `2747 / 24999` backlog 복구, CRC/schema-invalid `0`, legacy v1 `0`, ADXL345 `0xE5`, Raw `samples=512 quality=valid`를 확인했다.
- 부팅 시 LittleFS는 total `2,752,512`, used `2,363,392`, free `389,120`이었다. Raw spool은 신규 1건 저장 후 slot `512`에서 `RAW-SPOOL-FULL`이 반복됐다.
- Wi-Fi startup이 offline으로 남았고 reconnect timeout `27`회가 발생했다. NTP, HTTP, TLS, ACK 이벤트는 관측되지 않아 전송/TLS 판정은 보류한다.
- 최종 `raw_queue=8`, `pending=8`, `capture_queue_full=331`, `actual_drop_total=331`, `Dropped windows=332`, `processing_pending_full_events=178`, audio overwrite `76`회였다. storage timing 187건은 모두 `>1280ms`, 최대 `1,945,610us`였다.
- 해당 실행에서 panic, watchdog, assert, 추가 reboot는 없었다. 결론은 **정상 부팅·복구는 통과했지만 Wi-Fi 미연결 상태에서 queue/storage backpressure와 실제 window loss가 재현되어 rollout은 BLOCKED**다.
- 기존 루트의 `MotorDiagnosis_COM7_N8_20260911_115200_5min.log`와 report는 과거 기준선으로 `evidence/serial/archive/`에 이동했다. 프로젝트 밖 원본은 제거했다.

## 2026-09-11 핫스팟 재부팅 후 5분 실장비 로그

- 핫스팟을 켠 뒤 COM7을 GPIO0 high 유지 후 EN pulse 방식으로 재부팅했고, `evidence/serial/MotorDiagnosis_COM7_N8_20260911_hotspot_reboot_5min.log`에 cmd 화면 출력과 원본 로그를 함께 저장했다.
- 정상 앱 부팅, Boot session `1947`, LittleFS mount, backlog `2747 / 24999` 복구, CRC/schema-invalid `0`, legacy v1 `0`, ADXL345 `0xE5`를 확인했다.
- Wi-Fi 연결 성공(IP `172.20.10.2`, RSSI `-41 dBm`) 및 NTP 동기화 성공으로 이전 실행의 offline 원인은 제거됐다.
- Backend health HTTPS는 `start_ssl_client: -1`로 실패했다. Raw TX는 `loop_ready → time_ready → priority_scan_begin`까지 도달했지만 HTTP/ACK/delete 이벤트는 없었다.
- Raw spool은 2건 저장 후 slot `512` full을 반복했다. 최종 `raw_queue=8`, `pending=8`, `capture_queue_full=419`, `actual_drop_total=419`, `processing_pending_full_events=174`, `spool_capacity_full=182`였다.
- storage timing 184건은 모두 `>1280ms`, 최대 `2,463,322us`였다. Audio overwrite, panic, watchdog, assert, 추가 reboot는 관측되지 않았다.
- 결론은 **Wi-Fi/NTP는 통과했지만 TLS health 실패, Raw selector 정체, queue/storage backpressure 및 실제 window loss로 rollout BLOCKED**다. TLS 하위 원인은 Raw HTTP 미진입으로 확정하지 않는다.

## 2026-09-11 구조 최적화 및 selector 리팩토링

- `13_ESP32_구조최적화_리팩토링_관리대장_2026-09-11.md`에 13-01~13-08 번호로 기준선, 오류 전파, 변경, 검증, 구조도를 기록했다.
- `raw_spool_index.h`를 추가해 부팅 시 raw spool header를 1회 복구하고, 런타임 batch/priority/state/oldest 조회는 512-entry RAM catalog에서 처리한다. 기존 selector의 batch당 최대 2,048회 filesystem 조회는 RAM 512항목 1회 순회와 선택 파일 최대 4개 읽기로 축소됐다.
- `main.cpp`에서 telemetry queue 처리 뒤 도달 불가능했던 legacy 직접 HTTP 분기 164줄을 삭제해 summary 전송 경로를 하나로 유지했다.
- 검증은 native `175/175`, N8 build 성공, diff check 통과다. RAM은 107,732 / 327,680 bytes(32.9%), Flash는 1,093,437 / 3,342,336 bytes(32.7%)다.
- `MotorDiagnosis_main_architecture.html`을 architecture-diagram-generator v1.1 스타일로 갱신했다. 동기 LittleFS 쓰기와 TLS `-1`은 미해결 문제로, selector→HTTP 진행은 코드 수정 후 실장비 확인 대기로 표시했다.
- 실장비 upload/log는 수행하지 않았다. 처리 task의 동기 LittleFS 쓰기, restored/current boot 시간축 매핑, TLS 하위 원인은 변경하지 않았으며 rollout 판정은 계속 **BLOCKED**다. `MotorDiagnosis` 변경은 사용자 요청 전 commit/push/PR하지 않는다.

## 2026-09-11 spool index 리팩토링 실장비 재검증

- 사용자 요청으로 N8 firmware를 COM7에 업로드했고 모든 flash hash가 검증됐다. filesystem image, LittleFS format, erase-all은 수행하지 않았다.
- 약 85초 관측에서 persisted selector는 stat/header filesystem 조회 없이 84 us, pending selector는 283~397 us로 완료되어 기존 selector 정체 제거를 확인했다.
- 부팅 index 복구는 265개 header에서 32.54초가 걸렸다. 첫 빈 slot 42에 1건 저장한 뒤 cursor 43이 사용 중이자 앞쪽 빈 slot을 순환 탐색하지 않아, 266/512개만 사용하고 free 335,872 bytes인 상태에서 `slot=512` false-full을 반복했다.
- pre-send persistence 9회 실패로 Raw `http_begin`, ACK, delete는 0회였다. 최종 `actual_drop_total=59`, `spool_capacity_full=29`, raw/pending queue 8/8이었다.
- TLS `-1`은 health 요청에서 1회 재현됐고 panic/watchdog/reboot는 없었다. selector는 PASS지만 end-to-end는 FAIL이며 rollout은 계속 **BLOCKED**다.
- 원본 serial 로그에는 네트워크·장치 식별자가 있어 로컬에만 유지하고, 비식별 결과 보고서만 push한다. 제품 코드는 이번 검증에서 추가 수정하지 않았고 `MotorDiagnosis` commit/push/PR도 수행하지 않았다.
