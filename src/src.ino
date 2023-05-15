#include <Arduino.h>
#include <math.h>
#include <EEPROM.h>

// Used for the tft display
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

// Used for the accelerometer
#include <Adafruit_LIS3DH.h>
#include <Wire.h>

#include <RotaryEncoder.h>

#define LOG 2
#include "log.h"
#include "components.h"

/* ***************
TFT / SPI Definitions
******************/
#define TFT_CS GPIO_NUM_10
#define TFT_RST -1 // Share reset with board
#define TFT_DC GPIO_NUM_9

#define TFT_WIDTH 135
#define TFT_HEIGHT 240

#define MS_REFRESH 1000/60

#define KNOB_ROTARY_A GPIO_NUM_13
#define KNOB_ROTARY_B GPIO_NUM_12
#define KNOB_BUTTON GPIO_NUM_6

#define RIGHT_BUTTON GPIO_NUM_5

#define RAND_PIN A1

// Component drivers
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
Adafruit_LIS3DH accelerometer = Adafruit_LIS3DH();
RotaryEncoder knobRotary = RotaryEncoder(KNOB_ROTARY_A, KNOB_ROTARY_B);
Button knobButton = Button(KNOB_BUTTON);
Button rightButton = Button(RIGHT_BUTTON);

struct Player {
  int size;
  Point position;
  int color;
};

struct Tunnel {
  int depthResolution;
  int bottom;
  int depth;
  int color;
  int speed;
  int acceleration;
};

struct Obstacle {
  Point size;
  Point position;
  int color;
};

struct GameplayVars {
  Player player;
  Tunnel tunnel;
  Obstacle obstacle;
  int background;
  Point accelCalib;
  Point center;
};

struct MenuVars {
  int background;
  Point center;
  Tunnel tunnel;
};

struct GameOverVars {
  int background;
  Point center;
  Tunnel tunnel;
  int score;
};

struct Scores {
  unsigned scores[5];
};

struct ShowScoresVars {
  int background;
  Point center;
  Tunnel tunnel;
  Scores scores;
};

typedef Renderer<Adafruit_ST7789, GFXcanvas16> Render;
Render* renderer;

void rotarytick() {
  knobRotary.tick();
}

void transformDepth(Point square[2], int depth, int depth_resolution, Point scale);

App<Render>* game;

Scene<Render> *menu;

void menuEnter(App<Render>* app, Scene<Render> *scene);
void menuLoop(App<Render>* app, Scene<Render> *scene);

void drawTunnel(Render *render, Tunnel *tunnel, Point *center);

Scene<Render> *gameplay;

void gameplayEnter(App<Render>* app, Scene<Render> *scene);
void gameplayUpdate(App<Render>* app, Scene<Render> *scene);

void drawObstacle(Scene<Render>* scene);
void drawPlayer(Scene<Render> *scene);

Scene<Render> *gameover;

void gameoverEnter(App<Render>* app, Scene<Render> *scene);
void gameoverUpdate(App<Render>* app, Scene<Render> *scene);

Scene<Render> *showScores;

void showscoresEnter(App<Render>* app, Scene<Render> *scene);
void showscoresUpdate(App<Render>* app, Scene<Render> *scene);

void setup() {
  Serial.begin(9600);
  delay(1000);
  info("Initializing...\n");

  randomSeed(A0);

  tft.init(TFT_WIDTH, TFT_HEIGHT);

  renderer = new Renderer<Adafruit_ST7789, GFXcanvas16>(&tft);
  renderer->setCursor((tft.width()/2) - (strlen("INIT...")/2), (tft.height()/2)-8);
  renderer->print("INIT...");
  renderer->setTextWrap(false);
  renderer->setRotation(3);
  renderer->update();

  accelerometer.begin(0x18);
  accelerometer.setRange(LIS3DH_RANGE_8_G);

  attachInterrupt(KNOB_ROTARY_A, rotarytick, CHANGE);
  attachInterrupt(KNOB_ROTARY_B, rotarytick, CHANGE);

  renderer->setCursor((tft.width()/2) - (strlen("Setup game...")/2), (tft.height()/2)-8);
  renderer->print("Setup game...");
  renderer->update();

  menu = new Scene<Render>(renderer, 4);
  menu->setup = menuEnter;
  menu->update = menuLoop;

  gameplay = new Scene<Render>(renderer, 4);
  gameplay->setup = gameplayEnter;
  gameplay->update = gameplayUpdate;

  gameover = new Scene<Render>(renderer, 4);
  gameover->setup = gameoverEnter;
  gameover->update = gameoverUpdate;

  showScores = new Scene<Render>(renderer, 4);
  showScores->setup = showscoresEnter;
  showScores->update = showscoresUpdate;

  game = new App<Render>(4);
  game->registerScene(menu, 0);
  game->registerScene(gameplay, 1);
  game->registerScene(gameover, 2);
  game->registerScene(showScores, 3);
  game->start();

  info("Initialized!\n");
}

