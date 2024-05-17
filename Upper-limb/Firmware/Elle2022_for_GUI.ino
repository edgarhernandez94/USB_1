# define pi 3.14159265

float time_out_handshake = 0.1;
int Flag_python = 1; // 0 = MANUAL,  1 = GUI

int InstructionFlag = 0;
//--------------------- Available Commands -------------------------
// PWM Open-Loop and Calibration Commands: '1' for Extension and '2' for Flexion
// PID-Controlled Closed-Loop Commands:    '9' for Extension and '0' for Flexion
// PID-Controlled Closed-Loop Routine:     'r' for Flexion-and-Extension Routine
//----------------------------------------------------------------

//----------------------- Motion Parameter Selection -------------------
float MotionSpeed = 25; // Variable in the range [15,35];
float RoutineTime = 60; // Duration for "R" Back-and-Forth Movement Routine
float StepAngularDistance = 120; // Angular Motion for "9" and "0" PID-Controlled Movements
float MinAngle = -90; // Software Motion Limitations (Flexed)
float MaxAngle = -5; // Software Motion Limitations (Extended)
float MaxPower = 95; // Software PWM Limitation for PID
float OnOffPWM = 60; // PWM used for "1" and "2" On-Off Movements
//--------------------------------------------------------------------

//--------------------- Sensors input pins -------------------------
const int potPin = A4; // Analog input for potentiometer meassurement
//To install potentiometer, rotate elbow from -90 to 0 degrees and max the pot in that same direction; then give the pot a 180 degree turn back.
const int emgPin = A14; // Analog input for EMG Sensor
//----------------------------------------------------------------

//------------ Driver output pints for Monster Driver ----------------
const int d1Pin = 24; // Extension
const int pwmPin = 4; // Duty Cycle Pin
//----------------------------------------------------------------

//--------------------- Sensor variables ------------------------
int potentiometer_value;
int emg_value;
float angle;
float angular_velocity;
float MinPotVal = 65;
float MaxPotVal = 415;
//----------------------------------------------------------------

//----------------------- Driver variables ----------------------
float pwm_value; // variable in the range [-255,255];
//-------------------------------------------------------------

////////////////////////////////// Controller System variables ///////////////////////////////////////
const int Ref_Theta_mM[2] = { MinAngle, MaxAngle };//min and max angles (THE FIRST VALUE MUST BE THE LOWEST, AND BOTH VALUES MUST BE IN THE RANGE OF [-110,-5] IN VERSION 2)
const float Ref_angular_velocity_abs = MotionSpeed; //desired speed for the movement in any direction (THIS VALUE MUST BE BIGGER THAN 15, somtimes bigger depending on position and load)
const float Power_level = MaxPower; //Maximum power level allowed by the algorithm (this should be a number between 0 and 100, 80 is recomendable)
float routine_time = RoutineTime; //(s)
float Cal_values[4] = {MinPotVal, MaxPotVal, -90, 0};
// The first two values are the potentiometer values, the third is the minimum angle and the forth the maximum angle (potentiometer values should correspond with the angle values)

const float angle_inc = StepAngularDistance;//This is the resolution for the controlled increments in the movement ("regulated pulses")
const float Ts = 0.0016; //sampling time (s) for the controller (THIS VALUE MUST BE GREATHER THAN 1.6 MILISECONDS, "0.0016", IN VERSION3)
const float Multiplier = 1; //this value increases the overal angular accelaration used to achieve the desired speed. THE VALUE MUST BE IN THE RANGE [0.5,2]
const float pwm_minimum_movement[2] = { -40.0, 20.0};//These are values for overcoming the static friction (THE FIRST VALUE MUST BE THE LOWEST, AND IT IS RELATED TO FLEXON)
const float Power_level_pwm = floor(255 * min(max(Power_level, 0), 100) / 100) * 1.0;
const float pwm_constrains_mM[2] = { -Power_level_pwm, Power_level_pwm}; //These are additional constrains for the maximum and minimum pwm values (THE FIRST VALUE MUST BE THE LOWEST)

