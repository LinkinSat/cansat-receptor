#include <LoRa.h>
#include <SPI.h>

//Pines LoRa
#define LORA_SS 18
#define LORA_RST 14
#define LORA_DIO0 26
#define LORA_FREQ 433E6

void setup() {
  Serial.begin(115200);

  Serial.println("Se ha iniciado la placa receptora LoRa");

  SPI.begin(5,19,27);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if(!LoRa.begin(LORA_FREQ)){
    Serial.println("LoRa ha fallado satisfactoriamente");
  }else {
    Serial.println("LoRa iniciado correctamente");
  }

}

void loop() {
  int packetSize = LoRa.parsePacket();
  if(packetSize){
    Serial.print("Se ha recibido un paquete ");

    while (LoRa.available()){
      Serial.print((char) LoRa.read());
    }
    Serial.println();
  }



}
