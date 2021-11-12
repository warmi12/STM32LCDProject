#include "Play_WAV.h"

#include "string.h"
#include "WM8960.h"
#include "i2S.h"
#include "i2c.h"
#include "gpio.h"
#include "stdio.h"
#include "stm32f3xx_hal.h"

uint8_t CloseFileFlag;    //1:already open file have to close it.

uint8_t EndFileFlag;      //1:reach the wave file end;
                          //2:wait for last transfer;
                          //3:finish transfer stop dma.

__IO uint8_t FillBufFlag; //0:fill first half buf;
                          //1:fill second half buf;
                          //0xFF:do nothing.

FIL WAV_File;             //File struct

wavctrl WaveCtrlData;     //Play control struct

uint32_t Audio_LastBytes; //The WAV file last bytes to play

uint8_t WAV_Buffer[WAV_BUFFER_SIZE];  //The buffer to ache WAV data
//uint16_t WAV_16bit[WAV_BUFFER_SIZE/2];

uint32_t WAV_OFFSET;      //Offset of record FIL pointer in WAV file

uint32_t WAV_LastData;    //The size last data to be played

uint8_t Play_Flag;        //When the WAV file is being play, Play_Flag = 1.

uint8_t Pause_Flag;       //When the player is pause, Pause_Flag = 1.

uint8_t PausePlayFlag;

char Play_List[10][50] = {NULL}; //play list

uint8_t Music_Num_MAX;    //Number of music.

int8_t Music_Num = 0;    //the number of music which is being played.

uint8_t End_Flag;         //When the music is over, End_Flag = 1;

I2S_CallBack_Flag I2S_Flag; //I2S CallBack function flag.
uint8_t TempBuf[WAV_BUFFER_SIZE/2];

/**
  * @brief  Scan the WAV files that set the path.
  * @param  path: Path to scan.
  * @retval None
  */
FRESULT ScanWavefiles(char* path) {

  FRESULT res;
  FILINFO fno;
  DIR dir;
  uint16_t i,j;

  res = f_opendir(&dir, path);    //Open the directory
  if(res != FR_OK)  {
    printf("f_opendir error !\r\n");
    return res;
  }

  for(i=0;;i++) {                 //Scan the files in the directory
    res = f_readdir(&dir, &fno);  //read a item
    if(res != FR_OK)  {
      printf("f_readdir error !\r\n");
      return res;
    }
    if(fno.fname[0] == 0)         //scan to the end of the path
      break;

    for(j=0;j<_MAX_LFN;j++) {
      if(fno.fname[j] == '.')     //Check if the type of the file is WAV
      break;
    }

    if(((fno.fname[j+1] == 'w')||(fno.fname[j+1] == 'W'))
        &&((fno.fname[j+2] == 'a')||(fno.fname[j+2] == 'A'))
        &&((fno.fname[j+3] == 'v')||(fno.fname[j+3] == 'V'))) //The file is WAV
    {
      strcpy(Play_List[i], path);     //Copy type of file is WAV
      strcat(Play_List[i],"/");       // Add '/' to the buffer
      strcat(Play_List[i],fno.fname); // Add file name to the buffer
      printf("%s\r\n", Play_List[i]); // print the whole file path to the UART
    }
  }
  res = f_closedir(&dir);             //Close the directory
  if(res != FR_OK)  {
    printf("f_closedir error !\r\n");
    return res;
  }

  Music_Num_MAX = i;

  printf("Scan WAV Files complete ! Music_Num = %d\r\n",Music_Num_MAX);

  return res;
}


/**
  * @brief  Play the WAV file that set the path.
  * @param  path: Path of the WAV file.
  * @retval None
  */
