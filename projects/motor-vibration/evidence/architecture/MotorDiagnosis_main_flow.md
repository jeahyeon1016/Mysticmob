# MotorDiagnosis main 흐름 정리

작성 기준: 2026-09-11 코드와 핫스팟 실장비 로그

## 1. 먼저 확인한 소스 위치

실제 PlatformIO 빌드가 성립하는 완전한 firmware tree는 `D:\github\MotorDiagnosis\firmware\esp32_edge_node`다. `D:\ai agent\projects\motor-vibration\firmware\esp32_edge_node`에는 `main.cpp`, `firmware_logic.cpp`, `continuous_vibration_runtime.h`만 있고, `main.cpp`가 include하는 다음 파일들이 빠져 있다.

- `backend_http.h`
- `communication_quality.h/.cpp`
- `continuous_vibration_health.h`
- `device_health.h/.cpp`
- `edge_analysis.h/.cpp`
- `remote_config.h/.cpp`
- `vibration_window.h/.cpp`

따라서 이 문서와 다이어그램의 “실행 기준”은 완전한 `D:\github\MotorDiagnosis`이며, `ai-agent` 쪽은 문서/증거 저장소이자 불완전한 미러로 표시했다. 이번 리팩토링 소스 변경은 `MotorDiagnosis` 작업 트리에만 있으며 commit/push하지 않았다.

## 2. 빌드 모드

`platformio.ini`의 기본 환경은 ESP32-S3, Arduino, 8 MB flash, PSRAM 플래그이며 다음 기능이 켜져 있다.

- `CONTINUOUS_VIBRATION_ENABLED=1`
- `RAW_VIBRATION_ENABLED=1`
- `RAW_VIBRATION_DEBUG=1`
- ArduinoFFT, ArduinoJson

이 매크로 조합 때문에 구형 on-demand 경로가 아니라 `ContinuousVibration` 경로가 실제 주 경로다.

## 3. 부팅 및 런타임 진입

`main.cpp:setup()` 순서:

1. `BackendHttp::gate` mutex와 Serial 초기화
2. NVS `Preferences` 초기화
3. 원격 설정 복원 및 sequence/time-anchor 이력 로드
4. boot session 할당
5. LittleFS mutex, ring 파일 초기화, durable queue 복구
6. Device Health / Communication Quality 초기화
7. ADXL345 SPI, 오디오 링, 센서 observation queue, telemetry outbox 생성
8. `AudioCaptureTask` 생성; continuous 모드에서는 `VibrationTask` 대신 `ContinuousVibration::start()` 실행
9. Wi-Fi non-blocking 접속 및 NTP 동기화
10. pipeline ready

`setup()`이 만든 핵심 태스크:

| 태스크 | 코어 | 역할 | 주요 경계 |
|---|---:|---|---|
| `VibrationFIFO` | 0 | ADXL345 FIFO를 계속 읽어 0.64초 Raw window 생성 | `rawQueue` 8개, non-blocking send |
| `WindowFeatures` | 0 | Raw 진동 feature/오디오 window 분석 후 pending으로 전달 | `pending` 8개, `rawHold` 8개 |
| `AudioCaptureTask` | 1 | INMP441 I2S DMA를 계속 비워 오디오 ring 유지 | 오디오 ring + canary |
| `WindowHTTPS` | 1 | Raw spool/pending을 JSON으로 encode하고 HTTPS 전송 | raw network reservation |
| Arduino `loop()` | framework task | 상태·시간·설정·품질·분석·요약 telemetry 보조 | `telemetryTransmitQueue` 4개 |

## 4. 실제 Raw 진동 데이터 흐름

```text
ADXL345 SPI FIFO
  -> VibrationFIFO/captureTask
  -> Raw {xyz[Samples], audioWindow, index, bootId, quality}
  -> rawQueue(8)
  -> WindowFeatures/processingTask
  -> extract() + analyzeCommonAudioWindow()
  -> pending(8)
       ├─ 여유 있음: pending에 저장
       ├─ pending full: rawHold(8)에 보류, 불가하면 drop
       └─ WindowHTTPS가 가져감
  -> LittleFS raw spool + RawSpoolIndex(RAM metadata)
       ├─ 부팅 때 512 slot header 1회 복구
       └─ 이후 선택은 RAM 512항목 1회 순회 + 선택 파일 최대 4개 읽기
  -> raw JSON/base64 encode
  -> WiFiClientSecure + BackendHttp
  -> /api/devices/{DEVICE_ID}/raw-vibration-windows
  -> strictRawAck()
  -> ACK이면 pending 제거 또는 raw spool 파일 제거
```

`rawQueue`가 가득 차면 `finish()`는 센서 reader를 막지 않고 `processingDrops`를 증가시킨다. `pending`이 가득 차면 `processingTask`가 `rawHold`로 옮기며, 둘 다 가득 차면 추가 drop이다.

## 5. 저장소와 복구

- 일반 telemetry: LittleFS의 고정 binary ring, 25,000 physical slots / 24,999 logical capacity
- Raw overload: `/raw-spool-v2-{slot}.bin`, 현재 설정은 512 slots
- Raw spool metadata: `raw_spool_index.h`의 512-entry RAM catalog; file 존재·header 반복 scan 대신 mode/cutoff/index/quarantine 선택을 담당
- NVS: sequence, boot session, time anchor, health journal, remote config, quality outbox
- 모든 주요 파일 쓰기는 flush/close/read-back/CRC 검증을 거친다.
- 부팅 시 `restoreQueueFromFlash()`가 ring 전체를 검사하고 committed watermark 이후의 record를 복원한다.

