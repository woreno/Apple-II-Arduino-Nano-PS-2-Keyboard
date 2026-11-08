// este programa le o teclado PS/2 e 
// emite os bytes ASCII
// PLACA: Arduino Nano "OLD BOOTLOADER"
//#include <EEPROM.h>
//                ______________________
//               |                      |
//          _____| Teclado Externo PS/2 |
//			    |    |______________________|
//      ____|____
//			|       |             _____	    _______     __________
//			|       |-------------|   |-----|      |----| Teclado |
//			|Arduino|  Databus    |244|     |Socket|    | Apple// |
//			|  Nano |--------   --|___|-----|______|----|_________|
//			|_______|		|  |
//             _____|__|__________________________
//                |      |
//                |Socket|   Motherboard Apple//
//                |______|
//
//     activateDatabus(): desliga o 244 e deixa o Arduino a controlar
//                        as linhas do databus. O Arduino apanha os
//                        bits serie do teclado externo PS/2 e 
//                        transforma para caracteres ASCII no databus.
//                         ** teclado Externo PS/2 fica ativo
//
//   deactivateDatabus(): liga o 244 e este controla as linhas 
 //                       do databus; o Arduino se afasta do databus.
//                         ** o teclado original do Apple// fica ativo
//                         
//   cada vez que uma tecla e' premida no teclado externo PS/2
//   e' lhe passado a vez com activateDatabus(). 
//   se nenhuma tecla for premida neste, por um breve tempo,
//   e' lhe passada a vez ao teclado original do Apple// 
//  
//   teclado PS/2: a cada descer do sinal KBDATA os bits
//   sao apanhados da linha KBDATA e guardados num array. 
//   o loop transforma esses bits para ASCII e emite para 
//   o databus, se colecionados em grupos de 11 bits.
//
//   falta ajustar a typematic rate... OK
//   <!> crasha de vez enquando e é preciso fazer RESET ao arduino
//       parece que é quando se golpeam as teclas de seguida
//       o se tecla mais bruscamente

#define ENABLE_APPLE_KEYB       
#define ATOMIC_DELAY           __asm__ __volatile__ ("nop");
#define EMIT_DELAY delay(3);     // intervalo entre letras
#define INIT_DELAY delay(140);   // espera pelo interpretador de comandos do A2
#define IDLE_MAX_COUNT                620   
#define DATABUS_SETTLE_DELAY          ATOMIC_DELAY ATOMIC_DELAY ATOMIC_DELAY ATOMIC_DELAY
#define STROBE_HI_DELAY               ATOMIC_DELAY ATOMIC_DELAY ATOMIC_DELAY
#define STROBE_LO_DELAY               delay(40);
#define AFTER_SCANCODE_EMISSION_DELAY delay(10);
#define TYPEMATIC_DELAY_MS            300
       
 // auxiliares
//#define DEBUG_ON
//#define DEBUG_JUST_KEY
//#define DEBUG_PRINT_KEY 
//#define DEBUG_DATABUS   
//#define DEBUG_CLOCKS
//#define DEBUG_PRINT_SCANCODES

#define W0   void
#define W1   bool
#define W8   uint8_t
#define SW8  char
#define W32  uint32_t
#define SW16 int16_t
#define W16  uint16_t

#include "scancodes.h"
// pinos --------------------------------------------------------
#define PIN_LED            13	//LED ... 1 pino da esquerda
#define PIN_KBCLK          3
#define PIN_KBDATA         4
#define PIN_TEST           A1
#define PIN_OE_            A0
#define PIN_K1             5
#define PIN_K0             6
#define PIN_K3             7
#define PIN_K2             8
#define PIN_K6             9
#define PIN_K4             10
#define PIN_K5             11
#define PIN_STR            12
//#define KBCLK_IS_LOW  (0==((PORTD >> 4)&1))
//#define KBDATA        ((PORTD >> 3)&1)  
#define KBCLK_IS_LOW    (digitalRead(PIN_KBCLK)==LOW)
#define KBCLK_IS_HI     (digitalRead(PIN_KBCLK)==HIGH)
#define KBDATA           digitalRead(PIN_KBDATA)
#define PIN_OE_IS_HIGH   digitalRead(PIN_OE_)

