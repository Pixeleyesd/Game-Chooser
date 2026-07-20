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

// 128px wide / 6px per char = 21 cols. 64px tall / 8px per char = 8 rows
const int LCD_COLS = 21;
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

// each console gets its own game list now instead of sharing one per brand.
// 8 games per list to keep things simple. consoles that share the exact same library in real life (PSTV/Vita, Xbox One S/X, Series S/X) just point at the same array below instead of duplicating it

const int GAMES_PER_MODEL = 8;

const char* nesGames[GAMES_PER_MODEL] = {
  "Super Mario Bros.", "The Legend of Zelda", "Metroid", "Mega Man 2",
  "Duck Hunt", "Castlevania", "Kirby's Adventure", "Contra"
};

const char* snesGames[GAMES_PER_MODEL] = {
  "Super Mario World", "The Legend of Zelda: A Link to the Past", "Super Metroid", "Donkey Kong Country",
  "Chrono Trigger", "Super Mario Kart", "Star Fox", "EarthBound"
};

const char* n64Games[GAMES_PER_MODEL] = {
  "Super Mario 64", "The Legend of Zelda: Ocarina of Time", "GoldenEye 007", "Mario Kart 64",
  "Banjo-Kazooie", "Super Smash Bros.", "Star Fox 64", "Paper Mario"
};

const char* gamecubeGames[GAMES_PER_MODEL] = {
  "Super Mario Sunshine", "The Legend of Zelda: The Wind Waker", "Metroid Prime", "Super Smash Bros. Melee",
  "Animal Crossing", "Luigi's Mansion", "Mario Kart: Double Dash!!", "Pikmin"
};

const char* wiiGames[GAMES_PER_MODEL] = {
  "Wii Sports", "Super Mario Galaxy", "The Legend of Zelda: Skyward Sword", "Mario Kart Wii",
  "Super Smash Bros. Brawl", "Xenoblade Chronicles", "Donkey Kong Country Returns", "Metroid Prime 3: Corruption"
};

const char* wiiUGames[GAMES_PER_MODEL] = {
  "Super Mario 3D World", "Mario Kart 8", "Super Smash Bros. for Wii U", "The Legend of Zelda: Breath of the Wild",
  "Bayonetta 2", "Splatoon", "Pikmin 3", "Donkey Kong Country: Tropical Freeze"
};

const char* switchGames[GAMES_PER_MODEL] = {
  "The Legend of Zelda: Tears of the Kingdom", "Super Mario Odyssey", "Super Smash Bros. Ultimate", "Animal Crossing: New Horizons",
  "Splatoon 3", "Super Mario Bros. Wonder", "Metroid Dread", "Pikmin 4"
};

// switch 2 is relatively new, and will need updating n stuff
const char* switch2Games[GAMES_PER_MODEL] = {
  "Mario Kart World", "Donkey Kong Bananza", "The Duskbloods", "Splatoon Raiders",
  "Star Fox", "Metroid Prime 4: Beyond", "Fire Emblem: Fortune's Weave", "Pokemon Pokopia"
};

const char* gameboyGames[GAMES_PER_MODEL] = {
  "Pokemon Red and Blue", "The Legend of Zelda: Link's Awakening", "Super Mario Land", "Tetris",
  "Kirby's Dream Land", "Metroid II: Return of Samus", "Donkey Kong", "Wario Land: Super Mario Land 3"
};

const char* gbcGames[GAMES_PER_MODEL] = {
  "Pokemon Gold and Silver", "The Legend of Zelda: Oracle of Seasons", "Super Mario Bros. Deluxe", "Wario Land 3",
  "Pokemon Crystal", "Donkey Kong Country", "Link's Awakening DX", "Pokemon Trading Card Game"
};

const char* gbaGames[GAMES_PER_MODEL] = {
  "Pokemon Ruby and Sapphire", "The Legend of Zelda: The Minish Cap", "Metroid Fusion", "Fire Emblem",
  "Mario Kart: Super Circuit", "Golden Sun", "Metroid: Zero Mission", "Advance Wars"
};

const char* dsGames[GAMES_PER_MODEL] = {
  "New Super Mario Bros.", "Pokemon Diamond and Pearl", "The Legend of Zelda: Phantom Hourglass", "Animal Crossing: Wild World",
  "Mario Kart DS", "Nintendogs", "Professor Layton and the Curious Village", "Kirby: Canvas Curse"
};

