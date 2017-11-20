#include <ecog1.h>
#include <stdio.h>
#include "driver_lib.h"
#include "util.h"

// PIOA outputs for LEDs (on port K)
#define row1 0
#define row2 1
#define row3 2
#define row4 3
#define col1 4
#define col2 5
#define col3 6
#define extSounder 7

//keypad decode
#define key_0 7
#define key_1 0
#define key_2 4
#define key_3 8
#define key_4 1
#define key_5 5
#define key_6 9
#define key_7 2
#define key_8 6
#define key_9 10
#define key_ast 3
#define key_hash 11

/******************************************************************************
Declaration of static functions.
******************************************************************************/

static void alarm_state(int);
static void delay_5ms(void);
static int countdown(void);
static void dBounce(void);
static void setKey(void);

/******************************************************************************
Declaration of non-static functions.
******************************************************************************/

void alarmConfig(void);
int testCode(void);
char getCode(void);
char getKey(void);
static void delay(int length);

/******************************************************************************
Module global variables.
******************************************************************************/

static unsigned int do_pattern;
static char code[5] = "1234"; // The password
static char keypad_code[5] = "____";
static int exitTime = 30*4; // exit period 30 seconds
static int entryTime = 30*4; //entry period 30 seconds
static int alarmTime = 5*60*4; //alarm sounder period 5 minutes
static unsigned int buzzer_cnt;
static unsigned int buzzer250ms;
static unsigned int buzzer500ms;
static unsigned int buzzeralarm;
static unsigned int buzzer;
static int timer = 0;

/****************************************************************************** 


Methods Start Here.
******************************************************************************/

/*=============================================================================
Cyan Technology Limited

FILE - main.c

DESCRIPTION
    Initiates the Finite State Machine

=============================================================================*/

int main(int argc, char* argv[]) {
	
	//locals
	int sw;
	
	//INIT
	alarmConfig();
	
	//Output to uart
	printf("\n\n\r              ALARM LAB\n\r (c)Copyright University of Essex 2006 \n\r            Author: Steve Wood\n\r");
    
    // Start tick timer and enable interrupt
    rg.tim.ctrl_en = TIM_CTRL_EN_CNT1_CNT_MASK;
    rg.tim.int_en1 = TIM_INT_EN1_CNT1_EXP_MASK;
	
    //update leds from switches
	sw = pio_in(PIOA);	// input from PIOA,
	sw = sw & 0xFF00;   // mask switch inputs from all IO, 
	sw = sw >> 8;       // shift to output IO
	pio_out(PIOB, sw);  // and output on PIOB
	
	setKey(); // user supplies own key code
    while (1)
    {
        if (1 == do_pattern)
        {
			//update leds from switches
			sw = pio_in(PIOA);
			sw = (sw & 0xFF00) >> 8;
			sw = (sw & 0x007);
			pio_out(PIOB, sw); 
			sw = (sw & 0x007); // mask required switches (only first 3 switches required)
			do_pattern=0;
        }
		alarm_state(sw); // FSM routine
    }
    return (0);
}

/**********************************************************************************
Finite State Machine for Alarm
**********************************************************************************/

enum State {exit, un_set, set, entry, alarm, report}; // take values of 0,1,2,3,4
int col = 7; //code display column inital (7-10)