// forward -----------------------------------------------------
void(* resetFunc) (void) = 0;
  
void blink(W16 n_)
{
  for (W16 i=0; i< n_; i++)
  {
    digitalWrite(PIN_OE_, HIGH);
    delay(100);
    digitalWrite(PIN_OE_, LOW);
    delay(100);
  }
}
// variavel ----------------------------------------------------
#define BITBUFSZ 512
W1  bitbuf[BITBUFSZ];
volatile W16  bitbufit; 
volatile W8   databusIsActive;
volatile W32  idleCounter;
long int keyTime = 0;

  
enum KEYSTATE { STATE_NORMAL=0, STATE_SHIFTED=1, STATE_ALTED=2};
KEYSTATE keyState;
volatile W8   lastScancode = 0, lastIsBreak;
SW8  dgStr[32];
W8   data, biti;
const W8 fkeyArr[] PROGMEM = { 9, 0, 5, 3, 1, 2, 12, 0, 10, 8, 6, 4 }; // teclas de funcao (algumas)
const W8 STRING_INSTALLED[] PROGMEM = "SUPORTE TECLADO PS/2 INSTALADO";
const W8 STRING_KEYB_NOT_DETECTED[] PROGMEM = "TECLADO NAO DETETADO";
const W8 STRING_LOAD[] PROGMEM = "LOAD";
const W8 STRING_TXT[] PROGMEM = "TEXT";
const W8 STRING_HOME[] PROGMEM = "HOME";
const W8 STRING_LIST[] PROGMEM = "LIST";

void emptyBuffer() {
  lastIsBreak = 0; 
  bitbufit = 0;
}
void activateDatabus(){
  // poem o teclado externo a funcionar
  if (databusIsActive) return;
  digitalWrite (PIN_LED,  HIGH); 
  pinMode (PIN_K1,   OUTPUT); 
  pinMode (PIN_K0,   OUTPUT); 
  pinMode (PIN_K3,   OUTPUT); 
  pinMode (PIN_K2,   OUTPUT); 
  pinMode (PIN_K6,   OUTPUT); 
  pinMode (PIN_K4,   OUTPUT); 
  pinMode (PIN_K4,   OUTPUT); 
  pinMode (PIN_K5,   OUTPUT); 
  pinMode (PIN_STR,  OUTPUT); 
  digitalWrite (PIN_OE_,  HIGH); 
  databusIsActive = true;
  emptyBuffer();
  keyState = STATE_NORMAL;
  #ifdef DEBUG_DATABUS
    Serial.print("\r\nDB ARDUINO\r\n");
  #endif
}
void deactivateDatabus(){
  // poem o teclado do AppleII a funcionar
  if (!databusIsActive) return;
  digitalWrite (PIN_LED,  LOW); 
  pinMode (PIN_K1,   INPUT); 
  pinMode (PIN_K0,   INPUT); 
  pinMode (PIN_K3,   INPUT); 
  pinMode (PIN_K2,   INPUT); 
  pinMode (PIN_K6,   INPUT); 
  pinMode (PIN_K4,   INPUT); 
  pinMode (PIN_K4,   INPUT); 
  pinMode (PIN_K5,   INPUT); 
  databusIsActive = false;
  digitalWrite (PIN_OE_,  LOW); 
  pinMode (PIN_STR,  INPUT); 
  emptyBuffer();
  #ifdef DEBUG_DATABUS
    Serial.print("\r\nDB A2\n");
  #endif
}
void emitToAppleII(W8 ascii_, W1 strobe_=true){

  if (!databusIsActive)    activateDatabus();
  // escrever repondo a ordem 
  digitalWrite(PIN_K0,  ascii_&1);
  digitalWrite(PIN_K1, (ascii_>>1)&1);
  digitalWrite(PIN_K2, (ascii_>>2)&1);
  digitalWrite(PIN_K3, (ascii_>>3)&1);
  digitalWrite(PIN_K4, (ascii_>>4)&1);
  digitalWrite(PIN_K5, (ascii_>>5)&1);
  digitalWrite(PIN_K6, (ascii_>>6)&1);
  DATABUS_SETTLE_DELAY
  if (strobe_)
  {
    digitalWrite(PIN_STR, HIGH);
    STROBE_HI_DELAY
    digitalWrite(PIN_STR, LOW);
    STROBE_LO_DELAY
  }
}
void emitToAppleIIStrAndClear(const SW8* str_, W8 clear_=0, W16 delay_=0){


  #ifdef DEBUG_ON
    Serial.print(str_);
    return;
  #endif
  
  W16 strCnt=0; 
  do
  {
    const W8 ch = pgm_read_byte(str_+strCnt);
    if (ch==0) break;
    emitToAppleII(ch);
    strCnt++;
  }while(1);
  
  if (clear_==1)
  {
    delay(delay_);
    for (W16 i=0; i<strCnt; i++)
    {
        EMIT_DELAY  
        emitToAppleII(8); 
        EMIT_DELAY  
        emitToAppleII(' ');
        EMIT_DELAY  
        emitToAppleII(8); 
    }
  }else
  {
      EMIT_DELAY  
      emitToAppleII(13);
   }
}
void emitFunctionKey(W8 fkey_){
  #ifdef DEBUG_PRINT_KEY
    sprintf(dgStr, " -- F%i' ", fkey_);
    Serial.print(dgStr);
  #endif
  switch(fkey_)
  {
    case 1:
        emitToAppleIIStrAndClear(STRING_LOAD,2);
    break;
    case 2:
        emitToAppleIIStrAndClear(STRING_TXT,2);
    break;
    case 3:
        emitToAppleIIStrAndClear(STRING_HOME,2);
    break;
    case 4:
        emitToAppleIIStrAndClear(STRING_LIST,2);
    break;
  }
}