const float Kp_pid = Multiplier * 0.1; //proportional gain
const float Ki_pid = Multiplier * min(1.0, 1.0 / (100 * Ts)); //integral gain
const float Kd_pid = Multiplier * 5 * Ts; //derivative gain
const float Tc_Theta = max(0.02, 2 * Ts); //cutoff time (s) for angle's filter
const float Tc_Theta_p = max(0.05, 2 * Ts); //cutoff time(s) for velocity's filter
const float Tc_e_p = max(0.1, 2 * Ts); //cutoff time(s) for e_p's filter
const float Window_time_u = 0.1;//window time (s) considered by the filter of the manipulation
float Ref_angular_velocity;//auxiliary variable to switch between velocity's' reference
float pwm_constrains_dyn_mM[2];//auxiliary variable for pwm_minimum_movement
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void setup() {
  Serial.begin(38400);
  pinMode(potPin, INPUT);
  ///////////////////////
  pinMode(d1Pin, OUTPUT);
  pinMode(pwmPin, OUTPUT);
  digitalWrite(d1Pin, LOW);
  analogWrite(pwmPin, 0);

}

void loop() {
  //emg_value = analogRead(emgPin);
  //Serial.println(emg_value);

  int Flag_start = 0;
  if (Flag_python == 1) {
    if (Serial.available() == 2) {
      Flag_start = 1;
    }
  }
  else {
    if (Serial.available() > 0) {
      Flag_start = 1;
    }
  }
  if (Flag_start) {
    Flag_start = 0;

    int Flag_Movement;
    if (Flag_python == 0) {
      if (InstructionFlag == 0) {
        //--------------------- Command Help -------------------------
        Serial.println("PWM Open-Loop and Calibration Commands: '1' for Extension and '2' for Flexion");
        Serial.println("PID-Controlled Closed-Loop Commands:    '9' for Extension and '0' for Flexion");
        Serial.println("PID-Controlled Closed-Loop Routine:     'r' for Flexion-and-Extension Routine");
        //----------------------------------------------------------------
        InstructionFlag++;
      }
    }
    while (Serial.available() < 1 + Flag_python)  {
    }
    int Straight_key;
    if (Flag_python == 1) {
      Straight_key = Message_received_Python();
    }
    else {
      Straight_key = Serial.read();
    }

    long iterations = round(routine_time / Ts);
    if (Straight_key == 'r') {

      if (Flag_python == 0) {
        Serial.println("---- 'r' command ----");
        Serial.print("---- Start Extension-and-Flexion Routine for ");
        Serial.print(RoutineTime);
        Serial.println(" seconds. ----");
        Serial.println("---------------------");
      }

      int iclcount = 0;
      for (long ic1 = 0; ic1 < iterations; ic1++) {
        Time_control(1);
        potentiometer_value = 1.0 * analogRead(potPin); //voltage meassurenment with 10bits resolution
        angle = map_float(potentiometer_value, Cal_values[0], Cal_values[1], Cal_values[2], Cal_values[3]);//estimate angle
        angle = butter_filter_angle(angle, 0); //filter angle
        angular_velocity = angle_derivative(angle);//estimate angular velocity
        angular_velocity = butter_filter_velocity(angular_velocity);//filter angular velocity
        Ref_angular_velocity = Ref_vel(angle, Ref_Theta_mM[1]); //obtain desired angular velocity
        //---------------compute the pwm value, including its sign (manipulated variable)-------------------------
        pwm_value = PID_controller(Ref_angular_velocity, angular_velocity, Window_time_u, 0);
        //--------------------------------------------------------------------------------------------
        Execute_pwm(pwm_value);
        Time_control(2);
        iclcount = iclcount + 1;
        if (iclcount >= iterations / 10 && abs(angular_velocity) >= 10) {
          Message_send_Python();
          iclcount = 0;
        }
      }
      Execute_pwm(0);//stop the movement
      InstructionFlag = 0;
      Message_send_Python();
    }

    else if ((Straight_key == '9') || (Straight_key == '0')) {
      float velocity_inc;
      int reset_flag = 1;
      float sign_angle;
      if (Straight_key == '9') {
        if (Flag_python == 0) {
          Serial.println("---- '9' command ----");
          Serial.print("---- Start Extension Motion for ");
          Serial.print(StepAngularDistance);
          Serial.println(" degrees. ----");
          Serial.println("---------------------");
        }
        velocity_inc = MotionSpeed;
        sign_angle = 1.0;
        InstructionFlag = 0;
      }
      else {
        if (Flag_python == 0) {
          Serial.println("---- '0' command ----");
          Serial.print("---- Start Flexion Motion for ");
          Serial.print(StepAngularDistance);
          Serial.println(" degrees. ----");
          Serial.println("---------------------");
        }
        velocity_inc = -MotionSpeed;
        sign_angle = -1.0;
        InstructionFlag = 0;
      }

      int Flag_forward = 1;
      potentiometer_value = 1.0 * analogRead(potPin); //voltage meassurenment with 10bits resolution
      float Initial_angle = map_float(potentiometer_value, Cal_values[0], Cal_values[1], Cal_values[2], Cal_values[3]);//estimate initial angle used to meassure the increment
      Initial_angle = butter_filter_angle(Initial_angle, 1);

      while (Flag_forward) {
        Time_control(1);
        potentiometer_value = 1.0 * analogRead(potPin); //voltage meassurenment with 10bits resolution
        angle = map_float(potentiometer_value, Cal_values[0], Cal_values[1], Cal_values[2], Cal_values[3]);//estimate angle
        angle = butter_filter_angle(angle, 0); //filter angle
        angular_velocity = angle_derivative(angle); //estimate angular velocity
        angular_velocity = butter_filter_velocity(angular_velocity); //filter angular velocity
        pwm_value = PID_controller(velocity_inc, angular_velocity, Window_time_u, reset_flag);
        if ((sign_angle * (angle - Initial_angle) >= angle_inc - 2) || ((angle <= -90) && (velocity_inc < 0)) || ((angle >= -2) && (velocity_inc > 0))) {
          Flag_forward = 0;
          Execute_pwm(0);
        }
        else {
          Execute_pwm(pwm_value);
          reset_flag = 0;
        }
        Time_control(2);
      }
      Message_send_Python();
    }

    else if (Straight_key == '1') {

      Serial.println("---- '1' command ----");
      Serial.print("---- ON-OFF Extension with ");
      Serial.print(OnOffPWM);
      Serial.println(" PWM. ----");

      Execute_pwm(OnOffPWM);
      delay(200);
      Execute_pwm(0);
      delay(200);
      potentiometer_value = 1.0 * analogRead(potPin); //voltage meassurenment with 10bits resolution
      Serial.print("End PotValue = ");
      Serial.println(potentiometer_value);
      Serial.print("End Mapped Angle = ");
      Serial.println(map_float(potentiometer_value, MinPotVal, MaxPotVal, -90, 0));
      Serial.println("---------------------");
      InstructionFlag = 0;

    }
    else if (Straight_key == 'S') {

      do {
        potentiometer_value = 1.0 * analogRead(potPin); //voltage meassurenment with 10bits resolution
        angle = map_float(potentiometer_value, Cal_values[0], Cal_values[1], Cal_values[2], Cal_values[3]);//estimate angle
        angle = butter_filter_angle(angle, 0); //filter angle
        angular_velocity = angle_derivative(angle);//estimate angular velocity
        angular_velocity = butter_filter_velocity(angular_velocity);//filter angular velocity
        if (angle > -45) {
          Ref_angular_velocity = -Ref_angular_velocity_abs;
        }
        else {
          Ref_angular_velocity = Ref_angular_velocity_abs;
        }
        //---------------compute the pwm value, including its sign (manipulated variable)-------------------------
        pwm_value = PID_controller(Ref_angular_velocity, angular_velocity, Window_time_u, 0);
        //--------------------------------------------------------------------------------------------
        Execute_pwm(pwm_value);
      } while (abs(angle + 45) > 3);

      Execute_pwm(0);//stop the movement
      InstructionFlag = 0;
      Message_send_Python();

    }
    else if (Straight_key == '2') {

      Serial.println("---- '2' command ----");
      Serial.print("---- ON-OFF Flexion with ");
      Serial.print(OnOffPWM);
      Serial.println(" PWM. ----");

      Execute_pwm(-OnOffPWM);
      delay(200);
      Execute_pwm(0);
      delay(200);
      potentiometer_value = 1.0 * analogRead(potPin); //voltage meassurenment with 10bits resolution
      Serial.print("End PotValue = ");
      Serial.println(potentiometer_value);
      Serial.print("End Mapped Angle = ");
      Serial.println(map_float(potentiometer_value, MinPotVal, MaxPotVal, -90, 0));
      Serial.println("---------------------");
      InstructionFlag = 0;
    }
    else if (Straight_key == 'M') {
      Message_send_Python();
    }
  }
}

