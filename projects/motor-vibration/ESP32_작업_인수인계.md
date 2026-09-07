# 모터 진동 프로젝트 — ESP32 작업 인수인계

## 현재 상태

- 담당 범위: 하드웨어 및 ESP32 펌웨어
- 4주차 MVP: 완료
- 소스 저장소: `D:\ai agent`
- 최신 원격 저장소: https://github.com/jeahyeon1016/ai-agent
- 작업 브랜치: `feat/esp32-telemetry-integration`
- 기존 펌웨어 PR: https://github.com/ckdudwns/MotorDiagnosis/pull/34

## 완료된 작업

- 운영 HTTPS endpoint 및 device/site/asset ID 적용
- 장치 상태 보고용 Bearer 인증 경로 추가
- CA 인증서 검증 활성화
- LittleFS 이전 세션 격리 처리 지연 개선
- API v1.3 `lifecycleUpdates` 중첩 ACK 메타데이터 허용
- 필수 ACK 필드 타입 검증 유지 및 회귀 테스트 추가
- ESP32 build/upload 및 실서버 전송 검증

## 검증 결과

- Native firmware tests: `56/56 PASS`
- Live HTTPS telemetry: `HTTP 201`, `accepted=true`
- Replay duplicate: `HTTP 200`, `duplicate=true`
- 장치: `DEV-01-MOT-02`

## 다음 작업

- PR #34 리뷰 및 머지
- 모터 사양과 실제 설치 환경 확정
- 센서 부착 위치 및 측정 목표 확정
- 확정 후 실장비 검증 계획 수립

## 주의사항

- 실제 토큰과 `secrets.h`는 커밋하지 않음.
- Backend·Frontend·AI 기능은 담당 범위에서 제외.
- 저장소의 unrelated 변경사항은 별도로 보존.

## 이슈 작성 양식

이슈 작성 시 담당 파트, 담당자, 우선순위, 주차·기능 ID, 현재/변경 상태, 작업 범위, 제외 범위, API 연동, 완료 기준, 검증 방법, 참고 자료를 반드시 기록한다.
