# 관리 기준

규칙·기획·상태·참고 문서는 로컬 Markdown을 원본으로 삼고 Git으로 관리한다. Google Drive를 작업 전제로 사용하지 않는다.

파일을 수정하면 작업 범위에 해당하는 파일만 즉시 `ai-agent`에 commit·push해 최신화한다. 강제 push, 기존 변경 폐기, 무관한 파일 추가는 하지 않는다.

모터 진동 프로젝트의 실제 개발 저장소(`MotorDiagnosis`) commit·push·PR 생성은 사용자가 명시적으로 요청한 경우에만 수행한다. 모터 진동 변경사항의 `ai-agent` 백업 최신화는 일반 프로젝트와 동일하게 수행한다.

실제 토큰·비밀번호·개인 credential·`secrets.h` 등 민감정보는 어떤 경우에도 push하지 않는다.
