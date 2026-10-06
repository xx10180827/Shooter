# RifleShot.wav

原创程序合成的基础枪声音效，没有使用第三方采样。

48 kHz / 16-bit PCM / mono / 0.24 seconds。固定随机种子 904，短噪声瞬态、低通噪声尾音与衰减低频正弦混合，带起止淡变。用于项目功能验收，可替换为选定的最终音效。

UE 资源：/Game/Audio/SW_RifleShot。玩家和 AI 共用源声音，音量与 AI 空间衰减分别配置。

# T20 HitConfirm / KillConfirm（2026-10-06）

SW_HitConfirm.wav：44.1 kHz / 16-bit PCM / mono / 约 0.095 秒。普通命中音于 2026-10-06 根据反馈改为 2400 / 3650 Hz 双泛音短瞬态，减少噪声；默认播放音量 0.55。
SW_KillConfirm.wav：44.1 kHz / 16-bit PCM / mono / 约 0.20 秒。

均由 ShooterFeedbackSetupCommandlet 中的 C++ 确定性合成，随机种子 20261006；短噪声瞬态加衰减正弦，击杀音增加后置高音。起止淡变控制突变，无第三方采样。UE 资产在 /Game/Feedback，对应声音和音量在 DA_CombatFeedback 替换/调节。两种确认音只给本地玩家播放，不触发 AI 听觉事件。最终响度与风格等待用户试听验收。
