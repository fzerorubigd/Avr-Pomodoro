#ifndef F_CPU //define cpu clock speed if not defined
#define F_CPU 1000000UL    
#endif 
#include <stdlib.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include "lcd_lib.h"

//#define DEBUG

#define _PORT       	PORTC
#define _PIN			PINC
#define _DDR       	 	DDRC

#define SHORT_LED 	   	PC0
#define LONG_LED 	  	PC1
#define WORK_LED	  	PC2

#define SET_PIN 	   	PC3
#define UP_PIN		   	PC4
#define DOWN_PIN	   	PC5	

#define DEBOUNCE_TIME  	25
#define LOOP_DELAY      25

#define BUTTON_INACTIVE 0
#define BUTTON_PRESSED  1


//Some pre-default settings
uint8_t shortBreakTime 	= 5;
uint8_t longBreakTime 	= 10;
uint8_t pomodoTime 		= 15;
uint8_t shortBreakCount = 2;

uint8_t EEMEM  EEPROM_SBT      =  0x10;
uint8_t EEMEM  EEPROM_LBT      =  0x12;
uint8_t EEMEM  EEPROM_PT       =  0x13;
uint8_t EEMEM  EEPROM_SBC      =  0x14;
uint8_t EEMEM  EEPROM_INIT     =  0x15;

//Strings stored in AVR Flash memory
const uint8_t LCDWorkTime[] PROGMEM=            "Its work time :D\0";
const uint8_t LCDShortTime[] PROGMEM=           "Short break   ;)\0";
const uint8_t LCDLongTime[] PROGMEM=            "Long break    ;D\0";
const uint8_t LCDWorkTimeSetting[] PROGMEM=     "Work time:     \0";
const uint8_t LCDShortTimeSetting[] PROGMEM=    "Short Break:\0";
const uint8_t LCDLongTimeSetting[] PROGMEM=     "Long Break:\0";
const uint8_t LCDShortLongTimeSetting[] PROGMEM="Short break cnt:\0";
const uint8_t LCDMinute[] PROGMEM			=	"Minute    \0";
const uint8_t LCDEmpty[] PROGMEM			=	"0=Disable \0";

int8_t sbCounter;

#define POMODO_TIME 0
#define SHORT_BREAK 1
#define LONG_BREAK 2

#define SETTING_SHORT 3
#define SETTING_LONG  4
#define SETTING_POMO  5
#define SETTING_SBCOUNT 6

int8_t currentState ;

volatile int8_t secound;
volatile int8_t minute;


void myItoA(int8_t num,uint8_t buffer[],int8_t bufCount)
{
	memset( buffer, 0 , bufCount );
	itoa(num ,buffer , 10);
/*	if (bufCount == 2) 
	{
		if (!buffer[1])
		{
			buffer[1] = buffer[0];
			buffer[0] = '0';
		}
	}
	if (bufCount == 3)
	{
		while (!buffer[2])
		{
			buffer[2] = buffer[1];
			buffer[1] = buffer[0];
			buffer[0] = '0';
		}
	
	}*/
	int8_t i;
	while (!buffer[bufCount -1] )
	{
		for (i = bufCount -1 ; i >0 ; i--)
			buffer[i] = buffer[i-1];
		buffer[0] = '0';		
	}
}

void readSettings(void)
{
	// B stands for Bita :D
	if (eeprom_read_byte(&EEPROM_INIT) != 0xB)
	{
		shortBreakTime = eeprom_read_byte(&EEPROM_SBT);
		longBreakTime = eeprom_read_byte(&EEPROM_LBT);
		pomodoTime = eeprom_read_byte(&EEPROM_PT);
		shortBreakCount = eeprom_read_byte(&EEPROM_SBC);		
	}
	else
	{
		shortBreakTime = 5;
		longBreakTime = 15;
		pomodoTime = 25;
		shortBreakCount = 4;
		writeSettings();		
	}
	
	if (shortBreakTime <0 || shortBreakTime > 250 || 
		longBreakTime <0 || longBreakTime > 250 ||
		pomodoTime <0 || pomodoTime > 250 ||
		shortBreakCount <0 || shortBreakCount > 10)
	{
		shortBreakTime = 5;
		longBreakTime = 15;
		pomodoTime = 25;
		shortBreakCount = 4;
		writeSettings();		
	}
}

