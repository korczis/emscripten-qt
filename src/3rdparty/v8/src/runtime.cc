// Copyright 2006-2009 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <stdlib.h>

#include "v8.h"

#include "accessors.h"
#include "api.h"
#include "arguments.h"
#include "codegen.h"
#include "compiler.h"
#include "cpu.h"
#include "dateparser-inl.h"
#include "debug.h"
#include "execution.h"
#include "jsregexp.h"
#include "liveedit.h"
#include "parser.h"
#include "platform.h"
#include "runtime.h"
#include "scopeinfo.h"
#include "smart-pointer.h"
#include "stub-cache.h"
#include "v8threads.h"
#include "string-search.h"

namespace v8 {
namespace internal {


#define RUNTIME_ASSERT(value) \
  if (!(value)) return isolate->ThrowIllegalOperation();

// Cast the given object to a value of the specified type and store
// it in a variable with the given name.  If the object is not of the
// expected type call IllegalOperation and return.
#define CONVERT_CHECKED(Type, name, obj)                             \
  RUNTIME_ASSERT(obj->Is##Type());                                   \
  Type* name = Type::cast(obj);

#define CONVERT_ARG_CHECKED(Type, name, index)                       \
  RUNTIME_ASSERT(args[index]->Is##Type());                           \
  Handle<Type> name = args.at<Type>(index);

// Cast the given object to a boolean and store it in a variable with
// the given name.  If the object is not a boolean call IllegalOperation
// and return.
#define CONVERT_BOOLEAN_CHECKED(name, obj)                            \
  RUNTIME_ASSERT(obj->IsBoolean());                                   \
  bool name = (obj)->IsTrue();

// Cast the given object to a Smi and store its value in an int variable
// with the given name.  If the object is not a Smi call IllegalOperation
// and return.
#define CONVERT_SMI_CHECKED(name, obj)                            \
  RUNTIME_ASSERT(obj->IsSmi());                                   \
  int name = Smi::cast(obj)->value();

// Cast the given object to a double and store it in a variable with
// the given name.  If the object is not a number (as opposed to
// the number not-a-number) call IllegalOperation and return.
#define CONVERT_DOUBLE_CHECKED(name, obj)                            \
  RUNTIME_ASSERT(obj->IsNumber());                                   \
  double name = (obj)->Number();

// Call the specified converter on the object *comand store the result in
// a variable of the specified type with the given name.  If the
// object is not a Number call IllegalOperation and return.
#define CONVERT_NUMBER_CHECKED(type, name, Type, obj)                \
  RUNTIME_ASSERT(obj->IsNumber());                                   \
  type name = NumberTo##Type(obj);


MUST_USE_RESULT static Object* DeepCopyBoilerplate(Isolate* isolate,
                                                   JSObject* boilerplate) {
  StackLimitCheck check(isolate);
  if (check.HasOverflowed()) return isolate->StackOverflow();

  Heap* heap = isolate->heap();
  Object* result = heap->CopyJSObject(boilerplate);
  if (result->IsFailure()) return result;
  JSObject* copy = JSObject::cast(result);

  // Deep copy local properties.
  if (copy->HasFastProperties()) {
    FixedArray* properties = copy->properties();
    for (int i = 0; i < properties->length(); i++) {
      Object* value = properties->get(i);
      if (value->IsJSObject()) {
        JSObject* js_object = JSObject::cast(value);
        result = DeepCopyBoilerplate(isolate, js_object);
        if (result->IsFailure()) return result;
        properties->set(i, result);
      }
    }
    int nof = copy->map()->inobject_properties();
    for (int i = 0; i < nof; i++) {
      Object* value = copy->InObjectPropertyAt(i);
      if (value->IsJSObject()) {
        JSObject* js_object = JSObject::cast(value);
        result = DeepCopyBoilerplate(isolate, js_object);
        if (result->IsFailure()) return result;
        copy->InObjectPropertyAtPut(i, result);
      }
    }
  } else {
    result = heap->AllocateFixedArray(copy->NumberOfLocalProperties(NONE));
    if (result->IsFailure()) return result;
    FixedArray* names = FixedArray::cast(result);
    copy->GetLocalPropertyNames(names, 0);
    for (int i = 0; i < names->length(); i++) {
      ASSERT(names->get(i)->IsString());
      String* key_string = String::cast(names->get(i));
      PropertyAttributes attributes =
          copy->GetLocalPropertyAttribute(key_string);
      // Only deep copy fields from the object literal expression.
      // In particular, don't try to copy the length attribute of
      // an array.
      if (attributes != NONE) continue;
      Object* value = copy->GetProperty(key_string, &attributes);
      ASSERT(!value->IsFailure());
      if (value->IsJSObject()) {
        JSObject* js_object = JSObject::cast(value);
        result = DeepCopyBoilerplate(isolate, js_object);
        if (result->IsFailure()) return result;
        result = copy->SetProperty(key_string, result, NONE);
        if (result->IsFailure()) return result;
      }
    }
  }

  // Deep copy local elements.
  // Pixel elements cannot be created using an object literal.
  ASSERT(!copy->HasPixelElements() && !copy->HasExternalArrayElements());
  switch (copy->GetElementsKind()) {
    case JSObject::FAST_ELEMENTS: {
      FixedArray* elements = FixedArray::cast(copy->elements());
      if (elements->map() == heap->fixed_cow_array_map()) {
        isolate->counters()->cow_arrays_created_runtime()->Increment();
#ifdef DEBUG
        for (int i = 0; i < elements->length(); i++) {
          ASSERT(!elements->get(i)->IsJSObject());
        }
#endif
      } else {
        for (int i = 0; i < elements->length(); i++) {
          Object* value = elements->get(i);
          if (value->IsJSObject()) {
            JSObject* js_object = JSObject::cast(value);
            result = DeepCopyBoilerplate(isolate, js_object);
            if (result->IsFailure()) return result;
            elements->set(i, result);
          }
        }
      }
      break;
    }
    case JSObject::DICTIONARY_ELEMENTS: {
      NumberDictionary* element_dictionary = copy->element_dictionary();
      int capacity = element_dictionary->Capacity();
      for (int i = 0; i < capacity; i++) {
        Object* k = element_dictionary->KeyAt(i);
        if (element_dictionary->IsKey(k)) {
          Object* value = element_dictionary->ValueAt(i);
          if (value->IsJSObject()) {
            JSObject* js_object = JSObject::cast(value);
            result = DeepCopyBoilerplate(isolate, js_object);
            if (result->IsFailure()) return result;
            element_dictionary->ValueAtPut(i, result);
          }
        }
      }
      break;
    }
    default:
      UNREACHABLE();
      break;
  }
  return copy;
}


static Object* Runtime_CloneLiteralBoilerplate(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  CONVERT_CHECKED(JSObject, boilerplate, args[0]);
  return DeepCopyBoilerplate(isolate, boilerplate);
}


static Object* Runtime_CloneShallowLiteralBoilerplate(
    RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  CONVERT_CHECKED(JSObject, boilerplate, args[0]);
  return isolate->heap()->CopyJSObject(boilerplate);
}


static Handle<Map> ComputeObjectLiteralMap(
    Handle<Context> context,
    Handle<FixedArray> constant_properties,
    bool* is_result_from_cache) {
  Isolate* isolate = context->GetIsolate();
  int properties_length = constant_properties->length();
  int number_of_properties = properties_length / 2;
  if (FLAG_canonicalize_object_literal_maps) {
    // Check that there are only symbols and array indices among keys.
    int number_of_symbol_keys = 0;
    for (int p = 0; p != properties_length; p += 2) {
      Object* key = constant_properties->get(p);
      uint32_t element_index = 0;
      if (key->IsSymbol()) {
        number_of_symbol_keys++;
      } else if (key->ToArrayIndex(&element_index)) {
        // An index key does not require space in the property backing store.
        number_of_properties--;
      } else {
        // Bail out as a non-symbol non-index key makes caching impossible.
        // ASSERT to make sure that the if condition after the loop is false.
        ASSERT(number_of_symbol_keys != number_of_properties);
        break;
      }
    }
    // If we only have symbols and array indices among keys then we can
    // use the map cache in the global context.
    const int kMaxKeys = 10;
    if ((number_of_symbol_keys == number_of_properties) &&
        (number_of_symbol_keys < kMaxKeys)) {
      // Create the fixed array with the key.
      Handle<FixedArray> keys =
          isolate->factory()->NewFixedArray(number_of_symbol_keys);
      if (number_of_symbol_keys > 0) {
        int index = 0;
        for (int p = 0; p < properties_length; p += 2) {
          Object* key = constant_properties->get(p);
          if (key->IsSymbol()) {
            keys->set(index++, key);
          }
        }
        ASSERT(index == number_of_symbol_keys);
      }
      *is_result_from_cache = true;
      return isolate->factory()->ObjectLiteralMapFromCache(context, keys);
    }
  }
  *is_result_from_cache = false;
  return isolate->factory()->CopyMap(
      Handle<Map>(context->object_function()->initial_map()),
      number_of_properties);
}


static Handle<Object> CreateLiteralBoilerplate(
    Isolate* isolate,
    Handle<FixedArray> literals,
    Handle<FixedArray> constant_properties);


static Handle<Object> CreateObjectLiteralBoilerplate(
    Isolate* isolate,
    Handle<FixedArray> literals,
    Handle<FixedArray> constant_properties,
    bool should_have_fast_elements) {
  // Get the global context from the literals array.  This is the
  // context in which the function was created and we use the object
  // function from this context to create the object literal.  We do
  // not use the object function from the current global context
  // because this might be the object function from another context
  // which we should not have access to.
  Handle<Context> context =
      Handle<Context>(JSFunction::GlobalContextFromLiterals(*literals));

  bool is_result_from_cache;
  Handle<Map> map = ComputeObjectLiteralMap(context,
                                            constant_properties,
                                            &is_result_from_cache);

  Handle<JSObject> boilerplate = isolate->factory()->NewJSObjectFromMap(map);

  // Normalize the elements of the boilerplate to save space if needed.
  if (!should_have_fast_elements) NormalizeElements(boilerplate);

  {  // Add the constant properties to the boilerplate.
    int length = constant_properties->length();
    OptimizedObjectForAddingMultipleProperties opt(boilerplate,
                                                   length / 2,
                                                   !is_result_from_cache);
    for (int index = 0; index < length; index +=2) {
      Handle<Object> key(constant_properties->get(index+0), isolate);
      Handle<Object> value(constant_properties->get(index+1), isolate);
      if (value->IsFixedArray()) {
        // The value contains the constant_properties of a
        // simple object literal.
        Handle<FixedArray> array = Handle<FixedArray>::cast(value);
        value = CreateLiteralBoilerplate(isolate, literals, array);
        if (value.is_null()) return value;
      }
      Handle<Object> result;
      uint32_t element_index = 0;
      if (key->IsSymbol()) {
        // If key is a symbol it is not an array element.
        Handle<String> name(String::cast(*key));
        ASSERT(!name->AsArrayIndex(&element_index));
        result = SetProperty(boilerplate, name, value, NONE);
      } else if (key->ToArrayIndex(&element_index)) {
        // Array index (uint32).
        result = SetElement(boilerplate, element_index, value);
      } else {
        // Non-uint32 number.
        ASSERT(key->IsNumber());
        double num = key->Number();
        char arr[100];
        Vector<char> buffer(arr, ARRAY_SIZE(arr));
        const char* str = DoubleToCString(num, buffer);
        Handle<String> name =
            isolate->factory()->NewStringFromAscii(CStrVector(str));
        result = SetProperty(boilerplate, name, value, NONE);
      }
      // If setting the property on the boilerplate throws an
      // exception, the exception is converted to an empty handle in
      // the handle based operations.  In that case, we need to
      // convert back to an exception.
      if (result.is_null()) return result;
    }
  }

  return boilerplate;
}


static Handle<Object> CreateArrayLiteralBoilerplate(
    Isolate* isolate,
    Handle<FixedArray> literals,
    Handle<FixedArray> elements) {
  // Create the JSArray.
  Handle<JSFunction> constructor(
      JSFunction::GlobalContextFromLiterals(*literals)->array_function());
  Handle<Object> object = isolate->factory()->NewJSObject(constructor);

  const bool is_cow =
      (elements->map() == isolate->heap()->fixed_cow_array_map());
  Handle<FixedArray> copied_elements =
      is_cow ? elements : isolate->factory()->CopyFixedArray(elements);

  Handle<FixedArray> content = Handle<FixedArray>::cast(copied_elements);
  if (is_cow) {
#ifdef DEBUG
    // Copy-on-write arrays must be shallow (and simple).
    for (int i = 0; i < content->length(); i++) {
      ASSERT(!content->get(i)->IsFixedArray());
    }
#endif
  } else {
    for (int i = 0; i < content->length(); i++) {
      if (content->get(i)->IsFixedArray()) {
        // The value contains the constant_properties of a
        // simple object literal.
        Handle<FixedArray> fa(FixedArray::cast(content->get(i)));
        Handle<Object> result =
            CreateLiteralBoilerplate(isolate, literals, fa);
        if (result.is_null()) return result;
        content->set(i, *result);
      }
    }
  }

  // Set the elements.
  Handle<JSArray>::cast(object)->SetContent(*content);
  return object;
}


static Handle<Object> CreateLiteralBoilerplate(
    Isolate* isolate,
    Handle<FixedArray> literals,
    Handle<FixedArray> array) {
  Handle<FixedArray> elements = CompileTimeValue::GetElements(array);
  switch (CompileTimeValue::GetType(array)) {
    case CompileTimeValue::OBJECT_LITERAL_FAST_ELEMENTS:
      return CreateObjectLiteralBoilerplate(isolate, literals, elements, true);
    case CompileTimeValue::OBJECT_LITERAL_SLOW_ELEMENTS:
      return CreateObjectLiteralBoilerplate(isolate, literals, elements, false);
    case CompileTimeValue::ARRAY_LITERAL:
      return CreateArrayLiteralBoilerplate(isolate, literals, elements);
    default:
      UNREACHABLE();
      return Handle<Object>::null();
  }
}


static Object* Runtime_CreateArrayLiteralBoilerplate(
    RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  // Takes a FixedArray of elements containing the literal elements of
  // the array literal and produces JSArray with those elements.
  // Additionally takes the literals array of the surrounding function
  // which contains the context from which to get the Array function
  // to use for creating the array literal.
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  CONVERT_ARG_CHECKED(FixedArray, literals, 0);
  CONVERT_SMI_CHECKED(literals_index, args[1]);
  CONVERT_ARG_CHECKED(FixedArray, elements, 2);

  Handle<Object> object =
      CreateArrayLiteralBoilerplate(isolate, literals, elements);
  if (object.is_null()) return Failure::Exception();

  // Update the functions literal and return the boilerplate.
  literals->set(literals_index, *object);
  return *object;
}


static Object* Runtime_CreateObjectLiteral(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 4);
  CONVERT_ARG_CHECKED(FixedArray, literals, 0);
  CONVERT_SMI_CHECKED(literals_index, args[1]);
  CONVERT_ARG_CHECKED(FixedArray, constant_properties, 2);
  CONVERT_SMI_CHECKED(fast_elements, args[3]);
  bool should_have_fast_elements = fast_elements == 1;

  // Check if boilerplate exists. If not, create it first.
  Handle<Object> boilerplate(literals->get(literals_index), isolate);
  if (*boilerplate == isolate->heap()->undefined_value()) {
    boilerplate = CreateObjectLiteralBoilerplate(isolate,
                                                 literals,
                                                 constant_properties,
                                                 should_have_fast_elements);
    if (boilerplate.is_null()) return Failure::Exception();
    // Update the functions literal and return the boilerplate.
    literals->set(literals_index, *boilerplate);
  }
  return DeepCopyBoilerplate(isolate, JSObject::cast(*boilerplate));
}


static Object* Runtime_CreateObjectLiteralShallow(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 4);
  CONVERT_ARG_CHECKED(FixedArray, literals, 0);
  CONVERT_SMI_CHECKED(literals_index, args[1]);
  CONVERT_ARG_CHECKED(FixedArray, constant_properties, 2);
  CONVERT_SMI_CHECKED(fast_elements, args[3]);
  bool should_have_fast_elements = fast_elements == 1;

  // Check if boilerplate exists. If not, create it first.
  Handle<Object> boilerplate(literals->get(literals_index), isolate);
  if (*boilerplate == isolate->heap()->undefined_value()) {
    boilerplate = CreateObjectLiteralBoilerplate(isolate,
                                                 literals,
                                                 constant_properties,
                                                 should_have_fast_elements);
    if (boilerplate.is_null()) return Failure::Exception();
    // Update the functions literal and return the boilerplate.
    literals->set(literals_index, *boilerplate);
  }
  return isolate->heap()->CopyJSObject(JSObject::cast(*boilerplate));
}


static Object* Runtime_CreateArrayLiteral(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  CONVERT_ARG_CHECKED(FixedArray, literals, 0);
  CONVERT_SMI_CHECKED(literals_index, args[1]);
  CONVERT_ARG_CHECKED(FixedArray, elements, 2);

  // Check if boilerplate exists. If not, create it first.
  Handle<Object> boilerplate(literals->get(literals_index), isolate);
  if (*boilerplate == isolate->heap()->undefined_value()) {
    boilerplate = CreateArrayLiteralBoilerplate(isolate, literals, elements);
    if (boilerplate.is_null()) return Failure::Exception();
    // Update the functions literal and return the boilerplate.
    literals->set(literals_index, *boilerplate);
  }
  return DeepCopyBoilerplate(isolate, JSObject::cast(*boilerplate));
}


static Object* Runtime_CreateArrayLiteralShallow(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  CONVERT_ARG_CHECKED(FixedArray, literals, 0);
  CONVERT_SMI_CHECKED(literals_index, args[1]);
  CONVERT_ARG_CHECKED(FixedArray, elements, 2);

  // Check if boilerplate exists. If not, create it first.
  Handle<Object> boilerplate(literals->get(literals_index), isolate);
  if (*boilerplate == isolate->heap()->undefined_value()) {
    boilerplate = CreateArrayLiteralBoilerplate(isolate, literals, elements);
    if (boilerplate.is_null()) return Failure::Exception();
    // Update the functions literal and return the boilerplate.
    literals->set(literals_index, *boilerplate);
  }
  if (JSObject::cast(*boilerplate)->elements()->map() ==
      isolate->heap()->fixed_cow_array_map()) {
    COUNTERS->cow_arrays_created_runtime()->Increment();
  }
  return isolate->heap()->CopyJSObject(JSObject::cast(*boilerplate));
}


static Object* Runtime_CreateCatchExtensionObject(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 2);
  CONVERT_CHECKED(String, key, args[0]);
  Object* value = args[1];
  // Create a catch context extension object.
  JSFunction* constructor =
      isolate->context()->global_context()->
          context_extension_function();
  Object* object = isolate->heap()->AllocateJSObject(constructor);
  if (object->IsFailure()) return object;
  // Assign the exception value to the catch variable and make sure
  // that the catch variable is DontDelete.
  value = JSObject::cast(object)->SetProperty(key, value, DONT_DELETE);
  if (value->IsFailure()) return value;
  return object;
}


static Object* Runtime_ClassOf(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);
  Object* obj = args[0];
  if (!obj->IsJSObject()) return isolate->heap()->null_value();
  return JSObject::cast(obj)->class_name();
}


static Object* Runtime_IsInPrototypeChain(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);
  // See ECMA-262, section 15.3.5.3, page 88 (steps 5 - 8).
  Object* O = args[0];
  Object* V = args[1];
  while (true) {
    Object* prototype = V->GetPrototype();
    if (prototype->IsNull()) return isolate->heap()->false_value();
    if (O == prototype) return isolate->heap()->true_value();
    V = prototype;
  }
}


// Inserts an object as the hidden prototype of another object.
static Object* Runtime_SetHiddenPrototype(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);
  CONVERT_CHECKED(JSObject, jsobject, args[0]);
  CONVERT_CHECKED(JSObject, proto, args[1]);

  // Sanity checks.  The old prototype (that we are replacing) could
  // theoretically be null, but if it is not null then check that we
  // didn't already install a hidden prototype here.
  RUNTIME_ASSERT(!jsobject->GetPrototype()->IsHeapObject() ||
    !HeapObject::cast(jsobject->GetPrototype())->map()->is_hidden_prototype());
  RUNTIME_ASSERT(!proto->map()->is_hidden_prototype());

  // Allocate up front before we start altering state in case we get a GC.
  Object* map_or_failure = proto->map()->CopyDropTransitions();
  if (map_or_failure->IsFailure()) return map_or_failure;
  Map* new_proto_map = Map::cast(map_or_failure);

  map_or_failure = jsobject->map()->CopyDropTransitions();
  if (map_or_failure->IsFailure()) return map_or_failure;
  Map* new_map = Map::cast(map_or_failure);

  // Set proto's prototype to be the old prototype of the object.
  new_proto_map->set_prototype(jsobject->GetPrototype());
  proto->set_map(new_proto_map);
  new_proto_map->set_is_hidden_prototype();

  // Set the object's prototype to proto.
  new_map->set_prototype(proto);
  jsobject->set_map(new_map);

  return isolate->heap()->undefined_value();
}


static Object* Runtime_IsConstructCall(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 0);
  JavaScriptFrameIterator it;
  return isolate->heap()->ToBoolean(it.frame()->IsConstructor());
}


// Recursively traverses hidden prototypes if property is not found
static void GetOwnPropertyImplementation(JSObject* obj,
                                         String* name,
                                         LookupResult* result) {
  obj->LocalLookupRealNamedProperty(name, result);

  if (!result->IsProperty()) {
    Object* proto = obj->GetPrototype();
    if (proto->IsJSObject() &&
      JSObject::cast(proto)->map()->is_hidden_prototype())
      GetOwnPropertyImplementation(JSObject::cast(proto),
                                   name, result);
  }
}


// Enumerator used as indices into the array returned from GetOwnProperty
enum PropertyDescriptorIndices {
  IS_ACCESSOR_INDEX,
  VALUE_INDEX,
  GETTER_INDEX,
  SETTER_INDEX,
  WRITABLE_INDEX,
  ENUMERABLE_INDEX,
  CONFIGURABLE_INDEX,
  DESCRIPTOR_SIZE
};

// Returns an array with the property description:
//  if args[1] is not a property on args[0]
//          returns undefined
//  if args[1] is a data property on args[0]
//         [false, value, Writeable, Enumerable, Configurable]
//  if args[1] is an accessor on args[0]
//         [true, GetFunction, SetFunction, Enumerable, Configurable]
static Object* Runtime_GetOwnProperty(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 2);
  Heap* heap = isolate->heap();
  HandleScope scope(isolate);
  Handle<FixedArray> elms = isolate->factory()->NewFixedArray(DESCRIPTOR_SIZE);
  Handle<JSArray> desc = isolate->factory()->NewJSArrayWithElements(elms);
  LookupResult result;
  CONVERT_ARG_CHECKED(JSObject, obj, 0);
  CONVERT_ARG_CHECKED(String, name, 1);

  // This could be an element.
  uint32_t index;
  if (name->AsArrayIndex(&index)) {
    switch (obj->HasLocalElement(index)) {
      case JSObject::UNDEFINED_ELEMENT:
        return heap->undefined_value();

      case JSObject::STRING_CHARACTER_ELEMENT: {
        // Special handling of string objects according to ECMAScript 5
        // 15.5.5.2. Note that this might be a string object with elements
        // other than the actual string value. This is covered by the
        // subsequent cases.
        Handle<JSValue> js_value = Handle<JSValue>::cast(obj);
        Handle<String> str(String::cast(js_value->value()));
        Handle<String> substr = SubString(str, index, index+1, NOT_TENURED);

        elms->set(IS_ACCESSOR_INDEX, heap->false_value());
        elms->set(VALUE_INDEX, *substr);
        elms->set(WRITABLE_INDEX, heap->false_value());
        elms->set(ENUMERABLE_INDEX,  heap->false_value());
        elms->set(CONFIGURABLE_INDEX, heap->false_value());
        return *desc;
      }

      case JSObject::INTERCEPTED_ELEMENT:
      case JSObject::FAST_ELEMENT: {
        elms->set(IS_ACCESSOR_INDEX, heap->false_value());
        Handle<Object> element = GetElement(Handle<Object>(obj), index);
        elms->set(VALUE_INDEX, *element);
        elms->set(WRITABLE_INDEX, heap->true_value());
        elms->set(ENUMERABLE_INDEX,  heap->true_value());
        elms->set(CONFIGURABLE_INDEX, heap->true_value());
        return *desc;
      }

      case JSObject::DICTIONARY_ELEMENT: {
        NumberDictionary* dictionary = obj->element_dictionary();
        int entry = dictionary->FindEntry(index);
        ASSERT(entry != NumberDictionary::kNotFound);
        PropertyDetails details = dictionary->DetailsAt(entry);
        switch (details.type()) {
          case CALLBACKS: {
            // This is an accessor property with getter and/or setter.
            FixedArray* callbacks =
                FixedArray::cast(dictionary->ValueAt(entry));
            elms->set(IS_ACCESSOR_INDEX, heap->true_value());
            elms->set(GETTER_INDEX, callbacks->get(0));
            elms->set(SETTER_INDEX, callbacks->get(1));
            break;
          }
          case NORMAL:
            // This is a data property.
            elms->set(IS_ACCESSOR_INDEX, heap->false_value());
            elms->set(VALUE_INDEX, dictionary->ValueAt(entry));
            elms->set(WRITABLE_INDEX, heap->ToBoolean(!details.IsReadOnly()));
            break;
          default:
            UNREACHABLE();
            break;
        }
        elms->set(ENUMERABLE_INDEX, heap->ToBoolean(!details.IsDontEnum()));
        elms->set(CONFIGURABLE_INDEX, heap->ToBoolean(!details.IsDontDelete()));
        return *desc;
      }
    }
  }

  // Use recursive implementation to also traverse hidden prototypes
  GetOwnPropertyImplementation(*obj, *name, &result);

  if (!result.IsProperty()) {
    return heap->undefined_value();
  }
  if (result.type() == CALLBACKS) {
    Object* structure = result.GetCallbackObject();
    if (structure->IsProxy() || structure->IsAccessorInfo()) {
      // Property that is internally implemented as a callback or
      // an API defined callback.
      Object* value = obj->GetPropertyWithCallback(
          *obj, structure, *name, result.holder());
      if (value->IsFailure()) return value;
      elms->set(IS_ACCESSOR_INDEX, heap->false_value());
      elms->set(VALUE_INDEX, value);
      elms->set(WRITABLE_INDEX, heap->ToBoolean(!result.IsReadOnly()));
    } else if (structure->IsFixedArray()) {
      // __defineGetter__/__defineSetter__ callback.
      elms->set(IS_ACCESSOR_INDEX, heap->true_value());
      elms->set(GETTER_INDEX, FixedArray::cast(structure)->get(0));
      elms->set(SETTER_INDEX, FixedArray::cast(structure)->get(1));
    } else {
      return heap->undefined_value();
    }
  } else {
    elms->set(IS_ACCESSOR_INDEX, heap->false_value());
    elms->set(VALUE_INDEX, result.GetLazyValue());
    elms->set(WRITABLE_INDEX, heap->ToBoolean(!result.IsReadOnly()));
  }

  elms->set(ENUMERABLE_INDEX, heap->ToBoolean(!result.IsDontEnum()));
  elms->set(CONFIGURABLE_INDEX, heap->ToBoolean(!result.IsDontDelete()));
  return *desc;
}


static Object* Runtime_PreventExtensions(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 1);
  CONVERT_CHECKED(JSObject, obj, args[0]);
  return obj->PreventExtensions();
}

static Object* Runtime_IsExtensible(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 1);
  CONVERT_CHECKED(JSObject, obj, args[0]);
  return obj->map()->is_extensible() ? isolate->heap()->true_value()
                                     : isolate->heap()->false_value();
}


static Object* Runtime_RegExpCompile(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  CONVERT_ARG_CHECKED(JSRegExp, re, 0);
  CONVERT_ARG_CHECKED(String, pattern, 1);
  CONVERT_ARG_CHECKED(String, flags, 2);
  Handle<Object> result = RegExpImpl::Compile(re, pattern, flags);
  if (result.is_null()) return Failure::Exception();
  return *result;
}


static Object* Runtime_CreateApiFunction(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(FunctionTemplateInfo, data, 0);
  return *isolate->factory()->CreateApiFunction(data);
}


static Object* Runtime_IsTemplate(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 1);
  Object* arg = args[0];
  bool result = arg->IsObjectTemplateInfo() || arg->IsFunctionTemplateInfo();
  return isolate->heap()->ToBoolean(result);
}


static Object* Runtime_GetTemplateField(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 2);
  CONVERT_CHECKED(HeapObject, templ, args[0]);
  CONVERT_CHECKED(Smi, field, args[1]);
  int index = field->value();
  int offset = index * kPointerSize + HeapObject::kHeaderSize;
  InstanceType type = templ->map()->instance_type();
  RUNTIME_ASSERT(type ==  FUNCTION_TEMPLATE_INFO_TYPE ||
                 type ==  OBJECT_TEMPLATE_INFO_TYPE);
  RUNTIME_ASSERT(offset > 0);
  if (type == FUNCTION_TEMPLATE_INFO_TYPE) {
    RUNTIME_ASSERT(offset < FunctionTemplateInfo::kSize);
  } else {
    RUNTIME_ASSERT(offset < ObjectTemplateInfo::kSize);
  }
  return *HeapObject::RawField(templ, offset);
}


static Object* Runtime_DisableAccessChecks(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 1);
  CONVERT_CHECKED(HeapObject, object, args[0]);
  Map* old_map = object->map();
  bool needs_access_checks = old_map->is_access_check_needed();
  if (needs_access_checks) {
    // Copy map so it won't interfere constructor's initial map.
    Object* new_map = old_map->CopyDropTransitions();
    if (new_map->IsFailure()) return new_map;

    Map::cast(new_map)->set_is_access_check_needed(false);
    object->set_map(Map::cast(new_map));
  }
  return needs_access_checks ? isolate->heap()->true_value()
                             : isolate->heap()->false_value();
}


static Object* Runtime_EnableAccessChecks(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 1);
  CONVERT_CHECKED(HeapObject, object, args[0]);
  Map* old_map = object->map();
  if (!old_map->is_access_check_needed()) {
    // Copy map so it won't interfere constructor's initial map.
    Object* new_map = old_map->CopyDropTransitions();
    if (new_map->IsFailure()) return new_map;

    Map::cast(new_map)->set_is_access_check_needed(true);
    object->set_map(Map::cast(new_map));
  }
  return isolate->heap()->undefined_value();
}


static Object* ThrowRedeclarationError(Isolate* isolate,
                                       const char* type,
                                       Handle<String> name) {
  HandleScope scope(isolate);
  Handle<Object> type_handle =
      isolate->factory()->NewStringFromAscii(CStrVector(type));
  Handle<Object> args[2] = { type_handle, name };
  Handle<Object> error =
      isolate->factory()->NewTypeError("redeclaration", HandleVector(args, 2));
  return isolate->Throw(*error);
}


static Object* Runtime_DeclareGlobals(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  Handle<GlobalObject> global = Handle<GlobalObject>(
      isolate->context()->global());

  Handle<Context> context = args.at<Context>(0);
  CONVERT_ARG_CHECKED(FixedArray, pairs, 1);
  bool is_eval = Smi::cast(args[2])->value() == 1;

  // Compute the property attributes. According to ECMA-262, section
  // 13, page 71, the property must be read-only and
  // non-deletable. However, neither SpiderMonkey nor KJS creates the
  // property as read-only, so we don't either.
  PropertyAttributes base = is_eval ? NONE : DONT_DELETE;

  // Traverse the name/value pairs and set the properties.
  int length = pairs->length();
  for (int i = 0; i < length; i += 2) {
    HandleScope scope(isolate);
    Handle<String> name(String::cast(pairs->get(i)));
    Handle<Object> value(pairs->get(i + 1), isolate);

    // We have to declare a global const property. To capture we only
    // assign to it when evaluating the assignment for "const x =
    // <expr>" the initial value is the hole.
    bool is_const_property = value->IsTheHole();

    if (value->IsUndefined() || is_const_property) {
      // Lookup the property in the global object, and don't set the
      // value of the variable if the property is already there.
      LookupResult lookup;
      global->Lookup(*name, &lookup);
      if (lookup.IsProperty()) {
        // Determine if the property is local by comparing the holder
        // against the global object. The information will be used to
        // avoid throwing re-declaration errors when declaring
        // variables or constants that exist in the prototype chain.
        bool is_local = (*global == lookup.holder());
        // Get the property attributes and determine if the property is
        // read-only.
        PropertyAttributes attributes = global->GetPropertyAttribute(*name);
        bool is_read_only = (attributes & READ_ONLY) != 0;
        if (lookup.type() == INTERCEPTOR) {
          // If the interceptor says the property is there, we
          // just return undefined without overwriting the property.
          // Otherwise, we continue to setting the property.
          if (attributes != ABSENT) {
            // Check if the existing property conflicts with regards to const.
            if (is_local && (is_read_only || is_const_property)) {
              const char* type = (is_read_only) ? "const" : "var";
              return ThrowRedeclarationError(isolate, type, name);
            };
            // The property already exists without conflicting: Go to
            // the next declaration.
            continue;
          }
          // Fall-through and introduce the absent property by using
          // SetProperty.
        } else {
          if (is_local && (is_read_only || is_const_property)) {
            const char* type = (is_read_only) ? "const" : "var";
            return ThrowRedeclarationError(isolate, type, name);
          }
          // The property already exists without conflicting: Go to
          // the next declaration.
          continue;
        }
      }
    } else {
      // Copy the function and update its context. Use it as value.
      Handle<SharedFunctionInfo> shared =
          Handle<SharedFunctionInfo>::cast(value);
      Handle<JSFunction> function =
          isolate->factory()->NewFunctionFromSharedFunctionInfo(shared,
                                                                context,
                                                                TENURED);
      value = function;
    }

    LookupResult lookup;
    global->LocalLookup(*name, &lookup);

    PropertyAttributes attributes = is_const_property
        ? static_cast<PropertyAttributes>(base | READ_ONLY)
        : base;

    if (lookup.IsProperty()) {
      // There's a local property that we need to overwrite because
      // we're either declaring a function or there's an interceptor
      // that claims the property is absent.

      // Check for conflicting re-declarations. We cannot have
      // conflicting types in case of intercepted properties because
      // they are absent.
      if (lookup.type() != INTERCEPTOR &&
          (lookup.IsReadOnly() || is_const_property)) {
        const char* type = (lookup.IsReadOnly()) ? "const" : "var";
        return ThrowRedeclarationError(isolate, type, name);
      }
      SetProperty(global, name, value, attributes);
    } else {
      // If a property with this name does not already exist on the
      // global object add the property locally.  We take special
      // precautions to always add it as a local property even in case
      // of callbacks in the prototype chain (this rules out using
      // SetProperty).  Also, we must use the handle-based version to
      // avoid GC issues.
      IgnoreAttributesAndSetLocalProperty(global, name, value, attributes);
    }
  }

  return isolate->heap()->undefined_value();
}


static Object* Runtime_DeclareContextSlot(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 4);

  CONVERT_ARG_CHECKED(Context, context, 0);
  Handle<String> name(String::cast(args[1]));
  PropertyAttributes mode =
      static_cast<PropertyAttributes>(Smi::cast(args[2])->value());
  RUNTIME_ASSERT(mode == READ_ONLY || mode == NONE);
  Handle<Object> initial_value(args[3], isolate);

  // Declarations are always done in the function context.
  context = Handle<Context>(context->fcontext());

  int index;
  PropertyAttributes attributes;
  ContextLookupFlags flags = DONT_FOLLOW_CHAINS;
  Handle<Object> holder =
      context->Lookup(name, flags, &index, &attributes);

  if (attributes != ABSENT) {
    // The name was declared before; check for conflicting
    // re-declarations: This is similar to the code in parser.cc in
    // the AstBuildingParser::Declare function.
    if (((attributes & READ_ONLY) != 0) || (mode == READ_ONLY)) {
      // Functions are not read-only.
      ASSERT(mode != READ_ONLY || initial_value->IsTheHole());
      const char* type = ((attributes & READ_ONLY) != 0) ? "const" : "var";
      return ThrowRedeclarationError(isolate, type, name);
    }

    // Initialize it if necessary.
    if (*initial_value != NULL) {
      if (index >= 0) {
        // The variable or constant context slot should always be in
        // the function context or the arguments object.
        if (holder->IsContext()) {
          ASSERT(holder.is_identical_to(context));
          if (((attributes & READ_ONLY) == 0) ||
              context->get(index)->IsTheHole()) {
            context->set(index, *initial_value);
          }
        } else {
          // The holder is an arguments object.
          Handle<JSObject> arguments(Handle<JSObject>::cast(holder));
          SetElement(arguments, index, initial_value);
        }
      } else {
        // Slow case: The property is not in the FixedArray part of the context.
        Handle<JSObject> context_ext = Handle<JSObject>::cast(holder);
        SetProperty(context_ext, name, initial_value, mode);
      }
    }

  } else {
    // The property is not in the function context. It needs to be
    // "declared" in the function context's extension context, or in the
    // global context.
    Handle<JSObject> context_ext;
    if (context->has_extension()) {
      // The function context's extension context exists - use it.
      context_ext = Handle<JSObject>(context->extension());
    } else {
      // The function context's extension context does not exists - allocate
      // it.
      context_ext = isolate->factory()->NewJSObject(
          isolate->context_extension_function());
      // And store it in the extension slot.
      context->set_extension(*context_ext);
    }
    ASSERT(*context_ext != NULL);

    // Declare the property by setting it to the initial value if provided,
    // or undefined, and use the correct mode (e.g. READ_ONLY attribute for
    // constant declarations).
    ASSERT(!context_ext->HasLocalProperty(*name));
    Handle<Object> value(isolate->heap()->undefined_value(), isolate);
    if (*initial_value != NULL) value = initial_value;
    SetProperty(context_ext, name, value, mode);
    ASSERT(context_ext->GetLocalPropertyAttribute(*name) == mode);
  }

  return isolate->heap()->undefined_value();
}


static Object* Runtime_InitializeVarGlobal(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation nha;

  // Determine if we need to assign to the variable if it already
  // exists (based on the number of arguments).
  RUNTIME_ASSERT(args.length() == 1 || args.length() == 2);
  bool assign = args.length() == 2;

  CONVERT_ARG_CHECKED(String, name, 0);
  GlobalObject* global = isolate->context()->global();

  // According to ECMA-262, section 12.2, page 62, the property must
  // not be deletable.
  PropertyAttributes attributes = DONT_DELETE;

  // Lookup the property locally in the global object. If it isn't
  // there, there is a property with this name in the prototype chain.
  // We follow Safari and Firefox behavior and only set the property
  // locally if there is an explicit initialization value that we have
  // to assign to the property. When adding the property we take
  // special precautions to always add it as a local property even in
  // case of callbacks in the prototype chain (this rules out using
  // SetProperty).  We have IgnoreAttributesAndSetLocalProperty for
  // this.
  // Note that objects can have hidden prototypes, so we need to traverse
  // the whole chain of hidden prototypes to do a 'local' lookup.
  JSObject* real_holder = global;
  LookupResult lookup;
  while (true) {
    real_holder->LocalLookup(*name, &lookup);
    if (lookup.IsProperty()) {
      // Determine if this is a redeclaration of something read-only.
      if (lookup.IsReadOnly()) {
        // If we found readonly property on one of hidden prototypes,
        // just shadow it.
        if (real_holder != isolate->context()->global()) break;
        return ThrowRedeclarationError(isolate, "const", name);
      }

      // Determine if this is a redeclaration of an intercepted read-only
      // property and figure out if the property exists at all.
      bool found = true;
      PropertyType type = lookup.type();
      if (type == INTERCEPTOR) {
        HandleScope handle_scope(isolate);
        Handle<JSObject> holder(real_holder);
        PropertyAttributes intercepted = holder->GetPropertyAttribute(*name);
        real_holder = *holder;
        if (intercepted == ABSENT) {
          // The interceptor claims the property isn't there. We need to
          // make sure to introduce it.
          found = false;
        } else if ((intercepted & READ_ONLY) != 0) {
          // The property is present, but read-only. Since we're trying to
          // overwrite it with a variable declaration we must throw a
          // re-declaration error.  However if we found readonly property
          // on one of hidden prototypes, just shadow it.
          if (real_holder != isolate->context()->global()) break;
          return ThrowRedeclarationError(isolate, "const", name);
        }
      }

      if (found && !assign) {
        // The global property is there and we're not assigning any value
        // to it. Just return.
        return isolate->heap()->undefined_value();
      }

      // Assign the value (or undefined) to the property.
      Object* value = (assign) ? args[1] : isolate->heap()->undefined_value();
      return real_holder->SetProperty(&lookup, *name, value, attributes);
    }

    Object* proto = real_holder->GetPrototype();
    if (!proto->IsJSObject())
      break;

    if (!JSObject::cast(proto)->map()->is_hidden_prototype())
      break;

    real_holder = JSObject::cast(proto);
  }

  global = isolate->context()->global();
  if (assign) {
    return global->IgnoreAttributesAndSetLocalProperty(*name,
                                                       args[1],
                                                       attributes);
  }
  return isolate->heap()->undefined_value();
}


static Object* Runtime_InitializeConstGlobal(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  // All constants are declared with an initial value. The name
  // of the constant is the first argument and the initial value
  // is the second.
  RUNTIME_ASSERT(args.length() == 2);
  CONVERT_ARG_CHECKED(String, name, 0);
  Handle<Object> value = args.at<Object>(1);

  // Get the current global object from top.
  GlobalObject* global = isolate->context()->global();

  // According to ECMA-262, section 12.2, page 62, the property must
  // not be deletable. Since it's a const, it must be READ_ONLY too.
  PropertyAttributes attributes =
      static_cast<PropertyAttributes>(DONT_DELETE | READ_ONLY);

  // Lookup the property locally in the global object. If it isn't
  // there, we add the property and take special precautions to always
  // add it as a local property even in case of callbacks in the
  // prototype chain (this rules out using SetProperty).
  // We use IgnoreAttributesAndSetLocalProperty instead
  LookupResult lookup;
  global->LocalLookup(*name, &lookup);
  if (!lookup.IsProperty()) {
    return global->IgnoreAttributesAndSetLocalProperty(*name,
                                                       *value,
                                                       attributes);
  }

  // Determine if this is a redeclaration of something not
  // read-only. In case the result is hidden behind an interceptor we
  // need to ask it for the property attributes.
  if (!lookup.IsReadOnly()) {
    if (lookup.type() != INTERCEPTOR) {
      return ThrowRedeclarationError(isolate, "var", name);
    }

    PropertyAttributes intercepted = global->GetPropertyAttribute(*name);

    // Throw re-declaration error if the intercepted property is present
    // but not read-only.
    if (intercepted != ABSENT && (intercepted & READ_ONLY) == 0) {
      return ThrowRedeclarationError(isolate, "var", name);
    }

    // Restore global object from context (in case of GC) and continue
    // with setting the value because the property is either absent or
    // read-only. We also have to do redo the lookup.
    global = isolate->context()->global();

    // BUG 1213579: Handle the case where we have to set a read-only
    // property through an interceptor and only do it if it's
    // uninitialized, e.g. the hole. Nirk...
    global->SetProperty(*name, *value, attributes);
    return *value;
  }

  // Set the value, but only we're assigning the initial value to a
  // constant. For now, we determine this by checking if the
  // current value is the hole.
  PropertyType type = lookup.type();
  if (type == FIELD) {
    FixedArray* properties = global->properties();
    int index = lookup.GetFieldIndex();
    if (properties->get(index)->IsTheHole()) {
      properties->set(index, *value);
    }
  } else if (type == NORMAL) {
    if (global->GetNormalizedProperty(&lookup)->IsTheHole()) {
      global->SetNormalizedProperty(&lookup, *value);
    }
  } else {
    // Ignore re-initialization of constants that have already been
    // assigned a function value.
    ASSERT(lookup.IsReadOnly() && type == CONSTANT_FUNCTION);
  }

  // Use the set value as the result of the operation.
  return *value;
}


static Object* Runtime_InitializeConstContextSlot(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);

  Handle<Object> value(args[0], isolate);
  ASSERT(!value->IsTheHole());
  CONVERT_ARG_CHECKED(Context, context, 1);
  Handle<String> name(String::cast(args[2]));

  // Initializations are always done in the function context.
  context = Handle<Context>(context->fcontext());

  int index;
  PropertyAttributes attributes;
  ContextLookupFlags flags = FOLLOW_CHAINS;
  Handle<Object> holder =
      context->Lookup(name, flags, &index, &attributes);

  // In most situations, the property introduced by the const
  // declaration should be present in the context extension object.
  // However, because declaration and initialization are separate, the
  // property might have been deleted (if it was introduced by eval)
  // before we reach the initialization point.
  //
  // Example:
  //
  //    function f() { eval("delete x; const x;"); }
  //
  // In that case, the initialization behaves like a normal assignment
  // to property 'x'.
  if (index >= 0) {
    // Property was found in a context.
    if (holder->IsContext()) {
      // The holder cannot be the function context.  If it is, there
      // should have been a const redeclaration error when declaring
      // the const property.
      ASSERT(!holder.is_identical_to(context));
      if ((attributes & READ_ONLY) == 0) {
        Handle<Context>::cast(holder)->set(index, *value);
      }
    } else {
      // The holder is an arguments object.
      ASSERT((attributes & READ_ONLY) == 0);
      Handle<JSObject> arguments(Handle<JSObject>::cast(holder));
      SetElement(arguments, index, value);
    }
    return *value;
  }

  // The property could not be found, we introduce it in the global
  // context.
  if (attributes == ABSENT) {
    Handle<JSObject> global = Handle<JSObject>(
        isolate->context()->global());
    SetProperty(global, name, value, NONE);
    return *value;
  }

  // The property was present in a context extension object.
  Handle<JSObject> context_ext = Handle<JSObject>::cast(holder);

  if (*context_ext == context->extension()) {
    // This is the property that was introduced by the const
    // declaration.  Set it if it hasn't been set before.  NOTE: We
    // cannot use GetProperty() to get the current value as it
    // 'unholes' the value.
    LookupResult lookup;
    context_ext->LocalLookupRealNamedProperty(*name, &lookup);
    ASSERT(lookup.IsProperty());  // the property was declared
    ASSERT(lookup.IsReadOnly());  // and it was declared as read-only

    PropertyType type = lookup.type();
    if (type == FIELD) {
      FixedArray* properties = context_ext->properties();
      int index = lookup.GetFieldIndex();
      if (properties->get(index)->IsTheHole()) {
        properties->set(index, *value);
      }
    } else if (type == NORMAL) {
      if (context_ext->GetNormalizedProperty(&lookup)->IsTheHole()) {
        context_ext->SetNormalizedProperty(&lookup, *value);
      }
    } else {
      // We should not reach here. Any real, named property should be
      // either a field or a dictionary slot.
      UNREACHABLE();
    }
  } else {
    // The property was found in a different context extension object.
    // Set it if it is not a read-only property.
    if ((attributes & READ_ONLY) == 0) {
      Handle<Object> set = SetProperty(context_ext, name, value, attributes);
      // Setting a property might throw an exception.  Exceptions
      // are converted to empty handles in handle operations.  We
      // need to convert back to exceptions here.
      if (set.is_null()) {
        ASSERT(isolate->has_pending_exception());
        return Failure::Exception();
      }
    }
  }

  return *value;
}


static Object* Runtime_OptimizeObjectForAddingMultipleProperties(
    RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_CHECKED(JSObject, object, 0);
  CONVERT_SMI_CHECKED(properties, args[1]);
  if (object->HasFastProperties()) {
    NormalizeProperties(object, KEEP_INOBJECT_PROPERTIES, properties);
  }
  return *object;
}


static Object* Runtime_RegExpExec(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 4);
  CONVERT_ARG_CHECKED(JSRegExp, regexp, 0);
  CONVERT_ARG_CHECKED(String, subject, 1);
  // Due to the way the JS calls are constructed this must be less than the
  // length of a string, i.e. it is always a Smi.  We check anyway for security.
  CONVERT_SMI_CHECKED(index, args[2]);
  CONVERT_ARG_CHECKED(JSArray, last_match_info, 3);
  RUNTIME_ASSERT(last_match_info->HasFastElements());
  RUNTIME_ASSERT(index >= 0);
  RUNTIME_ASSERT(index <= subject->length());
  isolate->counters()->regexp_entry_runtime()->Increment();
  Handle<Object> result = RegExpImpl::Exec(regexp,
                                           subject,
                                           index,
                                           last_match_info);
  if (result.is_null()) return Failure::Exception();
  return *result;
}


static Object* Runtime_RegExpConstructResult(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 3);
  CONVERT_SMI_CHECKED(elements_count, args[0]);
  if (elements_count > JSArray::kMaxFastElementsLength) {
    return isolate->ThrowIllegalOperation();
  }
  Object* new_object =
      isolate->heap()->AllocateFixedArrayWithHoles(elements_count);
  if (new_object->IsFailure()) return new_object;
  FixedArray* elements = FixedArray::cast(new_object);
  new_object = isolate->heap()->AllocateRaw(JSRegExpResult::kSize,
                                            NEW_SPACE,
                                            OLD_POINTER_SPACE);
  if (new_object->IsFailure()) return new_object;
  {
    AssertNoAllocation no_gc;
    HandleScope scope(isolate);
    reinterpret_cast<HeapObject*>(new_object)->
        set_map(isolate->global_context()->regexp_result_map());
  }
  JSArray* array = JSArray::cast(new_object);
  array->set_properties(isolate->heap()->empty_fixed_array());
  array->set_elements(elements);
  array->set_length(Smi::FromInt(elements_count));
  // Write in-object properties after the length of the array.
  array->InObjectPropertyAtPut(JSRegExpResult::kIndexIndex, args[1]);
  array->InObjectPropertyAtPut(JSRegExpResult::kInputIndex, args[2]);
  return array;
}


static Object* Runtime_RegExpCloneResult(RUNTIME_CALLING_CONVENTION) {
  ASSERT(args.length() == 1);
  Map* regexp_result_map;
  {
    AssertNoAllocation no_gc;
    HandleScope handles;
    regexp_result_map = isolate->global_context()->regexp_result_map();
  }
  if (!args[0]->IsJSArray()) return args[0];

  JSArray* result = JSArray::cast(args[0]);
  // Arguments to RegExpCloneResult should always be fresh RegExp exec call
  // results (either a fresh JSRegExpResult or null).
  // If the argument is not a JSRegExpResult, or isn't unmodified, just return
  // the argument uncloned.
  if (result->map() != regexp_result_map) return result;

  // Having the original JSRegExpResult map guarantees that we have
  // fast elements and no properties except the two in-object properties.
  ASSERT(result->HasFastElements());
  ASSERT(result->properties() == isolate->heap()->empty_fixed_array());
  ASSERT_EQ(2, regexp_result_map->inobject_properties());

  Object* new_array_alloc = isolate->heap()->AllocateRaw(JSRegExpResult::kSize,
                                                         NEW_SPACE,
                                                         OLD_POINTER_SPACE);
  if (new_array_alloc->IsFailure()) return new_array_alloc;

  // Set HeapObject map to JSRegExpResult map.
  reinterpret_cast<HeapObject*>(new_array_alloc)->set_map(regexp_result_map);

  JSArray* new_array = JSArray::cast(new_array_alloc);

  // Copy JSObject properties.
  new_array->set_properties(result->properties());  // Empty FixedArray.

  // Copy JSObject elements as copy-on-write.
  FixedArray* elements = FixedArray::cast(result->elements());
  if (elements != isolate->heap()->empty_fixed_array()) {
    elements->set_map(isolate->heap()->fixed_cow_array_map());
  }
  new_array->set_elements(elements);

  // Copy JSArray length.
  new_array->set_length(result->length());

  // Copy JSRegExpResult in-object property fields input and index.
  new_array->FastPropertyAtPut(JSRegExpResult::kIndexIndex,
                               result->FastPropertyAt(
                                   JSRegExpResult::kIndexIndex));
  new_array->FastPropertyAtPut(JSRegExpResult::kInputIndex,
                               result->FastPropertyAt(
                                   JSRegExpResult::kInputIndex));
  return new_array;
}


static Object* Runtime_RegExpInitializeObject(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  AssertNoAllocation no_alloc;
  ASSERT(args.length() == 5);
  CONVERT_CHECKED(JSRegExp, regexp, args[0]);
  CONVERT_CHECKED(String, source, args[1]);

  Object* global = args[2];
  if (!global->IsTrue()) global = isolate->heap()->false_value();

  Object* ignoreCase = args[3];
  if (!ignoreCase->IsTrue()) ignoreCase = isolate->heap()->false_value();

  Object* multiline = args[4];
  if (!multiline->IsTrue()) multiline = isolate->heap()->false_value();

  Map* map = regexp->map();
  Object* constructor = map->constructor();
  if (constructor->IsJSFunction() &&
      JSFunction::cast(constructor)->initial_map() == map) {
    // If we still have the original map, set in-object properties directly.
    regexp->InObjectPropertyAtPut(JSRegExp::kSourceFieldIndex, source);
    // TODO(lrn): Consider skipping write barrier on booleans as well.
    // Both true and false should be in oldspace at all times.
    regexp->InObjectPropertyAtPut(JSRegExp::kGlobalFieldIndex, global);
    regexp->InObjectPropertyAtPut(JSRegExp::kIgnoreCaseFieldIndex, ignoreCase);
    regexp->InObjectPropertyAtPut(JSRegExp::kMultilineFieldIndex, multiline);
    regexp->InObjectPropertyAtPut(JSRegExp::kLastIndexFieldIndex,
                                  Smi::FromInt(0),
                                  SKIP_WRITE_BARRIER);
    return regexp;
  }

  // Map has changed, so use generic, but slower, method.
  PropertyAttributes final =
      static_cast<PropertyAttributes>(READ_ONLY | DONT_ENUM | DONT_DELETE);
  PropertyAttributes writable =
      static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE);
  regexp->IgnoreAttributesAndSetLocalProperty(isolate->heap()->source_symbol(),
                                              source,
                                              final);
  regexp->IgnoreAttributesAndSetLocalProperty(isolate->heap()->global_symbol(),
                                              global,
                                              final);
  regexp->IgnoreAttributesAndSetLocalProperty(
      isolate->heap()->ignore_case_symbol(), ignoreCase, final);
  regexp->IgnoreAttributesAndSetLocalProperty(
      isolate->heap()->multiline_symbol(), multiline, final);
  regexp->IgnoreAttributesAndSetLocalProperty(
      isolate->heap()->last_index_symbol(), Smi::FromInt(0), writable);
  return regexp;
}


static Object* Runtime_FinishArrayPrototypeSetup(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(JSArray, prototype, 0);
  // This is necessary to enable fast checks for absence of elements
  // on Array.prototype and below.
  prototype->set_elements(isolate->heap()->empty_fixed_array());
  return Smi::FromInt(0);
}


static Handle<JSFunction> InstallBuiltin(Isolate* isolate,
                                         Handle<JSObject> holder,
                                         const char* name,
                                         Builtins::Name builtin_name) {
  Handle<String> key = isolate->factory()->LookupAsciiSymbol(name);
  Handle<Code> code(isolate->builtins()->builtin(builtin_name));
  Handle<JSFunction> optimized =
      isolate->factory()->NewFunction(key,
                                      JS_OBJECT_TYPE,
                                      JSObject::kHeaderSize,
                                      code,
                                      false);
  optimized->shared()->DontAdaptArguments();
  SetProperty(holder, key, optimized, NONE);
  return optimized;
}


static Object* Runtime_SpecialArrayFunctions(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(JSObject, holder, 0);

  InstallBuiltin(isolate, holder, "pop", Builtins::ArrayPop);
  InstallBuiltin(isolate, holder, "push", Builtins::ArrayPush);
  InstallBuiltin(isolate, holder, "shift", Builtins::ArrayShift);
  InstallBuiltin(isolate, holder, "unshift", Builtins::ArrayUnshift);
  InstallBuiltin(isolate, holder, "slice", Builtins::ArraySlice);
  InstallBuiltin(isolate, holder, "splice", Builtins::ArraySplice);
  InstallBuiltin(isolate, holder, "concat", Builtins::ArrayConcat);

  return *holder;
}


static Object* Runtime_GetGlobalReceiver(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  // Returns a real global receiver, not one of builtins object.
  Context* global_context =
      isolate->context()->global()->global_context();
  return global_context->global()->global_receiver();
}


static Object* Runtime_MaterializeRegExpLiteral(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 4);
  CONVERT_ARG_CHECKED(FixedArray, literals, 0);
  int index = Smi::cast(args[1])->value();
  Handle<String> pattern = args.at<String>(2);
  Handle<String> flags = args.at<String>(3);

  // Get the RegExp function from the context in the literals array.
  // This is the RegExp function from the context in which the
  // function was created.  We do not use the RegExp function from the
  // current global context because this might be the RegExp function
  // from another context which we should not have access to.
  Handle<JSFunction> constructor =
      Handle<JSFunction>(
          JSFunction::GlobalContextFromLiterals(*literals)->regexp_function());
  // Compute the regular expression literal.
  bool has_pending_exception;
  Handle<Object> regexp =
      RegExpImpl::CreateRegExpLiteral(constructor, pattern, flags,
                                      &has_pending_exception);
  if (has_pending_exception) {
    ASSERT(isolate->has_pending_exception());
    return Failure::Exception();
  }
  literals->set(index, *regexp);
  return *regexp;
}


static Object* Runtime_FunctionGetName(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);

  CONVERT_CHECKED(JSFunction, f, args[0]);
  return f->shared()->name();
}


static Object* Runtime_FunctionSetName(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_CHECKED(JSFunction, f, args[0]);
  CONVERT_CHECKED(String, name, args[1]);
  f->shared()->set_name(name);
  return isolate->heap()->undefined_value();
}


static Object* Runtime_FunctionRemovePrototype(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);

  CONVERT_CHECKED(JSFunction, f, args[0]);
  Object* obj = f->RemovePrototype();
  if (obj->IsFailure()) return obj;

  return isolate->heap()->undefined_value();
}


static Object* Runtime_FunctionGetScript(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);

  CONVERT_CHECKED(JSFunction, fun, args[0]);
  Handle<Object> script = Handle<Object>(fun->shared()->script(), isolate);
  if (!script->IsScript()) return isolate->heap()->undefined_value();

  return *GetScriptWrapper(Handle<Script>::cast(script));
}


static Object* Runtime_FunctionGetSourceCode(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);

  CONVERT_CHECKED(JSFunction, f, args[0]);
  return f->shared()->GetSourceCode();
}


static Object* Runtime_FunctionGetScriptSourcePosition(
    RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);

  CONVERT_CHECKED(JSFunction, fun, args[0]);
  int pos = fun->shared()->start_position();
  return Smi::FromInt(pos);
}


static Object* Runtime_FunctionGetPositionForOffset(
    RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 2);

  CONVERT_CHECKED(JSFunction, fun, args[0]);
  CONVERT_NUMBER_CHECKED(int, offset, Int32, args[1]);

  Code* code = fun->code();
  RUNTIME_ASSERT(0 <= offset && offset < code->Size());

  Address pc = code->address() + offset;
  return Smi::FromInt(fun->code()->SourcePosition(pc));
}



static Object* Runtime_FunctionSetInstanceClassName(
    RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_CHECKED(JSFunction, fun, args[0]);
  CONVERT_CHECKED(String, name, args[1]);
  fun->SetInstanceClassName(name);
  return isolate->heap()->undefined_value();
}


static Object* Runtime_FunctionSetLength(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_CHECKED(JSFunction, fun, args[0]);
  CONVERT_CHECKED(Smi, length, args[1]);
  fun->shared()->set_length(length->value());
  return length;
}


static Object* Runtime_FunctionSetPrototype(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_CHECKED(JSFunction, fun, args[0]);
  ASSERT(fun->should_have_prototype());
  Object* obj = Accessors::FunctionSetPrototype(fun, args[1], NULL);
  if (obj->IsFailure()) return obj;
  return args[0];  // return TOS
}


static Object* Runtime_FunctionIsAPIFunction(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);

  CONVERT_CHECKED(JSFunction, f, args[0]);
  return f->shared()->IsApiFunction() ? isolate->heap()->true_value()
                                      : isolate->heap()->false_value();
}

static Object* Runtime_FunctionIsBuiltin(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);

  CONVERT_CHECKED(JSFunction, f, args[0]);
  return f->IsBuiltin() ? isolate->heap()->true_value() :
                          isolate->heap()->false_value();
}


static Object* Runtime_SetCode(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);

  CONVERT_ARG_CHECKED(JSFunction, target, 0);
  Handle<Object> code = args.at<Object>(1);

  Handle<Context> context(target->context());

  if (!code->IsNull()) {
    RUNTIME_ASSERT(code->IsJSFunction());
    Handle<JSFunction> fun = Handle<JSFunction>::cast(code);
    Handle<SharedFunctionInfo> shared(fun->shared());

    if (!EnsureCompiled(shared, KEEP_EXCEPTION)) {
      return Failure::Exception();
    }
    // Set the code, scope info, formal parameter count,
    // and the length of the target function.
    target->shared()->set_code(shared->code());
    target->set_code(shared->code());
    target->shared()->set_scope_info(shared->scope_info());
    target->shared()->set_length(shared->length());
    target->shared()->set_formal_parameter_count(
        shared->formal_parameter_count());
    // Set the source code of the target function to undefined.
    // SetCode is only used for built-in constructors like String,
    // Array, and Object, and some web code
    // doesn't like seeing source code for constructors.
    target->shared()->set_script(isolate->heap()->undefined_value());
    // Clear the optimization hints related to the compiled code as these are no
    // longer valid when the code is overwritten.
    target->shared()->ClearThisPropertyAssignmentsInfo();
    context = Handle<Context>(fun->context());

    // Make sure we get a fresh copy of the literal vector to avoid
    // cross context contamination.
    int number_of_literals = fun->NumberOfLiterals();
    Handle<FixedArray> literals =
        isolate->factory()->NewFixedArray(number_of_literals, TENURED);
    if (number_of_literals > 0) {
      // Insert the object, regexp and array functions in the literals
      // array prefix.  These are the functions that will be used when
      // creating object, regexp and array literals.
      literals->set(JSFunction::kLiteralGlobalContextIndex,
                    context->global_context());
    }
    // It's okay to skip the write barrier here because the literals
    // are guaranteed to be in old space.
    target->set_literals(*literals, SKIP_WRITE_BARRIER);
  }

  target->set_context(*context);
  return *target;
}


static Object* Runtime_SetExpectedNumberOfProperties(
    RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_CHECKED(JSFunction, function, 0);
  CONVERT_SMI_CHECKED(num, args[1]);
  RUNTIME_ASSERT(num >= 0);
  SetExpectedNofProperties(function, num);
  return isolate->heap()->undefined_value();
}


static Object* CharFromCode(Isolate* isolate, Object* char_code) {
  uint32_t code;
  if (char_code->ToArrayIndex(&code)) {
    if (code <= 0xffff) {
      return isolate->heap()->LookupSingleCharacterStringFromCode(code);
    }
  }
  return isolate->heap()->empty_string();
}


static Object* Runtime_StringCharCodeAt(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_CHECKED(String, subject, args[0]);
  Object* index = args[1];
  RUNTIME_ASSERT(index->IsNumber());

  uint32_t i = 0;
  if (index->IsSmi()) {
    int value = Smi::cast(index)->value();
    if (value < 0) return isolate->heap()->nan_value();
    i = value;
  } else {
    ASSERT(index->IsHeapNumber());
    double value = HeapNumber::cast(index)->value();
    i = static_cast<uint32_t>(DoubleToInteger(value));
  }

  // Flatten the string.  If someone wants to get a char at an index
  // in a cons string, it is likely that more indices will be
  // accessed.
  Object* flat = subject->TryFlatten();
  if (flat->IsFailure()) return flat;
  subject = String::cast(flat);

  if (i >= static_cast<uint32_t>(subject->length())) {
    return isolate->heap()->nan_value();
  }

  return Smi::FromInt(subject->Get(i));
}


static Object* Runtime_CharFromCode(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);
  return CharFromCode(isolate, args[0]);
}


class FixedArrayBuilder {
 public:
  explicit FixedArrayBuilder(Isolate* isolate, int initial_capacity)
      : array_(isolate->factory()->NewFixedArrayWithHoles(initial_capacity)),
        length_(0) {
    // Require a non-zero initial size. Ensures that doubling the size to
    // extend the array will work.
    ASSERT(initial_capacity > 0);
  }

  explicit FixedArrayBuilder(Handle<FixedArray> backing_store)
      : array_(backing_store),
        length_(0) {
    // Require a non-zero initial size. Ensures that doubling the size to
    // extend the array will work.
    ASSERT(backing_store->length() > 0);
  }

  bool HasCapacity(int elements) {
    int length = array_->length();
    int required_length = length_ + elements;
    return (length >= required_length);
  }

  void EnsureCapacity(int elements) {
    int length = array_->length();
    int required_length = length_ + elements;
    if (length < required_length) {
      int new_length = length;
      do {
        new_length *= 2;
      } while (new_length < required_length);
      Handle<FixedArray> extended_array =
          FACTORY->NewFixedArrayWithHoles(new_length);
      array_->CopyTo(0, *extended_array, 0, length_);
      array_ = extended_array;
    }
  }

  void Add(Object* value) {
    ASSERT(length_ < capacity());
    array_->set(length_, value);
    length_++;
  }

  void Add(Smi* value) {
    ASSERT(length_ < capacity());
    array_->set(length_, value);
    length_++;
  }

  Handle<FixedArray> array() {
    return array_;
  }

  int length() {
    return length_;
  }

  int capacity() {
    return array_->length();
  }

  Handle<JSArray> ToJSArray() {
    Handle<JSArray> result_array = FACTORY->NewJSArrayWithElements(array_);
    result_array->set_length(Smi::FromInt(length_));
    return result_array;
  }

  Handle<JSArray> ToJSArray(Handle<JSArray> target_array) {
    target_array->set_elements(*array_);
    target_array->set_length(Smi::FromInt(length_));
    return target_array;
  }

 private:
  Handle<FixedArray> array_;
  int length_;
};


// Forward declarations.
const int kStringBuilderConcatHelperLengthBits = 11;
const int kStringBuilderConcatHelperPositionBits = 19;

template <typename schar>
static inline void StringBuilderConcatHelper(String*,
                                             schar*,
                                             FixedArray*,
                                             int);

typedef BitField<int, 0, kStringBuilderConcatHelperLengthBits>
    StringBuilderSubstringLength;
typedef BitField<int,
                 kStringBuilderConcatHelperLengthBits,
                 kStringBuilderConcatHelperPositionBits>
    StringBuilderSubstringPosition;


class ReplacementStringBuilder {
 public:
  ReplacementStringBuilder(Heap* heap,
                           Handle<String> subject,
                           int estimated_part_count)
      : heap_(heap),
        array_builder_(heap->isolate(), estimated_part_count),
        subject_(subject),
        character_count_(0),
        is_ascii_(subject->IsAsciiRepresentation()) {
    // Require a non-zero initial size. Ensures that doubling the size to
    // extend the array will work.
    ASSERT(estimated_part_count > 0);
  }

  static inline void AddSubjectSlice(FixedArrayBuilder* builder,
                                     int from,
                                     int to) {
    ASSERT(from >= 0);
    int length = to - from;
    ASSERT(length > 0);
    if (StringBuilderSubstringLength::is_valid(length) &&
        StringBuilderSubstringPosition::is_valid(from)) {
      int encoded_slice = StringBuilderSubstringLength::encode(length) |
          StringBuilderSubstringPosition::encode(from);
      builder->Add(Smi::FromInt(encoded_slice));
    } else {
      // Otherwise encode as two smis.
      builder->Add(Smi::FromInt(-length));
      builder->Add(Smi::FromInt(from));
    }
  }


  void EnsureCapacity(int elements) {
    array_builder_.EnsureCapacity(elements);
  }


  void AddSubjectSlice(int from, int to) {
    AddSubjectSlice(&array_builder_, from, to);
    IncrementCharacterCount(to - from);
  }


  void AddString(Handle<String> string) {
    int length = string->length();
    ASSERT(length > 0);
    AddElement(*string);
    if (!string->IsAsciiRepresentation()) {
      is_ascii_ = false;
    }
    IncrementCharacterCount(length);
  }


  Handle<String> ToString() {
    if (array_builder_.length() == 0) {
      return FACTORY->empty_string();
    }

    Handle<String> joined_string;
    if (is_ascii_) {
      joined_string = NewRawAsciiString(character_count_);
      AssertNoAllocation no_alloc;
      SeqAsciiString* seq = SeqAsciiString::cast(*joined_string);
      char* char_buffer = seq->GetChars();
      StringBuilderConcatHelper(*subject_,
                                char_buffer,
                                *array_builder_.array(),
                                array_builder_.length());
    } else {
      // Non-ASCII.
      joined_string = NewRawTwoByteString(character_count_);
      AssertNoAllocation no_alloc;
      SeqTwoByteString* seq = SeqTwoByteString::cast(*joined_string);
      uc16* char_buffer = seq->GetChars();
      StringBuilderConcatHelper(*subject_,
                                char_buffer,
                                *array_builder_.array(),
                                array_builder_.length());
    }
    return joined_string;
  }


  void IncrementCharacterCount(int by) {
    if (character_count_ > String::kMaxLength - by) {
      V8::FatalProcessOutOfMemory("String.replace result too large.");
    }
    character_count_ += by;
  }

  Handle<JSArray> GetParts() {
    Handle<JSArray> result =
        heap_->isolate()->factory()->NewJSArrayWithElements(
            array_builder_.array());
    result->set_length(Smi::FromInt(array_builder_.length()));
    return result;
  }

 private:
  Handle<String> NewRawAsciiString(int size) {
    CALL_HEAP_FUNCTION(heap_->isolate(),
                       heap_->AllocateRawAsciiString(size), String);
  }


  Handle<String> NewRawTwoByteString(int size) {
    CALL_HEAP_FUNCTION(heap_->isolate(),
                       heap_->AllocateRawTwoByteString(size), String);
  }


  void AddElement(Object* element) {
    ASSERT(element->IsSmi() || element->IsString());
    ASSERT(array_builder_.capacity() > array_builder_.length());
    array_builder_.Add(element);
  }

  Heap* heap_;
  FixedArrayBuilder array_builder_;
  Handle<String> subject_;
  int character_count_;
  bool is_ascii_;
};


class CompiledReplacement {
 public:
  CompiledReplacement()
      : parts_(1), replacement_substrings_(0) {}

  void Compile(Handle<String> replacement,
               int capture_count,
               int subject_length);

  void Apply(ReplacementStringBuilder* builder,
             int match_from,
             int match_to,
             Handle<JSArray> last_match_info);

  // Number of distinct parts of the replacement pattern.
  int parts() {
    return parts_.length();
  }
 private:
  enum PartType {
    SUBJECT_PREFIX = 1,
    SUBJECT_SUFFIX,
    SUBJECT_CAPTURE,
    REPLACEMENT_SUBSTRING,
    REPLACEMENT_STRING,

    NUMBER_OF_PART_TYPES
  };

  struct ReplacementPart {
    static inline ReplacementPart SubjectMatch() {
      return ReplacementPart(SUBJECT_CAPTURE, 0);
    }
    static inline ReplacementPart SubjectCapture(int capture_index) {
      return ReplacementPart(SUBJECT_CAPTURE, capture_index);
    }
    static inline ReplacementPart SubjectPrefix() {
      return ReplacementPart(SUBJECT_PREFIX, 0);
    }
    static inline ReplacementPart SubjectSuffix(int subject_length) {
      return ReplacementPart(SUBJECT_SUFFIX, subject_length);
    }
    static inline ReplacementPart ReplacementString() {
      return ReplacementPart(REPLACEMENT_STRING, 0);
    }
    static inline ReplacementPart ReplacementSubString(int from, int to) {
      ASSERT(from >= 0);
      ASSERT(to > from);
      return ReplacementPart(-from, to);
    }

    // If tag <= 0 then it is the negation of a start index of a substring of
    // the replacement pattern, otherwise it's a value from PartType.
    ReplacementPart(int tag, int data)
        : tag(tag), data(data) {
      // Must be non-positive or a PartType value.
      ASSERT(tag < NUMBER_OF_PART_TYPES);
    }
    // Either a value of PartType or a non-positive number that is
    // the negation of an index into the replacement string.
    int tag;
    // The data value's interpretation depends on the value of tag:
    // tag == SUBJECT_PREFIX ||
    // tag == SUBJECT_SUFFIX:  data is unused.
    // tag == SUBJECT_CAPTURE: data is the number of the capture.
    // tag == REPLACEMENT_SUBSTRING ||
    // tag == REPLACEMENT_STRING:    data is index into array of substrings
    //                               of the replacement string.
    // tag <= 0: Temporary representation of the substring of the replacement
    //           string ranging over -tag .. data.
    //           Is replaced by REPLACEMENT_{SUB,}STRING when we create the
    //           substring objects.
    int data;
  };

  template<typename Char>
  static void ParseReplacementPattern(ZoneList<ReplacementPart>* parts,
                                      Vector<Char> characters,
                                      int capture_count,
                                      int subject_length) {
    int length = characters.length();
    int last = 0;
    for (int i = 0; i < length; i++) {
      Char c = characters[i];
      if (c == '$') {
        int next_index = i + 1;
        if (next_index == length) {  // No next character!
          break;
        }
        Char c2 = characters[next_index];
        switch (c2) {
        case '$':
          if (i > last) {
            // There is a substring before. Include the first "$".
            parts->Add(ReplacementPart::ReplacementSubString(last, next_index));
            last = next_index + 1;  // Continue after the second "$".
          } else {
            // Let the next substring start with the second "$".
            last = next_index;
          }
          i = next_index;
          break;
        case '`':
          if (i > last) {
            parts->Add(ReplacementPart::ReplacementSubString(last, i));
          }
          parts->Add(ReplacementPart::SubjectPrefix());
          i = next_index;
          last = i + 1;
          break;
        case '\'':
          if (i > last) {
            parts->Add(ReplacementPart::ReplacementSubString(last, i));
          }
          parts->Add(ReplacementPart::SubjectSuffix(subject_length));
          i = next_index;
          last = i + 1;
          break;
        case '&':
          if (i > last) {
            parts->Add(ReplacementPart::ReplacementSubString(last, i));
          }
          parts->Add(ReplacementPart::SubjectMatch());
          i = next_index;
          last = i + 1;
          break;
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9': {
          int capture_ref = c2 - '0';
          if (capture_ref > capture_count) {
            i = next_index;
            continue;
          }
          int second_digit_index = next_index + 1;
          if (second_digit_index < length) {
            // Peek ahead to see if we have two digits.
            Char c3 = characters[second_digit_index];
            if ('0' <= c3 && c3 <= '9') {  // Double digits.
              int double_digit_ref = capture_ref * 10 + c3 - '0';
              if (double_digit_ref <= capture_count) {
                next_index = second_digit_index;
                capture_ref = double_digit_ref;
              }
            }
          }
          if (capture_ref > 0) {
            if (i > last) {
              parts->Add(ReplacementPart::ReplacementSubString(last, i));
            }
            ASSERT(capture_ref <= capture_count);
            parts->Add(ReplacementPart::SubjectCapture(capture_ref));
            last = next_index + 1;
          }
          i = next_index;
          break;
        }
        default:
          i = next_index;
          break;
        }
      }
    }
    if (length > last) {
      if (last == 0) {
        parts->Add(ReplacementPart::ReplacementString());
      } else {
        parts->Add(ReplacementPart::ReplacementSubString(last, length));
      }
    }
  }

  ZoneList<ReplacementPart> parts_;
  ZoneList<Handle<String> > replacement_substrings_;
};


void CompiledReplacement::Compile(Handle<String> replacement,
                                  int capture_count,
                                  int subject_length) {
  ASSERT(replacement->IsFlat());
  if (replacement->IsAsciiRepresentation()) {
    AssertNoAllocation no_alloc;
    ParseReplacementPattern(&parts_,
                            replacement->ToAsciiVector(),
                            capture_count,
                            subject_length);
  } else {
    ASSERT(replacement->IsTwoByteRepresentation());
    AssertNoAllocation no_alloc;

    ParseReplacementPattern(&parts_,
                            replacement->ToUC16Vector(),
                            capture_count,
                            subject_length);
  }
  Isolate* isolate = replacement->GetIsolate();
  // Find substrings of replacement string and create them as String objects.
  int substring_index = 0;
  for (int i = 0, n = parts_.length(); i < n; i++) {
    int tag = parts_[i].tag;
    if (tag <= 0) {  // A replacement string slice.
      int from = -tag;
      int to = parts_[i].data;
      replacement_substrings_.Add(
          isolate->factory()->NewSubString(replacement, from, to));
      parts_[i].tag = REPLACEMENT_SUBSTRING;
      parts_[i].data = substring_index;
      substring_index++;
    } else if (tag == REPLACEMENT_STRING) {
      replacement_substrings_.Add(replacement);
      parts_[i].data = substring_index;
      substring_index++;
    }
  }
}


void CompiledReplacement::Apply(ReplacementStringBuilder* builder,
                                int match_from,
                                int match_to,
                                Handle<JSArray> last_match_info) {
  for (int i = 0, n = parts_.length(); i < n; i++) {
    ReplacementPart part = parts_[i];
    switch (part.tag) {
      case SUBJECT_PREFIX:
        if (match_from > 0) builder->AddSubjectSlice(0, match_from);
        break;
      case SUBJECT_SUFFIX: {
        int subject_length = part.data;
        if (match_to < subject_length) {
          builder->AddSubjectSlice(match_to, subject_length);
        }
        break;
      }
      case SUBJECT_CAPTURE: {
        int capture = part.data;
        FixedArray* match_info = FixedArray::cast(last_match_info->elements());
        int from = RegExpImpl::GetCapture(match_info, capture * 2);
        int to = RegExpImpl::GetCapture(match_info, capture * 2 + 1);
        if (from >= 0 && to > from) {
          builder->AddSubjectSlice(from, to);
        }
        break;
      }
      case REPLACEMENT_SUBSTRING:
      case REPLACEMENT_STRING:
        builder->AddString(replacement_substrings_[part.data]);
        break;
      default:
        UNREACHABLE();
    }
  }
}



static Object* StringReplaceRegExpWithString(Isolate* isolate,
                                             String* subject,
                                             JSRegExp* regexp,
                                             String* replacement,
                                             JSArray* last_match_info) {
  ASSERT(subject->IsFlat());
  ASSERT(replacement->IsFlat());

  HandleScope handles(isolate);

  int length = subject->length();
  Handle<String> subject_handle(subject);
  Handle<JSRegExp> regexp_handle(regexp);
  Handle<String> replacement_handle(replacement);
  Handle<JSArray> last_match_info_handle(last_match_info);
  Handle<Object> match = RegExpImpl::Exec(regexp_handle,
                                          subject_handle,
                                          0,
                                          last_match_info_handle);
  if (match.is_null()) {
    return Failure::Exception();
  }
  if (match->IsNull()) {
    return *subject_handle;
  }

  int capture_count = regexp_handle->CaptureCount();

  // CompiledReplacement uses zone allocation.
  CompilationZoneScope zone(DELETE_ON_EXIT);
  CompiledReplacement compiled_replacement;
  compiled_replacement.Compile(replacement_handle,
                               capture_count,
                               length);

  bool is_global = regexp_handle->GetFlags().is_global();

  // Guessing the number of parts that the final result string is built
  // from. Global regexps can match any number of times, so we guess
  // conservatively.
  int expected_parts =
      (compiled_replacement.parts() + 1) * (is_global ? 4 : 1) + 1;
  ReplacementStringBuilder builder(isolate->heap(),
                                   subject_handle,
                                   expected_parts);

  // Index of end of last match.
  int prev = 0;

  // Number of parts added by compiled replacement plus preceeding
  // string and possibly suffix after last match.  It is possible for
  // all components to use two elements when encoded as two smis.
  const int parts_added_per_loop = 2 * (compiled_replacement.parts() + 2);
  bool matched = true;
  do {
    ASSERT(last_match_info_handle->HasFastElements());
    // Increase the capacity of the builder before entering local handle-scope,
    // so its internal buffer can safely allocate a new handle if it grows.
    builder.EnsureCapacity(parts_added_per_loop);

    HandleScope loop_scope(isolate);
    int start, end;
    {
      AssertNoAllocation match_info_array_is_not_in_a_handle;
      FixedArray* match_info_array =
          FixedArray::cast(last_match_info_handle->elements());

      ASSERT_EQ(capture_count * 2 + 2,
                RegExpImpl::GetLastCaptureCount(match_info_array));
      start = RegExpImpl::GetCapture(match_info_array, 0);
      end = RegExpImpl::GetCapture(match_info_array, 1);
    }

    if (prev < start) {
      builder.AddSubjectSlice(prev, start);
    }
    compiled_replacement.Apply(&builder,
                               start,
                               end,
                               last_match_info_handle);
    prev = end;

    // Only continue checking for global regexps.
    if (!is_global) break;

    // Continue from where the match ended, unless it was an empty match.
    int next = end;
    if (start == end) {
      next = end + 1;
      if (next > length) break;
    }

    match = RegExpImpl::Exec(regexp_handle,
                             subject_handle,
                             next,
                             last_match_info_handle);
    if (match.is_null()) {
      return Failure::Exception();
    }
    matched = !match->IsNull();
  } while (matched);

  if (prev < length) {
    builder.AddSubjectSlice(prev, length);
  }

  return *(builder.ToString());
}


template <typename ResultSeqString>
static Object* StringReplaceRegExpWithEmptyString(Isolate* isolate,
                                                  String* subject,
                                                  JSRegExp* regexp,
                                                  JSArray* last_match_info) {
  ASSERT(subject->IsFlat());

  HandleScope handles(isolate);

  Handle<String> subject_handle(subject);
  Handle<JSRegExp> regexp_handle(regexp);
  Handle<JSArray> last_match_info_handle(last_match_info);
  Handle<Object> match = RegExpImpl::Exec(regexp_handle,
                                          subject_handle,
                                          0,
                                          last_match_info_handle);
  if (match.is_null()) return Failure::Exception();
  if (match->IsNull()) return *subject_handle;

  ASSERT(last_match_info_handle->HasFastElements());

  int start, end;
  {
    AssertNoAllocation match_info_array_is_not_in_a_handle;
    FixedArray* match_info_array =
        FixedArray::cast(last_match_info_handle->elements());

    start = RegExpImpl::GetCapture(match_info_array, 0);
    end = RegExpImpl::GetCapture(match_info_array, 1);
  }

  int length = subject->length();
  int new_length = length - (end - start);
  if (new_length == 0) {
    return isolate->heap()->empty_string();
  }
  Handle<ResultSeqString> answer;
  if (ResultSeqString::kHasAsciiEncoding) {
    answer =
        Handle<ResultSeqString>::cast(
            isolate->factory()->NewRawAsciiString(new_length));
  } else {
    answer =
        Handle<ResultSeqString>::cast(
            isolate->factory()->NewRawTwoByteString(new_length));
  }

  // If the regexp isn't global, only match once.
  if (!regexp_handle->GetFlags().is_global()) {
    if (start > 0) {
      String::WriteToFlat(*subject_handle,
                          answer->GetChars(),
                          0,
                          start);
    }
    if (end < length) {
      String::WriteToFlat(*subject_handle,
                          answer->GetChars() + start,
                          end,
                          length);
    }
    return *answer;
  }

  int prev = 0;  // Index of end of last match.
  int next = 0;  // Start of next search (prev unless last match was empty).
  int position = 0;

  do {
    if (prev < start) {
      // Add substring subject[prev;start] to answer string.
      String::WriteToFlat(*subject_handle,
                          answer->GetChars() + position,
                          prev,
                          start);
      position += start - prev;
    }
    prev = end;
    next = end;
    // Continue from where the match ended, unless it was an empty match.
    if (start == end) {
      next++;
      if (next > length) break;
    }
    match = RegExpImpl::Exec(regexp_handle,
                             subject_handle,
                             next,
                             last_match_info_handle);
    if (match.is_null()) return Failure::Exception();
    if (match->IsNull()) break;

    ASSERT(last_match_info_handle->HasFastElements());
    HandleScope loop_scope(isolate);
    {
      AssertNoAllocation match_info_array_is_not_in_a_handle;
      FixedArray* match_info_array =
          FixedArray::cast(last_match_info_handle->elements());
      start = RegExpImpl::GetCapture(match_info_array, 0);
      end = RegExpImpl::GetCapture(match_info_array, 1);
    }
  } while (true);

  if (prev < length) {
    // Add substring subject[prev;length] to answer string.
    String::WriteToFlat(*subject_handle,
                        answer->GetChars() + position,
                        prev,
                        length);
    position += length - prev;
  }

  if (position == 0) {
    return isolate->heap()->empty_string();
  }

  // Shorten string and fill
  int string_size = ResultSeqString::SizeFor(position);
  int allocated_string_size = ResultSeqString::SizeFor(new_length);
  int delta = allocated_string_size - string_size;

  answer->set_length(position);
  if (delta == 0) return *answer;

  Address end_of_string = answer->address() + string_size;
  isolate->heap()->CreateFillerObjectAt(end_of_string, delta);

  return *answer;
}


static Object* Runtime_StringReplaceRegExpWithString(
    RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 4);

  CONVERT_CHECKED(String, subject, args[0]);
  if (!subject->IsFlat()) {
    Object* flat_subject = subject->TryFlatten();
    if (flat_subject->IsFailure()) {
      return flat_subject;
    }
    subject = String::cast(flat_subject);
  }

  CONVERT_CHECKED(String, replacement, args[2]);
  if (!replacement->IsFlat()) {
    Object* flat_replacement = replacement->TryFlatten();
    if (flat_replacement->IsFailure()) {
      return flat_replacement;
    }
    replacement = String::cast(flat_replacement);
  }

  CONVERT_CHECKED(JSRegExp, regexp, args[1]);
  CONVERT_CHECKED(JSArray, last_match_info, args[3]);

  ASSERT(last_match_info->HasFastElements());

  if (replacement->length() == 0) {
    if (subject->HasOnlyAsciiChars()) {
      return StringReplaceRegExpWithEmptyString<SeqAsciiString>(
          isolate, subject, regexp, last_match_info);
    } else {
      return StringReplaceRegExpWithEmptyString<SeqTwoByteString>(
          isolate, subject, regexp, last_match_info);
    }
  }

  return StringReplaceRegExpWithString(isolate,
                                       subject,
                                       regexp,
                                       replacement,
                                       last_match_info);
}


// Perform string match of pattern on subject, starting at start index.
// Caller must ensure that 0 <= start_index <= sub->length(),
// and should check that pat->length() + start_index <= sub->length()
int Runtime::StringMatch(Isolate* isolate,
                         Handle<String> sub,
                         Handle<String> pat,
                         int start_index) {
  ASSERT(0 <= start_index);
  ASSERT(start_index <= sub->length());

  int pattern_length = pat->length();
  if (pattern_length == 0) return start_index;

  int subject_length = sub->length();
  if (start_index + pattern_length > subject_length) return -1;

  if (!sub->IsFlat()) FlattenString(sub);
  if (!pat->IsFlat()) FlattenString(pat);

  AssertNoAllocation no_heap_allocation;  // ensure vectors stay valid
  // Extract flattened substrings of cons strings before determining asciiness.
  String* seq_sub = *sub;
  if (seq_sub->IsConsString()) seq_sub = ConsString::cast(seq_sub)->first();
  String* seq_pat = *pat;
  if (seq_pat->IsConsString()) seq_pat = ConsString::cast(seq_pat)->first();

  // dispatch on type of strings
  if (seq_pat->IsAsciiRepresentation()) {
    Vector<const char> pat_vector = seq_pat->ToAsciiVector();
    if (seq_sub->IsAsciiRepresentation()) {
      return SearchString(isolate,
                          seq_sub->ToAsciiVector(),
                          pat_vector,
                          start_index);
    }
    return SearchString(isolate,
                        seq_sub->ToUC16Vector(),
                        pat_vector,
                        start_index);
  }
  Vector<const uc16> pat_vector = seq_pat->ToUC16Vector();
  if (seq_sub->IsAsciiRepresentation()) {
    return SearchString(isolate,
                        seq_sub->ToAsciiVector(),
                        pat_vector,
                        start_index);
  }
  return SearchString(isolate,
                      seq_sub->ToUC16Vector(),
                      pat_vector,
                      start_index);
}


static Object* Runtime_StringIndexOf(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);  // create a new handle scope
  ASSERT(args.length() == 3);

  CONVERT_ARG_CHECKED(String, sub, 0);
  CONVERT_ARG_CHECKED(String, pat, 1);

  Object* index = args[2];
  uint32_t start_index;
  if (!index->ToArrayIndex(&start_index)) return Smi::FromInt(-1);

  RUNTIME_ASSERT(start_index <= static_cast<uint32_t>(sub->length()));
  int position =
      Runtime::StringMatch(isolate, sub, pat, start_index);
  return Smi::FromInt(position);
}


template <typename schar, typename pchar>
static int StringMatchBackwards(Vector<const schar> subject,
                                Vector<const pchar> pattern,
                                int idx) {
  int pattern_length = pattern.length();
  ASSERT(pattern_length >= 1);
  ASSERT(idx + pattern_length <= subject.length());

  if (sizeof(schar) == 1 && sizeof(pchar) > 1) {
    for (int i = 0; i < pattern_length; i++) {
      uc16 c = pattern[i];
      if (c > String::kMaxAsciiCharCode) {
        return -1;
      }
    }
  }

  pchar pattern_first_char = pattern[0];
  for (int i = idx; i >= 0; i--) {
    if (subject[i] != pattern_first_char) continue;
    int j = 1;
    while (j < pattern_length) {
      if (pattern[j] != subject[i+j]) {
        break;
      }
      j++;
    }
    if (j == pattern_length) {
      return i;
    }
  }
  return -1;
}

static Object* Runtime_StringLastIndexOf(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);  // create a new handle scope
  ASSERT(args.length() == 3);

  CONVERT_ARG_CHECKED(String, sub, 0);
  CONVERT_ARG_CHECKED(String, pat, 1);

  Object* index = args[2];
  uint32_t start_index;
  if (!index->ToArrayIndex(&start_index)) return Smi::FromInt(-1);

  uint32_t pat_length = pat->length();
  uint32_t sub_length = sub->length();

  if (start_index + pat_length > sub_length) {
    start_index = sub_length - pat_length;
  }

  if (pat_length == 0) {
    return Smi::FromInt(start_index);
  }

  if (!sub->IsFlat()) FlattenString(sub);
  if (!pat->IsFlat()) FlattenString(pat);

  AssertNoAllocation no_heap_allocation;  // ensure vectors stay valid

  int position = -1;

  if (pat->IsAsciiRepresentation()) {
    Vector<const char> pat_vector = pat->ToAsciiVector();
    if (sub->IsAsciiRepresentation()) {
      position = StringMatchBackwards(sub->ToAsciiVector(),
                                      pat_vector,
                                      start_index);
    } else {
      position = StringMatchBackwards(sub->ToUC16Vector(),
                                      pat_vector,
                                      start_index);
    }
  } else {
    Vector<const uc16> pat_vector = pat->ToUC16Vector();
    if (sub->IsAsciiRepresentation()) {
      position = StringMatchBackwards(sub->ToAsciiVector(),
                                      pat_vector,
                                      start_index);
    } else {
      position = StringMatchBackwards(sub->ToUC16Vector(),
                                      pat_vector,
                                      start_index);
    }
  }

  return Smi::FromInt(position);
}


static Object* Runtime_StringLocaleCompare(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_CHECKED(String, str1, args[0]);
  CONVERT_CHECKED(String, str2, args[1]);

  if (str1 == str2) return Smi::FromInt(0);  // Equal.
  int str1_length = str1->length();
  int str2_length = str2->length();

  // Decide trivial cases without flattening.
  if (str1_length == 0) {
    if (str2_length == 0) return Smi::FromInt(0);  // Equal.
    return Smi::FromInt(-str2_length);
  } else {
    if (str2_length == 0) return Smi::FromInt(str1_length);
  }

  int end = str1_length < str2_length ? str1_length : str2_length;

  // No need to flatten if we are going to find the answer on the first
  // character.  At this point we know there is at least one character
  // in each string, due to the trivial case handling above.
  int d = str1->Get(0) - str2->Get(0);
  if (d != 0) return Smi::FromInt(d);

  str1->TryFlatten();
  str2->TryFlatten();

  StringInputBuffer& buf1 =
      *isolate->runtime_state()->string_locale_compare_buf1();
  StringInputBuffer& buf2 =
      *isolate->runtime_state()->string_locale_compare_buf2();

  buf1.Reset(str1);
  buf2.Reset(str2);

  for (int i = 0; i < end; i++) {
    uint16_t char1 = buf1.GetNext();
    uint16_t char2 = buf2.GetNext();
    if (char1 != char2) return Smi::FromInt(char1 - char2);
  }

  return Smi::FromInt(str1_length - str2_length);
}


static Object* Runtime_SubString(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 3);

  CONVERT_CHECKED(String, value, args[0]);
  Object* from = args[1];
  Object* to = args[2];
  int start, end;
  // We have a fast integer-only case here to avoid a conversion to double in
  // the common case where from and to are Smis.
  if (from->IsSmi() && to->IsSmi()) {
    start = Smi::cast(from)->value();
    end = Smi::cast(to)->value();
  } else {
    CONVERT_DOUBLE_CHECKED(from_number, from);
    CONVERT_DOUBLE_CHECKED(to_number, to);
    start = FastD2I(from_number);
    end = FastD2I(to_number);
  }
  RUNTIME_ASSERT(end >= start);
  RUNTIME_ASSERT(start >= 0);
  RUNTIME_ASSERT(end <= value->length());
  isolate->counters()->sub_string_runtime()->Increment();
  return value->SubString(start, end);
}


static Object* Runtime_StringMatch(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT_EQ(3, args.length());

  CONVERT_ARG_CHECKED(String, subject, 0);
  CONVERT_ARG_CHECKED(JSRegExp, regexp, 1);
  CONVERT_ARG_CHECKED(JSArray, regexp_info, 2);
  HandleScope handles;

  Handle<Object> match = RegExpImpl::Exec(regexp, subject, 0, regexp_info);

  if (match.is_null()) {
    return Failure::Exception();
  }
  if (match->IsNull()) {
    return isolate->heap()->null_value();
  }
  int length = subject->length();

  CompilationZoneScope zone_space(DELETE_ON_EXIT);
  ZoneList<int> offsets(8);
  do {
    int start;
    int end;
    {
      AssertNoAllocation no_alloc;
      FixedArray* elements = FixedArray::cast(regexp_info->elements());
      start = Smi::cast(elements->get(RegExpImpl::kFirstCapture))->value();
      end = Smi::cast(elements->get(RegExpImpl::kFirstCapture + 1))->value();
    }
    offsets.Add(start);
    offsets.Add(end);
    int index = start < end ? end : end + 1;
    if (index > length) break;
    match = RegExpImpl::Exec(regexp, subject, index, regexp_info);
    if (match.is_null()) {
      return Failure::Exception();
    }
  } while (!match->IsNull());
  int matches = offsets.length() / 2;
  Handle<FixedArray> elements = isolate->factory()->NewFixedArray(matches);
  for (int i = 0; i < matches ; i++) {
    int from = offsets.at(i * 2);
    int to = offsets.at(i * 2 + 1);
    Handle<String> match = isolate->factory()->NewSubString(subject, from, to);
    elements->set(i, *match);
  }
  Handle<JSArray> result = isolate->factory()->NewJSArrayWithElements(elements);
  result->set_length(Smi::FromInt(matches));
  return *result;
}


// Two smis before and after the match, for very long strings.
const int kMaxBuilderEntriesPerRegExpMatch = 5;


static void SetLastMatchInfoNoCaptures(Handle<String> subject,
                                       Handle<JSArray> last_match_info,
                                       int match_start,
                                       int match_end) {
  // Fill last_match_info with a single capture.
  last_match_info->EnsureSize(2 + RegExpImpl::kLastMatchOverhead);
  AssertNoAllocation no_gc;
  FixedArray* elements = FixedArray::cast(last_match_info->elements());
  RegExpImpl::SetLastCaptureCount(elements, 2);
  RegExpImpl::SetLastInput(elements, *subject);
  RegExpImpl::SetLastSubject(elements, *subject);
  RegExpImpl::SetCapture(elements, 0, match_start);
  RegExpImpl::SetCapture(elements, 1, match_end);
}


template <typename SubjectChar, typename PatternChar>
static bool SearchStringMultiple(Isolate* isolate,
                                 Vector<const SubjectChar> subject,
                                 Vector<const PatternChar> pattern,
                                 String* pattern_string,
                                 FixedArrayBuilder* builder,
                                 int* match_pos) {
  int pos = *match_pos;
  int subject_length = subject.length();
  int pattern_length = pattern.length();
  int max_search_start = subject_length - pattern_length;
  StringSearch<PatternChar, SubjectChar> search(isolate, pattern);
  while (pos <= max_search_start) {
    if (!builder->HasCapacity(kMaxBuilderEntriesPerRegExpMatch)) {
      *match_pos = pos;
      return false;
    }
    // Position of end of previous match.
    int match_end = pos + pattern_length;
    int new_pos = search.Search(subject, match_end);
    if (new_pos >= 0) {
      // A match.
      if (new_pos > match_end) {
        ReplacementStringBuilder::AddSubjectSlice(builder,
            match_end,
            new_pos);
      }
      pos = new_pos;
      builder->Add(pattern_string);
    } else {
      break;
    }
  }

  if (pos < max_search_start) {
    ReplacementStringBuilder::AddSubjectSlice(builder,
                                              pos + pattern_length,
                                              subject_length);
  }
  *match_pos = pos;
  return true;
}


static bool SearchStringMultiple(Isolate* isolate,
                                 Handle<String> subject,
                                 Handle<String> pattern,
                                 Handle<JSArray> last_match_info,
                                 FixedArrayBuilder* builder) {
  ASSERT(subject->IsFlat());
  ASSERT(pattern->IsFlat());

  // Treating as if a previous match was before first character.
  int match_pos = -pattern->length();

  for (;;) {  // Break when search complete.
    builder->EnsureCapacity(kMaxBuilderEntriesPerRegExpMatch);
    AssertNoAllocation no_gc;
    if (subject->IsAsciiRepresentation()) {
      Vector<const char> subject_vector = subject->ToAsciiVector();
      if (pattern->IsAsciiRepresentation()) {
        if (SearchStringMultiple(isolate,
                                 subject_vector,
                                 pattern->ToAsciiVector(),
                                 *pattern,
                                 builder,
                                 &match_pos)) break;
      } else {
        if (SearchStringMultiple(isolate,
                                 subject_vector,
                                 pattern->ToUC16Vector(),
                                 *pattern,
                                 builder,
                                 &match_pos)) break;
      }
    } else {
      Vector<const uc16> subject_vector = subject->ToUC16Vector();
      if (pattern->IsAsciiRepresentation()) {
        if (SearchStringMultiple(isolate,
                                 subject_vector,
                                 pattern->ToAsciiVector(),
                                 *pattern,
                                 builder,
                                 &match_pos)) break;
      } else {
        if (SearchStringMultiple(isolate,
                                 subject_vector,
                                 pattern->ToUC16Vector(),
                                 *pattern,
                                 builder,
                                 &match_pos)) break;
      }
    }
  }

  if (match_pos >= 0) {
    SetLastMatchInfoNoCaptures(subject,
                               last_match_info,
                               match_pos,
                               match_pos + pattern->length());
    return true;
  }
  return false;  // No matches at all.
}


static RegExpImpl::IrregexpResult SearchRegExpNoCaptureMultiple(
    Isolate* isolate,
    Handle<String> subject,
    Handle<JSRegExp> regexp,
    Handle<JSArray> last_match_array,
    FixedArrayBuilder* builder) {
  ASSERT(subject->IsFlat());
  int match_start = -1;
  int match_end = 0;
  int pos = 0;
  int required_registers = RegExpImpl::IrregexpPrepare(regexp, subject);
  if (required_registers < 0) return RegExpImpl::RE_EXCEPTION;

  OffsetsVector registers(required_registers);
  Vector<int32_t> register_vector(registers.vector(), registers.length());
  int subject_length = subject->length();

  for (;;) {  // Break on failure, return on exception.
    RegExpImpl::IrregexpResult result =
        RegExpImpl::IrregexpExecOnce(regexp,
                                     subject,
                                     pos,
                                     register_vector);
    if (result == RegExpImpl::RE_SUCCESS) {
      match_start = register_vector[0];
      builder->EnsureCapacity(kMaxBuilderEntriesPerRegExpMatch);
      if (match_end < match_start) {
        ReplacementStringBuilder::AddSubjectSlice(builder,
                                                  match_end,
                                                  match_start);
      }
      match_end = register_vector[1];
      HandleScope loop_scope(isolate);
      builder->Add(*isolate->factory()->NewSubString(subject,
                                                     match_start,
                                                     match_end));
      if (match_start != match_end) {
        pos = match_end;
      } else {
        pos = match_end + 1;
        if (pos > subject_length) break;
      }
    } else if (result == RegExpImpl::RE_FAILURE) {
      break;
    } else {
      ASSERT_EQ(result, RegExpImpl::RE_EXCEPTION);
      return result;
    }
  }

  if (match_start >= 0) {
    if (match_end < subject_length) {
      ReplacementStringBuilder::AddSubjectSlice(builder,
                                                match_end,
                                                subject_length);
    }
    SetLastMatchInfoNoCaptures(subject,
                               last_match_array,
                               match_start,
                               match_end);
    return RegExpImpl::RE_SUCCESS;
  } else {
    return RegExpImpl::RE_FAILURE;  // No matches at all.
  }
}


static RegExpImpl::IrregexpResult SearchRegExpMultiple(
    Isolate* isolate,
    Handle<String> subject,
    Handle<JSRegExp> regexp,
    Handle<JSArray> last_match_array,
    FixedArrayBuilder* builder) {

  ASSERT(subject->IsFlat());
  int required_registers = RegExpImpl::IrregexpPrepare(regexp, subject);
  if (required_registers < 0) return RegExpImpl::RE_EXCEPTION;

  OffsetsVector registers(required_registers);
  Vector<int32_t> register_vector(registers.vector(), registers.length());

  RegExpImpl::IrregexpResult result =
      RegExpImpl::IrregexpExecOnce(regexp,
                                   subject,
                                   0,
                                   register_vector);

  int capture_count = regexp->CaptureCount();
  int subject_length = subject->length();

  // Position to search from.
  int pos = 0;
  // End of previous match. Differs from pos if match was empty.
  int match_end = 0;
  if (result == RegExpImpl::RE_SUCCESS) {
    // Need to keep a copy of the previous match for creating last_match_info
    // at the end, so we have two vectors that we swap between.
    OffsetsVector registers2(required_registers);
    Vector<int> prev_register_vector(registers2.vector(), registers2.length());

    do {
      int match_start = register_vector[0];
      builder->EnsureCapacity(kMaxBuilderEntriesPerRegExpMatch);
      if (match_end < match_start) {
        ReplacementStringBuilder::AddSubjectSlice(builder,
                                                  match_end,
                                                  match_start);
      }
      match_end = register_vector[1];

      {
        // Avoid accumulating new handles inside loop.
        HandleScope temp_scope(isolate);
        // Arguments array to replace function is match, captures, index and
        // subject, i.e., 3 + capture count in total.
        Handle<FixedArray> elements =
            isolate->factory()->NewFixedArray(3 + capture_count);
        Handle<String> match = isolate->factory()->NewSubString(subject,
                                                                match_start,
                                                                match_end);
        elements->set(0, *match);
        for (int i = 1; i <= capture_count; i++) {
          int start = register_vector[i * 2];
          if (start >= 0) {
            int end = register_vector[i * 2 + 1];
            ASSERT(start <= end);
            Handle<String> substring = isolate->factory()->NewSubString(subject,
                                                                        start,
                                                                        end);
            elements->set(i, *substring);
          } else {
            ASSERT(register_vector[i * 2 + 1] < 0);
            elements->set(i, isolate->heap()->undefined_value());
          }
        }
        elements->set(capture_count + 1, Smi::FromInt(match_start));
        elements->set(capture_count + 2, *subject);
        builder->Add(*isolate->factory()->NewJSArrayWithElements(elements));
      }
      // Swap register vectors, so the last successful match is in
      // prev_register_vector.
      Vector<int32_t> tmp = prev_register_vector;
      prev_register_vector = register_vector;
      register_vector = tmp;

      if (match_end > match_start) {
        pos = match_end;
      } else {
        pos = match_end + 1;
        if (pos > subject_length) {
          break;
        }
      }

      result = RegExpImpl::IrregexpExecOnce(regexp,
                                            subject,
                                            pos,
                                            register_vector);
    } while (result == RegExpImpl::RE_SUCCESS);

    if (result != RegExpImpl::RE_EXCEPTION) {
      // Finished matching, with at least one match.
      if (match_end < subject_length) {
        ReplacementStringBuilder::AddSubjectSlice(builder,
                                                  match_end,
                                                  subject_length);
      }

      int last_match_capture_count = (capture_count + 1) * 2;
      int last_match_array_size =
          last_match_capture_count + RegExpImpl::kLastMatchOverhead;
      last_match_array->EnsureSize(last_match_array_size);
      AssertNoAllocation no_gc;
      FixedArray* elements = FixedArray::cast(last_match_array->elements());
      RegExpImpl::SetLastCaptureCount(elements, last_match_capture_count);
      RegExpImpl::SetLastSubject(elements, *subject);
      RegExpImpl::SetLastInput(elements, *subject);
      for (int i = 0; i < last_match_capture_count; i++) {
        RegExpImpl::SetCapture(elements, i, prev_register_vector[i]);
      }
      return RegExpImpl::RE_SUCCESS;
    }
  }
  // No matches at all, return failure or exception result directly.
  return result;
}


static Object* Runtime_RegExpExecMultiple(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 4);
  HandleScope handles;

  CONVERT_ARG_CHECKED(String, subject, 1);
  if (!subject->IsFlat()) { FlattenString(subject); }
  CONVERT_ARG_CHECKED(JSRegExp, regexp, 0);
  CONVERT_ARG_CHECKED(JSArray, last_match_info, 2);
  CONVERT_ARG_CHECKED(JSArray, result_array, 3);

  ASSERT(last_match_info->HasFastElements());
  ASSERT(regexp->GetFlags().is_global());
  Handle<FixedArray> result_elements;
  if (result_array->HasFastElements()) {
    result_elements =
        Handle<FixedArray>(FixedArray::cast(result_array->elements()));
  } else {
    result_elements = isolate->factory()->NewFixedArrayWithHoles(16);
  }
  FixedArrayBuilder builder(result_elements);

  if (regexp->TypeTag() == JSRegExp::ATOM) {
    Handle<String> pattern(
        String::cast(regexp->DataAt(JSRegExp::kAtomPatternIndex)));
    if (!pattern->IsFlat()) FlattenString(pattern);
    if (SearchStringMultiple(isolate, subject, pattern,
                             last_match_info, &builder)) {
      return *builder.ToJSArray(result_array);
    }
    return isolate->heap()->null_value();
  }

  ASSERT_EQ(regexp->TypeTag(), JSRegExp::IRREGEXP);

  RegExpImpl::IrregexpResult result;
  if (regexp->CaptureCount() == 0) {
    result = SearchRegExpNoCaptureMultiple(isolate,
                                           subject,
                                           regexp,
                                           last_match_info,
                                           &builder);
  } else {
    result = SearchRegExpMultiple(isolate,
                                  subject,
                                  regexp,
                                  last_match_info,
                                  &builder);
  }
  if (result == RegExpImpl::RE_SUCCESS) return *builder.ToJSArray(result_array);
  if (result == RegExpImpl::RE_FAILURE) return isolate->heap()->null_value();
  ASSERT_EQ(result, RegExpImpl::RE_EXCEPTION);
  return Failure::Exception();
}


static Object* Runtime_NumberToRadixString(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  // Fast case where the result is a one character string.
  if (args[0]->IsSmi() && args[1]->IsSmi()) {
    int value = Smi::cast(args[0])->value();
    int radix = Smi::cast(args[1])->value();
    if (value >= 0 && value < radix) {
      RUNTIME_ASSERT(radix <= 36);
      // Character array used for conversion.
      static const char kCharTable[] = "0123456789abcdefghijklmnopqrstuvwxyz";
      return isolate->heap()->
          LookupSingleCharacterStringFromCode(kCharTable[value]);
    }
  }

  // Slow case.
  CONVERT_DOUBLE_CHECKED(value, args[0]);
  if (isnan(value)) {
    return isolate->heap()->AllocateStringFromAscii(CStrVector("NaN"));
  }
  if (isinf(value)) {
    if (value < 0) {
      return isolate->heap()->AllocateStringFromAscii(CStrVector("-Infinity"));
    }
    return isolate->heap()->AllocateStringFromAscii(CStrVector("Infinity"));
  }
  CONVERT_DOUBLE_CHECKED(radix_number, args[1]);
  int radix = FastD2I(radix_number);
  RUNTIME_ASSERT(2 <= radix && radix <= 36);
  char* str = DoubleToRadixCString(value, radix);
  Object* result = isolate->heap()->AllocateStringFromAscii(CStrVector(str));
  DeleteArray(str);
  return result;
}


static Object* Runtime_NumberToFixed(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_DOUBLE_CHECKED(value, args[0]);
  if (isnan(value)) {
    return isolate->heap()->AllocateStringFromAscii(CStrVector("NaN"));
  }
  if (isinf(value)) {
    if (value < 0) {
      return isolate->heap()->AllocateStringFromAscii(CStrVector("-Infinity"));
    }
    return isolate->heap()->AllocateStringFromAscii(CStrVector("Infinity"));
  }
  CONVERT_DOUBLE_CHECKED(f_number, args[1]);
  int f = FastD2I(f_number);
  RUNTIME_ASSERT(f >= 0);
  char* str = DoubleToFixedCString(value, f);
  Object* res = isolate->heap()->AllocateStringFromAscii(CStrVector(str));
  DeleteArray(str);
  return res;
}


static Object* Runtime_NumberToExponential(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_DOUBLE_CHECKED(value, args[0]);
  if (isnan(value)) {
    return isolate->heap()->AllocateStringFromAscii(CStrVector("NaN"));
  }
  if (isinf(value)) {
    if (value < 0) {
      return isolate->heap()->AllocateStringFromAscii(CStrVector("-Infinity"));
    }
    return isolate->heap()->AllocateStringFromAscii(CStrVector("Infinity"));
  }
  CONVERT_DOUBLE_CHECKED(f_number, args[1]);
  int f = FastD2I(f_number);
  RUNTIME_ASSERT(f >= -1 && f <= 20);
  char* str = DoubleToExponentialCString(value, f);
  Object* res = isolate->heap()->AllocateStringFromAscii(CStrVector(str));
  DeleteArray(str);
  return res;
}


static Object* Runtime_NumberToPrecision(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_DOUBLE_CHECKED(value, args[0]);
  if (isnan(value)) {
    return isolate->heap()->AllocateStringFromAscii(CStrVector("NaN"));
  }
  if (isinf(value)) {
    if (value < 0) {
      return isolate->heap()->AllocateStringFromAscii(CStrVector("-Infinity"));
    }
    return isolate->heap()->AllocateStringFromAscii(CStrVector("Infinity"));
  }
  CONVERT_DOUBLE_CHECKED(f_number, args[1]);
  int f = FastD2I(f_number);
  RUNTIME_ASSERT(f >= 1 && f <= 21);
  char* str = DoubleToPrecisionCString(value, f);
  Object* res = isolate->heap()->AllocateStringFromAscii(CStrVector(str));
  DeleteArray(str);
  return res;
}


// Returns a single character string where first character equals
// string->Get(index).
static Handle<Object> GetCharAt(Handle<String> string, uint32_t index) {
  if (index < static_cast<uint32_t>(string->length())) {
    string->TryFlatten();
    return LookupSingleCharacterStringFromCode(
        string->Get(index));
  }
  return Execution::CharAt(string, index);
}


Object* Runtime::GetElementOrCharAt(Isolate* isolate,
                                    Handle<Object> object,
                                    uint32_t index) {
  // Handle [] indexing on Strings
  if (object->IsString()) {
    Handle<Object> result = GetCharAt(Handle<String>::cast(object), index);
    if (!result->IsUndefined()) return *result;
  }

  // Handle [] indexing on String objects
  if (object->IsStringObjectWithCharacterAt(index)) {
    Handle<JSValue> js_value = Handle<JSValue>::cast(object);
    Handle<Object> result =
        GetCharAt(Handle<String>(String::cast(js_value->value())), index);
    if (!result->IsUndefined()) return *result;
  }

  if (object->IsString() || object->IsNumber() || object->IsBoolean()) {
    Handle<Object> prototype = GetPrototype(object);
    return prototype->GetElement(index);
  }

  return GetElement(object, index);
}


Object* Runtime::GetElement(Handle<Object> object, uint32_t index) {
  return object->GetElement(index);
}


Object* Runtime::GetObjectProperty(Isolate* isolate,
                                   Handle<Object> object,
                                   Handle<Object> key) {
  HandleScope scope(isolate);

  if (object->IsUndefined() || object->IsNull()) {
    Handle<Object> args[2] = { key, object };
    Handle<Object> error =
        isolate->factory()->NewTypeError("non_object_property_load",
                                         HandleVector(args, 2));
    return isolate->Throw(*error);
  }

  // Check if the given key is an array index.
  uint32_t index;
  if (key->ToArrayIndex(&index)) {
    return GetElementOrCharAt(isolate, object, index);
  }

  // Convert the key to a string - possibly by calling back into JavaScript.
  Handle<String> name;
  if (key->IsString()) {
    name = Handle<String>::cast(key);
  } else {
    bool has_pending_exception = false;
    Handle<Object> converted =
        Execution::ToString(key, &has_pending_exception);
    if (has_pending_exception) return Failure::Exception();
    name = Handle<String>::cast(converted);
  }

  // Check if the name is trivially convertible to an index and get
  // the element if so.
  if (name->AsArrayIndex(&index)) {
    return GetElementOrCharAt(isolate, object, index);
  } else {
    PropertyAttributes attr;
    return object->GetProperty(*name, &attr);
  }
}


static Object* Runtime_GetProperty(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  Handle<Object> object = args.at<Object>(0);
  Handle<Object> key = args.at<Object>(1);

  return Runtime::GetObjectProperty(isolate, object, key);
}


// KeyedStringGetProperty is called from KeyedLoadIC::GenerateGeneric.
static Object* Runtime_KeyedGetProperty(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  // Fast cases for getting named properties of the receiver JSObject
  // itself.
  //
  // The global proxy objects has to be excluded since LocalLookup on
  // the global proxy object can return a valid result even though the
  // global proxy object never has properties.  This is the case
  // because the global proxy object forwards everything to its hidden
  // prototype including local lookups.
  //
  // Additionally, we need to make sure that we do not cache results
  // for objects that require access checks.
  if (args[0]->IsJSObject() &&
      !args[0]->IsJSGlobalProxy() &&
      !args[0]->IsAccessCheckNeeded() &&
      args[1]->IsString()) {
    JSObject* receiver = JSObject::cast(args[0]);
    String* key = String::cast(args[1]);
    if (receiver->HasFastProperties()) {
      // Attempt to use lookup cache.
      Map* receiver_map = receiver->map();
      KeyedLookupCache* keyed_lookup_cache = isolate->keyed_lookup_cache();
      int offset = keyed_lookup_cache->Lookup(receiver_map, key);
      if (offset != -1) {
        Object* value = receiver->FastPropertyAt(offset);
        return value->IsTheHole() ? isolate->heap()->undefined_value() : value;
      }
      // Lookup cache miss.  Perform lookup and update the cache if appropriate.
      LookupResult result;
      receiver->LocalLookup(key, &result);
      if (result.IsProperty() && result.type() == FIELD) {
        int offset = result.GetFieldIndex();
        keyed_lookup_cache->Update(receiver_map, key, offset);
        return receiver->FastPropertyAt(offset);
      }
    } else {
      // Attempt dictionary lookup.
      StringDictionary* dictionary = receiver->property_dictionary();
      int entry = dictionary->FindEntry(key);
      if ((entry != StringDictionary::kNotFound) &&
          (dictionary->DetailsAt(entry).type() == NORMAL)) {
        Object* value = dictionary->ValueAt(entry);
        if (!receiver->IsGlobalObject()) return value;
        value = JSGlobalPropertyCell::cast(value)->value();
        if (!value->IsTheHole()) return value;
        // If value is the hole do the general lookup.
      }
    }
  } else if (args[0]->IsString() && args[1]->IsSmi()) {
    // Fast case for string indexing using [] with a smi index.
    HandleScope scope(isolate);
    Handle<String> str = args.at<String>(0);
    int index = Smi::cast(args[1])->value();
    Handle<Object> result = GetCharAt(str, index);
    return *result;
  }

  // Fall back to GetObjectProperty.
  return Runtime::GetObjectProperty(isolate,
                                    args.at<Object>(0),
                                    args.at<Object>(1));
}


static Object* Runtime_DefineOrRedefineAccessorProperty(
    RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 5);
  HandleScope scope(isolate);
  CONVERT_ARG_CHECKED(JSObject, obj, 0);
  CONVERT_CHECKED(String, name, args[1]);
  CONVERT_CHECKED(Smi, flag_setter, args[2]);
  CONVERT_CHECKED(JSFunction, fun, args[3]);
  CONVERT_CHECKED(Smi, flag_attr, args[4]);
  int unchecked = flag_attr->value();
  RUNTIME_ASSERT((unchecked & ~(READ_ONLY | DONT_ENUM | DONT_DELETE)) == 0);
  RUNTIME_ASSERT(!obj->IsNull());
  LookupResult result;
  obj->LocalLookupRealNamedProperty(name, &result);

  PropertyAttributes attr = static_cast<PropertyAttributes>(unchecked);
  // If an existing property is either FIELD, NORMAL or CONSTANT_FUNCTION
  // delete it to avoid running into trouble in DefineAccessor, which
  // handles this incorrectly if the property is readonly (does nothing)
  if (result.IsProperty() &&
      (result.type() == FIELD || result.type() == NORMAL
       || result.type() == CONSTANT_FUNCTION)) {
    Object* ok = obj->DeleteProperty(name, JSObject::NORMAL_DELETION);
    if (ok->IsFailure()) return ok;
  }
  return obj->DefineAccessor(name, flag_setter->value() == 0, fun, attr);
}

static Object* Runtime_DefineOrRedefineDataProperty(
    RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 4);
  HandleScope scope(isolate);
  CONVERT_ARG_CHECKED(JSObject, js_object, 0);
  CONVERT_ARG_CHECKED(String, name, 1);
  Handle<Object> obj_value = args.at<Object>(2);

  CONVERT_CHECKED(Smi, flag, args[3]);
  int unchecked = flag->value();
  RUNTIME_ASSERT((unchecked & ~(READ_ONLY | DONT_ENUM | DONT_DELETE)) == 0);

  PropertyAttributes attr = static_cast<PropertyAttributes>(unchecked);

  // Check if this is an element.
  uint32_t index;
  bool is_element = name->AsArrayIndex(&index);

  // Special case for elements if any of the flags are true.
  // If elements are in fast case we always implicitly assume that:
  // DONT_DELETE: false, DONT_ENUM: false, READ_ONLY: false.
  if (((unchecked & (DONT_DELETE | DONT_ENUM | READ_ONLY)) != 0) &&
      is_element) {
    // Normalize the elements to enable attributes on the property.
    js_object->NormalizeElements();
    NumberDictionary* dictionary = js_object->element_dictionary();
    // Make sure that we never go back to fast case.
    dictionary->set_requires_slow_elements();
    PropertyDetails details = PropertyDetails(attr, NORMAL);
    dictionary->Set(index, *obj_value, details);
  }

  LookupResult result;
  js_object->LocalLookupRealNamedProperty(*name, &result);

  // Take special care when attributes are different and there is already
  // a property. For simplicity we normalize the property which enables us
  // to not worry about changing the instance_descriptor and creating a new
  // map. The current version of SetObjectProperty does not handle attributes
  // correctly in the case where a property is a field and is reset with
  // new attributes.
  if (result.IsProperty() && attr != result.GetAttributes()) {
    // New attributes - normalize to avoid writing to instance descriptor
    js_object->NormalizeProperties(CLEAR_INOBJECT_PROPERTIES, 0);
    // Use IgnoreAttributes version since a readonly property may be
    // overridden and SetProperty does not allow this.
    return js_object->IgnoreAttributesAndSetLocalProperty(*name,
                                                          *obj_value,
                                                          attr);
  }

  return Runtime::SetObjectProperty(isolate, js_object, name, obj_value, attr);
}


Object* Runtime::SetObjectProperty(Isolate* isolate,
                                   Handle<Object> object,
                                   Handle<Object> key,
                                   Handle<Object> value,
                                   PropertyAttributes attr) {
  HandleScope scope(isolate);

  if (object->IsUndefined() || object->IsNull()) {
    Handle<Object> args[2] = { key, object };
    Handle<Object> error =
        isolate->factory()->NewTypeError("non_object_property_store",
                                         HandleVector(args, 2));
    return isolate->Throw(*error);
  }

  // If the object isn't a JavaScript object, we ignore the store.
  if (!object->IsJSObject()) return *value;

  Handle<JSObject> js_object = Handle<JSObject>::cast(object);

  // Check if the given key is an array index.
  uint32_t index;
  if (key->ToArrayIndex(&index)) {
    // In Firefox/SpiderMonkey, Safari and Opera you can access the characters
    // of a string using [] notation.  We need to support this too in
    // JavaScript.
    // In the case of a String object we just need to redirect the assignment to
    // the underlying string if the index is in range.  Since the underlying
    // string does nothing with the assignment then we can ignore such
    // assignments.
    if (js_object->IsStringObjectWithCharacterAt(index)) {
      return *value;
    }

    Handle<Object> result = SetElement(js_object, index, value);
    if (result.is_null()) return Failure::Exception();
    return *value;
  }

  if (key->IsString()) {
    Handle<Object> result;
    if (Handle<String>::cast(key)->AsArrayIndex(&index)) {
      result = SetElement(js_object, index, value);
    } else {
      Handle<String> key_string = Handle<String>::cast(key);
      key_string->TryFlatten();
      result = SetProperty(js_object, key_string, value, attr);
    }
    if (result.is_null()) return Failure::Exception();
    return *value;
  }

  // Call-back into JavaScript to convert the key to a string.
  bool has_pending_exception = false;
  Handle<Object> converted = Execution::ToString(key, &has_pending_exception);
  if (has_pending_exception) return Failure::Exception();
  Handle<String> name = Handle<String>::cast(converted);

  if (name->AsArrayIndex(&index)) {
    return js_object->SetElement(index, *value);
  } else {
    return js_object->SetProperty(*name, *value, attr);
  }
}


Object* Runtime::ForceSetObjectProperty(Isolate* isolate,
                                        Handle<JSObject> js_object,
                                        Handle<Object> key,
                                        Handle<Object> value,
                                        PropertyAttributes attr) {
  HandleScope scope(isolate);

  // Check if the given key is an array index.
  uint32_t index;
  if (key->ToArrayIndex(&index)) {
    // In Firefox/SpiderMonkey, Safari and Opera you can access the characters
    // of a string using [] notation.  We need to support this too in
    // JavaScript.
    // In the case of a String object we just need to redirect the assignment to
    // the underlying string if the index is in range.  Since the underlying
    // string does nothing with the assignment then we can ignore such
    // assignments.
    if (js_object->IsStringObjectWithCharacterAt(index)) {
      return *value;
    }

    return js_object->SetElement(index, *value);
  }

  if (key->IsString()) {
    if (Handle<String>::cast(key)->AsArrayIndex(&index)) {
      return js_object->SetElement(index, *value);
    } else {
      Handle<String> key_string = Handle<String>::cast(key);
      key_string->TryFlatten();
      return js_object->IgnoreAttributesAndSetLocalProperty(*key_string,
                                                            *value,
                                                            attr);
    }
  }

  // Call-back into JavaScript to convert the key to a string.
  bool has_pending_exception = false;
  Handle<Object> converted = Execution::ToString(key, &has_pending_exception);
  if (has_pending_exception) return Failure::Exception();
  Handle<String> name = Handle<String>::cast(converted);

  if (name->AsArrayIndex(&index)) {
    return js_object->SetElement(index, *value);
  } else {
    return js_object->IgnoreAttributesAndSetLocalProperty(*name, *value, attr);
  }
}


Object* Runtime::ForceDeleteObjectProperty(Isolate* isolate,
                                           Handle<JSObject> js_object,
                                           Handle<Object> key) {
  HandleScope scope(isolate);

  // Check if the given key is an array index.
  uint32_t index;
  if (key->ToArrayIndex(&index)) {
    // In Firefox/SpiderMonkey, Safari and Opera you can access the
    // characters of a string using [] notation.  In the case of a
    // String object we just need to redirect the deletion to the
    // underlying string if the index is in range.  Since the
    // underlying string does nothing with the deletion, we can ignore
    // such deletions.
    if (js_object->IsStringObjectWithCharacterAt(index)) {
      return isolate->heap()->true_value();
    }

    return js_object->DeleteElement(index, JSObject::FORCE_DELETION);
  }

  Handle<String> key_string;
  if (key->IsString()) {
    key_string = Handle<String>::cast(key);
  } else {
    // Call-back into JavaScript to convert the key to a string.
    bool has_pending_exception = false;
    Handle<Object> converted = Execution::ToString(key, &has_pending_exception);
    if (has_pending_exception) return Failure::Exception();
    key_string = Handle<String>::cast(converted);
  }

  key_string->TryFlatten();
  return js_object->DeleteProperty(*key_string, JSObject::FORCE_DELETION);
}


static Object* Runtime_SetProperty(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  RUNTIME_ASSERT(args.length() == 3 || args.length() == 4);

  Handle<Object> object = args.at<Object>(0);
  Handle<Object> key = args.at<Object>(1);
  Handle<Object> value = args.at<Object>(2);

  // Compute attributes.
  PropertyAttributes attributes = NONE;
  if (args.length() == 4) {
    CONVERT_CHECKED(Smi, value_obj, args[3]);
    int unchecked_value = value_obj->value();
    // Only attribute bits should be set.
    RUNTIME_ASSERT(
        (unchecked_value & ~(READ_ONLY | DONT_ENUM | DONT_DELETE)) == 0);
    attributes = static_cast<PropertyAttributes>(unchecked_value);
  }
  return Runtime::SetObjectProperty(isolate, object, key, value, attributes);
}


// Set a local property, even if it is READ_ONLY.  If the property does not
// exist, it will be added with attributes NONE.
static Object* Runtime_IgnoreAttributesAndSetProperty(
    RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  RUNTIME_ASSERT(args.length() == 3 || args.length() == 4);
  CONVERT_CHECKED(JSObject, object, args[0]);
  CONVERT_CHECKED(String, name, args[1]);
  // Compute attributes.
  PropertyAttributes attributes = NONE;
  if (args.length() == 4) {
    CONVERT_CHECKED(Smi, value_obj, args[3]);
    int unchecked_value = value_obj->value();
    // Only attribute bits should be set.
    RUNTIME_ASSERT(
        (unchecked_value & ~(READ_ONLY | DONT_ENUM | DONT_DELETE)) == 0);
    attributes = static_cast<PropertyAttributes>(unchecked_value);
  }

  return object->
      IgnoreAttributesAndSetLocalProperty(name, args[2], attributes);
}


static Object* Runtime_DeleteProperty(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_CHECKED(JSObject, object, args[0]);
  CONVERT_CHECKED(String, key, args[1]);
  return object->DeleteProperty(key, JSObject::NORMAL_DELETION);
}


static Object* HasLocalPropertyImplementation(Isolate* isolate,
                                              Handle<JSObject> object,
                                              Handle<String> key) {
  if (object->HasLocalProperty(*key)) return isolate->heap()->true_value();
  // Handle hidden prototypes.  If there's a hidden prototype above this thing
  // then we have to check it for properties, because they are supposed to
  // look like they are on this object.
  Handle<Object> proto(object->GetPrototype());
  if (proto->IsJSObject() &&
      Handle<JSObject>::cast(proto)->map()->is_hidden_prototype()) {
    return HasLocalPropertyImplementation(isolate,
                                          Handle<JSObject>::cast(proto),
                                          key);
  }
  return isolate->heap()->false_value();
}


static Object* Runtime_HasLocalProperty(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);
  CONVERT_CHECKED(String, key, args[1]);

  Object* obj = args[0];
  // Only JS objects can have properties.
  if (obj->IsJSObject()) {
    JSObject* object = JSObject::cast(obj);
    // Fast case - no interceptors.
    if (object->HasRealNamedProperty(key)) return isolate->heap()->true_value();
    // Slow case.  Either it's not there or we have an interceptor.  We should
    // have handles for this kind of deal.
    HandleScope scope(isolate);
    return HasLocalPropertyImplementation(isolate,
                                          Handle<JSObject>(object),
                                          Handle<String>(key));
  } else if (obj->IsString()) {
    // Well, there is one exception:  Handle [] on strings.
    uint32_t index;
    if (key->AsArrayIndex(&index)) {
      String* string = String::cast(obj);
      if (index < static_cast<uint32_t>(string->length()))
        return isolate->heap()->true_value();
    }
  }
  return isolate->heap()->false_value();
}


static Object* Runtime_HasProperty(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation na;
  ASSERT(args.length() == 2);

  // Only JS objects can have properties.
  if (args[0]->IsJSObject()) {
    JSObject* object = JSObject::cast(args[0]);
    CONVERT_CHECKED(String, key, args[1]);
    if (object->HasProperty(key)) return isolate->heap()->true_value();
  }
  return isolate->heap()->false_value();
}


static Object* Runtime_HasElement(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation na;
  ASSERT(args.length() == 2);

  // Only JS objects can have elements.
  if (args[0]->IsJSObject()) {
    JSObject* object = JSObject::cast(args[0]);
    CONVERT_CHECKED(Smi, index_obj, args[1]);
    uint32_t index = index_obj->value();
    if (object->HasElement(index)) return isolate->heap()->true_value();
  }
  return isolate->heap()->false_value();
}


static Object* Runtime_IsPropertyEnumerable(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_CHECKED(JSObject, object, args[0]);
  CONVERT_CHECKED(String, key, args[1]);

  uint32_t index;
  if (key->AsArrayIndex(&index)) {
    return isolate->heap()->ToBoolean(object->HasElement(index));
  }

  PropertyAttributes att = object->GetLocalPropertyAttribute(key);
  return isolate->heap()->ToBoolean(att != ABSENT && (att & DONT_ENUM) == 0);
}


static Object* Runtime_GetPropertyNames(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(JSObject, object, 0);
  return *GetKeysFor(object);
}


// Returns either a FixedArray as Runtime_GetPropertyNames,
// or, if the given object has an enum cache that contains
// all enumerable properties of the object and its prototypes
// have none, the map of the object. This is used to speed up
// the check for deletions during a for-in.
static Object* Runtime_GetPropertyNamesFast(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 1);

  CONVERT_CHECKED(JSObject, raw_object, args[0]);

  if (raw_object->IsSimpleEnum()) return raw_object->map();

  HandleScope scope(isolate);
  Handle<JSObject> object(raw_object);
  Handle<FixedArray> content = GetKeysInFixedArrayFor(object,
                                                      INCLUDE_PROTOS);

  // Test again, since cache may have been built by preceding call.
  if (object->IsSimpleEnum()) return object->map();

  return *content;
}


// Find the length of the prototype chain that is to to handled as one. If a
// prototype object is hidden it is to be viewed as part of the the object it
// is prototype for.
static int LocalPrototypeChainLength(JSObject* obj) {
  int count = 1;
  Object* proto = obj->GetPrototype();
  while (proto->IsJSObject() &&
         JSObject::cast(proto)->map()->is_hidden_prototype()) {
    count++;
    proto = JSObject::cast(proto)->GetPrototype();
  }
  return count;
}


// Return the names of the local named properties.
// args[0]: object
static Object* Runtime_GetLocalPropertyNames(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  if (!args[0]->IsJSObject()) {
    return isolate->heap()->undefined_value();
  }
  CONVERT_ARG_CHECKED(JSObject, obj, 0);

  // Skip the global proxy as it has no properties and always delegates to the
  // real global object.
  if (obj->IsJSGlobalProxy()) {
    // Only collect names if access is permitted.
    if (obj->IsAccessCheckNeeded() &&
        !isolate->MayNamedAccess(*obj,
                                 isolate->heap()->undefined_value(),
                                 v8::ACCESS_KEYS)) {
      isolate->ReportFailedAccessCheck(*obj, v8::ACCESS_KEYS);
      return *isolate->factory()->NewJSArray(0);
    }
    obj = Handle<JSObject>(JSObject::cast(obj->GetPrototype()));
  }

  // Find the number of objects making up this.
  int length = LocalPrototypeChainLength(*obj);

  // Find the number of local properties for each of the objects.
  ScopedVector<int> local_property_count(length);
  int total_property_count = 0;
  Handle<JSObject> jsproto = obj;
  for (int i = 0; i < length; i++) {
    // Only collect names if access is permitted.
    if (jsproto->IsAccessCheckNeeded() &&
        !isolate->MayNamedAccess(*jsproto,
                                 isolate->heap()->undefined_value(),
                                 v8::ACCESS_KEYS)) {
      isolate->ReportFailedAccessCheck(*jsproto, v8::ACCESS_KEYS);
      return *isolate->factory()->NewJSArray(0);
    }
    int n;
    n = jsproto->NumberOfLocalProperties(static_cast<PropertyAttributes>(NONE));
    local_property_count[i] = n;
    total_property_count += n;
    if (i < length - 1) {
      jsproto = Handle<JSObject>(JSObject::cast(jsproto->GetPrototype()));
    }
  }

  // Allocate an array with storage for all the property names.
  Handle<FixedArray> names =
      isolate->factory()->NewFixedArray(total_property_count);

  // Get the property names.
  jsproto = obj;
  int proto_with_hidden_properties = 0;
  for (int i = 0; i < length; i++) {
    jsproto->GetLocalPropertyNames(*names,
                                   i == 0 ? 0 : local_property_count[i - 1]);
    if (!GetHiddenProperties(jsproto, false)->IsUndefined()) {
      proto_with_hidden_properties++;
    }
    if (i < length - 1) {
      jsproto = Handle<JSObject>(JSObject::cast(jsproto->GetPrototype()));
    }
  }

  // Filter out name of hidden propeties object.
  if (proto_with_hidden_properties > 0) {
    Handle<FixedArray> old_names = names;
    names = isolate->factory()->NewFixedArray(
        names->length() - proto_with_hidden_properties);
    int dest_pos = 0;
    for (int i = 0; i < total_property_count; i++) {
      Object* name = old_names->get(i);
      if (name == isolate->heap()->hidden_symbol()) {
        continue;
      }
      names->set(dest_pos++, name);
    }
  }

  return *isolate->factory()->NewJSArrayWithElements(names);
}


// Return the names of the local indexed properties.
// args[0]: object
static Object* Runtime_GetLocalElementNames(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  if (!args[0]->IsJSObject()) {
    return isolate->heap()->undefined_value();
  }
  CONVERT_ARG_CHECKED(JSObject, obj, 0);

  int n = obj->NumberOfLocalElements(static_cast<PropertyAttributes>(NONE));
  Handle<FixedArray> names = isolate->factory()->NewFixedArray(n);
  obj->GetLocalElementKeys(*names, static_cast<PropertyAttributes>(NONE));
  return *isolate->factory()->NewJSArrayWithElements(names);
}


// Return information on whether an object has a named or indexed interceptor.
// args[0]: object
static Object* Runtime_GetInterceptorInfo(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  if (!args[0]->IsJSObject()) {
    return Smi::FromInt(0);
  }
  CONVERT_ARG_CHECKED(JSObject, obj, 0);

  int result = 0;
  if (obj->HasNamedInterceptor()) result |= 2;
  if (obj->HasIndexedInterceptor()) result |= 1;

  return Smi::FromInt(result);
}


// Return property names from named interceptor.
// args[0]: object
static Object* Runtime_GetNamedInterceptorPropertyNames(
    RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(JSObject, obj, 0);

  if (obj->HasNamedInterceptor()) {
    v8::Handle<v8::Array> result = GetKeysForNamedInterceptor(obj, obj);
    if (!result.IsEmpty()) return *v8::Utils::OpenHandle(*result);
  }
  return isolate->heap()->undefined_value();
}


// Return element names from indexed interceptor.
// args[0]: object
static Object* Runtime_GetIndexedInterceptorElementNames(
    RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(JSObject, obj, 0);

  if (obj->HasIndexedInterceptor()) {
    v8::Handle<v8::Array> result = GetKeysForIndexedInterceptor(obj, obj);
    if (!result.IsEmpty()) return *v8::Utils::OpenHandle(*result);
  }
  return isolate->heap()->undefined_value();
}


static Object* Runtime_LocalKeys(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT_EQ(args.length(), 1);
  CONVERT_CHECKED(JSObject, raw_object, args[0]);
  HandleScope scope(isolate);
  Handle<JSObject> object(raw_object);
  Handle<FixedArray> contents = GetKeysInFixedArrayFor(object,
                                                       LOCAL_ONLY);
  // Some fast paths through GetKeysInFixedArrayFor reuse a cached
  // property array and since the result is mutable we have to create
  // a fresh clone on each invocation.
  int length = contents->length();
  Handle<FixedArray> copy = isolate->factory()->NewFixedArray(length);
  for (int i = 0; i < length; i++) {
    Object* entry = contents->get(i);
    if (entry->IsString()) {
      copy->set(i, entry);
    } else {
      ASSERT(entry->IsNumber());
      HandleScope scope(isolate);
      Handle<Object> entry_handle(entry, isolate);
      Handle<Object> entry_str =
          isolate->factory()->NumberToString(entry_handle);
      copy->set(i, *entry_str);
    }
  }
  return *isolate->factory()->NewJSArrayWithElements(copy);
}


static Object* Runtime_GetArgumentsProperty(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);

  // Compute the frame holding the arguments.
  JavaScriptFrameIterator it;
  it.AdvanceToArgumentsFrame();
  JavaScriptFrame* frame = it.frame();

  // Get the actual number of provided arguments.
  const uint32_t n = frame->GetProvidedParametersCount();

  // Try to convert the key to an index. If successful and within
  // index return the the argument from the frame.
  uint32_t index;
  if (args[0]->ToArrayIndex(&index) && index < n) {
    return frame->GetParameter(index);
  }

  // Convert the key to a string.
  HandleScope scope(isolate);
  bool exception = false;
  Handle<Object> converted =
      Execution::ToString(args.at<Object>(0), &exception);
  if (exception) return Failure::Exception();
  Handle<String> key = Handle<String>::cast(converted);

  // Try to convert the string key into an array index.
  if (key->AsArrayIndex(&index)) {
    if (index < n) {
      return frame->GetParameter(index);
    } else {
      return isolate->initial_object_prototype()->GetElement(index);
    }
  }

  // Handle special arguments properties.
  if (key->Equals(isolate->heap()->length_symbol())) return Smi::FromInt(n);
  if (key->Equals(isolate->heap()->callee_symbol())) return frame->function();

  // Lookup in the initial Object.prototype object.
  return isolate->initial_object_prototype()->GetProperty(*key);
}


static Object* Runtime_ToFastProperties(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);

  ASSERT(args.length() == 1);
  Handle<Object> object = args.at<Object>(0);
  if (object->IsJSObject()) {
    Handle<JSObject> js_object = Handle<JSObject>::cast(object);
    if (!js_object->HasFastProperties() && !js_object->IsGlobalObject()) {
      js_object->TransformToFastProperties(0);
    }
  }
  return *object;
}


static Object* Runtime_ToSlowProperties(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);

  ASSERT(args.length() == 1);
  Handle<Object> object = args.at<Object>(0);
  if (object->IsJSObject()) {
    Handle<JSObject> js_object = Handle<JSObject>::cast(object);
    js_object->NormalizeProperties(CLEAR_INOBJECT_PROPERTIES, 0);
  }
  return *object;
}


static Object* Runtime_ToBool(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);

  return args[0]->ToBoolean();
}


// Returns the type string of a value; see ECMA-262, 11.4.3 (p 47).
// Possible optimizations: put the type string into the oddballs.
static Object* Runtime_Typeof(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;

  Object* obj = args[0];
  if (obj->IsNumber()) return isolate->heap()->number_symbol();
  HeapObject* heap_obj = HeapObject::cast(obj);

  // typeof an undetectable object is 'undefined'
  if (heap_obj->map()->is_undetectable()) {
    return isolate->heap()->undefined_symbol();
  }

  InstanceType instance_type = heap_obj->map()->instance_type();
  if (instance_type < FIRST_NONSTRING_TYPE) {
    return isolate->heap()->string_symbol();
  }

  switch (instance_type) {
    case ODDBALL_TYPE:
      if (heap_obj->IsTrue() || heap_obj->IsFalse()) {
        return isolate->heap()->boolean_symbol();
      }
      if (heap_obj->IsNull()) {
        return isolate->heap()->object_symbol();
      }
      ASSERT(heap_obj->IsUndefined());
      return isolate->heap()->undefined_symbol();
    case JS_FUNCTION_TYPE: case JS_REGEXP_TYPE:
      return isolate->heap()->function_symbol();
    default:
      // For any kind of object not handled above, the spec rule for
      // host objects gives that it is okay to return "object"
      return isolate->heap()->object_symbol();
  }
}


static bool AreDigits(const char*s, int from, int to) {
  for (int i = from; i < to; i++) {
    if (s[i] < '0' || s[i] > '9') return false;
  }

  return true;
}


static int ParseDecimalInteger(const char*s, int from, int to) {
  ASSERT(to - from < 10);  // Overflow is not possible.
  ASSERT(from < to);
  int d = s[from] - '0';

  for (int i = from + 1; i < to; i++) {
    d = 10 * d + (s[i] - '0');
  }

  return d;
}


static Object* Runtime_StringToNumber(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);
  CONVERT_CHECKED(String, subject, args[0]);
  subject->TryFlatten();

  // Fast case: short integer or some sorts of junk values.
  int len = subject->length();
  if (subject->IsSeqAsciiString()) {
    if (len == 0) return Smi::FromInt(0);

    char const* data = SeqAsciiString::cast(subject)->GetChars();
    bool minus = (data[0] == '-');
    int start_pos = (minus ? 1 : 0);

    if (start_pos == len) {
      return isolate->heap()->nan_value();
    } else if (data[start_pos] > '9') {
      // Fast check for a junk value. A valid string may start from a
      // whitespace, a sign ('+' or '-'), the decimal point, a decimal digit or
      // the 'I' character ('Infinity'). All of that have codes not greater than
      // '9' except 'I'.
      if (data[start_pos] != 'I') {
        return isolate->heap()->nan_value();
      }
    } else if (len - start_pos < 10 && AreDigits(data, start_pos, len)) {
      // The maximal/minimal smi has 10 digits. If the string has less digits we
      // know it will fit into the smi-data type.
      int d = ParseDecimalInteger(data, start_pos, len);
      if (minus) {
        if (d == 0) return isolate->heap()->minus_zero_value();
        d = -d;
      } else if (!subject->HasHashCode() &&
                 len <= String::kMaxArrayIndexSize &&
                 (len == 1 || data[0] != '0')) {
        // String hash is not calculated yet but all the data are present.
        // Update the hash field to speed up sequential convertions.
        uint32_t hash = StringHasher::MakeArrayIndexHash(d, len);
#ifdef DEBUG
        subject->Hash();  // Force hash calculation.
        ASSERT_EQ(static_cast<int>(subject->hash_field()),
                  static_cast<int>(hash));
#endif
        subject->set_hash_field(hash);
      }
      return Smi::FromInt(d);
    }
  }

  // Slower case.
  return isolate->heap()->NumberFromDouble(StringToDouble(subject, ALLOW_HEX));
}


static Object* Runtime_StringFromCharCodeArray(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);

  CONVERT_CHECKED(JSArray, codes, args[0]);
  int length = Smi::cast(codes->length())->value();

  // Check if the string can be ASCII.
  int i;
  for (i = 0; i < length; i++) {
    Object* element = codes->GetElement(i);
    CONVERT_NUMBER_CHECKED(int, chr, Int32, element);
    if ((chr & 0xffff) > String::kMaxAsciiCharCode)
      break;
  }

  Object* object = NULL;
  if (i == length) {  // The string is ASCII.
    object = isolate->heap()->AllocateRawAsciiString(length);
  } else {  // The string is not ASCII.
    object = isolate->heap()->AllocateRawTwoByteString(length);
  }

  if (object->IsFailure()) return object;
  String* result = String::cast(object);
  for (int i = 0; i < length; i++) {
    Object* element = codes->GetElement(i);
    CONVERT_NUMBER_CHECKED(int, chr, Int32, element);
    result->Set(i, chr & 0xffff);
  }
  return result;
}


// kNotEscaped is generated by the following:
//
// #!/bin/perl
// for (my $i = 0; $i < 256; $i++) {
//   print "\n" if $i % 16 == 0;
//   my $c = chr($i);
//   my $escaped = 1;
//   $escaped = 0 if $c =~ m#[A-Za-z0-9@*_+./-]#;
//   print $escaped ? "0, " : "1, ";
// }


static bool IsNotEscaped(uint16_t character) {
  // Only for 8 bit characters, the rest are always escaped (in a different way)
  ASSERT(character < 256);
  static const char kNotEscaped[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1,
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  };
  return kNotEscaped[character] != 0;
}


static Object* Runtime_URIEscape(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  const char hex_chars[] = "0123456789ABCDEF";
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);
  CONVERT_CHECKED(String, source, args[0]);

  source->TryFlatten();

  int escaped_length = 0;
  int length = source->length();
  {
    Access<StringInputBuffer> buffer(
        isolate->runtime_state()->string_input_buffer());
    buffer->Reset(source);
    while (buffer->has_more()) {
      uint16_t character = buffer->GetNext();
      if (character >= 256) {
        escaped_length += 6;
      } else if (IsNotEscaped(character)) {
        escaped_length++;
      } else {
        escaped_length += 3;
      }
      // We don't allow strings that are longer than a maximal length.
      ASSERT(String::kMaxLength < 0x7fffffff - 6);  // Cannot overflow.
      if (escaped_length > String::kMaxLength) {
        isolate->context()->mark_out_of_memory();
        return Failure::OutOfMemoryException();
      }
    }
  }
  // No length change implies no change.  Return original string if no change.
  if (escaped_length == length) {
    return source;
  }
  Object* o = isolate->heap()->AllocateRawAsciiString(escaped_length);
  if (o->IsFailure()) return o;
  String* destination = String::cast(o);
  int dest_position = 0;

  Access<StringInputBuffer> buffer(
      isolate->runtime_state()->string_input_buffer());
  buffer->Rewind();
  while (buffer->has_more()) {
    uint16_t chr = buffer->GetNext();
    if (chr >= 256) {
      destination->Set(dest_position, '%');
      destination->Set(dest_position+1, 'u');
      destination->Set(dest_position+2, hex_chars[chr >> 12]);
      destination->Set(dest_position+3, hex_chars[(chr >> 8) & 0xf]);
      destination->Set(dest_position+4, hex_chars[(chr >> 4) & 0xf]);
      destination->Set(dest_position+5, hex_chars[chr & 0xf]);
      dest_position += 6;
    } else if (IsNotEscaped(chr)) {
      destination->Set(dest_position, chr);
      dest_position++;
    } else {
      destination->Set(dest_position, '%');
      destination->Set(dest_position+1, hex_chars[chr >> 4]);
      destination->Set(dest_position+2, hex_chars[chr & 0xf]);
      dest_position += 3;
    }
  }
  return destination;
}


static inline int TwoDigitHex(uint16_t character1, uint16_t character2) {
  static const signed char kHexValue['g'] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    0,  1,  2,   3,  4,  5,  6,  7,  8,  9, -1, -1, -1, -1, -1, -1,
    -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, 10, 11, 12, 13, 14, 15 };

  if (character1 > 'f') return -1;
  int hi = kHexValue[character1];
  if (hi == -1) return -1;
  if (character2 > 'f') return -1;
  int lo = kHexValue[character2];
  if (lo == -1) return -1;
  return (hi << 4) + lo;
}


static inline int Unescape(String* source,
                           int i,
                           int length,
                           int* step) {
  uint16_t character = source->Get(i);
  int32_t hi = 0;
  int32_t lo = 0;
  if (character == '%' &&
      i <= length - 6 &&
      source->Get(i + 1) == 'u' &&
      (hi = TwoDigitHex(source->Get(i + 2),
                        source->Get(i + 3))) != -1 &&
      (lo = TwoDigitHex(source->Get(i + 4),
                        source->Get(i + 5))) != -1) {
    *step = 6;
    return (hi << 8) + lo;
  } else if (character == '%' &&
      i <= length - 3 &&
      (lo = TwoDigitHex(source->Get(i + 1),
                        source->Get(i + 2))) != -1) {
    *step = 3;
    return lo;
  } else {
    *step = 1;
    return character;
  }
}


static Object* Runtime_URIUnescape(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);
  CONVERT_CHECKED(String, source, args[0]);

  source->TryFlatten();

  bool ascii = true;
  int length = source->length();

  int unescaped_length = 0;
  for (int i = 0; i < length; unescaped_length++) {
    int step;
    if (Unescape(source, i, length, &step) > String::kMaxAsciiCharCode) {
      ascii = false;
    }
    i += step;
  }

  // No length change implies no change.  Return original string if no change.
  if (unescaped_length == length)
    return source;

  Object* o = ascii ?
              isolate->heap()->AllocateRawAsciiString(unescaped_length) :
              isolate->heap()->AllocateRawTwoByteString(unescaped_length);
  if (o->IsFailure()) return o;
  String* destination = String::cast(o);

  int dest_position = 0;
  for (int i = 0; i < length; dest_position++) {
    int step;
    destination->Set(dest_position, Unescape(source, i, length, &step));
    i += step;
  }
  return destination;
}


static Object* Runtime_StringParseInt(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;

  CONVERT_CHECKED(String, s, args[0]);
  CONVERT_SMI_CHECKED(radix, args[1]);

  s->TryFlatten();

  RUNTIME_ASSERT(radix == 0 || (2 <= radix && radix <= 36));
  double value = StringToInt(s, radix);
  return isolate->heap()->NumberFromDouble(value);
}


static Object* Runtime_StringParseFloat(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  CONVERT_CHECKED(String, str, args[0]);

  // ECMA-262 section 15.1.2.3, empty string is NaN
  double value = StringToDouble(str, ALLOW_TRAILING_JUNK, OS::nan_value());

  // Create a number object from the value.
  return isolate->heap()->NumberFromDouble(value);
}


template <class Converter>
static Object* ConvertCaseHelper(Isolate* isolate,
                                 RuntimeState* state,
                                 String* s,
                                 int length,
                                 int input_string_length,
                                 unibrow::Mapping<Converter, 128>* mapping) {
  // We try this twice, once with the assumption that the result is no longer
  // than the input and, if that assumption breaks, again with the exact
  // length.  This may not be pretty, but it is nicer than what was here before
  // and I hereby claim my vaffel-is.
  //
  // Allocate the resulting string.
  //
  // NOTE: This assumes that the upper/lower case of an ascii
  // character is also ascii.  This is currently the case, but it
  // might break in the future if we implement more context and locale
  // dependent upper/lower conversions.
  Object* o = s->IsAsciiRepresentation()
      ? isolate->heap()->AllocateRawAsciiString(length)
      : isolate->heap()->AllocateRawTwoByteString(length);
  if (o->IsFailure()) return o;
  String* result = String::cast(o);
  bool has_changed_character = false;

  // Convert all characters to upper case, assuming that they will fit
  // in the buffer
  Access<StringInputBuffer> buffer(state->string_input_buffer());
  buffer->Reset(s);
  unibrow::uchar chars[Converter::kMaxWidth];
  // We can assume that the string is not empty
  uc32 current = buffer->GetNext();
  for (int i = 0; i < length;) {
    bool has_next = buffer->has_more();
    uc32 next = has_next ? buffer->GetNext() : 0;
    int char_length = mapping->get(current, next, chars);
    if (char_length == 0) {
      // The case conversion of this character is the character itself.
      result->Set(i, current);
      i++;
    } else if (char_length == 1) {
      // Common case: converting the letter resulted in one character.
      ASSERT(static_cast<uc32>(chars[0]) != current);
      result->Set(i, chars[0]);
      has_changed_character = true;
      i++;
    } else if (length == input_string_length) {
      // We've assumed that the result would be as long as the
      // input but here is a character that converts to several
      // characters.  No matter, we calculate the exact length
      // of the result and try the whole thing again.
      //
      // Note that this leaves room for optimization.  We could just
      // memcpy what we already have to the result string.  Also,
      // the result string is the last object allocated we could
      // "realloc" it and probably, in the vast majority of cases,
      // extend the existing string to be able to hold the full
      // result.
      int next_length = 0;
      if (has_next) {
        next_length = mapping->get(next, 0, chars);
        if (next_length == 0) next_length = 1;
      }
      int current_length = i + char_length + next_length;
      while (buffer->has_more()) {
        current = buffer->GetNext();
        // NOTE: we use 0 as the next character here because, while
        // the next character may affect what a character converts to,
        // it does not in any case affect the length of what it convert
        // to.
        int char_length = mapping->get(current, 0, chars);
        if (char_length == 0) char_length = 1;
        current_length += char_length;
        if (current_length > Smi::kMaxValue) {
          isolate->context()->mark_out_of_memory();
          return Failure::OutOfMemoryException();
        }
      }
      // Try again with the real length.
      return Smi::FromInt(current_length);
    } else {
      for (int j = 0; j < char_length; j++) {
        result->Set(i, chars[j]);
        i++;
      }
      has_changed_character = true;
    }
    current = next;
  }
  if (has_changed_character) {
    return result;
  } else {
    // If we didn't actually change anything in doing the conversion
    // we simple return the result and let the converted string
    // become garbage; there is no reason to keep two identical strings
    // alive.
    return s;
  }
}


namespace {

struct ToLowerTraits {
  typedef unibrow::ToLowercase UnibrowConverter;

  static bool ConvertAscii(char* dst, char* src, int length) {
    bool changed = false;
    for (int i = 0; i < length; ++i) {
      char c = src[i];
      if ('A' <= c && c <= 'Z') {
        c += ('a' - 'A');
        changed = true;
      }
      dst[i] = c;
    }
    return changed;
  }
};


struct ToUpperTraits {
  typedef unibrow::ToUppercase UnibrowConverter;

  static bool ConvertAscii(char* dst, char* src, int length) {
    bool changed = false;
    for (int i = 0; i < length; ++i) {
      char c = src[i];
      if ('a' <= c && c <= 'z') {
        c -= ('a' - 'A');
        changed = true;
      }
      dst[i] = c;
    }
    return changed;
  }
};

}  // namespace


template <typename ConvertTraits>
static Object* ConvertCase(
    Arguments args,
    Isolate* isolate,
    unibrow::Mapping<typename ConvertTraits::UnibrowConverter, 128>* mapping) {
  RuntimeState* state = isolate->runtime_state();
  NoHandleAllocation ha;
  CONVERT_CHECKED(String, s, args[0]);
  s = s->TryFlattenGetString();

  const int length = s->length();
  // Assume that the string is not empty; we need this assumption later
  if (length == 0) return s;

  // Simpler handling of ascii strings.
  //
  // NOTE: This assumes that the upper/lower case of an ascii
  // character is also ascii.  This is currently the case, but it
  // might break in the future if we implement more context and locale
  // dependent upper/lower conversions.
  if (s->IsSeqAsciiString()) {
    Object* o = isolate->heap()->AllocateRawAsciiString(length);
    if (o->IsFailure()) return o;
    SeqAsciiString* result = SeqAsciiString::cast(o);
    bool has_changed_character = ConvertTraits::ConvertAscii(
        result->GetChars(), SeqAsciiString::cast(s)->GetChars(), length);
    return has_changed_character ? result : s;
  }

  Object* answer = ConvertCaseHelper(
      isolate, state, s, length, length, mapping);
  if (answer->IsSmi()) {
    // Retry with correct length.
    answer = ConvertCaseHelper(isolate, state, s, Smi::cast(answer)->value(),
                               length, mapping);
  }
  return answer;  // This may be a failure.
}


static Object* Runtime_StringToLowerCase(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  return ConvertCase<ToLowerTraits>(
      args, isolate, isolate->runtime_state()->to_lower_mapping());
}


static Object* Runtime_StringToUpperCase(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  return ConvertCase<ToUpperTraits>(
      args, isolate, isolate->runtime_state()->to_upper_mapping());
}


static inline bool IsTrimWhiteSpace(unibrow::uchar c) {
  return unibrow::WhiteSpace::Is(c) || c == 0x200b;
}


static Object* Runtime_StringTrim(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 3);

  CONVERT_CHECKED(String, s, args[0]);
  CONVERT_BOOLEAN_CHECKED(trimLeft, args[1]);
  CONVERT_BOOLEAN_CHECKED(trimRight, args[2]);

  s->TryFlatten();
  int length = s->length();

  int left = 0;
  if (trimLeft) {
    while (left < length && IsTrimWhiteSpace(s->Get(left))) {
      left++;
    }
  }

  int right = length;
  if (trimRight) {
    while (right > left && IsTrimWhiteSpace(s->Get(right - 1))) {
      right--;
    }
  }
  return s->SubString(left, right);
}


template <typename SubjectChar, typename PatternChar>
void FindStringIndices(Isolate* isolate,
                       Vector<const SubjectChar> subject,
                       Vector<const PatternChar> pattern,
                       ZoneList<int>* indices,
                       unsigned int limit) {
  ASSERT(limit > 0);
  // Collect indices of pattern in subject, and the end-of-string index.
  // Stop after finding at most limit values.
  StringSearch<PatternChar, SubjectChar> search(isolate, pattern);
  int pattern_length = pattern.length();
  int index = 0;
  while (limit > 0) {
    index = search.Search(subject, index);
    if (index < 0) return;
    indices->Add(index);
    index += pattern_length;
    limit--;
  }
}


static Object* Runtime_StringSplit(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 3);
  HandleScope handle_scope(isolate);
  CONVERT_ARG_CHECKED(String, subject, 0);
  CONVERT_ARG_CHECKED(String, pattern, 1);
  CONVERT_NUMBER_CHECKED(uint32_t, limit, Uint32, args[2]);

  int subject_length = subject->length();
  int pattern_length = pattern->length();
  RUNTIME_ASSERT(pattern_length > 0);

  // The limit can be very large (0xffffffffu), but since the pattern
  // isn't empty, we can never create more parts than ~half the length
  // of the subject.

  if (!subject->IsFlat()) FlattenString(subject);

  static const int kMaxInitialListCapacity = 16;

  ZoneScope scope(DELETE_ON_EXIT);

  // Find (up to limit) indices of separator and end-of-string in subject
  int initial_capacity = Min<uint32_t>(kMaxInitialListCapacity, limit);
  ZoneList<int> indices(initial_capacity);
  if (!pattern->IsFlat()) FlattenString(pattern);

  // No allocation block.
  {
    AssertNoAllocation nogc;
    if (subject->IsAsciiRepresentation()) {
      Vector<const char> subject_vector = subject->ToAsciiVector();
      if (pattern->IsAsciiRepresentation()) {
        FindStringIndices(isolate,
                          subject_vector,
                          pattern->ToAsciiVector(),
                          &indices,
                          limit);
      } else {
        FindStringIndices(isolate,
                          subject_vector,
                          pattern->ToUC16Vector(),
                          &indices,
                          limit);
      }
    } else {
      Vector<const uc16> subject_vector = subject->ToUC16Vector();
      if (pattern->IsAsciiRepresentation()) {
        FindStringIndices(isolate,
                          subject_vector,
                          pattern->ToAsciiVector(),
                          &indices,
                          limit);
      } else {
        FindStringIndices(isolate,
                          subject_vector,
                          pattern->ToUC16Vector(),
                          &indices,
                          limit);
      }
    }
  }

  if (static_cast<uint32_t>(indices.length()) < limit) {
    indices.Add(subject_length);
  }

  // The list indices now contains the end of each part to create.

  // Create JSArray of substrings separated by separator.
  int part_count = indices.length();

  Handle<JSArray> result = isolate->factory()->NewJSArray(part_count);
  result->set_length(Smi::FromInt(part_count));

  ASSERT(result->HasFastElements());

  if (part_count == 1 && indices.at(0) == subject_length) {
    FixedArray::cast(result->elements())->set(0, *subject);
    return *result;
  }

  Handle<FixedArray> elements(FixedArray::cast(result->elements()));
  int part_start = 0;
  for (int i = 0; i < part_count; i++) {
    HandleScope local_loop_handle;
    int part_end = indices.at(i);
    Handle<String> substring =
        isolate->factory()->NewSubString(subject, part_start, part_end);
    elements->set(i, *substring);
    part_start = part_end + pattern_length;
  }

  return *result;
}


// Copies ascii characters to the given fixed array looking up
// one-char strings in the cache. Gives up on the first char that is
// not in the cache and fills the remainder with smi zeros. Returns
// the length of the successfully copied prefix.
static int CopyCachedAsciiCharsToArray(Heap* heap,
                                       const char* chars,
                                       FixedArray* elements,
                                       int length) {
  AssertNoAllocation nogc;
  FixedArray* ascii_cache = heap->single_character_string_cache();
  Object* undefined = heap->undefined_value();
  int i;
  for (i = 0; i < length; ++i) {
    Object* value = ascii_cache->get(chars[i]);
    if (value == undefined) break;
    ASSERT(!heap->InNewSpace(value));
    elements->set(i, value, SKIP_WRITE_BARRIER);
  }
  if (i < length) {
    ASSERT(Smi::FromInt(0) == 0);
    memset(elements->data_start() + i, 0, kPointerSize * (length - i));
  }
#ifdef DEBUG
  for (int j = 0; j < length; ++j) {
    Object* element = elements->get(j);
    ASSERT(element == Smi::FromInt(0) ||
           (element->IsString() && String::cast(element)->LooksValid()));
  }
#endif
  return i;
}


// Converts a String to JSArray.
// For example, "foo" => ["f", "o", "o"].
static Object* Runtime_StringToArray(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(String, s, 0);

  s->TryFlatten();
  const int length = s->length();

  Handle<FixedArray> elements;
  if (s->IsFlat() && s->IsAsciiRepresentation()) {
    Object* obj = isolate->heap()->AllocateUninitializedFixedArray(length);
    if (obj->IsFailure()) return obj;
    elements = Handle<FixedArray>(FixedArray::cast(obj));

    Vector<const char> chars = s->ToAsciiVector();
    // Note, this will initialize all elements (not only the prefix)
    // to prevent GC from seeing partially initialized array.
    int num_copied_from_cache = CopyCachedAsciiCharsToArray(isolate->heap(),
                                                            chars.start(),
                                                            *elements,
                                                            length);

    for (int i = num_copied_from_cache; i < length; ++i) {
      Handle<Object> str = LookupSingleCharacterStringFromCode(chars[i]);
      elements->set(i, *str);
    }
  } else {
    elements = isolate->factory()->NewFixedArray(length);
    for (int i = 0; i < length; ++i) {
      Handle<Object> str = LookupSingleCharacterStringFromCode(s->Get(i));
      elements->set(i, *str);
    }
  }

#ifdef DEBUG
  for (int i = 0; i < length; ++i) {
    ASSERT(String::cast(elements->get(i))->length() == 1);
  }
#endif

  return *isolate->factory()->NewJSArrayWithElements(elements);
}


static Object* Runtime_NewStringWrapper(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);
  CONVERT_CHECKED(String, value, args[0]);
  return value->ToObject();
}


bool Runtime::IsUpperCaseChar(RuntimeState* runtime_state, uint16_t ch) {
  unibrow::uchar chars[unibrow::ToUppercase::kMaxWidth];
  int char_length = runtime_state->to_upper_mapping()->get(ch, 0, chars);
  return char_length == 0;
}


static Object* Runtime_NumberToString(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);

  Object* number = args[0];
  RUNTIME_ASSERT(number->IsNumber());

  return isolate->heap()->NumberToString(number);
}


static Object* Runtime_NumberToStringSkipCache(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);

  Object* number = args[0];
  RUNTIME_ASSERT(number->IsNumber());

  return isolate->heap()->NumberToString(number, false);
}


static Object* Runtime_NumberToInteger(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);

  CONVERT_DOUBLE_CHECKED(number, args[0]);

  // We do not include 0 so that we don't have to treat +0 / -0 cases.
  if (number > 0 && number <= Smi::kMaxValue) {
    return Smi::FromInt(static_cast<int>(number));
  }
  return isolate->heap()->NumberFromDouble(DoubleToInteger(number));
}


static Object* Runtime_NumberToIntegerMapMinusZero(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);

  CONVERT_DOUBLE_CHECKED(number, args[0]);

  // We do not include 0 so that we don't have to treat +0 / -0 cases.
  if (number > 0 && number <= Smi::kMaxValue) {
    return Smi::FromInt(static_cast<int>(number));
  }

  double double_value = DoubleToInteger(number);
  // Map both -0 and +0 to +0.
  if (double_value == 0) double_value = 0;

  return isolate->heap()->NumberFromDouble(double_value);
}


static Object* Runtime_NumberToJSUint32(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);

  CONVERT_NUMBER_CHECKED(int32_t, number, Uint32, args[0]);
  return isolate->heap()->NumberFromUint32(number);
}


static Object* Runtime_NumberToJSInt32(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);

  CONVERT_DOUBLE_CHECKED(number, args[0]);

  // We do not include 0 so that we don't have to treat +0 / -0 cases.
  if (number > 0 && number <= Smi::kMaxValue) {
    return Smi::FromInt(static_cast<int>(number));
  }
  return isolate->heap()->NumberFromInt32(DoubleToInt32(number));
}


// Converts a Number to a Smi, if possible. Returns NaN if the number is not
// a small integer.
static Object* Runtime_NumberToSmi(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);

  Object* obj = args[0];
  if (obj->IsSmi()) {
    return obj;
  }
  if (obj->IsHeapNumber()) {
    double value = HeapNumber::cast(obj)->value();
    int int_value = FastD2I(value);
    if (value == FastI2D(int_value) && Smi::IsValid(int_value)) {
      return Smi::FromInt(int_value);
    }
  }
  return isolate->heap()->nan_value();
}


static Object* Runtime_NumberAdd(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_DOUBLE_CHECKED(x, args[0]);
  CONVERT_DOUBLE_CHECKED(y, args[1]);
  return isolate->heap()->NumberFromDouble(x + y);
}


static Object* Runtime_NumberSub(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_DOUBLE_CHECKED(x, args[0]);
  CONVERT_DOUBLE_CHECKED(y, args[1]);
  return isolate->heap()->NumberFromDouble(x - y);
}


static Object* Runtime_NumberMul(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_DOUBLE_CHECKED(x, args[0]);
  CONVERT_DOUBLE_CHECKED(y, args[1]);
  return isolate->heap()->NumberFromDouble(x * y);
}


static Object* Runtime_NumberUnaryMinus(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);

  CONVERT_DOUBLE_CHECKED(x, args[0]);
  return isolate->heap()->NumberFromDouble(-x);
}


static Object* Runtime_NumberAlloc(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 0);

  return isolate->heap()->NumberFromDouble(9876543210.0);
}


static Object* Runtime_NumberDiv(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_DOUBLE_CHECKED(x, args[0]);
  CONVERT_DOUBLE_CHECKED(y, args[1]);
  return isolate->heap()->NumberFromDouble(x / y);
}


static Object* Runtime_NumberMod(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_DOUBLE_CHECKED(x, args[0]);
  CONVERT_DOUBLE_CHECKED(y, args[1]);

  x = modulo(x, y);
  // NumberFromDouble may return a Smi instead of a Number object
  return isolate->heap()->NumberFromDouble(x);
}


static Object* Runtime_StringAdd(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);
  CONVERT_CHECKED(String, str1, args[0]);
  CONVERT_CHECKED(String, str2, args[1]);
  isolate->counters()->string_add_runtime()->Increment();
  return isolate->heap()->AllocateConsString(str1, str2);
}


template <typename sinkchar>
static inline void StringBuilderConcatHelper(String* special,
                                             sinkchar* sink,
                                             FixedArray* fixed_array,
                                             int array_length) {
  int position = 0;
  for (int i = 0; i < array_length; i++) {
    Object* element = fixed_array->get(i);
    if (element->IsSmi()) {
      // Smi encoding of position and length.
      int encoded_slice = Smi::cast(element)->value();
      int pos;
      int len;
      if (encoded_slice > 0) {
        // Position and length encoded in one smi.
        pos = StringBuilderSubstringPosition::decode(encoded_slice);
        len = StringBuilderSubstringLength::decode(encoded_slice);
      } else {
        // Position and length encoded in two smis.
        Object* obj = fixed_array->get(++i);
        ASSERT(obj->IsSmi());
        pos = Smi::cast(obj)->value();
        len = -encoded_slice;
      }
      String::WriteToFlat(special,
                          sink + position,
                          pos,
                          pos + len);
      position += len;
    } else {
      String* string = String::cast(element);
      int element_length = string->length();
      String::WriteToFlat(string, sink + position, 0, element_length);
      position += element_length;
    }
  }
}


static Object* Runtime_StringBuilderConcat(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 3);
  CONVERT_CHECKED(JSArray, array, args[0]);
  if (!args[1]->IsSmi()) {
    isolate->context()->mark_out_of_memory();
    return Failure::OutOfMemoryException();
  }
  int array_length = Smi::cast(args[1])->value();
  CONVERT_CHECKED(String, special, args[2]);

  // This assumption is used by the slice encoding in one or two smis.
  ASSERT(Smi::kMaxValue >= String::kMaxLength);

  int special_length = special->length();
  if (!array->HasFastElements()) {
    return isolate->Throw(isolate->heap()->illegal_argument_symbol());
  }
  FixedArray* fixed_array = FixedArray::cast(array->elements());
  if (fixed_array->length() < array_length) {
    array_length = fixed_array->length();
  }

  if (array_length == 0) {
    return isolate->heap()->empty_string();
  } else if (array_length == 1) {
    Object* first = fixed_array->get(0);
    if (first->IsString()) return first;
  }

  bool ascii = special->HasOnlyAsciiChars();
  int position = 0;
  for (int i = 0; i < array_length; i++) {
    int increment = 0;
    Object* elt = fixed_array->get(i);
    if (elt->IsSmi()) {
      // Smi encoding of position and length.
      int smi_value = Smi::cast(elt)->value();
      int pos;
      int len;
      if (smi_value > 0) {
        // Position and length encoded in one smi.
        pos = StringBuilderSubstringPosition::decode(smi_value);
        len = StringBuilderSubstringLength::decode(smi_value);
      } else {
        // Position and length encoded in two smis.
        len = -smi_value;
        // Get the position and check that it is a positive smi.
        i++;
        if (i >= array_length) {
          return isolate->Throw(isolate->heap()->illegal_argument_symbol());
        }
        Object* next_smi = fixed_array->get(i);
        if (!next_smi->IsSmi()) {
          return isolate->Throw(isolate->heap()->illegal_argument_symbol());
        }
        pos = Smi::cast(next_smi)->value();
        if (pos < 0) {
          return isolate->Throw(isolate->heap()->illegal_argument_symbol());
        }
      }
      ASSERT(pos >= 0);
      ASSERT(len >= 0);
      if (pos > special_length || len > special_length - pos) {
        return isolate->Throw(isolate->heap()->illegal_argument_symbol());
      }
      increment = len;
    } else if (elt->IsString()) {
      String* element = String::cast(elt);
      int element_length = element->length();
      increment = element_length;
      if (ascii && !element->HasOnlyAsciiChars()) {
        ascii = false;
      }
    } else {
      return isolate->Throw(isolate->heap()->illegal_argument_symbol());
    }
    if (increment > String::kMaxLength - position) {
      isolate->context()->mark_out_of_memory();
      return Failure::OutOfMemoryException();
    }
    position += increment;
  }

  int length = position;
  Object* object;

  if (ascii) {
    object = isolate->heap()->AllocateRawAsciiString(length);
    if (object->IsFailure()) return object;
    SeqAsciiString* answer = SeqAsciiString::cast(object);
    StringBuilderConcatHelper(special,
                              answer->GetChars(),
                              fixed_array,
                              array_length);
    return answer;
  } else {
    object = isolate->heap()->AllocateRawTwoByteString(length);
    if (object->IsFailure()) return object;
    SeqTwoByteString* answer = SeqTwoByteString::cast(object);
    StringBuilderConcatHelper(special,
                              answer->GetChars(),
                              fixed_array,
                              array_length);
    return answer;
  }
}


static Object* Runtime_NumberOr(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_NUMBER_CHECKED(int32_t, x, Int32, args[0]);
  CONVERT_NUMBER_CHECKED(int32_t, y, Int32, args[1]);
  return isolate->heap()->NumberFromInt32(x | y);
}


static Object* Runtime_NumberAnd(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_NUMBER_CHECKED(int32_t, x, Int32, args[0]);
  CONVERT_NUMBER_CHECKED(int32_t, y, Int32, args[1]);
  return isolate->heap()->NumberFromInt32(x & y);
}


static Object* Runtime_NumberXor(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_NUMBER_CHECKED(int32_t, x, Int32, args[0]);
  CONVERT_NUMBER_CHECKED(int32_t, y, Int32, args[1]);
  return isolate->heap()->NumberFromInt32(x ^ y);
}


static Object* Runtime_NumberNot(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);

  CONVERT_NUMBER_CHECKED(int32_t, x, Int32, args[0]);
  return isolate->heap()->NumberFromInt32(~x);
}


static Object* Runtime_NumberShl(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_NUMBER_CHECKED(int32_t, x, Int32, args[0]);
  CONVERT_NUMBER_CHECKED(int32_t, y, Int32, args[1]);
  return isolate->heap()->NumberFromInt32(x << (y & 0x1f));
}


static Object* Runtime_NumberShr(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_NUMBER_CHECKED(uint32_t, x, Uint32, args[0]);
  CONVERT_NUMBER_CHECKED(int32_t, y, Int32, args[1]);
  return isolate->heap()->NumberFromUint32(x >> (y & 0x1f));
}


static Object* Runtime_NumberSar(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_NUMBER_CHECKED(int32_t, x, Int32, args[0]);
  CONVERT_NUMBER_CHECKED(int32_t, y, Int32, args[1]);
  return isolate->heap()->NumberFromInt32(ArithmeticShiftRight(x, y & 0x1f));
}


static Object* Runtime_NumberEquals(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_DOUBLE_CHECKED(x, args[0]);
  CONVERT_DOUBLE_CHECKED(y, args[1]);
  if (isnan(x)) return Smi::FromInt(NOT_EQUAL);
  if (isnan(y)) return Smi::FromInt(NOT_EQUAL);
  if (x == y) return Smi::FromInt(EQUAL);
  Object* result;
  if ((fpclassify(x) == FP_ZERO) && (fpclassify(y) == FP_ZERO)) {
    result = Smi::FromInt(EQUAL);
  } else {
    result = Smi::FromInt(NOT_EQUAL);
  }
  return result;
}


static Object* Runtime_StringEquals(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_CHECKED(String, x, args[0]);
  CONVERT_CHECKED(String, y, args[1]);

  bool not_equal = !x->Equals(y);
  // This is slightly convoluted because the value that signifies
  // equality is 0 and inequality is 1 so we have to negate the result
  // from String::Equals.
  ASSERT(not_equal == 0 || not_equal == 1);
  STATIC_CHECK(EQUAL == 0);
  STATIC_CHECK(NOT_EQUAL == 1);
  return Smi::FromInt(not_equal);
}


static Object* Runtime_NumberCompare(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 3);

  CONVERT_DOUBLE_CHECKED(x, args[0]);
  CONVERT_DOUBLE_CHECKED(y, args[1]);
  if (isnan(x) || isnan(y)) return args[2];
  if (x == y) return Smi::FromInt(EQUAL);
  if (isless(x, y)) return Smi::FromInt(LESS);
  return Smi::FromInt(GREATER);
}


// Compare two Smis as if they were converted to strings and then
// compared lexicographically.
static Object* Runtime_SmiLexicographicCompare(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  // Extract the integer values from the Smis.
  CONVERT_CHECKED(Smi, x, args[0]);
  CONVERT_CHECKED(Smi, y, args[1]);
  int x_value = x->value();
  int y_value = y->value();

  // If the integers are equal so are the string representations.
  if (x_value == y_value) return Smi::FromInt(EQUAL);

  // If one of the integers are zero the normal integer order is the
  // same as the lexicographic order of the string representations.
  if (x_value == 0 || y_value == 0) return Smi::FromInt(x_value - y_value);

  // If only one of the integers is negative the negative number is
  // smallest because the char code of '-' is less than the char code
  // of any digit.  Otherwise, we make both values positive.
  if (x_value < 0 || y_value < 0) {
    if (y_value >= 0) return Smi::FromInt(LESS);
    if (x_value >= 0) return Smi::FromInt(GREATER);
    x_value = -x_value;
    y_value = -y_value;
  }

  // Arrays for the individual characters of the two Smis.  Smis are
  // 31 bit integers and 10 decimal digits are therefore enough.
  int* x_elms = isolate->runtime_state()->smi_lexicographic_compare_x_elms();
  int* y_elms = isolate->runtime_state()->smi_lexicographic_compare_y_elms();


  // Convert the integers to arrays of their decimal digits.
  int x_index = 0;
  int y_index = 0;
  while (x_value > 0) {
    x_elms[x_index++] = x_value % 10;
    x_value /= 10;
  }
  while (y_value > 0) {
    y_elms[y_index++] = y_value % 10;
    y_value /= 10;
  }

  // Loop through the arrays of decimal digits finding the first place
  // where they differ.
  while (--x_index >= 0 && --y_index >= 0) {
    int diff = x_elms[x_index] - y_elms[y_index];
    if (diff != 0) return Smi::FromInt(diff);
  }

  // If one array is a suffix of the other array, the longest array is
  // the representation of the largest of the Smis in the
  // lexicographic ordering.
  return Smi::FromInt(x_index - y_index);
}


static Object* StringInputBufferCompare(RuntimeState* state, String* x,
                                        String* y) {
  StringInputBuffer& bufx = *state->string_input_buffer_compare_bufx();
  StringInputBuffer& bufy = *state->string_input_buffer_compare_bufy();
  bufx.Reset(x);
  bufy.Reset(y);
  while (bufx.has_more() && bufy.has_more()) {
    int d = bufx.GetNext() - bufy.GetNext();
    if (d < 0) return Smi::FromInt(LESS);
    else if (d > 0) return Smi::FromInt(GREATER);
  }

  // x is (non-trivial) prefix of y:
  if (bufy.has_more()) return Smi::FromInt(LESS);
  // y is prefix of x:
  return Smi::FromInt(bufx.has_more() ? GREATER : EQUAL);
}


static Object* FlatStringCompare(String* x, String* y) {
  ASSERT(x->IsFlat());
  ASSERT(y->IsFlat());
  Object* equal_prefix_result = Smi::FromInt(EQUAL);
  int prefix_length = x->length();
  if (y->length() < prefix_length) {
    prefix_length = y->length();
    equal_prefix_result = Smi::FromInt(GREATER);
  } else if (y->length() > prefix_length) {
    equal_prefix_result = Smi::FromInt(LESS);
  }
  int r;
  if (x->IsAsciiRepresentation()) {
    Vector<const char> x_chars = x->ToAsciiVector();
    if (y->IsAsciiRepresentation()) {
      Vector<const char> y_chars = y->ToAsciiVector();
      r = CompareChars(x_chars.start(), y_chars.start(), prefix_length);
    } else {
      Vector<const uc16> y_chars = y->ToUC16Vector();
      r = CompareChars(x_chars.start(), y_chars.start(), prefix_length);
    }
  } else {
    Vector<const uc16> x_chars = x->ToUC16Vector();
    if (y->IsAsciiRepresentation()) {
      Vector<const char> y_chars = y->ToAsciiVector();
      r = CompareChars(x_chars.start(), y_chars.start(), prefix_length);
    } else {
      Vector<const uc16> y_chars = y->ToUC16Vector();
      r = CompareChars(x_chars.start(), y_chars.start(), prefix_length);
    }
  }
  Object* result;
  if (r == 0) {
    result = equal_prefix_result;
  } else {
    result = (r < 0) ? Smi::FromInt(LESS) : Smi::FromInt(GREATER);
  }
  ASSERT(result ==
      StringInputBufferCompare(Isolate::Current()->runtime_state(), x, y));
  return result;
}


static Object* Runtime_StringCompare(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_CHECKED(String, x, args[0]);
  CONVERT_CHECKED(String, y, args[1]);

  isolate->counters()->string_compare_runtime()->Increment();

  // A few fast case tests before we flatten.
  if (x == y) return Smi::FromInt(EQUAL);
  if (y->length() == 0) {
    if (x->length() == 0) return Smi::FromInt(EQUAL);
    return Smi::FromInt(GREATER);
  } else if (x->length() == 0) {
    return Smi::FromInt(LESS);
  }

  int d = x->Get(0) - y->Get(0);
  if (d < 0) return Smi::FromInt(LESS);
  else if (d > 0) return Smi::FromInt(GREATER);

  Object* obj = isolate->heap()->PrepareForCompare(x);
  if (obj->IsFailure()) return obj;
  obj = isolate->heap()->PrepareForCompare(y);
  if (obj->IsFailure()) return obj;

  return (x->IsFlat() && y->IsFlat()) ? FlatStringCompare(x, y)
      : StringInputBufferCompare(isolate->runtime_state(), x, y);
}


static Object* Runtime_Math_acos(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);
  isolate->counters()->math_acos()->Increment();

  CONVERT_DOUBLE_CHECKED(x, args[0]);
  return isolate->transcendental_cache()->Get(TranscendentalCache::ACOS, x);
}


static Object* Runtime_Math_asin(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);
  isolate->counters()->math_asin()->Increment();

  CONVERT_DOUBLE_CHECKED(x, args[0]);
  return isolate->transcendental_cache()->Get(TranscendentalCache::ASIN, x);
}


static Object* Runtime_Math_atan(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);
  isolate->counters()->math_atan()->Increment();

  CONVERT_DOUBLE_CHECKED(x, args[0]);
  return isolate->transcendental_cache()->Get(TranscendentalCache::ATAN, x);
}


static const double kPiDividedBy4 = 0.78539816339744830962;


static Object* Runtime_Math_atan2(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);
  isolate->counters()->math_atan2()->Increment();

  CONVERT_DOUBLE_CHECKED(x, args[0]);
  CONVERT_DOUBLE_CHECKED(y, args[1]);
  double result;
  if (isinf(x) && isinf(y)) {
    // Make sure that the result in case of two infinite arguments
    // is a multiple of Pi / 4. The sign of the result is determined
    // by the first argument (x) and the sign of the second argument
    // determines the multiplier: one or three.
    int multiplier = (x < 0) ? -1 : 1;
    if (y < 0) multiplier *= 3;
    result = multiplier * kPiDividedBy4;
  } else {
    result = atan2(x, y);
  }
  return isolate->heap()->AllocateHeapNumber(result);
}


static Object* Runtime_Math_ceil(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);
  isolate->counters()->math_ceil()->Increment();

  CONVERT_DOUBLE_CHECKED(x, args[0]);
  return isolate->heap()->NumberFromDouble(ceiling(x));
}


static Object* Runtime_Math_cos(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);
  isolate->counters()->math_cos()->Increment();

  CONVERT_DOUBLE_CHECKED(x, args[0]);
  return isolate->transcendental_cache()->Get(TranscendentalCache::COS, x);
}


static Object* Runtime_Math_exp(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);
  isolate->counters()->math_exp()->Increment();

  CONVERT_DOUBLE_CHECKED(x, args[0]);
  return isolate->transcendental_cache()->Get(TranscendentalCache::EXP, x);
}


static Object* Runtime_Math_floor(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);
  isolate->counters()->math_floor()->Increment();

  CONVERT_DOUBLE_CHECKED(x, args[0]);
  return isolate->heap()->NumberFromDouble(floor(x));
}


static Object* Runtime_Math_log(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);
  isolate->counters()->math_log()->Increment();

  CONVERT_DOUBLE_CHECKED(x, args[0]);
  return isolate->transcendental_cache()->Get(TranscendentalCache::LOG, x);
}


// Helper function to compute x^y, where y is known to be an
// integer. Uses binary decomposition to limit the number of
// multiplications; see the discussion in "Hacker's Delight" by Henry
// S. Warren, Jr., figure 11-6, page 213.
static double powi(double x, int y) {
  ASSERT(y != kMinInt);
  unsigned n = (y < 0) ? -y : y;
  double m = x;
  double p = 1;
  while (true) {
    if ((n & 1) != 0) p *= m;
    n >>= 1;
    if (n == 0) {
      if (y < 0) {
        // Unfortunately, we have to be careful when p has reached
        // infinity in the computation, because sometimes the higher
        // internal precision in the pow() implementation would have
        // given us a finite p. This happens very rarely.
        double result = 1.0 / p;
        return (result == 0 && isinf(p))
            ? pow(x, static_cast<double>(y))  // Avoid pow(double, int).
            : result;
      } else {
        return p;
      }
    }
    m *= m;
  }
}


static Object* Runtime_Math_pow(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);
  isolate->counters()->math_pow()->Increment();

  CONVERT_DOUBLE_CHECKED(x, args[0]);

  // If the second argument is a smi, it is much faster to call the
  // custom powi() function than the generic pow().
  if (args[1]->IsSmi()) {
    int y = Smi::cast(args[1])->value();
    return isolate->heap()->NumberFromDouble(powi(x, y));
  }

  CONVERT_DOUBLE_CHECKED(y, args[1]);

  if (!isinf(x)) {
    if (y == 0.5) {
      // It's not uncommon to use Math.pow(x, 0.5) to compute the
      // square root of a number. To speed up such computations, we
      // explictly check for this case and use the sqrt() function
      // which is faster than pow().
      return isolate->heap()->AllocateHeapNumber(sqrt(x));
    } else if (y == -0.5) {
      // Optimized using Math.pow(x, -0.5) == 1 / Math.pow(x, 0.5).
      return isolate->heap()->AllocateHeapNumber(1.0 / sqrt(x));
    }
  }

  if (y == 0) {
    return Smi::FromInt(1);
  } else if (isnan(y) || ((x == 1 || x == -1) && isinf(y))) {
    return isolate->heap()->nan_value();
  } else {
    return isolate->heap()->AllocateHeapNumber(pow(x, y));
  }
}

// Fast version of Math.pow if we know that y is not an integer and
// y is not -0.5 or 0.5. Used as slowcase from codegen.
static Object* Runtime_Math_pow_cfunction(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);
  CONVERT_DOUBLE_CHECKED(x, args[0]);
  CONVERT_DOUBLE_CHECKED(y, args[1]);
  if (y == 0) {
      return Smi::FromInt(1);
  } else if (isnan(y) || ((x == 1 || x == -1) && isinf(y))) {
      return isolate->heap()->nan_value();
  } else {
      return isolate->heap()->AllocateHeapNumber(pow(x, y));
  }
}


static Object* Runtime_RoundNumber(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);
  isolate->counters()->math_round()->Increment();

  if (!args[0]->IsHeapNumber()) {
    // Must be smi. Return the argument unchanged for all the other types
    // to make fuzz-natives test happy.
    return args[0];
  }

  HeapNumber* number = reinterpret_cast<HeapNumber*>(args[0]);

  double value = number->value();
  int exponent = number->get_exponent();
  int sign = number->get_sign();

  // We compare with kSmiValueSize - 3 because (2^30 - 0.1) has exponent 29 and
  // should be rounded to 2^30, which is not smi.
  if (!sign && exponent <= kSmiValueSize - 3) {
    return Smi::FromInt(static_cast<int>(value + 0.5));
  }

  // If the magnitude is big enough, there's no place for fraction part. If we
  // try to add 0.5 to this number, 1.0 will be added instead.
  if (exponent >= 52) {
    return number;
  }

  if (sign && value >= -0.5) return isolate->heap()->minus_zero_value();

  // Do not call NumberFromDouble() to avoid extra checks.
  return isolate->heap()->AllocateHeapNumber(floor(value + 0.5));
}


static Object* Runtime_Math_sin(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);
  isolate->counters()->math_sin()->Increment();

  CONVERT_DOUBLE_CHECKED(x, args[0]);
  return isolate->transcendental_cache()->Get(TranscendentalCache::SIN, x);
}


static Object* Runtime_Math_sqrt(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);
  isolate->counters()->math_sqrt()->Increment();

  CONVERT_DOUBLE_CHECKED(x, args[0]);
  return isolate->heap()->AllocateHeapNumber(sqrt(x));
}


static Object* Runtime_Math_tan(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);
  isolate->counters()->math_tan()->Increment();

  CONVERT_DOUBLE_CHECKED(x, args[0]);
  return isolate->transcendental_cache()->Get(TranscendentalCache::TAN, x);
}


static int MakeDay(int year, int month, int day) {
  static const int day_from_month[] = {0, 31, 59, 90, 120, 151,
                                       181, 212, 243, 273, 304, 334};
  static const int day_from_month_leap[] = {0, 31, 60, 91, 121, 152,
                                            182, 213, 244, 274, 305, 335};

  year += month / 12;
  month %= 12;
  if (month < 0) {
    year--;
    month += 12;
  }

  ASSERT(month >= 0);
  ASSERT(month < 12);

  // year_delta is an arbitrary number such that:
  // a) year_delta = -1 (mod 400)
  // b) year + year_delta > 0 for years in the range defined by
  //    ECMA 262 - 15.9.1.1, i.e. upto 100,000,000 days on either side of
  //    Jan 1 1970. This is required so that we don't run into integer
  //    division of negative numbers.
  // c) there shouldn't be an overflow for 32-bit integers in the following
  //    operations.
  static const int year_delta = 399999;
  static const int base_day = 365 * (1970 + year_delta) +
                              (1970 + year_delta) / 4 -
                              (1970 + year_delta) / 100 +
                              (1970 + year_delta) / 400;

  int year1 = year + year_delta;
  int day_from_year = 365 * year1 +
                      year1 / 4 -
                      year1 / 100 +
                      year1 / 400 -
                      base_day;

  if (year % 4 || (year % 100 == 0 && year % 400 != 0)) {
    return day_from_year + day_from_month[month] + day - 1;
  }

  return day_from_year + day_from_month_leap[month] + day - 1;
}


static Object* Runtime_DateMakeDay(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 3);

  CONVERT_SMI_CHECKED(year, args[0]);
  CONVERT_SMI_CHECKED(month, args[1]);
  CONVERT_SMI_CHECKED(date, args[2]);

  return Smi::FromInt(MakeDay(year, month, date));
}


static const int kDays4Years[] = {0, 365, 2 * 365, 3 * 365 + 1};
static const int kDaysIn4Years = 4 * 365 + 1;
static const int kDaysIn100Years = 25 * kDaysIn4Years - 1;
static const int kDaysIn400Years = 4 * kDaysIn100Years + 1;
static const int kDays1970to2000 = 30 * 365 + 7;
static const int kDaysOffset = 1000 * kDaysIn400Years + 5 * kDaysIn400Years -
                               kDays1970to2000;
static const int kYearsOffset = 400000;

static const char kDayInYear[] = {
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,

      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,

      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,

      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31};

static const char kMonthInYear[] = {
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1,
      2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
      2, 2, 2, 2, 2, 2,
      3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
      3, 3, 3, 3, 3,
      4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
      4, 4, 4, 4, 4, 4,
      5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
      5, 5, 5, 5, 5,
      6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
      6, 6, 6, 6, 6, 6,
      7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
      7, 7, 7, 7, 7, 7,
      8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
      8, 8, 8, 8, 8,
      9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
      9, 9, 9, 9, 9, 9,
      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
      11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
      11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,

      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1,
      2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
      2, 2, 2, 2, 2, 2,
      3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
      3, 3, 3, 3, 3,
      4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
      4, 4, 4, 4, 4, 4,
      5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
      5, 5, 5, 5, 5,
      6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
      6, 6, 6, 6, 6, 6,
      7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
      7, 7, 7, 7, 7, 7,
      8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
      8, 8, 8, 8, 8,
      9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
      9, 9, 9, 9, 9, 9,
      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
      11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
      11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,

      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1,
      2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
      2, 2, 2, 2, 2, 2,
      3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
      3, 3, 3, 3, 3,
      4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
      4, 4, 4, 4, 4, 4,
      5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
      5, 5, 5, 5, 5,
      6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
      6, 6, 6, 6, 6, 6,
      7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
      7, 7, 7, 7, 7, 7,
      8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
      8, 8, 8, 8, 8,
      9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
      9, 9, 9, 9, 9, 9,
      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
      11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
      11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,

      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1,
      2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
      2, 2, 2, 2, 2, 2,
      3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
      3, 3, 3, 3, 3,
      4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
      4, 4, 4, 4, 4, 4,
      5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
      5, 5, 5, 5, 5,
      6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
      6, 6, 6, 6, 6, 6,
      7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
      7, 7, 7, 7, 7, 7,
      8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
      8, 8, 8, 8, 8,
      9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
      9, 9, 9, 9, 9, 9,
      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
      11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
      11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11};


// This function works for dates from 1970 to 2099.
static inline void DateYMDFromTimeAfter1970(int date,
                                            int& year, int& month, int& day) {
#ifdef DEBUG
  int save_date = date;  // Need this for ASSERT in the end.
#endif

  year = 1970 + (4 * date + 2) / kDaysIn4Years;
  date %= kDaysIn4Years;

  month = kMonthInYear[date];
  day = kDayInYear[date];

  ASSERT(MakeDay(year, month, day) == save_date);
}


static inline void DateYMDFromTimeSlow(int date,
                                       int& year, int& month, int& day) {
#ifdef DEBUG
  int save_date = date;  // Need this for ASSERT in the end.
#endif

  date += kDaysOffset;
  year = 400 * (date / kDaysIn400Years) - kYearsOffset;
  date %= kDaysIn400Years;

  ASSERT(MakeDay(year, 0, 1) + date == save_date);

  date--;
  int yd1 = date / kDaysIn100Years;
  date %= kDaysIn100Years;
  year += 100 * yd1;

  date++;
  int yd2 = date / kDaysIn4Years;
  date %= kDaysIn4Years;
  year += 4 * yd2;

  date--;
  int yd3 = date / 365;
  date %= 365;
  year += yd3;

  bool is_leap = (!yd1 || yd2) && !yd3;

  ASSERT(date >= -1);
  ASSERT(is_leap || (date >= 0));
  ASSERT((date < 365) || (is_leap && (date < 366)));
  ASSERT(is_leap == ((year % 4 == 0) && (year % 100 || (year % 400 == 0))));
  ASSERT(is_leap || ((MakeDay(year, 0, 1) + date) == save_date));
  ASSERT(!is_leap || ((MakeDay(year, 0, 1) + date + 1) == save_date));

  if (is_leap) {
    day = kDayInYear[2*365 + 1 + date];
    month = kMonthInYear[2*365 + 1 + date];
  } else {
    day = kDayInYear[date];
    month = kMonthInYear[date];
  }

  ASSERT(MakeDay(year, month, day) == save_date);
}


static inline void DateYMDFromTime(int date,
                                   int& year, int& month, int& day) {
  if (date >= 0 && date < 32 * kDaysIn4Years) {
    DateYMDFromTimeAfter1970(date, year, month, day);
  } else {
    DateYMDFromTimeSlow(date, year, month, day);
  }
}


static Object* Runtime_DateYMDFromTime(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_DOUBLE_CHECKED(t, args[0]);
  CONVERT_CHECKED(JSArray, res_array, args[1]);

  int year, month, day;
  DateYMDFromTime(static_cast<int>(floor(t / 86400000)), year, month, day);

  RUNTIME_ASSERT(res_array->elements()->map() ==
                 isolate->heap()->fixed_array_map());
  FixedArray* elms = FixedArray::cast(res_array->elements());
  RUNTIME_ASSERT(elms->length() == 3);

  elms->set(0, Smi::FromInt(year));
  elms->set(1, Smi::FromInt(month));
  elms->set(2, Smi::FromInt(day));

  return isolate->heap()->undefined_value();
}


static Object* Runtime_NewArgumentsFast(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 3);

  JSFunction* callee = JSFunction::cast(args[0]);
  Object** parameters = reinterpret_cast<Object**>(args[1]);
  const int length = Smi::cast(args[2])->value();

  Object* result = isolate->heap()->AllocateArgumentsObject(callee, length);
  if (result->IsFailure()) return result;
  // Allocate the elements if needed.
  if (length > 0) {
    // Allocate the fixed array.
    Object* obj = isolate->heap()->AllocateRawFixedArray(length);
    if (obj->IsFailure()) return obj;

    AssertNoAllocation no_gc;
    FixedArray* array = reinterpret_cast<FixedArray*>(obj);
    array->set_map(isolate->heap()->fixed_array_map());
    array->set_length(length);

    WriteBarrierMode mode = array->GetWriteBarrierMode(no_gc);
    for (int i = 0; i < length; i++) {
      array->set(i, *--parameters, mode);
    }
    JSObject::cast(result)->set_elements(FixedArray::cast(obj));
  }
  return result;
}


static Object* Runtime_NewClosure(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_CHECKED(Context, context, 0);
  CONVERT_ARG_CHECKED(SharedFunctionInfo, shared, 1);

  PretenureFlag pretenure = (context->global_context() == *context)
      ? TENURED       // Allocate global closures in old space.
      : NOT_TENURED;  // Allocate local closures in new space.
  Handle<JSFunction> result =
      isolate->factory()->NewFunctionFromSharedFunctionInfo(shared,
                                                            context,
                                                            pretenure);
  return *result;
}

static Object* Runtime_NewObjectFromBound(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_CHECKED(JSFunction, function, 0);
  CONVERT_ARG_CHECKED(JSArray, params, 1);

  RUNTIME_ASSERT(params->HasFastElements());
  FixedArray* fixed = FixedArray::cast(params->elements());

  int fixed_length = Smi::cast(params->length())->value();
  SmartPointer<Object**> param_data(NewArray<Object**>(fixed_length));
  for (int i = 0; i < fixed_length; i++) {
    Handle<Object> val(fixed->get(i), isolate);
    param_data[i] = val.location();
  }

  bool exception = false;
  Handle<Object> result = Execution::New(
      function, fixed_length, *param_data, &exception);
  if (exception) {
      return Failure::Exception();
  }
  ASSERT(!result.is_null());
  return *result;
}


static void TrySettingInlineConstructStub(Isolate* isolate,
                                          Handle<JSFunction> function) {
  Handle<Object> prototype = isolate->factory()->null_value();
  if (function->has_instance_prototype()) {
    prototype = Handle<Object>(function->instance_prototype(), isolate);
  }
  if (function->shared()->CanGenerateInlineConstructor(*prototype)) {
    ConstructStubCompiler compiler;
    Object* code = compiler.CompileConstructStub(function->shared());
    if (!code->IsFailure()) {
      function->shared()->set_construct_stub(Code::cast(code));
    }
  }
}


static Object* Runtime_NewObject(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);

  Handle<Object> constructor = args.at<Object>(0);

  // If the constructor isn't a proper function we throw a type error.
  if (!constructor->IsJSFunction()) {
    Vector< Handle<Object> > arguments = HandleVector(&constructor, 1);
    Handle<Object> type_error =
        isolate->factory()->NewTypeError("not_constructor", arguments);
    return isolate->Throw(*type_error);
  }

  Handle<JSFunction> function = Handle<JSFunction>::cast(constructor);

  // If function should not have prototype, construction is not allowed. In this
  // case generated code bailouts here, since function has no initial_map.
  if (!function->should_have_prototype()) {
    Vector< Handle<Object> > arguments = HandleVector(&constructor, 1);
    Handle<Object> type_error =
        isolate->factory()->NewTypeError("not_constructor", arguments);
    return isolate->Throw(*type_error);
  }

#ifdef ENABLE_DEBUGGER_SUPPORT
  Debug* debug = isolate->debug();
  // Handle stepping into constructors if step into is active.
  if (debug->StepInActive()) {
    debug->HandleStepIn(function, Handle<Object>::null(), 0, true);
  }
#endif

  if (function->has_initial_map()) {
    if (function->initial_map()->instance_type() == JS_FUNCTION_TYPE) {
      // The 'Function' function ignores the receiver object when
      // called using 'new' and creates a new JSFunction object that
      // is returned.  The receiver object is only used for error
      // reporting if an error occurs when constructing the new
      // JSFunction. FACTORY->NewJSObject() should not be used to
      // allocate JSFunctions since it does not properly initialize
      // the shared part of the function. Since the receiver is
      // ignored anyway, we use the global object as the receiver
      // instead of a new JSFunction object. This way, errors are
      // reported the same way whether or not 'Function' is called
      // using 'new'.
      return isolate->context()->global();
    }
  }

  // The function should be compiled for the optimization hints to be available.
  Handle<SharedFunctionInfo> shared(function->shared());
  EnsureCompiled(shared, CLEAR_EXCEPTION);

  if (!function->has_initial_map() &&
      shared->IsInobjectSlackTrackingInProgress()) {
    // The tracking is already in progress for another function. We can only
    // track one initial_map at a time, so we force the completion before the
    // function is called as a constructor for the first time.
    shared->CompleteInobjectSlackTracking();
    TrySettingInlineConstructStub(isolate, function);
  }

  bool first_allocation = !shared->live_objects_may_exist();
  Handle<JSObject> result = isolate->factory()->NewJSObject(function);
  // Delay setting the stub if inobject slack tracking is in progress.
  if (first_allocation && !shared->IsInobjectSlackTrackingInProgress()) {
    TrySettingInlineConstructStub(isolate, function);
  }

  isolate->counters()->constructed_objects()->Increment();
  isolate->counters()->constructed_objects_runtime()->Increment();

  return *result;
}


static Object* Runtime_FinalizeInstanceSize(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);

  CONVERT_ARG_CHECKED(JSFunction, function, 0);
  function->shared()->CompleteInobjectSlackTracking();
  TrySettingInlineConstructStub(isolate, function);

  return isolate->heap()->undefined_value();
}


static Object* Runtime_LazyCompile(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);

  Handle<JSFunction> function = args.at<JSFunction>(0);
#ifdef DEBUG
  if (FLAG_trace_lazy && !function->shared()->is_compiled()) {
    PrintF("[lazy: ");
    function->shared()->name()->Print();
    PrintF("]\n");
  }
#endif

  // Compile the target function.  Here we compile using CompileLazyInLoop in
  // order to get the optimized version.  This helps code like delta-blue
  // that calls performance-critical routines through constructors.  A
  // constructor call doesn't use a CallIC, it uses a LoadIC followed by a
  // direct call.  Since the in-loop tracking takes place through CallICs
  // this means that things called through constructors are never known to
  // be in loops.  We compile them as if they are in loops here just in case.
  ASSERT(!function->is_compiled());
  if (!CompileLazyInLoop(function, KEEP_EXCEPTION)) {
    return Failure::Exception();
  }

  return function->code();
}


static Object* Runtime_GetFunctionDelegate(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  RUNTIME_ASSERT(!args[0]->IsJSFunction());
  return *Execution::GetFunctionDelegate(args.at<Object>(0));
}


static Object* Runtime_GetConstructorDelegate(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  RUNTIME_ASSERT(!args[0]->IsJSFunction());
  return *Execution::GetConstructorDelegate(args.at<Object>(0));
}


static Object* Runtime_NewContext(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);

  CONVERT_CHECKED(JSFunction, function, args[0]);
  int length = function->shared()->scope_info()->NumberOfContextSlots();
  Object* result = isolate->heap()->AllocateFunctionContext(length, function);
  if (result->IsFailure()) return result;

  isolate->set_context(Context::cast(result));

  return result;  // non-failure
}

static Object* PushContextHelper(Isolate* isolate,
                                 Object* object,
                                 bool is_catch_context) {
  // Convert the object to a proper JavaScript object.
  Object* js_object = object;
  if (!js_object->IsJSObject()) {
    js_object = js_object->ToObject();
    if (js_object->IsFailure()) {
      if (!Failure::cast(js_object)->IsInternalError()) return js_object;
      HandleScope scope(isolate);
      Handle<Object> handle(object, isolate);
      Handle<Object> result =
          isolate->factory()->NewTypeError("with_expression",
                                           HandleVector(&handle, 1));
      return isolate->Throw(*result);
    }
  }

  Object* result = isolate->heap()->AllocateWithContext(
      isolate->context(), JSObject::cast(js_object), is_catch_context);
  if (result->IsFailure()) return result;

  Context* context = Context::cast(result);
  isolate->set_context(context);

  return result;
}


static Object* Runtime_PushContext(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);
  return PushContextHelper(isolate, args[0], false);
}


static Object* Runtime_PushCatchContext(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);
  return PushContextHelper(isolate, args[0], true);
}


static Object* Runtime_LookupContext(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);

  CONVERT_ARG_CHECKED(Context, context, 0);
  CONVERT_ARG_CHECKED(String, name, 1);

  int index;
  PropertyAttributes attributes;
  ContextLookupFlags flags = FOLLOW_CHAINS;
  Handle<Object> holder =
      context->Lookup(name, flags, &index, &attributes);

  if (index < 0 && !holder.is_null()) {
    ASSERT(holder->IsJSObject());
    return *holder;
  }

  // No intermediate context found. Use global object by default.
  return isolate->context()->global();
}


// A mechanism to return a pair of Object pointers in registers (if possible).
// How this is achieved is calling convention-dependent.
// All currently supported x86 compiles uses calling conventions that are cdecl
// variants where a 64-bit value is returned in two 32-bit registers
// (edx:eax on ia32, r1:r0 on ARM).
// In AMD-64 calling convention a struct of two pointers is returned in rdx:rax.
// In Win64 calling convention, a struct of two pointers is returned in memory,
// allocated by the caller, and passed as a pointer in a hidden first parameter.
#ifdef V8_HOST_ARCH_64_BIT
struct ObjectPair {
  Object* x;
  Object* y;
};

static inline ObjectPair MakePair(Object* x, Object* y) {
  ObjectPair result = {x, y};
  // Pointers x and y returned in rax and rdx, in AMD-x64-abi.
  // In Win64 they are assigned to a hidden first argument.
  return result;
}
#else
typedef uint64_t ObjectPair;
static inline ObjectPair MakePair(Object* x, Object* y) {
  return reinterpret_cast<uint32_t>(x) |
      (reinterpret_cast<ObjectPair>(y) << 32);
}
#endif


static inline Object* Unhole(Heap* heap,
                             Object* x,
                             PropertyAttributes attributes) {
  ASSERT(!x->IsTheHole() || (attributes & READ_ONLY) != 0);
  USE(attributes);
  return x->IsTheHole() ? heap->undefined_value() : x;
}


static JSObject* ComputeReceiverForNonGlobal(Isolate* isolate,
                                             JSObject* holder) {
  ASSERT(!holder->IsGlobalObject());
  Context* top = isolate->context();
  // Get the context extension function.
  JSFunction* context_extension_function =
      top->global_context()->context_extension_function();
  // If the holder isn't a context extension object, we just return it
  // as the receiver. This allows arguments objects to be used as
  // receivers, but only if they are put in the context scope chain
  // explicitly via a with-statement.
  Object* constructor = holder->map()->constructor();
  if (constructor != context_extension_function) return holder;
  // Fall back to using the global object as the receiver if the
  // property turns out to be a local variable allocated in a context
  // extension object - introduced via eval.
  return top->global()->global_receiver();
}


static ObjectPair LoadContextSlotHelper(Arguments args,
                                        Isolate* isolate,
                                        bool throw_error) {
  HandleScope scope(isolate);
  ASSERT_EQ(2, args.length());

  if (!args[0]->IsContext() || !args[1]->IsString()) {
    return MakePair(isolate->ThrowIllegalOperation(), NULL);
  }
  Handle<Context> context = args.at<Context>(0);
  Handle<String> name = args.at<String>(1);

  int index;
  PropertyAttributes attributes;
  ContextLookupFlags flags = FOLLOW_CHAINS;
  Handle<Object> holder =
      context->Lookup(name, flags, &index, &attributes);

  // If the index is non-negative, the slot has been found in a local
  // variable or a parameter. Read it from the context object or the
  // arguments object.
  if (index >= 0) {
    // If the "property" we were looking for is a local variable or an
    // argument in a context, the receiver is the global object; see
    // ECMA-262, 3rd., 10.1.6 and 10.2.3.
    JSObject* receiver =
        isolate->context()->global()->global_receiver();
    Object* value = (holder->IsContext())
        ? Context::cast(*holder)->get(index)
        : JSObject::cast(*holder)->GetElement(index);
    return MakePair(Unhole(isolate->heap(), value, attributes), receiver);
  }

  // If the holder is found, we read the property from it.
  if (!holder.is_null() && holder->IsJSObject()) {
    ASSERT(Handle<JSObject>::cast(holder)->HasProperty(*name));
    JSObject* object = JSObject::cast(*holder);
    JSObject* receiver;
    if (object->IsGlobalObject()) {
      receiver = GlobalObject::cast(object)->global_receiver();
    } else if (context->is_exception_holder(*holder)) {
      receiver = isolate->context()->global()->global_receiver();
    } else {
      receiver = ComputeReceiverForNonGlobal(isolate, object);
    }
    // No need to unhole the value here. This is taken care of by the
    // GetProperty function.
    Object* value = object->GetProperty(*name);
    return MakePair(value, receiver);
  }

  if (throw_error) {
    // The property doesn't exist - throw exception.
    Handle<Object> reference_error =
        isolate->factory()->NewReferenceError("not_defined",
                                              HandleVector(&name, 1));
    return MakePair(isolate->Throw(*reference_error), NULL);
  } else {
    // The property doesn't exist - return undefined
    return MakePair(isolate->heap()->undefined_value(),
                    isolate->heap()->undefined_value());
  }
}


static ObjectPair Runtime_LoadContextSlot(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  return LoadContextSlotHelper(args, isolate, true);
}


static ObjectPair Runtime_LoadContextSlotNoReferenceError(
    RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  return LoadContextSlotHelper(args, isolate, false);
}


static Object* Runtime_StoreContextSlot(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);

  Handle<Object> value(args[0], isolate);
  CONVERT_ARG_CHECKED(Context, context, 1);
  CONVERT_ARG_CHECKED(String, name, 2);

  int index;
  PropertyAttributes attributes;
  ContextLookupFlags flags = FOLLOW_CHAINS;
  Handle<Object> holder =
      context->Lookup(name, flags, &index, &attributes);

  if (index >= 0) {
    if (holder->IsContext()) {
      // Ignore if read_only variable.
      if ((attributes & READ_ONLY) == 0) {
        Handle<Context>::cast(holder)->set(index, *value);
      }
    } else {
      ASSERT((attributes & READ_ONLY) == 0);
      Object* result =
          Handle<JSObject>::cast(holder)->SetElement(index, *value);
      USE(result);
      ASSERT(!result->IsFailure());
    }
    return *value;
  }

  // Slow case: The property is not in a FixedArray context.
  // It is either in an JSObject extension context or it was not found.
  Handle<JSObject> context_ext;

  if (!holder.is_null()) {
    // The property exists in the extension context.
    context_ext = Handle<JSObject>::cast(holder);
  } else {
    // The property was not found. It needs to be stored in the global context.
    ASSERT(attributes == ABSENT);
    attributes = NONE;
    context_ext = Handle<JSObject>(isolate->context()->global());
  }

  // Set the property, but ignore if read_only variable on the context
  // extension object itself.
  if ((attributes & READ_ONLY) == 0 ||
      (context_ext->GetLocalPropertyAttribute(*name) == ABSENT)) {
    Handle<Object> set = SetProperty(context_ext, name, value, attributes);
    if (set.is_null()) {
      // Failure::Exception is converted to a null handle in the
      // handle-based methods such as SetProperty.  We therefore need
      // to convert null handles back to exceptions.
      ASSERT(isolate->has_pending_exception());
      return Failure::Exception();
    }
  }
  return *value;
}


static Object* Runtime_Throw(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);

  return isolate->Throw(args[0]);
}


static Object* Runtime_ReThrow(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);

  return isolate->ReThrow(args[0]);
}


static Object* Runtime_PromoteScheduledException(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT_EQ(0, args.length());
  return isolate->PromoteScheduledException();
}


static Object* Runtime_ThrowReferenceError(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);

  Handle<Object> name(args[0], isolate);
  Handle<Object> reference_error =
    isolate->factory()->NewReferenceError("not_defined",
                                          HandleVector(&name, 1));
  return isolate->Throw(*reference_error);
}


static Object* Runtime_StackGuard(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 1);
  // First check if this is a real stack overflow.
  if (isolate->stack_guard()->IsStackOverflow()) {
    NoHandleAllocation na;
    return isolate->StackOverflow();
  }

  return Execution::HandleStackGuardInterrupt();
}


// NOTE: These PrintXXX functions are defined for all builds (not just
// DEBUG builds) because we may want to be able to trace function
// calls in all modes.
static void PrintString(String* str) {
  // not uncommon to have empty strings
  if (str->length() > 0) {
    SmartPointer<char> s =
        str->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
    PrintF("%s", *s);
  }
}


static void PrintObject(Object* obj) {
  if (obj->IsSmi()) {
    PrintF("%d", Smi::cast(obj)->value());
  } else if (obj->IsString() || obj->IsSymbol()) {
    PrintString(String::cast(obj));
  } else if (obj->IsNumber()) {
    PrintF("%g", obj->Number());
  } else if (obj->IsFailure()) {
    PrintF("<failure>");
  } else if (obj->IsUndefined()) {
    PrintF("<undefined>");
  } else if (obj->IsNull()) {
    PrintF("<null>");
  } else if (obj->IsTrue()) {
    PrintF("<true>");
  } else if (obj->IsFalse()) {
    PrintF("<false>");
  } else {
    PrintF("%p", reinterpret_cast<void*>(obj));
  }
}


static int StackSize() {
  int n = 0;
  for (JavaScriptFrameIterator it; !it.done(); it.Advance()) n++;
  return n;
}


static void PrintTransition(Object* result) {
  // indentation
  { const int nmax = 80;
    int n = StackSize();
    if (n <= nmax)
      PrintF("%4d:%*s", n, n, "");
    else
      PrintF("%4d:%*s", n, nmax, "...");
  }

  if (result == NULL) {
    // constructor calls
    JavaScriptFrameIterator it;
    JavaScriptFrame* frame = it.frame();
    if (frame->IsConstructor()) PrintF("new ");
    // function name
    Object* fun = frame->function();
    if (fun->IsJSFunction()) {
      PrintObject(JSFunction::cast(fun)->shared()->name());
    } else {
      PrintObject(fun);
    }
    // function arguments
    // (we are intentionally only printing the actually
    // supplied parameters, not all parameters required)
    PrintF("(this=");
    PrintObject(frame->receiver());
    const int length = frame->GetProvidedParametersCount();
    for (int i = 0; i < length; i++) {
      PrintF(", ");
      PrintObject(frame->GetParameter(i));
    }
    PrintF(") {\n");

  } else {
    // function result
    PrintF("} -> ");
    PrintObject(result);
    PrintF("\n");
  }
}


static Object* Runtime_TraceEnter(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 0);
  NoHandleAllocation ha;
  PrintTransition(NULL);
  return isolate->heap()->undefined_value();
}


static Object* Runtime_TraceExit(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  PrintTransition(args[0]);
  return args[0];  // return TOS
}


static Object* Runtime_DebugPrint(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);

#ifdef DEBUG
  if (args[0]->IsString()) {
    // If we have a string, assume it's a code "marker"
    // and print some interesting cpu debugging info.
    JavaScriptFrameIterator it;
    JavaScriptFrame* frame = it.frame();
    PrintF("fp = %p, sp = %p, caller_sp = %p: ",
           frame->fp(), frame->sp(), frame->caller_sp());
  } else {
    PrintF("DebugPrint: ");
  }
  args[0]->Print();
  if (args[0]->IsHeapObject()) {
    PrintF("\n");
    HeapObject::cast(args[0])->map()->Print();
  }
#else
  // ShortPrint is available in release mode. Print is not.
  args[0]->ShortPrint();
#endif
  PrintF("\n");
  Flush();

  return args[0];  // return TOS
}


static Object* Runtime_DebugTrace(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 0);
  NoHandleAllocation ha;
  isolate->PrintStack();
  return isolate->heap()->undefined_value();
}


static Object* Runtime_DateCurrentTime(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 0);

  // According to ECMA-262, section 15.9.1, page 117, the precision of
  // the number in a Date object representing a particular instant in
  // time is milliseconds. Therefore, we floor the result of getting
  // the OS time.
  double millis = floor(OS::TimeCurrentMillis());
  return isolate->heap()->NumberFromDouble(millis);
}


static Object* Runtime_DateParseString(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);

  CONVERT_ARG_CHECKED(String, str, 0);
  FlattenString(str);

  CONVERT_ARG_CHECKED(JSArray, output, 1);
  RUNTIME_ASSERT(output->HasFastElements());

  AssertNoAllocation no_allocation;

  FixedArray* output_array = FixedArray::cast(output->elements());
  RUNTIME_ASSERT(output_array->length() >= DateParser::OUTPUT_SIZE);
  bool result;
  if (str->IsAsciiRepresentation()) {
    result = DateParser::Parse(str->ToAsciiVector(), output_array);
  } else {
    ASSERT(str->IsTwoByteRepresentation());
    result = DateParser::Parse(str->ToUC16Vector(), output_array);
  }

  if (result) {
    return *output;
  } else {
    return isolate->heap()->null_value();
  }
}


static Object* Runtime_DateLocalTimezone(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);

  CONVERT_DOUBLE_CHECKED(x, args[0]);
  const char* zone = OS::LocalTimezone(x);
  return isolate->heap()->AllocateStringFromUtf8(CStrVector(zone));
}


static Object* Runtime_DateLocalTimeOffset(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 0);

  return isolate->heap()->NumberFromDouble(OS::LocalTimeOffset());
}


static Object* Runtime_DateDaylightSavingsOffset(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);

  CONVERT_DOUBLE_CHECKED(x, args[0]);
  return isolate->heap()->NumberFromDouble(OS::DaylightSavingsOffset(x));
}


static Object* Runtime_GlobalReceiver(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 1);
  Object* global = args[0];
  if (!global->IsJSGlobalObject()) return isolate->heap()->null_value();
  return JSGlobalObject::cast(global)->global_receiver();
}


static Object* Runtime_CompileString(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT_EQ(2, args.length());
  CONVERT_ARG_CHECKED(String, source, 0);
  CONVERT_ARG_CHECKED(Oddball, is_json, 1)

  // Compile source string in the global context.
  Handle<Context> context(isolate->context()->global_context());
  Compiler::ValidationState validate = (is_json->IsTrue())
    ? Compiler::VALIDATE_JSON : Compiler::DONT_VALIDATE_JSON;
  Handle<SharedFunctionInfo> shared = Compiler::CompileEval(source,
                                                            context,
                                                            true,
                                                            validate);
  if (shared.is_null()) return Failure::Exception();
  Handle<JSFunction> fun =
      isolate->factory()->NewFunctionFromSharedFunctionInfo(shared,
                                                            context,
                                                            NOT_TENURED);
  return *fun;
}


static ObjectPair CompileGlobalEval(Isolate* isolate,
                                    Handle<String> source,
                                    Handle<Object> receiver) {
  // Deal with a normal eval call with a string argument. Compile it
  // and return the compiled function bound in the local context.
  Handle<SharedFunctionInfo> shared = Compiler::CompileEval(
      source,
      Handle<Context>(isolate->context()),
      isolate->context()->IsGlobalContext(),
      Compiler::DONT_VALIDATE_JSON);
  if (shared.is_null()) return MakePair(Failure::Exception(), NULL);
  Handle<JSFunction> compiled =
      isolate->factory()->NewFunctionFromSharedFunctionInfo(
          shared, Handle<Context>(isolate->context()), NOT_TENURED);
  return MakePair(*compiled, *receiver);
}


static ObjectPair Runtime_ResolvePossiblyDirectEval(
    RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 3);
  if (!args[0]->IsJSFunction()) {
    return MakePair(isolate->ThrowIllegalOperation(), NULL);
  }

  HandleScope scope(isolate);
  Handle<JSFunction> callee = args.at<JSFunction>(0);
  Handle<Object> receiver;  // Will be overwritten.

  // Compute the calling context.
  Handle<Context> context = Handle<Context>(isolate->context());
#ifdef DEBUG
  // Make sure Isolate::context() agrees with the old code that traversed
  // the stack frames to compute the context.
  StackFrameLocator locator;
  JavaScriptFrame* frame = locator.FindJavaScriptFrame(0);
  ASSERT(Context::cast(frame->context()) == *context);
#endif

  // Find where the 'eval' symbol is bound. It is unaliased only if
  // it is bound in the global context.
  int index = -1;
  PropertyAttributes attributes = ABSENT;
  while (true) {
    receiver = context->Lookup(isolate->factory()->eval_symbol(),
                               FOLLOW_PROTOTYPE_CHAIN,
                               &index, &attributes);
    // Stop search when eval is found or when the global context is
    // reached.
    if (attributes != ABSENT || context->IsGlobalContext()) break;
    if (context->is_function_context()) {
      context = Handle<Context>(Context::cast(context->closure()->context()));
    } else {
      context = Handle<Context>(context->previous());
    }
  }

  // If eval could not be resolved, it has been deleted and we need to
  // throw a reference error.
  if (attributes == ABSENT) {
    Handle<Object> name = isolate->factory()->eval_symbol();
    Handle<Object> reference_error =
        isolate->factory()->NewReferenceError("not_defined",
                                              HandleVector(&name, 1));
    return MakePair(isolate->Throw(*reference_error), NULL);
  }

  if (!context->IsGlobalContext()) {
    // 'eval' is not bound in the global context. Just call the function
    // with the given arguments. This is not necessarily the global eval.
    if (receiver->IsContext()) {
      context = Handle<Context>::cast(receiver);
      receiver = Handle<Object>(context->get(index), isolate);
    } else if (receiver->IsJSContextExtensionObject()) {
      receiver = Handle<JSObject>(
          isolate->context()->global()->global_receiver());
    }
    return MakePair(*callee, *receiver);
  }

  // 'eval' is bound in the global context, but it may have been overwritten.
  // Compare it to the builtin 'GlobalEval' function to make sure.
  if (*callee != isolate->global_context()->global_eval_fun() ||
      !args[1]->IsString()) {
    return MakePair(*callee,
                    isolate->context()->global()->global_receiver());
  }

  return CompileGlobalEval(isolate, args.at<String>(1), args.at<Object>(2));
}


static ObjectPair Runtime_ResolvePossiblyDirectEvalNoLookup(
    RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 3);
  if (!args[0]->IsJSFunction()) {
    return MakePair(isolate->ThrowIllegalOperation(), NULL);
  }

  HandleScope scope(isolate);
  Handle<JSFunction> callee = args.at<JSFunction>(0);

  // 'eval' is bound in the global context, but it may have been overwritten.
  // Compare it to the builtin 'GlobalEval' function to make sure.
  if (*callee != isolate->global_context()->global_eval_fun() ||
      !args[1]->IsString()) {
    return MakePair(*callee,
                    isolate->context()->global()->global_receiver());
  }

  return CompileGlobalEval(isolate, args.at<String>(1), args.at<Object>(2));
}


static Object* Runtime_SetNewFunctionAttributes(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  // This utility adjusts the property attributes for newly created Function
  // object ("new Function(...)") by changing the map.
  // All it does is changing the prototype property to enumerable
  // as specified in ECMA262, 15.3.5.2.
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(JSFunction, func, 0);
  ASSERT(func->map()->instance_type() ==
         isolate->function_instance_map()->instance_type());
  ASSERT(func->map()->instance_size() ==
         isolate->function_instance_map()->instance_size());
  func->set_map(*isolate->function_instance_map());
  return *func;
}


static Object* Runtime_AllocateInNewSpace(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  // Allocate a block of memory in NewSpace (filled with a filler).
  // Use as fallback for allocation in generated code when NewSpace
  // is full.
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(Smi, size_smi, 0);
  int size = size_smi->value();
  RUNTIME_ASSERT(IsAligned(size, kPointerSize));
  RUNTIME_ASSERT(size > 0);
  Heap* heap = isolate->heap();
  const int kMinFreeNewSpaceAfterGC = heap->InitialSemiSpaceSize() * 3/4;
  RUNTIME_ASSERT(size <= kMinFreeNewSpaceAfterGC);
  Object* allocation = heap->new_space()->AllocateRaw(size);
  if (!allocation->IsFailure()) {
    heap->CreateFillerObjectAt(HeapObject::cast(allocation)->address(), size);
  }
  return allocation;
}


// Push an array unto an array of arrays if it is not already in the
// array.  Returns true if the element was pushed on the stack and
// false otherwise.
static Object* Runtime_PushIfAbsent(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 2);
  CONVERT_CHECKED(JSArray, array, args[0]);
  CONVERT_CHECKED(JSArray, element, args[1]);
  RUNTIME_ASSERT(array->HasFastElements());
  int length = Smi::cast(array->length())->value();
  FixedArray* elements = FixedArray::cast(array->elements());
  for (int i = 0; i < length; i++) {
    if (elements->get(i) == element) return isolate->heap()->false_value();
  }
  Object* obj = array->SetFastElement(length, element);
  if (obj->IsFailure()) return obj;
  return isolate->heap()->true_value();
}


/**
 * A simple visitor visits every element of Array's.
 * The backend storage can be a fixed array for fast elements case,
 * or a dictionary for sparse array. Since Dictionary is a subtype
 * of FixedArray, the class can be used by both fast and slow cases.
 * The second parameter of the constructor, fast_elements, specifies
 * whether the storage is a FixedArray or Dictionary.
 *
 * An index limit is used to deal with the situation that a result array
 * length overflows 32-bit non-negative integer.
 */
class ArrayConcatVisitor {
 public:
  ArrayConcatVisitor(Handle<FixedArray> storage,
                     uint32_t index_limit,
                     bool fast_elements) :
      storage_(storage), index_limit_(index_limit),
      index_offset_(0), fast_elements_(fast_elements) { }

  void visit(uint32_t i, Handle<Object> elm) {
    if (i >= index_limit_ - index_offset_) return;
    uint32_t index = index_offset_ + i;

    if (fast_elements_) {
      ASSERT(index < static_cast<uint32_t>(storage_->length()));
      storage_->set(index, *elm);

    } else {
      Handle<NumberDictionary> dict = Handle<NumberDictionary>::cast(storage_);
      Handle<NumberDictionary> result =
          storage_->GetIsolate()->factory()->DictionaryAtNumberPut(dict,
                                                                   index,
                                                                   elm);
      if (!result.is_identical_to(dict))
        storage_ = result;
    }
  }

  void increase_index_offset(uint32_t delta) {
    if (index_limit_ - index_offset_ < delta) {
      index_offset_ = index_limit_;
    } else {
      index_offset_ += delta;
    }
  }

  Handle<FixedArray> storage() { return storage_; }

 private:
  Handle<FixedArray> storage_;
  // Limit on the accepted indices. Elements with indices larger than the
  // limit are ignored by the visitor.
  uint32_t index_limit_;
  // Index after last seen index. Always less than or equal to index_limit_.
  uint32_t index_offset_;
  const bool fast_elements_;
};


template<class ExternalArrayClass, class ElementType>
static uint32_t IterateExternalArrayElements(Isolate* isolate,
                                             Handle<JSObject> receiver,
                                             bool elements_are_ints,
                                             bool elements_are_guaranteed_smis,
                                             uint32_t range,
                                             ArrayConcatVisitor* visitor) {
  Handle<ExternalArrayClass> array(
      ExternalArrayClass::cast(receiver->elements()));
  uint32_t len = Min(static_cast<uint32_t>(array->length()), range);

  if (visitor != NULL) {
    if (elements_are_ints) {
      if (elements_are_guaranteed_smis) {
        for (uint32_t j = 0; j < len; j++) {
          Handle<Smi> e(Smi::FromInt(static_cast<int>(array->get(j))));
          visitor->visit(j, e);
        }
      } else {
        for (uint32_t j = 0; j < len; j++) {
          int64_t val = static_cast<int64_t>(array->get(j));
          if (Smi::IsValid(static_cast<intptr_t>(val))) {
            Handle<Smi> e(Smi::FromInt(static_cast<int>(val)));
            visitor->visit(j, e);
          } else {
            Handle<Object> e =
                isolate->factory()->NewNumber(static_cast<ElementType>(val));
            visitor->visit(j, e);
          }
        }
      }
    } else {
      for (uint32_t j = 0; j < len; j++) {
        Handle<Object> e = isolate->factory()->NewNumber(array->get(j));
        visitor->visit(j, e);
      }
    }
  }

  return len;
}

/**
 * A helper function that visits elements of a JSObject. Only elements
 * whose index between 0 and range (exclusive) are visited.
 *
 * If the third parameter, visitor, is not NULL, the visitor is called
 * with parameters, 'visitor_index_offset + element index' and the element.
 *
 * It returns the number of visisted elements.
 */
static uint32_t IterateElements(Isolate* isolate,
                                Handle<JSObject> receiver,
                                uint32_t range,
                                ArrayConcatVisitor* visitor) {
  uint32_t num_of_elements = 0;

  switch (receiver->GetElementsKind()) {
    case JSObject::FAST_ELEMENTS: {
      Handle<FixedArray> elements(FixedArray::cast(receiver->elements()));
      uint32_t len = elements->length();
      if (range < len) {
        len = range;
      }

      for (uint32_t j = 0; j < len; j++) {
        Handle<Object> e(elements->get(j), isolate);
        if (!e->IsTheHole()) {
          num_of_elements++;
          if (visitor) {
            visitor->visit(j, e);
          }
        }
      }
      break;
    }
    case JSObject::PIXEL_ELEMENTS: {
      Handle<PixelArray> pixels(PixelArray::cast(receiver->elements()));
      uint32_t len = pixels->length();
      if (range < len) {
        len = range;
      }

      for (uint32_t j = 0; j < len; j++) {
        num_of_elements++;
        if (visitor != NULL) {
          Handle<Smi> e(Smi::FromInt(pixels->get(j)));
          visitor->visit(j, e);
        }
      }
      break;
    }
    case JSObject::EXTERNAL_BYTE_ELEMENTS: {
      num_of_elements =
          IterateExternalArrayElements<ExternalByteArray, int8_t>(
              isolate, receiver, true, true, range, visitor);
      break;
    }
    case JSObject::EXTERNAL_UNSIGNED_BYTE_ELEMENTS: {
      num_of_elements =
          IterateExternalArrayElements<ExternalUnsignedByteArray, uint8_t>(
              isolate, receiver, true, true, range, visitor);
      break;
    }
    case JSObject::EXTERNAL_SHORT_ELEMENTS: {
      num_of_elements =
          IterateExternalArrayElements<ExternalShortArray, int16_t>(
              isolate, receiver, true, true, range, visitor);
      break;
    }
    case JSObject::EXTERNAL_UNSIGNED_SHORT_ELEMENTS: {
      num_of_elements =
          IterateExternalArrayElements<ExternalUnsignedShortArray, uint16_t>(
              isolate, receiver, true, true, range, visitor);
      break;
    }
    case JSObject::EXTERNAL_INT_ELEMENTS: {
      num_of_elements =
          IterateExternalArrayElements<ExternalIntArray, int32_t>(
              isolate, receiver, true, false, range, visitor);
      break;
    }
    case JSObject::EXTERNAL_UNSIGNED_INT_ELEMENTS: {
      num_of_elements =
          IterateExternalArrayElements<ExternalUnsignedIntArray, uint32_t>(
              isolate, receiver, true, false, range, visitor);
      break;
    }
    case JSObject::EXTERNAL_FLOAT_ELEMENTS: {
      num_of_elements =
          IterateExternalArrayElements<ExternalFloatArray, float>(
              isolate, receiver, false, false, range, visitor);
      break;
    }
    case JSObject::DICTIONARY_ELEMENTS: {
      Handle<NumberDictionary> dict(receiver->element_dictionary());
      uint32_t capacity = dict->Capacity();
      for (uint32_t j = 0; j < capacity; j++) {
        Handle<Object> k(dict->KeyAt(j), isolate);
        if (dict->IsKey(*k)) {
          ASSERT(k->IsNumber());
          uint32_t index = static_cast<uint32_t>(k->Number());
          if (index < range) {
            num_of_elements++;
            if (visitor) {
              visitor->visit(index, Handle<Object>(dict->ValueAt(j)));
            }
          }
        }
      }
      break;
    }
    default:
      UNREACHABLE();
      break;
  }

  return num_of_elements;
}


/**
 * A helper function that visits elements of an Array object, and elements
 * on its prototypes.
 *
 * Elements on prototypes are visited first, and only elements whose indices
 * less than Array length are visited.
 *
 * If a ArrayConcatVisitor object is given, the visitor is called with
 * parameters, element's index + visitor_index_offset and the element.
 *
 * The returned number of elements is an upper bound on the actual number
 * of elements added. If the same element occurs in more than one object
 * in the array's prototype chain, it will be counted more than once, but
 * will only occur once in the result.
 */
static uint32_t IterateArrayAndPrototypeElements(Isolate* isolate,
                                                 Handle<JSArray> array,
                                                 ArrayConcatVisitor* visitor) {
  uint32_t range = static_cast<uint32_t>(array->length()->Number());
  Handle<Object> obj = array;

  static const int kEstimatedPrototypes = 3;
  List< Handle<JSObject> > objects(kEstimatedPrototypes);

  // Visit prototype first. If an element on the prototype is shadowed by
  // the inheritor using the same index, the ArrayConcatVisitor visits
  // the prototype element before the shadowing element.
  // The visitor can simply overwrite the old value by new value using
  // the same index.  This follows Array::concat semantics.
  while (!obj->IsNull()) {
    objects.Add(Handle<JSObject>::cast(obj));
    obj = Handle<Object>(obj->GetPrototype(), isolate);
  }

  uint32_t nof_elements = 0;
  for (int i = objects.length() - 1; i >= 0; i--) {
    Handle<JSObject> obj = objects[i];
    uint32_t encountered_elements =
        IterateElements(isolate, Handle<JSObject>::cast(obj), range, visitor);

    if (encountered_elements > JSObject::kMaxElementCount - nof_elements) {
      nof_elements = JSObject::kMaxElementCount;
    } else {
      nof_elements += encountered_elements;
    }
  }

  return nof_elements;
}


/**
 * A helper function of Runtime_ArrayConcat.
 *
 * The first argument is an Array of arrays and objects. It is the
 * same as the arguments array of Array::concat JS function.
 *
 * If an argument is an Array object, the function visits array
 * elements.  If an argument is not an Array object, the function
 * visits the object as if it is an one-element array.
 *
 * If the result array index overflows 32-bit unsigned integer, the rounded
 * non-negative number is used as new length. For example, if one
 * array length is 2^32 - 1, second array length is 1, the
 * concatenated array length is 0.
 * TODO(lrn) Change length behavior to ECMAScript 5 specification (length
 * is one more than the last array index to get a value assigned).
 */
static uint32_t IterateArguments(Isolate* isolate,
                                 Handle<JSArray> arguments,
                                 ArrayConcatVisitor* visitor) {
  uint32_t visited_elements = 0;
  uint32_t num_of_args = static_cast<uint32_t>(arguments->length()->Number());

  for (uint32_t i = 0; i < num_of_args; i++) {
    Handle<Object> obj(arguments->GetElement(i), isolate);
    if (obj->IsJSArray()) {
      Handle<JSArray> array = Handle<JSArray>::cast(obj);
      uint32_t len = static_cast<uint32_t>(array->length()->Number());
      uint32_t nof_elements =
          IterateArrayAndPrototypeElements(isolate, array, visitor);
      // Total elements of array and its prototype chain can be more than
      // the array length, but ArrayConcat can only concatenate at most
      // the array length number of elements. We use the length as an estimate
      // for the actual number of elements added.
      uint32_t added_elements = (nof_elements > len) ? len : nof_elements;
      if (JSArray::kMaxElementCount - visited_elements < added_elements) {
        visited_elements = JSArray::kMaxElementCount;
      } else {
        visited_elements += added_elements;
      }
      if (visitor) visitor->increase_index_offset(len);
    } else {
      if (visitor) {
        visitor->visit(0, obj);
        visitor->increase_index_offset(1);
      }
      if (visited_elements < JSArray::kMaxElementCount) {
        visited_elements++;
      }
    }
  }
  return visited_elements;
}


/**
 * Array::concat implementation.
 * See ECMAScript 262, 15.4.4.4.
 * TODO(lrn): Fix non-compliance for very large concatenations and update to
 * following the ECMAScript 5 specification.
 */
static Object* Runtime_ArrayConcat(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 1);
  HandleScope handle_scope(isolate);

  CONVERT_CHECKED(JSArray, arg_arrays, args[0]);
  Handle<JSArray> arguments(arg_arrays);

  // Pass 1: estimate the number of elements of the result
  // (it could be more than real numbers if prototype has elements).
  uint32_t result_length = 0;
  uint32_t num_of_args = static_cast<uint32_t>(arguments->length()->Number());

  { AssertNoAllocation nogc;
    for (uint32_t i = 0; i < num_of_args; i++) {
      Object* obj = arguments->GetElement(i);
      uint32_t length_estimate;
      if (obj->IsJSArray()) {
        length_estimate =
            static_cast<uint32_t>(JSArray::cast(obj)->length()->Number());
      } else {
        length_estimate = 1;
      }
      if (JSObject::kMaxElementCount - result_length < length_estimate) {
        result_length = JSObject::kMaxElementCount;
        break;
      }
      result_length += length_estimate;
    }
  }

  // Allocate an empty array, will set length and content later.
  Handle<JSArray> result = isolate->factory()->NewJSArray(0);

  uint32_t estimate_nof_elements = IterateArguments(isolate, arguments, NULL);
  // If estimated number of elements is more than half of length, a
  // fixed array (fast case) is more time and space-efficient than a
  // dictionary.
  bool fast_case = (estimate_nof_elements * 2) >= result_length;

  Handle<FixedArray> storage;
  if (fast_case) {
    // The backing storage array must have non-existing elements to
    // preserve holes across concat operations.
    storage = isolate->factory()->NewFixedArrayWithHoles(result_length);
    Handle<Map> fast_map =
        isolate->factory()->GetFastElementsMap(Handle<Map>(result->map()));
    result->set_map(*fast_map);
  } else {
    // TODO(126): move 25% pre-allocation logic into Dictionary::Allocate
    uint32_t at_least_space_for = estimate_nof_elements +
                                  (estimate_nof_elements >> 2);
    storage = Handle<FixedArray>::cast(
                  isolate->factory()->NewNumberDictionary(at_least_space_for));
    Handle<Map> slow_map =
        isolate->factory()->GetSlowElementsMap(Handle<Map>(result->map()));
    result->set_map(*slow_map);
  }

  Handle<Object> len =
      isolate->factory()->NewNumber(static_cast<double>(result_length));

  ArrayConcatVisitor visitor(storage, result_length, fast_case);

  IterateArguments(isolate, arguments, &visitor);

  result->set_length(*len);
  // Please note the storage might have changed in the visitor.
  result->set_elements(*visitor.storage());

  return *result;
}


// This will not allocate (flatten the string), but it may run
// very slowly for very deeply nested ConsStrings.  For debugging use only.
static Object* Runtime_GlobalPrint(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);

  CONVERT_CHECKED(String, string, args[0]);
  StringInputBuffer buffer(string);
  while (buffer.has_more()) {
    uint16_t character = buffer.GetNext();
    PrintF("%c", character);
  }
  return string;
}

// Moves all own elements of an object, that are below a limit, to positions
// starting at zero. All undefined values are placed after non-undefined values,
// and are followed by non-existing element. Does not change the length
// property.
// Returns the number of non-undefined elements collected.
static Object* Runtime_RemoveArrayHoles(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 2);
  CONVERT_CHECKED(JSObject, object, args[0]);
  CONVERT_NUMBER_CHECKED(uint32_t, limit, Uint32, args[1]);
  return object->PrepareElementsForSort(limit);
}


// Move contents of argument 0 (an array) to argument 1 (an array)
static Object* Runtime_MoveArrayContents(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 2);
  CONVERT_CHECKED(JSArray, from, args[0]);
  CONVERT_CHECKED(JSArray, to, args[1]);
  HeapObject* new_elements = from->elements();
  Object* new_map;
  if (new_elements->map() == isolate->heap()->fixed_array_map() ||
      new_elements->map() == isolate->heap()->fixed_cow_array_map()) {
    new_map = to->map()->GetFastElementsMap();
  } else {
    new_map = to->map()->GetSlowElementsMap();
  }
  if (new_map->IsFailure()) return new_map;
  to->set_map(Map::cast(new_map));
  to->set_elements(new_elements);
  to->set_length(from->length());
  Object* obj = from->ResetElements();
  if (obj->IsFailure()) return obj;
  from->set_length(Smi::FromInt(0));
  return to;
}


// How many elements does this object/array have?
static Object* Runtime_EstimateNumberOfElements(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 1);
  CONVERT_CHECKED(JSObject, object, args[0]);
  HeapObject* elements = object->elements();
  if (elements->IsDictionary()) {
    return Smi::FromInt(NumberDictionary::cast(elements)->NumberOfElements());
  } else if (object->IsJSArray()) {
    return JSArray::cast(object)->length();
  } else {
    return Smi::FromInt(FixedArray::cast(elements)->length());
  }
}


static Object* Runtime_SwapElements(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope handle_scope(isolate);

  ASSERT_EQ(3, args.length());

  CONVERT_ARG_CHECKED(JSObject, object, 0);
  Handle<Object> key1 = args.at<Object>(1);
  Handle<Object> key2 = args.at<Object>(2);

  uint32_t index1, index2;
  if (!key1->ToArrayIndex(&index1)
      || !key2->ToArrayIndex(&index2)) {
    return isolate->ThrowIllegalOperation();
  }

  Handle<JSObject> jsobject = Handle<JSObject>::cast(object);
  Handle<Object> tmp1 = GetElement(jsobject, index1);
  Handle<Object> tmp2 = GetElement(jsobject, index2);

  SetElement(jsobject, index1, tmp2);
  SetElement(jsobject, index2, tmp1);

  return isolate->heap()->undefined_value();
}


// Returns an array that tells you where in the [0, length) interval an array
// might have elements.  Can either return keys (positive integers) or
// intervals (pair of a negative integer (-start-1) followed by a
// positive (length)) or undefined values.
// Intervals can span over some keys that are not in the object.
static Object* Runtime_GetArrayKeys(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 2);
  HandleScope scope(isolate);
  CONVERT_ARG_CHECKED(JSObject, array, 0);
  CONVERT_NUMBER_CHECKED(uint32_t, length, Uint32, args[1]);
  if (array->elements()->IsDictionary()) {
    // Create an array and get all the keys into it, then remove all the
    // keys that are not integers in the range 0 to length-1.
    Handle<FixedArray> keys = GetKeysInFixedArrayFor(array, INCLUDE_PROTOS);
    int keys_length = keys->length();
    for (int i = 0; i < keys_length; i++) {
      Object* key = keys->get(i);
      uint32_t index;
      if (!key->ToArrayIndex(&index) || index >= length) {
        // Zap invalid keys.
        keys->set_undefined(i);
      }
    }
    return *isolate->factory()->NewJSArrayWithElements(keys);
  } else {
    ASSERT(array->HasFastElements());
    Handle<FixedArray> single_interval = isolate->factory()->NewFixedArray(2);
    // -1 means start of array.
    single_interval->set(0, Smi::FromInt(-1));
    uint32_t actual_length =
        static_cast<uint32_t>(FixedArray::cast(array->elements())->length());
    uint32_t min_length = actual_length < length ? actual_length : length;
    Handle<Object> length_object =
        isolate->factory()->NewNumber(static_cast<double>(min_length));
    single_interval->set(1, *length_object);
    return *isolate->factory()->NewJSArrayWithElements(single_interval);
  }
}


// DefineAccessor takes an optional final argument which is the
// property attributes (eg, DONT_ENUM, DONT_DELETE).  IMPORTANT: due
// to the way accessors are implemented, it is set for both the getter
// and setter on the first call to DefineAccessor and ignored on
// subsequent calls.
static Object* Runtime_DefineAccessor(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  RUNTIME_ASSERT(args.length() == 4 || args.length() == 5);
  // Compute attributes.
  PropertyAttributes attributes = NONE;
  if (args.length() == 5) {
    CONVERT_CHECKED(Smi, attrs, args[4]);
    int value = attrs->value();
    // Only attribute bits should be set.
    ASSERT((value & ~(READ_ONLY | DONT_ENUM | DONT_DELETE)) == 0);
    attributes = static_cast<PropertyAttributes>(value);
  }

  CONVERT_CHECKED(JSObject, obj, args[0]);
  CONVERT_CHECKED(String, name, args[1]);
  CONVERT_CHECKED(Smi, flag, args[2]);
  CONVERT_CHECKED(JSFunction, fun, args[3]);
  return obj->DefineAccessor(name, flag->value() == 0, fun, attributes);
}


static Object* Runtime_LookupAccessor(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 3);
  CONVERT_CHECKED(JSObject, obj, args[0]);
  CONVERT_CHECKED(String, name, args[1]);
  CONVERT_CHECKED(Smi, flag, args[2]);
  return obj->LookupAccessor(name, flag->value() == 0);
}


#ifdef ENABLE_DEBUGGER_SUPPORT
static Object* Runtime_DebugBreak(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 0);
  return Execution::DebugBreakHelper();
}


// Helper functions for wrapping and unwrapping stack frame ids.
static Smi* WrapFrameId(StackFrame::Id id) {
  ASSERT(IsAligned(OffsetFrom(id), static_cast<intptr_t>(4)));
  return Smi::FromInt(id >> 2);
}


static StackFrame::Id UnwrapFrameId(Smi* wrapped) {
  return static_cast<StackFrame::Id>(wrapped->value() << 2);
}


// Adds a JavaScript function as a debug event listener.
// args[0]: debug event listener function to set or null or undefined for
//          clearing the event listener function
// args[1]: object supplied during callback
static Object* Runtime_SetDebugEventListener(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 2);
  RUNTIME_ASSERT(args[0]->IsJSFunction() ||
                 args[0]->IsUndefined() ||
                 args[0]->IsNull());
  Handle<Object> callback = args.at<Object>(0);
  Handle<Object> data = args.at<Object>(1);
  isolate->debugger()->SetEventListener(callback, data);

  return isolate->heap()->undefined_value();
}


static Object* Runtime_Break(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 0);
  isolate->stack_guard()->DebugBreak();
  return isolate->heap()->undefined_value();
}


static Object* DebugLookupResultValue(Heap* heap,
                                      Object* receiver,
                                      String* name,
                                      LookupResult* result,
                                      bool* caught_exception) {
  Object* value;
  switch (result->type()) {
    case NORMAL:
      value = result->holder()->GetNormalizedProperty(result);
      if (value->IsTheHole()) {
        return heap->undefined_value();
      }
      return value;
    case FIELD:
      value =
          JSObject::cast(
              result->holder())->FastPropertyAt(result->GetFieldIndex());
      if (value->IsTheHole()) {
        return heap->undefined_value();
      }
      return value;
    case CONSTANT_FUNCTION:
      return result->GetConstantFunction();
    case CALLBACKS: {
      Object* structure = result->GetCallbackObject();
      if (structure->IsProxy() || structure->IsAccessorInfo()) {
        value = receiver->GetPropertyWithCallback(
            receiver, structure, name, result->holder());
        if (value->IsException()) {
          value = heap->isolate()->pending_exception();
          heap->isolate()->clear_pending_exception();
          if (caught_exception != NULL) {
            *caught_exception = true;
          }
        }
        return value;
      } else {
        return heap->undefined_value();
      }
    }
    case INTERCEPTOR:
    case MAP_TRANSITION:
    case CONSTANT_TRANSITION:
    case NULL_DESCRIPTOR:
      return heap->undefined_value();
    default:
      UNREACHABLE();
  }
  UNREACHABLE();
  return heap->undefined_value();
}


// Get debugger related details for an object property.
// args[0]: object holding property
// args[1]: name of the property
//
// The array returned contains the following information:
// 0: Property value
// 1: Property details
// 2: Property value is exception
// 3: Getter function if defined
// 4: Setter function if defined
// Items 2-4 are only filled if the property has either a getter or a setter
// defined through __defineGetter__ and/or __defineSetter__.
static Object* Runtime_DebugGetPropertyDetails(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);

  ASSERT(args.length() == 2);

  CONVERT_ARG_CHECKED(JSObject, obj, 0);
  CONVERT_ARG_CHECKED(String, name, 1);

  // Make sure to set the current context to the context before the debugger was
  // entered (if the debugger is entered). The reason for switching context here
  // is that for some property lookups (accessors and interceptors) callbacks
  // into the embedding application can occour, and the embedding application
  // could have the assumption that its own global context is the current
  // context and not some internal debugger context.
  SaveContext save(isolate);
  if (isolate->debug()->InDebugger()) {
    isolate->set_context(*isolate->debug()->debugger_entry()->GetContext());
  }

  // Skip the global proxy as it has no properties and always delegates to the
  // real global object.
  if (obj->IsJSGlobalProxy()) {
    obj = Handle<JSObject>(JSObject::cast(obj->GetPrototype()));
  }


  // Check if the name is trivially convertible to an index and get the element
  // if so.
  uint32_t index;
  if (name->AsArrayIndex(&index)) {
    Handle<FixedArray> details = isolate->factory()->NewFixedArray(2);
    Object* element_or_char = Runtime::GetElementOrCharAt(isolate,
                                                          obj,
                                                          index);
    details->set(0, element_or_char);
    details->set(1, PropertyDetails(NONE, NORMAL).AsSmi());
    return *isolate->factory()->NewJSArrayWithElements(details);
  }

  // Find the number of objects making up this.
  int length = LocalPrototypeChainLength(*obj);

  // Try local lookup on each of the objects.
  Handle<JSObject> jsproto = obj;
  for (int i = 0; i < length; i++) {
    LookupResult result;
    jsproto->LocalLookup(*name, &result);
    if (result.IsProperty()) {
      // LookupResult is not GC safe as it holds raw object pointers.
      // GC can happen later in this code so put the required fields into
      // local variables using handles when required for later use.
      PropertyType result_type = result.type();
      Handle<Object> result_callback_obj;
      if (result_type == CALLBACKS) {
        result_callback_obj = Handle<Object>(result.GetCallbackObject(),
                                             isolate);
      }
      Smi* property_details = result.GetPropertyDetails().AsSmi();
      // DebugLookupResultValue can cause GC so details from LookupResult needs
      // to be copied to handles before this.
      bool caught_exception = false;
      Object* raw_value = DebugLookupResultValue(isolate->heap(), *obj, *name,
                                                 &result, &caught_exception);
      if (raw_value->IsFailure()) return raw_value;
      Handle<Object> value(raw_value, isolate);

      // If the callback object is a fixed array then it contains JavaScript
      // getter and/or setter.
      bool hasJavaScriptAccessors = result_type == CALLBACKS &&
                                    result_callback_obj->IsFixedArray();
      Handle<FixedArray> details =
          isolate->factory()->NewFixedArray(hasJavaScriptAccessors ? 5 : 2);
      details->set(0, *value);
      details->set(1, property_details);
      if (hasJavaScriptAccessors) {
        details->set(2,
                     caught_exception ? isolate->heap()->true_value()
                                      : isolate->heap()->false_value());
        details->set(3, FixedArray::cast(*result_callback_obj)->get(0));
        details->set(4, FixedArray::cast(*result_callback_obj)->get(1));
      }

      return *isolate->factory()->NewJSArrayWithElements(details);
    }
    if (i < length - 1) {
      jsproto = Handle<JSObject>(JSObject::cast(jsproto->GetPrototype()));
    }
  }

  return isolate->heap()->undefined_value();
}


static Object* Runtime_DebugGetProperty(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);

  ASSERT(args.length() == 2);

  CONVERT_ARG_CHECKED(JSObject, obj, 0);
  CONVERT_ARG_CHECKED(String, name, 1);

  LookupResult result;
  obj->Lookup(*name, &result);
  if (result.IsProperty()) {
    return DebugLookupResultValue(isolate->heap(), *obj, *name, &result, NULL);
  }
  return isolate->heap()->undefined_value();
}


// Return the property type calculated from the property details.
// args[0]: smi with property details.
static Object* Runtime_DebugPropertyTypeFromDetails(
    RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 1);
  CONVERT_CHECKED(Smi, details, args[0]);
  PropertyType type = PropertyDetails(details).type();
  return Smi::FromInt(static_cast<int>(type));
}


// Return the property attribute calculated from the property details.
// args[0]: smi with property details.
static Object* Runtime_DebugPropertyAttributesFromDetails(
    RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 1);
  CONVERT_CHECKED(Smi, details, args[0]);
  PropertyAttributes attributes = PropertyDetails(details).attributes();
  return Smi::FromInt(static_cast<int>(attributes));
}


// Return the property insertion index calculated from the property details.
// args[0]: smi with property details.
static Object* Runtime_DebugPropertyIndexFromDetails(
    RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 1);
  CONVERT_CHECKED(Smi, details, args[0]);
  int index = PropertyDetails(details).index();
  return Smi::FromInt(index);
}


// Return property value from named interceptor.
// args[0]: object
// args[1]: property name
static Object* Runtime_DebugNamedInterceptorPropertyValue(
    RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_CHECKED(JSObject, obj, 0);
  RUNTIME_ASSERT(obj->HasNamedInterceptor());
  CONVERT_ARG_CHECKED(String, name, 1);

  PropertyAttributes attributes;
  return obj->GetPropertyWithInterceptor(*obj, *name, &attributes);
}


// Return element value from indexed interceptor.
// args[0]: object
// args[1]: index
static Object* Runtime_DebugIndexedInterceptorElementValue(
    RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_CHECKED(JSObject, obj, 0);
  RUNTIME_ASSERT(obj->HasIndexedInterceptor());
  CONVERT_NUMBER_CHECKED(uint32_t, index, Uint32, args[1]);

  return obj->GetElementWithInterceptor(*obj, index);
}


static Object* Runtime_CheckExecutionState(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() >= 1);
  CONVERT_NUMBER_CHECKED(int, break_id, Int32, args[0]);
  // Check that the break id is valid.
  if (isolate->debug()->break_id() == 0 ||
      break_id != isolate->debug()->break_id()) {
    return isolate->Throw(
        isolate->heap()->illegal_execution_state_symbol());
  }

  return isolate->heap()->true_value();
}


static Object* Runtime_GetFrameCount(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);

  // Check arguments.
  Object* result = Runtime_CheckExecutionState(args, isolate);
  if (result->IsFailure()) return result;

  // Count all frames which are relevant to debugging stack trace.
  int n = 0;
  StackFrame::Id id = isolate->debug()->break_frame_id();
  if (id == StackFrame::NO_ID) {
    // If there is no JavaScript stack frame count is 0.
    return Smi::FromInt(0);
  }
  for (JavaScriptFrameIterator it(id); !it.done(); it.Advance()) n++;
  return Smi::FromInt(n);
}


static const int kFrameDetailsFrameIdIndex = 0;
static const int kFrameDetailsReceiverIndex = 1;
static const int kFrameDetailsFunctionIndex = 2;
static const int kFrameDetailsArgumentCountIndex = 3;
static const int kFrameDetailsLocalCountIndex = 4;
static const int kFrameDetailsSourcePositionIndex = 5;
static const int kFrameDetailsConstructCallIndex = 6;
static const int kFrameDetailsAtReturnIndex = 7;
static const int kFrameDetailsDebuggerFrameIndex = 8;
static const int kFrameDetailsFirstDynamicIndex = 9;

// Return an array with frame details
// args[0]: number: break id
// args[1]: number: frame index
//
// The array returned contains the following information:
// 0: Frame id
// 1: Receiver
// 2: Function
// 3: Argument count
// 4: Local count
// 5: Source position
// 6: Constructor call
// 7: Is at return
// 8: Debugger frame
// Arguments name, value
// Locals name, value
// Return value if any
static Object* Runtime_GetFrameDetails(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);

  // Check arguments.
  Object* check = Runtime_CheckExecutionState(args, isolate);
  if (check->IsFailure()) return check;
  CONVERT_NUMBER_CHECKED(int, index, Int32, args[1]);
  Heap* heap = isolate->heap();

  // Find the relevant frame with the requested index.
  StackFrame::Id id = isolate->debug()->break_frame_id();
  if (id == StackFrame::NO_ID) {
    // If there are no JavaScript stack frames return undefined.
    return heap->undefined_value();
  }
  int count = 0;
  JavaScriptFrameIterator it(id);
  for (; !it.done(); it.Advance()) {
    if (count == index) break;
    count++;
  }
  if (it.done()) return heap->undefined_value();

  // Traverse the saved contexts chain to find the active context for the
  // selected frame.
  SaveContext* save = isolate->save_context();
  while (save != NULL && !save->below(it.frame())) {
    save = save->prev();
  }
  ASSERT(save != NULL);

  // Get the frame id.
  Handle<Object> frame_id(WrapFrameId(it.frame()->id()), isolate);

  // Find source position.
  int position =
      it.frame()->LookupCode(isolate)->SourcePosition(it.frame()->pc());

  // Check for constructor frame.
  bool constructor = it.frame()->IsConstructor();

  // Get scope info and read from it for local variable information.
  Handle<JSFunction> function(JSFunction::cast(it.frame()->function()));
  Handle<SerializedScopeInfo> scope_info(function->shared()->scope_info());
  ScopeInfo<> info(*scope_info);

  // Get the context.
  Handle<Context> context(Context::cast(it.frame()->context()));

  // Get the locals names and values into a temporary array.
  //
  // TODO(1240907): Hide compiler-introduced stack variables
  // (e.g. .result)?  For users of the debugger, they will probably be
  // confusing.
  Handle<FixedArray> locals =
      isolate->factory()->NewFixedArray(info.NumberOfLocals() * 2);
  for (int i = 0; i < info.NumberOfLocals(); i++) {
    // Name of the local.
    locals->set(i * 2, *info.LocalName(i));

    // Fetch the value of the local - either from the stack or from a
    // heap-allocated context.
    if (i < info.number_of_stack_slots()) {
      locals->set(i * 2 + 1, it.frame()->GetExpression(i));
    } else {
      Handle<String> name = info.LocalName(i);
      // Traverse the context chain to the function context as all local
      // variables stored in the context will be on the function context.
      while (!context->is_function_context()) {
        context = Handle<Context>(context->previous());
      }
      ASSERT(context->is_function_context());
      locals->set(i * 2 + 1,
                  context->get(scope_info->ContextSlotIndex(*name, NULL)));
    }
  }

  // Check whether this frame is positioned at return.
  int at_return = (index == 0) ?
      isolate->debug()->IsBreakAtReturn(it.frame()) : false;

  // If positioned just before return find the value to be returned and add it
  // to the frame information.
  Handle<Object> return_value = isolate->factory()->undefined_value();
  if (at_return) {
    StackFrameIterator it2;
    Address internal_frame_sp = NULL;
    while (!it2.done()) {
      if (it2.frame()->is_internal()) {
        internal_frame_sp = it2.frame()->sp();
      } else {
        if (it2.frame()->is_java_script()) {
          if (it2.frame()->id() == it.frame()->id()) {
            // The internal frame just before the JavaScript frame contains the
            // value to return on top. A debug break at return will create an
            // internal frame to store the return value (eax/rax/r0) before
            // entering the debug break exit frame.
            if (internal_frame_sp != NULL) {
              return_value =
                  Handle<Object>(Memory::Object_at(internal_frame_sp),
                                 isolate);
              break;
            }
          }
        }

        // Indicate that the previous frame was not an internal frame.
        internal_frame_sp = NULL;
      }
      it2.Advance();
    }
  }

  // Now advance to the arguments adapter frame (if any). It contains all
  // the provided parameters whereas the function frame always have the number
  // of arguments matching the functions parameters. The rest of the
  // information (except for what is collected above) is the same.
  it.AdvanceToArgumentsFrame();

  // Find the number of arguments to fill. At least fill the number of
  // parameters for the function and fill more if more parameters are provided.
  int argument_count = info.number_of_parameters();
  if (argument_count < it.frame()->GetProvidedParametersCount()) {
    argument_count = it.frame()->GetProvidedParametersCount();
  }

  // Calculate the size of the result.
  int details_size = kFrameDetailsFirstDynamicIndex +
                     2 * (argument_count + info.NumberOfLocals()) +
                     (at_return ? 1 : 0);
  Handle<FixedArray> details = isolate->factory()->NewFixedArray(details_size);

  // Add the frame id.
  details->set(kFrameDetailsFrameIdIndex, *frame_id);

  // Add the function (same as in function frame).
  details->set(kFrameDetailsFunctionIndex, it.frame()->function());

  // Add the arguments count.
  details->set(kFrameDetailsArgumentCountIndex, Smi::FromInt(argument_count));

  // Add the locals count
  details->set(kFrameDetailsLocalCountIndex,
               Smi::FromInt(info.NumberOfLocals()));

  // Add the source position.
  if (position != RelocInfo::kNoPosition) {
    details->set(kFrameDetailsSourcePositionIndex, Smi::FromInt(position));
  } else {
    details->set(kFrameDetailsSourcePositionIndex, heap->undefined_value());
  }

  // Add the constructor information.
  details->set(kFrameDetailsConstructCallIndex, heap->ToBoolean(constructor));

  // Add the at return information.
  details->set(kFrameDetailsAtReturnIndex, heap->ToBoolean(at_return));

  // Add information on whether this frame is invoked in the debugger context.
  details->set(kFrameDetailsDebuggerFrameIndex,
               heap->ToBoolean(*save->context() ==
                   *isolate->debug()->debug_context()));

  // Fill the dynamic part.
  int details_index = kFrameDetailsFirstDynamicIndex;

  // Add arguments name and value.
  for (int i = 0; i < argument_count; i++) {
    // Name of the argument.
    if (i < info.number_of_parameters()) {
      details->set(details_index++, *info.parameter_name(i));
    } else {
      details->set(details_index++, heap->undefined_value());
    }

    // Parameter value.
    if (i < it.frame()->GetProvidedParametersCount()) {
      details->set(details_index++, it.frame()->GetParameter(i));
    } else {
      details->set(details_index++, heap->undefined_value());
    }
  }

  // Add locals name and value from the temporary copy from the function frame.
  for (int i = 0; i < info.NumberOfLocals() * 2; i++) {
    details->set(details_index++, locals->get(i));
  }

  // Add the value being returned.
  if (at_return) {
    details->set(details_index++, *return_value);
  }

  // Add the receiver (same as in function frame).
  // THIS MUST BE DONE LAST SINCE WE MIGHT ADVANCE
  // THE FRAME ITERATOR TO WRAP THE RECEIVER.
  Handle<Object> receiver(it.frame()->receiver(), isolate);
  if (!receiver->IsJSObject()) {
    // If the receiver is NOT a JSObject we have hit an optimization
    // where a value object is not converted into a wrapped JS objects.
    // To hide this optimization from the debugger, we wrap the receiver
    // by creating correct wrapper object based on the calling frame's
    // global context.
    it.Advance();
    Handle<Context> calling_frames_global_context(
        Context::cast(Context::cast(it.frame()->context())->global_context()));
    receiver =
        isolate->factory()->ToObject(receiver, calling_frames_global_context);
  }
  details->set(kFrameDetailsReceiverIndex, *receiver);

  ASSERT_EQ(details_size, details_index);
  return *isolate->factory()->NewJSArrayWithElements(details);
}


// Copy all the context locals into an object used to materialize a scope.
static void CopyContextLocalsToScopeObject(
    Isolate* isolate,
    Handle<SerializedScopeInfo> serialized_scope_info,
    ScopeInfo<>& scope_info,
    Handle<Context> context,
    Handle<JSObject> scope_object) {
  // Fill all context locals to the context extension.
  for (int i = Context::MIN_CONTEXT_SLOTS;
       i < scope_info.number_of_context_slots();
       i++) {
    int context_index = serialized_scope_info->ContextSlotIndex(
        *scope_info.context_slot_name(i), NULL);

    // Don't include the arguments shadow (.arguments) context variable.
    if (*scope_info.context_slot_name(i) !=
        isolate->heap()->arguments_shadow_symbol()) {
      SetProperty(scope_object,
                  scope_info.context_slot_name(i),
                  Handle<Object>(context->get(context_index), isolate), NONE);
    }
  }
}


// Create a plain JSObject which materializes the local scope for the specified
// frame.
static Handle<JSObject> MaterializeLocalScope(Isolate* isolate,
                                              JavaScriptFrame* frame) {
  Handle<JSFunction> function(JSFunction::cast(frame->function()));
  Handle<SharedFunctionInfo> shared(function->shared());
  Handle<SerializedScopeInfo> serialized_scope_info(shared->scope_info());
  ScopeInfo<> scope_info(*serialized_scope_info);

  // Allocate and initialize a JSObject with all the arguments, stack locals
  // heap locals and extension properties of the debugged function.
  Handle<JSObject> local_scope =
      isolate->factory()->NewJSObject(isolate->object_function());

  // First fill all parameters.
  for (int i = 0; i < scope_info.number_of_parameters(); ++i) {
    SetProperty(local_scope,
                scope_info.parameter_name(i),
                Handle<Object>(frame->GetParameter(i), isolate), NONE);
  }

  // Second fill all stack locals.
  for (int i = 0; i < scope_info.number_of_stack_slots(); i++) {
    SetProperty(local_scope,
                scope_info.stack_slot_name(i),
                Handle<Object>(frame->GetExpression(i), isolate), NONE);
  }

  // Third fill all context locals.
  Handle<Context> frame_context(Context::cast(frame->context()));
  Handle<Context> function_context(frame_context->fcontext());
  CopyContextLocalsToScopeObject(isolate, serialized_scope_info, scope_info,
                                 function_context, local_scope);

  // Finally copy any properties from the function context extension. This will
  // be variables introduced by eval.
  if (function_context->closure() == *function) {
    if (function_context->has_extension() &&
        !function_context->IsGlobalContext()) {
      Handle<JSObject> ext(JSObject::cast(function_context->extension()));
      Handle<FixedArray> keys = GetKeysInFixedArrayFor(ext, INCLUDE_PROTOS);
      for (int i = 0; i < keys->length(); i++) {
        // Names of variables introduced by eval are strings.
        ASSERT(keys->get(i)->IsString());
        Handle<String> key(String::cast(keys->get(i)));
        SetProperty(local_scope, key, GetProperty(ext, key), NONE);
      }
    }
  }
  return local_scope;
}


// Create a plain JSObject which materializes the closure content for the
// context.
static Handle<JSObject> MaterializeClosure(Isolate* isolate,
                                           Handle<Context> context) {
  ASSERT(context->is_function_context());

  Handle<SharedFunctionInfo> shared(context->closure()->shared());
  Handle<SerializedScopeInfo> serialized_scope_info(shared->scope_info());
  ScopeInfo<> scope_info(*serialized_scope_info);

  // Allocate and initialize a JSObject with all the content of theis function
  // closure.
  Handle<JSObject> closure_scope =
      isolate->factory()->NewJSObject(isolate->object_function());

  // Check whether the arguments shadow object exists.
  int arguments_shadow_index =
      shared->scope_info()->ContextSlotIndex(
          isolate->heap()->arguments_shadow_symbol(), NULL);
  if (arguments_shadow_index >= 0) {
    // In this case all the arguments are available in the arguments shadow
    // object.
    Handle<JSObject> arguments_shadow(
        JSObject::cast(context->get(arguments_shadow_index)));
    for (int i = 0; i < scope_info.number_of_parameters(); ++i) {
      SetProperty(closure_scope,
                  scope_info.parameter_name(i),
                  Handle<Object>(arguments_shadow->GetElement(i), isolate),
                  NONE);
    }
  }

  // Fill all context locals to the context extension.
  CopyContextLocalsToScopeObject(isolate, serialized_scope_info, scope_info,
                                 context, closure_scope);

  // Finally copy any properties from the function context extension. This will
  // be variables introduced by eval.
  if (context->has_extension()) {
    Handle<JSObject> ext(JSObject::cast(context->extension()));
    Handle<FixedArray> keys = GetKeysInFixedArrayFor(ext, INCLUDE_PROTOS);
    for (int i = 0; i < keys->length(); i++) {
      // Names of variables introduced by eval are strings.
      ASSERT(keys->get(i)->IsString());
      Handle<String> key(String::cast(keys->get(i)));
      SetProperty(closure_scope, key, GetProperty(ext, key), NONE);
    }
  }

  return closure_scope;
}


// Iterate over the actual scopes visible from a stack frame. All scopes are
// backed by an actual context except the local scope, which is inserted
// "artifically" in the context chain.
class ScopeIterator {
 public:
  enum ScopeType {
    ScopeTypeGlobal = 0,
    ScopeTypeLocal,
    ScopeTypeWith,
    ScopeTypeClosure,
    // Every catch block contains an implicit with block (its parameter is
    // a JSContextExtensionObject) that extends current scope with a variable
    // holding exception object. Such with blocks are treated as scopes of their
    // own type.
    ScopeTypeCatch
  };

  explicit ScopeIterator(Isolate* isolate, JavaScriptFrame* frame)
    : isolate_(isolate),
      frame_(frame),
      function_(JSFunction::cast(frame->function())),
      context_(Context::cast(frame->context())),
      local_done_(false),
      at_local_(false) {

    // Check whether the first scope is actually a local scope.
    if (context_->IsGlobalContext()) {
      // If there is a stack slot for .result then this local scope has been
      // created for evaluating top level code and it is not a real local scope.
      // Checking for the existence of .result seems fragile, but the scope info
      // saved with the code object does not otherwise have that information.
      int index = function_->shared()->scope_info()->
          StackSlotIndex(isolate_->heap()->result_symbol());
      at_local_ = index < 0;
    } else if (context_->is_function_context()) {
      at_local_ = true;
    }
  }

  // More scopes?
  bool Done() { return context_.is_null(); }

  // Move to the next scope.
  void Next() {
    // If at a local scope mark the local scope as passed.
    if (at_local_) {
      at_local_ = false;
      local_done_ = true;

      // If the current context is not associated with the local scope the
      // current context is the next real scope, so don't move to the next
      // context in this case.
      if (context_->closure() != *function_) {
        return;
      }
    }

    // The global scope is always the last in the chain.
    if (context_->IsGlobalContext()) {
      context_ = Handle<Context>();
      return;
    }

    // Move to the next context.
    if (context_->is_function_context()) {
      context_ = Handle<Context>(Context::cast(context_->closure()->context()));
    } else {
      context_ = Handle<Context>(context_->previous());
    }

    // If passing the local scope indicate that the current scope is now the
    // local scope.
    if (!local_done_ &&
        (context_->IsGlobalContext() || (context_->is_function_context()))) {
      at_local_ = true;
    }
  }

  // Return the type of the current scope.
  int Type() {
    if (at_local_) {
      return ScopeTypeLocal;
    }
    if (context_->IsGlobalContext()) {
      ASSERT(context_->global()->IsGlobalObject());
      return ScopeTypeGlobal;
    }
    if (context_->is_function_context()) {
      return ScopeTypeClosure;
    }
    ASSERT(context_->has_extension());
    // Current scope is either an explicit with statement or a with statement
    // implicitely generated for a catch block.
    // If the extension object here is a JSContextExtensionObject then
    // current with statement is one frome a catch block otherwise it's a
    // regular with statement.
    if (context_->extension()->IsJSContextExtensionObject()) {
      return ScopeTypeCatch;
    }
    return ScopeTypeWith;
  }

  // Return the JavaScript object with the content of the current scope.
  Handle<JSObject> ScopeObject() {
    switch (Type()) {
      case ScopeIterator::ScopeTypeGlobal:
        return Handle<JSObject>(CurrentContext()->global());
        break;
      case ScopeIterator::ScopeTypeLocal:
        // Materialize the content of the local scope into a JSObject.
        return MaterializeLocalScope(isolate_, frame_);
        break;
      case ScopeIterator::ScopeTypeWith:
      case ScopeIterator::ScopeTypeCatch:
        // Return the with object.
        return Handle<JSObject>(CurrentContext()->extension());
        break;
      case ScopeIterator::ScopeTypeClosure:
        // Materialize the content of the closure scope into a JSObject.
        return MaterializeClosure(isolate_, CurrentContext());
        break;
    }
    UNREACHABLE();
    return Handle<JSObject>();
  }

  // Return the context for this scope. For the local context there might not
  // be an actual context.
  Handle<Context> CurrentContext() {
    if (at_local_ && context_->closure() != *function_) {
      return Handle<Context>();
    }
    return context_;
  }

#ifdef DEBUG
  // Debug print of the content of the current scope.
  void DebugPrint() {
    switch (Type()) {
      case ScopeIterator::ScopeTypeGlobal:
        PrintF("Global:\n");
        CurrentContext()->Print();
        break;

      case ScopeIterator::ScopeTypeLocal: {
        PrintF("Local:\n");
        ScopeInfo<> scope_info(function_->shared()->scope_info());
        scope_info.Print();
        if (!CurrentContext().is_null()) {
          CurrentContext()->Print();
          if (CurrentContext()->has_extension()) {
            Handle<JSObject> extension =
                Handle<JSObject>(CurrentContext()->extension());
            if (extension->IsJSContextExtensionObject()) {
              extension->Print();
            }
          }
        }
        break;
      }

      case ScopeIterator::ScopeTypeWith: {
        PrintF("With:\n");
        Handle<JSObject> extension =
            Handle<JSObject>(CurrentContext()->extension());
        extension->Print();
        break;
      }

      case ScopeIterator::ScopeTypeCatch: {
        PrintF("Catch:\n");
        Handle<JSObject> extension =
            Handle<JSObject>(CurrentContext()->extension());
        extension->Print();
        break;
      }

      case ScopeIterator::ScopeTypeClosure: {
        PrintF("Closure:\n");
        CurrentContext()->Print();
        if (CurrentContext()->has_extension()) {
          Handle<JSObject> extension =
              Handle<JSObject>(CurrentContext()->extension());
          if (extension->IsJSContextExtensionObject()) {
            extension->Print();
          }
        }
        break;
      }

      default:
        UNREACHABLE();
    }
    PrintF("\n");
  }
#endif

 private:
  Isolate* isolate_;
  JavaScriptFrame* frame_;
  Handle<JSFunction> function_;
  Handle<Context> context_;
  bool local_done_;
  bool at_local_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ScopeIterator);
};


static Object* Runtime_GetScopeCount(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);

  // Check arguments.
  Object* check = Runtime_CheckExecutionState(args, isolate);
  if (check->IsFailure()) return check;
  CONVERT_CHECKED(Smi, wrapped_id, args[1]);

  // Get the frame where the debugging is performed.
  StackFrame::Id id = UnwrapFrameId(wrapped_id);
  JavaScriptFrameIterator it(id);
  JavaScriptFrame* frame = it.frame();

  // Count the visible scopes.
  int n = 0;
  for (ScopeIterator it(isolate, frame); !it.Done(); it.Next()) {
    n++;
  }

  return Smi::FromInt(n);
}


static const int kScopeDetailsTypeIndex = 0;
static const int kScopeDetailsObjectIndex = 1;
static const int kScopeDetailsSize = 2;

// Return an array with scope details
// args[0]: number: break id
// args[1]: number: frame index
// args[2]: number: scope index
//
// The array returned contains the following information:
// 0: Scope type
// 1: Scope object
static Object* Runtime_GetScopeDetails(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);

  // Check arguments.
  Object* check = Runtime_CheckExecutionState(args, isolate);
  if (check->IsFailure()) return check;
  CONVERT_CHECKED(Smi, wrapped_id, args[1]);
  CONVERT_NUMBER_CHECKED(int, index, Int32, args[2]);

  // Get the frame where the debugging is performed.
  StackFrame::Id id = UnwrapFrameId(wrapped_id);
  JavaScriptFrameIterator frame_it(id);
  JavaScriptFrame* frame = frame_it.frame();

  // Find the requested scope.
  int n = 0;
  ScopeIterator it(isolate, frame);
  for (; !it.Done() && n < index; it.Next()) {
    n++;
  }
  if (it.Done()) {
    return isolate->heap()->undefined_value();
  }

  // Calculate the size of the result.
  int details_size = kScopeDetailsSize;
  Handle<FixedArray> details = isolate->factory()->NewFixedArray(details_size);

  // Fill in scope details.
  details->set(kScopeDetailsTypeIndex, Smi::FromInt(it.Type()));
  Handle<JSObject> scope_object = it.ScopeObject();
  details->set(kScopeDetailsObjectIndex, *scope_object);

  return *isolate->factory()->NewJSArrayWithElements(details);
}


static Object* Runtime_DebugPrintScopes(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 0);

#ifdef DEBUG
  // Print the scopes for the top frame.
  StackFrameLocator locator;
  JavaScriptFrame* frame = locator.FindJavaScriptFrame(0);
  for (ScopeIterator it(isolate, frame); !it.Done(); it.Next()) {
    it.DebugPrint();
  }
#endif
  return isolate->heap()->undefined_value();
}


static Object* Runtime_GetCFrames(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  Object* result = Runtime_CheckExecutionState(args, isolate);
  if (result->IsFailure()) return result;

#if V8_HOST_ARCH_64_BIT
  UNIMPLEMENTED();
  return isolate->heap()->undefined_value();
#else

  static const int kMaxCFramesSize = 200;
  ScopedVector<OS::StackFrame> frames(kMaxCFramesSize);
  int frames_count = OS::StackWalk(frames);
  if (frames_count == OS::kStackWalkError) {
    return isolate->heap()->undefined_value();
  }

  Handle<String> address_str = isolate->factory()->LookupAsciiSymbol("address");
  Handle<String> text_str = isolate->factory()->LookupAsciiSymbol("text");
  Handle<FixedArray> frames_array =
      isolate->factory()->NewFixedArray(frames_count);
  for (int i = 0; i < frames_count; i++) {
    Handle<JSObject> frame_value =
        isolate->factory()->NewJSObject(isolate->object_function());
    Handle<Object> frame_address =
        isolate->factory()->NewNumberFromInt(
            reinterpret_cast<int>(frames[i].address));

    frame_value->SetProperty(*address_str, *frame_address, NONE);

    // Get the stack walk text for this frame.
    Handle<String> frame_text;
    int frame_text_length = StrLength(frames[i].text);
    if (frame_text_length > 0) {
      Vector<const char> str(frames[i].text, frame_text_length);
      frame_text = isolate->factory()->NewStringFromAscii(str);
    }

    if (!frame_text.is_null()) {
      frame_value->SetProperty(*text_str, *frame_text, NONE);
    }

    frames_array->set(i, *frame_value);
  }
  return *isolate->factory()->NewJSArrayWithElements(frames_array);
#endif  // V8_HOST_ARCH_64_BIT
}


static Object* Runtime_GetThreadCount(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);

  // Check arguments.
  Object* result = Runtime_CheckExecutionState(args, isolate);
  if (result->IsFailure()) return result;

  // Count all archived V8 threads.
  int n = 0;
  for (ThreadState* thread =
          isolate->thread_manager()->FirstThreadStateInUse();
       thread != NULL;
       thread = thread->Next()) {
    n++;
  }

  // Total number of threads is current thread and archived threads.
  return Smi::FromInt(n + 1);
}


static const int kThreadDetailsCurrentThreadIndex = 0;
static const int kThreadDetailsThreadIdIndex = 1;
static const int kThreadDetailsSize = 2;

// Return an array with thread details
// args[0]: number: break id
// args[1]: number: thread index
//
// The array returned contains the following information:
// 0: Is current thread?
// 1: Thread id
static Object* Runtime_GetThreadDetails(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);

  // Check arguments.
  Object* check = Runtime_CheckExecutionState(args, isolate);
  if (check->IsFailure()) return check;
  CONVERT_NUMBER_CHECKED(int, index, Int32, args[1]);

  // Allocate array for result.
  Handle<FixedArray> details =
      isolate->factory()->NewFixedArray(kThreadDetailsSize);

  // Thread index 0 is current thread.
  if (index == 0) {
    // Fill the details.
    details->set(kThreadDetailsCurrentThreadIndex,
                 isolate->heap()->true_value());
    details->set(kThreadDetailsThreadIdIndex,
                 Smi::FromInt(
                     isolate->thread_manager()->CurrentId()));
  } else {
    // Find the thread with the requested index.
    int n = 1;
    ThreadState* thread =
        isolate->thread_manager()->FirstThreadStateInUse();
    while (index != n && thread != NULL) {
      thread = thread->Next();
      n++;
    }
    if (thread == NULL) {
      return isolate->heap()->undefined_value();
    }

    // Fill the details.
    details->set(kThreadDetailsCurrentThreadIndex,
                 isolate->heap()->false_value());
    details->set(kThreadDetailsThreadIdIndex, Smi::FromInt(thread->id()));
  }

  // Convert to JS array and return.
  return *isolate->factory()->NewJSArrayWithElements(details);
}


// Sets the disable break state
// args[0]: disable break state
static Object* Runtime_SetDisableBreak(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_BOOLEAN_CHECKED(disable_break, args[0]);
  isolate->debug()->set_disable_break(disable_break);
  return  isolate->heap()->undefined_value();
}


static Object* Runtime_GetBreakLocations(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);

  CONVERT_ARG_CHECKED(JSFunction, fun, 0);
  Handle<SharedFunctionInfo> shared(fun->shared());
  // Find the number of break points
  Handle<Object> break_locations = Debug::GetSourceBreakLocations(shared);
  if (break_locations->IsUndefined()) return isolate->heap()->undefined_value();
  // Return array as JS array
  return *isolate->factory()->NewJSArrayWithElements(
      Handle<FixedArray>::cast(break_locations));
}


// Set a break point in a function
// args[0]: function
// args[1]: number: break source position (within the function source)
// args[2]: number: break point object
static Object* Runtime_SetFunctionBreakPoint(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  CONVERT_ARG_CHECKED(JSFunction, fun, 0);
  Handle<SharedFunctionInfo> shared(fun->shared());
  CONVERT_NUMBER_CHECKED(int32_t, source_position, Int32, args[1]);
  RUNTIME_ASSERT(source_position >= 0);
  Handle<Object> break_point_object_arg = args.at<Object>(2);

  // Set break point.
  isolate->debug()->SetBreakPoint(shared, break_point_object_arg,
                                  &source_position);

  return Smi::FromInt(source_position);
}


Object* Runtime::FindSharedFunctionInfoInScript(Isolate* isolate,
                                                Handle<Script> script,
                                                int position) {
  // Iterate the heap looking for SharedFunctionInfo generated from the
  // script. The inner most SharedFunctionInfo containing the source position
  // for the requested break point is found.
  // NOTE: This might reqire several heap iterations. If the SharedFunctionInfo
  // which is found is not compiled it is compiled and the heap is iterated
  // again as the compilation might create inner functions from the newly
  // compiled function and the actual requested break point might be in one of
  // these functions.
  bool done = false;
  // The current candidate for the source position:
  int target_start_position = RelocInfo::kNoPosition;
  Handle<SharedFunctionInfo> target;
  while (!done) {
    HeapIterator iterator;
    for (HeapObject* obj = iterator.next();
         obj != NULL; obj = iterator.next()) {
      if (obj->IsSharedFunctionInfo()) {
        Handle<SharedFunctionInfo> shared(SharedFunctionInfo::cast(obj));
        if (shared->script() == *script) {
          // If the SharedFunctionInfo found has the requested script data and
          // contains the source position it is a candidate.
          int start_position = shared->function_token_position();
          if (start_position == RelocInfo::kNoPosition) {
            start_position = shared->start_position();
          }
          if (start_position <= position &&
              position <= shared->end_position()) {
            // If there is no candidate or this function is within the current
            // candidate this is the new candidate.
            if (target.is_null()) {
              target_start_position = start_position;
              target = shared;
            } else {
              if (target_start_position == start_position &&
                  shared->end_position() == target->end_position()) {
                  // If a top-level function contain only one function
                  // declartion the source for the top-level and the function is
                  // the same. In that case prefer the non top-level function.
                if (!shared->is_toplevel()) {
                  target_start_position = start_position;
                  target = shared;
                }
              } else if (target_start_position <= start_position &&
                         shared->end_position() <= target->end_position()) {
                // This containment check includes equality as a function inside
                // a top-level function can share either start or end position
                // with the top-level function.
                target_start_position = start_position;
                target = shared;
              }
            }
          }
        }
      }
    }

    if (target.is_null()) {
      return isolate->heap()->undefined_value();
    }

    // If the candidate found is compiled we are done. NOTE: when lazy
    // compilation of inner functions is introduced some additional checking
    // needs to be done here to compile inner functions.
    done = target->is_compiled();
    if (!done) {
      // If the candidate is not compiled compile it to reveal any inner
      // functions which might contain the requested source position.
      CompileLazyShared(target, KEEP_EXCEPTION);
    }
  }

  return *target;
}


// Changes the state of a break point in a script and returns source position
// where break point was set. NOTE: Regarding performance see the NOTE for
// GetScriptFromScriptData.
// args[0]: script to set break point in
// args[1]: number: break source position (within the script source)
// args[2]: number: break point object
static Object* Runtime_SetScriptBreakPoint(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  CONVERT_ARG_CHECKED(JSValue, wrapper, 0);
  CONVERT_NUMBER_CHECKED(int32_t, source_position, Int32, args[1]);
  RUNTIME_ASSERT(source_position >= 0);
  Handle<Object> break_point_object_arg = args.at<Object>(2);

  // Get the script from the script wrapper.
  RUNTIME_ASSERT(wrapper->value()->IsScript());
  Handle<Script> script(Script::cast(wrapper->value()));

  Object* result = Runtime::FindSharedFunctionInfoInScript(
      isolate, script, source_position);
  if (!result->IsUndefined()) {
    Handle<SharedFunctionInfo> shared(SharedFunctionInfo::cast(result));
    // Find position within function. The script position might be before the
    // source position of the first function.
    int position;
    if (shared->start_position() > source_position) {
      position = 0;
    } else {
      position = source_position - shared->start_position();
    }
    isolate->debug()->SetBreakPoint(shared, break_point_object_arg, &position);
    position += shared->start_position();
    return Smi::FromInt(position);
  }
  return  isolate->heap()->undefined_value();
}


// Clear a break point
// args[0]: number: break point object
static Object* Runtime_ClearBreakPoint(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  Handle<Object> break_point_object_arg = args.at<Object>(0);

  // Clear break point.
  isolate->debug()->ClearBreakPoint(break_point_object_arg);

  return isolate->heap()->undefined_value();
}


// Change the state of break on exceptions.
// args[0]: Enum value indicating whether to affect caught/uncaught exceptions.
// args[1]: Boolean indicating on/off.
static Object* Runtime_ChangeBreakOnException(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  RUNTIME_ASSERT(args[0]->IsNumber());
  CONVERT_BOOLEAN_CHECKED(enable, args[1]);

  // If the number doesn't match an enum value, the ChangeBreakOnException
  // function will default to affecting caught exceptions.
  ExceptionBreakType type =
      static_cast<ExceptionBreakType>(NumberToUint32(args[0]));
  // Update break point state.
  isolate->debug()->ChangeBreakOnException(type, enable);
  return isolate->heap()->undefined_value();
}


// Returns the state of break on exceptions
// args[0]: boolean indicating uncaught exceptions
static Object* Runtime_IsBreakOnException(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  RUNTIME_ASSERT(args[0]->IsNumber());

  ExceptionBreakType type =
      static_cast<ExceptionBreakType>(NumberToUint32(args[0]));
  bool result = isolate->debug()->IsBreakOnException(type);
  return Smi::FromInt(result);
}


// Prepare for stepping
// args[0]: break id for checking execution state
// args[1]: step action from the enumeration StepAction
// args[2]: number of times to perform the step, for step out it is the number
//          of frames to step down.
static Object* Runtime_PrepareStep(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  // Check arguments.
  Object* check = Runtime_CheckExecutionState(args, isolate);
  if (check->IsFailure()) return check;
  if (!args[1]->IsNumber() || !args[2]->IsNumber()) {
    return isolate->Throw(isolate->heap()->illegal_argument_symbol());
  }

  // Get the step action and check validity.
  StepAction step_action = static_cast<StepAction>(NumberToInt32(args[1]));
  if (step_action != StepIn &&
      step_action != StepNext &&
      step_action != StepOut &&
      step_action != StepInMin &&
      step_action != StepMin) {
    return isolate->Throw(isolate->heap()->illegal_argument_symbol());
  }

  // Get the number of steps.
  int step_count = NumberToInt32(args[2]);
  if (step_count < 1) {
    return isolate->Throw(isolate->heap()->illegal_argument_symbol());
  }

  // Clear all current stepping setup.
  isolate->debug()->ClearStepping();

  // Prepare step.
  isolate->debug()->PrepareStep(static_cast<StepAction>(step_action),
                                step_count);
  return isolate->heap()->undefined_value();
}


// Clear all stepping set by PrepareStep.
static Object* Runtime_ClearStepping(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 0);
  isolate->debug()->ClearStepping();
  return isolate->heap()->undefined_value();
}


// Creates a copy of the with context chain. The copy of the context chain is
// is linked to the function context supplied.
static Handle<Context> CopyWithContextChain(Handle<Context> context_chain,
                                            Handle<Context> function_context) {
  // At the bottom of the chain. Return the function context to link to.
  if (context_chain->is_function_context()) {
    return function_context;
  }

  // Recursively copy the with contexts.
  Handle<Context> previous(context_chain->previous());
  Handle<JSObject> extension(JSObject::cast(context_chain->extension()));
  Handle<Context> context = CopyWithContextChain(function_context, previous);
  return context->GetIsolate()->factory()->NewWithContext(
      context, extension, context_chain->IsCatchContext());
}


// Helper function to find or create the arguments object for
// Runtime_DebugEvaluate.
static Handle<Object> GetArgumentsObject(Isolate* isolate,
                                         JavaScriptFrame* frame,
                                         Handle<JSFunction> function,
                                         Handle<SerializedScopeInfo> scope_info,
                                         const ScopeInfo<>* sinfo,
                                         Handle<Context> function_context) {
  // Try to find the value of 'arguments' to pass as parameter. If it is not
  // found (that is the debugged function does not reference 'arguments' and
  // does not support eval) then create an 'arguments' object.
  int index;
  if (sinfo->number_of_stack_slots() > 0) {
    index = scope_info->StackSlotIndex(isolate->heap()->arguments_symbol());
    if (index != -1) {
      return Handle<Object>(frame->GetExpression(index), isolate);
    }
  }

  if (sinfo->number_of_context_slots() > Context::MIN_CONTEXT_SLOTS) {
    index = scope_info->ContextSlotIndex(isolate->heap()->arguments_symbol(),
                                         NULL);
    if (index != -1) {
      return Handle<Object>(function_context->get(index), isolate);
    }
  }

  const int length = frame->GetProvidedParametersCount();
  Handle<JSObject> arguments =
      isolate->factory()->NewArgumentsObject(function, length);
  Handle<FixedArray> array = isolate->factory()->NewFixedArray(length);

  AssertNoAllocation no_gc;
  WriteBarrierMode mode = array->GetWriteBarrierMode(no_gc);
  for (int i = 0; i < length; i++) {
    array->set(i, frame->GetParameter(i), mode);
  }
  arguments->set_elements(*array);
  return arguments;
}


static const char kSourceStr[] =
    "(function(arguments,__source__){return eval(__source__);})";


// Evaluate a piece of JavaScript in the context of a stack frame for
// debugging. This is accomplished by creating a new context which in its
// extension part has all the parameters and locals of the function on the
// stack frame. A function which calls eval with the code to evaluate is then
// compiled in this context and called in this context. As this context
// replaces the context of the function on the stack frame a new (empty)
// function is created as well to be used as the closure for the context.
// This function and the context acts as replacements for the function on the
// stack frame presenting the same view of the values of parameters and
// local variables as if the piece of JavaScript was evaluated at the point
// where the function on the stack frame is currently stopped.
static Object* Runtime_DebugEvaluate(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);

  // Check the execution state and decode arguments frame and source to be
  // evaluated.
  ASSERT(args.length() == 4);
  Object* check_result = Runtime_CheckExecutionState(args, isolate);
  if (check_result->IsFailure()) return check_result;
  CONVERT_CHECKED(Smi, wrapped_id, args[1]);
  CONVERT_ARG_CHECKED(String, source, 2);
  CONVERT_BOOLEAN_CHECKED(disable_break, args[3]);

  // Handle the processing of break.
  DisableBreak disable_break_save(disable_break);

  // Get the frame where the debugging is performed.
  StackFrame::Id id = UnwrapFrameId(wrapped_id);
  JavaScriptFrameIterator it(id);
  JavaScriptFrame* frame = it.frame();
  Handle<JSFunction> function(JSFunction::cast(frame->function()));
  Handle<SerializedScopeInfo> scope_info(function->shared()->scope_info());
  ScopeInfo<> sinfo(*scope_info);

  // Traverse the saved contexts chain to find the active context for the
  // selected frame.
  SaveContext* save = isolate->save_context();
  while (save != NULL && !save->below(frame)) {
    save = save->prev();
  }
  ASSERT(save != NULL);
  SaveContext savex(isolate);
  isolate->set_context(*(save->context()));

  // Create the (empty) function replacing the function on the stack frame for
  // the purpose of evaluating in the context created below. It is important
  // that this function does not describe any parameters and local variables
  // in the context. If it does then this will cause problems with the lookup
  // in Context::Lookup, where context slots for parameters and local variables
  // are looked at before the extension object.
  Handle<JSFunction> go_between =
      isolate->factory()->NewFunction(isolate->factory()->empty_string(),
                                      isolate->factory()->undefined_value());
  go_between->set_context(function->context());
#ifdef DEBUG
  ScopeInfo<> go_between_sinfo(go_between->shared()->scope_info());
  ASSERT(go_between_sinfo.number_of_parameters() == 0);
  ASSERT(go_between_sinfo.number_of_context_slots() == 0);
#endif

  // Materialize the content of the local scope into a JSObject.
  Handle<JSObject> local_scope = MaterializeLocalScope(isolate, frame);

  // Allocate a new context for the debug evaluation and set the extension
  // object build.
  Handle<Context> context =
      isolate->factory()->NewFunctionContext(Context::MIN_CONTEXT_SLOTS,
                                             go_between);
  context->set_extension(*local_scope);
  // Copy any with contexts present and chain them in front of this context.
  Handle<Context> frame_context(Context::cast(frame->context()));
  Handle<Context> function_context(frame_context->fcontext());
  context = CopyWithContextChain(frame_context, context);

  // Wrap the evaluation statement in a new function compiled in the newly
  // created context. The function has one parameter which has to be called
  // 'arguments'. This it to have access to what would have been 'arguments' in
  // the function being debugged.
  // function(arguments,__source__) {return eval(__source__);}

  Handle<String> function_source =
      isolate->factory()->NewStringFromAscii(
          Vector<const char>(kSourceStr, sizeof(kSourceStr) - 1));
  Handle<SharedFunctionInfo> shared =
      Compiler::CompileEval(function_source,
                            context,
                            context->IsGlobalContext(),
                            Compiler::DONT_VALIDATE_JSON);
  if (shared.is_null()) return Failure::Exception();
  Handle<JSFunction> compiled_function =
      isolate->factory()->NewFunctionFromSharedFunctionInfo(shared, context);

  // Invoke the result of the compilation to get the evaluation function.
  bool has_pending_exception;
  Handle<Object> receiver(frame->receiver(), isolate);
  Handle<Object> evaluation_function =
      Execution::Call(compiled_function, receiver, 0, NULL,
                      &has_pending_exception);
  if (has_pending_exception) return Failure::Exception();

  Handle<Object> arguments = GetArgumentsObject(isolate, frame,
                                                function, scope_info,
                                                &sinfo, function_context);

  // Invoke the evaluation function and return the result.
  const int argc = 2;
  Object** argv[argc] = { arguments.location(),
                          Handle<Object>::cast(source).location() };
  Handle<Object> result =
      Execution::Call(Handle<JSFunction>::cast(evaluation_function), receiver,
                      argc, argv, &has_pending_exception);
  if (has_pending_exception) return Failure::Exception();

  // Skip the global proxy as it has no properties and always delegates to the
  // real global object.
  if (result->IsJSGlobalProxy()) {
    result = Handle<JSObject>(JSObject::cast(result->GetPrototype()));
  }

  return *result;
}


static Object* Runtime_DebugEvaluateGlobal(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);

  // Check the execution state and decode arguments frame and source to be
  // evaluated.
  ASSERT(args.length() == 3);
  Object* check_result = Runtime_CheckExecutionState(args, isolate);
  if (check_result->IsFailure()) return check_result;
  CONVERT_ARG_CHECKED(String, source, 1);
  CONVERT_BOOLEAN_CHECKED(disable_break, args[2]);

  // Handle the processing of break.
  DisableBreak disable_break_save(disable_break);

  // Enter the top context from before the debugger was invoked.
  SaveContext save(isolate);
  SaveContext* top = &save;
  while (top != NULL && *top->context() == *isolate->debug()->debug_context()) {
    top = top->prev();
  }
  if (top != NULL) {
    isolate->set_context(*top->context());
  }

  // Get the global context now set to the top context from before the
  // debugger was invoked.
  Handle<Context> context = isolate->global_context();

  // Compile the source to be evaluated.
  Handle<SharedFunctionInfo> shared =
      Compiler::CompileEval(source,
                            context,
                            true,
                            Compiler::DONT_VALIDATE_JSON);
  if (shared.is_null()) return Failure::Exception();
  Handle<JSFunction> compiled_function =
      Handle<JSFunction>(
          isolate->factory()->NewFunctionFromSharedFunctionInfo(shared,
                                                                context));

  // Invoke the result of the compilation to get the evaluation function.
  bool has_pending_exception;
  Handle<Object> receiver = isolate->global();
  Handle<Object> result =
    Execution::Call(compiled_function, receiver, 0, NULL,
                    &has_pending_exception);
  if (has_pending_exception) return Failure::Exception();
  return *result;
}


static Object* Runtime_DebugGetLoadedScripts(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);
  ASSERT(args.length() == 0);

  // Fill the script objects.
  Handle<FixedArray> instances = isolate->debug()->GetLoadedScripts();

  // Convert the script objects to proper JS objects.
  for (int i = 0; i < instances->length(); i++) {
    Handle<Script> script = Handle<Script>(Script::cast(instances->get(i)));
    // Get the script wrapper in a local handle before calling GetScriptWrapper,
    // because using
    //   instances->set(i, *GetScriptWrapper(script))
    // is unsafe as GetScriptWrapper might call GC and the C++ compiler might
    // already have deferenced the instances handle.
    Handle<JSValue> wrapper = GetScriptWrapper(script);
    instances->set(i, *wrapper);
  }

  // Return result as a JS array.
  Handle<JSObject> result =
      isolate->factory()->NewJSObject(isolate->array_function());
  Handle<JSArray>::cast(result)->SetContent(*instances);
  return *result;
}


// Helper function used by Runtime_DebugReferencedBy below.
static int DebugReferencedBy(JSObject* target,
                             Object* instance_filter, int max_references,
                             FixedArray* instances, int instances_size,
                             JSFunction* arguments_function) {
  NoHandleAllocation ha;
  AssertNoAllocation no_alloc;

  // Iterate the heap.
  int count = 0;
  JSObject* last = NULL;
  HeapIterator iterator;
  HeapObject* heap_obj = NULL;
  while (((heap_obj = iterator.next()) != NULL) &&
         (max_references == 0 || count < max_references)) {
    // Only look at all JSObjects.
    if (heap_obj->IsJSObject()) {
      // Skip context extension objects and argument arrays as these are
      // checked in the context of functions using them.
      JSObject* obj = JSObject::cast(heap_obj);
      if (obj->IsJSContextExtensionObject() ||
          obj->map()->constructor() == arguments_function) {
        continue;
      }

      // Check if the JS object has a reference to the object looked for.
      if (obj->ReferencesObject(target)) {
        // Check instance filter if supplied. This is normally used to avoid
        // references from mirror objects (see Runtime_IsInPrototypeChain).
        if (!instance_filter->IsUndefined()) {
          Object* V = obj;
          while (true) {
            Object* prototype = V->GetPrototype();
            if (prototype->IsNull()) {
              break;
            }
            if (instance_filter == prototype) {
              obj = NULL;  // Don't add this object.
              break;
            }
            V = prototype;
          }
        }

        if (obj != NULL) {
          // Valid reference found add to instance array if supplied an update
          // count.
          if (instances != NULL && count < instances_size) {
            instances->set(count, obj);
          }
          last = obj;
          count++;
        }
      }
    }
  }

  // Check for circular reference only. This can happen when the object is only
  // referenced from mirrors and has a circular reference in which case the
  // object is not really alive and would have been garbage collected if not
  // referenced from the mirror.
  if (count == 1 && last == target) {
    count = 0;
  }

  // Return the number of referencing objects found.
  return count;
}


// Scan the heap for objects with direct references to an object
// args[0]: the object to find references to
// args[1]: constructor function for instances to exclude (Mirror)
// args[2]: the the maximum number of objects to return
static Object* Runtime_DebugReferencedBy(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 3);

  // First perform a full GC in order to avoid references from dead objects.
  isolate->heap()->CollectAllGarbage(false);

  // Check parameters.
  CONVERT_CHECKED(JSObject, target, args[0]);
  Object* instance_filter = args[1];
  RUNTIME_ASSERT(instance_filter->IsUndefined() ||
                 instance_filter->IsJSObject());
  CONVERT_NUMBER_CHECKED(int32_t, max_references, Int32, args[2]);
  RUNTIME_ASSERT(max_references >= 0);

  // Get the constructor function for context extension and arguments array.
  JSObject* arguments_boilerplate =
      isolate->context()->global_context()->arguments_boilerplate();
  JSFunction* arguments_function =
      JSFunction::cast(arguments_boilerplate->map()->constructor());

  // Get the number of referencing objects.
  int count;
  count = DebugReferencedBy(target, instance_filter, max_references,
                            NULL, 0, arguments_function);

  // Allocate an array to hold the result.
  Object* object = isolate->heap()->AllocateFixedArray(count);
  if (object->IsFailure()) return object;
  FixedArray* instances = FixedArray::cast(object);

  // Fill the referencing objects.
  count = DebugReferencedBy(target, instance_filter, max_references,
                            instances, count, arguments_function);

  // Return result as JS array.
  Object* result =
      isolate->heap()->AllocateJSObject(
          isolate->context()->global_context()->array_function());
  if (!result->IsFailure()) JSArray::cast(result)->SetContent(instances);
  return result;
}


// Helper function used by Runtime_DebugConstructedBy below.
static int DebugConstructedBy(JSFunction* constructor, int max_references,
                              FixedArray* instances, int instances_size) {
  AssertNoAllocation no_alloc;

  // Iterate the heap.
  int count = 0;
  HeapIterator iterator;
  HeapObject* heap_obj = NULL;
  while (((heap_obj = iterator.next()) != NULL) &&
         (max_references == 0 || count < max_references)) {
    // Only look at all JSObjects.
    if (heap_obj->IsJSObject()) {
      JSObject* obj = JSObject::cast(heap_obj);
      if (obj->map()->constructor() == constructor) {
        // Valid reference found add to instance array if supplied an update
        // count.
        if (instances != NULL && count < instances_size) {
          instances->set(count, obj);
        }
        count++;
      }
    }
  }

  // Return the number of referencing objects found.
  return count;
}


// Scan the heap for objects constructed by a specific function.
// args[0]: the constructor to find instances of
// args[1]: the the maximum number of objects to return
static Object* Runtime_DebugConstructedBy(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 2);

  // First perform a full GC in order to avoid dead objects.
  isolate->heap()->CollectAllGarbage(false);

  // Check parameters.
  CONVERT_CHECKED(JSFunction, constructor, args[0]);
  CONVERT_NUMBER_CHECKED(int32_t, max_references, Int32, args[1]);
  RUNTIME_ASSERT(max_references >= 0);

  // Get the number of referencing objects.
  int count;
  count = DebugConstructedBy(constructor, max_references, NULL, 0);

  // Allocate an array to hold the result.
  Object* object = isolate->heap()->AllocateFixedArray(count);
  if (object->IsFailure()) return object;
  FixedArray* instances = FixedArray::cast(object);

  // Fill the referencing objects.
  count = DebugConstructedBy(constructor, max_references, instances, count);

  // Return result as JS array.
  Object* result =
      isolate->heap()->AllocateJSObject(
          isolate->context()->global_context()->array_function());
  if (!result->IsFailure()) JSArray::cast(result)->SetContent(instances);
  return result;
}


// Find the effective prototype object as returned by __proto__.
// args[0]: the object to find the prototype for.
static Object* Runtime_DebugGetPrototype(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 1);

  CONVERT_CHECKED(JSObject, obj, args[0]);

  // Use the __proto__ accessor.
  return Accessors::ObjectPrototype.getter(obj, NULL);
}


static Object* Runtime_SystemBreak(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 0);
  CPU::DebugBreak();
  return isolate->heap()->undefined_value();
}


static Object* Runtime_DebugDisassembleFunction(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
#ifdef DEBUG
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  // Get the function and make sure it is compiled.
  CONVERT_ARG_CHECKED(JSFunction, func, 0);
  Handle<SharedFunctionInfo> shared(func->shared());
  if (!EnsureCompiled(shared, KEEP_EXCEPTION)) {
    return Failure::Exception();
  }
  func->code()->PrintLn();
#endif  // DEBUG
  return isolate->heap()->undefined_value();
}


static Object* Runtime_DebugDisassembleConstructor(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
#ifdef DEBUG
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  // Get the function and make sure it is compiled.
  CONVERT_ARG_CHECKED(JSFunction, func, 0);
  Handle<SharedFunctionInfo> shared(func->shared());
  if (!EnsureCompiled(shared, KEEP_EXCEPTION)) {
    return Failure::Exception();
  }
  shared->construct_stub()->PrintLn();
#endif  // DEBUG
  return isolate->heap()->undefined_value();
}


static Object* Runtime_FunctionGetInferredName(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);

  CONVERT_CHECKED(JSFunction, f, args[0]);
  return f->shared()->inferred_name();
}


static int FindSharedFunctionInfosForScript(Script* script,
                                            FixedArray* buffer) {
  AssertNoAllocation no_allocations;

  int counter = 0;
  int buffer_size = buffer->length();
  HeapIterator iterator;
  for (HeapObject* obj = iterator.next(); obj != NULL; obj = iterator.next()) {
    ASSERT(obj != NULL);
    if (!obj->IsSharedFunctionInfo()) {
      continue;
    }
    SharedFunctionInfo* shared = SharedFunctionInfo::cast(obj);
    if (shared->script() != script) {
      continue;
    }
    if (counter < buffer_size) {
      buffer->set(counter, shared);
    }
    counter++;
  }
  return counter;
}

// For a script finds all SharedFunctionInfo's in the heap that points
// to this script. Returns JSArray of SharedFunctionInfo wrapped
// in OpaqueReferences.
static Object* Runtime_LiveEditFindSharedFunctionInfosForScript(
    RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 1);
  HandleScope scope(isolate);
  CONVERT_CHECKED(JSValue, script_value, args[0]);

  Handle<Script> script = Handle<Script>(Script::cast(script_value->value()));

  const int kBufferSize = 32;

  Handle<FixedArray> array;
  array = isolate->factory()->NewFixedArray(kBufferSize);
  int number = FindSharedFunctionInfosForScript(*script, *array);
  if (number > kBufferSize) {
    array = isolate->factory()->NewFixedArray(number);
    FindSharedFunctionInfosForScript(*script, *array);
  }

  Handle<JSArray> result = isolate->factory()->NewJSArrayWithElements(array);
  result->set_length(Smi::FromInt(number));

  LiveEdit::WrapSharedFunctionInfos(result);

  return *result;
}

// For a script calculates compilation information about all its functions.
// The script source is explicitly specified by the second argument.
// The source of the actual script is not used, however it is important that
// all generated code keeps references to this particular instance of script.
// Returns a JSArray of compilation infos. The array is ordered so that
// each function with all its descendant is always stored in a continues range
// with the function itself going first. The root function is a script function.
static Object* Runtime_LiveEditGatherCompileInfo(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 2);
  HandleScope scope(isolate);
  CONVERT_CHECKED(JSValue, script, args[0]);
  CONVERT_ARG_CHECKED(String, source, 1);
  Handle<Script> script_handle = Handle<Script>(Script::cast(script->value()));

  JSArray* result =  LiveEdit::GatherCompileInfo(script_handle, source);

  if (isolate->has_pending_exception()) {
    return Failure::Exception();
  }

  return result;
}

// Changes the source of the script to a new_source.
// If old_script_name is provided (i.e. is a String), also creates a copy of
// the script with its original source and sends notification to debugger.
static Object* Runtime_LiveEditReplaceScript(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 3);
  HandleScope scope(isolate);
  CONVERT_CHECKED(JSValue, original_script_value, args[0]);
  CONVERT_ARG_CHECKED(String, new_source, 1);
  Handle<Object> old_script_name(args[2], isolate);

  CONVERT_CHECKED(Script, original_script_pointer,
                  original_script_value->value());
  Handle<Script> original_script(original_script_pointer);

  Object* old_script = LiveEdit::ChangeScriptSource(original_script,
                                                    new_source,
                                                    old_script_name);

  if (old_script->IsScript()) {
    Handle<Script> script_handle(Script::cast(old_script));
    return *(GetScriptWrapper(script_handle));
  } else {
    return isolate->heap()->null_value();
  }
}

// Replaces code of SharedFunctionInfo with a new one.
static Object* Runtime_LiveEditReplaceFunctionCode(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 2);
  HandleScope scope(isolate);
  CONVERT_ARG_CHECKED(JSArray, new_compile_info, 0);
  CONVERT_ARG_CHECKED(JSArray, shared_info, 1);

  return LiveEdit::ReplaceFunctionCode(new_compile_info, shared_info);
}

// Connects SharedFunctionInfo to another script.
static Object* Runtime_LiveEditFunctionSetScript(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 2);
  HandleScope scope(isolate);
  Handle<Object> function_object(args[0], isolate);
  Handle<Object> script_object(args[1], isolate);

  if (function_object->IsJSValue()) {
    Handle<JSValue> function_wrapper = Handle<JSValue>::cast(function_object);
    if (script_object->IsJSValue()) {
      CONVERT_CHECKED(Script, script, JSValue::cast(*script_object)->value());
      script_object = Handle<Object>(script, isolate);
    }

    LiveEdit::SetFunctionScript(function_wrapper, script_object);
  } else {
    // Just ignore this. We may not have a SharedFunctionInfo for some functions
    // and we check it in this function.
  }

  return isolate->heap()->undefined_value();
}


// In a code of a parent function replaces original function as embedded object
// with a substitution one.
static Object* Runtime_LiveEditReplaceRefToNestedFunction(
    RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 3);
  HandleScope scope(isolate);

  CONVERT_ARG_CHECKED(JSValue, parent_wrapper, 0);
  CONVERT_ARG_CHECKED(JSValue, orig_wrapper, 1);
  CONVERT_ARG_CHECKED(JSValue, subst_wrapper, 2);

  LiveEdit::ReplaceRefToNestedFunction(parent_wrapper, orig_wrapper,
                                       subst_wrapper);

  return isolate->heap()->undefined_value();
}


// Updates positions of a shared function info (first parameter) according
// to script source change. Text change is described in second parameter as
// array of groups of 3 numbers:
// (change_begin, change_end, change_end_new_position).
// Each group describes a change in text; groups are sorted by change_begin.
static Object* Runtime_LiveEditPatchFunctionPositions(
    RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 2);
  HandleScope scope(isolate);
  CONVERT_ARG_CHECKED(JSArray, shared_array, 0);
  CONVERT_ARG_CHECKED(JSArray, position_change_array, 1);

  return LiveEdit::PatchFunctionPositions(shared_array, position_change_array);
}


// For array of SharedFunctionInfo's (each wrapped in JSValue)
// checks that none of them have activations on stacks (of any thread).
// Returns array of the same length with corresponding results of
// LiveEdit::FunctionPatchabilityStatus type.
static Object* Runtime_LiveEditCheckAndDropActivations(
    RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 2);
  HandleScope scope(isolate);
  CONVERT_ARG_CHECKED(JSArray, shared_array, 0);
  CONVERT_BOOLEAN_CHECKED(do_drop, args[1]);

  return *LiveEdit::CheckAndDropActivations(shared_array, do_drop);
}

// Compares 2 strings line-by-line and returns diff in form of JSArray of
// triplets (pos1, pos1_end, pos2_end) describing list of diff chunks.
static Object* Runtime_LiveEditCompareStringsLinewise(
    RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 2);
  HandleScope scope(isolate);
  CONVERT_ARG_CHECKED(String, s1, 0);
  CONVERT_ARG_CHECKED(String, s2, 1);

  return *LiveEdit::CompareStringsLinewise(s1, s2);
}



// A testing entry. Returns statement position which is the closest to
// source_position.
static Object* Runtime_GetFunctionCodePositionFromSource(
    RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 2);
  HandleScope scope(isolate);
  CONVERT_ARG_CHECKED(JSFunction, function, 0);
  CONVERT_NUMBER_CHECKED(int32_t, source_position, Int32, args[1]);

  Handle<Code> code(function->code());

  RelocIterator it(*code, 1 << RelocInfo::STATEMENT_POSITION);
  int closest_pc = 0;
  int distance = kMaxInt;
  while (!it.done()) {
    int statement_position = static_cast<int>(it.rinfo()->data());
    // Check if this break point is closer that what was previously found.
    if (source_position <= statement_position &&
        statement_position - source_position < distance) {
      closest_pc =
          static_cast<int>(it.rinfo()->pc() - code->instruction_start());
      distance = statement_position - source_position;
      // Check whether we can't get any closer.
      if (distance == 0) break;
    }
    it.next();
  }

  return Smi::FromInt(closest_pc);
}


// Calls specified function with or without entering the debugger.
// This is used in unit tests to run code as if debugger is entered or simply
// to have a stack with C++ frame in the middle.
static Object* Runtime_ExecuteInDebugContext(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 2);
  HandleScope scope(isolate);
  CONVERT_ARG_CHECKED(JSFunction, function, 0);
  CONVERT_BOOLEAN_CHECKED(without_debugger, args[1]);

  Handle<Object> result;
  bool pending_exception;
  {
    if (without_debugger) {
      result = Execution::Call(function, isolate->global(), 0, NULL,
                               &pending_exception);
    } else {
      EnterDebugger enter_debugger;
      result = Execution::Call(function, isolate->global(), 0, NULL,
                               &pending_exception);
    }
  }
  if (!pending_exception) {
    return *result;
  } else {
    return Failure::Exception();
  }
}


#endif  // ENABLE_DEBUGGER_SUPPORT

#ifdef ENABLE_LOGGING_AND_PROFILING

static Object* Runtime_ProfilerResume(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_CHECKED(Smi, smi_modules, args[0]);
  CONVERT_CHECKED(Smi, smi_tag, args[1]);
  v8::V8::ResumeProfilerEx(smi_modules->value(), smi_tag->value());
  return isolate->heap()->undefined_value();
}


static Object* Runtime_ProfilerPause(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_CHECKED(Smi, smi_modules, args[0]);
  CONVERT_CHECKED(Smi, smi_tag, args[1]);
  v8::V8::PauseProfilerEx(smi_modules->value(), smi_tag->value());
  return isolate->heap()->undefined_value();
}

#endif  // ENABLE_LOGGING_AND_PROFILING

// Finds the script object from the script data. NOTE: This operation uses
// heap traversal to find the function generated for the source position
// for the requested break point. For lazily compiled functions several heap
// traversals might be required rendering this operation as a rather slow
// operation. However for setting break points which is normally done through
// some kind of user interaction the performance is not crucial.
static Handle<Object> Runtime_GetScriptFromScriptName(
    Handle<String> script_name) {
  // Scan the heap for Script objects to find the script with the requested
  // script data.
  Handle<Script> script;
  HeapIterator iterator;
  HeapObject* obj = NULL;
  while (script.is_null() && ((obj = iterator.next()) != NULL)) {
    // If a script is found check if it has the script data requested.
    if (obj->IsScript()) {
      if (Script::cast(obj)->name()->IsString()) {
        if (String::cast(Script::cast(obj)->name())->Equals(*script_name)) {
          script = Handle<Script>(Script::cast(obj));
        }
      }
    }
  }

  // If no script with the requested script data is found return undefined.
  if (script.is_null()) return FACTORY->undefined_value();

  // Return the script found.
  return GetScriptWrapper(script);
}


// Get the script object from script data. NOTE: Regarding performance
// see the NOTE for GetScriptFromScriptData.
// args[0]: script data for the script to find the source for
static Object* Runtime_GetScript(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  HandleScope scope(isolate);

  ASSERT(args.length() == 1);

  CONVERT_CHECKED(String, script_name, args[0]);

  // Find the requested script.
  Handle<Object> result =
      Runtime_GetScriptFromScriptName(Handle<String>(script_name));
  return *result;
}


// Determines whether the given stack frame should be displayed in
// a stack trace.  The caller is the error constructor that asked
// for the stack trace to be collected.  The first time a construct
// call to this function is encountered it is skipped.  The seen_caller
// in/out parameter is used to remember if the caller has been seen
// yet.
static bool ShowFrameInStackTrace(StackFrame* raw_frame, Object* caller,
    bool* seen_caller) {
  // Only display JS frames.
  if (!raw_frame->is_java_script())
    return false;
  JavaScriptFrame* frame = JavaScriptFrame::cast(raw_frame);
  Object* raw_fun = frame->function();
  // Not sure when this can happen but skip it just in case.
  if (!raw_fun->IsJSFunction())
    return false;
  if ((raw_fun == caller) && !(*seen_caller)) {
    *seen_caller = true;
    return false;
  }
  // Skip all frames until we've seen the caller.  Also, skip the most
  // obvious builtin calls.  Some builtin calls (such as Number.ADD
  // which is invoked using 'call') are very difficult to recognize
  // so we're leaving them in for now.
  return *seen_caller && !frame->receiver()->IsJSBuiltinsObject();
}


// Collect the raw data for a stack trace.  Returns an array of three
// element segments each containing a receiver, function and native
// code offset.
static Object* Runtime_CollectStackTrace(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT_EQ(args.length(), 2);
  Handle<Object> caller = args.at<Object>(0);
  CONVERT_NUMBER_CHECKED(int32_t, limit, Int32, args[1]);

  HandleScope scope(isolate);

  limit = Max(limit, 0);  // Ensure that limit is not negative.
  int initial_size = Min(limit, 10);
  Handle<JSArray> result = isolate->factory()->NewJSArray(initial_size * 3);

  StackFrameIterator iter;
  // If the caller parameter is a function we skip frames until we're
  // under it before starting to collect.
  bool seen_caller = !caller->IsJSFunction();
  int cursor = 0;
  int frames_seen = 0;
  while (!iter.done() && frames_seen < limit) {
    StackFrame* raw_frame = iter.frame();
    if (ShowFrameInStackTrace(raw_frame, *caller, &seen_caller)) {
      frames_seen++;
      JavaScriptFrame* frame = JavaScriptFrame::cast(raw_frame);
      Object* recv = frame->receiver();
      Object* fun = frame->function();
      Address pc = frame->pc();
      Address start = frame->LookupCode(isolate)->address();
      Smi* offset = Smi::FromInt(static_cast<int>(pc - start));
      FixedArray* elements = FixedArray::cast(result->elements());
      if (cursor + 2 < elements->length()) {
        elements->set(cursor++, recv);
        elements->set(cursor++, fun);
        elements->set(cursor++, offset);
      } else {
        HandleScope scope(isolate);
        Handle<Object> recv_handle(recv, isolate);
        Handle<Object> fun_handle(fun, isolate);
        SetElement(result, cursor++, recv_handle);
        SetElement(result, cursor++, fun_handle);
        SetElement(result, cursor++, Handle<Smi>(offset));
      }
    }
    iter.Advance();
  }

  result->set_length(Smi::FromInt(cursor));
  return *result;
}


// Returns V8 version as a string.
static Object* Runtime_GetV8Version(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT_EQ(args.length(), 0);

  NoHandleAllocation ha;

  const char* version_string = v8::V8::GetVersion();

  return isolate->heap()->AllocateStringFromAscii(CStrVector(version_string),
                                                  NOT_TENURED);
}


static Object* Runtime_Abort(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 2);
  OS::PrintError("abort: %s\n", reinterpret_cast<char*>(args[0]) +
                                    Smi::cast(args[1])->value());
  isolate->PrintStack();
  OS::Abort();
  UNREACHABLE();
  return NULL;
}


static Object* Runtime_DeleteHandleScopeExtensions(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 0);
  HandleScope::DeleteExtensions(isolate);
  return isolate->heap()->undefined_value();
}


static Object* CacheMiss(Isolate* isolate,
                         FixedArray* cache_obj,
                         int index,
                         Object* key_obj) {
  ASSERT(index % 2 == 0);  // index of the key
  ASSERT(index >= JSFunctionResultCache::kEntriesIndex);
  ASSERT(index < cache_obj->length());

  HandleScope scope(isolate);

  Handle<FixedArray> cache(cache_obj);
  Handle<Object> key(key_obj, isolate);
  Handle<JSFunction> factory(JSFunction::cast(
        cache->get(JSFunctionResultCache::kFactoryIndex)));
  // TODO(antonm): consider passing a receiver when constructing a cache.
  Handle<Object> receiver(isolate->global_context()->global(), isolate);

  Handle<Object> value;
  {
    // This handle is nor shared, nor used later, so it's safe.
    Object** argv[] = { key.location() };
    bool pending_exception = false;
    value = Execution::Call(factory,
                            receiver,
                            1,
                            argv,
                            &pending_exception);
    if (pending_exception) return Failure::Exception();
  }

  cache->set(index, *key);
  cache->set(index + 1, *value);
  cache->set(JSFunctionResultCache::kFingerIndex, Smi::FromInt(index));

  return *value;
}


static Object* Runtime_GetFromCache(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  // This is only called from codegen, so checks might be more lax.
  CONVERT_CHECKED(FixedArray, cache, args[0]);
  Object* key = args[1];

  const int finger_index =
      Smi::cast(cache->get(JSFunctionResultCache::kFingerIndex))->value();

  Object* o = cache->get(finger_index);
  if (o == key) {
    // The fastest case: hit the same place again.
    return cache->get(finger_index + 1);
  }

  for (int i = finger_index - 2;
       i >= JSFunctionResultCache::kEntriesIndex;
       i -= 2) {
    o = cache->get(i);
    if (o == key) {
      cache->set(JSFunctionResultCache::kFingerIndex, Smi::FromInt(i));
      return cache->get(i + 1);
    }
  }

  const int size =
      Smi::cast(cache->get(JSFunctionResultCache::kCacheSizeIndex))->value();
  ASSERT(size <= cache->length());

  for (int i = size - 2; i > finger_index; i -= 2) {
    o = cache->get(i);
    if (o == key) {
      cache->set(JSFunctionResultCache::kFingerIndex, Smi::FromInt(i));
      return cache->get(i + 1);
    }
  }

  // Cache miss.  If we have spare room, put new data into it, otherwise
  // evict post finger entry which must be least recently used.
  if (size < cache->length()) {
    cache->set(JSFunctionResultCache::kCacheSizeIndex, Smi::FromInt(size + 2));
    return CacheMiss(isolate, cache, size, key);
  } else {
    int target_index = finger_index + JSFunctionResultCache::kEntrySize;
    if (target_index == cache->length()) {
      target_index = JSFunctionResultCache::kEntriesIndex;
    }
    return CacheMiss(isolate, cache, target_index, key);
  }
}

#ifdef DEBUG
// ListNatives is ONLY used by the fuzz-natives.js in debug mode
// Exclude the code in release mode.
static Object* Runtime_ListNatives(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 0);
  HandleScope scope(isolate);
  Handle<JSArray> result = isolate->factory()->NewJSArray(0);
  int index = 0;
  bool inline_runtime_functions = false;
#define ADD_ENTRY(Name, argc, ressize)                                       \
  {                                                                          \
    HandleScope inner;                                                       \
    Handle<String> name;                                                     \
    /* Inline runtime functions have an underscore in front of the name. */  \
    if (inline_runtime_functions) {                                          \
      name = isolate->factory()->NewStringFromAscii(                         \
          Vector<const char>("_" #Name, StrLength("_" #Name)));              \
    } else {                                                                 \
      name = isolate->factory()->NewStringFromAscii(                         \
          Vector<const char>(#Name, StrLength(#Name)));                      \
    }                                                                        \
    Handle<JSArray> pair = isolate->factory()->NewJSArray(0);                \
    SetElement(pair, 0, name);                                               \
    SetElement(pair, 1, Handle<Smi>(Smi::FromInt(argc)));                    \
    SetElement(result, index++, pair);                                       \
  }
  inline_runtime_functions = false;
  RUNTIME_FUNCTION_LIST(ADD_ENTRY)
  inline_runtime_functions = true;
  INLINE_FUNCTION_LIST(ADD_ENTRY)
  INLINE_RUNTIME_FUNCTION_LIST(ADD_ENTRY)
#undef ADD_ENTRY
  return *result;
}
#endif


static Object* Runtime_Log(RUNTIME_CALLING_CONVENTION) {
  RUNTIME_GET_ISOLATE;
  ASSERT(args.length() == 2);
  CONVERT_CHECKED(String, format, args[0]);
  CONVERT_CHECKED(JSArray, elms, args[1]);
  Vector<const char> chars = format->ToAsciiVector();
  LOGGER->LogRuntime(chars, elms);
  return isolate->heap()->undefined_value();
}


static Object* Runtime_IS_VAR(RUNTIME_CALLING_CONVENTION) {
  UNREACHABLE();  // implemented as macro in the parser
  return NULL;
}


// ----------------------------------------------------------------------------
// Implementation of Runtime

#define F(name, number_of_args, result_size)                             \
  { Runtime::k##name, Runtime::RUNTIME, #name,   \
    FUNCTION_ADDR(Runtime_##name), number_of_args, result_size },


#define I(name, number_of_args, result_size)                             \
  { Runtime::kInline##name, Runtime::INLINE,     \
    "_" #name, NULL, number_of_args, result_size },

static const Runtime::Function kIntrinsicFunctions[] = {
  RUNTIME_FUNCTION_LIST(F)
  INLINE_FUNCTION_LIST(I)
  INLINE_RUNTIME_FUNCTION_LIST(I)
};


Object* Runtime::InitializeIntrinsicFunctionNames(Heap* heap,
                                                  Object* dictionary) {
  ASSERT(Isolate::Current()->heap() == heap);
  ASSERT(dictionary != NULL);
  ASSERT(StringDictionary::cast(dictionary)->NumberOfElements() == 0);
  for (int i = 0; i < kNumFunctions; ++i) {
    Object* name_symbol = heap->LookupAsciiSymbol(kIntrinsicFunctions[i].name);
    if (name_symbol->IsFailure()) return name_symbol;
    StringDictionary* string_dictionary = StringDictionary::cast(dictionary);
    dictionary = string_dictionary->Add(String::cast(name_symbol),
                                        Smi::FromInt(i),
                                        PropertyDetails(NONE, NORMAL));
    // Non-recoverable failure.  Calling code must restart heap initialization.
    if (dictionary->IsFailure()) return dictionary;
  }
  return dictionary;
}


const Runtime::Function* Runtime::FunctionForSymbol(Handle<String> name) {
  Heap* heap = HEAP;
  int entry = heap->intrinsic_function_names()->FindEntry(*name);
  if (entry != kNotFound) {
    Object* smi_index = heap->intrinsic_function_names()->ValueAt(entry);
    int function_index = Smi::cast(smi_index)->value();
    return &(kIntrinsicFunctions[function_index]);
  }
  return NULL;
}


const Runtime::Function* Runtime::FunctionForId(Runtime::FunctionId id) {
  return &(kIntrinsicFunctions[static_cast<int>(id)]);
}


void Runtime::PerformGC(Object* result) {
  Failure* failure = Failure::cast(result);
  if (failure->IsRetryAfterGC()) {
    // Try to do a garbage collection; ignore it if it fails. The C
    // entry stub will throw an out-of-memory exception in that case.
    HEAP->CollectGarbage(failure->requested(), failure->allocation_space());
  } else {
    // Handle last resort GC and make sure to allow future allocations
    // to grow the heap without causing GCs (if possible).
    COUNTERS->gc_last_resort_from_js()->Increment();
    HEAP->CollectAllGarbage(false);
  }
}


InlineRuntimeFunctionsTable::InlineRuntimeFunctionsTable() {
  // List of special runtime calls which are generated inline. For some of these
  // functions the code will be generated inline, and for others a call to a
  // code stub will be inlined.
  int lut_index = 0;
#define SETUP_RUNTIME_ENTRIES(Name, argc, resize)                              \
  entries_[lut_index].method = &CodeGenerator::Generate##Name;                 \
  entries_[lut_index].name = "_" #Name;                                        \
  entries_[lut_index++].nargs = argc;
  INLINE_RUNTIME_FUNCTION_LIST(SETUP_RUNTIME_ENTRIES);
#undef SETUP_RUNTIME_ENTRIES
}


} }  // namespace v8::internal