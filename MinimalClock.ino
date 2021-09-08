// Hollow Clock improved by SnowHead, Mart 2021
//
// Thanks to:
// shiura for stepper motor routines
// SnowHead for Internet-Time

#include "WTAClient.h"
#include "Digit.h"

extern unsigned long askFrequency;

// Motor and clock parameters
#define STEPS_PER_ROTATION 4096L 		// steps of a single rotation of motor (motor: 64 steps/rotation, gear: 1/64)
// wait for a single step of stepper
#define HIGH_SPEED_DELAY	   3		// minimal delay for highest speed, delay is calculated from steps and period

#if PRE_MOVE
static unsigned long time_needed = 0;	// sum of time for next move
Digit tcurrent =
{ 0, 0, 0, 0 };		// store current digit position
#endif
bool real_move = true;					// move motor real
static long currentPos;
static long currentStep;

//=== CLOCK ===
WTAClient wtaClient;
time_t locEpoch = 0, netEpoch = 0;

#define LED		16
#define PHASES  8

// ports used to control the stepper motor
// if your motor rotate to the opposite direction,
// change the order as {15, 13, 12, 14};
// ESP	 Stepper
// D5 -> IN1
// D6 -> IN2
// D7 -> IN3
// D8 -> IN4

int port[4] = { 14, 12, 13, 15 };
Digit current = { 0 };
Digit timedig = { 0 };