uint8_t PlayWaveFile(void)  {

  uint8_t res;

  CloseFileFlag = 0;
	EndFileFlag = 0;
	FillBufFlag = 0xFF;
  Play_Flag = 0;
  End_Flag = 0;
  PausePlayFlag = 1;
  I2S_Flag = I2S_No_CallBack;

  printf("Now Play: %s\r\n",Play_List[Music_Num]);
  Get_WAV_Message(Play_List[Music_Num],&WaveCtrlData);    //Get the messages of the WAV file

  WAV_OFFSET = WaveCtrlData.datastart;

  /*Start Play Music*/
  f_lseek(&WAV_File, WAV_OFFSET);
  Fill_WAV_Buffer(WAV_Buffer, WAV_BUFFER_SIZE);
  WAV_OFFSET += WAV_BUFFER_SIZE;
  while(End_Flag == 0)  {
    while(Play_Flag == 1) {
      if(I2S_Flag == I2S_Half_Callback) {
        f_lseek(&WAV_File, WAV_OFFSET);

        HAL_NVIC_DisableIRQ(EXTI3_IRQn);
        Fill_WAV_Buffer(WAV_Buffer,WAV_BUFFER_SIZE/2);
        HAL_NVIC_EnableIRQ(EXTI3_IRQn);
        WAV_OFFSET += WAV_BUFFER_SIZE/2;

        I2S_Flag = I2S_No_CallBack;
      }
      else if(I2S_Flag == I2S_Callback) {
        f_lseek(&WAV_File, WAV_OFFSET);
        HAL_NVIC_DisableIRQ(EXTI3_IRQn);
        Fill_WAV_Buffer((WAV_Buffer+WAV_BUFFER_SIZE/2),WAV_BUFFER_SIZE/2);
        HAL_NVIC_EnableIRQ(EXTI3_IRQn);
        WAV_OFFSET += WAV_BUFFER_SIZE/2;
        WAV_LastData -= WAV_BUFFER_SIZE;

        I2S_Flag = I2S_No_CallBack;
      }
      else  {
        Key_Control();
      }
    }
    Play_Flag = 1;

    if (End_Flag==0) {
    	HAL_I2S_Transmit_DMA(&hi2s2,(uint16_t*)WAV_Buffer, WAV_BUFFER_SIZE/2);
	}

  }

  res = f_close(&WAV_File);

  return res;
}

/**
  * @brief  Open the WAV file, get the message of the file.
  * @param  fname: name of the file you want to get its massage.
  * @param  wavx: the struct of data control.
  * @retval None
  */
uint8_t Get_WAV_Message(char* fname, wavctrl* wavx) {

  uint8_t res = 0;
  uint32_t br = 0;

  ChunkRIFF *riff;
	ChunkFMT *fmt;
	ChunkFACT *fact;
	ChunkDATA *data;

  res = f_open(&WAV_File, (TCHAR *)fname, FA_READ);     //Open the file
  if(res == FR_OK) {

    CloseFileFlag = 1;

    f_read(&WAV_File, TempBuf, WAV_BUFFER_SIZE/2, &br); //Read WAV_BUFFER_SIZE/2 bytes data

    riff = (ChunkRIFF *)TempBuf;      //Get RIFF Chunk

    if(riff->Format == 0x45564157)  { //Format = "WAV"

      fmt = (ChunkFMT *)(TempBuf+12); //Get FMT Chunk
      if(fmt->AudioFormat==1||fmt->AudioFormat==3)        //Linear PCM or 32 bits WAVE
      {
        fact=(ChunkFACT *)(TempBuf+12+8+fmt->ChunkSize);  //Read FACT chunk

        if((fact->ChunkID == 0x74636166)||(fact->ChunkID==0X5453494C))
          wavx->datastart=12+8+fmt->ChunkSize+8+fact->ChunkSize;  //When there is fact/LIST Chunk.
        else
          wavx->datastart=12+8+fmt->ChunkSize;
        data = (ChunkDATA *)(TempBuf+wavx->datastart);
        if(data->ChunkID==0X61746164) {           //Read DATA Chunk success
          wavx->audioformat=fmt->AudioFormat;     //Audio Format
          wavx->nchannels=fmt->NumOfChannels;     //channel number
          wavx->samplerate=fmt->SampleRate;				//Sample Rate
          wavx->bitrate=fmt->ByteRate*8;					//Byte Rate
          wavx->blockalign=fmt->BlockAlign;				//Block Align
          wavx->bps=fmt->BitsPerSample;						//number of chunk, 8/16/24/32 bits
          wavx->datasize=data->ChunkSize;					//Size of audio data chunk
          wavx->datastart=wavx->datastart+8;			//data stream start offest
          printf("WAV.audioformat:%d\r\n",wavx->audioformat);
          printf("WAV.nchannels:%d\r\n",wavx->nchannels);
          printf("WAV.samplerate:%d\r\n",wavx->samplerate);
          printf("WAV.bitrate:%d\r\n",wavx->bitrate);
          printf("WAV.blockalign:%d\r\n",wavx->blockalign);
          printf("WAV.bps:%d\r\n",wavx->bps);
          printf("WAV.datasize:%d\r\n",wavx->datasize);
          printf("WAV.datastart:%d\r\n",wavx->datastart);
        }
        else  {
          printf("Not find data chunk !!\r\n");
          printf("data->ChunkID = 0x%x\r\n",data->ChunkID);
          res = 4;
        }
      }
      else  {
        printf("Not linear PCM, not support !!\r\n");
        res = 3;
      }
    }
    else  {
      printf("Not WAV file !!\r\n");
      res = 2;
    }
  }
  else  {
    printf("Get_WAV_Message.f_open error !!\r\n");
    res = 1;
  }
  WAV_LastData = wavx->datasize;

  return res;
}

