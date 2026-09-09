# MythicMobs 현재 상태

## 기본 정보

- Git 루트: `D:\ai agent\projects\mythicmobs`
- 브랜치: `master`
- 최신 기준 로컬 경로: `D:\ai agent\projects\mythicmobs`
- 통합 백업 저장소: https://github.com/jeahyeon1016/ai-agent
- 통합 백업 저장소: https://github.com/jeahyeon1016/ai-agent.git
- 현재 공개 리소스팩 배포 저장소: https://github.com/jeahyeon1016/Mysticmob.git

## 구현 완료

- [x] 실바르 엔트 미니언 모델 및 리소스 반영
- [x] 실바르 엔트 워리어 모델 및 리소스 반영
- [x] 엔트 미니언/워리어 ModelEngine 블루프린트 확인
- [x] 엔트 미니언/워리어 리소스팩 모델·텍스처 확인
- [x] 엔트 미니언 검증 코드 확인: `validation/ValidateEntMinion.java`
- [x] 엔트 소서러 Attack2 미사일 연출 반영: `bukkit/plugins/MythicMobs/skills/엔트 소서러_스킬.yml`
- [x] 엔트 가디언 리소스팩·스킬 반영 및 PaperResources 리로드 확인
- [x] 엔트 가디언 필드보스 패턴 반영: sweep·arm_inground·smash_ground·체력 구간 뿌리 소환
- [x] 엔트 가디언 뿌리 소환을 `BONE_BLOCK` 위 5개씩, 체력 구간 4회 총 20개로 제한하고 사망 시 자식 정리

## 현재 판단

엔트 미니언과 워리어는 이미 제작·반영된 상태다. 엔트 소서러 Attack2 미사일 연출은 반영되었고 서버 리로드까지 확인했다.

## 진행 기록

| 날짜 | 작업 | 상태 | 확인 위치 |
|---|---|---|---|
| 2026-09-07 | 엔트 미니언·워리어 구현 상태 확인 | 완료 | `model/`, `bukkit/plugins/ModelEngine/`, `bukkit/plugins/MythicMobs/` |
| 2026-09-07 | 엔트 소서러 `.bbmodel`을 ModelEngine 블루프린트에 적용 | 적용 완료 / 리로드 대기 | `bukkit/plugins/ModelEngine/blueprints/ent_sorcerer.bbmodel` |
| 2026-09-08 | 엔트 소서러 Attack2 3발 미사일 연출 조정 | 완료 / 서버 리로드 확인 | `bukkit/plugins/MythicMobs/skills/엔트 소서러_스킬.yml` |
| 2026-09-09 | 엔트 가디언 리소스팩을 ai-agent로 통일 | 완료 / PaperResources 로드 확인 | `bukkit/plugins/ModelEngine/resource pack.zip`, `bukkit/plugins/PaperResources/resources.txt` |
| 2026-09-10 | 엔트 가디언 필드보스 패턴 및 반복 뿌리 모델 반영 | 완료 / `meg reload`, `mm reload` 확인 | `bukkit/plugins/MythicMobs/`, `bukkit/plugins/ModelEngine/` |

## 다음 업데이트 규칙

작업이 끝날 때마다 이 파일의 `구현 완료`, `현재 판단`, `진행 기록`을 함께 최신화한다. 완료로 표시할 때는 실제 파일 경로 또는 검증 결과를 남긴다.

기획서를 요청받으면 로컬 Markdown 요약본이 아니라 원본 Google Docs 링크를 제공한다.

원본 기획서: https://docs.google.com/document/d/19nH7ZbtBfQsa5KSMh-6AkD-h86iHA-omdYnnWKMTsWw/edit
