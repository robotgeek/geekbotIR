#ifndef DRIVE_CONTROL
#define DRIVE_CONTROL

#include "panning_range_sensor.h"

#define LEFT_SERVO_PIN 10
#define RIGHT_SERVO_PIN 11
#define LEFT_ENCODER_PIN 3
#define RIGHT_ENCODER_PIN 4

#define WheelRadius 0.05 //meters (50 millimeters)
#define WheelSpacing 0.195 //meters (195 millimeters)

Servo servoLeft, servoRight;      //wheel servo objects

//Constants
const int CW_MIN_SPEED = 1380; //1400;    //servo pulse in microseconds for slowest clockwise speed
const int CCW_MIN_SPEED = 1600;           //servo pulse in microseconds for slowest counter-clockwise speed
const int SERVO_STOP = 1500;        //servo pulse in microseconds for stopped servo
const int ENCODER_VALUE_THRESHOLD = 512; //ADC input value for high signal
const int encoderCounts_per_revolution = 64; //Number of slices on wheel encoder

const double meters_per_revolution = 2.0 * PI * WheelRadius; //Wheel circumference for distance traveled

const double wheel_to_wheel_circumference = 2.0 * PI * ( WheelSpacing / 2.0 ); //Circumference for in-place rotation
const double degrees_per_revolution = meters_per_revolution / wheel_to_wheel_circumference * 360;

const double meters_per_tick = meters_per_revolution / encoderCounts_per_revolution;
const double degrees_per_tick = degrees_per_revolution / encoderCounts_per_revolution;

int _servoSpeedLeft = SERVO_STOP;  //left servo speed
int _servoSpeedRight = SERVO_STOP; //right servo speed

bool _rightEncoderRising = true;    //state of encoder counting
bool _rightEncoderFalling = false;  //state of encoder counting
bool _leftEncoderRising = true;     //state of encoder counting
bool _leftEncoderFalling = false;   //state of encoder counting

int _rightEncoderCount = 0; //number of encoder ticks per command
int _leftEncoderCount = 0; //number of encoder ticks per command
double _degreesTraveled = 0.0; //total degrees of rotation per command
double _distanceTraveled = 0.0; //integrated velocity over time
unsigned long _ticksTraveledLeft = 0; //total ticks traveled per command
unsigned long _ticksTraveledRight = 0; //total ticks traveled per command

double _wheelSpeedRight = 0.0; //Current wheel speed in revolutions/second
double _wheelSpeedLeft = 0.0;  //Current wheel speed in revolutions/second

int _driveDirection = 1; //Current driving direction for wheel speed correction

unsigned long _last_timestamp = millis();

