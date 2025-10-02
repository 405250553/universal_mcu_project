#include "main.h"
#include "lan8742.h"
#include <stdint.h>
#include <string.h>
#include <stdarg.h>

/* Debug UART wrapper */
#define DBG_PRINT(fmt, ...) do { \
    char buf[128]; \
    mini_snprintf(buf, sizeof(buf), fmt, ##__VA_ARGS__); \
    UART_SendString(buf); \
} while(0)

int32_t ETH_PHY_IO_Init(void);
int32_t ETH_PHY_IO_DeInit (void);
int32_t ETH_PHY_IO_ReadReg(uint32_t DevAddr, uint32_t RegAddr, uint32_t *pRegVal);
int32_t ETH_PHY_IO_WriteReg(uint32_t DevAddr, uint32_t RegAddr, uint32_t RegVal);
int32_t ETH_PHY_IO_GetTick(void);

/* Prototypes ---------------------------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ETH_Init(void);
static void MX_USART1_UART_Init(void);
void ETH_TestRX(void);
void ETH_TestTX(void);
void showRxDescStatus(void);

/* Mini snprintf ------------------------------------------------------------*/
static int itoa_simple(int value, char* buf, int bufsize);
static int itoa_hex(unsigned int value, char* buf, int bufsize, int width);
int mini_snprintf(char* buf, size_t bufsize, const char* fmt, ...);
void UART_SendString(const char* str);

/* Private variables ---------------------------------------------------------*/
ETH_DMADescTypeDef DMARxDscrTab[ETH_RX_DESC_CNT] __attribute__((section(".ram_dma")));
ETH_DMADescTypeDef DMATxDscrTab[ETH_TX_DESC_CNT] __attribute__((section(".ram_dma")));

__attribute__((section(".ram_dma"), aligned(32))) uint8_t Tx_Buff[ETH_TX_DESC_CNT][ETH_TX_BUF_SIZE];
__attribute__((section(".ram_dma"), aligned(32))) uint8_t Rx_Buff[ETH_RX_DESC_CNT][ETH_RX_BUF_SIZE];

typedef enum { RXBUF_FREE, RXBUF_BUSY } RxBufStatus;
static RxBufStatus RxBufState[ETH_RX_DESC_CNT] = {RXBUF_FREE};

ETH_HandleTypeDef heth;
UART_HandleTypeDef huart1;
ETH_TxPacketConfig TxConfig;


lan8742_Object_t LAN8742;
lan8742_IOCtx_t  LAN8742_IOCtx = {
    ETH_PHY_IO_Init,
    ETH_PHY_IO_DeInit,
    ETH_PHY_IO_WriteReg,
    ETH_PHY_IO_ReadReg,
    ETH_PHY_IO_GetTick
};

// RX buffer 分配給 DMA
void HAL_ETH_RxAllocateCallback(uint8_t **buff) {
    static int idx = 0;
    *buff = Rx_Buff[idx];
    idx = (idx + 1) % ETH_RX_DESC_CNT;
}

// 收到封包 callback
void HAL_ETH_RxLinkCallback(void **pStart, void **pEnd, uint8_t *buff, uint16_t Length) {
    DBG_PRINT("RX Packet (%d bytes):\r\n", Length);

    for(int i=0;i<Length;i++){
        DBG_PRINT("%x ",buff[i]);
        if((i+1)%16==0) DBG_PRINT("\r\n");
    }
    DBG_PRINT("\r\n");

    // 標記 buffer 可重用
    for(int i=0;i<ETH_RX_DESC_CNT;i++){
        if(Rx_Buff[i]==buff){
            RxBufState[i]=RXBUF_FREE;
            break;
        }
    }

    *pStart = buff;
    *pEnd   = buff;
}

// 顯示每個 RX descriptor OWN bit
void showRxDescStatus(void){
    for(int i=0;i<ETH_RX_DESC_CNT;i++){
        uint32_t desc0 = DMARxDscrTab[i].DESC0;
        uint8_t own = (desc0>>31)&0x1;
        DBG_PRINT("RX Desc[%d] OWN=%d, DESC0=0x%x\r\n", i, own, desc0);
    }
}

// Main loop 定期 poll
void ETH_TestRX(void){
    void *rxBuff;
    HAL_ETH_ReadData(&heth, &rxBuff);
    //showRxDescStatus();
}

