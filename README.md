# 와인드업 크루 (CRANK) — MVP M1~M5 + 구역 F "장난감 방"

UE 5.8 C++ 2인 협동 래그돌 레이드. 설계: `windup_crew_mvp_design.md`.

## 실행
- 에디터: `Crank/Crank.uproject` → 시작 맵 `Lvl_Kitchen`. 혼자 PIE를 돌리면 2.5초 뒤 AI 더미 인형이 자동 합류합니다(`?nodummy`로 끄기).
- 게임 시작 맵: `Lvl_MainMenu` — 방 만들기(리슨 서버) / IP로 참가 / 솔로 테스트(AI 더미) / 종료.
  메뉴에 이 PC의 주소가 표시되고, 마지막으로 입력한 참가 주소를 기억합니다. 접속에 실패하면 메뉴에 이유가 표시됩니다.
- 방을 만든 호스트는 친구가 들어올 때까지 시계가 멈춘 채 "친구를 기다리는 중" 화면(내 주소·포트 표시)을 봅니다. `R`을 누르면 AI 인형과 바로 시작합니다.
- 2인 PIE: 에디터 Play 설정에서 Net Mode = Play As Listen Server, Players = 2.
- 패키지 빌드
  - 배포용(Win64 Shipping): `Dist/WindupCrew_v1.2_Win64.zip` (372 MB, 압축을 풀면 `WindupCrew_v1.2_Win64/Crank.exe` + `2인_플레이_방법.txt`). 같은 내용의 폴더 `Dist/WindupCrew_v1.2_Win64`, UAT 원본 출력(디버그 심볼 포함) `Dist/WindupCrew/Windows`.
  - Shipping은 명령줄 맵 지정(`?listen`, 접속 주소)을 무시하므로, 메뉴 버튼과 같은 경로를 타는 `-CrankHost` / `-CrankJoin=<주소>` 옵션을 제공합니다. 접속 기록은 `%LOCALAPPDATA%/Crank/Saved/Logs/CrankNet.log`(Shipping에도 남음).
  - 개발용(Win64 Development, 로그·콘솔 사용 가능): `Build/Windows/Crank.exe`.
  - 다시 패키징: 에디터를 닫고 `RunUAT.bat BuildCookRun -project=... -platform=Win64 -clientconfig=Shipping -build -cook -stage -pak -archive -archivedirectory=D:\Game\CRANK\Dist\WindupCrew` (에디터가 켜져 있으면 MCP 서버 포트 8000 충돌로 쿡이 실패합니다).
- 스크린샷: `Docs/Screenshots`

## 조작
| 입력 | 동작 |
|---|---|
| WASD / 마우스 / Space | 이동(토크가 높을수록 빠름) / 카메라 / 점프(30 T 이상) |
| 좌클릭 · 우클릭 홀드 | 왼손 · 오른손 잡기 |
| 친구 등 뒤에서 좌클릭 → 마우스로 원 그리기 | 태엽 감기 (0.3~0.5초/바퀴 리듬이면 보너스) |
| E 홀드 | 일정 속도로 감기 (혼자서도 친구 태엽을 잡음) |
| 감는 중 S 홀드 → 좌클릭 떼기 | 100 T 이상인 친구를 새총 발사 |
| F | 자가 해제 발사 (내 토크 100 T 이상, 발사하면 토크 30 소모) |
| 친구를 우클릭으로 잡고 좌클릭 | 업기 (달리며 놓으면 던지기) |
| 1~4 / Tab·F1 / Esc·M / R | 이모트 / 조작법 / 메뉴(Q: 메인 메뉴) / 레이드 종료 후 다시 하기 · 대기 중 혼자 시작 |

## 밸런스 (2차)
- 이동속도 약 12% 상승: 기어가기 45 / 걷기 170 / 달리기 335 / 질주 500 (토크 0 / 30 미만 / 70 미만 / 70 이상).
- 태엽 소모 약 40% 감소: 이동 중 초당 0.6 / 1.2 / 2.4, 점프 4, 일어서기 2.
- 발사(새총·자가 발사) 후 토크가 20으로 떨어지던 것을 30만 소모하도록 변경(100 → 70).

