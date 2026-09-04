# FallenDefender 구현 참고

사용자 제공 팩의 구현 사례. 프로젝트 규칙이나 검증된 정답이 아니다. Worker는 명세에 연결된 절만 읽고, 실제 적용 시 현재 모델·설치 버전의 문법을 확인한다.

## 출처와 읽는 범위

- 원본: `C:/Users/user1/Downloads/FallenDefender_ONLY_V1.2.zip` (압축 해제·설치하지 않음).
- ZIP 루트: `FallenDefender_ONLY_V1/`. `packinfo.yml`은 Version `1.0.0`, Author `Unknown`; 파일명의 V1.2와 구분한다. 대상 플러그인 버전은 미확인.
- 아래 `M/`, `S/`는 ZIP 안 `plugins/MythicMobs/Packs/em_fallendefender/Mobs/`, `Skills/`의 약칭이다. 행 번호는 ZIP 원본 기준이며 스킬 ID를 우선 검색한다.
- 주요 YAML의 대표 흐름과 `fallendefender.bbmodel`의 관련 애니메이션 메타데이터를 확인했다. 전체 패턴·PDF 설치 문서·인게임 동작을 검증한 문서는 아니다.
- 원본이 필요하면 ZIP의 지정 엔트리만 읽는다. 텍스처 포함 bbmodel 전체나 수천 줄 VFX를 컨텍스트에 덤프하지 않는다. Downloads 원본이 없으면 접근 가능한 사본을 요청하고 내용을 추정하지 않는다.

## 구조·상태 관리

| 구현 방식 | 원본 근거 | 활용 조건 |
|---|---|---|
| 기본 능력 → 성별/모델 → 활성/석상 변형을 Template으로 분리 | `M/FallenDefender.yml`: `defenderfallen_base`, `defenderfallen_male_base`, `defenderfallen_activated`, `defenderfallen_activable` | 실제 변형이 여럿일 때만 공통 설정 재사용. 단일 몹에 상속 계층을 추가할 이유는 아님 |
| `stance=normal/casting`과 caster 변수로 진입 제한 | `S/FallenDefenderSkills.yml:662`, `fdefender_ravageprotocol` | 행동 잠금과 running/shielding/hurtened 등 상태를 구분. 우리 프로젝트의 기존 상태 변수와 중복 구현하지 않음 |
| Aura로 주기 검사·재진입 방지·종료 처리 | `fdefender_runprotocol_start:644`, `fdefender_shieldstance_trigger:2573` | 필요한 행동 동안 검사 활성화. 예: 달리기 거리 검사 15틱, 방패 Aura onEnd 복구 |
| 거리·전투 여부·누적치로 행동 선택 | `fdefender_runprotocolcheck:625`, `fdefender_runproximitycheck:828` | 달리기 누적치를 거리별 확률로 증가시키고 임계값에서 진입. 근접/전투 종료/누적치 소진에 별도 종료 경로 |

## 애니메이션·이동 분리

- 기본 이동은 `DefaultState`로 WALK/IDLE을 run, walk_block, idle_blocking 등으로 교체한다. 근거: `M/FallenDefender.yml:334`, `S/FallenDefenderSkills.yml:644,2573`.
- 특수 행동은 `state`와 `li/lo`를 사용하고 이전 공격 상태를 명시적으로 제거한다. 긴 제거 목록 자체를 모방하지 말고, 현재 작업에서 충돌 가능한 상태만 관리한다.
- 돌진 사례 `fdefender_ravageprotocol_execute:692`: 진입 잠금 → 이전 상태 제거 → 몸/다리 준비 모션 + SLOW → `delay 12` → 거리 검사 Aura와 `run_aggressive` 시작·속도 2.3 설정 → 후속/종료 검사.
- 모델 경로: `plugins/ModelEngine/blueprints/fallen_defenders/defender/fallendefender.bbmodel`. `idle_runaggressive_legs` once 0.6초, `idle_runaggressive_body` once 0.75초, `run_aggressive` loop 0.7초. 준비와 지속 이동을 분리한 실제 사례다. 채널별 겹침·접합부는 별도 확인 필요.
- 종료 `fdefender_ravagerun_stop:804`: clearpath → 관련 Aura 제거 → running/runstacks 초기화 → 속도 복원 → 달리기 state 제거·종료 모션 → 대기 후 stance 복구.
- 원본은 SLOW, clearpath, spin, 제한된 look 반복 등을 사용한다. 이것이 AI OFF보다 우월하거나 오크킹 끊김을 해결한다는 증거는 아니다.

## 타격 타임라인·추적 종료

`attack_roll_1fdefender:1798`의 주요 흐름 (애니메이션 시작을 t=0으로 표기):