///////////////////This function is a second order filter for the angle//////////////////
float butter_filter_angle(float Theta_bfa, int flag_initialization) {
  static float Theta_filtered_bfa[4] = {Theta_bfa, Theta_bfa, Theta_bfa, Theta_bfa};//previous values of the filter
  static float Kc_bfa = Tc_Theta / (Ts * pi);//coefficient for the filter
  if (flag_initialization == 1) {
    for (int i_initialization = 0; i_initialization < 4; i_initialization++) {
      Theta_filtered_bfa[i_initialization] = Theta_bfa;
    }
  }

  Theta_filtered_bfa[0] = (pow(Kc_bfa, 3) * (3 * Theta_filtered_bfa[1] - 3 * Theta_filtered_bfa[2] + Theta_filtered_bfa[3]) +
                           2 * sq(Kc_bfa) * (2 * Theta_filtered_bfa[1] - Theta_filtered_bfa[2]) +
                           2 * Kc_bfa * Theta_filtered_bfa[1] +
                           Theta_bfa)
                          / (pow(Kc_bfa, 3) + 2 * sq(Kc_bfa) + 2 * Kc_bfa + 1);

  Theta_filtered_bfa[3] = Theta_filtered_bfa[2];
  Theta_filtered_bfa[2] = Theta_filtered_bfa[1];
  Theta_filtered_bfa[1] = Theta_filtered_bfa[0];

  return Theta_filtered_bfa[0];
}
////////////////////////////////////////////////////////////////////////////////////////

