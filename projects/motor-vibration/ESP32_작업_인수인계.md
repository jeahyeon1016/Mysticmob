# 모터 진동 프로젝트 — ESP32 작업 인수인계

## 현재 상태

- 담당 범위: 하드웨어 및 ESP32 펌웨어
- 4주차 MVP: 완료
- 소스 저장소: `D:\ai agent`
- 최신 원격 저장소: https://github.com/jeahyeon1016/ai-agent
- 외부 소스 작업본: `D:\github\MotorDiagnosis`
- 개인 정리 저장소: `D:\ai agent` / https://github.com/jeahyeon1016/ai-agent
- 외부 MotorDiagnosis 변경은 아직 로컬 검증 단계이며, 이번 기록에서는 공용 PR을 갱신하지 않음

## 완료된 작업

- 운영 HTTPS endpoint 및 device/site/asset ID 적용
- 장치 상태 보고용 Bearer 인증 경로 추가
- CA 인증서 검증 활성화
- LittleFS 이전 세션 격리 처리 지연 개선
- API v1.3 `lifecycleUpdates` 중첩 ACK 메타데이터 허용
- 필수 ACK 필드 타입 검증 유지 및 회귀 테스트 추가
- ESP32 build/upload 및 실서버 전송 검증
- Raw·Health·Telemetry를 단일 Network Task에서 순차 관리하는 로컬 수정안 작성
- Raw/Health persistent client 재사용 및 replay backoff 방향 적용
- Raw ACK의 `accepted` boolean `true` 검증 보강

## 기존 검증 결과

- Native firmware tests: `56/56 PASS`
- Live HTTPS telemetry: `HTTP 201`, `accepted=true`
- Replay duplicate: `HTTP 200`, `duplicate=true`
- 장치: `DEV-01-MOT-02`

## 2026-09-10 실장비 재검증 결과

- LittleFS 영역만 초기화하여 기존 대기 데이터를 제거함. 초기 로그: `Restored Queue : 0 / 24999`.
- N8 빌드 성공 및 COM7 업로드 성공.
- Wi-Fi/RSSI/NTP는 연결 구간에서 정상.
- Raw 샘플은 반복적으로 `512`, `quality=valid`.
- Raw/Health는 간헐적으로 각각 HTTP `202`/`200` 성공했지만, Raw 요청이 약 4~6초 소요 후 HTTP `-1` 또는 `-11`로 실패하는 구간이 반복됨.
- Telemetry replay는 `network-reserved`로 보류되는 구간이 반복됨.
- `pending=8`, `raw_queue=8`, `WINDOW dropped`, `audio overwritten` 재현.
- 초기 큐 0에서 관찰 중 약 68까지 증가하여 전송 병목이 해결되지 않았음을 확인.
- 재부팅·LittleFS assertion·메모리 부족은 이번 관찰에서 확인되지 않음.
- 보드 로그에 토큰·Wi-Fi 비밀번호는 기록하지 않음.

## 최근 구조 검토

- 단일 Network Task는 HTTP 요청 간 게이트 경합을 줄이지만, 동기식 `POST()`가 수 초 걸리면 해당 태스크가 계속 점유되는 한계가 있음.
- Replay마다 새 TLS를 만드는 경로는 줄였지만, 실제 핫스팟/TLS 지연 자체는 남아 있음.
- MQTT는 현재 제공된 API 명세에 포함되어 있지 않음. MQTT로 전환하려면 Broker, Subscriber, topic, QoS, application ACK 명세가 Backend와 별도로 필요함.
- HTTP `202/200`과 MQTT `PUBACK`은 의미가 다르므로 MQTT 전환 시 Raw 저장 ACK를 별도 설계해야 함.

## 서버 transmission 계약 메모

- 요청 최상위에 선택적 `transmission`을 추가할 수 있음.
- `mode`: `periodic`, `priority`, `replay`.
- `reason`: `none`, `rms_low`, `rms_high`, `severe`, `quality`, `baseline_missing`, `storage_pressure`, `recovery_hold`.
- `baselineId`와 부팅 누적 `droppedWindows`를 함께 기록.
- 구형 `windows` 전용 요청은 계속 허용.
- HTTP `202`는 신규 저장, HTTP `200`은 동일 원본 중복 ACK이며 RF66 판정 성공을 의미하지 않음.
- 정기/재전송 과거 데이터는 분석 이력으로 남고 현재 이벤트 상태와 분리됨.

## 보류 및 주의

- adaptive N8 파티션은 파일시스템을 약 4.69MiB로 늘리지만 두 번째 OTA 슬롯을 제거하므로 적용 전 백업·마이그레이션·OTA 정책 확인이 필요함.
- 실제 보드의 5분 저장 용량, Flash 쓰기 지연, 전원 차단 복구, 장시간 처리량은 미검증.
- 현재 외부 소스 변경은 commit/push/PR하지 않음. 사용자 요청 시 별도 처리.

## 다음 작업

- HTTPS 전송 병목 추가 원인 분석 및 최소 수정
- `transmission` 메타데이터 펌웨어 연동 여부 결정
- MQTT 전환 필요 시 Backend와 별도 프로토콜 명세 작성
- 모터 사양과 실제 설치 환경 확정
- 센서 부착 위치 및 측정 목표 확정
- 확정 후 실장비 검증 계획 수립

## 주의사항

- 실제 토큰과 `secrets.h`는 커밋하지 않음.
- Backend·Frontend·AI 기능은 담당 범위에서 제외.
- 저장소의 unrelated 변경사항은 별도로 보존.

## 이슈 작성 양식

이슈 작성 시 담당 파트, 담당자, 우선순위, 주차·기능 ID, 현재/변경 상태, 작업 범위, 제외 범위, API 연동, 완료 기준, 검증 방법, 참고 자료를 반드시 기록한다.