static void alarm_state(int s)
{
	int button;
	int zone; //zone alarm triggered in
	int fail;
	int i; // generic temp counter value

	static state = un_set; // Initial state
	pio_out(PIOB, s | state << 4); 	// takes the enum state value and bit shifts them
									// ready to output onto the leds along with switch indicators
	switch(state)
	{
/*--------------------------------------------------------------------------------
	UN_SET STATE (STATE 0)
		un_set led enabled
		Internal sounder disabledabled 
		Sensors not monitored.	
	COMMENTS
---------------------------------------------------------------------------------*/	
	case un_set:
		printf("\rUN-SET MODE        ");
	
		button = getKey();
	
		if (isdigit(button) && (col < 11))
		{
			keypad_code[col-7] = button;
			lcd_xy(7,2);	lcd_puts(keypad_code);
			col += 1;
		}
		if((button == '#') && (col > 7)) //Delete and replace the character with '_'
		{
			col -= 1;
			keypad_code[col-7] = '_';
			lcd_xy(7,2);	lcd_puts(keypad_code);
		}
		if(button == '*')
		{
			if(testCode() == 4)
			{
				fail = 0;
				lcd_rst();	lcd_puts("EXIT MODE");
				lcd_xy(1, 2);	lcd_puts("Code: ____");
				keypad_code[0] = '_';
				keypad_code[1] = '_';
				keypad_code[2] = '_';
				keypad_code[3] = '_';
				col = 7;
				fail = 0;
				timer=0;
				state = exit;
			}
			else
			{
				++fail;
				
				timer=0; //reset timer
				buzzer500ms=1; //enable buzzer for 500ms
				
				if(fail == 3) //3 incorrect codes
				{
					lcd_rst();    lcd_puts("ALARM MODE");
					lcd_xy(1, 2);	lcd_puts("Code: ____");
					keypad_code[0] = '_';
					keypad_code[1] = '_';
					keypad_code[2] = '_';
					keypad_code[3] = '_';
					col = 7;
					fail = 0;
					timer=0;
					fd.ssm.clk_en.pwm1 = 1;
					state = alarm;
				}
			}
		}

	break;

/*--------------------------------------------------------------------------------
	EXIT STATE (STATE 1)
		un_set led flashes

	break;

/*--------------------------------------------------------------------------------
	EXIT STATE (STATE 1)
		un_set led flashes
		Internal sounder enabled for 250msec every 500msec. 
--------------------------------------------------------------------------------*/	
	case exit:
		printf("\rEXIT MODE          ");
		
		if(timer<exitTime)
		{
			buzzer250ms=1;
			if (s>1)
			{
				lcd_rst();    lcd_puts("ALARM MODE");
				lcd_xy(1, 2);	lcd_puts("Code: ____");
				keypad_code[0] = '_';
				keypad_code[1] = '_';
				keypad_code[2] = '_';
				keypad_code[3] = '_';
				col = 7;
				buzzer250ms=0;
				buzzer500ms=0;
				fd.ssm.clk_dis.pwm1 = 1;
				state=alarm;
			}
			
			//allow user to enter code to go to unset state//
			button = getKey();
			if (isdigit(button) && (col < 11))
			{
				keypad_code[col-7] = button;
				lcd_xy(7,2);	lcd_puts(keypad_code);
				col += 1;
			}
			if((button == '#') && (col > 7)) //Delete and replace the character with '_'
			{
				col -= 1;
				keypad_code[col-7] = '_';
				lcd_xy(7,2);	lcd_puts(keypad_code);
			}
			if(button == '*')
			{
				if(testCode() == 4)
				{
					fail = 0;
					lcd_rst();	lcd_puts("UN-SET MODE");
					lcd_xy(1, 2);	lcd_puts("Code: ____");
					keypad_code[0] = '_';
					keypad_code[1] = '_';
					keypad_code[2] = '_';
					keypad_code[3] = '_';
					col = 7;
					fail = 0;
					timer=0;
					buzzer250ms=0;
					fd.ssm.clk_dis.pwm1 = 1;
					state = un_set;
				}
				else
				{
					++fail;
					
					if(fail == 4) //4 incorrect codes
					{
						lcd_rst();    lcd_puts("ALARM MODE");
						lcd_xy(1, 2);	lcd_puts("Code: ____");
						keypad_code[0] = '_';
						keypad_code[1] = '_';
						keypad_code[2] = '_';
						keypad_code[3] = '_';
						col = 7;
						fail = 0;
						timer=0;
						buzzer250ms=0;
						fd.ssm.clk_dis.pwm1 = 1;
						//fd.ssm.clk_en.pwm1 = 1;
						state = alarm;
					}
				}
			}
			//allow user to enter code to go to unset state//
		}
		else
		{
			lcd_rst(); lcd_puts("SET MODE           ");
			buzzer250ms=0;
			fd.ssm.clk_dis.pwm1 = 1;
			state=set;
		}

	break;
		
/*--------------------------------------------------------------------------------
	SET STATE (STATE 2) 
		set led enabled
		Internal sounder disabled.
---------------------------------------------------------------------------------*/	
	case set:
		printf("\rSET MODE           ");
	
		if (s==1)
		{
			lcd_rst(); lcd_puts("ENTRY MODE        ");
			lcd_xy(1, 2);	lcd_puts("Code: ____");
			keypad_code[0] = '_';
			keypad_code[1] = '_';
			keypad_code[2] = '_';
			keypad_code[3] = '_';
			col = 7;
			timer=0;
			state=entry;
		}
	
		if (s>1)
		{
			lcd_rst();    lcd_puts("ALARM MODE");
			lcd_xy(1, 2);	lcd_puts("Code: ____");
			keypad_code[0] = '_';
			keypad_code[1] = '_';
			keypad_code[2] = '_';
			keypad_code[3] = '_';
			col = 7;
			fd.ssm.clk_en.pwm1 = 1;
			state=alarm;
		}
		
	break;
		
/*--------------------------------------------------------------------------------
	ENTRY STATE (STATE 3) 
		un_set led flashes
		Internal sounder enabled for 250ms every 500ms.
---------------------------------------------------------------------------------*/	
	case entry:
		printf("\rENTRY MODE        ");
		printf("\r De-activate the alarm now");
	
		buzzer250ms=1; //enable buzzer 500ms 50% duty
	
		if(timer<entryTime)
		{
			button = getKey();
			if (isdigit(button) && (col < 11))
			{
				keypad_code[col-7] = button;
				lcd_xy(7,2);	lcd_puts(keypad_code);
				col += 1;
			}
			
			if((button == '#') && (col > 7)) //Delete and replace the character with '_'
			{
				col -= 1;
				keypad_code[col-7] = '_';
				lcd_xy(7,2);	lcd_puts(keypad_code);
			}
			
			if(button == '*')
			{
				if(testCode() == 4)
				{
					lcd_rst();	lcd_puts("UN-SET MODE");
					lcd_xy(1, 2);	lcd_puts("Code: ____");
					keypad_code[0] = '_';
					keypad_code[1] = '_';
					keypad_code[2] = '_';
					keypad_code[3] = '_';
					col = 7;
					buzzer250ms=0;
					buzzer500ms=0;
					fd.ssm.clk_dis.pwm1 = 1;
					state = un_set;
				}
			}
					
			if (s>1)
			{
				lcd_rst();    lcd_puts("ALARM MODE");
				lcd_xy(1, 2);	lcd_puts("Code: ____");
				keypad_code[0] = '_';
				keypad_code[1] = '_';
				keypad_code[2] = '_';
				keypad_code[3] = '_';
				col = 7;
				//buzzer250ms=0;
				fd.ssm.clk_dis.pwm1 = 1;
				state=alarm;
			}
		}
		else
		{
			//buzzer250ms=0;
			fd.ssm.clk_dis.pwm1 = 1;
			lcd_rst();    lcd_puts("ALARM MODE");
			lcd_xy(1, 2);	lcd_puts("Code: ____");
			//fd.ssm.clk_en.pwm1 = 1;
			state=alarm;
		}
	break;
		
/*--------------------------------------------------------------------------------
	ALARM STATE (STATE 4)
		alarm led enabled
		Internal and External sounders enabled.
---------------------------------------------------------------------------------*/	
	case alarm:
		printf("\rALARM ACTIVE - INTRUDER ALERT!    ");
	
		if (timer<alarmTime) //turn on sounder for alarmTime
		{
			gpio_wr(extSounder, 1); //turn on sounder
		}
		else
		{
			gpio_wr(extSounder, 0); //turn on sounder
		}
	
		//buzzer250ms=0;
		//buzzer500ms=0;
		
		buzzeralarm=1;
		
		//fd.ssm.clk_en.pwm1 = 1; //turn on buzzer
	
		button = getKey(); //get keypress
		if (isdigit(button) && (col < 11))
		{
			keypad_code[col-7] = button;
			lcd_xy(7,2);	lcd_puts(keypad_code);
			col += 1;
		}
		
		if((button == '#') && (col > 7)) //Delete and replace the character with '_'
		{
			col -= 1;
			keypad_code[col-7] = '_';
			lcd_xy(7,2);	lcd_puts(keypad_code);
		}		
		
		if(button == '*')
		{
			if(testCode() == 4)
			{
				lcd_rst();	lcd_puts("UN-SET MODE");
				lcd_xy(1, 2);	lcd_puts("Code: ____");
				keypad_code[0] = '_';
				keypad_code[1] = '_';
				keypad_code[2] = '_';
				keypad_code[3] = '_';
				col = 7;
				gpio_wr(extSounder, 0); //turn off sounder
				buzzeralarm=0;
				fd.ssm.clk_dis.pwm1 = 1;	 // Writing 1 to this pwm1 field disables the PWM1 timer clock.
				state = un_set;
			}
		}

	break;

/*--------------------------------------------------------------------------------
	REPORT (STATE 5)
		un-set and alarm leds enabled
		Internal sounder remains enabled.
		External sounder enabled.
---------------------------------------------------------------------------------*/	
	case report:
		printf("\rREPORT STATUS        ");
		//lcd_rst();    lcd_puts("UN-SET MODE        ");

	break;
	}
}

