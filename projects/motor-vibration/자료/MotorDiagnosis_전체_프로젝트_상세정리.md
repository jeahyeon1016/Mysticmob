# MotorDiagnosis 전체 프로젝트 상세 정리

> 기준 상태: ESP32 펌웨어 `v1.2-beta.11.8.5`, PR #13 병합 완료 기준  
> 프로젝트 목적: 모터의 진동·음향 상태를 ESP32-S3에서 측정·특징 추출하고, 네트워크를 통해 백엔드로 전송하여 모터 상태 모니터링·이상 감지·향후 AI 학습에 활용하는 Edge AI 센서 노드

---

## 1. 프로젝트 한눈에 보기

MotorDiagnosis는 모터에 부착한 **진동 센서 ADXL345**와 모터 주변 음향을 수집하는 **INMP441 MEMS 마이크**를 ESP32-S3에 연결하여 진동/음향 특징을 계산하고 서버로 전송하는 시스템이다.

```text
[모터]
  │
  ├─ ADXL345 3축 가속도 ── SPI ──┐
  │                               │
  └─ INMP441 음향 ─────── I2S ───┤
                                  ▼
                             ESP32-S3
                                  │
                    640 ms 동기화 측정/분석
                                  │
                ┌─────────────────┴──────────────────┐
                │                                    │
          진동 특징 추출                        음향 특징 추출
        - 3축 RMS 계산                       - Raw PCM RMS
        - 합성 RMS                           - FFT Peak Hz
        - 최강축 FFT Peak
                │                                    │
                └─────────────────┬──────────────────┘
                                  ▼
                         Telemetry JSON 생성
                                  │
                        Wi-Fi / HTTPS REST API
                                  │
                                  ▼
                              Backend
                                  │
                     저장 / 조회 / 이벤트 / AI
```

네트워크 장애 시에는 데이터가 사라지지 않도록 ESP32의 **LittleFS Flash 영역**에 임시 보관했다가 연결이 복구되면 FIFO 순서로 재전송한다.

---

## 2. ESP32 보드 / 메모리

### 2.1 실제 보드

| 항목 | 현재 상태 |
|---|---|
| MCU | ESP32-S3 |
| 실제 칩 리비전 | rev 0.2 |
| 펌웨어 타깃 | `esp32-s3-devkitc-1` |
| PlatformIO 표시 보드 | ESP32-S3-DevKitC-1-N8 |
| Flash | 8 MB |
| 내부 RAM | 320 KB |
| 최종 펌웨어 | `v1.2-beta.11.8.5` |

최종 빌드에서 내부 RAM 약 **65.7%**, Flash app 영역 약 **15.3%** 사용이 확인됐다.

### 2.2 PSRAM 상태

실보드 업로드 과정의 esptool에서는 **Embedded PSRAM 8 MB**가 감지된 적이 있다.

하지만 현재 PlatformIO 빌드 프로파일은:

```text
ESP32-S3-DevKitC-1-N8 (8 MB QD, No PSRAM)
```

로 동작하고 있고, 현재 펌웨어 코드도 `ps_malloc()`, `heap_caps_malloc(...SPIRAM...)` 같은 **PSRAM 전용 할당을 사용하지 않는다.**

따라서 현재 프로젝트 기준 판단은:

> **보드 하드웨어에는 PSRAM이 존재할 가능성이 높지만, 현재 MotorDiagnosis 펌웨어는 PSRAM을 실제 데이터 저장 공간으로 사용하도록 구성되어 있지 않다.**

---

## 3. Wi-Fi가 끊기면 어디에 저장되는가

이 부분은 **RAM과 LittleFS를 구분해야 한다.**

### 3.1 RAM에 있는 것

센서가 실시간으로 측정하는 **원시 신호/분석용 버퍼**는 RAM에 있다.

- ADXL345 X/Y/Z 샘플 배열
- INMP441 연속 PCM audio ring
- FFT 계산 배열
- 640 ms 공통 분석 window
- 현재 TelemetryPacket

INMP441의 연속 음향 ring buffer는:

- 16 kHz
- 16,384 samples
- 약 1.024초 분량

이며 **RAM의 실시간 처리용 ring buffer**다. 전원을 끄면 이 raw RAM 데이터는 사라진다.