void loop() {
  static long last_redraw=0;

  game->update();

  if (millis()-last_redraw >= MS_REFRESH) {
    long time = millis();
    renderer->update();
    last_redraw = millis();
    Serial.printf("Updated in %li ms\n", last_redraw-time);
  }
}

void menuEnter(App<Render>* app, Scene<Render>* scene) {
  scene->variables = new MenuVars;
  MenuVars* vars = (MenuVars*)scene->variables;

  vars->center = {scene->renderer->width()/2, scene->renderer->height()/2};
  vars->background = ST77XX_BLACK;
  vars->tunnel.color = ST77XX_WHITE;
  vars->tunnel.bottom = 50;
  vars->tunnel.depthResolution = 1000;
  vars->tunnel.speed = 3;
  vars->tunnel.acceleration = 0;
  vars->tunnel.depth = 0;

  scene->registerLayer([](Scene<Render>* scene) {
    scene->renderer->fillScreen(static_cast<MenuVars*>(scene->variables)->background);
  });
  scene->registerLayer([](Scene<Render>* scene) {
    drawTunnel(scene->renderer, &static_cast<MenuVars*>(scene->variables)->tunnel, &static_cast<MenuVars*>(scene->variables)->center);
  });
  scene->registerLayer([](Scene<Render>* scene) {
    int16_t tx, ty, instx, insty;
    uint16_t tw, th, instw, insth;
    tx = ty = instx = insty = 0;
    tw = th = instw = insth = 0u;
    String instruction = "Press right button to start";
    String title = "Adafall";

    scene->renderer->setTextSize(3);
    scene->renderer->getTextBounds(title, scene->renderer->width()/2, scene->renderer->height()*1/3, &tx, &ty, &tw, &th);
    scene->renderer->fillRect(tx-(tw/2) , ty-(th/2) , tw, th, ST77XX_WHITE);
    scene->renderer->setCursor(tx-(tw/2), ty - (th/2));
    scene->renderer->setTextColor(ST77XX_BLACK);
    scene->renderer->print(title);

    scene->renderer->setTextSize(1);
    scene->renderer->getTextBounds(instruction, scene->renderer->width()/2, scene->renderer->height()*2/3, &instx, &insty, &instw, &insth);
    scene->renderer->fillRect(instx-(instw/2) , insty-(insth/2) , tw, th, ST77XX_BLACK);
    scene->renderer->setCursor(instx-(instw/2), insty - (insth/2));
    scene->renderer->setTextColor(ST77XX_WHITE);
    scene->renderer->print(instruction);
  });
}

void menuLoop(App<Render>* app, Scene<Render>* scene) {
  MenuVars* vars = (MenuVars*)scene->variables;

  vars->tunnel.depth += vars->tunnel.speed;
  if (vars->tunnel.depth > vars->tunnel.depthResolution) {
    vars->tunnel.depth = 0;
  }

  if (rightButton.justPressed()) {
    app->transition(app->scene()+1);
  }
}

void gameplayEnter(App<Render>* app, Scene<Render>* scene) {
  trace("Entering gameplay scene\n");
  scene->variables = new GameplayVars;
  GameplayVars* vars = (GameplayVars*)scene->variables;

  accelerometer.read();

  vars->background = ST77XX_WHITE;
  vars->accelCalib = {accelerometer.x, accelerometer.y};
  vars->center = {scene->renderer->width()/2, scene->renderer->height()/2};

  vars->tunnel.bottom = 1000;
  vars->tunnel.depthResolution = 10000;
  vars->tunnel.depth = vars->tunnel.bottom;
  vars->tunnel.color = ST77XX_BLACK;
  vars->tunnel.speed = 15;
  vars->tunnel.acceleration = 1;

  vars->obstacle.color = 0x28A5;
  vars->obstacle.size = {0, 0};
  vars->obstacle.position = {-1, -1};

  vars->player.color = ST77XX_CYAN;
  vars->player.size = 20;
  vars->player.position = {
    scene->renderer->width()/2 - vars->player.size/2,
    scene->renderer->height()/2 - vars->player.size/2
  };

  scene->registerLayer([](Scene<Render>* scene) {
    scene->renderer->fillScreen(static_cast<GameplayVars*>(scene->variables)->background);
  });
  scene->registerLayer([](Scene<Render>* scene) {
    drawTunnel(scene->renderer, &static_cast<GameplayVars*>(scene->variables)->tunnel, &static_cast<GameplayVars*>(scene->variables)->center);
  });
  scene->registerLayer(drawObstacle);
  scene->registerLayer(drawPlayer);
}

