################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (9-2020-q2-update)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/Touch/Touch.c 

OBJS += \
./Drivers/Touch/Touch.o 

C_DEPS += \
./Drivers/Touch/Touch.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/Touch/%.o: ../Drivers/Touch/%.c Drivers/Touch/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F303xE -c -I"F:/AudioSTM32/ComplexProjectAudioLCD/Middlewares/Third_Party/FatFs/src" -I"F:/AudioSTM32/ComplexProjectAudioLCD/FATFS/App" -I"F:/AudioSTM32/ComplexProjectAudioLCD/FATFS/Target" -I"F:/AudioSTM32/ComplexProjectAudioLCD/Drivers" -I../Core/Inc -I../Drivers/STM32F3xx_HAL_Driver/Inc -I../Drivers/STM32F3xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F3xx/Include -I../Drivers/CMSIS/Include -O2 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

