#include <ESP32Servo.h>

// -------- ESP32 PINS --------
const int ENA = 18;  
const int IN1 = 22;  
const int IN2 = 23;  
const int IN3 = 19;  
const int IN4 = 21;  
const int ENB = 5; 

// IR Sensor Array
const int S[6] = {34, 35, 32, 33, 25, 26};

// Ultrasonic Pins
const int trigFront = 27; const int echoFront = 14; 
const int trigLeft  = 12; const int echoLeft  = 13; 
const int trigRight = 2;  const int echoRight = 4; 

// Servo Configuration
Servo myServo;
const int servoPin = 15;
int currentServoAngle = 90; 

// -------- TUNING VARIABLES --------
const int baseSpeed = 85;          
const int turnSpeed = 135;          
const int innerWheelSpeed = 75;    
const int sharpTurnSpeed = 160;    
const int sharpReverseSpeed = 100; 

// Increased to 20cm so the robot has physical room to spin 180 without hitting the box
const int obstacleThreshold = 15; 

// -------- SENSOR FREEZE VARIABLES --------
unsigned long frontFreezeUntil = 0;
unsigned long leftFreezeUntil = 0;
unsigned long rightFreezeUntil = 0;
const unsigned long freezeDuration = 2000; // 2 seconds (still used for side sensors)

// -------- MEMORY & TIMEOUT VARIABLES --------
char lastDirection = 'S';          
bool isLost = false;                
unsigned long lostStartTime = 0;   
const unsigned long sweepTimeoutStraight = 100;  
const unsigned long sweepTimeoutTurn = 700;     

// TIME TRACKING FOR DYNAMIC FREEZE
unsigned long runStartTime = 0; 

// STATE FLAG FOR SIDE DETECTION
bool enableSideDetection = true; 

void motorSetup() {
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  pinMode(ENA, OUTPUT); pinMode(ENB, OUTPUT);
  for(int i = 0; i < 6; i++) { pinMode(S[i], INPUT); }
  pinMode(trigFront, OUTPUT); pinMode(echoFront, INPUT);
  pinMode(trigLeft, OUTPUT);  pinMode(echoLeft, INPUT);
  pinMode(trigRight, OUTPUT); pinMode(echoRight, INPUT);
  
  myServo.attach(servoPin);
  myServo.write(90); 
  currentServoAngle = 90; 
}

void moveServoSlowly(int targetAngle) {
  int step = (currentServoAngle < targetAngle) ? 5 : -5;
  while (currentServoAngle != targetAngle) {
    currentServoAngle += step;
    myServo.write(currentServoAngle);
    delay(5); 
  }
}

void drive(int rightSpd, int leftSpd) {
  if (leftSpd > 0)       { digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW); }
  else if (leftSpd < 0)  { digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH); leftSpd = -leftSpd; }
  else                   { digitalWrite(IN1, LOW);  digitalWrite(IN2, LOW); }
  if (rightSpd > 0)      { digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW  ); }
  else if (rightSpd < 0) { digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH); rightSpd = -rightSpd; }
  else                   { digitalWrite(IN3, LOW);  digitalWrite(IN4, LOW); }
  analogWrite(ENA, leftSpd);
  analogWrite(ENB, rightSpd);
}

void brake() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW); 
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
  analogWrite(ENA, 0); analogWrite(ENB, 0);
}

long getDistance(int trig, int echo) {
  digitalWrite(trig, LOW); delayMicroseconds(2);
  digitalWrite(trig, HIGH); delayMicroseconds(10);
  digitalWrite(trig, LOW);
  long duration = pulseIn(echo, HIGH, 20000); 
  delay(10); // Cross-talk protection
  if (duration == 0) return 999;
  return duration * 0.034 / 2;
}

void handleObstacle(int angle, char sensor) {
  brake();
  moveServoSlowly(angle);
  Serial.print("Sensor "); Serial.print(sensor); Serial.println(" Triggered. Waiting 3s...");
  delay(3000); 
  moveServoSlowly(90); 
  delay(200);

  if (sensor == 'L') leftFreezeUntil = millis() + freezeDuration;
  if (sensor == 'R') rightFreezeUntil = millis() + freezeDuration;
  Serial.print("Sensor "); Serial.print(sensor); Serial.println(" frozen for 2s.");
}

