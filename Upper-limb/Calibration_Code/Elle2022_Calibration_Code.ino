const int potPin = A4;
const int d1Pin = 24; 
const int pwmPin = 4; 
//Left Elbow -90 Deg - 65 Pot Reading
//Left Elbow 0 Deg - 415 Pot Reading
//To install, rotate joint from 90 to 0 degrees and max the pot in that direction; give the pot a 180 degree turn back.

int value;
int angulo;
int dir;
//////////////////

int pwm = 20; // Duty. C en %

void setup() {
  Serial.begin(9600);
  pinMode(potPin, INPUT);
  ///////////////////////
  pinMode(d1Pin, OUTPUT);
  pinMode(pwmPin, OUTPUT);

  digitalWrite(d1Pin, LOW);
  analogWrite(pwmPin, 0);
  pwm = pwm*255/100;
}

void loop() {
  value = analogRead(potPin);
  Serial.println(value);
  
  if (Serial.available() > 0) {
  dir = Serial.read();
    
    if (dir == '1'){
      analogWrite(pwmPin, pwm);
      digitalWrite(d1Pin, LOW);
      delay(300);
      analogWrite(pwmPin, 0);
    }
    
    if (dir == '8'){
      analogWrite(pwmPin, pwm);
      digitalWrite(d1Pin, HIGH);
      delay(300);
      analogWrite(pwmPin, 0);
    }
    
  }
}
