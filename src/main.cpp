#include <Arduino.h>
#include <AiEsp32RotaryEncoder.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define ROTARY_ENCODER_A_PIN 4
#define ROTARY_ENCODER_B_PIN 16
#define ROTARY_ENCODER_BUTTON_PIN 17
#define ROTARY_ENCODER_VCC_PIN -1
#define ROTARY_ENCODER_STEPS 4
#define POWER_LED_PIN 2

// GM009605 0.96" oled, ssd1306 driver, 128x64
#define OLED_SDA_PIN 21
#define OLED_SCK_PIN 22
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_RESET -1
#define OLED_I2C_ADDR 0x3C

AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(
  ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN,
  ROTARY_ENCODER_BUTTON_PIN, ROTARY_ENCODER_VCC_PIN,
  ROTARY_ENCODER_STEPS
);

Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);

// keeping the same 16x2 character grid the old lcd used so all the menu math
// below stays untouched. size 1 font is 6x8px so this only uses the top left
// corner of the screen, plenty of room left under it if you ever want more rows
const int LCD_COLS = 32;
const int LCD_ROWS = 8;
const int CHAR_W = 6;
const int CHAR_H = 8;

// screen auto off after being idle for this long, to save power
const unsigned long SCREEN_TIMEOUT_MS = 5UL * 60UL * 1000UL; // 5 min
unsigned long lastActivityTime = 0;
bool screenAsleep = false;

void IRAM_ATTR readEncoderISR() { rotaryEncoder.readEncoder_ISR(); }
void IRAM_ATTR readButtonISR()  { rotaryEncoder.readButton_ISR(); }

enum Category { CAT_PLAYSTATION, CAT_XBOX, CAT_NINTENDO, CAT_COMPUTER, CAT_COUNT };

const int PS_COUNT = 8;
const char* psModels[PS_COUNT] = { "PSTV", "PS Vita", "PSP", "PS1", "PS2", "PS3", "PS4", "PS5" };
bool psOwned[PS_COUNT] = { false };

const int XBOX_COUNT = 6;
const char* xboxModels[XBOX_COUNT] = { "OG Xbox", "Xbox 360", "Xbox One S", "Xbox One X", "Xbox Series S", "Xbox Series X" };
bool xboxOwned[XBOX_COUNT] = { false };

const int NINTENDO_COUNT = 15;
const char* nintendoModels[NINTENDO_COUNT] = {
  "NES", "SNES", "N64", "GameCube", "Wii", "Wii U", "Switch", "Switch 2",
  "Game Boy", "Game Boy Color", "GBA", "GBA SP", "DS", "3DS", "Virtual Boy"
};
bool nintendoOwned[NINTENDO_COUNT] = { false };

bool computerOwned = false;

const int GAMES_PER_CATEGORY = 20;
  
const char* xboxGames[GAMES_PER_CATEGORY] = {
  "Halo Infinite", "Forza Horizon 5", "Gears 5", "Sea of Thieves", "Starfield",
  "Fable", "Psychonauts 2", "Doom Eternal", "Ori & Will of Wisps", "Hi-Fi Rush",
  "Quantum Break", "Sunset Overdrive", "State of Decay 2", "Scorn", "Pentiment",
  "Ryse", "Killer Instinct", "Redfall", "Avowed", "THPS 1+2 Remake"
};

const char* nintendoGames[GAMES_PER_CATEGORY] = {
  "Zelda: TOTK", "Zelda: BOTW", "Super Mario Odyssey", "Smash Ultimate", "Mario Kart 8",
  "Metroid Dread", "Animal Crossing", "Pokemon Scarlet", "Luigi Mansion 3", "Splatoon 3",
  "Fire Emblem Eng", "Xenoblade 3", "DK Tropical Freeze", "Pikmin 4", "Mario Wonder",
  "Paper Mario TTYD", "Metroid Prime Rmstr", "Kirby Forgotten Land", "Bayonetta 3", "Astral Chain"
};

const char* playstationGames[GAMES_PER_CATEGORY] = {
  "God of War Rag.", "Spider-Man 2", "The Last of Us", "Horizon FW", "Ghost of Tsushima",
  "Uncharted 4", "Bloodborne", "Ratchet & Clank", "Gran Turismo 7", "Returnal",
  "Demon's Souls", "Death Stranding", "Days Gone", "Infamous SS", "Sackboy",
  "Detroit Bcm Human", "Until Dawn", "Shadow of Colossus", "Stellar Blade", "Final Fantasy VII"
};

const char* pcGames[GAMES_PER_CATEGORY] = {
  "Minecraft", "Roblox", "Portal 2", "Counter-Strike 2", "Valorant",
  "Cyberpunk 2077", "Baldur's Gate 3", "Half-Life 2", "Team Fortress 2", "League of Legends",
  "Dota 2", "Left 4 Dead 2", "Terraria", "Stardew Valley", "Phasmophobia",
  "Rust", "Garry's Mod", "Civilization VI", "The Witcher 3", "Factorio"
};
  
