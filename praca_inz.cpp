#include <Arduino.h>
// Dołączanie bibliotek
#include <ESP32Servo.h> //Biblioteka potrzebna do działania Serwa
#include <TFT_eSPI.h> //Biblioteka potrzebna do działania wyświetlacza
#include <SPI.h> //Biblioteka potrzebna do działania wyświetlacza
#include <Wire.h>  //Biblioteka potrzebna do działania LiDAR'u
#include <VL53L0X.h> //Biblioteka potrzebna do działania LiDAR'u
#include <WiFi.h> //Biblioteka potrzebna do działania komunikacji z Firebase
#include <FirebaseESP32.h> //Biblioteka potrzebna do działania komunikacji z Firebase

//Definiowanie zmiennych
#define HIGH_SPEED //Zmienna do uruchomienia szybszego trybu działania LiDAR'u
#define WIFI_SSID "praca_inz" //Nazwa sieci WiFI
#define WIFI_PASSWORD "lidar_firebase#" //Hasło do sieci WiFi
#define FIREBASE_HOST "skanowanie-terenu-default-rtdb.europe-west1.firebasedatabase.app/" //Nazwa hosta Firebase
#define FIREBASE_AUTH "y4mkHhX3PSJzDRC6MPjrvWwDlsnLvSYNdZaOo9ta" //Zmienna uwierzytelniająca
#define USER_EMAIL "LiDAR.Praca.inz@gmail.com"
#define USER_PASSWORD "#lidarpracainz"
#define API_KEY "AIzaSyCLjtngIQpImHx4h-eJu4Qu3MAhwE4Y0eU"


//Deklaracja zmiennych
  VL53L0X sensor; //LiDAR
  Servo myservo; //Serwo
  FirebaseAuth auth; //Obiekt zawierajacy dane do zweryfikowania tej aplikacji w Firebase
  FirebaseConfig config; //Obiekt zawierajcy konfiguracje polaczenia z Firebase
  FirebaseData fbdo; //Obiekt typu FirebaseData do komunikacji z Firebase
  TFT_eSPI tft = TFT_eSPI();  //Wywołanie biblioteki
   
  byte w = tft.width(); //Szerokość wyświetlacza
  byte h = tft.height(); //Wysokość wyświetlacza
  byte Buzzer = 14; //Pin brzęczyka
  byte servoPin = 32; //Pin serwa
  int distance = 0; //Mierzony dystans
  int x,y; //Współrzedne x oraz y przeszkody
  float alpha = 30,beta = 180 - alpha;
  bool servo_direction = false;
  //X[] oraz Y[] to zmienne pomocnicze do tworzenia lini na wyswietlaczu, reprezentujacych położenie serwa
  byte Y[] = {82, 78, 74, 70, 65, 61, 56, 51, 45, 39, 33, 26, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 26, 33, 39, 45, 51, 56, 61, 65, 70, 74, 78, 82};
  byte X[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 9, 14, 19, 24, 28, 33, 38, 42, 47, 52, 56, 61, 66, 71, 75, 80, 85, 89, 94, 99, 104, 108, 113, 118, 122, 127, 132, 136, 141, 146, 151, 155, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160};
  byte x_lcd[61] = {0}; //Zmienna do przechowywania położenia przeszkód na wyświetlaczu w osi x 
  byte y_lcd[61] = {0}; //Zmienna do przechowywania położenia przeszkód na wyświetlaczu w osi y 
  byte xmap,ymap;
  String path  = "/Baza danych/Wspolrzedne"; //Scieżka do wezła bazy Firebase
  String path2 = "/Baza danych/Komunikacja"; //Scieżka do wezła bazy Firebase
  bool Problem = false; //Zmienna do sprawdzania czy dane zostały przesłane do Firebase
  bool send_alarm = false; //Zmienna informująca o wystąpeniu alarmu, przesyłana do Firebase
    
  FirebaseJsonArray arr_x, arr_y, distance_lidar, servo_deg, gps, imu;
  FirebaseJson json, json2;

void draw_map(byte alpha, int distance);
void send_to_RTDB();
void clean_screen(int alpha) ;
void alarm();

