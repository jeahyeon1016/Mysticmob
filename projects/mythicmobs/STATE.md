# MythicMobs 현재 상태

## 기본 정보

- Git 루트: `D:\ai agent\projects\mythicmobs`
- 브랜치: `main`
- GitHub: https://github.com/jeahyeon1016/Mysticmob.git

## 구현 완료

- [x] 실바르 엔트 미니언 모델 및 리소스 반영
- [x] 실바르 엔트 워리어 모델 및 리소스 반영
- [x] 엔트 미니언/워리어 ModelEngine 블루프린트 확인
- [x] 엔트 미니언/워리어 리소스팩 모델·텍스처 확인
- [x] 엔트 미니언 검증 코드 확인: `validation/ValidateEntMinion.java`

## 현재 판단

엔트 미니언과 워리어는 이미 제작·반영된 상태다. 다음 작업은 신규 제작이 아니라 설정 검증, 서버 테스트, 또는 다음 기획 항목 선정이다.

## 진행 기록

| 날짜 | 작업 | 상태 | 확인 위치 |
|---|---|---|---|
| 2026-09-07 | 엔트 미니언·워리어 구현 상태 확인 | 완료 | `model/`, `bukkit/plugins/ModelEngine/`, `bukkit/plugins/MythicMobs/` |
| 2026-09-07 | 엔트 소서러 `.bbmodel`을 ModelEngine 블루프린트에 적용 | 적용 완료 / 리로드 대기 | `bukkit/plugins/ModelEngine/blueprints/ent_sorcerer.bbmodel` |

## 다음 업데이트 규칙

작업이 끝날 때마다 이 파일의 `구현 완료`, `현재 판단`, `진행 기록`을 함께 최신화한다. 완료로 표시할 때는 실제 파일 경로 또는 검증 결과를 남긴다.

기획서를 요청받으면 로컬 Markdown 요약본이 아니라 원본 Google Docs 링크를 제공한다.

원본 기획서: https://docs.google.com/document/d/19nH7ZbtBfQsa5KSMh-6AkD-h86iHA-omdYnnWKMTsWw/edit