///////This is a second order approximation for the angular velocity (derivative of Theta)////////
float angle_derivative(float Theta_av) {
  static float Theta_av_array[3] = {Theta_av, Theta_av, Theta_av};//previous values of Theta_av
  float Theta_p_av;//derivative of Theta_av

  Theta_av_array[2] = Theta_av_array[1];
  Theta_av_array[1] = Theta_av_array[0];
  Theta_av_array[0] = Theta_av;

  Theta_p_av = (3 * Theta_av_array[0] - 4 * Theta_av_array[1] + 1 * Theta_av_array[2]) / (2 * Ts);
  Theta_p_av = min(max(Theta_p_av, -180), 180);

  return Theta_p_av;
}
//////////////////////////////////////////////////////////////////////////////////////////////////

////////////////This function is a third order filter for the angular velocity//////////////////
float butter_filter_velocity(float Theta_p_bfv) {

  static float Theta_filtered_bfv[4] = {Theta_p_bfv, Theta_p_bfv, Theta_p_bfv, Theta_p_bfv};//previous values of the filter
  static float Kc_bfv = Tc_Theta_p / (Ts * pi);//coefficient for the filter

  Theta_filtered_bfv[0] = (pow(Kc_bfv, 3) * (3 * Theta_filtered_bfv[1] - 3 * Theta_filtered_bfv[2] + Theta_filtered_bfv[3]) +
                           2 * sq(Kc_bfv) * (2 * Theta_filtered_bfv[1] - Theta_filtered_bfv[2]) +
                           2 * Kc_bfv * Theta_filtered_bfv[1] +
                           Theta_p_bfv)
                          / (pow(Kc_bfv, 3) + 2 * sq(Kc_bfv) + 2 * Kc_bfv + 1);

  Theta_filtered_bfv[3] = Theta_filtered_bfv[2];
  Theta_filtered_bfv[2] = Theta_filtered_bfv[1];
  Theta_filtered_bfv[1] = Theta_filtered_bfv[0];

  return Theta_filtered_bfv[0];
}
////////////////////////////////////////////////////////////////////////////////////////////