// 簡單測試封包
void prepareTestFrame(uint8_t* testFrame){
    memset(testFrame,0,64);
    // MAC header
    testFrame[0]=0xFF; testFrame[1]=0xFF; testFrame[2]=0xFF;
    testFrame[3]=0xFF; testFrame[4]=0xFF; testFrame[5]=0xFF;
    testFrame[6]=0x00; testFrame[7]=0x80; testFrame[8]=0xE1;
    testFrame[9]=0x00; testFrame[10]=0x00; testFrame[11]=0x01;
    testFrame[12]=0x08; testFrame[13]=0x00;
    const char payload[]="HELLO!!";
    memcpy(&testFrame[14], payload, sizeof(payload)-1);
}

void ETH_TestTX(void){
    static uint32_t tx_index=0;
    uint8_t* testFrame=Tx_Buff[tx_index];
    tx_index=(tx_index+1)%ETH_TX_DESC_CNT;

    prepareTestFrame(testFrame);
    SCB_CleanDCache_by_Addr((uint32_t*)testFrame, ETH_TX_BUF_SIZE);

    ETH_BufferTypeDef TxBuffer={.buffer=testFrame,.len=64,.next=NULL};
    memset(&TxConfig,0,sizeof(TxConfig));
    TxConfig.Attributes=ETH_TX_PACKETS_FEATURES_CSUM|ETH_TX_PACKETS_FEATURES_CRCPAD;
    TxConfig.ChecksumCtrl=ETH_CHECKSUM_DISABLE;
    TxConfig.CRCPadCtrl=ETH_CRC_PAD_INSERT;
    TxConfig.Length=64;
    TxConfig.TxBuffer=&TxBuffer;

    DBG_PRINT("TX start...\r\n");
    HAL_StatusTypeDef ret=HAL_ETH_Transmit(&heth,&TxConfig,1000);
    if(ret==HAL_OK) DBG_PRINT("TX OK!\r\n");
    else {
        DBG_PRINT("ErrorCode=0x%x, DMAErrorCode=0x%x\r\n",heth.ErrorCode,heth.DMAErrorCode);
        DBG_PRINT("TX FAIL!\r\n");
    }

    HAL_ETH_ReleaseTxPacket(&heth);
}

/* USER CODE END 0 */

int main(void){
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_ETH_Init();
    MX_USART1_UART_Init();

    uint32_t val;
    do{
        HAL_ETH_ReadPHYRegister(&heth,0,PHY_BSR,&val);
        DBG_PRINT("PHY Link Down first!\r\n");
        HAL_Delay(100);
    }while((val&(1<<2))==0);

    DBG_PRINT("PHY Link Up already!\r\n");

    DBG_PRINT("before HAL_ETH_Start MACCR=0x%x\r\n", heth.Instance->MACCR);
    DBG_PRINT("before HAL_ETH_Start DMAOMR=0x%x\r\n", heth.Instance->DMAOMR);

    HAL_ETH_Start(&heth);

    DBG_PRINT("after HAL_ETH_Start MACCR=0x%x\r\n", heth.Instance->MACCR);
    DBG_PRINT("after HAL_ETH_Start DMAOMR=0x%x\r\n", heth.Instance->DMAOMR);

    uint32_t phy_bmsr;
    ETH_PHY_IO_ReadReg(1,1,&phy_bmsr);
    DBG_PRINT("after HAL_ETH_Start phy_bmsr=0x%x\r\n",phy_bmsr);

    while(1){
        ETH_TestRX();
        //ETH_TestTX();
    }
}

//-------------------------------------------
// mini snprintf 支援 %d 和 %s
static int itoa_simple(int value, char* buf, int bufsize) {
    if(!buf || bufsize <= 0) return 0;
    int is_neg = 0;
    if(value < 0){ is_neg=1; value=-value; }
    char tmp[16]; int j=0;
    do{ tmp[j++]='0'+(value%10); value/=10; } while(value && j<16);
    if(is_neg) tmp[j++]='-';
    int k=0; while(j-- && k<bufsize-1) buf[k++]=tmp[j]; buf[k]='\0';
    return k;
}

// itoa 十六進位，支援補零
static int itoa_hex(unsigned int value, char* buf, int bufsize, int width) {
    if (!buf || bufsize <= 0) return 0;
    char tmp[16];
    int j = 0;
    do {
        int d = value & 0xF;
        tmp[j++] = (d < 10) ? ('0' + d) : ('a' + d - 10);
        value >>= 4;
    } while (value && j < 16);

    // 補零到 width
    while (j < width && j < 16) {
        tmp[j++] = '0';
    }

    int k = 0;
    while (j-- && k < bufsize - 1) {
        buf[k++] = tmp[j];
    }
    buf[k] = '\0';
    return k;
}

