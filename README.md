## PintOS Project 2. User Program - Team02

<details>

<summary>Open toggle to read contents</summary>
<div markdown="1">
  
****SW Jungle Week09,10.5 (26 May ~ 7 Jun, 2022)****
  
## Contributor
- 정글 4기 강찬익, 이승원, 한승희 
  
</div>
</details>  
  
---
## PintOS Project 1. Thread - Team02

<details>
<summary>Open toggle to read contents</summary>
<div markdown="1">
  
# PintOS Project 1. Thread - Team02
****SW Jungle Week08 (19 ~ 26 May, 2022)****
## Contributor
- 정글 4기 강찬익, 이승원, 한승희 
---
## TIL (Today I Learned) 

### `Thu. 19 May` - Week08 Start

### 20일 진행될 OS 강의 준비
- Georgia tech 강의

### 프로젝트에 대한 이해
- PintOS project Git Book 읽기
- 작업환경 세팅 (EC2, Ubuntu18.04)

### `Fri. 20`

### OS 강의 part.1 (10:50 ~ 13:05)

- KAIST 권영진 교수님

### Alarm Clock 학습

- Busy Waiting의 문제점
    - Thread가 CPU를 점유하면서 대기하고 있는 상태
    - CPU 자원이 낭비되고, 소모 전력이 불필요하게 낭비될 수 있음
- sleep / wake up로 문제점 개선
  
### `Sat. 21`

### alarm clock 구현

- PintOS thread.*
    - gitbook을 기반으로 thread.h, thread.c에 대한 이해
- 원유집 교수님 강의자료 바탕으로 alarm clock checklist 정리


### OS 강의

- KOCW 이화여대 반효경 교수님 - 운영체제

### `Sun. 22`

### alarm clock 완성

- 팀내 QnA, 피드백 진행
- alarm clock test result

### priority scheduling 학습

### `Mon. 23`

### Priority Scheduling (1) priority scheduling 구현

- Round Robin 방식을 우선순위를 고려하는 스케줄링 방법으로 개선
    - 우선순위를 비교하여 우선순위가 가장 높은 thread를 ready list의 맨 앞에 위치시킴
- 우선순위에 대한 이해
- 선점, 비선점 방법에 대한 이해
- 우선순위 알고리즘 문제점 - Starvation
    - 우선순위가 낮은 프로세스는 우선순위가 높은 프로세스가 있는 한 절대 실행이 안되는 상황
    - aging을 통해 해결 가능
        - 우선순위가 높은 프로세스가 실행될 때마다 우선순위를 높여줌
- 16 of 27 tests failed.

### `Tue. 24`

### Priority Scheduling (2) semaphore, lock, condition variable

- semaphore, lock, condition variable을 사용하여 priority scheduling 개선
    - PintOS는 FIFO 방식을 사용, sychronization 도구들을 기다릴 때
        
        우선순위가 가장 높은 thread가 CPU를 점유하도록 구현
        

### `Wed. 25`

### Priority Scheduling (3) Priority donation (priority inversion problem)

- Priority donation
    - Multiple donation
        - thread가 두 개 이상의 lock 보유시 각 lock을 획득하고자 하는 thread들에게 donation이 발생하여 여러번의 donation이 일어난 상황
    - Nested donation
        - 여러번의 단계적 donation이 일어나는 상황
    - 7 of 27 tests failed (20 passed)

### `Thu. 26`  - End of the week

[Wrap-up] Project 1 Thread (10:00~11:00)

---
<details>
<summary>Original PintOS README</summary>
<div markdown="1">
  
Brand new pintos for Operating Systems and Lab (CS330), KAIST, by Youngjin Kwon.
The manual is available at https://casys-kaist.github.io/pintos-kaist/.
  
</div>
</details>
  
</div>
</details>

*This page was most recently updated on May 28th, 2022*
