#include <SPI.h>

// ==========================================
// CONFIGURACIÓN DE PINES
// ==========================================
#define AD9833_FSYNC 10  // CS / FSYNC
#define AD9833_SCK   12  // SCK / CLK
#define AD9833_MOSI  11  // MOSI / DAT
#define SQR_PIN       7  // Cuadrada Hardware (GPIO 7)
#define MUX_CTRL_PIN 16  // Control Transistor MUX (GPIO 16)

const float MCLK = 25000000.0; // Cristal de 25 MHz

enum WaveType { SINE, TRIANGLE, SQUARE };
WaveType currentWave = SINE;
float currentFreq = 1000.0;

void writeAD9833(uint16_t data) {
  digitalWrite(AD9833_FSYNC, LOW);
  SPI.transfer16(data);
  digitalWrite(AD9833_FSYNC, HIGH);
}

void updateSignal() {
  // Ajuste de límites de seguridad
  if (currentFreq < 10.0) currentFreq = 10.0;
  if (currentFreq > 10000.0) currentFreq = 10000.0;

  if (currentWave == SQUARE) {
    // 1. Conmuta MUX al Canal AX (Pin 14) saturando el transistor
    digitalWrite(MUX_CTRL_PIN, HIGH); // Pin 11 del MUX cae a 0V

    // 2. Genera la onda cuadrada en GPIO 7
    #if defined(ESP_ARDUINO_VERSION) && ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
      ledcAttach(SQR_PIN, (uint32_t)currentFreq, 8);
      ledcWrite(SQR_PIN, 128); // 50% Duty Cycle
    #else
      ledcSetup(0, (uint32_t)currentFreq, 8);
      ledcAttachPin(SQR_PIN, 0);
      ledcWrite(0, 128);
    #endif
  } 
  else {
    // 1. Detener salida de cuadrada en GPIO 7
    #if defined(ESP_ARDUINO_VERSION) && ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
      ledcDetach(SQR_PIN);
    #else
      ledcDetachPin(SQR_PIN);
    #endif
    pinMode(SQR_PIN, OUTPUT);
    digitalWrite(SQR_PIN, LOW);

    // 2. Conmuta MUX al Canal AY (Pin 15) bloqueando el transistor
    digitalWrite(MUX_CTRL_PIN, LOW); // Pull-up lleva Pin 11 del MUX a 12V

    // 3. Re-configurar registros del AD9833
    uint32_t freqWord = (uint32_t)((currentFreq * 268435456.0) / MCLK);
    uint16_t LSB = (uint16_t)(freqWord & 0x3FFF) | 0x4000;
    uint16_t MSB = (uint16_t)((freqWord >> 14) & 0x3FFF) | 0x4000;

    uint16_t controlReg = 0x2000; // B28 = 1
    if (currentWave == TRIANGLE) {
      controlReg |= 0x0002;
    }

    // Escritura directa limpia
    writeAD9833(0x2100);     // Reset activado + B28=1
    writeAD9833(LSB);        // LSB de frecuencia
    writeAD9833(MSB);        // MSB de frecuencia
    writeAD9833(0x1C00);     // Fase 0
    writeAD9833(controlReg); // Aplica forma de onda y desactiva Reset
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(AD9833_FSYNC, OUTPUT);
  pinMode(MUX_CTRL_PIN, OUTPUT);
  digitalWrite(AD9833_FSYNC, HIGH);
  digitalWrite(MUX_CTRL_PIN, LOW);

  // Inicializa SPI
  SPI.begin(AD9833_SCK, -1, AD9833_MOSI, -1);
  SPI.setDataMode(SPI_MODE2);

  delay(100);
  updateSignal();

  Serial.println("\n--- GENERADOR DE SEÑALES LISTO ---");
  Serial.println("Comandos: 's' (Seno), 't' (Triangulo), 'q' (Cuadrada)");
  Serial.println("Frecuencia: Ingrese un valor de 10 a 10000");
}

void loop() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    if (input.equalsIgnoreCase("s")) {
      currentWave = SINE;
      Serial.println(">> Modo: SENOIDAL");
    } 
    else if (input.equalsIgnoreCase("t")) {
      currentWave = TRIANGLE;
      Serial.println(">> Modo: TRIANGULAR");
    } 
    else if (input.equalsIgnoreCase("q")) {
      currentWave = SQUARE;
      Serial.println(">> Modo: CUADRADA");
    } 
    else if (input.length() > 0) {
      float newFreq = input.toFloat();
      if (newFreq >= 10.0 && newFreq <= 10000.0) {
        currentFreq = newFreq;
        Serial.print(">> Frecuencia: ");
        Serial.print(currentFreq);
        Serial.println(" Hz");
      } else {
        Serial.println("!! Rango invalido (10 - 10000 Hz)");
      }
    }

    updateSignal();
  }
}