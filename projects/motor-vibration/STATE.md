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
