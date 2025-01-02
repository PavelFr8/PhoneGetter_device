#include <EEPROM.h>
#include <SoftwareSerial.h>
#include <EasyNextionLibrary.h>
#include <trigger.h>
 
EasyNex myNex(Serial2); // Инициализируем Nextion на Serial2
 
bool WiFisetup = false;
 
const int setupCompleteAddress = 0;  // Адрес в EEPROM
const int MAX_CELLS = 20;  // Максимальное количество клеток
 
// Структура для хранения данных о ячейках
struct CellData {
    String deviceName;
    String cellNumbers[MAX_CELLS]; // Массив для хранения номеров ячеек
    int userIds[MAX_CELLS];         // Массив для хранения ID пользователей
    bool states[MAX_CELLS];         // Массив для хранения состояний клеток
    int cellCount;                  // Количество клеток
};
 
CellData cells_data;
 
void setup() {
    Serial.begin(115200);   // Serial для логирования
    Serial1.begin(115200);
    Serial3.begin(115200);  // Serial3 для ESP8266
    myNex.begin(115200);
    delay(10000);
 
}
 
void loop() {
  myNex.NextionListen();
 
  // Проверяем, была ли настройка устройства завершена
  if (EEPROM.read(setupCompleteAddress) != 1) {
    
      if (myNex.readNumber("wifi_setup.setupState.val") != 1) {
        myNex.writeStr("page wifi_setup");
      }
      if (!WiFisetup) {
               Serial.println("No response");
               delay(5000);
      } else {
        Serial.println("Начался процесс установки WiFi...");
        setupWiFi();
      }
  } else {
        // Если настройка завершена, переходим к основной логике работы
        if (myNex.readNumber("cells1.cellsState.val") != 1 && myNex.readNumber("code_page.codeState.val") == 1) {
          myNex.writeStr("page cells1");
          Serial.println("Настройка уже была завершена.");
          requestCellData(); // Запрашиваем данные о ячейках
        }
    }
 
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n'); // Чтение строки до новой строки
    command.trim();

    if (command == "RESET") {
      // Сброс настройки и возврат к экрану Wi-Fi
      Serial.println("CLEAR");
      Serial.println("CLEAN");
      myNex.writeNum("sleep", 1);
      delay(1000);
      myNex.writeNum("sleep", 0);
      EEPROM.update(setupCompleteAddress, 0);
      myNex.writeStr("page wifi_setup");
      
    } else if (command == "DATA") {
      requestCellData();
    } else if (command == "OPEN") {
      OpenCell(1);
    } else if (command == "CODE") {
      String str = requestCodeFromESP(); // Запрос секретного кода от ESP
    } else {
      Serial3.println(command); // Передача команды на ESP8266
    }
  }
  delay(1000);
}
 
// Настройка Wi-Fi
void setupWiFi() {
    String ssid = myNex.readStr("ssid.txt");      // Чтение имени Wi-Fi сети с экрана
    String password = myNex.readStr("psw.txt");  // Чтение пароля Wi-Fi сети с экрана
 
    Serial.println("Ввод Wi-Fi данных...");
    Serial3.println("SET_WIFI:" + ssid + ":" + password);  // Передаем данные на ESP 
 
    delay(20000); // Ожидание ответа от ESP8266
    if (Serial3.available() > 0) {
        String response = Serial3.readString();
        response.trim();
        Serial.println(response);
        if (response == "WIFI_OK") {
            Serial.println("Wi-Fi подключение успешно");
            myNex.writeStr("setup_achv.result.txt", "Connection succed");
            delay(100);
            myNex.writeStr("page setup_achv");
            myNex.writeStr("code_page.code0.txt", requestCodeFromESP());
            delay(1000);
            delay(2000);
            myNex.writeStr("page code_page");
            delay(100);
            myNex.writeNum("code_page.codeState.val", 1);
            requestCellData();
            Serial.println("Настройка завершена.");
            EEPROM.update(setupCompleteAddress, 1);  // Сохраняем статус завершения настройки в EEPROM
        } else {
            Serial.println("Ошибка подключения к Wi-Fi");
            delay(50);
            myNex.writeStr("setup_achv.result.txt", "Setup error");
            delay(100);
            myNex.writeStr("page setup_achv");
            WiFisetup = false;
        }
    } else {
        Serial.println("Ответ от ESP не получен.");
    }
}
 
// Запрос секретного кода от ESP
String requestCodeFromESP() {
    Serial3.println("GET_CODE"); // Запрос кода
    delay(5000);
    if (Serial3.available()) {
        String code = Serial3.readStringUntil('\n');
        code.trim();
        Serial.println(code);
        return code;
    }
    return "-1";
}
 
