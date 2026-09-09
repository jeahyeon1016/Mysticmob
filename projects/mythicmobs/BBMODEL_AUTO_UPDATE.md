# BBModel 최신화

사용자가 `.bbmodel` 수정·모델링·ModelEngine 배치를 명시하면 아래 순서로 처리한다.

1. 최신 `.bbmodel`을 다음 폴더에 배치한다.

   ```text
   D:\ai agent\projects\mythicmobs\bukkit\plugins\ModelEngine\blueprints\
   ```

2. 서버 콘솔에서 실행한다.

   ```text
   meg reload
   ```

3. 다음 파일이 새로 생성·갱신됐는지 확인한다.

   ```text
   D:\ai agent\projects\mythicmobs\bukkit\plugins\ModelEngine\resource pack.zip
   ```

4. 최신 ZIP을 PaperResources가 읽는 GitHub 저장소에 커밋·푸시한다. 현재는 `jeahyeon1016/ai-agent` 저장소의 `master` 브랜치를 사용하며, 저장소 내부 실제 경로가 `projects/mythicmobs/bukkit/...`이므로 ZIP URL에도 이 경로를 포함한다. 저장소·브랜치·경로는 항상 `bukkit/plugins/PaperResources/resources.txt`의 실제 URL과 일치시킨다.

   ```text
   D:\ai agent\projects\mythicmobs\bukkit\plugins\ModelEngine\resource pack.zip
   ```

   현재 URL 예시:

   ```text
   https://github.com/jeahyeon1016/ai-agent/raw/refs/heads/master/projects/mythicmobs/bukkit/plugins/ModelEngine/resource%20pack.zip
   ```

   URL이 실제 GitHub Raw ZIP을 가리키고 `404`가 아닌지 확인한다. Paper 서버 `server.properties`의 `resource-pack`이 비어 있어도 PaperResources가 URL을 관리할 수 있다. 별도로 `resource-pack-sha1`을 사용하는 배포라면 로컬 ZIP SHA-1과 원격 ZIP SHA-1을 일치시킨다.

   ```properties
   resource-pack=<배포 환경에서 관리하는 실제 resource-pack URL>
   resource-pack-id=<유효한 UUID>
   resource-pack-sha1=<로컬 ZIP의 SHA-1>
   ```

5. 서버 콘솔에서 PaperResources만 리로드한다.

   ```text
   paperresources reload
   ```

   명령이 인식되지 않으면 추측해서 진행하지 말고, 먼저 `plugins/PaperResources-*.jar`의 `plugin.yml`, `/paperresources help`, 서버 로그를 확인해 실제 서브명령을 찾는다. RCON/콘솔 접근 방법도 먼저 확인한다. 전체 서버 재시작이나 `/reload`는 기본 절차로 사용하지 않는다.
6. 접속 후 리소스팩 다운로드와 모델 표시를 확인한다.

## 조건

- 단순 질문·검토에는 실행하지 않는다.
- GitHub 푸시 실패 시 기존 서버 설정을 유지한다.
- 원격 GitHub 다운로드 파일의 SHA-1과 로컬 ZIP의 SHA-1이 일치해야 완료다.
- 명령·플러그인·배포 URL을 못 찾으면 관련 설정, 플러그인 메타데이터, 콘솔 도움말, 최신 로그를 먼저 검색하고 확인된 방법만 실행한다.
