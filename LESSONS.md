# 開發過程學到的教訓

從 `update_tmp` 分支(相對 `main` 多出的 94 個 commit)整理出來,值得記住的坑與取捨。單純學習筆記,不是架構決策(架構決策記在 `docs/adr/`)。

## 同步 / 併發

**Binary semaphore 忘記給初始值,task 會永久卡死**
UART TX task 靠 `TX_DONE` semaphore 等硬體傳輸完成,但如果一開始沒有 give 一次初始值,第一次等待就會永遠等不到——因為根本沒人 give 過。
(`dc024a5`)

**非阻塞的 HAL IT/DMA 傳輸,function return 不代表硬體真的傳完**
`HAL_UART_Transmit_IT` 一呼叫就馬上返回,但 UART 實際傳輸是背景進行的。如果把 stack/local 變數的位址傳進去,function 一結束變數就被釋放或覆寫,UART 送出來的就是亂碼。傳給 DMA/IT 的 buffer,生命週期一定要蓋過硬體實際傳輸完成的時間點,不能只看 function return。
(`dfe7944`, `f479d59`)

**undefined pointer 導致的 memory corruption**
`stream_module.c` 曾經因為一個未初始化/野掉的指標造成記憶體損毀,這也是 Producer/Consumer 協定後來被重新設計(state machine → event group)的直接原因之一。
(`a25250e`)

**音訊 DMA/codec 啟動瞬間不穩定,靠丟棄前幾筆資料處理**
Audio DMA 剛啟動時前幾個 buffer 會有失真,workaround 是靜音前 2 個 audio buffer 再開始正常輸出。這種「硬體/codec 啟動瞬間不穩定」的模式在音訊/類比周邊很常見,之後遇到類似失真問題可以先往這個方向排查。
(`240ebb0`)

## 畫面 / 圖像處理

**LTDC 跟 SD 卡同時搬資料會撕裂畫面,改用 DMA2D 而非 memcpy**
當 LCD 顯示（LTDC）跟 SD 卡讀取同時要動同一塊記憶體時,用 CPU `memcpy` 搬資料會造成畫面撕裂(tearing)。改用 DMA2D 搬 frame 資料就解決了——這是避免 tearing 的正規做法,之後任何「LCD 同時被兩個來源存取」的情境都可以先想到 DMA2D。
(`580a88b`)

**Bitmap mask 邊界:矩形資料畫圓形圖示,邊界需要手動 padding**
音量圖示是圓形,但 bmp 檔案格式定義的資料範圍是矩形,邊界不透明是因為圓形範圍外、矩形範圍內的區域沒有正確設成透明,需要手動 padding 0。
(`3fe4c95`)

**待查:啟用 memcpy 後系統會掛掉**
`17769ad` 當時繞過了一個「一用 memcpy 系統就掛」的問題,但沒有查出根本原因。如果現在還沒解開,值得回頭排查是不是 SDRAM 存取時序、cache、或 unaligned access 造成的——這類問題在 Cortex-M7 + 外部 SDRAM 的組合上很常見。

## 檔案格式 / 協定知識

**AVI 容器格式理論上什麼像素格式都能塞,但實務上大家都預期 BGR888**
AVI 是個容器格式,規格上不限制影像資料的像素排列,但幾乎所有播放器都預期是 BGR888,塞 RGB888 進去大部分播放器認不得。這種「規格允許、但業界事實上有隱性慣例」的坑,寫轉檔工具時特別容易忘記。
(`5fbdd53`)

**動手刻功能前,先查函式庫有沒有現成的**
一開始自己刻了走訪 ARP table 的函式,後來才發現 lwIP 本來就有 `etharp_get_entry` 可以直接用。
(`518d83a`)

## Producer/Consumer 協定的演進(對應 `docs/adr/0001`)

這幾個 commit 串起來是同一條協定演進史:一開始用陣列 + state machine 管理播放狀態,後來改成 malloc + 更明確的 video state 系統(`fe35cce`),中間修過同步時機的 bug(`a241d7d`),最後整個換成 FreeRTOS event group,並把「音訊為主時間軸、畫面為輔時間軸」的設計定下來(`ea7a016`)。這也是目前 `docs/adr/0001-avi-playback-sync-module.md` 想進一步收斂的地方。
(`fe35cce`, `a241d7d`, `7ac5b00`, `ea7a016`)