### 3.2 Wi-Fi 장애 때 보관되는 데이터

Wi-Fi가 끊어져 서버 전송에 실패한 뒤 남겨야 하는 데이터는 **RAM이나 PSRAM이 아니라 LittleFS Flash**에 저장된다.

```text
센서 측정
   ↓
특징 추출
   ↓
Telemetry packet 생성
   ↓
HTTP/HTTPS 전송 실패
   ↓
LittleFS binary ring에 영구 저장
   ↓
재부팅해도 유지
   ↓
Wi-Fi 복구
   ↓
FIFO replay
   ↓
서버 ACK 확인
   ↓
ring에서 consume
```

| 용도 | 저장 위치 | 전원 OFF 후 유지 |
|---|---|---:|
| 실시간 ADXL raw sample | RAM | X |
| 실시간 INMP441 PCM ring | RAM | X |
| FFT 계산 버퍼 | RAM | X |
| 전송 대기 Telemetry | **LittleFS Flash** | **O** |
| sequence / watermark / 시간 anchor | NVS | O |
| isolation/rejected 데이터 | LittleFS Flash | O |

**현재 Wi-Fi 장애 대비 저장소는 PSRAM이 아니다. LittleFS Flash다.**

---

## 4. 센서 구성 및 정확한 배선

### 4.1 ADXL345 — 진동 센서

통신: **SPI**

| ADXL345 | ESP32-S3 |
|---|---:|
| VCC | 3.3V |
| GND | GND |
| CS | GPIO10 |
| SDI / MOSI | GPIO11 |
| SCL / SCK | GPIO12 |
| SDO / MISO | GPIO13 |

SPI 설정:

```text
SPI Bus  : FSPI
Clock    : 5 MHz
Bit Order: MSB First
Mode     : SPI MODE 3
```

실보드에서 ADXL345 `DEVID = 0xE5` 정상 확인 완료.

### 4.2 INMP441 — 음향 센서

통신: **I2S**

| INMP441 | ESP32-S3 |
|---|---:|
| VDD | 3.3V |
| GND | GND |
| SCK / BCLK | GPIO4 |
| WS / LRCLK | GPIO5 |
| SD | GPIO6 |
| L/R | GND |

`L/R → GND` 기준으로 Left channel을 사용한다.

---

## 5. 진동 데이터 수집 구조

ADXL345 설정:

- 측정 범위: **±16 g / Full Resolution**
- 환산 계수: 약 **0.0039 g/LSB**
- Sample rate: **800 Hz**
- Sample count: **512**
- 공통 측정 window: **640 ms**
- Sample period: **1.25 ms**

```text
512 samples / 800 Hz = 0.64 sec
```

각 축 RMS를 계산하고 3축 합성 RMS를 구한다.

```text
vibrationRmsRaw
= sqrt(RMS_X² + RMS_Y² + RMS_Z²)
```

단위는 `[g]`다.

FFT는 세 축 중 해당 window에서 **RMS가 가장 큰 축**을 선택한 뒤 Hamming window + FFT를 적용해 dominant frequency를 구한다.

최종 전송 진동 feature:

- `vibrationRmsRaw`
- `vibrationPeakHz`

---

## 6. 음향 데이터 수집 구조

INMP441 설정:

- Sample rate: **16 kHz**
- 연속 audio ring: **16,384 samples**
- RAM ring 분량: 약 **1.024초**
- DMA read: **128 samples = 약 8 ms**

진동 window와 동일한 640 ms 구간:

```text
10,240 / 16,000 Hz = 640 ms
```

을 분석한다.

음향 FFT:

- 2048-point FFT
- 10,240 samples = 2048 × 5 blocks
- 5개 magnitude spectrum을 평균
- 평균 spectrum의 dominant peak를 `acousticPeakHz`로 사용

최종 전송 음향 feature:

- `acousticRmsRaw`
- `acousticPeakHz`

현재 `acousticRmsRaw`는 보정 전 raw PCM RMS다.

---

## 7. 센서 동기화 / 멀티코어

실보드 기준:

```text
Vibration Task        -> Core 0
Continuous Audio Task -> Core 1
```

