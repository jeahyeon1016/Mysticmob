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
- [x] 엔트 가디언 뿌리 지속 소환 수정: 사망 전까지 누적 유지, 체력 구간 패턴은 `arm_inground` 사용, 기존 콤보 10 패턴과 `pull_jaws` 제거
- [x] 엔트 가디언 뿌리 피해를 `~onTimer:12`로 상시 반복하고 부모 연결(`summonerisparent`) 보강
- [x] 엔트 가디언 소환 패턴 범위 연출 반영: 지상 범위 파티클과 종료 시 반경 피해·효과음
- [x] 엔트 가디언 소환 패턴을 지진 3연타로 변경: 반경 4→8→10칸 순차 피해·잔해 연출, 각 단계 회피 간격 추가
- [x] 뿌리 자체의 부모 생존 감지 정리(`hasparent` + `isparentalive`) 추가

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
| 2026-09-10 | 엔트 가디언 뿌리 누적 및 패턴 정리 | 완료 / `mm reload` 확인 | `bukkit/plugins/MythicMobs/skills/엔트 가디언_스킬.yml` |
| 2026-09-13 | 엔트 가디언 뿌리 수명·피해 루프 및 소환 경고 범위 보강 | 완료 / `mm reload` 확인 | `bukkit/plugins/MythicMobs/skills/엔트 가디언_스킬.yml`, `bukkit/plugins/MythicMobs/mobs/엔트 가디언_몹.yml` |
| 2026-09-13 | 엔트 가디언 지진 3연타 및 뿌리 자가 정리 보강 | 완료 / `mm reload` 확인 | `bukkit/plugins/MythicMobs/skills/엔트 가디언_스킬.yml`, `bukkit/plugins/MythicMobs/mobs/엔트 가디언_몹.yml` |

## 다음 업데이트 규칙

작업이 끝날 때마다 이 파일의 `구현 완료`, `현재 판단`, `진행 기록`을 함께 최신화한다. 완료로 표시할 때는 실제 파일 경로 또는 검증 결과를 남긴다.

기획서를 요청받으면 로컬 Markdown 요약본이 아니라 원본 Google Docs 링크를 제공한다.

원본 기획서: https://docs.google.com/document/d/19nH7ZbtBfQsa5KSMh-6AkD-h86iHA-omdYnnWKMTsWw/edit