/**********************************************************************************


^ ^ ^ ^ ^ END OF THE STATE MACHINE ^ ^ ^ ^ ^


**********************************************************************************/

/**********************************************************************************
Keypad functionality
**********************************************************************************/

char getKey(void) // returns characters from the keypad
{
	int i, j;
	int key;
	char ch = ' ';
	key = 0xFFFF;
	for (i = 0; i <= (col3 - col1); i++) 
	{
		gpio_wr((i + col1), 0);
		gpio_cfg((i + col1), 0);
		dBounce();
		for (j = 0; j <= row4; j++) 
		{
			if (gpio_rd(j) == 0) 
			{
				key = (i * col1) + j;
				switch (key) {
					case key_0:
						ch = '0';
						break;
					case key_1:
						ch = '1';
						break;
					case key_2:
						ch = '2';
						break;
					case key_3:
						ch = '3';
						break;
					case key_4:
						ch = '4';
						break;
					case key_5:
						ch = '5';
						break;
					case key_6:
						ch = '6';
						break;
					case key_7:
						ch = '7';
						break;
					case key_8:
						ch = '8';
						break;
					case key_9:
						ch = '9';
						break;
					case key_ast:
						ch = '*';
						break;
					case key_hash:
						ch = '#';
						break;
					default:
						ch = ' ';
				}
				
			}		
		}
		gpio_cfg((i + col1), 1);
	}
	return ch;
}

