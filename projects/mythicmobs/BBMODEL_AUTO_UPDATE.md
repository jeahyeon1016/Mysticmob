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

4. 최신 ZIP을 GitHub 저장소에 커밋·푸시한다. 저장소는 `jeahyeon1016/Mysticmob`, 브랜치는 `main`으로 한다.

   ```text
   D:\ai agent\projects\mythicmobs\bukkit\plugins\ModelEngine\resource pack.zip
   ```

   서버 설정에는 다음 GitHub 다운로드 URL과 로컬 ZIP의 SHA-1을 반영한다.

   ```properties
   resource-pack=https://github.com/jeahyeon1016/Mysticmob/raw/refs/heads/main/bukkit/plugins/ModelEngine/resource%20pack.zip
   resource-pack-id=<유효한 UUID>
   resource-pack-sha1=<로컬 ZIP의 SHA-1>
   ```

5. 서버를 재시작한다. `/reload`만으로 완료 처리하지 않는다.
6. 접속 후 리소스팩 다운로드와 모델 표시를 확인한다.

## 조건

- 단순 질문·검토에는 실행하지 않는다.
- GitHub 푸시 실패 시 기존 서버 설정을 유지한다.
- 원격 GitHub 다운로드 파일의 SHA-1과 로컬 ZIP의 SHA-1이 일치해야 완료다.