두 센서 모두 논리적으로 **640 ms common window**를 사용한다.

실제 전체 acquisition/분석 시간은 약:

```text
985 ~ 987 ms
```

정도로 확인됐다.

---

## 8. 현재 서버로 전송하는 특징 및 메타데이터

핵심 feature 4개:

| 필드 | 의미 | 단위/상태 |
|---|---|---|
| `vibrationRmsRaw` | 3축 합성 가속도 RMS | g |
| `vibrationPeakHz` | dominant vibration frequency | Hz |
| `acousticRmsRaw` | INMP441 raw PCM RMS | raw, 미보정 |
| `acousticPeakHz` | dominant acoustic frequency | Hz |

Telemetry 주요 필드:

```text
timestamp
sequence
siteId
assetId
deviceId
rpm
vibrationRmsRaw
vibrationRmsMmS
vibrationPeakHz
acousticRmsRaw
acousticDb
acousticPeakHz
scenarioLabel
knownVibrationLabel
knownAcousticLabel
source
isSynthetic
vibrationUnitNote
acousticUnitNote
```

현재:

```text
rpm                = null
vibrationRmsMmS    = null
acousticDb          = null
scenarioLabel      = null
knownVibrationLabel= null
knownAcousticLabel = null
source              = "esp32-s3"
isSynthetic         = false
```

실제 ESP32는 임의의 ground truth label을 생성하지 않는다.

---

## 9. 장치 식별자

```text
siteId   = SITE-01
assetId  = SITE-01-MOT-02
deviceId = DEV-01-MOT-02
```

---

## 10. 통신 방식

```text
ESP32-S3
  ↓
Wi-Fi
  ↓
HTTPS REST API
  ↓
POST /api/telemetry/ingest
  ↓
Backend
```

Header:

```text
Authorization: Bearer <device-token>
Content-Type: application/json
```

운영 기준은 **HTTPS + CA 검증 + Bearer token**이다.

평문 HTTP는 로컬 개발에서만 명시적인 opt-in으로 허용한다.

---

## 11. 서버 ACK 계약

신규 정상 저장:

```text
HTTP 201
accepted=true
duplicate=false
```

동일 packet 재전송:

```text
HTTP 200
accepted=true
duplicate=true
```

ESP32는 2xx status만 보고 데이터 삭제하지 않고 다음을 검증한다.

- strict top-level JSON
- malformed/trailing/nested 여부
- duplicate required key 여부
- `accepted == true`
- `deviceId` 일치
- `sequence` 일치
- 201 → `duplicate=false`
- 200 → `duplicate=true`

검증 실패 시 telemetry는 보존한다.

---

## 12. Sequence 내구성

각 packet은 `sequence`로 식별된다.

sequence는 packet에 사용되기 전에 **NVS에 먼저 저장하고 read-back 확인**을 거친다.

서버 idempotency key는 사실상:

```text
(deviceId, sequence)
```

이다.

동일 sequence + 동일 payload는 duplicate-safe하게 처리되고, 동일 sequence + 다른 payload는 `SEQUENCE_CONFLICT`가 된다.

---

## 13. LittleFS Offline Binary Ring

현재 production 저장 구조:

```text
Physical slots : 25,000
Logical records: 24,999
Record size    : 48 bytes
Ring size      : 약 1.20 MB
```

spare slot 1개를 두어 full queue에서 새 record write/verify 전 기존 oldest를 먼저 삭제하지 않는다.

안전 순서:

```text
새 packet spare slot write
        ↓
read-back + CRC 검증
        ↓
새 데이터 durable 확인
        ↓
기존 oldest consume
```

Overflow 정책은 **oldest-drop**이며 최신 약 24시간 이상 window를 유지한다.

---

## 14. Ring record 무결성

read 결과를 개념적으로:

```text
OK
CORRUPT
IO_ERROR
```

로 구분한다.

- `OK`: 정상 처리
- `CORRUPT`: 실제 CRC/schema 손상으로 확인된 경우만 skip
- `IO_ERROR`: 일시적 filesystem 오류 가능성이 있으므로 **consume하지 않음**

LittleFS mount 실패 시 자동 format도 하지 않는다.

---

