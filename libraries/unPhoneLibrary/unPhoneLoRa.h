// unPhoneLoRa.h
// the LoRa board and TTN LoRaWAN
// (a tiny API to mediate between MCCI LMIC/LMIC-node/TTN and the unPhone
// class)

#ifndef UNPHONE_LORA_H
#define UNPHONE_LORA_H

void unphone_lora_setup();                       // initialise lora/ttn
void unphone_lora_loop();                        // service pending transactions
void unphone_lora_send(const char *, va_list);   // ttn message vsprintf style
void unphone_lora_shutdown();                    // shut down LMIC

#endif
