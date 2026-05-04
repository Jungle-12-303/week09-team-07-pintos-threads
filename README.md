# Pintos Threads/User Programs 협업 저장소

이 저장소는 3인 팀이 Pintos `threads`와 `userprog` 프로젝트를 구현하고 실험하기 위한 작업 공간입니다. 루트 디렉터리에는 팀 공통 문서와 개발환경 설정을 두고, 실제 Pintos 소스 코드는 `pintos/` 아래에서 관리합니다.

현재 작업 범위는 9주차 `Project 1: Threads`와 10주차 `Project 2: User Programs`를 함께 포함합니다. 9주차의 `threads` 구현은 이후 프로젝트의 기반이므로 회귀 테스트 대상으로 유지하고, 10주차에는 `userprog`의 argument passing, syscall, process wait/exit, file descriptor 흐름을 중심으로 작업합니다.

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
| `docs/week9-collaboration.md` | 9주차 Threads 협업 원칙과 PR/커밋 기준 |
| `docs/week9-implementation-plan.md` | 9주차 Threads 구현 순서와 테스트 운영 계획 |
| `docs/week10-collaboration.md` | 10주차 User Programs 협업 원칙과 PR/커밋 기준 |
| `docs/week10-implementation-plan.md` | 10주차 User Programs 구현 순서와 테스트 운영 계획 |
| `pintos/README.md` | Pintos 원본 안내 문서 |
| `pintos/threads/` | Project 1 thread/synchronization 관련 코드 |
| `pintos/userprog/` | Project 2 user process, syscall, process lifecycle 관련 코드 |
| `pintos/tests/threads/` | Project 1 thread 단계 테스트 코드 |
| `pintos/tests/userprog/` | Project 2 user program 단계 테스트 코드 |
| `.devcontainer/` | 팀 공통 컨테이너 개발환경 설정 |

## 작업 원칙

- 9주차 작업은 `pintos/threads/`, `pintos/devices/timer.c`, 관련 헤더와 thread 테스트 범위를 중심으로 진행합니다.
- 10주차 작업은 `pintos/userprog/`, `pintos/include/userprog/`, 필요한 thread/process 상태, userprog 테스트 범위를 중심으로 진행합니다.
- `vm`, `filesys` 디렉터리는 현재 프로젝트 요구와 직접 관련이 있을 때만 수정합니다.
- 임시 구현은 커밋과 PR 설명에 명확히 표시하고, 정식 구현으로 대체할 후속 작업을 남깁니다.
- 팀원별 브랜치에서 작업하고, 공통 문서나 설정 변경은 팀 합의 후 반영하는 것을 권장합니다.

## 참고 문서

- [개발환경 구축 가이드](docs/dev-environment.md)
- [9주차 Threads 협업 문서](docs/week9-collaboration.md)
- [9주차 Threads 구현 계획](docs/week9-implementation-plan.md)
- [10주차 User Programs 협업 문서](docs/week10-collaboration.md)
- [10주차 User Programs 구현 계획](docs/week10-implementation-plan.md)
- [Pintos 안내 문서](pintos/README.md)
- [KAIST Pintos Manual](https://casys-kaist.github.io/pintos-kaist/)
