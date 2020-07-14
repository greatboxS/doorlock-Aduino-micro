#include <TM1638plus.h>
#include <EEPROM.h>
#include <TM1638plus_font.h>

// GPIO I/O pins on the Arduino connected to strobe, clock, data,
//pick on any I/O you want.
#define STROBE_TM 5
#define CLOCK_TM 3
#define DIO_TM 2

#define DOOR_LOCK_PIN 10
#define DOOR_CLOSE 1
#define DOOR_WAIT 2
#define DOOR_OPEN 3

#define UP_BUTTON 0x80
#define DOWN_BUTTON 0x40
#define OPEN_BUTTON 0x01
#define STOP_BUTTON 0x10

#define EEPROM_ADDR 0x01

#define DOOR_OPEN_TIMEOUT 3     //sec
#define DIS_TIMEOUT 30          //sec

static uint8_t set_hour = 0;
static uint32_t sec = 0, min = 0, set_min = 0;
static uint32_t sys_tick = 0, but_tick = 0;
static uint8_t button_val = 0;
static uint8_t door_wait_time = 0, dis_wait_time = 0;
static bool dis_enabled = false;
static bool timer_overload = false;
static uint8_t door_state = false;

//Constructor object
TM1638plus tm(STROBE_TM, CLOCK_TM, DIO_TM);

void clock_runing()
{
    if (millis() - sys_tick >= 1000)
    {
        sys_tick = millis();
        sec++;

        if (door_state == DOOR_WAIT)
        {
            door_wait_time++;
            if (door_wait_time >= DOOR_OPEN_TIMEOUT)
            {
                tm.setLED(0, 0);
                door_state = DOOR_CLOSE;
                door_wait_time = 0;
                Serial.println("Door close again!");
            }
        }

        if (dis_enabled)
        {
            dis_wait_time++;

            Serial.print("Display tick: ");
            Serial.println(dis_wait_time, DEC);

            if (dis_wait_time >= DIS_TIMEOUT)
            {
                dis_enabled = false;
                dis_wait_time = 0;

                tm.reset();

                Serial.println("Turn off LED");
            }
        }

        if (sec >= 60)
        {
            sec = 0;
            min++;

            Serial.print("Min: ");
            Serial.println(min, DEC);

            if (min >= set_min && !timer_overload)
            {
                timer_overload = true;
                door_state = DOOR_OPEN;

                Serial.println("Door timer overflow");
                Serial.println("Door state: DOOR OPEN");
            }
        }
    }
}

void device_reset()
{
    display_intro_led();

    EEPROM.begin();
    uint8_t eeprom_val = EEPROM.read(EEPROM_ADDR);

    if (eeprom_val == 0xff)
        EEPROM.write(EEPROM_ADDR, 30);

    set_hour = EEPROM.read(EEPROM_ADDR);

    EEPROM.end();

    set_min = set_hour * 6;

    Serial.println(set_hour);

    display_set_number(set_hour);

    door_state = DOOR_CLOSE;
    dis_enabled = true;

    sec = 0;
    min = 0;
}

void display_set_number(uint8_t val)
{
    Serial.println(val, DEC);
    char text[20] = {0};
    sprintf(text, "    %d.%d  ", val / 10, val % 10);
    char prt[10]= {0};
    snprintf(prt, sizeof(prt), "%s", text);
    tm.displayText(prt);
}
void display_intro_led()
{
    for (int i = 0; i < 8; i++)
    {
        tm.setLED(i, 0);
    }

    for (int i = 0; i < 8; i++)
    {
        tm.setLED(i, 1);
        delay(100);
    }

    for (int i = 0; i < 8; i++)
    {
        tm.setLED(i, 0);
    }
}

void display_stop()
{
    for (int k = 0; k < 3; k++)
    {
        for (int i = 0; i < 8; i++)
        {
            tm.setLED(i, 1);
        }

        delay(100);

        for (int i = 0; i < 8; i++)
        {
            tm.setLED(i, 0);
        }
        delay(100);
    }
}

void checking_button()
{
    if (millis() - but_tick >= 50)
    {
        uint8_t current_val = 0;

        but_tick = millis();

        uint8_t but = tm.readButtons();

        if (but == button_val)
            return;

        button_val = but;

        if (button_val == 0)
            return;

        dis_enabled = true;
        dis_wait_time = 0;

        if (timer_overload)
        {
            if (button_val == (DOWN_BUTTON | UP_BUTTON) || button_val != 0)
            {
                timer_overload = false;
                min = 0;
                device_reset();
            }
            //reset device
        }
        else
        {
            switch (button_val)
            {
            case UP_BUTTON:

                Serial.println("UP_BUTTON");
                set_hour += 5;

                if (set_hour >= 250)
                    set_hour = 30;

                set_min = set_hour * 6;

                Serial.print("Set min: ");
                Serial.println(set_min);

                EEPROM.begin();
                current_val = set_hour;
                EEPROM.update(EEPROM_ADDR, current_val);
                EEPROM.end();

                display_set_number(current_val);

                Serial.println("Reset timer counter");
                min = 0;
                break;

            case DOWN_BUTTON:

                Serial.println("DOWN_BUTTON");
                set_hour -= 5;
                if (set_hour < 5)
                    set_hour = 30;

                set_min = set_hour * 6;

                Serial.print("Set min: ");
                Serial.println(set_min);

                EEPROM.begin();
                current_val = set_hour;
                EEPROM.update(EEPROM_ADDR, current_val);
                EEPROM.end();

                display_set_number(current_val);

                Serial.println("Reset timer counter");
                min = 0;
                break;

            case OPEN_BUTTON:
                door_state = DOOR_WAIT;
                tm.setLED(0, 1);
                Serial.println("OPEN_BUTTON");
                break;

            case OPEN_BUTTON | 0x02:
            case STOP_BUTTON:
                timer_overload = true;
                door_state = DOOR_OPEN;
                display_stop();
                Serial.println("Door state: DOOR OPEN");
                break;
            default:
                break;
            }
        }
    }
}

void setup()
{
    Serial.begin(115200);
    Serial.println("Hello world");

    tm.displayBegin();
    delay(500);

    pinMode(DOOR_LOCK_PIN, OUTPUT);
    digitalWrite(DOOR_LOCK_PIN, HIGH);

    device_reset();
}

void loop()
{
    clock_runing();

    checking_button();

    switch (door_state)
    {
    case DOOR_CLOSE:
        digitalWrite(DOOR_LOCK_PIN, LOW);
        break;

    case DOOR_OPEN:
    case DOOR_WAIT:
        digitalWrite(DOOR_LOCK_PIN, HIGH);
        break;

    default:
        break;
    }
}