////////////////This function is a third order filter for the derivative of the error//////////////////
float butter_filter_e_p(float ep_bfe) {
  static float ep_filtered_bfe[4] = {ep_bfe, ep_bfe, ep_bfe, ep_bfe};//previous values of the filter
  static float Kc_bfe = Tc_e_p / (Ts * pi);//coefficient for the filter

  ep_filtered_bfe[0] = (pow(Kc_bfe, 3) * (3 * ep_filtered_bfe[1] - 3 * ep_filtered_bfe[2] + ep_filtered_bfe[3]) +
                        2 * sq(Kc_bfe) * (2 * ep_filtered_bfe[1] - ep_filtered_bfe[2]) +
                        2 * Kc_bfe * ep_filtered_bfe[1] +
                        ep_bfe)
                       / (pow(Kc_bfe, 3) + 2 * sq(Kc_bfe) + 2 * Kc_bfe + 1);

  ep_filtered_bfe[3] = ep_filtered_bfe[2];
  ep_filtered_bfe[2] = ep_filtered_bfe[1];
  ep_filtered_bfe[1] = ep_filtered_bfe[0];

  return ep_filtered_bfe[0];
}
////////////////////////////////////////////////////////////////////////////////////////////

////////This function changes the sign of the reference for the angular velocity/////////
float Ref_vel(float Theta_rv, int Initial_condition_rv) {
  static int Ref_Theta_rv = Initial_condition_rv;
  float Ref_velocity_rv;
  float e_theta_rv = Ref_Theta_rv - Theta_rv;
  float Trheshold = 2;

  if (abs(e_theta_rv) < Trheshold) {
    if (Ref_Theta_rv == Ref_Theta_mM[0]) {
      Ref_Theta_rv = Ref_Theta_mM[1];
      Ref_velocity_rv = Ref_angular_velocity_abs;
    }
    else {
      Ref_Theta_rv = Ref_Theta_mM[0];
      Ref_velocity_rv = -Ref_angular_velocity_abs;
    }
  }
  else {
    if (e_theta_rv > 0) {
      Ref_velocity_rv = Ref_angular_velocity_abs;
    }
    else {
      Ref_velocity_rv = -Ref_angular_velocity_abs;
    }
  }

  return Ref_velocity_rv;
}
//////////////////////////////////////////////////////////////////////////////////////////