## 15. ACK Watermark / Power-Cut 복구

ACK마다 1.2MB ring 전체를 rewrite하지 않고 consumed ordinal을 NVS watermark로 관리한다.

전원 차단으로 watermark commit 전 ACK된 record가 다시 replay될 수 있으나 backend idempotency 때문에 안전하다.

실제 USB power-cut 테스트에서:

```text
서버 저장 성공
→ watermark commit 전 power cut
→ reboot
→ 동일 sequence replay
→ HTTP 200 duplicate:true
```

가 확인됐다.

전달 semantics:

```text
at-least-once + backend idempotency
```

이다.

---

## 16. Bounded FIFO Replay

backlog 전체를 비울 때까지 센서 측정을 막지 않는다.

현재 loop 1회당:

```text
최대 4 record replay
```

후 다시 신규 센서 acquisition을 수행한다.

따라서:

- FIFO 유지
- backlog 복구
- 신규 데이터 측정 지속

을 동시에 만족한다.

---

## 17. Timestamp / Offline Cold Boot

정상 온라인 부팅에서는 NTP로 UTC를 얻는다.

인터넷 없는 완전 cold boot에서는 ESP32가 absolute UTC를 알 수 없으므로 **가짜 timestamp를 만들지 않는다.**

현재 unresolved record에는 boot session/generation + capture monotonic time을 저장한다.

같은 boot session에서 나중에 UTC anchor를 얻으면 실제 monotonic delta로 시간을 복원할 수 있다.

이전 boot session에서 UTC anchor 없이 전원이 꺼진 데이터는 absolute time 복원이 불가능하므로 임의 시간으로 전송하지 않고 isolation에 보존한다.

---

## 18. Durable Time Anchor

UTC sync 후 시간 anchor는 세대 및 무결성 검증이 가능한 형태로 저장한다.

개념 필드:

```text
session/generation
monotonicMs
epochMs
version
CRC
```

과거 boot session anchor가 새 session record에 적용되지 않도록 검증한다.

---

## 19. Legacy Ring Migration

이전 PR #13 초기 펌웨어는 unresolved record를:

```text
schema v1
TIME_UNRESOLVED
epochSeconds=0
```

형태로 저장했다.

신규 펌웨어는 이를 corruption으로 폐기하지 않고 legacy migration 대상으로 인식한다.

```text
legacy unresolved 인식
       ↓
isolation archive 저장
       ↓
저장 성공 확인
       ↓
ring consume
```

실제 구버전 펌웨어로 unresolved record 3개 생성 후 LittleFS를 유지한 채 신규 펌웨어로 업그레이드한 실보드 테스트에서:

```text
Legacy v1 unresolved records pending migration: 3
CRC/schema-invalid slots observed: 0
```

및 `MIGRATION → ISOLATION → RING Consumed` 순서를 확인했다.

---

## 20. Isolation / Rejected Archive

영구 거부 packet이나 timestamp를 정확히 복원할 수 없는 raw capture를 무한히 쌓지 않도록 bounded fixed-slot archive를 사용한다.

```text
/telemetry_rejected/
```

현재 설계는 **128 fixed slots** 기반이다.

즉시 영구 거부 packet도 먼저 persistent ring에 durable하게 저장한 뒤 archive 처리한다.

```text
packet
 ↓
ring write + verify
 ↓
isolation archive
 ↓
archive 성공
 ↓
ring consume
```

archive replacement 중 전원이 꺼져도 ring source가 남도록 설계했다.

---

## 21. Canonical Payload

first-send와 replay가 다른 정밀도로 JSON을 만들면 동일 sequence가 다른 payload가 되어 `SEQUENCE_CONFLICT`가 날 수 있다.

그래서 동일 canonical serializer를 사용한다.

현재 주요 정밀도:

```text
vibrationRmsRaw : 6 decimals
vibrationPeakHz : 2 decimals
acousticRmsRaw  : 2 decimals
acousticPeakHz  : 2 decimals
```

ACK loss 후 replay되어도 동일 payload가 유지되도록 한다.

---

## 22. HTTP 오류 분류

### Retryable

```text
network error
408
425
429
5xx
```

→ queue 유지 후 재시도

### Packet-permanent

