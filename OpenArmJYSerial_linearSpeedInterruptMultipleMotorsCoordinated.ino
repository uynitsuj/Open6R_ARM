// For RAMPS 1.4
#define X_DIR_PIN          55
#define X_STEP_PIN         54
#define X_ENABLE_PIN       38

#define Y_DIR_PIN          61
#define Y_STEP_PIN         60
#define Y_ENABLE_PIN       56

#define Z_DIR_PIN          48
#define Z_STEP_PIN         46
#define Z_ENABLE_PIN       62

#define A_DIR_PIN          28
#define A_STEP_PIN         26
#define A_ENABLE_PIN       24

#define B_DIR_PIN          34
#define B_STEP_PIN         36
#define B_ENABLE_PIN       30

#define C_DIR_PIN          32
#define C_STEP_PIN         47
#define C_ENABLE_PIN       45

#define X_STEP_HIGH             PORTF |=  0b00000001;
#define X_STEP_LOW              PORTF &= ~0b00000001;

#define Y_STEP_HIGH             PORTF |=  0b01000000;
#define Y_STEP_LOW              PORTF &= ~0b01000000;

#define Z_STEP_HIGH             PORTL |=  0b00001000;
#define Z_STEP_LOW              PORTL &= ~0b00001000;

#define A_STEP_HIGH             PORTA |=  0b00010000;
#define A_STEP_LOW              PORTA &= ~0b00010000;

#define B_STEP_HIGH             PORTC |=  0b00000010;
#define B_STEP_LOW              PORTC &= ~0b00000010;

#define C_STEP_HIGH             PORTL |=  0b00000100;
#define C_STEP_LOW              PORTL &= ~0b00000100;

#define TIMER1_INTERRUPTS_ON    TIMSK1 |=  (1 << OCIE1A);
#define TIMER1_INTERRUPTS_OFF   TIMSK1 &= ~(1 << OCIE1A);

String inputString = "";         // a string to hold incoming data
boolean stringComplete = false;  // whether the string is complete
String commandString = "";
String i = "";

struct stepperInfo {
  // externally defined parameters
  float acceleration;
  volatile unsigned long minStepInterval; // ie. max speed, smaller is faster
  void (*dirFunc)(int);
  void (*stepFunc)();

  // derived parameters
  unsigned int c0;                // step interval for first step, determines acceleration
  long stepPosition;              // current position of stepper (total of all movements taken so far)

  // per movement variables (only changed once per movement)
  volatile int dir;                        // current direction of movement, used to keep track of position
  volatile unsigned int totalSteps;        // number of steps requested for current movement
  volatile bool movementDone = false;      // true if the current movement has been completed (used by main program to wait for completion)
  volatile unsigned int rampUpStepCount;   // number of steps taken to reach either max speed, or half-way to the goal (will be zero until this number is known)
  volatile unsigned long estStepsToSpeed;  // estimated steps required to reach max speed
  volatile unsigned long estTimeForMove;   // estimated time (interrupt ticks) required to complete movement
  volatile unsigned long rampUpStepTime;
  volatile float speedScale;               // used to slow down this motor to make coordinated movement with other motors

  // per iteration variables (potentially changed every interrupt)
  volatile unsigned int n;                 // index in acceleration curve, used to calculate next interval
  volatile float d;                        // current interval length
  volatile unsigned long di;               // above variable truncated
  volatile unsigned int stepCount;         // number of steps completed in current movement
};

void xStep() {
  X_STEP_HIGH
  X_STEP_LOW
}
void xDir(int dir) {
  digitalWrite(X_DIR_PIN, dir);
}

void yStep() {
  Y_STEP_HIGH
  Y_STEP_LOW
}
void yDir(int dir) {
  digitalWrite(Y_DIR_PIN, dir);
}

void zStep() {
  Z_STEP_HIGH
  Z_STEP_LOW
}
void zDir(int dir) {
  digitalWrite(Z_DIR_PIN, dir);
}

void aStep() {
  A_STEP_HIGH
  A_STEP_LOW
}
void aDir(int dir) {
  digitalWrite(A_DIR_PIN, dir);
}

void bStep() {
  B_STEP_HIGH
  B_STEP_LOW
}
void bDir(int dir) {
  digitalWrite(B_DIR_PIN, dir);
}

void cStep() {
  C_STEP_LOW
  C_STEP_HIGH
}
void cDir(int dir) {
  digitalWrite(C_DIR_PIN, dir);
}

void resetStepperInfo( stepperInfo& si ) {
  si.n = 0;
  si.d = 0;
  si.di = 0;
  si.stepCount = 0;
  si.rampUpStepCount = 0;
  si.rampUpStepTime = 0;
  si.totalSteps = 0;
  si.stepPosition = 0;
  si.movementDone = false;
}

#define NUM_STEPPERS 6

