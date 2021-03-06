#ifndef TC_WREN_H
#define TC_WREN_H

#include "tinycoffee.h"

#include "external/wren/src/include/wren.h"
#include "modules/wren/module.h"

#ifndef TCDEF
  #define TCDEF extern
#endif

typedef struct {
  WrenVM *vm;
  WrenConfiguration config;
  WrenHandle *updateHandle;
  WrenHandle *drawHandle;

  WrenHandle *classHandle;
  tc_bool mainLoaded;
} tc_wren;

TCDEF tc_wren tc_init_wren(tc_bool packed);
TCDEF void tc_wren_update(tc_wren wren);
TCDEF void tc_wren_draw(tc_wren wren);

TCDEF WrenForeignMethodFn tc_wren_bind_methods(WrenVM *vm, const char *module, const char *className, bool isStatic, const char *signature);
TCDEF WrenForeignClassMethods tc_wren_bind_class(WrenVM *vm, const char *module, const char *className);

TCDEF WrenForeignMethodFn tc_wren_read_from_lib(const char *signature, tc_wren_lib *lib);
TCDEF WrenForeignMethodFn tc_wren_read_from_class(const char *signature, tc_bool isStatic, tc_wren_class class);

TCDEF tc_color wrenGetSlotColorList(WrenVM *vm, int slot);
TCDEF tc_rectangle wrenGetSlotRectList(WrenVM *vm, int slot);

#endif /* TC_WREN_H */

#if defined(TC_WREN_IMPLEMENTATION)

#include "modules/wren/graphics.h"
#include "modules/wren/timer.h"
#include "modules/wren/input.h"
#include "modules/wren/audio.h"
#include "modules/wren/math.h"

static void tc_wren_write(WrenVM *vm, const char *str) {
  printf("%s", str);
}

static void tc_wren_error(WrenVM *vm, WrenErrorType type, const char *module, int line, const char *message) {
  printf("wren: %s:%d error: %s\n", module, line, message);
}

static const char *tc_wren_resolve_module(WrenVM *vm, const char *importer, const char *name) {
  if (strstr(name, "tico.")) {
    return name;
  }
  size_t size = strlen(name) + strlen(".wren");
  char *rname = tc_replace_char((char*)name, '.', '/');

//   name = tc_replace_char(name, '.', '/');
  char *path = malloc(size);
  sprintf(path, "%s.wren", rname);

  return path;
}

static char * tc_wren_load_module(WrenVM *vm, const char *name) {
  tc_wren_module wrenModules[] = {
    {"tico.graphics", tcWrenGraphicsModule},
    {"tico.input", tcWrenInputModule},
    {"tico.timer", tcWrenTimerModule},
    {"tico.audio", tcWrenAudioModule},
    {"tico.math", tcWrenMathModule}
  };
  for (int i = 0; i < 5; i++) {
    tc_wren_module mod = wrenModules[i];
    if (!strcmp(name, mod.name)) {
      size_t size = strlen(mod.buffer);
      char *ticoMod = malloc(size + 1);
      ticoMod[size] = '\0';
      memcpy(ticoMod, mod.buffer, size);
      return ticoMod;
    }
  }

  char *buffer = tc_read_file(name, NULL);
  return buffer;
}

TCDEF tc_wren tc_init_wren(tc_bool packed) {
  tc_wren wren;
  wrenInitConfiguration(&wren.config);
  wren.config.resolveModuleFn = tc_wren_resolve_module;
  wren.config.writeFn = tc_wren_write;
  wren.config.errorFn = tc_wren_error;
  wren.config.loadModuleFn = tc_wren_load_module;
  wren.config.bindForeignMethodFn = tc_wren_bind_methods;
  wren.config.bindForeignClassFn = tc_wren_bind_class;

  wren.vm = wrenNewVM(&wren.config);
  wrenInterpret(wren.vm, NULL, "System.print(\"Wren Module loaded\")");
  tc_bool mainExists = TC_FALSE;
  // printf("%d\n", CORE.packed);
  mainExists = tc_file_exists("main.wren");
  if (mainExists) {
    wren.updateHandle = wrenMakeCallHandle(wren.vm, "update(_)");
    wren.drawHandle = wrenMakeCallHandle(wren.vm, "draw()");

    wrenInterpret(wren.vm, "tico_game", "import \"main\" for Game\n var g = Game.load()");
    wrenEnsureSlots(wren.vm, 1);
    wrenGetVariable(wren.vm, "tico_game", "g", 0);
    wren.classHandle = wrenGetSlotHandle(wren.vm, 0);
    wren_init_vec2(wren.vm);

    wren.mainLoaded = TC_TRUE;
  } else {
    ERROR("WREN", "main.wren not found\n");
    wren.mainLoaded = TC_FALSE;
  }

  return wren;
}

