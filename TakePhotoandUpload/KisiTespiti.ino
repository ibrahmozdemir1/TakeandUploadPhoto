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

/*
 *  KisiTespiti örneği,
 *  TensorFlowLite yapay zeka kütüphanesinin kullanıldığı bu örnekte 
 *  Kamera görüntüleri seri terminalde çizdirmektedir.
 *  
 *  Deneyap Mini Kartın kamera bağlantısı olmadığı için bu örnek Deneyap Mini Kartını desteklememektedir.
 * 
 */

const char* ssid = "deneyap";
const char* password = "12345678";

#define API_KEY "AIzaSyC8R1yJDxImZSbuYf6kbAzj7l2KusZoUfo"
#define USER_EMAIL "yeter@gmail.com"
#define USER_PASSWORD "159753*"
#define STORAGE_BUCKET_ID "esp32cam-461c5.appspot.com"
#define FILE_PHOTO "/data/photo.jpg"

#include <TensorFlowLite_ESP32.h>
#include "esp_camera.h"
#include "main_functions.h"
#include "detection_responder.h"
#include "image_provider.h"
#include "model_settings.h"
#include "person_detect_model_data.h"
#include "tensorflow/lite/experimental/micro/kernels/micro_ops.h"
#include "tensorflow/lite/experimental/micro/micro_error_reporter.h"
#include "tensorflow/lite/experimental/micro/micro_interpreter.h"
#include "tensorflow/lite/experimental/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/version.h"
#include <WiFi.h>
#include "Base64.h"
#include "nvs_flash.h"
#include "Arduino.h"
#include "driver/rtc_io.h"
#include <SPIFFS.h>
#include <FS.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include "soc/soc.h"           // Disable brownout problems
#include "soc/rtc_cntl_reg.h"  // Disable brownout problems
#include "driver/rtc_io.h"
#include "app_camera_esp.h"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig configF;


boolean takeNewPhoto = true;
bool taskCompleted = false;
int i = 0;
String filephoto = "/data/"+String(i)+"photo.jpg";
bool okey = true;

bool checkPhoto( fs::FS &fs ) {
  File f_pic = fs.open( FILE_PHOTO );
  unsigned int pic_sz = f_pic.size();
  return ( pic_sz > 100 );
}


namespace {
tflite::ErrorReporter* error_reporter = nullptr;
const tflite::Model* model = nullptr;
tflite::MicroInterpreter* interpreter = nullptr;
TfLiteTensor* input = nullptr;

// Giriş, çıkış ve ara diziler için kullanılacak bir bellek alanı
constexpr int kTensorArenaSize = 70 * 950;
static uint8_t tensor_arena[kTensorArenaSize];
}  // namespace


void capturePhotoSaveSpiffs(void) {
  resetCam(100);
  delay(2000);
  int cam = setup_camera(FRAMESIZE_XGA,PIXFORMAT_JPEG);
  delay(1000);
  if(cam == 0){
    camera_fb_t * fb = NULL; // pointer
    bool ok = 0; // Boolean indicating if the picture has been taken correctly
    do {
    // Take a photo with the camera
    Serial.println("Taking a photo...");

    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      return;
    }
    // Photo file name
    Serial.printf("Picture file name: %s\n", filephoto);
    File file = SPIFFS.open(filephoto, FILE_WRITE);
    // Insert the data in the photo file
    if (!file) {
      Serial.println("Failed to open file in writing mode");
    }
    else {
      file.write(fb->buf, fb->len); // payload (image), payload length
      Serial.print("The picture has been saved in ");
      Serial.print(filephoto);
      Serial.print(" - Size: ");
      Serial.print(file.size());
      Serial.println(" bytes");
      i++;
    }
    // Close the file
    file.close();
    esp_camera_fb_return(fb);

    // check if file has been correctly saved in SPIFFS
    ok = checkPhoto(SPIFFS);
   } while ( !ok );
  }else{
    Serial.println("Kamera Resetlenmedi.");
  }
}



void uploadFirebase(){  
   capturePhotoSaveSpiffs();
   delay(1000);
   if(Firebase.ready()){
    Serial.println("Upload Picture...");
    delay(500);
    //MIME type should be valid to avoid the download problem.
    //The file systems for flash and SD/SDMMC can be changed in FirebaseFS.h.
    if (Firebase.Storage.upload(&fbdo, STORAGE_BUCKET_ID, filephoto, mem_storage_type_flash, filephoto, "image/jpeg")){
      Serial.printf("\nDownload URL: %s\n", fbdo.downloadURL().c_str());
      filephoto = "/data/"+String(i)+"photo.jpg";
    }
    else{
      Serial.println(fbdo.errorReason());
      delay(5000);
    }
   }
   resetCam(100);
   delay(30000);
   okey = true;
}