////////This function is the angular velocity controller algorithm/////////
float PID_controller(float Ref_angular_velocity_pid, float Theta_p_pid, float  Window_time_u_pid, int reset_pid) {
  float Kd_pid_dynamic;
  float u_pr_pid;
  static float u_i_pid[2] = {0.0, 0.0};
  float u_d_pid;
  float u_pid;
  float e_pid;
  static float e_p_pid[2] = {0.0, 0.0};
  static float e_p_filter_pid[2] = {0.0, 0.0};
  //--------------------------------------------------------------------------------------
  static float n_samples_u_pid = Window_time_u_pid / Ts;
  static float lambda_u_pid = exp(log(0.1) / n_samples_u_pid);
  static float a_filter_u_pid = log(1 / lambda_u_pid);
  float u_aux_pid;
  static float u_aux_filtered_pid[2] = {0.0, 0.0};
  static float Ref_angular_velocity_prio_pid = Ref_angular_velocity_pid;
  if (reset_pid == 1) {
    u_i_pid[0] = 0.0;
    u_i_pid[1] = 0.0;
    e_p_pid[0] = 0.0;
    e_p_pid[1] = 0.0;
    e_p_filter_pid[0] = 0.0;
    e_p_filter_pid[1] = 0.0;
    u_aux_filtered_pid[0] = 0.0;
    u_aux_filtered_pid[1] = 0.0;
  }
  //--------------------------------------------------------------------------------------

  e_pid = Ref_angular_velocity_pid - Theta_p_pid;//compute error in angular velocity

  Dynamic_contrains(Ref_angular_velocity_pid);//This function changes the constrains of pwm depending on the movement direction

  u_pr_pid = Kp_pid * e_pid; //compute proportional manipulation
  u_pr_pid = min(max(u_pr_pid, pwm_constrains_dyn_mM[0]), pwm_constrains_dyn_mM[1]);//saturate proportional manipulation

  e_p_pid[0] = -Theta_p_pid - e_p_pid[1]; //difference in error (derivative multiplied by Ts)
  e_p_pid[1] = -Theta_p_pid;

  //-------------------First order filter for e_p_pid-------------------------------
  e_p_filter_pid[0] = butter_filter_e_p(e_p_pid[0] / Ts);
  e_p_filter_pid[1] = e_p_filter_pid[0];
  //--------------------------------------------------------------------------------
  Kd_pid_dynamic = Kd_pid / (1 + 2 * (tanh((3 * abs(e_pid) / 4 - 1.5) / 2) + 1) / 2);//dynamic gain for the derivative
  u_d_pid = Kd_pid_dynamic * e_p_filter_pid[0];//compute proportional manipulation
  u_d_pid = min(max(u_d_pid, pwm_constrains_dyn_mM[0]), pwm_constrains_dyn_mM[1]);//saturate proportional manipulation

  u_aux_pid = u_pr_pid + u_d_pid;//auxiliary variable containing the sum of proportional an derivative manipulation
  u_aux_pid = min(max(u_aux_pid, pwm_constrains_dyn_mM[0]), pwm_constrains_dyn_mM[1]);//saturate the auxiliary variable

  u_aux_filtered_pid[0] = (u_aux_filtered_pid[1] + a_filter_u_pid * u_aux_pid) / (1 + a_filter_u_pid);//filter for the auxiliary variable, to avoid error and error derivative noise
  u_aux_filtered_pid[1] = u_aux_filtered_pid[0];//updates the previous value of the filter

  //------This part of the code resets the integrator when movement direction is changed---------
  //------it improves the ideal behaviour of the velocity when a change is made, but it -----------
  //------could decrease the comfort for the user due to high accelaration-----------------------
  /*if (abs(Ref_angular_velocity_prio_pid - Ref_angular_velocity_pid) > 1) {
    Ref_angular_velocity_prio_pid = Ref_angular_velocity_pid;
    u_i_pid[0] = 0;
    u_i_pid[1] = 0;
    }*/
  //----------------------------------------------------------------------------------------

  //-------This part of the code dinamycally saturates the value of the integrator part-----
  //-------of PID, so that lags in the behaviour of the integrator are decreased------------
  if ((u_aux_filtered_pid[0] + u_i_pid[1] > pwm_constrains_dyn_mM[1]) && (e_pid > 0)) {
    u_i_pid[0] = u_i_pid[1];
  }
  else if ((u_aux_filtered_pid[0] + u_i_pid[1] < pwm_constrains_dyn_mM[0]) && (e_pid < 0)) {
    u_i_pid[0] = u_i_pid[1];
  }
  else {
    u_i_pid[0] = Ts * Ki_pid * e_pid + u_i_pid[1]; //compute integral manipulation
    u_i_pid[0] = min(max(u_i_pid[0], pwm_constrains_dyn_mM[0]), pwm_constrains_dyn_mM[1]); //saturate integral manipulation
    u_i_pid[1] = u_i_pid[0];
  }
  //-----------------------------------------------------------------------------------------

  u_pid = u_i_pid[0] + u_aux_filtered_pid[0];//the whole PID manipulation is computed
  u_pid = min(max(u_pid, pwm_constrains_dyn_mM[0]), pwm_constrains_dyn_mM[1]);//saturate PID manipulation
  u_pid = Static_manipulation(u_pid, Theta_p_pid, Ref_angular_velocity_pid);//Static compensation is added to the PID manipulation

  return u_pid;
}
//////////////////////////////////////////////////////////////////////////////////////////

/////This function adds static compensation is to the manipulation computed by the PID////////
float Static_manipulation(float u_sm, float Theta_p_sm, float Ref_angular_velocity_sm) {
  float u_modified_sm;

  if (Ref_angular_velocity_sm > 0) {
    if (abs(u_sm) > 0.000001) {
      u_modified_sm = u_sm + pwm_minimum_movement[1];
    }
    else {
      u_modified_sm = u_sm;
    }
  }
  else {
    if (abs(u_sm) > 0.000001) {
      u_modified_sm = u_sm + pwm_minimum_movement[0];
    }
    else {
      u_modified_sm = u_sm;
    }
  }
  return u_modified_sm;
}
//////////////////////////////////////////////////////////////////////////////////////////////