int mini_snprintf(char* buf, size_t bufsize, const char* fmt, ...) {
    if (!buf || bufsize == 0) return 0;
    va_list args;
    va_start(args, fmt);
    size_t pos = 0;
    for (size_t i = 0; fmt[i] && pos < bufsize - 1; i++) {
        if (fmt[i] == '%') {
            i++;
            if (fmt[i] == 'd') {
                char tmp[16];
                itoa_simple(va_arg(args, int), tmp, sizeof(tmp));
                for (size_t j = 0; tmp[j] && pos < bufsize - 1; j++) buf[pos++] = tmp[j];
            }
            else if (fmt[i] == 's') {
                const char* s = va_arg(args, const char*);
                for (size_t j = 0; s[j] && pos < bufsize - 1; j++) buf[pos++] = s[j];
            }
            else if (fmt[i] == 'x') {
                // 預設 hex (不補 0)
                char tmp[16];
                itoa_hex(va_arg(args, unsigned int), tmp, sizeof(tmp), 0);
                for (size_t j = 0; tmp[j] && pos < bufsize - 1; j++) buf[pos++] = tmp[j];
            }
            else if (fmt[i] == '2' && fmt[i+1] == 'x') {
                // %2x, 固定兩位 hex
                char tmp[16];
                itoa_hex(va_arg(args, unsigned int), tmp, sizeof(tmp), 2);
                for (size_t j = 0; tmp[j] && pos < bufsize - 1; j++) buf[pos++] = tmp[j];
                i++; // 跳過 'x'
            }
            else {
                buf[pos++] = fmt[i];
            }
        } else {
            buf[pos++] = fmt[i];
        }
    }
    buf[pos] = '\0';
    va_end(args);
    return pos;
}

void UART_SendString(const char* str){
    if(!str) return;
    HAL_UART_Transmit(&huart1,(uint8_t*)str,strlen(str),HAL_MAX_DELAY);
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 400;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_6) != HAL_OK)
  {
    Error_Handler();
  }
}

/*******************************************************************************
                       PHI IO Functions
*******************************************************************************/
/**
  * @brief  Initializes the MDIO interface GPIO and clocks.
  * @param  None
  * @retval 0 if OK, -1 if ERROR
  */
int32_t ETH_PHY_IO_Init(void)
{
  /* We assume that MDIO GPIO configuration is already done
     in the ETH_MspInit() else it should be done here
  */

  /* Configure the MDIO Clock */
  HAL_ETH_SetMDIOClockRange(&heth);

  return 0;
}

/**
  * @brief  De-Initializes the MDIO interface .
  * @param  None
  * @retval 0 if OK, -1 if ERROR
  */
int32_t ETH_PHY_IO_DeInit (void)
{
  return 0;
}

/**
  * @brief  Read a PHY register through the MDIO interface.
  * @param  DevAddr: PHY port address
  * @param  RegAddr: PHY register address
  * @param  pRegVal: pointer to hold the register value
  * @retval 0 if OK -1 if Error
  */
int32_t ETH_PHY_IO_ReadReg(uint32_t DevAddr, uint32_t RegAddr, uint32_t *pRegVal)
{
  if(HAL_ETH_ReadPHYRegister(&heth, DevAddr, RegAddr, pRegVal) != HAL_OK)
  {
    return -1;
  }

  return 0;
}

/**
  * @brief  Write a value to a PHY register through the MDIO interface.
  * @param  DevAddr: PHY port address
  * @param  RegAddr: PHY register address
  * @param  RegVal: Value to be written
  * @retval 0 if OK -1 if Error
  */
int32_t ETH_PHY_IO_WriteReg(uint32_t DevAddr, uint32_t RegAddr, uint32_t RegVal)
{
  if(HAL_ETH_WritePHYRegister(&heth, DevAddr, RegAddr, RegVal) != HAL_OK)
  {
    return -1;
  }

  return 0;
}

/**
  * @brief  Get the time in millisecons used for internal PHY driver process.
  * @retval Time value
  */
int32_t ETH_PHY_IO_GetTick(void)
{
  return HAL_GetTick();
}