## 6. `loop()`의 분기

매 loop마다 다음 auxiliary service를 먼저 호출한다.

`serviceWiFi()` → 시간 복구/anchor → `serviceCommunicationQuality()` → `serviceRemoteConfiguration()` → `serviceEdgeAnalysis()` → `serviceSensors()`

그 후 continuous snapshot을 읽어 packet을 만든다.

- UTC 미확정: LittleFS ring에 보존
- 기존 backlog 존재: ring tail에 보존
- Wi-Fi offline: ring에 보존
- 정상 online + backlog 없음: `telemetryTransmitQueue(4)`에 pointer enqueue
- 다음 service pass에서 `serviceTelemetryNetworkOnce()`가 `postPacket()` 호출

현재 continuous 모드에서는 `serviceEdgeAnalysis()`가 즉시 return한다. `loop()`의 `telemetryTransmitQueue` enqueue/fallback 뒤 무조건 return하던 아래쪽 legacy 직접 `postPacket()` 분기 164줄은 삭제해 summary 전송 소유자를 `serviceTelemetryNetworkOnce()` 하나로 고정했다.

## 7. 외부 API 경계

기본 URL은 `INGEST_URL`에서 `/api/telemetry/ingest` suffix를 제거한 base를 사용한다.

| 채널 | 메서드/경로 | 호출 주체 | 상태 보존 |
|---|---|---|---|
| Raw window | POST `/api/devices/{id}/raw-vibration-windows` | `WindowHTTPS` | strict ACK 전까지 pending/spool 유지 |
| Summary telemetry | POST `/api/devices/{id}/vibration-windows` | `serviceTelemetryNetworkOnce()` | 실패 시 ring 보존 |
| Backend time | GET `/api/health` | NTP 실패 fallback / time recovery | 성공 전 timestamp unresolved |
| Device health | POST `DEVICE_HEALTH_URL` | `serviceDeviceHealth()` | journal + ACK boundary 보존 |
| Remote config | GET `/configuration/pending`, POST `/configuration/result` | `serviceRemoteConfiguration()` | 검증 후 NVS 반영 |
| Quality | POST `DEVICE_QUALITY_URL` | `serviceCommunicationQuality()` | NVS outbox 보존 |
| Edge analysis | GET/POST `/api/devices/{id}/analysis[/pending]` | legacy-only | continuous 모드 비활성 |

모든 HTTPS 요청은 `beginBackendHttp()`의 transport policy와 `BACKEND_CA_CERT` 검사를 통과해야 한다. `BackendHttp::gate`와 `networkReservation`이 Raw/Health/Telemetry의 동시 TLS 사용을 직렬화한다.

## 8. 실장비 로그와 코드의 접점

리팩토링 전 핫스팟 5분 로그(`MotorDiagnosis_COM7_N8_20260911_hotspot_reboot_5min.log`)는 다음을 확인했다.

- Wi-Fi/NTP: 성공
- `rawQueue=8`, `pending=8`
- actual drop `419`, raw spool capacity full `182`
- LittleFS 저장 샘플 `184개`, 모두 1280 ms 초과, 최대 약 2.463 s
- Backend health TLS: `-1`
- Raw TX stage: `priority_scan_begin` 이후 HTTP/ACK 없음
- crash/watchdog/assert/additional reboot: 없음

리팩토링은 selector의 반복 파일 조회를 RAM 인덱스로 격리했다. 따라서 다음 실장비 검증에서는 `RAW-SELECT-DIAG` 종료와 `http_begin`이 나타나는지 먼저 확인한다. 처리 task의 동기 LittleFS 쓰기와 TLS `-1`은 변경하지 않았으므로 다이어그램의 붉은 위험 구간으로 남긴다.

## 9. 리팩토링 결과와 다음 검증

1. 추가 파일시스템 scan 없이 인덱스 선택 순서, cutoff, mode, quarantine, erase를 native 테스트 3개로 고정했다.
2. 전체 native 175/175와 N8 빌드가 통과했다. RAM은 인덱스 때문에 8,192 bytes 증가해 32.9%다.
3. 실장비에서 selector 종료→HTTP→ACK/delete 진행을 확인해야 한다.
4. LittleFS 쓰기가 관측상 1.28~2.463초인 반면 Raw window는 0.64초마다 생성되므로, 지속 오프라인에서 무손실을 요구하면 파일 배치 형식 또는 별도 저장 파이프라인을 별도 설계해야 한다.
5. TLS `-1`은 selector와 분리해 HTTP 하위 진단으로 계속 추적한다.

## 10. 실장비 재검증 결과

1. runtime selector는 filesystem 재조회 없이 84~397 us에 완료되어 기존 selector 정체가 해소됐다.
2. 부팅 catalog 복구는 265개 header에서 32.54초가 걸려 startup 지연으로 남았다.
3. `writeRawSpool()` cursor는 다음 slot 하나만 검사한다. 첫 빈 slot 42 저장 후 43이 사용 중이자 다른 빈 slot을 찾지 않고 false-full을 반환했다.
4. 이 때문에 pre-send durability가 HTTP보다 먼저 9회 실패했고 Raw HTTP/ACK/delete는 실행되지 않았다.
5. 약 85초 만에 actual drop 59가 발생했다. 다음 수정 경계는 네트워크가 아니라 catalog 기반 circular empty-slot 선택이다.

이 문서는 리팩토링 후 코드 구조와 아직 실장비에서 확인하지 못한 경계를 함께 기록한다.
