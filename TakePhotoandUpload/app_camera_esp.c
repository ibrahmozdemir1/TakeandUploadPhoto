/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "app_camera_esp.h"
#include "esp_camera.h"


bool setup_camera(framesize_t frameSize, pixformat_t pixFormat) 
{
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = pixFormat;
    config.frame_size = frameSize;
    config.jpeg_quality = 30;
    config.fb_count = 1;
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Kamera başlatma 0x%x hatasıyla başarısız oldu", err);
      return -1;
    }
    //delay (500);
    sensor_t *sensor = esp_camera_sensor_get();
    sensor->set_framesize(sensor, frameSize);
    sensor->set_pixformat(sensor, pixFormat);
    return 0;
}


void resetCam(byte tick){
 esp_camera_deinit();
 digitalWrite(PWDN_GPIO_NUM,LOW);
 delay (1);
 digitalWrite(PWDN_GPIO_NUM,HIGH);
 delay (tick);
} 