void gameplayUpdate(App<Render>* app, Scene<Render>* scene) {
  GameplayVars *vars = (GameplayVars*)scene->variables;

  accelerometer.read();

  if (vars->tunnel.depth >= vars->tunnel.depthResolution) {

    // Check if player is touches obstacle
    // Don't need to transformDepth the obstacle since depth==depthResolution
    if (
      min(vars->player.position.x + vars->player.size, vars->obstacle.position.x + vars->obstacle.size.x) 
      - max(vars->player.position.x, vars->obstacle.position.x) > 0
      && min(vars->player.position.y + vars->player.size, vars->obstacle.position.y + vars->obstacle.size.y)
      - max(vars->player.position.y, vars->obstacle.position.y) > 0
    ) {

      app->transition(app->scene()+1);
      GameOverVars* overVars = static_cast<GameOverVars*>(app->getScene(app->scene())->variables);
      overVars->score = vars->tunnel.speed; // (vars->tunnel.depth + vars->tunnel.depthResolution * vars->tunnel.acceleration)/ vars->tunnel.depthResolution;
      // Transfer some of the tunel properties to get a smooth transition
      overVars->tunnel.depthResolution = vars->tunnel.depthResolution;
      overVars->tunnel.speed = vars->tunnel.speed;
      overVars->tunnel.bottom = vars->tunnel.bottom;
      overVars->center = vars->center;
      delete vars;
      return;
    }

    // Reseed every time since random gives the same resuults after a couple iterations whitout it
    unsigned seed = analogRead(RAND_PIN);
    srand(seed);

    info("seed: %i && first rand %i\n", seed, rand());

    vars->obstacle.position.x = random(0, scene->renderer->width() - 15);
    vars->obstacle.position.y = random(0, scene->renderer->height() - 15);

    vars->obstacle.size.x = random(25, scene->renderer->width() - vars->obstacle.position.x);
    vars->obstacle.size.y = random(25, scene->renderer->height() - vars->obstacle.position.y);

    info("Obst [(%03i %03i)(%03i %03i)]\n", vars->obstacle.position.x, vars->obstacle.position.y, vars->obstacle.size.x, vars->obstacle.size.y);

    vars->tunnel.depth = vars->tunnel.bottom;
    vars->tunnel.speed += vars->tunnel.acceleration;
    info("Fall speed: %i\n", vars->tunnel.speed);
  }

  vars->tunnel.depth += vars->tunnel.speed;
  
  vars->player.position.x += accelerometer.y / 100;
  vars->player.position.y += accelerometer.x / 100;

  if (vars->player.position.x > scene->renderer->width() - vars->player.size) vars->player.position.x = scene->renderer->width() - vars->player.size;
  if (vars->player.position.x < 0) vars->player.position.x = 0;
  if (vars->player.position.y > scene->renderer->height() - vars->player.size) vars->player.position.y = scene->renderer->height() - vars->player.size;
  if (vars->player.position.y < 0) vars->player.position.y = 0;

}