```text
400 invalid packet
413 payload too large
403 TELEMETRY_LABEL_FORBIDDEN
409 SEQUENCE_CONFLICT
```

→ isolation

### Configuration / authorization

```text
401
일반 403
404
405
415
DEVICE_MAPPING_MISMATCH
redirect
기타 계약 미정 4xx
```

→ telemetry를 삭제하지 않고 설정 수정 대기

---

## 23. Backend / API

현재 문서 기준:

```text
Backend: MotorDiagnosis/0.2
API spec: v1.3
```

주요 telemetry API:

```text
POST /api/telemetry/ingest
POST /api/telemetry/bulk
GET  /api/telemetry
```

프로젝트 backend 영역에는 추가로:

- Site / Asset / Device
- Device health
- Install point
- Network profile
- Connectivity test
- Event / Event review
- Alert
- Dataset export/version
- Model registry
- Audit log
- Sensor/environment inspection
- Dashboard

기능이 존재한다.

---

## 24. `/api/health` 시간 계약

펌웨어는 API v1.3 기준 **RFC3339 문자열 timestamp** 파서를 구현했다.

확인 당시 backend 구현은 numeric epoch(`time.time()`)를 반환하고 있어 backend 담당 수정 후 실제 integration 확인이 필요하다고 기록했다.

---

## 25. 설치점 / 센서 부착 기준

프로젝트 install-point 데이터 기준:

```text
Position           : bearing housing top
Orientation        : horizontal X + vertical Z
Mounting Method    : bolt fixed bracket
Acoustic Direction : 1.5m from cooling side
Ambient Noise      : nearby rotating equipment
```

실제 외함 장착 시 ADXL345 PCB 방향과 이 기준이 일치하도록 **X/Y/Z 방향 표시를 남겨야 한다.**

---

## 26. 3D 프린팅 외함 상태

현재 ESP32/센서를 위한 외함을 3D 프린팅해 실제 체결 테스트 단계까지 진행했다.

사용한 출력 환경 기준:

- Bambu Lab X1 Carbon 사용
- 0.4 mm nozzle
- Bambu PLA Basic
- 0.2 mm standard layer
- support 포함 slicing

남은 기구 작업:

1. ESP32/센서 실물 삽입 확인
2. 배선 간섭 확인
3. 뚜껑 체결 확인
4. M3 나사 길이 실제 선정
5. 모터 bracket 체결
6. ADXL345 X/Y/Z 방향 표시
7. 센서가 흔들리지 않도록 rigid mount 확인

M3 길이는 M3×8 / ×10 / ×12 후보를 실제 출력물에 대보고 확정하는 것이 안전하다.

---

## 27. 현재 RAW 데이터 저장 여부

매우 중요하다.

현재 장기 저장되는 것은 **원시 waveform이 아니라 feature packet**이다.

```text
ADXL raw samples
INMP441 raw PCM
    ↓
RAM에서 분석
    ↓
4개 feature
    ↓
Telemetry packet
    ↓
서버 또는 LittleFS
```

ADXL345의 512개 raw sample과 INMP441의 10,240개 PCM sample 자체는 장기 보존하지 않는다.

따라서 향후 AI가 raw waveform / spectrogram 기반 모델을 사용하려면 별도의 **RAW capture/export 모드**를 추가해야 한다.

---

## 28. 데이터 수집 간격 / 데이터 분량

실제 loop는 대략:

```text
약 0.99초 acquisition
+ 약 3초 measurement interval
≈ 약 4초 / feature packet
```

대략적인 feature record 수:

| 측정 시간 | 대략 record 수 |
|---:|---:|
| 10분 | 약 150 |
| 30분 | 약 450 |
| 1시간 | 약 900 |
| 3시간 | 약 2,700 |
| 24시간 | 약 21,600 |

주의: 이것은 저장 record 수이지 AI 학습에 충분한 데이터 수를 의미하지 않는다.

조건별 학습량은 정상/고장 종류, RPM, 부하, 모터 개체, 설치점, 주변소음, 반복 session을 포함해 별도로 설계해야 한다.

---

## 29. 측정 품질 / 허용 오차

아직 최종 수치로 확정되지 않은 부분:

