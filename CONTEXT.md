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
協調 Producer 與 Consumer 之間開機互等、檔案讀完通知、收工互等這三段流程的機制。
_Avoid_: Rendezvous, handshake protocol, 交握
