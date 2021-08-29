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
unsigned int delaytime;

//=== CLOCK ===
WTAClient wtaClient;
unsigned long locEpoch = 0, netEpoch = 0, start_time;

#define LED		16
#define PHASES  8

// ports used to control the stepper motor
// if your motor rotate to the opposite direction,
// change the order as {15, 13, 12, 14};
int port[4] =
{ 14, 12, 13, 15 };

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

void rotate(int step)
{
	static int phase = 0;
	int i, j;
	int delta = (step > 0) ? 1 : 7;
#if DEBUG
	Serial.print("rotating steps: ");
	Serial.println(step);
#endif

	step = (step > 0) ? step : -step;
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

	if (digitalRead(0))
	{
		delay(100);
	}
}

void findOrigin(void)
{
	while (analogRead(34) > ORIGIN_THRES)
	{ // if origin is sensed, back a bit
		rotate(-1);
	}
	while (analogRead(34) < ORIGIN_THRES)
	{ // find origin
		rotate(1);
	}
	rotate(ORIGIN_COMPENSATION);
	delay(1000);
}

#define KILL_BACKLASH 10

// avoid error accumuration of fractional part of 4096 / 10
void rotStep(int s)
{
	static long currentPos;
	static long currentStep;

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

  for(i = 0; i < DIGIT; i++) {
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
  Serial.print("up end : ");
  printDigit(current);
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
  Serial.print("down end : ");
  printDigit(current);
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

Digit current = { 0, 0, 0, 0 };

void setNumber(Digit n)
{
	Serial.print("Number: ");
	printDigit(n);
	for (int i = 0; i < DIGIT; i++)
	{
		current = setDigit(current, i, n.v[i]);
	}
}

void setup()
{
	start_time = millis();

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

	while (!(netEpoch = wtaClient.GetCurrentTime()))
	{
		delay(100);
	}

	Digit n;
	int i = 0;
	struct tm *tmtime;

	askFrequency = 60 * 60 * 1000;
	while (((netEpoch = wtaClient.GetCurrentTime()) == locEpoch) || (!netEpoch))
	{
		delay(100);
	}
	if (netEpoch)
	{
		tmtime = localtime((const time_t*)&netEpoch);
		wtaClient.PrintTime();
		if(lastmin != tmtime->tm_min)
		{
			lastmin = tmtime->tm_min;
#if EIGHT_DIGIT
		  	n.v[i++] = (tmtime->tm_mon + 1) / 10;
		  	n.v[i++] = (tmtime->tm_mon + 1) % 10;
	  		n.v[i++] = tmtime->tm_mday / 10;
	  		n.v[i++] = tmtime->tm_mday % 10;
#endif
			n.v[i++] = tmtime->tm_hour / 10;
			n.v[i++] = tmtime->tm_hour % 10;
			n.v[i++] = tmtime->tm_min / 10;
			n.v[i]   = tmtime->tm_min % 10;
			setNumber(n);
		}
	}
	else
	{
		delay(1000);
	}
	locEpoch = netEpoch;
}