```text
vibration 오차 ±x%
acoustic 오차 ±x%
반복성 CV
센서 포화 기준
SNR 하한
noise floor 기준
축 방향 허용오차
모터별 baseline 허용범위
```

또한:

```text
vibrationRmsRaw = acceleration [g]
vibrationRmsMmS = null
```

이므로 진동 속도 mm/s calibration은 아직 아니다.

음향도:

```text
acousticRmsRaw = raw PCM
acousticDb = null
```

이라 SPL dB calibration 전이다.

---

## 30. AI / 모델 현재 상태

최종 AI 모델 구조는 아직 확정되지 않았다.

현재 Edge에서 확정된 부분:

```text
ADXL345
 ├─ RMS_X/Y/Z
 ├─ resultant RMS
 └─ strongest-axis FFT peak

INMP441
 ├─ raw PCM RMS
 └─ averaged FFT peak
        ↓
Telemetry 4 features
```

Backend에는 anomaly score 프로토타입, dataset/version, model registry 같은 틀이 존재한다.

프로젝트 계획에는 향후:

```text
AI_FREQ_MODEL_01
기존 데이터셋 기반 주파수 특징 AI 선행학습
```

이 포함돼 있다.

따라서 현재는 **센서 수집 + Edge 특징 추출 + 안정적인 telemetry pipeline까지 확정**된 단계이고, 최종 fault classification/anomaly model은 후속 확정 단계다.

---

## 31. AI 담당과 먼저 합의할 갈림길

### A. 현재 4개 feature 기반 모델

입력 예:

```text
vibrationRmsRaw
vibrationPeakHz
acousticRmsRaw
acousticPeakHz
(+ rpm / operating condition)
```

장점:

- 데이터 작음
- 네트워크/저장 부담 적음
- 빠른 MVP
- 해석 쉬움

단점:

- raw waveform의 고장 signature 손실 가능

### B. RAW waveform 기반 모델

입력 예:

- ADXL 3축 waveform
- audio waveform
- spectrum
- spectrogram

가능 모델:

- 1D CNN
- spectrogram CNN
- TCN / Transformer
- handcrafted frequency bands + ML

이 경우 storage/network/data pipeline을 다시 설계하고 RAW export 모드를 추가해야 한다.

현재 펌웨어는 기본적으로 **A안에 가까운 구조**다.

---

## 32. 테스트 / 검증 상태

최종 follow-up native regression:

```text
55 / 55 PASS
```

검증 항목:

- strict ACK
- malformed/nested/trailing JSON
- duplicate key/type validation
- RFC3339 parser
- canonical payload
- bounded replay
- sequence durable persistence
- transient LittleFS I/O
- CRC corruption
- boot session timestamp
- stale anchor rejection
- anchor CRC
- full-ring spare slot
- power-cut recovery
- watermark ACK-loss replay
- bounded isolation archive
- archive fallback
- HTTPS/default security
- legacy schema migration
- isolation power-cut logic

ESP32-S3 build/upload PASS.

실보드에서 다음을 확인했다.

- 정상 Wi-Fi 전송
- Wi-Fi loss → LittleFS buffer
- reconnect → replay
- replay 중 신규 acquisition
- timeout 후 duplicate-safe retry
- cold boot offline acquisition
- NTP recovery
- multi-reboot stale timestamp 방지
- USB power-cut / ACK-loss recovery
- legacy v1 unresolved migration

---

## 33. 보안 상태

운영 방향:

```text
HTTPS 필수
CA 검증
Bearer token
```

실제 `secrets.h`는 Git에 올리지 않고 `secrets.example.h`만 공유한다.

---

## 34. Git / 협업 상태

ESP32 통합 작업 **PR #13은 최종 Merged** 상태가 확인됐다.

리뷰에서 집중 검증된 부분:

- 데이터 무결성
- strict ACK
- offline persistence
- power-cut
- timestamp integrity
- migration
- TLS
- bounded storage

브랜치는 팀 요청에 따라 삭제하지 않고 유지했다.

---

## 35. 현재 확정 / 미확정

### 확정

