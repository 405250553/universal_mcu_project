# 把 frame buffer 回收延後到 LTDC reload 中斷才做

`BSP_LCD_Reload(LCD_RELOAD_VERTICAL_BLANKING)` 只是請求 LTDC 在下一次垂直消隱時切換畫面位址，呼叫當下硬體可能還在讀舊的 buffer。原本 `DisplayTask` 呼叫完 reload 就立刻同步回收上一張 buffer，讓 `SdProduceTask` 有機會在 LTDC 還沒真的切換完成前就用 DMA 覆寫同一塊記憶體，造成撕裂。改為在 `HAL_LTDC_ReloadEventCallback`（硬體確認 reload 生效才觸發的中斷）裡才真正把 buffer 送回 `FrameFreeQueue`，`DisplayTask` 只負責標記 `PendingFreeBufferIdx`。

## Consequences

回收動作被拆成「task 標記」+「ISR 完成」兩個非同步的步驟後，`DisplayTask` 在檔案切換邊界收工（`vTaskDelete`）時，若 ISR 還沒觸發就先 reset 了 `FrameFreeQueue`，之後 ISR 才把同一個索引送進重灌過的 queue，會造成同一個 buffer 索引重複出現、被兩邊同時使用——這正是本次修的 race condition（見 `stream_module.c` `DisplayTask` 收工前等待 `PendingFreeBufferIdx == INVALID_IDX` 的迴圈）。往後任何會讓 `DisplayTask`/`AudioplayTask` 提前結束生命週期的改動，都要留意是否有同類「延後回收」尚未完成。