const char* threeDSGames[GAMES_PER_MODEL] = {
  "Pokemon X and Y", "The Legend of Zelda: Ocarina of Time 3D", "Super Mario 3D Land", "Animal Crossing: New Leaf",
  "Fire Emblem Awakening", "Luigi's Mansion: Dark Moon", "Kirby: Triple Deluxe", "Mario Kart 7"
};

const char* virtualBoyGames[GAMES_PER_MODEL] = {
  "Mario's Tennis", "Red Alarm", "Galactic Pinball", "Teleroboxer",
  "Virtual Boy Wario Land", "Virtual Fishing", "3D Tetris", "Panic Bomber"
};

// pstv and vita run the same library, so pstv just points at this array too
const char* vitaGames[GAMES_PER_MODEL] = {
  "Persona 4 Golden", "Uncharted: Golden Abyss", "Gravity Rush", "Tearaway",
  "Killzone: Mercenary", "WipEout 2048", "Rayman Legends", "Danganronpa: Trigger Happy Havoc"
};

const char* pspGames[GAMES_PER_MODEL] = {
  "God of War: Chains of Olympus", "Grand Theft Auto: Liberty City Stories", "Crisis Core: Final Fantasy VII", "Patapon",
  "LocoRoco", "Monster Hunter Freedom Unite", "Daxter", "Lumines"
};

const char* ps1Games[GAMES_PER_MODEL] = {
  "Final Fantasy VII", "Metal Gear Solid", "Crash Bandicoot", "Resident Evil 2",
  "Gran Turismo", "Tekken 3", "Castlevania: Symphony of the Night", "Spyro the Dragon"
};

const char* ps2Games[GAMES_PER_MODEL] = {
  "Shadow of the Colossus", "God of War", "Grand Theft Auto: San Andreas", "Metal Gear Solid 2: Sons of Liberty",
  "Final Fantasy X", "Kingdom Hearts", "Ratchet & Clank", "Devil May Cry"
};

const char* ps3Games[GAMES_PER_MODEL] = {
  "The Last of Us", "Uncharted 2: Among Thieves", "God of War III", "Metal Gear Solid 4: Guns of the Patriots",
  "Demon's Souls", "Journey", "LittleBigPlanet", "Heavy Rain"
};

const char* ps4Games[GAMES_PER_MODEL] = {
  "Bloodborne", "Horizon Zero Dawn", "God of War", "Spider-Man",
  "Uncharted 4: A Thief's End", "Ghost of Tsushima", "Persona 5", "Death Stranding"
};

const char* ps5Games[GAMES_PER_MODEL] = {
  "Spider-Man 2", "God of War Ragnarok", "Ratchet & Clank: Rift Apart", "Returnal",
  "Gran Turismo 7", "Final Fantasy VII Rebirth", "Astro Bot", "Stellar Blade"
};

const char* ogXboxGames[GAMES_PER_MODEL] = {
  "Halo: Combat Evolved", "Fable", "Ninja Gaiden", "Jade Empire",
  "Halo 2", "Star Wars: Knights of the Old Republic", "Conker's Bad Fur Day", "Project Gotham Racing"
};

const char* xbox360Games[GAMES_PER_MODEL] = {
  "Halo 3", "Gears of War", "Mass Effect 2", "Fable II",
  "Forza Motorsport 4", "Red Dead Redemption", "Halo: Reach", "Left 4 Dead"
};

// one s and one x run the same games, no exclusives between the two
const char* xboxOneGames[GAMES_PER_MODEL] = {
  "Halo 5: Guardians", "Forza Horizon 3", "Gears of War 4", "Sea of Thieves",
  "Ori and the Blind Forest", "Cuphead", "Sunset Overdrive", "Quantum Break"
};

// series s and series x also run the same games
const char* xboxSeriesGames[GAMES_PER_MODEL] = {
  "Halo Infinite", "Forza Horizon 5", "Gears 5", "Starfield",
  "Hi-Fi Rush", "Psychonauts 2", "Avowed", "Doom Eternal"
};

const char* pcGames[GAMES_PER_MODEL] = {
  "Minecraft", "Roblox", "Portal 2", "Counter-Strike 2",
  "Baldur's Gate 3", "Half-Life 2", "Stardew Valley", "The Witcher 3"
};

