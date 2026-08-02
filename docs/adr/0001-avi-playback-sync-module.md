# 把 AVI 播放的 Producer/Consumer 協定收進獨立的 Sync module

Producer 與兩個 Consumer(Display、Audio)之間的開機互等、檔案讀完通知、收工互等,一開始是用一個全域變數搭配 bit define 手刻,後來發現 FreeRTOS event group 剛好符合這個情境就換了過去,但一直沒有給它一個擁有者——三個 task 加兩個 ISR 都直接讀寫同一組 bit,協定本身只能靠同時看這五個地方才拼得出來。這也是這塊程式碼被重寫最多次的地方(state machine 改 event group、一次 flush 順序的修正、一次記憶體損毀的修復)。

決定把它抽成獨立的 `Drivers/MY_DRIVER/stream_sync.c`/`stream_sync.h`,event group 跟 bit 巨集變成該檔案的私有狀態,對外只開放 7 個函式(`SyncDisplayReady`/`SyncAudioReady`/`SyncProducerFileFinished`/`SyncIsProducerFinished`/`SyncDisplayExited`/`SyncAudioExited`/`SyncProducerAwaitBothExited`)。這次是純介面抽出,行為不變(全部維持永久阻塞、無 timeout),只順手修掉一個重複兩次的 clear-bit 小 bug。函式名稱刻意只用 `Sync` 前綴,沒有跟隨專案裡其他模組(`stream_module.h`/`stream_system.h`)既有的 `Avi` 前綴慣例。
