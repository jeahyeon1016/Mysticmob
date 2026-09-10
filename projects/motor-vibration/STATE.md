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
