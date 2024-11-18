#include <Servo.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_NeoPixel.h>
#include <DHT.h>
#include <HX711.h>
#include <EEPROM.h>  // Memorizzazione dei dati
#include <SoftwareSerial.h>  // Comunicazione Bluetooth

// Pin assegnati
#define FOOD_SERVO_PIN 9
#define WEIGHT_SENSOR_DOUT 4
#define WEIGHT_SENSOR_SCK 5
#define DHT_PIN 6
#define DHT_TYPE DHT22
#define GPS_RX 10
#define GPS_TX 11
#define RGB_PIN 7
#define MOTION_SENSOR_PIN 8
#define LASER_PIN 3
#define SOUND_SENSOR_PIN A0
#define LASER_X_PIN 11
#define LASER_Y_PIN 12
#define BUZZER_PIN 13 // Aggiungiamo un buzzer per segnali sonori
#define BATTERY_PIN A1 // Pin per monitoraggio della batteria

// Oggetti
Servo foodServo;
Servo laserServoX, laserServoY; // Servomotori per il movimento laser
DHT dht(DHT_PIN, DHT_TYPE);
HX711 scale;
Adafruit_NeoPixel rgbLED(1, RGB_PIN, NEO_GRB + NEO_KHZ800);

// Variabili globali
float temperature = 0.0;
float humidity = 0.0;
long foodWeight = 0;
bool motionDetected = false;
int laserPosX = 90;
int laserPosY = 90;
int lastSoundLevel = 0;
unsigned long lastMotionTime = 0; // Per il monitoraggio dell'attività
int autoFeedInterval = 3600000; // Intervallo per la distribuzione automatica del cibo (1 ora)
unsigned long lastFeedTime = 0; // Ultima volta che è stato distribuito il cibo
int lastBatteryLevel = 0; // Memorizza l'ultimo stato della batteria

// Variabili di memoria EEPROM
int lastTemperature = 0;
int lastHumidity = 0;
long lastFoodWeight = 0;

SoftwareSerial btSerial(2, 3);  // Comunicazione Bluetooth (RX, TX)

