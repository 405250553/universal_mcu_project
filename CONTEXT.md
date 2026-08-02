# universal_mcu_project

STM32 韌體練習專案,目前主力是 `stm32f746_avi_player`——把預先轉檔成 RGB565 的影片從 SD 卡讀出來,循環播放,音訊為主時間軸、畫面為輔時間軸。

## Language

**Producer**:
負責從 SD 卡讀取 AVI 檔案、解析出畫面與音訊資料的角色。
_Avoid_: reader task, 讀檔 task

**Consumer**:
負責把 Producer 產生的資料播放出來的角色;播放路徑裡有兩個地位對等的 Consumer(畫面、音訊),不分主副。
_Avoid_: player task, 播放 task

**Sync**:
協調 Producer 與 Consumer 之間開機互等、檔案讀完通知、收工互等這三段流程的機制。收工互等不是只有 task 之間互等——DisplayTask 收工前，還必須等待「Deferred recycle」真正完成，否則會跟下一輪的 FrameFreeQueue 初始化互相踩到（見 `docs/adr/0002`）。
_Avoid_: Rendezvous, handshake protocol, 交握

**Deferred recycle**（延後回收）:
一個 buffer 被消費完畢後，並非立刻歸還給 free pool，而是先標記成「待回收（Pending）」，等某個之後才會發生的事件（例如 LTDC 的 reload 中斷，確認硬體真的已經切換到新畫面）確認安全後，才真正歸還。這個延遲的存在，代表 task 收工/重置 free pool 時必須先確認沒有懸而未決的待回收項目，否則同一個 buffer 可能被歸還兩次。
_Avoid_: lazy free, async free
