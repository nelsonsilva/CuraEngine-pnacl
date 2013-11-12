#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mount.h>

#include <ppapi/c/ppb_input_event.h>
#include <ppapi/cpp/var.h>
#include <ppapi/cpp/var_array.h>
#include <ppapi/cpp/var_array_buffer.h>
#include <ppapi/cpp/var_dictionary.h>

#include "nacl_io/nacl_io.h"

#include "ppapi/c/pp_var.h"
#include "ppapi_simple/ps.h"
#include "ppapi_simple/ps_event.h"
#include "ppapi_simple/ps_interface.h"
#include "ppapi_simple/ps_main.h"

#ifdef SEL_LDR
#define cura_engine_main main
#endif

extern int nacl_cura_engine_main(int argc, char *argv[]);

void PostMessage(const char* type, const char* data) {
  pp::VarDictionary message;
  message.Set("type", type);
  message.Set("data", data);
  PSInterfaceMessaging()->PostMessage(PSGetInstanceId(), message.pp_var());
}

char* VprintfToNewString(const char* format, va_list args) {
  va_list args_copy;
  int length;
  char* buffer;
  int result;

  va_copy(args_copy, args);
  length = vsnprintf(NULL, 0, format, args);
  buffer = (char*)malloc(length + 1); /* +1 for NULL-terminator. */
  result = vsnprintf(&buffer[0], length + 1, format, args_copy);
  if (result != length) {
    assert(0);
    return NULL;
  }
  return buffer;
}

void PostLog(const char* format, ...) {
  va_list args;
  va_start(args, format);
  char *msg = VprintfToNewString(format, args);
  PostMessage("log", msg);
  va_end(args);
  free(msg);
}

void call_cura_engine(const char *stl_file, const char * gcode_file) {

  char stl_path[strlen(stl_file) + 1];
  char gcode_path[strlen(gcode_file) + 1];
  sprintf(stl_path, "/%s", stl_file);
  sprintf(gcode_path, "/%s", gcode_file);

  const char *myargv[] = {"CuraEngine", "-v", "-o", gcode_path, stl_path};
  PostLog("Converting %s to %s\n", stl_file, gcode_file);  
  nacl_cura_engine_main(5, (char**)myargv);
  PostLog("Conversion done!");
}

void HandleEvent(PSEvent* ps_event) {
   if (ps_event->type == PSE_INSTANCE_HANDLEMESSAGE) {
    // Convert Pepper Simple message to PPAPI C++ vars
    pp::Var var(ps_event->as_var);
    if (var.is_dictionary()) {
      pp::VarDictionary dictionary(var);
      std::string message = dictionary.Get("cmd").AsString();
      PostLog("Running command: '%s'\n", message.c_str());
      if (message == "process") {
        const char * stl_file = strdup(dictionary.Get("stl").AsString().c_str());
        const char * gcode_file = strdup(dictionary.Get("gcode").AsString().c_str());
        call_cura_engine(stl_file, gcode_file);
        PostMessage("done", gcode_file);
      }
    } else {
      printf("Handle message unknown type: %s\n", var.DebugString().c_str());
    }
  }
}

int cura_engine_main(int argc, char* argv[]) {

  PostLog("Running Cura Engine");

  umount("/");
  mount("", "/", "html5fs", 0, "type=TEMPORARY");

  PSEventSetFilter(PSE_ALL);
  while (true) {
    PSEvent* ps_event;
    // Consume all available events
    while ((ps_event = PSEventTryAcquire()) != NULL) {
      HandleEvent(ps_event);
      PSEventRelease(ps_event);
    }

  }
}

PPAPI_SIMPLE_REGISTER_MAIN(cura_engine_main)