void processEncoders()
{
  int rightEncoderValue = analogRead(RIGHT_ENCODER_PIN); // get encoder value
  int leftEncoderValue = analogRead(LEFT_ENCODER_PIN);

  //Catch falling edge ( black stripe ) for right and left wheel speed encoders
  if (_rightEncoderFalling && rightEncoderValue < ENCODER_VALUE_THRESHOLD)
  {
    ++_rightEncoderCount;
    _rightEncoderRising = true;
    _rightEncoderFalling = false;
  }
  if (_leftEncoderFalling && leftEncoderValue < ENCODER_VALUE_THRESHOLD)
  {
    ++_leftEncoderCount; 
    _leftEncoderRising = true;
    _leftEncoderFalling = false;
  }

  //Catch rising edge ( white stripe ) for right and left wheel speed encoders
  if (_rightEncoderRising && rightEncoderValue > ENCODER_VALUE_THRESHOLD) 
  {
    ++_rightEncoderCount;     
    _rightEncoderRising = false;
    _rightEncoderFalling = true;
  } 
  if (_leftEncoderRising && leftEncoderValue > ENCODER_VALUE_THRESHOLD) 
  {
    ++_leftEncoderCount;     
    _leftEncoderRising = false;
    _leftEncoderFalling = true;
  }
  
  if ( _last_timestamp + 100ul < millis() )
  {
    _last_timestamp = millis();

    _wheelSpeedRight = (double)_rightEncoderCount/(double)encoderCounts_per_revolution;
    _wheelSpeedLeft = (double)_leftEncoderCount/(double)encoderCounts_per_revolution;
    _wheelSpeedRight *= 10.0; //Sampling at 10Hz but reporting speed at 1 Revolution/Second
    _wheelSpeedLeft *= 10.0;

#ifdef USB_DEBUG
    Serial.print("Ticks L:");
    Serial.print(_leftEncoderCount);
    Serial.print(" R:");
    Serial.print(_rightEncoderCount);
    Serial.print(" RPS L:");
    Serial.print(_wheelSpeedLeft);
    Serial.print(" R:");
    Serial.print(_wheelSpeedRight);
#endif

    //Wheel speed compensation
    if ( _driveDirection != 0 ) //Only compensate for FWD and REV ( +1, -1 )
    {
      if ( _leftEncoderCount - _rightEncoderCount > 1 )
      {
        _servoSpeedLeft += _driveDirection * 3;
        _servoSpeedRight -= _driveDirection * 3;
      }
      else if ( _rightEncoderCount - _leftEncoderCount > 1 )
      {
        _servoSpeedLeft -= _driveDirection * 3;
        _servoSpeedRight += _driveDirection * 3;
      }
    }
#ifdef USB_DEBUG
    Serial.print( " PWM L: " );
    Serial.print( _servoSpeedLeft );

    Serial.print( " R: " );
    Serial.print( _servoSpeedRight );

    Serial.print( " Range: " );
    Serial.println( irsensorValue );
#endif
    
    _ticksTraveledLeft += _leftEncoderCount;
    _ticksTraveledRight += _rightEncoderCount;
    
    _distanceTraveled += (_leftEncoderCount + _rightEncoderCount)/2.0 * meters_per_tick;
    _degreesTraveled += (_leftEncoderCount + _rightEncoderCount)/2.0 * degrees_per_tick;
    
    _rightEncoderCount = 0;
    _leftEncoderCount = 0;
  }

  servoLeft.writeMicroseconds( _servoSpeedLeft );
  servoRight.writeMicroseconds( _servoSpeedRight );
}

double driveStop()
{
  double total_distance_traveled = 0.0;
  
  _servoSpeedLeft = SERVO_STOP;
  _servoSpeedRight = SERVO_STOP;
  servoLeft.writeMicroseconds( _servoSpeedLeft );
  servoRight.writeMicroseconds( _servoSpeedRight );
  
#ifdef USB_DEBUG
  Serial.print( "Distance Traveled: " );
  Serial.print( _distanceTraveled );
  Serial.print( " Ticks Traveled: " );
  Serial.print( _ticksTraveledLeft );
  Serial.print( " Degrees Traveled: " );
  Serial.println( _degreesTraveled );
#endif

#ifdef USE_LEDS
  flashLEDs( 3, 250 );
#endif

  total_distance_traveled = _distanceTraveled;
  
  _ticksTraveledLeft = 0;
  _ticksTraveledRight = 0;
  _distanceTraveled = 0.0;
  _degreesTraveled = 0.0;

  return total_distance_traveled;
}

void driveForward( double meters )
{
  _servoSpeedLeft = CCW_MIN_SPEED;
  _servoSpeedRight = CW_MIN_SPEED;
  _driveDirection = 1;
  while ( _distanceTraveled < meters )
  {
    processEncoders();
  }
  driveStop();
}

void driveReverse( double meters )
{
  _servoSpeedLeft = CW_MIN_SPEED;
  _servoSpeedRight = CCW_MIN_SPEED;
  _driveDirection = -1;
  while ( _distanceTraveled < meters )
  {
    processEncoders();
  }
  driveStop();
}

void driveLeft( double p_degrees )
{
  _servoSpeedLeft = CW_MIN_SPEED;
  _servoSpeedRight = CW_MIN_SPEED;
  _driveDirection = 0;
  while ( _degreesTraveled < p_degrees )
  {
    processEncoders();
  }
  driveStop();
}

