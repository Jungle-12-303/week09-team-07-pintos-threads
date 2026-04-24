# Pintos Threads 협업 저장소

이 저장소는 3인 팀이 Pintos `threads` 프로젝트를 구현하고 실험하기 위한 작업 공간입니다. 루트 디렉터리에는 팀 공통 문서와 개발환경 설정을 두고, 실제 Pintos 소스 코드는 `pintos/` 아래에서 관리합니다.

교육 일정이 바뀔 수 있으므로 이 문서에서는 특정 주차 번호를 고정해서 쓰지 않습니다. 현재 단계의 중심 작업은 `threads`이며, 다른 단계 디렉터리는 필요할 때만 참고하는 것을 전제로 합니다.

## 빠른 시작

1. Docker Desktop을 설치합니다.
2. VSCode에서 이 저장소를 엽니다.
3. `Dev Containers: Reopen in Container`로 컨테이너 환경에 들어갑니다.
4. 필요하면 컨테이너 셸에서 `source /workspaces/week09-team-07-pintos-threads/pintos/activate`를 실행합니다.

자세한 환경 구성 방법은 [개발환경 구축 가이드](docs/dev-environment.md)를 참고하세요.

## 주요 경로

| 경로 | 설명 |
|------|------|
| `README.md` | 저장소 개요와 협업 진입 문서 |
| `docs/dev-environment.md` | Docker, VSCode DevContainer 기반 개발환경 안내 |
| `pintos/README.md` | Pintos 원본 안내 문서 |
| `pintos/threads/` | 현재 구현의 중심이 되는 thread 관련 코드 |
| `pintos/tests/threads/` | thread 단계 테스트 코드 |
| `.devcontainer/` | 팀 공통 컨테이너 개발환경 설정 |

## 작업 원칙

- 구현은 우선 `pintos/threads/`와 관련 헤더, 테스트 범위 안에서 진행합니다.
- `userprog`, `vm`, `filesys` 디렉터리는 현재 작업과 직접 관련이 있을 때만 수정합니다.
- 팀원별 브랜치에서 작업하고, 공통 문서나 설정 변경은 팀 합의 후 반영하는 것을 권장합니다.

## 참고 문서

- [개발환경 구축 가이드](docs/dev-environment.md)
- [Pintos 안내 문서](pintos/README.md)
- [KAIST Pintos Manual](https://casys-kaist.github.io/pintos-kaist/)