TCDEF void tc_wren_update(tc_wren wren) {
  if (!wren.mainLoaded) return;
  wrenSetSlotHandle(wren.vm, 0, wren.classHandle);
  wrenSetSlotDouble(wren.vm, 1, tc_get_delta());
  wrenCall(wren.vm, wren.updateHandle);
}

TCDEF void tc_wren_draw(tc_wren wren) {
  if (!wren.mainLoaded) return;
  wrenSetSlotHandle(wren.vm, 0, wren.classHandle);
  wrenCall(wren.vm, wren.drawHandle);
}

TCDEF WrenForeignMethodFn tc_wren_read_from_lib(const char *signature, tc_wren_lib *lib) {
  if (!lib) return NULL;
  for (tc_wren_lib *l = lib; l; l++) {
    if (!strcmp(l->name, signature))
      return l->fun;
  }

  return NULL;
}

TCDEF WrenForeignMethodFn tc_wren_read_from_class(const char *signature, tc_bool isStatic, tc_wren_class class) {
  size_t size = strlen(signature) + 2;
  char sSignature[size];
  memcpy(sSignature, signature, size);
  sSignature[size - 1] = '\0';
  if (isStatic) sprintf(sSignature, "s_%s", signature);

  return tc_wren_read_from_lib(sSignature, class.lib);
}

TCDEF tc_color wrenGetSlotColorList(WrenVM *vm, int slot) {
  tc_color color = WHITE;
  int size = wrenGetListCount(vm, slot);
  for (int i = 0; i < size; i++) {
    wrenGetListElement(vm, slot, i, 0);
    color.data[i] = wrenGetSlotDouble(vm, 0);
  }

  return color;
}

TCDEF tc_rectangle wrenGetSlotRectList(WrenVM *vm, int slot) {
  tc_rectangle rect = {0};
  int size = wrenGetListCount(vm, slot);
  for (int i = 0; i < size; i++) {
    wrenGetListElement(vm, slot, i, 0);
    rect.data[i] = wrenGetSlotDouble(vm, 0);
  }

  return rect;
}

/* Wren system methods */

tc_wren_class tcWrenClasses[] = {
  registerWrenClassInline("Render", NULL, NULL, wrenRenderLib),
  registerWrenClassInline("Input", NULL, NULL, wrenInputLib),
  registerWrenClassInline("Timer", NULL, NULL, wrenTimerLib),
  registerWrenClassInline("Texture", wren_texture_allocate, wren_texture_finalize, wrenTextureLib),
  registerWrenClassInline("ColorF", wren_color_allocate, wren_color_finalize, wrenColorLib),
  registerWrenClassInline("Sound", wren_sound_allocate, wren_sound_finalize, wrenSoundLib),
  registerWrenClassInline("Audio", NULL, NULL, wrenAudioLib),
  registerWrenClassInline("Canvas", wren_canvas_allocate, wren_canvas_finalize, wrenCanvasLib),
  registerWrenClassInline("Vector2", wren_vec2_allocate, wren_vec2_finalize, wrenVector2Lib),
  {NULL}
};

TCDEF WrenForeignMethodFn tc_wren_bind_methods(WrenVM *vm, const char *module, const char *className, bool isStatic, const char *signature) {
  for (tc_wren_class *class = tcWrenClasses; class; class++) {
    if (!strcmp(className, class->name))
      return tc_wren_read_from_class(signature, isStatic, *class);
  }

  return NULL;
}

TCDEF WrenForeignClassMethods tc_wren_bind_class(WrenVM *vm, const char *module, const char *className) {
  WrenForeignClassMethods methods;
  for (tc_wren_class *class = tcWrenClasses; class; class++) {
    if (!strcmp(className, class->name)) {
      methods.allocate = class->allocate;
      methods.finalize = class->finalize;
      return methods;
    }
  }

  methods.allocate = NULL;
  methods.finalize = NULL;

  return methods;
}

#endif /* TC_WREN_IMPLEMENTATION */
