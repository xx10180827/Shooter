# Energy 体力条素材记录

2026-10-04。原始图片为用户提供的 Energy.jpg（保留不改写），棋盘格已画入 JPEG，并非真实透明通道。

使用内置 image_gen 编辑生成 Energy_Transparent.png；未使用外部 API/CLI。衍生图为 2172×724 RGBA，已读取确认左上角 alpha=0、主体保留。生成工具可能对细节进行重绘，属于原设计的透明衍生素材。

最终提示词：

Use case: background-extraction. Edit target: Energy.jpg, a cyan blue curved futuristic stamina HUD frame. Remove ONLY the baked checkerboard background, all background shadows, and thin stray black lines extending outside the main frame. Preserve the exact wide curved metallic silver-black housing, cyan inner luminous outline, dark blue panel, asymmetric subtle texture, shape and original proportions. Output a clean isolated main HUD frame on genuine transparent alpha, no checkerboard painted into pixels, no ground shadow, no text/numbers, no added objects. Tight horizontal canvas with a small transparent margin around the main frame, preserving the whole frame without cropping its tips. This is a game texture used directly on top of gameplay. Keep the existing design as close as possible.

UE 使用 T_StaminaEnergy 纹理和 M_StaminaEnergy UI 材质；StaminaFraction 从 GAS 实时读入，仅压暗已消耗区域的蓝色能量，金属框始终保留。没有把固定百分比或数字画入图片，数值由 HUD 绘制。