// ==========================================
// DYNAMIC 180-DEGREE TURN LOGIC
// ==========================================
void execute180Turn() {
  brake(); delay(200);
  Serial.println("FRONT OBSTACLE DETECTED! Executing 180 Turn...");

  // 1. Start spinning Right
  drive(sharpTurnSpeed+10, -sharpReverseSpeed-55); 
  
  // 2. Blind spin for half a second to force the sensors OFF the current line
  delay(500); 

  // 3. Dynamic Catch: Keep spinning until the center sensors see the line behind us
  unsigned long turnStartTime = millis();
  while (millis() - turnStartTime < 5000) { // 5-second failsafe
    
    // Read the two center sensors
    int s2 = digitalRead(S[2]);
    int s3 = digitalRead(S[3]);
    
    // The moment either center sensor hits black, we have completed the 180!
    if (s2 == HIGH || s3 == HIGH) {
      Serial.println("Line re-acquired! 180 complete.");
      break; 
    }
  }

  brake(); delay(200);

  // 4. MUTE ALL ULTRASONIC SENSORS BASED ON NEW FORMULA
  unsigned long timeTaken = millis() - runStartTime; 
  unsigned long dynamicFreezeDuration = 0;
  
  // 3 seconds * 1 = 3000 milliseconds
  // Safety check: Prevent negative calculation if it hits obstacle in under 3 seconds
  if (timeTaken > 3000) {
    dynamicFreezeDuration = 2 * (timeTaken - 3000);
  } else {
    dynamicFreezeDuration = 0; // Don't freeze if we hit the obstacle too early
  }
  
  unsigned long safeEscapeTime = millis() + dynamicFreezeDuration; 
  frontFreezeUntil = safeEscapeTime;
  leftFreezeUntil = safeEscapeTime;
  rightFreezeUntil = safeEscapeTime;
  
  Serial.print("All Ultrasonic Sensors muted for ");
  Serial.print(dynamicFreezeDuration / 1000);
  Serial.println(" seconds.");
}

void setup() {
  Serial.begin(9600);
  ESP32PWM::allocateTimer(0); ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2); ESP32PWM::allocateTimer(3);
  motorSetup();
  delay(3000);
  
  // Mark the exact time the robot starts moving after setup
  runStartTime = millis(); 
}

void loop() {
  unsigned long currentTime = millis();

  // 1. CHECK ULTRASONIC SENSORS
  
  // FRONT SENSOR (Always Active) - triggers the 180 Turn
  if (currentTime > frontFreezeUntil) {
    if (getDistance(trigFront, echoFront) < obstacleThreshold) {
      execute180Turn();
      return; 
    }
  }

  // SIDE SENSORS (Only Active if Phase 1 is true)
  if (enableSideDetection) {
    if (currentTime > leftFreezeUntil) {
      if (getDistance(trigLeft, echoLeft) < obstacleThreshold+5) {
        handleObstacle(180, 'L'); 
        return;
      }
    }

    if (currentTime > rightFreezeUntil) {
      if (getDistance(trigRight, echoRight) < obstacleThreshold+5) {
        handleObstacle(0, 'R'); 
        return;
      }
    }
  }

  // 2. READ IR SENSORS
  int s[6];
  for (int i = 0; i < 6; i++) { s[i] = digitalRead(S[i]); }
  int sensorState = (s[0] << 5) | (s[1] << 4) | (s[2] << 3) | (s[3] << 2) | (s[4] << 1) | s[5];

  if (sensorState != 0b000000) isLost = false; 

  // 3. LINE FOLLOWING LOGIC
  switch (sensorState) {
    case 0b001100: case 0b001000: case 0b000100: case 0b011110:
      lastDirection = 'S'; 
      drive(baseSpeed, baseSpeed);           
      break;  

    case 0b011000: case 0b010000: case 0b011100:
      lastDirection = 'L'; drive(innerWheelSpeed, turnSpeed+10); 
      break;

    case 0b110000: case 0b111000: case 0b101000:
      lastDirection = 'L'; drive(0, turnSpeed+10); 
      break;

    case 0b100000: case 0b111100: case 0b111110: 
      lastDirection = 'L'; brake(); delay(30); 
      drive(-sharpReverseSpeed-5, sharpTurnSpeed+5); delay(150); 
      break;

    case 0b000110: case 0b000010: case 0b001110:
      lastDirection = 'R'; drive(turnSpeed+10, innerWheelSpeed); 
      break;

    case 0b000011: case 0b000111: case 0b000101:
      lastDirection = 'R'; drive(turnSpeed+10, 0); 
      break;

    case 0b000001: case 0b001111: case 0b011111:
      lastDirection = 'R'; brake(); delay(30); 
      drive(sharpTurnSpeed+5, -sharpReverseSpeed-5); delay(150); 
      break;

    case 0b111111: brake(); break;

    case 0b000000: { 
      if (!isLost) { isLost = true; lostStartTime = millis(); }
      unsigned long initialTimeout = (lastDirection == 'S') ? sweepTimeoutStraight : sweepTimeoutTurn;
      if (millis() - lostStartTime <= initialTimeout) {
        if (lastDirection == 'L') drive(-innerWheelSpeed-10, turnSpeed+20); 
        else if (lastDirection == 'R') drive(turnSpeed+20, -innerWheelSpeed-10);
        else drive(baseSpeed, baseSpeed);
      } else {
        while (true) {
          if (millis() - lostStartTime > 10000) { brake(); break; }
          int found = 0;
          for(int i=0; i<6; i++) if(digitalRead(S[i])) found++;
          if (found > 0) { isLost = false; break; }
          if (lastDirection == 'L') drive(-innerWheelSpeed-15, turnSpeed+23); 
          else if (lastDirection == 'R') drive(turnSpeed+23, -innerWheelSpeed-15);
          else drive(baseSpeed, baseSpeed);
          delay(100); 
        }
      }
      break;
    }
  }
}