void kbHandler(){ 
  if (bitbufit==BITBUFSZ) {  emptyBuffer();delay(300); emptyBuffer();return; }
  bitbuf[bitbufit++]=KBDATA;
}
void emitScancode(W8 scancode_){

  if ((lastScancode!=0) && (lastScancode == scancode_))
  {
    if ((millis() - keyTime) < TYPEMATIC_DELAY_MS)
    {
      keyTime = millis(); 
      return;
    }
  }
  keyTime = millis(); 
  lastScancode = scancode_;
  
  if (scancode_==0x66)
  {
      emitToAppleII(8); 
      emitToAppleII(' '); 
      emitToAppleII(8);
      return;
  }

  if (scancode_>127)
  {
    #ifdef DEBUG_PRINT_KEY 
      sprintf(dgStr, "\r\n -- sc:%02X st:%1u", scancode_, keyState);
      Serial.print(dgStr);
    #endif 
    return;
  }
  
  const W8 ascii = pgm_read_byte(arr+keyState*128+(0x7F & scancode_)) ;   
 
  #ifdef DEBUG_PRINT_KEY 
    const W8 ch = (ascii<32)? ' ': ascii;
    sprintf(dgStr, "\r\n -- sc:%02X s:%1u ascii:0x%02X '%c' ", scancode_, keyState, ascii, ch);
    Serial.print(dgStr);
  #endif 

  #ifdef DEBUG_JUST_KEY
   sprintf(dgStr, "'%c' ",ascii);
    Serial.print(dgStr);
  #endif

  if (ascii) 
  { emitToAppleII(ascii); }
  else if (scancode_==0x78)
    emitFunctionKey(11);
  else if (scancode_==0x83)
    emitFunctionKey(7);
  else if ((scancode_!=2) && (scancode_!=8) && (scancode_>0 && scancode_<0xD))
      emitFunctionKey(pgm_read_byte(fkeyArr+scancode_-1 )); 
  AFTER_SCANCODE_EMISSION_DELAY 

}
void setup() {

      // o arduino controla se o teclado do AppleII está ligado
      // diretamente ao bus do teclado, ou se o afasta por este pino
  pinMode (PIN_STR,     OUTPUT); 
  pinMode (PIN_OE_,     OUTPUT); 
  digitalWrite(PIN_OE_, HIGH);
  digitalWrite(PIN_OE_, LOW);
  
      // estes pinos são da porta PS/2, por ainda sempre de entrada
      // (porque nao sao enviados comandos ao teclado)
 
  pinMode (PIN_TEST,   INPUT);
  pinMode (PIN_KBCLK,   INPUT);
  pinMode (PIN_KBDATA,  INPUT);   
  databusIsActive = false;
  keyState = STATE_NORMAL;
  idleCounter = 0;
  activateDatabus();
}