/**
  * @brief  Open the WAV file, get the message of the file.
  * @param  BUFF: the pointer of the buffer to cached data.
  * @param  size: the byte mumber of data.
  * @retval None
  */
uint32_t Fill_WAV_Buffer(uint8_t *BUFF, uint16_t size) {

  uint32_t NeedReadSize=0;
  uint32_t ReadSize;
  uint32_t i;
  uint8_t *p;
  float *f;
  int sound;

  //It has been read last time, return.
  if(EndFileFlag==1)
		return 0;

  if(WaveCtrlData.nchannels==2) {
    if(WaveCtrlData.bps == 16)          //16-bit audio,read data directly
    {
			f_read(&WAV_File,BUFF,size,(UINT*)&ReadSize);
    }
		else if(WaveCtrlData.bps==24)       //24-bit audio, adjust the order between the read data and the DMA cache
		{
      printf("WaveCtrlData.bps = %d\r\n",WaveCtrlData.bps);
			NeedReadSize=(size/4)*3;                                  //Number of bytes to read
			f_read(&WAV_File,TempBuf,NeedReadSize,(UINT*)&ReadSize);	//Read data
			p=TempBuf;
			ReadSize=(ReadSize/3)*4;                                  //Size of data after fill
			//printf("%d\r\n",ReadSize);
			for(i=0;i<ReadSize;)
			{
				BUFF[i]=p[1];
				BUFF[i+1]=p[2];
				BUFF[i+2]=0;
				BUFF[i+3]=p[0];
				i+=4;
				p+=3;
			}
		}
		else if(WaveCtrlData.bps == 8)      //8-bit audio, data need to be transformed to 16-bit mode before play
		{
      printf("WaveCtrlData.bps = %d\r\n",WaveCtrlData.bps);
			NeedReadSize=size/2;                                      //Number of bytes to read
			f_read(&WAV_File,TempBuf,NeedReadSize,(UINT*)&ReadSize);	//Read data
			p=TempBuf;
			ReadSize=ReadSize*2;                                      //Size of data after fill
			for(i=0;i<ReadSize;)
			{
				BUFF[i]=0;
				BUFF[i+1]=*p+0x80;
				p++;
				i=i+2;
			}
		}
		else if(WaveCtrlData.bps == 32)     //32bit WAVE, floating-point numbers [(-1) ~ 1] to represent sound
		{
      printf("WaveCtrlData.bps = %d\r\n",WaveCtrlData.bps);
			f_read(&WAV_File,TempBuf,size,(UINT*)&ReadSize);					//Read data
			f=(float*)TempBuf;
			for(i=0;i<ReadSize;)
			{
				//printf("f=%f\r\n",*f);
				sound=0x7FFFFFFF*(*f);
				BUFF[i]=(uint8_t)(sound>>16);
				BUFF[i+1]=(uint8_t)(sound>>24);
				BUFF[i+2]=(uint8_t)(sound);
				BUFF[i+3]=(uint8_t)(sound>>8);
				f++;
				i=i+4;
			}
		}
    else  {
      printf("WaveCtrlData.bps = %d\r\n",WaveCtrlData.bps);
      printf("Error !!\r\n");
    }
	}
	//Signal channelŁ¬adjust to dual channel data for playback
	else
	{
		if(WaveCtrlData.bps==16)
		{
			NeedReadSize=size/2;                                      //Number of bytes to read
			f_read(&WAV_File,TempBuf,NeedReadSize,(UINT*)&ReadSize);	//Read data
			p=TempBuf;
			ReadSize=ReadSize*2;                                      //Size of data after fill
			for(i=0;i<ReadSize;)
			{
				BUFF[i]=p[0];
				BUFF[i+1]=p[1];
				BUFF[i+2]=p[0];
				BUFF[i+3]=p[1];
				i+=4;
				p=p+2;
			}
		}
		else if(WaveCtrlData.bps==24)																	//24-bit audio
		{
			NeedReadSize=(size/8)*3;                                  //Number of bytes to read
			f_read(&WAV_File,TempBuf,NeedReadSize,(UINT*)&ReadSize);  //Read data
			p=TempBuf;
			ReadSize=(ReadSize/3)*8;                                  //Size of data after fill
			for(i=0;i<ReadSize;)
			{
				BUFF[i]=p[1];
				BUFF[i+1]=p[2];
				BUFF[i+2]=0;
				BUFF[i+3]=p[0];
				BUFF[i+4]=p[1];
				BUFF[i+5]=p[2];
				BUFF[i+6]=0;
				BUFF[i+7]=p[0];
				p+=3;
				i+=8;
			}
		}
		else if(WaveCtrlData.bps==8)                                //8-bit audio
		{
			NeedReadSize=size/4;                                      //Number of bytes to read
			f_read(&WAV_File,TempBuf,NeedReadSize,(UINT*)&ReadSize);  //Read data
			p=TempBuf;
			ReadSize=ReadSize*4;                                      //Size of data after fill
			for(i=0;i<ReadSize;)
			{
				BUFF[i]=0;
				BUFF[i+1]=*p+0x80;
				BUFF[i+2]=0;
				BUFF[i+3]=*p+0x80;
				i+=4;
				p++;
			}
		}
		else                                                        //32-bit audio
		{
			NeedReadSize=size/2;                                      //Number of bytes to read
			f_read(&WAV_File,TempBuf,NeedReadSize,(UINT*)&ReadSize);	//Read data
			f=(float*)TempBuf;
			ReadSize=ReadSize*2;                                      //Size of data after fill
			for(i=0;i<ReadSize;)
			{
				sound=0x7FFFFFFF*(*f);
				BUFF[i+4] = BUFF[i]   = (uint8_t)(sound>>16);
				BUFF[i+5] = BUFF[i+1] = (uint8_t)(sound>>24);
				BUFF[i+6] = BUFF[i+2] = (uint8_t)(sound);
				BUFF[i+7] = BUFF[i+3] = (uint8_t)(sound>>8);
				f++;
				i=i+8;
      }
		}
	}
	if(ReadSize<size)   //Data is not enough, supplementary '0'
	{
		EndFileFlag=1;
		for(i=ReadSize;i<size-ReadSize;i++)
			BUFF[i] = 0;
	}
  f_sync(&WAV_File);
	return ReadSize;
}