static void setKey(void) // set user key code - this is the password
{
	printf("\n\r  PLEASE ENTER YOUR OWN 4 DIGIT  CODE ");

	// Set code here

	printf("\n\r");
} 

int testCode(void) // get entered code and compare with user key code (code[5])
{
	//locals
	int accepted = 0; // By default, this fails

	printf("\n\r  PLEASE ENTER CODE ");

	// Check code here
	
	if(keypad_code[0] == code[0])
	{		
		accepted = 1;
		
		if(keypad_code[1] == code[1])
		{		
			accepted = 2;	
			
			if(keypad_code[2] == code[2])
			{
				accepted = 3;
				
				if(keypad_code[3] == code[3])
				{
					accepted = 4;
				}
			}
		}
	}

	printf("\n\r");
	return(accepted); // return true / false on the password check
}

/**********************************************************************************
Delays for exit timer and switch de-bounce
**********************************************************************************/

static void delay_5ms(void)
{
	//locals
    unsigned int m;

    // Assume 25 MHz CPU and cache enabled: 5ms = 125000 NOPs
    // This will be 7*18000 instructions
    for (m = 0; m < 18000; m++)
    {
		void nop(); // do nothing
    }
	
}

static void delay(int length) 
{	// Provides a multiple of 5 msec delay.
	// The multiple is specified by the value of the 'length' argument.
	
	//locals
	unsigned int h;
	
	// making the same assumptions for timing as those in delay_5ms
    for (h = 0; h < length; h++)
    {
		delay_5ms(); // a delay function defined elsewhere in source code
	}
}