void initWiFi(){
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
}

void initSPIFFS(){
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    ESP.restart();
  }
  else{
    delay(500);
    Serial.println("SPIFFS mounted successfully");
  }
}

// Arduino uyumluluğu için bu fonksiyonun ismi önemlidir.
void setup() {
  Serial.begin(115200);

  Serial.println("ssid: " + (String)ssid);
  Serial.println("password: " + (String)password);
    
  // Set up logging. Google style is to avoid globals or statics because of
  // lifetime uncertainty, but since this has a trivial destructor it's okay.
  // NOLINTNEXTLINE(çalışma zamanı-global-değişkenler)
  static tflite::MicroErrorReporter micro_error_reporter;
  error_reporter = &micro_error_reporter;

  // Modeli kullanılabilir bir veri yapısına eşleyin. Bu herhangi bir şey içermiyor.
  // kopyalama veya ayrıştırma, çok hafif bir işlemdir
  model = tflite::GetModel(g_person_detect_model_data);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    error_reporter->Report(
        "Sağlanan model şema sürümü %d eşit değil"
        "desteklenen sürüm %d.",
        model->version(), TFLITE_SCHEMA_VERSION);
    return;
  }

  // Yalnızca ihtiyacımız olan işlem uygulamalarını içeri çekin
  // Bu grafiğin ihtiyaç duyduğu tüm işlemlerin tüm listesine dayanır
  // AllOpsResolver'ı kullanmak daha kolay bir yaklaşımdır, ancak bu
  // olmayan op uygulamaları için kod alanında bazı cezalara maruz kalır
  // bu grafik tarafında gerekli
  // tflite::ops::micro::AllOpsResolver çözümleyici;
  // NOLINTNEXTLINE(çalışma zamanı-global-değişkenler)
  static tflite::MicroMutableOpResolver micro_mutable_op_resolver;
  micro_mutable_op_resolver.AddBuiltin(
      tflite::BuiltinOperator_DEPTHWISE_CONV_2D,
      tflite::ops::micro::Register_DEPTHWISE_CONV_2D());
  micro_mutable_op_resolver.AddBuiltin(tflite::BuiltinOperator_CONV_2D,
                                       tflite::ops::micro::Register_CONV_2D());
  micro_mutable_op_resolver.AddBuiltin(
      tflite::BuiltinOperator_AVERAGE_POOL_2D,
      tflite::ops::micro::Register_AVERAGE_POOL_2D());

  // Modeli çalıştırmak için bir yorumlayıcı oluşturun
  static tflite::MicroInterpreter static_interpreter(
      model, micro_mutable_op_resolver, tensor_arena, kTensorArenaSize,
      error_reporter);
  interpreter = &static_interpreter;

  // Modelin tensörleri için tensor_arena'dan bellek ayırın
  TfLiteStatus allocate_status = interpreter->AllocateTensors();
  if (allocate_status != kTfLiteOk) {
    error_reporter->Report("AllocateTensors() hatası");
    return;
  }

  // Modelin girişi için kullanılacak bellek alanı hakkında bilgi alın
  input = interpreter->input(0);

  
  initWiFi();
  initSPIFFS();
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  //Firebase
  // Assign the api key
  configF.api_key = API_KEY;
  //Assign the user sign in credentials
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  //Assign the callback function for the long running token generation task
  configF.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h
  //initNetwork();
}

// Arduino uyumluluğu için bu fonksiyon önemlidir
void loop() {
  // Kameradan görüntü alın
  Serial.println(okey);
  if (kTfLiteOk != GetImage(error_reporter, kNumCols, kNumRows, kNumChannels,
                            input->data.uint8,okey)) {
    error_reporter->Report("Görüntü yakalama başarısız oldu");
  }
  okey = false;


  // Modeli bu girdi üzerinde çalıştırın ve başarılı olduğundan emin olun
  if (kTfLiteOk != interpreter->Invoke()) {
    error_reporter->Report("Çağrı başarısız");
  }

  

  TfLiteTensor* output = interpreter->output(0);
  

  // Çıkarım sonuçlarını işleyin
  uint8_t person_score = output->data.uint8[kPersonIndex];
  uint8_t no_person_score = output->data.uint8[kNotAPersonIndex];
  size_t image_size = output -> data.uint8[kMaxImageSize];
  RespondToDetection(error_reporter, person_score, no_person_score);
  Serial.println(ESP.getFreeHeap());
  if(person_score > no_person_score){
    Firebase.begin(&configF, &auth);
    Firebase.reconnectWiFi(true);
    uploadFirebase();
  }
}
