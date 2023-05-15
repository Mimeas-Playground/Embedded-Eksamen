#ifndef _COMPONENTS_H_
#define _COMPONENTS_H_

#include <Adafruit_GFX.h>
#include <cstddef>
#include <cstdint>
#include "Adafruit_ST77xx.h"
#include "log.h"

template<class Display, class Canvas> class Renderer;
template<class R> class Scene;
template<class R> class App;

class Button;

template<class Display, class Canvas> class Renderer: public Adafruit_GFX {
  public:
    Renderer(Display *display) : Adafruit_GFX(display->width(), display->height()) {
      _display = display;
      _canvas = new Canvas(_display->width(), _display->height());
    }

    void update() {
      if (_display->getRotation() != rotation)
        _display->setRotation(rotation);

      _display->drawRGBBitmap(0, 0, _canvas->getBuffer(), _width, _height);
    }

  public:
  /**********************************************************************/
  /*!
    @brief  Draw to the screen/framebuffer/etc.
    Must be overridden in subclass.
    @param  x    X coordinate in pixels
    @param  y    Y coordinate in pixels
    @param color  16-bit pixel color.
  */
  /**********************************************************************/
  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    _canvas->drawPixel(x, y, color);
  }

  // TRANSACTION API / CORE DRAW API
  // These MAY be overridden by the subclass to provide device-specific
  // optimized code.  Otherwise 'generic' versions are used.
  void startWrite(void) override {
    _transaction = true;
  }

  void writePixel(int16_t x, int16_t y, uint16_t color) override {
    _canvas->writePixel(x, y, color);
  }

  void writeFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override {
    _canvas->writeFillRect(x, y, w, h, color);
  }

  void writeFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override {
    _canvas->writeFastVLine(x, y, h, color);
  }

  void writeFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override {
    _canvas->writeFastHLine(x, y, w, color);
  }

  void writeLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) override {
    _canvas->writeLine(x0, y0, x1, y1, color);
  }

  void endWrite(void) override {
    _transaction = false;
    this->update();
  }

  // CONTROL API
  // These MAY be overridden by the subclass to provide device-specific
  // optimized code.  Otherwise 'generic' versions are used.

  // The canvas is not rotated as this causes issues when drawing the buffer to the display
  // The canvas subclasses does not have a resize buffer method so we create a new canvas with
  // the new dimensions.
  void setRotation(uint8_t r) override {
    int old = _display->getRotation();
    _display->setRotation(r);
    rotation = r;
    _width = _display->width();
    _height = _display->height();
    delete _canvas;
    _canvas = new Canvas(_width, _height);
  }

  void invertDisplay(bool i) override {
    _display->invertDisplay(i);
  }

  // BASIC DRAW API
  // These MAY be overridden by the subclass to provide device-specific
  // optimized code.  Otherwise 'generic' versions are used.

  // It's good to implement those, even if using transaction API
  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override {
    _canvas->drawFastVLine(x, y, h, color);
  }
  
  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override {
    _canvas->drawFastHLine(x, y, w, color);
  }

  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override {
    _canvas->fillRect(x, y, w, h, color);
  }

  void fillScreen(uint16_t color) override {
    _canvas->fillScreen(color);
  }

  // Optional and probably not necessary to change
  void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) override {
    _canvas->drawLine(x0, y0, x1, y1, color);
  }

  void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override {
    _canvas->drawRect(x, y, w, h, color);
  }

  // Adafruit_GFX says these only need to exist in the base class, but 
  // they need to be shadowed to transfer the calls to the canvas
  // (otherwise the canvas will not know about text related changes)

  void setCursor(int16_t x, int16_t y) {
    _canvas->setCursor(x, y);
    this->Adafruit_GFX::setCursor(x, y);
  }

  void setTextColor(uint16_t c) {
    _canvas->setTextColor(c);
    this->Adafruit_GFX::setTextColor(c);
  }

  void setTextColor(uint16_t c, uint16_t bg) {
    _canvas->setTextColor(c, bg);
    this->Adafruit_GFX::setTextColor(c, bg);
  }

  void setTextSize(uint8_t s) {
    _canvas->setTextSize(s);
    this->Adafruit_GFX::setTextSize(s);
  }

  void setTextSize(uint8_t sx, uint8_t sy) {
    _canvas->setTextSize(sx, sy);
    this->Adafruit_GFX::setTextSize(sx, sy);
  }

  void setFont(const GFXfont *f = NULL) {
    _canvas->setFont(f);
    this->Adafruit_GFX::setFont(f);
  }

  void setTextWrap(bool w) {
    _canvas->setTextWrap(w);
    this->Adafruit_GFX::setTextWrap(w);
  }

  size_t write(uint8_t c) override {
    return _canvas->write(c);
  }

  private:
    Display* _display;
    Canvas* _canvas;

    bool _transaction = false;
    using Adafruit_GFX::rotation;
    using Adafruit_GFX::_width;
    using Adafruit_GFX::_height;
};