// maps each model index within a category to its game list.
// duplicate entries (eg xbox one s and x both pointing at xboxOneGames) are intentional
const char** psGameLists[PS_COUNT]       = { vitaGames, vitaGames, pspGames, ps1Games, ps2Games, ps3Games, ps4Games, ps5Games };
const char** xboxGameLists[XBOX_COUNT]   = { ogXboxGames, xbox360Games, xboxOneGames, xboxOneGames, xboxSeriesGames, xboxSeriesGames };
const char** nintendoGameLists[NINTENDO_COUNT] = {
  nesGames, snesGames, n64Games, gamecubeGames, wiiGames, wiiUGames, switchGames, switch2Games,
  gameboyGames, gbcGames, gbaGames, gbaGames, dsGames, threeDSGames, virtualBoyGames
};

void renderMenu(const char* items[], int itemCount, int selectedIndex, int scrollOffset, bool* ownedFlags, const char* title = nullptr);
int  selectFromMenu(const char* items[], int itemCount, bool* ownedFlags, const char* title = nullptr);
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
const char* pickRandomGameForModel(Category cat, int modelIndex);
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

// title is optional, if given it draws on row 0 and the item list shifts down under it
void renderMenu(const char* items[], int itemCount, int selectedIndex, int scrollOffset, bool* ownedFlags, const char* title) {
  display.clearDisplay();

  int firstRow = 0;
  if (title != nullptr) {
    display.setCursor(0, 0);
    display.print(title);
    firstRow = 1;
  }

  for (int row = firstRow; row < LCD_ROWS; row++) {
    int i = scrollOffset + (row - firstRow);
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

int selectFromMenu(const char* items[], int itemCount, bool* ownedFlags, const char* title) {
  flushEncoderEvents();

  int index = 0;
  int scrollOffset = 0;
  int visibleRows = LCD_ROWS - (title != nullptr ? 1 : 0);

  rotaryEncoder.setBoundaries(0, itemCount - 1, true); // wrap around
  rotaryEncoder.setEncoderValue(0);
  renderMenu(items, itemCount, index, scrollOffset, ownedFlags, title);

  while (true) {
    sleepScreenIfIdle();

    bool rotated = rotaryEncoder.encoderChanged();
    bool clicked = rotaryEncoder.isEncoderButtonClicked();

    if (!rotated && !clicked) continue;

    if (wakeScreenIfAsleep()) {
      renderMenu(items, itemCount, index, scrollOffset, ownedFlags, title); // just redraw, ignore this input
      continue;
    }

    registerActivity();

    if (rotated) {
      index = (int)rotaryEncoder.readEncoder();

      if (index < scrollOffset) {
        scrollOffset = index;
      } else if (index >= scrollOffset + visibleRows) {
        scrollOffset = index - visibleRows + 1;
      }
      renderMenu(items, itemCount, index, scrollOffset, ownedFlags, title);
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

// word wraps across all available rows, since full game titles can run long
void printWrapped(String full) {
  int row = 0;
  int start = 0;
  int len = full.length();

  while (start < len && row < LCD_ROWS) {
    int remaining = len - start;
    int chunkLen = min(remaining, LCD_COLS);

    int breakAt = -1;
    if (remaining > LCD_COLS) {
      // find the last space inside this chunk so we don't cut a word in half
      for (int i = chunkLen; i > 0; i--) {
        if (full.charAt(start + i - 1) == ' ') {
          breakAt = i;
          break;
        }
      }
    }
    if (breakAt == -1) breakAt = chunkLen;

    display.setCursor(0, row * CHAR_H);
    display.print(full.substring(start, start + breakAt));

    start += breakAt;
    while (start < len && full.charAt(start) == ' ') start++; // skip the space we broke on
    row++;
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

// picks a game for the exact console model, not just the brand
const char* pickRandomGameForModel(Category cat, int modelIndex) {
  const char** list;

  switch (cat) {
    case CAT_PLAYSTATION: list = psGameLists[modelIndex]; break;
    case CAT_XBOX:         list = xboxGameLists[modelIndex]; break;
    case CAT_NINTENDO:     list = nintendoGameLists[modelIndex]; break;
    case CAT_COMPUTER:     list = pcGames; break;
    default:               return "";
  }

  int idx = random(0, GAMES_PER_MODEL);
  return list[idx];
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
    const char* game = pickRandomGameForModel(cat, modelIndex);
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
  int choice = selectFromMenu(items, 2, nullptr, "Select another?");
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

  randomSeed(esp_random());

  registerActivity();
  showWelcomeScreen();
  digitalWrite(POWER_LED_PIN, HIGH);
}

void loop() {
  mainMenu();
}