| 시점 | 동작 |
|---|---|
| t=0 | casting stance, 이전 상태 제거, `roll_attack_1` + cloak 모션, SLOW 32틱, look repeat=5/repeatInterval=1 |
| t=8 | casting 변수 설정, spin(v=0,d=14), FX를 delay=3으로 예약 |
| t=9 | 전방 원뿔 140도·반경 4.5에 상태 효과 및 basedamage 1.5배 |
| t=22 | casting 해제, clearpath, normal 복원, 후속 공격 호출 |

- 준비 초반에만 look 반복을 지정하고 이후 spin을 사용한다. 타격까지 무제한으로 타깃을 쫓는 구현과 구분해 참조한다.
- 피해·효과에 `hurtened != 1` 조건을 재검사한다. 예약된 후속 동작이 있다는 이유만으로 타격을 무조건 실행하지 않는 패턴이다.
- FX 예약 시점과 피해 시점이 같지는 않다. 연출 선후관계는 원본 모델과 함께 해석하며 숫자를 복사하지 않는다.
- 새 구현은 취소·사망·타깃 상실 시 예약 작업과 잠금이 남지 않는지 확인한다. 원본의 모든 지연 경로가 안전하다고 보장하지 않는다.

## 모델 초기화·방패·VFX

- `M/FallenDefender.yml:154`의 모델 연결은 `initrender=false` 후 bindhitbox/brightness/submodel 설정을 사용한다. 활성 변형은 `renderinit`을 별도 호출한다. 초기 표시 준비를 나누는 사례이며, 상속·지연의 실제 실행 순서는 적용 전에 확인한다.
- 방패는 `ob_shieldenn` 본에 `defender_shieldennbox`를 bindhitbox하고 hitboxconfig를 적용한다. 방패 상태에서 `@Children`을 변수 조건으로 선별한다 (`fdefender_shieldstance_trigger`). 별도 판정 개체가 필요한 경우에만 참고한다.
- `M/FDefender_VFX.yml:1`: ITEM_DISPLAY 기반 공통 VFX 템플릿, NoGravity·Collidable false, spawn 후 200틱 제거. 파생 효과에 수명 상한을 두는 사례다.
- `defender_expFXmarker1:36`: 모델을 붙인 뒤 `changepart`로 리소스 모델의 frame2, frame3…를 틱별 교체한다. 본 애니메이션 외에 파츠 교체로 프레임 효과를 구현한다.
- `S/v2fallendefenderfx.yml`, `fdefenderatk1FX:1`: forwardOffset/sideOffset/y와 delay를 지정한 다수 파티클 점으로 궤적을 구성한다. 기존 효과로 충분하면 대량 좌표·행을 복제하지 않는다.
- `@ModelPart` 로컬 위치에 파티클을 붙이고, 사운드는 리소스팩 namespace를 참조한다. 적용 시 본 ID·효과 모델·sounds.json 및 실제 음원 의존성을 확인한다.

## 그대로 가져오면 안 되는 점

- `M/FallenDefender.yml:48`은 DefaultState의 닫는 `}`가 누락된 형태다. 같은 파일의 기본 몹에 AIGoalSelectors 키도 반복된다 (266/276행).
- `S/FallenDefenderSkills.yml`의 `lunge_fdefender_zigzag`는 1100/1231행에 중복 정의된다. 실제 로더 처리 확인 없이 복사 금지.
- `attackc1_loop`는 이름과 달리 bbmodel에서 once 5.5초다. 이름만 보고 loop 모드·재생 길이를 추정하지 않는다.
- 확인한 모델 연결에는 `usm`이 명시되지 않았다. 오크킹의 `usm=true`와 같은 핸들러라고 가정하지 않는다. `sync=true`도 클라이언트 애니메이션 동기화 보장으로 해석하지 않는다.
- 긴 상태 제거 목록·상시 장기 Aura·대량 파티클은 이 팩의 구현 선택이다. 프로젝트 표준이나 성능 최적화로 승격하지 않는다.
- 오크킹의 ‘가까운 다른 모델이 있으면 개선’ 현상을 설명하거나 해결한 자료가 아니다. 현재 읽기 전용 진단을 변경 허가로 해석하지 않는다.

## Worker 적용 방법

Main은 대상 명세에 `참고: tasks/FallenDefender_구현참고.md — <관련 절>, 적용 목적 <한 줄>`만 추가한다. Worker는 해당 절 → 필요한 원본 스킬 → 현재 프로젝트의 대응 코드 순으로 확인한다. 일반적인 구현은 이 요약만으로 시작하고 원본 전체를 읽지 않는다. 참고 구조를 적용하더라도 요청 범위·기존 ID·모델 원본 소유권을 유지한다.