void driveRight( double p_degrees )
{
  _servoSpeedLeft = CCW_MIN_SPEED;
  _servoSpeedRight = CCW_MIN_SPEED;
  _driveDirection = 0;
  while ( _degreesTraveled < p_degrees )
  { 
    processEncoders();
  }
  driveStop();
}

double driveForwardToDistance( double meter_limit, int p_stopRange )
{
  _servoSpeedLeft = CCW_MIN_SPEED;
  _servoSpeedRight = CW_MIN_SPEED;
  _driveDirection = 1;
  
  lookForward();
  processDistanceSensor();
  while ( _distanceTraveled < meter_limit && irsensorValue > p_stopRange )
  {
    processDistanceSensor();
    processEncoders();
  }
  return driveStop();
}

void wallFollowLeft( int p_distance, double p_meters )
{
  _servoSpeedLeft = CCW_MIN_SPEED;
  _servoSpeedRight = CW_MIN_SPEED;
  _driveDirection = 1;

  lookLeft();
  
  while ( _distanceTraveled < p_meters )
  {
    processDistanceSensor();
    if ( irsensorValue > p_distance )
    {
      _servoSpeedLeft = CCW_MIN_SPEED - DISTANCE_TURN_GAIN;
      _servoSpeedRight = CW_MIN_SPEED;
    }
    else
    {
      _servoSpeedRight = CW_MIN_SPEED + DISTANCE_TURN_GAIN;
      _servoSpeedLeft = CCW_MIN_SPEED;
    }

    processEncoders();
    setBlinksLeft( 2 );
    processLEDs();
  }
  driveStop();
}

double wallFollowLeftUntil( int p_distance, int p_stopRange )
{
  _servoSpeedLeft = CCW_MIN_SPEED;
  _servoSpeedRight = CW_MIN_SPEED;
  _driveDirection = 1;

  lookLeft();
  processDistanceSensor();
  
  while ( irsensorValue < p_stopRange )
  {
    processDistanceSensor();
    if ( irsensorValue > p_distance )
    {
      _servoSpeedLeft = CCW_MIN_SPEED - DISTANCE_TURN_GAIN;
      _servoSpeedRight = CW_MIN_SPEED;
    }
    else
    {
      _servoSpeedRight = CW_MIN_SPEED + DISTANCE_TURN_GAIN;
      _servoSpeedLeft = CCW_MIN_SPEED;
    }

    processEncoders();
    setBlinksLeft( 2 );
    processLEDs();
  }

  return driveStop();
}

void wallFollowRight( int p_distance, double p_meters )
{
  _servoSpeedLeft = CCW_MIN_SPEED;
  _servoSpeedRight = CW_MIN_SPEED;
  _driveDirection = 1;

  lookRight();
  
  while ( _distanceTraveled < p_meters )
  {
    processDistanceSensor();
    if ( irsensorValue > p_distance )
    {
      _servoSpeedLeft = CCW_MIN_SPEED;
      _servoSpeedRight = CW_MIN_SPEED + DISTANCE_TURN_GAIN;
    }
    else
    {
      _servoSpeedRight = CW_MIN_SPEED;
      _servoSpeedLeft = CCW_MIN_SPEED - DISTANCE_TURN_GAIN;
    }

    processEncoders();
    setBlinksRight( 2 );
    processLEDs();
  }
  driveStop();
}

double wallFollowRightUntil( int p_distance, int p_stopRange )
{
  _servoSpeedLeft = CCW_MIN_SPEED;
  _servoSpeedRight = CW_MIN_SPEED;
  _driveDirection = 1;

  lookRight();
  processDistanceSensor();
  
  while ( irsensorValue < p_stopRange )
  {
    processDistanceSensor();
    if ( irsensorValue > p_distance )
    {
      _servoSpeedLeft = CCW_MIN_SPEED;
      _servoSpeedRight = CW_MIN_SPEED + DISTANCE_TURN_GAIN;
    }
    else
    {
      _servoSpeedRight = CW_MIN_SPEED;
      _servoSpeedLeft = CCW_MIN_SPEED - DISTANCE_TURN_GAIN;
    }

    processEncoders();
    setBlinksRight( 2 );
    processLEDs();
  }

  return driveStop();
}

#endif //--DRIVE_CONTROL