// Функция обновления состояния ячейки
void updateData(int changed_cell) {
    String state;
 
    // Переключаем состояние ячейки
    cells_data.states[changed_cell] = !cells_data.states[changed_cell];
 
    // Печатаем текущее состояние ячейки
    // Serial.println(cells_data.states[changed_cell]);
 
    // Устанавливаем строковое представление состояния
    if (cells_data.states[changed_cell] == false) {
        state = "false";
    } else {
        state = "true";
    }
 
  // Передаем обновленные данные на ESP8266
  Serial.println("UPDATE_DATA:" + String(changed_cell + 1) + ":" + state);
  Serial3.println("UPDATE_DATA:" + String(changed_cell + 1) + ":" + state);
}
 
// Запрос данных по ячейкам от ESP
void requestCellData() {
    Serial3.println("GIVE_DATA"); // Запрос данных
    delay(5000);
    if (Serial3.available() > 0) {
        String cellData = Serial3.readStringUntil('\n');
        cellData.trim();
        Serial.println(cellData);
        cells_data = parseCellData(cellData); // Парсим данные ячеек
        
        Serial.println("Данные клеток получены:");
        for (int i = 0; i < cells_data.cellCount; i++) {
            Serial.print("Клетка: ");
            Serial.print(cells_data.cellNumbers[i]);
            Serial.print(", ID: ");
            Serial.print(cells_data.userIds[i]);
            Serial.print(", Состояние: ");
            Serial.println(cells_data.states[i] ? "True" : "False");
        }
    }
}
 
// Парсим входящие ячейки от ESP
CellData parseCellData(const String& dataString) {
    CellData data;
    // Извлекаем имя устройства до первого символа ';'
    int firstSemicolon = dataString.indexOf(';');
    data.deviceName = dataString.substring(0, firstSemicolon);
    data.cellCount = 0;
 
    String cellsString = dataString.substring(firstSemicolon + 1);
    int start = 0;

    // Обрабатываем данные о каждой ячейке, разделенной символом ';'
    while (true) {
        int end = cellsString.indexOf(';', start);
        if (end == -1) break; 
 
        String cellData = cellsString.substring(start, end);
        int firstComma = cellData.indexOf(',');
        int secondComma = cellData.indexOf(',', firstComma + 1);

        // Добавляем данные о ячейке, если количество ячеек меньше максимально допустимого
        if (data.cellCount < MAX_CELLS) {
            String cellNumber = cellData.substring(0, firstComma);
            int id = cellData.substring(firstComma + 1, secondComma).toInt();
            bool state = cellData.substring(secondComma + 1).equals("1"); 
 
            data.cellNumbers[data.cellCount] = cellNumber;
            data.userIds[data.cellCount] = id;
            data.states[data.cellCount] = state; 
            data.cellCount++; 
        }
 
        start = end + 1; 
    }
 
    return data;
}

// Функция для открытия ячейки
void OpenCell(int cell_num) {
  int pin = 22 + cell_num; 
  pinMode(pin, OUTPUT); 
  digitalWrite(pin, HIGH);
  delay(1000);
  digitalWrite(pin, LOW); 
}

// Функция-обработчик: подтверждение ввода данных Wi-Fi с экрана
void trigger0() {
  WiFisetup = true;
  Serial.println("Данные WiFi были введены");
}

// Функция-обработчик: открытие ячейки
void trigger(int cellIndex) {
    // Формируем имя компонента на основе индекса ячейки
    String errorComponent = "cells" + String(cellIndex) + ".error.val";

    // Проверяем, занята ли ячейка
    if (myNex.readNumber(errorComponent) != 1) {
        OpenCell(cellIndex);   // Открываем ячейку
        updateData(cellIndex - 1); // Обновляем данные о состоянии ячейки (индекс сдвигается на -1)
    }
}

// Аналогичных обработчики для каждой ячейки
void trigger1() { trigger(1); }
void trigger2() { trigger(2); }
void trigger3() { trigger(3); }
void trigger4() { trigger(4); }
void trigger5() { trigger(5); }
void trigger6() { trigger(6); }
void trigger7() { trigger(7); }
void trigger8() { trigger(8); }
void trigger9() { trigger(9); }
void trigger10() { trigger(10); }
void trigger11() { trigger(11); }
void trigger12() { trigger(12); }
void trigger13() { trigger(13); }
void trigger14() { trigger(14); }
void trigger15() { trigger(15); }
void trigger16() { trigger(16); }
void trigger17() { trigger(17); }
void trigger18() { trigger(18); }
void trigger19() { trigger(19); }
void trigger20() { trigger(20); }