volatile stepperInfo steppers[NUM_STEPPERS];

void setup() {

  Serial.begin(115200);
  Serial.println("connection established");

  pinMode(X_STEP_PIN,   OUTPUT);
  pinMode(X_DIR_PIN,    OUTPUT);
  pinMode(X_ENABLE_PIN, OUTPUT);

  pinMode(Y_STEP_PIN,   OUTPUT);
  pinMode(Y_DIR_PIN,    OUTPUT);
  pinMode(Y_ENABLE_PIN, OUTPUT);

  pinMode(Z_STEP_PIN,   OUTPUT);
  pinMode(Z_DIR_PIN,    OUTPUT);
  pinMode(Z_ENABLE_PIN, OUTPUT);

  pinMode(A_STEP_PIN,   OUTPUT);
  pinMode(A_DIR_PIN,    OUTPUT);
  pinMode(A_ENABLE_PIN, OUTPUT);

  pinMode(B_STEP_PIN,   OUTPUT);
  pinMode(B_DIR_PIN,    OUTPUT);
  pinMode(B_ENABLE_PIN, OUTPUT);

  pinMode(C_STEP_PIN,   OUTPUT);
  pinMode(C_DIR_PIN,    OUTPUT);
  pinMode(C_ENABLE_PIN, OUTPUT);

  digitalWrite(X_ENABLE_PIN, LOW);
  digitalWrite(Y_ENABLE_PIN, LOW);
  digitalWrite(Z_ENABLE_PIN, LOW);
  digitalWrite(A_ENABLE_PIN, LOW);
  digitalWrite(B_ENABLE_PIN, LOW);
  digitalWrite(C_ENABLE_PIN, HIGH);

  noInterrupts();
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1  = 0;

  OCR1A = 1000;                             // compare value
  TCCR1B |= (1 << WGM12);                   // CTC mode
  TCCR1B |= ((1 << CS11) | (1 << CS10));    // 64 prescaler
  interrupts();

  steppers[5].dirFunc = bDir;
  steppers[5].stepFunc = bStep;
  steppers[5].acceleration = 5000;
  steppers[5].minStepInterval = 160;

  steppers[4].dirFunc = aDir;
  steppers[4].stepFunc = aStep;
  steppers[4].acceleration = 3500;
  steppers[4].minStepInterval = 120;

  steppers[1].dirFunc = cDir;
  steppers[1].stepFunc = cStep;
  steppers[1].acceleration = 6000;
  steppers[1].minStepInterval = 200;

  steppers[0].dirFunc = xDir;
  steppers[0].stepFunc = xStep;
  steppers[0].acceleration = 4000;
  steppers[0].minStepInterval = 100;

  steppers[2].dirFunc = yDir;
  steppers[2].stepFunc = yStep;
  steppers[2].acceleration = 4000;
  steppers[2].minStepInterval = 120;

  steppers[3].dirFunc = zDir;
  steppers[3].stepFunc = zStep;
  steppers[3].acceleration = 4000;
  steppers[3].minStepInterval = 100;
}

void resetStepper(volatile stepperInfo& si) {
  si.c0 = si.acceleration;
  si.d = si.c0;
  si.di = si.d;
  si.stepCount = 0;
  si.n = 0;
  si.rampUpStepCount = 0;
  si.movementDone = false;
  si.speedScale = 1;

  float a = si.minStepInterval / (float)si.c0;
  a *= 0.676;

  float m = ((a * a - 1) / (-2 * a));
  float n = m * m;

  si.estStepsToSpeed = n;
}

volatile byte remainingSteppersFlag = 0;

float getDurationOfAcceleration(volatile stepperInfo& s, unsigned int numSteps) {
  float d = s.c0;
  float totalDuration = 0;
  for (unsigned int n = 1; n < numSteps; n++) {
    d = d - (2 * d) / (4 * n + 1);
    totalDuration += d;
  }
  return totalDuration;
}

void prepareMovement(int whichMotor, long steps) {
  volatile stepperInfo& si = steppers[whichMotor];
  si.dirFunc( steps < 0 ? HIGH : LOW );
  si.dir = steps > 0 ? 1 : -1;
  si.totalSteps = abs(steps);
  resetStepper(si);

  remainingSteppersFlag |= (1 << whichMotor);

  unsigned long stepsAbs = abs(steps);

  if ( (2 * si.estStepsToSpeed) < stepsAbs ) {
    // there will be a period of time at full speed
    unsigned long stepsAtFullSpeed = stepsAbs - 2 * si.estStepsToSpeed;
    float accelDecelTime = getDurationOfAcceleration(si, si.estStepsToSpeed);
    si.estTimeForMove = 2 * accelDecelTime + stepsAtFullSpeed * si.minStepInterval;
  }
  else {
    // will not reach full speed before needing to slow down again
    float accelDecelTime = getDurationOfAcceleration( si, stepsAbs / 2 );
    si.estTimeForMove = 2 * accelDecelTime;
  }
}