- ESP32-S3
- ADXL345
- INMP441
- SPI/I2S 배선
- 640 ms common window
- 800 Hz vibration
- 16 kHz audio
- feature 4종
- Wi-Fi telemetry
- HTTPS 운영 방향
- device/site/asset ID
- LittleFS 24h+ offline ring
- ACK/idempotency
- power-cut recovery
- session/UTC 처리
- bounded replay
- legacy migration

### 최종 확정 필요

- 향후 PSRAM 활성 사용 여부
- vibration mm/s calibration
- acoustic dB calibration
- RPM 입력 방식
- 실제 장착 후 XYZ 최종 축 표준
- 장착 torque / 기구 허용오차
- 측정 품질 acceptance criterion
- 조건별 데이터 수집량
- RAW waveform 저장 여부
- 최종 AI feature/model
- fault label taxonomy
- 실제 production HTTPS endpoint/certificate
- `/api/health` RFC3339 backend 연동 최종 확인

---

## 36. 팀원 전달용 핵심 요약

```text
보드:
ESP32-S3, 8MB Flash.
물리 PSRAM 8MB가 감지된 적은 있으나 현재 PlatformIO는 No PSRAM 설정이고
펌웨어도 PSRAM 전용 할당을 사용하지 않음.

센서:
ADXL345(SPI)
CS=10, MOSI=11, SCK=12, MISO=13

INMP441(I2S)
BCLK=4, WS=5, SD=6, L/R=GND

진동:
800Hz, 512 samples, 640ms.
3축 RMS -> 합성 RMS.
가장 RMS 큰 축 FFT peak.

음향:
16kHz 연속 수집.
10,240 samples = 640ms.
2048 FFT x 5 spectrum 평균.

전송 feature:
vibrationRmsRaw
vibrationPeakHz
acousticRmsRaw
acousticPeakHz

ID:
siteId=SITE-01
assetId=SITE-01-MOT-02
deviceId=DEV-01-MOT-02

통신:
Wi-Fi -> HTTPS REST POST /api/telemetry/ingest
Bearer token.

Wi-Fi 장애:
원시 신호 처리 버퍼는 RAM.
전송 실패 Telemetry는 RAM/PSRAM이 아니라 LittleFS Flash binary ring에 저장.
재부팅 후에도 유지되며 네트워크 복구 시 FIFO replay.

Offline capacity:
physical 25,000 slots
logical 24,999 records
48 bytes/record
약 1.2MB
24시간 이상 목표.

현재 raw waveform은 장기 저장하지 않음.
RAM에서 feature를 만든 뒤 feature packet만 서버/LittleFS에 보존.

설치 기준:
bearing housing top
horizontal X + vertical Z
bolt-fixed bracket
mic 약 1.5m from cooling side.

현재 미확정:
측정 허용오차, mm/s/dB calibration, 조건별 최종 데이터량,
RAW 저장 여부, 최종 AI feature/model 구조.
```

---

## 37. 최종 판단

현재 MotorDiagnosis ESP32 쪽은 단순 센서값 전송 수준을 넘어 다음을 구현·검증한 상태다.

1. 진동/음향 동기 수집
2. Edge FFT/RMS 특징 추출
3. 실제 telemetry 계약
4. Wi-Fi 장애 시 Flash store-and-forward
5. 24시간 이상 offline ring
6. 재부팅/전원차단 데이터 복구
7. strict ACK/idempotency
8. 부정확한 timestamp 생성 방지
9. legacy 데이터 migration
10. TLS 운영 경로
11. 회귀 테스트 + 실보드 power-cut 검증

다음 핵심 단계는 **실제 모터 장착 후 어떤 조건에서 어떤 데이터를 얼마나 수집하고, AI 입력을 feature 기반으로 할지 RAW waveform 기반으로 할지 확정하는 것**이다.

---

## 참고 근거

- MotorDiagnosis ESP32 `main.cpp` / firmware logic 계열
- Bind Edge AI API 명세서 v1.3
- MotorDiagnosis backend `server.py` / data model
- MotorDiagnosis 개발·협업 표준
- ESP32-S3 PlatformIO build/upload 로그
- Wi-Fi loss/replay 로그
- power-cut ACK-loss recovery 로그
- legacy schema v1 migration 실보드 로그
- native firmware regression 55/55 PASS 결과