////////This function implements the pwm value considering the direction of movement/////////
void Execute_pwm(float pwm_epw) {
  if (pwm_epw > 0) {
    digitalWrite(d1Pin, HIGH);
    analogWrite(pwmPin, pwm_epw);
  }
  else if (pwm_epw < 0) {
    digitalWrite(d1Pin, LOW);
    analogWrite(pwmPin, abs(pwm_epw));
  }
  else {
    digitalWrite(d1Pin, LOW);
    analogWrite(pwmPin, 0);
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////

////////This function linearly maps val_mf onto val_f_mf using the ranges especified as inputs and with float presicion /////////
float map_float(float val_mf, float min_val_mf, float max_val_mf, float min_val_f_mf, float max_val_f_mf) {
  float val_f_mf;
  float a_mf;

  a_mf = (val_mf - min_val_mf) / (max_val_mf - min_val_mf);
  val_f_mf = max_val_f_mf * a_mf + min_val_f_mf * (1 - a_mf);

  return val_f_mf;
}
////////////////////////////////////////////////////////////////////////////////////////////////

////////This function makes the sampling time of the controller equal to Ts /////////
void Time_control(int bf_tc) {
  static float Time_stamp = 0;
  if (bf_tc == 1) {
    Time_stamp = micros();
  }
  else {
    delayMicroseconds(Ts * 1000000 - (micros() - Time_stamp));
  }
}
/////////////////////////////////////////////////////////////////////////////////////

////////This function changes the contrains of pwm depending on movement direction /////////
void Dynamic_contrains(float Ref_angular_velocity_dc) {
  if (Ref_angular_velocity_dc > 0) {
    pwm_constrains_dyn_mM[0] = min(pwm_constrains_mM[0] - pwm_minimum_movement[1], 0);
    pwm_constrains_dyn_mM[1] = max(pwm_constrains_mM[1] - pwm_minimum_movement[1], 0);
  }
  else {
    pwm_constrains_dyn_mM[0] = min(pwm_constrains_mM[0] - pwm_minimum_movement[0], 0);
    pwm_constrains_dyn_mM[1] = max(pwm_constrains_mM[1] - pwm_minimum_movement[0], 0);
  }
}

////////This function returns 1 when "Timer_ts surpassed" the threshold "threshold_ts" /////////
int Timeout_step2(float threshold_ts2, int reset_ts2) {
  int Flag_timerout_stLK2 = 0;
  static float Timer_ts2 = 0;
  static float Time_stamp_ts2 = 0;
  if (reset_ts2 == 1) {
    Time_stamp_ts2 = micros();
    Timer_ts2 = 0;
  }
  else {
    Timer_ts2 = max(micros() - Time_stamp_ts2, 0) / 1000000;//This part relies on the correct implementation of the  sampling time "Sampling_time"
  }

  if (Timer_ts2 >= threshold_ts2) {
    Flag_timerout_stLK2 = 1;
  }
  else {
    Flag_timerout_stLK2 = 0;
  }
  return Flag_timerout_stLK2;
}

/////////////////////////////////////////////////////////////////////////////////////

int Message_received_Python() {
  int Data_2bytes = 0;
  int Flag_correctness = 0;

  Data_2bytes = Serial.read();
  Data_2bytes = Data_2bytes * 256 + Serial.read();
  Serial.write(highByte(Data_2bytes));
  Serial.write(lowByte(Data_2bytes));

  Timeout_step2(time_out_handshake, 1);

  while ((Timeout_step2(time_out_handshake, 0) == 0) && (Serial.available() < 2)) {
  }

  if (Serial.available() == 2) {
    Flag_correctness = Serial.read();
    Flag_correctness = Flag_correctness * 256 + Serial.read();
  }
  else {
    while (Serial.available()) {
      Serial.read();
    }
  }

  if (Flag_correctness == 1777) {
    return Data_2bytes;
  }
  else {
    return 'N';
  }
}

/////////////////////////////////////////////////////////////////////////////////////

void Message_send_Python() {
  int Data_2bytes = 1888;
  Serial.write(highByte(Data_2bytes));
  Serial.write(lowByte(Data_2bytes));
}

////////////////////////////////////////////////////////////////////////////////////////////