void test()
{
  W8 key = 0;
  while(1)
  {
    delay(500);    
    //while( digitalRead(PIN_TEST)==LOW ) {}
    digitalWrite(PIN_LED, 1);
    delay(200);    
   // while( digitalRead(PIN_TEST)==HIGH) {}
    digitalWrite(PIN_LED, 0);
    emitToAppleII(key);
    key++;
  }  
}


void loop() {  

  if (digitalRead(PIN_TEST)==LOW)
  {
    test();  
  }
  #ifdef DEBUG_ON
      Serial.begin(9600); 
  #endif
  
  // espera que a linha de comando do monitor do Apple//
    // esteja ativa
  #ifndef DEBUG_ON
     INIT_DELAY
  #endif
    emitToAppleIIStrAndClear(STRING_INSTALLED,1,1000);
    blink(5);
    if (digitalRead(PIN_KBCLK)==LOW)
    {
      emitToAppleIIStrAndClear(STRING_KEYB_NOT_DETECTED,1,1000);
      blink(3);
    }
    else
    {
      #ifdef DEBUG_ON
       emitToAppleIIStrAndClear(": OK");
      #endif
     }
 
  
  emptyBuffer();
  attachInterrupt(digitalPinToInterrupt(PIN_KBCLK), kbHandler, FALLING);
  sei(); 
  deactivateDatabus();
 
   while(1)
   { 
    delay(10);   
     cli();
     #ifdef ENABLE_APPLE_KEYB
     // se nao houve interrupcoes e a linha está estável
     // por muito tempo passar o controlo ao Apple II
     if (digitalRead(PIN_KBCLK) && bitbufit==0)
     {  
        // se passou muito tempo passar controlo ao AppleII 
        if (idleCounter++>IDLE_MAX_COUNT)
        {
          deactivateDatabus();
          idleCounter = 0;
        }
      }
   #endif

   // se o buffer tem N clocks divisiveis
   if ((bitbufit>0) && (bitbufit%11==0))  { 
      #ifdef DEBUG_CLOCKS
        Serial.print("\r\n NBits:");
        Serial.print(bitbufit);
      #endif
      for (W16 k=1; k<bitbufit; )
      {
          data = 0;
          for (W16 j=0; j<8; j++, k++)
            data|=((bitbuf[k])<<j);
          
          #ifdef DEBUG_PRINT_SCANCODES
            sprintf(dgStr, "\r\n 0x%02X ", data);
            Serial.print(dgStr); 
          #endif

          // isto nao é bem assim...
            switch (data)
            { 
              case 0xF0: 
                  lastIsBreak=1; 
                  #ifdef DEBUG_PRINT_SCANCODES
                    Serial.print("break");
                  #endif
              break;
              case 0xE0: 
                  #ifdef DEBUG_PRINT_SCANCODES
                    Serial.print("extended");
                  #endif
                  goto skip;
              break;
              case 0x11: 
                  keyState = STATE_ALTED;
                  if (lastIsBreak)
                  {
                    keyState = STATE_NORMAL;
                    lastIsBreak = 0;
                  }
                  #ifdef DEBUG_PRINT_SCANCODES
                    Serial.print("alt");
                  #endif
              break;
              case 0x59:  // right shift
              case 0x12:  // left shift
                  keyState = STATE_SHIFTED;
                  if (lastIsBreak)
                  {
                    keyState = STATE_NORMAL;
                    lastIsBreak = 0;
                  }
                  #ifdef DEBUG_PRINT_SCANCODES
                    Serial.print("shift");
                  #endif
              break;
              default:
                if (lastIsBreak==0) 
                {
                  #ifdef DEBUG_PRINT_SCANCODES
                      Serial.print("make");
                  #endif
                  emitScancode(data); 
                  idleCounter = 0;
                }
                else 
                {
                  goto skip;
                  lastIsBreak=0;
                }
              break;
           }
           k+=3;
      } 
      skip:
      emptyBuffer();
  } 
  sei();

   }
}
 