void renderMenu(const char* items[], int itemCount, int selectedIndex, int scrollOffset, bool* ownedFlags);
int  selectFromMenu(const char* items[], int itemCount, bool* ownedFlags);
void flushEncoderEvents();
void waitForAnyInput();
bool waitForClickOrRotate();
void showMessage(const char* line0, const char* line1, int delayMs);
void showResultAndWait(const char* line0, String line1);
void printWrapped(String full);

void registerActivity();
void sleepScreenIfIdle();
bool wakeScreenIfAsleep();

int  categoryModelCount(Category cat);
const char** categoryModelNames(Category cat);
bool* categoryOwnedArray(Category cat);
const char* categoryName(Category cat);
const char* modelNameFor(Category cat, int index);
int  countOwnedInCategory(Category cat);
int  countOwnedDevices();
void pickRandomOwnedDevice(Category &catOut, int &modelIndexOut);
const char* pickRandomGameForCategory(Category cat);
void removeAllDevices();

void showWelcomeScreen();
void mainMenu();
void handleGameOption();
void handleDifficultyOption();
void handleDurationOption();
void handleConfigOption();
bool configSubmenuPickOne(Category cat);
bool askAddMore();

// power saving stuff

void registerActivity() {
  lastActivityTime = millis();
}

void sleepScreenIfIdle() {
  if (!screenAsleep && millis() - lastActivityTime > SCREEN_TIMEOUT_MS) {
    display.ssd1306_command(SSD1306_DISPLAYOFF);
    screenAsleep = true;
  }
}

// wakes the screen back up if it was off, returns true if it did.
// caller should treat that input as just a wake up, not a real action
bool wakeScreenIfAsleep() {
  if (screenAsleep) {
    display.ssd1306_command(SSD1306_DISPLAYON);
    screenAsleep = false;
    registerActivity();
    return true;
  }
  return false;
}

void renderMenu(const char* items[], int itemCount, int selectedIndex, int scrollOffset, bool* ownedFlags) {
  display.clearDisplay();
  for (int row = 0; row < LCD_ROWS; row++) {
    int i = scrollOffset + row;
    if (i >= itemCount) break;

    display.setCursor(0, row * CHAR_H);

    bool isOwned = ownedFlags != nullptr && ownedFlags[i];
    if (i == selectedIndex) {
      display.print(isOwned ? ">*" : "> ");
    } else {
      display.print(isOwned ? " *" : "  ");
    }

    String label = String(items[i]);
    if (label.length() > (unsigned int)(LCD_COLS - 2)) {
      label = label.substring(0, LCD_COLS - 2);
    }
    display.print(label);
  }
  display.display();
}

int selectFromMenu(const char* items[], int itemCount, bool* ownedFlags) {
  flushEncoderEvents();

  int index = 0;
  int scrollOffset = 0;

  rotaryEncoder.setBoundaries(0, itemCount - 1, true); // wrap around
  rotaryEncoder.setEncoderValue(0);
  renderMenu(items, itemCount, index, scrollOffset, ownedFlags);

  while (true) {
    sleepScreenIfIdle();

    bool rotated = rotaryEncoder.encoderChanged();
    bool clicked = rotaryEncoder.isEncoderButtonClicked();

    if (!rotated && !clicked) continue;

    if (wakeScreenIfAsleep()) {
      renderMenu(items, itemCount, index, scrollOffset, ownedFlags); // just redraw, ignore this input
      continue;
    }

    registerActivity();

    if (rotated) {
      index = (int)rotaryEncoder.readEncoder();

      if (index < scrollOffset) {
        scrollOffset = index;
      } else if (index >= scrollOffset + LCD_ROWS) {
        scrollOffset = index - LCD_ROWS + 1;
      }
      renderMenu(items, itemCount, index, scrollOffset, ownedFlags);
    }

    if (clicked) {
      return index;
    }
  }
}

void flushEncoderEvents() {
  rotaryEncoder.encoderChanged();
  rotaryEncoder.isEncoderButtonClicked();
}

bool waitForClickOrRotate() {
  flushEncoderEvents();
  while (true) {
    sleepScreenIfIdle();

    bool clicked = rotaryEncoder.isEncoderButtonClicked();
    bool rotated = rotaryEncoder.encoderChanged();

    if (!clicked && !rotated) continue;

    if (wakeScreenIfAsleep()) continue; // just a wake up, not a real input

    registerActivity();

    if (clicked) return true;
    if (rotated) return false;
  }
}

void waitForAnyInput() {
  waitForClickOrRotate();
}