void setup() {
    Serial.begin(9600); // Comunicazione seriale
    btSerial.begin(9600); // Comunicazione Bluetooth

    foodServo.attach(FOOD_SERVO_PIN);
    laserServoX.attach(LASER_X_PIN);
    laserServoY.attach(LASER_Y_PIN);
    pinMode(MOTION_SENSOR_PIN, INPUT);
    pinMode(LASER_PIN, OUTPUT);
    pinMode(SOUND_SENSOR_PIN, INPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(BATTERY_PIN, INPUT);

    // Inizializzazione sensori
    dht.begin();
    scale.begin(WEIGHT_SENSOR_DOUT, WEIGHT_SENSOR_SCK);
    rgbLED.begin();

    // Stato iniziale
    foodServo.write(0);
    laserServoX.write(laserPosX); // Posizione iniziale laser
    laserServoY.write(laserPosY); // Posizione iniziale laser
    digitalWrite(LASER_PIN, LOW);
    rgbLED.setPixelColor(0, rgbLED.Color(0, 0, 255)); // Blu di default
    rgbLED.show();

    // Carica dati dalla EEPROM (se presenti)
    loadDataFromEEPROM();

    Serial.println("SmartPet Ultra avviato!");
    btSerial.println("SmartPet Ultra - Pronto per il controllo remoto!");
}

void loop() {
    // Lettura sensori ambientali
    temperature = dht.readTemperature();
    humidity = dht.readHumidity();
    foodWeight = scale.get_units();

    // Lettura movimento
    motionDetected = digitalRead(MOTION_SENSOR_PIN);

    // Lettura suoni
    int soundLevel = analogRead(SOUND_SENSOR_PIN);
    if (soundLevel > 500 && soundLevel != lastSoundLevel) {
        Serial.println("Suono rilevato: potrebbe essere il tuo animale!");
        lastSoundLevel = soundLevel;
    }

    // Lettura livello batteria
    int batteryLevel = analogRead(BATTERY_PIN);
    if (batteryLevel < 500) {
        Serial.println("Batteria scarica! Ricarica il dispositivo.");
    }

    // Aggiorna LED RGB in base alla temperatura
    updateLighting(temperature);

    // Comandi automatizzati
    if (motionDetected) {
        activateLaser();
        moveLaserInteractive(); // Muovi il laser dinamicamente
        lastMotionTime = millis(); // Aggiorna il timestamp dell'ultimo movimento
    } else {
        deactivateLaser();
    }

    // Controlla se è il momento di distribuire il cibo
    autoFeedControl();

    // Verifica il peso del cibo per avvisare quando è basso
    checkFoodLevel();

    // Funzioni di risparmio energetico
    powerSavingMode();

    // Ricezione comandi via Bluetooth
    receiveBluetoothCommand();

    // Salvataggio stato ogni minuto
    if (millis() - lastFeedTime >= 60000) {
        saveDataToEEPROM();
    }

    // Stampa stato su Serial Monitor
    printStatus();

    delay(1000);
}

void distributeFood() {
    Serial.println("Distribuzione del cibo in corso...");
    foodServo.write(90);
    delay(2000);
    foodServo.write(0);
    Serial.println("Cibo distribuito.");
}

void activateLaser() {
    Serial.println("Laser attivato!");
    digitalWrite(LASER_PIN, HIGH);
}

void deactivateLaser() {
    Serial.println("Laser disattivato.");
    digitalWrite(LASER_PIN, LOW);
}

void updateLighting(float temp) {
    if (temp < 20) {
        rgbLED.setPixelColor(0, rgbLED.Color(0, 0, 255)); // Blu
    } else if (temp >= 20 && temp < 30) {
        rgbLED.setPixelColor(0, rgbLED.Color(0, 255, 0)); // Verde
    } else {
        rgbLED.setPixelColor(0, rgbLED.Color(255, 0, 0)); // Rosso
    }
    rgbLED.show();
}

void checkFoodLevel() {
    if (foodWeight < 50) {  // Soglia per avviso di cibo basso
        Serial.println("Attenzione: Cibo in esaurimento!");
        soundAlarm();
    }
}

void printStatus() {
    Serial.println("===== Stato attuale =====");
    Serial.print("Temperatura: ");
    Serial.print(temperature);
    Serial.println(" °C");

    Serial.print("Umidità: ");
    Serial.print(humidity);
    Serial.println(" %");

    Serial.print("Peso cibo rimanente: ");
    Serial.print(foodWeight);
    Serial.println(" grammi");

    Serial.print("Movimento: ");
    Serial.println(motionDetected ? "Rilevato" : "Non rilevato");

    // Stato del laser
    if (digitalRead(LASER_PIN) == HIGH) {
        Serial.println("Laser: Attivo");
    } else {
        Serial.println("Laser: Inattivo");
    }
}

void moveLaserInteractive() {
    // Muovi il laser in modo interattivo
    for (int i = 0; i < 3; i++) {
        int posX = random(30, 150); // Posizione casuale sull'asse X
        int posY = random(30, 150); // Posizione casuale sull'asse Y

        laserServoX.write(posX);
        laserServoY.write(posY);

        Serial.print("Laser spostato a: X=");
        Serial.print(posX);
        Serial.print(" Y=");
        Serial.println(posY);

        delay(500); // Breve pausa tra i movimenti
    }
}

void smoothLaserMovement(int targetX, int targetY) {
    // Muove il laser con un movimento più fluido (sinusoidale)
    int currentX = laserServoX.read();
    int currentY = laserServoY.read();

    for (int i = 0; i <= 100; i++) {
        int xPos = currentX + (targetX - currentX) * i / 100;
        int yPos = currentY + (targetY - currentY) * i / 100;
        laserServoX.write(xPos);
        laserServoY.write(yPos);

        delay(15);  // Piccola pausa per creare il movimento fluido
    }
}

void playSoundEffect() {
    // Suona un allarme (buzzer)
    tone(BUZZER_PIN, 1000, 500); // Tonalità 1000Hz per 500ms
}

void soundAlarm() {
    // Attiva il suono di allarme per cibo basso
    playSoundEffect();
    rgbLED.setPixelColor(0, rgbLED.Color(255, 0, 0)); // LED rosso lampeggiante
    rgbLED.show();
}

void autoFeedControl() {
    if (millis() - lastFeedTime >= autoFeedInterval) {
        distributeFood();
        lastFeedTime = millis(); // Aggiorna l'orario dell'ultima distribuzione
    }
}

void powerSavingMode() {
    // Se non c'è movimento da più di 10 minuti, spegni il laser e riduci l'uso di energia
    if (millis() - lastMotionTime > 600000) {
        deactivateLaser();
        Serial.println("Modalità risparmio energetico attivata.");
    }
}

void receiveBluetoothCommand() {
    // Comandi via Bluetooth
    if (btSerial.available()) {
        char command = btSerial.read();
        if (command == 'L') {
            activateLaser();
        } else if (command == 'l') {
            deactivateLaser();
        } else if (command == 'F') {
            distributeFood();
        }
    }
}

void saveDataToEEPROM() {
    // Salva dati in EEPROM per il monitoraggio storico
    EEPROM.write(0, temperature);
    EEPROM.write(1, humidity);
    EEPROM.write(2, foodWeight);
    EEPROM.write(3, batteryLevel);

    Serial.println("Dati salvati in EEPROM.");
}

void loadDataFromEEPROM() {
    // Carica dati da EEPROM all'avvio
    lastTemperature = EEPROM.read(0);
    lastHumidity = EEPROM.read(1);
    lastFoodWeight = EEPROM.read(2);
}