void gameoverEnter(App<Render> *app, Scene<Render> *scene) {
  trace("Entering gameover scene\n");
  scene->variables = new GameOverVars;
  GameOverVars* vars = static_cast<GameOverVars*>(scene->variables);

  // Update scoreboard
  Scores scores = {0};
  EEPROM.get(0, scores);
  for (int s=5; s>=0; s--) {
    if (scores.scores[s] < vars->score) {
      for (int i=5; i>s; i--) {
        scores.scores[i] = scores.scores[i+1];
      }
      scores.scores[s] = vars->score;
      break;
    }
  }
  EEPROM.put(0, scores);

  vars->tunnel = {0};
  vars->background = 0x2800; // Darker red
  vars->tunnel.color = 0xF800;

  // These are set in case the gameover scene is not entered from the gameplay scene
  vars->tunnel.depthResolution = 10000;
  vars->tunnel.bottom = 1000;
  vars->tunnel.depth = vars->tunnel.bottom;
  vars->tunnel.speed = 20;

  knobRotary.setPosition(0);

  scene->registerLayer([](Scene<Render>* scene) {
    scene->renderer->fillScreen(static_cast<GameOverVars*>(scene->variables)->background);
  });
  scene->registerLayer([](Scene<Render>* scene) {
    drawTunnel(scene->renderer, &static_cast<GameOverVars*>(scene->variables)->tunnel, &static_cast<GameOverVars*>(scene->variables)->center);
  });
  scene->registerLayer([](Scene<Render>* scene) {
    int16_t x=0, y=0;
    uint16_t w=0, h=0;
    String score = "Score: " + String(static_cast<GameOverVars*>(scene->variables)->score);

    scene->renderer->setTextSize(3);
    scene->renderer->getTextBounds(score, scene->renderer->width()/2, scene->renderer->height()*1/3, &x, &y, &w, &h);
    scene->renderer->fillRect(x-(w/2) , y-(h/2) , w, h, 0x0000);
    scene->renderer->setCursor(x-(w/2), y - (h/2));
    scene->renderer->setTextColor(ST77XX_WHITE);
    scene->renderer->print(score);
  });
  scene->registerLayer([](Scene<Render>* scene) {
    int16_t x=0, y=0;
    uint16_t w=0, h=0;
    
    scene->renderer->setTextSize(2);

    String text = "Scores";

    scene->renderer->getTextBounds(text, scene->renderer->width() * 1/6, scene->renderer->height()*4/5, &x, &y, &w, &h);

    scene->renderer->fillRect(x-(w/2) , y-(h/2) , w, h, knobRotary.getPosition() < 0? 0x1084 : 0x0000);
    scene->renderer->setCursor(x-(w/2), y - (h/2));
    scene->renderer->setTextColor(knobRotary.getPosition() < 0? 0xFFFF : 0x1084);
    scene->renderer->print(text);

    text = "Retry";
    scene->renderer->getTextBounds(text, scene->renderer->width() * 3/6, scene->renderer->height()*4/5, &x, &y, &w, &h);

    scene->renderer->fillRect(x-(w/2) , y-(h/2) , w, h, knobRotary.getPosition() == 0? 0x1084 : 0x0000);
    scene->renderer->setCursor(x-(w/2), y - (h/2));
    scene->renderer->setTextColor(knobRotary.getPosition() == 0? 0xFFFF : 0x1084);
    scene->renderer->print(text);

    text = "Menu";
    scene->renderer->getTextBounds(text, scene->renderer->width() * 5/6, scene->renderer->height()*4/5, &x, &y, &w, &h);

    scene->renderer->fillRect(x-(w/2) , y-(h/2) , w, h, knobRotary.getPosition() > 0? 0x1084 : 0x0000);
    scene->renderer->setCursor(x-(w/2), y - (h/2));
    scene->renderer->setTextColor(knobRotary.getPosition() > 0? 0xFFFF : 0x1084);
    scene->renderer->print(text);
  });
}

void gameoverUpdate(App<Render> *app, Scene<Render> *scene) {
  GameOverVars* vars = static_cast<GameOverVars*>(scene->variables);

  if (rightButton.justPressed()) {
    if (knobRotary.getPosition() < 0) {
      app->transition(app->scene()+1);
      return;
    }
    if (knobRotary.getPosition() == 0) {
      app->transition(app->scene()-1);
      return;
    }
    if (knobRotary.getPosition() == 1) {
      app->transition(0);
      return;
    }
  }

  if (vars->tunnel.depth >= vars->tunnel.depthResolution) {
    vars->tunnel.depth = vars->tunnel.bottom;
    return;
  }

  vars->tunnel.depth += vars->tunnel.speed;
}