class Button {
public:
  Button(int pin) {
    this->pin = pin;
    this->pressed = false;
    pinMode(pin, INPUT);
  }
  Button() {return;}

  bool justPressed() {
    if (digitalRead(pin)) {
      return pressed? false : pressed = true;
    }
    else 
      return pressed = false;
  }

private:
  int pin;
  bool pressed;
};

template<class R> class Scene {
  typedef void(*draw_callback)(Scene<R>* scene);
  public:
    Scene(R* renderer, int layer_capacity=3) {
      trace("Init scene with %i layers (renderer: %p)\n", layer_capacity, renderer);
      this->renderer = renderer;

      capacity = layer_capacity;
      layers = new draw_callback[capacity];
      memset(layers, 0, capacity * sizeof(void*));
      size=0;
    }

    ~Scene() {
      trace("Deleting scene with %i layers\n", capacity);
      onExit();
      delete[] layers;
    };

    void onEnter(App<R>* app) {
      info("Scene OnEnter (setup: %x)\n", setup);
      if (setup!=NULL){
        setup(app, this);
      }
    }

    void onUpdate(App<R>* app) {
      trace("Scene onUpdate (update: %x)\n", update);
      if (update!=NULL) {
        update(app, this);
      }
    }

    void onExit(App<R>* app) {
      info("Scene onExit (teardown: %x)\n", teardown);
      if (teardown!=NULL) {
        teardown(app, this);
      } 
      clearLayers();
    }

    void draw() {
      trace("scene draw (%i layers)\n", size);
      for (int layer=0; layer<size; layer++) {
        trace("Layer %i\n", layer);
        layers[layer](this);
      }
    }

  bool registerLayer(draw_callback layer) {
    if (size >= capacity) return false;
    info("Regisering layer %p (%i/%i)\n", layer, size+1, capacity);

    this->layers[size] = layer;
    size++;
    trace("registered %p as layer %i\n", layer, size);
    for (int i=0; i<size; i++) {
      trace("(Layer: %i)= %p\n", i, layers[i]);
    }
    return true;
  }

  // Remove a draw layer by function pointer
  void removeLayer(void(*layer_to_remove)(R *renderer)) {
    // Search for layer
    for (int l=0; l < size; l++) {

      // Remove layer
      if (layers[l] == layer_to_remove) {
        layers[l] = NULL;
        size--;

        // Update offsets
        for (; l<size; l++) {
          layers[l] = layers[l+1];
        }

        Serial.printf("removed layer %i\n", size);
        break;
      }
    }
  }

  // Remove all layers
  void clearLayers() {
    Serial.printf("Cleared all %i layers\n", size);
    memset(layers, 0, sizeof(void*) * size);
    size = 0;
  }

  public:
    void(*setup)(App<R>* app, Scene<R>* scene) = NULL;
    void(*update)(App<R>* app, Scene<R>* scene) = NULL;
    void(*teardown)(App<R>* app, Scene<R>* scene) = NULL;

    R *renderer = NULL;
    void *variables = NULL;
  private:
    draw_callback *layers = NULL;
    int capacity;
    int size;
};

template<class R> class App {
public:
  App(int scenes) {
    info("Setting up game (scene capacity: %i)\n", scenes);
    _scene = 0;
    _scene_capacity = scenes;
    _scenes = new Scene<R>*[_scene_capacity];
  }

  ~App() {
    _scenes[_scene]->onExit();
  }

  bool registerScene(Scene<R> *scene, int idx) {
    if (idx >= _scene_capacity) {
      warn("Tried to register a scene outside of capacity (%i/%i)\n", idx, _scene_capacity);
      return false;
    }
    if (scene == NULL) {
      warn("Tried to register NULL\n");
      return false;
    }

    _scenes[idx] = scene;
    info("Registered scene: %p to idx: %i\n", scene, idx);

    return true;
  }

  void start(int init_scene=0) {
    _scene = init_scene;
    if (_scene < _scene_capacity && _scenes[_scene] != NULL) {
      _scenes[_scene]->onEnter(this);
    }
    else {
      warn("[Warn] Scene %i not registered couln't start\n", _scene);
    }
  }

  void update() {
    _scenes[_scene]->onUpdate(this);
    _scenes[_scene]->draw();
  }

  int scene() {
    return _scene;
  }

  Scene<R>* getScene(int scene) {
    return _scenes[scene];
  }

  void transition(int scene) {
    info("Scene transition %i->%i (max: %i)\n", _scene, scene, _scene_capacity);
    _scenes[_scene]->onExit(this);
    _scene = scene;

    if (_scene < _scene_capacity && _scenes[_scene] != NULL) {
      _scenes[_scene]->onEnter(this);
    }
    else {
      warn("[Warn] Scene %i not registered couln't transition\n", _scene);
    }
  }

private:
  int _scene;
  int _scene_capacity;
  Scene<R> **_scenes;
};

typedef struct {
  int x;
  int y;
} Point;


#endif // !_COMPONENTS_H_