int seq[PHASES][4] =
#if (PHASES == 4)
{
{ HIGH, LOW,  HIGH,  LOW },
{ LOW,  HIGH, HIGH, LOW },
{ LOW,  HIGH, LOW,  HIGH },
{ HIGH, LOW,  LOW,  HIGH },
#else
// sequence of stepper motor control
		{
		{ LOW, HIGH, HIGH, LOW },
		{ LOW, LOW, HIGH, LOW },
		{ LOW, LOW, HIGH, HIGH },
		{ LOW, LOW, LOW, HIGH },
		{ HIGH, LOW, LOW, HIGH },
		{ HIGH, LOW, LOW, LOW },
		{ HIGH, HIGH, LOW, LOW },
		{ LOW, HIGH, LOW, LOW },
#endif
		};

void rotate(long step)
{
	static int phase = 0;
	int i, j;
	int delta = (step > 0) ? 1 : 7;

#if DEBUG
	if (real_move)
	{
		Serial.print("rotating steps: ");
		Serial.println(step);
	}
#endif
	step = labs(step);
#if PRE_MOVE
	if (!real_move)
	{
		time_needed += (step * ((unsigned long)HIGH_SPEED_DELAY));
		return;
	}
#endif
	for (j = 0; j < step; j++)
	{
		phase = (phase + delta) % PHASES;
		for (i = 0; i < 4; i++)
		{
			digitalWrite(port[i], seq[phase][i]);
		}
		delay(HIGH_SPEED_DELAY);
	}
	// power cut

	for (i = 0; i < 4; i++)
	{
		digitalWrite(port[i], LOW);
	}
}

void findOrigin(void)
{
int adcval;

#if ORIGIN_BRIGHTMARK
	while ((adcval = analogRead(PIN_A0)) < ORIGIN_THRES)
#else
	while ((adcval = analogRead(PIN_A0)) > ORIGIN_THRES)
#endif
	{ // if origin is sensed, back a bit
		rotate(-1);
#if DEBUG
		Serial.print("ADC: ");
		Serial.print(adcval);
		Serial.print(" ");
#endif
	}
#if ORIGIN_BRIGHTMARK
	while ((adcval = analogRead(PIN_A0)) > ORIGIN_THRES)
#else
	while ((adcval = analogRead(PIN_A0)) < ORIGIN_THRES)
#endif
	{ // find origin
		rotate(1);
#if DEBUG
		Serial.print("ADC: ");
		Serial.print(adcval);
		Serial.print(" ");
#endif
	}
	rotate(ORIGIN_COMPENSATION);
	delay(1000);
}

#define KILL_BACKLASH 10

// avoid error accumuration of fractional part of 4096 / 10
void rotStep(int s)
{
	currentPos += s;
	long diff = currentPos * STEPS_PER_ROTATION / 10 - currentStep;
	if (diff < 0)
		diff -= KILL_BACKLASH;
	else
		diff += KILL_BACKLASH;
	rotate(diff);
	currentStep += diff;
}

void printDigit(Digit d)
{
#if DEBUG
	String s = "        ";
	int i;

	for (i = 0; i < DIGIT; i++)
	{
		s.setCharAt(i, d.v[i] + '0');
	}
	Serial.println(s);
#endif
}

//increase specified digit
Digit rotUp(Digit current, int digit, int num)
{
	int freeplay = 0;
	int i;

	for (i = digit; i < DIGIT - 1; i++)
	{
		int id = current.v[i];
		int nd = current.v[i + 1];
		if (id <= nd)
			id += 10;
		freeplay += id - nd - 1;
	}
	freeplay += num;
	rotStep(-1 * freeplay);
	current.v[digit] = (current.v[digit] + num) % 10;
	for (i = digit + 1; i < DIGIT; i++)
	{
		current.v[i] = (current.v[i - 1] + 9) % 10;
	}
#if DEBUG
	if (real_move)
	{
		Serial.print("up end : ");
		printDigit(current);
	}
#endif
	return current;
}

// decrease specified digit
Digit rotDown(Digit current, int digit, int num)
{
	int freeplay = 0;
	int i;

	for (i = digit; i < DIGIT - 1; i++)
	{
		int id = current.v[i];
		int nd = current.v[i + 1];
		if (id > nd)
			nd += 10;
		freeplay += nd - id;
	}
	freeplay += num;
	rotStep(1 * freeplay);
	current.v[digit] = (current.v[digit] - num + 10) % 10;
	for (i = digit + 1; i < DIGIT; i++)
	{
		current.v[i] = current.v[i - 1];
	}
#if DEBUG
	if (real_move)
	{
		Serial.print("down end : ");
		printDigit(current);
	}
#endif
	return current;
}

// decrease or increase specified digit
Digit rotDigit(Digit current, int digit, int num)
{
	if (num > 0)
	{
		return rotUp(current, digit, num);
	}
	else if (num < 0)
	{
		return rotDown(current, digit, -num);
	}
	else
		return current;
}

// set single digit to the specified number
Digit setDigit(Digit current, int digit, int num)
{
	if (digit == 0)
	{ // most significant digit
		int rot = num - current.v[0];
		// use decreasing rotation because following digits tend to be 000 or 999
		if (rot > 1)
			rot -= 10;
		return rotDigit(current, digit, rot);
	}
	int cd = current.v[digit];
	int pd = current.v[digit - 1];
	if (cd == num)
		return current;

	// check if increasing rotation is possible
	int n2 = num;
	if (n2 < cd)
		n2 += 10;
	if (pd < cd)
		pd += 10;
	if (pd <= cd || pd > n2)
	{
		return rotDigit(current, digit, n2 - cd);
	}
	// if not, do decrease rotation
	if (num > cd)
		cd += 10;
	return rotDigit(current, digit, num - cd);
}

void setNumber(Digit n)
{
	if (real_move)
	{
		Serial.print("Number: ");
		printDigit(n);
	}
	for (int i = 0; i < DIGIT; i++)
	{
		current = setDigit(current, i, n.v[i]);
	}
}

void setup()
{
	pinMode(port[0], OUTPUT);
	pinMode(port[1], OUTPUT);
	pinMode(port[2], OUTPUT);
	pinMode(port[3], OUTPUT);
	pinMode(0, INPUT_PULLUP);
#if DEBUG
	pinMode(LED, OUTPUT);
	digitalWrite(LED, 1);
#endif

	Serial.begin(115200);
	wtaClient.Setup();
	askFrequency = 50;

#if ORIGIN_SENSOR
  findOrigin();
#endif

	rotate(STEPS_PER_ROTATION * (DIGIT - 1));
}

void loop()
{
	static int lastmin = -1;
	struct tm *tmtime;

	askFrequency = 60 * 60 * 1000;
	while (((netEpoch = wtaClient.GetCurrentTime()) == locEpoch) || (!netEpoch))
	{
		delay(100);
	}
	if (netEpoch)
	{
#if PRE_MOVE
		long tcurrentPos;
		long tcurrentStep;
		time_t tEpoch;

		netEpoch += ((time_needed + 500) / 1000);
#endif
		tmtime = localtime(&netEpoch);
		if (lastmin != tmtime->tm_min)
		{
			int j = 0;

			wtaClient.PrintTime();
			lastmin = tmtime->tm_min;
#if EIGHT_DIGIT
			timedig.v[j++] = (tmtime->tm_mon + 1) / 10;
			timedig.v[j++] = (tmtime->tm_mon + 1) % 10;
			timedig.v[j++] = tmtime->tm_mday / 10;
			timedig.v[j++] = tmtime->tm_mday % 10;
#endif
			timedig.v[j++] = tmtime->tm_hour / 10;
			timedig.v[j++] = tmtime->tm_hour % 10;
			timedig.v[j++] = tmtime->tm_min / 10;
			timedig.v[j] = tmtime->tm_min % 10;
			setNumber(timedig);
#if PRE_MOVE
			real_move = false;
			time_needed = 0;
			j = 0;
			tcurrent = current;
			tcurrentPos = currentPos;
			tcurrentStep = currentStep;
			tEpoch = netEpoch + 61;
			tmtime = localtime(&tEpoch);
#if EIGHT_DIGIT
			timedig.v[j++] = (tmtime->tm_mon + 1) / 10;
			timedig.v[j++] = (tmtime->tm_mon + 1) % 10;
			timedig.v[j++] = tmtime->tm_mday / 10;
			timedig.v[j++] = tmtime->tm_mday % 10;
#endif
			timedig.v[j++] = tmtime->tm_hour / 10;
			timedig.v[j++] = tmtime->tm_hour % 10;
			timedig.v[j++] = tmtime->tm_min / 10;
			timedig.v[j] = tmtime->tm_min % 10;
			setNumber(timedig);
#if DEBUG
			Serial.print("calculated time [ms]: ");
			Serial.println(time_needed);
#endif
			current = tcurrent;
			currentPos = tcurrentPos;
			currentStep = tcurrentStep;
			real_move = true;
#endif
		}
	}
	else
	{
		delay(100);
	}
	locEpoch = netEpoch;
}