volatile byte nextStepperFlag = 0;

void setNextInterruptInterval() {

  bool movementComplete = true;

  unsigned long mind = 999999;
  for (int i = 0; i < NUM_STEPPERS; i++) {
    if ( ((1 << i) & remainingSteppersFlag) && steppers[i].di < mind ) {
      mind = steppers[i].di;
    }
  }

  nextStepperFlag = 0;
  for (int i = 0; i < NUM_STEPPERS; i++) {
    if ( ! steppers[i].movementDone )
      movementComplete = false;
    if ( ((1 << i) & remainingSteppersFlag) && steppers[i].di == mind )
      nextStepperFlag |= (1 << i);
  }

  if ( remainingSteppersFlag == 0 ) {
    TIMER1_INTERRUPTS_OFF
    OCR1A = 65500;
  }

  OCR1A = mind;
}

ISR(TIMER1_COMPA_vect)
{
  unsigned int tmpCtr = OCR1A;

  OCR1A = 65500;

  for (int i = 0; i < NUM_STEPPERS; i++) {

    if ( ! ((1 << i) & remainingSteppersFlag) )
      continue;

    if ( ! (nextStepperFlag & (1 << i)) ) {
      steppers[i].di -= tmpCtr;
      continue;
    }

    volatile stepperInfo& s = steppers[i];

    if ( s.stepCount < s.totalSteps ) {
      s.stepFunc();
      s.stepCount++;
      s.stepPosition += s.dir;
      if ( s.stepCount >= s.totalSteps ) {
        s.movementDone = true;
        remainingSteppersFlag &= ~(1 << i);
      }
    }

    if ( s.rampUpStepCount == 0 ) {
      s.n++;
      s.d = s.d - (2 * s.d) / (4 * s.n + 1);
      if ( s.d <= s.minStepInterval ) {
        s.d = s.minStepInterval;
        s.rampUpStepCount = s.stepCount;
      }
      if ( s.stepCount >= s.totalSteps / 2 ) {
        s.rampUpStepCount = s.stepCount;
      }
      s.rampUpStepTime += s.d;
    }
    else if ( s.stepCount >= s.totalSteps - s.rampUpStepCount ) {
      s.d = (s.d * (4 * s.n + 1)) / (4 * s.n + 1 - 2);
      s.n--;
    }

    s.di = s.d * s.speedScale; // integer
  }

  setNextInterruptInterval();

  TCNT1  = 0;
}


void runAndWait() {
  adjustSpeedScales();
  setNextInterruptInterval();
  TIMER1_INTERRUPTS_ON
  while ( remainingSteppersFlag );
  remainingSteppersFlag = 0;
  nextStepperFlag = 0;
}

void adjustSpeedScales() {
  float maxTime = 0;

  for (int i = 0; i < NUM_STEPPERS; i++) {
    if ( ! ((1 << i) & remainingSteppersFlag) )
      continue;
    if ( steppers[i].estTimeForMove > maxTime )
      maxTime = steppers[i].estTimeForMove;
  }

  if ( maxTime != 0 ) {
    for (int i = 0; i < NUM_STEPPERS; i++) {
      if ( ! ( (1 << i) & remainingSteppersFlag) )
        continue;
      steppers[i].speedScale = maxTime / steppers[i].estTimeForMove;
    }
  }
}

void loop() {
  if (stringComplete) {
    stringComplete = false;
    getCommand();
    i = getStepFromPy();
    if (commandString.equals("MTR1"))
    {
      prepareMovement(0, i.toInt());
    }
    if (commandString.equals("MTR2"))
    {
      prepareMovement(1, i.toInt());
    }
    if (commandString.equals("MTR3"))
    {
      prepareMovement(2, i.toInt());
    }
    if (commandString.equals("MTR4"))
    {
      prepareMovement(3, i.toInt());
    }
    if (commandString.equals("MTR5"))
    {
      prepareMovement(4, i.toInt());
    }
    if (commandString.equals("MTR6"))
    {
      prepareMovement(5, i.toInt());
    }
    if (commandString.equals("EXEC"))
    {
      runAndWait();
    }
    inputString = "";
  }
}


String getStepFromPy()
{
  String value = inputString.substring(5, inputString.length() - 1);
  return value;
}

void getCommand()
{
  if (inputString.length() > 0)
  {
    commandString = inputString.substring(1, 5);
  }
}

void serialEvent() {
  while (Serial.available()) {
    // get the new byte:
    char inChar = (char)Serial.read();
    // add it to the inputString:
    inputString += inChar;
    // if the incoming character is a newline, set a flag
    // so the main loop can do something about it:
    if (inChar == '\n') {
      stringComplete = true;
    }
  }
}
