# ai-agent 환경 설정 예시

새 컴퓨터에서 참고하는 공개용 설정 안내다. 실제 토큰·비밀번호는 넣지 않는다.

## 시작 순서

1. `start.md`
2. `AGENTS.md`
3. `rules/AGENT_RULES.md`
4. 작업 프로젝트의 `RULES.md`, `README.md`, `STATE.md`

## 저장소

```powershell
git clone https://github.com/jeahyeon1016/ai-agent.git
cd ai-agent
git pull --ff-only origin master
```

## 개인 설정 예시

```text
프로젝트/
├─ .env.example       # 공유 가능
├─ .env               # 실제 값, 공유 금지
├─ secrets.example.h  # 공유 가능
└─ secrets.h          # 실제 값, 공유 금지
```

## 확인

```powershell
git status --short
git check-ignore -v <private-file>
```

개인 설정 파일이 없으면 해당 프로젝트의 예시 파일을 복사해 만든다.
