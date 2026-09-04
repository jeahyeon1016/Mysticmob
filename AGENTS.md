# MythicMobs
- 현재 폴더가 작업 루트. origin: https://github.com/jeahyeon1016/rpg.git
- 공통 규칙: https://docs.google.com/document/d/1Bg80xIFTi8ORz1-TVtqHi6dxOt_9tw7AhxbwhE31Obg/edit
- 프로젝트 규칙: https://docs.google.com/document/d/1Kj1HJcptt0HGto7XjzA8CyLoj-Z5G7WzakIcxTyG7eY/edit
- 자료·참고(필요할 때만): https://docs.google.com/document/d/12GnFiAuetxv45QzKSu5pec5CyzUZKoDVmF9FmAU2hm4/edit
- agent.json과 실제 Git으로 연결 확인. Docs의 이전 전 경로 기록은 과거 상태다.
- 규칙은 문맥당 한 번, 대상 문서는 관련 절만 읽는다. 과거 로컬 MD 절차는 적용하지 않는다.
- 설계 후 구현 지시로 진행. 긴 작업만 임시 목록 하나를 주요 단계에서 부분 갱신한다.
- 몹 문서는 마무리 요청 때, 규칙·현재 상태는 각각 명시적 요청 때만 갱신한다.
- 관리하는 MD는 짧은 시작 안내만 유지한다. Docs 읽기용 캐시는 아래 규칙을 따른다. 필요한 기록은 Docs에 간결하게 통합한다.

- 몹 기획 원본(초안): https://docs.google.com/document/d/19nH7ZbtBfQsa5KSMh-6AkD-h86iHA-omdYnnWKMTsWw/edit
- 몹 탭: 에르덴=t.zg6q672r2g15, 바르칸=t.3b5m2cv5j1xc, 실바르=t.g4yk7wqs1gj. get_document_text의 tab_id를 지정하고 요청한 몹 구간만 읽는다.
- 던전 보스는 해당 던전 탭의 몹·전투 패턴만 추가 참조한다. 다른 지역은 탭 제목으로 먼저 위치 확인. 메뉴·경제·직업·퀘스트 등 비관련 기획과 전체 본문은 읽거나 복제하지 않는다. 초안이 현재 사양과 다르면 확인 후 적용한다.

## Docs 읽기용 캐시
- 원본은 Docs. 관련 자료는 먼저 `.agent-work/docs-cache/`에서 찾고, 출처·범위가 맞는 사본은 재사용한다. 매 작업마다 원격 최신 여부를 조회하지 않는다.
- 사본이 없거나 필요한 범위가 빠졌으면 해당 Docs의 필요한 부분만 가져와 `<문서ID>__<탭ID 또는 main>__<범위명>.md`로 저장한다. 전체 문서·전체 드라이브를 미리 받지 않는다.
- 각 사본 머리에 `source_url`, `tab_id`, `scope`, `fetched_at`(시간대 포함 ISO 8601)을 기록한다. 발췌 범위를 명확히 하고 기획 수치·조건을 임의 요약하거나 보완하지 않는다.
- 사용자가 Docs 변경을 알리거나 최신화를 요청하면 대상 사본을 새로 가져온다. AI가 원본을 수정한 경우에도 해당 사본을 갱신한다. 새 조회가 성공하기 전 기존 사본을 덮어쓰지 않는다. 실패하면 최신화 실패와 구버전임을 알린다.
- 사본은 직접 편집하거나 Docs로 역동기화하지 않는다. 오래된 상태 기록은 실제 코드·Git과 대조한다. Docs 원본 변경은 기존의 명시적 요청 원칙을 따른다.
- 캐시는 Git 업로드 대상이 아니다(`/.agent-work/` 제외 규칙 사용). 다른 컴퓨터나 캐시가 없는 프로젝트에서는 필요할 때 다시 가져온다. 별도 자동 동기화·상시 검사·캐시 관리 장부는 만들지 않는다.

- `model/`는 현재 몹 파일, `model-library/`는 재사용 후보 모델·원본 패키지 보관함이다. 보관함과 `.agent-work/`는 기본 탐색에서 제외하고 모델 찾기·가져오기 조사 요청 때만 읽는다. 과거 가져오기 기록의 경로는 당시 값이다.
