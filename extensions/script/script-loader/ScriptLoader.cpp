#include <dlfcn.h>
#include <cassert>

namespace org::apache::nifi::minifi::extensions::script {
  class ScriptLoader {
    ScriptLoader() {
      lib_python_handle_ = dlopen("/usr/lib/libpython3.10.so", RTLD_NOW | RTLD_GLOBAL);
      concrete_scripting_extension_ = dlopen("./libminifi-script-extensions.so", RTLD_NOW | RTLD_LOCAL);
      assert(lib_python_handle_);
      assert(concrete_scripting_extension_);
    }

    ~ScriptLoader() {
      dlclose(lib_python_handle_);
      dlclose(concrete_scripting_extension_);
    }

   private:
    void* lib_python_handle_ = nullptr;
    void* concrete_scripting_extension_ = nullptr;
    static ScriptLoader script_loader_;
  };

  ScriptLoader ScriptLoader::script_loader_ = ScriptLoader();
}