void writeSettings(void)
{
	if (shortBreakTime <0 || shortBreakTime > 250 || 
		longBreakTime <0 || longBreakTime > 250 ||
		pomodoTime <0 || pomodoTime > 250 ||
		shortBreakCount <0 || shortBreakCount > 10)
	{
		shortBreakTime = 5;
		longBreakTime = 15;
		pomodoTime = 25;
		shortBreakCount = 4;		
	}	
	eeprom_write_byte(&EEPROM_SBT,shortBreakTime);
	eeprom_write_byte(&EEPROM_LBT,longBreakTime);
	eeprom_write_byte(&EEPROM_PT,pomodoTime);
	eeprom_write_byte(&EEPROM_SBC,shortBreakCount);
	eeprom_write_byte(&EEPROM_INIT,0xB);
}

void delay_ms(uint16_t ms) 
{
	while ( ms ) 
	{
		_delay_ms(1);
		ms--;
	}
}

void resetTimers(void)
{
	secound = minute = 0;
}

void buzz(int8_t newState)
{
	//TODO
}

void init(void)
{
	int8_t DDRState = _BV(SHORT_LED) | _BV(LONG_LED) | _BV(WORK_LED);
	_DDR   =   DDRState;
	_PORT  =   _BV(SET_PIN)  | _BV(UP_PIN) | _BV(DOWN_PIN);
	
	TCCR1B |= (1 << WGM12); // Configure timer 1 for CTC mode
	
	TIMSK |= (1 << OCIE1A); // Enable CTC interrupt
	
	sei(); //  Enable global interrupts
	
	OCR1A   = 15624; // Set CTC compare value to 1Hz at 1MHz AVR clock, with a prescaler of 64
	
	TCCR1B |= ((1 << CS10) | (1 << CS11)); // Start timer at Fcpu/64

	LCDinit();
	LCDclr();
}

void showProgress(int8_t progress, uint8_t max)
{
	uint8_t buffer[5];
/*	itoa(minute, buffer, 10);
	if (!buffer[1])
		buffer[1]=' ';*/
	myItoA(minute, buffer,3);
	buffer[3] = ':';
	LCDGotoXY(0 , 1);
	LCDstring(buffer,4);
/*	itoa(secound, buffer, 10);
	if (!buffer[1])
		buffer[1]=' ';	*/
	myItoA(secound,buffer,2);
	LCDGotoXY(4 , 1);
	LCDstring(buffer,2);
	LCDGotoXY(6 , 1);
	LCDprogressBar(progress, max , 10);
}

void showTime(uint8_t now)
{
	uint8_t max;
	switch(currentState)
	{
		case POMODO_TIME : 
			max = pomodoTime;
			CopyStringtoLCD(LCDWorkTime,0,0);
			break;
		case SHORT_BREAK :
			max = shortBreakTime;
			CopyStringtoLCD(LCDShortTime,0,0);
			break;
		case LONG_BREAK :
			max = longBreakTime;
			CopyStringtoLCD(LCDLongTime,0,0);
			break;
		default :
			return;
	}
	
	showProgress(now , max);
}


void processState(int8_t now)
{
	switch (currentState)
	{
		case POMODO_TIME :
			if (now >= pomodoTime)
			{
				//Switch to break :D base on sbCounter
				if ( sbCounter >= shortBreakCount || shortBreakCount == 0 )
				{
					//Its a long break!!
					currentState = LONG_BREAK;
					sbCounter = 0;
					resetTimers();
					buzz(LONG_BREAK);
				}
				else
				{
					currentState = SHORT_BREAK;
					sbCounter++;
					resetTimers();
					buzz(SHORT_BREAK);
				}
			}
			else
			{
				_PORT = _BV(SHORT_LED) | _BV(LONG_LED) ;
			}
		break;
		case SHORT_BREAK :
			if (now >= shortBreakTime)
			{
				//Get back to work!
				currentState = POMODO_TIME;
				resetTimers();
				buzz(POMODO_TIME);
			}
			else
			{
				_PORT = _BV(WORK_LED) | _BV(LONG_LED) ;
			}			
		break;
		case LONG_BREAK :
			if (now >= longBreakTime)
			{
				//Get back to work!
				currentState = POMODO_TIME;
				resetTimers();
				buzz(POMODO_TIME);
			}	
			else
			{
				_PORT = _BV(WORK_LED) ;
			}					
		break;
	}	
}

int buttonIsPressed(int8_t bit)
{
	/* the button is pressed when bit is clear */
	if (bit_is_clear(_PIN, bit))
	{
		delay_ms(DEBOUNCE_TIME);
		if (bit_is_clear(_PIN, bit)) return 1;
	}
	return 0;
}