/**
  * @brief  Check whether keys is pressed.
  * @param  None
  * @retval None
  */
void Key_Control(void)  {
	//Pause/Resume
//	if(PausePlayFlag == 0){//Pull-up mode, press 0
//		HAL_I2S_DMAPause(&hi2s2); //Pause DMA data flow
//	}
//	else
//	{
//		//HAL_I2S_DMAResume(&hi2s2);
//	}



//	if(PausePlayFlag == 0){//Pull-up mode, press 0
//		//HAL_Delay(10);						//Eliminate jitter
//		if(PausePlayFlag == 0){
//			if(Pause_Flag == 0) {
//				HAL_I2S_DMAPause(&hi2s2); //Pause DMA data flow
//				/*********************************************************
//				Please do not use initialization, shutdown, retransmit DMA
//				and other operations during pause,this type of operation
//				may cause noise in the headset
//				*********************************************************/
//				printf("Pause!!\r\n");
//				Pause_Flag = 1;
//			}
//			else  {
//				HAL_I2S_DMAResume(&hi2s2);//Resume DMA data stream
//				printf("Resume!!\r\n");
//				Pause_Flag = 0;
//			}
//		}
//		while(PausePlayFlag == 0);//Release test
//	}
//
//	//next track
//	if(HAL_GPIO_ReadPin(B_GPIO_Port,B_Pin) == 0){
//		HAL_Delay(10);
//		if(HAL_GPIO_ReadPin(B_GPIO_Port,B_Pin) == 0){
//			printf("Next!\r\n");
//			Play_Flag = 0;
//			End_Flag = 1;
//			if(Pause_Flag == 1) {
//				Pause_Flag = 0;
//				HAL_I2S_DMAStop(&hi2s2);
//			}
//		}
//		while(HAL_GPIO_ReadPin(B_GPIO_Port,B_Pin) == 0);
//	}
//
//	//previous piece
//  if(HAL_GPIO_ReadPin(C_GPIO_Port,C_Pin) == 0){
//		HAL_Delay(10);
//		if(HAL_GPIO_ReadPin(C_GPIO_Port,C_Pin) == 0){
//			printf("Last!\r\n");
//			Music_Num -= 2;
//			if(Music_Num < 0)
//				Music_Num += Music_Num_MAX;
//			Play_Flag = 0;
//			End_Flag = 1;
//			if(Pause_Flag == 1) {
//				HAL_I2S_DMAStop(&hi2s2);
//				Pause_Flag = 0;
//			}
//		}
//		while(HAL_GPIO_ReadPin(C_GPIO_Port,C_Pin) == 0);
//	}
}

