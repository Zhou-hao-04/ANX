# Smoothness Pipeline v1.0
# 分型面光顺性审查-标记-修复三步工作流
# 2026-07-22

## 目录结构
Smoothness Pipeline/
  ai_workflow.py       # Python AI接口（可被AI智能体调用）
  workflow.yaml        # AI工作流定义（用于Codex/AI识别）
  dll/
    mark_review.dll    # Phase 1: 审查+标记（v36检测+绿色）
    mark_final.dll     # Phase 2: 修复（边合并+删愈+补孔G0）
  bin/
    playdll_v2.exe     # DLL执行器（批处理模式用）
  config/
    inspect_config.txt # 输入输出路径配置
  output/
    reviewed.prt       # Phase 1输出
    repaired.prt       # Phase 2输出
    report.txt         # 审查报告

## 执行方式

### AI自动执行（推荐）
from ai_workflow import run_full_pipeline
result = run_full_pipeline("input.prt")

### 手动分步执行
Phase 1: NX中Ctrl+U → 选 dll/mark_review.dll
Phase 2: NX中Ctrl+U → 选 dll/mark_final.dll
Phase 3: 选中绿色面 → 删除面(Heal=true)

## 预期结果
审查: 检测 ~157 扭曲面 / 425 总面数
修复: 边合并 ~36 条, 删愈 ~25 面
残余: 绿色面需手动处理
