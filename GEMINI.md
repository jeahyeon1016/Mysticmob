# 시작
이 폴더의 AGENTS.md를 읽고 연결된 규칙을 따른다. 이미 읽은 문서는 반복 조회하지 않는다.

## Google Docs 연결
- Docs 읽기·수정은 `D:\ai agent\agent-system\gdocs.cmd`로 직접 실행한다. 실행 옵션은 `--help`. 일반 Docs API를 사용하므로 Preview 등록이 필요한 docs MCP는 이 연결의 전제 조건이 아니다.
- 최초 인증은 `gdocs.cmd auth`의 AUTH_URL을 사용자가 브라우저에서 승인한다. 코드를 채팅에 요구하거나 다른 앱 인증 파일을 탐색하지 않는다. 이후 읽기·수정 시 토큰은 자동 갱신된다.
- `read-text <문서ID> --tab-id <탭ID>`로 필요한 탭만 읽는다. `update-text <문서ID> <UTF-8파일>`은 본문 전체 교체이므로 명시적으로 전체 교체를 요청받은 경우에만 사용하고, 실행 후 다시 읽어 확인한다. 기존 문서 작성·갱신 시점 규칙을 그대로 따른다.