void setup() {
  Serial.begin (115200);  //Ustawienie prędkości komunikacji mikrokotrolera z komupterem poprzez UART
  
  Wire.begin(); //Uruchumienie komunikacji poprzez I2C
  sensor.setTimeout(500); //Ustawienie limitu czasu na połączenie ESP32 z LiDAR'em
  if (!sensor.init()) // Sprawdzenie czy udało się połączyć z LiDAR'em
  {
    Serial.println("Nie udało sie zainicjalizować LiDAR'u!");
    while (1) {} //Pętla nieskończona do której program wchodzi gdy nie udało się połączyć z LiDAR'em
  }
  #if defined HIGH_SPEED //Jeśli zdefiniowana jest zmienna HIGH_SPEED
  //Zredukowanie limitu czasu jaki może zając pomiar odległosci do 20 ms (domyślna wartość to 33 ms) 
    sensor.setMeasurementTimingBudget(20000);
  #elif defined HIGH_ACCURACY //Jeśli zdefiniowana jest zmienna HIGH_ACCURACY
  //Zwiększenie limitu czasu do 200 ms  
    sensor.setMeasurementTimingBudget(200000);
  #endif
  
  config.host = FIREBASE_HOST; //Ustawienie hosta Firebase 
  config.api_key = API_KEY; //Uswtawienie klucza apliakcji
  auth.user.email = USER_EMAIL; //Ustawienie emaila użytkownika jakim ten progoram weryfikuje się w Firebase
  auth.user.password = USER_PASSWORD; //Ustawienie hasła użytkownika jakim ten progoram weryfikuje się w Firebase
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD); //Rozpoczęcie połączenia z WiFi
  Serial.print("Łączenie z Wi-Fi");       
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print("."); delay(300);// Co 300 ms wypisywana jest kropka jeśli nie udało się jeszcze połączyć z WiFi 
  }    
  Serial.println();
  Serial.print("\n Udało się połączyć z siecią o IP: ");
  Serial.println(WiFi.localIP()); //Wypisania IP sieci
  Serial.println();
  Firebase.begin(&config, &auth); //Rozpoczęcie połączenia z Firebase
  Firebase.reconnectWiFi(true); //Włączenie opcji ponownego połączenia z WiFi
  Firebase.setwriteSizeLimit(fbdo, "tiny" ); //Ustawienie limitu czau na przesłanie danych do bazy na 1 s 
  //Ustawienie wartości alarmowej w bazie Firebase jako "false"  
  Serial.printf("Set bool... %s\n", Firebase.setBool(fbdo, path2+"/Alarm", send_alarm) ? "ok" : fbdo.errorReason().c_str());
  
  //Alakokacja timerów potrzebnych do sterowania serwomechanizmem
  ESP32PWM::allocateTimer(0); 
  ESP32PWM::allocateTimer(1); 
  ESP32PWM::allocateTimer(2); 
  ESP32PWM::allocateTimer(3);
  myservo.setPeriodHertz(50);    
  //Ustawienie częstotliwości serwa
  myservo.attach(servoPin, 500, 2500); //Przyłączenie serwa do ESP32 i ustawienie zekresu ruchu serwa od 0 do 180 stopni
  myservo.write(150); //Ustawienie serwa w pozycji początkowej
  
  tft.init(); //Rozpoczęcie komunikacji z wyświetlaczem
  tft.setRotation(1); //Ustawienie orientacji wyświetlacza
  tft.fillScreen(TFT_BLACK); //Wypełnienie wyświetlacza czarnym kolorem
  delay(300); //Oczekiwanie aż serwo ustawi się w początkowej pozycji
    
  pinMode (Buzzer, OUTPUT); //Ustawienie pinu do jakiego podłączony jest brzęczyk jako wyjście     
}


