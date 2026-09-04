# MythicMobs
- 현재 폴더가 작업 루트. origin: https://github.com/jeahyeon1016/Mysticmob.git
- 공통 규칙: https://docs.google.com/document/d/1Bg80xIFTi8ORz1-TVtqHi6dxOt_9tw7AhxbwhE31Obg/edit
- 프로젝트 규칙: https://docs.google.com/document/d/1Kj1HJcptt0HGto7XjzA8CyLoj-Z5G7WzakIcxTyG7eY/edit
- 자료·참고(필요할 때만): https://docs.google.com/document/d/12GnFiAuetxv45QzKSu5pec5CyzUZKoDVmF9FmAU2hm4/edit
- agent.json과 실제 Git으로 연결 확인. Docs의 이전 전 경로 기록은 과거 상태다.
- 규칙은 문맥당 한 번, 대상 문서는 관련 절만 읽는다. 과거 로컬 MD 절차는 적용하지 않는다.
- 설계 후 구현 지시로 진행. 긴 작업만 임시 목록 하나를 주요 단계에서 부분 갱신한다.
- 몹 문서는 마무리 요청 때, 규칙·현재 상태는 각각 명시적 요청 때만 갱신한다.
- 관리하는 MD는 짧은 시작 안내만 유지한다. 필요한 기록은 Docs에 간결하게 통합한다.

- 몹 기획 원본(초안): https://docs.google.com/document/d/19nH7ZbtBfQsa5KSMh-6AkD-h86iHA-omdYnnWKMTsWw/edit
- 몹 탭: 에르덴=t.zg6q672r2g15, 바르칸=t.3b5m2cv5j1xc, 실바르=t.g4yk7wqs1gj. get_document_text의 tab_id를 지정하고 요청한 몹 구간만 읽는다.
- 던전 보스는 해당 던전 탭의 몹·전투 패턴만 추가 참조한다. 다른 지역은 탭 제목으로 먼저 위치 확인. 메뉴·경제·직업·퀘스트 등 비관련 기획과 전체 본문은 읽거나 복제하지 않는다. 초안이 현재 사양과 다르면 확인 후 적용한다.

- `model/`는 현재 몹 파일, `library/`는 재사용 자료 보관함이다. `library/몹`은 몹 관련 자료, `library/스킬`은 스킬 관련 자료다. 보관함은 기본 탐색에서 제외하고 모델·스킬 찾기/가져오기 조사 요청 때만 읽는다. 과거 가져오기 기록의 경로는 당시 값이다.

- “최신화 해놔”는 현재 작업 관련 파일의 Git 커밋·일반 push와 관련 Docs 갱신·재조회 검증까지 요청한 뜻이다. 추가 확인 없이 수행하며 제외 대상·실패 처리는 공통 규칙을 따른다.