/**
  * @brief  Check the ETH link state then update ETH driver and netif link accordingly.
  * @retval None
  */
void ethernet_link_check_state()
{
  ETH_MACConfigTypeDef MACConf = {0};
  int32_t PHYLinkState = 0;
  uint32_t linkchanged = 0U, speed = 0U, duplex = 0U;

  PHYLinkState = LAN8742_GetLinkState(&LAN8742);

    switch (PHYLinkState)
    {
    case LAN8742_STATUS_100MBITS_FULLDUPLEX:
      duplex = ETH_FULLDUPLEX_MODE;
      speed = ETH_SPEED_100M;
      linkchanged = 1;
      break;
    case LAN8742_STATUS_100MBITS_HALFDUPLEX:
      duplex = ETH_HALFDUPLEX_MODE;
      speed = ETH_SPEED_100M;
      linkchanged = 1;
      break;
    case LAN8742_STATUS_10MBITS_FULLDUPLEX:
      duplex = ETH_FULLDUPLEX_MODE;
      speed = ETH_SPEED_10M;
      linkchanged = 1;
      break;
    case LAN8742_STATUS_10MBITS_HALFDUPLEX:
      duplex = ETH_HALFDUPLEX_MODE;
      speed = ETH_SPEED_10M;
      linkchanged = 1;
      break;
    default:
      break;
    }

    if(linkchanged)
    {
      /* Get MAC Config MAC */
      HAL_ETH_GetMACConfig(&heth, &MACConf);
      MACConf.DuplexMode = duplex;
      MACConf.Speed = speed;
      HAL_ETH_SetMACConfig(&heth, &MACConf);
      HAL_ETH_Start(&heth);
    }
  }

/**
  * @brief ETH Initialization Function
  * @param None
  * @retval None
  */
static void MX_ETH_Init(void)
{

  /* USER CODE BEGIN ETH_Init 0 */

  /* USER CODE END ETH_Init 0 */

   static uint8_t MACAddr[6];
   HAL_StatusTypeDef hal_eth_init_status = HAL_OK;

  /* USER CODE BEGIN ETH_Init 1 */

  /* USER CODE END ETH_Init 1 */
  heth.Instance = ETH;
  MACAddr[0] = 0x00;
  MACAddr[1] = 0x80;
  MACAddr[2] = 0xE1;
  MACAddr[3] = 0x00;
  MACAddr[4] = 0x00;
  MACAddr[5] = 0x00;
  heth.Init.MACAddr = &MACAddr[0];
  heth.Init.MediaInterface = HAL_ETH_RMII_MODE;
  heth.Init.TxDesc = DMATxDscrTab;
  heth.Init.RxDesc = DMARxDscrTab;
  heth.Init.RxBuffLen = 1536;

  /* USER CODE BEGIN MACADDRESS */

  /* USER CODE END MACADDRESS */

  hal_eth_init_status = HAL_ETH_Init(&heth);

  memset(&TxConfig, 0 , sizeof(ETH_TxPacketConfig));
  TxConfig.Attributes = ETH_TX_PACKETS_FEATURES_CSUM | ETH_TX_PACKETS_FEATURES_CRCPAD;
  TxConfig.ChecksumCtrl = ETH_CHECKSUM_IPHDR_PAYLOAD_INSERT_PHDR_CALC;
  TxConfig.CRCPadCtrl = ETH_CRC_PAD_INSERT;
  /* USER CODE BEGIN ETH_Init 2 */
  LAN8742_RegisterBusIO(&LAN8742, &LAN8742_IOCtx);

  /* Initialize the LAN8742 ETH PHY */
  if(LAN8742_Init(&LAN8742) != LAN8742_STATUS_OK)
  {
    return;
  }

  if (hal_eth_init_status == HAL_OK)
  {
    /* Get link state */
    ethernet_link_check_state();
  }
  else
  {
    Error_Handler();
  }
  /* USER CODE END ETH_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOJ_CLK_ENABLE();
  __HAL_RCC_GPIOI_CLK_ENABLE();
  __HAL_RCC_GPIOK_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOI, ARDUINO_D7_Pin|ARDUINO_D8_Pin, GPIO_PIN_RESET);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
    GPIO_InitStruct.Pin=GPIO_PIN_1;
    GPIO_InitStruct.Mode=GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull=GPIO_NOPULL;
    GPIO_InitStruct.Speed=GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOI,&GPIO_InitStruct);
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