## 구역 F — 장난감 방 / 거대 보스 "짝짝이 대왕"
- 부엌 북쪽 벽의 쥐구멍을 지나면 장난감 방. 9 m짜리 태엽 심벌즈 원숭이(Tripo AI로 생성한 모델, Mixamo 리그 + Blender 키프레임 클립 19종)가 잠들어 있습니다.
- **체력 100. 친구를 발사해서 부딪히면 무조건 피해**: 몸통 5 · 얼굴 7(공격 준비 중이면 기절해 패턴이 끊김) · 등 뒤 태엽 열쇠 18 · 태엽이 풀려 주저앉았을 때(8~11초) 열쇠 30(치명타). 체력 66/33에서 페이즈가 오릅니다. 화면 위 체력 바와 피해 숫자로 표시됩니다.
- 패턴(2차 밸런스): 심벌즈 박수 충격파(점프로 넘기, 반경 21 m, 페이즈 2+ 2연속) · 점프 내려찍기(착지 반경 4.8 m, 페이즈 2+ 2연속) · 부메랑 심벌즈 투척(페이즈 2+, 던진 손으로 되돌아옴) · 걷기 추적. 공격 준비 동작이 1.0~1.9초로 길어지고 공격 사이 대기도 1.8~2.6초로 늘었습니다.
- 넉백 뒤 2.6초 동안은 다시 넉백되지 않으며, 입구(체크포인트 F) 반경 13 m 안으로는 보스가 들어오지 않습니다.
- 격파하면 황금 태엽 열쇠가 튀어나옵니다. **메인 스프링(먼지먹개)과 황금 열쇠를 모두 작업대에 넣어야 클리어**입니다.
- 애니메이션: Idle / Walk(제자리 회전 시 스텝 포함) / Dormant / ClapWindup / Clap / HopWindup / HopAir / Land / ThrowWindup·Throw(좌·우) / WindDownEnter / WoundDown / Rewind / HitReact(피격 레이어) / Stagger / PhaseRoar / Defeat. 서버 상태+서버 시각으로 모든 PC에서 같은 포즈를 재생합니다.

## 치트 (개발 빌드 콘솔 `~`)
`CrankTorque 100`, `CrankTorqueAll 100`, `CrankGoto Start|A|B|C|D|E|F|Return`, `CrankDummy`, `CrankRupture`, `CrankRagdoll`, `CrankLaunch 1800`, `CrankLaunchPlayer 1 1800`, `CrankTime 60`, `CrankParts`, `CrankBossWake|BossStun|BossPhase 3|BossKill|BossDock`, `CrankGiantWake|GiantDown|GiantPhase 2|GiantHit|GiantKill|GiantAttack 0~2`

## 에셋 파이프라인 (재생성)
- Blender 스크립트: `ArtSource/Blender` (`crank_kit.py`, `doll.py`, `arch.py`, `props.py`, `fx_boss.py`, `toyroom.py`, `giant_anims.py`) → `ArtSource/Export/*.fbx`. 거대 보스 원본·리그·클립은 `ArtSource/Blender/giant_monkey.blend`(심벌즈 전용 본 CymbalL/R 포함).
- 사운드: `ArtSource/Audio/synth_sfx.py`, `synth_giant.py` (절차 합성 WAV)
- 언리얼 임포트/구성: `ArtSource/Unreal` — `import_doll.py`, `import_static_all.py`, `import_audio.py`, `make_materials.py`, `make_blueprints.py`, `import_giant.py`(보스·장난감 방·사운드), `import_giant_rig.py`(보스 SK+클립만 재임포트; 레벨에 보스가 있는 맵이 열려 있지 않을 때 실행), `make_giant_bps.py`, `build_level.py`(Lvl_Kitchen), `build_menu.py`(Lvl_MainMenu). 에디터 Python(원격 실행)으로 실행합니다.
- 화면에 보이는 C++ 액터는 모두 `/Game/Crank/Blueprints/BP_*` 서브클래스로 배치합니다.