void loop() {
  for (int i=0; i<=60; i++)
  {
    if(servo_direction) alpha = 150 - i*2; //alpha to kat wychylenia serwa
    else alpha = 30 + i*2; 
    //Wykonanie pomiaru odleglosci
    distance = sensor.readRangeSingleMillimeters()/10;
    if (sensor.timeoutOccurred()) 
    { // Timeout
      distance = -1;
      Serial.println(" TIMEOUT");
    }
    if (distance >0 && distance <= 120)
    { // Obiekt w zasięgu LiDAR-u
      beta = 180 - alpha;
      x = distance * cos(beta * 3.14159/180); //Wspolrzedne polozenia obiektu w osi x 
      y = distance * sin(beta * 3.14159/180); //Wspolrzedne polozenia obiektu w osi y 
      if (distance <= 20) alarm(); 
      else if (distance <= 50)  digitalWrite(Buzzer,HIGH);  // dźwięk ostrzegawczy
    }
    else    
    { // Obiekt poza zasięgiem
      x = 0;
      y = 0;  
    }
    //Ustawienie serwa w pozycji do kolejnego pomiaru
    if(servo_direction && i < 60) myservo.write(182 - alpha); //Obrot serwa od prawej do lewej
    else if(!servo_direction && i < 60) myservo.write(178 - alpha); //Obrot serwa od lewej do prawej   
    //Wypelnianie tablic przesylanych do bazy danych
    distance_lidar.add(distance);
    servo_deg.add(alpha);
    arr_x.add(x);
    arr_y.add(y);
    // Przeskalowanie współrzędnej y: [0, 128] cm => [128, 0] pikseli
    ymap = map(y,0,128,128,0);  
    // Przeskalowanie współrzędnej x: [-80, 80] cm => [0, 128] pikseli
    if (x >= 0)  xmap = map(x, 0, 80, h/2, h);
    else if (x < 0) xmap = map(x, -80, 0, 0, h/2); 
    x_lcd[i] = xmap;
    y_lcd[i] = ymap;

    digitalWrite(Buzzer,LOW); //Wylaczenie brzeczyka

    if(servo_direction) //dla obrotow serwa od prawej do lewej
    {
      //Tworzenie lini reprezentujacej polozenie serwa
      tft.drawLine(h/2, w, X[60-i], Y[60-i], TFT_BLUE);
      //USuwanie poprzednio narysowanej lini
      if(i >0)  tft.drawLine(h/2, w, X[61-i], Y[61-i], TFT_BLACK);
      //Tworzenie mapy terenu 
      draw_map(alpha, distance);
    }
    else  //dla obrotow serwa od lewej do prawej
    {
      tft.drawLine(h/2, w, X[i], Y[i], TFT_BLUE);
      if(i >0)  tft.drawLine(h/2, w, X[i-1], Y[i-1], TFT_BLACK);
      draw_map(alpha, distance);
    }
    Serial.println(alpha);  
    Serial.println(distance); 
  }
  //Zakonczony cykl skanowania
  
  //Wypelnianie tablic przesylanych do RTDB Firebase
  gps.add("Szerokosc Geograficzna, Dlugosc geograficzna");
  imu.add("accX, accY, accZ, gyroX, gyroY, gyroZ, magX, magY, magZ");
  //Wysylanie danych do Firebase
  send_to_RTDB();
  //Usuwanie ostatniej narysowanej linii 
  if(servo_direction) tft.drawLine(h/2, w, X[0], Y[0], TFT_BLACK);
  else tft.drawLine(h/2, w, X[60], Y[60], TFT_BLACK);
  //Usuwanie z wyswietlacza punktow reprezentujacych polozenie obiektow
  clean_screen(alpha);
  servo_direction = !servo_direction; //Zmiana kierunku obrotu serwa
}


void alarm()
{  
  digitalWrite(Buzzer, HIGH); //Włączenie brzęczyka
  send_alarm =true;
  //Ustawianie zmiennej alarmowej w Firebase
  Serial.printf("Set bool ... %s\n", Firebase.setBool(fbdo, path2 +"/Alarm", send_alarm) 
  ? "ok" : fbdo.errorReason().c_str());
  
  while(true)
  {
    tft.setCursor(0, 0, 2);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.print((String)"Odl."+distance+"cm"); //Wypisywanie odległości na wyświetlaczu
       
    digitalWrite(Buzzer, HIGH); //Włączenie brzęczyka
    delay(250);
    digitalWrite(Buzzer, LOW);  //Wyłączenie brzęczyka
    delay(125);
    
    distance = sensor.readRangeSingleMillimeters()/10; //Dokonywanie pomiaru odległości
    if (distance > 20) 
    {
      send_alarm =false;
      //Ustawianie zanegowanej zmiennej alarmowej w Firebase  
      Serial.printf("Set bool ... %s\n", Firebase.setBool(fbdo, path2+"/Alarm", send_alarm) 
      ? "przesłany" : fbdo.errorReason().c_str()); 
      break;
    }
  }
}