int isSettingMode(void)
{
	static int8_t state = BUTTON_INACTIVE;
	
	if (buttonIsPressed(SET_PIN))
	{
		if (state == BUTTON_INACTIVE)
		{
			state = BUTTON_PRESSED;
		}
	 }
	 else
	 {
		if ( state == BUTTON_PRESSED)
		{
			if (currentState < SETTING_SHORT)
				currentState = SETTING_SHORT;
			else if (currentState < SETTING_SBCOUNT)
				currentState++;
			else
			{
				writeSettings();
				currentState = POMODO_TIME;
				sbCounter = 0;
				resetTimers();
			}				
			state = BUTTON_INACTIVE;
			LCDclr();
		}
	}
	if (currentState > LONG_BREAK )
		return 1;
	else
		return 0;
}

void processSettings(void)
{
	static int8_t upState = BUTTON_INACTIVE;
	static int8_t downState = BUTTON_INACTIVE;
	//Check for up
	if (buttonIsPressed(UP_PIN))
	{
		if (upState == BUTTON_INACTIVE)
		{
			upState = BUTTON_PRESSED;
		}
	}
	else if (upState == BUTTON_PRESSED)
	{
		switch (currentState)
		{
			case SETTING_SHORT : if(shortBreakTime < 250)  shortBreakTime++; break;
			case SETTING_LONG  :  if(longBreakTime < 250) longBreakTime++; break;
			case SETTING_POMO  :  if(pomodoTime < 250) pomodoTime++; break;
			case SETTING_SBCOUNT:  if(shortBreakCount < 50) shortBreakCount++; break;
		}			
		upState = BUTTON_INACTIVE;
	}
	
	if (buttonIsPressed(DOWN_PIN))
	{
		if (downState == BUTTON_INACTIVE)
		{
			downState = BUTTON_PRESSED;
		}
	}
	else if (downState == BUTTON_PRESSED)
	{
		switch (currentState)
		{
			case SETTING_SHORT : if (shortBreakTime > 1 ) shortBreakTime--; break;
			case SETTING_LONG  : if (longBreakTime > 1 )longBreakTime--; break;
			case SETTING_POMO  : if (pomodoTime > 1 )pomodoTime--; break;
			case SETTING_SBCOUNT: if (pomodoTime > 0 )shortBreakCount--; break;
		}			
		downState = BUTTON_INACTIVE;
	}		
}

void showSettings(void)
{
	int8_t showMe = 0;
	switch (currentState)
	{
		case SETTING_SHORT : 
			showMe = shortBreakTime;
			_PORT = _BV(LONG_LED) | _BV(WORK_LED);  
			CopyStringtoLCD(LCDShortTimeSetting,0 , 0);
			break;
		case SETTING_LONG  : 
			showMe = longBreakTime; 
			_PORT = _BV(SHORT_LED) | _BV(WORK_LED) ;
			CopyStringtoLCD(LCDLongTimeSetting,0 , 0);
			break;
		case SETTING_POMO  : 
			showMe = pomodoTime; 	
			_PORT = _BV(SHORT_LED) | _BV(LONG_LED); 
			CopyStringtoLCD(LCDWorkTimeSetting,0 ,0);
			break;
		case SETTING_SBCOUNT: 
			showMe = shortBreakCount;
			_PORT = _BV(WORK_LED) ; 
			CopyStringtoLCD(LCDShortLongTimeSetting,0,0);
			break;
	}
	
	uint8_t buffer[4];
/*	itoa(showMe, buffer, 10);
	if (!buffer[1])
		buffer[1]=' ';
	*/
	myItoA(showMe, buffer,3 )	;	
	LCDGotoXY(0,1);
	LCDstring(buffer , 3);
	if (currentState != SETTING_SBCOUNT)
		CopyStringtoLCD(LCDMinute , 5,1);
	else
		CopyStringtoLCD(LCDEmpty , 5,1);
}

int main (void)
{
   readSettings();
   resetTimers();
   init();
   currentState = POMODO_TIME;
   
   for (;;)
   {
	   if (!isSettingMode())
	   {
#ifdef DEBUG
			processState(secound);
			showTime(secound);
#else
			processState(minute);
			showTime(minute);
#endif
	   }
	   else
	   {
			processSettings();
			showSettings();
	   }
	   delay_ms(LOOP_DELAY);
   }
}

ISR(TIMER1_COMPA_vect)
{
	secound++; 
	if (secound >= 60)
	{
		secound = 0;
		minute++;
	}
}
