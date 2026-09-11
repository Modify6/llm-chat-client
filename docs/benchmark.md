# Benchmark 压测报告

> 测试环境:RTX 5080 (16GB),llama-server + Qwen2.5-7B-Instruct-Q4_K_M

---

## 硬件信息

| 组件 | 规格 |
|---|---|
| GPU | NVIDIA RTX 5080 (sm_120, 16GB GDDR7) |
| CPU | AMD / Intel (以实际为准) |
| 模型 | Qwen2.5-7B-Instruct-Q4_K_M (~4.6GB, 7.6B params) |
| llama-server | 内置 CUDA 后端 |
| 操作系统 | Windows 11 x64 |

---

## 压测方法

- **请求格式**:固定 prompt "Reply OK only." + 32 tokens 填充
- **max_tokens**:16(减少生成时间,测请求吞吐而非生成速度)
- **temperature**:0.1(确定性输出)
- **超时**:60 秒
- **指标**:QPS / 平均延迟 / P50 / P99 / 成功率

---

## 实测数据

### 2026-09-11 测试

| 并发 | 请求数 | QPS | 平均延迟(ms) | P50(ms) | P99(ms) | 成功率 | 总耗时(s) |
|---|---|---|---|---|---|---|---|
| 1 | 20 | 32.05 | 27.85 | 27.05 | 31.20 | 100% | 0.62 |
| 2 | 20 | 49.73 | 37.23 | 37.22 | 45.50 | 100% | 0.40 |
| 4 | 10 | 71.96 | 43.86 | 40.88 | 52.30 | 100% | 0.14 |
| 8 | 5 | 56.75 | 45.83 | 42.28 | 58.10 | 100% | 0.09 |

### 观察

1. **并发 4 时 QPS 最高(71.96)**,之后并发 8 反而下降——说明 GPU 队列开始排队,单次请求等待时间变长
2. **延迟随并发增加而上升**:27ms → 46ms,但增长缓慢,GPU 还没到瓶颈
3. **成功率 100%**:llama-server 在低并发下非常稳定
4. **max_tokens=16** 时生成极快,延迟主要来自 prefill + 网络往返

### 不同 max_tokens 的影响(待补充)

| max_tokens | 并发 | QPS | 平均延迟 | 备注 |
|---|---|---|---|---|
| 16 | 1 | 32 | 28ms | 短回复 |
| 128 | 1 | 待测 | 待测 | 中等回复 |
| 512 | 1 | 待测 | 待测 | 长回复 |

### GPU 显存占用(待补充)

| 并发 | 显存占用 | 利用率 |
|---|---|---|
| 1 | 待测 | 待测 |
| 8 | 待测 | 待测 |

---

## 分析与优化建议

### 当前瓶颈
- **max_tokens=16** 时,请求在 GPU 上排队时间很短,主要瓶颈在 llama-server 的请求解析和调度
- 并发 4→8 QPS 下降约 21%,说明 GPU 调度开销开始显现

### 优化方向

| 方向 | 预期收益 | 操作 |
|---|---|---|
| 调大 batch size | 中 | llama-server `-b 2048`(默认 512) |
| 增加 GPU 层数 | 中 | `-ngl 99`(全 GPU,已默认) |
| 降低 model 精度 | 高 | 换 Q3_K_M / Q2_K,速度提升 30-50% |
| 增大 n_ctx | 低 | `-c 8192`,支持更长上下文 |
| 启用 speculative decoding | 高 | llama.cpp 最新版支持 |
| 换 Flash Attention | 中 | 需要重新编译 llama.cpp |

### 结论

- **当前配置在低并发(1-4)下吞吐良好,成功率 100%**
- **并发 > 4 时 GPU 排队明显**,建议生产环境并发控制在 4-8
- **换更小模型(Q3_K_M)或增大 batch size 可进一步提升 QPS**

---

## 如何自己跑压测

### 方法 1:UI 压测面板

1. 启动 `llm-chat-client.exe`
2. 菜单 → 工具 → 并发压测(或 Ctrl+T)
3. 设并发数、总请求数 → 点"开始压测"
4. 跑完后点"导出 CSV"保存报告

### 方法 2:PowerShell 快速测试

```powershell
$body = @{
    model = "qwen2.5-7b"
    messages = @(@{role="user"; content="Hello"})
    temperature = 0.1
    max_tokens = 16
    stream = $false
} | ConvertTo-Json -Depth 5

1..10 | ForEach-Object {
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $r = Invoke-WebRequest -Uri "http://127.0.0.1:8080/v1/chat/completions" `
        -Method POST -Body $body -ContentType "application/json" -UseBasicParsing
    $sw.Stop()
    Write-Output "Request $_ : $($sw.Elapsed.TotalMilliseconds) ms"
}
```

### 方法 3:用 curl 批量测试

```bash
for i in $(seq 1 10); do
  curl -s -o /dev/null -w "$i: %{time_total}s\n" \
    http://127.0.0.1:8080/v1/chat/completions \
    -H "Content-Type: application/json" \
    -d '{"model":"qwen2.5-7b","messages":[{"role":"user","content":"Hello"}],"max_tokens":16}'
done
```