void clean_screen(int alpha) 
{
  for (int j=0; j<=60; j++) 
  {
    if (y_lcd[j]!=0) tft.fillCircle(x_lcd[j],y_lcd[j],3,TFT_BLACK);
    x_lcd[j]=0;
    y_lcd[j]=0;
  }
}


void send_to_RTDB()
{
  //Tworzenie obiektu typu JSON, zawierajacego dane dotyczące wykrytych przeszkód, 
  //oraz położenie i orientacje urzadzenia w terenie
  json.set("Serwo kat", servo_deg);
  json.set("LiDAR odleglosc", distance_lidar);
  json.set("GPS", gps);
  json.set("IMU", imu);
  //Wysyłanie jsona do bazy danych Firebase
  Serial.printf("Set JSON ... %s\n", Firebase.setJSON(fbdo, "/Baza danych/Szczegolowe dane", json) 
  ? "ok" : fbdo.errorReason().c_str());     

  //Tworzenie obiektu typu JSON, zawierajacego współrzędne położenia wykrytych przeszkód
  json2.set("X", arr_x);
  json2.set("Y", arr_y);
  //Wysyłanie jsona do bazy danych Firebase
  Serial.printf("Set JSON ... %s\n", Firebase.setJSON(fbdo, "/Baza danych/Wspolrzedne", json2) 
  ? "ok" : fbdo.errorReason().c_str());

  //Czyszczenie tablic
  servo_deg.clear(); 
  distance_lidar.clear();
  gps.clear();
  imu.clear();
  arr_x.clear();
  arr_y.clear();
}


void draw_map(byte alpha, int distance) { 
  byte j;
  if (distance < 120) 
  {
    tft.setCursor(0, 0, 2); //Ustawienie położenia dla następnego napisu
    tft.setTextColor(TFT_GREEN,TFT_BLACK); //Ustawienie koloru i koloru tła następnego napisu
    tft.print((String)"Odl."+distance+"cm"); //Wyświetlenie odległości od przeszkody
  }
  else 
  {
    tft.setCursor(0, 0, 2); //Ustawienie położenia dla następnego napisu
    tft.setTextColor(TFT_GREEN, TFT_BLACK); //Ustawienie koloru i koloru tła następnego napisu
    tft.print((String)"Odl."+"PZ"+"    "); //Wyświetlenie odległości jako poza zasięgiem (PZ)
  }
  tft.setCursor(w-27, 0, 2); //Ustawienie położenia dla następnego napisu
  tft.setTextColor(TFT_BLUE, TFT_BLACK); //Ustawienie koloru i koloru tła następnego napisu
  tft.print((String)"Kat: "+alpha+"  "); //Wyświetlenie kątu wychylenia serwomechanizmu

  tft.drawCircle(h/2, w, 30, 0x07DF); //Rysowanie okręgu o promieniu 30 pikseli
  tft.drawCircle(h/2, w, 60, 0x07DF); //Rysowanie okręgu o promieniu 60 pikseli
  tft.drawCircle(h/2, w, 90, 0x07DF); //Rysowanie okręgu o promieniu 90 pikseli
  tft.setTextColor(TFT_GREEN, TFT_BLACK); //Ustawienie koloru i koloru tła następnego napisu
  tft.drawCentreString("90", h/2, 43, 1); //Napis wyśrodkowany w punkcie o współrzednych h/2,43
  tft.setTextColor(TFT_GREEN, TFT_BLACK); //Ustawienie koloru i koloru tła następnego napisu
  tft.drawCentreString("60", h/2, 73, 1); //Napis wyśrodkowany w punkcie o współrzednych h/2,73
  tft.setTextColor(TFT_GREEN, TFT_BLACK); //Ustawienie koloru i koloru tła następnego napisu
  tft.drawCentreString("30", h/2, 103, 1); //Napis wyśrodkowany w punkcie o współrzednych h/2,103
  //Rysowanie wszystkich punktów z danego cyklu
  for(j = 0; j <= 60; j++) 
  {
    if(y_lcd[j]!=0) tft.fillCircle(x_lcd[j], y_lcd[j], 3, 0xFFE0);            
  }
  tft.fillCircle(xmap, ymap, 3, 0xF81F);
}  