static int countdown(void)
{	
	//locals
	int cancel;
	unsigned int c;
	int s;
	
	// making the same assumptions for timing as those in delay_5ms
	
	// invoked by the Exit and Entry Modes to provide an exit/entry period
	
	printf("\n\r countdown start\n");
	for (c = 0; c < exitTime;)
	{

		s = pio_in(PIOA);
		s = s & 0xFF00;
		s = s >> 8;
		pio_out(PIOB, s);
		switch(s)
		{
		case 2:
			c = exitTime; // end loop count
			cancel = 2; // cancellation indicator
		break;
		
		case 4:
			c = exitTime; // end loop count
			cancel = 2; // cancellation indicator
		break;
		
		default:
			if(getKey()== '#') // to interrupt the loop when cancelling exit mode
			{
				if(testCode()==4)
				{
					c = exitTime; // end loop count
					cancel = 1; // cancellation indicator
				}
			}
			else
			{
				pio_out(PIOB, s | 0x10); // un_set led Flash on
				delay_5ms();
				cancel = 0;
				c++; // only increments loop count if not interrupted by user
				printf("\r%d ", c); // displays counter value
				pio_out(PIOB, s | 0x00); // un_set led Flash off
			}
		break;
		}	
	}
	printf("\n\r countdown ends\n");
	return (cancel);
}

static void dBounce(void) // introduces delay between keys registered for switch de-bounce
{	
	//locals
	unsigned int h;
	
	// making the same assumptions for timing as those in delay_5ms
    for (h = 0; h < 15; h++)
    {
		delay_5ms();
	}
}

/**********************************************************************************
Configurations
**********************************************************************************/

void __irq_entry tick_handler(void)
{
    fd.tim.int_clr1.cnt1_exp = 1;
    do_pattern = 1;
	if (buzzeralarm==1)
	{
		buzzer250ms=0;
		buzzer500ms=0;
		fd.ssm.clk_en.pwm1 = 1;
	}
	if (buzzer250ms==1) //if buzzer 500ms 50% duty enabled
	{
		if (buzzer==0)
		{
			fd.ssm.clk_en.pwm1 = 1;
			buzzer=1;
		}
		else
		{
			fd.ssm.clk_dis.pwm1 = 1;
			buzzer=0;
		}
	}
	if (buzzer500ms==1)
	{
		fd.ssm.clk_en.pwm1 = 1;
		if (timer==2)
		{
			buzzer500ms=0;
			fd.ssm.clk_dis.pwm1 = 1;
		}
	}
	timer++; //incremented every 0.25 seconds
}

void alarmConfig(void) 
{
	//locals
	int i;

	//set keypad pins as inputs (tristate)
	//set the pins so they are 0 when outputs
	for (i = 0; i < col3; i++) 
	{
		gpio_cfg(i, 1);
		gpio_wr(i, 0);
		
	}
	
	//set lcd to inital state
	lcd_rst();	lcd_puts("UN-SET MODE");
	lcd_xy(1, 2);	lcd_puts("Code: ____");
	
	//set sounder as outputs
	gpio_cfg(extSounder, 0);
	gpio_wr(extSounder, 0);
	
	// external sounder config and enable ******************
	ssm_pwm1_clk(SSM_LOW_PLL, 9);
	rg.tim.pwm1_ld = 6; // reload the downcounter with start value 6
	rg.tim.pwm1_val = 3; // transition point when downcounter reaches 3
	fd.tim.pwm1_cfg.pol = 1; // make pwm output initially high on timer reload
	fd.tim.pwm1_cfg.sw_reload = 1; //reload with value in register tim.pwm1_ld when timer reaches zero
	// also need to set pwm1_auto_re-ld bit (bit 3) in tim.ctrl_en register for the autoematic reload
	// to work (see below)
	fd.tim.cmd.pwm1_ld = 1; // change the pwm1_ld bit to 1

	rg.tim.ctrl_en = TIM_CTRL_EN_PWM1_CNT_MASK | //Writing 1 to pwm1_cnt bit in tim.ctrl_en register enables the PWM1 timer.
					TIM_CTRL_EN_PWM1_AUTO_RE_LD_MASK;//set pwm1_auto_re-ld bit (bit 3) to 1
	//******************************************************	
		
	fd.ssm.clk_dis.pwm1 = 1;	 // Writing 1 to this pwm1 field disables the PWM1 timer clock.
}