void showscoresEnter(App<Render> *app, Scene<Render> *scene) {
  trace("Entering showscores scene\n");
  scene->variables = new ShowScoresVars;
  ShowScoresVars* vars = static_cast<ShowScoresVars*>(scene->variables);
  
  EEPROM.get(0, vars->scores);
  vars->center = {scene->renderer->width()/2, scene->renderer->height()/2};

  scene->registerLayer([](Scene<Render>* scene) {
    ShowScoresVars* vars = static_cast<ShowScoresVars*>(scene->variables);
    scene->renderer->fillScreen(0x1084);
    drawTunnel(scene->renderer, &vars->tunnel, &vars->center);
  });
  scene->registerLayer([](Scene<Render>* scene) {
    int16_t x=0, y=0;
    uint16_t w=0, h=0;
    String score = "Scores";

    scene->renderer->setTextSize(2);
    scene->renderer->getTextBounds(score, scene->renderer->width()/2, scene->renderer->height()*2/7, &x, &y, &w, &h);

    scene->renderer->fillRect(x-(w/2) , y-(h/2) , w, h, 0x0000);
    scene->renderer->setCursor(x-(w/2), y - (h/2));
    scene->renderer->setTextColor(ST77XX_WHITE);
    scene->renderer->println(score);

    for (int s=3; s<7; s++) {
      String score = String(s-2) + ". " + String(static_cast<ShowScoresVars*>(scene->variables)->scores.scores[s-3]);
      scene->renderer->println(score);
    }
  });
}

void showscoresUpdate(App<Render> *app, Scene<Render> *scene) {
  if (rightButton.justPressed()) {
    app->transition(0);
  }
}

void showScoresExit(App<Render> *app, Scene<Render> *scene) {
  delete static_cast<ShowScoresVars*>(scene->variables);
}

void drawTunnel(Render* render, Tunnel* tunnel, Point* center) {
  static const Point minSquare[] = {
    {
      center->x - (tunnel->bottom * render->width()) / tunnel->depthResolution,
      center->y - (tunnel->bottom * render->height()) / tunnel->depthResolution
    },
    {
      center->x + (tunnel->bottom * render->width()) / tunnel->depthResolution,
      center->y + (tunnel->bottom * render->height()) / tunnel->depthResolution
    }
  };

  Point square[2] = {0};
  square[0].x = center->x - (tunnel->depth * render->width()) / tunnel->depthResolution;
  square[0].y = center->y - (tunnel->depth * render->height()) / tunnel->depthResolution;

  square[1].x = center->x + (tunnel->depth * render->width()) / tunnel->depthResolution;
  square[1].y = center->y + (tunnel->depth * render->height()) / tunnel->depthResolution;

  trace("Tunnel rect (%03i, %03i) (%03i, %03i) depth: %03i center: (%03i, %03i)\n", square[0].x, square[0].y, square[1].x, square[1].y, tunnel->depth, center->x, center->y);
  render->fillRect(minSquare[0].x, minSquare[0].y, minSquare[1].x - minSquare[0].x, minSquare[1].y - minSquare[0].y, tunnel->color);
  // Draw centered cube
  render->drawRect(square[0].x, square[0].y, square[1].x - square[0].x, square[1].y -square[0].y, tunnel->color);

  // // Draw diagonals to cube
  render->drawLine(0, 0, minSquare[0].x, minSquare[0].y, tunnel->color);
  render->drawLine(render->width(), 0, minSquare[1].x, minSquare[0].y, tunnel->color);

  render->drawLine(render->width(), render->height(), minSquare[1].x, minSquare[1].y, tunnel->color);
  render->drawLine(0, render->height(),  minSquare[0].x, minSquare[1].y, tunnel->color);
}

void drawObstacle(Scene<Render>* scene) {
  trace("Drawing obstacle\n");
  GameplayVars *vars = (GameplayVars*)scene->variables;

  Point square[2] = {
    vars->obstacle.position,
    vars->obstacle.size
  };

  transformDepth(square, vars->tunnel.depth, vars->tunnel.depthResolution, {scene->renderer->width(), scene->renderer->height()});
  scene->renderer->fillRect(square[0].x, square[0].y, square[1].x, square[1].y, vars->obstacle.color);
}

void drawPlayer(Scene<Render> *scene) {
  trace("Drawing Player\n");
  GameplayVars *vars = (GameplayVars*)scene->variables;
  scene->renderer->fillRect(vars->player.position.x, vars->player.position.y, vars->player.size, vars->player.size, vars->player.color);
}

void transformDepth(Point square[2], int depth, int depth_resolution, Point scale) {
  // Move anchor to center
  square[0].x = square[0].x - scale.x/2;
  square[0].y = square[0].y - scale.y/2;

  // Scale to depth
  square[0].x = (square[0].x * depth) / depth_resolution;
  square[0].y = (square[0].y * depth) / depth_resolution;

  square[1].x = (square[1].x * depth) / depth_resolution;
  square[1].y = (square[1].y * depth) / depth_resolution;

  // Move anchor back to original position
  square[0].x = square[0].x + scale.x/2;
  square[0].y = square[0].y + scale.y/2;
}

