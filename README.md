# Hide Studio Mode Transition Panel

OBS 스튜디오 모드에서 미리보기와 프로그램 사이에 있는 전환 패널(화면 전환 버튼, 빠른 화면 전환, T바)을 숨겨서
미리보기와 프로그램 화면이 그만큼 넓어지게 하는 플러그인입니다.

- OBS 32.x (Windows)에서 쓰도록 만들었습니다.
- 메뉴 **도구 → "스튜디오 모드 가운데 전환 패널 숨기기"**로 켜고 끌 수 있습니다. 설정은 저장됩니다.
- 패널을 숨겨도 단축키로 하는 화면 전환은 그대로 작동합니다.

## 설치

1. GitHub Actions에서 받은 zip 파일의 압축을 풉니다.
2. 안에 있는 `hide-studio-transition-panel` 폴더를 `C:\ProgramData\obs-studio\plugins\` 안에 복사합니다.
   (`plugins` 폴더가 없으면 새로 만듭니다.)
3. OBS를 다시 시작합니다.

## 제거

`C:\ProgramData\obs-studio\plugins\hide-studio-transition-panel` 폴더를 지우고 OBS를 다시 시작합니다.
