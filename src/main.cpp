#include <Arduino.h>
#include <AiEsp32RotaryEncoder.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define ROTARY_ENCODER_A_PIN 4
#define ROTARY_ENCODER_B_PIN 16
#define ROTARY_ENCODER_BUTTON_PIN 17
#define ROTARY_ENCODER_VCC_PIN -1
#define ROTARY_ENCODER_STEPS 4
#define POWER_LED_PIN 2 

AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(
  ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN,
  ROTARY_ENCODER_BUTTON_PIN, ROTARY_ENCODER_VCC_PIN,
  ROTARY_ENCODER_STEPS
);

LiquidCrystal_I2C lcd(0x27, 16, 2);

const int LCD_COLS = 16;
const int LCD_ROWS = 2;

void IRAM_ATTR readEncoderISR() { rotaryEncoder.readEncoder_ISR(); }
void IRAM_ATTR readButtonISR()  { rotaryEncoder.readButton_ISR(); }

enum Category { CAT_PLAYSTATION, CAT_XBOX, CAT_NINTENDO, CAT_COMPUTER, CAT_COUNT };

const int PS_COUNT = 8;
const char* psModels[PS_COUNT] = { "PSTV", "PS Vita", "PSP", "PS1", "PS2", "PS3", "PS4", "PS5" };
bool psOwned[PS_COUNT] = { false };

const int XBOX_COUNT = 6;x`
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

void renderMenu(const char* items[], int itemCount, int selectedIndex, int scrollOffset, bool* ownedFlags) {
  lcd.clear();
  for (int row = 0; row < LCD_ROWS; row++) {
    int i = scrollOffset + row;
    if (i >= itemCount) break;

    lcd.setCursor(0, row);

    bool isOwned = ownedFlags != nullptr && ownedFlags[i];
    if (i == selectedIndex) {
      lcd.print(isOwned ? ">*" : "> ");
    } else {
      lcd.print(isOwned ? " *" : "  ");
    }

    String label = String(items[i]);
    if (label.length() > (unsigned int)(LCD_COLS - 2)) {
      label = label.substring(0, LCD_COLS - 2);
    }
    lcd.print(label);
  }
}

int selectFromMenu(const char* items[], int itemCount, bool* ownedFlags) {
  flushEncoderEvents();

  int index = 0;
  int scrollOffset = 0;

  rotaryEncoder.setBoundaries(0, itemCount - 1, true); // wrap around
  rotaryEncoder.setEncoderValue(0);
  renderMenu(items, itemCount, index, scrollOffset, ownedFlags);

  while (true) {
    if (rotaryEncoder.encoderChanged()) {
      index = (int)rotaryEncoder.readEncoder();

      if (index < scrollOffset) {
        scrollOffset = index;
      } else if (index >= scrollOffset + LCD_ROWS) {
        scrollOffset = index - LCD_ROWS + 1;
      }
      renderMenu(items, itemCount, index, scrollOffset, ownedFlags);
    }

    if (rotaryEncoder.isEncoderButtonClicked()) {
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
    if (rotaryEncoder.isEncoderButtonClicked()) return true;
    if (rotaryEncoder.encoderChanged()) return false;
  }
}

void waitForAnyInput() {
  waitForClickOrRotate();
}

void showMessage(const char* line0, const char* line1, int delayMs) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line0);
  if (line1 != nullptr && strlen(line1) > 0) {
    lcd.setCursor(0, 1);
    lcd.print(line1);
  }
  if (delayMs > 0) delay(delayMs);
}

void showResultAndWait(const char* line0, String line1) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line0);
  lcd.setCursor(0, 1);
  lcd.print(line1);
  waitForAnyInput();
}

void printWrapped(String full) {
  if (full.length() <= (unsigned int)LCD_COLS) {
    lcd.setCursor(0, 0);
    lcd.print(full);
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
    lcd.setCursor(0, 0);
    lcd.print(full.substring(0, LCD_COLS));
    lcd.setCursor(0, 1);
    lcd.print(full.substring(LCD_COLS, min(total, maxLen)));
  } else {
    lcd.setCursor(0, 0);
    lcd.print(full.substring(0, breakAt));
    lcd.setCursor(0, 1);
    lcd.print(full.substring(breakAt + 1, min(total, maxLen)));
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
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Game randomiser");
  lcd.setCursor(0, 1);
  lcd.print("Input to start");
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

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(categoryName(cat));
  lcd.setCursor(0, 1);
  lcd.print(model);
  delay(1200);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(model);
  lcd.setCursor(0, 1);
  lcd.print("Click to choose!");

  if (!waitForClickOrRotate()) {
    return;
  }

  while (true) {
    const char* game = pickRandomGameForCategory(cat);
    lcd.clear();
    printWrapped(String("Play ") + String(game));

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

  lcd.init();
  lcd.backlight();

  rotaryEncoder.begin();
  rotaryEncoder.setup(readEncoderISR, readButtonISR);
  rotaryEncoder.setAcceleration(150);

  randomSeed(analogRead(34));

  showWelcomeScreen();
  digitalWrite(POWER_LED_PIN, HIGH);
}

void loop() {
  mainMenu();
}