void showMessage(const char* line0, const char* line1, int delayMs) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print(line0);
  if (line1 != nullptr && strlen(line1) > 0) {
    display.setCursor(0, CHAR_H);
    display.print(line1);
  }
  display.display();
  if (delayMs > 0) delay(delayMs);
}

void showResultAndWait(const char* line0, String line1) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print(line0);
  display.setCursor(0, CHAR_H);
  display.print(line1);
  display.display();
  waitForAnyInput();
}

void printWrapped(String full) {
  if (full.length() <= (unsigned int)LCD_COLS) {
    display.setCursor(0, 0);
    display.print(full);
    return;
  }

  int breakAt = -1;
  for (int i = LCD_COLS - 1; i >= 0; i--) {
    if (full.charAt(i) == ' ') {
      breakAt = i;
      break;
    }
  }

  unsigned int total = full.length();
  unsigned int maxLen = (unsigned int)(LCD_COLS * LCD_ROWS);

  if (breakAt == -1) {
    display.setCursor(0, 0);
    display.print(full.substring(0, LCD_COLS));
    display.setCursor(0, CHAR_H);
    display.print(full.substring(LCD_COLS, min(total, maxLen)));
  } else {
    display.setCursor(0, 0);
    display.print(full.substring(0, breakAt));
    display.setCursor(0, CHAR_H);
    display.print(full.substring(breakAt + 1, min(total, maxLen)));
  }
}

int categoryModelCount(Category cat) {
  switch (cat) {
    case CAT_PLAYSTATION: return PS_COUNT;
    case CAT_XBOX:         return XBOX_COUNT;
    case CAT_NINTENDO:     return NINTENDO_COUNT;
    default:               return 0; // Computer has no sub-models
  }
}

const char** categoryModelNames(Category cat) {
  switch (cat) {
    case CAT_PLAYSTATION: return psModels;
    case CAT_XBOX:         return xboxModels;
    case CAT_NINTENDO:     return nintendoModels;
    default:               return nullptr;
  }
}

bool* categoryOwnedArray(Category cat) {
  switch (cat) {
    case CAT_PLAYSTATION: return psOwned;
    case CAT_XBOX:         return xboxOwned;
    case CAT_NINTENDO:     return nintendoOwned;
    default:               return nullptr;
  }
}

const char* categoryName(Category cat) {
  switch (cat) {
    case CAT_PLAYSTATION: return "Playstation";
    case CAT_XBOX:         return "Xbox";
    case CAT_NINTENDO:     return "Nintendo";
    case CAT_COMPUTER:     return "Computer";
    default:               return "";
  }
}

const char* modelNameFor(Category cat, int index) {
  if (cat == CAT_COMPUTER) return "Computer";
  return categoryModelNames(cat)[index];
}

int countOwnedInCategory(Category cat) {
  if (cat == CAT_COMPUTER) return computerOwned ? 1 : 0;

  int n = categoryModelCount(cat);
  bool* arr = categoryOwnedArray(cat);
  int c = 0;
  for (int i = 0; i < n; i++) {
    if (arr[i]) c++;
  }
  return c;
}

int countOwnedDevices() {
  int total = 0;
  for (int c = 0; c < CAT_COUNT; c++) {
    total += countOwnedInCategory((Category)c);
  }
  return total;
}

void pickRandomOwnedDevice(Category &catOut, int &modelIndexOut) {
  Category available[CAT_COUNT];
  int availCount = 0;

  for (int c = 0; c < CAT_COUNT; c++) {
    if (countOwnedInCategory((Category)c) > 0) {
      available[availCount++] = (Category)c;
    }
  }

  Category chosenCat = available[random(0, availCount)];
  catOut = chosenCat;

  if (chosenCat == CAT_COMPUTER) {
    modelIndexOut = -1;
    return;
  }

  int n = categoryModelCount(chosenCat);
  bool* arr = categoryOwnedArray(chosenCat);

  int ownedIndices[NINTENDO_COUNT]; // large enough for the biggest category
  int ownedCount = 0;
  for (int i = 0; i < n; i++) {
    if (arr[i]) ownedIndices[ownedCount++] = i;
  }

  modelIndexOut = ownedIndices[random(0, ownedCount)];
}

const char* pickRandomGameForCategory(Category cat) {
  int idx = random(0, GAMES_PER_CATEGORY);
  switch (cat) {
    case CAT_XBOX:         return xboxGames[idx];
    case CAT_NINTENDO:     return nintendoGames[idx];
    case CAT_PLAYSTATION: return playstationGames[idx];
    case CAT_COMPUTER:     return pcGames[idx];
    default:               return "";
  }
}

