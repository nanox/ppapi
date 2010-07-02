// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note that the single accessor, Module::Get(), is not actually implemented
// in this file.  This is an intentional hook that allows users of ppapi's
// C++ wrapper objects to provide difference semantics for how the singleton
// object is accessed.
//
// In general, users of ppapi will also link in ppp_entrypoints.cc, which
// provides a simple default implementation of Module::Get().
//
// A notable exception where the default ppp_entrypoints will not work is
// when implementing "internal plugins" that are statically linked into the
// browser. In this case, the process may actually have multiple Modules
// loaded at once making a traditional "singleton" unworkable.  To get around
// this, the users of ppapi need to get creative about how to properly
// implement the Module::Get() so that ppapi's C++ wrappers can find the
// right Module object.  One example solution is to use thread local storage
// to change the Module* returned based on which thread is invoking the
// function. Leaving Module::Get() unimplemented provides a hook for
// implementing such behavior.

#include "ppapi/cpp/module.h"

#include <string.h>

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/c/ppp_printing.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/resource.h"
#include "ppapi/cpp/url_loader.h"
#include "ppapi/cpp/var.h"

namespace pp {

// PPP_Instance implementation -------------------------------------------------

bool Instance_New(PP_Instance instance) {
  Module* module_singleton = Module::Get();
  if (!module_singleton)
    return false;
  Instance* obj = module_singleton->CreateInstance(instance);
  if (obj) {
    module_singleton->current_instances_[instance] = obj;
    return true;
  }
  return false;
}

void Instance_Delete(PP_Instance instance) {
  Module* module_singleton = Module::Get();
  if (!module_singleton)
    return;
  Module::InstanceMap::iterator found =
      module_singleton->current_instances_.find(instance);
  if (found == module_singleton->current_instances_.end())
    return;

  // Remove it from the map before deleting to try to catch reentrancy.
  Instance* obj = found->second;
  module_singleton->current_instances_.erase(found);
  delete obj;
}

bool Instance_Initialize(PP_Instance pp_instance,
                         uint32_t argc,
                         const char* argn[],
                         const char* argv[]) {
  Module* module_singleton = Module::Get();
  if (!module_singleton)
    return false;
  Instance* instance = module_singleton->InstanceForPPInstance(pp_instance);
  if (!instance)
    return false;
  return instance->Init(argc, argn, argv);
}

bool Instance_HandleDocumentLoad(PP_Instance pp_instance,
                                 PP_Resource pp_url_loader) {
  Module* module_singleton = Module::Get();
  if (!module_singleton)
    return false;
  Instance* instance = module_singleton->InstanceForPPInstance(pp_instance);
  if (!instance)
    return false;
  return instance->HandleDocumentLoad(URLLoader(pp_url_loader));
}

bool Instance_HandleEvent(PP_Instance pp_instance,
                          const PP_Event* event) {
  Module* module_singleton = Module::Get();
  if (!module_singleton)
    return false;
  Instance* instance = module_singleton->InstanceForPPInstance(pp_instance);
  if (!instance)
    return false;
  return instance->HandleEvent(*event);
}

PP_Var Instance_GetInstanceObject(PP_Instance pp_instance) {
  Module* module_singleton = Module::Get();
  if (!module_singleton)
    return Var().Detach();
  Instance* instance = module_singleton->InstanceForPPInstance(pp_instance);
  if (!instance)
    return Var().Detach();
  return instance->GetInstanceObject().Detach();
}

void Instance_ViewChanged(PP_Instance pp_instance,
                          const PP_Rect* position,
                          const PP_Rect* clip) {
  Module* module_singleton = Module::Get();
  if (!module_singleton)
    return;
  Instance* instance = module_singleton->InstanceForPPInstance(pp_instance);
  if (!instance)
    return;
  instance->ViewChanged(*position, *clip);
}

static PPP_Instance instance_interface = {
  &Instance_New,
  &Instance_Delete,
  &Instance_Initialize,
  &Instance_HandleDocumentLoad,
  &Instance_HandleEvent,
  &Instance_GetInstanceObject,
  &Instance_ViewChanged,
};

// PPP_Printing implementation -------------------------------------------------

const PP_PrintOutputFormat* Printing_QuerySupportedFormats(
    uint32_t* format_count) {
  Module* module_singleton = Module::Get();
  if (!module_singleton) {
    *format_count = 0;
    return NULL;
  }
  return module_singleton->QuerySupportedPrintOutputFormats(format_count);
}

int32_t Printing_Begin(PP_Instance pp_instance,
                       const PP_PrintSettings* print_settings) {
  Module* module_singleton = Module::Get();
  if (!module_singleton)
    return 0;
  Instance* instance = module_singleton->InstanceForPPInstance(pp_instance);
  if (!instance)
    return 0;

  // See if we support the specified print output format.
  uint32_t format_count = 0;
  const PP_PrintOutputFormat* formats =
      module_singleton->QuerySupportedPrintOutputFormats(&format_count);
  if (!formats)
    return 0;
  for (uint32_t index = 0; index < format_count; index++) {
    if (formats[index] == print_settings->format)
      return instance->PrintBegin(*print_settings);
  }
  return 0;
}

PP_Resource Printing_PrintPages(PP_Instance pp_instance,
                                const PP_PrintPageNumberRange* page_ranges,
                                uint32_t page_range_count) {
  Module* module_singleton = Module::Get();
  if (!module_singleton)
    return Resource().pp_resource();
  Instance* instance = module_singleton->InstanceForPPInstance(pp_instance);
  if (!instance)
    return Resource().pp_resource();
  return instance->PrintPages(page_ranges, page_range_count).pp_resource();
}

void Printing_End(PP_Instance pp_instance) {
  Module* module_singleton = Module::Get();
  if (!module_singleton)
    return;
  Instance* instance = module_singleton->InstanceForPPInstance(pp_instance);
  if (!instance)
    return;
  return instance->PrintEnd();
}

static PPP_Printing printing_interface = {
  &Printing_QuerySupportedFormats,
  &Printing_Begin,
  &Printing_PrintPages,
  &Printing_End,
};

// Module ----------------------------------------------------------------------

Module::Module() : pp_module_(NULL), get_browser_interface_(NULL), core_(NULL) {
}

Module::~Module() {
}

const void* Module::GetInstanceInterface(const char* interface_name) {
  if (strcmp(interface_name, PPP_INSTANCE_INTERFACE) == 0)
    return &instance_interface;
  if (strcmp(interface_name, PPP_PRINTING_INTERFACE) == 0)
    return &printing_interface;

  return NULL;
}

const void* Module::GetBrowserInterface(const char* interface_name) {
  return get_browser_interface_(interface_name);
}

Instance* Module::InstanceForPPInstance(PP_Instance instance) {
  InstanceMap::iterator found = current_instances_.find(instance);
  if (found == current_instances_.end())
    return NULL;
  return found->second;
}

bool Module::InternalInit(PP_Module mod,
                          PPB_GetInterface get_browser_interface) {
  pp_module_ = mod;
  get_browser_interface_ = get_browser_interface;
  core_ = reinterpret_cast<const PPB_Core*>(GetBrowserInterface(
      PPB_CORE_INTERFACE));
  if (!core_)
    return false;  // Can't run without the core interface.

  return Init();
}

}  // namespace pp