void removeAllDevices() {
  for (int i = 0; i < PS_COUNT; i++) psOwned[i] = false;
  for (int i = 0; i < XBOX_COUNT; i++) xboxOwned[i] = false;
  for (int i = 0; i < NINTENDO_COUNT; i++) nintendoOwned[i] = false;
  computerOwned = false;
}


// switched on, welcome screen, turn on power led
void showWelcomeScreen() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Game randomiser");
  display.setCursor(0, CHAR_H);
  display.print("Input to start");
  display.display();
  waitForAnyInput();
}

// "input for start randomly choosing: option 1: game, option 2: difficulty,
//  option 3: playing duration, option 4: config"
void mainMenu() {
  const char* items[] = { "Pick a Game", "Difficulty", "Play Duration", "Config" };
  int choice = selectFromMenu(items, 4, nullptr);

  switch (choice) {
    case 0: handleGameOption();       break;
    case 1: handleDifficultyOption(); break;
    case 2: handleDurationOption();   break;
    case 3: handleConfigOption();     break;
  }
}

void handleGameOption() {
  if (countOwnedDevices() == 0) {
    showMessage("Add devices in", "option 4: Config", 2000);
    return;
  }

  Category cat;
  int modelIndex;
  pickRandomOwnedDevice(cat, modelIndex);
  const char* model = modelNameFor(cat, modelIndex);

  display.clearDisplay();
  display.setCursor(0, 0);
  display.print(categoryName(cat));
  display.setCursor(0, CHAR_H);
  display.print(model);
  display.display();
  delay(1200);

  display.clearDisplay();
  display.setCursor(0, 0);
  display.print(model);
  display.setCursor(0, CHAR_H);
  display.print("Click to choose!");
  display.display();

  if (!waitForClickOrRotate()) {
    return;
  }

  while (true) {
    const char* game = pickRandomGameForCategory(cat);
    display.clearDisplay();
    printWrapped(String("Play ") + String(game));
    display.display();

    if (!waitForClickOrRotate()) {
      return;
    }
  }
}

void handleDifficultyOption() {
  const char* difficulties[] = { "Easy", "Medium", "Hard" };
  int idx = (int)random(0, 3);
  showResultAndWait("Difficulty:", String(difficulties[idx]));
}

void handleDurationOption() {
  int totalMinutes = (int)random(4, 25) * 5; // 4*5=20 ... 24*5=120
  int hours = totalMinutes / 60;
  int mins = totalMinutes % 60;

  String line1;
  if (hours > 0) {
    line1 = String(hours) + "h " + String(mins) + "m";
  } else {
    line1 = String(mins) + " min";
  }

  showResultAndWait("Play for:", line1);
}

void handleConfigOption() {
  while (true) {
    const char* items[] = { "Playstation", "Xbox", "Nintendo", "Computer", "Remove All" };
    int choice = selectFromMenu(items, 5, nullptr);
    bool keepConfiguring = true;

    switch (choice) {
      case 0: keepConfiguring = configSubmenuPickOne(CAT_PLAYSTATION); break;
      case 1: keepConfiguring = configSubmenuPickOne(CAT_XBOX); break;
      case 2: keepConfiguring = configSubmenuPickOne(CAT_NINTENDO); break;
      case 3:
        computerOwned = true;
        showMessage("Added:", "Computer", 1200);
        keepConfiguring = askAddMore();
        break;
      case 4:
        removeAllDevices();
        showMessage("All devices", "removed", 1200);
        keepConfiguring = true;
        break;
    }

    if (!keepConfiguring) return;
  }
}

bool configSubmenuPickOne(Category cat) {
  int count = categoryModelCount(cat);
  const char** items = categoryModelNames(cat);
  bool* owned = categoryOwnedArray(cat);

  int choice = selectFromMenu(items, count, owned);
  owned[choice] = true;

  showMessage("Added:", items[choice], 1200);
  return askAddMore();
}

bool askAddMore() {
  const char* items[] = { "Yes", "No" };
  int choice = selectFromMenu(items, 2, nullptr);
  return choice == 0; // true = Yes
}

void setup() {
  Serial.begin(115200);

  pinMode(POWER_LED_PIN, OUTPUT);
  digitalWrite(POWER_LED_PIN, LOW);

  Wire.begin(OLED_SDA_PIN, OLED_SCK_PIN);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR)) {
    Serial.println("oled not found, check wiring/address");
    while (true) delay(1000);
  }
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.cp437(true);
  display.clearDisplay();
  display.display();

  rotaryEncoder.begin();
  rotaryEncoder.setup(readEncoderISR, readButtonISR);
  rotaryEncoder.setAcceleration(150);

  randomSeed(analogRead(34));

  registerActivity();
  showWelcomeScreen();
  digitalWrite(POWER_LED_PIN, HIGH);
}

void loop() {
  mainMenu();
}